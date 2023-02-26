/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "server/server.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory/zmalloc.h"
#include "server/client.h"
#include "utils/log.h"

// Anti-warning macro
#define CUTIS_NOT_USED(v) (void)(v)

// Static functions
static void interrupt_handler(int sig);
static int ServerCron(struct AeEventLoop *event_loop,
                      long long id, void *client_data);
static int AcceptHandler(AeEventLoop *event_loop, int fd,
                         void *client_data, int mask);

// Implement interface

CutisServer *GetSingletonServer() {
  static CutisServer server;
  return &server;
}

void InitServerConfig(CutisServer *server) {
  server->port = CUTIS_SERVER_PORT;
  server->bind_addr = NULL;
  server->log_file = NULL;
  server->verbosity = CUTIS_DEBUG;
  server->el = NULL;
  server->fd = -1;
}

void InitServer(CutisServer *server) {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGINT, &interrupt_handler);
  server->el = AeCreateEventLoop();
  server->fd = anetTcpServer(server->neterr, server->port, server->bind_addr);
  if (server->fd == -1) {
    CutisLog(CUTIS_WARNING, "Opening TCP port: %s", server->neterr);
    exit(1);
  }
  AeCreateTimeEvent(server->el, 1000, ServerCron, NULL, NULL);
}

int ServerStart(CutisServer *server) {
  if (AeCreateFileEvent(server->el, server->fd, AE_READABLE,
                        AcceptHandler, server, NULL) == AE_ERR) {
    CutisOom("creating file event");
  }
  CutisLog(CUTIS_NOTICE, "The server is now ready to accept connections");
  AeMain(server->el);
  AeDeleteEventLoop(server->el);
  return CUTIS_OK;
}

static void interrupt_handler(int sig) {
  CUTIS_NOT_USED(sig);
  CutisLog(CUTIS_WARNING, "catch interrupt signal");
  AeStop(GetSingletonServer()->el);
}

static int ServerCron(struct AeEventLoop *event_loop,
                      long long id, void *client_data) {
  CUTIS_NOT_USED(event_loop);
  CUTIS_NOT_USED(id);
  CUTIS_NOT_USED(client_data);

  CutisLog(CUTIS_DEBUG, "memory used: %zu", zmalloc_used_memory());

  return 1000;
}

static int AcceptHandler(AeEventLoop *event_loop, int fd,
                         void *client_data, int mask) {
  int cfd;
  int cport;
  char cip[128];
  CutisServer *server = (CutisServer*)client_data;

  CUTIS_NOT_USED(event_loop);
  CUTIS_NOT_USED(mask);

  cfd = anetAccept(server->neterr, fd, cip, &cport);
  if (cfd == ANET_ERR) {
    CutisLog(CUTIS_DEBUG, "Accepting client connection: %s", server->neterr);
    return ANET_ERR;
  }
  CutisLog(CUTIS_DEBUG, "Accepted %s:%d", cip, cport);
  if (CreateClient(server, cfd) == NULL) {
    CutisLog(CUTIS_WARNING, "Error allocating resources for the client");
    close(cfd);
    return ANET_ERR;
  }

  return ANET_OK;
}
