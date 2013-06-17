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
#include "configure.h"
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <ctype.h>
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif
#include "log.h"
#include "net.h"
#include "atree.h"
#include "query.h"
#include "config.h"
#include "default.h"

#if HAVE_JEMALLOC == 1
#include <jemalloc/jemalloc.h>
#endif

extern char *aeApiName();

static gbServer server;

void gbReadQueryHandler( gbEventLoop *el, int fd, void *privdata, int mask );
void gbWriteReplyHandler( gbEventLoop *el, int fd, void *privdata, int mask );
void gbAcceptHandler(gbEventLoop *e, int fd, void *privdata, int mask);
void gbMemoryFreeHandler( atree_item_t *elem, size_t level, void *data );
int  gbServerCronHandler(struct gbEventLoop *eventLoop, long long id, void *data);
void gbDaemonize();
void gbProcessInit();
void gbServerDestroy( gbServer *server );

void gbHelpMenu( char **argv, int exitcode ){
	printf( "Gibson cache server v%s\nCopyright %s\nReleased under %s\n\n", VERSION, AUTHOR, LICENSE );

	printf( "%s [-h|--help] [-c|--config FILE]\n\n", argv[0] );

	printf("  -h, --help          Print this help and exit.\n");
	printf("  -c, --config FILE   Set configuration file to load, default %s.\n\n", GB_DEFAULT_CONFIGURATION );

	exit(exitcode);
}

