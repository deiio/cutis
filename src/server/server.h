/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef SERVER_SERVER_H_
#define SERVER_SERVER_H_

#include "event/ae.h"
#include "net/anet.h"

// Static server configuration
#define CUTIS_SERVER_PORT   6380  // TCP port

// Error codes
#define CUTIS_OK            0
#define CUTIS_ERR           -1

// Server state structure
typedef struct CutisServer {
  int port;
  int fd;
  AeEventLoop *el;
  char neterr[ANET_ERR_LEN];
  char *bind_addr;
  char *log_file;
  int verbosity;
} CutisServer;


CutisServer *GetSingletonServer();
void InitServerConfig(CutisServer *server);
void InitServer(CutisServer *server);
int ServerStart(CutisServer *server);


#endif  // SERVER_SERVER_H_
