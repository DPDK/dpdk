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
#define MAX_BURST 256

/*
 * the sizes to enqueue and dequeue in testing
 * (marked volatile so they won't be seen as compile-time constants)
 */
static const volatile unsigned int bulk_sizes[] = { 8, 32, 64, 128, 256 };

struct lcore_pair {
	unsigned c1, c2;
};

static RTE_ATOMIC(unsigned int) lcore_count;

static void
test_ring_print_test_string(unsigned int api_type, int esize,
	unsigned int bsz, double value)
{
	if (esize == -1)
		printf("legacy APIs");
	else
		printf("elem APIs (size:%2dB)", esize);

	if (api_type == TEST_RING_IGNORE_API_TYPE)
		return;

	if ((api_type & TEST_RING_THREAD_DEF) == TEST_RING_THREAD_DEF)
		printf(" - default enqueue/dequeue");
	else if ((api_type & TEST_RING_THREAD_SPSC) == TEST_RING_THREAD_SPSC)
		printf(" - SP/SC");
	else if ((api_type & TEST_RING_THREAD_MPMC) == TEST_RING_THREAD_MPMC)
		printf(" - MP/MC");

	if ((api_type & TEST_RING_ELEM_SINGLE) == TEST_RING_ELEM_SINGLE)
		printf(" - single - ");
	else if ((api_type & TEST_RING_ELEM_BULK) == TEST_RING_ELEM_BULK)
		printf(" - bulk (n:%-3u) - ", bsz);
	else if ((api_type & TEST_RING_ELEM_BURST) == TEST_RING_ELEM_BURST)
		printf(" - burst (n:%-3u) - ", bsz);
	else if ((api_type & (TEST_RING_ELEM_BURST_ZC |
			TEST_RING_ELEM_BURST_ZC_COMPRESS_PTR_16 |
			TEST_RING_ELEM_BURST_ZC_COMPRESS_PTR_32)) != 0)
		printf(" - burst zero copy (n:%-3u) - ", bsz);

	printf("cycles per elem: %.3F\n", value);
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
	const unsigned int iter_shift = 29;
	const unsigned int iterations = 1 << iter_shift;
	unsigned int i = 0;
	void *burst[MAX_BURST];

	const unsigned int bulk_iterations = iterations / bulk_sizes[0];
	const uint64_t start = rte_rdtsc();
	for (i = 0; i < bulk_iterations; i++)
		test_ring_dequeue(r, burst, esize, bulk_sizes[0], api_type);
	const uint64_t end = rte_rdtsc();

	test_ring_print_test_string(api_type, esize, bulk_sizes[0],
					((double)end - start) / iterations);
}

/* describes the ring used by the enqueue and dequeue thread */
struct ring_params {
	struct rte_ring *r;
	unsigned int elem_size;
	unsigned int bulk_sizes_i; /* index into bulk_size array */
	unsigned int ring_flags; /* flags for test_ring_enqueue/dequeue */
};

/* Used to specify enqueue and dequeue ring operations and their results */
struct thread_params {
	struct ring_params *ring_params;
	double *results; /* result array size must be equal to bulk_sizes */
};

/*
 * Helper function to call bulk SP/MP enqueue functions.
 * flag == 0 -> enqueue
 * flag == 1 -> dequeue
 */
static __rte_always_inline int
enqueue_dequeue_bulk_helper(const unsigned int flag, struct thread_params *p)
{
	int ret;
	const unsigned int iter_shift = 22;
	const unsigned int iterations = 1 << iter_shift;
	unsigned int i;
	void *burst = NULL;
	unsigned int n_remaining;
	const unsigned int bulk_n = bulk_sizes[p->ring_params->bulk_sizes_i];

	if (rte_atomic_fetch_add_explicit(&lcore_count, 1, rte_memory_order_relaxed) + 1 != 2) {
		while (rte_atomic_load_explicit(&lcore_count, rte_memory_order_relaxed) != 2)
			rte_pause();
	}

	burst = test_ring_calloc(MAX_BURST, p->ring_params->elem_size);
	if (burst == NULL)
		return -1;

	const uint64_t sp_start = rte_rdtsc();
	const unsigned int bulk_iterations = iterations / bulk_n;
	for (i = 0; i < bulk_iterations; i++) {
		n_remaining = bulk_n;
		do {
			if (flag == 0)
				ret = test_ring_enqueue(p->ring_params->r,
						burst,
						p->ring_params->elem_size,
						n_remaining,
						p->ring_params->ring_flags);
			else if (flag == 1)
				ret = test_ring_dequeue(p->ring_params->r,
						burst,
						p->ring_params->elem_size,
						n_remaining,
						p->ring_params->ring_flags);
			if (ret == 0)
				rte_pause();
			else
				n_remaining -= ret;
		} while (n_remaining > 0);
	}
	const uint64_t sp_end = rte_rdtsc();

	p->results[p->ring_params->bulk_sizes_i] =
			((double)sp_end - sp_start) / iterations;

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

	return enqueue_dequeue_bulk_helper(0, params);
}

