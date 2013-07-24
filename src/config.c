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
#include <ctype.h>
#include <time.h>
#include "config.h"

void gbConfigLoad( atree_t *config, char *filename ){
	FILE *fp = fopen( filename, "rt" );

	if( fp ){
		at_init_tree( *config );

		char line[0xFF] = {0},
			 key[0xFF]  = {0},
			 value[0xFF] = {0},
			*p = NULL,
			*pd = NULL;
		size_t lineno = 0;

		while( !feof(fp) ){
			if( fgets( line, 0xFF, fp ) ){
				++lineno;

				// skip comments
				if( line[0] == '#' )
					continue;

				p = &line[0];

				// eat spaces
				while( isspace( *p ) && *p ) ++p;

				// skip empty lines
				if( *p == 0x00 )
					continue;

				memset( key,   0x00, 0xFF );
				memset( value, 0x00, 0xFF );

				// read key
				pd = &key[0];
				while( !isspace( *p ) && *p ) *pd++ = *p++;

				// eat spaces
				while( isspace( *p ) && *p ) ++p;

				if( *p == 0x00 ){
					fclose( fp );
					printf( "Error on line %zu of %s, unexpected end of line.\n", lineno, filename );
					exit( 1 );
				}

				// read value
				pd = &value[0];
				while( !isspace( *p ) && *p ) *pd++ = *p++;

				at_insert( config, (unsigned char *)key, strlen(key), zstrdup( value ) );
			}
		}

		fclose ( fp );
	}
	else{
		perror( filename );
		exit( 1 );
	}
}

void gbConfigMerge( atree_t *config, char *skip, struct option *options, int argc, char **argv ){
    int c = 0, oindex = 0;
    char *key;

    while( ( c = getopt_long( argc, argv, skip, options, &oindex ) ) != -1 ){
        if( c && strchr( skip, c ) ){
            continue;
        }
        else if( oindex >= 0 && optarg != NULL ){
            key = (char *)options[oindex].name;

            at_insert( config, (unsigned char *)key, strlen(key), zstrdup(optarg) ); 
        }
    } 
}

int gbConfigReadInt( atree_t *config, const char *key, int def ){
	char *value = at_find( config, (unsigned char *)key, strlen( key ) );
	if( value ){
	    char * p;
	    long l = strtol( value, &p, 10 );

	    return ( *p == '\0' ? (int)l : def );
	}

	return def;
}

unsigned long gbConfigReadSize( atree_t *config, const char *key, unsigned long def ){
	char *value = at_find( config, (unsigned char *)key, strlen( key ) );
	if( value ){
		size_t len = strlen(value);
		char unit = value[len - 1];
		long mul = 0;

		if( unit == 'B' || unit == 'b' )
			mul = 1;

		else if( unit == 'K' || unit == 'k' )
			mul = 1024;

		else if( unit == 'M' || unit == 'm' )
			mul = 1024 * 1024;

		else if( unit == 'G' || unit == 'g' )
			mul = 1024 * 1024 * 1024;

		if( mul )
			value[ len - 1 ] = 0x00;
        else
            mul = 1;

		char * p;
		long l = strtol( value, &p, 10 );

		return ( *p == '\0' ? l * mul : def );
	}

	return def;
}

time_t gbConfigReadTime( atree_t *config, const char *key, time_t def ){
    char *value = at_find( config, (unsigned char *)key, strlen( key ) );
	if( value ){
		size_t len = strlen(value);
		char unit = value[len - 1];
		long mul = 0;

		if( unit == 's' || unit == 'S' )
			mul = 1;

		else if( unit == 'm' || unit == 'M' )
			mul = 60;

		else if( unit == 'h' || unit == 'H' )
			mul = 60 * 60;

		else if( unit == 'd' || unit == 'd' )
			mul = 60 * 60 * 24;

		if( mul )
			value[ len - 1 ] = 0x00;
        else
            mul = 1;

		char * p;
		time_t l = (time_t)strtol( value, &p, 10 );

		return ( *p == '\0' ? l * mul : def );
	}

	return def;
}

const char *gbConfigReadString( atree_t *config, const char *key, const char *def ){
	char *value = at_find( config, (unsigned char *)key, strlen( key ) );

	return value ? value : def;
}
