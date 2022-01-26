/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "test.h"

#include <unistd.h>
#include <string.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_distributor(void)
{
	printf("distributor not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_distributor.h>
#include <rte_string_fns.h>

#define ITER_POWER 20 /* log 2 of how many iterations we do when timing. */
#define BURST 32
#define BIG_BATCH 1024

typedef uint32_t seq_dynfield_t;
static int seq_dynfield_offset = -1;

static inline seq_dynfield_t *
seq_field(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, seq_dynfield_offset, seq_dynfield_t *);
}

struct worker_params {
	char name[64];
	struct rte_distributor *dist;
};

struct worker_params worker_params;

/* statics - all zero-initialized by default */
static volatile int quit;      /**< general quit variable for all threads */
static volatile int zero_quit; /**< var for when we just want thr0 to quit*/
static volatile int zero_sleep; /**< thr0 has quit basic loop and is sleeping*/
static volatile unsigned worker_idx;
static volatile unsigned zero_idx;

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
		count += __atomic_load_n(&worker_stats[i].handled_packets,
				__ATOMIC_RELAXED);
	return count;
}

/* resets the packet counts for a new test */
static inline void
clear_packet_count(void)
{
	unsigned int i;
	for (i = 0; i < RTE_MAX_LCORE; i++)
		__atomic_store_n(&worker_stats[i].handled_packets, 0,
			__ATOMIC_RELAXED);
}

/* this is the basic worker function for sanity test
 * it does nothing but return packets and count them.
 */
static int
handle_work(void *arg)
{
	struct rte_mbuf *buf[8] __rte_cache_aligned;
	struct worker_params *wp = arg;
	struct rte_distributor *db = wp->dist;
	unsigned int num;
	unsigned int id = __atomic_fetch_add(&worker_idx, 1, __ATOMIC_RELAXED);

	num = rte_distributor_get_pkt(db, id, buf, NULL, 0);
	while (!quit) {
		__atomic_fetch_add(&worker_stats[id].handled_packets, num,
				__ATOMIC_RELAXED);
		num = rte_distributor_get_pkt(db, id,
				buf, buf, num);
	}
	__atomic_fetch_add(&worker_stats[id].handled_packets, num,
			__ATOMIC_RELAXED);
	rte_distributor_return_pkt(db, id, buf, num);
	return 0;
}

/* do basic sanity testing of the distributor. This test tests the following:
 * - send 32 packets through distributor with the same tag and ensure they
 *   all go to the one worker
 * - send 32 packets through the distributor with two different tags and
 *   verify that they go equally to two different workers.
 * - send 32 packets with different tags through the distributors and
 *   just verify we get all packets back.
 * - send 1024 packets through the distributor, gathering the returned packets
 *   as we go. Then verify that we correctly got all 1024 pointers back again,
 *   not necessarily in the same order (as different flows).
 */
