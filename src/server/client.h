/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef SERVER_CLIENT_H_
#define SERVER_CLIENT_H_

#include "commands/command.h"
#include "data_struct/adlist.h"
#include "data_struct/sds.h"

#include <time.h>

typedef struct CutisServer CutisServer;

// Static server configuration
#define CUTIS_QUERY_BUF_LEN 1024
#define CUTIS_MAX_ARGS      16

// With multiplexing we need to take pre-client state.
// Clients are taken in a liked list.
typedef struct CutisClient {
  int fd;                             // TCP connection fd
  sds query_buf;                      // read from fd
  List *reply;                        // reply message
  int sent_len;                       // sent length for first element in reply
  time_t last_interaction;            // used for timeout
  CutisObject *argv[CUTIS_MAX_ARGS];  // arguments array
  int argc;                           // arguments count
  int bulk_len;                       // bulk read len. -1 single read mode
  CutisServer *server;                // pointed to the server
} CutisClient;


CutisClient *CreateClient(CutisServer *server, int fd);
void FreeClient(CutisClient *c);
void ResetClient(CutisClient *c);
int AddReply(CutisClient *c);
int ParseQuery(CutisClient *c);
int ParseBulkQuery(CutisClient *c);
int ParseNonBulkQuery(CutisClient *c);

#endif  // SERVER_CLIENT_H_
