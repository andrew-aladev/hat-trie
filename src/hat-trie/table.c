// This file is part of hat-trie.
// Copyright (c) 2011 by Daniel C. Jones <dcjones@cs.washington.edu>

#include "table.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

const double htr_table_max_load_factor    = 100000.0; // arbitrary large number => don't resize
const const size_t htr_table_initial_size = 4096;
static const uint16_t LONG_KEYLEN_MASK    = 0x7fff;

static inline
size_t keylen ( htr_slot slot )
{
    if ( 0x1 & * slot ) {
        return ( size_t ) ( * ( ( uint16_t * ) slot ) >> 1 );
    } else {
        return ( size_t ) ( * slot >> 1 );
    }
}

htr_table * htr_table_new_n ( size_t n )
{
    htr_table * table = malloc ( sizeof ( htr_table ) );
    if ( table == NULL ) {
        return NULL;
    }
    table->flag = 0;
    table->c0   = table->c1 = '\0';

    table->slots_count = n;
    table->pairs_count = 0;
    table->max_pairs_count = ( size_t ) ( htr_table_max_load_factor * ( double ) table->slots_count );
    htr_slot * slots = malloc ( n * sizeof ( htr_slot ) );
    if ( slots == NULL ) {
        free ( table );
        return NULL;
    }
    memset ( slots, 0, n * sizeof ( htr_slot ) );
    table->slots = slots;

    size_t * slot_sizes = malloc ( n * sizeof ( size_t ) );
    if ( slot_sizes == NULL ) {
        free ( slots );
        free ( table );
        return NULL;
    }
    memset ( slot_sizes, 0, n * sizeof ( size_t ) );
    table->slots_sizes = slot_sizes;

    return table;
}

void htr_table_free ( htr_table * table )
{
    if ( table == NULL ) {
        return;
    }
    size_t i;
    for ( i = 0; i < table->slots_count; i++ ) {
        free ( table->slots[i] );
    }
    free ( table->slots );
    free ( table->slots_sizes );
    free ( table );
}

uint8_t htr_table_clear ( htr_table * table )
{
    size_t i;
    for ( i = 0; i < table->slots_count; i++ ) {
        free ( table->slots[i] );
    }

    htr_slot * slots = realloc ( table->slots, htr_table_initial_size * sizeof ( htr_slot ) );
    if ( slots == NULL ) {
        return 1;
    }
    memset ( slots, 0, htr_table_initial_size * sizeof ( htr_slot ) );
    table->slots = slots;

    size_t * slots_sizes = realloc ( table->slots_sizes, htr_table_initial_size * sizeof ( size_t ) );
    if ( slots_sizes == NULL ) {
        return 2;
    }
    memset ( slots_sizes, 0, htr_table_initial_size * sizeof ( size_t ) );
    table->slots_sizes = slots_sizes;

    table->slots_count = htr_table_initial_size;

    return 0;
}


static htr_slot ins_key ( htr_slot s, const char * key, size_t len, htr_value ** val )
{
    // key length
    if ( len < 128 ) {
        s[0] = ( unsigned char ) ( len << 1 );
        s += 1;
    } else {
        /* The most significant bit is set to indicate that two bytes are
         * being used to store the key length. */
        * ( ( uint16_t* ) s ) = ( ( uint16_t ) len << 1 ) | 0x1;
        s += 2;
    }

    // key
    memcpy ( s, key, len * sizeof ( unsigned char ) );
    s += len;

    // value
    *val = ( htr_value * ) s;
    **val = 0;
    s += sizeof ( htr_value );

    return s;
}


