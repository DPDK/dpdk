/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "test.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_mbuf_dyn.h>

#define MEMPOOL_CACHE_SIZE      32
#define MBUF_DATA_SIZE          2048
#define NB_MBUF                 128
#define MBUF_TEST_DATA_LEN      1464
#define MBUF_TEST_DATA_LEN2     50
#define MBUF_TEST_DATA_LEN3     256
#define MBUF_TEST_HDR1_LEN      20
#define MBUF_TEST_HDR2_LEN      30
#define MBUF_TEST_ALL_HDRS_LEN  (MBUF_TEST_HDR1_LEN+MBUF_TEST_HDR2_LEN)
#define MBUF_TEST_SEG_SIZE      64
#define MBUF_TEST_BURST         8
#define EXT_BUF_TEST_DATA_LEN   1024
#define MBUF_MAX_SEG            16
#define MBUF_NO_HEADER		0
#define MBUF_HEADER		1
#define MBUF_NEG_TEST_READ	2
#define VAL_NAME(flag)          { flag, #flag }

/* chain length in bulk test */
#define CHAIN_LEN 16

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

static volatile uint32_t refcnt_stop_workers;
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
	rte_pktmbuf_free(m);
	return -1;
}

static uint16_t
testclone_refcnt_read(struct rte_mbuf *m)
{
	return RTE_MBUF_HAS_PINNED_EXTBUF(m) ?
	       rte_mbuf_ext_refcnt_read(m->shinfo) :
	       rte_mbuf_refcnt_read(m);
}

