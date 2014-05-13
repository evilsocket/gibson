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
#include "trie.h"
#include "lzf.h"
#include "configure.h"

#define min(a,b) ( a < b ? a : b )

extern void gbWriteReplyHandler( gbEventLoop *el, int fd, void *privdata, int mask );

__inline__ __attribute__((always_inline)) unsigned int gbQueryParseLong( byte_t *v, size_t vlen, long *l )
{
    assert( v != NULL );
    assert( vlen > 0 );
    assert( l != NULL );
        
    register size_t i;
    register long n = 0;
    register char c = v[0];
    int sign = 1;
    int start = 0;

    if( c == '0' )
    {
        *l = 0;
        return 1;
    }
    else if( c == '-' )
    {
        sign = -1;
        start = 1;
    }

    for( i = start; i < vlen; ++i )
    {
        c = v[i];
        if( c >= '0' && c <= '9' )
        {
            n = ( n * 10 ) + ( c - '0' );
        }
        else
            return 0;
    }

    *l = n * sign;

    return 1;
}

static gbItem *gbCreateVolatileItem( void *data, size_t size, gbItemEncoding encoding )
{
    gbItem *item = ( gbItem * )zmalloc( sizeof( gbItem ) );

    assert( item != NULL );

    item->data 	   = data;
    item->size 	   = size;
    item->encoding = encoding;
    item->time	   = 0;
    item->last_access_time	= 0;
    item->ttl	   = -1;
    item->lock	   = 0;

    return item;
}

static void gbDestroyVolatileItem( gbItem *item )
{
    assert( item != NULL );

    if( item->encoding != GB_ENC_NUMBER && item->data != NULL )
    {
        zfree( item->data );
        item->data = NULL;
    }

    zfree( item );
}

static gbItem *gbCreateItem( gbServer *server, void *data, size_t size, gbItemEncoding encoding, int ttl )
{
    assert( server != NULL );
    assert( size == 0 || data != NULL );

    gbItem *item = ( gbItem * )zmalloc( sizeof( gbItem ) );

    assert( item != NULL );

    item->data 	           = data;
    item->size 	           = size;
    item->encoding         = encoding;
    item->time             = 
    item->last_access_time = server->stats.time;
    item->ttl	           = ttl;
    item->lock	           = 0;

    if( encoding == GB_ENC_LZF )
    {
        ++server->stats.ncompressed;
    }

    if( server->stats.firstin == 0 )
        server->stats.firstin = server->stats.time;

    server->stats.lastin  = server->stats.time;
    server->stats.memused = zmem_used(); 
    server->stats.sizeavg = server->stats.memused / ++server->stats.nitems;

    if( server->stats.memused > server->stats.mempeak )
        server->stats.mempeak = server->stats.memused;

    return item;
}

void gbDestroyItem( gbServer *server, gbItem *item )
{
    assert( server != NULL );
    assert( item != NULL );

    if( item->encoding == GB_ENC_LZF )
    {
        --server->stats.ncompressed;
    }

    if( item->encoding != GB_ENC_NUMBER && item->data != NULL )
    {
        zfree( item->data );
        item->data = NULL;
    }

    zfree( item );

    server->stats.memused = zmem_used();	
    server->stats.nitems -= 1;
    server->stats.sizeavg = server->stats.nitems == 0 ? 0 : server->stats.memused / server->stats.nitems;
}

static int gbItemIsLocked( gbItem *item, gbServer *server, time_t eta )
{
    assert( item != NULL );
    assert( server != NULL );
    
    eta = eta == 0 ? server->stats.time - item->time : eta;
    return ( item->lock == -1 || eta < item->lock );
}

static int gbIsNodeStillValid( tnode_t *node, gbItem *item, gbServer *server, int remove )
{
    assert( node != NULL );
    assert( item != NULL );
    assert( server != NULL );

    register time_t eta = server->stats.time - item->time,
             ttl = item->ttl;

    if( ttl > 0 && eta >= ttl )
    {
        gbLog( DEBUG, "[ACCESS] TTL of %ds expired for item at %p.", ttl, item );

        if( remove )
            node->data = NULL;

        gbDestroyItem( server, item );

        return 0;
    }

    return 1;
}

