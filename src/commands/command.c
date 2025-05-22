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

#include "commands/command.h"

#include <string.h>
#include <stdlib.h>

#include "commands/object.h"
#include "memory/zmalloc.h"
#include "server/client.h"
#include "server/server.h"
#include "utils/log.h"
#include "utils/string_util.h"

static CutisCommand cmdTable[] = {
    {"get", GetCommand, 2, CUTIS_CMD_INLINE},
    {"set", SetCommand, 3, CUTIS_CMD_BULK},
    {"setnx", SetnxCommand, 3, CUTIS_CMD_BULK},
    {"exists", ExistsCommand, 2, CUTIS_CMD_INLINE},
    {"del", DelCommand, 2, CUTIS_CMD_INLINE},
    {"incr", IncrCommand, 2, CUTIS_CMD_INLINE},
    {"decr", DecrCommand, 2, CUTIS_CMD_INLINE},
    {"rpush", RPushCommand, 3, CUTIS_CMD_BULK},
    {"lpush", LPushCommand, 3, CUTIS_CMD_BULK},
    {"rpop", RPopCommand, 2, CUTIS_CMD_INLINE},
    {"lpop", LPopCommand, 2, CUTIS_CMD_INLINE},
    {"llen", LLenCommand, 2, CUTIS_CMD_INLINE},
    {"lindex", LIndexCommand, 3, CUTIS_CMD_INLINE},
    {"lrange", LRangeCommand, 4, CUTIS_CMD_INLINE},
    {"ltrim", LTrimCommand, 4, CUTIS_CMD_INLINE},
    {"lset", LSetCommand, 4, CUTIS_CMD_BULK},
    {"sadd", SAddCommand, 3, CUTIS_CMD_BULK},
    {"srem", SRemCommand, 3, CUTIS_CMD_BULK},
    {"sismember", SIsMemberCommand, 3, CUTIS_CMD_BULK},
    {"scard", SCardCommand, 2, CUTIS_CMD_INLINE},
    {"sinter", SInterCommand, -2, CUTIS_CMD_INLINE},
    {"smembers", SInterCommand, 2, CUTIS_CMD_INLINE},
    {"select", SelectCommand, 2, CUTIS_CMD_INLINE},
    {"move", MoveCommand, 3, CUTIS_CMD_INLINE},
    {"rename", RenameCommand, 3, CUTIS_CMD_INLINE},
    {"renamenx", RenamenxCommand, 3, CUTIS_CMD_INLINE},
    {"randomkey", RandomKeyCommand, 1, CUTIS_CMD_INLINE},
    {"keys", KeysCommand, 2, CUTIS_CMD_INLINE},
    {"dbsize", DbsizeCommand, 1, CUTIS_CMD_INLINE},
    {"save", SaveCommand, 1, CUTIS_CMD_INLINE},
    {"bgsave", BgsaveCommand, 1, CUTIS_CMD_INLINE},
    {"shutdown", ShutDownCommand, 1, CUTIS_CMD_INLINE},
    {"ping", PingCommand, 1, CUTIS_CMD_INLINE},
    {"echo", EchoCommand, 2, CUTIS_CMD_INLINE},
    {"lastsave", LastSaveCommand, 1, CUTIS_CMD_INLINE},
    {"type", TypeCommand, 2, CUTIS_CMD_INLINE},
    {NULL, NULL, 0, 0},
};

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
  } else if ((cmd->arity > 0 && cmd->arity != c->argc) ||
             (c->argc < -cmd->arity)) {
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
  return SetGenericCommand(c, 0);
}

void SetnxCommand(CutisClient *c) {
  return SetGenericCommand(c, 1);
}

void ExistsCommand(CutisClient *c) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  if (de == NULL) {
    AddReply(c, shared.zero);
  } else {
    AddReply(c, shared.one);
  }
}

void DelCommand(CutisClient *c) {
  if (DictDelete(c->dict, c->argv[1]) == DICT_OK) {
    c->server->dirty++;
  }
  AddReply(c, shared.ok);
}