int main( int argc, char **argv)
{
	int c, option_index = 0;

	static struct option long_options[] =
	{
		{"help",    no_argument,       0, 'h'},
		{"config",  required_argument, 0, 'c'},
		{0, 0, 0, 0}
	};

    char *configuration = GB_DEFAULT_CONFIGURATION;

    while(1){
    	c = getopt_long( argc, argv, "hc:", long_options, &option_index );
    	if( c == -1 )
    		break;

    	switch (c)
		{
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
					break;

			break;

			case 'h':

				gbHelpMenu(argv,0);

			break;

			case 'c':

				configuration = optarg;

			break;

			default:
				gbHelpMenu(argv,1);
		}
    }

	memset( &server, 0x00, sizeof(gbServer) );

    gbConfigLoad( &server.config, configuration );

	gbLogInit
	(
	  gbConfigReadString( &server.config, "logfile", GB_DEAFULT_LOG_FILE ),
	  gbConfigReadInt( &server.config, "loglevel", GB_DEFAULT_LOG_LEVEL ),
	  gbConfigReadInt( &server.config, "logflushrate", GB_DEFAULT_LOG_FLUSH_LEVEL )
	);

	const char *sock = gbConfigReadString( &server.config, "unix_socket", NULL );
	if( sock != NULL ){
		gbLog( INFO, "Creating unix server socket on %s ...", sock );

		strncpy( server.address, sock, 0xFF );
		unlink( server.address );

		server.type	= UNIX;
		server.fd   = gbNetUnixServer( server.error, server.address, 0777 );
	}
	else {
		const char *address = gbConfigReadString( &server.config, "address", GB_DEFAULT_ADDRESS );
		int port = gbConfigReadInt( &server.config, "port", GB_DEFAULT_PORT );

		gbLog( INFO, "Creating tcp server socket on %s:%d ...", address, port );

		strncpy( server.address, address, 0xFF );

		server.type	= TCP;
		server.port	= port;
		server.fd   = gbNetTcpServer( server.error, server.port, server.address );
	}

	if( server.fd == GBNET_ERR ){
		gbLog( ERROR, "Error creating server : %s", server.error );
		exit(1);
	}

	// read server limit values from config
	server.limits.maxidletime     = gbConfigReadInt( &server.config, "max_idletime",       GBNET_DEFAULT_MAX_IDLE_TIME );
	server.limits.maxclients      = gbConfigReadInt( &server.config, "max_clients",        GBNET_DEFAULT_MAX_CLIENTS );
	server.limits.maxrequestsize  = gbConfigReadSize( &server.config, "max_request_size",  GBNET_DEFAULT_MAX_REQUEST_BUFFER_SIZE );
	server.limits.maxitemttl	  = gbConfigReadInt( &server.config, "max_item_ttl",       GB_DEFAULT_MAX_ITEM_TTL );
	server.limits.maxmem		  = gbConfigReadSize( &server.config, "max_memory",        GB_DEFAULT_MAX_MEMORY );
	server.limits.maxkeysize	  = gbConfigReadSize( &server.config, "max_key_size",      GB_DEFAULT_MAX_QUERY_KEY_SIZE );
	server.limits.maxvaluesize	  = gbConfigReadSize( &server.config, "max_value_size",    GB_DEFAULT_MAX_QUERY_VALUE_SIZE );
	server.limits.maxresponsesize = gbConfigReadSize( &server.config, "max_response_size", GB_DEFAULT_MAX_RESPONSE_SIZE );

	// initialize server statistics
	server.stats.started     =
	server.stats.time	     = time(NULL);
	server.stats.memused     =
	server.stats.mempeak     =
	server.stats.firstin     =
	server.stats.lastin      =
	server.stats.crondone    =
	server.stats.nclients    =
	server.stats.nitems	     =
	server.stats.ncompressed =
	server.stats.sizeavg	 = 0;
	server.stats.memavail    = gbMemAvailable();

	if( server.limits.maxmem > server.stats.memavail ){
		char drop[0xFF] = {0};

		gbMemFormat( server.stats.memavail / 2, drop, 0xFF );

		gbLog( WARNING, "max_memory setting is higher than total available memory, dropping to %s.", drop );

		server.limits.maxmem = server.stats.memavail / 2;
	}

	server.compression = gbConfigReadSize( &server.config, "compression",	   GB_DEFAULT_COMPRESSION );
	server.daemon	   = gbConfigReadInt( &server.config, "daemonize", 		   0 );
	server.cronperiod  = gbConfigReadInt( &server.config, "cron_period", 	   GB_DEFAULT_CRON_PERIOD );
	server.pidfile	   = gbConfigReadString( &server.config, "pidfile",        GB_DEFAULT_PID_FILE );
	server.events 	   = gbCreateEventLoop( server.limits.maxclients + 1024 );
	server.clients 	   = ll_prealloc( server.limits.maxclients );
	server.m_keys	   = ll_prealloc( 255 );
	server.m_values	   = ll_prealloc( 255 );
	server.idlecron	   = server.limits.maxidletime * 1000;
	server.lzf_buffer  = calloc( 1, server.limits.maxrequestsize );
	server.m_buffer	   = calloc( 1, server.limits.maxresponsesize );
	server.shutdown	   = 0;

	at_init_tree( server.tree );

	char reqsize[0xFF] = {0},
		 maxmem[0xFF] = {0},
		 maxkey[0xFF] = {0},
		 maxvalue[0xFF] = {0},
		 maxrespsize[0xFF] = {0},
		 compr[0xFF] = {0};

	gbMemFormat( server.limits.maxrequestsize, reqsize, 0xFF );
	gbMemFormat( server.limits.maxmem, maxmem, 0xFF );
	gbMemFormat( server.limits.maxkeysize, maxkey, 0xFF );
	gbMemFormat( server.limits.maxvaluesize, maxvalue, 0xFF );
	gbMemFormat( server.limits.maxresponsesize, maxrespsize, 0xFF );
	gbMemFormat( server.compression, compr, 0xFF );

	gbLog( INFO, "Server starting ..." );
	gbLog( INFO, "Multiplexing API : '%s'", aeApiName() );
#if HAVE_JEMALLOC == 1
	const char *p;
	size_t s = sizeof(p);
	mallctl("version", &p,  &s, NULL, 0);

	gbLog( INFO, "Memory allocator : 'jemalloc %s'", p );
#else
	gbLog( INFO, "Memory allocator : 'malloc'" );
#endif
	gbLog( INFO, "Max idle time    : %ds", server.limits.maxidletime );
	gbLog( INFO, "Max clients      : %d", server.limits.maxclients );
	gbLog( INFO, "Max request size : %s", reqsize );
	gbLog( INFO, "Max memory       : %s", maxmem );
	gbLog( INFO, "Max key size     : %s", maxkey );
	gbLog( INFO, "Max value size   : %s", maxvalue );
	gbLog( INFO, "Max resp. size   : %s", maxrespsize );
	gbLog( INFO, "Data LZF compr.  : %s", compr );
	gbLog( INFO, "Cron period      : %dms", server.cronperiod );

	gbProcessInit();

	server.cron_id = gbCreateTimeEvent( server.events, 1, gbServerCronHandler, &server, NULL );

	gbCreateFileEvent( server.events, server.fd, GB_READABLE, gbAcceptHandler, &server );

	gbEventLoopMain( server.events );
	gbDeleteEventLoop( server.events );

	return 0;
}

