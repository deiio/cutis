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

#ifndef COMMANDS_COMMAND_H_
#define COMMANDS_COMMAND_H_

#define CUTIS_CMD_INLINE  0
#define CUTIS_CMD_BULK    1

#define CUTIS_HEAD        0
#define CUTIS_TAIL        1

#define CUTIS_MAX_STRING_LENGTH 1024*1024*1024

typedef struct CutisClient CutisClient;

typedef void CutisCommandProc(CutisClient *c);
typedef struct CutisCommand {
  char *name;
  CutisCommandProc *proc;
  int arity;
  int type;
} CutisCommand;

int ProcessCommand(CutisClient *c);
CutisCommand *LookupCommand(char *name);

// Commands implementation.
void GetCommand(CutisClient *c);
void SetCommand(CutisClient *c);
void SetnxCommand(CutisClient *c);
void ExistsCommand(CutisClient *c);
void DelCommand(CutisClient *c);
void IncrCommand(CutisClient *c);
void DecrCommand(CutisClient *c);
void RandomKeyCommand(CutisClient *c);

void RPushCommand(CutisClient *c);
void LPushCommand(CutisClient *c);
void RPopCommand(CutisClient *c);
void LPopCommand(CutisClient *c);
void LLenCommand(CutisClient *c);
void LIndexCommand(CutisClient *c);
void LRangeCommand(CutisClient *c);
void LTrimCommand(CutisClient *c);
void LSetCommand(CutisClient *c);

void SAddCommand(CutisClient *c);
void SRemCommand(CutisClient *c);
void SIsMemberCommand(CutisClient *c);
void SCardCommand(CutisClient *c);
void SInterCommand(CutisClient *c);

void TypeCommand(CutisClient *c);
void SelectCommand(CutisClient *c);
void MoveCommand(CutisClient *c);
void RenameCommand(CutisClient *c);
void RenamenxCommand(CutisClient *c);
void KeysCommand(CutisClient *c);
void DbsizeCommand(CutisClient *c);
void SaveCommand(CutisClient *c);
void BgsaveCommand(CutisClient *c);
void ShutDownCommand(CutisClient *c);
void PingCommand(CutisClient *c);
void EchoCommand(CutisClient *c);
void LastSaveCommand(CutisClient *c);

#endif  // COMMANDS_COMMAND_H_
