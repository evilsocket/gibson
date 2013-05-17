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

llist_t *ll_prealloc( size_t elements ){
	llist_t *list = ll_create();
	size_t i;

	for( i = 0; i < elements; i++ )
		ll_append( list, NULL );

	return list;
}

void ll_append( llist_t *ll, void *data ){
	ll_item_t *item;
	for( item = ll->head; item; item = item->next ){
		if( item->data == NULL )
		{
			item->data = data;
			return;
		}
	}

	ll_append_new( ll, data );
}

void ll_append_new( llist_t *ll, void *data ){
	ll_item_t *item = (ll_item_t *)calloc( 1, sizeof(ll_item_t) );

	item->data = data;

	if( ll->head == NULL ){
		ll->head = item;
	}
	else{
		ll->tail->next = item;
		item->prev 	   = ll->tail;
	}

	ll->tail = item;
}

void ll_prepend( llist_t *ll, void *data ){
	ll_item_t *item = (ll_item_t *)calloc( 1, sizeof(ll_item_t) );

	item->data = data;

	if( ll->head == NULL ){
		ll->tail = item;
	}
	else{
		ll->head->prev = item;
		item->next 	   = ll->head;
	}

	ll->head = item;
}

void ll_move( llist_t *from, llist_t *to, ll_item_t *item ){
	if( item->prev == NULL ){
		from->head = item->next;
	}
	else{
		item->prev->next = item->next;
	}
	if( item->next == NULL ){
		from->tail = item->prev;
	}
	else{
		item->next->prev = item->prev;
	}

	if( to->head == NULL ){
		to->head = item;
		item->prev = NULL;
	}
	else{
		to->tail->next = item;
		item->prev 	   = to->tail;
	}

	to->tail   = item;
	item->next = NULL;
}

void ll_copy( llist_t *from, llist_t *to ){
	ll_item_t *item;
	for( item = from->head; item; item = item->next ){
		ll_append( to, item->data );
	}
}

void ll_merge( llist_t *dest, llist_t *source ){
	ll_item_t *item = source->head,
			  *next;

	while(item){
		next = item->next;

		if( item->prev == NULL ){
			source->head = next;
		}
		else{
			item->prev->next = next;
		}
		if( next == NULL ){
			source->tail = item->prev;
		}
		else{
			next->prev = item->prev;
		}

		if( dest->head == NULL ){
			dest->head = item;
			item->prev = NULL;
		}
		else{
			dest->tail->next = item;
			item->prev 	     = dest->tail;
		}

		dest->tail = item;
		item->next = NULL;

		item = next;
	}
}

void ll_remove( llist_t *ll, ll_item_t *item ){
	if( item->prev == NULL ){
		ll->head = item->next;
	}
	else{
		item->prev->next = item->next;
	}
	if( item->next == NULL ){
		ll->tail = item->prev;
	}
	else{
		item->next->prev = item->prev;
	}

	free(item);
}

void ll_clear( llist_t *ll ){
	ll_item_t *item = ll->head,
			  *next;

	while(item){
		next = item->next;
		free(item);
		item = next;
	}

	ll_init(ll);
}
