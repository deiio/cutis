/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "commands/command.h"

#include <string.h>

#include "memory/zmalloc.h"
#include "server/client.h"
#include "utils/log.h"

static CutisCommand cmd_table[] = {
    {"", NULL, 0, 0},
};

int ProcessCommand(CutisClient *c) {
  CutisCommand *cmd;

  sdstolower(c->argv[0]->ptr);
  // The QUIT command is handled as a special case. Normal command
  // procs are unable to close the client connection safely
  if (!strcmp(c->argv[0]->ptr, "quit")) {
    FreeClient(c);
    return 0;
  }

  cmd = LookupCommand(c->argv[0]->ptr);
  if (cmd == NULL) {
    AddReplySds(c, sdsnew("-ERR unknown command\r\n"));
    ResetClient(c);
    return 1;
  } else if (cmd->arity != c->argc) {
    AddReplySds(c, sdsnew("-ERR wrong number of arguments\r\n"));
    ResetClient(c);
    return 1;
  } else if (cmd->type == CUTIS_CMD_BULK && c->bulk_len == -1) {
    //
  }

  // Exec the command
  cmd->proc(c);
  ResetClient(c);
  return 1;
}

CutisCommand *LookupCommand(const char *name) {
  int i = 0;
  while (cmd_table[i].name != NULL) {
    if (strcmp(name, cmd_table[i].name) == 0) {
      return &cmd_table[i];
    }
    i++;
  }
  return NULL;
}
