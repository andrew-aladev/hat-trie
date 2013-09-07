#ifndef MURMURHASH3_H
#define MURMURHASH3_H

#include <stdint.h>
#include <stdlib.h>

uint32_t murmur_hash ( const uint8_t * data, size_t len );

#endif
