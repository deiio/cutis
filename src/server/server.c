/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "server/server.h"

#include <assert.h>
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
#define CUTIS_DEFAULT_DBNUM   16        // database number
#define CUTIS_CONFIG_LINE_MAX 1024      // maximum characters for one line
#define CUTIS_LOAD_BUF_LEN    1024      // default load DB buffer size

#define CUTIS_TMP_FILENAME    "dump-%d.%ld.cdb"
#define CUTIS_DB_SIGNATURE    "CUTIS0000"
#define CUTIS_SELECT_DB       254
#define CUTIS_EOF             255

// Static functions
static void interrupt_handler(int sig);
static int ServerCron(struct AeEventLoop *event_loop,
                      long long id, void *client_data);
static int AcceptHandler(AeEventLoop *event_loop, int fd,
                         void *client_data, int mask);
static unsigned int sdsDictHashFunction(const void *key);
static int sdsDictKeyCompare(void *priv_data, const void *key1,
                             const void *key2);
static void sdsDictKeyDestructor(void *priv_data, void *val);
static void sdsDictValDestructor(void *priv_data, void *val);


DictType sdsDictType = {
    sdsDictHashFunction,
    NULL,
    NULL,
    sdsDictKeyCompare,
    sdsDictKeyDestructor,
    sdsDictValDestructor,
};

// Implement interface

CutisServer *GetSingletonServer() {
  static CutisServer server;
  return &server;
}

void InitServerConfig(CutisServer *server) {
  server->db_num = CUTIS_DEFAULT_DBNUM;
  server->port = CUTIS_SERVER_PORT;
  server->el = NULL;
  server->fd = -1;

  server->bind_addr = NULL;
  server->log_file = NULL;
  server->verbosity = CUTIS_DEBUG;
  server->max_idle_time = CUTIS_MAX_IDLE_TIME;
}

void InitServer(CutisServer *server) {
  int i;
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGINT, &interrupt_handler);

  server->clients = listCreate();
  server->free_objs = listCreate();
  InitSharedObjects();
  server->el = AeCreateEventLoop();
  server->dict = zmalloc(sizeof(Dict*) * server->db_num);
  if (!server->clients || !server->free_objs || !server->dict) {
    CutisOom("server initialization");
  }
  server->fd = anetTcpServer(server->neterr, server->port, server->bind_addr);
  if (server->fd == -1) {
    CutisLog(CUTIS_WARNING, "Opening TCP port: %s", server->neterr);
    exit(1);
  }
  for (i = 0; i < server->db_num; i++) {
    server->dict[i] = DictCreate(&sdsDictType, NULL);
    if (!server->dict[i]) {
      CutisOom("server initialization");
    }
  }

  server->cron_loops = 0;
  server->last_save = time(NULL);
  server->bg_saving = 0;
  server->dirty = 0;
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
  int i;

  CutisLog(CUTIS_NOTICE, "Clean up server");

  // Release clients
  li = listGetIterator(server->clients, AL_START_HEAD);
  if (li != NULL) {
    while ((ln = listNextElement(li)) != NULL) {
      FreeClient(listNodeValue(ln));
    }
  }
  listReleaseIterator(li);

  for (i = 0; i < server->db_num; i++) {
    DictRelease(server->dict[i]);
  }
  zfree(server->dict);

  ReleaseSharedObjects();
  listRelease(server->clients);

  li = listGetIterator(server->free_objs, AL_START_HEAD);
  if (li != NULL) {
    while ((ln = listNextElement(li)) != NULL) {
      ReleaseCutisObject(listNodeValue(ln));
    }
  }
  listReleaseIterator(li);
  listRelease(server->free_objs);

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

#define CutisSaveDBRelease() do { \
    fclose(fp);                   \
    CutisLog(CUTIS_WARNING, "Error saving DB on disk: %s", strerror(errno)); \
    if (di) {                     \
      DictReleaseIterator(di);    \
    }                             \
    return CUTIS_ERR;             \
  } while (0)

