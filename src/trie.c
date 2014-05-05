/*
 * Copyright (c) 2013, Simone Margaritelli <evilsocket at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Gibson nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "trie.h"

static size_t tr_node_children( trie_t *trie )
{
    return trie->nodes == NULL ? 0 : ( trie->n_nodes + 1 );
}

// This is O(N) since the trie->nodes array is not sorted.
static tnode_t *tr_find_next_node( trie_t *trie, unsigned char value )
{
	int n = tr_node_children(trie);
	trie_t *node = NULL;

    if( n && trie->nodes[trie->last].value == value )
    {
        return &trie->nodes[trie->last];
    }

    while( --n >= 0 )
    {
		node = trie->nodes + n;
		if( node->value == value )
        {
            trie->last = n;

			return node;
		}
	}

	return NULL;
}

void *tr_insert( trie_t *trie, unsigned char *key, int len, void *value )
{
	tnode_t *parent = trie, *node = NULL;
	size_t i;
	char v;
	unsigned short current_size;

	for( i = 0; i < len; ++i )
    {
		v = key[i];
		node  = tr_find_next_node( parent, v );

		if( node == NULL )
        {
			/*
			 * Allocate, initialize and append a new node.
			 *
			 * NOTE: As a future optimization, the parent->nodes new array
			 * will be quick-sorted on every reallocation, so the search with
			 * tr_find_next_node would be O(log N) instead of O(N).
			 */
			current_size    = tr_node_children( parent );
			parent->nodes   = zrealloc( parent->nodes, sizeof(tnode_t) * ( current_size + 1 ) );
			parent->n_nodes = current_size;
            node		    = parent->nodes + current_size;

			node->value   = v;
			node->data    =
			node->nodes   = NULL;
			node->n_nodes = 0;
		}

		parent = node;
	}

	void *old = node->data;
    node->data = value;

	return old;
}

trie_t *tr_find_node( trie_t *trie, unsigned char *key, int len )
{
	tnode_t *node = trie;
	int i = 0;

	do
    {
		// Find next node ad continue.
		node = tr_find_next_node( node, key[i++] );
	}
	while( --len && node );

	return node;
}

void *tr_find( trie_t *trie, unsigned char *key, int len )
{
	tnode_t *node = tr_find_node( trie, key, len );
	/*
	 * End of the chain, if e_data is NULL this chain is not complete,
	 * therefore 'key' does not map any alive object.
	 */
	return ( node ? node->data : NULL );
}

void tr_recurse( trie_t *trie, tr_recurse_handler handler, void *data, size_t level ) 
{
	size_t i, nnodes = tr_node_children(trie);

	handler( trie, level, data );

	for( i = 0; i < nnodes; ++i )
    {
		tr_recurse( trie->nodes + i, handler, data, level + 1 );
	}
}

struct tr_search_data 
{
	llist_t **keys;
	llist_t **values;
	char    *current;
	size_t   total;
};

static void tr_search_recursive_handler(tnode_t *node, size_t level, void *data)
{
	struct tr_search_data *search = data;

	search->current[ level ] = node->value;

	// found a value
	if( node->data != NULL )
    {
		++search->total;
		search->current[ level + 1 ] = '\0';

		ll_append( *search->keys, zstrdup( search->current ) );

        if( search->values != NULL )
        {
		    ll_append( *search->values, node->data );
        }
	}
}

size_t tr_search( trie_t *trie, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **values ) 
{
	struct tr_search_data searchdata;

	searchdata.keys    = keys;
	searchdata.values  = values;
	searchdata.current = alloca( maxkeylen );
	searchdata.total   = 0;

	tnode_t *start = tr_find_node( trie, prefix, len );

	if( start )
    {
		strncpy( searchdata.current, (char *)prefix, len );

		tr_recurse( start, tr_search_recursive_handler, &searchdata, len - 1 );
	}

	return searchdata.total;
}

struct tr_search_nodes_data 
{
	llist_t **keys;
	llist_t **nodes;
	char    *current;
	size_t   total;
};

static void tr_search_nodes_recursive_handler(tnode_t *node, size_t level, void *data)
{
	struct tr_search_nodes_data *search = data;

	search->current[ level ] = node->value;

	// found a value
	if( node->data != NULL )
    {
		++search->total;
		search->current[ level + 1 ] = '\0';

		ll_append( *search->keys,  zstrdup( search->current ) );
		ll_append( *search->nodes, node );
	}
}

size_t tr_search_nodes( trie_t *trie, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **nodes )
{
	struct tr_search_nodes_data searchdata;

	searchdata.keys    = keys;
	searchdata.nodes   = nodes;
	searchdata.current = alloca( maxkeylen );
	searchdata.total   = 0;

	tnode_t *start = tr_find_node( trie, prefix, len );

	if( start )
    {
		strncpy( searchdata.current, (char *)prefix, len );

		tr_recurse( start, tr_search_nodes_recursive_handler, &searchdata, len - 1 );
	}

	return searchdata.total;
}

void *tr_remove( trie_t *trie, unsigned char *key, int len )
{
	tnode_t *node = trie;
	int i = 0;

	do
    {
		// Find next link ad continue.
		node = tr_find_next_node( node, key[i++] );
	}
	while( --len && node );

	/*
	 * End of the chain, if e_data is NULL this chain is not complete,
	 * therefore 'key' does not map any alive object.
	 */
	if( node && node->data )
    {
		void *retn = node->data;

		node->data = NULL;

		return retn;
	}
	else
    {
		return NULL;
    }
}

void tr_free( trie_t *trie )
{
	int i, nnodes = tr_node_children(trie);
	/*
	 * Better be safe than sorry ;)
	 */
	if( nnodes )
    {
		/*
		 * First of all, loop all the sub links.
		 */
		for( i = 0; i < nnodes; ++i ){
			/*
			 * Free this node children.
			 */
			tr_free( trie->nodes + i );
		}

		/*
		 * Free the node itself.
		 */
		zfree( trie->nodes );
		trie->nodes = NULL;
	}
}
