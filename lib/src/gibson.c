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
#include "gibson.h"

int gb_fd_select(int fd, int timeout, int readable ) {
	struct timeval tv;
	fd_set fds;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (readable == 1)
		return select(fd + 1, &fds, NULL, NULL, &tv);

	return select(fd + 1, NULL, &fds, NULL, &tv);
}

int gb_create_socket(int domain) {
    int s, on = 1;

    if ((s = socket(domain, SOCK_STREAM, 0)) == -1) {
        return -1;
    }
    else if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        return -1;
    }
    else
    	return s;
}

int gb_tcp_connect(gbClient *c, char *address, int port, int timeout) {
	int flags, yes = 1;
	struct sockaddr_in sa;

	c->fd =
	c->port =
	c->error = 0;
	c->timeout = timeout;
	c->address[0] = 0x00;

	if( address == NULL ) address = "127.0.0.1";

	if( port == 0 ) port = 10128;

	if( ( c->fd = gb_create_socket( AF_INET ) ) == -1 ||
		( c->error = setsockopt( c->fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &yes, sizeof(yes) ) ) == -1 ||
		( c->error = setsockopt( c->fd, IPPROTO_TCP, TCP_NODELAY, (void *) &yes, sizeof(yes) ) ) == -1 )
	{
		return c->error;
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);

	struct addrinfo hints, *info;

	if( inet_aton( address, &sa.sin_addr ) == 0 ){
		memset(&hints, 0, sizeof(hints));

		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		getaddrinfo( address, NULL, &hints, &info );
		memcpy(&sa.sin_addr.s_addr, &(info->ai_addr->sa_data[2]), sizeof(in_addr_t) );
		freeaddrinfo(info);
	}

	flags = fcntl( c->fd, F_GETFL );
	if( ( c->error = fcntl( c->fd, F_SETFL, flags | O_NONBLOCK ) ) < 0 ) {
		return c->error;
	}

	if( ( c->error = connect( c->fd, (struct sockaddr *)&sa, sizeof(sa) ) ) != 0 ){
		if (errno != EINPROGRESS)
			return c->error;

		else if( gb_fd_select( c->fd, c->timeout, 0 ) > 0) {
			int err;
			unsigned int len = sizeof(err);
			if( ( c->error = getsockopt( c->fd, SOL_SOCKET, SO_ERROR, &err, &len) ) == -1 || err ){
				return ( c->error = err );
			}
		}
		else
			return c->error;
	}

	strncpy( c->address, inet_ntoa(sa.sin_addr), 0xFF );
	c->port = port;

	GB_INIT_BUFFER( c->reply );
	GB_INIT_BUFFER( c->request );

	return 0;
}

int gb_unix_connect( gbClient *c, char *socket, int timeout ){
	int flags;
	struct sockaddr_un sa;

	c->fd =
	c->port =
	c->error = 0;
	c->timeout = timeout;
	c->address[0] = 0x00;

	if( ( c->fd = gb_create_socket( AF_LOCAL ) ) == -1 ){
		return c->error;
	}

	sa.sun_family = AF_LOCAL;
	strncpy( sa.sun_path, socket, sizeof(sa.sun_path) - 1 );

	flags = fcntl( c->fd, F_GETFL );
	if( ( c->error = fcntl( c->fd, F_SETFL, flags | O_NONBLOCK ) ) < 0 ) {
		return c->error;
	}

	if( ( c->error = connect( c->fd, (struct sockaddr *)&sa, sizeof(sa) ) ) != 0 ){
		if (errno != EINPROGRESS)
			return c->error;

		else if( gb_fd_select( c->fd, c->timeout, 0 ) > 0) {
			int err;
			unsigned int len = sizeof(err);
			if( ( c->error = getsockopt( c->fd, SOL_SOCKET, SO_ERROR, &err, &len) ) == -1 || err ){
				return ( c->error = err );
			}
		}
		else
			return c->error;
	}

	strncpy( c->address, socket, 0xFF );
	c->port = -1;

	GB_INIT_BUFFER( c->reply );
	GB_INIT_BUFFER( c->request );

	return 0;
}

int gb_recv( gbClient *c, unsigned int msecs, void *buf, int size ) {
	c->error = gb_fd_select( c->fd, msecs, 1 );

	if (c->error > 0){
		c->error = 0;
		return recv( c->fd, buf, size, 0 );
	}
	else if ( c->error == 0 )
		return ( c->error = -2 );
	else
		return ( c->error = -1 );
}