static void IncrDecrCommand(CutisClient *c, int incr) {
  int retval;
  sds newval;
  CutisObject *o;
  long long value;
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  if (de == NULL) {
    value = 0;
  } else {
    o = DictGetEntryVal(de);
    if (o->type != CUTIS_STRING) {
      value = 0;
    } else {
      char *end;
      value = strtoll(o->ptr, &end, 10);
    }
  }

  value += incr;
  newval = sdscatprintf(sdsempty(), "%lld", value);
  o = CreateCutisObject(CUTIS_STRING, newval);
  retval = DictAdd(c->dict, c->argv[1], o);
  if (retval == DICT_ERR) {
    DictReplace(c->dict, c->argv[1], o);
  } else {
    // Now the key is in the hash entry, don't free it
    c->argv[1] = NULL;
  }

  c->server->dirty++;
  AddReply(c, o);
  AddReply(c, shared.crlf);
}

void IncrCommand(CutisClient *c) {
  IncrDecrCommand(c, 1);
}

void DecrCommand(CutisClient *c) {
  IncrDecrCommand(c, -1);
}

void RandomKeyCommand(CutisClient *c) {
  DictEntry *de = DictGetRandomKey(c->dict);
  if (de == NULL) {
    AddReply(c, shared.crlf);
  } else {
    AddReplySds(c, sdsdup(DictGetEntryKey(de)));
    AddReply(c, shared.crlf);
  }
}

static void PushGenericCommand(CutisClient *c, int where) {
   CutisObject *el = NULL, *o = NULL;
   DictEntry *de = NULL;
   List *l = NULL;

   el = CreateCutisObject(CUTIS_STRING, c->argv[2]);
   c->argv[2] = NULL;

   de = DictFind(c->dict, c->argv[1]);
   if (!de) {
     o = CreateListObject();
     l = o->ptr;
     if (where == CUTIS_HEAD) {
       if (!listAddNodeHead(l, el)) {
         CutisOom("listAddNodeHead");
       }
     } else {
       if (!listAddNodeTail(l, el)) {
         CutisOom("listAddNodeTail");
       }
     }
     DictAdd(c->dict, c->argv[1], o);
     // Now the key is in the hash entry, don't free it.
     c->argv[1] = NULL;
   } else {
     o = DictGetEntryVal(de);
     if (o->type != CUTIS_LIST) {
       DecrRefCount(el);
       char *err = "-ERR push against existing key not holding a list \r\n";
       AddReplySds(c, sdsnew(err));
       return;
     }
     l = o->ptr;
     if (where == CUTIS_HEAD) {
       if (!listAddNodeHead(l, el)) {
         CutisOom("listAddNodeHead");
       }
     } else {
       if (!listAddNodeTail(l, el)) {
         CutisOom("listAddNodeTail");
       }
     }
   }
   c->server->dirty++;
   AddReply(c, shared.ok);
}

void RPushCommand(CutisClient *c) {
  PushGenericCommand(c, CUTIS_TAIL);
}

void LPushCommand(CutisClient *c) {
  PushGenericCommand(c, CUTIS_HEAD);
}

static void PopGenericCommand(CutisClient *c, int where) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  if (!de) {
    AddReply(c, shared.nil);
  } else {
    CutisObject *o = DictGetEntryVal(de);
    if (o->type != CUTIS_LIST) {
      char *err = "POP against key not holding a list value";
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n%s\r\n",
                                  -(int)strlen(err), err));
    } else {
      List *l = o->ptr;
      ListNode *ln;

      if (where == CUTIS_HEAD) {
        ln = listFirst(l);
      } else {
        ln = listLast(l);
      }

      if (ln == NULL) {
        AddReply(c, shared.nil);
      } else {
        CutisObject *el = listNodeValue(ln);
        AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n", sdslen(el->ptr)));
        AddReply(c, el);
        AddReply(c, shared.crlf);
        listDelNode(l, ln);
        c->server->dirty++;
      }
    }
  }
}

void RPopCommand(CutisClient *c) {
  PopGenericCommand(c, CUTIS_TAIL);
}

void LPopCommand(CutisClient *c) {
  PopGenericCommand(c, CUTIS_HEAD);
}

