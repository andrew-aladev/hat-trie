// This file is part of hat-trie.
// Copyright (c) 2011 by Daniel C. Jones <dcjones@cs.washington.edu>

#ifndef HTR_COMMON_H
#define HTR_COMMON_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t * slot_t;
typedef uint64_t  value_t;
typedef struct htr_table_iter_t htr_table_iter;

typedef uint32_t ( * hash_function ) ( const uint8_t * data, size_t len );

#endif
