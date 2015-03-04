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

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_random.h>
#include <rte_cycles.h>

#include "test.h"

#define MBUF_SIZE               2048
#define NB_MBUF                 128
#define MBUF_TEST_DATA_LEN      1464
#define MBUF_TEST_DATA_LEN2     50
#define MBUF_TEST_HDR1_LEN      20
#define MBUF_TEST_HDR2_LEN      30
#define MBUF_TEST_ALL_HDRS_LEN  (MBUF_TEST_HDR1_LEN+MBUF_TEST_HDR2_LEN)

#define REFCNT_MAX_ITER         64
#define REFCNT_MAX_TIMEOUT      10
#define REFCNT_MAX_REF          (RTE_MAX_LCORE)
#define REFCNT_MBUF_NUM         64
#define REFCNT_MBUF_SIZE        (sizeof (struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define REFCNT_RING_SIZE        (REFCNT_MBUF_NUM * REFCNT_MAX_REF)

#define MAKE_STRING(x)          # x

static struct rte_mempool *pktmbuf_pool = NULL;

#ifdef RTE_MBUF_REFCNT_ATOMIC

static struct rte_mempool *refcnt_pool = NULL;
static struct rte_ring *refcnt_mbuf_ring = NULL;
static volatile uint32_t refcnt_stop_slaves;
static unsigned refcnt_lcore[RTE_MAX_LCORE];

#endif

/*
 * MBUF
 * ====
 *
 * #. Allocate a mbuf pool.
 *
 *    - The pool contains NB_MBUF elements, where each mbuf is MBUF_SIZE
 *      bytes long.
 *
 * #. Test multiple allocations of mbufs from this pool.
 *
 *    - Allocate NB_MBUF and store pointers in a table.
 *    - If an allocation fails, return an error.
 *    - Free all these mbufs.
 *    - Repeat the same test to check that mbufs were freed correctly.
 *
 * #. Test data manipulation in pktmbuf.
 *
 *    - Alloc an mbuf.
 *    - Append data using rte_pktmbuf_append().
 *    - Test for error in rte_pktmbuf_append() when len is too large.
 *    - Trim data at the end of mbuf using rte_pktmbuf_trim().
 *    - Test for error in rte_pktmbuf_trim() when len is too large.
 *    - Prepend a header using rte_pktmbuf_prepend().
 *    - Test for error in rte_pktmbuf_prepend() when len is too large.
 *    - Remove data at the beginning of mbuf using rte_pktmbuf_adj().
 *    - Test for error in rte_pktmbuf_adj() when len is too large.
 *    - Check that appended data is not corrupt.
 *    - Free the mbuf.
 *    - Between all these tests, check data_len and pkt_len, and
 *      that the mbuf is contiguous.
 *    - Repeat the test to check that allocation operations
 *      reinitialize the mbuf correctly.
 *
 */

#define GOTO_FAIL(str, ...) do {					\
		printf("mbuf test FAILED (l.%d): <" str ">\n",		\
		       __LINE__,  ##__VA_ARGS__);			\
		goto fail;						\
} while(0)

/*
 * test data manipulation in mbuf with non-ascii data
 */
