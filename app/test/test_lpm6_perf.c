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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_memory.h>
#include <rte_lpm6.h>

#include "test.h"
#include "test_lpm6_data.h"

#define TEST_LPM_ASSERT(cond) do {                                            \
	if (!(cond)) {                                                        \
		printf("Error at line %d: \n", __LINE__);                     \
		return -1;                                                    \
	}                                                                     \
} while(0)

#define ITERATIONS (1 << 10)
#define BATCH_SIZE 100000
#define NUMBER_TBL8S                                           (1 << 16)

static void
print_route_distribution(const struct rules_tbl_entry *table, uint32_t n)
{
	unsigned i, j;

	printf("Route distribution per prefix width: \n");
	printf("DEPTH    QUANTITY (PERCENT)\n");
	printf("--------------------------- \n");

	/* Count depths. */
	for(i = 1; i <= 128; i++) {
		unsigned depth_counter = 0;
		double percent_hits;

		for (j = 0; j < n; j++)
			if (table[j].depth == (uint8_t) i)
				depth_counter++;

		percent_hits = ((double)depth_counter)/((double)n) * 100;
		printf("%.2u%15u (%.2f)\n", i, depth_counter, percent_hits);
	}
	printf("\n");
}

static int
test_lpm6_perf(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint64_t begin, total_time;
	unsigned i, j;
	uint8_t next_hop_add = 0xAA, next_hop_return = 0;
	int status = 0;
	int64_t count = 0;

	config.max_rules = 1000000;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	rte_srand(rte_rdtsc());

	printf("No. routes = %u\n", (unsigned) NUM_ROUTE_ENTRIES);

	print_route_distribution(large_route_table, (uint32_t) NUM_ROUTE_ENTRIES);

	/* Only generate IPv6 address of each item in large IPS table,
	 * here next_hop is not needed.
	 */
	generate_large_ips_table(0);

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Measure add. */
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		if (rte_lpm6_add(lpm, large_route_table[i].ip,
				large_route_table[i].depth, next_hop_add) == 0)
			status++;
	}
	/* End Timer. */
	total_time = rte_rdtsc() - begin;

	printf("Unique added entries = %d\n", status);
	printf("Average LPM Add: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	/* Measure single Lookup */
	total_time = 0;
	count = 0;

	for (i = 0; i < ITERATIONS; i ++) {
		begin = rte_rdtsc();

		for (j = 0; j < NUM_IPS_ENTRIES; j ++) {
			if (rte_lpm6_lookup(lpm, large_ips_table[j].ip,
					&next_hop_return) != 0)
				count++;
		}

		total_time += rte_rdtsc() - begin;

	}
	printf("Average LPM Lookup: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Measure bulk Lookup */
	total_time = 0;
	count = 0;

	uint8_t ip_batch[NUM_IPS_ENTRIES][16];
	int16_t next_hops[NUM_IPS_ENTRIES];

	for (i = 0; i < NUM_IPS_ENTRIES; i++)
		memcpy(ip_batch[i], large_ips_table[i].ip, 16);

	for (i = 0; i < ITERATIONS; i ++) {

		/* Lookup per batch */
		begin = rte_rdtsc();
		rte_lpm6_lookup_bulk_func(lpm, ip_batch, next_hops, NUM_IPS_ENTRIES);
		total_time += rte_rdtsc() - begin;

		for (j = 0; j < NUM_IPS_ENTRIES; j++)
			if (next_hops[j] < 0)
				count++;
	}
	printf("BULK LPM Lookup: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Delete */
	status = 0;
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		/* rte_lpm_delete(lpm, ip, depth) */
		status += rte_lpm6_delete(lpm, large_route_table[i].ip,
				large_route_table[i].depth);
	}

	total_time += rte_rdtsc() - begin;

	printf("Average LPM Delete: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	rte_lpm6_delete_all(lpm);
	rte_lpm6_free(lpm);

	return 0;
}

REGISTER_TEST_COMMAND(lpm6_perf_autotest, test_lpm6_perf);