static int
testclone_testupdate_testdetach(struct rte_mempool *pktmbuf_pool,
				struct rte_mempool *clone_pool)
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
	clone = rte_pktmbuf_clone(m, clone_pool);
	if (clone == NULL)
		GOTO_FAIL("cannot clone data\n");

	data = rte_pktmbuf_mtod(clone, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone\n");

	if (testclone_refcnt_read(m) != 2)
		GOTO_FAIL("invalid refcnt in m\n");

	/* free the clone */
	rte_pktmbuf_free(clone);
	clone = NULL;

	/* same test with a chained mbuf */
	m->next = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m->next == NULL)
		GOTO_FAIL("Next Pkt Null\n");
	m->nb_segs = 2;

	rte_pktmbuf_append(m->next, sizeof(uint32_t));
	m->pkt_len = 2 * sizeof(uint32_t);

	data = rte_pktmbuf_mtod(m->next, unaligned_uint32_t *);
	*data = MAGIC_DATA;

	clone = rte_pktmbuf_clone(m, clone_pool);
	if (clone == NULL)
		GOTO_FAIL("cannot clone data\n");

	data = rte_pktmbuf_mtod(clone, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone\n");

	data = rte_pktmbuf_mtod(clone->next, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone->next\n");

	if (testclone_refcnt_read(m) != 2)
		GOTO_FAIL("invalid refcnt in m\n");

	if (testclone_refcnt_read(m->next) != 2)
		GOTO_FAIL("invalid refcnt in m->next\n");

	/* try to clone the clone */

	clone2 = rte_pktmbuf_clone(clone, clone_pool);
	if (clone2 == NULL)
		GOTO_FAIL("cannot clone the clone\n");

	data = rte_pktmbuf_mtod(clone2, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone2\n");

	data = rte_pktmbuf_mtod(clone2->next, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone2->next\n");

	if (testclone_refcnt_read(m) != 3)
		GOTO_FAIL("invalid refcnt in m\n");

	if (testclone_refcnt_read(m->next) != 3)
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
	rte_pktmbuf_free(m);
	rte_pktmbuf_free(clone);
	rte_pktmbuf_free(clone2);
	return -1;
}

static int
test_pktmbuf_copy(struct rte_mempool *pktmbuf_pool,
		  struct rte_mempool *clone_pool)
{
	struct rte_mbuf *m = NULL;
	struct rte_mbuf *copy = NULL;
	struct rte_mbuf *copy2 = NULL;
	struct rte_mbuf *clone = NULL;
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

	/* copy the allocated mbuf */
	copy = rte_pktmbuf_copy(m, pktmbuf_pool, 0, UINT32_MAX);
	if (copy == NULL)
		GOTO_FAIL("cannot copy data\n");

	if (rte_pktmbuf_pkt_len(copy) != sizeof(uint32_t))
		GOTO_FAIL("copy length incorrect\n");

	if (rte_pktmbuf_data_len(copy) != sizeof(uint32_t))
		GOTO_FAIL("copy data length incorrect\n");

	data = rte_pktmbuf_mtod(copy, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in copy\n");

	/* free the copy */
	rte_pktmbuf_free(copy);
	copy = NULL;

	/* same test with a cloned mbuf */
	clone = rte_pktmbuf_clone(m, clone_pool);
	if (clone == NULL)
		GOTO_FAIL("cannot clone data\n");

	if ((!RTE_MBUF_HAS_PINNED_EXTBUF(m) &&
	     !RTE_MBUF_CLONED(clone)) ||
	    (RTE_MBUF_HAS_PINNED_EXTBUF(m) &&
	     !RTE_MBUF_HAS_EXTBUF(clone)))
		GOTO_FAIL("clone did not give a cloned mbuf\n");

	copy = rte_pktmbuf_copy(clone, pktmbuf_pool, 0, UINT32_MAX);
	if (copy == NULL)
		GOTO_FAIL("cannot copy cloned mbuf\n");

	if (RTE_MBUF_CLONED(copy))
		GOTO_FAIL("copy of clone is cloned?\n");

	if (rte_pktmbuf_pkt_len(copy) != sizeof(uint32_t))
		GOTO_FAIL("copy clone length incorrect\n");

	if (rte_pktmbuf_data_len(copy) != sizeof(uint32_t))
		GOTO_FAIL("copy clone data length incorrect\n");

	data = rte_pktmbuf_mtod(copy, unaligned_uint32_t *);
	if (*data != MAGIC_DATA)
		GOTO_FAIL("invalid data in clone copy\n");
	rte_pktmbuf_free(clone);
	rte_pktmbuf_free(copy);
	copy = NULL;
	clone = NULL;


	/* same test with a chained mbuf */
	m->next = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m->next == NULL)
		GOTO_FAIL("Next Pkt Null\n");
	m->nb_segs = 2;

	rte_pktmbuf_append(m->next, sizeof(uint32_t));
	m->pkt_len = 2 * sizeof(uint32_t);
	data = rte_pktmbuf_mtod(m->next, unaligned_uint32_t *);
	*data = MAGIC_DATA + 1;

	copy = rte_pktmbuf_copy(m, pktmbuf_pool, 0, UINT32_MAX);
	if (copy == NULL)
		GOTO_FAIL("cannot copy data\n");

	if (rte_pktmbuf_pkt_len(copy) != 2 * sizeof(uint32_t))
		GOTO_FAIL("chain copy length incorrect\n");

	if (rte_pktmbuf_data_len(copy) != 2 * sizeof(uint32_t))
		GOTO_FAIL("chain copy data length incorrect\n");

	data = rte_pktmbuf_mtod(copy, unaligned_uint32_t *);
	if (data[0] != MAGIC_DATA || data[1] != MAGIC_DATA + 1)
		GOTO_FAIL("invalid data in copy\n");

	rte_pktmbuf_free(copy2);

	/* test offset copy */
	copy2 = rte_pktmbuf_copy(copy, pktmbuf_pool,
				 sizeof(uint32_t), UINT32_MAX);
	if (copy2 == NULL)
		GOTO_FAIL("cannot copy the copy\n");

	if (rte_pktmbuf_pkt_len(copy2) != sizeof(uint32_t))
		GOTO_FAIL("copy with offset, length incorrect\n");

	if (rte_pktmbuf_data_len(copy2) != sizeof(uint32_t))
		GOTO_FAIL("copy with offset, data length incorrect\n");

	data = rte_pktmbuf_mtod(copy2, unaligned_uint32_t *);
	if (data[0] != MAGIC_DATA + 1)
		GOTO_FAIL("copy with offset, invalid data\n");

	rte_pktmbuf_free(copy2);

	/* test truncation copy */
	copy2 = rte_pktmbuf_copy(copy, pktmbuf_pool,
				 0, sizeof(uint32_t));
	if (copy2 == NULL)
		GOTO_FAIL("cannot copy the copy\n");

	if (rte_pktmbuf_pkt_len(copy2) != sizeof(uint32_t))
		GOTO_FAIL("copy with truncate, length incorrect\n");

	if (rte_pktmbuf_data_len(copy2) != sizeof(uint32_t))
		GOTO_FAIL("copy with truncate, data length incorrect\n");

	data = rte_pktmbuf_mtod(copy2, unaligned_uint32_t *);
	if (data[0] != MAGIC_DATA)
		GOTO_FAIL("copy with truncate, invalid data\n");

	/* free mbuf */
	rte_pktmbuf_free(m);
	rte_pktmbuf_free(copy);
	rte_pktmbuf_free(copy2);

	m = NULL;
	copy = NULL;
	copy2 = NULL;
	printf("%s ok\n", __func__);
	return 0;

fail:
	rte_pktmbuf_free(m);
	rte_pktmbuf_free(copy);
	rte_pktmbuf_free(copy2);
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
	rte_pktmbuf_free(m);
	rte_pktmbuf_free(clone);
	rte_pktmbuf_free(clone2);
	return -1;
}

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
		rte_pktmbuf_free(m[i]);
	}

	return ret;
}

/*
 * test bulk allocation and bulk free of mbufs
 */
static int
test_pktmbuf_pool_bulk(void)
{
	struct rte_mempool *pool = NULL;
	struct rte_mempool *pool2 = NULL;
	unsigned int i;
	struct rte_mbuf *m;
	struct rte_mbuf *mbufs[NB_MBUF];
	int ret = 0;

	/* We cannot use the preallocated mbuf pools because their caches
	 * prevent us from bulk allocating all objects in them.
	 * So we create our own mbuf pools without caches.
	 */
	printf("Create mbuf pools for bulk allocation.\n");
	pool = rte_pktmbuf_pool_create("test_pktmbuf_bulk",
			NB_MBUF, 0, 0, MBUF_DATA_SIZE, SOCKET_ID_ANY);
	if (pool == NULL) {
		printf("rte_pktmbuf_pool_create() failed. rte_errno %d\n",
		       rte_errno);
		goto err;
	}
	pool2 = rte_pktmbuf_pool_create("test_pktmbuf_bulk2",
			NB_MBUF, 0, 0, MBUF_DATA_SIZE, SOCKET_ID_ANY);
	if (pool2 == NULL) {
		printf("rte_pktmbuf_pool_create() failed. rte_errno %d\n",
		       rte_errno);
		goto err;
	}

	/* Preconditions: Mempools must be full. */
	if (!(rte_mempool_full(pool) && rte_mempool_full(pool2))) {
		printf("Test precondition failed: mempools not full\n");
		goto err;
	}
	if (!(rte_mempool_avail_count(pool) == NB_MBUF &&
			rte_mempool_avail_count(pool2) == NB_MBUF)) {
		printf("Test precondition failed: mempools: %u+%u != %u+%u",
		       rte_mempool_avail_count(pool),
		       rte_mempool_avail_count(pool2),
		       NB_MBUF, NB_MBUF);
		goto err;
	}

	printf("Test single bulk alloc, followed by multiple bulk free.\n");

	/* Bulk allocate all mbufs in the pool, in one go. */
	ret = rte_pktmbuf_alloc_bulk(pool, mbufs, NB_MBUF);
	if (ret != 0) {
		printf("rte_pktmbuf_alloc_bulk() failed: %d\n", ret);
		goto err;
	}
	/* Test that they have been removed from the pool. */
	if (!rte_mempool_empty(pool)) {
		printf("mempool not empty\n");
		goto err;
	}
	/* Bulk free all mbufs, in four steps. */
	RTE_BUILD_BUG_ON(NB_MBUF % 4 != 0);
	for (i = 0; i < NB_MBUF; i += NB_MBUF / 4) {
		rte_pktmbuf_free_bulk(&mbufs[i], NB_MBUF / 4);
		/* Test that they have been returned to the pool. */
		if (rte_mempool_avail_count(pool) != i + NB_MBUF / 4) {
			printf("mempool avail count incorrect\n");
			goto err;
		}
	}

	printf("Test multiple bulk alloc, followed by single bulk free.\n");

	/* Bulk allocate all mbufs in the pool, in four steps. */
	for (i = 0; i < NB_MBUF; i += NB_MBUF / 4) {
		ret = rte_pktmbuf_alloc_bulk(pool, &mbufs[i], NB_MBUF / 4);
		if (ret != 0) {
			printf("rte_pktmbuf_alloc_bulk() failed: %d\n", ret);
			goto err;
		}
	}
	/* Test that they have been removed from the pool. */
	if (!rte_mempool_empty(pool)) {
		printf("mempool not empty\n");
		goto err;
	}
	/* Bulk free all mbufs, in one go. */
	rte_pktmbuf_free_bulk(mbufs, NB_MBUF);
	/* Test that they have been returned to the pool. */
	if (!rte_mempool_full(pool)) {
		printf("mempool not full\n");
		goto err;
	}

	printf("Test bulk free of single long chain.\n");

	/* Bulk allocate all mbufs in the pool, in one go. */
	ret = rte_pktmbuf_alloc_bulk(pool, mbufs, NB_MBUF);
	if (ret != 0) {
		printf("rte_pktmbuf_alloc_bulk() failed: %d\n", ret);
		goto err;
	}
	/* Create a long mbuf chain. */
	for (i = 1; i < NB_MBUF; i++) {
		ret = rte_pktmbuf_chain(mbufs[0], mbufs[i]);
		if (ret != 0) {
			printf("rte_pktmbuf_chain() failed: %d\n", ret);
			goto err;
		}
		mbufs[i] = NULL;
	}
	/* Free the mbuf chain containing all the mbufs. */
	rte_pktmbuf_free_bulk(mbufs, 1);
	/* Test that they have been returned to the pool. */
	if (!rte_mempool_full(pool)) {
		printf("mempool not full\n");
		goto err;
	}

	printf("Test bulk free of multiple chains using multiple pools.\n");

	/* Create mbuf chains containing mbufs from different pools. */
	RTE_BUILD_BUG_ON(CHAIN_LEN % 2 != 0);
	RTE_BUILD_BUG_ON(NB_MBUF % (CHAIN_LEN / 2) != 0);
	for (i = 0; i < NB_MBUF * 2; i++) {
		m = rte_pktmbuf_alloc((i & 4) ? pool2 : pool);
		if (m == NULL) {
			printf("rte_pktmbuf_alloc() failed (%u)\n", i);
			goto err;
		}
		if ((i % CHAIN_LEN) == 0)
			mbufs[i / CHAIN_LEN] = m;
		else
			rte_pktmbuf_chain(mbufs[i / CHAIN_LEN], m);
	}
	/* Test that both pools have been emptied. */
	if (!(rte_mempool_empty(pool) && rte_mempool_empty(pool2))) {
		printf("mempools not empty\n");
		goto err;
	}
	/* Free one mbuf chain. */
	rte_pktmbuf_free_bulk(mbufs, 1);
	/* Test that the segments have been returned to the pools. */
	if (!(rte_mempool_avail_count(pool) == CHAIN_LEN / 2 &&
			rte_mempool_avail_count(pool2) == CHAIN_LEN / 2)) {
		printf("all segments of first mbuf have not been returned\n");
		goto err;
	}
	/* Free the remaining mbuf chains. */
	rte_pktmbuf_free_bulk(&mbufs[1], NB_MBUF * 2 / CHAIN_LEN - 1);
	/* Test that they have been returned to the pools. */
	if (!(rte_mempool_full(pool) && rte_mempool_full(pool2))) {
		printf("mempools not full\n");
		goto err;
	}

	ret = 0;
	goto done;

err:
	ret = -1;

done:
	printf("Free mbuf pools for bulk allocation.\n");
	rte_mempool_free(pool);
	rte_mempool_free(pool2);
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
test_refcnt_worker(void *arg)
{
	unsigned lcore, free;
	void *mp = 0;
	struct rte_ring *refcnt_mbuf_ring = arg;

	lcore = rte_lcore_id();
	printf("%s started at lcore %u\n", __func__, lcore);

	free = 0;
	while (refcnt_stop_workers == 0) {
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
	 * - enqueue it N times into the ring for worker cores to free.
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

	/* wait till worker lcores  will consume all mbufs */
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
test_refcnt_main(struct rte_mempool *refcnt_pool,
		   struct rte_ring *refcnt_mbuf_ring)
{
	unsigned i, lcore;

	lcore = rte_lcore_id();
	printf("%s started at lcore %u\n", __func__, lcore);

	for (i = 0; i != REFCNT_MAX_ITER; i++)
		test_refcnt_iter(lcore, i, refcnt_pool, refcnt_mbuf_ring);

	refcnt_stop_workers = 1;
	rte_wmb();

	printf("%s finished at lcore %u\n", __func__, lcore);
	return 0;
}

#endif

static int
test_refcnt_mbuf(void)
{
#ifdef RTE_MBUF_REFCNT_ATOMIC
	unsigned int main_lcore, worker, tref;
	int ret = -1;
	struct rte_mempool *refcnt_pool = NULL;
	struct rte_ring *refcnt_mbuf_ring = NULL;

	if (rte_lcore_count() < 2) {
		printf("Not enough cores for test_refcnt_mbuf, expecting at least 2\n");
		return TEST_SKIPPED;
	}

	printf("starting %s, at %u lcores\n", __func__, rte_lcore_count());

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

	refcnt_stop_workers = 0;
	memset(refcnt_lcore, 0, sizeof (refcnt_lcore));

	rte_eal_mp_remote_launch(test_refcnt_worker, refcnt_mbuf_ring, SKIP_MAIN);

	test_refcnt_main(refcnt_pool, refcnt_mbuf_ring);

	rte_eal_mp_wait_lcore();

	/* check that we processed all references */
	tref = 0;
	main_lcore = rte_get_main_lcore();

	RTE_LCORE_FOREACH_WORKER(worker)
		tref += refcnt_lcore[worker];

	if (tref != refcnt_lcore[main_lcore])
		rte_panic("referenced mbufs: %u, freed mbufs: %u\n",
			  tref, refcnt_lcore[main_lcore]);

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

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_failing_mbuf_sanity_check(struct rte_mempool *pktmbuf_pool)
{
	RTE_SET_USED(pktmbuf_pool);
	return TEST_SKIPPED;
}
#else

#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>

/* use fork() to test mbuf errors panic */
static int
verify_mbuf_check_panics(struct rte_mbuf *buf)
{
	int pid;
	int status;

	pid = fork();

	if (pid == 0) {
		struct rlimit rl;

		/* No need to generate a coredump when panicking. */
		rl.rlim_cur = rl.rlim_max = 0;
		setrlimit(RLIMIT_CORE, &rl);
		rte_mbuf_sanity_check(buf, 1); /* should panic */
		exit(0);  /* return normally if it doesn't panic */
	} else if (pid < 0) {
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

	if (RTE_IOVA_IN_MBUF) {
		badbuf = *buf;
		rte_mbuf_iova_set(&badbuf, 0);
		if (verify_mbuf_check_panics(&badbuf)) {
			printf("Error with bad-physaddr mbuf test\n");
			return -1;
		}
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

#endif /* !RTE_EXEC_ENV_WINDOWS */

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

/*
 * Helper function for test_tx_ofload
 */
static inline void
set_tx_offload(struct rte_mbuf *mb, uint64_t il2, uint64_t il3, uint64_t il4,
	uint64_t tso, uint64_t ol3, uint64_t ol2)
{
	mb->l2_len = il2;
	mb->l3_len = il3;
	mb->l4_len = il4;
	mb->tso_segsz = tso;
	mb->outer_l3_len = ol3;
	mb->outer_l2_len = ol2;
}

static int
test_tx_offload(void)
{
	struct rte_mbuf *mb;
	uint64_t tm, v1, v2;
	size_t sz;
	uint32_t i;

	static volatile struct {
		uint16_t l2;
		uint16_t l3;
		uint16_t l4;
		uint16_t tso;
	} txof;

	const uint32_t num = 0x10000;

	txof.l2 = rte_rand() % (1 <<  RTE_MBUF_L2_LEN_BITS);
	txof.l3 = rte_rand() % (1 <<  RTE_MBUF_L3_LEN_BITS);
	txof.l4 = rte_rand() % (1 <<  RTE_MBUF_L4_LEN_BITS);
	txof.tso = rte_rand() % (1 <<   RTE_MBUF_TSO_SEGSZ_BITS);

	printf("%s started, tx_offload = {\n"
		"\tl2_len=%#hx,\n"
		"\tl3_len=%#hx,\n"
		"\tl4_len=%#hx,\n"
		"\ttso_segsz=%#hx,\n"
		"\touter_l3_len=%#x,\n"
		"\touter_l2_len=%#x,\n"
		"};\n",
		__func__,
		txof.l2, txof.l3, txof.l4, txof.tso, txof.l3, txof.l2);

	sz = sizeof(*mb) * num;
	mb = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	if (mb == NULL) {
		printf("%s failed, out of memory\n", __func__);
		return -ENOMEM;
	}

	memset(mb, 0, sz);
	tm = rte_rdtsc_precise();

	for (i = 0; i != num; i++)
		set_tx_offload(mb + i, txof.l2, txof.l3, txof.l4,
			txof.tso, txof.l3, txof.l2);

	tm = rte_rdtsc_precise() - tm;
	printf("%s set tx_offload by bit-fields: %u iterations, %"
		PRIu64 " cycles, %#Lf cycles/iter\n",
		__func__, num, tm, (long double)tm / num);

	v1 = mb[rte_rand() % num].tx_offload;

	memset(mb, 0, sz);
	tm = rte_rdtsc_precise();

	for (i = 0; i != num; i++)
		mb[i].tx_offload = rte_mbuf_tx_offload(txof.l2, txof.l3,
			txof.l4, txof.tso, txof.l3, txof.l2, 0);

	tm = rte_rdtsc_precise() - tm;
	printf("%s set raw tx_offload: %u iterations, %"
		PRIu64 " cycles, %#Lf cycles/iter\n",
		__func__, num, tm, (long double)tm / num);

	v2 = mb[rte_rand() % num].tx_offload;

	rte_free(mb);

	printf("%s finished\n"
		"expected tx_offload value: 0x%" PRIx64 ";\n"
		"rte_mbuf_tx_offload value: 0x%" PRIx64 ";\n",
		__func__, v1, v2);

	return (v1 == v2) ? 0 : -EINVAL;
}

static int
test_get_rx_ol_flag_list(void)
{
	int len = 6, ret = 0;
	char buf[256] = "";
	int buflen = 0;

	/* Test case to check with null buffer */
	ret = rte_get_rx_ol_flag_list(0, NULL, 0);
	if (ret != -1)
		GOTO_FAIL("%s expected: -1, received = %d\n", __func__, ret);

	/* Test case to check with zero buffer len */
	ret = rte_get_rx_ol_flag_list(RTE_MBUF_F_RX_L4_CKSUM_MASK, buf, 0);
	if (ret != -1)
		GOTO_FAIL("%s expected: -1, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen != 0)
		GOTO_FAIL("%s buffer should be empty, received = %d\n",
				__func__, buflen);

	/* Test case to check with reduced buffer len */
	ret = rte_get_rx_ol_flag_list(0, buf, len);
	if (ret != -1)
		GOTO_FAIL("%s expected: -1, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen != (len - 1))
		GOTO_FAIL("%s invalid buffer length retrieved, expected: %d,"
				"received = %d\n", __func__,
				(len - 1), buflen);

	/* Test case to check with zero mask value */
	ret = rte_get_rx_ol_flag_list(0, buf, sizeof(buf));
	if (ret != 0)
		GOTO_FAIL("%s expected: 0, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen == 0)
		GOTO_FAIL("%s expected: %s, received length = 0\n", __func__,
				"non-zero, buffer should not be empty");

	/* Test case to check with valid mask value */
	ret = rte_get_rx_ol_flag_list(RTE_MBUF_F_RX_SEC_OFFLOAD, buf,
				      sizeof(buf));
	if (ret != 0)
		GOTO_FAIL("%s expected: 0, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen == 0)
		GOTO_FAIL("%s expected: %s, received length = 0\n", __func__,
				"non-zero, buffer should not be empty");

	return 0;
fail:
	return -1;
}

static int
test_get_tx_ol_flag_list(void)
{
	int len = 6, ret = 0;
	char buf[256] = "";
	int buflen = 0;

	/* Test case to check with null buffer */
	ret = rte_get_tx_ol_flag_list(0, NULL, 0);
	if (ret != -1)
		GOTO_FAIL("%s expected: -1, received = %d\n", __func__, ret);

	/* Test case to check with zero buffer len */
	ret = rte_get_tx_ol_flag_list(RTE_MBUF_F_TX_IP_CKSUM, buf, 0);
	if (ret != -1)
		GOTO_FAIL("%s expected: -1, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen != 0) {
		GOTO_FAIL("%s buffer should be empty, received = %d\n",
				__func__, buflen);
	}

	/* Test case to check with reduced buffer len */
	ret = rte_get_tx_ol_flag_list(0, buf, len);
	if (ret != -1)
		GOTO_FAIL("%s expected: -1, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen != (len - 1))
		GOTO_FAIL("%s invalid buffer length retrieved, expected: %d,"
				"received = %d\n", __func__,
				(len - 1), buflen);

	/* Test case to check with zero mask value */
	ret = rte_get_tx_ol_flag_list(0, buf, sizeof(buf));
	if (ret != 0)
		GOTO_FAIL("%s expected: 0, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen == 0)
		GOTO_FAIL("%s expected: %s, received length = 0\n", __func__,
				"non-zero, buffer should not be empty");

	/* Test case to check with valid mask value */
	ret = rte_get_tx_ol_flag_list(RTE_MBUF_F_TX_UDP_CKSUM, buf,
				      sizeof(buf));
	if (ret != 0)
		GOTO_FAIL("%s expected: 0, received = %d\n", __func__, ret);

	buflen = strlen(buf);
	if (buflen == 0)
		GOTO_FAIL("%s expected: %s, received length = 0\n", __func__,
				"non-zero, buffer should not be empty");

	return 0;
fail:
	return -1;

}

struct flag_name {
	uint64_t flag;
	const char *name;
};

static int
test_get_rx_ol_flag_name(void)
{
	uint16_t i;
	const char *flag_str = NULL;
	const struct flag_name rx_flags[] = {
		VAL_NAME(RTE_MBUF_F_RX_VLAN),
		VAL_NAME(RTE_MBUF_F_RX_RSS_HASH),
		VAL_NAME(RTE_MBUF_F_RX_FDIR),
		VAL_NAME(RTE_MBUF_F_RX_L4_CKSUM_BAD),
		VAL_NAME(RTE_MBUF_F_RX_L4_CKSUM_GOOD),
		VAL_NAME(RTE_MBUF_F_RX_L4_CKSUM_NONE),
		VAL_NAME(RTE_MBUF_F_RX_IP_CKSUM_BAD),
		VAL_NAME(RTE_MBUF_F_RX_IP_CKSUM_GOOD),
		VAL_NAME(RTE_MBUF_F_RX_IP_CKSUM_NONE),
		VAL_NAME(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD),
		VAL_NAME(RTE_MBUF_F_RX_VLAN_STRIPPED),
		VAL_NAME(RTE_MBUF_F_RX_IEEE1588_PTP),
		VAL_NAME(RTE_MBUF_F_RX_IEEE1588_TMST),
		VAL_NAME(RTE_MBUF_F_RX_FDIR_ID),
		VAL_NAME(RTE_MBUF_F_RX_FDIR_FLX),
		VAL_NAME(RTE_MBUF_F_RX_QINQ_STRIPPED),
		VAL_NAME(RTE_MBUF_F_RX_LRO),
		VAL_NAME(RTE_MBUF_F_RX_SEC_OFFLOAD),
		VAL_NAME(RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED),
		VAL_NAME(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD),
		VAL_NAME(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD),
		VAL_NAME(RTE_MBUF_F_RX_OUTER_L4_CKSUM_INVALID),
	};

	/* Test case to check with valid flag */
	for (i = 0; i < RTE_DIM(rx_flags); i++) {
		flag_str = rte_get_rx_ol_flag_name(rx_flags[i].flag);
		if (flag_str == NULL)
			GOTO_FAIL("%s: Expected flagname = %s; received null\n",
					__func__, rx_flags[i].name);
		if (strcmp(flag_str, rx_flags[i].name) != 0)
			GOTO_FAIL("%s: Expected flagname = %s; received = %s\n",
				__func__, rx_flags[i].name, flag_str);
	}
	/* Test case to check with invalid flag */
	flag_str = rte_get_rx_ol_flag_name(0);
	if (flag_str != NULL) {
		GOTO_FAIL("%s: Expected flag name = null; received = %s\n",
				__func__, flag_str);
	}

	return 0;
fail:
	return -1;
}

static int
test_get_tx_ol_flag_name(void)
{
	uint16_t i;
	const char *flag_str = NULL;
	const struct flag_name tx_flags[] = {
		VAL_NAME(RTE_MBUF_F_TX_VLAN),
		VAL_NAME(RTE_MBUF_F_TX_IP_CKSUM),
		VAL_NAME(RTE_MBUF_F_TX_TCP_CKSUM),
		VAL_NAME(RTE_MBUF_F_TX_SCTP_CKSUM),
		VAL_NAME(RTE_MBUF_F_TX_UDP_CKSUM),
		VAL_NAME(RTE_MBUF_F_TX_IEEE1588_TMST),
		VAL_NAME(RTE_MBUF_F_TX_TCP_SEG),
		VAL_NAME(RTE_MBUF_F_TX_IPV4),
		VAL_NAME(RTE_MBUF_F_TX_IPV6),
		VAL_NAME(RTE_MBUF_F_TX_OUTER_IP_CKSUM),
		VAL_NAME(RTE_MBUF_F_TX_OUTER_IPV4),
		VAL_NAME(RTE_MBUF_F_TX_OUTER_IPV6),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_VXLAN),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_GRE),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_IPIP),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_GENEVE),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_MPLSINUDP),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_IP),
		VAL_NAME(RTE_MBUF_F_TX_TUNNEL_UDP),
		VAL_NAME(RTE_MBUF_F_TX_QINQ),
		VAL_NAME(RTE_MBUF_F_TX_MACSEC),
		VAL_NAME(RTE_MBUF_F_TX_SEC_OFFLOAD),
		VAL_NAME(RTE_MBUF_F_TX_UDP_SEG),
		VAL_NAME(RTE_MBUF_F_TX_OUTER_UDP_CKSUM),
	};

	/* Test case to check with valid flag */
	for (i = 0; i < RTE_DIM(tx_flags); i++) {
		flag_str = rte_get_tx_ol_flag_name(tx_flags[i].flag);
		if (flag_str == NULL)
			GOTO_FAIL("%s: Expected flagname = %s; received null\n",
				__func__, tx_flags[i].name);
		if (strcmp(flag_str, tx_flags[i].name) != 0)
			GOTO_FAIL("%s: Expected flagname = %s; received = %s\n",
				__func__, tx_flags[i].name, flag_str);
	}
	/* Test case to check with invalid flag */
	flag_str = rte_get_tx_ol_flag_name(0);
	if (flag_str != NULL) {
		GOTO_FAIL("%s: Expected flag name = null; received = %s\n",
				__func__, flag_str);
	}

	return 0;
fail:
	return -1;

}

static int
test_mbuf_validate_tx_offload(const char *test_name,
		struct rte_mempool *pktmbuf_pool,
		uint64_t ol_flags,
		uint16_t segsize,
		int expected_retval)
{
	struct rte_mbuf *m = NULL;
	int ret = 0;

	/* alloc a mbuf and do sanity check */
	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("%s: mbuf allocation failed!\n", __func__);
	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("%s: Bad packet length\n", __func__);
	rte_mbuf_sanity_check(m, 0);
	m->ol_flags = ol_flags;
	m->tso_segsz = segsize;
	ret = rte_validate_tx_offload(m);
	if (ret != expected_retval)
		GOTO_FAIL("%s(%s): expected ret val: %d; received: %d\n",
				__func__, test_name, expected_retval, ret);
	rte_pktmbuf_free(m);
	m = NULL;
	return 0;
fail:
	if (m) {
		rte_pktmbuf_free(m);
		m = NULL;
	}
	return -1;
}

static int
test_mbuf_validate_tx_offload_one(struct rte_mempool *pktmbuf_pool)
{
	/* test to validate tx offload flags */
	uint64_t ol_flags = 0;

	/* test to validate if IP checksum is counted only for IPV4 packet */
	/* set both IP checksum and IPV6 flags */
	ol_flags |= RTE_MBUF_F_TX_IP_CKSUM;
	ol_flags |= RTE_MBUF_F_TX_IPV6;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_IP_CKSUM_IPV6_SET",
				pktmbuf_pool,
				ol_flags, 0, -EINVAL) < 0)
		GOTO_FAIL("%s failed: IP cksum is set incorrect.\n", __func__);
	/* resetting ol_flags for next testcase */
	ol_flags = 0;

	/* test to validate if IP type is set when required */
	ol_flags |= RTE_MBUF_F_TX_L4_MASK;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_IP_TYPE_NOT_SET",
				pktmbuf_pool,
				ol_flags, 0, -EINVAL) < 0)
		GOTO_FAIL("%s failed: IP type is not set.\n", __func__);

	/* test if IP type is set when TCP SEG is on */
	ol_flags |= RTE_MBUF_F_TX_TCP_SEG;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_IP_TYPE_NOT_SET",
				pktmbuf_pool,
				ol_flags, 0, -EINVAL) < 0)
		GOTO_FAIL("%s failed: IP type is not set.\n", __func__);

	ol_flags = 0;
	/* test to confirm IP type (IPV4/IPV6) is set */
	ol_flags = RTE_MBUF_F_TX_L4_MASK;
	ol_flags |= RTE_MBUF_F_TX_IPV6;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_IP_TYPE_SET",
				pktmbuf_pool,
				ol_flags, 0, 0) < 0)
		GOTO_FAIL("%s failed: tx offload flag error.\n", __func__);

	ol_flags = 0;
	/* test to check TSO segment size is non-zero */
	ol_flags |= RTE_MBUF_F_TX_IPV4;
	ol_flags |= RTE_MBUF_F_TX_TCP_SEG;
	/* set 0 tso segment size */
	if (test_mbuf_validate_tx_offload("MBUF_TEST_NULL_TSO_SEGSZ",
				pktmbuf_pool,
				ol_flags, 0, -EINVAL) < 0)
		GOTO_FAIL("%s failed: tso segment size is null.\n", __func__);

	/* retain IPV4 and RTE_MBUF_F_TX_TCP_SEG mask */
	/* set valid tso segment size but IP CKSUM not set */
	if (test_mbuf_validate_tx_offload("MBUF_TEST_TSO_IP_CKSUM_NOT_SET",
				pktmbuf_pool,
				ol_flags, 512, -EINVAL) < 0)
		GOTO_FAIL("%s failed: IP CKSUM is not set.\n", __func__);

	/* test to validate if IP checksum is set for TSO capability */
	/* retain IPV4, TCP_SEG, tso_seg size */
	ol_flags |= RTE_MBUF_F_TX_IP_CKSUM;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_TSO_IP_CKSUM_SET",
				pktmbuf_pool,
				ol_flags, 512, 0) < 0)
		GOTO_FAIL("%s failed: tx offload flag error.\n", __func__);

	/* test to confirm TSO for IPV6 type */
	ol_flags = 0;
	ol_flags |= RTE_MBUF_F_TX_IPV6;
	ol_flags |= RTE_MBUF_F_TX_TCP_SEG;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_TSO_IPV6_SET",
				pktmbuf_pool,
				ol_flags, 512, 0) < 0)
		GOTO_FAIL("%s failed: TSO req not met.\n", __func__);

	ol_flags = 0;
	/* test if outer IP checksum set for non outer IPv4 packet */
	ol_flags |= RTE_MBUF_F_TX_IPV6;
	ol_flags |= RTE_MBUF_F_TX_OUTER_IP_CKSUM;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_OUTER_IPV4_NOT_SET",
				pktmbuf_pool,
				ol_flags, 512, -EINVAL) < 0)
		GOTO_FAIL("%s failed: Outer IP cksum set.\n", __func__);

	ol_flags = 0;
	/* test to confirm outer IP checksum is set for outer IPV4 packet */
	ol_flags |= RTE_MBUF_F_TX_OUTER_IP_CKSUM;
	ol_flags |= RTE_MBUF_F_TX_OUTER_IPV4;
	if (test_mbuf_validate_tx_offload("MBUF_TEST_OUTER_IPV4_SET",
				pktmbuf_pool,
				ol_flags, 512, 0) < 0)
		GOTO_FAIL("%s failed: tx offload flag error.\n", __func__);

	ol_flags = 0;
	/* test to confirm if packets with no TX_OFFLOAD_MASK are skipped */
	if (test_mbuf_validate_tx_offload("MBUF_TEST_OL_MASK_NOT_SET",
				pktmbuf_pool,
				ol_flags, 512, 0) < 0)
		GOTO_FAIL("%s failed: tx offload flag error.\n", __func__);
	return 0;