static int gbIsItemStillValid( gbItem *item, gbServer *server, unsigned char *key, size_t klen, int remove )
{
    assert( item != NULL );
    assert( server != NULL );
    assert( key != NULL );
    assert( klen > 0 );
            
    register time_t eta = server->stats.time - item->time,
             ttl = item->ttl;

    if( ttl > 0 && eta >= ttl )
    {
        gbLog( DEBUG, "[ACCESS] TTL of %ds expired for item at %p.", ttl, item );

        if( remove )
            tr_remove( &server->tree, key, klen );

        gbDestroyItem( server, item );

        return 0;
    }

    return 1;
}

static int gbParseKeyValue( gbServer *server, byte_t *buffer, size_t size, byte_t **key, byte_t **value, size_t *klen, size_t *vlen )
{
    assert( server != NULL );
    assert( buffer != NULL );
    assert( klen != NULL );
    assert( key != NULL );

    register byte_t *p = buffer;
    register size_t i = 0, end;

    // parse the key, the end data is the minimum among total request size and maxkeysize
    *key = p;
    end  = min( size, server->limits.maxkeysize );
    while( *p != ' ' && i++ < end )
    {
        ++p;
    }

    *klen = p++ - *key;

    // if the value should be parsed ...
    if( value )
    {
        assert( vlen != NULL );

        *value = p;
        *vlen  = size - *klen - 1;
        *vlen  = min( *vlen, server->limits.maxvaluesize );
    }

    // check if length conditions are verified
    if( *klen <= 0 )
        return 0;

    else if( value && vlen <= 0 )
        return 0;

    else
        return 1;
}

static int gbParseTtlKeyValue( gbServer *server, byte_t *buffer, size_t size, byte_t **ttl, byte_t **key, byte_t **value, size_t *ttllen, size_t *klen, size_t *vlen )
{
    assert( server != NULL );
    assert( buffer != NULL );
    assert( size > 0 );
    assert( klen != NULL );
    assert( key != NULL );
    
    register byte_t *p = buffer;
    register size_t i = 0, end;

    // parse the ttl value
    *ttl = p;
    end = min( size, server->limits.maxkeysize );
    while( *p != ' ' && i++ < end )
    {
        ++p;
    }

    *ttllen = p++ - *ttl;

    // parse the key
    *key = p;
    end = min( size, server->limits.maxkeysize );
    while( *p != ' ' && i++ < end )
    {
        ++p;
    }

    *klen = p++ - *key;

    // finally parse the value if needed
    if( value )
    {
        assert( vlen != NULL );
        
        *value = p;
        *vlen  = size - *ttllen - *klen - 2;
        *vlen  = min( *vlen, server->limits.maxvaluesize );
    }

    // check length conditions
    if( *ttllen <= 0 )
        return 0;

    else if( *klen <= 0 )
        return 0;

    else if( value && vlen <= 0 )
        return 0;

    else
        return 1;
}

static gbItem *gbSingleSet( byte_t *v, size_t vlen, byte_t *k, size_t klen, gbServer *server )
{
    assert( v != NULL );
    assert( vlen > 0 );
    assert( k != NULL );
    assert( klen > 0 );
    assert( server != NULL );

    gbItemEncoding encoding = GB_ENC_PLAIN;
    void *data = v;
    size_t comprlen = vlen, needcompr = vlen - 4; // compress at least of 4 bytes
    gbItem *item, *old;

    // should we compress ?
    if( vlen > server->compression )
    {
        comprlen = lzf_compress( v, vlen, server->lzf_buffer, needcompr );
        // not enough compression
        if( comprlen == 0 )
        {
            encoding = GB_ENC_PLAIN;
            data	 = zmemdup( v, vlen );
        }
        // succesfully compressed
        else {
            double rate = 100.0 - ( ( comprlen * 100.0 ) / vlen );

            if( server->stats.compravg == 0 )
                server->stats.compravg = rate;
            else
                server->stats.compravg = ( server->stats.compravg + rate ) / 2.0;

            encoding = GB_ENC_LZF;
            vlen 	 = comprlen;
            data 	 = zmemdup( server->lzf_buffer, comprlen );
        }
    }
    else {
        encoding = GB_ENC_PLAIN;
        data = zmemdup( v, vlen );
    }

    item = gbCreateItem( server, data, vlen, encoding, -1 );
    old = tr_insert( &server->tree, k, klen, item );
    if( old )
    {
        gbDestroyItem( server, old );
    }

    return item;
}

