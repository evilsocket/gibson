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
#include "configure.h"
#include "trie.h"
#include "net.h"
#include "lzf.h"
#include "log.h"
#include "query.h"
#include "endianness.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>

/* Include the best multiplexing layer supported by this system.
 * The following should be ordered by performances, descending. */
#ifdef HAVE_EVPORT
#include "mux/evport.c"
#else
#ifdef HAVE_EPOLL
#include "mux/epoll.c"
#else
#ifdef HAVE_KQUEUE
#include "mux/kqueue.c"
#else
#include "mux/select.c"
#endif
#endif
#endif

gbEventLoop *gbCreateEventLoop(int setsize)
{
    assert( setsize > 0 );

    gbEventLoop *eventLoop;
    int i;

    if ((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;
    eventLoop->events = zmalloc(sizeof(gbFileEvent)*setsize);
    eventLoop->fired = zmalloc(sizeof(gbFiredEvent)*setsize);
    if (eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
    eventLoop->setsize = setsize;
    eventLoop->lastTime = time(NULL);
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 0;
    eventLoop->stop = 0;
    eventLoop->maxfd = -1;
    eventLoop->beforesleep = NULL;
    if (aeApiCreate(eventLoop) == -1) goto err;
    /* Events with mask == GB_NONE are not set. So let's initialize the
     * vector with it. */
    for (i = 0; i < setsize; i++)
        eventLoop->events[i].mask = GB_NONE;
    return eventLoop;

err:

    if (eventLoop)
    {
        zfree(eventLoop->events);
        zfree(eventLoop->fired);
        zfree(eventLoop);
    }

    assert(0);

    return NULL;
}

int gbGetSetSize(gbEventLoop *eventLoop)
{
    assert( eventLoop != NULL );

    return eventLoop->setsize;
}

/* Resize the maximum set size of the event loop.
 * If the requested set size is smaller than the current set size, but
 * there is already a file descriptor in use that is >= the requested
 * set size minus one, GB_ERR is returned and the operation is not
 * performed at all.
 *
 * Otherwise GB_OK is returned and the operation is successful. */
int gbResizeSetSize(gbEventLoop *eventLoop, int setsize)
{
    assert( eventLoop != NULL );
    assert( setsize != 0 );
        
    int i;

    if( setsize == eventLoop->setsize )
        return GB_OK;
    else if( eventLoop->maxfd >= setsize )
        return GB_ERR;
    else if( aeApiResize(eventLoop,setsize) == -1 )
        return GB_ERR;

    eventLoop->events  = zrealloc( eventLoop->events, sizeof(gbFileEvent) * setsize );
    eventLoop->fired   = zrealloc( eventLoop->fired, sizeof(gbFiredEvent) * setsize );
    eventLoop->setsize = setsize;

    assert( eventLoop->events != NULL );
    assert( eventLoop->fired != NULL );

    /* Make sure that if we created new slots, they are initialized with
     * an AE_NONE mask. */
    for (i = eventLoop->maxfd+1; i < setsize; i++)
        eventLoop->events[i].mask = GB_NONE;

    return GB_OK;
}

void gbDeleteEventLoop(gbEventLoop *eventLoop)
{
    assert( eventLoop != NULL );
    assert( eventLoop->events != NULL );
    assert( eventLoop->fired != NULL );

    aeApiFree(eventLoop);
    zfree(eventLoop->events);
    zfree(eventLoop->fired);
    zfree(eventLoop);
}

void gbStopEventLoop(gbEventLoop *eventLoop)
{
    assert( eventLoop != NULL );

    eventLoop->stop = 1;
}

int gbCreateFileEvent(gbEventLoop *eventLoop, int fd, int mask, gbFileProc *proc, void *clientData)
{
    assert( eventLoop != NULL );

    if (fd >= eventLoop->setsize)
    {
        errno = ERANGE;
        return GB_ERR;
    }
    gbFileEvent *fe = &eventLoop->events[fd];

    if (aeApiAddEvent(eventLoop, fd, mask) == -1)
        return GB_ERR;
    fe->mask |= mask;
    if (mask & GB_READABLE) fe->rfileProc = proc;
    if (mask & GB_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if (fd > eventLoop->maxfd)
        eventLoop->maxfd = fd;
    return GB_OK;
}

void gbDeleteFileEvent(gbEventLoop *eventLoop, int fd, int mask)
{
    assert( eventLoop != NULL );

    if (fd >= eventLoop->setsize) return;
    gbFileEvent *fe = &eventLoop->events[fd];

    if (fe->mask == GB_NONE) return;
    fe->mask = fe->mask & (~mask);
    if (fd == eventLoop->maxfd && fe->mask == GB_NONE)
    {
        /* Update the max fd */
        int j;

        for (j = eventLoop->maxfd-1; j >= 0; --j )
            if (eventLoop->events[j].mask != GB_NONE) break;
        eventLoop->maxfd = j;
    }
    aeApiDelEvent(eventLoop, fd, mask);
}

int gbGetFileEvents(gbEventLoop *eventLoop, int fd)
{
    assert( eventLoop != NULL );

    if (fd >= eventLoop->setsize) return 0;
    gbFileEvent *fe = &eventLoop->events[fd];

    return fe->mask;
}

static void gbGetTime(long *seconds, long *milliseconds)
{
    assert( seconds != NULL );
    assert( milliseconds != NULL );

    /*
     * On FreeBSD calling gettimeofday() causes all the cores on a multicore
     * system to be synchronized. On a heavily loaded system with a
     * significantly CPU bound application this can cause a 40% overall
     * system degradation.
     *
     * A better option is to use clock_gettime() and pass in the
     * CLOCK_REALTIME_FAST clock as the clock to use. This achieves the
     * same behavior as gettimeofday() on Linux.
     */
#if defined(__FreeBSD__)
#include <sys/time.h>
    struct timespec ts;
    int r = clock_gettime(CLOCK_REALTIME_FAST, &ts);

    *seconds = ts.tv_sec;
    *milliseconds = ts.tv_nsec/1000000;
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);

    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec/1000;
#endif
}

static void gbAddMillisecondsToNow(long long milliseconds, long *sec, long *ms)
{
    assert( sec != NULL );
    assert( ms != NULL );

    long cur_sec, cur_ms, when_sec, when_ms;

    gbGetTime(&cur_sec, &cur_ms);
    when_sec = cur_sec + milliseconds/1000;
    when_ms = cur_ms + milliseconds%1000;
    if (when_ms >= 1000)
    {
        ++when_sec;
        when_ms -= 1000;
    }
    *sec = when_sec;
    *ms = when_ms;
}

long long gbCreateTimeEvent(gbEventLoop *eventLoop, long long milliseconds,
        gbTimeProc *proc, void *clientData,
        gbEventFinalizerProc *finalizerProc)
{
    assert( eventLoop != NULL );

    long long id = eventLoop->timeEventNextId++;
    gbTimeEvent *te;

    te = zmalloc(sizeof(*te));
    if (te == NULL) return GB_ERR;
    te->id = id;
    gbAddMillisecondsToNow(milliseconds,&te->when_sec,&te->when_ms);
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;
    te->next = eventLoop->timeEventHead;
    eventLoop->timeEventHead = te;
    return id;
}

int gbDeleteTimeEvent(gbEventLoop *eventLoop, long long id)
{
    assert( eventLoop != NULL );

    gbTimeEvent *te, *prev = NULL;

    te = eventLoop->timeEventHead;
    while(te)
    {
        if (te->id == id)
        {
            if (prev == NULL)
                eventLoop->timeEventHead = te->next;
            else
                prev->next = te->next;
            if (te->finalizerProc)
                te->finalizerProc(eventLoop, te->clientData);
            zfree(te);
            return GB_OK;
        }
        prev = te;
        te = te->next;
    }
    return GB_ERR; /* NO event with the specified ID found */
}

/* Search the first timer to fire.
 * This operation is useful to know how many time the select can be
 * put in sleep without to delay any event.
 * If there are no timers NULL is returned.
 */
static gbTimeEvent *aeSearchNearestTimer(gbEventLoop *eventLoop)
{
    assert( eventLoop != NULL );

    gbTimeEvent *te = eventLoop->timeEventHead;
    gbTimeEvent *nearest = te;

    while(te)
    {
        if( te->when_sec < nearest->when_sec || 
                ( te->when_sec == nearest->when_sec && te->when_ms < nearest->when_ms) )
        {
            nearest = te;
        }

        te = te->next;
    }

    return nearest;
}

/* Process time events */
static int processTimeEvents(gbEventLoop *eventLoop)
{
    assert( eventLoop != NULL );

    int processed = 0;
    gbTimeEvent *te;
    long long maxId;
    time_t now = time(NULL);

    /* If the system clock is moved to the future, and then set back to the
     * right value, time events may be delayed in a random way. Often this
     * means that scheduled operations will not be performed soon enough.
     *
     * Here we try to detect system clock skews, and force all the time
     * events to be processed ASAP when this happens: the idea is that
     * processing events earlier is less dangerous than delaying them
     * indefinitely, and practice suggests it is. */
    if (now < eventLoop->lastTime)
    {
        te = eventLoop->timeEventHead;
        while(te)
        {
            te->when_sec = 0;
            te = te->next;
        }
    }
    eventLoop->lastTime = now;

    te = eventLoop->timeEventHead;
    maxId = eventLoop->timeEventNextId-1;
    while(te)
    {
        long now_sec, now_ms;
        long long id;

        if (te->id > maxId)
        {
            te = te->next;
            continue;
        }
        gbGetTime(&now_sec, &now_ms);
        if (now_sec > te->when_sec ||
                (now_sec == te->when_sec && now_ms >= te->when_ms))
        {
            int retval;

            id = te->id;
            retval = te->timeProc(eventLoop, id, te->clientData);
            ++processed;
            /* After an event is processed our time event list may
             * no longer be the same, so we restart from head.
             * Still we make sure to don't process events registered
             * by event handlers itself in order to don't loop forever.
             * To do so we saved the max ID we want to handle.
             *
             * FUTURE OPTIMIZATIONS:
             * Note that this is NOT great algorithmically. Gibson uses
             * a single time event so it's not a problem but the right
             * way to do this is to add the new elements on head, and
             * to flag deleted elements in a special way for later
             * deletion (putting references to the nodes to delete into
             * another linked list). */
            if (retval != GB_NOMORE)
            {
                gbAddMillisecondsToNow(retval,&te->when_sec,&te->when_ms);
            }
            else
            {
                gbDeleteTimeEvent(eventLoop, id);
            }
            te = eventLoop->timeEventHead;
        }
        else
        {
            te = te->next;
        }
    }
    return processed;
}

/* Process every pending time event, then every pending file event
 * (that may be registered by time event callbacks just processed).
 * Without special flags the function sleeps until some file event
 * fires, or when the next time event occurs (if any).
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has GB_ALL_EVENTS set, all the kind of events are processed.
 * if flags has GB_FILE_EVENTS set, file events are processed.
 * if flags has GB_TIME_EVENTS set, time events are processed.
 * if flags has GB_DONT_WAIT set the function returns ASAP until all
 * the events that's possible to process without to wait are processed.
 *
 * The function returns the number of events processed. */
int gbProcessEvents(gbEventLoop *eventLoop, int flags)
{
    assert( eventLoop != NULL );

    int processed = 0, 
        time_events = ( flags & GB_TIME_EVENTS ),
        file_events = ( flags & GB_FILE_EVENTS ),
        dont_wait   = ( flags & GB_DONT_WAIT ),
        numevents;

    /* Nothing to do? return ASAP */
    if (!time_events && !file_events) 
        return 0;

    /* Note that we want to poll even if there are no
     * file events to process as long as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */
    if ( eventLoop->maxfd != -1 || ( time_events && !dont_wait ) ) 
    {
        int j;
        gbTimeEvent *shortest = NULL;
        struct timeval tv, *tvp;

        if ( time_events && !dont_wait )
            shortest = aeSearchNearestTimer(eventLoop);

        if (shortest)
        {
            long now_sec, now_ms;

            // Compute the time missing for the nearest timer to fire.
            gbGetTime(&now_sec, &now_ms);
            tvp = &tv;
            tvp->tv_sec = shortest->when_sec - now_sec;

            if (shortest->when_ms < now_ms)
            {
                tvp->tv_usec = ((shortest->when_ms+1000) - now_ms)*1000;
                tvp->tv_sec --;
            }
            else
            {
                tvp->tv_usec = (shortest->when_ms - now_ms)*1000;
            }

            if (tvp->tv_sec < 0) tvp->tv_sec = 0;
            if (tvp->tv_usec < 0) tvp->tv_usec = 0;
        } 
        // no time events scheduled
        else
        {
            /* If we have to check for events but need to return
             * ASAP because of GB_DONT_WAIT we need to set the timeout
             * to zero */
            if (dont_wait)
            {
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;
            }
            else
            {
                /* Otherwise we can block */
                tvp = NULL; /* wait forever */
            }
        }

        numevents = aeApiPoll(eventLoop, tvp);
        for (j = 0; j < numevents; j++)
        {
            gbFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];
            int mask = eventLoop->fired[j].mask;
            int fd = eventLoop->fired[j].fd;
            int rfired = 0;

            /* note the fe->mask & mask & ... code: maybe an already processed
             * event removed an element that fired and we still didn't
             * processed, so we check if the event is still valid. */
            if ( fe->mask && mask & GB_READABLE)
            {
                rfired = 1;
                fe->rfileProc(eventLoop,fd,fe->clientData,mask);
            }
            if ( fe->mask && mask & GB_WRITABLE)
            {
                if (!rfired || fe->wfileProc != fe->rfileProc)
                    fe->wfileProc(eventLoop,fd,fe->clientData,mask);
            }

            ++processed;
        }
    }
    /* Check time events */
    if (flags & GB_TIME_EVENTS)
        processed += processTimeEvents(eventLoop);

    return processed; /* return the number of processed file/time events */
}

/* Wait for milliseconds until the given file descriptor becomes
 * writable/readable/exception */
int gbWaitEvents(int fd, int mask, long long milliseconds)
{
    struct pollfd pfd;
    int retmask = 0, retval;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    if (mask & GB_READABLE) pfd.events |= POLLIN;
    if (mask & GB_WRITABLE) pfd.events |= POLLOUT;

    if ((retval = poll(&pfd, 1, milliseconds))== 1)
    {
        if (pfd.revents & POLLIN) retmask |= GB_READABLE;
        if (pfd.revents & POLLOUT) retmask |= GB_WRITABLE;
        if (pfd.revents & POLLERR) retmask |= GB_WRITABLE;
        if (pfd.revents & POLLHUP) retmask |= GB_WRITABLE;
        return retmask;
    }
    else
    {
        return retval;
    }
}

void gbEventLoopMain(gbEventLoop *eventLoop)
{
    assert( eventLoop != NULL );

    eventLoop->stop = 0;
    while (!eventLoop->stop)
    {
        if (eventLoop->beforesleep != NULL)
            eventLoop->beforesleep(eventLoop);
        gbProcessEvents(eventLoop, GB_ALL_EVENTS);
    }
}

char *gbGetEventApiName(void)
{
    return aeApiName();
}

void gbSetBeforeSleepProc(gbEventLoop *eventLoop, gbBeforeSleepProc *beforesleep)
{
    assert( eventLoop != NULL );

    eventLoop->beforesleep = beforesleep;
}


static void gbNetSetError(char *err, const char *fmt, ...)
{
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, GBNET_ERR_LEN, fmt, ap);
    va_end(ap);
}

int gbNetNonBlock(char *err, int fd)
{
    int flags;

    /* Set the socket non-blocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(fd, F_GETFL)) == -1)
    {
        gbNetSetError(err, "fcntl(F_GETFL): %s", strerror(errno));
        return GBNET_ERR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        gbNetSetError(err, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
        return GBNET_ERR;
    }
    return GBNET_OK;
}

/* Set TCP keep alive option to detect dead peers. The interval option
 * is only used for Linux as we are using Linux-specific APIs to set
 * the probe send time, interval, and count. */
int gbNetKeepAlive(char *err, int fd, int interval)
{
    int val = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1)
    {
        gbNetSetError(err, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
        return GBNET_ERR;
    }

#ifdef __linux__
    /* Default settings are more or less garbage, with the keepalive time
     * set to 7200 by default on Linux. Modify settings to make the feature
     * actually useful. */

    /* Send first probe after interval. */
    val = interval;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0)
    {
        gbNetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
        return GBNET_ERR;
    }

    /* Send next probes after the specified interval. Note that we set the
     * delay as interval / 3, as we send three probes before detecting
     * an error (see the next setsockopt call). */
    val = interval/3;
    if (val == 0) val = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0)
    {
        gbNetSetError(err, "setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
        return GBNET_ERR;
    }

    /* Consider the socket in error state after three we send three ACK
     * probes without getting a reply. */
    val = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0)
    {
        gbNetSetError(err, "setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
        return GBNET_ERR;
    }
#endif

    return GBNET_OK;
}

static int gbNetSetTcpNoDelay(char *err, int fd, int val)
{
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) == -1)
    {
        gbNetSetError(err, "setsockopt TCP_NODELAY: %s", strerror(errno));
        return GBNET_ERR;
    }
    return GBNET_OK;
}

