/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "server/client.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "event/ae.h"
#include "memory/zmalloc.h"
#include "net/anet.h"
#include "server/server.h"
#include "utils/log.h"

static int AddReply(CutisClient *c);
static int ReadQueryFromClient(AeEventLoop *event_loop, int fd,
                               void *client_data, int mask);
static int SendReplyToClient(AeEventLoop *event_loop, int fd,
                             void *client_data, int mask);

CutisClient *CreateClient(CutisServer *server, int fd) {
  CutisClient *c = zmalloc(sizeof(*c));
  anetNonBlock(NULL, fd);
  anetTcpNoDelay(NULL, fd);
  if (!c) {
    return NULL;
  }

  c->fd = fd;
  c->query_buf = sdsempty();
  c->server = server;
  if (AeCreateFileEvent(server->el, c->fd, AE_READABLE, ReadQueryFromClient,
                        c, NULL) == AE_ERR) {
    FreeClient(c);
    return NULL;
  }

  return c;
}

void FreeClient(CutisClient *c) {
  AeDeleteFileEvent(c->server->el, c->fd, AE_READABLE);
  AeDeleteFileEvent(c->server->el, c->fd, AE_WRITABLE);
  close(c->fd);
  sdsfree(c->query_buf);
  zfree(c);
}

static int AddReply(CutisClient *c) {
  if (AeCreateFileEvent(c->server->el, c->fd, AE_WRITABLE,
                        SendReplyToClient, c, NULL) == AE_ERR) {
    return AE_ERR;
  }
  return AE_OK;
}

static int ReadQueryFromClient(AeEventLoop *event_loop, int fd,
                               void *client_data, int mask) {
  CutisClient *c = (CutisClient*)client_data;
  int nread;
  char buf[CUTIS_QUERY_BUF_LEN];

  nread = read(fd, buf, CUTIS_QUERY_BUF_LEN);
  if (nread == -1) {
    if (errno == EAGAIN) {
      nread = 0;
    } else {
      CutisLog(CUTIS_DEBUG, "Reading from client: %s", strerror(errno));
      FreeClient(c);
      return AE_ERR;
    }
  } else if (nread == 0) {
    CutisLog(CUTIS_DEBUG, "Client closed connection. %d", c->fd);
    FreeClient(c);
    return AE_ERR;
  }

  if (nread) {
    c->query_buf = sdscatlen(c->query_buf, buf, nread);
  } else {
    return AE_ERR;
  }

  return AddReply(c);
}


static int SendReplyToClient(AeEventLoop *event_loop, int fd,
                             void *client_data, int mask) {
  CutisClient *c = (CutisClient*)client_data;
  int nwritten = 0;

  nwritten = write(c->fd, c->query_buf, sdslen(c->query_buf));
  if (nwritten == -1) {
    if (errno == EAGAIN) {
      nwritten = 0;
    } else {
      CutisLog(CUTIS_DEBUG, "Error writing to client: %s, %d", strerror(errno), errno);
      FreeClient(c);
      return AE_ERR;
    }
  }
  c->query_buf = sdsrange(c->query_buf, nwritten, -1);
  if (sdslen(c->query_buf) == 0) {
    AeDeleteFileEvent(event_loop, c->fd, AE_WRITABLE);
  }
  return AE_OK;
}