static void htr_table_expand ( htr_table * T, htr_hash_function hash_function )
{
    /* Resizing a table is essentially building a brand new one.
     * One little shortcut we can take on the memory allocation front is to
     * figure out how much memory each slot needs in advance.
     */
    assert ( T->slots_count > 0 );
    size_t new_n = 2 * T->slots_count;
    size_t* slots_sizes = malloc ( new_n * sizeof ( size_t ) );
    memset ( slots_sizes, 0, new_n * sizeof ( size_t ) );

    const char* key;
    size_t len = 0;
    size_t m = 0;
    htr_table_iterator * i = htr_table_iter_begin ( T, false );
    while ( !htr_table_iter_finished ( i ) ) {
        key = htr_table_iter_key ( i, &len );
        slots_sizes[hash_function ( key, len ) % new_n] +=
            len + sizeof ( htr_value ) + ( len >= 128 ? 2 : 1 );

        ++m;
        htr_table_iter_next ( i );
    }
    assert ( m == T->pairs_count );
    htr_table_iter_free ( i );


    /* allocate slots */
    htr_slot * slots = malloc ( new_n * sizeof ( htr_slot ) );
    size_t j;
    for ( j = 0; j < new_n; ++j ) {
        if ( slots_sizes[j] > 0 ) {
            slots[j] = malloc ( slots_sizes[j] );
        } else slots[j] = NULL;
    }

    /* rehash values. A few shortcuts can be taken here as well, as we know
     * there will be no collisions. Instead of the regular insertion routine,
     * we keep track of the ends of every slot and simply insert keys.
     * */
    htr_slot* slots_next = malloc ( new_n * sizeof ( htr_slot ) );
    memcpy ( slots_next, slots, new_n * sizeof ( htr_slot ) );
    size_t h;
    m = 0;
    htr_value * u;
    htr_value * v;
    i = htr_table_iter_begin ( T, false );
    while ( !htr_table_iter_finished ( i ) ) {

        key = htr_table_iter_key ( i, &len );
        h = hash_function ( key, len ) % new_n;

        slots_next[h] = ins_key ( slots_next[h], key, len, &u );
        v = htr_table_iter_val ( i );
        *u = *v;

        ++m;
        htr_table_iter_next ( i );
    }
    assert ( m == T->pairs_count );
    htr_table_iter_free ( i );


    free ( slots_next );
    for ( j = 0; j < T->slots_count; ++j ) free ( T->slots[j] );

    free ( T->slots );
    T->slots = slots;

    free ( T->slots_sizes );
    T->slots_sizes = slots_sizes;

    T->slots_count = new_n;
    T->max_pairs_count = ( size_t ) ( htr_table_max_load_factor * ( double ) T->slots_count );
}


static htr_value * get_key ( htr_table * T, htr_hash_function hash_function, const char* key, size_t len, bool insert_missing )
{
    /* if we are at capacity, preemptively resize */
    if ( insert_missing && T->pairs_count >= T->max_pairs_count ) {
        htr_table_expand ( T, hash_function );
    }


    uint32_t i = hash_function ( key, len ) % T->slots_count;
    size_t k;
    htr_slot s;
    htr_value * val;

    /* search the array for our key */
    s = T->slots[i];
    while ( ( size_t ) ( s - T->slots[i] ) < T->slots_sizes[i] ) {
        /* get the key length */
        k = keylen ( s );
        s += k < 128 ? 1 : 2;

        /* skip keys that are longer than ours */
        if ( k != len ) {
            s += k + sizeof ( htr_value );
            continue;
        }

        /* key found. */
        if ( memcmp ( s, key, len ) == 0 ) {
            return ( htr_value * ) ( s + len );
        }
        /* key not found. */
        else {
            s += k + sizeof ( htr_value );
            continue;
        }
    }


    if ( insert_missing ) {
        /* the key was not found, so we must insert it. */
        size_t new_size = T->slots_sizes[i];
        new_size += 1 + ( len >= 128 ? 1 : 0 );  // key length
        new_size += len * sizeof ( unsigned char ); // key
        new_size += sizeof ( htr_value );          // value

        T->slots[i] = realloc ( T->slots[i], new_size );

        ++T->pairs_count;
        ins_key ( T->slots[i] + T->slots_sizes[i], key, len, &val );
        T->slots_sizes[i] = new_size;

        return val;
    } else return NULL;
}


htr_value * htr_table_get ( htr_table * T, htr_hash_function hash_function, const char* key, size_t len )
{
    return get_key ( T, hash_function, key, len, true );
}


htr_value * htr_table_tryget ( htr_table * T, htr_hash_function hash_function, const char* key, size_t len )
{
    return get_key ( T, hash_function, key, len, false );
}