static int gbQuerySetHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *t = NULL,
           *k = NULL,
           *v = NULL;
    size_t ttllen = 0, klen = 0, vlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;
    long ttl;

    if( server->stats.memused <= server->limits.maxmem )
    {
        if( gbParseTtlKeyValue( server, p, client->buffer_size - sizeof(short), &t, &k, &v, &ttllen, &klen, &vlen ) )
        {
            if( gbQueryParseLong( t, ttllen, &ttl ) )
            {
                item = tr_find( &server->tree, k, klen );
                // locked item
                if( item && gbItemIsLocked( item, server, 0 ) )
                {
                    return gbClientEnqueueCode( client, REPL_ERR_LOCKED, gbWriteReplyHandler, 0 );
                }

                item = gbSingleSet( v, vlen, k, klen, server );
                if( ttl > 0 )
                {
                    item->time = server->stats.time;
                    item->ttl  = min( server->limits.maxitemttl, ttl );
                }
    
                return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
            }
            else
                return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR_MEM, gbWriteReplyHandler, 0 );
}

static int gbQueryMultiSetHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL,
           *v = NULL;
    size_t exprlen = 0, vlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;

    if( server->stats.memused <= server->limits.maxmem )
    {
        if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen ) )
        {
            size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
            if( found )
            {
                ll_foreach_2( server->m_keys, server->m_values, ki, vi )
                {
                    item = vi->data;

                    if( gbItemIsLocked( item, server, 0 ) )
                        --found;

                    else if( gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) == 0 )
                        --found;

                    else
                        gbSingleSet( v, vlen, ki->data, strlen(ki->data), server );

                    // free allocated key
                    zfree( ki->data );
                    ki->data = NULL;
                }

                ll_reset( server->m_keys );
                ll_reset( server->m_values );

                if( found )
                    return gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
                else
                    return gbClientEnqueueCode( client, REPL_ERR_LOCKED, gbWriteReplyHandler, 0 );
            }
            else
                return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR_MEM, gbWriteReplyHandler, 0 );
}

