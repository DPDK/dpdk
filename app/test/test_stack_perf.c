/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */


#include <stdio.h>
#include <inttypes.h>

#include <rte_cycles.h>
#include <rte_launch.h>
#include <rte_pause.h>
#include <rte_stack.h>

#include "test.h"

#define STACK_NAME "STACK_PERF"
#define MAX_BURST 32
#define STACK_SIZE (RTE_MAX_LCORE * MAX_BURST)

/*
 * Push/pop bulk sizes, marked volatile so they aren't treated as compile-time
 * constants.
 */
static volatile unsigned int bulk_sizes[] = {8, MAX_BURST};

static uint32_t lcore_barrier;

struct lcore_pair {
	unsigned int c1;
	unsigned int c2;
};

static int
get_two_hyperthreads(struct lcore_pair *lcp)
{
	unsigned int socket[2];
	unsigned int core[2];
	unsigned int id[2];

	RTE_LCORE_FOREACH(id[0]) {
		RTE_LCORE_FOREACH(id[1]) {
			if (id[0] == id[1])
				continue;
			core[0] = rte_lcore_to_cpu_id(id[0]);
			core[1] = rte_lcore_to_cpu_id(id[1]);
			socket[0] = rte_lcore_to_socket_id(id[0]);
			socket[1] = rte_lcore_to_socket_id(id[1]);
			if ((core[0] == core[1]) && (socket[0] == socket[1])) {
				lcp->c1 = id[0];
				lcp->c2 = id[1];
				return 0;
			}
		}
	}

	return 1;
}

static int
get_two_cores(struct lcore_pair *lcp)
{
	unsigned int socket[2];
	unsigned int core[2];
	unsigned int id[2];

	RTE_LCORE_FOREACH(id[0]) {
		RTE_LCORE_FOREACH(id[1]) {
			if (id[0] == id[1])
				continue;
			core[0] = rte_lcore_to_cpu_id(id[0]);
			core[1] = rte_lcore_to_cpu_id(id[1]);
			socket[0] = rte_lcore_to_socket_id(id[0]);
			socket[1] = rte_lcore_to_socket_id(id[1]);
			if ((core[0] != core[1]) && (socket[0] == socket[1])) {
				lcp->c1 = id[0];
				lcp->c2 = id[1];
				return 0;
			}
		}
	}

	return 1;
}

static int
get_two_sockets(struct lcore_pair *lcp)
{
	unsigned int socket[2];
	unsigned int id[2];

	RTE_LCORE_FOREACH(id[0]) {
		RTE_LCORE_FOREACH(id[1]) {
			if (id[0] == id[1])
				continue;
			socket[0] = rte_lcore_to_socket_id(id[0]);
			socket[1] = rte_lcore_to_socket_id(id[1]);
			if (socket[0] != socket[1]) {
				lcp->c1 = id[0];
				lcp->c2 = id[1];
				return 0;
			}
		}
	}

	return 1;
}

/* Measure the cycle cost of popping an empty stack. */
static void
test_empty_pop(struct rte_stack *s)
{
	unsigned int iterations = 100000000;
	void *objs[MAX_BURST];
	unsigned int i;

	uint64_t start = rte_rdtsc();

	for (i = 0; i < iterations; i++)
		rte_stack_pop(s, objs, bulk_sizes[0]);

	uint64_t end = rte_rdtsc();

	printf("Stack empty pop: %.2F\n",
	       (double)(end - start) / iterations);
}

struct thread_args {
	struct rte_stack *s;
	unsigned int sz;
	double avg;
};

/* Measure the average per-pointer cycle cost of stack push and pop */
static int
bulk_push_pop(void *p)
{
	unsigned int iterations = 1000000;
	struct thread_args *args = p;
	void *objs[MAX_BURST] = {0};
	unsigned int size, i;
	struct rte_stack *s;

	s = args->s;
	size = args->sz;

	__atomic_fetch_sub(&lcore_barrier, 1, __ATOMIC_RELAXED);
	rte_wait_until_equal_32(&lcore_barrier, 0, __ATOMIC_RELAXED);

	uint64_t start = rte_rdtsc();

	for (i = 0; i < iterations; i++) {
		rte_stack_push(s, objs, size);
		rte_stack_pop(s, objs, size);
	}

	uint64_t end = rte_rdtsc();

	args->avg = ((double)(end - start))/(iterations * size);

	return 0;
}

/*
 * Run bulk_push_pop() simultaneously on pairs of cores, to measure stack
 * perf when between hyperthread siblings, cores on the same socket, and cores
 * on different sockets.
 */
static void
run_on_core_pair(struct lcore_pair *cores, struct rte_stack *s,
		 lcore_function_t fn)
{
	struct thread_args args[2];
	unsigned int i;

	for (i = 0; i < RTE_DIM(bulk_sizes); i++) {
		__atomic_store_n(&lcore_barrier, 2, __ATOMIC_RELAXED);

		args[0].sz = args[1].sz = bulk_sizes[i];
		args[0].s = args[1].s = s;

		if (cores->c1 == rte_get_main_lcore()) {
			rte_eal_remote_launch(fn, &args[1], cores->c2);
			fn(&args[0]);
			rte_eal_wait_lcore(cores->c2);
		} else {
			rte_eal_remote_launch(fn, &args[0], cores->c1);
			rte_eal_remote_launch(fn, &args[1], cores->c2);
			rte_eal_wait_lcore(cores->c1);
			rte_eal_wait_lcore(cores->c2);
		}

		printf("Average cycles per object push/pop (bulk size: %u): %.2F\n",
		       bulk_sizes[i], (args[0].avg + args[1].avg) / 2);
	}
}

