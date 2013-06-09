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
#ifndef __GIBSON_H__
#define __GIBSON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>

#define GB_DEFAULT_BUFFER_SIZE 65535

// Gibson protocol constants

// requests
#define OP_SET     1
#define OP_TTL     2
#define OP_GET     3
#define OP_DEL     4
#define OP_INC     5
#define OP_DEC     6
#define OP_LOCK    7
#define OP_UNLOCK  8
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
#define OP_END     0xFF
// replies
#define REPL_ERR 		   0
#define REPL_ERR_NOT_FOUND 1
#define REPL_ERR_NAN 	   2
#define REPL_ERR_MEM	   3
#define REPL_ERR_LOCKED    4
#define REPL_OK  		   5
#define REPL_VAL 		   6
#define REPL_KVAL		   7

typedef unsigned char gbEncoding;

// the item is in plain encoding and data points to its buffer
#define	GB_ENC_PLAIN  0x00
// PLAIN but compressed data with lzf
#define GB_ENC_LZF    0x01
// the item contains a number and data pointer is actually that number
#define GB_ENC_NUMBER 0x02

typedef struct
{
	short code;   		   // used to tag this buffer ( for reply code )
	gbEncoding encoding;   // buffer encoding
	unsigned char *buffer; // buffer
	size_t rsize; 		   // real buffer size
	size_t size;  		   // current buffer size
}
gbBuffer;

#define GB_INIT_BUFFER(b) (b).buffer   = malloc( GB_DEFAULT_BUFFER_SIZE ); \
						  (b).encoding = GB_ENC_PLAIN; \
						  (b).code     = 0; \
						  (b).rsize    = \
						  (b).size     = GB_DEFAULT_BUFFER_SIZE

typedef struct
{
	size_t count;
	char    **keys;
	gbBuffer *values;
}
gbMultiBuffer;

typedef struct
{
	int  fd;
	char address[0xFF];
	int  port;
	int  timeout;
	int  error;
	gbBuffer request;
	gbBuffer reply;
}
gbClient;

int gb_tcp_connect( gbClient *c, char *address, int port, int timeout );
int gb_unix_connect( gbClient *c, char *socket, int timeout );

int gb_set(gbClient *c, char *key, int klen, char *value, int vlen, int ttl );
int gb_mset(gbClient *c, char *expr, int elen, char *value, int vlen );
int gb_ttl(gbClient *c, char *key, int klen, int ttl );
int gb_mttl(gbClient *c, char *expr, int elen, int ttl);
int gb_get(gbClient *c, char *key, int klen);
int gb_mget(gbClient *c, char *expr, int elen);
int gb_del(gbClient *c, char *key, int klen);
int gb_mdel(gbClient *c, char *expr, int elen);
int gb_inc(gbClient *c, char *key, int klen);
int gb_minc(gbClient *c, char *expr, int elen);
int gb_mdec(gbClient *c, char *expr, int elen);
int gb_dec(gbClient *c, char *key, int klen);
int gb_lock(gbClient *c, char *key, int klen, int time);
int gb_mlock(gbClient *c, char *expr, int elen, int time);
int gb_unlock(gbClient *c, char *key, int klen);
int gb_munlock(gbClient *c, char *expr, int elen);
int gb_count(gbClient *c, char *expr, int elen);
int gb_stats(gbClient *c);
int gb_quit(gbClient *c);

const unsigned char *gb_reply_raw(gbClient *c);
long gb_reply_number(gbClient *c);
void gb_reply_multi(gbClient *c, gbMultiBuffer *b);
void gb_reply_multi_free(gbMultiBuffer *b);

void gb_disconnect( gbClient *c );

#ifdef __cplusplus
}
#endif

#endif