int gbNetEnableTcpNoDelay(char *err, int fd)
{
    return gbNetSetTcpNoDelay(err, fd, 1);
}

int gbNetDisableTcpNoDelay(char *err, int fd)
{
    return gbNetSetTcpNoDelay(err, fd, 0);
}

int gbNetSetSendBuffer(char *err, int fd, int buffsize)
{
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffsize, sizeof(buffsize)) == -1)
    {
        gbNetSetError(err, "setsockopt SO_SNDBUF: %s", strerror(errno));
        return GBNET_ERR;
    }
    return GBNET_OK;
}

int gbNetTcpKeepAlive(char *err, int fd)
{
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) == -1)
    {
        gbNetSetError(err, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
        return GBNET_ERR;
    }
    return GBNET_OK;
}

int gbNetResolve(char *err, char *host, char *ipbuf)
{
    struct sockaddr_in sa;

    sa.sin_family = AF_INET;
    if (inet_aton(host, &sa.sin_addr) == 0)
    {
        struct hostent *he;

        he = gethostbyname(host);
        if (he == NULL)
        {
            gbNetSetError(err, "can't resolve: %s", host);
            return GBNET_ERR;
        }
        memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
    }
    strcpy(ipbuf,inet_ntoa(sa.sin_addr));
    return GBNET_OK;
}

