/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2019 Arm Limited
 */


#include <stdio.h>
#include <inttypes.h>
#include <rte_ring.h>
#include <rte_cycles.h>
#include <rte_launch.h>
#include <rte_pause.h>
#include <string.h>

#include "test.h"
#include "test_ring.h"

/*
 * Ring performance test cases, measures performance of various operations
 * using rdtsc for legacy and 16B size ring elements.
 */

#define RING_NAME "RING_PERF"
#define RING_SIZE 4096
#define MAX_BURST 32

/*
 * the sizes to enqueue and dequeue in testing
 * (marked volatile so they won't be seen as compile-time constants)
 */
static const volatile unsigned bulk_sizes[] = { 8, 32 };

struct lcore_pair {
	unsigned c1, c2;
};

static volatile unsigned lcore_count = 0;

static void
test_ring_print_test_string(unsigned int api_type, int esize,
	unsigned int bsz, double value)
{
	if (esize == -1)
		printf("legacy APIs");
	else
		printf("elem APIs: element size %dB", esize);

	if (api_type == TEST_RING_IGNORE_API_TYPE)
		return;

	if ((api_type & TEST_RING_THREAD_DEF) == TEST_RING_THREAD_DEF)
		printf(": default enqueue/dequeue: ");
	else if ((api_type & TEST_RING_THREAD_SPSC) == TEST_RING_THREAD_SPSC)
		printf(": SP/SC: ");
	else if ((api_type & TEST_RING_THREAD_MPMC) == TEST_RING_THREAD_MPMC)
		printf(": MP/MC: ");

	if ((api_type & TEST_RING_ELEM_SINGLE) == TEST_RING_ELEM_SINGLE)
		printf("single: ");
	else if ((api_type & TEST_RING_ELEM_BULK) == TEST_RING_ELEM_BULK)
		printf("bulk (size: %u): ", bsz);
	else if ((api_type & TEST_RING_ELEM_BURST) == TEST_RING_ELEM_BURST)
		printf("burst (size: %u): ", bsz);

	printf("%.2F\n", value);
}

/**** Functions to analyse our core mask to get cores for different tests ***/

static int
get_two_hyperthreads(struct lcore_pair *lcp)
{
	unsigned id1, id2;
	unsigned c1, c2, s1, s2;
	RTE_LCORE_FOREACH(id1) {
		/* inner loop just re-reads all id's. We could skip the first few
		 * elements, but since number of cores is small there is little point
		 */
		RTE_LCORE_FOREACH(id2) {
			if (id1 == id2)
				continue;

			c1 = rte_lcore_to_cpu_id(id1);
			c2 = rte_lcore_to_cpu_id(id2);
			s1 = rte_lcore_to_socket_id(id1);
			s2 = rte_lcore_to_socket_id(id2);
			if ((c1 == c2) && (s1 == s2)){
				lcp->c1 = id1;
				lcp->c2 = id2;
				return 0;
			}
		}
	}
	return 1;
}

static int
get_two_cores(struct lcore_pair *lcp)
{
	unsigned id1, id2;
	unsigned c1, c2, s1, s2;
	RTE_LCORE_FOREACH(id1) {
		RTE_LCORE_FOREACH(id2) {
			if (id1 == id2)
				continue;

			c1 = rte_lcore_to_cpu_id(id1);
			c2 = rte_lcore_to_cpu_id(id2);
			s1 = rte_lcore_to_socket_id(id1);
			s2 = rte_lcore_to_socket_id(id2);
			if ((c1 != c2) && (s1 == s2)){
				lcp->c1 = id1;
				lcp->c2 = id2;
				return 0;
			}
		}
	}
	return 1;
}

static int
get_two_sockets(struct lcore_pair *lcp)
{
	unsigned id1, id2;
	unsigned s1, s2;
	RTE_LCORE_FOREACH(id1) {
		RTE_LCORE_FOREACH(id2) {
			if (id1 == id2)
				continue;
			s1 = rte_lcore_to_socket_id(id1);
			s2 = rte_lcore_to_socket_id(id2);
			if (s1 != s2){
				lcp->c1 = id1;
				lcp->c2 = id2;
				return 0;
			}
		}
	}
	return 1;
}