static int
sanity_test(struct worker_params *wp, struct rte_mempool *p)
{
	struct rte_distributor *db = wp->dist;
	struct rte_mbuf *bufs[BURST];
	struct rte_mbuf *returns[BURST*2];
	unsigned int i, count;
	unsigned int retries;
	unsigned int processed;

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

	processed = 0;
	while (processed < BURST)
		processed += rte_distributor_process(db, &bufs[processed],
			BURST - processed);

	count = 0;
	do {

		rte_distributor_flush(db);
		count += rte_distributor_returned_pkts(db,
				returns, BURST*2);
	} while (count < BURST);

	if (total_packet_count() != BURST) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST, total_packet_count());
		rte_mempool_put_bulk(p, (void *)bufs, BURST);
		return -1;
	}

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
			__atomic_load_n(&worker_stats[i].handled_packets,
					__ATOMIC_RELAXED));
	printf("Sanity test with all zero hashes done.\n");

	/* pick two flows and check they go correctly */
	if (rte_lcore_count() >= 3) {
		clear_packet_count();
		for (i = 0; i < BURST; i++)
			bufs[i]->hash.usr = (i & 1) << 8;

		rte_distributor_process(db, bufs, BURST);
		count = 0;
		do {
			rte_distributor_flush(db);
			count += rte_distributor_returned_pkts(db,
					returns, BURST*2);
		} while (count < BURST);
		if (total_packet_count() != BURST) {
			printf("Line %d: Error, not all packets flushed. "
					"Expected %u, got %u\n",
					__LINE__, BURST, total_packet_count());
			rte_mempool_put_bulk(p, (void *)bufs, BURST);
			return -1;
		}

		for (i = 0; i < rte_lcore_count() - 1; i++)
			printf("Worker %u handled %u packets\n", i,
				__atomic_load_n(
					&worker_stats[i].handled_packets,
					__ATOMIC_RELAXED));
		printf("Sanity test with two hash values done\n");
	}

	/* give a different hash value to each packet,
	 * so load gets distributed */
	clear_packet_count();
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = i+1;

	rte_distributor_process(db, bufs, BURST);
	count = 0;
	do {
		rte_distributor_flush(db);
		count += rte_distributor_returned_pkts(db,
				returns, BURST*2);
	} while (count < BURST);
	if (total_packet_count() != BURST) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST, total_packet_count());
		rte_mempool_put_bulk(p, (void *)bufs, BURST);
		return -1;
	}

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
			__atomic_load_n(&worker_stats[i].handled_packets,
					__ATOMIC_RELAXED));
	printf("Sanity test with non-zero hashes done\n");

	rte_mempool_put_bulk(p, (void *)bufs, BURST);

	/* sanity test with BIG_BATCH packets to ensure they all arrived back
	 * from the returned packets function */
	clear_packet_count();
	struct rte_mbuf *many_bufs[BIG_BATCH], *return_bufs[BIG_BATCH];
	unsigned num_returned = 0;
	unsigned int num_being_processed = 0;
	unsigned int return_buffer_capacity = 127;/* RTE_DISTRIB_RETURNS_MASK */

	/* flush out any remaining packets */
	rte_distributor_flush(db);
	rte_distributor_clear_returns(db);

	if (rte_mempool_get_bulk(p, (void *)many_bufs, BIG_BATCH) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}
	for (i = 0; i < BIG_BATCH; i++)
		many_bufs[i]->hash.usr = i << 2;

	printf("=== testing big burst (%s) ===\n", wp->name);
	for (i = 0; i < BIG_BATCH/BURST; i++) {
		rte_distributor_process(db,
				&many_bufs[i*BURST], BURST);
		num_being_processed += BURST;
		do {
			count = rte_distributor_returned_pkts(db,
					&return_bufs[num_returned],
					BIG_BATCH - num_returned);
			num_being_processed -= count;
			num_returned += count;
			rte_distributor_flush(db);
		} while (num_being_processed + BURST > return_buffer_capacity);
	}
	retries = 0;
	do {
		rte_distributor_flush(db);
		count = rte_distributor_returned_pkts(db,
				&return_bufs[num_returned],
				BIG_BATCH - num_returned);
		num_returned += count;
		retries++;
	} while ((num_returned < BIG_BATCH) && (retries < 100));

	if (num_returned != BIG_BATCH) {
		printf("line %d: Missing packets, expected %d\n",
				__LINE__, num_returned);
		rte_mempool_put_bulk(p, (void *)many_bufs, BIG_BATCH);
		return -1;
	}

	/* big check -  make sure all packets made it back!! */
	for (i = 0; i < BIG_BATCH; i++) {
		unsigned j;
		struct rte_mbuf *src = many_bufs[i];
		for (j = 0; j < BIG_BATCH; j++) {
			if (return_bufs[j] == src)
				break;
		}

		if (j == BIG_BATCH) {
			printf("Error: could not find source packet #%u\n", i);
			rte_mempool_put_bulk(p, (void *)many_bufs, BIG_BATCH);
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
	struct rte_mbuf *buf[8] __rte_cache_aligned;
	struct worker_params *wp = arg;
	struct rte_distributor *d = wp->dist;
	unsigned int i;
	unsigned int num;
	unsigned int id = __atomic_fetch_add(&worker_idx, 1, __ATOMIC_RELAXED);

	num = rte_distributor_get_pkt(d, id, buf, NULL, 0);
	while (!quit) {
		__atomic_fetch_add(&worker_stats[id].handled_packets, num,
				__ATOMIC_RELAXED);
		for (i = 0; i < num; i++)
			rte_pktmbuf_free(buf[i]);
		num = rte_distributor_get_pkt(d, id, buf, NULL, 0);
	}
	__atomic_fetch_add(&worker_stats[id].handled_packets, num,
			__ATOMIC_RELAXED);
	rte_distributor_return_pkt(d, id, buf, num);
	return 0;
}

/* Perform a sanity test of the distributor with a large number of packets,
 * where we allocate a new set of mbufs for each burst. The workers then
 * free the mbufs. This ensures that we don't have any packet leaks in the
 * library.
 */
static int
sanity_test_with_mbuf_alloc(struct worker_params *wp, struct rte_mempool *p)
{
	struct rte_distributor *d = wp->dist;
	unsigned i;
	struct rte_mbuf *bufs[BURST];
	unsigned int processed;

	printf("=== Sanity test with mbuf alloc/free (%s) ===\n", wp->name);

	clear_packet_count();
	for (i = 0; i < ((1<<ITER_POWER)); i += BURST) {
		unsigned j;
		while (rte_mempool_get_bulk(p, (void *)bufs, BURST) < 0)
			rte_distributor_process(d, NULL, 0);
		for (j = 0; j < BURST; j++) {
			bufs[j]->hash.usr = (i+j) << 1;
		}

		processed = 0;
		while (processed < BURST)
			processed += rte_distributor_process(d,
				&bufs[processed], BURST - processed);
	}

	rte_distributor_flush(d);

	rte_delay_us(10000);

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
	struct rte_mbuf *buf[8] __rte_cache_aligned;
	struct worker_params *wp = arg;
	struct rte_distributor *d = wp->dist;
	unsigned int num;
	unsigned int zero_id = 0;
	unsigned int zero_unset;
	const unsigned int id = __atomic_fetch_add(&worker_idx, 1,
			__ATOMIC_RELAXED);

	num = rte_distributor_get_pkt(d, id, buf, NULL, 0);

	if (num > 0) {
		zero_unset = RTE_MAX_LCORE;
		__atomic_compare_exchange_n(&zero_idx, &zero_unset, id,
			false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
	}
	zero_id = __atomic_load_n(&zero_idx, __ATOMIC_ACQUIRE);

	/* wait for quit single globally, or for worker zero, wait
	 * for zero_quit */
	while (!quit && !(id == zero_id && zero_quit)) {
		__atomic_fetch_add(&worker_stats[id].handled_packets, num,
				__ATOMIC_RELAXED);
		num = rte_distributor_get_pkt(d, id, buf, NULL, 0);

		if (num > 0) {
			zero_unset = RTE_MAX_LCORE;
			__atomic_compare_exchange_n(&zero_idx, &zero_unset, id,
				false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
		}
		zero_id = __atomic_load_n(&zero_idx, __ATOMIC_ACQUIRE);
	}

	__atomic_fetch_add(&worker_stats[id].handled_packets, num,
			__ATOMIC_RELAXED);
	if (id == zero_id) {
		rte_distributor_return_pkt(d, id, NULL, 0);

		/* for worker zero, allow it to restart to pick up last packet
		 * when all workers are shutting down.
		 */
		__atomic_store_n(&zero_sleep, 1, __ATOMIC_RELEASE);
		while (zero_quit)
			usleep(100);
		__atomic_store_n(&zero_sleep, 0, __ATOMIC_RELEASE);

		num = rte_distributor_get_pkt(d, id, buf, NULL, 0);

		while (!quit) {
			__atomic_fetch_add(&worker_stats[id].handled_packets,
					num, __ATOMIC_RELAXED);
			num = rte_distributor_get_pkt(d, id, buf, NULL, 0);
		}
	}
	rte_distributor_return_pkt(d, id, buf, num);
	return 0;
}


/* Perform a sanity test of the distributor with a large number of packets,
 * where we allocate a new set of mbufs for each burst. The workers then
 * free the mbufs. This ensures that we don't have any packet leaks in the
 * library.
 */
static int
sanity_test_with_worker_shutdown(struct worker_params *wp,
		struct rte_mempool *p)
{
	struct rte_distributor *d = wp->dist;
	struct rte_mbuf *bufs[BURST];
	struct rte_mbuf *bufs2[BURST];
	unsigned int i;
	unsigned int failed = 0;
	unsigned int processed = 0;

	printf("=== Sanity test of worker shutdown ===\n");

	clear_packet_count();

	if (rte_mempool_get_bulk(p, (void *)bufs, BURST) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}

	/*
	 * Now set all hash values in all buffers to same value so all
	 * pkts go to the one worker thread
	 */
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = 1;

	processed = 0;
	while (processed < BURST)
		processed += rte_distributor_process(d, &bufs[processed],
			BURST - processed);
	rte_distributor_flush(d);

	/* at this point, we will have processed some packets and have a full
	 * backlog for the other ones at worker 0.
	 */

	/* get more buffers to queue up, again setting them to the same flow */
	if (rte_mempool_get_bulk(p, (void *)bufs2, BURST) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		rte_mempool_put_bulk(p, (void *)bufs, BURST);
		return -1;
	}
	for (i = 0; i < BURST; i++)
		bufs2[i]->hash.usr = 1;

	/* get worker zero to quit */
	zero_quit = 1;
	rte_distributor_process(d, bufs2, BURST);

	/* flush the distributor */
	rte_distributor_flush(d);
	while (!__atomic_load_n(&zero_sleep, __ATOMIC_ACQUIRE))
		rte_distributor_flush(d);

	zero_quit = 0;
	while (__atomic_load_n(&zero_sleep, __ATOMIC_ACQUIRE))
		rte_delay_us(100);

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
			__atomic_load_n(&worker_stats[i].handled_packets,
					__ATOMIC_RELAXED));

	if (total_packet_count() != BURST * 2) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST * 2, total_packet_count());
		failed = 1;
	}

	rte_mempool_put_bulk(p, (void *)bufs, BURST);
	rte_mempool_put_bulk(p, (void *)bufs2, BURST);

	if (failed)
		return -1;

	printf("Sanity test with worker shutdown passed\n\n");
	return 0;
}

