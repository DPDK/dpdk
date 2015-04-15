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
#include <inttypes.h>
#include <rte_ring.h>
#include <rte_cycles.h>
#include <rte_launch.h>

#include "test.h"

/*
 * Ring
 * ====
 *
 * Measures performance of various operations using rdtsc
 *  * Empty ring dequeue
 *  * Enqueue/dequeue of bursts in 1 threads
 *  * Enqueue/dequeue of bursts in 2 threads
 */

#define RING_NAME "RING_PERF"
#define RING_SIZE 4096
#define MAX_BURST 32

/*
 * the sizes to enqueue and dequeue in testing
 * (marked volatile so they won't be seen as compile-time constants)
 */
static const volatile unsigned bulk_sizes[] = { 8, 32 };

/* The ring structure used for tests */
static struct rte_ring *r;

struct lcore_pair {
	unsigned c1, c2;
};

static volatile unsigned lcore_count = 0;

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
			c1 = lcore_config[id1].core_id;
			c2 = lcore_config[id2].core_id;
			s1 = lcore_config[id1].socket_id;
			s2 = lcore_config[id2].socket_id;
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
			c1 = lcore_config[id1].core_id;
			c2 = lcore_config[id2].core_id;
			s1 = lcore_config[id1].socket_id;
			s2 = lcore_config[id2].socket_id;
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
			s1 = lcore_config[id1].socket_id;
			s2 = lcore_config[id2].socket_id;
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
test_empty_dequeue(void)
{
	const unsigned iter_shift = 26;
	const unsigned iterations = 1<<iter_shift;
	unsigned i = 0;
	void *burst[MAX_BURST];

	const uint64_t sc_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		rte_ring_sc_dequeue_bulk(r, burst, bulk_sizes[0]);
	const uint64_t sc_end = rte_rdtsc();

	const uint64_t mc_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		rte_ring_mc_dequeue_bulk(r, burst, bulk_sizes[0]);
	const uint64_t mc_end = rte_rdtsc();

	printf("SC empty dequeue: %.2F\n",
			(double)(sc_end-sc_start) / iterations);
	printf("MC empty dequeue: %.2F\n",
			(double)(mc_end-mc_start) / iterations);
}

/*
 * for the separate enqueue and dequeue threads they take in one param
 * and return two. Input = burst size, output = cycle average for sp/sc & mp/mc
 */
struct thread_params {
	unsigned size;        /* input value, the burst size */
	double spsc, mpmc;    /* output value, the single or multi timings */
};

/*
 * Function that uses rdtsc to measure timing for ring enqueue. Needs pair
 * thread running dequeue_bulk function
 */
static int
enqueue_bulk(void *p)
{
	const unsigned iter_shift = 23;
	const unsigned iterations = 1<<iter_shift;
	struct thread_params *params = p;
	const unsigned size = params->size;
	unsigned i;
	void *burst[MAX_BURST] = {0};

	if ( __sync_add_and_fetch(&lcore_count, 1) != 2 )
		while(lcore_count != 2)
			rte_pause();

	const uint64_t sp_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		while (rte_ring_sp_enqueue_bulk(r, burst, size) != 0)
			rte_pause();
	const uint64_t sp_end = rte_rdtsc();

	const uint64_t mp_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		while (rte_ring_mp_enqueue_bulk(r, burst, size) != 0)
			rte_pause();
	const uint64_t mp_end = rte_rdtsc();

	params->spsc = ((double)(sp_end - sp_start))/(iterations*size);
	params->mpmc = ((double)(mp_end - mp_start))/(iterations*size);
	return 0;
}

/*
 * Function that uses rdtsc to measure timing for ring dequeue. Needs pair
 * thread running enqueue_bulk function
 */