/*
 * Function that uses rdtsc to measure timing for ring dequeue. Needs pair
 * thread running enqueue_bulk function
 */
static int
dequeue_bulk(void *p)
{
	struct thread_params *params = p;

	return enqueue_dequeue_bulk_helper(1, params);
}

/*
 * Function that calls the enqueue and dequeue bulk functions on pairs of cores.
 * used to measure ring perf between hyperthreads, cores and sockets.
 */
static int
run_on_core_pair(struct lcore_pair *cores,
		struct thread_params *param1, struct thread_params *param2)
{
	unsigned i;
	struct ring_params *ring_params = param1->ring_params;

	for (i = 0; i < RTE_DIM(bulk_sizes); i++) {
		rte_atomic_store_explicit(&lcore_count, 0, rte_memory_order_relaxed);
		ring_params->bulk_sizes_i = i;
		if (cores->c1 == rte_get_main_lcore()) {
			rte_eal_remote_launch(dequeue_bulk, param2, cores->c2);
			enqueue_bulk(param1);
			rte_eal_wait_lcore(cores->c2);
		} else {
			rte_eal_remote_launch(enqueue_bulk, param1, cores->c1);
			rte_eal_remote_launch(dequeue_bulk, param2, cores->c2);
			if (rte_eal_wait_lcore(cores->c1) < 0)
				return -1;
			if (rte_eal_wait_lcore(cores->c2) < 0)
				return -1;
		}
		test_ring_print_test_string(
				ring_params->ring_flags,
				ring_params->elem_size,
				bulk_sizes[i],
				param1->results[i] + param2->results[i]);
	}

	return 0;
}

