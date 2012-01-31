/* 
    Measure latency of IPC using tcp sockets

    Copyright (c) 2011 Steven Smith <sos22@cam.ac.uk>

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

/* Simple shared-memory IPC throughput test, with spin-waits rather
   than blocking.  The shared area is divided into packets, each of
   which starts with a header.  The header is the size of a cache
   line, but the only really interesting field is the size of the
   packet.  To transmit, you just copy the data you want to transmit
   it and then go back and write the header.  Meanwhile, the receiver
   will be sitting spinning waiting for the header to arrive, and will
   read the packet out as soon as it does.  Once it's done so, it'll
   go back and set the finished flag in the header, so that the
   transmitter knows when it's safe to reuse a bit of buffer for a new
   message.  The benchmark which we build on top of this just keeps
   sending fixed-sized messages for soem number of times, waiting as
   appropriate for the receiver to pick them up. */

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <linux/futex.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#undef USE_MWAIT
#ifndef NO_FUTEX
#define USE_FUTEX
#endif

#include "test.h"
#include "xutil.h"
#include "futex.h"

#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
static unsigned nr_shared_pages = 512;
#define ring_size (PAGE_SIZE * nr_shared_pages)

struct msg_header {
#define MH_FLAG_READY 1
#define MH_FLAG_STOP 2
#define MH_FLAG_WAITING 4
#define MH_FLAGS (MH_FLAG_READY|MH_FLAG_STOP|MH_FLAG_WAITING)
  unsigned size_and_flags;
  int pad[CACHE_LINE_SIZE / sizeof(int) - 1];
};

struct ring_state {
  void* ringmem;
  unsigned long next_tx_offset;
  unsigned long first_unacked_msg;
  unsigned long next_message_start;
  struct iovec vecs[2];
};

static unsigned long
mask_ring_index(unsigned long idx)
{
  return idx & (ring_size - 1);
}

static void
init_test(test_data *td)
{
  struct ring_state* rs = (struct ring_state*)xmalloc(sizeof(struct ring_state));
  rs->ringmem = establish_shm_segment(nr_shared_pages, td->numa_node);
  td->data = rs;
}

#ifdef USE_MWAIT
static void
mwait(void)
{
  asm volatile("mwait\n"
	       :
	       : "a" (0), "c" (0));
}
static void
monitor(const volatile void *what)
{
  asm volatile("monitor\n"
	       :
	       : "a" (what),
		 "c" (0),
		 "d" (0)
	       );
}
#else
static void
mwait(void)
{
}
static void
monitor(const volatile void *what)
{
}
#endif

/* Deferred write state */
#ifdef USE_FUTEX
static struct {
  volatile unsigned *ptr;
  unsigned val;
  unsigned cntr;
} deferred_write;

static void
_set_message_ready(volatile unsigned *ptr, unsigned val)
{
  int sz;
  sz = atomic_xchg(ptr, val);
  if (sz & MH_FLAG_WAITING)
    futex_wake(ptr);
}
#endif

static int
wait_for_message_ready(volatile struct msg_header *mh, int desired_state)
{
  int sz;
#ifdef USE_FUTEX
  int new_sz;
  if (deferred_write.ptr) {
    _set_message_ready(deferred_write.ptr, deferred_write.val);
    deferred_write.ptr = NULL;
  }
  while (1) {
    sz = mh->size_and_flags;
    if ((sz & MH_FLAG_READY) == desired_state)
      break;
    new_sz = sz | MH_FLAG_WAITING;
    if (new_sz == sz ||
	atomic_cmpxchg(&mh->size_and_flags, sz, new_sz) == sz)
      futex_wait_while_equal(&mh->size_and_flags, new_sz);
  }
#else
  sz = mh->size_and_flags;
  if ((sz & MH_FLAG_READY) != desired_state) {
    while (1) {
      monitor(&mh->size_and_flags);
      sz = mh->size_and_flags;
      if ((sz & MH_FLAG_READY) == desired_state)
	break;
      mwait();
    }
  }
#endif
  return sz;
}