static int gbNetCreateSocket(char *err, int domain)
{
    int s, on = 1;
    if ((s = socket(domain, SOCK_STREAM, 0)) == -1)
    {
        gbNetSetError(err, "creating socket: %s", strerror(errno));
        return GBNET_ERR;
    }

    /* Make sure connection-intensive things will be able to close/open
       sockets a zillion of times */
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
    {
        gbNetSetError(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return GBNET_ERR;
    }
    return s;
}

#define GBNET_CONNECT_NONE 0
#define GBNET_CONNECT_NONBLOCK 1
static int gbNetTcpGenericConnect(char *err, char *addr, int port, int flags)
{
    int s;
    struct sockaddr_in sa;

    if ((s = gbNetCreateSocket(err,AF_INET)) == GBNET_ERR)
        return GBNET_ERR;

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_aton(addr, &sa.sin_addr) == 0)
    {
        struct hostent *he;

        he = gethostbyname(addr);
        if (he == NULL)
        {
            gbNetSetError(err, "can't resolve: %s", addr);
            close(s);
            return GBNET_ERR;
        }
        memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
    }
    if (flags & GBNET_CONNECT_NONBLOCK)
    {
        if (gbNetNonBlock(err,s) != GBNET_OK)
            return GBNET_ERR;
    }
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1)
    {
        if (errno == EINPROGRESS && ( flags & GBNET_CONNECT_NONBLOCK ) )
            return s;

        gbNetSetError(err, "connect: %s", strerror(errno));
        close(s);
        return GBNET_ERR;
    }
    return s;
}

