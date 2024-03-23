/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "anet.h"

#include <arpa/inet.h>  // inet_aton
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>  // gethostbyname
#include <netinet/in.h>
#include <netinet/ip.h>  // superset of netinet/in.h
#include <netinet/tcp.h>  // option: TCP_NODELAY
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static void anetSetError(char *err, const char *fmt, ...) {
  va_list ap;
  if (!err) {
    return;
  }

  va_start(ap, fmt);
  vsnprintf(err, ANET_ERR_LEN, fmt, ap);
  va_end(ap);
}

#define ANET_CONNECT_NONE 0
#define ANET_CONNECT_NONBLOCK 1

static int anetTcpGenericConnect(char *err, char *addr, int port, int flags) {
  int s;
  int on = 1;
  struct sockaddr_in sa;

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    anetSetError(err, "creating socket: %s\n", strerror(errno));
    return ANET_ERR;
  }
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  if (inet_aton(addr, &sa.sin_addr) == 0) {
    struct hostent *he;
    he = gethostbyname(addr);
    if (he == NULL) {
      anetSetError(err, "can't resolve: %s\n", addr);
      close(s);
      return ANET_ERR;
    }
    memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
  }

  if (flags & ANET_CONNECT_NONBLOCK) {
    if (anetNonBlock(err, s) != ANET_OK) {
      return ANET_ERR;
    }
  }

  if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
    if (errno == EINPROGRESS && flags & ANET_CONNECT_NONBLOCK) {
      return s;
    }
    anetSetError(err, "connect: %s\n", strerror(errno));
    close(s);
    return ANET_ERR;
  }

  return s;
}

int anetTcpConnect(char *err, char *addr, int port) {
  return anetTcpGenericConnect(err, addr, port, ANET_CONNECT_NONE);
}

int anetTcpNonBlockConnect(char *err, char *addr, int port) {
  return anetTcpGenericConnect(err, addr, port, ANET_CONNECT_NONBLOCK);
}

// Like read(2) but make sure 'count' is read before to return
// (unless error or EOF condition is encountered)
int anetRead(int fd, void *buf, int count) {
  int nread = 0;
  int total_len = 0;

  while (total_len != count) {
    nread = read(fd, buf, count - total_len);
    if (nread == 0) {
      return total_len;
    }
    if (nread == -1) {
      return -1;
    }
    total_len += nread;
    buf += nread;
  }
  return total_len;
}

// Like write(2) but make sure 'count' is read before to return
// (unless error is encountered)
int anetWrite(int fd, void *buf, int count) {
  int nwritten = 0;
  int total_len = 0;

  while (total_len != count) {
    nwritten = write(fd, buf, count - total_len);
    if (nwritten == 0) {
      return total_len;
    }
    if (nwritten == -1) {
      return -1;
    }
    total_len += nwritten;
    buf += nwritten;
  }
  return total_len;
}

int anetResolve(char *err, char *host, char *ip_buf) {
  struct sockaddr_in sa;

  sa.sin_family = AF_INET;
  if (inet_aton(host, &sa.sin_addr) == 0) {
    struct hostent *he;
    // NOTE: gethostbyname is obsolete.
    he = gethostbyname(host);
    if (he == NULL) {
      anetSetError(err, "can't resolve: %s\n", host);
      return ANET_ERR;
    }
    memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
  }
  strcpy(ip_buf, inet_ntoa(sa.sin_addr));
  return ANET_OK;
}

int anetTcpServer(char *err, int port, char *bind_addr) {
  int s = -1;
  int on = 1;
  struct sockaddr_in sa;

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    anetSetError(err, "socket: %s\n", strerror(errno));
    return ANET_ERR;
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
    anetSetError(err, "setsockopt SO_REUSEADDR: %s\n", strerror(errno));
    return ANET_ERR;
  }

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind_addr) {
    if (inet_aton(bind_addr, &sa.sin_addr) == 0) {
      anetSetError(err, "Invalid bind address\n");
      close(s);
      return ANET_ERR;
    }
  }

  if (bind(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
    anetSetError(err, "bind: %s\n", strerror(errno));
    close(s);
    return ANET_ERR;
  }

  if (listen(s, 32) == -1) {
    anetSetError(err, "listen: %s\n", strerror(errno));
    close(s);
    return ANET_ERR;
  }

  return s;
}

int anetAccept(char *err, int sock, char *ip, int *port) {
  int fd;
  struct sockaddr_in sa;
  unsigned int sa_len;

  while (1) {
    sa_len = sizeof(sa);
    fd = accept(sock, (struct sockaddr*)&sa, &sa_len);
    if (fd == -1) {
      if (errno == EAGAIN) {
        continue;
      } else {
        anetSetError(err, "accept: %s\n", strerror(errno));
        return ANET_ERR;
      }
    }
    break;
  }

  if (ip) {
    strcpy(ip, inet_ntoa(sa.sin_addr));
  }

  if (port) {
    *port = ntohs(sa.sin_port);
  }

  return fd;
}

int anetNonBlock(char *err, int fd) {
  int flags;

  // Set the socket nonblocking.
  if ((flags = fcntl(fd, F_GETFL)) == -1) {
    anetSetError(err, "fcntl(F_GETFL): %s\n", strerror(errno));
    return ANET_ERR;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    anetSetError(err, "fcntl(F_SETFL, O_NONBLOCK): %s\n", strerror(errno));
    return ANET_ERR;
  }
  return ANET_OK;
}

int anetTcpNoDelay(char *err, int fd) {
  int yes = 1;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) {
    anetSetError(err, "setsockopt TCO_NODELAY: %s\n", strerror(errno));
    return ANET_ERR;
  }
  return ANET_OK;
}

int anetTcpKeepAlive(char *err, int fd) {
  int yes = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) == -1) {
    anetSetError(err, "setsockopt SO_KEEPALIVE: %s\n", strerror(errno));
    return ANET_ERR;
  }
  return ANET_OK;
}
