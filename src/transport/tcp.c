/*
 * PeerHayes — TCP transport (listen, dial, VMCP framing, raw online mode)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/tcp.h"
#include "vmodem/log.h"
#include "vmodem/modem.h"
#include "vmodem/vmcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET vmod_sock_t;
#define VMOD_INVALID_SOCK INVALID_SOCKET
#define VMOD_SOCK_ERR SOCKET_ERROR
#define vmod_sock_close closesocket
#define vmod_sock_errno WSAGetLastError()
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int vmod_sock_t;
#define VMOD_INVALID_SOCK (-1)
#define VMOD_SOCK_ERR (-1)
#define vmod_sock_close close
#define vmod_sock_errno errno
#endif

typedef struct {
  vmod_sock_t listen_fd;
  vmod_sock_t conn_fd;
  char rx[4096];
  size_t rx_len;
  int net_ready;
} tcp_impl_t;

static int g_net_inited = 0;

vmod_status_t vmod_net_init(void) {
#ifdef _WIN32
  WSADATA wsa;
  if (g_net_inited)
    return VMOD_OK;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    return VMOD_ERR_IO;
#endif
  g_net_inited = 1;
  return VMOD_OK;
}

void vmod_net_shutdown(void) {
#ifdef _WIN32
  if (g_net_inited)
    WSACleanup();
#endif
  g_net_inited = 0;
}

static int set_nonblock(vmod_sock_t fd) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int parse_host_port(const char *ep, char *host, size_t host_sz, int *port) {
  const char *colon;
  size_t hlen;
  if (!ep || !host || !port)
    return -1;
  /* [ipv6]:port not supported in MVP; host:port or :port / 0.0.0.0:port */
  colon = strrchr(ep, ':');
  if (!colon)
    return -1;
  hlen = (size_t)(colon - ep);
  if (hlen >= host_sz)
    return -1;
  if (hlen == 0)
    snprintf(host, host_sz, "0.0.0.0");
  else {
    memcpy(host, ep, hlen);
    host[hlen] = '\0';
  }
  *port = atoi(colon + 1);
  return (*port > 0 && *port < 65536) ? 0 : -1;
}

static void close_conn(tcp_impl_t *impl) {
  if (impl->conn_fd != VMOD_INVALID_SOCK) {
    vmod_sock_close(impl->conn_fd);
    impl->conn_fd = VMOD_INVALID_SOCK;
  }
  impl->rx_len = 0;
}

