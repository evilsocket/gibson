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
	gbEventLoop *events;
	char 	 error[0xFF];
	int 	 port;
	char     address[0xFF];
	gbServerType type;
	byte_t	 daemon;
	char	*pidfile;
	atree_t  tree;
	int 	 fd;
	time_t 	 time;
	time_t   firstin;
	time_t	 lastin;
	llist_t *clients;
	uint     nitems;
	uint 	 nclients;
	uint	 cronperiod;
	uint	 crondone;
	uint 	 maxclients;
	time_t 	 maxidletime;
	size_t	 maxrequestsize;
	size_t   maxitemttl;
	unsigned long memused;
	unsigned long maxmem;
	time_t	 freeolderthan;
	int		 shutdown;
}
gbServer;

typedef struct gbClient
{
	int		  fd;
	byte_t   *buffer;
	int		  buffer_size;
	int		  read;
	int 	  wrote;
	time_t    seen;
	gbServer *server;
	byte_t	  shutdown;
}
gbClient;

typedef enum
{
	PLAIN  = 0x00,
	NUMBER
}
gbItemEncoding;

typedef struct
{
	void  		  *data;
	size_t 		   size;
	gbItemEncoding encoding;
	time_t		   time;
	int			   ttl;
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


gbClient *gbClientCreate( int fd, gbServer *server );
void      gbClientReset( gbClient *client );
int 	  gbClientEnqueueData( gbClient *client, short code, byte_t *reply, size_t size, gbFileProc *proc, short shutdown );
int       gbClientEnqueueCode( gbClient *client, short code, gbFileProc, short shutdown );
int		  gbClientEnqueueItem( gbClient *client, short code, gbItem *item, gbFileProc *proc, short shutdown );
void	  gbClientDestroy( gbClient *client );

#endif
