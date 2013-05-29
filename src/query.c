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

__inline__ __attribute__((always_inline)) short gbQueryParseLong( byte_t *v, size_t vlen, long *l ){
	char *p;

	// make sure v is null terminated, strtol is such a bitch!
	*( v + vlen ) = 0x00;
	*l = strtol( (const char *)v, &p, 10 );

	return ( *p == '\0' );
}

gbItem *gbCreateVolatileItem( void *data, size_t size, gbItemEncoding encoding ) {
	gbItem *item = ( gbItem * )malloc( sizeof( gbItem ) );

	item->data 	   = data;
	item->size 	   = size;
	item->encoding = encoding;
	item->time	   = 0;
	item->ttl	   = -1;
	item->lock	   = 0;

	return item;
}


void gbDestroyVolatileItem( gbItem *item ){
	if( item->encoding != GB_ENC_NUMBER && item->data != NULL ){
		free( item->data );
		item->data = NULL;
	}

	free( item );
	item = NULL;
}

gbItem *gbCreateItem( gbServer *server, void *data, size_t size, gbItemEncoding encoding, int ttl ) {
	gbItem *item = ( gbItem * )malloc( sizeof( gbItem ) );
	unsigned long mem = size + sizeof( gbItem );

	item->data 	   = data;
	item->size 	   = size;
	item->encoding = encoding;
	item->time	   = time(NULL);
	item->ttl	   = ttl;
	item->lock	   = 0;

	if( encoding == GB_ENC_LZF )
		++server->stats.ncompressed;

	if( server->stats.firstin == 0 )
		server->stats.firstin = server->stats.time;

	server->stats.lastin  = server->stats.time;
	server->stats.memused += mem;
	server->stats.sizeavg = server->stats.memused / ++server->stats.nitems;

	if( server->stats.memused > server->stats.mempeak )
		server->stats.mempeak = server->stats.memused;

	return item;
}

void gbDestroyItem( gbServer *server, gbItem *item ){
	unsigned long mem = item->size + sizeof( gbItem );

	server->stats.memused -= mem;
	server->stats.sizeavg = server->stats.nitems == 1 ? 0 : server->stats.memused / --server->stats.nitems;

	if( item->encoding == GB_ENC_LZF ) --server->stats.ncompressed;

	if( item->encoding != GB_ENC_NUMBER && item->data != NULL ){
		free( item->data );
		item->data = NULL;
	}

	free( item );
	item = NULL;
}

int gbIsNodeStillValid( atree_item_t *node, gbItem *item, gbServer *server, int remove ){
	time_t eta = server->stats.time - item->time;

	if( item->ttl > 0 )
	{
		if( eta > item->ttl )
		{
			// item locked, skip
			if( item->lock == -1 || eta < item->lock ) return 1;

			gbLog( DEBUG, "[ACCESS] TTL of %ds expired for item at %p.", item->ttl, item );

			gbDestroyItem( server, item );

			// remove from container
			if( remove )
				node->e_marker = NULL;

			return 0;
		}
	}

	return 1;
}

int gbIsItemStillValid( gbItem *item, gbServer *server, unsigned char *key, size_t klen, int remove ) {
	time_t eta = server->stats.time - item->time;

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

	size_t i = 0, end = min( size, server->limits.maxkeysize );
	while( *p != ' ' && i++ < end ){
		++p;
	}

	*klen = p++ - *key;

	if( value ){
		*value = p;
		*vlen  = size - *klen - 1;
		*vlen  = min( *vlen, server->limits.maxvaluesize );
	}
}

