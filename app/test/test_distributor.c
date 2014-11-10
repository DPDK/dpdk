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
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_distributor.h>

#define ITER_POWER 20 /* log 2 of how many iterations we do when timing. */
#define BURST 32
#define BIG_BATCH 1024

/* statics - all zero-initialized by default */
static volatile int quit;      /**< general quit variable for all threads */
static volatile int zero_quit; /**< var for when we just want thr0 to quit*/
static volatile unsigned worker_idx;

struct worker_stats {
	volatile unsigned handled_packets;
} __rte_cache_aligned;
struct worker_stats worker_stats[RTE_MAX_LCORE];

/* returns the total count of the number of packets handled by the worker
 * functions given below.
 */
static inline unsigned
total_packet_count(void)
{
	unsigned i, count = 0;
	for (i = 0; i < worker_idx; i++)
		count += worker_stats[i].handled_packets;
	return count;
}

/* resets the packet counts for a new test */
static inline void
clear_packet_count(void)
{
	memset(&worker_stats, 0, sizeof(worker_stats));
}

/* this is the basic worker function for sanity test
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

/* do basic sanity testing of the distributor. This test tests the following:
 * - send 32 packets through distributor with the same tag and ensure they
 *   all go to the one worker
 * - send 32 packets throught the distributor with two different tags and
 *   verify that they go equally to two different workers.
 * - send 32 packets with different tags through the distributors and
 *   just verify we get all packets back.
 * - send 1024 packets through the distributor, gathering the returned packets
 *   as we go. Then verify that we correctly got all 1024 pointers back again,
 *   not necessarily in the same order (as different flows).
 */
static int
sanity_test(struct rte_distributor *d, struct rte_mempool *p)
{
	struct rte_mbuf *bufs[BURST];
	unsigned i;

	printf("=== Basic distributor sanity tests ===\n");
	clear_packet_count();
	if (rte_mempool_get_bulk(p, (void *)bufs, BURST) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}

	/* now set all hash values in all buffers to zero, so all pkts go to the
	 * one worker thread */
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = 0;

	rte_distributor_process(d, bufs, BURST);
	rte_distributor_flush(d);
	if (total_packet_count() != BURST) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST, total_packet_count());
		return -1;
	}

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
				worker_stats[i].handled_packets);
	printf("Sanity test with all zero hashes done.\n");
	if (worker_stats[0].handled_packets != BURST)
		return -1;

	/* pick two flows and check they go correctly */
	if (rte_lcore_count() >= 3) {
		clear_packet_count();
		for (i = 0; i < BURST; i++)
			bufs[i]->hash.usr = (i & 1) << 8;

		rte_distributor_process(d, bufs, BURST);
		rte_distributor_flush(d);
		if (total_packet_count() != BURST) {
			printf("Line %d: Error, not all packets flushed. "
					"Expected %u, got %u\n",
					__LINE__, BURST, total_packet_count());
			return -1;
		}

		for (i = 0; i < rte_lcore_count() - 1; i++)
			printf("Worker %u handled %u packets\n", i,
					worker_stats[i].handled_packets);
		printf("Sanity test with two hash values done\n");

		if (worker_stats[0].handled_packets != 16 ||
				worker_stats[1].handled_packets != 16)
			return -1;
	}

	/* give a different hash value to each packet,
	 * so load gets distributed */
	clear_packet_count();
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = i;

	rte_distributor_process(d, bufs, BURST);
	rte_distributor_flush(d);
	if (total_packet_count() != BURST) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST, total_packet_count());
		return -1;
	}

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
				worker_stats[i].handled_packets);
	printf("Sanity test with non-zero hashes done\n");

	rte_mempool_put_bulk(p, (void *)bufs, BURST);

	/* sanity test with BIG_BATCH packets to ensure they all arrived back
	 * from the returned packets function */
	clear_packet_count();
	struct rte_mbuf *many_bufs[BIG_BATCH], *return_bufs[BIG_BATCH];
	unsigned num_returned = 0;

	/* flush out any remaining packets */
	rte_distributor_flush(d);
	rte_distributor_clear_returns(d);
	if (rte_mempool_get_bulk(p, (void *)many_bufs, BIG_BATCH) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}
	for (i = 0; i < BIG_BATCH; i++)
		many_bufs[i]->hash.usr = i << 2;

	for (i = 0; i < BIG_BATCH/BURST; i++) {
		rte_distributor_process(d, &many_bufs[i*BURST], BURST);
		num_returned += rte_distributor_returned_pkts(d,
				&return_bufs[num_returned],
				BIG_BATCH - num_returned);
	}
	rte_distributor_flush(d);
	num_returned += rte_distributor_returned_pkts(d,
			&return_bufs[num_returned], BIG_BATCH - num_returned);

	if (num_returned != BIG_BATCH) {
		printf("line %d: Number returned is not the same as "
				"number sent\n", __LINE__);
		return -1;
	}
	/* big check -  make sure all packets made it back!! */
	for (i = 0; i < BIG_BATCH; i++) {
		unsigned j;
		struct rte_mbuf *src = many_bufs[i];
		for (j = 0; j < BIG_BATCH; j++)
			if (return_bufs[j] == src)
				break;

		if (j == BIG_BATCH) {
			printf("Error: could not find source packet #%u\n", i);
			return -1;
		}
	}
	printf("Sanity test of returned packets done\n");

	rte_mempool_put_bulk(p, (void *)many_bufs, BIG_BATCH);

	printf("\n");
	return 0;
}