static int
dequeue_bulk(void *p)
{
	const unsigned iter_shift = 23;
	const unsigned iterations = 1<<iter_shift;
	struct thread_params *params = p;
	const unsigned size = params->size;
	unsigned i;
	void *burst[MAX_BURST] = {0};

	if ( __sync_add_and_fetch(&lcore_count, 1) != 2 )
		while(lcore_count != 2)
			rte_pause();

	const uint64_t sc_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		while (rte_ring_sc_dequeue_bulk(r, burst, size) != 0)
			rte_pause();
	const uint64_t sc_end = rte_rdtsc();

	const uint64_t mc_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		while (rte_ring_mc_dequeue_bulk(r, burst, size) != 0)
			rte_pause();
	const uint64_t mc_end = rte_rdtsc();

	params->spsc = ((double)(sc_end - sc_start))/(iterations*size);
	params->mpmc = ((double)(mc_end - mc_start))/(iterations*size);
	return 0;
}

/*
 * Function that calls the enqueue and dequeue bulk functions on pairs of cores.
 * used to measure ring perf between hyperthreads, cores and sockets.
 */
static void
run_on_core_pair(struct lcore_pair *cores,
		lcore_function_t f1, lcore_function_t f2)
{
	struct thread_params param1 = {0}, param2 = {0};
	unsigned i;
	for (i = 0; i < sizeof(bulk_sizes)/sizeof(bulk_sizes[0]); i++) {
		lcore_count = 0;
		param1.size = param2.size = bulk_sizes[i];
		if (cores->c1 == rte_get_master_lcore()) {
			rte_eal_remote_launch(f2, &param2, cores->c2);
			f1(&param1);
			rte_eal_wait_lcore(cores->c2);
		} else {
			rte_eal_remote_launch(f1, &param1, cores->c1);
			rte_eal_remote_launch(f2, &param2, cores->c2);
			rte_eal_wait_lcore(cores->c1);
			rte_eal_wait_lcore(cores->c2);
		}
		printf("SP/SC bulk enq/dequeue (size: %u): %.2F\n", bulk_sizes[i],
				param1.spsc + param2.spsc);
		printf("MP/MC bulk enq/dequeue (size: %u): %.2F\n", bulk_sizes[i],
				param1.mpmc + param2.mpmc);
	}
}

/*
 * Test function that determines how long an enqueue + dequeue of a single item
 * takes on a single lcore. Result is for comparison with the bulk enq+deq.
 */
static void
test_single_enqueue_dequeue(void)
{
	const unsigned iter_shift = 24;
	const unsigned iterations = 1<<iter_shift;
	unsigned i = 0;
	void *burst = NULL;

	const uint64_t sc_start = rte_rdtsc();
	for (i = 0; i < iterations; i++) {
		rte_ring_sp_enqueue(r, burst);
		rte_ring_sc_dequeue(r, &burst);
	}
	const uint64_t sc_end = rte_rdtsc();

	const uint64_t mc_start = rte_rdtsc();
	for (i = 0; i < iterations; i++) {
		rte_ring_mp_enqueue(r, burst);
		rte_ring_mc_dequeue(r, &burst);
	}
	const uint64_t mc_end = rte_rdtsc();

	printf("SP/SC single enq/dequeue: %"PRIu64"\n",
			(sc_end-sc_start) >> iter_shift);
	printf("MP/MC single enq/dequeue: %"PRIu64"\n",
			(mc_end-mc_start) >> iter_shift);
}

/*
 * Test that does both enqueue and dequeue on a core using the burst() API calls
 * instead of the bulk() calls used in other tests. Results should be the same
 * as for the bulk function called on a single lcore.
 */
