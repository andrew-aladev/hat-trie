// This file is part of hat-trie.
// Copyright (c) 2011 by Daniel C. Jones <dcjones@cs.washington.edu>
//
// This is an implementation of the 'cache-conscious' hash tables described in, Askitis, N., & Zobel, J. (2005).
// Cache-conscious collision resolution in string hash tables.
// String Processing and Information Retrieval (pp. 91–102). Springer.
//
// Briefly, the idea is, as opposed to separate chaining with linked lists, to store keys contiguously in one big array,
// thereby improving the caching behavior, and reducing space requirments.

#ifndef HTR_TABLE_H
#define HTR_TABLE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "common.h"

typedef struct htr_table_t {
    // these fields are reserved for hattrie to fiddle with
    uint8_t flag;
    uint8_t c0;
    uint8_t c1;

    size_t n;     // number of slots
    size_t m;     // numbur of key/value pairs stored
    size_t max_m; // number of stored keys before we resize

    size_t * slot_sizes;
    htr_slot * slots;
} htr_table;

extern const double htr_table_max_load_factor;
extern const size_t htr_table_initial_size;

// Create an empty hash table, with n slots reserved.
htr_table * htr_table_create_n ( size_t n );

// Create an empty hash table.
inline
htr_table * htr_table_create()
{
    return htr_table_create_n ( htr_table_initial_size );
}

// Free all memory used by a table.
void htr_table_free ( htr_table * table );

// Remove all entries.
uint8_t htr_table_clear ( htr_table * table );

// Number of stored keys.
inline
size_t htr_table_size ( const htr_table * table )
{
    return table->m;
}

// Find the given key in the table, inserting it if it does not exist, and returning a pointer to it's key.
// This pointer is not guaranteed to be valid after additional calls to htr_table_get, htr_table_del, htr_table_clear, or other functions that modifies the table.
htr_value * htr_table_get ( htr_table * table, const char * key, size_t len );

// Find a given key in the table, returning a NULL pointer if it does not exist.
htr_value * htr_table_tryget ( htr_table * table, const char * key, size_t len );

int htr_table_del ( htr_table * table, const char * key, size_t len );

htr_table_iterator * htr_table_iter_begin    ( const htr_table *, bool sorted );
void             htr_table_iter_next     ( htr_table_iterator * );
bool             htr_table_iter_finished ( htr_table_iterator * );
void             htr_table_iter_free     ( htr_table_iterator * );
const char *     htr_table_iter_key      ( htr_table_iterator *, size_t * len );
htr_value *        htr_table_iter_val      ( htr_table_iterator * );

#endif
