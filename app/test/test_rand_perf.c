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

#define BEST_CASE_BOUND (1<<16)
#define WORST_CASE_BOUND (BEST_CASE_BOUND + 1)

enum rand_type {
	rand_type_64,
	rand_type_float,
	rand_type_bounded_best_case,
	rand_type_bounded_worst_case
};

static const char *
rand_type_desc(enum rand_type rand_type)
{
	switch (rand_type) {
	case rand_type_64:
		return "Full 64-bit [rte_rand()]";
	case rand_type_float:
		return "Floating point [rte_drand()]";
	case rand_type_bounded_best_case:
		return "Bounded average best-case [rte_rand_max()]";
	case rand_type_bounded_worst_case:
		return "Bounded average worst-case [rte_rand_max()]";
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
		case rand_type_float:
			sum += 1000. * rte_drand();
			break;
		case rand_type_bounded_best_case:
			sum += rte_rand_max(BEST_CASE_BOUND);
			break;
		case rand_type_bounded_worst_case:
			sum += rte_rand_max(WORST_CASE_BOUND);
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
	test_rand_perf_type(rand_type_float);
	test_rand_perf_type(rand_type_bounded_best_case);
	test_rand_perf_type(rand_type_bounded_worst_case);

	return 0;
}

REGISTER_PERF_TEST(rand_perf_autotest, test_rand_perf);
