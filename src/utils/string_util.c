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
