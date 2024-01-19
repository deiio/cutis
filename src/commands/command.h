/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef COMMANDS_COMMAND_H_
#define COMMANDS_COMMAND_H_

#include "commands/object.h"

#define CUTIS_CMD_BULK    1
#define CUTIS_CMD_INLINE  0

typedef struct CutisClient CutisClient;

typedef void CutisCommandProc(CutisClient *c);
typedef struct CutisCommand {
    char *name;
    CutisCommandProc *proc;
    int arity;
    int type;
} CutisCommand;

int ProcessCommand(CutisClient *c);
CutisCommand *LookupCommand(const char *name);

#endif  // COMMANDS_COMMAND_H_