fail:
	return -1;
}

/*
 * Test for allocating a bulk of mbufs
 * define an array with positive sizes for mbufs allocations.
 */
static int
test_pktmbuf_alloc_bulk(struct rte_mempool *pktmbuf_pool)
{
	int ret = 0;
	unsigned int idx, loop;
	unsigned int alloc_counts[] = {
		0,
		MEMPOOL_CACHE_SIZE - 1,
		MEMPOOL_CACHE_SIZE + 1,
		MEMPOOL_CACHE_SIZE * 1.5,
		MEMPOOL_CACHE_SIZE * 2,
		MEMPOOL_CACHE_SIZE * 2 - 1,
		MEMPOOL_CACHE_SIZE * 2 + 1,
		MEMPOOL_CACHE_SIZE,
	};

	/* allocate a large array of mbuf pointers */
	struct rte_mbuf *mbufs[NB_MBUF] = { 0 };
	for (idx = 0; idx < RTE_DIM(alloc_counts); idx++) {
		ret = rte_pktmbuf_alloc_bulk(pktmbuf_pool, mbufs,
				alloc_counts[idx]);
		if (ret == 0) {
			for (loop = 0; loop < alloc_counts[idx] &&
					mbufs[loop] != NULL; loop++)
				rte_pktmbuf_free(mbufs[loop]);
		} else if (ret != 0) {
			printf("%s: Bulk alloc failed count(%u); ret val(%d)\n",
					__func__, alloc_counts[idx], ret);
			return -1;
		}
	}
	return 0;
}