static int
test_pktmbuf_with_non_ascii_data(void)
{
	struct rte_mbuf *m = NULL;
	char *data;

	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("Cannot allocate mbuf");
	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("Bad length");

	data = rte_pktmbuf_append(m, MBUF_TEST_DATA_LEN);
	if (data == NULL)
		GOTO_FAIL("Cannot append data");
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad pkt length");
	if (rte_pktmbuf_data_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad data length");
	memset(data, 0xff, rte_pktmbuf_pkt_len(m));
	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");
	rte_pktmbuf_dump(stdout, m, MBUF_TEST_DATA_LEN);

	rte_pktmbuf_free(m);

	return 0;

fail:
	if(m) {
		rte_pktmbuf_free(m);
	}
	return -1;
}

/*
 * test data manipulation in mbuf
 */
static int
test_one_pktmbuf(void)
{
	struct rte_mbuf *m = NULL;
	char *data, *data2, *hdr;
	unsigned i;

	printf("Test pktmbuf API\n");

	/* alloc a mbuf */

	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("Cannot allocate mbuf");
	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("Bad length");

	rte_pktmbuf_dump(stdout, m, 0);

	/* append data */

	data = rte_pktmbuf_append(m, MBUF_TEST_DATA_LEN);
	if (data == NULL)
		GOTO_FAIL("Cannot append data");
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad pkt length");
	if (rte_pktmbuf_data_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad data length");
	memset(data, 0x66, rte_pktmbuf_pkt_len(m));
	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");
	rte_pktmbuf_dump(stdout, m, MBUF_TEST_DATA_LEN);
	rte_pktmbuf_dump(stdout, m, 2*MBUF_TEST_DATA_LEN);

	/* this append should fail */

	data2 = rte_pktmbuf_append(m, (uint16_t)(rte_pktmbuf_tailroom(m) + 1));
	if (data2 != NULL)
		GOTO_FAIL("Append should not succeed");

	/* append some more data */

	data2 = rte_pktmbuf_append(m, MBUF_TEST_DATA_LEN2);
	if (data2 == NULL)
		GOTO_FAIL("Cannot append data");
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN + MBUF_TEST_DATA_LEN2)
		GOTO_FAIL("Bad pkt length");
	if (rte_pktmbuf_data_len(m) != MBUF_TEST_DATA_LEN + MBUF_TEST_DATA_LEN2)
		GOTO_FAIL("Bad data length");
	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");

	/* trim data at the end of mbuf */

	if (rte_pktmbuf_trim(m, MBUF_TEST_DATA_LEN2) < 0)
		GOTO_FAIL("Cannot trim data");
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad pkt length");
	if (rte_pktmbuf_data_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad data length");
	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");

	/* this trim should fail */

	if (rte_pktmbuf_trim(m, (uint16_t)(rte_pktmbuf_data_len(m) + 1)) == 0)
		GOTO_FAIL("trim should not succeed");

	/* prepend one header */

	hdr = rte_pktmbuf_prepend(m, MBUF_TEST_HDR1_LEN);
	if (hdr == NULL)
		GOTO_FAIL("Cannot prepend");
	if (data - hdr != MBUF_TEST_HDR1_LEN)
		GOTO_FAIL("Prepend failed");
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN + MBUF_TEST_HDR1_LEN)
		GOTO_FAIL("Bad pkt length");
	if (rte_pktmbuf_data_len(m) != MBUF_TEST_DATA_LEN + MBUF_TEST_HDR1_LEN)
		GOTO_FAIL("Bad data length");
	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");
	memset(hdr, 0x55, MBUF_TEST_HDR1_LEN);

	/* prepend another header */

	hdr = rte_pktmbuf_prepend(m, MBUF_TEST_HDR2_LEN);
	if (hdr == NULL)
		GOTO_FAIL("Cannot prepend");
	if (data - hdr != MBUF_TEST_ALL_HDRS_LEN)
		GOTO_FAIL("Prepend failed");
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN + MBUF_TEST_ALL_HDRS_LEN)
		GOTO_FAIL("Bad pkt length");
	if (rte_pktmbuf_data_len(m) != MBUF_TEST_DATA_LEN + MBUF_TEST_ALL_HDRS_LEN)
		GOTO_FAIL("Bad data length");
	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");
	memset(hdr, 0x55, MBUF_TEST_HDR2_LEN);

	rte_mbuf_sanity_check(m, 1);
	rte_mbuf_sanity_check(m, 0);
	rte_pktmbuf_dump(stdout, m, 0);

	/* this prepend should fail */

	hdr = rte_pktmbuf_prepend(m, (uint16_t)(rte_pktmbuf_headroom(m) + 1));
	if (hdr != NULL)
		GOTO_FAIL("prepend should not succeed");

	/* remove data at beginning of mbuf (adj) */

	if (data != rte_pktmbuf_adj(m, MBUF_TEST_ALL_HDRS_LEN))
		GOTO_FAIL("rte_pktmbuf_adj failed");
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad pkt length");
	if (rte_pktmbuf_data_len(m) != MBUF_TEST_DATA_LEN)
		GOTO_FAIL("Bad data length");
	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");

	/* this adj should fail */

	if (rte_pktmbuf_adj(m, (uint16_t)(rte_pktmbuf_data_len(m) + 1)) != NULL)
		GOTO_FAIL("rte_pktmbuf_adj should not succeed");

	/* check data */

	if (!rte_pktmbuf_is_contiguous(m))
		GOTO_FAIL("Buffer should be continuous");

	for (i=0; i<MBUF_TEST_DATA_LEN; i++) {
		if (data[i] != 0x66)
			GOTO_FAIL("Data corrupted at offset %u", i);
	}

	/* free mbuf */

	rte_pktmbuf_free(m);
	m = NULL;
	return 0;

fail:
	if (m)
		rte_pktmbuf_free(m);
	return -1;
}

