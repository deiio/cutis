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
void ReleaseCutisObject(CutisObject *o);
CutisObject *CreateListObject();
CutisObject *CreateSetObject();
void FreeStringObject(CutisObject *o);
void FreeListObject(CutisObject *o);
void FreeSetObject(CutisObject *o);
void IncrRefCount(CutisObject *o);
void DecrRefCount(CutisObject *o);

void InitSharedObjects();
void ReleaseSharedObjects();

#endif  // COMMANDS_OBJECT_H_
