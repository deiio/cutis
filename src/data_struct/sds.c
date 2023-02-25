/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "data_struct/sds.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory/zmalloc.h"

static void sdsOOMAbort() {
  fprintf(stderr, "SDS: Out Of Memory (SDS_ABORT_ON_OOM defined)\n");
  abort();
}

static sds sdsMakeRoorFor(sds s, size_t addlen) {
  sds_hdr *sh, *newsh;
  size_t free = sdsavail(s);
  size_t len, newlen;

  if (free >= addlen) {
    return s;
  }

  len = sdslen(s);
  sh = (void*) (s - sizeof(sds_hdr));
  newlen = (len + addlen) * 2;
  newsh = zrealloc(sh, sizeof(sds_hdr) + newlen + 1);
  if (newsh == NULL) {
#ifdef SDS_ABORT_ON_OOM
    sdsOOMAbort();
#else
    return NULL;
#endif
  }
  newsh->free = newlen - len;
  return newsh->buf;
}

sds sdsnewlen(const void *init, size_t initlen) {
  sds_hdr *sh;
  sh = zmalloc(sizeof(sds_hdr) + initlen + 1);
  if (sh == NULL) {
#ifdef SDS_ABORT_ON_OOM
    sdsOOMAbort();
#else
    return NULL;
#endif
  }

  sh->len = initlen;
  sh->free = 0;
  if (initlen) {
    if (init) {
      memcpy(sh->buf, init, initlen);
    } else {
      memset(sh->buf, 0, initlen);
    }
  }
  sh->buf[initlen] = '\0';
  return (char*)sh->buf;
}

sds sdsnew(const char *init) {
  size_t initlen = (init == NULL) ? 0 : strlen(init);
  return sdsnewlen(init, initlen);
}

sds sdsempty() {
  return sdsnewlen("", 0);
}

size_t sdslen(const sds s) {
  sds_hdr *sh = (void*) (s - sizeof(sds_hdr));
  return sh->len;
}

sds sdsdup(const sds s) {
  return sdsnewlen(s, sdslen(s));
}

void sdsfree(sds s) {
  if (s == NULL) {
    return;
  }
  zfree(s - sizeof(sds_hdr));
}

size_t sdsavail(sds s) {
  sds_hdr *sh = (void*) (s - sizeof(sds_hdr));
  return sh->free;
}

sds sdscatlen(sds s, void *t, size_t len) {
  sds_hdr *sh;
  size_t curlen = sdslen(s);

  s = sdsMakeRoorFor(s, len);
  if (s == NULL) {
    return NULL;
  }
  sh = (void*) (s - sizeof(sds_hdr));
  memcpy(s + curlen, t, len);
  sh->len = curlen + len;
  sh->free -= len;
  s[sh->len] = '\0';
  return s;
}

sds sdscat(sds s, char *t) {
  return sdscatlen(s, t, strlen(t));
}

sds sdscpylen(sds s, char *t, size_t len) {
  sds_hdr *sh = (void*) (s - sizeof(sds_hdr));
  size_t totlen = sh->free + sh->len;

  if (totlen < len) {
    s = sdsMakeRoorFor(s, len - totlen);
    if (s == NULL) {
      return NULL;
    }
    sh = (void*) (s - sizeof(sds_hdr));
    totlen = sh->free + sh->len;
  }
  memcpy(s, t, len);
  s[len] = '\0';
  sh->len = len;
  sh->free = totlen - len;
  return s;
}

sds sdscpy(sds s, char *t) {
  return sdscpylen(s, t, strlen(t));
}

sds sdscatprintf(sds s, const char *fmt, ...) {
  va_list ap;
  char *buf;
  char *t;
  size_t buflen = 32;

  while (1) {
    int len;
    buf = zmalloc(buflen);
    if (buf == NULL) {
#ifdef SDS_ABORT_ON_OOM
      sdsOOMAbort();
#else
      return NULL;
#endif
    }
    va_start(ap, fmt);
    len = vsnprintf(buf, buflen, fmt, ap);
    va_end(ap);
    if (len > 0 && (size_t)len >= buflen) {
      zfree(buf);
      buflen = len;
      continue;
    }
    break;
  }
  t = sdscat(s, buf);
  zfree(buf);
  return t;
}