static void
test_burst_enqueue_dequeue(void)
{
	const unsigned iter_shift = 23;
	const unsigned iterations = 1<<iter_shift;
	unsigned sz, i = 0;
	void *burst[MAX_BURST] = {0};

	for (sz = 0; sz < sizeof(bulk_sizes)/sizeof(bulk_sizes[0]); sz++) {
		const uint64_t sc_start = rte_rdtsc();
		for (i = 0; i < iterations; i++) {
			rte_ring_sp_enqueue_burst(r, burst, bulk_sizes[sz]);
			rte_ring_sc_dequeue_burst(r, burst, bulk_sizes[sz]);
		}
		const uint64_t sc_end = rte_rdtsc();

		const uint64_t mc_start = rte_rdtsc();
		for (i = 0; i < iterations; i++) {
			rte_ring_mp_enqueue_burst(r, burst, bulk_sizes[sz]);
			rte_ring_mc_dequeue_burst(r, burst, bulk_sizes[sz]);
		}
		const uint64_t mc_end = rte_rdtsc();

		uint64_t mc_avg = ((mc_end-mc_start) >> iter_shift) / bulk_sizes[sz];
		uint64_t sc_avg = ((sc_end-sc_start) >> iter_shift) / bulk_sizes[sz];

		printf("SP/SC burst enq/dequeue (size: %u): %"PRIu64"\n", bulk_sizes[sz],
				sc_avg);
		printf("MP/MC burst enq/dequeue (size: %u): %"PRIu64"\n", bulk_sizes[sz],
				mc_avg);
	}
}

/* Times enqueue and dequeue on a single lcore */
static void
test_bulk_enqueue_dequeue(void)
{
	const unsigned iter_shift = 23;
	const unsigned iterations = 1<<iter_shift;
	unsigned sz, i = 0;
	void *burst[MAX_BURST] = {0};

	for (sz = 0; sz < sizeof(bulk_sizes)/sizeof(bulk_sizes[0]); sz++) {
		const uint64_t sc_start = rte_rdtsc();
		for (i = 0; i < iterations; i++) {
			rte_ring_sp_enqueue_bulk(r, burst, bulk_sizes[sz]);
			rte_ring_sc_dequeue_bulk(r, burst, bulk_sizes[sz]);
		}
		const uint64_t sc_end = rte_rdtsc();

		const uint64_t mc_start = rte_rdtsc();
		for (i = 0; i < iterations; i++) {
			rte_ring_mp_enqueue_bulk(r, burst, bulk_sizes[sz]);
			rte_ring_mc_dequeue_bulk(r, burst, bulk_sizes[sz]);
		}
		const uint64_t mc_end = rte_rdtsc();

		double sc_avg = ((double)(sc_end-sc_start) /
				(iterations * bulk_sizes[sz]));
		double mc_avg = ((double)(mc_end-mc_start) /
				(iterations * bulk_sizes[sz]));

		printf("SP/SC bulk enq/dequeue (size: %u): %.2F\n", bulk_sizes[sz],
				sc_avg);
		printf("MP/MC bulk enq/dequeue (size: %u): %.2F\n", bulk_sizes[sz],
				mc_avg);
	}
}

static int
test_ring_perf(void)
{
	struct lcore_pair cores;
	r = rte_ring_create(RING_NAME, RING_SIZE, rte_socket_id(), 0);
	if (r == NULL && (r = rte_ring_lookup(RING_NAME)) == NULL)
		return -1;

	printf("### Testing single element and burst enq/deq ###\n");
	test_single_enqueue_dequeue();
	test_burst_enqueue_dequeue();

	printf("\n### Testing empty dequeue ###\n");
	test_empty_dequeue();

	printf("\n### Testing using a single lcore ###\n");
	test_bulk_enqueue_dequeue();

	if (get_two_hyperthreads(&cores) == 0) {
		printf("\n### Testing using two hyperthreads ###\n");
		run_on_core_pair(&cores, enqueue_bulk, dequeue_bulk);
	}
	if (get_two_cores(&cores) == 0) {
		printf("\n### Testing using two physical cores ###\n");
		run_on_core_pair(&cores, enqueue_bulk, dequeue_bulk);
	}
	if (get_two_sockets(&cores) == 0) {
		printf("\n### Testing using two NUMA nodes ###\n");
		run_on_core_pair(&cores, enqueue_bulk, dequeue_bulk);
	}
	return 0;
}

static struct test_command ring_perf_cmd = {
	.command = "ring_perf_autotest",
	.callback = test_ring_perf,
};
REGISTER_TEST_COMMAND(ring_perf_cmd);