/* Get cycle counts for dequeuing from an empty ring. Should be 2 or 3 cycles */
static void
test_empty_dequeue(struct rte_ring *r, const int esize,
			const unsigned int api_type)
{
	const unsigned int iter_shift = 26;
	const unsigned int iterations = 1 << iter_shift;
	unsigned int i = 0;
	void *burst[MAX_BURST];

	const uint64_t start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		test_ring_dequeue(r, burst, esize, bulk_sizes[0], api_type);
	const uint64_t end = rte_rdtsc();

	test_ring_print_test_string(api_type, esize, bulk_sizes[0],
					((double)(end - start)) / iterations);
}

/*
 * for the separate enqueue and dequeue threads they take in one param
 * and return two. Input = burst size, output = cycle average for sp/sc & mp/mc
 */
struct thread_params {
	struct rte_ring *r;
	unsigned size;        /* input value, the burst size */
	double spsc, mpmc;    /* output value, the single or multi timings */
};

/*
 * Helper function to call bulk SP/MP enqueue functions.
 * flag == 0 -> enqueue
 * flag == 1 -> dequeue
 */
static __rte_always_inline int
enqueue_dequeue_bulk_helper(const unsigned int flag, const int esize,
	struct thread_params *p)
{
	int ret;
	const unsigned int iter_shift = 15;
	const unsigned int iterations = 1 << iter_shift;
	struct rte_ring *r = p->r;
	unsigned int bsize = p->size;
	unsigned int i;
	void *burst = NULL;

#ifdef RTE_USE_C11_MEM_MODEL
	if (__atomic_fetch_add(&lcore_count, 1, __ATOMIC_RELAXED) + 1 != 2)
#else
	if (__sync_add_and_fetch(&lcore_count, 1) != 2)
#endif
		while(lcore_count != 2)
			rte_pause();

	burst = test_ring_calloc(MAX_BURST, esize);
	if (burst == NULL)
		return -1;

	const uint64_t sp_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		do {
			if (flag == 0)
				ret = test_ring_enqueue(r, burst, esize, bsize,
						TEST_RING_THREAD_SPSC |
						TEST_RING_ELEM_BULK);
			else if (flag == 1)
				ret = test_ring_dequeue(r, burst, esize, bsize,
						TEST_RING_THREAD_SPSC |
						TEST_RING_ELEM_BULK);
			if (ret == 0)
				rte_pause();
		} while (!ret);
	const uint64_t sp_end = rte_rdtsc();

	const uint64_t mp_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		do {
			if (flag == 0)
				ret = test_ring_enqueue(r, burst, esize, bsize,
						TEST_RING_THREAD_MPMC |
						TEST_RING_ELEM_BULK);
			else if (flag == 1)
				ret = test_ring_dequeue(r, burst, esize, bsize,
						TEST_RING_THREAD_MPMC |
						TEST_RING_ELEM_BULK);
			if (ret == 0)
				rte_pause();
		} while (!ret);
	const uint64_t mp_end = rte_rdtsc();

	p->spsc = ((double)(sp_end - sp_start))/(iterations * bsize);
	p->mpmc = ((double)(mp_end - mp_start))/(iterations * bsize);
	return 0;
}

/*
 * Function that uses rdtsc to measure timing for ring enqueue. Needs pair
 * thread running dequeue_bulk function
 */
static int
enqueue_bulk(void *p)
{
	struct thread_params *params = p;

	return enqueue_dequeue_bulk_helper(0, -1, params);
}

static int
enqueue_bulk_16B(void *p)
{
	struct thread_params *params = p;

	return enqueue_dequeue_bulk_helper(0, 16, params);
}

/*
 * Function that uses rdtsc to measure timing for ring dequeue. Needs pair
 * thread running enqueue_bulk function
 */
static int
dequeue_bulk(void *p)
{
	struct thread_params *params = p;

	return enqueue_dequeue_bulk_helper(1, -1, params);
}

static int
dequeue_bulk_16B(void *p)
{
	struct thread_params *params = p;

	return enqueue_dequeue_bulk_helper(1, 16, params);
}

/*
 * Function that calls the enqueue and dequeue bulk functions on pairs of cores.
 * used to measure ring perf between hyperthreads, cores and sockets.
 */
