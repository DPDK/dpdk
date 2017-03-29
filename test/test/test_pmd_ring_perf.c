/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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
#include <rte_ethdev.h>
#include <rte_eth_ring.h>

#include "test.h"

#define RING_NAME "RING_PERF"
#define RING_SIZE 4096
#define MAX_BURST 32

/*
 * the sizes to enqueue and dequeue in testing
 * (marked volatile so they won't be seen as compile-time constants)
 */
static const volatile unsigned bulk_sizes[] = { 1, 8, 32 };

/* The ring structure used for tests */
static struct rte_ring *r;
static uint8_t ring_ethdev_port;

/* Get cycle counts for dequeuing from an empty ring. Should be 2 or 3 cycles */
static void
test_empty_dequeue(void)
{
	const unsigned iter_shift = 26;
	const unsigned iterations = 1 << iter_shift;
	unsigned i = 0;
	void *burst[MAX_BURST];

	const uint64_t sc_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		rte_ring_sc_dequeue_bulk(r, burst, bulk_sizes[0], NULL);
	const uint64_t sc_end = rte_rdtsc();

	const uint64_t eth_start = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		rte_eth_rx_burst(ring_ethdev_port, 0, (void *)burst,
				bulk_sizes[0]);
	const uint64_t eth_end = rte_rdtsc();

	printf("Ring empty dequeue  : %.1F\n",
			(double)(sc_end - sc_start) / iterations);
	printf("Ethdev empty dequeue: %.1F\n",
			(double)(eth_end - eth_start) / iterations);
}

/*
 * Test function that determines how long an enqueue + dequeue of a single item
 * takes on a single lcore. Result is for comparison with the bulk enq+deq.
 */
static void
test_single_enqueue_dequeue(void)
{
	const unsigned iter_shift = 24;
	const unsigned iterations = 1 << iter_shift;
	unsigned i = 0;
	void *burst = NULL;
	struct rte_mbuf *mburst[1] = { NULL };

	const uint64_t sc_start = rte_rdtsc_precise();
	rte_compiler_barrier();
	for (i = 0; i < iterations; i++) {
		rte_ring_enqueue_bulk(r, &burst, 1, NULL);
		rte_ring_dequeue_bulk(r, &burst, 1, NULL);
	}
	const uint64_t sc_end = rte_rdtsc_precise();
	rte_compiler_barrier();

	const uint64_t eth_start = rte_rdtsc_precise();
	rte_compiler_barrier();
	for (i = 0; i < iterations; i++) {
		rte_eth_tx_burst(ring_ethdev_port, 0, mburst, 1);
		rte_eth_rx_burst(ring_ethdev_port, 0, mburst, 1);
	}
	const uint64_t eth_end = rte_rdtsc_precise();
	rte_compiler_barrier();

	printf("Ring single enq/dequeue  : %"PRIu64"\n",
			(sc_end-sc_start) >> iter_shift);
	printf("Ethdev single enq/dequeue: %"PRIu64"\n",
			(eth_end-eth_start) >> iter_shift);
}

/* Times enqueue and dequeue on a single lcore */
static void
test_bulk_enqueue_dequeue(void)
{
	const unsigned iter_shift = 23;
	const unsigned iterations = 1 << iter_shift;
	unsigned sz, i = 0;
	struct rte_mbuf *burst[MAX_BURST] = {0};

	for (sz = 0; sz < sizeof(bulk_sizes)/sizeof(bulk_sizes[0]); sz++) {
		const uint64_t sc_start = rte_rdtsc();
		for (i = 0; i < iterations; i++) {
			rte_ring_sp_enqueue_bulk(r, (void *)burst,
					bulk_sizes[sz], NULL);
			rte_ring_sc_dequeue_bulk(r, (void *)burst,
					bulk_sizes[sz], NULL);
		}
		const uint64_t sc_end = rte_rdtsc();

		const uint64_t eth_start = rte_rdtsc_precise();
		rte_compiler_barrier();
		for (i = 0; i < iterations; i++) {
			rte_eth_tx_burst(ring_ethdev_port, 0, burst, bulk_sizes[sz]);
			rte_eth_rx_burst(ring_ethdev_port, 0, burst, bulk_sizes[sz]);
		}
		const uint64_t eth_end = rte_rdtsc_precise();
		rte_compiler_barrier();

		double sc_avg = ((double)(sc_end-sc_start) /
				(iterations * bulk_sizes[sz]));
		double eth_avg = ((double)(eth_end-eth_start) /
				(iterations * bulk_sizes[sz]));

		printf("ring bulk enq/deq (size: %u) : %.1F\n", bulk_sizes[sz],
				sc_avg);
		printf("ethdev bulk enq/deq (size:%u): %.1F\n", bulk_sizes[sz],
				eth_avg);

		printf("\n");
	}
}

static int
test_ring_pmd_perf(void)
{
	r = rte_ring_create(RING_NAME, RING_SIZE, rte_socket_id(),
			RING_F_SP_ENQ|RING_F_SC_DEQ);
	if (r == NULL && (r = rte_ring_lookup(RING_NAME)) == NULL)
		return -1;

	ring_ethdev_port = rte_eth_from_ring(r);

	printf("\n### Testing const single element enq/deq ###\n");
	test_single_enqueue_dequeue();

	printf("\n### Testing empty dequeue ###\n");
	test_empty_dequeue();

	printf("\n### Testing using a single lcore ###\n");
	test_bulk_enqueue_dequeue();

	return 0;
}

REGISTER_TEST_COMMAND(ring_pmd_perf_autotest, test_ring_pmd_perf);
