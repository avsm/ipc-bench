/* 
    Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
    Copyright (c) 2011 Chris Smowton <chris@smowton.net>

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use,
    copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following
    conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
*/

#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <err.h>
#include <inttypes.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <errno.h>

#include <asm/mman.h>

#if defined(USE_HUGE_PAGES) && !defined(MAP_HUGETLB)
#error No hugetlb support?
#endif

#include "test.h"
#include "xutil.h"

#ifndef VMSPLICE_COOP
#ifdef USE_HUGE_PAGES
const char* test_name = "vmsplice_hugepage_pipe_thr";
#else
const char* test_name = "vmsplice_pipe_thr";
#endif
#else
#ifdef USE_HUGE_PAGES
const char* test_name = "vmsplice_hugepage_coop_pipe_thr";
#else
const char* test_name = "vmsplice_coop_pipe_thr";
#endif
#endif

int coop_reporting_chunk_size;

typedef struct {
  int fds[2];
  int ret_fds[2];
  int fin_fds[2];
  void* coop_buf;
  void* mapped;
  unsigned long write_offset;
  unsigned long bytes_written;
  unsigned long chunks_written;
  unsigned long chunks_read;
  unsigned long ring_size;
  unsigned long total_read;
  void* read_buf;
  struct iovec iov;
} pipe_state;

static void
init_test(test_data *td)
{
  pipe_state *ps = xmalloc(sizeof(pipe_state));
  if (pipe(ps->fds) == -1)
    err(1, "pipe");
  if (pipe(ps->fin_fds) == -1)
    err(1, "pipe");
  if (pipe(ps->ret_fds) == -1)
    err(1, "pipe");
  td->data = (void *)ps;
}

static void
init_child(test_data *td)
{
  pipe_state *ps = (pipe_state *)td->data;
  ps->total_read = 0;
  ps->read_buf = xmalloc(td->size);
  ps->iov.iov_base = ps->read_buf;
  ps->iov.iov_len = td->size;
}

static struct iovec* get_read_buffer(test_data* td, int len, int* n_vecs) {

  pipe_state *ps = (pipe_state *)td->data;
  xread(ps->fds[0], ps->read_buf, td->size);
  *n_vecs = 1;
  return &ps->iov;

}

static void release_read_buffer(test_data* td, struct iovec* vecs, int n_vecs) {

  pipe_state *ps = (pipe_state *)td->data;
  assert(vecs == &ps->iov && n_vecs == 1);

  ps->total_read += td->size;
#ifdef VMSPLICE_COOP
  while(ps->total_read >= coop_reporting_chunk_size) {
    xwrite(ps->ret_fds[1], &coop_reporting_chunk_size, sizeof(int));
    ps->total_read -= coop_reporting_chunk_size;
  }
#endif  
  
}

static void child_finish(test_data* td) {
  pipe_state *ps = (pipe_state *)td->data;  
  xwrite(ps->fin_fds[1], "X", 1);
}

#define ALLOC_PAGES 512
// 2MB == my L2 cache size / 2

static void
init_parent(test_data *td)
{
  pipe_state *ps = (pipe_state *)td->data;

  ps->coop_buf = xmalloc(4096);

  ps->mapped = 0;
  ps->write_offset = 0;
  ps->bytes_written = 0;
  ps->chunks_written = 0;
  ps->chunks_read = 0;
  ps->ring_size = ALLOC_PAGES * 4096;
}

