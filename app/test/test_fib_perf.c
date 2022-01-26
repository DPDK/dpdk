/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_branch_prediction.h>
#include <rte_ip.h>

#include "test.h"
#include "test_xmmt_ops.h"

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_fib_perf(void)
{
	printf("fib_perf not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_fib.h>

#define TEST_FIB_ASSERT(cond) do {				\
	if (!(cond)) {						\
		printf("Error at line %d:\n", __LINE__);	\
		return -1;					\
	}                                                       \
} while (0)

#define ITERATIONS (1 << 10)
#define BATCH_SIZE (1 << 12)
#define BULK_SIZE 32

#define MAX_RULE_NUM (1200000)

struct route_rule {
	uint32_t ip;
	uint8_t depth;
};

static struct route_rule large_route_table[MAX_RULE_NUM];

static uint32_t num_route_entries;
#define NUM_ROUTE_ENTRIES num_route_entries

enum {
	IP_CLASS_A,
	IP_CLASS_B,
	IP_CLASS_C
};
#define RTE_FIB_MAX_DEPTH	32
/* struct route_rule_count defines the total number of rules in following a/b/c
 * each item in a[]/b[]/c[] is the number of common IP address class A/B/C, not
 * including the ones for private local network.
 */
struct route_rule_count {
	uint32_t a[RTE_FIB_MAX_DEPTH];
	uint32_t b[RTE_FIB_MAX_DEPTH];
	uint32_t c[RTE_FIB_MAX_DEPTH];
};

/* All following numbers of each depth of each common IP class are just
 * got from previous large constant table in app/test/test_lpm_routes.h .
 * In order to match similar performance, they keep same depth and IP
 * address coverage as previous constant table. These numbers don't
 * include any private local IP address. As previous large const rule
 * table was just dumped from a real router, there are no any IP address
 * in class C or D.
 */
static struct route_rule_count rule_count = {
	.a = { /* IP class A in which the most significant bit is 0 */
		    0, /* depth =  1 */
		    0, /* depth =  2 */
		    1, /* depth =  3 */
		    0, /* depth =  4 */
		    2, /* depth =  5 */
		    1, /* depth =  6 */
		    3, /* depth =  7 */
		  185, /* depth =  8 */
		   26, /* depth =  9 */
		   16, /* depth = 10 */
		   39, /* depth = 11 */
		  144, /* depth = 12 */
		  233, /* depth = 13 */
		  528, /* depth = 14 */
		  866, /* depth = 15 */
		 3856, /* depth = 16 */
		 3268, /* depth = 17 */
		 5662, /* depth = 18 */
		17301, /* depth = 19 */
		22226, /* depth = 20 */
		11147, /* depth = 21 */
		16746, /* depth = 22 */
		17120, /* depth = 23 */
		77578, /* depth = 24 */
		  401, /* depth = 25 */
		  656, /* depth = 26 */
		 1107, /* depth = 27 */
		 1121, /* depth = 28 */
		 2316, /* depth = 29 */
		  717, /* depth = 30 */
		   10, /* depth = 31 */
		   66  /* depth = 32 */
	},
	.b = { /* IP class A in which the most 2 significant bits are 10 */
		    0, /* depth =  1 */
		    0, /* depth =  2 */
		    0, /* depth =  3 */
		    0, /* depth =  4 */
		    1, /* depth =  5 */
		    1, /* depth =  6 */
		    1, /* depth =  7 */
		    3, /* depth =  8 */
		    3, /* depth =  9 */
		   30, /* depth = 10 */
		   25, /* depth = 11 */
		  168, /* depth = 12 */
		  305, /* depth = 13 */
		  569, /* depth = 14 */
		 1129, /* depth = 15 */
		50800, /* depth = 16 */
		 1645, /* depth = 17 */
		 1820, /* depth = 18 */
		 3506, /* depth = 19 */
		 3258, /* depth = 20 */
		 3424, /* depth = 21 */
		 4971, /* depth = 22 */
		 6885, /* depth = 23 */
		39771, /* depth = 24 */
		  424, /* depth = 25 */
		  170, /* depth = 26 */
		  433, /* depth = 27 */
		   92, /* depth = 28 */
		  366, /* depth = 29 */
		  377, /* depth = 30 */
		    2, /* depth = 31 */
		  200  /* depth = 32 */
	},
	.c = { /* IP class A in which the most 3 significant bits are 110 */
		     0, /* depth =  1 */
		     0, /* depth =  2 */
		     0, /* depth =  3 */
		     0, /* depth =  4 */
		     0, /* depth =  5 */
		     0, /* depth =  6 */
		     0, /* depth =  7 */
		    12, /* depth =  8 */
		     8, /* depth =  9 */
		     9, /* depth = 10 */
		    33, /* depth = 11 */
		    69, /* depth = 12 */
		   237, /* depth = 13 */
		  1007, /* depth = 14 */
		  1717, /* depth = 15 */
		 14663, /* depth = 16 */
		  8070, /* depth = 17 */
		 16185, /* depth = 18 */
		 48261, /* depth = 19 */
		 36870, /* depth = 20 */
		 33960, /* depth = 21 */
		 50638, /* depth = 22 */
		 61422, /* depth = 23 */
		466549, /* depth = 24 */
		  1829, /* depth = 25 */
		  4824, /* depth = 26 */
		  4927, /* depth = 27 */
		  5914, /* depth = 28 */
		 10254, /* depth = 29 */
		  4905, /* depth = 30 */
		     1, /* depth = 31 */
		   716  /* depth = 32 */
	}
};

static void generate_random_rule_prefix(uint32_t ip_class, uint8_t depth)
{
/* IP address class A, the most significant bit is 0 */
#define IP_HEAD_MASK_A			0x00000000
#define IP_HEAD_BIT_NUM_A		1

/* IP address class B, the most significant 2 bits are 10 */
#define IP_HEAD_MASK_B			0x80000000
#define IP_HEAD_BIT_NUM_B		2

/* IP address class C, the most significant 3 bits are 110 */
#define IP_HEAD_MASK_C			0xC0000000
#define IP_HEAD_BIT_NUM_C		3

	uint32_t class_depth;
	uint32_t range;
	uint32_t mask;
	uint32_t step;
	uint32_t start;
	uint32_t fixed_bit_num;
	uint32_t ip_head_mask;
	uint32_t rule_num;
	uint32_t k;
	struct route_rule *ptr_rule;

	if (ip_class == IP_CLASS_A) {        /* IP Address class A */
		fixed_bit_num = IP_HEAD_BIT_NUM_A;
		ip_head_mask = IP_HEAD_MASK_A;
		rule_num = rule_count.a[depth - 1];
	} else if (ip_class == IP_CLASS_B) { /* IP Address class B */
		fixed_bit_num = IP_HEAD_BIT_NUM_B;
		ip_head_mask = IP_HEAD_MASK_B;
		rule_num = rule_count.b[depth - 1];
	} else {                             /* IP Address class C */
		fixed_bit_num = IP_HEAD_BIT_NUM_C;
		ip_head_mask = IP_HEAD_MASK_C;
		rule_num = rule_count.c[depth - 1];
	}

	if (rule_num == 0)
		return;

	/* the number of rest bits which don't include the most significant
	 * fixed bits for this IP address class
	 */
	class_depth = depth - fixed_bit_num;

	/* range is the maximum number of rules for this depth and
	 * this IP address class
	 */
	range = 1 << class_depth;

	/* only mask the most depth significant generated bits
	 * except fixed bits for IP address class
	 */
	mask = range - 1;

	/* Widen coverage of IP address in generated rules */
	if (range <= rule_num)
		step = 1;
	else
		step = round((double)range / rule_num);

	/* Only generate rest bits except the most significant
	 * fixed bits for IP address class
	 */
	start = lrand48() & mask;
	ptr_rule = &large_route_table[num_route_entries];
	for (k = 0; k < rule_num; k++) {
		ptr_rule->ip = (start << (RTE_FIB_MAX_DEPTH - depth))
			| ip_head_mask;
		ptr_rule->depth = depth;
		ptr_rule++;
		start = (start + step) & mask;
	}
	num_route_entries += rule_num;
}

static void insert_rule_in_random_pos(uint32_t ip, uint8_t depth)
{
	uint32_t pos;
	int try_count = 0;
	struct route_rule tmp;

	do {
		pos = lrand48();
		try_count++;
	} while ((try_count < 10) && (pos > num_route_entries));

	if ((pos > num_route_entries) || (pos >= MAX_RULE_NUM))
		pos = num_route_entries >> 1;

	tmp = large_route_table[pos];
	large_route_table[pos].ip = ip;
	large_route_table[pos].depth = depth;
	if (num_route_entries < MAX_RULE_NUM)
		large_route_table[num_route_entries++] = tmp;
}

static void generate_large_route_rule_table(void)
{
	uint32_t ip_class;
	uint8_t  depth;

	num_route_entries = 0;
	memset(large_route_table, 0, sizeof(large_route_table));

	for (ip_class = IP_CLASS_A; ip_class <= IP_CLASS_C; ip_class++) {
		for (depth = 1; depth <= RTE_FIB_MAX_DEPTH; depth++)
			generate_random_rule_prefix(ip_class, depth);
	}

	/* Add following rules to keep same as previous large constant table,
	 * they are 4 rules with private local IP address and 1 all-zeros prefix
	 * with depth = 8.
	 */
	insert_rule_in_random_pos(RTE_IPV4(0, 0, 0, 0), 8);
	insert_rule_in_random_pos(RTE_IPV4(10, 2, 23, 147), 32);
	insert_rule_in_random_pos(RTE_IPV4(192, 168, 100, 10), 24);
	insert_rule_in_random_pos(RTE_IPV4(192, 168, 25, 100), 24);
	insert_rule_in_random_pos(RTE_IPV4(192, 168, 129, 124), 32);
}

static void
print_route_distribution(const struct route_rule *table, uint32_t n)
{
	unsigned int i, j;

	printf("Route distribution per prefix width:\n");
	printf("DEPTH    QUANTITY (PERCENT)\n");
	printf("---------------------------\n");

	/* Count depths. */
	for (i = 1; i <= 32; i++) {
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

static int
test_fib_perf(void)
{
	struct rte_fib *fib = NULL;
	struct rte_fib_conf config;

	config.max_routes = 2000000;
	config.rib_ext_sz = 0;
	config.type = RTE_FIB_DIR24_8;
	config.default_nh = 0;
	config.dir24_8.nh_sz = RTE_FIB_DIR24_8_4B;
	config.dir24_8.num_tbl8 = 65535;
	uint64_t begin, total_time;
	unsigned int i, j;
	uint32_t next_hop_add = 0xAA;
	int status = 0;
	int64_t count = 0;

	rte_srand(rte_rdtsc());

	generate_large_route_rule_table();

	printf("No. routes = %u\n", (unsigned int) NUM_ROUTE_ENTRIES);

	print_route_distribution(large_route_table,
		(uint32_t) NUM_ROUTE_ENTRIES);

	fib = rte_fib_create(__func__, SOCKET_ID_ANY, &config);
	TEST_FIB_ASSERT(fib != NULL);

	/* Measure add. */
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		if (rte_fib_add(fib, large_route_table[i].ip,
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
	for (i = 0; i < ITERATIONS; i++) {
		static uint32_t ip_batch[BATCH_SIZE];
		uint64_t next_hops[BULK_SIZE];

		/* Create array of random IP addresses */
		for (j = 0; j < BATCH_SIZE; j++)
			ip_batch[j] = rte_rand();

		/* Lookup per batch */
		begin = rte_rdtsc();
		for (j = 0; j < BATCH_SIZE; j += BULK_SIZE) {
			uint32_t k;
			rte_fib_lookup_bulk(fib, &ip_batch[j], next_hops,
				BULK_SIZE);
			for (k = 0; k < BULK_SIZE; k++)
				if (unlikely(!(next_hops[k] != 0)))
					count++;
		}

		total_time += rte_rdtsc() - begin;
	}
	printf("BULK FIB Lookup: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Delete */
	status = 0;
	begin = rte_rdtsc();
	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		/* rte_lpm_delete(lpm, ip, depth) */
		status += rte_fib_delete(fib, large_route_table[i].ip,
				large_route_table[i].depth);
	}

	total_time += rte_rdtsc() - begin;

	printf("Average FIB Delete: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	rte_fib_free(fib);

	return 0;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(fib_perf_autotest, test_fib_perf);