static int
testclone_testupdate_testdetach(void)
{
	struct rte_mbuf *mc = NULL;
	struct rte_mbuf *clone = NULL;

	/* alloc a mbuf */

	mc = rte_pktmbuf_alloc(pktmbuf_pool);
	if (mc == NULL)
		GOTO_FAIL("ooops not allocating mbuf");

	if (rte_pktmbuf_pkt_len(mc) != 0)
		GOTO_FAIL("Bad length");


	/* clone the allocated mbuf */
	clone = rte_pktmbuf_clone(mc, pktmbuf_pool);
	if (clone == NULL)
		GOTO_FAIL("cannot clone data\n");
	rte_pktmbuf_free(clone);

	mc->next = rte_pktmbuf_alloc(pktmbuf_pool);
	if(mc->next == NULL)
		GOTO_FAIL("Next Pkt Null\n");

	clone = rte_pktmbuf_clone(mc, pktmbuf_pool);
	if (clone == NULL)
		GOTO_FAIL("cannot clone data\n");

	/* free mbuf */
	rte_pktmbuf_free(mc);
	rte_pktmbuf_free(clone);
	mc = NULL;
	clone = NULL;
	return 0;

fail:
	if (mc)
		rte_pktmbuf_free(mc);
	return -1;
}
#undef GOTO_FAIL



/*
 * test allocation and free of mbufs
 */
static int
test_pktmbuf_pool(void)
{
	unsigned i;
	struct rte_mbuf *m[NB_MBUF];
	int ret = 0;

	for (i=0; i<NB_MBUF; i++)
		m[i] = NULL;

	/* alloc NB_MBUF mbufs */
	for (i=0; i<NB_MBUF; i++) {
		m[i] = rte_pktmbuf_alloc(pktmbuf_pool);
		if (m[i] == NULL) {
			printf("rte_pktmbuf_alloc() failed (%u)\n", i);
			ret = -1;
		}
	}
	struct rte_mbuf *extra = NULL;
	extra = rte_pktmbuf_alloc(pktmbuf_pool);
	if(extra != NULL) {
		printf("Error pool not empty");
		ret = -1;
	}
	extra = rte_pktmbuf_clone(m[0], pktmbuf_pool);
	if(extra != NULL) {
		printf("Error pool not empty");
		ret = -1;
	}
	/* free them */
	for (i=0; i<NB_MBUF; i++) {
		if (m[i] != NULL)
			rte_pktmbuf_free(m[i]);
	}

	return ret;
}

/*
 * test that the pointer to the data on a packet mbuf is set properly
 */
