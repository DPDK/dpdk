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

#define MBUF_DATA_SIZE          2048
#define NB_MBUF                 128
#define MBUF_TEST_DATA_LEN      1464
#define MBUF_TEST_DATA_LEN2     50
#define MBUF_TEST_HDR1_LEN      20
#define MBUF_TEST_HDR2_LEN      30
#define MBUF_TEST_ALL_HDRS_LEN  (MBUF_TEST_HDR1_LEN+MBUF_TEST_HDR2_LEN)

/* size of private data for mbuf in pktmbuf_pool2 */
#define MBUF2_PRIV_SIZE         128

#define REFCNT_MAX_ITER         64
#define REFCNT_MAX_TIMEOUT      10
#define REFCNT_MAX_REF          (RTE_MAX_LCORE)
#define REFCNT_MBUF_NUM         64
#define REFCNT_RING_SIZE        (REFCNT_MBUF_NUM * REFCNT_MAX_REF)

#define MAGIC_DATA              0x42424242

#define MAKE_STRING(x)          # x

#ifdef RTE_MBUF_REFCNT_ATOMIC

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
 * #. Test packet cloning
 *    - Clone a mbuf and verify the data
 *    - Clone the cloned mbuf and verify the data
 *    - Attach a mbuf to another that does not have the same priv_size.
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
test_pktmbuf_with_non_ascii_data(struct rte_mempool *pktmbuf_pool)
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
test_one_pktmbuf(struct rte_mempool *pktmbuf_pool)
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
testclone_testupdate_testdetach(struct rte_mempool *pktmbuf_pool)
{
	struct rte_mbuf *m = NULL;
	struct rte_mbuf *clone = NULL;
	struct rte_mbuf *clone2 = NULL;
	unaligned_uint32_t *data;

	/* alloc a mbuf */
	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("ooops not allocating mbuf");

	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("Bad length");

	rte_pktmbuf_append(m, sizeof(uint32_t));
	data = rte_pktmbuf_mtod(m, unaligned_uint32_t *);
	*data = MAGIC_DATA;

	/* clone the allocated mbuf */
	clone = rte_pktmbuf_clone(m, pktmbuf_pool);
	if (clone == NULL)
		GOTO_FAIL("cannot clone data\n");

	data = rte_pktmbuf_mtod(clone, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone\n");

	if (rte_mbuf_refcnt_read(m) != 2)
		GOTO_FAIL("invalid refcnt in m\n");

	/* free the clone */
	rte_pktmbuf_free(clone);
	clone = NULL;

	/* same test with a chained mbuf */
	m->next = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m->next == NULL)
		GOTO_FAIL("Next Pkt Null\n");

	rte_pktmbuf_append(m->next, sizeof(uint32_t));
	data = rte_pktmbuf_mtod(m->next, unaligned_uint32_t *);
	*data = MAGIC_DATA;

	clone = rte_pktmbuf_clone(m, pktmbuf_pool);
	if (clone == NULL)
		GOTO_FAIL("cannot clone data\n");

	data = rte_pktmbuf_mtod(clone, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone\n");

	data = rte_pktmbuf_mtod(clone->next, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone->next\n");

	if (rte_mbuf_refcnt_read(m) != 2)
		GOTO_FAIL("invalid refcnt in m\n");

	if (rte_mbuf_refcnt_read(m->next) != 2)
		GOTO_FAIL("invalid refcnt in m->next\n");

	/* try to clone the clone */

	clone2 = rte_pktmbuf_clone(clone, pktmbuf_pool);
	if (clone2 == NULL)
		GOTO_FAIL("cannot clone the clone\n");

	data = rte_pktmbuf_mtod(clone2, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone2\n");

	data = rte_pktmbuf_mtod(clone2->next, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone2->next\n");

	if (rte_mbuf_refcnt_read(m) != 3)
		GOTO_FAIL("invalid refcnt in m\n");

	if (rte_mbuf_refcnt_read(m->next) != 3)
		GOTO_FAIL("invalid refcnt in m->next\n");

	/* free mbuf */
	rte_pktmbuf_free(m);
	rte_pktmbuf_free(clone);
	rte_pktmbuf_free(clone2);

	m = NULL;
	clone = NULL;
	clone2 = NULL;
	printf("%s ok\n", __func__);
	return 0;

fail:
	if (m)
		rte_pktmbuf_free(m);
	if (clone)
		rte_pktmbuf_free(clone);
	if (clone2)
		rte_pktmbuf_free(clone2);
	return -1;
}

static int
test_attach_from_different_pool(struct rte_mempool *pktmbuf_pool,
				struct rte_mempool *pktmbuf_pool2)
{
	struct rte_mbuf *m = NULL;
	struct rte_mbuf *clone = NULL;
	struct rte_mbuf *clone2 = NULL;
	char *data, *c_data, *c_data2;

	/* alloc a mbuf */
	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("cannot allocate mbuf");

	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("Bad length");

	data = rte_pktmbuf_mtod(m, char *);

	/* allocate a new mbuf from the second pool, and attach it to the first
	 * mbuf */
	clone = rte_pktmbuf_alloc(pktmbuf_pool2);
	if (clone == NULL)
		GOTO_FAIL("cannot allocate mbuf from second pool\n");

	/* check data room size and priv size, and erase priv */
	if (rte_pktmbuf_data_room_size(clone->pool) != 0)
		GOTO_FAIL("data room size should be 0\n");
	if (rte_pktmbuf_priv_size(clone->pool) != MBUF2_PRIV_SIZE)
		GOTO_FAIL("data room size should be %d\n", MBUF2_PRIV_SIZE);
	memset(clone + 1, 0, MBUF2_PRIV_SIZE);

	/* save data pointer to compare it after detach() */
	c_data = rte_pktmbuf_mtod(clone, char *);
	if (c_data != (char *)clone + sizeof(*clone) + MBUF2_PRIV_SIZE)
		GOTO_FAIL("bad data pointer in clone");
	if (rte_pktmbuf_headroom(clone) != 0)
		GOTO_FAIL("bad headroom in clone");

	rte_pktmbuf_attach(clone, m);

	if (rte_pktmbuf_mtod(clone, char *) != data)
		GOTO_FAIL("clone was not attached properly\n");
	if (rte_pktmbuf_headroom(clone) != RTE_PKTMBUF_HEADROOM)
		GOTO_FAIL("bad headroom in clone after attach");
	if (rte_mbuf_refcnt_read(m) != 2)
		GOTO_FAIL("invalid refcnt in m\n");

	/* allocate a new mbuf from the second pool, and attach it to the first
	 * cloned mbuf */
	clone2 = rte_pktmbuf_alloc(pktmbuf_pool2);
	if (clone2 == NULL)
		GOTO_FAIL("cannot allocate clone2 from second pool\n");

	/* check data room size and priv size, and erase priv */
	if (rte_pktmbuf_data_room_size(clone2->pool) != 0)
		GOTO_FAIL("data room size should be 0\n");
	if (rte_pktmbuf_priv_size(clone2->pool) != MBUF2_PRIV_SIZE)
		GOTO_FAIL("data room size should be %d\n", MBUF2_PRIV_SIZE);
	memset(clone2 + 1, 0, MBUF2_PRIV_SIZE);

	/* save data pointer to compare it after detach() */
	c_data2 = rte_pktmbuf_mtod(clone2, char *);
	if (c_data2 != (char *)clone2 + sizeof(*clone2) + MBUF2_PRIV_SIZE)
		GOTO_FAIL("bad data pointer in clone2");
	if (rte_pktmbuf_headroom(clone2) != 0)
		GOTO_FAIL("bad headroom in clone2");

	rte_pktmbuf_attach(clone2, clone);

	if (rte_pktmbuf_mtod(clone2, char *) != data)
		GOTO_FAIL("clone2 was not attached properly\n");
	if (rte_pktmbuf_headroom(clone2) != RTE_PKTMBUF_HEADROOM)
		GOTO_FAIL("bad headroom in clone2 after attach");
	if (rte_mbuf_refcnt_read(m) != 3)
		GOTO_FAIL("invalid refcnt in m\n");

	/* detach the clones */
	rte_pktmbuf_detach(clone);
	if (c_data != rte_pktmbuf_mtod(clone, char *))
		GOTO_FAIL("clone was not detached properly\n");
	if (rte_mbuf_refcnt_read(m) != 2)
		GOTO_FAIL("invalid refcnt in m\n");

	rte_pktmbuf_detach(clone2);
	if (c_data2 != rte_pktmbuf_mtod(clone2, char *))
		GOTO_FAIL("clone2 was not detached properly\n");
	if (rte_mbuf_refcnt_read(m) != 1)
		GOTO_FAIL("invalid refcnt in m\n");

	/* free the clones and the initial mbuf */
	rte_pktmbuf_free(clone2);
	rte_pktmbuf_free(clone);
	rte_pktmbuf_free(m);
	printf("%s ok\n", __func__);
	return 0;

fail:
	if (m)
		rte_pktmbuf_free(m);
	if (clone)
		rte_pktmbuf_free(clone);
	if (clone2)
		rte_pktmbuf_free(clone2);
	return -1;
}
#undef GOTO_FAIL

/*
 * test allocation and free of mbufs
 */
static int
test_pktmbuf_pool(struct rte_mempool *pktmbuf_pool)
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
test_pktmbuf_pool_ptr(struct rte_mempool *pktmbuf_pool)
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
			break;
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
			break;
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
test_pktmbuf_free_segment(struct rte_mempool *pktmbuf_pool)
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
 * For more efficiency, recommended to run with RTE_LIBRTE_MBUF_DEBUG defined.
 */

#ifdef RTE_MBUF_REFCNT_ATOMIC

static int
test_refcnt_slave(void *arg)
{
	unsigned lcore, free;
	void *mp = 0;
	struct rte_ring *refcnt_mbuf_ring = arg;

	lcore = rte_lcore_id();
	printf("%s started at lcore %u\n", __func__, lcore);

	free = 0;
	while (refcnt_stop_slaves == 0) {
		if (rte_ring_dequeue(refcnt_mbuf_ring, &mp) == 0) {
			free++;
			rte_pktmbuf_free(mp);
		}
	}

	refcnt_lcore[lcore] += free;
	printf("%s finished at lcore %u, "
	       "number of freed mbufs: %u\n",
	       __func__, lcore, free);
	return 0;
}

static void
test_refcnt_iter(unsigned int lcore, unsigned int iter,
		 struct rte_mempool *refcnt_pool,
		 struct rte_ring *refcnt_mbuf_ring)
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
	for (i = 0, n = rte_mempool_avail_count(refcnt_pool);
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
		if ((i = rte_mempool_avail_count(refcnt_pool)) == n) {
			refcnt_lcore[lcore] += tref;
			printf("%s(lcore=%u, iter=%u) completed, "
			    "%u references processed\n",
			    __func__, lcore, iter, tref);
			return;
		}
		rte_delay_ms(100);
	}

	rte_panic("(lcore=%u, iter=%u): after %us only "
	          "%u of %u mbufs left free\n", lcore, iter, wn, i, n);
}

static int
test_refcnt_master(struct rte_mempool *refcnt_pool,
		   struct rte_ring *refcnt_mbuf_ring)
{
	unsigned i, lcore;

	lcore = rte_lcore_id();
	printf("%s started at lcore %u\n", __func__, lcore);

	for (i = 0; i != REFCNT_MAX_ITER; i++)
		test_refcnt_iter(lcore, i, refcnt_pool, refcnt_mbuf_ring);

	refcnt_stop_slaves = 1;
	rte_wmb();

	printf("%s finished at lcore %u\n", __func__, lcore);
	return 0;
}

#endif

static int
test_refcnt_mbuf(void)
{
#ifdef RTE_MBUF_REFCNT_ATOMIC
	unsigned lnum, master, slave, tref;
	int ret = -1;
	struct rte_mempool *refcnt_pool = NULL;
	struct rte_ring *refcnt_mbuf_ring = NULL;

	if ((lnum = rte_lcore_count()) == 1) {
		printf("skipping %s, number of lcores: %u is not enough\n",
		    __func__, lnum);
		return 0;
	}

	printf("starting %s, at %u lcores\n", __func__, lnum);

	/* create refcnt pool & ring if they don't exist */

	refcnt_pool = rte_pktmbuf_pool_create(MAKE_STRING(refcnt_pool),
					      REFCNT_MBUF_NUM, 0, 0, 0,
					      SOCKET_ID_ANY);
	if (refcnt_pool == NULL) {
		printf("%s: cannot allocate " MAKE_STRING(refcnt_pool) "\n",
		    __func__);
		return -1;
	}

	refcnt_mbuf_ring = rte_ring_create("refcnt_mbuf_ring",
			rte_align32pow2(REFCNT_RING_SIZE), SOCKET_ID_ANY,
					RING_F_SP_ENQ);
	if (refcnt_mbuf_ring == NULL) {
		printf("%s: cannot allocate " MAKE_STRING(refcnt_mbuf_ring)
		    "\n", __func__);
		goto err;
	}

	refcnt_stop_slaves = 0;
	memset(refcnt_lcore, 0, sizeof (refcnt_lcore));

	rte_eal_mp_remote_launch(test_refcnt_slave, refcnt_mbuf_ring,
				 SKIP_MASTER);

	test_refcnt_master(refcnt_pool, refcnt_mbuf_ring);

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

	ret = 0;

err:
	rte_mempool_free(refcnt_pool);
	rte_ring_free(refcnt_mbuf_ring);
	return ret;
#else
	return 0;
#endif
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
test_failing_mbuf_sanity_check(struct rte_mempool *pktmbuf_pool)
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
test_mbuf_linearize(struct rte_mempool *pktmbuf_pool, int pkt_len,
		    int nb_segs)
{

	struct rte_mbuf *m = NULL, *mbuf = NULL;
	uint8_t *data;
	int data_len = 0;
	int remain;
	int seg, seg_len;
	int i;

	if (pkt_len < 1) {
		printf("Packet size must be 1 or more (is %d)\n", pkt_len);
		return -1;
	}

	if (nb_segs < 1) {
		printf("Number of segments must be 1 or more (is %d)\n",
				nb_segs);
		return -1;
	}

	seg_len = pkt_len / nb_segs;
	if (seg_len == 0)
		seg_len = 1;

	remain = pkt_len;

	/* Create chained mbuf_src and fill it generated data */
	for (seg = 0; remain > 0; seg++) {

		m = rte_pktmbuf_alloc(pktmbuf_pool);
		if (m == NULL) {
			printf("Cannot create segment for source mbuf");
			goto fail;
		}

		/* Make sure if tailroom is zeroed */
		memset(rte_pktmbuf_mtod(m, uint8_t *), 0,
				rte_pktmbuf_tailroom(m));

		data_len = remain;
		if (data_len > seg_len)
			data_len = seg_len;

		data = (uint8_t *)rte_pktmbuf_append(m, data_len);
		if (data == NULL) {
			printf("Cannot append %d bytes to the mbuf\n",
					data_len);
			goto fail;
		}

		for (i = 0; i < data_len; i++)
			data[i] = (seg * seg_len + i) % 0x0ff;

		if (seg == 0)
			mbuf = m;
		else
			rte_pktmbuf_chain(mbuf, m);

		remain -= data_len;
	}

	/* Create destination buffer to store coalesced data */
	if (rte_pktmbuf_linearize(mbuf)) {
		printf("Mbuf linearization failed\n");
		goto fail;
	}

	if (!rte_pktmbuf_is_contiguous(mbuf)) {
		printf("Source buffer should be contiguous after "
				"linearization\n");
		goto fail;
	}

	data = rte_pktmbuf_mtod(mbuf, uint8_t *);

	for (i = 0; i < pkt_len; i++)
		if (data[i] != (i % 0x0ff)) {
			printf("Incorrect data in linearized mbuf\n");
			goto fail;
		}

	rte_pktmbuf_free(mbuf);
	return 0;

fail:
	if (mbuf)
		rte_pktmbuf_free(mbuf);
	return -1;
}

static int
test_mbuf_linearize_check(struct rte_mempool *pktmbuf_pool)
{
	struct test_mbuf_array {
		int size;
		int nb_segs;
	} mbuf_array[] = {
			{ 128, 1 },
			{ 64, 64 },
			{ 512, 10 },
			{ 250, 11 },
			{ 123, 8 },
	};
	unsigned int i;

	printf("Test mbuf linearize API\n");

	for (i = 0; i < RTE_DIM(mbuf_array); i++)
		if (test_mbuf_linearize(pktmbuf_pool, mbuf_array[i].size,
				mbuf_array[i].nb_segs)) {
			printf("Test failed for %d, %d\n", mbuf_array[i].size,
					mbuf_array[i].nb_segs);
			return -1;
		}

	return 0;
}

static int
test_mbuf(void)
{
	int ret = -1;
	struct rte_mempool *pktmbuf_pool = NULL;
	struct rte_mempool *pktmbuf_pool2 = NULL;


	RTE_BUILD_BUG_ON(sizeof(struct rte_mbuf) != RTE_CACHE_LINE_MIN_SIZE * 2);

	/* create pktmbuf pool if it does not exist */
	pktmbuf_pool = rte_pktmbuf_pool_create("test_pktmbuf_pool",
			NB_MBUF, 32, 0, MBUF_DATA_SIZE, SOCKET_ID_ANY);

	if (pktmbuf_pool == NULL) {
		printf("cannot allocate mbuf pool\n");
		goto err;
	}

	/* create a specific pktmbuf pool with a priv_size != 0 and no data
	 * room size */
	pktmbuf_pool2 = rte_pktmbuf_pool_create("test_pktmbuf_pool2",
			NB_MBUF, 32, MBUF2_PRIV_SIZE, 0, SOCKET_ID_ANY);

	if (pktmbuf_pool2 == NULL) {
		printf("cannot allocate mbuf pool\n");
		goto err;
	}

	/* test multiple mbuf alloc */
	if (test_pktmbuf_pool(pktmbuf_pool) < 0) {
		printf("test_mbuf_pool() failed\n");
		goto err;
	}

	/* do it another time to check that all mbufs were freed */
	if (test_pktmbuf_pool(pktmbuf_pool) < 0) {
		printf("test_mbuf_pool() failed (2)\n");
		goto err;
	}

	/* test that the pointer to the data on a packet mbuf is set properly */
	if (test_pktmbuf_pool_ptr(pktmbuf_pool) < 0) {
		printf("test_pktmbuf_pool_ptr() failed\n");
		goto err;
	}

	/* test data manipulation in mbuf */
	if (test_one_pktmbuf(pktmbuf_pool) < 0) {
		printf("test_one_mbuf() failed\n");
		goto err;
	}


	/*
	 * do it another time, to check that allocation reinitialize
	 * the mbuf correctly
	 */
	if (test_one_pktmbuf(pktmbuf_pool) < 0) {
		printf("test_one_mbuf() failed (2)\n");
		goto err;
	}

	if (test_pktmbuf_with_non_ascii_data(pktmbuf_pool) < 0) {
		printf("test_pktmbuf_with_non_ascii_data() failed\n");
		goto err;
	}

	/* test free pktmbuf segment one by one */
	if (test_pktmbuf_free_segment(pktmbuf_pool) < 0) {
		printf("test_pktmbuf_free_segment() failed.\n");
		goto err;
	}

	if (testclone_testupdate_testdetach(pktmbuf_pool) < 0) {
		printf("testclone_and_testupdate() failed \n");
		goto err;
	}

	if (test_attach_from_different_pool(pktmbuf_pool, pktmbuf_pool2) < 0) {
		printf("test_attach_from_different_pool() failed\n");
		goto err;
	}

	if (test_refcnt_mbuf()<0){
		printf("test_refcnt_mbuf() failed \n");
		goto err;
	}

	if (test_failing_mbuf_sanity_check(pktmbuf_pool) < 0) {
		printf("test_failing_mbuf_sanity_check() failed\n");
		goto err;
	}

	if (test_mbuf_linearize_check(pktmbuf_pool) < 0) {
		printf("test_mbuf_linearize_check() failed\n");
		goto err;
	}
	ret = 0;

err:
	rte_mempool_free(pktmbuf_pool);
	rte_mempool_free(pktmbuf_pool2);
	return ret;
}

REGISTER_TEST_COMMAND(mbuf_autotest, test_mbuf);
