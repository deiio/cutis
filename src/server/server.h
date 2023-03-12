/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef SERVER_SERVER_H_
#define SERVER_SERVER_H_

#include "data_struct/adlist.h"
#include "event/ae.h"
#include "net/anet.h"

// Error codes
#define CUTIS_OK            0
#define CUTIS_ERR           -1
#define CUTIS_AGAIN         1

// Server state structure
typedef struct CutisServer {
  int port;                   // listen port
  int fd;                     // listen fd
  int cron_loops;             // number of times the cron function fun
  List *clients;              // connected clients list
  AeEventLoop *el;            // event loop
  char neterr[ANET_ERR_LEN];  // network error message

  // Configuration
  char *bind_addr;            // band address
  char *log_file;             // log file
  int verbosity;              // log level
  int max_idle_time;          // client's maximum idle time (second)
} CutisServer;


CutisServer *GetSingletonServer();
void InitServerConfig(CutisServer *server);
void InitServer(CutisServer *server);
int LogServerConfig(CutisServer *server, const char *filename);
int ServerStart(CutisServer *server);
void CloseTimeoutClients(CutisServer *server);

#endif  // SERVER_SERVER_H_
