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
#include "query.h"
#include "log.h"
#include "atree.h"
#include "lzf.h"

#define min(a,b) ( a < b ? a : b )

extern void gbWriteReplyHandler( gbEventLoop *el, int fd, void *privdata, int mask );

__inline__ __attribute__((always_inline)) short gbIsNumeric(const char * s, long *num ){
    char *p;
    *num = strtol (s, &p, 10);
    return ( *p == '\0' );
}

gbItem *gbCreateItem( gbServer *server, void *data, size_t size, gbItemEncoding encoding, int ttl ) {
	gbItem *item = ( gbItem * )malloc( sizeof( gbItem ) );

	item->data 	   = data;
	item->size 	   = size;
	item->encoding = encoding;
	item->time	   = time(NULL);
	item->ttl	   = ttl;
	item->lock	   = 0;

	++server->nitems;

	server->memused += size + sizeof( gbItem );

	if( encoding == COMPRESSED ) ++server->ncompressed;

	if( server->firstin == 0 ) server->firstin = server->time;

	server->lastin = server->time;

	return item;
}

void gbDestroyItem( gbServer *server, gbItem *item ){
	--server->nitems;
	server->memused -= item->size + sizeof( gbItem );

	if( item->encoding == COMPRESSED ) --server->ncompressed;

	if( item->encoding != NUMBER && item->data != NULL ){
		free( item->data );
		item->data = NULL;
	}

	free( item );
	item = NULL;
}

int gbIsNodeStillValid( atree_item_t *node, gbItem *item, gbServer *server, int remove ){
	time_t eta = server->time - item->time;

	if( item->ttl > 0 )
	{
		if( eta > item->ttl )
		{
			// item locked, skip
			if( item->lock == -1 || eta < item->lock ) return 1;

			gbLog( DEBUG, "TTL of %ds expired for item at %p.", item->ttl, item );

			gbDestroyItem( server, item );

			// remove from container
			if( remove )
				node->e_marker = NULL;

			return 0;
		}
	}

	return 1;
}

int gbIsItemStillValid( gbItem *item, gbServer *server, char *key, size_t klen, int remove ) {
	time_t eta = server->time - item->time;

	if( item->ttl > 0 )
	{
		if( eta > item->ttl )
		{
			// item locked, skip
			if( item->lock == -1 || eta < item->lock ) return 1;

			gbLog( DEBUG, "TTL of %ds expired for item at %p.", item->ttl, item );

			if( remove )
				at_remove( &server->tree, key, klen );

			gbDestroyItem( server, item );

			return 0;
		}
	}

	return 1;
}

void gbParseKeyValue( gbServer *server, byte_t *buffer, size_t size, byte_t **key, byte_t **value, size_t *klen, size_t *vlen ){
	byte_t *p = buffer;

	*key = p;

	size_t i = 0, end = min( size, server->maxkeysize );
	while( *p != ' ' && i++ < end ){
		++p;
	}

	*klen = p++ - *key;

	if( value ){
		*value = p;
		*vlen   = min( size - *klen - 1, server->maxvaluesize );
	}
}

gbItem *gbSingleSet( byte_t *v, size_t vlen, byte_t *k, size_t klen, gbServer *server ){
	gbItemEncoding encoding = PLAIN;
	void *data = v;
	size_t comprlen = vlen, needcompr = vlen - 4; // compress at least of 4 bytes
	gbItem *item, *old;

	// should we compress ?
	if( vlen > server->compression ){
		comprlen = lzf_compress( v, vlen, server->lzf_buffer, needcompr );
		// not enough compression
		if( comprlen == 0 ){
			encoding = PLAIN;
			data	 = gbMemDup( v, vlen );
		}
		// succesfully compressed
		else {
			encoding = COMPRESSED;
			vlen 	 = comprlen;
			data 	 = gbMemDup( server->lzf_buffer, comprlen );
		}
	}
	else {
		encoding = PLAIN;
		data = gbMemDup( v, vlen );
	}

	item = gbCreateItem( server, data, vlen, encoding, -1 );
	old = at_insert( &server->tree, k, klen, item );
	if( old ){
		gbDestroyItem( server, old );
	}

	return item;
}

int gbQuerySetHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL,
		   *v = NULL;
	size_t klen = 0, vlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	if( server->memused <= server->maxmem ) {
		gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen );

		item = gbSingleSet( v, vlen, k, klen, server );

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_MEM, gbWriteReplyHandler, 0 );
}

int gbQueryMultiSetHandler( gbClient *client, byte_t *p ){
	byte_t *expr = NULL,
		   *v = NULL;
	size_t exprlen = 0, vlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	if( server->memused <= server->maxmem ) {
		gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen );

		size_t found = at_search( &server->tree, expr, exprlen, server->maxkeysize, &server->m_keys, &server->m_values );
		if( found ){
			ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
				gbSingleSet( v, vlen, ki->data, strlen(ki->data), server );

				// free allocated key
				free( ki->data );
				ki->data = NULL;
			}

			ll_reset( server->m_keys );
			ll_reset( server->m_values );

			return gbClientEnqueueData( client, REPL_VAL, &found, sizeof(size_t), gbWriteReplyHandler, 0 );
		}
		else
			return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_MEM, gbWriteReplyHandler, 0 );
}

int gbQueryTtlHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL,
		   *v = NULL;
	size_t klen = 0, vlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen );

	item = at_find( &server->tree, k, klen );
	if( item )
	{
		long ttl;

		*( v + vlen ) = 0x00;

		if( gbIsNumeric( (const char *)v, &ttl ) )
		{
			item->time = time(NULL);
			item->ttl  = min( server->maxitemttl, ttl );

			return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
		}
		else
			return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryMultiTtlHandler( gbClient *client, byte_t *p ){
	byte_t *expr = NULL,
		   *v = NULL;
	size_t exprlen = 0, vlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen );

	long ttl;
	*( v + vlen ) = 0x00;

	if( gbIsNumeric( (const char *)v, &ttl ) )
	{
		size_t found = at_search( &server->tree, expr, exprlen, server->maxkeysize, &server->m_keys, &server->m_values );
		if( found ){
			ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
				item = vi->data;
				item->time = time(NULL);
				item->ttl  = min( server->maxitemttl, ttl );

				// free allocated key
				free( ki->data );
				ki->data = NULL;
			}

			ll_reset( server->m_keys );
			ll_reset( server->m_values );

			return gbClientEnqueueData( client, REPL_VAL, &found, sizeof(size_t), gbWriteReplyHandler, 0 );
		}
		else
			return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
}

int gbQueryGetHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	atree_item_t *node = NULL;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	node = at_find_node( &server->tree, k, klen );

	if( node && ( item = node->e_marker ) && gbIsNodeStillValid( node, node->e_marker, server, 1 ) )
		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );

	else
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryMultiGetHandler( gbClient *client, byte_t *p ){
	byte_t *expr = NULL;
	size_t exprlen = 0;
	gbServer *server = client->server;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL );

	size_t found = at_search( &server->tree, expr, exprlen, server->maxkeysize, &server->m_keys, &server->m_values );
	if( found ){
		int ret = gbClientEnqueueKeyValueSet( client, found, gbWriteReplyHandler, 0 );

		ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
			// free allocated key
			free( ki->data );
			ki->data = NULL;
		}

		ll_reset( server->m_keys );
		ll_reset( server->m_values );

		return ret;
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryDelHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	atree_item_t *node = at_find_node( &server->tree, k, klen );
	if( node && node->e_marker )
	{
		item = node->e_marker;

		time_t eta = server->time - item->time;

		if( item->lock == -1 || eta < item->lock )
			return gbClientEnqueueCode( client, REPL_ERR_LOCKED, gbWriteReplyHandler, 0 );

		else
		{
			int valid = gbIsNodeStillValid( node, item, server, 0 );

			gbDestroyItem( server, item );

			// Remove item from tree
			node->e_marker = NULL;

			if( valid )
				return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
		}
	}

	return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryMultiDelHandler( gbClient *client, byte_t *p ){
	byte_t *expr = NULL;
	size_t exprlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL );
	size_t found = at_search_nodes( &server->tree, expr, exprlen, server->maxkeysize, &server->m_keys, &server->m_values );
	if( found ){
		ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
			atree_item_t *node = vi->data;

			if( node && node->e_marker )
			{
				item = node->e_marker;

				time_t eta = server->time - item->time;

				if( item->lock == -1 || eta < item->lock )
				{
					--found;
				}
				else
				{
					// Remove item from tree
					node->e_marker = NULL;

					int valid = gbIsNodeStillValid( node, item, server, 0 );

					gbDestroyItem( server, item );

					if( !valid )
						--found;
				}
			}

			// free allocated key
			free( ki->data );
			ki->data = NULL;
		}

		ll_reset( server->m_keys );
		ll_reset( server->m_values );

		return gbClientEnqueueData( client, REPL_VAL, &found, sizeof(size_t), gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryIncDecHandler( gbClient *client, byte_t *p, short delta ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	atree_item_t *node = NULL;
	gbItem *item = NULL;
	long num = 0;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	node = at_find_node( &server->tree, k, klen );

	item = node ? node->e_marker : NULL;
	if( item == NULL ) {
		item = gbCreateItem( server, (void *)1, sizeof( long ), NUMBER, -1 );
		// just reuse the node
		if( node )
			node->e_marker = item;
		else
			at_insert( &server->tree, k, klen, item );

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else if( gbIsNodeStillValid( node, item, server, 1 ) == 0 ){
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
	}
	else if( item->encoding == NUMBER ){
		item->data = (void *)( (long)item->data + delta );

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else if( item->encoding == PLAIN && gbIsNumeric( item->data, &num ) ){
		num += delta;

		server->memused -= ( item->size - sizeof(long) );

		if( item->data != NULL ){
			free( item->data );
			item->data = NULL;
		}

		item->encoding = NUMBER;
		item->data	   = (void *)num;
		item->size	   = sizeof(long);

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
}

int gbQueryMultiIncDecHandler( gbClient *client, byte_t *p, short delta ){
	byte_t *expr = NULL;
	size_t exprlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;
	long num = 0;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL );

	size_t found = at_search( &server->tree, expr, exprlen, server->maxkeysize, &server->m_keys, &server->m_values );
	if( found ){
		ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
			item = vi->data;

			if( gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) == 0 ){
				--found;
			}
			else if( item->encoding == NUMBER ){
				item->data = (void *)( (long)item->data + delta );
			}
			else if( item->encoding == PLAIN ){

				((char *)item->data)[ item->size - 1 ] = '\0';

				if( gbIsNumeric( item->data, &num ) )
				{
					num += delta;

					server->memused -= ( item->size - sizeof(long) );

					if( item->data != NULL ){
						free( item->data );
						item->data = NULL;
					}

					item->encoding = NUMBER;
					item->data	   = (void *)num;
					item->size	   = sizeof(long);
				}
				else
					--found;
			}

			// free allocated key
			free( ki->data );
			ki->data = NULL;
		}

		ll_reset( server->m_keys );
		ll_reset( server->m_values );

		if( found )
			return gbClientEnqueueData( client, REPL_VAL, &found, sizeof(size_t), gbWriteReplyHandler, 0 );

		else
			return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryLockHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL,
		   *v = NULL;
	size_t klen = 0, vlen = 0;
	gbServer *server = client->server;
	atree_item_t *node = NULL;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen );

	node = at_find_node( &server->tree, k, klen );
	if( node && ( item = node->e_marker ) && gbIsNodeStillValid( node, item, server, 1 ) )
	{
		long locktime;

		*( v + vlen ) = 0x00;

		if( gbIsNumeric( (const char *)v, &locktime ) )
		{
			item->time = time(NULL);
			item->lock = locktime;

			return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
		}
		else
			return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryMultiLockHandler( gbClient *client, byte_t *p ){
	byte_t *expr = NULL,
		   *v = NULL;
	size_t exprlen = 0, vlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen );

	long locktime;
	*( v + vlen ) = 0x00;

	if( gbIsNumeric( (const char *)v, &locktime ) )
	{
		size_t found = at_search( &server->tree, expr, exprlen, server->maxkeysize, &server->m_keys, &server->m_values );
		if( found ){
			ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
				item = vi->data;
				item->time = time(NULL);
				item->lock = locktime;

				// free allocated key
				free( ki->data );
				ki->data = NULL;
			}

			ll_reset( server->m_keys );
			ll_reset( server->m_values );

			return gbClientEnqueueData( client, REPL_VAL, &found, sizeof(size_t), gbWriteReplyHandler, 0 );
		}
		else
			return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
}

int gbQueryUnlockHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	atree_item_t *node = NULL;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	node = at_find_node( &server->tree, k, klen );
	if( node && ( item = node->e_marker ) && gbIsNodeStillValid( node, item, server, 1 ) )
	{
		item->lock = 0;

		return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
	}

	return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryMultiUnlockHandler( gbClient *client, byte_t *p ){
	byte_t *expr = NULL;
	size_t exprlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL );

	size_t found = at_search( &server->tree, expr, exprlen, server->maxkeysize, &server->m_keys, &server->m_values );
	if( found ){
		ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
			item = vi->data;

			if( gbIsItemStillValid( item, server, expr, exprlen, 1 ) )
				item->lock = 0;

			else
				--found;

			// free allocated key
			free( ki->data );
			ki->data = NULL;
		}

		ll_reset( server->m_keys );
		ll_reset( server->m_values );

		if( found )
			return gbClientEnqueueData( client, REPL_VAL, &found, sizeof(size_t), gbWriteReplyHandler, 0 );
	}

	return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbProcessQuery( gbClient *client ) {

	short  op = *(short *)&client->buffer[0];
	byte_t *p =  client->buffer + sizeof(short);

	if( op == OP_SET )
	{
		return gbQuerySetHandler( client, p );
	}
	else if( op == OP_MSET )
	{
		return gbQueryMultiSetHandler( client, p );
	}
	else if( op == OP_TTL )
	{
		return gbQueryTtlHandler( client, p );
	}
	else if( op == OP_MTTL )
	{
		return gbQueryMultiTtlHandler( client, p );
	}
	else if( op == OP_GET )
	{
		return gbQueryGetHandler( client, p );
	}
	else if( op == OP_MGET )
	{
		return gbQueryMultiGetHandler( client, p );
	}
	else if( op == OP_DEL )
	{
		return gbQueryDelHandler( client, p );
	}
	else if( op == OP_MDEL )
	{
		return gbQueryMultiDelHandler( client, p );
	}
	else if( op == OP_INC || op == OP_DEC )
	{
		return gbQueryIncDecHandler( client, p, op == OP_INC ? +1 : -1 );
	}
	else if( op == OP_MINC || op == OP_MDEC )
	{
		return gbQueryMultiIncDecHandler( client, p, op == OP_MINC ? +1 : -1 );
	}
	else if( op == OP_LOCK )
	{
		return gbQueryLockHandler( client, p );
	}
	else if( op == OP_MLOCK )
	{
		return gbQueryMultiLockHandler( client, p );
	}
	else if( op == OP_UNLOCK )
	{
		return gbQueryUnlockHandler( client, p );
	}
	else if( op == OP_MUNLOCK )
	{
		return gbQueryMultiUnlockHandler( client, p );
	}
	else if( op == OP_END )
	{
		return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 1 );
	}

	return GB_ERR;
}
