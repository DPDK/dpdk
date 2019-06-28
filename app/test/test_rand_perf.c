/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Ericsson AB
 */

#include <inttypes.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_random.h>

#include "test.h"

static volatile uint64_t vsum;

#define ITERATIONS (100000000)

enum rand_type {
	rand_type_64
};

static const char *
rand_type_desc(enum rand_type rand_type)
{
	switch (rand_type) {
	case rand_type_64:
		return "Full 64-bit [rte_rand()]";
	default:
		return NULL;
	}
}

static __rte_always_inline void
test_rand_perf_type(enum rand_type rand_type)
{
	uint64_t start;
	uint32_t i;
	uint64_t end;
	uint64_t sum = 0;
	uint64_t op_latency;

	start = rte_rdtsc();

	for (i = 0; i < ITERATIONS; i++) {
		switch (rand_type) {
		case rand_type_64:
			sum += rte_rand();
			break;
		}
	}

	end = rte_rdtsc();

	/* to avoid an optimizing compiler removing the whole loop */
	vsum = sum;

	op_latency = (end - start) / ITERATIONS;

	printf("%s: %"PRId64" TSC cycles/op\n", rand_type_desc(rand_type),
	       op_latency);
}

static int
test_rand_perf(void)
{
	rte_srand(42);

	printf("Pseudo-random number generation latencies:\n");

	test_rand_perf_type(rand_type_64);

	return 0;
}

REGISTER_TEST_COMMAND(rand_perf_autotest, test_rand_perf);