sds sdstrim(sds s, const char *cset) {
  sds_hdr *sh = (void*) (s - sizeof(sds_hdr));
  char *start, *end, *sp, *ep;
  size_t len;

  sp = start = s;
  ep = end = s + sdslen(s) - 1;
  while (sp <= end && strchr(cset, *sp)) {
    sp++;
  }
  while (ep >= start && strchr(cset, *ep)) {
    ep--;
  }
  len = (sp > ep) ? 0 : ((ep - sp) + 1);
  if (sh->buf != sp) {
    memmove(sh->buf, sp, len);
  }
  sh->buf[len] = '\0';
  sh->free = sh->free + (sh->len - len);
  sh->len = len;
  return s;
}

sds sdsrange(sds s, long start, long end) {
  sds_hdr *sh = (void*) (s - sizeof(sds_hdr));
  size_t newlen;
  size_t len = sdslen(s);

  if (len == 0) {
    return s;
  }
  if (start < 0) {
    start = len + start;
    if (start < 0) {
      start = 0;
    }
  }
  if (end < 0) {
    end = len + end;
    if (end < 0) {
      end = 0;
    }
  }
  newlen = (start > end) ? 0 : (end - start) + 1;
  if (newlen != 0) {
    if (start >= (long)len) {
      start = len - 1;
    }
    if (end >= (long)len) {
      end = end - 1;
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
  } else {
    start = 0;
  }

  if (start != 0) {
    memmove(sh->buf, sh->buf + start, newlen);
  }

  sh->buf[newlen] = '\0';
  sh->free += sh->len - newlen;
  sh->len = newlen;
  return s;
}

void sdsupdatelen(sds s) {
  sds_hdr *sh = (void*) (s - sizeof(sds_hdr));
  int reallen = strlen(s);
  sh->free += (sh->len - reallen);
  sh->len = reallen;
}

int sdscmp(sds s1, sds s2) {
  size_t l1, l2;
  size_t minlen;
  int cmp;

  l1 = sdslen(s1);
  l2 = sdslen(s2);
  minlen = (l1 < l2) ? l1 : l2;
  cmp = memcmp(s1, s2, minlen);
  if (cmp == 0) {
    return l1 - l2;
  }
  return cmp;
}

/**
 * Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 */
sds *sdssplitlen(char *s, int len, char *sep, int seplen, int *count) {
  int elements = 0;
  int slots = 5;
  int start = 0;
  int j;
  sds *tokens = zmalloc(sizeof(sds) * slots);
  int clean_up = 0;

#ifdef SDS_ABORT_ON_OOM
  if (tokens == NULL) {
    sdsOOMAbort();
  }
#endif
  if (seplen < 1 || len < 0 || tokens == NULL) {
    return NULL;
  }
  for (j = 0; j < (len - (seplen - 1)); j++) {
    // make sure there is room for the next element and the final one
    if (slots < elements + 2) {
      slots *= 2;
      sds *newtokens = zrealloc(tokens, sizeof(sds) * slots);
      if (newtokens == NULL) {
        clean_up = 1;
#ifdef SDS_ABORT_ON_OOM
        sdsOOMAbort();
#else
        break;
#endif
      }
      tokens = newtokens;
    }
    // search the separator
    if ((seplen == 1 && *(s + j) == sep[0]) ||
        (memcmp(s + j, sep, seplen) == 0)) {
      tokens[elements] = sdsnewlen(s + start, j - start);
      if (tokens[elements] == NULL) {
        clean_up = 1;
#ifdef SDS_ABORT_ON_OOM
        sdsOOMAbort();
#else
        break;
#endif
      }
      elements++;
      start = j + seplen;
      j = j + seplen - 1;  // skip the separator
    }
  }

  if (!clean_up) {
    // add the final element. We are sure there is room in the tokens array.
    tokens[elements] = sdsnewlen(s + start, len - start);
    if (tokens[elements] != NULL) {
      elements++;
      *count = elements;
      return tokens;
    }
  }

  // cleanup
  {
    int i;
    for (i = 0; i < elements; i++) {
      sdsfree(tokens[i]);
    }
    zfree(tokens);
    return NULL;
  }
}

void sdstolower(sds s) {
  size_t len = sdslen(s);
  size_t j;
  for (j = 0; j < len; j++) {
    s[j] = (char)tolower(s[j]);
  }
}

void sdstoupper(sds s) {
  size_t len = sdslen(s);
  size_t j;
  for (j = 0; j < len; j++) {
    s[j] = (char)toupper(s[j]);
  }
}
