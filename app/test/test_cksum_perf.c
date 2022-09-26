/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Ericsson AB
 */

#include <stdio.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_random.h>

#include "test.h"

#define NUM_BLOCKS 10
#define ITERATIONS 1000000

static const size_t data_sizes[] = { 20, 21, 100, 101, 1500, 1501 };

static __rte_noinline uint16_t
do_rte_raw_cksum(const void *buf, size_t len)
{
	return rte_raw_cksum(buf, len);
}

static void
init_block(char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		buf[i] = (char)rte_rand();
}

static int
test_cksum_perf_size_alignment(size_t block_size, bool aligned)
{
	char *data[NUM_BLOCKS];
	char *blocks[NUM_BLOCKS];
	unsigned int i;
	uint64_t start;
	uint64_t end;
	/* Floating point to handle low (pseudo-)TSC frequencies */
	double block_latency;
	double byte_latency;
	volatile __rte_unused uint64_t sum = 0;

	for (i = 0; i < NUM_BLOCKS; i++) {
		data[i] = rte_malloc(NULL, block_size + 1, 0);

		if (data[i] == NULL) {
			printf("Failed to allocate memory for block\n");
			return TEST_FAILED;
		}

		init_block(data[i], block_size + 1);

		blocks[i] = aligned ? data[i] : data[i] + 1;
	}

	start = rte_rdtsc();

	for (i = 0; i < ITERATIONS; i++) {
		unsigned int j;
		for (j = 0; j < NUM_BLOCKS; j++)
			sum += do_rte_raw_cksum(blocks[j], block_size);
	}

	end = rte_rdtsc();

	block_latency = (end - start) / (double)(ITERATIONS * NUM_BLOCKS);
	byte_latency = block_latency / block_size;

	printf("%-9s %10zd %19.1f %16.2f\n", aligned ? "Aligned" : "Unaligned",
	       block_size, block_latency, byte_latency);

	for (i = 0; i < NUM_BLOCKS; i++)
		rte_free(data[i]);

	return TEST_SUCCESS;
}

static int
test_cksum_perf_size(size_t block_size)
{
	int rc;

	rc = test_cksum_perf_size_alignment(block_size, true);
	if (rc != TEST_SUCCESS)
		return rc;

	rc = test_cksum_perf_size_alignment(block_size, false);

	return rc;
}

static int
test_cksum_perf(void)
{
	uint16_t i;

	printf("### rte_raw_cksum() performance ###\n");
	printf("Alignment  Block size    TSC cycles/block  TSC cycles/byte\n");

	for (i = 0; i < RTE_DIM(data_sizes); i++) {
		int rc;

		rc = test_cksum_perf_size(data_sizes[i]);
		if (rc != TEST_SUCCESS)
			return rc;
	}

	return TEST_SUCCESS;
}


REGISTER_TEST_COMMAND(cksum_perf_autotest, test_cksum_perf);
