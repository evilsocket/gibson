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
 
#include "mem.h"
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

/* Author:  David Robert Nadeau */
size_t gbMemAvailable(){
	#if defined(_WIN32) && (defined(__CYGWIN__) || defined(__CYGWIN32__))
		MEMORYSTATUS status;
		status.dwLength = sizeof(status);
		GlobalMemoryStatus(&status);
		return (size_t) status.dwTotalPhys;
	#elif defined(_WIN32)
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(status);
		GlobalMemoryStatusEx( &status );
		return (size_t)status.ullTotalPhys;
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
		if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) return (size_t) size;
		return 0L;			/* Failed? */

	#elif defined(_SC_AIX_REALMEM)
		return (size_t)sysconf( _SC_AIX_REALMEM ) * (size_t)1024L;
	#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
		/* FreeBSD, Linux, OpenBSD, and Solaris. */
		return (size_t)sysconf( _SC_PHYS_PAGES ) * (size_t)sysconf( _SC_PAGESIZE );

	#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
		/* Legacy. */
		return (size_t)sysconf( _SC_PHYS_PAGES ) * (size_t)sysconf( _SC_PAGE_SIZE );
	#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
		/* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
		int mib[2];
		mib[0] = CTL_HW;
	#if defined(HW_REALMEM)
		mib[1] = HW_REALMEM;		/* FreeBSD. ----------------- */
	#elif defined(HW_PYSMEM)
		mib[1] = HW_PHYSMEM;		/* Others. ------------------ */
	#endif
		unsigned int size = 0;		/* 32-bit */
		size_t len = sizeof( size );
		if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) return (size_t)size;
		return 0L;			/* Failed? */
	#endif /* sysctl and sysconf variants */

	#else
		return 0L;			/* Unknown OS. */
	#endif
}

void gbMemFormat( unsigned long used, char *buffer, size_t size ){
	memset( buffer, 0x00, size );
	char *suffix[] = { "B", "KB", "MB", "GB", "TB" };
	size_t i = 0;
	double d = used;
    
    for( ; i < 5 && d >= 1024; ++i ){
        d /= 1024.0;
    }

	sprintf( buffer, "%.1f%s", d, suffix[i] );
}

void *gbMemDup( void *mem, size_t size ){
	unsigned char *dup = zmalloc( size );

	memcpy( dup, mem, size );

	return dup;
}

void *gbMemReuse( void *old, void *new, size_t size ){
	old = zrealloc( old, size );

	memset( old, 0x00, size );
	memcpy( old, new, size );

	return old;
}
