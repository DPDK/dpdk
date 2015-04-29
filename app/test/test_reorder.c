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
#include "stdio.h"

#include <unistd.h>
#include <string.h>

#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_mbuf.h>
#include <rte_reorder.h>
#include <rte_lcore.h>
#include <rte_malloc.h>

#include "test.h"

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
	printf("DEBUG: b= %p, orig_b= %p\n", b, test_params->b);
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
	const unsigned int num_bufs = 6;
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

	ret = rte_mempool_get_bulk(p, (void *)bufs, num_bufs);
	TEST_ASSERT_SUCCESS(ret, "Error getting mbuf from pool");

	/* late packet */
	bufs[0]->seqn = 3 * size;
	ret = rte_reorder_insert(b, bufs[0]);
	if (!((ret == -1) && (rte_errno == ERANGE))) {
		printf("%s:%d: No error inserting late packet with seqn:"
				" 3 * size\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

	for (i = 0; i < num_bufs; i++)
		bufs[i]->seqn = i;

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

	/* early packet from current sequence window - full ready buffer */
	bufs[5]->seqn = 2 * size;
	ret = rte_reorder_insert(b, bufs[5]);
	if (!((ret == -1) && (rte_errno == ENOSPC))) {
		printf("%s:%d: No error inserting early packet with full ready buffer\n",
				__func__, __LINE__);
		ret = -1;
		goto exit;
	}

	ret = 0;
exit:
	rte_mempool_put_bulk(p, (void *)bufs, num_bufs);
	rte_reorder_free(b);
	return ret;
}

static int
test_reorder_drain(void)
{
	struct rte_reorder_buffer *b = NULL;
	struct rte_mempool *p = test_params->p;
	const unsigned int size = 4;
	const unsigned int num_bufs = 10;
	struct rte_mbuf *bufs[num_bufs];
	int ret = 0;
	unsigned i, cnt;

	/* This would create a reorder buffer instance consisting of:
	 * reorder_seq = 0
	 * ready_buf: RB[size] = {NULL, NULL, NULL, NULL}
	 * order_buf: OB[size] = {NULL, NULL, NULL, NULL}
	 */
	b = rte_reorder_create("test_insert", rte_socket_id(), size);
	TEST_ASSERT_NOT_NULL(b, "Failed to create reorder buffer");

	ret = rte_mempool_get_bulk(p, (void *)bufs, num_bufs);
	TEST_ASSERT_SUCCESS(ret, "Error getting mbuf from pool");

	/* Check no drained packets if reorder is empty */
	cnt = rte_reorder_drain(b, bufs, 1);
	if (cnt != 0) {
		printf("%s:%d: drained packets from empty reorder buffer\n",
				__func__, __LINE__);
		ret = -1;
		goto exit;
	}

	for (i = 0; i < num_bufs; i++)
		bufs[i]->seqn = i;

	/* Insert packet with seqn 1:
	 * reorder_seq = 0
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {NULL, 1, NULL, NULL}
	 */
	rte_reorder_insert(b, bufs[1]);

	/* Check no drained packets if no ready/order packets */
	cnt = rte_reorder_drain(b, bufs, 1);
	if (cnt != 0) {
		printf("%s:%d: drained packets from empty reorder buffer\n",
				__func__, __LINE__);
		ret = -1;
		goto exit;
	}

	/* Insert more packets
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {0, 1, NULL, 3}
	 */
	rte_reorder_insert(b, bufs[0]);
	rte_reorder_insert(b, bufs[3]);

	/* drained expected packets */
	cnt = rte_reorder_drain(b, bufs, 4);
	if (cnt != 2) {
		printf("%s:%d:%d: number of expected packets not drained\n",
				__func__, __LINE__, cnt);
		ret = -1;
		goto exit;
	}

	/*
	 * RB[] = {NULL, NULL, NULL, NULL}
	 * OB[] = {NULL, 3, NULL, NULL}
	 */

	rte_reorder_insert(b, bufs[4]);
	rte_reorder_insert(b, bufs[7]);

	/*
	 * RB[] = {3, 4, NULL, NULL}
	 * OB[] = {NULL, NULL, 7, NULL}
	 */

	cnt = rte_reorder_drain(b, bufs, 4);
	if (cnt != 2) {
		printf("%s:%d:%d: number of expected packets not drained\n",
				__func__, __LINE__, cnt);
		ret = -1;
		goto exit;
	}

	ret = 0;
exit:
	rte_mempool_put_bulk(p, (void *)bufs, num_bufs);
	rte_reorder_free(b);
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

static struct unit_test_suite reorder_test_suite  = {

	.setup = test_setup,
	.suite_name = "Reorder Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE(test_reorder_create),
		TEST_CASE(test_reorder_init),
		TEST_CASE(test_reorder_find_existing),
		TEST_CASE(test_reorder_free),
		TEST_CASE(test_reorder_insert),
		TEST_CASE(test_reorder_drain),
		TEST_CASES_END()
	}
};

static int
test_reorder(void)
{
	return unit_test_suite_runner(&reorder_test_suite);
}

static struct test_command reorder_cmd = {
	.command = "reorder_autotest",
	.callback = test_reorder,
};
REGISTER_TEST_COMMAND(reorder_cmd);
