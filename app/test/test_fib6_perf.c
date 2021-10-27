/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_memory.h>
#include <rte_fib6.h>

#include "test.h"
#include "test_lpm6_data.h"

#define TEST_FIB_ASSERT(cond) do {				\
	if (!(cond)) {						\
		printf("Error at line %d:\n", __LINE__);	\
		return -1;					\
	}							\
} while (0)

#define ITERATIONS (1 << 10)
#define BATCH_SIZE 100000
#define NUMBER_TBL8S                                           (1 << 16)

static void
print_route_distribution(const struct rules_tbl_entry *table, uint32_t n)
{
	unsigned int i, j;

	printf("Route distribution per prefix width:\n");
	printf("DEPTH    QUANTITY (PERCENT)\n");
	printf("---------------------------\n");

	/* Count depths. */
	for (i = 1; i <= 128; i++) {
		unsigned int depth_counter = 0;
		double percent_hits;

		for (j = 0; j < n; j++)
			if (table[j].depth == (uint8_t) i)
				depth_counter++;

		percent_hits = ((double)depth_counter)/((double)n) * 100;
		printf("%.2u%15u (%.2f)\n", i, depth_counter, percent_hits);
	}
	printf("\n");
}

static inline uint8_t
bits_in_nh(uint8_t nh_sz)
{
	return 8 * (1 << nh_sz);
}

static inline uint64_t
get_max_nh(uint8_t nh_sz)
{
	return ((1ULL << (bits_in_nh(nh_sz) - 1)) - 1);
}

static int
test_fib6_perf(void)
{
	struct rte_fib6 *fib = NULL;
	struct rte_fib6_conf conf;
	uint64_t begin, total_time;
	unsigned int i, j;
	uint64_t next_hop_add;
	int status = 0;
	int64_t count = 0;
	uint8_t ip_batch[NUM_IPS_ENTRIES][16];
	uint64_t next_hops[NUM_IPS_ENTRIES];

	conf.type = RTE_FIB6_TRIE;
	conf.default_nh = 0;
	conf.max_routes = 1000000;
	conf.rib_ext_sz = 0;
	conf.trie.nh_sz = RTE_FIB6_TRIE_4B;
	conf.trie.num_tbl8 = RTE_MIN(get_max_nh(conf.trie.nh_sz), 1000000U);

	rte_srand(rte_rdtsc());

	printf("No. routes = %u\n", (unsigned int) NUM_ROUTE_ENTRIES);

	print_route_distribution(large_route_table,
		(uint32_t)NUM_ROUTE_ENTRIES);

	/* Only generate IPv6 address of each item in large IPS table,
	 * here next_hop is not needed.
	 */
	generate_large_ips_table(0);

	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &conf);
	TEST_FIB_ASSERT(fib != NULL);

	/* Measure add. */
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		next_hop_add = (i & ((1 << 14) - 1)) + 1;
		if (rte_fib6_add(fib, large_route_table[i].ip,
				large_route_table[i].depth, next_hop_add) == 0)
			status++;
	}
	/* End Timer. */
	total_time = rte_rdtsc() - begin;

	printf("Unique added entries = %d\n", status);
	printf("Average FIB Add: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	/* Measure bulk Lookup */
	total_time = 0;
	count = 0;

	for (i = 0; i < NUM_IPS_ENTRIES; i++)
		memcpy(ip_batch[i], large_ips_table[i].ip, 16);

	for (i = 0; i < ITERATIONS; i++) {

		/* Lookup per batch */
		begin = rte_rdtsc();
		rte_fib6_lookup_bulk(fib, ip_batch, next_hops, NUM_IPS_ENTRIES);
		total_time += rte_rdtsc() - begin;

		for (j = 0; j < NUM_IPS_ENTRIES; j++)
			if (next_hops[j] == 0)
				count++;
	}
	printf("BULK FIB Lookup: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Delete */
	status = 0;
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		/* rte_fib_delete(fib, ip, depth) */
		status += rte_fib6_delete(fib, large_route_table[i].ip,
				large_route_table[i].depth);
	}

	total_time = rte_rdtsc() - begin;

	printf("Average FIB Delete: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	rte_fib6_free(fib);

	return 0;
}

REGISTER_TEST_COMMAND(fib6_perf_autotest, test_fib6_perf);
