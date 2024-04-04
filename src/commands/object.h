/*
* Copyright (c) 2023 furzoom.com, All rights reserved.
* Author: mn, mn@furzoom.com
*/

#ifndef COMMANDS_OBJECT_H_
#define COMMANDS_OBJECT_H_

// Object types.
#define CUTIS_STRING      0
#define CUTIS_LIST        1
#define CUTIS_SET         2

// A cutis object, that holds a string
typedef struct CutisObject {
  int type;
  void *ptr;
  int refcount;
} CutisObject;

// Shared objects.
typedef struct SharedObject {
  CutisObject *crlf;
  CutisObject *ok;
  CutisObject *err;
  CutisObject *zerobulk;
  CutisObject *nil;
  CutisObject *zero;
  CutisObject *one;
  CutisObject *pong;
} SharedObject;

extern SharedObject shared;

CutisObject *CreateCutisObject(int type, void *ptr);
CutisObject *CreateListObject();
void FreeStringObject(CutisObject *o);
void FreeListObject(CutisObject *o);
void FreeSetObject(CutisObject *o);
void IncrRefCount(CutisObject *o);
void DecrRefCount(CutisObject *o);

void InitSharedObjects();

#endif  // COMMANDS_OBJECT_H_