static vmod_status_t tcp_listen(vmod_transport_t *t, const char *bind_ep) {
  tcp_impl_t *impl = (tcp_impl_t *)t->impl;
  char host[128];
  int port = 0;
  struct addrinfo hints, *res = NULL, *ai;
  char portstr[16];
  int yes = 1;

  if (parse_host_port(bind_ep, host, sizeof(host), &port) != 0)
    return VMOD_ERR_INVAL;

  if (impl->listen_fd != VMOD_INVALID_SOCK) {
    vmod_sock_close(impl->listen_fd);
    impl->listen_fd = VMOD_INVALID_SOCK;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  snprintf(portstr, sizeof(portstr), "%d", port);
  if (getaddrinfo(strcmp(host, "0.0.0.0") == 0 ? NULL : host, portstr, &hints,
                  &res) != 0)
    return VMOD_ERR_IO;

  for (ai = res; ai; ai = ai->ai_next) {
    impl->listen_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (impl->listen_fd == VMOD_INVALID_SOCK)
      continue;
    setsockopt(impl->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes,
               sizeof(yes));
    if (bind(impl->listen_fd, ai->ai_addr, (int)ai->ai_addrlen) == 0)
      break;
    vmod_sock_close(impl->listen_fd);
    impl->listen_fd = VMOD_INVALID_SOCK;
  }
  freeaddrinfo(res);
  if (impl->listen_fd == VMOD_INVALID_SOCK)
    return VMOD_ERR_IO;
  if (listen(impl->listen_fd, 4) != 0) {
    vmod_sock_close(impl->listen_fd);
    impl->listen_fd = VMOD_INVALID_SOCK;
    return VMOD_ERR_IO;
  }
  set_nonblock(impl->listen_fd);
  t->connected = 0;
  t->control_phase = 1;
  VMOD_LOGI("TCP listening on %s", bind_ep);
  return VMOD_OK;
}

static vmod_status_t tcp_dial(vmod_transport_t *t, const char *endpoint) {
  tcp_impl_t *impl = (tcp_impl_t *)t->impl;
  char host[128];
  int port = 0;
  struct addrinfo hints, *res = NULL, *ai;
  char portstr[16];

  if (parse_host_port(endpoint, host, sizeof(host), &port) != 0)
    return VMOD_ERR_INVAL;

  close_conn(impl);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  snprintf(portstr, sizeof(portstr), "%d", port);
  if (getaddrinfo(host, portstr, &hints, &res) != 0)
    return VMOD_ERR_IO;

  for (ai = res; ai; ai = ai->ai_next) {
    impl->conn_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (impl->conn_fd == VMOD_INVALID_SOCK)
      continue;
    if (connect(impl->conn_fd, ai->ai_addr, (int)ai->ai_addrlen) == 0)
      break;
    vmod_sock_close(impl->conn_fd);
    impl->conn_fd = VMOD_INVALID_SOCK;
  }
  freeaddrinfo(res);
  if (impl->conn_fd == VMOD_INVALID_SOCK) {
    VMOD_LOGW("TCP connect failed: %s", endpoint);
    return VMOD_ERR_IO;
  }
  set_nonblock(impl->conn_fd);
  {
    int one = 1;
    setsockopt(impl->conn_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one,
               sizeof(one));
  }
  t->connected = 1;
  t->control_phase = 1;
  impl->rx_len = 0;
  VMOD_LOGI("TCP connected to %s", endpoint);
  return VMOD_OK;
}

static vmod_status_t tcp_send(vmod_transport_t *t, const uint8_t *data,
                              size_t len) {
  tcp_impl_t *impl = (tcp_impl_t *)t->impl;
  size_t off = 0;
  if (!t->connected || impl->conn_fd == VMOD_INVALID_SOCK)
    return VMOD_ERR_IO;
  while (off < len) {
    int n = send(impl->conn_fd, (const char *)data + off, (int)(len - off), 0);
    if (n == VMOD_SOCK_ERR) {
#ifdef _WIN32
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK)
        continue;
#else
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
#endif
      return VMOD_ERR_IO;
    }
    if (n == 0)
      return VMOD_ERR_IO;
    off += (size_t)n;
  }
  return VMOD_OK;
}

static void tcp_close(vmod_transport_t *t) {
  tcp_impl_t *impl = (tcp_impl_t *)t->impl;
  int was = t->connected;
  close_conn(impl);
  t->connected = 0;
  t->control_phase = 1;
  (void)was;
}

static void process_rx(vmod_transport_t *t) {
  tcp_impl_t *impl = (tcp_impl_t *)t->impl;
  size_t i;
  size_t len;

  if (!t->modem || impl->rx_len == 0)
    return;

  if (!t->control_phase) {
    vmod_modem_on_peer_data(t->modem, (const uint8_t *)impl->rx, impl->rx_len);
    impl->rx_len = 0;
    return;
  }

  /*
   * VMCP handlers (NOANSWER / BUSY / HANGUP) may call tcp_close(), which
   * zeroes rx_len via close_conn(). Snapshot length and abort safely if the
   * connection disappears mid-parse — otherwise size_t underflow in the
   * memmove tail corrupts the heap (seen as segfault after unanswered calls).
   */
  len = impl->rx_len;
  i = 0;
  while (i < len) {
    size_t start = i;
    while (i < len && impl->rx[i] != '\r')
      i++;
    if (i >= len)
      break;
    vmod_modem_handle_vmcp_line(t->modem, impl->rx + start, i - start);
    if (!t->connected || impl->conn_fd == VMOD_INVALID_SOCK) {
      impl->rx_len = 0;
      return;
    }
    i++;
    if (i < len && impl->rx[i] == '\n')
      i++;
    if (!t->control_phase) {
      size_t rem = len - i;
      if (rem)
        vmod_modem_on_peer_data(t->modem, (const uint8_t *)impl->rx + i, rem);
      impl->rx_len = 0;
      return;
    }
    len = impl->rx_len;
  }
  if (i > 0 && i <= impl->rx_len) {
    memmove(impl->rx, impl->rx + i, impl->rx_len - i);
    impl->rx_len -= i;
  } else if (i > impl->rx_len) {
    impl->rx_len = 0;
  }
}