/* Test that the flush function is able to move packets between workers when
 * one worker shuts down..
 */
static int
test_flush_with_worker_shutdown(struct worker_params *wp,
		struct rte_mempool *p)
{
	struct rte_distributor *d = wp->dist;
	struct rte_mbuf *bufs[BURST];
	unsigned int i;
	unsigned int failed = 0;
	unsigned int processed;

	printf("=== Test flush fn with worker shutdown (%s) ===\n", wp->name);

	clear_packet_count();
	if (rte_mempool_get_bulk(p, (void *)bufs, BURST) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}

	/* now set all hash values in all buffers to zero, so all pkts go to the
	 * one worker thread */
	for (i = 0; i < BURST; i++)
		bufs[i]->hash.usr = 0;

	processed = 0;
	while (processed < BURST)
		processed += rte_distributor_process(d, &bufs[processed],
			BURST - processed);
	/* at this point, we will have processed some packets and have a full
	 * backlog for the other ones at worker 0.
	 */

	/* get worker zero to quit */
	zero_quit = 1;

	/* flush the distributor */
	rte_distributor_flush(d);

	while (!__atomic_load_n(&zero_sleep, __ATOMIC_ACQUIRE))
		rte_distributor_flush(d);

	zero_quit = 0;

	while (__atomic_load_n(&zero_sleep, __ATOMIC_ACQUIRE))
		rte_delay_us(100);

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
			__atomic_load_n(&worker_stats[i].handled_packets,
					__ATOMIC_RELAXED));

	if (total_packet_count() != BURST) {
		printf("Line %d: Error, not all packets flushed. "
				"Expected %u, got %u\n",
				__LINE__, BURST, total_packet_count());
		failed = 1;
	}

	rte_mempool_put_bulk(p, (void *)bufs, BURST);

	if (failed)
		return -1;

	printf("Flush test with worker shutdown passed\n\n");
	return 0;
}