static int
test_pktmbuf_pool_ptr(void)
{
	unsigned i;
	struct rte_mbuf *m[NB_MBUF];
	int ret = 0;

	for (i=0; i<NB_MBUF; i++)
		m[i] = NULL;

	/* alloc NB_MBUF mbufs */
	for (i=0; i<NB_MBUF; i++) {
		m[i] = rte_pktmbuf_alloc(pktmbuf_pool);
		if (m[i] == NULL) {
			printf("rte_pktmbuf_alloc() failed (%u)\n", i);
			ret = -1;
		}
		m[i]->data_off += 64;
	}

	/* free them */
	for (i=0; i<NB_MBUF; i++) {
		if (m[i] != NULL)
			rte_pktmbuf_free(m[i]);
	}

	for (i=0; i<NB_MBUF; i++)
		m[i] = NULL;

	/* alloc NB_MBUF mbufs */
	for (i=0; i<NB_MBUF; i++) {
		m[i] = rte_pktmbuf_alloc(pktmbuf_pool);
		if (m[i] == NULL) {
			printf("rte_pktmbuf_alloc() failed (%u)\n", i);
			ret = -1;
		}
		if (m[i]->data_off != RTE_PKTMBUF_HEADROOM) {
			printf("invalid data_off\n");
			ret = -1;
		}
	}

	/* free them */
	for (i=0; i<NB_MBUF; i++) {
		if (m[i] != NULL)
			rte_pktmbuf_free(m[i]);
	}

	return ret;
}

static int
test_pktmbuf_free_segment(void)
{
	unsigned i;
	struct rte_mbuf *m[NB_MBUF];
	int ret = 0;

	for (i=0; i<NB_MBUF; i++)
		m[i] = NULL;

	/* alloc NB_MBUF mbufs */
	for (i=0; i<NB_MBUF; i++) {
		m[i] = rte_pktmbuf_alloc(pktmbuf_pool);
		if (m[i] == NULL) {
			printf("rte_pktmbuf_alloc() failed (%u)\n", i);
			ret = -1;
		}
	}

	/* free them */
	for (i=0; i<NB_MBUF; i++) {
		if (m[i] != NULL) {
			struct rte_mbuf *mb, *mt;

			mb = m[i];
			while(mb != NULL) {
				mt = mb;
				mb = mb->next;
				rte_pktmbuf_free_seg(mt);
			}
		}
	}

	return ret;
}

/*
 * Stress test for rte_mbuf atomic refcnt.
 * Implies that RTE_MBUF_REFCNT_ATOMIC is defined.
 * For more efficency, recomended to run with RTE_LIBRTE_MBUF_DEBUG defined.
 */

#ifdef RTE_MBUF_REFCNT_ATOMIC

static int
test_refcnt_slave(__attribute__((unused)) void *arg)
{
	unsigned lcore, free;
	void *mp = 0;

	lcore = rte_lcore_id();
	printf("%s started at lcore %u\n", __func__, lcore);

	free = 0;
	while (refcnt_stop_slaves == 0) {
		if (rte_ring_dequeue(refcnt_mbuf_ring, &mp) == 0) {
			free++;
			rte_pktmbuf_free((struct rte_mbuf *)mp);
		}
	}

	refcnt_lcore[lcore] += free;
	printf("%s finished at lcore %u, "
	       "number of freed mbufs: %u\n",
	       __func__, lcore, free);
	return (0);
}

static void
test_refcnt_iter(unsigned lcore, unsigned iter)
{
	uint16_t ref;
	unsigned i, n, tref, wn;
	struct rte_mbuf *m;

	tref = 0;

	/* For each mbuf in the pool:
	 * - allocate mbuf,
	 * - increment it's reference up to N+1,
	 * - enqueue it N times into the ring for slave cores to free.
	 */
	for (i = 0, n = rte_mempool_count(refcnt_pool);
	    i != n && (m = rte_pktmbuf_alloc(refcnt_pool)) != NULL;
	    i++) {
		ref = RTE_MAX(rte_rand() % REFCNT_MAX_REF, 1UL);
		tref += ref;
		if ((ref & 1) != 0) {
			rte_pktmbuf_refcnt_update(m, ref);
			while (ref-- != 0)
				rte_ring_enqueue(refcnt_mbuf_ring, m);
		} else {
			while (ref-- != 0) {
				rte_pktmbuf_refcnt_update(m, 1);
				rte_ring_enqueue(refcnt_mbuf_ring, m);
			}
		}
		rte_pktmbuf_free(m);
	}

	if (i != n)
		rte_panic("(lcore=%u, iter=%u): was able to allocate only "
		          "%u from %u mbufs\n", lcore, iter, i, n);

	/* wait till slave lcores  will consume all mbufs */
	while (!rte_ring_empty(refcnt_mbuf_ring))
		;

	/* check that all mbufs are back into mempool by now */
	for (wn = 0; wn != REFCNT_MAX_TIMEOUT; wn++) {
		if ((i = rte_mempool_count(refcnt_pool)) == n) {
			refcnt_lcore[lcore] += tref;
			printf("%s(lcore=%u, iter=%u) completed, "
			    "%u references processed\n",
			    __func__, lcore, iter, tref);
			return;
		}
		rte_delay_ms(1000);
	}

	rte_panic("(lcore=%u, iter=%u): after %us only "
	          "%u of %u mbufs left free\n", lcore, iter, wn, i, n);
}

