/* 
    Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>

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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <errno.h>
#include <assert.h>

#include "test.h"
#include "xutil.h"

typedef struct {
  int fds[2];
  int fin_fds[2];
  struct iovec buffer;
} pipe_state;

static void
init_test(test_data *td)
{
  pipe_state *ps = xmalloc(sizeof(pipe_state));
  if (pipe(ps->fds) == -1)
    err(1, "pipe");
  if (pipe(ps->fin_fds) == -1)
    err(1, "pipe");
  td->data = (void *)ps;
}

static void
init_local(test_data *td)
{
  pipe_state *ps = (pipe_state*)td->data;
  ps->buffer.iov_base = xmalloc(td->size);
  ps->buffer.iov_len = td->size;
}

static struct iovec*
get_read_buf(test_data *td, int len, int* n_vecs)
{
  pipe_state *ps = (pipe_state *)td->data;
  xread(ps->fds[0], ps->buffer.iov_base, len);
  *n_vecs = 1;
  return &ps->buffer;
}

static void
release_read_buf(test_data *td, struct iovec* vecs, int n_vecs) {
  pipe_state *ps = (pipe_state *)td->data;  
  assert(n_vecs == 1);
  assert(vecs == &ps->buffer);
}

static void child_fin(test_data *td) {

  pipe_state *ps = (pipe_state *)td->data;  
  xwrite(ps->fin_fds[1], "X", 1);

}

static struct iovec* get_write_buf(test_data *td, int len, int* n_vecs) {
  pipe_state *ps = (pipe_state *)td->data;
  assert(len == td->size);
  *n_vecs = 1;
  return &ps->buffer;
}

static void
release_write_buf(test_data *td, struct iovec* vecs, int n_vecs)
{
  pipe_state *ps = (pipe_state *)td->data;
  assert(vecs == &ps->buffer && n_vecs == 1);
  xwrite(ps->fds[1], vecs[0].iov_base, vecs[0].iov_len);
}

static void parent_fin(test_data *td) {
  pipe_state *ps = (pipe_state*)td->data;
  xread(ps->fin_fds[0], ps->buffer.iov_base, 1);
}

int
main(int argc, char *argv[])
{
  test_t t = { 
    .name = "pipe_thr",
    .is_latency_test = 0,
    .init_test = init_test,
    .init_parent = init_local,
    .finish_parent = parent_fin,
    .init_child = init_local,
    .finish_child = child_fin,
    .get_write_buffer = get_write_buf,
    .release_write_buffer = release_write_buf,
    .get_read_buffer = get_read_buf,
    .release_read_buffer = release_read_buf
  };
  run_test(argc, argv, &t);
  return 0;
}