static int
handle_and_mark_work(void *arg)
{
	struct rte_mbuf *buf[8] __rte_cache_aligned;
	struct worker_params *wp = arg;
	struct rte_distributor *db = wp->dist;
	unsigned int num, i;
	unsigned int id = __atomic_fetch_add(&worker_idx, 1, __ATOMIC_RELAXED);
	num = rte_distributor_get_pkt(db, id, buf, NULL, 0);
	while (!quit) {
		__atomic_fetch_add(&worker_stats[id].handled_packets, num,
				__ATOMIC_RELAXED);
		for (i = 0; i < num; i++)
			*seq_field(buf[i]) += id + 1;
		num = rte_distributor_get_pkt(db, id,
				buf, buf, num);
	}
	__atomic_fetch_add(&worker_stats[id].handled_packets, num,
			__ATOMIC_RELAXED);
	rte_distributor_return_pkt(db, id, buf, num);
	return 0;
}

/* sanity_mark_test sends packets to workers which mark them.
 * Every packet has also encoded sequence number.
 * The returned packets are sorted and verified if they were handled
 * by proper workers.
 */
static int
sanity_mark_test(struct worker_params *wp, struct rte_mempool *p)
{
	const unsigned int buf_count = 24;
	const unsigned int burst = 8;
	const unsigned int shift = 12;
	const unsigned int seq_shift = 10;

	struct rte_distributor *db = wp->dist;
	struct rte_mbuf *bufs[buf_count];
	struct rte_mbuf *returns[buf_count];
	unsigned int i, count, id;
	unsigned int sorted[buf_count], seq;
	unsigned int failed = 0;
	unsigned int processed;

	printf("=== Marked packets test ===\n");
	clear_packet_count();
	if (rte_mempool_get_bulk(p, (void *)bufs, buf_count) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return -1;
	}

	/* bufs' hashes will be like these below, but shifted left.
	 * The shifting is for avoiding collisions with backlogs
	 * and in-flight tags left by previous tests.
	 * [1, 1, 1, 1, 1, 1, 1, 1
	 *  1, 1, 1, 1, 2, 2, 2, 2
	 *  2, 2, 2, 2, 1, 1, 1, 1]
	 */
	for (i = 0; i < burst; i++) {
		bufs[0 * burst + i]->hash.usr = 1 << shift;
		bufs[1 * burst + i]->hash.usr = ((i < burst / 2) ? 1 : 2)
			<< shift;
		bufs[2 * burst + i]->hash.usr = ((i < burst / 2) ? 2 : 1)
			<< shift;
	}
	/* Assign a sequence number to each packet. The sequence is shifted,
	 * so that lower bits will hold mark from worker.
	 */
	for (i = 0; i < buf_count; i++)
		*seq_field(bufs[i]) = i << seq_shift;

	count = 0;
	for (i = 0; i < buf_count/burst; i++) {
		processed = 0;
		while (processed < burst)
			processed += rte_distributor_process(db,
				&bufs[i * burst + processed],
				burst - processed);
		count += rte_distributor_returned_pkts(db, &returns[count],
			buf_count - count);
	}

	do {
		rte_distributor_flush(db);
		count += rte_distributor_returned_pkts(db, &returns[count],
			buf_count - count);
	} while (count < buf_count);

	for (i = 0; i < rte_lcore_count() - 1; i++)
		printf("Worker %u handled %u packets\n", i,
			__atomic_load_n(&worker_stats[i].handled_packets,
					__ATOMIC_RELAXED));

	/* Sort returned packets by sent order (sequence numbers). */
	for (i = 0; i < buf_count; i++) {
		seq = *seq_field(returns[i]) >> seq_shift;
		id = *seq_field(returns[i]) - (seq << seq_shift);
		sorted[seq] = id;
	}

	/* Verify that packets [0-11] and [20-23] were processed
	 * by the same worker
	 */
	for (i = 1; i < 12; i++) {
		if (sorted[i] != sorted[0]) {
			printf("Packet number %u processed by worker %u,"
				" but should be processes by worker %u\n",
				i, sorted[i], sorted[0]);
			failed = 1;
		}
	}
	for (i = 20; i < 24; i++) {
		if (sorted[i] != sorted[0]) {
			printf("Packet number %u processed by worker %u,"
				" but should be processes by worker %u\n",
				i, sorted[i], sorted[0]);
			failed = 1;
		}
	}
	/* And verify that packets [12-19] were processed
	 * by the another worker
	 */
	for (i = 13; i < 20; i++) {
		if (sorted[i] != sorted[12]) {
			printf("Packet number %u processed by worker %u,"
				" but should be processes by worker %u\n",
				i, sorted[i], sorted[12]);
			failed = 1;
		}
	}

	rte_mempool_put_bulk(p, (void *)bufs, buf_count);

	if (failed)
		return -1;

	printf("Marked packets test passed\n");
	return 0;
}

