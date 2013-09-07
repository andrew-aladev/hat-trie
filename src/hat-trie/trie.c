// This file is part of hat-trie.
// Copyright (c) 2011 by Daniel C. Jones <dcjones@cs.washington.edu>
// Copyright (c) 2013 by Andrew Aladjev <aladjev.andrew@gmail.com>

#include "trie.h"
#include "table.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <talloc2/tree.h>
#include <talloc2/ext/destructor.h>

#define HT_UNUSED(x) x=x

// maximum number of keys that may be stored in a bucket before it is burst
static const size_t MAX_BUCKET_SIZE = 16384;
#define NODE_MAXCHAR 0xff // 0x7f for 7-bit ASCII
#define NODE_CHILDS (NODE_MAXCHAR+1)

static const uint8_t NODE_TYPE_TRIE          = 1;
static const uint8_t NODE_TYPE_PURE_BUCKET   = 1 << 1;
static const uint8_t NODE_TYPE_HYBRID_BUCKET = 1 << 2;
static const uint8_t NODE_HAS_VAL            = 1 << 3;

// Node's may be trie nodes or buckets. This union allows us to keep non-specific pointer.
typedef union htr_node_ptr_t {
    htr_table *              table;
    struct htr_trie_node_t * trie_node;
    uint8_t *                flag;
} htr_node_ptr;

struct htr_t {
    htr_node_ptr      root;
    size_t            pairs_count;
    htr_hash_function hash_function;
};

typedef struct htr_trie_node_t {
    uint8_t flag;

    // the value for the key that is consumed on a trie node
    htr_value value;

    // Map a character to either a htr_trie_node_t or a htr_table_t.
    // The first byte must be examined to determine which.
    htr_node_ptr xs[NODE_CHILDS];
} htr_trie_node;

// Create a new trie node with all pointer pointing to the given child (which can be NULL).
static htr_trie_node * alloc_trie_node ( htr * trie, htr_node_ptr child )
{
    htr_trie_node * node = malloc ( sizeof ( htr_trie_node ) );
    node->flag = NODE_TYPE_TRIE;
    node->value  = 0;

    /* pass trie to allow custom allocator for trie. */
    HT_UNUSED ( trie ); /* unused now */

    size_t i;
    for ( i = 0; i < NODE_CHILDS; ++i ) node->xs[i] = child;
    return node;
}

// iterate trie nodes until string is consumed or bucket is found
static inline
htr_node_ptr consume ( htr_node_ptr * p, const char ** k, size_t * l, unsigned brk )
{
    htr_node_ptr node = p->trie_node->xs[ ( unsigned char ) ** k];
    while ( * node.flag & NODE_TYPE_TRIE && * l > brk ) {
        ++ * k;
        -- * l;
        * p  = node;
        node = node.trie_node->xs[ ( unsigned char ) ** k];
    }

    // copy and writeback variables if it's faster

    assert ( *p->flag & NODE_TYPE_TRIE );
    return node;
}

// use node value and return pointer to it
static inline
htr_value * useval ( htr * trie, htr_node_ptr node )
{
    if ( ! ( node.trie_node->flag & NODE_HAS_VAL ) ) {
        node.trie_node->flag |= NODE_HAS_VAL;
        trie->pairs_count++;
    }
    return &node.trie_node->value;
}

// clear node value if exists
static inline
int clear_value ( htr * trie, htr_node_ptr node )
{
    if ( ! ( node.trie_node->flag & NODE_HAS_VAL ) ) {
        return -1;
    }
    node.trie_node->flag &= ~NODE_HAS_VAL;
    node.trie_node->value = 0;
    trie->pairs_count--;
    return 0;
}

// find node in trie
static htr_node_ptr hattrie_find ( htr* T, const char **key, size_t *len )
{
    htr_node_ptr parent = T->root;
    assert ( *parent.flag & NODE_TYPE_TRIE );

    if ( *len == 0 ) return parent;

    htr_node_ptr node = consume ( &parent, key, len, 1 );

    /* if the trie node consumes value, use it */
    if ( *node.flag & NODE_TYPE_TRIE ) {
        if ( ! ( node.trie_node->flag & NODE_HAS_VAL ) ) {
            node.flag = NULL;
        }
        return node;
    }

    /* pure bucket holds only key suffixes, skip current char */
    if ( *node.flag & NODE_TYPE_PURE_BUCKET ) {
        *key += 1;
        *len -= 1;
    }

    /* do not scan bucket, it's not needed for this operation */
    return node;
}

