/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "commands/object.h"

#include "memory/zmalloc.h"
#include "utils/log.h"

CutisObject *CreateCutisObject(int type, void *ptr) {
  CutisObject *o = zmalloc(sizeof(CutisObject));
  if (o == NULL) {
    CutisOom("CreateCutisObject");
  }
  o->ptr = ptr;
  return o;
}
