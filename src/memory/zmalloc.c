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

#include "memory/zmalloc.h"

#include <stdlib.h>
#include <string.h>

static size_t used_memory = 0;

void *zmalloc(size_t size) {
  void *ptr = malloc(size + sizeof(size_t));
  *((size_t*)ptr) = size;
  used_memory += size + sizeof(size_t);
  return ptr + sizeof(size_t);
}

void *zrealloc(void *ptr, size_t size) {
  void *realptr;
  size_t oldsize;
  void *newptr;

  if (ptr == NULL) {
    return zmalloc(size);
  }

  realptr = ptr - sizeof(size_t);
  oldsize = *((size_t*)realptr);
  newptr = realloc(realptr, size + sizeof(size_t));
  if (!newptr) {
    return NULL;
  }

  *((size_t*)newptr) = size;
  used_memory -= oldsize;
  used_memory += size;
  return newptr + sizeof(size_t);
}

void zfree(void *ptr) {
  void *realptr;
  size_t oldsize;

  if (ptr == NULL) {
    return;
  }

  realptr = ptr - sizeof(size_t);
  oldsize = *((size_t*)realptr);
  used_memory -= oldsize + sizeof(size_t);
  free(realptr);
}

char *zstrdup(const char *s) {
  size_t l = strlen(s) + 1;
  char *p = zmalloc(l);

  memcpy(p, s, l);
  return p;
}

size_t zmalloc_used_memory() {
  return used_memory;
}