/*
 * Negative testing for allocating a bulk of mbufs
 */
static int
test_neg_pktmbuf_alloc_bulk(struct rte_mempool *pktmbuf_pool)
{
	int ret = 0;
	unsigned int idx, loop;
	unsigned int neg_alloc_counts[] = {
		MEMPOOL_CACHE_SIZE - NB_MBUF,
		NB_MBUF + 1,
		NB_MBUF * 8,
		UINT_MAX
	};
	struct rte_mbuf *mbufs[NB_MBUF * 8] = { 0 };

	for (idx = 0; idx < RTE_DIM(neg_alloc_counts); idx++) {
		ret = rte_pktmbuf_alloc_bulk(pktmbuf_pool, mbufs,
				neg_alloc_counts[idx]);
		if (ret == 0) {
			printf("%s: Bulk alloc must fail! count(%u); ret(%d)\n",
					__func__, neg_alloc_counts[idx], ret);
			for (loop = 0; loop < neg_alloc_counts[idx] &&
					mbufs[loop] != NULL; loop++)
				rte_pktmbuf_free(mbufs[loop]);
			return -1;
		}
	}
	return 0;
}

/*
 * Test to read mbuf packet using rte_pktmbuf_read
 */
static int
test_pktmbuf_read(struct rte_mempool *pktmbuf_pool)
{
	struct rte_mbuf *m = NULL;
	char *data = NULL;
	const char *data_copy = NULL;
	int off;

	/* alloc a mbuf */
	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("%s: mbuf allocation failed!\n", __func__);
	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("%s: Bad packet length\n", __func__);
	rte_mbuf_sanity_check(m, 0);

	data = rte_pktmbuf_append(m, MBUF_TEST_DATA_LEN2);
	if (data == NULL)
		GOTO_FAIL("%s: Cannot append data\n", __func__);
	if (rte_pktmbuf_pkt_len(m) != MBUF_TEST_DATA_LEN2)
		GOTO_FAIL("%s: Bad packet length\n", __func__);
	memset(data, 0xfe, MBUF_TEST_DATA_LEN2);

	/* read the data from mbuf */
	data_copy = rte_pktmbuf_read(m, 0, MBUF_TEST_DATA_LEN2, NULL);
	if (data_copy == NULL)
		GOTO_FAIL("%s: Error in reading data!\n", __func__);
	for (off = 0; off < MBUF_TEST_DATA_LEN2; off++) {
		if (data_copy[off] != (char)0xfe)
			GOTO_FAIL("Data corrupted at offset %u", off);
	}
	rte_pktmbuf_free(m);
	m = NULL;

	return 0;
fail:
	if (m) {
		rte_pktmbuf_free(m);
		m = NULL;
	}
	return -1;
}