int gbNetTcpConnect(char *err, char *addr, int port)
{
    return gbNetTcpGenericConnect(err,addr,port,GBNET_CONNECT_NONE);
}

int gbNetTcpNonBlockConnect(char *err, char *addr, int port)
{
    return gbNetTcpGenericConnect(err,addr,port,GBNET_CONNECT_NONBLOCK);
}

int gbNetUnixGenericConnect(char *err, char *path, int flags)
{
    int s;
    struct sockaddr_un sa;

    if ((s = gbNetCreateSocket(err,AF_LOCAL)) == GBNET_ERR)
        return GBNET_ERR;

    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path,path,sizeof(sa.sun_path)-1);
    if (flags & GBNET_CONNECT_NONBLOCK)
    {
        if (gbNetNonBlock(err,s) != GBNET_OK)
            return GBNET_ERR;
    }
    if (connect(s,(struct sockaddr*)&sa,sizeof(sa)) == -1)
    {
        if (errno == EINPROGRESS && ( flags & GBNET_CONNECT_NONBLOCK ) )
            return s;

        gbNetSetError(err, "connect: %s", strerror(errno));
        close(s);
        return GBNET_ERR;
    }
    return s;
}

int gbNetUnixConnect(char *err, char *path)
{
    return gbNetUnixGenericConnect(err,path,GBNET_CONNECT_NONE);
}