void gbWriteReplyHandler( gbEventLoop *el, int fd, void *privdata, int mask ) {
    gbClient *client = privdata;
    size_t nwrote = 0, towrite = 0;

    towrite = client->buffer_size - client->wrote;
    nwrote  = write( client->fd, client->buffer + client->wrote, towrite );

    if (nwrote == -1){
		if (errno == EAGAIN){
			nwrote = 0;
		}
		else{
			gbLog( WARNING, "Error writing to client: %s",strerror(errno));
			gbClientDestroy(client);
			return;
		}
	}
	else if (nwrote == 0){
		gbLog( DEBUG, "Client closed connection.");
		gbClientDestroy(client);
		return;
	}
	else{
		client->wrote += nwrote;
		client->seen = client->server->stats.time;

		if( client->wrote == client->buffer_size ){
			if( client->shutdown )
				gbClientDestroy(client);

			else{
				gbClientReset(client);
				gbDeleteFileEvent( client->server->events, client->fd, GB_WRITABLE );
			}
		}

	}
}

void gbReadQueryHandler( gbEventLoop *el, int fd, void *privdata, int mask ) {
	gbClient *client = ( gbClient * )privdata;
	gbServer *server = client->server;
	int nread, toread;

	if( client->buffer_size < 0 && client->read >= sizeof( int ) ){
		client->buffer_size = *(int *)client->buffer;
		client->read = 0;

		if( client->buffer_size > server->limits.maxrequestsize || client->buffer_size < 0 ){
			gbLog( WARNING, "Client request size %d invalid.", client->buffer_size );
			gbClientDestroy(client);
			return;
		}
	}

	toread = client->buffer_size >= 0 ? client->buffer_size : sizeof( int );
	nread =  read( fd, client->buffer + client->read, toread );

	if (nread == -1){
		if (errno == EAGAIN){
			nread = 0;
		}
		else{
			gbLog( WARNING, "Error reading from client: %s",strerror(errno));
			gbClientDestroy(client);
			return;
		}
	}
	else if (nread == 0){
		gbLog( DEBUG, "Client closed connection.");
		gbClientDestroy(client);
		return;
	}
	else{
    	client->read += nread;
    	client->seen = client->server->stats.time;

    	if( client->read == client->buffer_size ){
    		if( gbProcessQuery(client) != GB_OK ){
    			gbLog( WARNING, "Malformed query, dropping client." );

    			char tmp[400] = {0},
    			    *p = &tmp[0],
    			    c;
    			int i, sz = client->buffer_size < 100 ? client->buffer_size : 100;
    			for( i = 0; i < sz; i++ ){
    				c = client->buffer[i];
    				if( isprint(c) ){
    					sprintf( p, "%c", c );
    					++p;
    				}
    				else {
    					sprintf( p, " %02X ", c );
    					p += 4;
    				}
    			}

    			gbLog( WARNING, "  Buffer size: %d opcode:%d - First %d bytes:", client->buffer_size, *(short *)&client->buffer[0], sz );
    			gbLog( WARNING, "  %s", tmp );
    			gbClientDestroy(client);
    		}
    	}
    }
}

void gbAcceptHandler(gbEventLoop *e, int fd, void *privdata, int mask) {
    int client_port = 0, client_fd;
    char client_ip[128] = {0};
    gbServer *server = (gbServer *)privdata;

    if( server->type == TCP )
    	client_fd = gbNetTcpAccept( server->error, fd, client_ip, &client_port );
    else
    	client_fd = gbNetUnixAccept( server->error, fd );

    if (client_fd == GB_ERR) {
    	gbLog( WARNING, "Error accepting client connection: %s", server->error );
        return;
    }
    else if( server->stats.nclients >= server->limits.maxclients ) {
    	close(client_fd);
    	gbLog( WARNING, "Dropping connection, current clients = %d, max = %d.", server->stats.nclients, server->limits.maxclients );
    }

    gbLog( DEBUG, "New connection from %s:%d", *client_ip ? client_ip : server->address, client_port );

	if (client_fd != -1) {
		gbNetNonBlock(NULL,client_fd);
		gbNetEnableTcpNoDelay(NULL,client_fd);

	    gbClient *client = gbClientCreate(client_fd,server);

		if( gbCreateFileEvent( e, client_fd, GB_READABLE, gbReadQueryHandler, client ) == GB_ERR ) {
			gbClientDestroy( client );
			close(client_fd);
			return;
		}
	}
}

