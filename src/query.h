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
#ifndef __QUERY_H__
#define __QUERY_H__

#include <unistd.h>
#include "net.h"
#include "default.h"

// single
#define OP_SET     1
#define OP_TTL     2
#define OP_GET     3
#define OP_DEL     4
#define OP_INC     5
#define OP_DEC     6
#define OP_LOCK    7
#define OP_UNLOCK  8
// multi
#define OP_MSET    9
#define OP_MTTL    10
#define OP_MGET    11
#define OP_MDEL    12
#define OP_MINC    13
#define OP_MDEC    14
#define OP_MLOCK   15
#define OP_MUNLOCK 16
#define OP_COUNT   17
#define OP_STATS   18

#define OP_END    0xFF


/*
 * Reply
 */
#define REPL_ERR 		   0
#define REPL_ERR_NOT_FOUND 1
#define REPL_ERR_NAN 	   2
#define REPL_ERR_MEM	   3
#define REPL_ERR_LOCKED    4
#define REPL_OK  		   5
#define REPL_VAL 		   6
#define REPL_KVAL		   7

gbItem *gbCreateItem( gbServer *server, void *data, size_t size, gbItemEncoding encoding, int ttl );
void    gbDestroyItem( gbServer *server, gbItem *item );
int     gbProcessQuery( gbClient *client );

#endif