static inline
void htr_free_node ( htr_node_ptr node )
{
    if ( *node.flag & NODE_TYPE_TRIE ) {
        size_t i;
        for ( i = 0; i < NODE_CHILDS; ++i ) {
            if ( i > 0 && node.trie_node->xs[i].trie_node == node.trie_node->xs[i - 1].trie_node ) continue;

            /* XXX: recursion might not be the best choice here. It is possible
             * to build a very deep trie. */
            if ( node.trie_node->xs[i].trie_node ) htr_free_node ( node.trie_node->xs[i] );
        }
        free ( node.trie_node );
    } else {
        htr_table_free ( node.table );
    }
}

static
uint8_t htr_free ( void * child_data, void * user_data )
{
    htr * trie = child_data;
    htr_free_node ( trie->root );
    return 0;
}

htr * htr_new ( void * ctx, htr_hash_function hash_function )
{
    htr * trie = talloc ( ctx, sizeof ( htr ) );
    if ( trie == NULL ) {
        return NULL;
    }
    if ( talloc_add_destructor ( trie, htr_free, NULL ) != 0 ) {
        talloc_free ( trie );
        return NULL;
    }
    trie->pairs_count   = 0;
    trie->hash_function = hash_function;

    htr_node_ptr node;
    node.table = htr_table_new ();
    node.table->flag = NODE_TYPE_HYBRID_BUCKET;
    node.table->c0 = 0x00;
    node.table->c1 = NODE_MAXCHAR;
    trie->root.trie_node = alloc_trie_node ( trie, node );

    return trie;
}

/* Perform one split operation on the given node with the given parent.
 */
