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

#include "utils/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "server/server.h"

void CutisLog(int level, const char *fmt, ...) {
  va_list ap;
  FILE *file;
  CutisServer *server = GetSingletonServer();

  file = (server->log_file == NULL) ? stdout : fopen(server->log_file, "a");
  if (!file) {
    return;
  }

  va_start(ap, fmt);
  if (level >= server->verbosity) {
    char *c = ".-*";
    fprintf(file, "%c ", c[level]);
    vfprintf(file, fmt, ap);
    fprintf(file, "\n");
    fflush(file);
  }
  va_end(ap);

  if (server->log_file) {
    fclose(file);
  }
}

// Cutis generally does not try to recover from out of memory conditions
// when allocating objects or strings, it is not clear if it will be possible
// to report this condition to the client since the networking layer itself
// is based on heap allocation for send buffers, so we simply abort.
// At least the code will be simpler to read...
void CutisOom(const char *msg) {
  fprintf(stderr, "%s: Out of memory\n", msg);
  fflush(stderr);
  sleep(1);
  abort();
}
