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

double isNumeric (const char * s, long *num )
{
    char * p;
    *num = strtol (s, &p, 10);
    return *p == '\0';
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

	if( server->firstin == 0 )
		server->firstin = server->time;

	server->lastin = server->time;

	return item;
}

void gbDestroyItem( gbServer *server, gbItem *item ){
	--server->nitems;
	server->memused -= item->size + sizeof( gbItem );

	if( item->encoding == PLAIN && item->data != NULL )
		free( item->data );

	free( item );
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
	while( *p != ' ' && i++ < end )
	{
		++p;
	}

	*klen = p++ - *key;

	if( value ){
		*value = p;
		*vlen   = min( size - *klen - 1, server->maxvaluesize );
	}
}

int gbQuerySetHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL,
		   *v = NULL;
	size_t klen = 0, vlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	if( server->memused <= server->maxmem ) {
		gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen );

		gbItemEncoding encoding = PLAIN;
		void *data = v;
		size_t comprlen = vlen, needcompr = vlen - 4; // compress at least of 4 bytes

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
		gbItem * old = at_insert( &server->tree, k, klen, item );
		if( old )
		{
			gbDestroyItem( server, old );
		}

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
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

		if( isNumeric( (const char *)v, &ttl ) )
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

int gbQueryGetHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	item = at_find( &server->tree, k, klen );

	if( item && gbIsItemStillValid( item, server, k, klen, 1 ) )
		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );

	else
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryDelHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	item = at_remove( &server->tree, k, klen );
	if( item )
	{
		time_t eta = server->time - item->time;

		if( item->lock == -1 || eta < item->lock )
			return gbClientEnqueueCode( client, REPL_ERR_LOCKED, gbWriteReplyHandler, 0 );

		else
		{
			int valid = gbIsItemStillValid( item, server, k, klen, 0 );

			gbDestroyItem( server, item );

			if( valid )
			{
				return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
			}
		}
	}

	return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
}

int gbQueryIncDecHandler( gbClient *client, byte_t *p, short delta ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;
	long num = 0;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	item = at_find( &server->tree, k, klen );
	if( item == NULL ) {
		item = gbCreateItem( server, (void *)1, sizeof( long ), NUMBER, -1 );

		at_insert( &server->tree, k, klen, item );

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else if( gbIsItemStillValid( item, server, k, klen, 1 ) == 0 ){
		return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
	}
	else if( item->encoding == NUMBER ){
		item->data = (void *)( (long)item->data + delta );

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else if( item->encoding == PLAIN && isNumeric( item->data, &num ) ){
		num += delta;

		server->memused -= ( item->size - sizeof(long) );

		if( item->data )
			free( item->data );

		item->encoding = NUMBER;
		item->data	   = (void *)num;
		item->size	   = sizeof(long);

		return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
	}
	else
		return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
}

int gbQueryLockHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL,
		   *v = NULL;
	size_t klen = 0, vlen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen );

	item = at_find( &server->tree, k, klen );
	if( item && gbIsItemStillValid( item, server, k, klen, 1 ) )
	{
		long locktime;

		*( v + vlen ) = 0x00;

		if( isNumeric( (const char *)v, &locktime ) )
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

int gbQueryUnlockHandler( gbClient *client, byte_t *p ){
	byte_t *k = NULL;
	size_t klen = 0;
	gbServer *server = client->server;
	gbItem *item = NULL;

	gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL );

	item = at_remove( &server->tree, k, klen );
	if( item && gbIsItemStillValid( item, server, k, klen, 1 ) )
	{
		item->lock = 0;

		return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
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
	else if( op == OP_TTL )
	{
		return gbQueryTtlHandler( client, p );
	}
	else if( op == OP_GET )
	{
		return gbQueryGetHandler( client, p );
	}
	else if( op == OP_DEL )
	{
		return gbQueryDelHandler( client, p );
	}
	else if( op == OP_INC || op == OP_DEC )
	{
		return gbQueryIncDecHandler( client, p, op == OP_INC ? +1 : -1 );
	}
	else if( op == OP_LOCK )
	{
		return gbQueryLockHandler( client, p );
	}
	else if( op == OP_UNLOCK )
	{
		return gbQueryUnlockHandler( client, p );
	}
	else if( op == OP_END )
	{
		return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 1 );
	}

	return GB_ERR;
}
