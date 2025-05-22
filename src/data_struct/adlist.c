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

#include "data_struct/adlist.h"

#include "memory/zmalloc.h"

List *listCreate() {
  List *list;
  if ((list = zmalloc(sizeof(*list))) == NULL) {
    return NULL;
  }
  list->head = NULL;
  list->tail = NULL;
  list->len = 0;
  list->dup = NULL;
  list->free = NULL;
  list->match = NULL;
  return list;
}

void listRelease(List *list) {
  unsigned int len;
  ListNode *current;

  current = list->head;
  len = list->len;
  while (len--) {
    ListNode *next = current->next;
    if (list->free) {
      list->free(current->value);
    }
    zfree(current);
    current = next;
  }
  zfree(list);
}

List *listAddNodeHead(List *list, void *value) {
  ListNode *node;

  if ((node = zmalloc(sizeof(*node))) == NULL) {
    return NULL;
  }
  node->value = value;
  if (list->len == 0) {
    list->head = node;
    list->tail = node;
    node->prev = NULL;
    node->next = NULL;
  } else {
    node->prev = NULL;
    node->next = list->head;
    list->head->prev = node;
    list->head = node;
  }
  list->len++;
  return list;
}

List *listAddNodeTail(List *list, void *value) {
  ListNode *node;

  if ((node = zmalloc(sizeof(*node))) == NULL) {
    return NULL;
  }

  node->value = value;
  if (list->len == 0) {
    list->head = node;
    list->tail = node;
    node->prev = NULL;
    node->next = NULL;
  } else {
    node->prev = list->tail;
    node->next = NULL;
    list->tail->next = node;
    list->tail = node;
  }
  list->len++;
  return list;
}

void listDelNode(List *list, ListNode *node) {
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    list->head = node->next;
  }

  if (node->next) {
    node->next->prev = node->prev;
  } else {
    list->tail = node->prev;
  }

  if (list->free) {
    list->free(node->value);
  }
  zfree(node);
  list->len--;
}

ListIter *listGetIterator(List *list, int direction) {
  ListIter *iter;

  if ((iter = zmalloc(sizeof(*iter))) == NULL) {
    return NULL;
  }
  if (direction == AL_START_HEAD) {
    iter->next = list->head;
  } else {
    iter->next = list->tail;
  }
  iter->direction = direction;
  return iter;
}

ListNode *listNextElement(ListIter *iter) {
  ListNode *current = iter->next;
  if (current != NULL) {
    if (iter->direction == AL_START_HEAD) {
      iter->next = current->next;
    } else {
      iter->next = current->prev;
    }
  }
  return current;
}

void listReleaseIterator(ListIter *iter) {
  zfree(iter);
}

List *listDup(List *orig) {
  List *copy;
  ListIter *iter;
  ListNode *node;

  if ((copy = listCreate()) == NULL) {
    return NULL;
  }
  copy->dup = orig->dup;
  copy->free = orig->free;
  copy->match = orig->match;

  iter = listGetIterator(orig, AL_START_HEAD);
  while ((node = listNextElement(iter)) != NULL) {
    void *value;

    if (copy->dup) {
      value = copy->dup(node->value);
      if (value == NULL) {
        listRelease(copy);
        listReleaseIterator(iter);
        return NULL;
      }
    } else {
      value = node->value;
    }

    if (listAddNodeTail(copy, value) == NULL) {
      listRelease(copy);
      listReleaseIterator(iter);
      return NULL;
    }
  }
  listReleaseIterator(iter);
  return copy;
}

ListNode *listSearchKey(List *list, void *key) {
  ListIter *iter;
  ListNode *node;

  iter = listGetIterator(list, AL_START_HEAD);
  while ((node = listNextElement(iter)) != NULL) {
    if (list->match) {
      if (list->match(node->value, key)) {
        listReleaseIterator(iter);
        return node;
      }
    } else {
      if (key == node->value) {
        listReleaseIterator(iter);
        return node;
      }
    }
  }
  listReleaseIterator(iter);
  return NULL;
}

ListNode *listIndex(List *list, int index) {
  ListNode *node;

  if (index < 0) {
    index = (-index) - 1;
    node = list->tail;
    while (index-- && node) {
      node = node->prev;
    }
  } else {
    node = list->head;
    while (index-- && node) {
      node = node->next;
    }
  }
  return node;
}