void gbMemoryFreeHandler( atree_item_t *elem, size_t level, void *data ) {
	gbServer *server = data;
	gbItem	 *item = elem->e_marker;
	time_t	  eta = item ? ( server->stats.time - item->time ) : 0;

	// item is older enough to be deleted
	if( eta && eta >= server->gcdelta ) {
		// item locked, skip
		if( item->lock == -1 || eta < item->lock ) return;

		// item is freeable
		elem->e_marker = NULL;

		gbDestroyItem( server, item );
	}
}

void gbHandleDeadTTLHandler( atree_item_t *elem, size_t level, void *data ){
	gbServer *server = data;
	gbItem	 *item = elem->e_marker;
	time_t	  eta = item ? ( server->stats.time - item->time ) : 0;

	// item is older enough to be deleted
	if( item && item->ttl > 0 && eta >= item->ttl ) {
		// item locked, skip
		if( item->lock == -1 || eta < item->lock ) return;

		gbLog( DEBUG, "[CRON] TTL of %ds expired for item at %p.", item->ttl, item );

		gbDestroyItem( server, item );

		// item is freeable
		elem->e_marker = NULL;
	}
}

#define CRON_EVERY(_ms_) if ((_ms_ <= server->cronperiod) || !(server->stats.crondone % ((_ms_)/server->cronperiod)))

int gbServerCronHandler(struct gbEventLoop *eventLoop, long long id, void *data) {
	gbServer *server = data;
	time_t now = time(NULL);
	char used[0xFF] = {0},
		 max[0xFF] = {0},
		 freed[0xFF] = {0},
		 uptime[0xFF] = {0},
		 avgsize[0xFF] = {0};
	unsigned long before = 0, deleted = 0;

	server->stats.time = now;

	// shutdown requested
	if( server->shutdown )
		gbServerDestroy( server );

	CRON_EVERY( 15000 ) {
		before = server->stats.memused;

		at_recurse( &server->tree, gbHandleDeadTTLHandler, server, 0 );

		deleted = before - server->stats.memused;

		if( deleted > 0 ){
			gbMemFormat( deleted, freed,  0xFF );

			gbLog( INFO, "Freed %s of expired data, left %d items.", freed, server->stats.nitems );
		}
	}

	CRON_EVERY( 5000 ) {
		if( server->stats.memused > server->limits.maxmem ){
			// TODO: Implement a better algorithm for this!
			server->gcdelta = ( server->stats.time - server->stats.firstin ) / 5.0;

			before = server->stats.memused;

			gbLog( WARNING, "Max memory exhausted, trying to free data older than %ds.", server->gcdelta );

			at_recurse( &server->tree, gbMemoryFreeHandler, server, 0 );

			gbMemFormat( before - server->stats.memused, freed,  0xFF );

			gbLog( INFO, "Freed %s, left %d items.", freed, server->stats.nitems );

			server->gcdelta = 0;
		}
	}

	CRON_EVERY( server->idlecron ) {
		// Check for clients in idle state for too long.
		ll_foreach( server->clients, llitem )
		{
			gbClient *client = ll_data( gbClient *, llitem );
			if( client != NULL && ( now - client->seen ) > server->limits.maxidletime )
			{
				gbLog( WARNING, "Removing dead client.", client );
				gbClientDestroy(client);
				// Do not break the loop but start again.
				llitem = server->clients->head;
				// Have we removed the only client?
				if( !llitem )
					break;
			}
		}
	}

	CRON_EVERY( 15000 ){
		gbMemFormat( server->stats.memused, used, 0xFF );
		gbMemFormat( server->limits.maxmem, max,  0xFF );
		gbMemFormat( server->stats.sizeavg, avgsize, 0xFF );

		gbServerFormatUptime( server, uptime );

		gbLog
		(
		  INFO,
		  "MEM %s/%s - CLIENTS %d - OBJECTS %d ( %d COMPRESSED ) - AVERAGE SIZE %s - UPTIME %s",
		  used,
		  max,
		  server->stats.nclients,
		  server->stats.nitems,
		  server->stats.ncompressed,
		  avgsize,
		  uptime
		);
	}

	++server->stats.crondone;

	return server->cronperiod;
}

