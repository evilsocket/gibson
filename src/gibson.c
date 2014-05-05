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

// command line arguments
static struct option long_options[] =
{
    { "help",    no_argument,       0, 'h' },
    { "config",  required_argument, 0, 'c' },
    // overridable configuration directives
    { "logfile", required_argument, 0, 0x00 },
    { "loglevel", required_argument, 0, 0x00 },
    { "logflushrate", required_argument, 0, 0x00 },
    { "unix_socket", required_argument, 0, 0x00 },
    { "address", required_argument, 0, 0x00 },
    { "port", required_argument, 0, 0x00 },
    { "max_idletime", required_argument, 0, 0x00 },
    { "max_clients", required_argument, 0, 0x00 },
    { "max_request_size", required_argument, 0, 0x00 },
    { "max_item_ttl", required_argument, 0, 0x00 },
    { "max_memory", required_argument, 0, 0x00 },
    { "max_key_size", required_argument, 0, 0x00 },
    { "max_value_size", required_argument, 0, 0x00 },
    { "max_response_size", required_argument, 0, 0x00 },
    { "compression", required_argument, 0, 0x00 },
    { "daemonize", required_argument, 0, 0x00 },
    { "cron_period", required_argument, 0, 0x00 },
    { "pidfile", required_argument, 0, 0x00 },
    { "gc_ratio", required_argument, 0, 0x00 },
    { "max_mem_cron", required_argument, 0, 0x00 },
    { "expired_cron", required_argument, 0, 0x00 },

    {0, 0, 0, 0}
};

static char *descriptions[] = {
    "Print this help menu and exit.",
    "Set configuration file to load.",
    "The log file path, or /dev/stdout to log on the terminal output.",
    "Integer number representing the verbosity of the log manager.",
    "How often to flush logfile, where 1 stands for 'flush the log file every new line'.",
    "The UNIX socket path to use if Gibson will run in a local environment, use the directives address and port to create a TCP server instead.",
    "Address to bind the TCP server to.",
    "TCP port to use for server listening.",
    "Maximum time in seconds a client can be idle ( without read or write operations ), after this period the client connection will be closed.",
    "Maximum number of clients Gibson can hadle concurrently.",
    "Maximum size of a client request.",
    "Maximum time-to-live an object can have.",
    "Maximum memory to be used by the Gibson server, when this value is reached, the server will try to deallocate old objects to free space ( see gc_ratio ) and, if there aren't freeable objects at the moment, will refuse to accept new objects with a REPL_ERR_MEM error reply.",
    "Maximum size of the key for a Gibson object.",
    "Maximum size of the value for a Gibson object.",
    "Maximum Gibson response size, used to limit I/O when a M* operator is used.",
    "Objects above this size will be compressed in memory.",
    "If 1 the process server will be daemonized ( put on background ), otherwise will run synchronously with the caller process.",
    "Number of milliseconds between each cron schedule, do not put a value higher than 1000.",
    "File to be used to save the current Gibson process id.",
    "If max_memory is reached, data that is not being accessed in this amount of time ( i.e. gc_ratio 1h = data that is not being accessed in the last hour ) get deleted to release memory for the server.",
    "Check if max memory usage is reached every 'max_mem_cron' seconds.",
    "Check for expired items every 'expired_cron' seconds."
};

// the global server instance
gbServer server;

void gbHelpMenu( char **argv, int exitcode ){
    size_t i = 0;
    struct option *popt = &long_options[0];
    char s[0xFF];

	printf( "Gibson cache server v%s %s ( built %s )\n", VERSION, BUILD_GIT_BRANCH, BUILD_DATETIME );
    printf( "Released under %s\n\n", LICENSE );

	printf( "Usage: %s [options]\n", argv[0] );
    printf( "Options:\n" );

    while( popt && popt->name ){
        memset( s, 0x00, 0xFF );

        if( popt->val != 0x00 )
            sprintf( s, "  -%c, --%s", popt->val, popt->name );

        else
            sprintf( s, "  --%s", popt->name );

        if( popt->has_arg == required_argument )
            strcat( s, " VALUE" );

        printf( "%-35s %s\n", s, descriptions[i] );

        ++popt;
        ++i;
    }

    printf("\n");

    printf( "To report issues : <http://github.com/evilsocket/gibson/issues>\n" );
    printf( "Gibson website   : <http://gibson-db.in>\n" );
    printf( "Documentation    : <http://gibson-db.in/documentation.html>\n\n" );

	exit(exitcode);
}

