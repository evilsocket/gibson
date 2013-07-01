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
#if HAVE_SELECT
#include "net.h"

#if HAVE_JEMALLOC == 1
#include <jemalloc/jemalloc.h>
#endif

typedef struct aeApiState {
    fd_set rfds, wfds;
    /* We need to have a copy of the fd sets as it's not safe to reuse
     * FD sets after select(). */
    fd_set _rfds, _wfds;
} aeApiState;

static int aeApiCreate(gbEventLoop *eventLoop) {
    aeApiState *state = malloc(sizeof(aeApiState));

    if (!state) return -1;
    FD_ZERO(&state->rfds);
    FD_ZERO(&state->wfds);
    eventLoop->apidata = state;
    return 0;
}

static int aeApiResize(gbEventLoop *eventLoop, int setsize) {
    /* Just ensure we have enough room in the fd_set type. */
    if (setsize >= FD_SETSIZE) return -1;
    return 0;
}

static void aeApiFree(gbEventLoop *eventLoop) {
    free(eventLoop->apidata);
}

static int aeApiAddEvent(gbEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;

    if (mask & GB_READABLE) FD_SET(fd,&state->rfds);
    if (mask & GB_WRITABLE) FD_SET(fd,&state->wfds);
    return 0;
}

static void aeApiDelEvent(gbEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;

    if (mask & GB_READABLE) FD_CLR(fd,&state->rfds);
    if (mask & GB_WRITABLE) FD_CLR(fd,&state->wfds);
}

static int aeApiPoll(gbEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, j, numevents = 0;

    memcpy(&state->_rfds,&state->rfds,sizeof(fd_set));
    memcpy(&state->_wfds,&state->wfds,sizeof(fd_set));

    retval = select(eventLoop->maxfd+1,
                &state->_rfds,&state->_wfds,NULL,tvp);
    if (retval > 0) {
        for (j = 0; j <= eventLoop->maxfd; j++) {
            int mask = 0;
            gbFileEvent *fe = &eventLoop->events[j];

            if (fe->mask == GB_NONE) continue;
            if ( ( fe->mask & GB_READABLE ) && FD_ISSET(j,&state->_rfds))
                mask |= GB_READABLE;
            if ( ( fe->mask & GB_WRITABLE ) && FD_ISSET(j,&state->_wfds))
                mask |= GB_WRITABLE;
            eventLoop->fired[numevents].fd = j;
            eventLoop->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;
}

char *aeApiName(void) {
    return "select";
}
#else
void AVOID_EMPTY_UNIT_WARNING_BY_GCC_SELECT(){ }
#endif