static int
run_on_core_pair(struct lcore_pair *cores, struct rte_ring *r, const int esize)
{
	lcore_function_t *f1, *f2;
	struct thread_params param1 = {0}, param2 = {0};
	unsigned i;

	if (esize == -1) {
		f1 = enqueue_bulk;
		f2 = dequeue_bulk;
	} else {
		f1 = enqueue_bulk_16B;
		f2 = dequeue_bulk_16B;
	}

	for (i = 0; i < RTE_DIM(bulk_sizes); i++) {
		lcore_count = 0;
		param1.size = param2.size = bulk_sizes[i];
		param1.r = param2.r = r;
		if (cores->c1 == rte_get_main_lcore()) {
			rte_eal_remote_launch(f2, &param2, cores->c2);
			f1(&param1);
			rte_eal_wait_lcore(cores->c2);
		} else {
			rte_eal_remote_launch(f1, &param1, cores->c1);
			rte_eal_remote_launch(f2, &param2, cores->c2);
			if (rte_eal_wait_lcore(cores->c1) < 0)
				return -1;
			if (rte_eal_wait_lcore(cores->c2) < 0)
				return -1;
		}
		test_ring_print_test_string(
			TEST_RING_THREAD_SPSC | TEST_RING_ELEM_BULK,
			esize, bulk_sizes[i], param1.spsc + param2.spsc);
		test_ring_print_test_string(
			TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BULK,
			esize, bulk_sizes[i], param1.mpmc + param2.mpmc);
	}

	return 0;
}

static uint32_t synchro;
static uint64_t queue_count[RTE_MAX_LCORE];

#define TIME_MS 100

static int
load_loop_fn_helper(struct thread_params *p, const int esize)
{
	uint64_t time_diff = 0;
	uint64_t begin = 0;
	uint64_t hz = rte_get_timer_hz();
	uint64_t lcount = 0;
	const unsigned int lcore = rte_lcore_id();
	struct thread_params *params = p;
	void *burst = NULL;

	burst = test_ring_calloc(MAX_BURST, esize);
	if (burst == NULL)
		return -1;

	/* wait synchro for workers */
	if (lcore != rte_get_main_lcore())
		rte_wait_until_equal_32(&synchro, 1, __ATOMIC_RELAXED);

	begin = rte_get_timer_cycles();
	while (time_diff < hz * TIME_MS / 1000) {
		test_ring_enqueue(params->r, burst, esize, params->size,
				TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BULK);
		test_ring_dequeue(params->r, burst, esize, params->size,
				TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BULK);
		lcount++;
		time_diff = rte_get_timer_cycles() - begin;
	}
	queue_count[lcore] = lcount;

	rte_free(burst);

	return 0;
}

static int
load_loop_fn(void *p)
{
	struct thread_params *params = p;

	return load_loop_fn_helper(params, -1);
}

static int
load_loop_fn_16B(void *p)
{
	struct thread_params *params = p;

	return load_loop_fn_helper(params, 16);
}

static int
run_on_all_cores(struct rte_ring *r, const int esize)
{
	uint64_t total;
	struct thread_params param;
	lcore_function_t *lcore_f;
	unsigned int i, c;

	if (esize == -1)
		lcore_f = load_loop_fn;
	else
		lcore_f = load_loop_fn_16B;

	memset(&param, 0, sizeof(struct thread_params));
	for (i = 0; i < RTE_DIM(bulk_sizes); i++) {
		total = 0;
		printf("\nBulk enq/dequeue count on size %u\n", bulk_sizes[i]);
		param.size = bulk_sizes[i];
		param.r = r;

		/* clear synchro and start workers */
		__atomic_store_n(&synchro, 0, __ATOMIC_RELAXED);
		if (rte_eal_mp_remote_launch(lcore_f, &param, SKIP_MAIN) < 0)
			return -1;

		/* start synchro and launch test on main */
		__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
		lcore_f(&param);

		rte_eal_mp_wait_lcore();

		RTE_LCORE_FOREACH(c) {
			printf("Core [%u] count = %"PRIu64"\n",
					c, queue_count[c]);
			total += queue_count[c];
		}

		printf("Total count (size: %u): %"PRIu64"\n",
				bulk_sizes[i], total);
	}

	return 0;
}

/*
 * Test function that determines how long an enqueue + dequeue of a single item
 * takes on a single lcore. Result is for comparison with the bulk enq+deq.
 */
static int
test_single_enqueue_dequeue(struct rte_ring *r, const int esize,
	const unsigned int api_type)
{
	const unsigned int iter_shift = 24;
	const unsigned int iterations = 1 << iter_shift;
	unsigned int i = 0;
	void *burst = NULL;

	/* alloc dummy object pointers */
	burst = test_ring_calloc(1, esize);
	if (burst == NULL)
		return -1;

	const uint64_t start = rte_rdtsc();
	for (i = 0; i < iterations; i++) {
		test_ring_enqueue(r, burst, esize, 1, api_type);
		test_ring_dequeue(r, burst, esize, 1, api_type);
	}
	const uint64_t end = rte_rdtsc();

	test_ring_print_test_string(api_type, esize, 1,
					((double)(end - start)) / iterations);

	rte_free(burst);

	return 0;
}