void gbDaemonize(){
    int fd;

	// parent exits
    if (fork() != 0) exit(0);
	// create a new session
    setsid();

    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
}

static void gbSignalHandler(int sig) {
	if( sig == SIGTERM ){
		gbLog( WARNING, "Received SIGTERM, scheduling shutdown..." );
		server.shutdown = 1;
	}
	else if( sig == SIGSEGV ){
		gbLog( CRITICAL, "" );
		gbLog( CRITICAL, "********* SEGMENTATION FAULT *********" );
		gbLog( CRITICAL, "" );

		void *trace[32];
		size_t size, i;
		char **strings;
#if HAVE_BACKTRACE
		size    = backtrace( trace, 32 );
		strings = backtrace_symbols( trace, size );
#endif
		char used[0xFF] = {0},
			 max[0xFF] = {0},
			 uptime[0xFF] = {0};

		gbMemFormat( server.stats.memused, used, 0xFF );
		gbMemFormat( server.limits.maxmem, max,  0xFF );
		gbServerFormatUptime( &server, uptime );

		gbLog( CRITICAL, "INFO:" );
		gbLog( CRITICAL, "" );

		gbLog( CRITICAL, "  Uptime          : %s", uptime );
		gbLog( CRITICAL, "  Memory Used     : %s/%s", used, max );
		gbLog( CRITICAL, "  Current Items   : %d", server.stats.nitems );
		gbLog( CRITICAL, "  Current Clients : %d", server.stats.nclients );
#if HAVE_BACKTRACE
		gbLog( CRITICAL, "" );
		gbLog( CRITICAL, "BACKTRACE:" );
		gbLog( CRITICAL, "" );

		for( i = 0; i < size; i++ ){
			gbLog( CRITICAL, "  %s", strings[i] );
		}
#endif
		gbLog( CRITICAL, "" );
		gbLog( CRITICAL, "***************************************" );

		exit(-1);
	}
}

void gbProcessInit(){
	struct sigaction act;

	if( server.daemon )
		gbDaemonize();

	// ignore SIGHUP and SIGPIPE since we're gonna handle dead clients
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	// set SIGTERM and SIGSEGV custom handler
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = gbSignalHandler;

	sigaction( SIGTERM, &act, NULL );
	sigaction( SIGSEGV, &act, NULL );

	FILE *fp = fopen(server.pidfile,"w+t");
	if (fp) {
		fprintf(fp,"%d\n",(int)getpid());
		fclose(fp);
	}
	else
		gbLog( WARNING, "Error creating pid file %s.", server.pidfile );
}

void gbObjectDestroyHandler( atree_item_t *elem, size_t level, void *data ){
	gbItem *item = elem->e_marker;
	if( item )
		gbDestroyItem( data, item );
}

void gbConfigDestroyHandler( atree_item_t *elem, size_t level, void *data ){
	char *item = elem->e_marker;
	if( item )
		free( item );
}

void gbServerDestroy( gbServer *server ){
	if( server->clients ){
		ll_foreach( server->clients, citem ){
			gbClient *client = citem->data;
			if( client ){
				gbClientDestroy(client);
			}
		}

		ll_destroy( server->clients );
	}

	ll_destroy( server->m_keys );
	ll_destroy( server->m_values );

	free( server->m_buffer );
	free( server->lzf_buffer );

	at_recurse( &server->tree, gbObjectDestroyHandler, server, 0 );
	at_recurse( &server->config, gbConfigDestroyHandler, NULL, 0 );

	at_free( &server->tree );
	at_free( &server->config );

	gbDeleteTimeEvent( server->events, server->cron_id );
	gbDeleteEventLoop( server->events );
	gbLogFinalize();

	exit( 0 );
}
