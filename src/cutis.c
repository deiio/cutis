/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include <stdio.h>

#include "server/server.h"
#include "utils/log.h"
#include "version.h"

int main(int argc, char *argv[]) {
  CutisServer *server = GetSingletonServer();
  InitServerConfig(server);
  if (argc == 2) {
    if (LoadServerConfig(server, argv[1]) == CUTIS_ERR) {
      return 1;
    }
  } else if (argc > 2) {
    fprintf(stderr, "Usage: %s [/path/to/cutis.conf]\n", argv[0]);
    return 1;
  }

  InitServer(server);
  CutisLog(CUTIS_NOTICE, "Server started, Cutis version " CUTIS_VERSION);
  if (StartServer(server) != CUTIS_OK) {
    CutisLog(CUTIS_WARNING, "Server started failed");
  }
  CleanServer(server);

  return 0;
}
