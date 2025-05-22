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

#ifndef AE_H_
#define AE_H_

struct AeEventLoop;

/* Types and data structures */
typedef int AeFileProc(struct AeEventLoop *event_loop, int fd,
                       void *client_data, int mask);
typedef int AeTimeProc(struct AeEventLoop *event_loop,
                       long long id, void *client_data);
typedef int AeEventFinalizerProc(struct AeEventLoop *event_loop,
                                 void *client_data);

// File event structure
typedef struct AeFileEvent {
  int fd;
  int mask; // one of AE_(READABLE|WRITABLE|EXCEPTION)
  AeFileProc *file_proc;
  AeEventFinalizerProc *finalizer_proc;
  void *client_data;
  struct AeFileEvent *next;
} AeFileEvent;

// Time event structure
typedef struct AeTimeEvent {
  long long id;  // time event identifier
  long when_sec;  // second
  long when_ms;   // milliseconds
  AeTimeProc *time_proc;
  AeEventFinalizerProc *finalizer_proc;
  void *client_data;
  struct AeTimeEvent *next;
} AeTimeEvent;

// State of an event based program
typedef struct AeEventLoop {
  long long time_event_next_id;
  AeFileEvent *file_event_head;
  AeTimeEvent *time_event_head;
  int stop;
} AeEventLoop;

// Defines
#define AE_OK 0
#define AE_ERR -1

#define AE_READABLE 1
#define AE_WRITABLE 2
#define AE_EXCEPTION 4

#define AE_FILE_EVENTS 1
#define AE_TIME_EVENTS 2
#define AE_ALL_EVENTS (AE_FILE_EVENTS | AE_TIME_EVENTS)
#define AE_DONT_WAIT 4

#define AE_NOMORE -1

#define AE_NOT_USED(v) ((void)v)

AeEventLoop *AeCreateEventLoop();
void AeDeleteEventLoop(AeEventLoop *event_loop);
void AeStop(AeEventLoop *event_loop);

int AeCreateFileEvent(AeEventLoop *event_loop, int fd, int mask,
                      AeFileProc *proc, void *client_data,
                      AeEventFinalizerProc *finalizer_proc);
void AeDeleteFileEvent(AeEventLoop *event_loop, int fd, int mask);

long long AeCreateTimeEvent(AeEventLoop *event_loop, long long milliseconds,
                            AeTimeProc *proc, void *client_data,
                            AeEventFinalizerProc *finalizer_proc);
int AeDeleteTimeEvent(AeEventLoop *event_loop, long long id);

int AeProcessEvents(AeEventLoop *event_loop, int flags);
int AeWait(int fd, int mask, long long milliseconds);

void AeMain(AeEventLoop *eventLoop);

#endif  // AE_H_