static int gbQueryTtlHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *k = NULL,
           *v = NULL;
    size_t klen = 0, vlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;
    long ttl;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen ) )
    {
        item = tr_find( &server->tree, k, klen );
        if( item && gbIsItemStillValid( item, server, k, klen, 1 ) )
        {
            if( gbQueryParseLong( v, vlen, &ttl ) )
            {
                item->last_access_time = 
                    item->time = server->stats.time;
                item->ttl  = min( server->limits.maxitemttl, ttl );

                return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
            }
            else
                return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryMultiTtlHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL,
           *v = NULL;
    size_t exprlen = 0, vlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;
    long ttl;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen ) )
    {
        if( gbQueryParseLong( v, vlen, &ttl ) )
        {
            size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
            if( found )
            {
                ll_foreach_2( server->m_keys, server->m_values, ki, vi )
                {
                    item = vi->data;

                    if( gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) == 0 )
                        --found;

                    else {
                        item->last_access_time = 
                            item->time = server->stats.time;
                        item->ttl  = min( server->limits.maxitemttl, ttl );
                    }

                    // free allocated key
                    zfree( ki->data );
                    ki->data = NULL;
                }

                ll_reset( server->m_keys );
                ll_reset( server->m_values );

                if( found )
                    return gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );

                else
                    return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
            }
            else
                return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryGetHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *k = NULL;
    size_t klen = 0;
    gbServer *server = client->server;
    tnode_t *node = NULL;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL ) )
    {
        node = tr_find_node( &server->tree, k, klen );
        if( node &&                                               // key exists
                ( item = node->data ) &&                            // value exists 
                gbIsNodeStillValid( node, node->data, server, 1 ) ) // item is not expired 
        {
            item = node->data;
            item->last_access_time = server->stats.time;

            return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );

    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryMultiGetHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL;
    size_t exprlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL ) )
    {
        size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
        if( found )
        {
            ll_foreach_2( server->m_keys, server->m_values, ki, vi )
            {
                item = vi->data;

                if( !item || gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) == 0 )
                {
                    --found;
                    vi->data = NULL;
                }
                else
                    item->last_access_time = server->stats.time;
            }

            if( found )
            {

                int ret = gbClientEnqueueKeyValueSet( client, found, gbWriteReplyHandler, 0 );

                ll_foreach_2( server->m_keys, server->m_values, ki, vi )
                {
                    // free allocated key
                    zfree( ki->data );
                    ki->data = NULL;
                }

                ll_reset( server->m_keys );
                ll_reset( server->m_values );

                return ret;
            }
            else {
                ll_foreach_2( server->m_keys, server->m_values, ki, vi )
                {
                    // free allocated key
                    zfree( ki->data );
                    ki->data = NULL;
                }

                ll_reset( server->m_keys );
                ll_reset( server->m_values );

                return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
            }
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryDelHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *k = NULL;
    size_t klen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL ) )
    {
        tnode_t *node = tr_find_node( &server->tree, k, klen );
        if( node && node->data )
        {
            item = node->data;

            if( gbItemIsLocked( item, server, 0 ) )
                return gbClientEnqueueCode( client, REPL_ERR_LOCKED, gbWriteReplyHandler, 0 );

            else if( gbIsNodeStillValid( node, item, server, 1 ) )
            {
                gbDestroyItem( server, item );

                // Remove item from tree
                node->data = NULL;

                return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
            }
        }

        return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryMultiDelHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL;
    size_t exprlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL ) )
    {
        size_t found = tr_search_nodes( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
        if( found )
        {
            ll_foreach_2( server->m_keys, server->m_values, ki, vi )
            {
                tnode_t *node = vi->data;

                if( node && node->data )
                {
                    item = node->data;

                    // locked item
                    if( gbItemIsLocked( item, server, 0 ))
                    {
                        --found;
                    }
                    else if( gbIsNodeStillValid( node, item, server, 1 ) )
                    {
                        node->data = NULL;
                        gbDestroyItem( server, item );
                    }
                    else
                        --found;
                }

                // free allocated key
                zfree( ki->data );
                ki->data = NULL;
            }

            ll_reset( server->m_keys );
            ll_reset( server->m_values );

            return gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryIncDecHandler( gbClient *client, byte_t *p, short delta )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *k = NULL;
    size_t klen = 0;
    gbServer *server = client->server;
    tnode_t *node = NULL;
    gbItem *item = NULL;
    long num = 0;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL ) )
    {
        node = tr_find_node( &server->tree, k, klen );

        item = node ? node->data : NULL;
        if( item == NULL )
        {
            item = gbCreateItem( server, (void *)1, sizeof( long ), GB_ENC_NUMBER, -1 );
            // just reuse the node
            if( node )
                node->data = item;
            else
                tr_insert( &server->tree, k, klen, item );

            return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
        }
        else if( gbIsNodeStillValid( node, item, server, 1 ) == 0 )
        {
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
        }
        else {
            if( gbItemIsLocked( item, server, 0 ) )
                return gbClientEnqueueCode( client, REPL_ERR_LOCKED, gbWriteReplyHandler, 0 );

            item->last_access_time = server->stats.time;

            if( item->encoding == GB_ENC_NUMBER )
            {
                item->data = (void *)( (long)item->data + delta );

                return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
            }
            else if( item->encoding == GB_ENC_PLAIN && gbQueryParseLong( item->data, item->size, &num ) )
            {
                num += delta;

                if( item->data != NULL )
                {
                    zfree( item->data );
                    item->data = NULL;
                }

                server->stats.memused = zmem_used();

                item->encoding = GB_ENC_NUMBER;
                item->data	   = (void *)num;
                item->size	   = sizeof(long);

                return gbClientEnqueueItem( client, REPL_VAL, item, gbWriteReplyHandler, 0 );
            }
            else
                return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
        }
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryMultiIncDecHandler( gbClient *client, byte_t *p, short delta )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL;
    size_t exprlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;
    long num = 0;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL ) )
    {
        size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
        if( found )
        {
            ll_foreach_2( server->m_keys, server->m_values, ki, vi )
            {
                item = vi->data;
                item->last_access_time = server->stats.time;

                if( gbItemIsLocked( item, server, 0 ) )
                {
                    --found;
                }
                if( gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) == 0 )
                {
                    --found;
                }
                else if( item->encoding == GB_ENC_NUMBER )
                {
                    item->data = (void *)( (long)item->data + delta );
                }
                else if( item->encoding == GB_ENC_PLAIN )
                {

                    if( gbQueryParseLong( item->data, item->size, &num ) )
                    {
                        num += delta;

                        if( item->data != NULL )
                        {
                            zfree( item->data );
                            item->data = NULL;
                        }

                        server->stats.memused = zmem_used();

                        item->encoding = GB_ENC_NUMBER;
                        item->data	   = (void *)num;
                        item->size	   = sizeof(long);
                    }
                    else
                        --found;
                }

                // free allocated key
                zfree( ki->data );
                ki->data = NULL;
            }

            ll_reset( server->m_keys );
            ll_reset( server->m_values );

            if( found )
                return gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
            else
                return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryLockHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *k = NULL,
           *v = NULL;
    size_t klen = 0, vlen = 0;
    gbServer *server = client->server;
    tnode_t *node = NULL;
    gbItem *item = NULL;
    long locktime;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &v, &klen, &vlen ) )
    {
        node = tr_find_node( &server->tree, k, klen );
        if( node && ( item = node->data ) && gbIsNodeStillValid( node, item, server, 1 ) )
        {
            if( gbQueryParseLong( v, vlen, &locktime ) )
            {
                item->last_access_time = server->stats.time;

                if( gbItemIsLocked( item, server, 0 ) == 0 )
                {
                    item->time = server->stats.time;
                    item->lock = locktime;

                    return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
                }
                else
                    return gbClientEnqueueCode( client, REPL_ERR_LOCKED, gbWriteReplyHandler, 0 );
            }
            else
                return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryMultiLockHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL,
           *v = NULL;
    size_t exprlen = 0, vlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;
    long locktime;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, &v, &exprlen, &vlen ) )
    {
        if( gbQueryParseLong( v, vlen, &locktime ) )
        {
            size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
            if( found )
            {
                ll_foreach_2( server->m_keys, server->m_values, ki, vi )
                {
                    item = vi->data;

                    if( gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) && gbItemIsLocked( item, server, 0 ) == 0 )
                    {
                        item->last_access_time = 
                            item->time = server->stats.time;
                        item->lock = locktime;
                    }
                    else
                        --found;

                    // free allocated key
                    zfree( ki->data );
                    ki->data = NULL;
                }

                ll_reset( server->m_keys );
                ll_reset( server->m_values );

                if( found )
                    return gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
                else
                    return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
            }
            else
                return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NAN, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryUnlockHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *k = NULL;
    size_t klen = 0;
    gbServer *server = client->server;
    tnode_t *node = NULL;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, NULL, &klen, NULL ) )
    {
        node = tr_find_node( &server->tree, k, klen );
        if( node && ( item = node->data ) && gbIsNodeStillValid( node, item, server, 1 ) )
        {
            item->lock = 0;
            item->last_access_time = server->stats.time;

            return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
        }

        return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryMultiUnlockHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL;
    size_t exprlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL ) )
    {
        size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
        if( found )
        {
            ll_foreach_2( server->m_keys, server->m_values, ki, vi )
            {
                item = vi->data;

                if( gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) )
                {
                    item->lock = 0;
                    item->last_access_time = server->stats.time;
                }

                else
                    --found;

                // free allocated key
                zfree( ki->data );
                ki->data = NULL;
            }

            ll_reset( server->m_keys );
            ll_reset( server->m_values );

            if( found )
                return gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
        }

        return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryCountHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL;
    size_t exprlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL ) )
    {
        size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_keys, &server->m_values );
        if( found )
        {
            ll_foreach_2( server->m_keys, server->m_values, ki, vi )
            {
                item = vi->data;

                if( !item || !gbIsItemStillValid( item, server, ki->data, strlen(ki->data), 1 ) )
                    --found;
                else
                    item->last_access_time = server->stats.time;

                // free allocated key
                zfree( ki->data );
                ki->data = NULL;
            }

            ll_reset( server->m_keys );
            ll_reset( server->m_values );
        }

        return gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&found, sizeof(size_t), gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryStatsHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    gbServer *server = client->server;
    size_t elems = 0;
    char s[0xFF] = {0};