void LLenCommand(CutisClient *c) {
  List *l;
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  if (!de) {
    AddReply(c, shared.zero);
    return;
  } else {
    CutisObject *o = DictGetEntryVal(de);
    if (o->type != CUTIS_LIST) {
      AddReplySds(c, sdsnew("-1\r\n"));
    } else {
      l = o->ptr;
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n", listLength(l)));
    }
  }
}

void LIndexCommand(CutisClient *c) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  int index = atoi(c->argv[2]);

  if (!de) {
    AddReply(c, shared.nil);
  } else {
    CutisObject *o = DictGetEntryVal(de);
    if (o->type != CUTIS_LIST) {
      char *err = "LINDEX against key not holding a list value";
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n%s\r\n",
                                  -(int)strlen(err), err));
    } else {
      List *l = o->ptr;
      ListNode *ln = listIndex(l, index);
      if (!ln) {
        AddReply(c, shared.nil);
      } else {
        CutisObject *el = listNodeValue(ln);
        AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n", sdslen(el->ptr)));
        AddReply(c, el);
        AddReply(c, shared.crlf);
      }
    }
  }
}

void LRangeCommand(CutisClient *c) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  int start = atoi(c->argv[2]);
  int end = atoi(c->argv[3]);

  if (!de) {
    AddReply(c, shared.nil);
  } else {
    CutisObject *o = DictGetEntryVal(de);
    if (o->type != CUTIS_LIST) {
      char *err = "LRANGE against key not holding a list value";
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n%s\r\n",
                                  -(int)strlen(err), err));
    } else {
      List *l = o->ptr;
      ListNode *ln = NULL;
      int llen = listLength(l);
      int range_len;
      int j;

      // convert negative indexes
      if (start < 0) {
        start = llen + start;
      }
      if (end < 0) {
        end = llen + end;
      }
      if (start < 0) {
        start = 0;
      }
      if (end < 0) {
        end = 0;
      }

      // indexes sanity checks
      if (start > end || start >= llen) {
        // Out of range start or start > end result in empty list
        AddReply(c, shared.zero);
        return;
      }

      if (end >= llen) {
        end = llen - 1;
      }
      range_len = (end - start) + 1;

      // Return the result in form of a multi-bulk reply
      ln = listIndex(l, start);
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n", range_len));
      for (j = 0; j < range_len; j++) {
        CutisObject *el = listNodeValue(ln);
        AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n", sdslen(el->ptr)));
        AddReply(c, el);
        AddReply(c, shared.crlf);
        ln = ln->next;
      }
    }
  }
}

void LTrimCommand(CutisClient *c) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  int start = atoi(c->argv[2]);
  int end = atoi(c->argv[3]);

  if (!de) {
    AddReplySds(c, sdsnew("-ERR no such key\r\n"));
  } else {
    CutisObject *o = DictGetEntryVal(de);
    if (o->type != CUTIS_LIST) {
      char *err = "-ERR LTRIM against key not holding a list value\r\n";
      AddReplySds(c, sdsnew(err));
    } else {
      List *l = o->ptr;
      ListNode *ln = NULL;
      int llen = listLength(l);
      int j;
      int ltrim, rtrim;

      // convert negative indexes
      if (start < 0) {
        start = llen + start;
      }
      if (end < 0) {
        end = llen + end;
      }
      if (start < 0) {
        start = 0;
      }
      if (end < 0) {
        end = 0;
      }

      // indexes sanity checks.
      if (start > end || start >= llen) {
        // Out of range start or start > end result in empty list.
        ltrim = llen;
        rtrim = 0;
      } else {
        if (end >= llen) {
          end = llen - 1;
        }
        ltrim = start;
        rtrim = llen - end - 1;
      }

      // Remove list elements to perform the trim.
      for (j = 0; j < ltrim; j++) {
        ln = listFirst(l);
        listDelNode(l, ln);
      }
      for (j = 0; j < rtrim; j++) {
        ln = listLast(l);
        listDelNode(l, ln);
      }
      AddReply(c, shared.ok);
      c->server->dirty++;
    }
  }
}