static
int test_error_distributor_create_name(void)
{
	struct rte_distributor *d = NULL;
	struct rte_distributor *db = NULL;
	char *name = NULL;

	d = rte_distributor_create(name, rte_socket_id(),
			rte_lcore_count() - 1,
			RTE_DIST_ALG_SINGLE);
	if (d != NULL || rte_errno != EINVAL) {
		printf("ERROR: No error on create() with NULL name param\n");
		return -1;
	}

	db = rte_distributor_create(name, rte_socket_id(),
			rte_lcore_count() - 1,
			RTE_DIST_ALG_BURST);
	if (db != NULL || rte_errno != EINVAL) {
		printf("ERROR: No error on create() with NULL param\n");
		return -1;
	}

	return 0;
}


static
int test_error_distributor_create_numworkers(void)
{
	struct rte_distributor *ds = NULL;
	struct rte_distributor *db = NULL;

	ds = rte_distributor_create("test_numworkers", rte_socket_id(),
			RTE_MAX_LCORE + 10,
			RTE_DIST_ALG_SINGLE);
	if (ds != NULL || rte_errno != EINVAL) {
		printf("ERROR: No error on create() with num_workers > MAX\n");
		return -1;
	}

	db = rte_distributor_create("test_numworkers", rte_socket_id(),
			RTE_MAX_LCORE + 10,
			RTE_DIST_ALG_BURST);
	if (db != NULL || rte_errno != EINVAL) {
		printf("ERROR: No error on create() num_workers > MAX\n");
		return -1;
	}

	return 0;
}


