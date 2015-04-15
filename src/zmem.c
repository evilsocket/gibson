/*
 * Copyright (c) 2013, Simone Margaritelli <evilsocket at gmail dot com>
 * Copyright (c) 2009-2015, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "zmem.h"
#include <string.h>
#include <unistd.h>

#if defined(_WIN32)
	#include <Windows.h>
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/param.h>
#endif
#if defined(BSD)
	#include <sys/sysctl.h>
#endif

#ifdef HAVE_MALLOC_SIZE
#   define ZMEM_PREFIX_SIZE (0)
#else
#   if defined(__sun) || defined(__sparc) || defined(__sparc__)
#       define ZMEM_PREFIX_SIZE (sizeof(long long))
#   else
#       define ZMEM_PREFIX_SIZE (sizeof(size_t))
#   endif
#endif

#define SIZE_OF_LONG_MASK (sizeof(long)-1)
// increment used memory statistic by __n padded to sizeof(long)
#define zmem_incr_mem(__n) do { \
    size_t _n = (__n); \
    if( _n & SIZE_OF_LONG_MASK ) _n += sizeof(long) - ( _n & SIZE_OF_LONG_MASK ); \
    used_memory += _n; \
} while(0)
// decrement used memory statistic by __n padded to sizeof(long)
#define zmem_decr_mem(__n) do { \
    size_t _n = (__n); \
    if( _n & SIZE_OF_LONG_MASK ) _n += sizeof(long) - ( _n & SIZE_OF_LONG_MASK ); \
    used_memory -= _n; \
} while(0)
// write the size to the first bytes of p internal pointer
#define zmem_write_prefix(p,size) *((size_t *)(p)) = (size)
// read the size from the first bytes of p
#define zmem_read_prefix(p)       *((size_t *)(p))
// userland ptr -> zmem prefixed ptr
#define zmem_head_ptr(p) ((char *)(p) - ZMEM_PREFIX_SIZE)
// zmem prefixed ptr -> userland ptr
#define zmem_real_ptr(p) ((char *)(p) + ZMEM_PREFIX_SIZE)


/* This function provide us access to the original libc free(). This is useful
 * for instance to free results obtained by backtrace_symbols(). We need
 * to define this function before including zmalloc.h that may shadow the
 * free implementation if we use jemalloc or another non standard allocator. */
void zlibc_free(void *ptr) {
    assert( ptr != NULL );

    free(ptr);
}

void zmem_allocator( char *buffer, size_t size ){
    assert( buffer != NULL );

#if HAVE_JEMALLOC == 1
	const char *p;
	size_t s = sizeof(p);
	mallctl("version", &p,  &s, NULL, 0);

	snprintf( buffer, size, "jemalloc %s", p );
#else
    strncpy( buffer, "malloc", size );
#endif
}

/* Author:  David Robert Nadeau */
unsigned long long zmem_available(){
#if defined(_WIN32) && (defined(__CYGWIN__) || defined(__CYGWIN32__))
    MEMORYSTATUS status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatus(&status);
    return (unsigned long long) status.dwTotalPhys;
#elif defined(_WIN32)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx( &status );
    return (unsigned long long)status.ullTotalPhys;
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
    /* UNIX variants. ------------------------------------------- */
    /* Prefer sysctl() over sysconf() except sysctl() HW_REALMEM and HW_PHYSMEM */

#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
    int mib[2];
    mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
    mib[1] = HW_MEMSIZE;		/* OSX */
#elif defined(HW_PHYSMEM64)
    mib[1] = HW_PHYSMEM64;		/* NetBSD, OpenBSD.*/
#endif
    int64_t size = 0;		/* 64-bit */
    size_t len = sizeof(size);
    if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) return (unsigned long long) size;
    return 0L;			/* Failed? */

#elif defined(_SC_AIX_REALMEM)
    return (unsigned long long)sysconf( _SC_AIX_REALMEM ) * (size_t)1024L;
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    /* FreeBSD, Linux, OpenBSD, and Solaris. */
    return (unsigned long long)sysconf( _SC_PHYS_PAGES ) * (long long)sysconf( _SC_PAGESIZE );

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
    /* Legacy. */
    return (unsigned long long)sysconf( _SC_PHYS_PAGES ) * (unsigned long long)sysconf( _SC_PAGE_SIZE );
#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
    /* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
    int mib[2];
    mib[0] = CTL_HW;
#if defined(HW_REALMEM)
    mib[1] = HW_REALMEM;		/* FreeBSD. ----------------- */
#elif defined(HW_PYSMEM)
    mib[1] = HW_PHYSMEM;		/* Others. ------------------ */
#endif
    unsigned long long size = 0;		/* 32-bit */
    size_t len = sizeof( size );
    if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) return (unsigned long long)size;
    return 0;			/* Failed? */
#endif /* sysctl and sysconf variants */

#else
    return 0;			/* Unknown OS. */
#endif
}

static size_t used_memory = 0;

static void zmalloc_default_oom(size_t size) {
    fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n",size);
    fflush(stderr);
    abort();
}

static void (*zmalloc_oom_handler)(size_t) = zmalloc_default_oom;

void *zmalloc(size_t size) {
    assert( size > 0 );

    void *ptr = malloc(size+ZMEM_PREFIX_SIZE);

    if (!ptr) {
		zmalloc_oom_handler(size);
		return NULL;
	}

#ifdef HAVE_MALLOC_SIZE
    zmem_incr_mem(zmalloc_size(ptr));
    return ptr;
#else
    zmem_write_prefix(ptr,size);
    zmem_incr_mem(size+ZMEM_PREFIX_SIZE);
    return zmem_real_ptr(ptr);
#endif
}

