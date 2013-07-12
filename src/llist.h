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
#ifndef __LLIST_H__
#define __LLIST_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zmem.h"

/*
 * Linked list generic item container.
 */
typedef struct ll_item {
	struct ll_item *next;
    void    *data;
}
ll_item_t;
/*
 * The linked list container itself.
 */
typedef struct {
	ll_item_t *head;
	ll_item_t *tail;
    ll_item_t *free; // next free element if ll_prealloc was used
}
llist_t;

/*
 * Macro to obtain item data and cast it to its original type.
 */
#define ll_data( t, llitem ) ((t)((llitem)->data))
/*
 * Macro to easily loop the list.
 * NOTE: This macro has to be used only for read only loops,
 * if during a loop the list is modified (i.e. an element is removed),
 * you have to MANUALLY fix the pointers.
 */
#define ll_foreach( ll, item ) ll_item_t *item = NULL; for( item = (ll)->head; item; item = item->next )
/*
 * Same as ll_foreach but on two lists.
 */
#define ll_foreach_2( la, lb, aitem, bitem ) ll_item_t *aitem = NULL, *bitem = NULL; \
		for( aitem = (la)->head, bitem = (lb)->head; aitem && bitem && aitem->data && bitem->data; aitem = aitem->next, bitem = bitem->next )
/*
 * Macro to easily loop the list until a certain item.
 * NOTE: This macro has to be used only for read only loops,
 * if during a loop the list is modified (i.e. an element is removed),
 * you have to MANUALLY fix the pointers.
 */
#define ll_foreach_to( ll, item, i, to ) for( i = 0, item = (ll)->head; i < to; ++i, item = item->next )
/*
 * Allocate and initialize a new linked list.
 */
#define ll_create() (llist_t *)zcalloc( sizeof(llist_t) )

llist_t *ll_prealloc( size_t elements );

/*
 * Initialize a linked list.
 */
#define ll_init( ll ) (ll)->head = NULL; \
					  (ll)->tail = NULL; \
                      (ll)->free = NULL

/*
 * Add an element to the end of the list.
 */
void 	ll_append( llist_t *ll, void *data );
/*
 * Clear and deallocate each item of the list.
 */
void 	ll_clear( llist_t *ll );
/*
 * Clear and deallocate the list itself previously
 * created with the ll_create macro.
 */
#define ll_destroy( ll ) ll_clear( (ll) ); \
						 zfree(ll)

void ll_init_space( llist_t *ll, size_t elements );

void ll_reset( llist_t *ll );

#endif