#define APPEND_LONG_STAT( key, value ) ++elems; \
    ll_append( server->m_keys, key ); \
    ll_append( server->m_values, gbCreateVolatileItem( (void *)(long)value, sizeof(long), GB_ENC_NUMBER ) )

#define APPEND_STRING_STAT( key, value ) ++elems; \
    ll_append( server->m_keys, key ); \
    ll_append( server->m_values, gbCreateVolatileItem( zstrdup(value), strlen(value), GB_ENC_PLAIN ) )

#define APPEND_FLOtr_STAT( key, value ) memset( s, 0x00, 0xFF ); \
    sprintf( s, "%f", (value) ); \
    APPEND_STRING_STAT( key, s )

    APPEND_STRING_STAT( "server_version",        VERSION );
    APPEND_STRING_STAT( "server_build_datetime", BUILD_DATETIME );
#if HAVE_JEMALLOC == 1
    APPEND_STRING_STAT( "server_allocator", "jemalloc" );
#else
    APPEND_STRING_STAT( "server_allocator", "malloc" );
#endif
    APPEND_STRING_STAT( "server_arch", (sizeof(long) == 8) ? "64" : "32" );
    APPEND_LONG_STAT( "server_started",         server->stats.started );
    APPEND_LONG_STAT( "server_time",            server->stats.time );
    APPEND_LONG_STAT( "first_item_seen",        server->stats.firstin );
    APPEND_LONG_STAT( "last_item_seen",         server->stats.lastin );
    APPEND_LONG_STAT( "total_items",            server->stats.nitems );
    APPEND_LONG_STAT( "total_compressed_items", server->stats.ncompressed );
    APPEND_LONG_STAT( "total_clients",          server->stats.nclients );
    APPEND_LONG_STAT( "total_cron_done",        server->stats.crondone );
    APPEND_LONG_STAT( "total_connections",      server->stats.connections );
    APPEND_LONG_STAT( "total_requests",         server->stats.requests );
    APPEND_LONG_STAT( "memory_available",       server->stats.memavail );
    APPEND_LONG_STAT( "memory_usable",          server->limits.maxmem );
    APPEND_LONG_STAT( "memory_used",            server->stats.memused );
    APPEND_LONG_STAT( "memory_peak", 			server->stats.mempeak );
    APPEND_FLOtr_STAT( "memory_fragmentation",  zmem_fragmentation_ratio() );
    APPEND_LONG_STAT( "item_size_avg",          server->stats.sizeavg );
    APPEND_LONG_STAT( "compr_rate_avg",         server->stats.compravg );
    APPEND_FLOtr_STAT( "reqs_per_client_avg",   server->stats.requests / (double)server->stats.connections );

