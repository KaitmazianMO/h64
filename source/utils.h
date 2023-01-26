/*
 * MIT License
 *
 * Copyright (c) 2023 Kaitmazian Maksim
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>

#define xcalloc(n, size)						\
({									\
	void *ret = calloc((n), (size));				\
	assert(ret && "Allocation failed");				\
	ret;								\
})

#define aligned_xalloc(alignment, size)					\
({									\
	void *ret = aligned_alloc((alignment), (size));			\
	assert(ret && "Allocation failed");				\
	ret;								\
})

#if !defined(static_assert)
#define static_assert _Static_assert
#endif

#if defined(__GNUC__)
#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)
#else
#define likely(x)    (x)
#define unlikely(x)  (x)
#endif

#if defined(__GNUC__)
#define alignof(n) __attribute__ ((aligned((n))))
#endif

#define MAX(a, b)  ((a) > (b) ? (a) : (b))

static inline uint64_t
mixer64(uint64_t n)
{
	const uint64_t z = 0x9FB21C651E98DF25;

	n ^= ((n << 49) | (n >> 15)) ^ ((n << 24) | (n >> 40));
	n *= z;
	n ^= n >> 35;
	n *= z;
	n ^= n >> 28;

	return n;
}


static inline size_t
roundup_to_pow2(size_t n)
{
	n -= 1;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	n += 1;
	return n;
}

static inline int
is_power_of_2(uint64_t n)
{
	return (n & (n - 1)) == 0;
}