static void
set_message_ready(volatile struct msg_header *mh, int size)
{
#ifdef USE_FUTEX
  if (deferred_write.ptr) {
    deferred_write.cntr--;
    mh->size_and_flags = size;
    if (!deferred_write.cntr) {
      _set_message_ready(deferred_write.ptr, deferred_write.val);
      deferred_write.ptr = NULL;
    }
  } else {
    deferred_write.ptr = &mh->size_and_flags;
    deferred_write.val = size;
    deferred_write.cntr = 2;
  }
#else
  mh->size_and_flags = size;
#endif
}

static void
init_child(test_data *td)
{
  struct ring_state* rs = (struct ring_state*)td->data;
  volatile struct msg_header *mh = rs->ringmem;

  /* Sync up with parent */
  mh->size_and_flags = 0xf008;
  while (mh->size_and_flags == 0xf008)
    ;

  rs->next_message_start = 0;
  /* Enter main message loop */
}

static struct iovec* get_read_buffer(test_data* td, int len, int* n_vecs) {

  struct ring_state* rs = (struct ring_state*)td->data;
  volatile struct msg_header *mh;
  int sz;
  assert(rs->next_message_start % CACHE_LINE_SIZE == 0);
  mh = rs->ringmem + mask_ring_index(rs->next_message_start);
  sz = wait_for_message_ready(mh, MH_FLAG_READY);
  if (sz & MH_FLAG_STOP) { /* End of test */
    *n_vecs = 0;
    return 0;
  }
  sz &= ~MH_FLAGS;
  if(sz != td->size) {
    exit(1);
  }
  unsigned long offset = mask_ring_index(rs->next_message_start + sizeof(struct msg_header));
  if (offset + td->size <= ring_size) {
    rs->vecs[0].iov_base = rs->ringmem + offset;
    rs->vecs[0].iov_len = td->size;
    *n_vecs = 1;
  }
  else {
    rs->vecs[0].iov_base = rs->ringmem + offset;
    rs->vecs[0].iov_len = ring_size - offset;
    rs->vecs[1].iov_base = rs->ringmem;
    rs->vecs[1].iov_len = td->size - (ring_size - offset);
    *n_vecs = 2;
  }

  return rs->vecs;

}

static void release_read_buffer(test_data* td, struct iovec* vecs, int n_vecs) {

  struct ring_state* rs = (struct ring_state*)td->data;
  volatile struct msg_header *mh = rs->ringmem + mask_ring_index(rs->next_message_start);
  
  set_message_ready(mh, td->size);

  rs->next_message_start += td->size + sizeof(struct msg_header);

}

static void child_finish(test_data* td) {

#ifdef USE_FUTEX
  if(deferred_write.ptr)
    _set_message_ready(deferred_write.ptr, deferred_write.val);
#endif

}

static void
init_parent(test_data *td)
{
  struct ring_state* rs = (struct ring_state*)td->data;
  volatile struct msg_header *mh = rs->ringmem;

  assert(td->size < ring_size - sizeof(struct msg_header));

  /* Wait for child to show up. */
  while (mh->size_and_flags != 0xf008)
    ;
  mh->size_and_flags = 0;

  rs->next_tx_offset = 0;
  rs->first_unacked_msg = 0;

  /* Round up to multiple of cache line size, for sanity. */
  td->size = td->size + CACHE_LINE_SIZE - 1;
  td->size -= td->size % CACHE_LINE_SIZE;
}

