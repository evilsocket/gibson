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
#ifndef _TRIE_H_
#	define _TRIE_H_

#include <stdlib.h>
#include <string.h>
#include "llist.h"

typedef struct _trie
{
	// The byte value of this node.
	unsigned char value;
	// Data of the node (end marker of a chain).
	void*   	  data;
	// Number of children ( base 0 ).
	unsigned char n_nodes;
	// Child nodes dynamic array.
	struct _trie *nodes;
}
__attribute__((packed)) trie_t;

typedef trie_t tnode_t;

typedef void (*tr_recurse_handler)(tnode_t *, size_t, void *);

#define tr_init_tree( t ) \
    (t).n_nodes = 0; \
    (t).data    = 0; \
	(t).nodes   = NULL

void   *tr_insert( trie_t *at, unsigned char *key, int len, void *value );
trie_t *tr_find_node( trie_t *at, unsigned char *key, int len );
void   *tr_find( trie_t *at, unsigned char *key, int len );
void    tr_recurse( trie_t *at, tr_recurse_handler handler, void *data, size_t level );
size_t  tr_search( trie_t *at, unsigned char *prefix, int len, long limit, int maxkeylen, llist_t **keys, llist_t **values );
size_t  tr_search_nodes( trie_t *at, unsigned char *prefix, int len, int maxkeylen, llist_t **keys, llist_t **nodes );
void   *tr_remove( trie_t *at, unsigned char *key, int len );
void    tr_free( trie_t *at );

#endif