void *zcalloc(size_t size) {
    assert( size > 0 );

    void *ptr = calloc(1, size+ZMEM_PREFIX_SIZE);

    if (!ptr) {
		zmalloc_oom_handler(size);
		return NULL;
	}

#ifdef HAVE_MALLOC_SIZE
    zmem_incr_mem(zmalloc_size(ptr));
    return ptr;
#else
    zmem_write_prefix(ptr,size);
    zmem_incr_mem(size+ZMEM_PREFIX_SIZE);
    return zmem_real_ptr(ptr);
#endif
}

void *zrealloc(void *ptr, size_t size) {
    assert( size > 0 );

#ifndef HAVE_MALLOC_SIZE
    void *realptr;
#endif
    size_t oldsize;
    void *newptr;

    if (ptr == NULL) return zmalloc(size);
#ifdef HAVE_MALLOC_SIZE
    oldsize = zmalloc_size(ptr);
    newptr = realloc(ptr,size);
    if (!newptr) zmalloc_oom_handler(size);

    zmem_decr_mem(oldsize);
    zmem_incr_mem(zmalloc_size(newptr));
    return newptr;
#else
    realptr = zmem_head_ptr(ptr);
    oldsize = zmem_read_prefix(realptr);
    newptr  = realloc(realptr,size+ZMEM_PREFIX_SIZE);
    if (!newptr) {
		zmalloc_oom_handler(size);
		return NULL;
	}

    zmem_write_prefix(newptr,size);
    zmem_decr_mem(oldsize);
    zmem_incr_mem(size);
    return zmem_real_ptr(newptr);
#endif
}

/* Provide zmalloc_size() for systems where this function is not provided by
 * malloc itself, given that in that case we store an header with this
 * information as the first bytes of every allocation. */
#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr) {
    assert( ptr != NULL );

    void *realptr = zmem_head_ptr(ptr);
    size_t size   = zmem_read_prefix(realptr);
    // assume at least that all the allocations are padded at sizeof(long) by the underlying allocator.
    if( size & SIZE_OF_LONG_MASK )
        size += sizeof(long) - ( size & SIZE_OF_LONG_MASK );

    return size + ZMEM_PREFIX_SIZE;
}
#endif

void *zmemdup(void *ptr, size_t size){
    assert( ptr != NULL );
    assert( size > 0 );

	unsigned char *dup = zmalloc( size );

	memcpy( dup, ptr, size );

	return dup;
}

void zfree(void *ptr) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
    size_t oldsize;
#endif

    if (ptr == NULL) return;
#ifdef HAVE_MALLOC_SIZE
    zmem_decr_mem(zmalloc_size(ptr));
    free(ptr);
#else
    realptr = zmem_head_ptr(ptr);
    oldsize = zmem_read_prefix(realptr);
    zmem_decr_mem(oldsize+ZMEM_PREFIX_SIZE);
    free(realptr);
#endif
}

char *zstrdup(const char *s) {
    assert( s != NULL );

    size_t l = strlen(s)+1;
    char *p = zmalloc(l);
    memcpy(p,s,l);
    return p;
}

size_t zmem_used(void) {
    return used_memory;
}

void zmem_set_oom_handler(void (*oom_handler)(size_t)) {
    zmalloc_oom_handler = oom_handler;
}

/* Get the RSS information in an OS-specific way. */

#if defined(HAVE_PROC_STAT)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

size_t zmem_rss(void) {
    int page = sysconf(_SC_PAGESIZE), rn = 0;
    size_t rss;
    char buf[4096] = {0};
    char filename[256] = {0};
    int fd, count;
    char *p, *x;

    snprintf(filename,256,"/proc/%d/stat",getpid());
    if ((fd = open(filename,O_RDONLY)) == -1) return 0;

	rn = read(fd,buf,4095);
	if( rn <= 0) {
        close(fd);
        return 0;
    }
    close(fd);

	buf[rn + 1] = 0x00;

    p = buf;
    count = 23; /* RSS is the 24th field in /proc/<pid>/stat */
    while(p && count--) {
        p = strchr(p,' ');
        if (p) p++;
    }
    if (!p) return 0;
    x = strchr(p,' ');
    if (!x) return 0;
    *x = '\0';

    rss = strtoll(p,NULL,10);
    rss *= page;
    return rss;
}
#elif defined(HAVE_TASKINFO)
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/task.h>
#include <mach/mach_init.h>

size_t zmem_rss(void) {
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
        return 0;
    task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

    return t_info.resident_size;
}
#else
size_t zmem_rss(void) {
    /* If we can't get the RSS in an OS-specific way for this system just
     * return the memory usage we estimated in zmalloc()..
     *
     * Fragmentation will appear to be always 1 (no fragmentation)
     * of course... */
    return zmem_used();
}
#endif

/* Fragmentation = RSS / allocated-bytes */
float zmem_fragmentation_ratio(void) {
    return (float)zmem_rss()/zmem_used();
}

#if defined(HAVE_PROC_SMAPS)
size_t zmem_private_dirty(void) {
    char line[1024];
    size_t pd = 0;
    FILE *fp = fopen("/proc/self/smaps","r");

    if (!fp) return 0;
    while(fgets(line,sizeof(line),fp) != NULL) {
        if (strncmp(line,"Private_Dirty:",14) == 0) {
            char *p = strchr(line,'k');
            if (p) {
                *p = '\0';
                pd += strtol(line+14,NULL,10) * 1024;
            }
        }
    }
    fclose(fp);
    return pd;
}
#else
size_t zmem_private_dirty(void) {
    return 0;
}
#endif
