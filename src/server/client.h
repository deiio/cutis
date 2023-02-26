/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef SERVER_CLIENT_H_
#define SERVER_CLIENT_H_

#include "data_struct/sds.h"

typedef struct CutisServer CutisServer;

// Static server configuration
#define CUTIS_QUERY_BUF_LEN 1024

// With multiplexing we need to take pre-client state.
// Clients are taken in a liked list.
typedef struct CutisClient {
  int fd;
  sds query_buf;
  CutisServer *server;
} CutisClient;


CutisClient *CreateClient(CutisServer *server, int fd);
void FreeClient(CutisClient *c);

#endif  // SERVER_CLIENT_H_
