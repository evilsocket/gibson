/*
 * Copyright (c) 2013, Simone Margaritelli <evilsocket at gmail dot com>
 *
 * Based on Redis zmalloc library by Salvatore Sanfilippo.
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

#ifndef __ZMEM_H__
#define __ZMEM_H__

#include "configure.h"

#if defined(HAVE_JEMALLOC)
#   include <jemalloc/jemalloc.h>
#endif

#if defined(__APPLE__)
#   include <malloc/malloc.h>
#   define HAVE_MALLOC_SIZE 1
#   define zmalloc_size(p) malloc_size(p)
#endif

#include <stdlib.h>

// get system available memory
size_t zmem_available();
// get memory used by the process
size_t zmem_used(void);
// set a custom out of memory handler
void   zmem_set_oom_handler(void (*oom_handler)(size_t));
// get RSS / used ratio
float  zmem_fragmentation_ratio(void);
// get resident set size aka process memory held by the ram
size_t zmem_rss(void);
// get private dirty memory field
size_t zmem_private_dirty(void);

void *zmalloc(size_t size);
void *zcalloc(size_t size);
void *zrealloc(void *ptr, size_t size);
void  zfree(void *ptr);
void *zmemdup(void *ptr, size_t size);
char *zstrdup(const char *s);

#endif