/*
 * Test to read mbuf packet data from offset
 */
static int
test_pktmbuf_read_from_offset(struct rte_mempool *pktmbuf_pool)
{
	struct rte_mbuf *m = NULL;
	struct ether_hdr *hdr = NULL;
	char *data = NULL;
	const char *data_copy = NULL;
	unsigned int off;
	unsigned int hdr_len = sizeof(struct rte_ether_hdr);

	/* alloc a mbuf */
	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("%s: mbuf allocation failed!\n", __func__);

	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("%s: Bad packet length\n", __func__);
	rte_mbuf_sanity_check(m, 0);

	/* prepend an ethernet header */
	hdr = (struct ether_hdr *)rte_pktmbuf_prepend(m, hdr_len);
	if (hdr == NULL)
		GOTO_FAIL("%s: Cannot prepend header\n", __func__);
	if (rte_pktmbuf_pkt_len(m) != hdr_len)
		GOTO_FAIL("%s: Bad pkt length", __func__);
	if (rte_pktmbuf_data_len(m) != hdr_len)
		GOTO_FAIL("%s: Bad data length", __func__);
	memset(hdr, 0xde, hdr_len);

	/* read mbuf header info from 0 offset */
	data_copy = rte_pktmbuf_read(m, 0, hdr_len, NULL);
	if (data_copy == NULL)
		GOTO_FAIL("%s: Error in reading header!\n", __func__);
	for (off = 0; off < hdr_len; off++) {
		if (data_copy[off] != (char)0xde)
			GOTO_FAIL("Header info corrupted at offset %u", off);
	}

	/* append sample data after ethernet header */
	data = rte_pktmbuf_append(m, MBUF_TEST_DATA_LEN2);
	if (data == NULL)
		GOTO_FAIL("%s: Cannot append data\n", __func__);
	if (rte_pktmbuf_pkt_len(m) != hdr_len + MBUF_TEST_DATA_LEN2)
		GOTO_FAIL("%s: Bad packet length\n", __func__);
	if (rte_pktmbuf_data_len(m) != hdr_len + MBUF_TEST_DATA_LEN2)
		GOTO_FAIL("%s: Bad data length\n", __func__);
	memset(data, 0xcc, MBUF_TEST_DATA_LEN2);

	/* read mbuf data after header info */
	data_copy = rte_pktmbuf_read(m, hdr_len, MBUF_TEST_DATA_LEN2, NULL);
	if (data_copy == NULL)
		GOTO_FAIL("%s: Error in reading header data!\n", __func__);
	for (off = 0; off < MBUF_TEST_DATA_LEN2; off++) {
		if (data_copy[off] != (char)0xcc)
			GOTO_FAIL("Data corrupted at offset %u", off);
	}

	/* partial reading of mbuf data */
	data_copy = rte_pktmbuf_read(m, hdr_len + 5, MBUF_TEST_DATA_LEN2 - 5,
			NULL);
	if (data_copy == NULL)
		GOTO_FAIL("%s: Error in reading packet data!\n", __func__);
	for (off = 0; off < MBUF_TEST_DATA_LEN2 - 5; off++) {
		if (data_copy[off] != (char)0xcc)
			GOTO_FAIL("Data corrupted at offset %u", off);
	}

	/* read length greater than mbuf data_len */
	if (rte_pktmbuf_read(m, hdr_len, rte_pktmbuf_data_len(m) + 1,
				NULL) != NULL)
		GOTO_FAIL("%s: Requested len is larger than mbuf data len!\n",
				__func__);

	/* read length greater than mbuf pkt_len */
	if (rte_pktmbuf_read(m, hdr_len, rte_pktmbuf_pkt_len(m) + 1,
				NULL) != NULL)
		GOTO_FAIL("%s: Requested len is larger than mbuf pkt len!\n",
				__func__);

	/* read data of zero len from valid offset */
	data_copy = rte_pktmbuf_read(m, hdr_len, 0, NULL);
	if (data_copy == NULL)
		GOTO_FAIL("%s: Error in reading packet data!\n", __func__);
	for (off = 0; off < MBUF_TEST_DATA_LEN2; off++) {
		if (data_copy[off] != (char)0xcc)
			GOTO_FAIL("Data corrupted at offset %u", off);
	}

	/* read data of zero length from zero offset */
	data_copy = rte_pktmbuf_read(m, 0, 0, NULL);
	if (data_copy == NULL)
		GOTO_FAIL("%s: Error in reading packet data!\n", __func__);
	/* check if the received address is the beginning of header info */
	if (hdr != (const struct ether_hdr *)data_copy)
		GOTO_FAIL("%s: Corrupted data address!\n", __func__);

	/* read data of max length from valid offset */
	data_copy = rte_pktmbuf_read(m, hdr_len, UINT_MAX, NULL);
	if (data_copy == NULL)
		GOTO_FAIL("%s: Error in reading packet data!\n", __func__);
	/* check if the received address is the beginning of data segment */
	if (data_copy != data)
		GOTO_FAIL("%s: Corrupted data address!\n", __func__);

	/* try to read from mbuf with max size offset */
	data_copy = rte_pktmbuf_read(m, UINT_MAX, 0, NULL);
	if (data_copy != NULL)
		GOTO_FAIL("%s: Error in reading packet data!\n", __func__);

	/* try to read from mbuf with max size offset and len */
	data_copy = rte_pktmbuf_read(m, UINT_MAX, UINT_MAX, NULL);
	if (data_copy != NULL)
		GOTO_FAIL("%s: Error in reading packet data!\n", __func__);

	rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));

	rte_pktmbuf_free(m);
	m = NULL;

	return 0;
fail:
	if (m) {
		rte_pktmbuf_free(m);
		m = NULL;
	}
	return -1;
}

struct test_case {
	unsigned int seg_count;
	unsigned int flags;
	uint32_t read_off;
	uint32_t read_len;
	unsigned int seg_lengths[MBUF_MAX_SEG];
};

/* create a mbuf with different sized segments
 *  and fill with data [0x00 0x01 0x02 ...]
 */
static struct rte_mbuf *
create_packet(struct rte_mempool *pktmbuf_pool,
		struct test_case *test_data)
{
	uint16_t i, ret, seg, seg_len = 0;
	uint32_t last_index = 0;
	unsigned int seg_lengths[MBUF_MAX_SEG];
	unsigned int hdr_len;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf	*pkt_seg = NULL;
	char *hdr = NULL;
	char *data = NULL;

