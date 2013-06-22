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
/*
 * Find next link with 'ascii' byte.
 */
atree_item_t *at_find_next_link( atree_t *at, unsigned char ascii ){
	int n = at->n_links;
	atree_t *node = NULL;

	while( --n >= 0 ){
		node = at->links + n;
		if( node->ascii == ascii ){
			return node;
		}
	}

	return NULL;
}

void *at_insert( atree_t *at, unsigned char *key, int len, void *value ){
	unsigned char first = key[0];

	/*
	 * End of the chain, set the marker value and exit the recursion.
	 */
	if(len == 0){
		void *old = at->e_marker;
		at->e_marker = value;

		return old;
	}
	/*
	 * Has the item a link with given byte?
	 */
	atree_item_t *link = at_find_next_link( at, first );
	if( link ){
		/*
		 * Next recursion, search next byte,
		 */
		return at_insert( link, ++key, --len, value );
	}
	/*
	 * Nothing found.
	 */
	else{
		unsigned short current = at->n_links;
		/*
		 * Allocate, initialize and append a new link.
		 */
		at->links = realloc( at->links, sizeof(atree_t) * ++at->n_links );

		link = at->links + current;
		link->ascii    = first;
		link->e_marker =
		link->links    = NULL;
		link->n_links  = 0;
		/*
		 * Continue with next byte.
		 */
		return at_insert( link, ++key, --len, value );
	}

	return NULL;
}

atree_t *at_find_node( atree_t *at, unsigned char *key, int len ){
	atree_item_t *link = at;
	int i = 0;

	do{
		/*
		 * Find next link ad continue.
		 */
		link = at_find_next_link( link, key[i++] );
	}
	while( --len && link );

	return link;
}

void *at_find( atree_t *at, unsigned char *key, int len ){
	atree_item_t *link = at_find_node( at, key, len );
	/*
	 * End of the chain, if e_marker is NULL this chain is not complete,
	 * therefore 'key' does not map any alive object.
	 */
	return ( link ? link->e_marker : NULL );
}

void at_recurse( atree_t *at, at_recurse_handler handler, void *data, size_t level ) {
	size_t i;

	handler( at, level, data );

	for( i = 0; i < at->n_links; i++ ){
		at_recurse( at->links + i, handler, data, level + 1 );
	}
}

struct at_search_data {
	llist_t **keys;
	llist_t **values;
	char    *current;
	size_t   total;
};

void at_search_recursive_handler(atree_item_t *node, size_t level, void *data){
	struct at_search_data *search = data;

	search->current[ level ] = node->ascii;

	// found a value
	if( node->e_marker != NULL ){
		++search->total;
		search->current[ level + 1 ] = '\0';

		ll_append( *search->keys,   strdup( search->current ) );
		ll_append( *search->values, node->e_marker );
	}
}

size_t at_search( atree_t *at, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **values ) {
	struct at_search_data searchdata;

	searchdata.keys    = keys;
	searchdata.values  = values;
	searchdata.current = NULL;
	searchdata.total   = 0;

	atree_item_t *start = at_find_node( at, prefix, len );

	if( start ){
		searchdata.current = calloc( 1, maxkeylen );

		strncpy( searchdata.current, (char *)prefix, len );

		at_recurse( start, at_search_recursive_handler, &searchdata, len - 1 );

		free( searchdata.current );
	}

	return searchdata.total;
}

struct at_search_nodes_data {
	llist_t **keys;
	llist_t **nodes;
	char    *current;
	size_t   total;
};

void at_search_nodes_recursive_handler(atree_item_t *node, size_t level, void *data){
	struct at_search_nodes_data *search = data;

	search->current[ level ] = node->ascii;

	// found a value
	if( node->e_marker != NULL ){
		++search->total;
		search->current[ level + 1 ] = '\0';

		ll_append( *search->keys,  strdup( search->current ) );
		ll_append( *search->nodes, node );
	}
}

size_t at_search_nodes( atree_t *at, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **nodes ){
	struct at_search_nodes_data searchdata;

	searchdata.keys    = keys;
	searchdata.nodes   = nodes;
	searchdata.current = NULL;
	searchdata.total   = 0;

	atree_item_t *start = at_find_node( at, prefix, len );

	if( start ){
		searchdata.current = calloc( 1, maxkeylen );

		strncpy( searchdata.current, (char *)prefix, len );

		at_recurse( start, at_search_nodes_recursive_handler, &searchdata, len - 1 );

		free( searchdata.current );
	}

	return searchdata.total;
}

void *at_remove( atree_t *at, unsigned char *key, int len ){
	atree_item_t *link = at;
	int i = 0;

	do{
		/*
		 * Find next link ad continue.
		 */
		link = at_find_next_link( link, key[i++] );
	}
	while( --len && link );
	/*
	 * End of the chain, if e_marker is NULL this chain is not complete,
	 * therefore 'key' does not map any alive object.
	 */
	if( link && link->e_marker ){
		void *retn = link->e_marker;

		link->e_marker = NULL;

		return retn;
	}
	else
		return NULL;
}

void at_free( atree_t *at ){
	int i, n_links = at->n_links;
	/*
	 * Better be safe than sorry ;)
	 */
	if( at->links ){
		/*
		 * First of all, loop all the sub links.
		 */
		for( i = 0; i < n_links; ++i, --at->n_links ){
			/*
			 * Free this link sub-links.
			 */
			at_free( at->links + i );
		}

		/*
		 * Free the link itself.
		 */
		free( at->links );
		at->links = NULL;
	}
}