/* to test that the distributor does not lose packets, we use this worker
 * function which frees mbufs when it gets them. The distributor thread does
 * the mbuf allocation. If distributor drops packets we'll eventually run out
 * of mbufs.
 */
static int
handle_work_with_free_mbufs(void *arg)
{
	struct rte_mbuf *pkt = NULL;
	struct rte_distributor *d = arg;
	unsigned count = 0;
	unsigned id = __sync_fetch_and_add(&worker_idx, 1);

	pkt = rte_distributor_get_pkt(d, id, NULL);
	while (!quit) {
		worker_stats[id].handled_packets++, count++;
		rte_pktmbuf_free(pkt);
		pkt = rte_distributor_get_pkt(d, id, pkt);
	}
	worker_stats[id].handled_packets++, count++;
	rte_distributor_return_pkt(d, id, pkt);
	return 0;
}

/* Perform a sanity test of the distributor with a large number of packets,
 * where we allocate a new set of mbufs for each burst. The workers then
 * free the mbufs. This ensures that we don't have any packet leaks in the
 * library.
 */
static int
sanity_test_with_mbuf_alloc(struct rte_distributor *d, struct rte_mempool *p)
{
	unsigned i;
	struct rte_mbuf *bufs[BURST];

	printf("=== Sanity test with mbuf alloc/free  ===\n");
	clear_packet_count();
	for (i = 0; i < ((1<<ITER_POWER)); i += BURST) {
		unsigned j;
		while (rte_mempool_get_bulk(p, (void *)bufs, BURST) < 0)
			rte_distributor_process(d, NULL, 0);
		for (j = 0; j < BURST; j++) {
			bufs[j]->hash.usr = (i+j) << 1;
			rte_mbuf_refcnt_set(bufs[j], 1);
		}

		rte_distributor_process(d, bufs, BURST);
	}

	rte_distributor_flush(d);
	if (total_packet_count() < (1<<ITER_POWER)) {
		printf("Line %u: Packet count is incorrect, %u, expected %u\n",
				__LINE__, total_packet_count(),
				(1<<ITER_POWER));
		return -1;
	}

	printf("Sanity test with mbuf alloc/free passed\n\n");
	return 0;
}

static int
handle_work_for_shutdown_test(void *arg)
{
	struct rte_mbuf *pkt = NULL;
	struct rte_distributor *d = arg;
	unsigned count = 0;
	const unsigned id = __sync_fetch_and_add(&worker_idx, 1);

	pkt = rte_distributor_get_pkt(d, id, NULL);
	/* wait for quit single globally, or for worker zero, wait
	 * for zero_quit */
	while (!quit && !(id == 0 && zero_quit)) {
		worker_stats[id].handled_packets++, count++;
		rte_pktmbuf_free(pkt);
		pkt = rte_distributor_get_pkt(d, id, NULL);
	}
	worker_stats[id].handled_packets++, count++;
	rte_distributor_return_pkt(d, id, pkt);

	if (id == 0) {
		/* for worker zero, allow it to restart to pick up last packet
		 * when all workers are shutting down.
		 */
		while (zero_quit)
			usleep(100);
		pkt = rte_distributor_get_pkt(d, id, NULL);
		while (!quit) {
			worker_stats[id].handled_packets++, count++;
			rte_pktmbuf_free(pkt);
			pkt = rte_distributor_get_pkt(d, id, NULL);
		}
		rte_distributor_return_pkt(d, id, pkt);
	}
	return 0;
}


/* Perform a sanity test of the distributor with a large number of packets,
 * where we allocate a new set of mbufs for each burst. The workers then
 * free the mbufs. This ensures that we don't have any packet leaks in the
 * library.
 */
static int
sanity_test_with_worker_shutdown(struct rte_distributor *d,
		struct rte_mempool *p)
{
	struct rte_mbuf *bufs[BURST];
	unsigned i;

	printf("=== Sanity test of worker shutdown ===\n");

	clear_packet_count();
	if (rte_mempool_get_bulk(p, (void *)bufs, BURST) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}

	/* now set all hash values in all buffers to zero, so all pkts go to the
	 * one worker thread */
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = 0;

	rte_distributor_process(d, bufs, BURST);
	/* at this point, we will have processed some packets and have a full
	 * backlog for the other ones at worker 0.
	 */