/* Run bulk_push_pop() simultaneously on 1+ cores. */
static void
run_on_n_cores(struct rte_stack *s, lcore_function_t fn, int n)
{
	struct thread_args args[RTE_MAX_LCORE];
	unsigned int i;

	for (i = 0; i < RTE_DIM(bulk_sizes); i++) {
		unsigned int lcore_id;
		int cnt = 0;
		double avg;

		__atomic_store_n(&lcore_barrier, n, __ATOMIC_RELAXED);

		RTE_LCORE_FOREACH_WORKER(lcore_id) {
			if (++cnt >= n)
				break;

			args[lcore_id].s = s;
			args[lcore_id].sz = bulk_sizes[i];

			if (rte_eal_remote_launch(fn, &args[lcore_id],
						  lcore_id))
				rte_panic("Failed to launch lcore %d\n",
					  lcore_id);
		}

		lcore_id = rte_lcore_id();

		args[lcore_id].s = s;
		args[lcore_id].sz = bulk_sizes[i];

		fn(&args[lcore_id]);

		rte_eal_mp_wait_lcore();

		avg = args[rte_lcore_id()].avg;

		cnt = 0;
		RTE_LCORE_FOREACH_WORKER(lcore_id) {
			if (++cnt >= n)
				break;
			avg += args[lcore_id].avg;
		}

		printf("Average cycles per object push/pop (bulk size: %u): %.2F\n",
		       bulk_sizes[i], avg / n);
	}
}

/*
 * Measure the cycle cost of pushing and popping a single pointer on a single
 * lcore.
 */
static void
test_single_push_pop(struct rte_stack *s)
{
	unsigned int iterations = 16000000;
	void *obj = NULL;
	unsigned int i;

	uint64_t start = rte_rdtsc();

	for (i = 0; i < iterations; i++) {
		rte_stack_push(s, &obj, 1);
		rte_stack_pop(s, &obj, 1);
	}

	uint64_t end = rte_rdtsc();

	printf("Average cycles per single object push/pop: %.2F\n",
	       ((double)(end - start)) / iterations);
}

/* Measure the cycle cost of bulk pushing and popping on a single lcore. */
static void
test_bulk_push_pop(struct rte_stack *s)
{
	unsigned int iterations = 8000000;
	void *objs[MAX_BURST];
	unsigned int sz, i;

	for (sz = 0; sz < RTE_DIM(bulk_sizes); sz++) {
		uint64_t start = rte_rdtsc();

		for (i = 0; i < iterations; i++) {
			rte_stack_push(s, objs, bulk_sizes[sz]);
			rte_stack_pop(s, objs, bulk_sizes[sz]);
		}

		uint64_t end = rte_rdtsc();

		double avg = ((double)(end - start) /
			      (iterations * bulk_sizes[sz]));

		printf("Average cycles per object push/pop (bulk size: %u): %.2F\n",
		       bulk_sizes[sz], avg);
	}
}

static int
__test_stack_perf(uint32_t flags)
{
	struct lcore_pair cores;
	struct rte_stack *s;

	__atomic_store_n(&lcore_barrier, 0, __ATOMIC_RELAXED);

	s = rte_stack_create(STACK_NAME, STACK_SIZE, rte_socket_id(), flags);
	if (s == NULL) {
		printf("[%s():%u] failed to create a stack\n",
		       __func__, __LINE__);
		return -1;
	}

	printf("### Testing single element push/pop ###\n");
	test_single_push_pop(s);

	printf("\n### Testing empty pop ###\n");
	test_empty_pop(s);

	printf("\n### Testing using a single lcore ###\n");
	test_bulk_push_pop(s);

	if (get_two_hyperthreads(&cores) == 0) {
		printf("\n### Testing using two hyperthreads ###\n");
		run_on_core_pair(&cores, s, bulk_push_pop);
	}
	if (get_two_cores(&cores) == 0) {
		printf("\n### Testing using two physical cores ###\n");
		run_on_core_pair(&cores, s, bulk_push_pop);
	}
	if (get_two_sockets(&cores) == 0) {
		printf("\n### Testing using two NUMA nodes ###\n");
		run_on_core_pair(&cores, s, bulk_push_pop);
	}

	printf("\n### Testing on all %u lcores ###\n", rte_lcore_count());
	run_on_n_cores(s, bulk_push_pop, rte_lcore_count());

	rte_stack_free(s);
	return 0;
}

static int
test_stack_perf(void)
{
	return __test_stack_perf(0);
}

static int
test_lf_stack_perf(void)
{
#if defined(RTE_STACK_LF_SUPPORTED)
	return __test_stack_perf(RTE_STACK_F_LF);
#else
	return TEST_SKIPPED;
#endif
}

REGISTER_PERF_TEST(stack_perf_autotest, test_stack_perf);
REGISTER_PERF_TEST(stack_lf_perf_autotest, test_lf_stack_perf);
