// This file is part of hat-trie.
// Copyright (c) 2011 by Daniel C. Jones <dcjones@cs.washington.edu>

#ifndef HTR_COMMON_H
#define HTR_COMMON_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t * htr_slot;
typedef uint64_t  htr_value;
typedef struct htr_table_iterator_t htr_table_iterator;

typedef uint32_t ( * htr_hash_function ) ( const uint8_t * data, size_t len );

#endif