	/* get more buffers to queue up, again setting them to the same flow */
	if (rte_mempool_get_bulk(p, (void *)bufs, BURST) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = 0;

	/* get worker zero to quit */
	zero_quit = 1;
	rte_distributor_process(d, bufs, BURST);

	/* flush the distributor */
	rte_distributor_flush(d);
	if (total_packet_count() != BURST * 2) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST * 2, total_packet_count());
		return -1;
	}

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
				worker_stats[i].handled_packets);

	printf("Sanity test with worker shutdown passed\n\n");
	return 0;
}

/* Test that the flush function is able to move packets between workers when
 * one worker shuts down..
 */
static int
test_flush_with_worker_shutdown(struct rte_distributor *d,
		struct rte_mempool *p)
{
	struct rte_mbuf *bufs[BURST];
	unsigned i;

	printf("=== Test flush fn with worker shutdown ===\n");

	clear_packet_count();
	if (rte_mempool_get_bulk(p, (void *)bufs, BURST) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}

	/* now set all hash values in all buffers to zero, so all pkts go to the
	 * one worker thread */
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = 0;

	rte_distributor_process(d, bufs, BURST);
	/* at this point, we will have processed some packets and have a full
	 * backlog for the other ones at worker 0.
	 */

	/* get worker zero to quit */
	zero_quit = 1;

	/* flush the distributor */
	rte_distributor_flush(d);

	zero_quit = 0;
	if (total_packet_count() != BURST) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST, total_packet_count());
		return -1;
	}

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
				worker_stats[i].handled_packets);

	printf("Flush test with worker shutdown passed\n\n");
	return 0;
}

static
int test_error_distributor_create_name(void)
{
	struct rte_distributor *d = NULL;
	char *name = NULL;

	d = rte_distributor_create(name, rte_socket_id(),
			rte_lcore_count() - 1);
	if (d != NULL || rte_errno != EINVAL) {
		printf("ERROR: No error on create() with NULL name param\n");
		return -1;
	}

	return 0;
}


static
int test_error_distributor_create_numworkers(void)
{
	struct rte_distributor *d = NULL;
	d = rte_distributor_create("test_numworkers", rte_socket_id(),
			RTE_MAX_LCORE + 10);
	if (d != NULL || rte_errno != EINVAL) {
		printf("ERROR: No error on create() with num_workers > MAX\n");
		return -1;
	}
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

	zero_quit = 0;
	quit = 1;
	for (i = 0; i < num_workers; i++)
		bufs[i]->hash.usr = i << 1;
	rte_distributor_process(d, bufs, num_workers);

	rte_mempool_put_bulk(p, (void *)bufs, num_workers);

	rte_distributor_process(d, NULL, 0);
	rte_distributor_flush(d);
	rte_eal_mp_wait_lcore();
	quit = 0;
	worker_idx = 0;
}

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

static int
test_distributor(void)
{
	static struct rte_distributor *d;
	static struct rte_mempool *p;

	if (rte_lcore_count() < 2) {
		printf("ERROR: not enough cores to test distributor\n");
		return -1;
	}

	if (d == NULL) {
		d = rte_distributor_create("Test_distributor", rte_socket_id(),
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
		p = rte_mempool_create("DT_MBUF_POOL", nb_bufs,
				MBUF_SIZE, BURST,
				sizeof(struct rte_pktmbuf_pool_private),
				rte_pktmbuf_pool_init, NULL,
				rte_pktmbuf_init, NULL,
				rte_socket_id(), 0);
		if (p == NULL) {
			printf("Error creating mempool\n");
			return -1;
		}
	}

	rte_eal_mp_remote_launch(handle_work, d, SKIP_MASTER);
	if (sanity_test(d, p) < 0)
		goto err;
	quit_workers(d, p);

	rte_eal_mp_remote_launch(handle_work_with_free_mbufs, d, SKIP_MASTER);
	if (sanity_test_with_mbuf_alloc(d, p) < 0)
		goto err;
	quit_workers(d, p);

	if (rte_lcore_count() > 2) {
		rte_eal_mp_remote_launch(handle_work_for_shutdown_test, d,
				SKIP_MASTER);
		if (sanity_test_with_worker_shutdown(d, p) < 0)
			goto err;
		quit_workers(d, p);

		rte_eal_mp_remote_launch(handle_work_for_shutdown_test, d,
				SKIP_MASTER);
		if (test_flush_with_worker_shutdown(d, p) < 0)
			goto err;
		quit_workers(d, p);

	} else {
		printf("Not enough cores to run tests for worker shutdown\n");
	}

	if (test_error_distributor_create_numworkers() == -1 ||
			test_error_distributor_create_name() == -1) {
		printf("rte_distributor_create parameter check tests failed");
		return -1;
	}

	return 0;

err:
	quit_workers(d, p);
	return -1;
}

static struct test_command distributor_cmd = {
	.command = "distributor_autotest",
	.callback = test_distributor,
};
REGISTER_TEST_COMMAND(distributor_cmd);
