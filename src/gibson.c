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
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include "log.h"
#include "net.h"
#include "atree.h"
#include "query.h"
#include "config.h"
#include "default.h"
#include "configure.h"

static gbServer server;
static atree_t  config;

void gbReadQueryHandler( gbEventLoop *el, int fd, void *privdata, int mask );
void gbWriteReplyHandler( gbEventLoop *el, int fd, void *privdata, int mask );
void gbAcceptHandler(gbEventLoop *e, int fd, void *privdata, int mask);
void gbMemoryFreeHandler( atree_item_t *elem, void *data );
int  gbServerCronHandler(struct gbEventLoop *eventLoop, long long id, void *data);
void gbDaemonize();
void gbProcessInit();

void gbHelpMenu( char **argv, int exitcode ){
	printf( "Gibson cache server v%s\nCopyright %s\nReleased under %s\n\n", VERSION, AUTHOR, LICENSE );

	printf( "%s [-h|--help] [-c|--config FILE]\n\n", argv[0] );

	printf("  -h, --help          print this help and exit\n");
	printf("  -c, --config FILE   set configuration file to load\n\n");

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

    while (1)
    {
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

    gbConfigLoad( &config, configuration );

	gbLogInit
	(
	  gbConfigReadString( &config, "logfile", GB_DEAFULT_LOG_FILE ),
	  gbConfigReadInt( &config, "loglevel", GB_DEFAULT_LOG_LEVEL ),
	  gbConfigReadInt( &config, "logflushrate", GB_DEFAULT_LOG_FLUSH_LEVEL )
	);

	memset( &server, 0x00, sizeof(gbServer) );

	const char *sock = gbConfigReadString( &config, "unix_socket", NULL );
	if( sock != NULL ){
		gbLog( INFO, "Creating unix server socket on %s ...", sock );

		strncpy( server.address, sock, 0xFF );
		unlink( server.address );

		server.type	= UNIX;
		server.fd   = gbNetUnixServer( server.error, server.address, 0 );
	}
	else {
		const char *address = gbConfigReadString( &config, "address", GB_DEFAULT_ADDRESS );
		int port = gbConfigReadInt( &config, "port", GB_DEFAULT_PORT );

		gbLog( INFO, "Creating tcp server socket on %s:%d ...", address, port );

		strncpy( server.address, address, 0xFF );

		server.type	= TCP;
		server.port	= port;
		server.fd   = gbNetUnixServer( server.error, server.address, 0 );
	}

	if( server.fd == GBNET_ERR ){
		gbLog( ERROR, "Error creating server : %s", server.error );
		exit(1);
	}

	server.maxidletime    = gbConfigReadInt( &config, "max_idletime",      GBNET_DEFAULT_MAX_IDLE_TIME );
	server.maxclients     = gbConfigReadInt( &config, "max_clients",       GBNET_DEFAULT_MAX_CLIENTS );
	server.maxrequestsize = gbConfigReadSize( &config, "max_request_size", GBNET_DEFAULT_MAX_REQUEST_BUFFER_SIZE );
	server.maxitemttl	  = gbConfigReadInt( &config, "max_item_ttl",      GB_DEFAULT_MAX_ITEM_TTL );
	server.maxmem		  = gbConfigReadSize( &config, "max_memory",       GB_DEFAULT_MAX_MEMORY );
	server.daemon		  = gbConfigReadInt( &config, "daemonize", 		   0 );
	server.cronperiod	  = gbConfigReadInt( &config, "cron_period", 	   GB_DEFAULT_CRON_PERIOD );
	server.pidfile		  = gbConfigReadString( &config, "pidfile",        GB_DEFAULT_PID_FILE );
	server.events 	      = gbCreateEventLoop( server.maxclients + 1024 );
	server.clients 	      = ll_prealloc( server.maxclients );
	server.memused		  =
	server.firstin		  =
	server.lastin		  =
	server.crondone		  =
	server.nclients	      =
	server.nitems	      =
	server.shutdown		  = 0;

	at_init_tree( server.tree );

	char reqsize[0xFF] = {0},
		 maxmem[0xFF] = {0};

	gbMemFormat( server.maxrequestsize, reqsize, 0xFF );
	gbMemFormat( server.maxmem, maxmem, 0xFF );

	gbLog( INFO, "Server starting ..." );
	gbLog( INFO, "Max idle time    : %ds", server.maxidletime );
	gbLog( INFO, "Max clients      : %d", server.maxclients );
	gbLog( INFO, "Max request size : %s", reqsize );
	gbLog( INFO, "Max memory       : %s", maxmem );
	gbLog( INFO, "Cron period      : %dms", server.cronperiod );

	gbProcessInit();

	gbCreateTimeEvent( server.events, 1, gbServerCronHandler, &server, NULL );
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

    if (nwrote == -1)
	{
		if (errno == EAGAIN)
		{
			nwrote = 0;
		}
		else
		{
			gbLog( WARNING, "Error writing to client: %s",strerror(errno));
			gbClientDestroy(client);
			return;
		}
	}
	else if (nwrote == 0)
	{
		gbLog( INFO, "Client closed connection.");
		gbClientDestroy(client);
		return;
	}


	if (nwrote)
	{
		client->wrote += nwrote;
		client->seen = client->server->time;

		if( client->wrote == client->buffer_size )
		{
			if( client->shutdown )
				gbClientDestroy(client);

			else
			{
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

	if( client->buffer_size < 0 && client->read >= sizeof( client->buffer_size ) )
	{
		client->buffer_size = *(int *)client->buffer;
		client->read = 0;

		if( client->buffer_size <= server->maxrequestsize && client->buffer_size >= 0 )
		{
			client->buffer = realloc( client->buffer, client->buffer_size );
		}
		else
		{
			gbLog( WARNING, "Client request %d > server->maxrequestsize or < 0 .", client->buffer_size );
			gbClientDestroy(client);
			return;
		}
	}

	toread = client->buffer_size >= 0 ? client->buffer_size : sizeof( client->buffer_size );
	nread =  read( fd, client->buffer + client->read, toread );

	if (nread == -1)
	{
		if (errno == EAGAIN)
		{
			nread = 0;
		}
		else
		{
			gbLog( WARNING, "Error reading from client: %s",strerror(errno));
			gbClientDestroy(client);
			return;
		}
	}
	else if (nread == 0)
	{
		gbLog( INFO, "Client closed connection.");
		gbClientDestroy(client);
		return;
	}


    if (nread)
    {
    	client->read += nread;
    	client->seen = client->server->time;

    	if( client->read == client->buffer_size )
    	{
    		if( gbProcessQuery(client) != GB_OK )
    		{
    			gbLog( WARNING, "Malformed query, dropping client." );
    			gbClientDestroy(client);
    		}
    	}
    }
}

void gbAcceptHandler(gbEventLoop *e, int fd, void *privdata, int mask) {
    int cport, cfd;
    char cip[128] = {0};
    gbServer *server = (gbServer *)privdata;

    if( server->type == TCP )
    	cfd = gbNetTcpAccept( server->error, fd, cip, &cport );
    else
    	cfd = gbNetUnixAccept( server->error, fd );

    if (cfd == GB_ERR) {
    	gbLog( WARNING, "Error accepting client connection: %s", server->error );
        return;
    }
    else if( server->nclients >= server->maxclients )
    {
    	close(cfd);
    	gbLog( WARNING, "Dropping connection, current clients = %d, max = %d.", server->nclients, server->maxclients );
    }

    gbLog( INFO, "New connection from %s:%d", *cip ? cip : server->address, cport );

	if (cfd != -1) {
		gbNetNonBlock(NULL,cfd);
		gbNetEnableTcpNoDelay(NULL,cfd);

	    gbClient *client = gbClientCreate(cfd,server);

		if ( gbCreateFileEvent( e, cfd, GB_READABLE, gbReadQueryHandler, client ) == GB_ERR )
		{
			gbClientDestroy( client );
			close(cfd);
			return;
		}
	}
}

void gbMemoryFreeHandler( atree_item_t *elem, void *data ) {
	gbServer *server = data;
	gbItem	 *item = elem->e_marker;
	time_t	  eta = item ? ( server->time - item->time ) : 0;

	// item is older enough to be deleted
	if( eta >= server->freeolderthan ) {
		// item locked, skip
		if( item->lock == -1 || eta < item->lock ) return;

		// item is freeable
		elem->e_marker = NULL;

		gbDestroyItem( server, item );
	}
}

#define CRON_EVERY(_ms_) if ((_ms_ <= server->cronperiod) || !(server->crondone%((_ms_)/server->cronperiod)))

int gbServerCronHandler(struct gbEventLoop *eventLoop, long long id, void *data) {
	gbServer *server = data;
	time_t now = time(NULL);
	char used[0xFF] = {0},
		 max[0xFF] = {0},
		 freed[0xFF] = {0};

	server->time = now;

	CRON_EVERY( 5000 ) {

		gbMemFormat( server->memused, used, 0xFF );
		gbMemFormat( server->maxmem,  max,  0xFF );

		gbLog( INFO, "%s/%s, clients = %d, stored items = %d", used, max, server->nclients, server->nitems );

		if( server->memused > server->maxmem ){
			time_t delta = ( server->time - server->firstin ) / 2.0;
			unsigned long before = server->memused;

			gbLog( WARNING, "Max memory exhausted, trying to free data older than %ds.", delta );

			server->freeolderthan = delta;

			at_recurse( &server->tree, gbMemoryFreeHandler, server );

			gbMemFormat( before - server->memused, freed,  0xFF );

			gbLog( INFO, "Freed %s, left %d items.", freed, server->nitems );

			server->freeolderthan = 0;
		}
	}

	CRON_EVERY( server->maxidletime * 1000 ) {
		// Check for clients in idle state for too long.
		ll_foreach( server->clients, llitem )
		{
			gbClient *client = ll_data( gbClient *, llitem );
			if( client != NULL && ( now - client->seen ) > server->maxidletime )
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

	if( server->shutdown )
		exit( 0 );

	++server->crondone;

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

static void gbSigTermHandler(int sig) {
    gbLog( WARNING, "Received SIGTERM, scheduling shutdown..." );

    server.shutdown = 1;
}

void gbProcessInit(){
	if( server.daemon )
		gbDaemonize();

	// ignore SIGHUP and SIGPIPE since we're gonna handle dead clients
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	// set SIGTERM custom handler
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = gbSigTermHandler;
	sigaction(SIGTERM, &act, NULL);

	FILE *fp = fopen(server.pidfile,"w+t");
	if (fp) {
		fprintf(fp,"%d\n",(int)getpid());
		fclose(fp);
	}
	else
		gbLog( WARNING, "Error creating pid file %s.", server.pidfile );
}
