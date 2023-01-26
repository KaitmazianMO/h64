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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

#include <immintrin.h>

#include "utils.h"
#include "h64/h64.h"

enum {
	L1CACHE_LINE_SIZE = 64,
	DEFAULT_SIZE = 4,
	MIN_SIZE = DEFAULT_SIZE,
	GROUP_ENTRIES = H64_INTERNAL_GROUP_ENTRIES,
	ENTRIES_MASK = 0x7F,
};

#define MAX_LOAD_FACTOR  0.67
#define MIN_LOAD_FACTOR  (MAX_LOAD_FACTOR / 4.0)

static double
h64_load_factor(const struct h64 *h);

#ifdef H64_STORE_STATISTICS
	#define DUMP_IF_STORING_STATS(h) do {					 \
		size_t size_in_groups = h->size_in_groups;			 \
		size_t count = h->count;					 \
		double average_hint = h->hint_count != 0 ? 1. * h->hint_sum / h->hint_count : 0;		 \
		double hitrate = h->compare_count != 0 ? 1. * h->equal_count / h->compare_count : 0;		 \
		double find_average_probe_length = h->find_count != 0 ? 1. * h->find_probe_count / h->find_count : 0;	\
		size_t find_max_probe_length = h->find_max_probe_count;	 \
		double insert_average_probe_length = h->insert_count != 0 ? 1. * h->insert_probe_count / h->insert_count : 0;	\
		size_t insert_max_probe_length = h->insert_max_probe_count;	 \
		fprintf(stderr, "%s:\n", __func__);				 \
		fprintf(stderr, "\t""size_in_groups: %zu\n", size_in_groups);    \
		fprintf(stderr, "\t""count: %zu\n", count);			 \
		fprintf(stderr, "\t""avg_hint: %lg\n", average_hint);		 \
		fprintf(stderr, "\t""load_factor: %lg\n", h64_load_factor(h));   \
		fprintf(stderr, "\t""hitrate: %lg\n", hitrate);			 \
		fprintf(stderr, "\t""find:\n"					 \
				"\t\t""avg_probe_length: %lg\n"		 	 \
				"\t\t""max_probe_length: %zu\n",		 \
				find_average_probe_length,			 \
				find_max_probe_length);	       		 	 \
		fprintf(stderr, "\t""insert:\n"					 \
				"\t\t""avg_probe_length: %lg\n"			 \
				"\t\t""max_probe_length: %zu\n",		 \
				insert_average_probe_length,			 \
				insert_max_probe_length);	       		 	 \
	} while(0)

	#define IF_STORING_STATS(...) __VA_ARGS__

#else
	#define DUMP_IF_STORING_STATS(h)  ;

	#define IF_STORING_STATS(...)  ;

#endif

static int
group_was_full(const struct h64_group *group)
{
	return group->status >> (CHAR_BIT - 1);
}

static int
group_is_full(const struct h64_group *group)
{
	return (group->status & ENTRIES_MASK) == ENTRIES_MASK;
}

static void
group_insert(struct h64_group *group, void *entry, uint8_t hint, size_t idx)
{
	assert(idx < GROUP_ENTRIES);
	assert(group->entries[idx] == NULL);
	assert(((group->status >> idx) & 0x1) == 0);
	group->entries[idx] = entry;
	group->hints[idx] = hint;
	group->status |= 0x1 << idx;
	if (group_is_full(group)) {
		group->status = 0xFF;
	}
}

static void
group_update(struct h64_group *group, void *entry, size_t idx)
{
	assert(idx < GROUP_ENTRIES);
	assert(((group->status >> idx) & 0x1) == 1);
	group->entries[idx] = entry;
}

static void *
group_erase_entry(struct h64_group* group, size_t idx)
{
	assert(idx < GROUP_ENTRIES);
	void *entry = group->entries[idx];
	group->entries[idx] = NULL;
	group->hints[idx] = 0;
	group->status &= ~(0x1 << idx);
	return entry;
}

static void
h64_init(struct h64 *h, size_t size,
	 h64_hasher_f hasher, h64_equals_f equals)
{
	assert(is_power_of_2(size) && "Size must be a power of 2.");

	h->hasher = hasher;
	h->equals = equals;
	size = size < MIN_SIZE ? MIN_SIZE : size;
	struct h64_group *groups = aligned_xalloc(
		L1CACHE_LINE_SIZE, size * sizeof(*groups)
	);
	memset(groups, 0, size * sizeof(*groups));
	h->groups = groups;
	h->size_in_groups = size;
	h->seed = mixer64((uint64_t)groups);
	h->count = 0;
	IF_STORING_STATS(
		h->hint_sum = 0;
		h->hint_count = 0;
		h->find_count = 0;
		h->find_probe_count = 0;
		h->find_max_probe_count = 0;
		h->insert_count = 0;
		h->insert_probe_count = 0;
		h->insert_max_probe_count = 0;
		h->equal_count = 0;
		h->compare_count = 0;
	)
}

