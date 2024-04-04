/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef COMMANDS_COMMAND_H_
#define COMMANDS_COMMAND_H_

#define CUTIS_CMD_INLINE  0
#define CUTIS_CMD_BULK    1

// Object types.
#define CUTIS_STRING      0
#define CUTIS_LIST        1
#define CUTIS_SET         2

#define CUTIS_MAX_STRING_LENGTH 1024*1024*1024

typedef struct CutisClient CutisClient;

// A cutis object, that holds a string
typedef struct CutisObject {
  int type;
  void *ptr;
  int refcount;
} CutisObject;

typedef void CutisCommandProc(CutisClient *c);
typedef struct CutisCommand {
  char *name;
  CutisCommandProc *proc;
  int arity;
  int type;
} CutisCommand;

CutisObject *CreateCutisObject(int type, void *ptr);
CutisObject *CreateListObject();
void FreeStringObject(CutisObject *o);
void FreeListObject(CutisObject *o);
void FreeSetObject(CutisObject *o);
void IncrRefCount(CutisObject *o);
void DecrRefCount(CutisObject *o);

void InitSharedObjects();

int ProcessCommand(CutisClient *c);
CutisCommand *LookupCommand(char *name);

// Commands implementation.
void GetCommand(CutisClient *c);
void SetCommand(CutisClient *c);

#endif  // COMMANDS_COMMAND_H_
