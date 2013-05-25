/*
 * Copyright (c) 2013, Simone Margaritelli <evilsocket at gmail dot com>
 *
 * Based on Redis network library by Salvatore Sanfilippo.
 *
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
#ifndef __NET_H__
#	define __NET_H__

#include <time.h>
#include <sys/stat.h>
#include "atree.h"
#include "llist.h"
#include "mem.h"
#include "default.h"

#if defined(__sun)
#define AF_LOCAL AF_UNIX
#endif

#define GB_OK 0
#define GB_ERR -1

#define GB_NONE 0
#define GB_READABLE 1
#define GB_WRITABLE 2

#define GB_FILE_EVENTS 1
#define GB_TIME_EVENTS 2
#define GB_ALL_EVENTS (GB_FILE_EVENTS|GB_TIME_EVENTS)
#define GB_DONT_WAIT 4

#define GB_NOMORE -1

/* Macros */
#define GB_NOTUSED(V) ((void) V)

#define GBNET_OK 0
#define GBNET_ERR -1
#define GBNET_ERR_LEN 256

typedef unsigned char byte_t;

struct gbEventLoop;

/* Types and data structures */
typedef void gbFileProc(struct gbEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef int  gbTimeProc(struct gbEventLoop *eventLoop, long long id, void *clientData);
typedef void gbEventFinalizerProc(struct gbEventLoop *eventLoop, void *clientData);
typedef void gbBeforeSleepProc(struct gbEventLoop *eventLoop);

/* File event structure */
typedef struct gbFileEvent
{
    int mask; /* one of AE_(READABLE|WRITABLE) */
    gbFileProc *rfileProc;
    gbFileProc *wfileProc;
    void *clientData;
}
gbFileEvent;

/* Time event structure */
typedef struct gbTimeEvent
{
    long long id; /* time event identifier. */
    long when_sec; /* seconds */
    long when_ms; /* milliseconds */
    gbTimeProc *timeProc;
    gbEventFinalizerProc *finalizerProc;
    void *clientData;
    struct gbTimeEvent *next;
}
gbTimeEvent;

/* A fired event */
typedef struct gbFiredEvent
{
    int fd;
    int mask;
}
gbFiredEvent;

/* State of an event based program */
typedef struct gbEventLoop
{
    int maxfd;   /* highest file descriptor currently registered */
    int setsize; /* max number of file descriptors tracked */
    long long timeEventNextId;
    time_t lastTime;     /* Used to detect system clock skew */
    gbFileEvent *events; /* Registered events */
    gbFiredEvent *fired; /* Fired events */
    gbTimeEvent *timeEventHead;
    int stop;
    void *apidata; /* This is used for polling API specific data */
    gbBeforeSleepProc *beforesleep;
}
gbEventLoop;

typedef enum
{
	UNIX = 0x00,
	TCP
}
gbServerType;

typedef struct gbServer
{
	// the main event loop structure
	gbEventLoop *events;
	// error buffer
	char 	 error[0xFF];
	// tcp port to use
	int 	 port;
	// tcp address or unix socket path to use
	char     address[0xFF];
	// TCP or UNIX
	gbServerType type;
	// 1 if we're running in daemon mode, otherwise 0
	byte_t	 daemon;
	// path of the pidfile
	const char *pidfile;
	// the main object container
	atree_t  tree;
	// server main file descriptor
	int 	 fd;
	// time the server was started
	time_t   started;
	// server time updated every cron loop
	time_t 	 time;
	// time of the first created object
	time_t   firstin;
	// time of the last created object
	time_t	 lastin;
	// list of currently connected clients
	llist_t *clients;
	// number total of items stored in the container
	unsigned int nitems;
	// number of compressed items
	unsigned int ncompressed;
	// number of currently connected clients
	unsigned int nclients;
	// period in milliseconds of the cron loop
	unsigned int cronperiod;
	// number of cron loops performed
	unsigned int crondone;
	// maximum number of connected clients
	unsigned int maxclients;
	// maximum number of seconds a client can stay idle before being disconnected
	time_t 	 maxidletime;
	// number of milliseconds to check for dead idle clients
	time_t   idlecron;
	// maximum size of a request
	size_t	 maxrequestsize;
	// maximum number of seconds of an item TTL
	size_t   maxitemttl;
	// data bigger then this is going to be compressed
	unsigned long compression;
	// buffer used for lzf (de)compression, alloc'd only once
	byte_t *lzf_buffer;
	// static lists used for multi-* operands
	llist_t *m_keys;
	llist_t *m_values;
	// buffer used to send multi get responses
	byte_t *m_buffer;
	// maximum size of an item key
	unsigned long maxkeysize;
	// maximum size of an item value
	unsigned long maxvaluesize;
	// maximum size of a response
	unsigned long maxresponsesize;
	// currently used memory
	unsigned long memused;
	// maximum size of used memory
	unsigned long maxmem;
	// cron timed event id
	long long cron_id;
	// number of seconds less than an object will be freed if memused >= maxmem
	time_t	 gcdelta;
	// flag to say the server to shutdown ASAP
	int		 shutdown;
	// plain configuration instance
	atree_t	 config;
}
gbServer;

typedef struct gbClient
{
	// main client file descriptor
	int		  fd;
	// client request/response buffer
	byte_t   *buffer;
	// client request/response buffer size
	int		  buffer_size;
	// number of bytes currently read
	int		  read;
	// number of bytes currently wrote
	int 	  wrote;
	// last time this client was seen alive
	time_t    seen;
	// pointer to the main server structure
	gbServer *server;
	// flag to make the client disconnect after the next I/O operation
	byte_t	  shutdown;
}
gbClient;

typedef enum
{
	// the item is in plain encoding and data points to its buffer
	PLAIN  = 0x00,
	// PLAIN but compressed data with lzf
	COMPRESSED,
	// the item contains a number and data pointer is actually that number
	NUMBER
}
gbItemEncoding;

typedef struct
{
	// the item buffer
	void  		  *data;
	// the item buffer size
	size_t 		   size;
	// the item encoding
	gbItemEncoding encoding;
	// time the item was created
	time_t		   time;
	// TTL of this item
	int			   ttl;
	// flag to lock the item
	int			   lock;
}
gbItem;

gbEventLoop *gbCreateEventLoop(int setsize);
void gbDeleteEventLoop(gbEventLoop *eventLoop);
void gbStopEventLoop(gbEventLoop *eventLoop);
int gbCreateFileEvent(gbEventLoop *eventLoop, int fd, int mask,gbFileProc *proc, void *clientData);
void gbDeleteFileEvent(gbEventLoop *eventLoop, int fd, int mask);
int gbGetFileEvents(gbEventLoop *eventLoop, int fd);
long long gbCreateTimeEvent(gbEventLoop *eventLoop, long long milliseconds,gbTimeProc *proc, void *clientData,gbEventFinalizerProc *finalizerProc);
int gbDeleteTimeEvent(gbEventLoop *eventLoop, long long id);
int gbProcessEvents(gbEventLoop *eventLoop, int flags);
int gbWaitEvents(int fd, int mask, long long milliseconds);
void gbEventLoopMain(gbEventLoop *eventLoop);
char *gbGetEventApiName(void);
void gbSetBeforeSleepProc(gbEventLoop *eventLoop, gbBeforeSleepProc *beforesleep);

int gbNetTcpConnect(char *err, char *addr, int port);
int gbNetTcpNonBlockConnect(char *err, char *addr, int port);
int gbNetUnixConnect(char *err, char *path);
int gbNetUnixNonBlockConnect(char *err, char *path);
int gbNetRead(int fd, char *buf, int count);
int gbNetResolve(char *err, char *host, char *ipbuf);
int gbNetTcpServer(char *err, int port, char *bindaddr);
int gbNetUnixServer(char *err, char *path, mode_t perm);
int gbNetTcpAccept(char *err, int serversock, char *ip, int *port);
int gbNetUnixAccept(char *err, int serversock);
int gbNetWrite(int fd, char *buf, int count);
int gbNetNonBlock(char *err, int fd);
int gbNetEnableTcpNoDelay(char *err, int fd);
int gbNetDisableTcpNoDelay(char *err, int fd);
int gbNetTcpKeepAlive(char *err, int fd);
int gbNetPeerToString(int fd, char *ip, int *port);
int gbNetKeepAlive(char *err, int fd, int interval);

void gbServerFormatUptime( gbServer *server, char *s );

gbClient *gbClientCreate( int fd, gbServer *server );
void      gbClientReset( gbClient *client );
int 	  gbClientEnqueueData( gbClient *client, short code, byte_t *reply, size_t size, gbFileProc *proc, short shutdown );
int       gbClientEnqueueCode( gbClient *client, short code, gbFileProc, short shutdown );
int		  gbClientEnqueueItem( gbClient *client, short code, gbItem *item, gbFileProc *proc, short shutdown );
int		  gbClientEnqueueKeyValueSet( gbClient *client, size_t elements, gbFileProc *proc, short shutdown );
void	  gbClientDestroy( gbClient *client );

#endif
