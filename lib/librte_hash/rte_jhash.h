/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_JHASH_H
#define _RTE_JHASH_H

/**
 * @file
 *
 * jhash functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* jhash.h: Jenkins hash support.
 *
 * Copyright (C) 1996 Bob Jenkins (bob_jenkins@burtleburtle.net)
 *
 * http://burtleburtle.net/bob/hash/
 *
 * These are the credits from Bob's sources:
 *
 * lookup2.c, by Bob Jenkins, December 1996, Public Domain.
 * hash(), hash2(), hash3, and mix() are externally useful functions.
 * Routines to test the hash are included if SELF_TEST is defined.
 * You can use this free for any purpose.  It has no warranty.
 *
 * $FreeBSD$
 */

/** @internal Internal function. NOTE: Arguments are modified. */
#define __rte_jhash_mix(a, b, c) do { \
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<<8); \
	c -= a; c -= b; c ^= (b>>13); \
	a -= b; a -= c; a ^= (c>>12); \
	b -= c; b -= a; b ^= (a<<16); \
	c -= a; c -= b; c ^= (b>>5); \
	a -= b; a -= c; a ^= (c>>3); \
	b -= c; b -= a; b ^= (a<<10); \
	c -= a; c -= b; c ^= (b>>15); \
} while (0)

/** The golden ratio: an arbitrary value. */
#define RTE_JHASH_GOLDEN_RATIO      0x9e3779b9

/**
 * The most generic version, hashes an arbitrary sequence
 * of bytes.  No alignment or length assumptions are made about
 * the input key.
 *
 * @param key
 *   Key to calculate hash of.
 * @param length
 *   Length of key in bytes.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash(const void *key, uint32_t length, uint32_t initval)
{
	uint32_t a, b, c, len;
	const uint8_t *k = (const uint8_t *)key;
	const uint32_t *k32 = (const uint32_t *)key;

	len = length;
	a = b = RTE_JHASH_GOLDEN_RATIO;
	c = initval;

	while (len >= 12) {
		a += k32[0];
		b += k32[1];
		c += k32[2];

		__rte_jhash_mix(a,b,c);

		k += (3 * sizeof(uint32_t)), k32 += 3;
		len -= (3 * sizeof(uint32_t));
	}

	c += length;
	switch (len) {
		case 11: c += ((uint32_t)k[10] << 24);
		case 10: c += ((uint32_t)k[9] << 16);
		case 9 : c += ((uint32_t)k[8] << 8);
		case 8 : b += ((uint32_t)k[7] << 24);
		case 7 : b += ((uint32_t)k[6] << 16);
		case 6 : b += ((uint32_t)k[5] << 8);
		case 5 : b += k[4];
		case 4 : a += ((uint32_t)k[3] << 24);
		case 3 : a += ((uint32_t)k[2] << 16);
		case 2 : a += ((uint32_t)k[1] << 8);
		case 1 : a += k[0];
		default: break;
	};

	__rte_jhash_mix(a,b,c);

	return c;
}

/**
 * A special optimized version that handles 1 or more of uint32_ts.
 * The length parameter here is the number of uint32_ts in the key.
 *
 * @param k
 *   Key to calculate hash of.
 * @param length
 *   Length of key in units of 4 bytes.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash2(const uint32_t *k, uint32_t length, uint32_t initval)
{
	uint32_t a, b, c, len;

	a = b = RTE_JHASH_GOLDEN_RATIO;
	c = initval;
	len = length;

	while (len >= 3) {
		a += k[0];
		b += k[1];
		c += k[2];
		__rte_jhash_mix(a, b, c);
		k += 3; len -= 3;
	}

	c += length * 4;

	switch (len) {
		case 2 : b += k[1];
		case 1 : a += k[0];
		default: break;
	};

	__rte_jhash_mix(a,b,c);

	return c;
}


/**
 * A special ultra-optimized versions that knows it is hashing exactly
 * 3 words.
 *
 * @param a
 *   First word to calcuate hash of.
 * @param b
 *   Second word to calcuate hash of.
 * @param c
 *   Third word to calcuate hash of.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash_3words(uint32_t a, uint32_t b, uint32_t c, uint32_t initval)
{
	a += RTE_JHASH_GOLDEN_RATIO;
	b += RTE_JHASH_GOLDEN_RATIO;
	c += initval;

	__rte_jhash_mix(a, b, c);

	/*
	 * NOTE: In particular the "c += length; __rte_jhash_mix(a,b,c);"
	 *       normally done at the end is not done here.
	 */
	return c;
}

/**
 * A special ultra-optimized versions that knows it is hashing exactly
 * 2 words.
 *
 * @param a
 *   First word to calcuate hash of.
 * @param b
 *   Second word to calcuate hash of.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash_2words(uint32_t a, uint32_t b, uint32_t initval)
{
	return rte_jhash_3words(a, b, 0, initval);
}

/**
 * A special ultra-optimized versions that knows it is hashing exactly
 * 1 word.
 *
 * @param a
 *   Word to calcuate hash of.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash_1word(uint32_t a, uint32_t initval)
{
	return rte_jhash_3words(a, 0, 0, initval);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_JHASH_H */
