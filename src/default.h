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
#ifndef __DEFAULT_H__
#define __DEFAULT_H__

#define GB_DEFAULT_CONFIGURATION			  "/etc/gibson/gibson.conf"

#define GB_DEAFULT_LOG_FILE					  "/dev/stdout"
#define GB_DEFAULT_LOG_LEVEL				  INFO
#define GB_DEFAULT_LOG_FLUSH_LEVEL			  1

#define GB_DEFAULT_UNIX_SOCKET				  "/var/run/gibson.sock"
#define GB_DEFAULT_ADDRESS					  "127.0.0.1"
#define GB_DEFAULT_PORT						  10128

#define GB_DEFAULT_PID_FILE					  "/var/run/gibson.pid"

#define GBNET_DEFAULT_MAX_CLIENTS			  1024
#define GBNET_DEFAULT_MAX_REQUEST_BUFFER_SIZE 4096 * 1024
#define GBNET_DEFAULT_MAX_IDLE_TIME			  1

#define GB_DEFAULT_MAX_ITEM_TTL 			  2592000

#define GB_DEFAULT_MAX_MEMORY  				  2147483648
#define GB_DEFAULT_MAX_QUERY_KEY_SIZE   	  512
#define GB_DEFAULT_MAX_QUERY_VALUE_SIZE       4096

#define GB_DEFAULT_COMPRESSION				  40960

#define GB_DEFAULT_CRON_PERIOD 				  100

#endif
