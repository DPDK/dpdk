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

#include "test.h"

#include <unistd.h>
#include <string.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_distributor.h>

#define ITER_POWER 20 /* log 2 of how many iterations we do when timing. */
#define BURST 32
#define BIG_BATCH 1024

/* static vars - zero initialized by default */
static volatile int quit;
static volatile unsigned worker_idx;

struct worker_stats {
	volatile unsigned handled_packets;
} __rte_cache_aligned;
struct worker_stats worker_stats[RTE_MAX_LCORE];

/* worker thread used for testing the time to do a round-trip of a cache
 * line between two cores and back again
 */
static void
flip_bit(volatile uint64_t *arg)
{
	uint64_t old_val = 0;
	while (old_val != 2) {
		while (!*arg)
			rte_pause();
		old_val = *arg;
		*arg = 0;
	}
}

/* test case to time the number of cycles to round-trip a cache line between
 * two cores and back again.
 */
static void
time_cache_line_switch(void)
{
	/* allocate a full cache line for data, we use only first byte of it */
	uint64_t data[RTE_CACHE_LINE_SIZE*3 / sizeof(uint64_t)];

	unsigned i, slaveid = rte_get_next_lcore(rte_lcore_id(), 0, 0);
	volatile uint64_t *pdata = &data[0];
	*pdata = 1;
	rte_eal_remote_launch((lcore_function_t *)flip_bit, &data[0], slaveid);
	while (*pdata)
		rte_pause();

	const uint64_t start_time = rte_rdtsc();
	for (i = 0; i < (1 << ITER_POWER); i++) {
		while (*pdata)
			rte_pause();
		*pdata = 1;
	}
	const uint64_t end_time = rte_rdtsc();

	while (*pdata)
		rte_pause();
	*pdata = 2;
	rte_eal_wait_lcore(slaveid);
	printf("==== Cache line switch test ===\n");
	printf("Time for %u iterations = %"PRIu64" ticks\n", (1<<ITER_POWER),
			end_time-start_time);
	printf("Ticks per iteration = %"PRIu64"\n\n",
			(end_time-start_time) >> ITER_POWER);
}

/* returns the total count of the number of packets handled by the worker
 * functions given below.
 */
static unsigned
total_packet_count(void)
{
	unsigned i, count = 0;
	for (i = 0; i < worker_idx; i++)
		count += worker_stats[i].handled_packets;
	return count;
}

/* resets the packet counts for a new test */
static void
clear_packet_count(void)
{
	memset(&worker_stats, 0, sizeof(worker_stats));
}

/* this is the basic worker function for performance tests.
 * it does nothing but return packets and count them.
 */
static int
handle_work(void *arg)
{
	struct rte_mbuf *pkt = NULL;
	struct rte_distributor *d = arg;
	unsigned count = 0;
	unsigned id = __sync_fetch_and_add(&worker_idx, 1);

	pkt = rte_distributor_get_pkt(d, id, NULL);
	while (!quit) {
		worker_stats[id].handled_packets++, count++;
		pkt = rte_distributor_get_pkt(d, id, pkt);
	}
	worker_stats[id].handled_packets++, count++;
	rte_distributor_return_pkt(d, id, pkt);
	return 0;
}

/* this basic performance test just repeatedly sends in 32 packets at a time
 * to the distributor and verifies at the end that we got them all in the worker
 * threads and finally how long per packet the processing took.
 */
static inline int
perf_test(struct rte_distributor *d, struct rte_mempool *p)
{
	unsigned i;
	uint64_t start, end;
	struct rte_mbuf *bufs[BURST];

	clear_packet_count();
	if (rte_mempool_get_bulk(p, (void *)bufs, BURST) != 0) {
		printf("Error getting mbufs from pool\n");
		return -1;
	}
	/* ensure we have different hash value for each pkt */
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = i;

	start = rte_rdtsc();
	for (i = 0; i < (1<<ITER_POWER); i++)
		rte_distributor_process(d, bufs, BURST);
	end = rte_rdtsc();

	do {
		usleep(100);
		rte_distributor_process(d, NULL, 0);
	} while (total_packet_count() < (BURST << ITER_POWER));

	printf("=== Performance test of distributor ===\n");
	printf("Time per burst:  %"PRIu64"\n", (end - start) >> ITER_POWER);
	printf("Time per packet: %"PRIu64"\n\n",
			((end - start) >> ITER_POWER)/BURST);
	rte_mempool_put_bulk(p, (void *)bufs, BURST);

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
				worker_stats[i].handled_packets);
	printf("Total packets: %u (%x)\n", total_packet_count(),
			total_packet_count());
	printf("=== Perf test done ===\n\n");

	return 0;
}

/* Useful function which ensures that all worker functions terminate */
static void
quit_workers(struct rte_distributor *d, struct rte_mempool *p)
{
	const unsigned num_workers = rte_lcore_count() - 1;
	unsigned i;
	struct rte_mbuf *bufs[RTE_MAX_LCORE];
	rte_mempool_get_bulk(p, (void *)bufs, num_workers);

	quit = 1;
	for (i = 0; i < num_workers; i++)
		bufs[i]->hash.usr = i << 1;
	rte_distributor_process(d, bufs, num_workers);

	rte_mempool_put_bulk(p, (void *)bufs, num_workers);

	rte_distributor_process(d, NULL, 0);
	rte_eal_mp_wait_lcore();
	quit = 0;
	worker_idx = 0;
}

static int
test_distributor_perf(void)
{
	static struct rte_distributor *d;
	static struct rte_mempool *p;

	if (rte_lcore_count() < 2) {
		printf("ERROR: not enough cores to test distributor\n");
		return -1;
	}

	/* first time how long it takes to round-trip a cache line */
	time_cache_line_switch();

	if (d == NULL) {
		d = rte_distributor_create("Test_perf", rte_socket_id(),
				rte_lcore_count() - 1);
		if (d == NULL) {
			printf("Error creating distributor\n");
			return -1;
		}
	} else {
		rte_distributor_flush(d);
		rte_distributor_clear_returns(d);
	}

	const unsigned nb_bufs = (511 * rte_lcore_count()) < BIG_BATCH ?
			(BIG_BATCH * 2) - 1 : (511 * rte_lcore_count());
	if (p == NULL) {
		p = rte_pktmbuf_pool_create("DPT_MBUF_POOL", nb_bufs, BURST,
			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
		if (p == NULL) {
			printf("Error creating mempool\n");
			return -1;
		}
	}

	rte_eal_mp_remote_launch(handle_work, d, SKIP_MASTER);
	if (perf_test(d, p) < 0)
		return -1;
	quit_workers(d, p);

	return 0;
}

REGISTER_TEST_COMMAND(distributor_perf_autotest, test_distributor_perf);