int main( int argc, char **argv)
{
	int c, option_index = 0;
    char *configuration = GB_DEFAULT_CONFIGURATION;

    while( ( c = getopt_long( argc, argv, "hc:", long_options, &option_index ) ) != -1 ){
    	switch (c)
		{
			case 'h':

				gbHelpMenu(argv,0);

			break;

			case 'c':

				configuration = optarg;

			break;
		}
    }

    // this must be resetted to 1 in order to allow another getopt_long to parse argv
    optind = 1;

    zmem_set_oom_handler(gbOOM);

	memset( &server, 0x00, sizeof(gbServer) );

    // load configuration from file
    gbConfigLoad( &server.config, configuration );
    // perform configuration override from command line
    gbConfigMerge( &server.config, "hc:", long_options, argc, argv );
    
    // start reading configuration
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
    server.stats.mempeak     =
    server.stats.memused     = zmem_used();
	server.stats.memavail    = zmem_available();

	if( server.limits.maxmem > server.stats.memavail ){
		char drop[0xFF] = {0};

		gbMemFormat( server.stats.memavail / 2, drop, 0xFF );

		gbLog( WARNING, "max_memory setting is higher than total available memory, dropping to %s.", drop );

		server.limits.maxmem = server.stats.memavail / 2;
	}

	server.compression = gbConfigReadSize( &server.config, "compression",	 GB_DEFAULT_COMPRESSION );
	server.daemon	   = gbConfigReadInt( &server.config, "daemonize", 		 0 );
	server.cronperiod  = gbConfigReadInt( &server.config, "cron_period", 	 GB_DEFAULT_CRON_PERIOD );
	server.pidfile	   = gbConfigReadString( &server.config, "pidfile",      GB_DEFAULT_PID_FILE );
    server.gc_ratio    = gbConfigReadTime( &server.config, "gc_ratio",       GB_DEFAULT_GC_RATIO );
    server.max_mem_cron = gbConfigReadTime( &server.config, "max_mem_cron",  GB_DEFAULT_MAX_MEM_CRON ) * 1000;
    server.expired_cron = gbConfigReadTime( &server.config, "expired_cron",  GB_DEFAULT_EXPIRED_CRON ) * 1000;
	server.clients 	   = ll_prealloc( server.limits.maxclients );
	server.m_keys	   = ll_prealloc( 255 );
	server.m_values	   = ll_prealloc( 255 );
	server.idlecron	   = server.limits.maxidletime * 1000;
	server.lzf_buffer  = zcalloc( server.limits.maxrequestsize );
	server.m_buffer	   = zcalloc( server.limits.maxresponsesize );
	server.shutdown	   = 0;

	tr_init_tree( server.tree );

	char reqsize[0xFF] = {0},
		 maxmem[0xFF] = {0},
         sysmem[0xFF] = {0},
		 maxkey[0xFF] = {0},
		 maxvalue[0xFF] = {0},
		 maxrespsize[0xFF] = {0},
		 compr[0xFF] = {0},
         allocator[0xFF] = {0};

	gbMemFormat( server.limits.maxrequestsize, reqsize, 0xFF );
	gbMemFormat( server.limits.maxmem, maxmem, 0xFF );
    gbMemFormat( server.stats.memavail, sysmem, 0xFF );
	gbMemFormat( server.limits.maxkeysize, maxkey, 0xFF );
	gbMemFormat( server.limits.maxvaluesize, maxvalue, 0xFF );
	gbMemFormat( server.limits.maxresponsesize, maxrespsize, 0xFF );
	gbMemFormat( server.compression, compr, 0xFF );

    zmem_allocator( allocator, 0xFF );

	gbLog( INFO, "Server starting ..." );
	gbLog( INFO, "Git Branch       : '%s'", BUILD_GIT_BRANCH );
	gbLog( INFO, "Multiplexing API : '%s'", GB_MUX_API );
	gbLog( INFO, "Memory allocator : '%s'", allocator );
	gbLog( INFO, "Max idle time    : %ds", server.limits.maxidletime );
	gbLog( INFO, "Max clients      : %d", server.limits.maxclients );
	gbLog( INFO, "Max request size : %s", reqsize );
	gbLog( INFO, "Max memory       : %s", maxmem );
    gbLog( INFO, "System memory    : %s", sysmem );
    gbLog( INFO, "GC Ratio         : %ds", server.gc_ratio );
	gbLog( INFO, "Max key size     : %s", maxkey );
	gbLog( INFO, "Max value size   : %s", maxvalue );
	gbLog( INFO, "Max resp. size   : %s", maxrespsize );
	gbLog( INFO, "Data LZF compr.  : %s", compr );
	gbLog( INFO, "Cron period      : %dms", server.cronperiod );

	gbProcessInit();
	
	/*
	 * Under FreeBSD events must be created after gbProcessInit since daemonizing
	 * the process would mess up timers ( see issue #14 ).
	 */
	server.events  = gbCreateEventLoop( server.limits.maxclients + 1024 );
	server.cron_id = gbCreateTimeEvent( server.events, 1, gbServerCronHandler, &server, NULL );

	gbCreateFileEvent( server.events, server.fd, GB_READABLE, gbAcceptHandler, &server );

	gbEventLoopMain( server.events );
	gbDeleteEventLoop( server.events );

	return 0;
}
