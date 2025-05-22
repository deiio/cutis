/*
 * Cutis is a key/value database.
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef SERVER_CLIENT_H_
#define SERVER_CLIENT_H_

#include "commands/command.h"
#include "commands/object.h"
#include "data_struct/adlist.h"
#include "data_struct/dict.h"
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
  sds argv[CUTIS_MAX_ARGS];           // arguments array
  int argc;                           // arguments count
  int bulk_len;                       // bulk read len. -1 single read mode
  Dict *dict;                         // database's dict
  CutisServer *server;                // pointed to the server
} CutisClient;


CutisClient *CreateClient(CutisServer *server, int fd);
void FreeClient(CutisClient *c);
void ResetClient(CutisClient *c);
int AddReply(CutisClient *c, CutisObject* o);
int AddReplySds(CutisClient *c, sds s);
int ParseQuery(CutisClient *c);
int ParseBulkQuery(CutisClient *c);
int ParseNonBulkQuery(CutisClient *c);

int SelectDB(CutisClient *c, int id);

#endif  // SERVER_CLIENT_H_