static struct iovec*
get_write_buffer(test_data *td, int len, int* n_vecs) {

  pipe_state *ps = (pipe_state *)td->data;
  int map_condition = !ps->mapped;
#ifndef VMSPLICE_COOP
  map_condition = map_condition || ((ps->write_offset + td->size) > (ps->ring_size));
#endif
  if(map_condition) {
    if(ps->mapped)
      if(munmap(ps->mapped, ps->ring_size))
	err(1, "munmap");
    int flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE;
#ifdef USE_HUGE_PAGES
      flags |= MAP_HUGETLB;
#endif
    ps->mapped = mmap(0, ps->ring_size, PROT_WRITE | PROT_READ, flags, -1, 0);
    if(ps->mapped == MAP_FAILED)
      err(1, "mmap");
    ps->write_offset = 0;
  }
#ifdef VMSPLICE_COOP
  if(ps->write_offset >= ps->ring_size)
    ps->write_offset -= ps->ring_size;
  // Note we only do this now before starting the write, *not* during the write.
  // This opens the following opportunity for deadlock: whilst we're writing (vmsplicing) into the pipe,
  // the pipe buffer could fill. Whilst we're blocked waiting on the reader to clear the pipe, he might
  // write into the reporting pipe, which is also full - we wait for each other for want of a poll() call (but boo, more syscalls in the fast path).
  // This can't happen so long as the writer would *need* to reclaim tokens before possibly writing enough to cause the reader to fill the token buffer.
  // That is, the kernel pipe buffer size is large enough to contain sizeof(int) * (ring_size / reporting_chunk_size).
  while((ps->ring_size - ((ps->chunks_written - ps->chunks_read) * coop_reporting_chunk_size)) < td->size) {
    int rep_bytes = read(ps->ret_fds[0], ps->coop_buf, 4096);
    assert(rep_bytes % 4 == 0);
    int i;
    int* int_buf = (int*)ps->coop_buf;
    for(i = 0; i < rep_bytes/4; i++) {
      if(int_buf[0] != coop_reporting_chunk_size)
	err(1, "Bad chunk");
      ps->chunks_read++;
    }
  }
#endif
  ps->iov.iov_base = ps->mapped + ps->write_offset;
  ps->iov.iov_len = td->size;

  *n_vecs = 1;
  return &ps->iov;
}

static void release_write_buffer(test_data* td, struct iovec* vecs, int n_vecs) {

  pipe_state *ps = (pipe_state *)td->data;
  assert(n_vecs == 1);

#ifdef VMSPLICE_COOP
  ps->bytes_written += td->size;
  while(ps->bytes_written >= coop_reporting_chunk_size) {
    ps->chunks_written++;
    ps->bytes_written -= coop_reporting_chunk_size;
  }
#endif
  ps->write_offset += td->size;
  while(vecs[0].iov_len > 0) {
    ssize_t this_write = vmsplice(ps->fds[1], vecs, n_vecs, 0);
    if(this_write < 0)
      err(1, "vmsplice");
    else if(this_write == 0)
      break;
    vecs[0].iov_len -= this_write;
    vecs[0].iov_base = ((char*)vecs[0].iov_base) + this_write;
  }

}

static void parent_finish(test_data* td) {

  char buf;
  pipe_state *ps = (pipe_state *)td->data;
  xread(ps->fin_fds[0], &buf, 1);

}

int
main(int argc, char *argv[])
{
#ifdef VMSPLICE_COOP
  char* chunk_str = getenv("VMSPLICE_COOP_CHUNK");
  if(chunk_str) {
    char* str_end;
    coop_reporting_chunk_size = strtol(chunk_str, &str_end, 10);
    if(str_end[0]) {
      err(1, "VMSPLICE_COOP_CHUNK must be an integer");
    }
  }
  else {
    coop_reporting_chunk_size = 1024*1024;
  }
  printf("Cooperative reporting: chunk size %dK\n", coop_reporting_chunk_size/1024);
#endif

  test_t t = { 
    .name = test_name,
    .is_latency_test = 0,
    .init_test = init_test,
    .init_parent = init_parent,
    .finish_parent = parent_finish,
    .init_child = init_child,
    .finish_child = child_finish,
    .get_write_buffer = get_write_buffer,
    .release_write_buffer = release_write_buffer,
    .get_read_buffer = get_read_buffer,
    .release_read_buffer = release_read_buffer
  };
  run_test(argc, argv, &t);
  return 0;
}
