/*
* Copyright (c) 2023 furzoom.com, All rights reserved.
* Author: mn, mn@furzoom.com
*/

#include "utils/string_util.h"

#include <ctype.h>
#include <string.h>

int StringMatch(const char *pattern, int pat_len,
                const char *string, int str_len, int no_case) {
  while (pat_len) {
    switch (pattern[0]) {
    case '*':
      while (pattern[1] == '*') {
        pattern++;
        pat_len--;
      }
      if (pat_len == 1) {
        return 1; // match
      }
      while (str_len) {
        if (StringMatch(pattern + 1, pat_len - 1, string, str_len, no_case)) {
          return 1; // match
        }
        string++;
        str_len--;
      }
      return 0; // no match
      break;
    case '?':
      if (str_len == 0) {
        return 0; // no match
      }
      string++;
      str_len--;
      break;
    case '[': {
      int not;
      int match = 0;

      pattern++;
      pat_len--;
      not = (pattern[0] == '^');
      if (not) {
        pattern++;
        pat_len--;
      }
      while (1) {
        if (pattern[0] == '\\') {
          pattern++;
          pat_len--;
          if (pattern[0] == string[0]) {
            match = 1;
          }
        } else if (pattern[0] == ']') {
          break;
        } else if (pat_len == 0) {
          pattern--;
          pat_len++;
          break;
        } else if (pattern[1] == '-' && pat_len >= 3) {
          int start = pattern[0];
          int end = pattern[2];
          int c = string[0];
          if (start > end) {
            int t = start;
            start = end;
            end = t;
          }
          if (no_case) {
            start = tolower(start);
            end = tolower(end);
            c = tolower(c);
          }
          pattern += 2;
          pat_len -= 2;
          if (c >= start && c <= end) {
            match = 1;
          }
        } else {
          if (!no_case) {
            if (pattern[0] == string[0]) {
              match = 1;
            }
          } else {
            if (tolower(pattern[0]) == tolower(string[0])) {
              match = 1;
            }
          }
        }
        pattern++;
        pat_len--;
      }
      if (not) {
        match = !match;
      }
      if (!match) {
        return 0; // no match
      }
      string++;
      str_len--;
      break;
    }
    case '\\':
      if (pat_len >= 2) {
        pattern++;
        pat_len--;
      }
      // fall through
    default:
      if (!no_case) {
        if (pattern[0] != string[0]) {
          return 0; // no match
        }
      } else {
        if (tolower(pattern[0]) != tolower(string[0])) {
          return 0; // no match
        }
      }
      string++;
      str_len--;
      break;
    }
    pattern++;
    pat_len--;
    if (str_len == 0) {
      while (*pattern == '*') {
        pattern++;
        pat_len--;
      }
      break;
    }
  }

  if (pat_len == 0 && str_len == 0) {
    return 1;
  }
  return 0;
}