	memcpy(seg_lengths, test_data->seg_lengths,
			sizeof(unsigned int)*test_data->seg_count);
	for (seg = 0; seg < test_data->seg_count; seg++) {
		hdr_len = 0;
		seg_len =  seg_lengths[seg];
		pkt_seg = rte_pktmbuf_alloc(pktmbuf_pool);
		if (pkt_seg == NULL)
			GOTO_FAIL("%s: mbuf allocation failed!\n", __func__);
		if (rte_pktmbuf_pkt_len(pkt_seg) != 0)
			GOTO_FAIL("%s: Bad packet length\n", __func__);
		rte_mbuf_sanity_check(pkt_seg, 0);
		/* Add header only for the first segment */
		if (test_data->flags == MBUF_HEADER && seg == 0) {
			hdr_len = sizeof(struct rte_ether_hdr);
			/* prepend a header and fill with dummy data */
			hdr = (char *)rte_pktmbuf_prepend(pkt_seg, hdr_len);
			if (hdr == NULL)
				GOTO_FAIL("%s: Cannot prepend header\n",
						__func__);
			if (rte_pktmbuf_pkt_len(pkt_seg) != hdr_len)
				GOTO_FAIL("%s: Bad pkt length", __func__);
			if (rte_pktmbuf_data_len(pkt_seg) != hdr_len)
				GOTO_FAIL("%s: Bad data length", __func__);
			for (i = 0; i < hdr_len; i++)
				hdr[i] = (last_index + i) % 0xffff;
			last_index += hdr_len;
		}
		/* skip appending segment with 0 length */
		if (seg_len == 0)
			continue;
		data = rte_pktmbuf_append(pkt_seg, seg_len);
		if (data == NULL)
			GOTO_FAIL("%s: Cannot append data segment\n", __func__);
		if (rte_pktmbuf_pkt_len(pkt_seg) != hdr_len + seg_len)
			GOTO_FAIL("%s: Bad packet segment length: %d\n",
					__func__, rte_pktmbuf_pkt_len(pkt_seg));
		if (rte_pktmbuf_data_len(pkt_seg) != hdr_len + seg_len)
			GOTO_FAIL("%s: Bad data length\n", __func__);
		for (i = 0; i < seg_len; i++)
			data[i] = (last_index + i) % 0xffff;
		/* to fill continuous data from one seg to another */
		last_index += i;
		/* create chained mbufs */
		if (seg == 0)
			pkt = pkt_seg;
		else {
			ret = rte_pktmbuf_chain(pkt, pkt_seg);
			if (ret != 0)
				GOTO_FAIL("%s:FAIL: Chained mbuf creation %d\n",
						__func__, ret);
		}

		pkt_seg = pkt_seg->next;
	}
	return pkt;
fail:
	if (pkt != NULL) {
		rte_pktmbuf_free(pkt);
		pkt = NULL;
	}
	if (pkt_seg != NULL) {
		rte_pktmbuf_free(pkt_seg);
		pkt_seg = NULL;
	}
	return NULL;
}

static int
test_pktmbuf_read_from_chain(struct rte_mempool *pktmbuf_pool)
{
	struct rte_mbuf *m;
	struct test_case test_cases[] = {
		{
			.seg_lengths = { 100, 100, 100 },
			.seg_count = 3,
			.flags = MBUF_NO_HEADER,
			.read_off = 0,
			.read_len = 300
		},
		{
			.seg_lengths = { 100, 125, 150 },
			.seg_count = 3,
			.flags = MBUF_NO_HEADER,
			.read_off = 99,
			.read_len = 201
		},
		{
			.seg_lengths = { 100, 100 },
			.seg_count = 2,
			.flags = MBUF_NO_HEADER,
			.read_off = 0,
			.read_len = 100
		},
		{
			.seg_lengths = { 100, 200 },
			.seg_count = 2,
			.flags = MBUF_HEADER,
			.read_off = sizeof(struct rte_ether_hdr),
			.read_len = 150
		},
		{
			.seg_lengths = { 1000, 100 },
			.seg_count = 2,
			.flags = MBUF_NO_HEADER,
			.read_off = 0,
			.read_len = 1000
		},
		{
			.seg_lengths = { 1024, 0, 100 },
			.seg_count = 3,
			.flags = MBUF_NO_HEADER,
			.read_off = 100,
			.read_len = 1001
		},
		{
			.seg_lengths = { 1000, 1, 1000 },
			.seg_count = 3,
			.flags = MBUF_NO_HEADER,
			.read_off = 1000,
			.read_len = 2
		},
		{
			.seg_lengths = { MBUF_TEST_DATA_LEN,
					MBUF_TEST_DATA_LEN2,
					MBUF_TEST_DATA_LEN3, 800, 10 },
			.seg_count = 5,
			.flags = MBUF_NEG_TEST_READ,
			.read_off = 1000,
			.read_len = MBUF_DATA_SIZE
		},
	};

	uint32_t i, pos;
	const char *data_copy = NULL;
	char data_buf[MBUF_DATA_SIZE];

	memset(data_buf, 0, MBUF_DATA_SIZE);

	for (i = 0; i < RTE_DIM(test_cases); i++) {
		m = create_packet(pktmbuf_pool, &test_cases[i]);
		if (m == NULL)
			GOTO_FAIL("%s: mbuf allocation failed!\n", __func__);

		data_copy = rte_pktmbuf_read(m, test_cases[i].read_off,
				test_cases[i].read_len, data_buf);
		if (test_cases[i].flags == MBUF_NEG_TEST_READ) {
			if (data_copy != NULL)
				GOTO_FAIL("%s: mbuf data read should fail!\n",
						__func__);
			else {
				rte_pktmbuf_free(m);
				m = NULL;
				continue;
			}
		}
		if (data_copy == NULL)
			GOTO_FAIL("%s: Error in reading packet data!\n",
					__func__);
		for (pos = 0; pos < test_cases[i].read_len; pos++) {
			if (data_copy[pos] !=
					(char)((test_cases[i].read_off + pos)
						% 0xffff))
				GOTO_FAIL("Data corrupted at offset %u is %2X",
						pos, data_copy[pos]);
		}
		rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
		rte_pktmbuf_free(m);
		m = NULL;
	}
	return 0;

fail:
	if (m != NULL) {
		rte_pktmbuf_free(m);
		m = NULL;
	}
	return -1;
}

/* Define a free call back function to be used for external buffer */
static void
ext_buf_free_callback_fn(void *addr, void *opaque)
{
	bool *freed = opaque;

	if (addr == NULL) {
		printf("External buffer address is invalid\n");
		return;
	}
	rte_free(addr);
	*freed = true;
	printf("External buffer freed via callback\n");
}

/*
 * Test to initialize shared data in external buffer before attaching to mbuf
 *  - Allocate mbuf with no data.
 *  - Allocate external buffer with size should be large enough to accommodate
 *     rte_mbuf_ext_shared_info.
 *  - Invoke pktmbuf_ext_shinfo_init_helper to initialize shared data.
 *  - Invoke rte_pktmbuf_attach_extbuf to attach external buffer to the mbuf.
 *  - Clone another mbuf and attach the same external buffer to it.
 *  - Invoke rte_pktmbuf_detach_extbuf to detach the external buffer from mbuf.
 */
static int
test_pktmbuf_ext_shinfo_init_helper(struct rte_mempool *pktmbuf_pool)
{
	struct rte_mbuf *m = NULL;
	struct rte_mbuf *clone = NULL;
	struct rte_mbuf_ext_shared_info *ret_shinfo = NULL;
	rte_iova_t buf_iova;
	void *ext_buf_addr = NULL;
	uint16_t buf_len = EXT_BUF_TEST_DATA_LEN +
				sizeof(struct rte_mbuf_ext_shared_info);
	bool freed = false;

	/* alloc a mbuf */
	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("%s: mbuf allocation failed!\n", __func__);
	if (rte_pktmbuf_pkt_len(m) != 0)
		GOTO_FAIL("%s: Bad packet length\n", __func__);
	rte_mbuf_sanity_check(m, 0);

	ext_buf_addr = rte_malloc("External buffer", buf_len,
			RTE_CACHE_LINE_SIZE);
	if (ext_buf_addr == NULL)
		GOTO_FAIL("%s: External buffer allocation failed\n", __func__);

	ret_shinfo = rte_pktmbuf_ext_shinfo_init_helper(ext_buf_addr, &buf_len,
		ext_buf_free_callback_fn, &freed);
	if (ret_shinfo == NULL)
		GOTO_FAIL("%s: Shared info initialization failed!\n", __func__);

	if (rte_mbuf_ext_refcnt_read(ret_shinfo) != 1)
		GOTO_FAIL("%s: External refcount is not 1\n", __func__);

	if (rte_mbuf_refcnt_read(m) != 1)
		GOTO_FAIL("%s: Invalid refcnt in mbuf\n", __func__);

	buf_iova = rte_mem_virt2iova(ext_buf_addr);
	rte_pktmbuf_attach_extbuf(m, ext_buf_addr, buf_iova, buf_len,
		ret_shinfo);
	if (m->ol_flags != RTE_MBUF_F_EXTERNAL)
		GOTO_FAIL("%s: External buffer is not attached to mbuf\n",
				__func__);

	/* allocate one more mbuf */
	clone = rte_pktmbuf_clone(m, pktmbuf_pool);
	if (clone == NULL)
		GOTO_FAIL("%s: mbuf clone allocation failed!\n", __func__);
	if (rte_pktmbuf_pkt_len(clone) != 0)
		GOTO_FAIL("%s: Bad packet length\n", __func__);

	/* attach the same external buffer to the cloned mbuf */
	rte_pktmbuf_attach_extbuf(clone, ext_buf_addr, buf_iova, buf_len,
			ret_shinfo);
	if (clone->ol_flags != RTE_MBUF_F_EXTERNAL)
		GOTO_FAIL("%s: External buffer is not attached to mbuf\n",
				__func__);

	if (rte_mbuf_ext_refcnt_read(ret_shinfo) != 2)
		GOTO_FAIL("%s: Invalid ext_buf ref_cnt\n", __func__);
	if (freed)
		GOTO_FAIL("%s: extbuf should not be freed\n", __func__);

	/* test to manually update ext_buf_ref_cnt from 2 to 3*/
	rte_mbuf_ext_refcnt_update(ret_shinfo, 1);
	if (rte_mbuf_ext_refcnt_read(ret_shinfo) != 3)
		GOTO_FAIL("%s: Update ext_buf ref_cnt failed\n", __func__);
	if (freed)
		GOTO_FAIL("%s: extbuf should not be freed\n", __func__);

	/* reset the ext_refcnt before freeing the external buffer */
	rte_mbuf_ext_refcnt_set(ret_shinfo, 2);
	if (rte_mbuf_ext_refcnt_read(ret_shinfo) != 2)
		GOTO_FAIL("%s: set ext_buf ref_cnt failed\n", __func__);
	if (freed)
		GOTO_FAIL("%s: extbuf should not be freed\n", __func__);

	/* detach the external buffer from mbufs */
	rte_pktmbuf_detach_extbuf(m);
	/* check if ref cnt is decremented */
	if (rte_mbuf_ext_refcnt_read(ret_shinfo) != 1)
		GOTO_FAIL("%s: Invalid ext_buf ref_cnt\n", __func__);
	if (freed)
		GOTO_FAIL("%s: extbuf should not be freed\n", __func__);

	rte_pktmbuf_detach_extbuf(clone);
	if (!freed)
		GOTO_FAIL("%s: extbuf should be freed\n", __func__);
	freed = false;

	rte_pktmbuf_free(m);
	m = NULL;
	rte_pktmbuf_free(clone);
	clone = NULL;

	return 0;

fail:
	if (m) {
		rte_pktmbuf_free(m);
		m = NULL;
	}
	if (clone) {
		rte_pktmbuf_free(clone);
		clone = NULL;
	}
	if (ext_buf_addr != NULL) {
		rte_free(ext_buf_addr);
		ext_buf_addr = NULL;
	}
	return -1;
}