int SaveDB(CutisServer *server, const char *filename) {
  int j = 0;
  uint8_t type = 0;
  uint32_t len = 0;
  FILE *fp = NULL;
  char tmpfile[256];
  DictIterator *di = NULL;
  DictEntry *de = NULL;

  snprintf(tmpfile, sizeof(tmpfile), CUTIS_TMP_FILENAME,
           (int)time(NULL), (long int) random());
  fp = fopen(tmpfile, "w");
  if (!fp) {
    CutisLog(CUTIS_WARNING, "Failed saving the DB: %s", strerror(errno));
    return CUTIS_ERR;
  }

  if (fwrite(CUTIS_DB_SIGNATURE, 9, 1, fp) == 0) {
    CutisSaveDBRelease();
  }

  for (j = 0; j < server->db_num; j++) {
    Dict *dict = server->dict[j];
    if (DictGetHashTableUsed(dict) == 0) {
      continue;
    }
    di = DictGetIterator(dict);
    if (!di) {
      fclose(fp);
      return CUTIS_ERR;
    }

    // Write the SELECT DB opcode
    type = CUTIS_SELECT_DB;
    len = htonl(j);
    if (fwrite(&type, 1, 1, fp) == 0) {
      CutisSaveDBRelease();
    }
    if (fwrite(&len, 4, 1, fp) == 0) {
      CutisSaveDBRelease();
    }

    // Iterate this DB writing every entry.
    while ((de = DictNext(di)) != NULL) {
      sds key = DictGetEntryKey(de);
      CutisObject *o = DictGetEntryVal(de);

      type = o->type;
      len = htonl(sdslen(key));
      if (fwrite(&type, 1, 1, fp) == 0) {
        CutisSaveDBRelease();
      }
      if (fwrite(&len, 4, 1, fp) == 0) {
        CutisSaveDBRelease();
      }
      if (fwrite(key, 1, sdslen(key), fp) == 0) {
        CutisSaveDBRelease();
      }
      if (type == CUTIS_STRING) {
        // Save a string value
        sds val = o->ptr;
        len = htonl(sdslen(val));
        if (fwrite(&len, 4, 1, fp) == 0) {
          CutisSaveDBRelease();
        }
        if (fwrite(val, 1, sdslen(val), fp) == 0) {
          CutisSaveDBRelease();
        }
      } else if (type == CUTIS_LIST) {
        // Save a list value.
        List *l = o->ptr;
        ListNode *ln = l->head;

        len = htonl(listLength(l));
        if (fwrite(&len, 4, 1, fp) == 0) {
          CutisSaveDBRelease();
        }
        while (ln) {
          o = listNodeValue(ln);
          len = htonl(sdslen(o->ptr));
          if (fwrite(&len, 4, 1, fp) == 0) {
            CutisSaveDBRelease();
          }
          if (fwrite(o->ptr, 1, sdslen(o->ptr), fp) == 0) {
            CutisSaveDBRelease();
          }
          ln = ln->next;
        }
      } else {
        assert(0);
      }
    }
    DictReleaseIterator(di);
    di = NULL;
  }

  // EOF opcode
  type = CUTIS_EOF;
  if (fwrite(&type, 1, 1, fp) == 0) {
    CutisSaveDBRelease();
  }
  fclose(fp);

  // Use RENAME to make sure the DB file is changed atomically only
  // if the generate DB file is OK.
  if (rename(tmpfile, filename) == -1) {
    CutisLog(CUTIS_WARNING, "Error moving temp DB file to the final "
                            "destination: %s", strerror(errno));
    unlink(tmpfile);
    return CUTIS_ERR;
  }
  CutisLog(CUTIS_NOTICE, "DB saved on disk");
  server->dirty = 0;
  server->last_save = time(NULL);
  return CUTIS_OK;
}

int SaveDBBackground(CutisServer *server, const char *filename) {
  pid_t child;

  if (server->bg_saving) {
    return CUTIS_ERR;
  }

  if ((child = fork()) == 0) {
    // Child
    close(server->fd);
    if (SaveDB(server, filename) == CUTIS_OK) {
      exit(0);
    } else {
      exit(1);
    }
  } else {
    // Parent
    CutisLog(CUTIS_NOTICE, "Background saving started by pid %d", child);
    server->bg_saving = 1;
    return CUTIS_OK;
  }
  return CUTIS_OK; // unreachable
}

#define CutisLoadDBRelease() do {                          \
    fclose(fp);                                            \
    if (key != buf) {                                      \
      zfree(key);                                          \
    }                                                      \
    if (val != vbuf) {                                     \
        zfree(val);                                        \
    }                                                      \
    CutisLog(CUTIS_WARNING, "Short read loading DB. "      \
             "Unrecoverable error, exiting now.");         \
    exit(1);                                               \
    return CUTIS_ERR;                                      \
  } while (0)