int htr_table_del ( htr_table * T, htr_hash_function hash_function, const char* key, size_t len )
{
    uint32_t i = hash_function ( key, len ) % T->slots_count;
    size_t k;
    htr_slot s;

    /* search the array for our key */
    s = T->slots[i];
    while ( ( size_t ) ( s - T->slots[i] ) < T->slots_sizes[i] ) {
        /* get the key length */
        k = keylen ( s );
        s += k < 128 ? 1 : 2;

        /* skip keys that are longer than ours */
        if ( k != len ) {
            s += k + sizeof ( htr_value );
            continue;
        }

        /* key found. */
        if ( memcmp ( s, key, len ) == 0 ) {
            /* move everything over, resize the array */
            unsigned char* t = s + len + sizeof ( htr_value );
            s -= k < 128 ? 1 : 2;
            memmove ( s, t, T->slots_sizes[i] - ( size_t ) ( t - T->slots[i] ) );
            T->slots_sizes[i] -= ( size_t ) ( t - s );
            --T->pairs_count;
            return 0;
        }
        /* key not found. */
        else {
            s += k + sizeof ( htr_value );
            continue;
        }
    }

    // Key was not found. Do nothing.
    return -1;
}



static int cmpkey ( const void* a_, const void* b_ )
{
    htr_slot a = * ( htr_slot * ) a_;
    htr_slot b = * ( htr_slot * ) b_;

    size_t ka = keylen ( a ), kb = keylen ( b );

    a += ka < 128 ? 1 : 2;
    b += kb < 128 ? 1 : 2;

    int c = memcmp ( a, b, ka < kb ? ka : kb );
    return c == 0 ? ( int ) ka - ( int ) kb : c;
}


/* Sorted/unsorted iterators are kept private and exposed by passing the
sorted flag to htr_table_iter_begin. */

typedef struct htr_table_sorted_iter_t_ {
    const htr_table * T; // parent
    htr_slot * xs; // pointers to keys
    size_t i; // current key
} htr_table_sorted_iter_t;


static htr_table_sorted_iter_t* htr_table_sorted_iter_begin ( const htr_table * T )
{
    htr_table_sorted_iter_t* i = malloc ( sizeof ( htr_table_sorted_iter_t ) );
    i->T = T;
    i->xs = malloc ( T->pairs_count * sizeof ( htr_slot ) );
    i->i = 0;

    htr_slot s;
    size_t j, k, u;
    for ( j = 0, u = 0; j < T->slots_count; ++j ) {
        s = T->slots[j];
        while ( s < T->slots[j] + T->slots_sizes[j] ) {
            i->xs[u++] = s;
            k = keylen ( s );
            s += k < 128 ? 1 : 2;
            s += k + sizeof ( htr_value );
        }
    }

    qsort ( i->xs, T->pairs_count, sizeof ( htr_slot ), cmpkey );

    return i;
}


static bool htr_table_sorted_iter_finished ( htr_table_sorted_iter_t* i )
{
    return i->i >= i->T->pairs_count;
}


static void htr_table_sorted_iter_next ( htr_table_sorted_iter_t* i )
{
    if ( htr_table_sorted_iter_finished ( i ) ) return;
    ++i->i;
}


static void htr_table_sorted_iter_free ( htr_table_sorted_iter_t* i )
{
    if ( i == NULL ) return;
    free ( i->xs );
    free ( i );
}


static const char* htr_table_sorted_iter_key ( htr_table_sorted_iter_t* i, size_t* len )
{
    if ( htr_table_sorted_iter_finished ( i ) ) return NULL;

    htr_slot s = i->xs[i->i];
    *len = keylen ( s );

    return ( const char* ) ( s + ( *len < 128 ? 1 : 2 ) );
}


static htr_value *  htr_table_sorted_iter_val ( htr_table_sorted_iter_t* i )
{
    if ( htr_table_sorted_iter_finished ( i ) ) return NULL;

    htr_slot s = i->xs[i->i];
    size_t k = keylen ( s );

    s += k < 128 ? 1 : 2;
    s += k;

    return ( htr_value * ) s;
}


typedef struct htr_table_unsorted_iter_t_ {
    const htr_table * T; // parent
    size_t i;           // slot index
    htr_slot s;           // slot position
} htr_table_unsorted_iter_t;


static htr_table_unsorted_iter_t* htr_table_unsorted_iter_begin ( const htr_table * T )
{
    htr_table_unsorted_iter_t* i = malloc ( sizeof ( htr_table_unsorted_iter_t ) );
    i->T = T;

    for ( i->i = 0; i->i < i->T->slots_count; ++i->i ) {
        i->s = T->slots[i->i];
        if ( ( size_t ) ( i->s - T->slots[i->i] ) >= T->slots_sizes[i->i] ) continue;
        break;
    }

    return i;
}