void LSetCommand(CutisClient *c) {
  int index = atoi(c->argv[2]);
  DictEntry *de = DictFind(c->dict, c->argv[1]);

  if (!de) {
    AddReplySds(c, sdsnew("-ERR no such key\r\n"));
  } else {
    CutisObject *o = DictGetEntryVal(de);
    if (o->type != CUTIS_LIST) {
      const char *err = "-ERR LSET against key not holding a list value\r\n";
      AddReplySds(c, sdsnew(err));
    } else {
      List *l = o->ptr;
      ListNode *ln = listIndex(l, index);

      if (!ln) {
        AddReplySds(c, sdsnew("-ERR index out of range\r\n"));
      } else {
        CutisObject *el = listNodeValue(ln);
        DecrRefCount(el);
        listNodeValue(ln) = CreateCutisObject(CUTIS_STRING, c->argv[3]);
        c->argv[3] = NULL;
        AddReply(c, shared.ok);
        c->server->dirty++;
      }
    }
  }
}

void SAddCommand(CutisClient *c) {
  CutisObject *el, *set;
  DictEntry *de = DictFind(c->dict, c->argv[1]);

  if (!de) {
    set = CreateSetObject();
    DictAdd(c->dict, c->argv[1], set);
    c->argv[1] = NULL;
  } else {
    set = DictGetEntryVal(de);
    if (set->type != CUTIS_SET) {
      char *err = "-ERR SADD against key not holding a set value\r\n";
      AddReplySds(c, sdsnew(err));
      return;
    }
  }
  el = CreateCutisObject(CUTIS_STRING, c->argv[2]);
  if (DictAdd(set->ptr, el, NULL) == DICT_OK) {
    c->server->dirty++;
  } else {
    DecrRefCount(el);
  }
  c->argv[2] = NULL;
  AddReply(c, shared.ok);
}

void SRemCommand(CutisClient *c) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  if (!de) {
    AddReplySds(c, sdsnew("-ERR no such key\r\n"));
  } else {
    CutisObject *set, *el;
    set = DictGetEntryVal(de);
    if (set->type != CUTIS_SET) {
      char *err = "-ERR SREM against key not holding a set value\r\n";
      AddReplySds(c, sdsnew(err));
      return;
    }
    el = CreateCutisObject(CUTIS_STRING, c->argv[2]);
    if (DictDelete(set->ptr, el) == DICT_OK) {
      c->server->dirty++;
    }
    el->ptr = NULL;
    DecrRefCount(el);
    AddReply(c, shared.ok);
  }
}

void SIsMemberCommand(CutisClient *c) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);

  if (!de) {
    AddReplySds(c, sdsnew("-1\r\n"));
  } else {
    CutisObject *set, *el;
    set = DictGetEntryVal(de);
    if (set->type != CUTIS_SET) {
      AddReplySds(c, sdsnew("-1\r\n"));
      return;
    }
    el = CreateCutisObject(CUTIS_STRING, c->argv[2]);
    if (DictFind(set->ptr, el)) {
      AddReply(c, shared.one);
    } else {
      AddReply(c, shared.zero);
    }
    el->ptr = NULL;
    DecrRefCount(el);
  }
}

void SCardCommand(CutisClient *c) {
  DictEntry *de = DictFind(c->dict, c->argv[1]);

  if (!de) {
    AddReply(c, shared.zero);
    return;
  } else {
    CutisObject *set = DictGetEntryVal(de);
    if (set->type != CUTIS_SET) {
      AddReplySds(c, sdsnew("-1\r\n"));
    } else {
      Dict *d = set->ptr;
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n",
                                  DictGetHashTableUsed(d)));
    }
  }
}

static int qsortCompareSetsByCardinality(const void *s1, const void *s2) {
  Dict **d1 = (void*)s1, **d2 = (void*)s2;
  return (int)DictGetHashTableUsed(*d1) - (int)DictGetHashTableUsed(*d2);
}

