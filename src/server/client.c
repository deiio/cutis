/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "server/client.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "event/ae.h"
#include "memory/zmalloc.h"
#include "net/anet.h"
#include "server/server.h"
#include "utils/log.h"

static int ReadQueryFromClient(AeEventLoop *event_loop, int fd,
                               void *client_data, int mask);
static int SendReplyToClient(AeEventLoop *event_loop, int fd,
                             void *client_data, int mask);
static void FreeClientArgv(CutisClient *c);

CutisClient *CreateClient(CutisServer *server, int fd) {
  CutisClient *c = zmalloc(sizeof(*c));

  anetNonBlock(NULL, fd);
  anetTcpNoDelay(NULL, fd);
  if (!c) {
    return NULL;
  }

  c->fd = fd;
  c->query_buf = sdsempty();
  c->last_interaction = time(NULL);
  c->argc = 0;
  c->bulk_len = -1;
  c->sent_len = 0;
  c->server = server;

  if ((c->reply = listCreate()) == NULL) {
    CutisOom("listCreate");
  }

  if (AeCreateFileEvent(server->el, c->fd, AE_READABLE, ReadQueryFromClient,
                        c, NULL) == AE_ERR) {
    FreeClient(c);
    return NULL;
  }

  if (!listAddNodeTail(server->clients, c)) {
    CutisOom("listAddNodeTail");
  }

  return c;
}

void FreeClient(CutisClient *c) {
  ListNode *ln;

  AeDeleteFileEvent(c->server->el, c->fd, AE_READABLE);
  AeDeleteFileEvent(c->server->el, c->fd, AE_WRITABLE);
  sdsfree(c->query_buf);
  listRelease(c->reply);
  FreeClientArgv(c);
  close(c->fd);

  ln = listSearchKey(c->server->clients, c);
  assert(ln != NULL);
  listDelNode(c->server->clients, ln);
  zfree(c);
}

void ResetClient(CutisClient *c) {
  FreeClientArgv(c);
  c->bulk_len = -1;
}

int AddReply(CutisClient *c) {
  if (AeCreateFileEvent(c->server->el, c->fd, AE_WRITABLE,
                        SendReplyToClient, c, NULL) == AE_ERR) {
    return AE_ERR;
  }
  return AE_OK;
}

int ParseQuery(CutisClient *c) {
  int res = CUTIS_ERR;
  do {
    if (c->bulk_len == -1) {
      res = ParseNonBulkQuery(c);
    } else {
      res = ParseBulkQuery(c);
    }
  } while (res == CUTIS_AGAIN);
  return res;
}

int ParseBulkQuery(CutisClient *c) {
  // Bulk read handling. Note that if we are at this point the
  // client already sent a command terminated with a newline.
  // We are reading the bulk data that is actually the last
  // argument of the command.
  int bulk_len = sdslen(c->query_buf);
  if (c->bulk_len <= bulk_len) {
    // Copy everything but the final CRLF as final argument
    sds d = sdsrange(c->query_buf, 0, c->bulk_len - 2);
    c->argv[c->argc] = CreateCutisObject(0, d);
    c->argc++;
    c->query_buf = sdsrange(c->query_buf, c->bulk_len, -1);
    // Execute the command. If the client is still valid after
    // ProcessCommand() return and there is something on the
    // query buffer try to process the next command.
    if (ProcessCommand(c) && sdslen(c->query_buf) > 0) {
      return CUTIS_AGAIN;
    }
  }
  return CUTIS_OK;
}

int ParseNonBulkQuery(CutisClient *c) {
  // Read the first line of the query
  char *p = strchr(c->query_buf, '\n');
  size_t query_len;

  if (p) {
    sds query;
    sds *argv;
    int argc;
    int i;

    query = c->query_buf;
    c->query_buf = sdsempty();
    query_len = 1 + (p - query);  // include the '\n'
    if (sdslen(query) > query_len) {
      // leave data after the first line of the query in the buffer
      c->query_buf = sdscatlen(c->query_buf, query + query_len,
                               sdslen(query) - query_len);
    }
    *p = '\0';  // remove '\n'
    if (*(p-1) == '\r') {
      *(p-1) = '\0';  // remove '\r'
    }
    sdsupdatelen(query);

    // now we can split the query in arguments
    if (sdslen(query) == 0) {
      // ignore empty query
      sdsfree(query);
      return CUTIS_OK;
    }

    argv = sdssplitlen(query, sdslen(query), " ", 1, &argc);
    sdsfree(query);
    if (argv == NULL) {
      CutisOom("sdssplitlen");
    }

    for (i = 0; i < argc && c->argc < CUTIS_MAX_ARGS; i++) {
      if (sdslen(argv[i]) > 0) {
        c->argv[c->argc] = CreateCutisObject(0, argv[i]);
        c->argc++;
      } else {
        sdsfree(argv[i]);
      }
    }
    for (; i < argc; i++) {
      sdsfree(argv[i]);
    }
    zfree(argv);
    // Execute the command. If the client is still valid after
    // ProcessCommand() return and there is something on the
    // query buffer try to process the next command.
    if (ProcessCommand(c) && sdslen(c->query_buf) > 0) {
      return CUTIS_AGAIN;
    }
  } else if (sdslen(c->query_buf) >= 1024) {
    CutisLog(CUTIS_DEBUG, "Client protocol error");
    FreeClient(c);
    return CUTIS_OK;
  }
  return CUTIS_OK;
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
    c->last_interaction = time(NULL);
  } else {
    return AE_ERR;
  }

  return ParseQuery(c);
}

static int SendReplyToClient(AeEventLoop *event_loop, int fd,
                             void *client_data, int mask) {
  CutisClient *c = (CutisClient*)client_data;
  int nwritten = 0;
  int total_written = 0;

  while (listLength(c->reply)) {
    CutisObject *o = listNodeValue(listFirst(c->reply));
    int len = sdslen(o->ptr);

    if (len == 0) {
      sdsfree(o->ptr);
      zfree(o);
      listDelNode(c->reply, listFirst(c->reply));
      continue;
    }

    nwritten = write(c->fd, o->ptr + c->sent_len, len - c->sent_len);
    if (nwritten <= 0) {
      break;
    }

    // nwritten > 0
    total_written += nwritten;
    c->sent_len += nwritten;
    if (c->sent_len == len) {
      sdsfree(o->ptr);
      zfree(o);
      listDelNode(c->reply, listFirst(c->reply));
      c->sent_len = 0;
    }
  }

  if (nwritten == -1) {
    if (errno == EAGAIN) {
      nwritten = 0;
    } else {
      CutisLog(CUTIS_DEBUG, "Error writing to client: %s, %d", strerror(errno), errno);
      FreeClient(c);
      return AE_ERR;
    }
  }

  if (total_written > 0) {
    c->last_interaction = time(NULL);
  }

  if (listLength(c->reply) == 0) {
    c->sent_len = 0;
    AeDeleteFileEvent(event_loop, c->fd, AE_WRITABLE);
  }

  return AE_OK;
}

static void FreeClientArgv(CutisClient *c) {
  int i;
  for (i = 0; i < c->argc; i++) {
    sdsfree(c->argv[i]->ptr);
    zfree(c->argv[i]);
  }
  c->argc = 0;
}
