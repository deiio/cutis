/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "server/server.h"
#include "utils/log.h"
#include "version.h"

int main() {
  CutisServer *server = GetSingletonServer();
  InitServerConfig(server);
  InitServer(server);
  CutisLog(CUTIS_NOTICE, "Server started, Cutis version " CUTIS_VERSION);
  return ServerStart(server);
}
