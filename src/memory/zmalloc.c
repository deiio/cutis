/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
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