static void
h64_swap(struct h64 *h1, struct h64 *h2)
{
	struct h64 tmp = *h1;
	*h1 = *h2;
	*h2 = tmp;
}

struct h64 *
h64_create(h64_hasher_f hasher, h64_equals_f equals)
{
	assert(hasher != NULL && "Need a hash function.");
	assert(equals != NULL && "Need an equals function.");
	struct h64 *h = xcalloc(1, sizeof(*h));
	h64_init(h, DEFAULT_SIZE, hasher, equals);
	return h;
}

static void
h64_free(struct h64 *h)
{
	free(h->groups);
}

void
h64_destroy(struct h64 *h)
{
	h64_free(h);
	free(h);
}

/* Prefetch memory with groups in order to avoid cache-misses. */
static void
h64_prefetch_groups(const struct h64 *h)
{
	__builtin_prefetch(h->groups, 0, 1);
}

static void
h64_resize(struct h64 *h, size_t size)
{
	DUMP_IF_STORING_STATS(h);
	assert(is_power_of_2(size) && "Size must be a power of 2.");

	h64_prefetch_groups(h);
	struct h64 tmp;
	h64_init(&tmp, size, h->hasher, h->equals);
	h64_for_each(h, entry)
		h64_insert_new(&tmp, entry);

	h64_swap(h, &tmp);
	h64_free(&tmp);

}

void
h64_reserve(struct h64 *h, size_t entries_count)
{
	size_t total_entries = entries_count / MAX_LOAD_FACTOR;
	size_t size_in_groups = roundup_to_pow2(total_entries / GROUP_ENTRIES + 1);
	h64_resize(h, size_in_groups);
}

static void
h64_grow_up(struct h64 *h)
{
	h64_resize(h, h->size_in_groups * 2);
}

static void
h64_grow_down(struct h64 *h)
{
	h64_resize(h, h->size_in_groups / 2);
}

static uint64_t
h64_hash(const struct h64 *h, const void *entry)
{
	return h->hasher(entry, h->seed);
}

static bool
h64_equals(const struct h64 *h, const void *lsh, const void *rhs)
{
	return h->equals(lsh, rhs);
}

static uint8_t
hash_hint(uint64_t hash)
{
	/* Leftmost byte. */
	return hash >> (CHAR_BIT * 7);
}

/**
 * Quadratic probing sequence.
 * It assumes that hash table size is power of 2, so I can substitute mod with
 * using a mask, and stepping formula step[i] = start + (i^2 + i) / 2 guarantees
 * that every group will be traversed only once.
 */
struct probe_sequence {
	size_t start;
	size_t iteration;
	size_t size_mask;
};

static void
ps_init(struct probe_sequence *ps, uint64_t hash, size_t size)
{
	ps->size_mask = size - 1;
	ps->iteration = 0;
	ps->start = hash & ps->size_mask;
}

static void
ps_next(struct probe_sequence *ps)
{
	ps->iteration += 1;
}

static size_t
ps_position(const struct probe_sequence *ps)
{
	size_t s = ps->start;
	size_t i = ps->iteration;
	size_t mask = ps->size_mask;
	return (s + i * (i + 1) / 2) & mask;
}

static uint8_t
group_match_inserted(const struct h64_group* group, uint8_t hint)
{
	__m128i hints = _mm_lddqu_si128((const __m128i *)group->hints);
	__m128i target = _mm_set1_epi8(hint);
	__m128i match = _mm_cmpeq_epi8(target, hints);
	return _mm_movemask_epi8(match) & group->status & 0x7F;
}

struct find_result {
	struct h64_group *group;
	size_t index;
	bool found;
};

static void
find_result_init(struct find_result *result,
		 struct h64_group *group, size_t index, bool found)
{
	result->group = group;
	result->index = index;
	result->found = found;
}