static void hattrie_split ( htr * T, htr_node_ptr parent, htr_node_ptr node )
{
    /* only buckets may be split */
    assert ( *node.flag & NODE_TYPE_PURE_BUCKET ||
             *node.flag & NODE_TYPE_HYBRID_BUCKET );

    assert ( *parent.flag & NODE_TYPE_TRIE );

    if ( *node.flag & NODE_TYPE_PURE_BUCKET ) {
        /* turn the pure bucket into a hybrid bucket */
        parent.trie_node->xs[node.table->c0].trie_node = alloc_trie_node ( T, node );

        /* if the bucket had an empty key, move it to the new trie node */
        htr_value * val = htr_table_tryget ( node.table, T->hash_function, NULL, 0 );
        if ( val ) {
            parent.trie_node->xs[node.table->c0].trie_node->value = *val;
            parent.trie_node->xs[node.table->c0].trie_node->flag  |= NODE_HAS_VAL;
            *val = 0;
            htr_table_del ( node.table, T->hash_function, NULL, 0 );
        }

        node.table->c0   = 0x00;
        node.table->c1   = NODE_MAXCHAR;
        node.table->flag = NODE_TYPE_HYBRID_BUCKET;

        return;
    }

    /* This is a hybrid bucket. Perform a proper split. */

    /* count the number of occourances of every leading character */
    unsigned int cs[NODE_CHILDS]; // occurance count for leading chars
    memset ( cs, 0, NODE_CHILDS * sizeof ( unsigned int ) );
    size_t len;
    const char* key;

    htr_table_iterator * i = htr_table_iter_begin ( node.table, false );
    while ( !htr_table_iter_finished ( i ) ) {
        key = htr_table_iter_key ( i, &len );
        assert ( len > 0 );
        cs[ ( unsigned char ) key[0]] += 1;
        htr_table_iter_next ( i );
    }
    htr_table_iter_free ( i );

    /* choose a split point */
    unsigned int left_m, right_m, all_m;
    unsigned char j = node.table->c0;
    all_m   = htr_table_size ( node.table );
    left_m  = cs[j];
    right_m = all_m - left_m;
    int d;

    while ( j + 1 < node.table->c1 ) {
        d = abs ( ( int ) ( left_m + cs[j + 1] ) - ( int ) ( right_m - cs[j + 1] ) );
        if ( d <= abs ( left_m - right_m ) && left_m + cs[j + 1] < all_m ) {
            j += 1;
            left_m  += cs[j];
            right_m -= cs[j];
        } else break;
    }

    /* now split into two node cooresponding to ranges [0, j] and
     * [j + 1, NODE_MAXCHAR], respectively. */


    /* create new left and right nodes */

    /* TODO: Add a special case if either node is a hybrid bucket containing all
     * the keys. In such a case, do not build a new table, just use the old one.
     * */
    size_t num_slots;


    for ( num_slots = htr_table_initial_size;
            ( double ) left_m > htr_table_max_load_factor * ( double ) num_slots;
            num_slots *= 2 );

    htr_node_ptr left, right;
    left.table  = htr_table_new_n ( num_slots );
    left.table->c0   = node.table->c0;
    left.table->c1   = j;
    left.table->flag = left.table->c0 == left.table->c1 ?
                       NODE_TYPE_PURE_BUCKET : NODE_TYPE_HYBRID_BUCKET;


    for ( num_slots = htr_table_initial_size;
            ( double ) right_m > htr_table_max_load_factor * ( double ) num_slots;
            num_slots *= 2 );

    right.table = htr_table_new_n ( num_slots );
    right.table->c0   = j + 1;
    right.table->c1   = node.table->c1;
    right.table->flag = right.table->c0 == right.table->c1 ?
                        NODE_TYPE_PURE_BUCKET : NODE_TYPE_HYBRID_BUCKET;


    /* update the parent's pointer */

    unsigned int c;
    for ( c = node.table->c0; c <= j; ++c ) parent.trie_node->xs[c] = left;
    for ( ; c <= node.table->c1; ++c )      parent.trie_node->xs[c] = right;



    /* distribute keys to the new left or right node */
    htr_value * u;
    htr_value * v;
    i = htr_table_iter_begin ( node.table, false );
    while ( !htr_table_iter_finished ( i ) ) {
        key = htr_table_iter_key ( i, &len );
        u   = htr_table_iter_val ( i );
        assert ( len > 0 );

        /* left */
        if ( ( unsigned char ) key[0] <= j ) {
            if ( *left.flag & NODE_TYPE_PURE_BUCKET ) {
                v = htr_table_get ( left.table, T->hash_function, key + 1, len - 1 );
            } else {
                v = htr_table_get ( left.table, T->hash_function, key, len );
            }
            *v = *u;
        }

        /* right */
        else {
            if ( *right.flag & NODE_TYPE_PURE_BUCKET ) {
                v = htr_table_get ( right.table, T->hash_function, key + 1, len - 1 );
            } else {
                v = htr_table_get ( right.table, T->hash_function, key, len );
            }
            *v = *u;
        }

        htr_table_iter_next ( i );
    }

    htr_table_iter_free ( i );
    htr_table_free ( node.table );
}

htr_value * htr_get ( htr * T, const char* key, size_t len )
{
    htr_node_ptr parent = T->root;
    assert ( *parent.flag & NODE_TYPE_TRIE );

    if ( len == 0 ) return &parent.trie_node->value;

    /* consume all trie nodes, now parent must be trie and child anything */
    htr_node_ptr node = consume ( &parent, &key, &len, 0 );
    assert ( *parent.flag & NODE_TYPE_TRIE );

    /* if the key has been consumed on a trie node, use its value */
    if ( len == 0 ) {
        if ( *node.flag & NODE_TYPE_TRIE ) {
            return useval ( T, node );
        } else if ( *node.flag & NODE_TYPE_HYBRID_BUCKET ) {
            return useval ( T, parent );
        }
    }


    /* preemptively split the bucket if it is full */
    while ( htr_table_size ( node.table ) >= MAX_BUCKET_SIZE ) {
        hattrie_split ( T, parent, node );

        /* after the split, the node pointer is invalidated, so we search from
         * the parent again. */
        node = consume ( &parent, &key, &len, 0 );

        /* if the key has been consumed on a trie node, use its value */
        if ( len == 0 ) {
            if ( *node.flag & NODE_TYPE_TRIE ) {
                return useval ( T, node );
            } else if ( *node.flag & NODE_TYPE_HYBRID_BUCKET ) {
                return useval ( T, parent );
            }
        }
    }

    assert ( *node.flag & NODE_TYPE_PURE_BUCKET || *node.flag & NODE_TYPE_HYBRID_BUCKET );

    assert ( len > 0 );
    size_t m_old = node.table->pairs_count;
    htr_value * val;
    if ( *node.flag & NODE_TYPE_PURE_BUCKET ) {
        val = htr_table_get ( node.table, T->hash_function, key + 1, len - 1 );
    } else {
        val = htr_table_get ( node.table, T->hash_function, key, len );
    }
    T->pairs_count += ( node.table->pairs_count - m_old );

    return val;
}


