/*
 * Copyright (c) 2023 furzoom.com, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#include "data_struct/dict.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory/zmalloc.h"

void DictFreeEntryVal(Dict *ht, DictEntry *entry) {
  if (ht->type->valDestructor) {
    ht->type->valDestructor(ht->priv_data, entry->val);
  }
}

void DictSetHashVal(Dict *ht, DictEntry *entry, void *val) {
  if (ht->type->valDup) {
    entry->val = ht->type->valDup(ht->priv_data, val);
  } else {
    entry->val = val;
  }
}

void DictFreeEntryKey(Dict *ht, DictEntry *entry) {
  if (ht->type->keyDestructor) {
    ht->type->keyDestructor(ht->priv_data, entry->key);
  }
}

void DictSetHashKey(Dict *ht, DictEntry *entry, void *key) {
  if (ht->type->keyDup) {
    entry->key = ht->type->keyDup(ht->priv_data, key);
  } else {
    entry->key = key;
  }
}

int DictCompareHashKeys(Dict *ht, const void *key1, const void *key2) {
  if (ht->type->keyCompare) {
    return ht->type->keyCompare(ht->priv_data, key1, key2);
  } else {
    return key1 == key2;
  }
}

unsigned int DictHashKey(Dict *ht, const void *key) {
  return ht->type->hashFunction(key);
}

void *DictGetEntryKey(DictEntry *he) {
  return he->key;
}

void *DictGetEntryVal(DictEntry *he) {
  return he->val;
}

unsigned int DictGetHashTableSize(Dict *ht) {
  return ht->size;
}

unsigned int DictGetHashTableUsed(Dict *ht) {
  return ht->used;
}

// Utility functions
static void _DictPanic(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  fprintf(stderr, "\nDICT LIBRARY PANIC: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n\n");
  va_end(ap);
}

// Heap management wrappers
static void *_DictAlloc(int size) {
  void *p = zmalloc(size);
  if (p == NULL) {
    _DictPanic("Out of memory");
  }
  return p;
}

static void _DictFree(void *ptr) {
  zfree(ptr);
}

// Private prototypes
static int _DictExpandIfNeeded(Dict *ht);
static unsigned int _DictNextPower(unsigned int size);
static int _DictKeyIndex(Dict *ht, const void *key);
static int _DictInit(Dict *ht, DictType *type, void *priv_data);
static void _DictReset(Dict *ht);

// hash functions
// Thomas Wang's 32 bit Mix Function
unsigned int DictIntHashFunction(unsigned int key) {
  key += ~(key << 15);
  key ^=  (key >> 10);
  key +=  (key << 3);
  key ^=  (key >> 6);
  key += ~(key << 11);
  key ^=  (key >> 16);
  return key;
}

// Identity hash function for integer keys
unsigned int DictIdentityHashFunction(unsigned int key) {
  return key;
}

// Generic hash function (a popular one from Bernstein).
// I tested a few and this was the best.
unsigned int DictGenHashFunction(const unsigned char *buf, int len) {
  unsigned int hash = 5381;
  while (len--) {
    hash = ((hash << 5) + hash) + (*buf++);  // hash * 33 + c
  }
  return hash;
}

// API

// Create a new hash table
Dict *DictCreate(DictType *type, void *priv_data) {
  Dict *ht = _DictAlloc(sizeof(*ht));
  _DictInit(ht, type, priv_data);
  return ht;
}

// Expand or create the hash table
int DictExpand(Dict *ht, unsigned int size) {
  Dict n;
  unsigned int real_size = _DictNextPower(size);
  unsigned int i;

  // The size is invalid if it is smaller than the number of
  // elements already inside the hast table.
  if (ht->size > size) {
    return DICT_ERR;
  }

  _DictInit(&n, ht->type, ht->priv_data);
  n.size = real_size;
  n.size_mask = real_size - 1;
  n.table = _DictAlloc(real_size * sizeof(DictEntry*));

  // Initialize all the pointer to NULL
  memset(n.table, 0, real_size * sizeof(DictEntry*));

  // Copy all the elements from the old to the new table.
  // If the old hash table is empty, ht->size is zero,
  // so DictExpand just creates a hash table.
  n.used = ht->used;
  for (i = 0; i < ht->size && ht->used > 0; i++) {
    DictEntry *he;
    DictEntry *next_he;

    if (ht->table[i] == NULL) {
      continue;
    }

    // For each hast table entry on this slot...
    he = ht->table[i];
    while (he) {
      unsigned int h;
      next_he = he->next;

      // Get the new element index
      h = DictHashKey(ht, he->key) & n.size_mask;
      he->next = n.table[h];
      n.table[h] = he;
      ht->used--;
      // Pass to the next element
      he = next_he;
    }
  }
  assert(ht->used == 0);
  _DictFree(ht->table);

  // Remap the new hash table in the old
  *ht = n;
  return DICT_OK;
}

// Add an element to the target hast table
int DictAdd(Dict *ht, void *key, void *val) {
  int index;
  DictEntry *entry;

  // Get the index of the new element, or -1 if
  // the element already exists.
  if ((index = _DictKeyIndex(ht, key)) == -1) {
    return DICT_ERR;
  }

  // Allocates the memory and stores the key
  entry = _DictAlloc(sizeof(*entry));
  entry->next = ht->table[index];
  ht->table[index] = entry;

  // Set the hash entry fields
  DictSetHashKey(ht, entry, key);
  DictSetHashVal(ht, entry, val);
  ht->used++;
  return DICT_OK;
}

// Add an element, discarding the old if the key already exists
int DictReplace(Dict *ht, void *key, void *val) {
  DictEntry *entry;

  // Try to add the element. If the key does not exist, DictAdd
  // will succeed.
  if (DictAdd(ht, key, val) == DICT_OK) {
    return DICT_OK;
  }

  // It already exists, get the entry
  entry = DictFind(ht, key);
  // Free the old value and set the new one
  DictFreeEntryVal(ht, entry);
  DictSetHashVal(ht, entry, val);
  return DICT_OK;
}

// Search and remove an element
static int DictGenericDelete(Dict *ht, const void *key, int no_free) {
  unsigned int h;
  DictEntry *he;
  DictEntry *prev_he;

  if (ht->size == 0) {
    return DICT_ERR;
  }

  h = DictHashKey(ht, key);
  he = ht->table[h];

  prev_he = NULL;
  while (he) {
    if (DictCompareHashKeys(ht, key, he->key)) {
      // Unlink the element from the list
      if (prev_he) {
        prev_he->next = he->next;
      } else {
        ht->table[h] = he->next;
      }
      if (!no_free) {
        DictFreeEntryKey(ht, he);
        DictFreeEntryVal(ht, he);
      }
      _DictFree(he);
      ht->used--;
      return DICT_OK;
    }
    prev_he = he;
    he = he->next;
  }
  return DICT_ERR;  // not found
}

int DictDelete(Dict *ht, const void *key) {
  return DictGenericDelete(ht, key, 0);
}

int DictDeleteNoFree(Dict *ht, const void *key) {
  return DictGenericDelete(ht, key, 1);
}

// Destroy an entire hash table
static int _DictClear(Dict *ht) {
  unsigned int i;
  // Free all the elements
  for (i = 0; i < ht->size && ht->used > 0; i++) {
    DictEntry *he;
    DictEntry *next_he;

    if ((he = ht->table[i]) == NULL) {
      continue;
    }
    while (he) {
      next_he = he->next;
      DictFreeEntryKey(ht, he);
      DictFreeEntryVal(ht, he);
      _DictFree(he);
      ht->used--;
      he = next_he;
    }
  }

  // Free the table and the allocated cache structure
  _DictFree(ht->table);
  // Re-initialize the table
  _DictReset(ht);
  return DICT_OK;  // never fails
}

void DictRelease(Dict *ht) {
  _DictClear(ht);
  _DictFree(ht);
}

DictEntry *DictFind(Dict *ht, const void *key) {
  DictEntry *he;
  unsigned int h;

  if (ht->size == 0) {
    return NULL;
  }

  h = DictHashKey(ht, key) & ht->size_mask;
  he = ht->table[h];
  while (he) {
    if (DictCompareHashKeys(ht, key, he->key)) {
      return he;
    }
    he = he->next;
  }
  return NULL;
}

// Resize the table to the minimal size that contains all the elements,
// but with the invariant of a USE/BUCKETS ration near to <= 1.
int DictResize(Dict *ht) {
  unsigned int minimal = ht->used;
  if (minimal < DICT_HT_INITIAL_SIZE) {
    minimal = DICT_HT_INITIAL_SIZE;
  }
  return DictExpand(ht, minimal);
}

DictIterator *DictGetIterator(Dict *ht) {
  DictIterator *iter = _DictAlloc(sizeof(*iter));

  iter->ht = ht;
  iter->index = -1;
  iter->entry = NULL;
  iter->next_entry = NULL;
  return iter;
}

DictEntry *DictNext(DictIterator *iter) {
  while (1) {
    if (iter->entry == NULL) {
      iter->index++;
      if (iter->index >= (signed)iter->ht->size) {
        break;
      }
      iter->entry = iter->ht->table[iter->index];
    } else {
      iter->entry = iter->next_entry;
    }

    if (iter->entry) {
      // Save the 'next' here, the iterator user may delete the
      // entry we are returning.
      iter->next_entry = iter->entry->next;
      return iter->entry;
    }
  }
  return NULL;
}

void DictReleaseIterator(DictIterator *iter) {
  _DictFree(iter);
}

// Return a random entry from the hash table. Useful to
// implement randomized algorithms
DictEntry *DictGetRandomKey(Dict *ht) {
  DictEntry *he;
  unsigned int h;
  int list_len = 0;
  int list_ele;

  if (ht->size == 0) {
    return NULL;
  }

  do {
    h = random() & ht->size_mask;
    he = ht->table[h];
  } while (he == NULL);

  // Now we found a non-empty slot, but it is a linked list,
  // and we need to get a random element from the list.
  // The only sane way to do so is to count the element and
  // select a random index
  while (he) {
    he = he->next;
    list_len++;
  }
  list_ele = random() % list_len;
  he = ht->table[h];
  while (list_ele--) {
    he = he->next;
  }
  return he;
}

#define DICT_STATS_VEC_LEN 50
void DictPrintStats(Dict *ht) {
  unsigned int i;
  unsigned int slots = 0;
  unsigned int max_chain_len = 0;
  unsigned int total_chain_len = 0;
  unsigned int cl_vector[DICT_STATS_VEC_LEN];

  if (ht->used == 0) {
    fprintf(stdout, "No stats available for empty dictionaries\n");
    return;
  }

  for (i = 0; i < DICT_STATS_VEC_LEN; i++) {
    cl_vector[i] = 0;
  }
  for (i = 0; i < ht->size; i++) {
    unsigned int chain_len;
    unsigned int chain_index;
    DictEntry *he;
    if (ht->table[i] == NULL) {
      cl_vector[0]++;
      continue;
    }
    slots++;
    // For each hash entry on this slot
    chain_len = 0;
    he = ht->table[i];
    while (he) {
      chain_len++;
      he = he->next;
    }
    chain_index = (chain_len < DICT_STATS_VEC_LEN)
                      ? chain_len : (DICT_STATS_VEC_LEN - 1);
    cl_vector[chain_index]++;
    if (chain_len > max_chain_len) {
      max_chain_len = chain_len;
    }
    total_chain_len += chain_len;
  }

  fprintf(stdout, "Hash table stats:\n");
  fprintf(stdout, " table size: %d\n", ht->size);
  fprintf(stdout, " number of elements: %d\n", ht->used);
  fprintf(stdout, " different slots: %d\n", slots);
  fprintf(stdout, " max chain length: %u\n", max_chain_len);
  fprintf(stdout, " avg chain length (counted): %.02f\n",
          (double)total_chain_len / slots);
  fprintf(stdout, " avg chain length (computed): %.02f\n",
          (double)ht->used / slots);
  fprintf(stdout, " chain length distribution:\n");
  for (i = 0; i < DICT_STATS_VEC_LEN - 1; i++) {
    if (cl_vector[i] == 0) {
      continue;
    }
    fprintf(stdout, "    %s%d: %d (%.02f%%)\n",
            (i == DICT_STATS_VEC_LEN - 1) ? ">=" : "", i, cl_vector[i],
            ((double)cl_vector[i] / ht->size) * 100);
  }
}

void DictEmpty(Dict *ht) {
  _DictClear(ht);
}

// Private functions

// Expand the hash table if needed
static int _DictExpandIfNeeded(Dict *ht) {
  // If the hash table is empty expand it to the initial size,
  // if the table is "full", double its size.
  if (ht->size == 0) {
    return DictExpand(ht, DICT_HT_INITIAL_SIZE);
  }
  if (ht->used == ht->size) {
    return DictExpand(ht, ht->size * 2);
  }
  return DICT_OK;
}

// Our hash table capability is a power of two
static unsigned int _DictNextPower(unsigned int size) {
  unsigned int i = DICT_HT_INITIAL_SIZE;

  if (size >= 0x80000000) {
    return 0x80000000;
  }
  while (1) {
    if (i >= size) {
      return i;
    }
    i *= 2;
  }
}

// Returns the index of a free slot that can be populated with
// a hash entry for the given 'key'. If the key already exists,
// -1 is returned.
static int _DictKeyIndex(Dict *ht, const void *key) {
  unsigned int h;
  DictEntry *he;

  // Expand the hash table if needed
  if (_DictExpandIfNeeded(ht) == DICT_ERR) {
    return -1;
  }
  // Compute the key hash value
  h = DictHashKey(ht, key) & ht->size_mask;
  // Search if this slot does not already contain the given key
  he = ht->table[h];
  while (he) {
    if (DictCompareHashKeys(ht, key, he->key)) {
      return -1;
    }
    he = he->next;
  }
  return h;
}

// Initialize the hash table
static int _DictInit(Dict *ht, DictType *type, void *priv_data) {
  _DictReset(ht);
  ht->type = type;
  ht->priv_data = priv_data;
  return DICT_OK;
}

// Reset a hash table
static void _DictReset(Dict *ht) {
  ht->table = NULL;
  ht->size = 0;
  ht->used = 0;
  ht->size_mask = 0;
}

// String Copy hash table type
static unsigned int _DictStringCopyHTHashFunction(const void *key) {
  return DictGenHashFunction((void*)key, (int)strlen(key));
}

static void *_DictStringCopyHTKeyDup(void *priv_data, const void *key) {
  int len = strlen(key);
  char *copy = _DictAlloc(len + 1);
  DICT_NOT_USED(priv_data);

  memcpy(copy, key, len);
  copy[len] = '\0';
  return copy;
}

static void *_DictStringCopyHTValDup(void *priv_data, const void *val) {
  int len = strlen(val);
  char *copy = _DictAlloc(len + 1);
  DICT_NOT_USED(priv_data);

  memcpy(copy, val, len);
  copy[len] = '\0';
  return copy;
}

static int _DictStringCopyHTKeyCompare(void *priv_data, const void *key1,
                                       const void *key2) {
  DICT_NOT_USED(priv_data);

  return strcmp(key1, key2) == 0;
}

static void _DictStringCopyHTKeyDestructor(void *priv_data, void *key) {
  DICT_NOT_USED(priv_data);
  _DictFree(key);
}

static void _DictStringCopyHTValDestructor(void *priv_data, void *val) {
  DICT_NOT_USED(priv_data);
  _DictFree(val);
}

DictType DictTypeHeapStringCopyKey = {
    _DictStringCopyHTHashFunction,  // hash function
    _DictStringCopyHTKeyDup,        // key dup
    NULL,                           // val dup
    _DictStringCopyHTKeyCompare,    // key compare
    _DictStringCopyHTKeyDestructor, // key destructor
    NULL,                           // val destructor
};

// This is like StringCopy but does not auto-duplicate the key.
// It's used for interpreter's shared strings.
DictType DictTypeHeapStrings = {
    _DictStringCopyHTHashFunction,  // hash function
    NULL,                           // key dup
    NULL,                           // val dup
    _DictStringCopyHTKeyCompare,    // key compare
    NULL,                           // key destructor
    NULL,                           // val destructor
};

// This is like StringCopy but also automatically handle dynamic
// allocated C strings as values.
DictType DictTypeHeapStringCopyKeyValue = {
    _DictStringCopyHTHashFunction,  // hash function
    _DictStringCopyHTKeyDup,        // key dup
    _DictStringCopyHTValDup,        // val dup
    _DictStringCopyHTKeyCompare,    // key compare
    _DictStringCopyHTKeyDestructor, // key destructor
    _DictStringCopyHTValDestructor, // val destructor
};