/*
 * Test the mbuf pool with pinned external data buffers
 *  - Allocate memory zone for external buffer
 *  - Create the mbuf pool with pinned external buffer
 *  - Check the created pool with relevant mbuf pool unit tests
 */
static int
test_pktmbuf_ext_pinned_buffer(struct rte_mempool *std_pool)
{

	struct rte_pktmbuf_extmem ext_mem;
	struct rte_mempool *pinned_pool = NULL;
	const struct rte_memzone *mz = NULL;

	printf("Test mbuf pool with external pinned data buffers\n");

	/* Allocate memzone for the external data buffer */
	mz = rte_memzone_reserve("pinned_pool",
				 NB_MBUF * MBUF_DATA_SIZE,
				 SOCKET_ID_ANY,
				 RTE_MEMZONE_2MB | RTE_MEMZONE_SIZE_HINT_ONLY);
	if (mz == NULL)
		GOTO_FAIL("%s: Memzone allocation failed\n", __func__);

	/* Create the mbuf pool with pinned external data buffer */
	ext_mem.buf_ptr = mz->addr;
	ext_mem.buf_iova = mz->iova;
	ext_mem.buf_len = mz->len;
	ext_mem.elt_size = MBUF_DATA_SIZE;

	pinned_pool = rte_pktmbuf_pool_create_extbuf("test_pinned_pool",
				NB_MBUF, MEMPOOL_CACHE_SIZE, 0,
				MBUF_DATA_SIZE,	SOCKET_ID_ANY,
				&ext_mem, 1);
	if (pinned_pool == NULL)
		GOTO_FAIL("%s: Mbuf pool with pinned external"
			  " buffer creation failed\n", __func__);
	/* test multiple mbuf alloc */
	if (test_pktmbuf_pool(pinned_pool) < 0)
		GOTO_FAIL("%s: test_mbuf_pool(pinned) failed\n",
			  __func__);

	/* do it another time to check that all mbufs were freed */
	if (test_pktmbuf_pool(pinned_pool) < 0)
		GOTO_FAIL("%s: test_mbuf_pool(pinned) failed (2)\n",
			  __func__);

	/* test that the data pointer on a packet mbuf is set properly */
	if (test_pktmbuf_pool_ptr(pinned_pool) < 0)
		GOTO_FAIL("%s: test_pktmbuf_pool_ptr(pinned) failed\n",
			  __func__);

	/* test data manipulation in mbuf with non-ascii data */
	if (test_pktmbuf_with_non_ascii_data(pinned_pool) < 0)
		GOTO_FAIL("%s: test_pktmbuf_with_non_ascii_data(pinned)"
			  " failed\n", __func__);

	/* test free pktmbuf segment one by one */
	if (test_pktmbuf_free_segment(pinned_pool) < 0)
		GOTO_FAIL("%s: test_pktmbuf_free_segment(pinned) failed\n",
			  __func__);

	if (testclone_testupdate_testdetach(pinned_pool, std_pool) < 0)
		GOTO_FAIL("%s: testclone_and_testupdate(pinned) failed\n",
			  __func__);

	if (test_pktmbuf_copy(pinned_pool, std_pool) < 0)
		GOTO_FAIL("%s: test_pktmbuf_copy(pinned) failed\n",
			  __func__);

	if (test_failing_mbuf_sanity_check(pinned_pool) < 0)
		GOTO_FAIL("%s: test_failing_mbuf_sanity_check(pinned)"
			  " failed\n", __func__);

	if (test_mbuf_linearize_check(pinned_pool) < 0)
		GOTO_FAIL("%s: test_mbuf_linearize_check(pinned) failed\n",
			  __func__);

	/* test for allocating a bulk of mbufs with various sizes */
	if (test_pktmbuf_alloc_bulk(pinned_pool) < 0)
		GOTO_FAIL("%s: test_rte_pktmbuf_alloc_bulk(pinned) failed\n",
			  __func__);

	/* test for allocating a bulk of mbufs with various sizes */
	if (test_neg_pktmbuf_alloc_bulk(pinned_pool) < 0)
		GOTO_FAIL("%s: test_neg_rte_pktmbuf_alloc_bulk(pinned)"
			  " failed\n", __func__);

	/* test to read mbuf packet */
	if (test_pktmbuf_read(pinned_pool) < 0)
		GOTO_FAIL("%s: test_rte_pktmbuf_read(pinned) failed\n",
			  __func__);

	/* test to read mbuf packet from offset */
	if (test_pktmbuf_read_from_offset(pinned_pool) < 0)
		GOTO_FAIL("%s: test_rte_pktmbuf_read_from_offset(pinned)"
			  " failed\n", __func__);

	/* test to read data from chain of mbufs with data segments */
	if (test_pktmbuf_read_from_chain(pinned_pool) < 0)
		GOTO_FAIL("%s: test_rte_pktmbuf_read_from_chain(pinned)"
			  " failed\n", __func__);

	RTE_SET_USED(std_pool);
	rte_mempool_free(pinned_pool);
	rte_memzone_free(mz);
	return 0;

fail:
	rte_mempool_free(pinned_pool);
	rte_memzone_free(mz);
	return -1;
}

static int
test_mbuf_dyn(struct rte_mempool *pktmbuf_pool)
{
	const struct rte_mbuf_dynfield dynfield = {
		.name = "test-dynfield",
		.size = sizeof(uint8_t),
		.align = __alignof__(uint8_t),
		.flags = 0,
	};
	const struct rte_mbuf_dynfield dynfield2 = {
		.name = "test-dynfield2",
		.size = sizeof(uint16_t),
		.align = __alignof__(uint16_t),
		.flags = 0,
	};
	const struct rte_mbuf_dynfield dynfield3 = {
		.name = "test-dynfield3",
		.size = sizeof(uint8_t),
		.align = __alignof__(uint8_t),
		.flags = 0,
	};
	const struct rte_mbuf_dynfield dynfield_fail_big = {
		.name = "test-dynfield-fail-big",
		.size = 256,
		.align = 1,
		.flags = 0,
	};
	const struct rte_mbuf_dynfield dynfield_fail_align = {
		.name = "test-dynfield-fail-align",
		.size = 1,
		.align = 3,
		.flags = 0,
	};
	const struct rte_mbuf_dynfield dynfield_fail_flag = {
		.name = "test-dynfield",
		.size = sizeof(uint8_t),
		.align = __alignof__(uint8_t),
		.flags = 1,
	};
	const struct rte_mbuf_dynflag dynflag_fail_flag = {
		.name = "test-dynflag",
		.flags = 1,
	};
	const struct rte_mbuf_dynflag dynflag = {
		.name = "test-dynflag",
		.flags = 0,
	};
	const struct rte_mbuf_dynflag dynflag2 = {
		.name = "test-dynflag2",
		.flags = 0,
	};
	const struct rte_mbuf_dynflag dynflag3 = {
		.name = "test-dynflag3",
		.flags = 0,
	};
	struct rte_mbuf *m = NULL;
	int offset, offset2, offset3;
	int flag, flag2, flag3;
	int ret;

	printf("Test mbuf dynamic fields and flags\n");
	rte_mbuf_dyn_dump(stdout);

	offset = rte_mbuf_dynfield_register(&dynfield);
	if (offset == -1)
		GOTO_FAIL("failed to register dynamic field, offset=%d: %s",
			offset, strerror(errno));

	ret = rte_mbuf_dynfield_register(&dynfield);
	if (ret != offset)
		GOTO_FAIL("failed to lookup dynamic field, ret=%d: %s",
			ret, strerror(errno));

	offset2 = rte_mbuf_dynfield_register(&dynfield2);
	if (offset2 == -1 || offset2 == offset || (offset2 & 1))
		GOTO_FAIL("failed to register dynamic field 2, offset2=%d: %s",
			offset2, strerror(errno));

	offset3 = rte_mbuf_dynfield_register_offset(&dynfield3,
				offsetof(struct rte_mbuf, dynfield1[1]));
	if (offset3 != offsetof(struct rte_mbuf, dynfield1[1])) {
		if (rte_errno == EBUSY)
			printf("mbuf test error skipped: dynfield is busy\n");
		else
			GOTO_FAIL("failed to register dynamic field 3, offset="
				"%d: %s", offset3, strerror(errno));
	}

	printf("dynfield: offset=%d, offset2=%d, offset3=%d\n",
		offset, offset2, offset3);

	ret = rte_mbuf_dynfield_register(&dynfield_fail_big);
	if (ret != -1)
		GOTO_FAIL("dynamic field creation should fail (too big)");

	ret = rte_mbuf_dynfield_register(&dynfield_fail_align);
	if (ret != -1)
		GOTO_FAIL("dynamic field creation should fail (bad alignment)");

	ret = rte_mbuf_dynfield_register_offset(&dynfield_fail_align,
				offsetof(struct rte_mbuf, ol_flags));
	if (ret != -1)
		GOTO_FAIL("dynamic field creation should fail (not avail)");

	ret = rte_mbuf_dynfield_register(&dynfield_fail_flag);
	if (ret != -1)
		GOTO_FAIL("dynamic field creation should fail (invalid flag)");

	ret = rte_mbuf_dynflag_register(&dynflag_fail_flag);
	if (ret != -1)
		GOTO_FAIL("dynamic flag creation should fail (invalid flag)");

	flag = rte_mbuf_dynflag_register(&dynflag);
	if (flag == -1)
		GOTO_FAIL("failed to register dynamic flag, flag=%d: %s",
			flag, strerror(errno));

	ret = rte_mbuf_dynflag_register(&dynflag);
	if (ret != flag)
		GOTO_FAIL("failed to lookup dynamic flag, ret=%d: %s",
			ret, strerror(errno));

	flag2 = rte_mbuf_dynflag_register(&dynflag2);
	if (flag2 == -1 || flag2 == flag)
		GOTO_FAIL("failed to register dynamic flag 2, flag2=%d: %s",
			flag2, strerror(errno));

	flag3 = rte_mbuf_dynflag_register_bitnum(&dynflag3,
						rte_bsf64(RTE_MBUF_F_LAST_FREE));
	if ((uint32_t)flag3 != rte_bsf64(RTE_MBUF_F_LAST_FREE))
		GOTO_FAIL("failed to register dynamic flag 3, flag3=%d: %s",
			flag3, strerror(errno));

	printf("dynflag: flag=%d, flag2=%d, flag3=%d\n", flag, flag2, flag3);

	/* set, get dynamic field */
	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("Cannot allocate mbuf");

	*RTE_MBUF_DYNFIELD(m, offset, uint8_t *) = 1;
	if (*RTE_MBUF_DYNFIELD(m, offset, uint8_t *) != 1)
		GOTO_FAIL("failed to read dynamic field");
	*RTE_MBUF_DYNFIELD(m, offset2, uint16_t *) = 1000;
	if (*RTE_MBUF_DYNFIELD(m, offset2, uint16_t *) != 1000)
		GOTO_FAIL("failed to read dynamic field");

	/* set a dynamic flag */
	m->ol_flags |= (1ULL << flag);

	rte_mbuf_dyn_dump(stdout);
	rte_pktmbuf_free(m);
	return 0;
fail:
	rte_pktmbuf_free(m);
	return -1;
}

