/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "ae.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include "zmalloc.h"

static AeTimeEvent *AeSearchNearestTimer(AeEventLoop *event_loop);
static void AeGetTime(long *seconds, long *milliseconds);
static void AeAddMillisecondsToNow(long long milliseconds, long *sec, long *ms);

AeEventLoop *AeCreateEventLoop() {
  AeEventLoop *event_loop;

  event_loop = zmalloc(sizeof(*event_loop));
  if (!event_loop) {
    return NULL;
  }

  event_loop->file_event_head = NULL;
  event_loop->time_event_head = NULL;
  event_loop->time_event_next_id = 0;
  event_loop->stop = 0;

  return event_loop;
}

void AeDeleteEventLoop(AeEventLoop *event_loop) {
  zfree(event_loop);
}

void AeStop(AeEventLoop *event_loop) {
  event_loop->stop = 1;
}

int AeCreateFileEvent(AeEventLoop *event_loop, int fd, int mask,
                      AeFileProc *proc, void *client_data,
                      AeEventFinalizerProc *finalizer_proc) {
  AeFileEvent *fe;

  fe = zmalloc(sizeof(*fe));
  if (fe == NULL) {
    return AE_ERR;
  }

  fe->fd = fd;
  fe->mask = mask;
  fe->file_proc = proc;
  fe->finalizer_proc = finalizer_proc;
  fe->client_data = client_data;
  fe->next = event_loop->file_event_head;
  event_loop->file_event_head = fe;
  return AE_OK;
}

void AeDeleteFileEvent(AeEventLoop *event_loop, int fd, int mask) {
  AeFileEvent *fe;
  AeFileEvent *prev = NULL;

  fe = event_loop->file_event_head;
  while (fe) {
    if (fe->fd == fd && fe->mask == mask) {
       if (prev == NULL) {
         event_loop->file_event_head = fe->next;
       } else {
         prev->next = fe->next;
       }
       zfree(fe);
       return;
    }
    prev = fe;
    fe = fe->next;
  }
}

long long AeCreateTimeEvent(AeEventLoop *event_loop, long long milliseconds,
                            AeTimeProc *proc, void *client_data,
                            AeEventFinalizerProc *finalizer_proc) {
    long long id = event_loop->time_event_next_id++;
    AeTimeEvent *te;

    te = zmalloc(sizeof(*te));
    if (te == NULL) {
      return AE_ERR;
    }

    te->id = id;
    AeAddMillisecondsToNow(milliseconds, &te->when_sec, &te->when_ms);
    te->time_proc = proc;
    te->finalizer_proc = finalizer_proc;
    te->client_data = client_data;
    te->next = event_loop->time_event_head;
    event_loop->time_event_head = te;
    return id;
}

int AeDeleteTimeEvent(AeEventLoop *event_loop, long long id) {
  AeTimeEvent *te;
  AeTimeEvent *prev = NULL;

  te = event_loop->time_event_head;
  while (te) {
    if (te->id == id) {
      if (prev == NULL) {
        event_loop->time_event_head = te->next;
      } else {
        prev->next = te->next;
      }
      zfree(te);
      return AE_OK;
    }
    prev = te;
    te = te->next;
  }
  return AE_ERR;
}

