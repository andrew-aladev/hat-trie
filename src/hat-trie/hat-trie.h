// This file is part of hat-trie
// Copyright (c) 2011 by Daniel C. Jones <dcjones@cs.washington.edu>
// Copyright (c) 2013 by Andrew Aladjev <aladjev.andrew@gmail.com>
// This is an implementation of the HAT-trie data structure described in,
// Askitis, N., & Sinha, R. (2007). HAT-trie: a cache-conscious trie-based data structure for strings.
// Proceedings of the thirtieth Australasian conference on Computer science-Volume 62 (pp. 97–105). Australian Computer Society, Inc.
// The HAT-trie is in essence a hybrid data structure, combining tries and hash tables in a clever way to try to get the best of both worlds.

#ifndef HTR_HATTRIE_H
#define HTR_HATTRIE_H

#include "common.h"
#include <stdlib.h>
#include <stdbool.h>

typedef struct htr_t htr;

htr * htr_create ();
void  htr_free   ( htr * trie );
void  htr_clear  ( htr * trie );


// Find the given key in the trie, inserting it if it does not exist, and returning a pointer to it's key.
// This pointer is not guaranteed to be valid after additional calls to hattrie_get, hattrie_del, hattrie_clear, or other functions that modifies the trie.
htr_value * htr_get ( htr * trie, const char * key, size_t length );


// Find a given key in the table, returning a NULL pointer if it does not exist.
htr_value * htr_tryget ( htr * trie, const char * key, size_t length );

// Delete a given key from trie. Returns 0 if successful or -1 if not found.
int htr_del ( htr * trie, const char * key, size_t length );

typedef struct htr_iterator_t htr_iterator;

htr_iterator * htr_iterator_begin     ( const htr * trie, bool sorted );
void           htr_iterator_next      ( htr_iterator * iterator );
bool           htr_iterator_finished  ( htr_iterator * iterator );
void           htr_iterator_free      ( htr_iterator * iterator );
const char *   htr_iterator_key       ( htr_iterator * iterator, size_t * length );
htr_value  *   htr_iterator_val       ( htr_iterator * iterator );

#endif