static void
h64_find_entry(const struct h64 *h, const void *entry,
	       uint64_t hash, struct find_result *result)
{
	IF_STORING_STATS(
		((struct h64 *)h)->find_count += 1;
	)

	uint8_t hint = hash_hint(hash);
	struct probe_sequence seq;
	ps_init(&seq, hash, h->size_in_groups);

	while (true) {

		IF_STORING_STATS(
			((struct h64 *)h)->find_probe_count += 1;
			((struct h64 *)h)->find_max_probe_count = MAX(
				seq.iteration + 1,
				h->find_max_probe_count
			);
		)

		size_t position = ps_position(&seq);
		struct h64_group *group = &h->groups[position];
		void **entries = group->entries;
		uint8_t match_byte = group_match_inserted(group, hint);
		while (match_byte != 0) {
			IF_STORING_STATS(((struct h64 *)h)->compare_count += 1;)
			uint8_t match_bit = match_byte & (-match_byte);
			uint8_t idx = __builtin_ctz(match_byte);
			if (likely(h64_equals(h, entry, entries[idx]))) {
				IF_STORING_STATS(((struct h64 *)h)->equal_count += 1;)
				return find_result_init(result, group, idx, true);
			}
			match_byte ^= match_bit;
		}

		if (likely(!group_was_full(group)))
			return find_result_init(result, NULL, -1, false);

		ps_next(&seq);
	}
}

static void
h64_find_empty_entry(const struct h64 *h, uint64_t hash,
		     struct find_result *result)
{
	IF_STORING_STATS(
		((struct h64 *)h)->insert_count += 1;
	)

	uint8_t hint = hash_hint(hash);
	struct probe_sequence seq;
	ps_init(&seq, hash, h->size_in_groups);

	while (true) {

		IF_STORING_STATS(
			((struct h64 *)h)->insert_probe_count += 1;
			((struct h64 *)h)->insert_max_probe_count = MAX(
				seq.iteration + 1,
				h->insert_max_probe_count
			);
		)

		size_t position = ps_position(&seq);
		struct h64_group *group = &h->groups[position];
		if (likely(!group_is_full(group))) {
			/* get an index of the first zero bit from right. */
			size_t index = __builtin_ctz(~group->status);
			return find_result_init(result, group, index, true);
		}
		ps_next(&seq);
	}
}

void *
h64_find(const struct h64 *h, const void *entry)
{
	DUMP_IF_STORING_STATS(h);

	h64_prefetch_groups(h);
	uint64_t hash = h64_hash(h, entry);
	struct find_result result;
	h64_find_entry(h, entry, hash, &result);
	return result.found ? result.group->entries[result.index]
			    : NULL;
}

static double
h64_load_factor(const struct h64 *h)
{
	return h->count / (h->size_in_groups * GROUP_ENTRIES * 1.0);
}

static bool
h64_should_grow_up(const struct h64 *h)
{
	size_t max_count = MAX_LOAD_FACTOR * (h->size_in_groups * GROUP_ENTRIES);
	return h->count > max_count;
}

static bool
h64_should_grow_down(const struct h64 *h)
{
	size_t min_count = MIN_LOAD_FACTOR * (h->size_in_groups * GROUP_ENTRIES);
	return h->count < min_count && h->size_in_groups > MIN_SIZE;
}

void
h64_insert_new(struct h64 *h, void *entry)
{
	if (h64_should_grow_up(h))
		h64_grow_up(h);

	h64_prefetch_groups(h);
	uint64_t hash = h64_hash(h, entry);
	uint8_t hint = hash_hint(hash);
	struct find_result result;
	h64_find_empty_entry(h, hash, &result);
	group_insert(result.group, entry, hint, result.index);
	h->count += 1;
}

void
h64_insert(struct h64 *h, void *entry)
{
	DUMP_IF_STORING_STATS(h);

	if (h64_should_grow_up(h))
		h64_grow_up(h);

	h64_prefetch_groups(h);
	uint64_t hash = h64_hash(h, entry);
	uint8_t hint = hash_hint(hash);
	struct find_result result;
	h64_find_entry(h, entry, hash, &result);
	if (result.found) {
		group_update(result.group, entry, result.index);
	} else {
		IF_STORING_STATS(
			h->hint_sum += hint;
			h->hint_count += 1;
		)

		h64_find_empty_entry(h, hash, &result);
		group_insert(result.group, entry, hint, result.index);
		h->count += 1;
	}
}

void *
h64_erase(struct h64 *h, const void *entry)
{
	DUMP_IF_STORING_STATS(h);

	h64_prefetch_groups(h);
	uint64_t hash = h64_hash(h, entry);
	struct find_result result;
	h64_find_entry(h, entry, hash, &result);
	if (result.found) {
		void *ret = group_erase_entry(result.group, result.index);
		h->count -= 1;

		if (h64_should_grow_down(h))
			h64_grow_down(h);

		return ret;
	}

	return NULL;
}
