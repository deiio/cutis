/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "adlist.h"
#include "ae.h"
#include "anet.h"
#include "dict.h"
#include "sds.h"
#include "zmalloc.h"

#define CUTIS_VERSION "0.01"

// Error codes
#define CUTIS_OK            0
#define CUTIS_ERR           -1

// Static server configuration
#define CUTIS_QUERY_BUF_LEN 1024

// Static server configuration
#define CUTIS_SERVER_PORT   6380  // TCP port

// Log levels
#define CUTIS_DEBUG   0
#define CUTIS_NOTICE  1
#define CUTIS_WARNING 2

// Anti-warning macro
#define CUTIS_NOT_USED(v) (void)(v)

/*============================== Data types ==============================*/

// With multiplexing we need to toke pre-client state.
// Clients are taken in a liked list.
typedef struct CutisClient {
  int fd;
  sds query_buf;
} CutisClient;

// Global server state structure
typedef struct CutisServer {
  int port;
  int fd;
  AeEventLoop *el;
  char neterr[ANET_ERR_LEN];
  char *bind_addr;
  char *log_file;
  int verbosity;
} CutisServer;

/*============================== Prototypes ==============================*/

/*=============================== Globals ================================*/

// Global vars
static CutisServer server; // server global state

/*============================== Data types ==============================*/

/*=========================== Utility functions ==========================*/

void CutisLog(int level, const char *fmt, ...) {
  va_list ap;
  FILE *file;

  file = (server.log_file == NULL) ? stdout : fopen(server.log_file, "a");
  if (!file) {
    return;
  }

  va_start(ap, fmt);
  if (level >= server.verbosity) {
    char *c = ".-*";
    fprintf(file, "%c", c[level]);
    vfprintf(file, fmt, ap);
    fprintf(file, "\n");
    fflush(file);
  }
  va_end(ap);

  if (server.log_file) {
    fclose(file);
  }
}

/*===================== Hash table type implementation ===================*/
/*======================== Random utility functions ======================*/

// Cutis generally does not try to recover from out of memory conditions
// when allocating objects or strings, it is not clear if it will be possible
// to report this condition to the client since the networking layer itself
// is based on heap allocation for send buffers, so we simply abort.
// At least the code will be simpler to read...
static void Oom(const char *msg) {
  fprintf(stderr, "%s: Out of memory\n", msg);
  fflush(stderr);
  sleep(1);
  abort();
}

/*====================== Cutis server networking stuff ===================*/

static void FreeClient(CutisClient *c) {
  AeDeleteFileEvent(server.el, c->fd, AE_READABLE);
  AeDeleteFileEvent(server.el, c->fd, AE_WRITABLE);
  close(c->fd);
  zfree(c);
}

static int ServerCron(struct AeEventLoop *event_loop, long long id,
                      void *client_data) {
  CUTIS_NOT_USED(event_loop);
  CUTIS_NOT_USED(id);
  CUTIS_NOT_USED(client_data);

  return 1000;
}

static void InitServerConfig() {
  server.port = CUTIS_SERVER_PORT;
  server.bind_addr = NULL;
  server.log_file = NULL;
  server.verbosity = CUTIS_DEBUG;
  server.el = NULL;
  server.fd = -1;
}

static void InitServer() {
  server.el = AeCreateEventLoop();
  server.fd = anetTcpServer(server.neterr, server.port, server.bind_addr);
  if (server.fd == -1) {
    CutisLog(CUTIS_WARNING, "Opening TCP port: %s", server.neterr);
    exit(1);
  }
  AeCreateTimeEvent(server.el, 1000, ServerCron, NULL, NULL);
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
      CutisLog(CUTIS_DEBUG, "Error writing to client: %s", strerror(errno));
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

static int AddReply(CutisClient *c) {
  if (AeCreateFileEvent(server.el, c->fd, AE_WRITABLE,
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

static CutisClient *CreateClient(int fd) {
  CutisClient *c = zmalloc(sizeof(*c));
  anetNonBlock(NULL, fd);
  anetTcpNoDelay(NULL, fd);
  if (!c) {
    return NULL;
  }

  c->fd = fd;
  c->query_buf = sdsempty();
  if (AeCreateFileEvent(server.el, c->fd, AE_READABLE, ReadQueryFromClient,
                        c, NULL) == AE_ERR) {
    FreeClient(c);
    return NULL;
  }

  return c;
}

static int AcceptHandler(AeEventLoop *event_loop, int fd,
                         void *client_data, int mask) {
  int cfd;
  int cport;
  char cip[128];

  CUTIS_NOT_USED(event_loop);
  CUTIS_NOT_USED(client_data);
  CUTIS_NOT_USED(mask);

  cfd = anetAccept(server.neterr, fd, cip, &cport);
  if (cfd == ANET_ERR) {
    CutisLog(CUTIS_DEBUG, "Accepting client connection: %s", server.neterr);
    return ANET_ERR;
  }
  CutisLog(CUTIS_DEBUG, "Accepted %s:%d", cip, cport);
  if (CreateClient(cfd) == NULL) {
    CutisLog(CUTIS_WARNING, "Error allocating resources for the client");
    close(cfd);
    return ANET_ERR;
  }

  return ANET_OK;
}

/*======================= Cutis objects implementation ===================*/
/*============================ DB saving/loading =========================*/
/*=============================== Commands ===============================*/
/*=============================== Strings ================================*/
/*========================= Type agnostic commands =======================*/
/*================================ Sets ==================================*/
/*================================ Main! =================================*/


int main() {
  InitServerConfig();
  InitServer();
  CutisLog(CUTIS_NOTICE, "Server started, Cutis version " CUTIS_VERSION);
  if (AeCreateFileEvent(server.el, server.fd, AE_READABLE,
                        AcceptHandler, NULL, NULL) == AE_ERR) {
    Oom("creating file event");
  }
  CutisLog(CUTIS_NOTICE, "The server is now ready to accept connections");
  AeMain(server.el);
  AeDeleteEventLoop(server.el);
  return 0;
}
