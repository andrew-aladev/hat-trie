
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "str_map.h"
#include <hat-trie/hat-trie.h>
#include <talloc2/tree.h>

/* Simple random string generation. */
void randstr ( char* x, size_t len )
{
    x[len] = '\0';
    while ( len > 0 ) {
        x[--len] = '\x20' + ( rand() % ( '\x7e' - '\x20' + 1 ) );
    }
}

const size_t n = 100000;  // how many unique strings
const size_t m_low  = 50;  // minimum length of each string
const size_t m_high = 500; // maximum length of each string
const size_t k = 200000;  // number of insertions
const size_t d = 50000;

char** xs;
char** ds;

htr * T;
str_map* M;


void setup()
{
    fprintf ( stderr, "generating %zu keys ... ", n );
    xs = malloc ( n * sizeof ( char* ) );
    ds = malloc ( d * sizeof ( char* ) );
    size_t i;
    size_t m;
    for ( i = 0; i < n; ++i ) {
        m = m_low + rand() % ( m_high - m_low );
        xs[i] = malloc ( m + 1 );
        randstr ( xs[i], m );
    }
    for ( i = 0; i < d; ++i ) {
        m = rand() % n;
        ds[i] = xs[m];
    }

    T = htr_new ( NULL );
    M = str_map_create();
    fprintf ( stderr, "done.\n" );
}


void teardown()
{
    talloc_free ( T );
    str_map_destroy ( M );

    size_t i;
    for ( i = 0; i < n; ++i ) {
        free ( xs[i] );
    }
    free ( xs );
    free ( ds );
}


void test_hattrie_insert()
{
    fprintf ( stderr, "inserting %zu keys ... \n", k );

    size_t i, j;
    htr_value * u;
    htr_value   v;

    for ( j = 0; j < k; ++j ) {
        i = rand() % n;


        v = 1 + str_map_get ( M, xs[i], strlen ( xs[i] ) );
        str_map_set ( M, xs[i], strlen ( xs[i] ), v );


        u = htr_get ( T, xs[i], strlen ( xs[i] ) );
        *u += 1;


        if ( *u != v ) {
            fprintf ( stderr, "[error] tally mismatch (reported: %lu, correct: %lu)\n",
                      *u, v );
        }
    }

    fprintf ( stderr, "deleting %zu keys ... \n", d );
    for ( j = 0; j < d; ++j ) {
        str_map_del ( M, ds[j], strlen ( ds[j] ) );
        htr_del ( T, ds[j], strlen ( ds[j] ) );
        u = htr_tryget ( T, ds[j], strlen ( ds[j] ) );
        if ( u ) {
            fprintf ( stderr, "[error] item %zu still found in trie after delete\n",
                      j );
        }
    }

    fprintf ( stderr, "done.\n" );
}



void test_hattrie_iteration()
{
    fprintf ( stderr, "iterating through %zu keys ... \n", k );

    htr_iterator * i = htr_iterator_begin ( T, false );

    size_t count = 0;
    htr_value * u;
    htr_value   v;

    size_t len;
    const char* key;

    while ( !htr_iterator_finished ( i ) ) {
        ++count;

        key = htr_iterator_key ( i, &len );
        u   = htr_iterator_val ( i );

        v = str_map_get ( M, key, len );

        if ( *u != v ) {
            if ( v == 0 ) {
                fprintf ( stderr, "[error] incorrect iteration (%lu, %lu)\n", *u, v );
            } else {
                fprintf ( stderr, "[error] incorrect iteration tally (%lu, %lu)\n", *u, v );
            }
        }

        // this way we will see an error if the same key is iterated through
        // twice
        str_map_set ( M, key, len, 0 );

        htr_iterator_next ( i );
    }

    if ( count != M->m ) {
        fprintf ( stderr, "[error] iterated through %zu element, expected %zu\n",
                  count, M->m );
    }

    htr_iterator_free ( i );

    fprintf ( stderr, "done.\n" );
}


int cmpkey ( const char* a, size_t ka, const char* b, size_t kb )
{
    int c = memcmp ( a, b, ka < kb ? ka : kb );
    return c == 0 ? ( int ) ka - ( int ) kb : c;
}


void test_hattrie_sorted_iteration()
{
    fprintf ( stderr, "iterating in order through %zu keys ... \n", k );

    htr_iterator * i = htr_iterator_begin ( T, true );

    size_t count = 0;
    htr_value * u;
    htr_value   v;

    char* key_copy = malloc ( m_high + 1 );
    char* prev_key = malloc ( m_high + 1 );
    memset ( prev_key, 0, m_high + 1 );
    size_t prev_len = 0;

    const char *key = NULL;
    size_t len = 0;

    while ( !htr_iterator_finished ( i ) ) {
        memcpy ( prev_key, key_copy, len );
        prev_key[len] = '\0';
        prev_len = len;
        ++count;

        key = htr_iterator_key ( i, &len );

        /* memory for key may be changed on iter, copy it */
        strncpy ( key_copy, key, len );

        if ( prev_key != NULL && cmpkey ( prev_key, prev_len, key, len ) > 0 ) {
            fprintf ( stderr, "[error] iteration is not correctly ordered.\n" );
        }

        u = htr_iterator_val ( i );
        v = str_map_get ( M, key, len );

        if ( *u != v ) {
            if ( v == 0 ) {
                fprintf ( stderr, "[error] incorrect iteration (%lu, %lu)\n", *u, v );
            } else {
                fprintf ( stderr, "[error] incorrect iteration tally (%lu, %lu)\n", *u, v );
            }
        }

        // this way we will see an error if the same key is iterated through
        // twice
        str_map_set ( M, key, len, 0 );

        htr_iterator_next ( i );
    }

    if ( count != M->m ) {
        fprintf ( stderr, "[error] iterated through %zu element, expected %zu\n",
                  count, M->m );
    }

    htr_iterator_free ( i );
    free ( prev_key );
    free ( key_copy );

    fprintf ( stderr, "done.\n" );
}


void test_trie_non_ascii()
{
    fprintf ( stderr, "checking non-ascii... \n" );

    htr_value * u;
    htr * T = htr_new ( NULL );
    char* txt = "\x81\x70";

    u = htr_get ( T, txt, strlen ( txt ) );
    *u = 10;

    u = htr_tryget ( T, txt, strlen ( txt ) );
    if ( *u != 10 ) {
        fprintf ( stderr, "can't store non-ascii strings\n" );
    }
    talloc_free ( T );

    fprintf ( stderr, "done.\n" );
}




int main()
{
    test_trie_non_ascii();

    setup();
    test_hattrie_insert();
    test_hattrie_iteration();
    teardown();

    setup();
    test_hattrie_insert();
    test_hattrie_sorted_iteration();
    teardown();

    return 0;
}