static int
test_refcnt_master(void)
{
	unsigned i, lcore;

	lcore = rte_lcore_id();
	printf("%s started at lcore %u\n", __func__, lcore);

	for (i = 0; i != REFCNT_MAX_ITER; i++)
		test_refcnt_iter(lcore, i);

	refcnt_stop_slaves = 1;
	rte_wmb();

	printf("%s finished at lcore %u\n", __func__, lcore);
	return (0);
}

#endif

static int
test_refcnt_mbuf(void)
{
#ifdef RTE_MBUF_REFCNT_ATOMIC

	unsigned lnum, master, slave, tref;


	if ((lnum = rte_lcore_count()) == 1) {
		printf("skipping %s, number of lcores: %u is not enough\n",
		    __func__, lnum);
		return (0);
	}

	printf("starting %s, at %u lcores\n", __func__, lnum);

	/* create refcnt pool & ring if they don't exist */

	if (refcnt_pool == NULL &&
			(refcnt_pool = rte_mempool_create(
			MAKE_STRING(refcnt_pool),
			REFCNT_MBUF_NUM, REFCNT_MBUF_SIZE, 0,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,
			SOCKET_ID_ANY, 0)) == NULL) {
		printf("%s: cannot allocate " MAKE_STRING(refcnt_pool) "\n",
		    __func__);
		return (-1);
	}

	if (refcnt_mbuf_ring == NULL &&
			(refcnt_mbuf_ring = rte_ring_create("refcnt_mbuf_ring",
			REFCNT_RING_SIZE, SOCKET_ID_ANY,
			RING_F_SP_ENQ)) == NULL) {
		printf("%s: cannot allocate " MAKE_STRING(refcnt_mbuf_ring)
		    "\n", __func__);
		return (-1);
	}

	refcnt_stop_slaves = 0;
	memset(refcnt_lcore, 0, sizeof (refcnt_lcore));

	rte_eal_mp_remote_launch(test_refcnt_slave, NULL, SKIP_MASTER);

	test_refcnt_master();

	rte_eal_mp_wait_lcore();

	/* check that we porcessed all references */
	tref = 0;
	master = rte_get_master_lcore();

	RTE_LCORE_FOREACH_SLAVE(slave)
		tref += refcnt_lcore[slave];

	if (tref != refcnt_lcore[master])
		rte_panic("refernced mbufs: %u, freed mbufs: %u\n",
		          tref, refcnt_lcore[master]);

	rte_mempool_dump(stdout, refcnt_pool);
	rte_ring_dump(stdout, refcnt_mbuf_ring);

#endif
	return (0);
}

#include <unistd.h>
#include <sys/wait.h>

/* use fork() to test mbuf errors panic */
static int
verify_mbuf_check_panics(struct rte_mbuf *buf)
{
	int pid;
	int status;

	pid = fork();

	if (pid == 0) {
		rte_mbuf_sanity_check(buf, 1); /* should panic */
		exit(0);  /* return normally if it doesn't panic */
	} else if (pid < 0){
		printf("Fork Failed\n");
		return -1;
	}
	wait(&status);
	if(status == 0)
		return -1;

	return 0;
}