int gbNetUnixNonBlockConnect(char *err, char *path)
{
    return gbNetUnixGenericConnect(err,path,GBNET_CONNECT_NONBLOCK);
}

/* Like read(2) but make sure 'count' is read before to return
 * (unless error or EOF condition is encountered) */
int gbNetRead(int fd, char *buf, int count)
{
    int nread, totlen = 0;
    while(totlen != count)
    {
        nread = read(fd,buf,count-totlen);
        if (nread == 0) return totlen;
        if (nread == -1) return -1;
        totlen += nread;
        buf += nread;
    }
    return totlen;
}

/* Like write(2) but make sure 'count' is read before to return
 * (unless error is encountered) */
int gbNetWrite(int fd, char *buf, int count)
{
    int nwritten, totlen = 0;
    while(totlen != count)
    {
        nwritten = write(fd,buf,count-totlen);
        if (nwritten == 0) return totlen;
        if (nwritten == -1) return -1;
        totlen += nwritten;
        buf += nwritten;
    }
    return totlen;
}

static int gbNetListen(char *err, int s, struct sockaddr *sa, socklen_t len)
{
    assert( sa != NULL );

    if (bind(s,sa,len) == -1)
    {
        gbNetSetError(err, "bind: %s", strerror(errno));
        close(s);
        return GBNET_ERR;
    }

    /* Use a backlog of 512 entries. We pass 511 to the listen() call because
     * the kernel does: backlogsize = roundup_pow_of_two(backlogsize + 1);
     * which will thus give us a backlog of 512 entries */
    if (listen(s, 511) == -1)
    {
        gbNetSetError(err, "listen: %s", strerror(errno));
        close(s);
        return GBNET_ERR;
    }
    return GBNET_OK;
}