#undef APPEND_LONG_STAT
#undef APPEND_STRING_STAT

    int ret = gbClientEnqueueKeyValueSet( client, elems, gbWriteReplyHandler, 0 );

    ll_foreach_2( server->m_keys, server->m_values, ki, vi )
    {
        if( vi->data != NULL )
        {
            gbDestroyVolatileItem( vi->data );
            vi->data = NULL;
        }

        // no need to free ki->data since it's static 
    }

    ll_reset( server->m_keys );
    ll_reset( server->m_values );

    return ret;
}

static int gbGetItemMeta( gbServer *server, gbItem *item, byte_t *m, size_t mlen, long *v )
{
    assert( server != NULL );
    assert( item != NULL );
    assert( m != NULL );
    assert( mlen > 0 );
    assert( v != NULL );

    if( strncmp( (char *)m, "size", min( mlen, 4 ) ) == 0 )
    {
        *v = item->size;
        return 1;
    }
    else if( strncmp( (char *)m, "encoding", min( mlen, 8 ) ) == 0 )
    {
        *v = item->encoding;
        return 1;
    }
    else if( strncmp( (char *)m, "access", min( mlen, 6 ) ) == 0 )
    {
        *v = item->last_access_time;
        return 1;
    }
    else if( strncmp( (char *)m, "created", min( mlen, 7 ) ) == 0 )
    {
        *v = item->time;
        return 1;
    }
    else if( strncmp( (char *)m, "ttl", min( mlen, 3 ) ) == 0 )
    {
        *v = item->ttl;
        return 1;
    }
    else if( strncmp( (char *)m, "left", min( mlen, 4 ) ) == 0 )
    {
        *v = item->ttl <= 0 ? -1 : item->ttl - ( server->stats.time - item->time ); 
        return 1;
    }
    else if( strncmp( (char *)m, "lock", min( mlen, 4 ) ) == 0 )
    {
        *v = item->lock;
        return 1;
    }

    return 0; 
}