/* Useful function which ensures that all worker functions terminate */
static void
quit_workers(struct worker_params *wp, struct rte_mempool *p)
{
	struct rte_distributor *d = wp->dist;
	const unsigned num_workers = rte_lcore_count() - 1;
	unsigned i;
	struct rte_mbuf *bufs[RTE_MAX_LCORE];
	struct rte_mbuf *returns[RTE_MAX_LCORE];
	if (rte_mempool_get_bulk(p, (void *)bufs, num_workers) != 0) {
		printf("line %d: Error getting mbufs from pool\n", __LINE__);
		return;
	}

	zero_quit = 0;
	quit = 1;
	for (i = 0; i < num_workers; i++) {
		bufs[i]->hash.usr = i << 1;
		rte_distributor_process(d, &bufs[i], 1);
	}

	rte_distributor_process(d, NULL, 0);
	rte_distributor_flush(d);
	rte_eal_mp_wait_lcore();

	while (rte_distributor_returned_pkts(d, returns, RTE_MAX_LCORE))
		;

	rte_distributor_clear_returns(d);
	rte_mempool_put_bulk(p, (void *)bufs, num_workers);

	quit = 0;
	worker_idx = 0;
	zero_idx = RTE_MAX_LCORE;
	zero_quit = 0;
	zero_sleep = 0;
}

