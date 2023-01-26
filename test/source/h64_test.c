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

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "h64/h64.h"

static int
str_equals(const void *ptr1, const void *ptr2)
{
	return strcmp(ptr1, ptr2) == 0;
}

static uint64_t
str_hash(const void *ptr, uint64_t seed)
{
	const char *str = ptr;
	return h64_byte_hash(str, strlen(str), seed);
}

static void
general_test()
{
	char *str1 = "help";
	char *str2 = "me";

	struct h64 *h64 = h64_create(str_hash, str_equals);

	assert(h64_count(h64) == 0);
	assert(h64_find(h64, str1) == NULL);
	assert(h64_find(h64, str2) == NULL);

	h64_insert(h64, str1);
	assert(h64_count(h64) == 1);
	assert(h64_find(h64, str1) == str1);
	h64_erase(h64, str1);
	assert(h64_count(h64) == 0);
	assert(h64_find(h64, str1) == NULL);

	h64_insert(h64, str1);
	assert(h64_count(h64) == 1);
	h64_insert(h64, str1);
	assert(h64_count(h64) == 1);
	h64_insert(h64, str2);
	assert(h64_count(h64) == 2);
	assert(h64_find(h64, str1) == str1);
	assert(h64_find(h64, str2) == str2);
	assert(h64_find(h64, "not in the table") == NULL);
	h64_erase(h64, str2);
	assert(h64_count(h64) == 1);
	assert(h64_find(h64, str1) == str1);
	assert(h64_find(h64, str2) == NULL);
	h64_erase(h64, str1);
	assert(h64_count(h64) == 0);
	assert(h64_find(h64, str1) == NULL);
	assert(h64_find(h64, str2) == NULL);

	h64_destroy(h64);
}

static int
int_equals(const void *ptr1, const void *ptr2)
{
	const int *i1 = ptr1;
	const int *i2 = ptr2;
	return *i1 == *i2;
}

static uint64_t
int_hash(const void *ptr, uint64_t seed)
{
	const int *i = ptr;
	return h64_byte_hash(i, sizeof(*i), seed);
}

static void
resize_test()
{
	struct h64 *h64 = h64_create(int_hash, int_equals);

	enum { N = 1000 };
	int data[N];
	for (int i = 0; i < N; ++i)
		data[i] = i;

	for (int i = 0; i < N; ++i)
		h64_insert(h64, &data[i]);
	assert(h64_count(h64) == N);

	for (int i = 0; i < N; ++i) {
		int *found = h64_find(h64, &data[i]);
		assert(found != NULL && *found == data[i]);
	}

	for (int i = 0; i < N / 2; ++i)
		h64_erase(h64, &data[i]);
	assert(h64_count(h64) == N / 2);

	for (int i = 0; i < N; ++i) {
		int *found = h64_find(h64, &data[i]);
		if (i < N / 2)
			assert(found == NULL);
		else
			assert(found != NULL && *found == data[i]);
	}

	for (int i = 0; i < N; ++i)
		h64_erase(h64, &data[i]);
	assert(h64_count(h64) == 0);

	for (int i = 0; i < N; ++i) {
		int *found = h64_find(h64, &data[i]);
		assert(found == NULL);
	}

	h64_destroy(h64);
}

int main()
{
	general_test();
	resize_test();
	return 0;
}
