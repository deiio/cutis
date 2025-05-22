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

#include "commands/object.h"

#include <assert.h>
#include <stdlib.h>

#include "data_struct/sds.h"
#include "memory/zmalloc.h"
#include "server/server.h"
#include "utils/log.h"

static int SetDictKeyCompare(void *priv_data, const void *key1,
                             const void *key2);
static unsigned int SetDictHashFunction(const void *key);

DictType SetDictType = {
    SetDictHashFunction,  // hash function
    NULL,  // key dup
    NULL,  // val dup
    SetDictKeyCompare,  // key compare
    sdsDictValDestructor,  // key destructor
    NULL,  // val destructor
};

SharedObject shared;

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

void ReleaseCutisObject(CutisObject *o) {
  zfree(o);
}

CutisObject *CreateListObject() {
  List *l = listCreate();
  if (!l) {
    CutisOom("CreateListObject");
  }
  listSetFreeMethod(l, (void(*)(void*))(&DecrRefCount));
  return CreateCutisObject(CUTIS_LIST, l);
}

CutisObject *CreateSetObject() {
  Dict *d = DictCreate(&SetDictType, NULL);
  if (!d) {
    CutisOom("CreateSetObject");
  }
  return CreateCutisObject(CUTIS_SET, d);
}

void FreeStringObject(CutisObject *o) {
  sdsfree(o->ptr);
}

void FreeListObject(CutisObject *o) {
  listRelease(o->ptr);
}

void FreeSetObject(CutisObject *o) {
  DictRelease(o->ptr);
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

void ReleaseSharedObjects() {
  DecrRefCount(shared.crlf);
  DecrRefCount(shared.ok);
  DecrRefCount(shared.err);
  DecrRefCount(shared.zerobulk);
  DecrRefCount(shared.nil);
  DecrRefCount(shared.zero);
  DecrRefCount(shared.one);
  DecrRefCount(shared.pong);
}

int SetDictKeyCompare(void *priv_data, const void *key1, const void *key2) {
  const CutisObject *o1 = key1, *o2 = key2;
  return sdsDictKeyCompare(priv_data, o1->ptr, o2->ptr);
}

unsigned int SetDictHashFunction(const void *key) {
  const CutisObject *o = key;
  return DictGenHashFunction(o->ptr, sdslen(o->ptr));
}
