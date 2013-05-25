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
#ifndef _ATREE_H_
#	define _ATREE_H_

#include <stdlib.h>
#include <string.h>
#include "llist.h"

/*
 * Implementation of an n-ary tree, where each link is represented
 * by a char of a key and its links by next chars.
 */
typedef struct _atree {
	/*
	 * The byte value of this link.
	 */
	unsigned char ascii;
	/*
	 * Value of the link (end marker of a chain).
	 */
	void*   	  e_marker;
	/*
	 * Number of sub links.
	 */
	unsigned short n_links;
	/*
	 * Sub links dynamic array.
	 */
	struct _atree *links;
}
atree_t;

typedef atree_t atree_item_t;

/*
 * Initialize the head of the tree.
 */
#define at_init_tree( t )    (t).n_links  = 0; \
						     (t).e_marker = 0; \
						     (t).links    = NULL
/*
 * Allocate and initialize a link of the tree.
 */
#define at_init_link( l, k ) l = (atree_t *)calloc( 1, sizeof(atree_t) ); \
						     l->ascii = k[0]

#define at_clear at_free

/*
 * Insert 'value' inside 'at' ascii tree, mapped by
 * the given 'key' of 'len' bytes.
 */
void *at_insert( atree_t *at, unsigned char *key, int len, void *value );

atree_t *at_find_node( atree_t *at, unsigned char *key, int len );

/*
 * Find the object mapped mapped by the given 'key'
 * of 'len' bytes inside 'at' ascii tree.
 */
void *at_find( atree_t *at, unsigned char *key, int len );

typedef void (*at_recurse_handler)(atree_item_t *, size_t, void *);

void at_recurse( atree_t *at, at_recurse_handler handler, void *data, size_t level );

size_t at_search( atree_t *at, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **values );
size_t at_search_nodes( atree_t *at, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **nodes );

/*
 * Remove the object from the tree and return its pointer.
 */
void *at_remove( atree_t *at, unsigned char *key, int len );
/*
 * Free the tree nodes.
 */
void  at_free( atree_t *at );

#endif
