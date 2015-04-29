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
#include "obpool.h"
#include <assert.h>

static void opool_alloc_block( opool_block_t *block, size_t object_size, size_t capacity )
{
    assert( block != NULL );
    assert( capacity > 0 );
    assert( object_size >= sizeof(uintptr_t) );

    block->capacity = capacity;
    block->next     = NULL;
    block->memory   = zcalloc( object_size * capacity );

    assert( block->memory != NULL );
}

static void opool_free_block( opool_block_t *block )
{
    assert( block != NULL );
    assert( block->memory != NULL );
    assert( block->capacity > 0 );

    zfree( block->memory );

    block->capacity = 0;
    block->next     = NULL;
    block->memory   = NULL;
}

void opool_create( opool_t *pool, size_t object_size, size_t initial_capacity, size_t max_block_size )
{
    assert( pool != NULL );
    assert( object_size >= sizeof(uintptr_t) );
    assert( initial_capacity > 0 );
    assert( max_block_size > ( object_size * initial_capacity ) );

    pool->first_free     = NULL;
    pool->used           = 0;
    pool->object_size    = object_size;
    pool->capacity       = initial_capacity;
    pool->total_capacity = initial_capacity;
    pool->max_block_size = max_block_size;

    opool_alloc_block( &pool->first_block, object_size, initial_capacity );

    pool->memory     = pool->first_block.memory;
    pool->last_block = &pool->first_block;
}

void *opool_alloc_object( opool_t *pool )
{
    assert( pool != NULL );
    assert( pool->memory != NULL );
    assert( pool->capacity > 0 );

    uint8_t *obj = NULL;
    size_t new_capacity = 0;
    opool_block_t *new_block = NULL;

    if( pool->first_free )
    {
        // get first free object
        obj = (uint8_t *)pool->first_free;
        // update free list with the next free object
        pool->first_free = ((opool_header_t *)obj)->next;
    }
    else
    {
        // grow pool if needed
        if( pool->used >= pool->capacity )
        {
            new_capacity = pool->used << 1;

            if( new_capacity > pool->max_block_size )
                new_capacity = pool->max_block_size;

            new_block = (opool_block_t *)zmalloc( sizeof(opool_block_t) );

            assert( new_block != NULL );
            assert( new_capacity > pool->used );

            opool_alloc_block( new_block, pool->object_size, new_capacity );

            pool->last_block->next = new_block;
            pool->last_block       = new_block;
            pool->memory           = new_block->memory;
            pool->used             = 0;
            pool->capacity         = new_capacity;
            pool->total_capacity  += new_capacity;
        }

        obj = (uint8_t *)pool->memory;

        // if no new block was created, increment pointer
        // accordingly to currently used objects
        obj += pool->used * pool->object_size;

        ++pool->used;
    }

    return obj;
}

void opool_free_object( opool_t *pool, void *obj )
{
    assert( pool != NULL );
    assert( obj != NULL );

    // since this object is "freed", write on top of it
    // the address of the current first free object
    ((opool_header_t *)obj)->next = pool->first_free;
    // then make the first free object point to this
    // something like when you do on linked lists:
    //
    //  curr->next = first;
    //  first = curr;
    //
    pool->first_free = obj;
}

void opool_destroy( opool_t *pool )
{
    assert( pool != NULL );

    opool_block_t *next, *block = &pool->first_block;

    while(block)
    {
        next = block->next;

        opool_free_block( block );

        if( block != &pool->first_block )
            zfree( block );

        block = next;
    }

    pool->first_free     = NULL;
    pool->used           = 0;
    pool->capacity       = 0;
    pool->memory         = NULL;
    pool->last_block     = NULL;
    pool->total_capacity = 0;
}