void SInterCommand(CutisClient *c) {
  Dict **dv = zmalloc(sizeof(Dict*) * (c->argc - 1));
  DictIterator *di;
  DictEntry *de;
  CutisObject *lenobj;
  int j, cardinality = 0;

  if (!dv) {
    CutisOom("sinterCommand");
  }

  for (j = 0; j < c->argc - 1; j++) {
    CutisObject *set;
    DictEntry *de;
    de = DictFind(c->dict, c->argv[j+1]);
    if (!de) {
      char *err = "No such key";
      zfree(dv);
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n%s\r\n",
                                  -(int)strlen(err), err));
      return;
    }
    set = DictGetEntryVal(de);
    if (set->type != CUTIS_SET) {
      char *err = "SINTER against key not holding a set value";
      AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n%d\r\n",
                                  -(int)strlen(err), err));
      zfree(dv);
    }
    dv[j] = set->ptr;
  }

  // Sort sets from the smallest to largest, this will improve our
  // algorithm's performance.
  qsort(dv, c->argc-1, sizeof(Dict*), qsortCompareSetsByCardinality);

  // The first thing we should output is the total number of elements.
  // Since this is a multi-bulk write, but at this stage we don't know
  // the intersection set size, so we use a trick, append an empty
  // object to the output list and save the pointer to later modify
  // it with the right length.
  lenobj = CreateCutisObject(CUTIS_STRING, NULL);
  AddReply(c, lenobj);
  DecrRefCount(lenobj);

  // Iterate all the elements of the first (smallest) set, and test
  // the element against all the other sets, if at least one set does
  // not include the element it is discarded.
  di = DictGetIterator(dv[0]);
  if (!di) {
    CutisOom("DictGetIterator");
  }

  while ((de = DictNext(di)) != NULL) {
    CutisObject *el;
    for (j = 1; j < c->argc - 1; j++) {
      if (DictFind(dv[j], DictGetEntryKey(de)) == NULL) {
        break;
      }
    }
    if (j != c->argc - 1) {
      // At least one set don't contain the member.
      continue;
    }
    el = DictGetEntryKey(de);
    AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n", sdslen(el->ptr)));
    AddReply(c, el);
    AddReply(c, shared.crlf);
    cardinality++;
  }
  lenobj->ptr = sdscatprintf(sdsempty(), "%d\r\n", cardinality);
  DictReleaseIterator(di);
  zfree(dv);
}

void TypeCommand(CutisClient *c) {
  char *type;
  DictEntry *de = DictFind(c->dict, c->argv[1]);
  if (!de) {
    type = "none";
  } else {
    CutisObject *o = DictGetEntryVal(de);
    switch (o->type) {
    case CUTIS_STRING:
      type = "string";
      break;
    case CUTIS_LIST:
      type = "list";
      break;
    case CUTIS_SET:
      type = "set";
      break;
    default:
      type = "unknown";
      break;
    }
  }
  AddReplySds(c, sdsnew(type));
  AddReply(c, shared.crlf);
}

void SelectCommand(CutisClient *c) {
  int id = atoi(c->argv[1]);
  if (SelectDB(c, id) == CUTIS_ERR) {
    AddReplySds(c, sdsnew("-ERR invalid DB index\r\n"));
  } else {
    AddReply(c, shared.ok);
  }
}

void MoveCommand(CutisClient *c) {
  Dict *src, *dst;
  DictEntry *de;
  sds key;
  CutisObject *o;

  // Obtain source and target DB pointers.
  src = c->dict;
  if (SelectDB(c, atoi(c->argv[2])) == CUTIS_ERR) {
    AddReplySds(c, sdsnew("-ERR target DB out of range\r\n"));
    return;
  }
  dst = c->dict;
  c->dict = src;

  // If the user is moving using as target the same
  // DB as the source DB it is probably an error.
  if (src == dst) {
    AddReplySds(c, sdsnew("-ERR source DB is the same as target DB\r\n"));
    return;
  }

  // Check if the element exists and get a reference.
  de = DictFind(c->dict, c->argv[1]);
  if (!de) {
    AddReplySds(c, sdsnew("-ERR no such key\r\n"));
    return;
  }

  // Try to add the element to the target DB.
  key = DictGetEntryKey(de);
  o = DictGetEntryVal(de);
  if (DictAdd(dst, key, o) == DICT_ERR) {
    AddReplySds(c, sdsnew("-ERR target DB already contains the moved key\r\n"));
    return;
  }

  // OK. key moved, free the entry in the source DB.
  DictDeleteNoFree(src, c->argv[1]);
  c->server->dirty++;
  AddReply(c, shared.ok);
}

