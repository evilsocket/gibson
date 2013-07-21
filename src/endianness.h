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
#ifndef __ENDIANNESS_H__
#define __ENDIANNESS_H__

#include <stdint.h>

void *memrev16(void *p);
void *memrev32(void *p);
void *memrev64(void *p);
uint16_t uintrev16(uint16_t v);
uint32_t uintrev32(uint32_t v);
uint64_t uintrev64(uint64_t v);
int16_t intrev16(int16_t v);
int32_t intrev32(int32_t v);
int64_t intrev64(int64_t v);

#if (BYTE_ORDER == LITTLE_ENDIAN)
#   define memrev16ifbe(p) (p)
#   define memrev32ifbe(p) (p)
#   define memrev64ifbe(p) (p)
#   define uintrev16ifbe(v) (v)
#   define uintrev32ifbe(v) (v)
#   define uintrev64ifbe(v) (v)
#   define intrev16ifbe(v) (v)
#   define intrev32ifbe(v) (v)
#   define intrev64ifbe(v) (v)
#else
#   define memrev16ifbe(p) memrev16(p)
#   define memrev32ifbe(p) memrev32(p)
#   define memrev64ifbe(p) memrev64(p)
#   define uintrev16ifbe(v) uintrev16(v)
#   define uintrev32ifbe(v) uintrev32(v)
#   define uintrev64ifbe(v) uintrev64(v)
#   define intrev16ifbe(v) intrev16(v)
#   define intrev32ifbe(v) intrev32(v)
#   define intrev64ifbe(v) intrev64(v)

#endif

#endif
