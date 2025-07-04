/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */


#include <stdio.h>
#include <inttypes.h>
#include <rte_ring.h>
#include <rte_cycles.h>
#include <rte_launch.h>
#include <rte_ethdev.h>
#include <rte_eth_ring.h>
#include <rte_bus_vdev.h>

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
static uint16_t ring_ethdev_port;

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

	for (sz = 0; sz < RTE_DIM(bulk_sizes); sz++) {
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
	char name[RTE_ETH_NAME_MAX_LEN];

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

	/* release port and ring resources */
	if (rte_eth_dev_stop(ring_ethdev_port) != 0)
		return -1;
	rte_eth_dev_get_name_by_port(ring_ethdev_port, name);
	rte_vdev_uninit(name);
	rte_ring_free(r);
	return 0;
}

REGISTER_PERF_TEST(ring_pmd_perf_autotest, test_ring_pmd_perf);