int gbNetTcpServer(char *err, int port, char *bindaddr)
{
    assert( bindaddr != NULL );

    int s;
    struct sockaddr_in sa;

    if ((s = gbNetCreateSocket(err,AF_INET)) == GBNET_ERR)
        return GBNET_ERR;

    memset(&sa,0,sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bindaddr && inet_aton(bindaddr, &sa.sin_addr) == 0)
    {
        gbNetSetError(err, "invalid bind address");
        close(s);
        return GBNET_ERR;
    }
    if (gbNetListen(err,s,(struct sockaddr*)&sa,sizeof(sa)) == GBNET_ERR)
        return GBNET_ERR;
    return s;
}

int gbNetUnixServer(char *err, char *path, mode_t perm)
{
    assert( path != NULL );

    int s;
    struct sockaddr_un sa;

    if ((s = gbNetCreateSocket(err,AF_LOCAL)) == GBNET_ERR)
        return GBNET_ERR;

    memset(&sa,0,sizeof(sa));
    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path,path,sizeof(sa.sun_path)-1);
    if (gbNetListen(err,s,(struct sockaddr*)&sa,sizeof(sa)) == GBNET_ERR)
        return GBNET_ERR;
    if (perm)
        chmod(sa.sun_path, perm);
    return s;
}

static int gbNetGenericAccept(char *err, int s, struct sockaddr *sa, socklen_t *len)
{
    assert( sa != NULL );
    assert( len != NULL );

    int fd;
    while(1)
    {
        fd = accept(s,sa,len);
        if (fd == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                gbNetSetError(err, "accept: %s", strerror(errno));
                return GBNET_ERR;
            }
        }
        break;
    }
    return fd;
}

int gbNetTcpAccept(char *err, int s, char *ip, int *port)
{
    int fd;
    struct sockaddr_in sa;
    socklen_t salen = sizeof(sa);
    if ((fd = gbNetGenericAccept(err,s,(struct sockaddr*)&sa,&salen)) == GBNET_ERR)
        return GBNET_ERR;

    if (ip) strcpy(ip,inet_ntoa(sa.sin_addr));
    if (port) *port = ntohs(sa.sin_port);
    return fd;
}

int gbNetUnixAccept(char *err, int s)
{
    int fd;
    struct sockaddr_un sa;
    socklen_t salen = sizeof(sa);
    if ((fd = gbNetGenericAccept(err,s,(struct sockaddr*)&sa,&salen)) == GBNET_ERR)
        return GBNET_ERR;

    return fd;
}

int gbNetPeerToString(int fd, char *ip, int *port)
{
    struct sockaddr_in sa;
    socklen_t salen = sizeof(sa);

    if (getpeername(fd,(struct sockaddr*)&sa,&salen) == -1)
    {
        *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return -1;
    }
    if (ip) strcpy(ip,inet_ntoa(sa.sin_addr));
    if (port) *port = ntohs(sa.sin_port);
    return 0;
}