htr_value * htr_tryget ( htr * T, const char* key, size_t len )
{
    /* find node for given key */
    htr_node_ptr node = hattrie_find ( T, &key, &len );
    if ( node.flag == NULL ) {
        return NULL;
    }

    /* if the trie node consumes value, use it */
    if ( *node.flag & NODE_TYPE_TRIE ) {
        return &node.trie_node->value;
    }

    return htr_table_tryget ( node.table, T->hash_function, key, len );
}


int htr_del ( htr * T, const char* key, size_t len )
{
    htr_node_ptr parent = T->root;
    assert ( *parent.flag & NODE_TYPE_TRIE );

    /* find node for deletion */
    htr_node_ptr node = hattrie_find ( T, &key, &len );
    if ( node.flag == NULL ) {
        return -1;
    }

    /* if consumed on a trie node, clear the value */
    if ( *node.flag & NODE_TYPE_TRIE ) {
        return clear_value ( T, node );
    }

    /* remove from bucket */
    size_t m_old = htr_table_size ( node.table );
    int ret =  htr_table_del ( node.table, T->hash_function, key, len );
    T->pairs_count -= ( m_old - htr_table_size ( node.table ) );

    /* merge empty buckets */
    /*! \todo */

    return ret;
}


/* plan for iteration:
 * This is tricky, as we have no parent pointers currently, and I would like to
 * avoid adding them. That means maintaining a stack
 *
 */

typedef struct hattrie_node_stack_t_ {
    unsigned char   c;
    size_t level;

    htr_node_ptr node;
    struct hattrie_node_stack_t_* next;

} hattrie_node_stack_t;


struct htr_iterator_t {
    char* key;
    size_t keysize; // space reserved for the key
    size_t level;

    /* keep track of keys stored in trie nodes */
    bool    has_nil_key;
    htr_value nil_val;

    const htr * T;
    bool sorted;
    htr_table_iterator * i;
    hattrie_node_stack_t* stack;
};


static void htr_iterator_pushchar ( htr_iterator * i, size_t level, char c )
{
    if ( i->keysize < level ) {
        i->keysize *= 2;
        i->key = realloc ( i->key, i->keysize * sizeof ( char ) );
    }

    if ( level > 0 ) {
        i->key[level - 1] = c;
    }

    i->level = level;
}


static void htr_iterator_nextnode ( htr_iterator * i )
{
    if ( i->stack == NULL ) return;

    /* pop the stack */
    htr_node_ptr node;
    hattrie_node_stack_t* next;
    unsigned char   c;
    size_t level;

    node  = i->stack->node;
    next  = i->stack->next;
    c     = i->stack->c;
    level = i->stack->level;

    free ( i->stack );
    i->stack = next;

    if ( *node.flag & NODE_TYPE_TRIE ) {
        htr_iterator_pushchar ( i, level, c );

        if ( node.trie_node->flag & NODE_HAS_VAL ) {
            i->has_nil_key = true;
            i->nil_val = node.trie_node->value;
        }

        /* push all child nodes from right to left */
        int j;
        for ( j = NODE_MAXCHAR; j >= 0; --j ) {

            /* skip repeated pointers to hybrid bucket */
            if ( j < NODE_MAXCHAR && node.trie_node->xs[j].trie_node == node.trie_node->xs[j + 1].trie_node ) continue;

            // push stack
            next = i->stack;
            i->stack = malloc ( sizeof ( hattrie_node_stack_t ) );
            i->stack->node  = node.trie_node->xs[j];
            i->stack->next  = next;
            i->stack->level = level + 1;
            i->stack->c     = ( unsigned char ) j;
        }
    } else {
        if ( *node.flag & NODE_TYPE_PURE_BUCKET ) {
            htr_iterator_pushchar ( i, level, c );
        } else {
            i->level = level - 1;
        }

        i->i = htr_table_iter_begin ( node.table, i->sorted );
    }
}


