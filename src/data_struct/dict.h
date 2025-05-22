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

#ifndef DICT_H_
#define DICT_H_

#define DICT_OK   0
#define DICT_ERR  1

// Unused arguments generate annoying warnings
#define DICT_NOT_USED(v) ((void)v)
// Initial size of every hash table
#define DICT_HT_INITIAL_SIZE 16

typedef struct DictEntry {
  void *key;
  void *val;
  struct DictEntry *next;
} DictEntry;

typedef struct DictType {
  unsigned int (*hashFunction)(const void *key);
  void *(*keyDup)(void *priv_data, const void *key);
  void *(*valDup)(void *priv_data, const void *obj);
  int (*keyCompare)(void *priv_data, const void *key1, const void *key2);
  void (*keyDestructor)(void *priv_data, void *key);
  void (*valDestructor)(void *priv_data, void *obj);
} DictType;

typedef struct Dict {
  DictEntry **table;
  DictType *type;
  unsigned int size;
  unsigned int size_mask;
  unsigned int used;
  void *priv_data;
} Dict;

typedef struct DictIterator {
  Dict *ht;
  int index;
  DictEntry *entry;
  DictEntry *next_entry;
} DictIterator;

void DictFreeEntryVal(Dict *ht, DictEntry *entry);
void DictSetHashVal(Dict *ht, DictEntry *entry, void *val);
void DictFreeEntryKey(Dict *ht, DictEntry *entry);
void DictSetHashKey(Dict *ht, DictEntry *entry, void *key);
int DictCompareHashKeys(Dict *ht, const void *key1, const void *key2);
unsigned int DictHashKey(Dict *ht, const void *key);
void *DictGetEntryKey(DictEntry *he);
void *DictGetEntryVal(DictEntry *he);
unsigned int DictGetHashTableSize(Dict *ht);
unsigned int DictGetHashTableUsed(Dict *ht);

// API
Dict *DictCreate(DictType *type, void *priv_data);
int DictExpand(Dict *ht, unsigned int size);
int DictAdd(Dict *ht, void *key, void *val);
int DictReplace(Dict *ht, void *key, void *val);
int DictDelete(Dict *ht, const void *key);
int DictDeleteNoFree(Dict *ht, const void *key);
void DictRelease(Dict *ht);
DictEntry *DictFind(Dict *ht, const void *key);
int DictResize(Dict *ht);

DictIterator *DictGetIterator(Dict *ht);
DictEntry *DictNext(DictIterator *iter);
void DictReleaseIterator(DictIterator *iter);
DictEntry *DictGetRandomKey(Dict *ht);
void DictPrintStats(Dict *ht);
unsigned int DictGenHashFunction(const unsigned char *buf, int len);
void DictEmpty(Dict *ht);

// Hash table types
extern DictType DictTypeHeapStringCopyKey;
extern DictType DictTypeHeapStrings;
extern DictType DictTypeHeapStringCopyKeyValue;

#endif  // DICT_H_