static bool htr_table_unsorted_iter_finished ( htr_table_unsorted_iter_t* i )
{
    return i->i >= i->T->slots_count;
}


static void htr_table_unsorted_iter_next ( htr_table_unsorted_iter_t* i )
{
    if ( htr_table_unsorted_iter_finished ( i ) ) return;

    /* get the key length */
    size_t k = keylen ( i->s );
    i->s += k < 128 ? 1 : 2;

    /* skip to the next key */
    i->s += k + sizeof ( htr_value );

    if ( ( size_t ) ( i->s - i->T->slots[i->i] ) >= i->T->slots_sizes[i->i] ) {
        do {
            ++i->i;
        } while ( i->i < i->T->slots_count &&
                  i->T->slots_sizes[i->i] == 0 );

        if ( i->i < i->T->slots_count ) i->s = i->T->slots[i->i];
        else i->s = NULL;
    }
}


static void htr_table_unsorted_iter_free ( htr_table_unsorted_iter_t* i )
{
    free ( i );
}


static const char* htr_table_unsorted_iter_key ( htr_table_unsorted_iter_t* i, size_t* len )
{
    if ( htr_table_unsorted_iter_finished ( i ) ) return NULL;

    htr_slot s = i->s;
    size_t k;
    if ( 0x1 & *s ) {
        k = ( size_t ) ( * ( ( uint16_t* ) s ) ) >> 1;
        s += 2;
    } else {
        k = ( size_t ) ( *s >> 1 );
        s += 1;
    }

    *len = k;
    return ( const char* ) s;
}


static htr_value * htr_table_unsorted_iter_val ( htr_table_unsorted_iter_t* i )
{
    if ( htr_table_unsorted_iter_finished ( i ) ) return NULL;

    htr_slot s = i->s;

    size_t k;
    if ( 0x1 & *s ) {
        k = ( size_t ) ( * ( ( uint16_t* ) s ) ) >> 1;
        s += 2;
    } else {
        k = ( size_t ) ( *s >> 1 );
        s += 1;
    }

    s += k;
    return ( htr_value * ) s;
}


struct htr_table_iterator_t {
    bool sorted;
    union {
        htr_table_unsorted_iter_t* unsorted;
        htr_table_sorted_iter_t* sorted;
    } i;
};


htr_table_iterator * htr_table_iter_begin ( const htr_table * T, bool sorted )
{
    htr_table_iterator * i = malloc ( sizeof ( htr_table_iterator ) );
    i->sorted = sorted;
    if ( sorted ) i->i.sorted   = htr_table_sorted_iter_begin ( T );
    else        i->i.unsorted = htr_table_unsorted_iter_begin ( T );
    return i;
}


void htr_table_iter_next ( htr_table_iterator * i )
{
    if ( i->sorted ) htr_table_sorted_iter_next ( i->i.sorted );
    else           htr_table_unsorted_iter_next ( i->i.unsorted );
}


bool htr_table_iter_finished ( htr_table_iterator * i )
{
    if ( i->sorted ) return htr_table_sorted_iter_finished ( i->i.sorted );
    else           return htr_table_unsorted_iter_finished ( i->i.unsorted );
}


void htr_table_iter_free ( htr_table_iterator * i )
{
    if ( i == NULL ) return;
    if ( i->sorted ) htr_table_sorted_iter_free ( i->i.sorted );
    else           htr_table_unsorted_iter_free ( i->i.unsorted );
    free ( i );
}


const char* htr_table_iter_key ( htr_table_iterator * i, size_t* len )
{
    if ( i->sorted ) return htr_table_sorted_iter_key ( i->i.sorted, len );
    else           return htr_table_unsorted_iter_key ( i->i.unsorted, len );
}


htr_value * htr_table_iter_val ( htr_table_iterator * i )
{
    if ( i->sorted ) return htr_table_sorted_iter_val ( i->i.sorted );
    else           return htr_table_unsorted_iter_val ( i->i.unsorted );
}

extern inline
htr_table * htr_table_new();

extern inline
size_t htr_table_size ( const htr_table * table );