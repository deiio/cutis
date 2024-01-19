/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef COMMANDS_OBJECT_H_
#define COMMANDS_OBJECT_H_

// Object types
#define CUTIS_OBJECT_STRING   0
#define CUTIS_OBJECT_LIST     1
#define CUTIS_OBJECT_SET      2

// A cutis object, that holds a string
typedef struct CutisObject {
  int type;
  int ref_count;
  void *ptr;
} CutisObject;

CutisObject *CreateCutisObject(int type, void *ptr);

#endif  // COMMANDS_OBJECT_H_