int gbNetSockName(int fd, char *ip, int *port)
{
    struct sockaddr_in sa;
    socklen_t salen = sizeof(sa);

    if (getsockname(fd,(struct sockaddr*)&sa,&salen) == -1)
    {
        *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return -1;
    }
    if (ip) strcpy(ip,inet_ntoa(sa.sin_addr));
    if (port) *port = ntohs(sa.sin_port);
    return 0;
}

void gbServerFormatUptime( gbServer *server, char *s )
{
    assert( server != NULL );
    assert( s != NULL );

    int uptime = difftime( server->stats.time, server->stats.started );
    int days = 0,
        hours = 0,
        minutes = 0,
        seconds = 0;

    if( uptime >= 86400 )
    {
        days = (int)( uptime / 86400 );
        uptime %= 86400;
    }

    if( uptime >= 3600 )
    {
        hours = (int)( uptime / 3600 );
        uptime %= 3600;
    }

    if( uptime >= 60 )
    {
        minutes = (int)( uptime / 60 );
        uptime %= 60;
    }

    seconds = uptime;

    sprintf( s, "%dd %dh %dm %ds", days, hours, minutes, seconds );
}

gbClient* gbClientCreate( int fd, gbServer *server  )
{
    assert( server != NULL );

    gbClient *client = (gbClient *)zmalloc( sizeof( gbClient ) );

    assert( client != NULL );

    client->fd 			= fd;
    client->buffer 		= NULL;
    client->buffer_size = 0;
    client->status		= STATUS_WAITING_SIZE;
    client->read 		= 0;
    client->wrote 		= 0;
    client->server 		= server;
    client->shutdown 	= 0;

    ll_append( server->clients, client );

    ++server->stats.nclients;

    return client;
}

void gbClientReset( gbClient *client )
{
    assert( client != NULL );

    if( client->buffer != NULL )
    {
        zfree( client->buffer );
    }

    client->buffer      = NULL;
    client->buffer_size = 0;
    client->status		= STATUS_WAITING_SIZE;
    client->read 		= 0;
    client->wrote 		= 0;
    client->shutdown 	= 0;
}

void gbClientDestroy( gbClient *client )
{
    assert( client != NULL );
    assert( client->server != NULL );

    gbServer *server = client->server;

    if( client->buffer != NULL )
    {
        zfree( client->buffer );
        client->buffer = NULL;
    }

    if (client->fd != -1)
    {
        assert( server->events != NULL );

        gbDeleteFileEvent( server->events, client->fd, GB_READABLE );
        gbDeleteFileEvent( server->events, client->fd, GB_WRITABLE );
        close(client->fd);
    }

    ll_item_t *item = NULL;
    for( item = server->clients->head; item; item = item->next )
    {
        if( ll_data( gbClient *, item ) == client )
        {
            // do not free the list item itself
            item->data            = NULL;
            server->clients->free = item;
            break;
        }
    }

    --server->stats.nclients;

    zfree( client );
}

int gbClientEnqueueData( gbClient *client, short code, gbItemEncoding encoding, byte_t *reply, uint32_t size, gbFileProc *proc, short shutdown )
{
    assert( client != NULL );
    assert( reply != NULL );
    assert( size > 0 );

    if( client->fd <= 0 ) return GB_ERR;

    uint32_t rsize = sizeof( short )  + // reply opcode
        sizeof( gbItemEncoding ) +      // data type
        sizeof( uint32_t ) + 	        // data length
        size;			  		        // data

    // realloc only if needed
    if( rsize > client->buffer_size )
    {
        client->buffer = (byte_t *)zrealloc( client->buffer, rsize );
    }

    assert( client->buffer != NULL );
    
    client->buffer_size = rsize;
    client->read  		= 0;
    client->wrote 		= 0;
    client->shutdown 	= shutdown;

    memcpy( client->buffer, 				  					          					  
            memrev16ifbe(&code), 	 
            sizeof( short ) );

    memcpy( client->buffer + sizeof( short ),					          					  
            &encoding, 
            sizeof( gbItemEncoding ) );

    memcpy( client->buffer + sizeof( short ) + sizeof( gbItemEncoding ),  				     
            memrev32ifbe(&size), 	 
            sizeof( uint32_t ) );

    memcpy( client->buffer + sizeof( short ) + sizeof( gbItemEncoding ) + sizeof( uint32_t ), 
            reply, 	 
            size );

    return gbCreateFileEvent( client->server->events, client->fd, GB_WRITABLE, proc, client );
}

