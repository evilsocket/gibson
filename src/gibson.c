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
#include "server.h"


// the global server instance
gbServer server;

void gbHelpMenu( char **argv, int exitcode ){
	printf( "Gibson cache server v%s %s ( built %s )\nCopyright %s\nReleased under %s\n\n", VERSION, BUILD_GIT_BRANCH, BUILD_DATETIME, AUTHOR, LICENSE );

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

    zmem_set_oom_handler(gbOOM);

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
    server.stats.requests    =
    server.stats.connections =
	server.stats.sizeavg	 = 
    server.stats.compravg    = 0;
	server.stats.memavail    = zmem_available();

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
    server.gc_ratio    = gbConfigReadTime( &server.config, "gc_ratio",         GB_DEFAULT_GC_RATIO );
	server.events 	   = gbCreateEventLoop( server.limits.maxclients + 1024 );
	server.clients 	   = ll_prealloc( server.limits.maxclients );
	server.m_keys	   = ll_prealloc( 255 );
	server.m_values	   = ll_prealloc( 255 );
	server.idlecron	   = server.limits.maxidletime * 1000;
	server.lzf_buffer  = zcalloc( server.limits.maxrequestsize );
	server.m_buffer	   = zcalloc( server.limits.maxresponsesize );
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

    extern char *aeApiName();

	gbLog( INFO, "Server starting ..." );
	gbLog( INFO, "Git Branch       : '%s'", BUILD_GIT_BRANCH );
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
    gbLog( INFO, "GC Ratio         : %ds", server.gc_ratio );
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
