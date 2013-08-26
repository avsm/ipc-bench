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
#include <assert.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "test.h"
#include "xutil.h"

struct tcp_state {
  struct addrinfo* info;
  int fd;
  struct iovec buffer;
};

static void
init_test(test_data *td)
{
  int ret;
  char portbuf[32];
  struct addrinfo hints;
  int port = 3490+td->num;

  struct tcp_state* state = xmalloc(sizeof(struct tcp_state));
  td->data = state;

  snprintf(portbuf, sizeof portbuf, "%d", port);
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((ret = getaddrinfo("127.0.0.1", portbuf, &hints, (struct addrinfo **)&state->info)) != 0)
    errx(1, "getaddrinfo: %s\n", gai_strerror(ret));
}

static void init_local(test_data* td, struct tcp_state* state) {
  state->buffer.iov_base = xmalloc(td->size);
  state->buffer.iov_len = td->size;
}

static void
init_child(test_data *td)
{
  int sockfd;
  struct tcp_state* state = (struct tcp_state*)td->data;
  struct addrinfo *res = state->info;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;

  init_local(td, state);
 
  int i = 1;

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    err(1, "socket");

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) == -1)
    err(1, "setsockopt");

#ifdef USE_NODELAY
  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) == -1)
    err(1, "setsockopt");
#endif
    
  while (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    { }
    
  if (listen(sockfd, 1) == -1)
    err(1, "listen");
    
  addr_size = sizeof their_addr;
    
  if ((state->fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1)
    err(1, "accept");
}

static struct iovec*
get_read_buf(test_data *td, int len, int* n_vecs)
{
  struct tcp_state *ps = (struct tcp_state *)td->data;
  xread(ps->fd, ps->buffer.iov_base, len);
  *n_vecs = 1;
  return &ps->buffer;
}

static void
release_read_buf(test_data *td, struct iovec* vecs, int n_vecs) {
  struct tcp_state *ps = (struct tcp_state *)td->data;  
  assert(n_vecs == 1);
  assert(vecs == &ps->buffer);
}

static struct iovec* get_write_buf(test_data *td, int len, int* n_vecs) {
  struct tcp_state *ps = (struct tcp_state *)td->data;
  assert(len == td->size);
  *n_vecs = 1;
  return &ps->buffer;
}

static void
release_write_buf(test_data *td, struct iovec* vecs, int n_vecs)
{
  struct tcp_state *ps = (struct tcp_state *)td->data;
  assert(vecs == &ps->buffer && n_vecs == 1);
  xwrite(ps->fd, vecs[0].iov_base, vecs[0].iov_len);
}

static void child_finish(test_data* td) {
  struct tcp_state* state = (struct tcp_state*)td->data;
  xwrite(state->fd, "X", 1);
}

static void
init_parent(test_data *td)
{
  struct tcp_state* state = (struct tcp_state*)td->data;
  struct addrinfo *res = state->info;
  int sockfd;

  init_local(td, state);

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    err(1, "socket");
    
  while (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    { }

#ifdef USE_NODELAY
  int i = 1;
  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int)) == -1)
    err(1, "setsockopt");
#endif

  state->fd = sockfd;
}

static void parent_finish(test_data* td) {
  struct tcp_state* state = (struct tcp_state*)td->data;
  xread(state->fd, state->buffer.iov_base, 1);
}

int
main(int argc, char *argv[])
{
  test_t t = { 
    .name = "tcp_"
    #ifdef USE_NODELAY
    "nodelay_"
    #endif
    "thr"
    ,
    .is_latency_test = 0,
    .init_test = init_test,
    .init_parent = init_parent,
    .finish_parent = parent_finish,
    .init_child = init_child,
    .finish_child = child_finish,
    .get_write_buffer = get_write_buf,
    .release_write_buffer = release_write_buf,
    .get_read_buffer = get_read_buf,
    .release_read_buffer = release_read_buf
  };
  run_test(argc, argv, &t);
  return 0;
}