int gb_send(gbClient *c, unsigned int msecs, void *buf, int size) {
	fd_set fds;
	struct timeval tv;
	int sent = 0;

	/* NOTE: On Linux, select() modifies timeout to reflect the amount
	 * of time not slept, on other systems it is likely not the same */
	tv.tv_sec = msecs / 1000;
	tv.tv_usec = (msecs % 1000) * 1000;

	while (sent < size) {
		FD_ZERO(&fds);
		FD_SET(c->fd, &fds);

		c->error = select(c->fd + 1, NULL, &fds, NULL, &tv);

		if (c->error > 0) {
			c->error = send(c->fd, (char *)buf + sent, size - sent, 0);
			if (c->error < 0)
				return c->error;

			sent += c->error;
		// timeout || error
		}
		else if ( c->error <= 0 )
			return c->error;
	}

	c->error = 0;

	return sent;
}

int gb_send_command( gbClient *c, short cmd, void *data, int len ){
	int csize = sizeof(short) + len;
	size_t rsize = 0;

	if( c->fd )
	{
		if( ( c->error = gb_send( c, c->timeout, &csize, sizeof(int) ) ) != sizeof(int) )
			return c->error;

		else if( ( c->error = gb_send( c, c->timeout, &cmd, sizeof(short) ) ) != sizeof(short) )
			return c->error;

		else if( ( c->error = gb_send( c, c->timeout, data, len ) ) != len )
			return c->error;

		else if( ( c->error = gb_recv( c, c->timeout, &c->reply.code, sizeof(short) ) ) != sizeof(short) )
			return c->error;

		else if( ( c->error = gb_recv( c, c->timeout, &c->reply.encoding, sizeof(gbEncoding) ) ) != sizeof(gbEncoding) )
			return c->error;

		else if( ( c->error = gb_recv( c, c->timeout, &rsize, sizeof(size_t) ) ) != sizeof(size_t) )
			return c->error;

		c->error = 0;

		if( rsize > c->reply.rsize ){
			c->reply.buffer = realloc( c->reply.buffer, rsize );
			c->reply.rsize = rsize;
		}

		c->reply.size = rsize;

		if( rsize > 0 )
		{
			if( ( c->error = gb_recv( c, c->timeout, c->reply.buffer, rsize ) ) != rsize )
				return c->error;
			else
				c->error = 0;
		}
	}

	return c->error;
}

int gb_send_command_assert( gbClient *c, short cmd, void *data, int len, short reply ){
	if( gb_send_command( c, cmd, data, len ) == 0 ){
		if( c->reply.code == reply )
			c->error = 0;
		else
			c->error = c->reply.code;
	}

	return c->error;
}

#define gb_realloc_if_needed( c, len ) if( (c)->request.rsize < (len) ){ \
											(c)->request.buffer = realloc( (c)->request.buffer, (len) ); \
											(c)->request.rsize  = (len); \
										} \
										(c)->request.size = (len)

void gb_build_command(gbClient *c, size_t len, char *key, int klen, char *value, int vlen, int num ){
	gb_realloc_if_needed( c, len );

	unsigned char *p = c->request.buffer;

	memcpy( p, key, klen ); p += klen;
	memcpy( p, " ", 1 ); ++p;

	if( value != NULL )
		memcpy( p, value, vlen );

	else
		sprintf( (char *)p, "%d", num );
}

int gb_digits(int number){
    int digits = number <= 0 ? 1 : 0;
    while (number) {
        number /= 10;
        ++digits;
    }
    return digits;
}

int gb_set(gbClient *c, char *key, int klen, char *value, int vlen, int ttl ) {
	int digits = gb_digits(ttl);
	size_t rsize = digits + 1 + klen + 1 + vlen;

	gb_realloc_if_needed( c, rsize );

	unsigned char *p = c->request.buffer;

	sprintf( (char *)p, "%d ", ttl ); p += digits + 1;
	memcpy( p, key, klen ); p += klen;
	memcpy( p, " ", 1 ); ++p;
	memcpy( p, value, vlen );

	return gb_send_command_assert( c, OP_SET, c->request.buffer, c->request.size, REPL_VAL );
}

int gb_mset(gbClient *c, char *expr, int elen, char *value, int vlen ) {
	gb_build_command( c, elen + 1 + vlen, expr, elen, value, vlen, 0 );

	return gb_send_command_assert( c, OP_MSET, c->request.buffer, c->request.size, REPL_VAL );
}

int gb_ttl( gbClient *c, char *key, int klen, int ttl ) {
	gb_build_command( c, klen + 1 + gb_digits(ttl), key, klen, NULL, 0, ttl );

	return gb_send_command_assert(c, OP_TTL, c->request.buffer, c->request.size, REPL_OK);
}

int gb_mttl(gbClient *c, char *expr, int elen, int ttl) {
	gb_build_command( c, elen + 1 + gb_digits(ttl), expr, elen, NULL, 0, ttl );

	return gb_send_command_assert(c, OP_MTTL, c->request.buffer, c->request.size, REPL_VAL);
}

