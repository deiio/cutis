/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "server/server.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "memory/zmalloc.h"
#include "server/client.h"
#include "utils/log.h"

// Anti-warning macro
#define CUTIS_NOT_USED(v) (void)(v)

// Static server configuration
#define CUTIS_SERVER_PORT     6380      // TCP port
#define CUTIS_MAX_IDLE_TIME   (60 * 5)  // default client timeout
#define CUTIS_CONFIG_LINE_MAX 1024      // maximum characters for one line

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
  server->el = NULL;
  server->fd = -1;

  server->bind_addr = NULL;
  server->log_file = NULL;
  server->verbosity = CUTIS_DEBUG;
  server->max_idle_time = CUTIS_MAX_IDLE_TIME;
}

void InitServer(CutisServer *server) {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGINT, &interrupt_handler);

  server->clients = listCreate();
  server->el = AeCreateEventLoop();
  if (!server->clients) {
    CutisOom("server initialization");
  }
  server->fd = anetTcpServer(server->neterr, server->port, server->bind_addr);
  if (server->fd == -1) {
    CutisLog(CUTIS_WARNING, "Opening TCP port: %s", server->neterr);
    exit(1);
  }
  server->cron_loops = 0;
  AeCreateTimeEvent(server->el, 1000, ServerCron, server, NULL);
}

int LoadServerConfig(CutisServer *server, const char *filename) {
  FILE *fp = fopen(filename, "r");
  char buf[CUTIS_CONFIG_LINE_MAX + 1];
  sds err = NULL;
  int line_num = 0;
  sds line = NULL;
  sds *argv;
  int argc;

  if (!fp) {
    CutisLog(CUTIS_WARNING, "Fatal error, can't open config file");
    return CUTIS_ERR;
  }

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    int j;

    line_num++;
    line = sdsnew(buf);
    line = sdstrim(line, " \t\r\n");

    // Skip comments and blank lines
    if (line[0] == '#' || line[0] == '\0') {
      sdsfree(line);
      continue;
    }

    // Split into arguments
    argv = sdssplitlen(line, sdslen(line), " ", 1, &argc);
    sdstolower(argv[0]);

    // Execute config directives
    if (strcmp(argv[0], "port") == 0 && argc == 2) {
      server->port = atoi(argv[1]);
      if (server->port < 1 || server->port > 65535) {
        err = sdsnew("Invalid port");
        break;
      }
    } else if (strcmp(argv[0], "loglevel") == 0 && argc == 2) {
      if (strcmp(argv[1], "debug") == 0) {
        server->verbosity = CUTIS_DEBUG;
      } else if (strcmp(argv[1], "notice") ==0 ) {
        server->verbosity = CUTIS_NOTICE;
      } else if (strcmp(argv[1], "warning") == 0) {
        server->verbosity = CUTIS_WARNING;
      } else {
        err = sdsnew("Invalid log level. "
                     "Must be one of debug, notice, warning");
        break;
      }
    } else if (strcmp(argv[0], "logfile") == 0 && argc == 2) {
      server->log_file = zstrdup(argv[1]);
      if (strcmp(server->log_file, "stdout") == 0) {
        zfree(server->log_file);
        server->log_file = NULL;
      }

      if (server->log_file) {
        // Test if we are able to open the file. The server will not be able
        // to abort just for this problem later...
        FILE *log = fopen(server->log_file, "a");
        if (log == NULL) {
          err = sdscatprintf(sdsempty(), "Can't open the log file: %s",
                             strerror(errno));
          break;
        }
        fclose(log);
      }
    } else {
      err = sdsnew("Bad directive or wrong number of arguments");
      break;
    }

    for (j = 0; j < argc; j++) {
      sdsfree(argv[j]);
    }
    zfree(argv);
    sdsfree(line);
  }

  fclose(fp);
  if (err == NULL) {
    return CUTIS_OK;
  } else {
    int j;

    fprintf(stderr, "\n*** FATAL CONFIG FILE ERROR ***\n");
    fprintf(stderr, "Reading the configuration file, at line %d\n", line_num);
    fprintf(stderr, ">>> '%s'\n", line);
    fprintf(stderr, "%s\n", err);

    for (j = 0; j < argc; j++) {
      sdsfree(argv[j]);
    }
    zfree(argv);
    sdsfree(line);
    sdsfree(err);
    return CUTIS_ERR;
  }
}

int StartServer(CutisServer *server) {
  if (AeCreateFileEvent(server->el, server->fd, AE_READABLE,
                        AcceptHandler, server, NULL) == AE_ERR) {
    CutisOom("creating file event");
  }
  CutisLog(CUTIS_NOTICE, "The server is now ready to accept connections");
  AeMain(server->el);
  return CUTIS_OK;
}

int CleanServer(CutisServer *server) {
  ListIter *li;
  ListNode *ln;
  size_t used_size = 0;

  CutisLog(CUTIS_NOTICE, "Clean up server");

  // Release clients
  li = listGetIterator(server->clients, AL_START_HEAD);
  if (li != NULL) {
    while ((ln = listNextElement(li)) != NULL) {
      FreeClient(listNodeValue(ln));
    }
  }
  listReleaseIterator(li);
  listRelease(server->clients);

  AeDeleteEventLoop(server->el);

  if (server->log_file != NULL) {
    used_size = sizeof(size_t) + strlen(server->log_file) + 1;
  }
  CutisLog(CUTIS_DEBUG, "After clean up %zu bytes in use. "
                        "(include %zu bytes for log filename)",
           zmalloc_used_memory(), used_size);

  if (server->log_file != NULL) {
    zfree(server->log_file);
  }

  return CUTIS_OK;
}

void CloseTimeoutClients(CutisServer *server) {
  ListIter *li;
  ListNode *ln;
  CutisClient *c;
  time_t now = time(NULL);

  li = listGetIterator(server->clients, AL_START_HEAD);
  if (!li) {
    return;
  }

  while ((ln = listNextElement(li)) != NULL) {
    c = listNodeValue(ln);
    if ((now - c->last_interaction) > server->max_idle_time) {
      CutisLog(CUTIS_DEBUG, "Closing idle client");
      FreeClient(c);
    }
  }
  listReleaseIterator(li);
}

static void interrupt_handler(int sig) {
  CUTIS_NOT_USED(sig);
  CutisLog(CUTIS_WARNING, "catch interrupt signal");
  AeStop(GetSingletonServer()->el);
}

static int ServerCron(struct AeEventLoop *event_loop,
                      long long id, void *client_data) {
  CutisServer *server = (CutisServer *)client_data;
  int loops = server->cron_loops++;
  CUTIS_NOT_USED(event_loop);
  CUTIS_NOT_USED(id);

  // Show information about memory used and connected clients
  if (loops % 5 == 0) {
    CutisLog(CUTIS_DEBUG, "%d clients connected, %zu bytes in use",
             listLength(server->clients), zmalloc_used_memory());
  }

  // Close connections of timeout clients
  if (loops % 10 == 0) {
    CloseTimeoutClients(server);
  }

  return 1000;
}

static int AcceptHandler(AeEventLoop *event_loop, int fd,
                         void *client_data, int mask) {
  int cfd;
  int cport;
  char cip[128];
  CutisServer *server = (CutisServer *)client_data;

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
