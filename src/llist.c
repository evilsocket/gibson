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
#include "llist.h"

void ll_append_new( llist_t *ll, void *data );

llist_t *ll_prealloc( size_t elements ){
	llist_t *list = ll_create();

	while( elements-- )
		ll_append( list, NULL );

	return list;
}

void ll_append( llist_t *ll, void *data ){
	ll_item_t *item;
	for( item = ll->head; item; item = item->next ){
		if( item->data == NULL ){
			item->data = data;
			return;
		}
	}

	ll_append_new( ll, data );
}

void ll_append_new( llist_t *ll, void *data ){
	ll_item_t *item = (ll_item_t *)zcalloc( sizeof(ll_item_t) );

	item->data = data;

	if( ll->head == NULL ){
		ll->head = item;
	}
	else{
		ll->tail->next = item;
	}
	ll->tail = item;
}

void ll_clear( llist_t *ll ){
	ll_item_t *item = ll ? ll->head : NULL,
			  *next;

	while(item){
		next = item->next;
		zfree(item);
		item = next;
	}

	ll_init(ll);
}

void ll_reset( llist_t *ll ){
	ll_foreach( ll, i ){
		i->data = NULL;
	}
}