// Process every pending time event, then every pending file event
// (that may be registered by time event callbacks just processed).
// Without special flags the function sleeps until some file event
// fires, or when the next time event occurs (if any).
//
// If flags is 0, the function does nothing and returns.
// If flags has AE_ALL_EVENTS set, all the kind of events are processed.
// If flags has AE_FILE_EVENTS set, file events are processed.
// If flags has AE_TIME_EVENTS set, time events are processed.
// If flags has AE_DONT_WAIT set, the function returns ASAP until all
// the events that's possible to process without to wait are processed.
//
// The function returns the number of events processed.
int AeProcessEvents(AeEventLoop *event_loop, int flags) {
  fd_set rfds, wfds, efds;
  int max_fd = 0;
  int processed = 0;

  if (!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS)) {
    return AE_OK;
  }

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_ZERO(&efds);

  // Check file events
  if (flags & AE_FILE_EVENTS) {
    AeFileEvent *fe = event_loop->file_event_head;
    while (fe) {
      if (fe->mask & AE_READABLE) {
        FD_SET(fe->fd, &rfds);
      }
      if (fe->mask & AE_WRITABLE) {
        FD_SET(fe->fd, &wfds);
      }
      if (fe->mask & AE_EXCEPTION) {
        FD_SET(fe->fd, &efds);
      }
      if (max_fd < fe->fd) {
        max_fd = fe->fd;
      }
      fe = fe->next;
    }
  }

  // We want to call select() even if there are no file events to
  // process as long as we want to process time events, in order
  // to sleep until the next time event is ready to fire.
  if (max_fd != 0 || ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
    int ret_val;
    AeTimeEvent *shortest = NULL;
    struct timeval tv, *tvp;

    if (flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT)) {
      shortest = AeSearchNearestTimer(event_loop);
    }

    if (shortest) {
      long now_sec;
      long now_ms;

      AeGetTime(&now_sec, &now_ms);
      tvp = &tv;
      tvp->tv_sec = shortest->when_sec - now_sec;
      if (shortest->when_ms < now_ms) {
        tvp->tv_usec = ((shortest->when_ms + 1000) - now_ms) * 1000;
        tvp->tv_sec--;
      } else {
        tvp->tv_usec = (shortest->when_ms - now_ms) * 1000;
      }
    } else {
      // If we have to check for events but need to return
      // ASAP because of AE_DONT_WAIT we need to set the timeout
      // to zero.
      if (flags & AE_DONT_WAIT) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        tvp = &tv;
      } else {
        // Otherwise we can block
        tvp = NULL;
      }
    }

    ret_val = select(max_fd + 1, &rfds, &wfds, &efds, tvp);
    if (ret_val > 0) {
      AeFileEvent *fe;
      fe = event_loop->file_event_head;
      while (fe) {
        int fd = fe->fd;

        if ((fe->mask & AE_READABLE && FD_ISSET(fd, &rfds)) ||
            (fe->mask & AE_WRITABLE && FD_ISSET(fd, &wfds)) ||
            (fe->mask & AE_EXCEPTION && FD_ISSET(fd, &efds))) {
          int mask = 0;
          if (fe->mask & AE_READABLE && FD_ISSET(fd, &rfds)) {
            mask |= AE_READABLE;
          }
          if (fe->mask & AE_WRITABLE && FD_ISSET(fd, &wfds)) {
            mask |= AE_WRITABLE;
          }
          if (fe->mask & AE_EXCEPTION && FD_ISSET(fd, &efds)) {
            mask |= AE_EXCEPTION;
          }
          fe->file_proc(event_loop, fd, fe->client_data, mask);
          processed++;
          // After an event is processed our file event list
          // may no longer be the same, so what we do is to
          // clear the bit for this file descriptor and
          // restart again from the head.
          fe = event_loop->file_event_head;
          FD_CLR(fd, &rfds);
          FD_CLR(fd, &wfds);
          FD_CLR(fd, &efds);
        } else {
          fe = fe->next;
        }
      }
    } else if (ret_val < 0) {
      exit(1);
    }
  }

  // Check time events
  if (flags & AE_TIME_EVENTS) {
    long long max_id = event_loop->time_event_next_id - 1;
    AeTimeEvent *te = event_loop->time_event_head;

    while (te) {
      long now_sec;
      long now_ms;

      if (te->id > max_id) {
        te = te->next;
        continue;
      }

      AeGetTime(&now_sec, &now_ms);
      if (now_sec > te->when_sec ||
          (now_sec == te->when_sec && now_ms >= te->when_ms)) {
        int ret;
        long long id;

        id = te->id;
        ret = te->time_proc(event_loop, id, te->client_data);
        // After an event is processed, our time event list may no
        // longer be the same, so we restart from head.
        // Still we make sure to don't process events registered
        // by event handlers itself in order to don't loop forever.
        // To do so we saved the max ID we want to handle.
        if (ret != AE_NOMORE) {
          AeAddMillisecondsToNow(ret, &te->when_sec, &te->when_ms);
        } else {
          AeDeleteTimeEvent(event_loop, id);
        }
        te = event_loop->time_event_head;
      } else {
        te = te->next;
      }
    }
  }

  return processed;
}

// Wait for milliseconds until the given file descriptor becomes
// writable, readable or exception.
int AeWait(int fd, int mask, long long milliseconds) {
  struct timeval tv;
  fd_set rfds, wfds, efds;
  int ret_mask = 0;
  int ret_val = 0;

  tv.tv_sec = milliseconds / 1000;
  tv.tv_usec = (milliseconds % 1000) * 1000;
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_ZERO(&efds);

  if (mask & AE_READABLE) {
    FD_SET(fd, &rfds);
  }
  if (mask & AE_WRITABLE) {
    FD_SET(fd, &wfds);
  }
  if (mask & AE_EXCEPTION) {
    FD_SET(fd, &efds);
  }
  if ((ret_val = select(fd + 1, &rfds, &wfds, &efds, &tv)) > 0) {
    if (FD_ISSET(fd, &rfds)) {
      ret_mask |= AE_READABLE;
    }
    if (FD_ISSET(fd, &wfds)) {
      ret_mask |= AE_WRITABLE;
    }
    if (FD_ISSET(fd, &efds)) {
      ret_mask |= AE_EXCEPTION;
    }
    return ret_mask;
  } else {
    return ret_val;
  }
}

void AeMain(AeEventLoop *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
      AeProcessEvents(eventLoop, AE_ALL_EVENTS);
    }
}

// Private functions
static AeTimeEvent *AeSearchNearestTimer(AeEventLoop *event_loop) {
  AeTimeEvent *te = event_loop->time_event_head;
  AeTimeEvent *nearest = NULL;

  while (te) {
    if (!nearest || te->when_sec < nearest->when_sec ||
        (te->when_sec == nearest->when_sec &&
         te->when_ms < nearest->when_ms)) {
      nearest = te;
    }
    te = te->next;
  }

  return nearest;
}

static void AeGetTime(long *seconds, long *milliseconds) {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  *seconds = tv.tv_sec;
  *milliseconds = tv.tv_usec / 1000;
}

static void AeAddMillisecondsToNow(long long milliseconds, long *sec, long *ms) {
  long cur_sec;
  long cur_ms;

  AeGetTime(&cur_sec, &cur_ms);
  *sec = cur_sec + milliseconds / 1000;
  *ms = cur_ms + milliseconds % 1000;
  if (*ms >= 1000) {
    *sec = *sec + 1;
    *ms = *ms - 1000;
  }
}
