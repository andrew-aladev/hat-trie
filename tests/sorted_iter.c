
/* A quick test of the degree to which ordered iteration is slower than unordered. */

#include <hat-trie/hat-trie.h>
#include <stdio.h>
#include <time.h>


/* Simple random string generation. */
void randstr ( char* x, size_t len )
{
    x[len] = '\0';
    while ( len > 0 ) {
        x[--len] = '\x20' + ( rand() % ( '\x7e' - '\x20' + 1 ) );
    }
}

int main()
{
    htr * T = htr_create();
    const size_t n = 1000000;  // how many strings
    const size_t m_low  = 50;  // minimum length of each string
    const size_t m_high = 500; // maximum length of each string
    char x[501];

    size_t i, m;
    for ( i = 0; i < n; ++i ) {
        m = m_low + rand() % ( m_high - m_low );
        randstr ( x, m );
        *htr_get ( T, x, m ) = 1;
    }

    htr_iterator * it;
    clock_t t0, t;
    const size_t repetitions = 100;
    size_t r;

    /* iterate in unsorted order */
    fprintf ( stderr, "iterating out of order ... " );
    t0 = clock();
    for ( r = 0; r < repetitions; ++r ) {
        it = htr_iterator_begin ( T, false );
        while ( !htr_iterator_finished ( it ) ) {
            htr_iterator_next ( it );
        }
        htr_iterator_free ( it );
    }
    t = clock();
    fprintf ( stderr, "finished. (%0.2f seconds)\n", ( double ) ( t - t0 ) / ( double ) CLOCKS_PER_SEC );


    /* iterate in sorted order */
    fprintf ( stderr, "iterating in order ... " );
    t0 = clock();
    for ( r = 0; r < repetitions; ++r ) {
        it = htr_iterator_begin ( T, true );
        while ( !htr_iterator_finished ( it ) ) {
            htr_iterator_next ( it );
        }
        htr_iterator_free ( it );
    }
    t = clock();
    fprintf ( stderr, "finished. (%0.2f seconds)\n", ( double ) ( t - t0 ) / ( double ) CLOCKS_PER_SEC );


    htr_free ( T );

    return 0;
}
