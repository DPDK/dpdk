/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2020 Arm Limited
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_branch_prediction.h>
#include <rte_malloc.h>
#include <rte_ip.h>
#include <rte_lpm.h>
#include <rte_spinlock.h>

#include "test.h"
#include "test_xmmt_ops.h"

struct rte_lpm *lpm;
static struct rte_rcu_qsbr *rv;
static volatile uint8_t writer_done;
static volatile uint32_t thr_id;
static uint64_t gwrite_cycles;
static uint32_t num_writers;

/* LPM APIs are not thread safe, use spinlock */
static rte_spinlock_t lpm_lock = RTE_SPINLOCK_INITIALIZER;

/* Report quiescent state interval every 1024 lookups. Larger critical
 * sections in reader will result in writer polling multiple times.
 */
#define QSBR_REPORTING_INTERVAL 1024

#define TEST_LPM_ASSERT(cond) do {                                            \
	if (!(cond)) {                                                        \
		printf("Error at line %d: \n", __LINE__);                     \
		return -1;                                                    \
	}                                                                     \
} while(0)

#define ITERATIONS (1 << 10)
#define RCU_ITERATIONS 10
#define BATCH_SIZE (1 << 12)
#define BULK_SIZE 32

#define MAX_RULE_NUM (1200000)

struct route_rule {
	uint32_t ip;
	uint8_t depth;
};

static struct route_rule large_route_table[MAX_RULE_NUM];
/* Route table for routes with depth > 24 */
struct route_rule large_ldepth_route_table[MAX_RULE_NUM];

static uint32_t num_route_entries;
static uint32_t num_ldepth_route_entries;
#define NUM_ROUTE_ENTRIES num_route_entries
#define NUM_LDEPTH_ROUTE_ENTRIES num_ldepth_route_entries

#define TOTAL_WRITES (RCU_ITERATIONS * NUM_LDEPTH_ROUTE_ENTRIES)

enum {
	IP_CLASS_A,
	IP_CLASS_B,
	IP_CLASS_C
};

/* struct route_rule_count defines the total number of rules in following a/b/c
 * each item in a[]/b[]/c[] is the number of common IP address class A/B/C, not
 * including the ones for private local network.
 */
