/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_thash.h>

#include "test.h"

#define ITERATIONS	(1 << 15)
#define BATCH_SZ	(1 << 10)

#define IPV4_2_TUPLE_LEN	(8)
#define IPV4_4_TUPLE_LEN	(12)
#define IPV6_2_TUPLE_LEN	(32)
#define IPV6_4_TUPLE_LEN	(36)


static const uint8_t default_rss_key[] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};

enum test_rss_type {
	TEST_SOFTRSS,
	TEST_SOFTRSS_BE,
	TEST_RSS_GFNI
};

static inline uint64_t
run_rss_calc(uint32_t *tuples[BATCH_SZ], enum test_rss_type type, int len,
	const void *key)
{
	int i, j;
	uint64_t start_tsc, end_tsc;
	volatile uint32_t hash = 0;

	start_tsc = rte_rdtsc_precise();
	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < BATCH_SZ; j++) {
			if (type == TEST_SOFTRSS)
				hash ^= rte_softrss(tuples[j], len /
					sizeof(uint32_t), (const uint8_t *)key);
			else if (type == TEST_SOFTRSS_BE)
				hash ^= rte_softrss_be(tuples[j], len /
					sizeof(uint32_t), (const uint8_t *)key);
			else
				hash ^= rte_thash_gfni((const uint64_t *)key,
					(uint8_t *)tuples[j], len);
		}
	}
	end_tsc = rte_rdtsc_precise();

	/* To avoid compiler warnings set hash to used. */
	RTE_SET_USED(hash);

	return end_tsc - start_tsc;
}

static inline uint64_t
run_rss_calc_bulk(uint32_t *tuples[BATCH_SZ], int len, const void *key)
{
	int i;
	uint64_t start_tsc, end_tsc;
	uint32_t bulk_hash[BATCH_SZ] = { 0 };

	start_tsc = rte_rdtsc_precise();
	for (i = 0; i < ITERATIONS; i++)
		rte_thash_gfni_bulk((const uint64_t *)key, len,
			(uint8_t **)tuples, bulk_hash, BATCH_SZ);

	end_tsc = rte_rdtsc_precise();

	return end_tsc - start_tsc;
}

static void
run_thash_test(unsigned int tuple_len)
{
	uint32_t *tuples[BATCH_SZ];
	unsigned int i, j;
	uint32_t len = RTE_ALIGN_CEIL(tuple_len, sizeof(uint32_t));
	uint64_t tsc_diff;

	for (i = 0; i < BATCH_SZ; i++) {
		tuples[i] = rte_zmalloc(NULL, len, 0);
		for (j = 0; j < len / sizeof(uint32_t); j++)
			tuples[i][j] = rte_rand();
	}

	tsc_diff = run_rss_calc(tuples, TEST_SOFTRSS, len, default_rss_key);
	printf("Average rte_softrss() takes \t\t%.1f cycles for key len %d\n",
		(double)(tsc_diff) / (double)(ITERATIONS * BATCH_SZ), len);

	tsc_diff = run_rss_calc(tuples, TEST_SOFTRSS_BE, len,
		default_rss_key);
	printf("Average rte_softrss_be() takes \t\t%.1f cycles for key len %d\n",
		(double)(tsc_diff) / (double)(ITERATIONS * BATCH_SZ), len);

	if (!rte_thash_gfni_supported())
		return;

	uint64_t rss_key_matrixes[RTE_DIM(default_rss_key)];

	rte_thash_complete_matrix(rss_key_matrixes, default_rss_key,
		RTE_DIM(default_rss_key));

	tsc_diff = run_rss_calc(tuples, TEST_RSS_GFNI, len, rss_key_matrixes);
	printf("Average rte_thash_gfni takes \t\t%.1f cycles for key len %d\n",
		(double)(tsc_diff) / (double)(ITERATIONS * BATCH_SZ), len);

	tsc_diff = run_rss_calc_bulk(tuples, len, rss_key_matrixes);
	printf("Average rte_thash_gfni_bulk takes \t%.1f cycles for key len %d\n",
		(double)(tsc_diff) / (double)(ITERATIONS * BATCH_SZ), len);
}

static int
test_thash_perf(void)
{
	run_thash_test(IPV4_2_TUPLE_LEN);
	run_thash_test(IPV4_4_TUPLE_LEN);
	run_thash_test(IPV6_2_TUPLE_LEN);
	run_thash_test(IPV6_4_TUPLE_LEN);

	return 0;
}

REGISTER_PERF_TEST(thash_perf_autotest, test_thash_perf);
