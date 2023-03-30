/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "test.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_mbuf.h>
#include <rte_reorder.h>
#include <rte_lcore.h>
#include <rte_malloc.h>

#define BURST 32
#define REORDER_BUFFER_SIZE 16384
#define NUM_MBUFS (2*REORDER_BUFFER_SIZE)
#define REORDER_BUFFER_SIZE_INVALID 2049

struct reorder_unittest_params {
	struct rte_mempool *p;
	struct rte_reorder_buffer *b;
};

static struct reorder_unittest_params default_params  = {
	.p = NULL,
	.b = NULL
};

static struct reorder_unittest_params *test_params = &default_params;

static int
test_reorder_create(void)
{
	struct rte_reorder_buffer *b = NULL;

	b = rte_reorder_create(NULL, rte_socket_id(), REORDER_BUFFER_SIZE);
	TEST_ASSERT((b == NULL) && (rte_errno == EINVAL),
			"No error on create() with NULL name");

	b = rte_reorder_create("PKT", rte_socket_id(), REORDER_BUFFER_SIZE_INVALID);
	TEST_ASSERT((b == NULL) && (rte_errno == EINVAL),
			"No error on create() with invalid buffer size param.");

	b = rte_reorder_create("PKT_RO1", rte_socket_id(), REORDER_BUFFER_SIZE);
	TEST_ASSERT_EQUAL(b, test_params->b,
			"New reorder instance created with already existing name");

	return 0;
}

static int
test_reorder_init(void)
{
	struct rte_reorder_buffer *b = NULL;
	unsigned int size;
	/*
	 * The minimum memory area size that should be passed to library is,
	 * sizeof(struct rte_reorder_buffer) + (2 * size * sizeof(struct rte_mbuf *));
	 * Otherwise error will be thrown
	 */

	size = 100;
	b = rte_reorder_init(b, size, "PKT1", REORDER_BUFFER_SIZE);
	TEST_ASSERT((b == NULL) && (rte_errno == EINVAL),
			"No error on init with NULL buffer.");

	b = rte_malloc(NULL, size, 0);
	b = rte_reorder_init(b, size, "PKT1", REORDER_BUFFER_SIZE);
	TEST_ASSERT((b == NULL) && (rte_errno == EINVAL),
			"No error on init with invalid mem zone size.");
	rte_free(b);

	size = 262336;
	b = rte_malloc(NULL, size, 0);
	b = rte_reorder_init(b, size, "PKT1", REORDER_BUFFER_SIZE_INVALID);
	TEST_ASSERT((b == NULL) && (rte_errno == EINVAL),
			"No error on init with invalid buffer size param.");

	b = rte_reorder_init(b, size, NULL, REORDER_BUFFER_SIZE);
	TEST_ASSERT((b == NULL) && (rte_errno == EINVAL),
			"No error on init with invalid name.");
	rte_free(b);

	return 0;
}

static int
test_reorder_find_existing(void)
{
	struct rte_reorder_buffer *b = NULL;

	/* Try to find existing reorder buffer instance */
	b = rte_reorder_find_existing("PKT_RO1");
	TEST_ASSERT_EQUAL(b, test_params->b,
			"existing reorder buffer instance not found");

	/* Try to find non existing reorder buffer instance */
	b = rte_reorder_find_existing("ro_find_non_existing");
	TEST_ASSERT((b == NULL) && (rte_errno == ENOENT),
			"non existing reorder buffer instance found");

	return 0;
}

static int
test_reorder_free(void)
{
	struct rte_reorder_buffer *b1 = NULL, *b2 = NULL;
	const char *name = "test_free";

	b1 = rte_reorder_create(name, rte_socket_id(), 8);
	TEST_ASSERT_NOT_NULL(b1, "Failed to create reorder buffer.");

	b2 = rte_reorder_find_existing(name);
	TEST_ASSERT_EQUAL(b1, b2, "Failed to find existing reorder buffer");

	rte_reorder_free(b1);

	b2 = rte_reorder_find_existing(name);
	TEST_ASSERT((b2 == NULL) && (rte_errno == ENOENT),
			"Found previously freed reorder buffer");

	return 0;
}