struct route_rule_count {
	uint32_t a[RTE_LPM_MAX_DEPTH];
	uint32_t b[RTE_LPM_MAX_DEPTH];
	uint32_t c[RTE_LPM_MAX_DEPTH];
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
	struct route_rule *ptr_rule, *ptr_ldepth_rule;

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
	start = rte_rand() & mask;
	ptr_rule = &large_route_table[num_route_entries];
	ptr_ldepth_rule = &large_ldepth_route_table[num_ldepth_route_entries];
	for (k = 0; k < rule_num; k++) {
		ptr_rule->ip = (start << (RTE_LPM_MAX_DEPTH - depth))
			| ip_head_mask;
		ptr_rule->depth = depth;
		/* If the depth of the route is more than 24, store it
		 * in another table as well.
		 */
		if (depth > 24) {
			ptr_ldepth_rule->ip = ptr_rule->ip;
			ptr_ldepth_rule->depth = ptr_rule->depth;
			ptr_ldepth_rule++;
			num_ldepth_route_entries++;
		}
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
		pos = rte_rand();
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
	num_ldepth_route_entries = 0;
	memset(large_route_table, 0, sizeof(large_route_table));

	for (ip_class = IP_CLASS_A; ip_class <= IP_CLASS_C; ip_class++) {
		for (depth = 1; depth <= RTE_LPM_MAX_DEPTH; depth++) {
			generate_random_rule_prefix(ip_class, depth);
		}
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
	unsigned i, j;

	printf("Route distribution per prefix width: \n");
	printf("DEPTH    QUANTITY (PERCENT)\n");
	printf("--------------------------- \n");

	/* Count depths. */
	for (i = 1; i <= 32; i++) {
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

/* Check condition and return an error if true. */
static uint16_t enabled_core_ids[RTE_MAX_LCORE];
static unsigned int num_cores;

/* Simple way to allocate thread ids in 0 to RTE_MAX_LCORE space */
static inline uint32_t
alloc_thread_id(void)
{
	uint32_t tmp_thr_id;

	tmp_thr_id = __atomic_fetch_add(&thr_id, 1, __ATOMIC_RELAXED);
	if (tmp_thr_id >= RTE_MAX_LCORE)
		printf("Invalid thread id %u\n", tmp_thr_id);

	return tmp_thr_id;
}

/*
 * Reader thread using rte_lpm data structure without RCU.
 */
static int
test_lpm_reader(void *arg)
{
	int i;
	uint32_t ip_batch[QSBR_REPORTING_INTERVAL];
	uint32_t next_hop_return = 0;

	RTE_SET_USED(arg);
	do {
		for (i = 0; i < QSBR_REPORTING_INTERVAL; i++)
			ip_batch[i] = rte_rand();

		for (i = 0; i < QSBR_REPORTING_INTERVAL; i++)
			rte_lpm_lookup(lpm, ip_batch[i], &next_hop_return);

	} while (!writer_done);

	return 0;
}

/*
 * Reader thread using rte_lpm data structure with RCU.
 */
static int
test_lpm_rcu_qsbr_reader(void *arg)
{
	int i;
	uint32_t thread_id = alloc_thread_id();
	uint32_t ip_batch[QSBR_REPORTING_INTERVAL];
	uint32_t next_hop_return = 0;

	RTE_SET_USED(arg);
	/* Register this thread to report quiescent state */
	rte_rcu_qsbr_thread_register(rv, thread_id);
	rte_rcu_qsbr_thread_online(rv, thread_id);

	do {
		for (i = 0; i < QSBR_REPORTING_INTERVAL; i++)
			ip_batch[i] = rte_rand();

		for (i = 0; i < QSBR_REPORTING_INTERVAL; i++)
			rte_lpm_lookup(lpm, ip_batch[i], &next_hop_return);

		/* Update quiescent state */
		rte_rcu_qsbr_quiescent(rv, thread_id);
	} while (!writer_done);

	rte_rcu_qsbr_thread_offline(rv, thread_id);
	rte_rcu_qsbr_thread_unregister(rv, thread_id);

	return 0;
}

/*
 * Writer thread using rte_lpm data structure with RCU.
 */
static int
test_lpm_rcu_qsbr_writer(void *arg)
{
	unsigned int i, j, si, ei;
	uint64_t begin, total_cycles;
	uint32_t next_hop_add = 0xAA;
	uint8_t pos_core = (uint8_t)((uintptr_t)arg);

	si = (pos_core * NUM_LDEPTH_ROUTE_ENTRIES) / num_writers;
	ei = ((pos_core + 1) * NUM_LDEPTH_ROUTE_ENTRIES) / num_writers;

	/* Measure add/delete. */
	begin = rte_rdtsc_precise();
	for (i = 0; i < RCU_ITERATIONS; i++) {
		/* Add all the entries */
		for (j = si; j < ei; j++) {
			rte_spinlock_lock(&lpm_lock);
			if (rte_lpm_add(lpm, large_ldepth_route_table[j].ip,
					large_ldepth_route_table[j].depth,
					next_hop_add) != 0) {
				printf("Failed to add iteration %d, route# %d\n",
					i, j);
				goto error;
			}
			rte_spinlock_unlock(&lpm_lock);
		}

		/* Delete all the entries */
		for (j = si; j < ei; j++) {
			rte_spinlock_lock(&lpm_lock);
			if (rte_lpm_delete(lpm, large_ldepth_route_table[j].ip,
				large_ldepth_route_table[j].depth) != 0) {
				printf("Failed to delete iteration %d, route# %d\n",
					i, j);
				goto error;
			}
			rte_spinlock_unlock(&lpm_lock);
		}
	}

	total_cycles = rte_rdtsc_precise() - begin;

	__atomic_fetch_add(&gwrite_cycles, total_cycles, __ATOMIC_RELAXED);

	return 0;

error:
	rte_spinlock_unlock(&lpm_lock);
	return -1;
}

/*
 * Functional test:
 * 1/2 writers, rest are readers
 */
static int
test_lpm_rcu_perf_multi_writer(uint8_t use_rcu)
{
	struct rte_lpm_config config;
	size_t sz;
	unsigned int i, j;
	uint16_t core_id;
	struct rte_lpm_rcu_config rcu_cfg = {0};
	int (*reader_f)(void *arg) = NULL;

	if (rte_lcore_count() < 3) {
		printf("Not enough cores for lpm_rcu_perf_autotest, expecting at least 3\n");
		return TEST_SKIPPED;
	}

	num_cores = 0;
	RTE_LCORE_FOREACH_WORKER(core_id) {
		enabled_core_ids[num_cores] = core_id;
		num_cores++;
	}

	for (j = 1; j < 3; j++) {
		if (use_rcu)
			printf("\nPerf test: %d writer(s), %d reader(s),"
			       " RCU integration enabled\n", j, num_cores - j);
		else
			printf("\nPerf test: %d writer(s), %d reader(s),"
			       " RCU integration disabled\n", j, num_cores - j);

		num_writers = j;

		/* Create LPM table */
		config.max_rules = NUM_LDEPTH_ROUTE_ENTRIES;
		config.number_tbl8s = NUM_LDEPTH_ROUTE_ENTRIES;
		config.flags = 0;
		lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
		TEST_LPM_ASSERT(lpm != NULL);

		/* Init RCU variable */
		if (use_rcu) {
			sz = rte_rcu_qsbr_get_memsize(num_cores);
			rv = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
							RTE_CACHE_LINE_SIZE);
			rte_rcu_qsbr_init(rv, num_cores);

			rcu_cfg.v = rv;
			/* Assign the RCU variable to LPM */
			if (rte_lpm_rcu_qsbr_add(lpm, &rcu_cfg) != 0) {
				printf("RCU variable assignment failed\n");
				goto error;
			}

			reader_f = test_lpm_rcu_qsbr_reader;
		} else
			reader_f = test_lpm_reader;

		writer_done = 0;
		__atomic_store_n(&gwrite_cycles, 0, __ATOMIC_RELAXED);

		__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

		/* Launch reader threads */
		for (i = j; i < num_cores; i++)
			rte_eal_remote_launch(reader_f, NULL,
						enabled_core_ids[i]);

		/* Launch writer threads */
		for (i = 0; i < j; i++)
			rte_eal_remote_launch(test_lpm_rcu_qsbr_writer,
						(void *)(uintptr_t)i,
						enabled_core_ids[i]);

		/* Wait for writer threads */
		for (i = 0; i < j; i++)
			if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
				goto error;

		printf("Total LPM Adds: %d\n", TOTAL_WRITES);
		printf("Total LPM Deletes: %d\n", TOTAL_WRITES);
		printf("Average LPM Add/Del: %"PRIu64" cycles\n",
			__atomic_load_n(&gwrite_cycles, __ATOMIC_RELAXED)
			/ TOTAL_WRITES);

		writer_done = 1;
		/* Wait until all readers have exited */
		for (i = j; i < num_cores; i++)
			rte_eal_wait_lcore(enabled_core_ids[i]);

		rte_lpm_free(lpm);
		rte_free(rv);
		lpm = NULL;
		rv = NULL;
	}

	return 0;

error:
	writer_done = 1;
	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	rte_lpm_free(lpm);
	rte_free(rv);

	return -1;
}

static int
test_lpm_perf(void)
{
	struct rte_lpm_config config;

	config.max_rules = 2000000;
	config.number_tbl8s = 2048;
	config.flags = 0;
	uint64_t begin, total_time, lpm_used_entries = 0;
	unsigned i, j;
	uint32_t next_hop_add = 0xAA, next_hop_return = 0;
	int status = 0;
	uint64_t cache_line_counter = 0;
	int64_t count = 0;

	rte_srand(rte_rdtsc());

	generate_large_route_rule_table();

	printf("No. routes = %u\n", (unsigned) NUM_ROUTE_ENTRIES);

	print_route_distribution(large_route_table, (uint32_t) NUM_ROUTE_ENTRIES);

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Measure add. */
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		if (rte_lpm_add(lpm, large_route_table[i].ip,
				large_route_table[i].depth, next_hop_add) == 0)
			status++;
	}
	/* End Timer. */
	total_time = rte_rdtsc() - begin;

	printf("Unique added entries = %d\n", status);
	/* Obtain add statistics. */
	for (i = 0; i < RTE_LPM_TBL24_NUM_ENTRIES; i++) {
		if (lpm->tbl24[i].valid)
			lpm_used_entries++;

		if (i % 32 == 0) {
			if ((uint64_t)count < lpm_used_entries) {
				cache_line_counter++;
				count = lpm_used_entries;
			}
		}
	}

	printf("Used table 24 entries = %u (%g%%)\n",
			(unsigned) lpm_used_entries,
			(lpm_used_entries * 100.0) / RTE_LPM_TBL24_NUM_ENTRIES);
	printf("64 byte Cache entries used = %u (%u bytes)\n",
			(unsigned) cache_line_counter, (unsigned) cache_line_counter * 64);

	printf("Average LPM Add: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	/* Measure single Lookup */
	total_time = 0;
	count = 0;

	for (i = 0; i < ITERATIONS; i++) {
		static uint32_t ip_batch[BATCH_SIZE];

		for (j = 0; j < BATCH_SIZE; j++)
			ip_batch[j] = rte_rand();

		/* Lookup per batch */
		begin = rte_rdtsc();

		for (j = 0; j < BATCH_SIZE; j++) {
			if (rte_lpm_lookup(lpm, ip_batch[j], &next_hop_return) != 0)
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
	for (i = 0; i < ITERATIONS; i++) {
		static uint32_t ip_batch[BATCH_SIZE];
		uint32_t next_hops[BULK_SIZE];

		/* Create array of random IP addresses */
		for (j = 0; j < BATCH_SIZE; j++)
			ip_batch[j] = rte_rand();

		/* Lookup per batch */
		begin = rte_rdtsc();
		for (j = 0; j < BATCH_SIZE; j += BULK_SIZE) {
			unsigned k;
			rte_lpm_lookup_bulk(lpm, &ip_batch[j], next_hops, BULK_SIZE);
			for (k = 0; k < BULK_SIZE; k++)
				if (unlikely(!(next_hops[k] & RTE_LPM_LOOKUP_SUCCESS)))
					count++;
		}

		total_time += rte_rdtsc() - begin;
	}
	printf("BULK LPM Lookup: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Measure LookupX4 */
	total_time = 0;
	count = 0;
	for (i = 0; i < ITERATIONS; i++) {
		static uint32_t ip_batch[BATCH_SIZE];
		uint32_t next_hops[4];

		/* Create array of random IP addresses */
		for (j = 0; j < BATCH_SIZE; j++)
			ip_batch[j] = rte_rand();

		/* Lookup per batch */
		begin = rte_rdtsc();
		for (j = 0; j < BATCH_SIZE; j += RTE_DIM(next_hops)) {
			unsigned k;
			xmm_t ipx4;

			ipx4 = vect_loadu_sil128((xmm_t *)(ip_batch + j));
			ipx4 = *(xmm_t *)(ip_batch + j);
			rte_lpm_lookupx4(lpm, ipx4, next_hops, UINT32_MAX);
			for (k = 0; k < RTE_DIM(next_hops); k++)
				if (unlikely(next_hops[k] == UINT32_MAX))
					count++;
		}

		total_time += rte_rdtsc() - begin;
	}
	printf("LPM LookupX4: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Measure Delete */
	status = 0;
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		/* rte_lpm_delete(lpm, ip, depth) */
		status += rte_lpm_delete(lpm, large_route_table[i].ip,
				large_route_table[i].depth);
	}

	total_time = rte_rdtsc() - begin;

	printf("Average LPM Delete: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	rte_lpm_delete_all(lpm);
	rte_lpm_free(lpm);

	if (test_lpm_rcu_perf_multi_writer(0) < 0)
		return -1;

	if (test_lpm_rcu_perf_multi_writer(1) < 0)
		return -1;

	return 0;
}

REGISTER_PERF_TEST(lpm_perf_autotest, test_lpm_perf);
