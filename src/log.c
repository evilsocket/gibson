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
#include "log.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

static FILE 	  *__log_fp       = NULL;
static gbLogLevel __log_level     = DEBUG;
static int		  __log_flushrate = 1;
static int		  __log_counter   = 0;

void gbLogInit( const char *filename, gbLogLevel level, unsigned int flushrate ) 
{
    assert( filename != NULL );
    
	__log_fp = fopen( filename, "a+t" );
	if( __log_fp == NULL ){
		printf( "ERROR: Unable to open logfile %s!\n", filename );
		exit(1);
	}

	__log_level = level;
	__log_flushrate = flushrate;
}


void gbLog( gbLogLevel level, const char *format, ... )
{
    assert( format != NULL );
    assert( __log_fp != NULL );

	char 	buffer[0xFF] 	= {0},
			timestamp[0xFF] = {0},
		   *slevel = "???";
	va_list ap;
	time_t 		rawtime  = 0;
  	struct tm * timeinfo = NULL;

	if( level >= __log_level )
    {
		va_start( ap, format );
			vsnprintf( buffer, 0xFF, format, ap );
		va_end(ap);

		time( &rawtime );
  		timeinfo = localtime( &rawtime );

  		strftime( timestamp, 0xFF, "%m/%d/%Y %X", timeinfo );

		switch( level )
		{
			case DEBUG    : slevel = "DBG"; break;
			case WARNING  : slevel = "WAR"; break;
			case INFO     : slevel = "INF"; break;
			case ERROR    : slevel = "ERR"; break;
			case CRITICAL : slevel = "CRT"; break;
		}

		fprintf( __log_fp, "[%s] [%s] %s\n", timestamp, slevel, buffer );

		if( ( ++__log_counter % __log_flushrate ) == 0 )
			fflush(__log_fp);
    }
}

void gbLogDumpBuffer( gbLogLevel level, unsigned char *buffer, unsigned int size )
{
    assert( buffer != NULL );
    assert( size > 0 );

	char *logline = alloca( size * 3 ),
		*p = &logline[0];
	unsigned char byte;
	unsigned int i;
	unsigned char prev_was_print = 1;

	for( i = 0; i < size; i++ )
    {
		byte = buffer[i];
		if( isprint(byte) )
        {
			sprintf( p, "%s%c", prev_was_print ? "" : " ", byte );
			p += prev_was_print ? 1 : 2;
			prev_was_print = 1;
		}
		else 
        {
			sprintf( p, " %02X", byte );
			p += 3;
			prev_was_print = 0;
		}
	}

	gbLog( level, "%s", logline );
}

void gbLogFinalize()
{
	if( __log_fp ){
		fflush(__log_fp);
		fclose(__log_fp);
	}
}