static void tcp_poll(vmod_transport_t *t) {
  tcp_impl_t *impl = (tcp_impl_t *)t->impl;
  fd_set rfds;
  struct timeval tv;
#ifndef _WIN32
  int maxfd = 0;
#endif
  int rc;

  FD_ZERO(&rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if (impl->listen_fd != VMOD_INVALID_SOCK) {
    FD_SET(impl->listen_fd, &rfds);
#ifndef _WIN32
    if ((int)impl->listen_fd > maxfd)
      maxfd = (int)impl->listen_fd;
#endif
  }
  if (impl->conn_fd != VMOD_INVALID_SOCK) {
    FD_SET(impl->conn_fd, &rfds);
#ifndef _WIN32
    if ((int)impl->conn_fd > maxfd)
      maxfd = (int)impl->conn_fd;
#endif
  }

  rc = select(
#ifdef _WIN32
      0,
#else
      maxfd + 1,
#endif
      &rfds, NULL, NULL, &tv);
  if (rc <= 0)
    return;

  if (impl->listen_fd != VMOD_INVALID_SOCK &&
      FD_ISSET(impl->listen_fd, &rfds)) {
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    vmod_sock_t cli =
        accept(impl->listen_fd, (struct sockaddr *)&addr, &alen);
    if (cli != VMOD_INVALID_SOCK) {
      if (impl->conn_fd != VMOD_INVALID_SOCK ||
          (t->modem && t->modem->state != VMOD_STATE_IDLE)) {
        VMOD_LOGW("TCP reject inbound (busy)");
        vmod_sock_close(cli);
      } else {
        int one = 1;
        impl->conn_fd = cli;
        set_nonblock(impl->conn_fd);
        setsockopt(impl->conn_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one,
                   sizeof(one));
        t->connected = 1;
        t->control_phase = 1;
        impl->rx_len = 0;
        VMOD_LOGI("TCP inbound accepted");
      }
    }
  }

  if (impl->conn_fd != VMOD_INVALID_SOCK && FD_ISSET(impl->conn_fd, &rfds)) {
    char buf[1024];
    int n = recv(impl->conn_fd, buf, sizeof(buf), 0);
    if (n == 0 || n == VMOD_SOCK_ERR) {
#ifdef _WIN32
      if (n == VMOD_SOCK_ERR && WSAGetLastError() == WSAEWOULDBLOCK)
        return;
#else
      if (n == VMOD_SOCK_ERR && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;
#endif
      VMOD_LOGI("TCP peer closed");
      close_conn(impl);
      t->connected = 0;
      t->control_phase = 1;
      if (t->modem)
        vmod_modem_on_transport_closed(t->modem);
      return;
    }
    if (impl->rx_len + (size_t)n > sizeof(impl->rx))
      n = (int)(sizeof(impl->rx) - impl->rx_len);
    if (n > 0) {
      memcpy(impl->rx + impl->rx_len, buf, (size_t)n);
      impl->rx_len += (size_t)n;
      process_rx(t);
    }
  }
}

static const vmod_transport_ops_t g_tcp_ops = {tcp_listen, tcp_dial, tcp_send,
                                               tcp_close, tcp_poll};

vmod_status_t vmod_tcp_transport_create(vmod_transport_t **out) {
  vmod_transport_t *t;
  tcp_impl_t *impl;
  if (!out)
    return VMOD_ERR_INVAL;
  if (vmod_net_init() != VMOD_OK)
    return VMOD_ERR_IO;
  t = (vmod_transport_t *)calloc(1, sizeof(*t));
  impl = (tcp_impl_t *)calloc(1, sizeof(*impl));
  if (!t || !impl) {
    free(t);
    free(impl);
    return VMOD_ERR_NOMEM;
  }
  impl->listen_fd = VMOD_INVALID_SOCK;
  impl->conn_fd = VMOD_INVALID_SOCK;
  t->ops = &g_tcp_ops;
  t->impl = impl;
  t->control_phase = 1;
  *out = t;
  return VMOD_OK;
}

void vmod_tcp_transport_destroy(vmod_transport_t *t) {
  tcp_impl_t *impl;
  if (!t)
    return;
  impl = (tcp_impl_t *)t->impl;
  if (impl) {
    close_conn(impl);
    if (impl->listen_fd != VMOD_INVALID_SOCK)
      vmod_sock_close(impl->listen_fd);
    free(impl);
  }
  free(t);
}