static int
test_failing_mbuf_sanity_check(void)
{
	struct rte_mbuf *buf;
	struct rte_mbuf badbuf;

	printf("Checking rte_mbuf_sanity_check for failure conditions\n");

	/* get a good mbuf to use to make copies */
	buf = rte_pktmbuf_alloc(pktmbuf_pool);
	if (buf == NULL)
		return -1;
	printf("Checking good mbuf initially\n");
	if (verify_mbuf_check_panics(buf) != -1)
		return -1;

	printf("Now checking for error conditions\n");

	if (verify_mbuf_check_panics(NULL)) {
		printf("Error with NULL mbuf test\n");
		return -1;
	}

	badbuf = *buf;
	badbuf.pool = NULL;
	if (verify_mbuf_check_panics(&badbuf)) {
		printf("Error with bad-pool mbuf test\n");
		return -1;
	}

	badbuf = *buf;
	badbuf.buf_physaddr = 0;
	if (verify_mbuf_check_panics(&badbuf)) {
		printf("Error with bad-physaddr mbuf test\n");
		return -1;
	}

	badbuf = *buf;
	badbuf.buf_addr = NULL;
	if (verify_mbuf_check_panics(&badbuf)) {
		printf("Error with bad-addr mbuf test\n");
		return -1;
	}

	badbuf = *buf;
	badbuf.refcnt = 0;
	if (verify_mbuf_check_panics(&badbuf)) {
		printf("Error with bad-refcnt(0) mbuf test\n");
		return -1;
	}

	badbuf = *buf;
	badbuf.refcnt = UINT16_MAX;
	if (verify_mbuf_check_panics(&badbuf)) {
		printf("Error with bad-refcnt(MAX) mbuf test\n");
		return -1;
	}

	return 0;
}


static int
test_mbuf(void)
{
	RTE_BUILD_BUG_ON(sizeof(struct rte_mbuf) != RTE_CACHE_LINE_SIZE * 2);

	/* create pktmbuf pool if it does not exist */
	if (pktmbuf_pool == NULL) {
		pktmbuf_pool =
			rte_mempool_create("test_pktmbuf_pool", NB_MBUF,
					   MBUF_SIZE, 32,
					   sizeof(struct rte_pktmbuf_pool_private),
					   rte_pktmbuf_pool_init, NULL,
					   rte_pktmbuf_init, NULL,
					   SOCKET_ID_ANY, 0);
	}

	if (pktmbuf_pool == NULL) {
		printf("cannot allocate mbuf pool\n");
		return -1;
	}

	/* test multiple mbuf alloc */
	if (test_pktmbuf_pool() < 0) {
		printf("test_mbuf_pool() failed\n");
		return -1;
	}

	/* do it another time to check that all mbufs were freed */
	if (test_pktmbuf_pool() < 0) {
		printf("test_mbuf_pool() failed (2)\n");
		return -1;
	}

	/* test that the pointer to the data on a packet mbuf is set properly */
	if (test_pktmbuf_pool_ptr() < 0) {
		printf("test_pktmbuf_pool_ptr() failed\n");
		return -1;
	}

	/* test data manipulation in mbuf */
	if (test_one_pktmbuf() < 0) {
		printf("test_one_mbuf() failed\n");
		return -1;
	}


	/*
	 * do it another time, to check that allocation reinitialize
	 * the mbuf correctly
	 */
	if (test_one_pktmbuf() < 0) {
		printf("test_one_mbuf() failed (2)\n");
		return -1;
	}

	if (test_pktmbuf_with_non_ascii_data() < 0) {
		printf("test_pktmbuf_with_non_ascii_data() failed\n");
		return -1;
	}

	/* test free pktmbuf segment one by one */
	if (test_pktmbuf_free_segment() < 0) {
		printf("test_pktmbuf_free_segment() failed.\n");
		return -1;
	}

	if (testclone_testupdate_testdetach()<0){
		printf("testclone_and_testupdate() failed \n");
		return -1;
	}

	if (test_refcnt_mbuf()<0){
		printf("test_refcnt_mbuf() failed \n");
		return -1;
	}

	if (test_failing_mbuf_sanity_check() < 0) {
		printf("test_failing_mbuf_sanity_check() failed\n");
		return -1;
	}
	return 0;
}

static struct test_command mbuf_cmd = {
	.command = "mbuf_autotest",
	.callback = test_mbuf,
};
REGISTER_TEST_COMMAND(mbuf_cmd);