static int gbQueryMetaHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *k = NULL, *m = NULL;
    size_t klen = 0, mlen = 0;
    gbServer *server = client->server;
    tnode_t *node = NULL;
    gbItem *item = NULL;
    long v = 0;
    int ret = 0;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &k, &m, &klen, &mlen ) )
    {
        node = tr_find_node( &server->tree, k, klen );
        if(node && node->data && gbIsNodeStillValid( node, node->data, server, 1 ) )
        {
            item = node->data;

            if( gbGetItemMeta( server, item, m, mlen, &v ) == 1 )
            {
                ret = gbClientEnqueueData( client, REPL_VAL, GB_ENC_NUMBER, (byte_t *)&v, sizeof(long), gbWriteReplyHandler, 0 );
            }
            else {
                ret = gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
            }

            item->last_access_time = server->stats.time;

            return ret;
        }

        return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }

    return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

static int gbQueryKeysHandler( gbClient *client, byte_t *p )
{
    assert( client != NULL );
    assert( p != NULL );

    byte_t *expr = NULL;
    size_t exprlen = 0;
    gbServer *server = client->server;
    gbItem *item = NULL;

    if( gbParseKeyValue( server, p, client->buffer_size - sizeof(short), &expr, NULL, &exprlen, NULL ) )
    {
        size_t found = tr_search( &server->tree, expr, exprlen, server->limits.maxkeysize, &server->m_values, NULL );
        long unsigned int i;

        if( found )
        {
            char index[0xFF] = {0};
            ll_item_t *key = NULL;

            ll_reset( server->m_keys );
            for( i = 0, key = server->m_values->head; i < found && key; key = key->next, ++i )
            {
                sprintf( index, "%lu", i );
                ll_append( server->m_keys, zstrdup(index) );
                // convert strings to gbItems
                key->data = gbCreateVolatileItem( key->data, strlen(key->data), GB_ENC_PLAIN );
            }

            int ret = gbClientEnqueueKeyValueSet( client, found, gbWriteReplyHandler, 0 );

            ll_foreach_2( server->m_keys, server->m_values, ki, vi )
            {
                // free volatile item
                gbDestroyVolatileItem( vi->data );
                // free zstrdup'ed key
                zfree( ki->data );
                ki->data = NULL;
            }

            ll_reset( server->m_keys );
            ll_reset( server->m_values );

            return ret;
        }
        else
            return gbClientEnqueueCode( client, REPL_ERR_NOT_FOUND, gbWriteReplyHandler, 0 );
    }
    else
        return gbClientEnqueueCode( client, REPL_ERR, gbWriteReplyHandler, 0 );
}

int gbProcessQuery( gbClient *client )
{
    assert( client != NULL );
    assert( client->buffer_size >= sizeof(short) );

    short  op = *(short *)&client->buffer[0];
    byte_t *p =  client->buffer + sizeof(short);

    ++client->server->stats.requests;

    if( op == OP_GET )
    {
        return gbQueryGetHandler( client, p );
    }
    else if( op == OP_SET )
    {
        return gbQuerySetHandler( client, p );
    }
    else if( op == OP_TTL )
    {
        return gbQueryTtlHandler( client, p );
    }
    else if( op == OP_MSET )
    {
        return gbQueryMultiSetHandler( client, p );
    }
    else if( op == OP_MTTL )
    {
        return gbQueryMultiTtlHandler( client, p );
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
    else if( op == OP_COUNT )
    {
        return gbQueryCountHandler( client, p );
    }
    else if( op == OP_STATS )
    {
        return gbQueryStatsHandler( client, p );
    }
    else if( op == OP_PING )
    {
        return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 0 );
    }
    else if( op == OP_META )
    {
        return gbQueryMetaHandler( client, p );
    }
    else if( op == OP_KEYS )
    {
        return gbQueryKeysHandler( client, p );
    }
    else if( op == OP_END )
    {
        return gbClientEnqueueCode( client, REPL_OK, gbWriteReplyHandler, 1 );
    }
    else
        return GB_ERR;
}
