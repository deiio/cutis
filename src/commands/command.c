/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "commands/command.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "memory/zmalloc.h"
#include "server/client.h"
#include "server/server.h"
#include "utils/log.h"

static CutisCommand cmdTable[] = {
    {"get", GetCommand, 2, CUTIS_CMD_INLINE},
    {"set", SetCommand, 3, CUTIS_CMD_BULK},
    {NULL, NULL, 0, 0},
};

struct SharedObject {
  CutisObject *crlf;
  CutisObject *ok;
  CutisObject *err;
  CutisObject *zerobulk;
  CutisObject *nil;
  CutisObject *zero;
  CutisObject *one;
  CutisObject *pong;
} shared;

CutisObject *CreateCutisObject(int type, void *ptr) {
  CutisObject *o = NULL;
  CutisServer *s = GetSingletonServer();
  if (listLength(s->free_objs)) {
    ListNode *head = listFirst(s->free_objs);
    o = listNodeValue(head);
    listDelNode(s->free_objs, head);
  } else {
    o = zmalloc(sizeof(CutisObject));
  }
  if (o == NULL) {
    CutisOom("CreateCutisObject");
  }

  o->ptr = ptr;
  o->type = type;
  o->refcount = 1;
  return o;
}

CutisObject *CreateListObject() {
  List *l = listCreate();
  if (!l) {
    CutisOom("CreateListObject");
  }
  listSetFreeMethod(l, (void(*)(void*))(&DecrRefCount));
  return CreateCutisObject(CUTIS_LIST, l);
}

void FreeStringObject(CutisObject *o) {
  sdsfree(o->ptr);
}

void FreeListObject(CutisObject *o) {
  listRelease(o->ptr);
}

void FreeSetObject(CutisObject *o) {
  // TODO
}

void IncrRefCount(CutisObject *o) {
  o->refcount++;
}

void DecrRefCount(CutisObject *o) {
  if (--(o->refcount) == 0) {
    CutisServer *s = GetSingletonServer();
    switch (o->type) {
    case CUTIS_STRING:
      FreeStringObject(o);
      break;
    case CUTIS_LIST:
      FreeListObject(o);
      break;
    case CUTIS_SET:
      FreeSetObject(o);
      break;
    default:
      assert(0);
      break;
    }
    if (!listAddNodeHead(s->free_objs, o)) {
      zfree(o);
    }
  }
}

void InitSharedObjects() {
  shared.crlf = CreateCutisObject(CUTIS_STRING, sdsnew("\r\n"));
  shared.ok = CreateCutisObject(CUTIS_STRING, sdsnew("+OK\r\n"));
  shared.err = CreateCutisObject(CUTIS_STRING, sdsnew("-ERR\r\n"));
  shared.zerobulk = CreateCutisObject(CUTIS_STRING, sdsnew("0\r\n\r\n"));
  shared.nil = CreateCutisObject(CUTIS_STRING, sdsnew("nil\r\n"));
  shared.zero = CreateCutisObject(CUTIS_STRING, sdsnew("0\r\n"));
  shared.one = CreateCutisObject(CUTIS_STRING, sdsnew("1\r\n"));
  shared.pong = CreateCutisObject(CUTIS_STRING, sdsnew("+PONG\r\n"));
}

int ProcessCommand(CutisClient *c) {
  sdstolower(c->argv[0]);
  if (!strcmp(c->argv[0], "quit")) {
    FreeClient(c);
    return 0;
  }

  CutisCommand *cmd = LookupCommand(c->argv[0]);
  if (!cmd) {
    AddReplySds(c, sdsnew("-ERR unknown command\r\n"));
    ResetClient(c);
    return 1;
  } else if (cmd->arity != c->argc) {
    AddReplySds(c, sdsnew("-ERR wrong number of arguments\r\n"));
    ResetClient(c);
    return 1;
  } else if (cmd->type == CUTIS_CMD_BULK && c->bulk_len == -1) {
    int bulk_len = atoi(c->argv[c->argc-1]);
    sdsfree(c->argv[c->argc-1]);

    if (bulk_len < 0 || bulk_len > CUTIS_MAX_STRING_LENGTH) {
      c->argc--;
      c->argv[c->argc] = NULL;
      AddReplySds(c, sdsnew("-ERR invalid bulk write count\r\n"));
      ResetClient(c);
      return 1;
    }

    c->argv[c->argc-1] = NULL;
    c->argc--;
    c->bulk_len = bulk_len + 2; // Add two bytes for CRLF
    /* It is possible that the bulk read is already in the
     * buffer. Check this condition and handle it accordingly. */
    if (sdslen(c->query_buf) >= c->bulk_len) {
      c->argv[c->argc] = sdsnewlen(c->query_buf, c->bulk_len-2);
      c->argc++;
      c->query_buf = sdsrange(c->query_buf, c->bulk_len, -1);
    } else {
      return 1;
    }
  }

  // Exec cmd command.
  cmd->proc(c);
  ResetClient(c);
  return 1;
}

CutisCommand *LookupCommand(char *name) {
  int i = 0;
  while (cmdTable[i].name != NULL) {
    if (!strcmp(cmdTable[i].name, name)) {
      return &cmdTable[i];
    }
    i++;
  }
  return NULL;
}

// Command implementations.
static void SetGenericCommand(CutisClient *c, int nx) {
  int ret;
  CutisObject *o = CreateCutisObject(CUTIS_STRING, c->argv[2]);
  c->argv[2] = NULL;
  ret = DictAdd(c->dict, c->argv[1], o);
  if (ret == DICT_ERR) {
    if (!nx) {
      DictReplace(c->dict, c->argv[1], o);
    } else {
      DecrRefCount(o);
    }
  } else {
    // Now the key is in the hash entry, don't free it.
    c->argv[1] = NULL;
  }

  c->server->dirty++;
  AddReply(c, shared.ok);
}

void GetCommand(CutisClient *c) {
  CutisLog(CUTIS_DEBUG, "%s", __func__);
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  if (de == NULL) {
    AddReply(c, shared.nil);
  } else {
    CutisObject *o = DictGetEntryVal(de);
    if (o->type != CUTIS_STRING) {
      char *err = "GET against key not holding a string value";
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n%s\r\n",
                  -((int)strlen(err)), err));
    } else {
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n", (int)sdslen(o->ptr)));
      AddReply(c, o);
      AddReply(c, shared.crlf);
    }
  }
}

void SetCommand(CutisClient *c) {
  CutisLog(CUTIS_DEBUG, "%s", __func__);
  return SetGenericCommand(c, 0);
}
