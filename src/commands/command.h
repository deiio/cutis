/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef COMMANDS_COMMAND_H_
#define COMMANDS_COMMAND_H_

#define CUTIS_CMD_INLINE  0
#define CUTIS_CMD_BULK    1

#define CUTIS_MAX_STRING_LENGTH 1024*1024*1024

typedef struct CutisClient CutisClient;

typedef void CutisCommandProc(CutisClient *c);
typedef struct CutisCommand {
  char *name;
  CutisCommandProc *proc;
  int arity;
  int type;
} CutisCommand;

int ProcessCommand(CutisClient *c);
CutisCommand *LookupCommand(char *name);

// Commands implementation.
void GetCommand(CutisClient *c);
void SetCommand(CutisClient *c);

#endif  // COMMANDS_COMMAND_H_
