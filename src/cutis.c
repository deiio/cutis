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

#include <stdio.h>

#include "server/server.h"
#include "utils/log.h"
#include "version.h"

int main(int argc, char *argv[]) {
  CutisServer *server = GetSingletonServer();
  InitServerConfig(server);
  if (argc == 2) {
    ResetServerSaveParams(server);
    if (LoadServerConfig(server, argv[1]) == CUTIS_ERR) {
      return 1;
    }
  } else if (argc > 2) {
    fprintf(stderr, "Usage: %s [/path/to/cutis.conf]\n", argv[0]);
    return 1;
  }

  InitServer(server);

  if (LoadDB(server, CUTIS_DB_NAME) == CUTIS_OK) {
    CutisLog(CUTIS_NOTICE, "DB loaded from disk");
  }
  CutisLog(CUTIS_NOTICE, "Server started, Cutis version " CUTIS_VERSION);
  if (StartServer(server) != CUTIS_OK) {
    CutisLog(CUTIS_WARNING, "Server started failed");
  }
  CleanServer(server);

  return 0;
}
