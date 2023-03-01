/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "commands/command.h"

#include <string.h>

#include "memory/zmalloc.h"
#include "server/client.h"
#include "utils/log.h"

CutisObject *CreateCutisObject(int type, void *ptr) {
  CutisObject *o = zmalloc(sizeof(CutisObject));
  if (o == NULL) {
    CutisOom("CreateCutisObject");
  }
  o->ptr = ptr;
  return o;
}

int ProcessCommand(CutisClient *c) {
  int i;
  sdstolower(c->argv[0]->ptr);
  if (!strcmp(c->argv[0]->ptr, "quit")) {
    FreeClient(c);
    return 0;
  }

  for (i = 0; i < c->argc; i++) {
    listAddNodeTail(c->reply, c->argv[i]);
  }
  c->argc = 0;

  AddReply(c);
  return 1;
}
