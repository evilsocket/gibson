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
#include "atree.h"

static size_t at_node_children( atree_t *at )
{
    return at->nodes == NULL ? 0 : ( at->n_nodes + 1 );
}

// This is O(N) since the at->nodes array is not sorted.
static anode_t *at_find_next_node( atree_t *at, unsigned char ascii ){
	int n = at_node_children(at);
	atree_t *node = NULL;

	while( --n >= 0 ){
		node = at->nodes + n;
		if( node->ascii == ascii ){
			return node;
		}
	}

	return NULL;
}

void *at_insert( atree_t *at, unsigned char *key, int len, void *value ){
	anode_t *parent = at, *node = NULL;
	size_t i;
	char ascii;
	unsigned short current_size;

	for( i = 0; i < len; ++i ){
		ascii = key[i];
		node  = at_find_next_node( parent, ascii );
		if( node == NULL ){
			/*
			 * Allocate, initialize and append a new node.
			 *
			 * NOTE: As a future optimization, the parent->nodes new array
			 * will be quick-sorted on every reallocation, so the search with
			 * at_find_next_node would be O(log N) instead of O(N).
			 */
			current_size    = at_node_children( parent );
			parent->nodes   = zrealloc( parent->nodes, sizeof(atree_t) * ( current_size + 1 ) );
			parent->n_nodes = current_size;
            node		    = parent->nodes + current_size;

			node->ascii   = ascii;
			node->marker  =
			node->nodes   = NULL;
			node->n_nodes = 0;
		}

		parent = node;
	}

	void *old = node->marker;
    node->marker = value;

	return old;
}

atree_t *at_find_node( atree_t *at, unsigned char *key, int len ){
	anode_t *node = at;
	int i = 0;

	do{
		// Find next node ad continue.
		node = at_find_next_node( node, key[i++] );
	}
	while( --len && node );

	return node;
}

void *at_find( atree_t *at, unsigned char *key, int len ){
	anode_t *node = at_find_node( at, key, len );
	/*
	 * End of the chain, if e_marker is NULL this chain is not complete,
	 * therefore 'key' does not map any alive object.
	 */
	return ( node ? node->marker : NULL );
}

void at_recurse( atree_t *at, at_recurse_handler handler, void *data, size_t level ) {
	size_t i, nnodes = at_node_children(at);

	handler( at, level, data );

	for( i = 0; i < nnodes; ++i ){
		at_recurse( at->nodes + i, handler, data, level + 1 );
	}
}

struct at_search_data {
	llist_t **keys;
	llist_t **values;
	char    *current;
	size_t   total;
};

static void at_search_recursive_handler(anode_t *node, size_t level, void *data){
	struct at_search_data *search = data;

	search->current[ level ] = node->ascii;

	// found a value
	if( node->marker != NULL ){
		++search->total;
		search->current[ level + 1 ] = '\0';

		ll_append( *search->keys, zstrdup( search->current ) );

        if( search->values != NULL )
		    ll_append( *search->values, node->marker );
	}
}

size_t at_search( atree_t *at, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **values ) {
	struct at_search_data searchdata;

	searchdata.keys    = keys;
	searchdata.values  = values;
	searchdata.current = alloca( maxkeylen );
	searchdata.total   = 0;

	anode_t *start = at_find_node( at, prefix, len );

	if( start ){
		strncpy( searchdata.current, (char *)prefix, len );

		at_recurse( start, at_search_recursive_handler, &searchdata, len - 1 );
	}

	return searchdata.total;
}

struct at_search_nodes_data {
	llist_t **keys;
	llist_t **nodes;
	char    *current;
	size_t   total;
};

static void at_search_nodes_recursive_handler(anode_t *node, size_t level, void *data){
	struct at_search_nodes_data *search = data;

	search->current[ level ] = node->ascii;

	// found a value
	if( node->marker != NULL ){
		++search->total;
		search->current[ level + 1 ] = '\0';

		ll_append( *search->keys,  zstrdup( search->current ) );
		ll_append( *search->nodes, node );
	}
}

size_t at_search_nodes( atree_t *at, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **nodes ){
	struct at_search_nodes_data searchdata;

	searchdata.keys    = keys;
	searchdata.nodes   = nodes;
	searchdata.current = alloca( maxkeylen );
	searchdata.total   = 0;

	anode_t *start = at_find_node( at, prefix, len );

	if( start ){
		strncpy( searchdata.current, (char *)prefix, len );

		at_recurse( start, at_search_nodes_recursive_handler, &searchdata, len - 1 );
	}

	return searchdata.total;
}

void *at_remove( atree_t *at, unsigned char *key, int len ){
	anode_t *node = at;
	int i = 0;

	do{
		// Find next link ad continue.
		node = at_find_next_node( node, key[i++] );
	}
	while( --len && node );
	/*
	 * End of the chain, if e_marker is NULL this chain is not complete,
	 * therefore 'key' does not map any alive object.
	 */
	if( node && node->marker ){
		void *retn = node->marker;

		node->marker = NULL;

		return retn;
	}
	else
		return NULL;
}

void at_free( atree_t *at ){
	int i, nnodes = at_node_children(at);
	/*
	 * Better be safe than sorry ;)
	 */
	if( nnodes ){
		/*
		 * First of all, loop all the sub links.
		 */
		for( i = 0; i < nnodes; ++i ){
			/*
			 * Free this node children.
			 */
			at_free( at->nodes + i );
		}

		/*
		 * Free the node itself.
		 */
		zfree( at->nodes );
		at->nodes = NULL;
	}
}