static void RenameGenericCommand(CutisClient *c, int nx) {
  DictEntry *de;
  CutisObject *o;

  // To use the same key as src and dst is probably an error.
  if (sdscmp(c->argv[1], c->argv[2]) == 0) {
    AddReplySds(c, sdsnew("-ERR src and dest key are the same\r\n"));
    return;
  }

  de = DictFind(c->dict, c->argv[1]);
  if (!de) {
    AddReplySds(c, sdsnew("-ERR no such key\r\n"));
    return;
  }

  o = DictGetEntryVal(de);
  IncrRefCount(o);
  if (DictAdd(c->dict, c->argv[2], o) == DICT_ERR) {
    if (nx) {
      DecrRefCount(o);
      AddReplySds(c, sdsnew("-ERR destination key exists\r\n"));
      return;
    }
    DictReplace(c->dict, c->argv[2], o);
  } else {
    c->argv[2] = NULL;
  }
  DictDelete(c->dict, c->argv[1]);
  c->server->dirty++;
  AddReply(c, shared.ok);
}

void RenameCommand(CutisClient *c) {
  RenameGenericCommand(c, 0);
}

void RenamenxCommand(CutisClient *c) {
  RenameGenericCommand(c, 1);
}

void KeysCommand(CutisClient *c) {
  DictIterator *di = NULL;
  DictEntry *de = NULL;
  sds keys, reply;
  sds pattern = c->argv[1];
  int plen = sdslen(pattern);

  di = DictGetIterator(c->dict);
  keys = sdsempty();

  while ((de = DictNext(di)) != NULL) {
    sds key = DictGetEntryKey(de);
    if ((pattern[0] == '*' && pattern[1] == '\0') ||
        StringMatch(pattern, plen, key, sdslen(key), 0)) {
      keys = sdscatlen(keys, key, sdslen(key));
      keys = sdscatlen(keys, " ", 1);
    }
  }
  DictReleaseIterator(di);
  keys = sdstrim(keys, " ");
  reply = sdscatprintf(sdsempty(), "%lu\r\n", sdslen(keys));
  reply = sdscatlen(reply, keys, sdslen(keys));
  reply = sdscatlen(reply, "\r\n", 2);
  sdsfree(keys);
  AddReplySds(c, reply);
}

void DbsizeCommand(CutisClient *c) {
  AddReplySds(c, sdscatprintf(sdsempty(), "%lu\r\n",
                              DictGetHashTableUsed(c->dict)));
}

void SaveCommand(CutisClient *c) {
  if (SaveDB(c->server, CUTIS_DB_NAME) == CUTIS_OK) {
    AddReply(c, shared.ok);
  } else {
    AddReply(c, shared.err);
  }
}

void BgsaveCommand(CutisClient *c) {
  if (c->server->bg_saving) {
    AddReplySds(c, sdsnew("-ERR background save already in process\r\n"));
    return;
  }
  if (SaveDBBackground(c->server, CUTIS_DB_NAME) == CUTIS_OK) {
    AddReply(c, shared.ok);
  } else {
    AddReply(c, shared.err);
  }
}

void ShutDownCommand(CutisClient *c) {
  CutisLog(CUTIS_WARNING, "User requested shutdown, saving DB...");
  if (SaveDB(c->server, CUTIS_DB_NAME) == CUTIS_OK) {
    CutisLog(CUTIS_WARNING, "Server exit now, bye bye...");
    AeStop(c->server->el);
  } else {
    CutisLog(CUTIS_WARNING, "Error trying to save the DB, can't exit");
    AddReplySds(c, sdsnew("-ERR can't quit, problems saving the DB\r\n"));
  }
}

void PingCommand(CutisClient *c) {
  AddReply(c, shared.pong);
}

void EchoCommand(CutisClient *c) {
  AddReplySds(c, sdscatprintf(sdsempty(), "%d\r\n",
                              (int)sdslen(c->argv[1])));
  AddReplySds(c, c->argv[1]);
  AddReply(c, shared.crlf);
  c->argv[1] = NULL;
}

void LastSaveCommand(CutisClient *c) {
  AddReplySds(c, sdscatprintf(sdsempty(), "%lu\r\n",
                              c->server->last_save));
}