int gb_get(gbClient *c, char *key, int klen) {
	return gb_send_command_assert(c, OP_GET, key, klen, REPL_VAL);
}

int gb_mget(gbClient *c, char *expr, int elen) {
	return gb_send_command_assert(c, OP_MGET, expr, elen, REPL_KVAL);
}

int gb_del(gbClient *c, char *key, int klen) {
	return gb_send_command_assert(c, OP_DEL, key, klen, REPL_OK);
}

int gb_mdel(gbClient *c, char *expr, int elen) {
	return gb_send_command_assert(c, OP_MDEL, expr, elen, REPL_VAL);
}

int gb_inc(gbClient *c, char *key, int klen) {
	return gb_send_command_assert(c, OP_INC, key, klen, REPL_VAL);
}

int gb_minc(gbClient *c, char *expr, int elen) {
	return gb_send_command_assert(c, OP_MINC, expr, elen, REPL_VAL);
}

int gb_dec(gbClient *c, char *key, int klen) {
	return gb_send_command_assert(c, OP_DEC, key, klen, REPL_VAL);
}

int gb_mdec(gbClient *c, char *expr, int elen) {
	return gb_send_command_assert(c, OP_MDEC, expr, elen, REPL_VAL);
}

int gb_lock(gbClient *c, char *key, int klen, int time) {
	gb_build_command( c, klen + 1 + gb_digits(time), key, klen, NULL, 0, time );

	return gb_send_command_assert(c, OP_LOCK, c->request.buffer, c->request.size, REPL_OK);
}

int gb_mlock(gbClient *c, char *expr, int elen, int time) {
	gb_build_command( c, elen + 1 + gb_digits(time), expr, elen, NULL, 0, time );

	return gb_send_command_assert(c, OP_MLOCK, c->request.buffer, c->request.size, REPL_VAL);
}

int gb_unlock(gbClient *c, char *key, int klen) {
	return gb_send_command_assert(c, OP_UNLOCK, key, klen, REPL_OK);
}

int gb_munlock(gbClient *c, char *expr, int elen) {
	return gb_send_command_assert(c, OP_MUNLOCK, expr, elen, REPL_VAL);
}

int gb_count(gbClient *c, char *expr, int elen) {
	return gb_send_command_assert(c, OP_COUNT, expr, elen, REPL_VAL);
}

int gb_stats(gbClient *c) {
	return gb_send_command_assert(c, OP_STATS, NULL, 0, REPL_KVAL);
}

int gb_quit(gbClient *c) {
	return gb_send_command_assert(c, OP_END, NULL, 0, REPL_OK);
}

const unsigned char *gb_reply_raw(gbClient *c){
	return c->reply.buffer;
}

long gb_reply_number(gbClient *c){
	return *(long *)c->reply.buffer;
}

void gb_reply_multi(gbClient *c, gbMultiBuffer *b){
	size_t i, klen, vsize;
	gbEncoding enc;
	gbBuffer *v;
	unsigned char *p = c->reply.buffer;

	b->count  = 0;
	b->keys   = (char **)NULL;
	b->values = NULL;

	b->count  = *(size_t *)p; p += sizeof(size_t);
	b->keys   = (char **)malloc( b->count * sizeof(char *) );
	b->values = (gbBuffer *)malloc( b->count * sizeof(gbBuffer) );

	for( i = 0; i < b->count; i++ ){
		GB_INIT_BUFFER( b->values[i] );

		v = &b->values[i];

		klen = *(size_t *)p; p += sizeof(size_t);

		b->keys[i] = (char *)calloc( 1, klen );

		memcpy( b->keys[i], p, klen ); p += klen;

		enc   = *(gbEncoding *)p; p += sizeof(gbEncoding);

		vsize = *(size_t *)p; p += sizeof(size_t);

		if( vsize > v->rsize ){
			v->buffer = realloc( v->buffer, vsize );
			v->rsize = vsize;
		}

		v->encoding = enc;
		v->size = vsize;

		memcpy( v->buffer, p, vsize ); p += vsize;
	}
}

void gb_reply_multi_free(gbMultiBuffer *b){
	size_t i;
	for( i = 0; i < b->count; i++ ){
		free( b->values[i].buffer );
		free( b->keys[i] );

		b->values[i].buffer = (unsigned char *)NULL;
		b->keys[i] = NULL;
	}

	free( b->values );
	free( b->keys );

	b->values = NULL;
	b->keys = (char **)NULL;
	b->count = 0;
}

void gb_disconnect( gbClient *c ){
	if( c->fd > 0 )
		close(c->fd);

	c->fd =
	c->port =
	c->error = -1;
	c->address[0] = 0x00;

	free( c->reply.buffer );
	free( c->request.buffer );
}
