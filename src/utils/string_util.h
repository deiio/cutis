/*
* Copyright (c) 2023 furzoom.com, All rights reserved.
* Author: mn, mn@furzoom.com
*/

#ifndef UTILS_STRING_UTIL_H_
#define UTILS_STRING_UTIL_H_

// Glob-style pattern matching.
int StringMatch(const char *pattern, int pat_len,
                const char *string, int str_len, int no_case);

#endif  // UTILS_STRING_UTIL_H_
