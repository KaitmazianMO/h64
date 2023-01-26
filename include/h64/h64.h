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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/** Entries comparison function. Must return 0 if entries are equal, non-0 otherwise. */
typedef int (*h64_equals_f)(const void *lhs, const void *rhs);
/** Entries hash function. Must have good distribution for all bits. */
typedef uint64_t (*h64_hasher_f)(const void *entry, uint64_t seed);

enum H64_INTERNAL_CONSTANTS {
	H64_INTERNAL_GROUP_ENTRIES = 7,
};

/**
 * Group of entries with some metadata.
 *
 * Entries are stored in groups for 2 purposes:
 * 1) Improving data locality and storing processing data in L1 cache line.
 * 2) Avoiding tombstones for entries. We only need to know if a group has
 *    ever been full to stop or continue probing. That's only 1 bit for a group.
 */
struct h64_group {
	/*
	 * 0 - 7 bits determine whether the entry is present (1) or not (0).
	 * the 8th bit is set to 1 if group has ever been full and 0 otherwise.
	 * The last bit is used as a condition to stop or continue probing:
	 * if an entry is not found and the group was never full,
	 * the entry can't be in the next group. If the group was full the entry
	 * may be in the next group.
	 */
	uint8_t status;
	/*
	 * Comparison hints for entries. Keeps 1 byte from a hash of the entry.
	 * Helps to avoid calling of comparison function for entries with
	 * different hashes.
	 */
	uint8_t hints[H64_INTERNAL_GROUP_ENTRIES];
	/* The entries. */
	void *entries[H64_INTERNAL_GROUP_ENTRIES];
};

static_assert(sizeof(struct h64_group) == 64,
	      "Group size must be equal to L1 cache line size");

/**
 * Flat hash table.
 */
struct h64 {
	/** Hashing and comparison functions for entries. */
	h64_hasher_f  hasher;
	h64_equals_f  equals;
	/** Hash seed, which is used to randomize hash values. */
	uint64_t seed;
	/** Groups of entries and its quantity. */
	struct h64_group *groups;
	size_t size_in_groups;
	/** Number of entries presented in the table. */
	size_t count;

#ifdef H64_STORE_STATISTICS
	/** hint_sum / hint_count must be close to 255 / 2 */
	uint64_t hint_sum;
	uint64_t hint_count;
	/** gives average and max probe length for finds */
	uint64_t find_count;
	uint64_t find_probe_count;
	uint64_t find_max_probe_count;

	/** gives average and max probe length for inserts  */
	uint64_t insert_count;
	uint64_t insert_probe_count;
	uint64_t insert_max_probe_count;

	/** gives average hitrate */
	uint64_t compare_count;
	uint64_t equal_count;
#endif
};

#define h64_for_each(ht, name)						       \
	void *(name) = NULL;						       \
	for (size_t i_ = 0; i_ < (ht)->size_in_groups; ++i_)		       \
		for (size_t j_ = 0; j_ < H64_INTERNAL_GROUP_ENTRIES; ++j_)     \
			if (((name) = (ht)->groups[i_].entries[j_]) != NULL)   \

/**
 * Constructor for a table.
 * Hahs function must give every value from 0 to 2^64 - 1 with the same
 * propability, otherwise expect performance issues. h64_byte_hash - suitable
 * in most cases.
 */
struct h64 *
h64_create(h64_hasher_f hasher, h64_equals_f equals);

/** Destructor for a table. */
void
h64_destroy(struct h64 *h);

/**
 * Insert an entry in the table.
 * While entry is inside the table, hash(entry) must remain constant.
 * Otherwise you likely won't find it before resizing.
 */
void
h64_insert(struct h64 *h, void *entry);

/**
 * Insert a new entry in the table. If the inserting entry is already presented
 * in the table will store both entries until you erase one of them.
 * While entry is inside the table, hash(entry) must remain constant.
 * Otherwise you likely won't find it before resizing.
 */
void
h64_insert_new(struct h64 *h, void *entry);

/**
 * Reserve memory for size entries.
 * Adding size entries will not result in memory allocations.
 */
void
h64_reserve(struct h64 *h, size_t size);

/**
 * Find an entry in the table.
 * You can use any entry which has the same hash and equals to
 * the entry you are looking for.
 * Return an equal entry from the table.
 */
void *
h64_find(const struct h64 *h, const void *entry);

/**
 * Erase equal entry from the table without calling free() for the erased entry.
 * Erased entry is returned so that you can properly release it.
 */
void *
h64_erase(struct h64 *h, const void *entry);

/** Number of entries presented in the table. */
static inline size_t
h64_count(const struct h64 *h)
{
	return h->count;
}

/** It's Murmurhash. Suitable for the table and yields good results. */
static inline uint64_t
h64_byte_hash(const void *key, int len, uint64_t seed)
{
	const uint64_t m = 0xc6a4a7935bd1e995ULL;
	const int r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t *data = (const uint64_t *)key;
	const uint64_t *end = data + (len / 8);

	while (data != end) {
		uint64_t k = *data++;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	const unsigned char * data2 = (const unsigned char*)data;

	switch (len & 7) {
	case 7: h ^= (uint64_t)data2[6] << 48;
	case 6: h ^= (uint64_t)data2[5] << 40;
	case 5: h ^= (uint64_t)data2[4] << 32;
	case 4: h ^= (uint64_t)data2[3] << 24;
	case 3: h ^= (uint64_t)data2[2] << 16;
	case 2: h ^= (uint64_t)data2[1] << 8;
	case 1: h ^= (uint64_t)data2[0];
		h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}

#ifdef __cplusplus
}  // extern "C"
#endif
