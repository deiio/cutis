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

#ifndef SERVER_SERVER_H_
#define SERVER_SERVER_H_

#include <time.h>

#include "data_struct/adlist.h"
#include "data_struct/dict.h"
#include "event/ae.h"
#include "net/anet.h"

// Error codes
#define CUTIS_OK            0
#define CUTIS_ERR           -1
#define CUTIS_AGAIN         1

#define CUTIS_DB_NAME       "dump.cdb"

typedef struct SaveParam {
  time_t seconds;
  int changes;
} SaveParam;

// Server state structure
typedef struct CutisServer {
  int port;                   // listen port
  int fd;                     // listen fd
  int cron_loops;             // number of times the cron function run
  long long dirty;            // changes to DB form the last save
  List *clients;              // connected clients list
  AeEventLoop *el;            // event loop
  char neterr[ANET_ERR_LEN];  // network error message
  List *free_objs;            // a list of freed objects to avoid malloc
  Dict **dict;                // each dict corresponds to a database

  time_t last_save;           // the timestamp of last save DB
  int bg_saving;              // background saving in process?
  int save_param_len;         // save_params's length
  SaveParam *save_params;     // save DB rules

  // Configuration
  char *bind_addr;            // band address
  char *log_file;             // log file
  int verbosity;              // log level
  int max_idle_time;          // client's maximum idle time (second)
  int db_num;                 // db number
} CutisServer;


CutisServer *GetSingletonServer();
void InitServerConfig(CutisServer *server);
void InitServer(CutisServer *server);
int LoadServerConfig(CutisServer *server, const char *filename);
int StartServer(CutisServer *server);
int CleanServer(CutisServer *server);
void CloseTimeoutClients(CutisServer *server);
int SaveDB(CutisServer *server, const char *filename);
int SaveDBBackground(CutisServer *server, const char *filename);
int LoadDB(CutisServer *server, const char *filename);

// SaveParams
void AppendServerSaveParams(CutisServer *server, time_t seconds, int changes);
void ResetServerSaveParams(CutisServer *server);

// DictType functions
unsigned int sdsDictHashFunction(const void *key);
int sdsDictKeyCompare(void *priv_data, const void *key1, const void *key2);
void sdsDictKeyDestructor(void *priv_data, void *val);
void sdsDictValDestructor(void *priv_data, void *val);

#endif  // SERVER_SERVER_H_