int LoadDB(CutisServer *server, const char *filename) {
  FILE *fp = NULL;
  char buf[CUTIS_LOAD_BUF_LEN];
  char vbuf[CUTIS_LOAD_BUF_LEN];
  char *key = NULL, *val = NULL;
  int retval = 0;
  uint8_t type = 0;
  uint32_t klen, vlen, dbid;
  Dict *dict = server->dict[0];

  fp = fopen(filename, "r");
  if (!fp) {
    return CUTIS_ERR;
  }
  if (fread(buf, 1, 9, fp) == 0) {
    CutisLoadDBRelease();
  }
  if (memcmp(buf, CUTIS_DB_SIGNATURE, 9) != 0) {
    fclose(fp);
    CutisLog(CUTIS_WARNING, "Wrong signature trying to load DB from file");
    return CUTIS_ERR;
  }

  while (1) {
    CutisObject *o;
    // Read type
    if (fread(&type, 1, 1, fp) == 0) {
      CutisLoadDBRelease();
    }
    if (type == CUTIS_EOF) {
      break;
    }
    // Handle SELECT DB opcode as a special case
    if (type == CUTIS_SELECT_DB) {
      if (fread(&dbid, 4, 1, fp) == 0) {
        CutisLoadDBRelease();
      }
      dbid = ntohl(dbid);
      if (dbid >= (unsigned) server->db_num) {
        CutisLog(CUTIS_WARNING, "FATAL: Data file was created with a "
                                "Cutis server compiled to handle more than %d"
                                " databases. Exiting\n", server->db_num);
        exit(1);
      }
      dict = server->dict[dbid];
      continue;
    }

    // Read key
    if (fread(&klen, 4, 1, fp) == 0) {
      CutisLoadDBRelease();
    }
    klen = ntohl(klen);
    if (klen <= CUTIS_LOAD_BUF_LEN) {
      key = buf;
    } else {
      key = zmalloc(klen);
      if (!key) {
        CutisOom("Loading DB from file");
      }
    }
    if (fread(key, 1, klen, fp) == 0) {
      CutisLoadDBRelease();
    }

    if (type == CUTIS_STRING) {
      // Read string value.
      if (fread(&vlen, 4, 1, fp) == 0) {
        CutisLoadDBRelease();
      }
      vlen = ntohl(vlen);
      if (vlen <= CUTIS_LOAD_BUF_LEN) {
        val = vbuf;
      } else {
        val = zmalloc(vlen);
        if (!val) {
          CutisOom("Loading DB from file");
        }
      }
      if (fread(val, 1, vlen, fp) == 0) {
        CutisLoadDBRelease();
      }
      o = CreateCutisObject(CUTIS_STRING, sdsnewlen(val, vlen));
    } else if (type == CUTIS_LIST) {
      // Read list value.
      uint32_t llen;
      if (fread(&llen, 4, 1, fp) == 0) {
        CutisLoadDBRelease();
      }
      llen = ntohl(llen);
      o = CreateListObject();
      // Load every single element of the list.
      while (llen--) {
        CutisObject *el;
        if (fread(&vlen, 4, 1, fp) == 0) {
          CutisLoadDBRelease();
        }
        vlen = ntohl(vlen);
        if (vlen <= CUTIS_LOAD_BUF_LEN) {
          val = vbuf;
        } else {
          val = zmalloc(vlen);
          if (!val) {
            CutisOom("Loading DB from file");
          }
        }
        if (fread(val, 1, vlen, fp) == 0) {
          CutisLoadDBRelease();
        }
        el = CreateCutisObject(CUTIS_STRING, sdsnewlen(val, vlen));
        if (!listAddNodeTail(o->ptr, el)) {
          CutisOom("listAddNodeTail");
        }
        // free the temp buffer if needed
        if (val != vbuf) {
          zfree(val);
        }
        val = NULL;
      }
    } else {
      assert(0);
    }

    // Add the new object in the hash table.
    retval = DictAdd(dict, sdsnewlen(key, klen), o);
    if (retval == DICT_ERR) {
      CutisLog(CUTIS_WARNING, "Loading DB, duplicated key found! "
                              "Unrecoverable error, exiting now");
      exit(1);
    }

    // Iteration cleanup
    if (key != buf) {
      zfree(key);
    }
    if (val != vbuf) {
      zfree(val);
    }
    key = NULL;
    val = NULL;
  }

  fclose(fp);
  return CUTIS_OK;
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
    CutisLog(CUTIS_DEBUG, "%d clients connected, %lld dirty, "
                          "%zu bytes in use", listLength(server->clients),
             server->dirty, zmalloc_used_memory());
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

static unsigned int sdsDictHashFunction(const void *key) {
  return DictGenHashFunction(key, sdslen((sds)key));
}

static int sdsDictKeyCompare(void *priv_data, const void *key1,
                             const void *key2) {
  CUTIS_NOT_USED(priv_data);
  return sdscmp((sds)key1, (sds)key2) == 0;
}

static void sdsDictKeyDestructor(void *priv_data, void *val) {
  CUTIS_NOT_USED(priv_data);
  sdsfree(val);
}

static void sdsDictValDestructor(void *priv_data, void *val) {
  CUTIS_NOT_USED(priv_data);
  DecrRefCount(val);
}