int gbClientEnqueueCode( gbClient *client, short code, gbFileProc proc, short shutdown )
{
    assert( client != NULL );

    byte_t zero = 0x00;

    return gbClientEnqueueData( client, code, GB_ENC_PLAIN, &zero, 1, proc, shutdown );
}

int gbClientEnqueueItem( gbClient *client, short code, gbItem *item, gbFileProc *proc, short shutdown )
{
    assert( client != NULL );
    assert( item != NULL );
    assert( item->data != NULL || item->encoding == GB_ENC_NUMBER );
    assert( item->size > 0 );

    if( item->encoding == GB_ENC_PLAIN )
    {
        return gbClientEnqueueData( client, code, GB_ENC_PLAIN, item->data, item->size, proc, shutdown );
    }
    else if( item->encoding == GB_ENC_LZF )
    {
        size_t declen = lzf_decompress
        (
            item->data,
            item->size,
            client->server->lzf_buffer,
            client->server->limits.maxrequestsize
        );

        assert( declen > item->size );

        return gbClientEnqueueData( client, code, GB_ENC_PLAIN, client->server->lzf_buffer, declen, proc, shutdown );
    }
    else if( item->encoding == GB_ENC_NUMBER )
    {
        long num = (long)item->data;
#if __x86_64__ || __ppc64__
        byte_t *v = (byte_t *)memrev64ifbe(&num);
#else
        byte_t *v = (byte_t *)memrev32ifbe(&num);
#endif	

        return gbClientEnqueueData( client, code, GB_ENC_NUMBER, v, item->size, proc, shutdown );
    }
    else
        return GBNET_ERR;
}

int gbClientEnqueueKeyValueSet( gbClient *client, uint32_t elements, gbFileProc *proc, short shutdown )
{
    assert( client != NULL );
    assert( client->server != NULL );
    assert( elements > 0 );
    assert( client->server->m_buffer != NULL );

    gbServer *server = client->server;
    gbItem *item = NULL;
    uint32_t sz = sizeof(uint32_t),
             vsize = 0,
             space = server->limits.maxresponsesize;
    byte_t *data = server->m_buffer,
           *p = data,
           *v = NULL;
    gbItemEncoding encoding;
    long num;

#define CHECK_SPACE(needed) if( needed > space ) \
    { \
        gbLog( WARNING, "Max response size reached, asked for %u more bytes.", needed ); \
            return GBNET_ERR; \
    }

#define SAFE_MEMCPY( p, data, size ) CHECK_SPACE(size); memcpy( p, data, size ); p += size; space -= size

    SAFE_MEMCPY( p, memrev32ifbe(&elements), sz );

    ll_foreach_2( server->m_keys, server->m_values, ki, vi )
    {
        // handle expired/nulled items
        if( vi->data != NULL )
        {
            item 	 = vi->data;
            encoding = item->encoding;

            // write key size + key
            sz = strlen( ki->data );

            assert( sz > 0 );

            SAFE_MEMCPY( p, memrev32ifbe(&sz), sizeof(uint32_t) );
            SAFE_MEMCPY( p, ki->data, sz );

            // write value size + value
            if( encoding == GB_ENC_PLAIN )
            {
                vsize = item->size;
                v	  = item->data;
            }
            else if( encoding == GB_ENC_LZF )
            {
                encoding = GB_ENC_PLAIN;
                vsize = lzf_decompress
                    (
                     item->data,
                     item->size,
                     client->server->lzf_buffer,
                     client->server->limits.maxrequestsize
                    );

                v = server->lzf_buffer;
            }
            else if( item->encoding == GB_ENC_NUMBER )
            {
                num = (long)item->data;
#if __x86_64__ || __ppc64__
                v = (byte_t *)memrev64ifbe(&num);
#else
                v = (byte_t *)memrev32ifbe(&num);
#endif	
                vsize = item->size;
            }

            assert( v != NULL );
            assert( vsize > 0 );

            SAFE_MEMCPY( p, &encoding,            sizeof( gbItemEncoding ) );
            SAFE_MEMCPY( p, memrev32ifbe(&vsize), sizeof( uint32_t ) );
            SAFE_MEMCPY( p, v, 		              vsize );
        }
    }

    int ret = gbClientEnqueueData( client, REPL_KVAL, GB_ENC_PLAIN, data, p - data, proc, shutdown );

    return ret;
}