struct iovec*
get_write_buffer(test_data* td, int len, int* n_vecs) {

  struct ring_state* rs = (struct ring_state*)td->data;
  volatile struct msg_header *mh;

  assert(len == td->size);

  /* Check for available ring space (eom = end of message) */
  unsigned long eom = rs->next_tx_offset + td->size + sizeof(struct msg_header) * 2;
  while (eom - rs->first_unacked_msg > ring_size) {
    int size;
    mh = rs->ringmem + mask_ring_index(rs->first_unacked_msg);
    size = wait_for_message_ready(mh, 0);
    size &= ~MH_FLAGS;
    rs->first_unacked_msg += size + sizeof(struct msg_header);
  }

  unsigned long offset = mask_ring_index(rs->next_tx_offset + sizeof(struct msg_header));
  if (offset + td->size <= ring_size) {
    rs->vecs[0].iov_base = rs->ringmem + offset;
    rs->vecs[0].iov_len = td->size;
    *n_vecs = 1;
  } else {
    rs->vecs[0].iov_base = rs->ringmem + offset;
    rs->vecs[0].iov_len = ring_size - offset;
    rs->vecs[1].iov_base = rs->ringmem;
    rs->vecs[1].iov_len = td->size - (ring_size - offset);
    *n_vecs = 2;
  }

  return rs->vecs;

}

void release_write_buffer(test_data* td, struct iovec* vecs, int nvecs) {

  struct ring_state* rs = (struct ring_state*)td->data;
  volatile struct msg_header *mh;
  volatile struct msg_header *mh2;
  assert(vecs == rs->vecs);

  /* Send message */
  mh = rs->ringmem + mask_ring_index(rs->next_tx_offset);

  /* Make sure that the size field in the *next* message is clear
     before setting the size field in *this* message.  That makes
     sure that the receiver stops and spins in the right place,
     rather than wandering off into la-la land if it picks up a
     stale message. */
  mh2 = rs->ringmem + mask_ring_index(rs->next_tx_offset + td->size + sizeof(struct msg_header));
  mh2->size_and_flags = 0;
  
  set_message_ready(mh, td->size | MH_FLAG_READY);
  
  rs->next_tx_offset += td->size + sizeof(struct msg_header);

}

void parent_finish(test_data* td) {

  struct ring_state* rs = (struct ring_state*)td->data;
  volatile struct msg_header *mh;

  mh = rs->ringmem + mask_ring_index(rs->next_tx_offset);
  mh->size_and_flags = MH_FLAG_READY | MH_FLAG_STOP;
#ifdef USE_FUTEX
  futex_wake(&mh->size_and_flags);
#endif

  /* Wait for child to acknowledge receipt of all messages */
  while (rs->first_unacked_msg != rs->next_tx_offset) {
    int size;
    mh = rs->ringmem + mask_ring_index(rs->first_unacked_msg);
    size = wait_for_message_ready(mh, 0);
    size &= ~MH_FLAGS;
    rs->first_unacked_msg += size + sizeof(struct msg_header);
  }

}

#ifdef USE_MWAIT
static void
cpuid(int leaf, unsigned long *a, unsigned long *b, unsigned long *c, unsigned long *d)
{
  unsigned long _a, _b, _c, _d;
  asm ("cpuid"
       : "=a" (_a), "=b" (_b), "=c" (_c), "=d" (_d)
       : "0" (leaf)
       );
  if (a)
    *a = _a;
  if (b)
    *b = _b;
  if (c)
    *c = _c;
  if (d)
    *d = _d;
}

static void
check_monitor_line_size(void)
{
  unsigned long a, b, c;
  cpuid(5, &a, &b, NULL, NULL);
  assert(a == b);
  assert(a == CACHE_LINE_SIZE);
  cpuid(1, NULL, NULL, &c, NULL);
  printf("Available: %d\n", !!(c & (1 << 3)));
}
#else
static void
check_monitor_line_size(void)
{
}
#endif

int
main(int argc, char *argv[])
{
  test_t t = { 
    .name = "mempipe_"
#ifdef NO_FUTEX
    "spin_"
#endif
    "thr",
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
  char *ring_order = getenv("MEMPIPE_RING_ORDER");
  check_monitor_line_size();
  if (ring_order) {
    int as_int;
    if (sscanf(ring_order, "%d", &as_int) != 1)
      err(1, "MEMPIPE_RING_ORDER must be an integer");
    if (as_int < 0 || as_int > 15)
      errx(1, "MEMPIPE_RING_ORDER must be between 0 and 15");
    nr_shared_pages = 1 << as_int;
  }
  run_test(argc, argv, &t);
  return 0;
}