static RTE_ATOMIC(uint32_t) synchro;
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
	struct ring_params *ring_params = p->ring_params;
	void *burst = NULL;

	burst = test_ring_calloc(MAX_BURST, esize);
	if (burst == NULL)
		return -1;

	/* wait synchro for workers */
	if (lcore != rte_get_main_lcore())
		rte_wait_until_equal_32((uint32_t *)(uintptr_t)&synchro, 1,
				rte_memory_order_relaxed);

	begin = rte_get_timer_cycles();
	while (time_diff < hz * TIME_MS / 1000) {
		test_ring_enqueue(ring_params->r, burst, esize,
				ring_params->elem_size,
				TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BULK);
		test_ring_dequeue(ring_params->r, burst, esize,
				ring_params->elem_size,
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
	struct ring_params ring_params = {0};
	struct thread_params params = { .ring_params = &ring_params };
	lcore_function_t *lcore_f;
	unsigned int i, c;

	if (esize == -1)
		lcore_f = load_loop_fn;
	else
		lcore_f = load_loop_fn_16B;

	for (i = 0; i < RTE_DIM(bulk_sizes); i++) {
		total = 0;
		printf("\nBulk enq/dequeue count on size %u\n", bulk_sizes[i]);
		params.ring_params->bulk_sizes_i = i;
		params.ring_params->r = r;

		/* clear synchro and start workers */
		rte_atomic_store_explicit(&synchro, 0, rte_memory_order_relaxed);
		if (rte_eal_mp_remote_launch(lcore_f, &params, SKIP_MAIN) < 0)
			return -1;

		/* start synchro and launch test on main */
		rte_atomic_store_explicit(&synchro, 1, rte_memory_order_relaxed);
		lcore_f(&params);

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
	const unsigned int iter_shift = 26;
	const unsigned int iterations = 1 << iter_shift;
	unsigned int sz, i;
	void **burst = NULL;

	burst = test_ring_calloc(MAX_BURST, esize);
	if (burst == NULL)
		return -1;

	for (sz = 0; sz < RTE_DIM(bulk_sizes); sz++) {
		const unsigned int n = iterations / bulk_sizes[sz];
		const uint64_t start = rte_rdtsc();
		for (i = 0; i < n; i++) {
			test_ring_enqueue(r, burst, esize, bulk_sizes[sz],
					api_type);
			test_ring_dequeue(r, burst, esize, bulk_sizes[sz],
					api_type);
		}
		const uint64_t end = rte_rdtsc();

		test_ring_print_test_string(api_type, esize, bulk_sizes[sz],
					((double)end - start) / iterations);
	}

	rte_free(burst);

	return 0;
}

static __rte_always_inline int
test_ring_perf_esize_run_on_two_cores(
		struct thread_params *param1, struct thread_params *param2)
{
	struct lcore_pair cores;

	if (get_two_hyperthreads(&cores) == 0) {
		printf("\n### Testing using two hyperthreads ###\n");
		if (run_on_core_pair(&cores, param1, param2) < 0)
			return -1;
	}
	if (get_two_cores(&cores) == 0) {
		printf("\n### Testing using two physical cores ###\n");
		if (run_on_core_pair(&cores, param1, param2) < 0)
			return -1;
	}
	if (get_two_sockets(&cores) == 0) {
		printf("\n### Testing using two NUMA nodes ###\n");
		if (run_on_core_pair(&cores, param1, param2) < 0)
			return -1;
	}
	return 0;
}

/* Run all tests for a given element size */
static __rte_always_inline int
test_ring_perf_esize(const int esize)
{
	struct rte_ring *r = NULL;
	double results_enq[RTE_DIM(bulk_sizes)];
	double results_deq[RTE_DIM(bulk_sizes)];
	struct ring_params ring_params = {
			.elem_size = esize, .ring_flags = TEST_RING_ELEM_BULK };
	struct thread_params param1 = {
			.ring_params = &ring_params, .results = results_enq };
	struct thread_params param2 = {
			.ring_params = &ring_params, .results = results_deq };

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

	ring_params.r = r;

	ring_params.ring_flags = TEST_RING_THREAD_SPSC | TEST_RING_ELEM_BULK;
	test_ring_perf_esize_run_on_two_cores(&param1, &param2);

	ring_params.ring_flags = TEST_RING_THREAD_MPMC | TEST_RING_ELEM_BULK;
	test_ring_perf_esize_run_on_two_cores(&param1, &param2);

	printf("\n### Testing using all worker nodes ###\n");
	if (run_on_all_cores(r, esize) < 0)
		goto test_fail;

	rte_ring_free(r);

	return 0;

test_fail:
	rte_ring_free(r);

	return -1;
}


static __rte_always_inline int
test_ring_perf_compression(void)
{
	double results1[RTE_DIM(bulk_sizes)];
	double results2[RTE_DIM(bulk_sizes)];
	double results1_comp[2][RTE_DIM(bulk_sizes)];
	double results2_comp[2][RTE_DIM(bulk_sizes)];

	struct lcore_pair cores;
	int ret = -1;
	unsigned int i, j;
	struct ring_params ring_params = { .elem_size = sizeof(void *) };
	struct thread_params param1 = {
			.ring_params = &ring_params, .results = results1 };
	struct thread_params param2 = {
			.ring_params = &ring_params, .results = results2 };

	printf("\n### Testing compression gain ###");

	ring_params.r = rte_ring_create_elem(
			RING_NAME, sizeof(void *),
			RING_SIZE, rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

	if (ring_params.r == NULL)
		return -1;

	if (get_two_cores(&cores) == 0) {
		printf("\n### Testing zero copy ###\n");
		ring_params.ring_flags = TEST_RING_ELEM_BURST_ZC;
		ret = run_on_core_pair(&cores, &param1, &param2);
	}

	rte_ring_free(ring_params.r);

	if (ret != 0)
		return ret;

	/* rings allow only multiples of 4 as sizes,
	 * we allocate size 4 despite only using 2 bytes
	 * and use half of RING_SIZE as the number of elements
	 */
	ring_params.r = rte_ring_create_elem(
			RING_NAME, sizeof(uint32_t),
			RING_SIZE / 2, rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

	if (ring_params.r == NULL)
		return -1;

	param1.results = results1_comp[0];
	param2.results = results2_comp[0];

	if (get_two_cores(&cores) == 0) {
		printf("\n### Testing zero copy with compression (16b) ###\n");
		ring_params.ring_flags =
				TEST_RING_ELEM_BURST_ZC_COMPRESS_PTR_16;
		ret = run_on_core_pair(&cores, &param1, &param2);
	}

	rte_ring_free(ring_params.r);

	if (ret != 0)
		return ret;

	ring_params.r = rte_ring_create_elem(
			RING_NAME, sizeof(uint32_t),
			RING_SIZE, rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

	if (ring_params.r == NULL)
		return -1;

	param1.results = results1_comp[1];
	param2.results = results2_comp[1];

	if (get_two_cores(&cores) == 0) {
		printf("\n### Testing zero copy with compression (32b) ###\n");
		ring_params.ring_flags =
				TEST_RING_ELEM_BURST_ZC_COMPRESS_PTR_32;
		ret = run_on_core_pair(&cores, &param1, &param2);
	}

	rte_ring_free(ring_params.r);

	for (j = 0; j < 2; j++) {
		printf("\n### Potential gain from compression (%d-bit offsets) "
		"###\n", (j + 1) * 16);
		for (i = 0; i < RTE_DIM(bulk_sizes); i++) {
			const double result = results1[i] + results2[i];
			const double result_comp = results1_comp[j][i] +
				results2_comp[j][i];
			const double gain = 100 - (result_comp / result) * 100;

			printf("Gain of %5.1F%% for burst of %-3u elems\n",
					gain, bulk_sizes[i]);
		}
	}

	return ret;
}

static int
test_ring_perf(void)
{
	/* Run all the tests for different element sizes */
	if (test_ring_perf_esize(-1) == -1)
		return -1;

	if (test_ring_perf_esize(16) == -1)
		return -1;

	/* Test for performance gain of compression */
	if (test_ring_perf_compression() == -1)
		return -1;

	return 0;
}

REGISTER_PERF_TEST(ring_perf_autotest, test_ring_perf);
