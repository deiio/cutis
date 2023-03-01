/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef COMMANDS_COMMAND_H_
#define COMMANDS_COMMAND_H_

typedef struct CutisClient CutisClient;

// A cutis object, that holds a string
typedef struct CutisObject {
  void *ptr;
} CutisObject;

CutisObject *CreateCutisObject(int type, void *ptr);
int ProcessCommand(CutisClient *c);

#endif  // COMMANDS_COMMAND_H_