static int
test_distributor(void)
{
	static struct rte_distributor *ds;
	static struct rte_distributor *db;
	static struct rte_distributor *dist[2];
	static struct rte_mempool *p;
	int i;

	static const struct rte_mbuf_dynfield seq_dynfield_desc = {
		.name = "test_distributor_dynfield_seq",
		.size = sizeof(seq_dynfield_t),
		.align = __alignof__(seq_dynfield_t),
	};
	seq_dynfield_offset =
		rte_mbuf_dynfield_register(&seq_dynfield_desc);
	if (seq_dynfield_offset < 0) {
		printf("Error registering mbuf field\n");
		return TEST_FAILED;
	}

	if (rte_lcore_count() < 2) {
		printf("Not enough cores for distributor_autotest, expecting at least 2\n");
		return TEST_SKIPPED;
	}

	if (db == NULL) {
		db = rte_distributor_create("Test_dist_burst", rte_socket_id(),
				rte_lcore_count() - 1,
				RTE_DIST_ALG_BURST);
		if (db == NULL) {
			printf("Error creating burst distributor\n");
			return -1;
		}
	} else {
		rte_distributor_flush(db);
		rte_distributor_clear_returns(db);
	}

	if (ds == NULL) {
		ds = rte_distributor_create("Test_dist_single",
				rte_socket_id(),
				rte_lcore_count() - 1,
			RTE_DIST_ALG_SINGLE);
		if (ds == NULL) {
			printf("Error creating single distributor\n");
			return -1;
		}
	} else {
		rte_distributor_flush(ds);
		rte_distributor_clear_returns(ds);
	}

	const unsigned nb_bufs = (511 * rte_lcore_count()) < BIG_BATCH ?
			(BIG_BATCH * 2) - 1 : (511 * rte_lcore_count());
	if (p == NULL) {
		p = rte_pktmbuf_pool_create("DT_MBUF_POOL", nb_bufs, BURST,
			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
		if (p == NULL) {
			printf("Error creating mempool\n");
			return -1;
		}
	}

	dist[0] = ds;
	dist[1] = db;

	for (i = 0; i < 2; i++) {

		worker_params.dist = dist[i];
		if (i)
			strlcpy(worker_params.name, "burst",
					sizeof(worker_params.name));
		else
			strlcpy(worker_params.name, "single",
					sizeof(worker_params.name));

		rte_eal_mp_remote_launch(handle_work,
				&worker_params, SKIP_MAIN);
		if (sanity_test(&worker_params, p) < 0)
			goto err;
		quit_workers(&worker_params, p);

		rte_eal_mp_remote_launch(handle_work_with_free_mbufs,
				&worker_params, SKIP_MAIN);
		if (sanity_test_with_mbuf_alloc(&worker_params, p) < 0)
			goto err;
		quit_workers(&worker_params, p);

		if (rte_lcore_count() > 2) {
			rte_eal_mp_remote_launch(handle_work_for_shutdown_test,
					&worker_params,
					SKIP_MAIN);
			if (sanity_test_with_worker_shutdown(&worker_params,
					p) < 0)
				goto err;
			quit_workers(&worker_params, p);

			rte_eal_mp_remote_launch(handle_work_for_shutdown_test,
					&worker_params,
					SKIP_MAIN);
			if (test_flush_with_worker_shutdown(&worker_params,
					p) < 0)
				goto err;
			quit_workers(&worker_params, p);

			rte_eal_mp_remote_launch(handle_and_mark_work,
					&worker_params, SKIP_MAIN);
			if (sanity_mark_test(&worker_params, p) < 0)
				goto err;
			quit_workers(&worker_params, p);

		} else {
			printf("Too few cores to run worker shutdown test\n");
		}

	}

	if (test_error_distributor_create_numworkers() == -1 ||
			test_error_distributor_create_name() == -1) {
		printf("rte_distributor_create parameter check tests failed");
		return -1;
	}

	return 0;

err:
	quit_workers(&worker_params, p);
	return -1;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(distributor_autotest, test_distributor);
