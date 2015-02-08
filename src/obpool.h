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
#ifndef __OBPOOL_H__
#define __OBPOOL_H__

#include "zmem.h"
#include <stdint.h>

typedef struct _opool_block
{
    // block memory
    void                *memory;
    // number of objects stored in this block
    size_t               capacity;
    // pointer to next block or NULL if this is the last one
    struct _opool_block *next;
}
opool_block_t;

// this structure is used as a "header" inside free 
// objects to make them point to the previous free
// object and have a memory unpexpensive free stack
typedef struct
{
    opool_block_t *next;
}
opool_header_t;

typedef struct
{
    // pointer to the current used memory
    void          *memory;
    // pointer to the first free object
    void          *first_free;
    // total objects used inside the current block
    size_t         used;
    // current block capacity in objects
    size_t         capacity;
    // total number of objects allocated inside blocks
    size_t         total_capacity;
    // size in bytes of a single object in the pool
    size_t         object_size;
    // first allocated block
    opool_block_t  first_block;
    // pointer to the last allocated block
    opool_block_t *last_block;
    // maximum objects that can be stored in a block
    size_t         max_block_size;
}
opool_t;

void  opool_create( opool_t *pool, size_t object_size, size_t initial_capacity, size_t max_block_size );
void *opool_alloc_object( opool_t *pool );
void  opool_free_object( opool_t *pool, void *obj );
void  opool_destroy( opool_t *pool );

#endif