/*
 * Test that does both enqueue and dequeue on a core using the burst/bulk API
 * calls Results should be the same as for the bulk function called on a
 * single lcore.
 */
static int
test_burst_bulk_enqueue_dequeue(struct rte_ring *r, const int esize,
	const unsigned int api_type)
{
	const unsigned int iter_shift = 23;
	const unsigned int iterations = 1 << iter_shift;
	unsigned int sz, i = 0;
	void **burst = NULL;

	burst = test_ring_calloc(MAX_BURST, esize);
	if (burst == NULL)
		return -1;

	for (sz = 0; sz < RTE_DIM(bulk_sizes); sz++) {
		const uint64_t start = rte_rdtsc();
		for (i = 0; i < iterations; i++) {
			test_ring_enqueue(r, burst, esize, bulk_sizes[sz],
						api_type);
			test_ring_dequeue(r, burst, esize, bulk_sizes[sz],
						api_type);
		}
		const uint64_t end = rte_rdtsc();

		test_ring_print_test_string(api_type, esize, bulk_sizes[sz],
					((double)(end - start)) / iterations);
	}

	rte_free(burst);

	return 0;
}

/* Run all tests for a given element size */
static __rte_always_inline int
test_ring_perf_esize(const int esize)
{
	struct lcore_pair cores;
	struct rte_ring *r = NULL;

	/*
	 * Performance test for legacy/_elem APIs
	 * SP-SC/MP-MC, single
	 */
	r = test_ring_create(RING_NAME, esize, RING_SIZE, rte_socket_id(), 0);
	if (r == NULL)
		goto test_fail;

	printf("\n### Testing single element enq/deq ###\n");
	if (test_single_enqueue_dequeue(r, esize,
			TEST_RING_THREAD_SPSC | TEST_RING_ELEM_SINGLE) < 0)
		goto test_fail;
	if (test_single_enqueue_dequeue(r, esize,
			TEST_RING_THREAD_MPMC | TEST_RING_ELEM_SINGLE) < 0)
		goto test_fail;

	printf("\n### Testing burst enq/deq ###\n");
	if (test_burst_bulk_enqueue_dequeue(r, esize,
			TEST_RING_THREAD_SPSC | TEST_RING_ELEM_BURST) < 0)
		goto test_fail;
	if (test_burst_bulk_enqueue_dequeue(r, esize,
			TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BURST) < 0)
		goto test_fail;

	printf("\n### Testing bulk enq/deq ###\n");
	if (test_burst_bulk_enqueue_dequeue(r, esize,
			TEST_RING_THREAD_SPSC | TEST_RING_ELEM_BULK) < 0)
		goto test_fail;
	if (test_burst_bulk_enqueue_dequeue(r, esize,
			TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BULK) < 0)
		goto test_fail;

	printf("\n### Testing empty bulk deq ###\n");
	test_empty_dequeue(r, esize,
			TEST_RING_THREAD_SPSC | TEST_RING_ELEM_BULK);
	test_empty_dequeue(r, esize,
			TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BULK);

	if (get_two_hyperthreads(&cores) == 0) {
		printf("\n### Testing using two hyperthreads ###\n");
		if (run_on_core_pair(&cores, r, esize) < 0)
			goto test_fail;
	}

	if (get_two_cores(&cores) == 0) {
		printf("\n### Testing using two physical cores ###\n");
		if (run_on_core_pair(&cores, r, esize) < 0)
			goto test_fail;
	}
	if (get_two_sockets(&cores) == 0) {
		printf("\n### Testing using two NUMA nodes ###\n");
		if (run_on_core_pair(&cores, r, esize) < 0)
			goto test_fail;
	}

	printf("\n### Testing using all worker nodes ###\n");
	if (run_on_all_cores(r, esize) < 0)
		goto test_fail;

	rte_ring_free(r);

	return 0;

test_fail:
	rte_ring_free(r);

	return -1;
}

static int
test_ring_perf(void)
{
	/* Run all the tests for different element sizes */
	if (test_ring_perf_esize(-1) == -1)
		return -1;

	if (test_ring_perf_esize(16) == -1)
		return -1;

	return 0;
}

REGISTER_PERF_TEST(ring_perf_autotest, test_ring_perf);