/* check that m->nb_segs and m->next are reset on mbuf free */
static int
test_nb_segs_and_next_reset(void)
{
	struct rte_mbuf *m0 = NULL, *m1 = NULL, *m2 = NULL;
	struct rte_mempool *pool = NULL;

	pool = rte_pktmbuf_pool_create("test_mbuf_reset",
			3, 0, 0, MBUF_DATA_SIZE, SOCKET_ID_ANY);
	if (pool == NULL)
		GOTO_FAIL("Failed to create mbuf pool");

	/* alloc mbufs */
	m0 = rte_pktmbuf_alloc(pool);
	m1 = rte_pktmbuf_alloc(pool);
	m2 = rte_pktmbuf_alloc(pool);
	if (m0 == NULL || m1 == NULL || m2 == NULL)
		GOTO_FAIL("Failed to allocate mbuf");

	/* append data in all of them */
	if (rte_pktmbuf_append(m0, 500) == NULL ||
			rte_pktmbuf_append(m1, 500) == NULL ||
			rte_pktmbuf_append(m2, 500) == NULL)
		GOTO_FAIL("Failed to append data in mbuf");

	/* chain them in one mbuf m0 */
	rte_pktmbuf_chain(m1, m2);
	rte_pktmbuf_chain(m0, m1);
	if (m0->nb_segs != 3 || m0->next != m1 || m1->next != m2 ||
			m2->next != NULL) {
		m1 = m2 = NULL;
		GOTO_FAIL("Failed to chain mbufs");
	}

	/* split m0 chain in two, between m1 and m2 */
	m0->nb_segs = 2;
	m0->pkt_len -= m2->data_len;
	m1->next = NULL;
	m2->nb_segs = 1;

	/* free the 2 mbuf chains m0 and m2  */
	rte_pktmbuf_free(m0);
	rte_pktmbuf_free(m2);

	/* realloc the 3 mbufs */
	m0 = rte_mbuf_raw_alloc(pool);
	m1 = rte_mbuf_raw_alloc(pool);
	m2 = rte_mbuf_raw_alloc(pool);
	if (m0 == NULL || m1 == NULL || m2 == NULL)
		GOTO_FAIL("Failed to reallocate mbuf");

	/* ensure that m->next and m->nb_segs are reset allocated mbufs */
	if (m0->nb_segs != 1 || m0->next != NULL ||
			m1->nb_segs != 1 || m1->next != NULL ||
			m2->nb_segs != 1 || m2->next != NULL)
		GOTO_FAIL("nb_segs or next was not reset properly");

	rte_mempool_free(pool);
	return 0;

fail:
	rte_mempool_free(pool);
	return -1;
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
			NB_MBUF, MEMPOOL_CACHE_SIZE, 0, MBUF_DATA_SIZE,
			SOCKET_ID_ANY);

	if (pktmbuf_pool == NULL) {
		printf("cannot allocate mbuf pool\n");
		goto err;
	}

	/* test registration of dynamic fields and flags */
	if (test_mbuf_dyn(pktmbuf_pool) < 0) {
		printf("mbuf dynflag test failed\n");
		goto err;
	}

	/* create a specific pktmbuf pool with a priv_size != 0 and no data
	 * room size */
	pktmbuf_pool2 = rte_pktmbuf_pool_create("test_pktmbuf_pool2",
			NB_MBUF, MEMPOOL_CACHE_SIZE, MBUF2_PRIV_SIZE, 0,
			SOCKET_ID_ANY);

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

	/* test bulk mbuf alloc and free */
	if (test_pktmbuf_pool_bulk() < 0) {
		printf("test_pktmbuf_pool_bulk() failed\n");
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

	if (testclone_testupdate_testdetach(pktmbuf_pool, pktmbuf_pool) < 0) {
		printf("testclone_and_testupdate() failed \n");
		goto err;
	}

	if (test_pktmbuf_copy(pktmbuf_pool, pktmbuf_pool) < 0) {
		printf("test_pktmbuf_copy() failed\n");
		goto err;
	}

	if (test_attach_from_different_pool(pktmbuf_pool, pktmbuf_pool2) < 0) {
		printf("test_attach_from_different_pool() failed\n");
		goto err;
	}

	if (test_refcnt_mbuf() < 0) {
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

	if (test_tx_offload() < 0) {
		printf("test_tx_offload() failed\n");
		goto err;
	}

	if (test_get_rx_ol_flag_list() < 0) {
		printf("test_rte_get_rx_ol_flag_list() failed\n");
		goto err;
	}

	if (test_get_tx_ol_flag_list() < 0) {
		printf("test_rte_get_tx_ol_flag_list() failed\n");
		goto err;
	}

	if (test_get_rx_ol_flag_name() < 0) {
		printf("test_rte_get_rx_ol_flag_name() failed\n");
		goto err;
	}

	if (test_get_tx_ol_flag_name() < 0) {
		printf("test_rte_get_tx_ol_flag_name() failed\n");
		goto err;
	}

	if (test_mbuf_validate_tx_offload_one(pktmbuf_pool) < 0) {
		printf("test_mbuf_validate_tx_offload_one() failed\n");
		goto err;
	}

	/* test for allocating a bulk of mbufs with various sizes */
	if (test_pktmbuf_alloc_bulk(pktmbuf_pool) < 0) {
		printf("test_rte_pktmbuf_alloc_bulk() failed\n");
		goto err;
	}

	/* test for allocating a bulk of mbufs with various sizes */
	if (test_neg_pktmbuf_alloc_bulk(pktmbuf_pool) < 0) {
		printf("test_neg_rte_pktmbuf_alloc_bulk() failed\n");
		goto err;
	}

	/* test to read mbuf packet */
	if (test_pktmbuf_read(pktmbuf_pool) < 0) {
		printf("test_rte_pktmbuf_read() failed\n");
		goto err;
	}

	/* test to read mbuf packet from offset */
	if (test_pktmbuf_read_from_offset(pktmbuf_pool) < 0) {
		printf("test_rte_pktmbuf_read_from_offset() failed\n");
		goto err;
	}

	/* test to read data from chain of mbufs with data segments */
	if (test_pktmbuf_read_from_chain(pktmbuf_pool) < 0) {
		printf("test_rte_pktmbuf_read_from_chain() failed\n");
		goto err;
	}

	/* test to initialize shared info. at the end of external buffer */
	if (test_pktmbuf_ext_shinfo_init_helper(pktmbuf_pool) < 0) {
		printf("test_pktmbuf_ext_shinfo_init_helper() failed\n");
		goto err;
	}

	/* test the mbuf pool with pinned external data buffers */
	if (test_pktmbuf_ext_pinned_buffer(pktmbuf_pool) < 0) {
		printf("test_pktmbuf_ext_pinned_buffer() failed\n");
		goto err;
	}

	/* test reset of m->nb_segs and m->next on mbuf free */
	if (test_nb_segs_and_next_reset() < 0) {
		printf("test_nb_segs_and_next_reset() failed\n");
		goto err;
	}

	ret = 0;
err:
	rte_mempool_free(pktmbuf_pool);
	rte_mempool_free(pktmbuf_pool2);
	return ret;
}
#undef GOTO_FAIL

REGISTER_TEST_COMMAND(mbuf_autotest, test_mbuf);
