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
#include <errno.h>
#include <netinet/tcp.h>

#include "test.h"
#include "xutil.h"

struct tcp_state {
  struct addrinfo* info;
  int fd;
  void* buf;
};

static void
init_test(test_data *td)
{
  int ret;
  char portbuf[32];
  struct addrinfo hints;
  struct tcp_state* ts = (struct tcp_state*)xmalloc(sizeof(struct tcp_state));
  td->data = ts;
  int port = 3490+td->num;
  snprintf(portbuf, sizeof portbuf, "%d", port);
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((ret = getaddrinfo("127.0.0.1", portbuf, &hints, (struct addrinfo **)&ts->info)) != 0)
    errx(1, "getaddrinfo: %s\n", gai_strerror(ret));
}

static void
child_init(test_data *td)
{
  int sockfd, new_fd, i;
  struct tcp_state* ts = (struct tcp_state*)td->data;
  ts->buf = xmalloc(td->size);
  struct addrinfo *res = ts->info;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
 
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    err(1, "socket");

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) == -1)
    err(1, "setsockopt");
    
  while (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    { }
    
  if (listen(sockfd, 1) == -1)
    err(1, "listen");
    
  addr_size = sizeof their_addr;
    
  if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1)
    err(1, "accept");

#ifdef USE_NODELAY
  int flag = 1;
  if (setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) == -1)
    err(1, "setsockopt");
#endif

  ts->fd = new_fd;
}

static void child_ping(test_data* td) {
  struct tcp_state* ts = (struct tcp_state*)td->data;
  xread(ts->fd, ts->buf, td->size);
  xwrite(ts->fd, ts->buf, td->size);
}

static void
init_parent(test_data *td)
{
  struct tcp_state* ts = (struct tcp_state*)td->data;
  ts->buf = xmalloc(td->size);
  struct addrinfo *res = ts->info;
  int sockfd;
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    err(1, "socket");
    
  while (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    { }

#ifdef USE_NODELAY
  int flag = 1;
  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) == -1)
    err(1, "setsockopt");
#endif

  ts->fd = sockfd;
}

static void parent_ping(test_data* td) {
  struct tcp_state* ts = (struct tcp_state*)td->data;
  xwrite(ts->fd, ts->buf, td->size);
  xread(ts->fd, ts->buf, td->size);
}

int
main(int argc, char *argv[])
{
  test_t t = { 
#ifdef USE_NODELAY
    .name = "tcp_nodelay_lat", 
#else
    .name = "tcp_lat", 
#endif
    .is_latency_test = 1,
    .init_test = init_test, 
    .init_parent = init_parent,
    .init_child = child_init,
    .parent_ping = parent_ping,
    .child_ping = child_ping
};
  run_test(argc, argv, &t);
  return 0;
}
