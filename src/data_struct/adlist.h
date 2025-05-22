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

#ifndef ADLIST_H_
#define ADLIST_H_

// Node, List, and Iterator are the only data structures used currently

typedef struct ListNode {
  struct ListNode *prev;
  struct ListNode *next;
  void *value;
} ListNode;

typedef struct List {
  ListNode *head;
  ListNode *tail;
  void *(*dup)(void *ptr);
  void (*free)(void *ptr);
  int (*match)(void *ptr, void *key);
  unsigned int len;
} List;

typedef struct ListIter {
  ListNode *next;
  ListNode *prev;
  int direction;
} ListIter;

// Functions implemented as macros
#define listLength(l) ((l)->len)
#define listFirst(l) ((l)->head)
#define listLast(l) ((l)->tail)
#define listPrevNode(n) ((n)->prev)
#define listNextNode(n) ((n)->next)
#define listNodeValue(n) ((n)->value)

#define listSetDupMethod(l, m) ((l)->dup = (m))
#define listSetFreeMethod(l, m) ((l)->free = (m))
#define listSetMatchMethod(l, m) ((l)->match = (m))

#define listGetDupMethod(l) ((l)->dup)
#define listGetFreeMethod(l) ((l)->free)
#define listGetMatchMethod(l) ((l)->match)

// Prototypes
List *listCreate();
void listRelease(List *list);

List *listAddNodeHead(List *list, void *value);
List *listAddNodeTail(List *list, void *value);

void listDelNode(List *list, ListNode *node);

ListIter *listGetIterator(List *list, int direction);
ListNode *listNextElement(ListIter *iter);
void listReleaseIterator(ListIter *iter);

List *listDup(List *orig);

ListNode *listSearchKey(List *list, void *key);
ListNode *listIndex(List *list, int index);

// Directions for iterators
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif  // ADLIST_H_