gbItem *gbSingleSet( byte_t *v, size_t vlen, byte_t *k, size_t klen, gbServer *server ){
	gbItemEncoding encoding = GB_ENC_PLAIN;
	void *data = v;
	size_t comprlen = vlen, needcompr = vlen - 4; // compress at least of 4 bytes
	gbItem *item, *old;

	// should we compress ?
	if( vlen > server->compression ){
		comprlen = lzf_compress( v, vlen, server->lzf_buffer, needcompr );
		// not enough compression
		if( comprlen == 0 ){
			encoding = GB_ENC_PLAIN;
			data	 = gbMemDup( v, vlen );
		}
		// succesfully compressed
		else {
			encoding = GB_ENC_LZF;
			vlen 	 = comprlen;
			data 	 = gbMemDup( server->lzf_buffer, comprlen );
		}
	}
	else {
		encoding = GB_ENC_PLAIN;
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

	if( server->stats.memused <= server->limits.maxmem ) {
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

	if( server->stats.memused <= server->limits.maxmem ) {
		gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen );

		size_t found = at_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
		if( found ){
			ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
				gbSingleSet( v, vlen, ki->data, strlen(ki->data), server );

				// free allocated key
				free( ki->data );
				ki->data = NULL;
			}

			ll_reset( server->m_keys );
			ll_reset( server->m_values );

			return gbClientEnqueueData( client, REPL_VAL, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
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
	long ttl;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen );

	item = at_find( &server->tree, k, klen );
	if( item )
	{
		if( gbQueryParseLong( v, vlen, &ttl ) )
		{
			item->time = time(NULL);
			item->ttl  = min( server->limits.maxitemttl, ttl );

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
	long ttl;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen );

	if( gbQueryParseLong( v, vlen, &ttl ) )
	{
		size_t found = at_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
		if( found ){
			ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
				item = vi->data;
				item->time = time(NULL);
				item->ttl  = min( server->limits.maxitemttl, ttl );

				// free allocated key
				free( ki->data );
				ki->data = NULL;
			}

			ll_reset( server->m_keys );
			ll_reset( server->m_values );

			return gbClientEnqueueData( client, REPL_VAL, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
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

	size_t found = at_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
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

		time_t eta = server->stats.time - item->time;

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
	size_t found = at_search_nodes( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
	if( found ){
		ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
			atree_item_t *node = vi->data;

			if( node && node->e_marker )
			{
				item = node->e_marker;

				time_t eta = server->stats.time - item->time;

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

		return gbClientEnqueueData( client, REPL_VAL, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
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
		item = gbCreateItem( server, (void *)1, sizeof( long ), GB_ENC_NUMBER, -1 );
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
	else if( item->encoding == GB_ENC_NUMBER ){
		item->data = (void *)( (long)item->data + delta );

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else if( item->encoding == GB_ENC_PLAIN && gbQueryParseLong( item->data, item->size, &num ) ){
		num += delta;

		server->stats.memused -= ( item->size - sizeof(long) );

		if( item->data != NULL ){
			free( item->data );
			item->data = NULL;
		}

		item->encoding = GB_ENC_NUMBER;
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

	size_t found = at_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
	if( found ){
		ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
			item = vi->data;

			if( gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) == 0 ){
				--found;
			}
			else if( item->encoding == GB_ENC_NUMBER ){
				item->data = (void *)( (long)item->data + delta );
			}
			else if( item->encoding == GB_ENC_PLAIN ){

				if( gbQueryParseLong( item->data, item->size, &num ) )
				{
					num += delta;

					server->stats.memused -= ( item->size - sizeof(long) );

					if( item->data != NULL ){
						free( item->data );
						item->data = NULL;
					}

					item->encoding = GB_ENC_NUMBER;
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
			return gbClientEnqueueData( client, REPL_VAL, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );

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
	long locktime;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen );

	node = at_find_node( &server->tree, k, klen );
	if( node && ( item = node->e_marker ) && gbIsNodeStillValid( node, item, server, 1 ) )
	{
		if( gbQueryParseLong( v, vlen, &locktime ) )
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
	long locktime;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen );

	if( gbQueryParseLong( v, vlen, &locktime ) )
	{
		size_t found = at_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
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

			return gbClientEnqueueData( client, REPL_VAL, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
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

	size_t found = at_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
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
			return gbClientEnqueueData( client, REPL_VAL, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
	}

	return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryCountHandler( gbClient *client, byte_t *p ){
	byte_t *expr = NULL;
	size_t exprlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL );

	size_t found = at_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
	if( found ){
		ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
			item = vi->data;
			if( !gbIsItemStillValid( item, server, expr, exprlen, 1 ) )
				--found;

			// free allocated key
			free( ki->data );
			ki->data = NULL;
		}

		ll_reset( server->m_keys );
		ll_reset( server->m_values );
	}

	return gbClientEnqueueData( client, REPL_VAL, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
}

int gbQueryStatsHandler( gbClient *client, byte_t *p ){
	gbServer *server = client->server;
	size_t elems = 0;

#define APPEND_STATS( key, value, size, encoding ) ++elems; \
												   ll_append( server->m_keys, key ); \
												   ll_append( server->m_values, gbCreateVolatileItem( (void *)(long)value, size, encoding ) )

	APPEND_STATS( "server_started",         server->stats.started, sizeof(time_t), GB_ENC_NUMBER );
	APPEND_STATS( "server_time",            server->stats.time, sizeof(time_t), GB_ENC_NUMBER );
	APPEND_STATS( "first_item_seen",        server->stats.firstin, sizeof(time_t), GB_ENC_NUMBER );
	APPEND_STATS( "last_item_seen",         server->stats.lastin, sizeof(time_t), GB_ENC_NUMBER );
	APPEND_STATS( "total_items",            server->stats.nitems, sizeof(unsigned int), GB_ENC_NUMBER );
	APPEND_STATS( "total_compressed_items", server->stats.ncompressed, sizeof(unsigned int), GB_ENC_NUMBER );
	APPEND_STATS( "total_clients",          server->stats.nclients, sizeof(unsigned int), GB_ENC_NUMBER );
	APPEND_STATS( "total_cron_done",        server->stats.crondone, sizeof(unsigned int), GB_ENC_NUMBER );
	APPEND_STATS( "memory_used",            server->stats.memused, sizeof(unsigned long), GB_ENC_NUMBER );
	APPEND_STATS( "memory_peak", 			server->stats.mempeak, sizeof(unsigned long), GB_ENC_NUMBER );
	APPEND_STATS( "item_size_avg",          (unsigned int)server->stats.sizeavg, sizeof(unsigned int), GB_ENC_NUMBER );

#undef APPEND_STATS

	int ret = gbClientEnqueueKeyValueSet( client, elems, gbWriteReplyHandler, 0 );

	ll_foreach_2( server->m_keys, server->m_values, ki, vi ){
		gbDestroyVolatileItem( vi->data );
		vi->data = NULL;
	}

	ll_reset( server->m_keys );
	ll_reset( server->m_values );

	return ret;
}

int gbProcessQuery( gbClient *client ) {

	short  op = *(short *)&client->buffer[0];
	byte_t *p =  client->buffer + sizeof(short);

	if( op == OP_GET ){
		return gbQueryGetHandler( client, p );
	}
	else if( op == OP_SET ){
		return gbQuerySetHandler( client, p );
	}
	else if( op == OP_TTL ){
		return gbQueryTtlHandler( client, p );
	}
	else if( op == OP_MSET ){
		return gbQueryMultiSetHandler( client, p );
	}
	else if( op == OP_MTTL ){
		return gbQueryMultiTtlHandler( client, p );
	}
	else if( op == OP_MGET ){
		return gbQueryMultiGetHandler( client, p );
	}
	else if( op == OP_DEL ){
		return gbQueryDelHandler( client, p );
	}
	else if( op == OP_MDEL ){
		return gbQueryMultiDelHandler( client, p );
	}
	else if( op == OP_INC || op == OP_DEC ){
		return gbQueryIncDecHandler( client, p, op == OP_INC ? +1 : -1 );
	}
	else if( op == OP_MINC || op == OP_MDEC ){
		return gbQueryMultiIncDecHandler( client, p, op == OP_MINC ? +1 : -1 );
	}
	else if( op == OP_LOCK ){
		return gbQueryLockHandler( client, p );
	}
	else if( op == OP_MLOCK ){
		return gbQueryMultiLockHandler( client, p );
	}
	else if( op == OP_UNLOCK ){
		return gbQueryUnlockHandler( client, p );
	}
	else if( op == OP_MUNLOCK ){
		return gbQueryMultiUnlockHandler( client, p );
	}
	else if( op == OP_COUNT ){
		return gbQueryCountHandler( client, p );
	}
	else if( op == OP_STATS ){
		return gbQueryStatsHandler( client, p );
	}
	else if( op == OP_END ){
		return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 1 );
	}
	else
		return GB_ERR;
}
