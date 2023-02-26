/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef UTILS_LOG_H_
#define UTILS_LOG_H_

// Log levels
#define CUTIS_DEBUG   0
#define CUTIS_NOTICE  1
#define CUTIS_WARNING 2

void CutisLog(int level, const char *fmt, ...);

void CutisOom(const char *msg);

#endif  // UTILS_LOG_H_