static int
test_reorder_insert(void)
{
	struct rte_reorder_buffer *b = NULL;
	struct rte_mempool *p = test_params->p;
	const unsigned int size = 4;
	const unsigned int num_bufs = 7;
	struct rte_mbuf *bufs[num_bufs];
	int ret = 0;
	unsigned i;

	/* This would create a reorder buffer instance consisting of:
	 * reorder_seq = 0
	 * ready_buf: RB[size] = {NULL, NULL, NULL, NULL}
	 * order_buf: OB[size] = {NULL, NULL, NULL, NULL}
	 */
	b = rte_reorder_create("test_insert", rte_socket_id(), size);
	TEST_ASSERT_NOT_NULL(b, "Failed to create reorder buffer");

	for (i = 0; i < num_bufs; i++) {
		bufs[i] = rte_pktmbuf_alloc(p);
		TEST_ASSERT_NOT_NULL(bufs[i], "Packet allocation failed\n");
		*rte_reorder_seqn(bufs[i]) = i;
	}

	/* This should fill up order buffer:
	 * reorder_seq = 0
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {0, 1, 2, 3}
	 */
	for (i = 0; i < size; i++) {
		ret = rte_reorder_insert(b, bufs[i]);
		if (ret != 0) {
			printf("%s:%d: Error inserting packet with seqn less than size\n",
					__func__, __LINE__);
			ret = -1;
			goto exit;
		}
		bufs[i] = NULL;
	}

	/* early packet - should move mbufs to ready buf and move sequence window
	 * reorder_seq = 4
	 * RB[] = {0, 1, 2, 3}
	 * OB[] = {4, NULL, NULL, NULL}
	 */
	ret = rte_reorder_insert(b, bufs[4]);
	if (ret != 0) {
		printf("%s:%d: Error inserting early packet with seqn: size\n",
				__func__, __LINE__);
		ret = -1;
		goto exit;
	}
	bufs[4] = NULL;

	/* early packet from current sequence window - full ready buffer */
	*rte_reorder_seqn(bufs[5]) = 2 * size;
	ret = rte_reorder_insert(b, bufs[5]);
	if (!((ret == -1) && (rte_errno == ENOSPC))) {
		printf("%s:%d: No error inserting early packet with full ready buffer\n",
				__func__, __LINE__);
		ret = -1;
		goto exit;
	}
	bufs[5] = NULL;

	/* late packet */
	*rte_reorder_seqn(bufs[6]) = 3 * size;
	ret = rte_reorder_insert(b, bufs[6]);
	if (!((ret == -1) && (rte_errno == ERANGE))) {
		printf("%s:%d: No error inserting late packet with seqn:"
				" 3 * size\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}
	bufs[6] = NULL;

	ret = 0;
exit:
	rte_reorder_free(b);
	for (i = 0; i < num_bufs; i++) {
		rte_pktmbuf_free(bufs[i]);
	}
	return ret;
}

static int
test_reorder_drain(void)
{
	struct rte_reorder_buffer *b = NULL;
	struct rte_mempool *p = test_params->p;
	const unsigned int size = 4;
	const unsigned int num_bufs = 8;
	struct rte_mbuf *bufs[num_bufs];
	struct rte_mbuf *robufs[num_bufs];
	int ret = 0;
	unsigned i, cnt;

	/* initialize all robufs to NULL */
	for (i = 0; i < num_bufs; i++)
		robufs[i] = NULL;

	/* This would create a reorder buffer instance consisting of:
	 * reorder_seq = 0
	 * ready_buf: RB[size] = {NULL, NULL, NULL, NULL}
	 * order_buf: OB[size] = {NULL, NULL, NULL, NULL}
	 */
	b = rte_reorder_create("test_drain", rte_socket_id(), size);
	TEST_ASSERT_NOT_NULL(b, "Failed to create reorder buffer");

	/* Check no drained packets if reorder is empty */
	cnt = rte_reorder_drain(b, robufs, 1);
	if (cnt != 0) {
		printf("%s:%d: drained packets from empty reorder buffer\n",
				__func__, __LINE__);
		ret = -1;
		goto exit;
	}

	for (i = 0; i < num_bufs; i++) {
		bufs[i] = rte_pktmbuf_alloc(p);
		TEST_ASSERT_NOT_NULL(bufs[i], "Packet allocation failed\n");
		*rte_reorder_seqn(bufs[i]) = i;
	}

	/* Insert packet with seqn 1:
	 * reorder_seq = 0
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {1, NULL, NULL, NULL}
	 */
	rte_reorder_insert(b, bufs[1]);
	bufs[1] = NULL;

	cnt = rte_reorder_drain(b, robufs, 1);
	if (cnt != 1) {
		printf("%s:%d:%d: number of expected packets not drained\n",
				__func__, __LINE__, cnt);
		ret = -1;
		goto exit;
	}
	rte_pktmbuf_free(robufs[0]);
	memset(robufs, 0, sizeof(robufs));

	/* Insert more packets
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {NULL, 2, 3, NULL}
	 */
	rte_reorder_insert(b, bufs[2]);
	rte_reorder_insert(b, bufs[3]);
	bufs[2] = NULL;
	bufs[3] = NULL;

	/* Insert more packets
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {NULL, 2, 3, 4}
	 */
	rte_reorder_insert(b, bufs[4]);
	bufs[4] = NULL;

	/* Insert more packets
	 * RB[] = {2, 3, 4, NULL}
	 * OB[] = {NULL, NULL, 7, NULL}
	 */
	rte_reorder_insert(b, bufs[7]);
	bufs[7] = NULL;

	/* drained expected packets */
	cnt = rte_reorder_drain(b, robufs, 4);
	if (cnt != 3) {
		printf("%s:%d:%d: number of expected packets not drained\n",
				__func__, __LINE__, cnt);
		ret = -1;
		goto exit;
	}
	for (i = 0; i < 3; i++) {
		rte_pktmbuf_free(robufs[i]);
	}
	memset(robufs, 0, sizeof(robufs));

	/*
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {NULL, NULL, 7, NULL}
	 */
	cnt = rte_reorder_drain(b, robufs, 1);
	if (cnt != 0) {
		printf("%s:%d:%d: number of expected packets not drained\n",
				__func__, __LINE__, cnt);
		ret = -1;
		goto exit;
	}
	ret = 0;
exit:
	rte_reorder_free(b);
	for (i = 0; i < num_bufs; i++) {
		rte_pktmbuf_free(bufs[i]);
		rte_pktmbuf_free(robufs[i]);
	}
	return ret;
}

static void
buffer_to_reorder_move(struct rte_mbuf **mbuf, struct rte_reorder_buffer *b)
{
	rte_reorder_insert(b, *mbuf);
	*mbuf = NULL;
}

static int
test_reorder_drain_up_to_seqn(void)
{
	struct rte_mempool *p = test_params->p;
	struct rte_reorder_buffer *b = NULL;
	const unsigned int num_bufs = 10;
	const unsigned int size = 4;
	unsigned int i, cnt;
	int ret = 0;

	struct rte_mbuf *bufs[num_bufs];
	struct rte_mbuf *robufs[num_bufs];

	/* initialize all robufs to NULL */
	memset(robufs, 0, sizeof(robufs));

	/* This would create a reorder buffer instance consisting of:
	 * reorder_seq = 0
	 * ready_buf: RB[size] = {NULL, NULL, NULL, NULL}
	 * order_buf: OB[size] = {NULL, NULL, NULL, NULL}
	 */
	b = rte_reorder_create("test_drain_up_to_seqn", rte_socket_id(), size);
	TEST_ASSERT_NOT_NULL(b, "Failed to create reorder buffer");

	for (i = 0; i < num_bufs; i++) {
		bufs[i] = rte_pktmbuf_alloc(p);
		TEST_ASSERT_NOT_NULL(bufs[i], "Packet allocation failed\n");
		*rte_reorder_seqn(bufs[i]) = i;
	}

	/* Insert packet with seqn 1 and 3:
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {1, 2, 3, NULL}
	 */
	buffer_to_reorder_move(&bufs[1], b);
	buffer_to_reorder_move(&bufs[2], b);
	buffer_to_reorder_move(&bufs[3], b);
	/* Draining 1, 2 */
	cnt = rte_reorder_drain_up_to_seqn(b, robufs, num_bufs, 3);
	if (cnt != 2) {
		printf("%s:%d:%d: number of expected packets not drained\n",
				__func__, __LINE__, cnt);
		ret = -1;
		goto exit;
	}
	for (i = 0; i < 2; i++)
		rte_pktmbuf_free(robufs[i]);
	memset(robufs, 0, sizeof(robufs));

	/* Insert more packets
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {3, 4, NULL, 6}
	 */
	buffer_to_reorder_move(&bufs[4], b);
	buffer_to_reorder_move(&bufs[6], b);
	/* Insert more packets to utilize Ready buffer
	 * RB[] = {3, NULL, 5, 6}
	 * OB[] = {NULL, NULL, 8, NULL}
	 */
	buffer_to_reorder_move(&bufs[8], b);

	/* Drain 3 and 5 */
	cnt = rte_reorder_drain_up_to_seqn(b, robufs, num_bufs, 6);
	if (cnt != 2) {
		printf("%s:%d:%d: number of expected packets not drained\n",
				__func__, __LINE__, cnt);
		ret = -1;
		goto exit;
	}
	for (i = 0; i < 2; i++)
		rte_pktmbuf_free(robufs[i]);
	memset(robufs, 0, sizeof(robufs));

	ret = 0;
exit:
	rte_reorder_free(b);
	for (i = 0; i < num_bufs; i++) {
		rte_pktmbuf_free(bufs[i]);
		rte_pktmbuf_free(robufs[i]);
	}
	return ret;
}

static int
test_reorder_set_seqn(void)
{
	struct rte_mempool *p = test_params->p;
	struct rte_reorder_buffer *b = NULL;
	const unsigned int num_bufs = 7;
	const unsigned int size = 4;
	unsigned int i;
	int ret = 0;

	struct rte_mbuf *bufs[num_bufs];

	/* This would create a reorder buffer instance consisting of:
	 * reorder_seq = 0
	 * ready_buf: RB[size] = {NULL, NULL, NULL, NULL}
	 * order_buf: OB[size] = {NULL, NULL, NULL, NULL}
	 */
	b = rte_reorder_create("test_min_seqn_set", rte_socket_id(), size);
	TEST_ASSERT_NOT_NULL(b, "Failed to create reorder buffer");

	for (i = 0; i < num_bufs; i++) {
		bufs[i] = rte_pktmbuf_alloc(p);
		if (bufs[i] == NULL) {
			printf("Packet allocation failed\n");
			goto exit;
		}
		*rte_reorder_seqn(bufs[i]) = i;
	}

	ret = rte_reorder_min_seqn_set(b, 5);
	if (ret != 0) {
		printf("%s:%d: Error in setting min sequence number\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

	ret = rte_reorder_insert(b, bufs[0]);
	if (ret >= 0) {
		printf("%s:%d: Insertion with value less the min seq number\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

	ret = rte_reorder_insert(b, bufs[5]);
	if (ret != 0) {
		printf("%s:%d: Error inserting packet with valid seqn\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}
	bufs[5] = NULL;

	ret = rte_reorder_min_seqn_set(b, 0);
	if (ret >= 0) {
		printf("%s:%d: Error in setting min sequence number with non-empty buffer\n",
				__func__, __LINE__);
		ret = -1;
		goto exit;
	}

	ret = 0;
exit:
	rte_reorder_free(b);
	for (i = 0; i < num_bufs; i++)
		rte_pktmbuf_free(bufs[i]);

	return ret;
}

static int
test_setup(void)
{
	/* reorder buffer instance creation */
	if (test_params->b == NULL) {
		test_params->b = rte_reorder_create("PKT_RO1", rte_socket_id(),
							REORDER_BUFFER_SIZE);
		if (test_params->b == NULL) {
			printf("%s: Error creating reorder buffer instance b\n",
					__func__);
			return -1;
		}
	} else
		rte_reorder_reset(test_params->b);

	/* mempool creation */
	if (test_params->p == NULL) {
		test_params->p = rte_pktmbuf_pool_create("RO_MBUF_POOL",
			NUM_MBUFS, BURST, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
			rte_socket_id());
		if (test_params->p == NULL) {
			printf("%s: Error creating mempool\n", __func__);
			return -1;
		}
	}
	return 0;
}

static void
test_teardown(void)
{
	rte_reorder_free(test_params->b);
	test_params->b = NULL;
	rte_mempool_free(test_params->p);
	test_params->p = NULL;
}


static struct unit_test_suite reorder_test_suite  = {

	.setup = test_setup,
	.teardown = test_teardown,
	.suite_name = "Reorder Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE(test_reorder_create),
		TEST_CASE(test_reorder_init),
		TEST_CASE(test_reorder_find_existing),
		TEST_CASE(test_reorder_free),
		TEST_CASE(test_reorder_insert),
		TEST_CASE(test_reorder_drain),
		TEST_CASE(test_reorder_drain_up_to_seqn),
		TEST_CASE(test_reorder_set_seqn),
		TEST_CASES_END()
	}
};

static int
test_reorder(void)
{
	return unit_test_suite_runner(&reorder_test_suite);
}


REGISTER_TEST_COMMAND(reorder_autotest, test_reorder);