htr_iterator * htr_iterator_begin ( const htr * T, bool sorted )
{
    htr_iterator * i = malloc ( sizeof ( htr_iterator ) );
    i->T = T;
    i->sorted = sorted;
    i->i = NULL;
    i->keysize = 16;
    i->key = malloc ( i->keysize * sizeof ( char ) );
    i->level   = 0;
    i->has_nil_key = false;
    i->nil_val     = 0;

    i->stack = malloc ( sizeof ( hattrie_node_stack_t ) );
    i->stack->next   = NULL;
    i->stack->node   = T->root;
    i->stack->c      = '\0';
    i->stack->level  = 0;


    while ( ( ( i->i == NULL || htr_table_iter_finished ( i->i ) ) && !i->has_nil_key ) &&
            i->stack != NULL ) {

        htr_table_iter_free ( i->i );
        i->i = NULL;
        htr_iterator_nextnode ( i );
    }

    if ( i->i != NULL && htr_table_iter_finished ( i->i ) ) {
        htr_table_iter_free ( i->i );
        i->i = NULL;
    }

    return i;
}


void htr_iterator_next ( htr_iterator * i )
{
    if ( htr_iterator_finished ( i ) ) return;

    if ( i->i != NULL && !htr_table_iter_finished ( i->i ) ) {
        htr_table_iter_next ( i->i );
    } else if ( i->has_nil_key ) {
        i->has_nil_key = false;
        i->nil_val = 0;
        htr_iterator_nextnode ( i );
    }

    while ( ( ( i->i == NULL || htr_table_iter_finished ( i->i ) ) && !i->has_nil_key ) &&
            i->stack != NULL ) {

        htr_table_iter_free ( i->i );
        i->i = NULL;
        htr_iterator_nextnode ( i );
    }

    if ( i->i != NULL && htr_table_iter_finished ( i->i ) ) {
        htr_table_iter_free ( i->i );
        i->i = NULL;
    }
}


bool htr_iterator_finished ( htr_iterator * i )
{
    return i->stack == NULL && i->i == NULL && !i->has_nil_key;
}


void htr_iterator_free ( htr_iterator * i )
{
    if ( i == NULL ) return;
    if ( i->i ) htr_table_iter_free ( i->i );

    hattrie_node_stack_t* next;
    while ( i->stack ) {
        next = i->stack->next;
        free ( i->stack );
        i->stack = next;
    }

    free ( i->key );
    free ( i );
}


const char* htr_iterator_key ( htr_iterator * i, size_t* len )
{
    if ( htr_iterator_finished ( i ) ) return NULL;

    size_t sublen;
    const char* subkey;

    if ( i->has_nil_key ) {
        subkey = NULL;
        sublen = 0;
    } else subkey = htr_table_iter_key ( i->i, &sublen );

    if ( i->keysize < i->level + sublen + 1 ) {
        while ( i->keysize < i->level + sublen + 1 ) i->keysize *= 2;
        i->key = realloc ( i->key, i->keysize * sizeof ( char ) );
    }

    memcpy ( i->key + i->level, subkey, sublen );
    i->key[i->level + sublen] = '\0';

    *len = i->level + sublen;
    return i->key;
}


htr_value * htr_iterator_val ( htr_iterator * i )
{
    if ( i->has_nil_key ) return &i->nil_val;

    if ( htr_iterator_finished ( i ) ) return NULL;

    return htr_table_iter_val ( i->i );
}



