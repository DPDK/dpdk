/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

#include <rte_event_ring.h>

#include "test.h"

/*
 * Event Ring
 * ===========
 *
 * Test some basic ops for the event rings.
 * Does not fully test everything, since most code is reused from rte_ring
 * library and tested as part of the normal ring autotests.
 */

#define RING_SIZE 4096
#define MAX_BULK 32

static struct rte_event_ring *r;

/*
 * ensure failure to create ring with a bad ring size
 */
static int
test_event_ring_creation_with_wrong_size(void)
{
	struct rte_event_ring *rp = NULL;

	/* Test if ring size is not power of 2 */
	rp = rte_event_ring_create("test_bad_ring_size", RING_SIZE + 1,
			SOCKET_ID_ANY, 0);
	if (rp != NULL)
		return -1;

	/* Test if ring size is exceeding the limit */
	rp = rte_event_ring_create("test_bad_ring_size", (RTE_RING_SZ_MASK + 1),
			SOCKET_ID_ANY, 0);
	if (rp != NULL)
		return -1;
	return 0;
}

/*
 * Test to check if a non-power-of-2 count causes the create
 * function to fail correctly
 */
static int
test_create_count_odd(void)
{
	struct rte_event_ring *r = rte_event_ring_create("test_event_ring_count",
			4097, SOCKET_ID_ANY, 0);
	if (r != NULL)
		return -1;
	return 0;
}

static int
test_lookup_null(void)
{
	struct rte_event_ring *rlp = rte_event_ring_lookup("ring_not_found");
	if (rlp == NULL && rte_errno != ENOENT) {
		printf("test failed to return error on null pointer\n");
		return -1;
	}
	return 0;
}

static int
test_basic_event_enqueue_dequeue(void)
{
	struct rte_event_ring *sr = NULL;
	struct rte_event evs[16];
	uint16_t ret, free_count, used_count;

	memset(evs, 0, sizeof(evs));
	sr = rte_event_ring_create("spsc_ring", 32, rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (sr == NULL) {
		printf("Failed to create sp/sc ring\n");
		return -1;
	}
	if (rte_event_ring_get_capacity(sr) != 31) {
		printf("Error, invalid capacity\n");
		goto error;
	}

	/* test sp/sc ring */
	if (rte_event_ring_count(sr) != 0) {
		printf("Error, ring not empty as expected\n");
		goto error;
	}
	if (rte_event_ring_free_count(sr) != rte_event_ring_get_capacity(sr)) {
		printf("Error, ring free count not as expected\n");
		goto error;
	}

	ret = rte_event_ring_enqueue_burst(sr, evs, RTE_DIM(evs), &free_count);
	if (ret != RTE_DIM(evs) ||
			free_count != rte_event_ring_get_capacity(sr) - ret) {
		printf("Error, status after enqueue is unexpected\n");
		goto error;
	}

	ret = rte_event_ring_enqueue_burst(sr, evs, RTE_DIM(evs), &free_count);
	if (ret != RTE_DIM(evs) - 1 ||
			free_count != 0) {
		printf("Error, status after enqueue is unexpected\n");
		goto error;
	}

	ret = rte_event_ring_dequeue_burst(sr, evs, RTE_DIM(evs), &used_count);
	if (ret != RTE_DIM(evs) ||
			used_count != rte_event_ring_get_capacity(sr) - ret) {
		printf("Error, status after enqueue is unexpected\n");
		goto error;
	}
	ret = rte_event_ring_dequeue_burst(sr, evs, RTE_DIM(evs), &used_count);
	if (ret != RTE_DIM(evs) - 1 ||
			used_count != 0) {
		printf("Error, status after enqueue is unexpected\n");
		goto error;
	}

	rte_event_ring_free(sr);
	return 0;
error:
	rte_event_ring_free(sr);
	return -1;
}

static int
test_event_ring_with_exact_size(void)
{
	struct rte_event_ring *std_ring, *exact_sz_ring;
	struct rte_event ev = { .mbuf = NULL };
	struct rte_event ev_array[16];
	static const unsigned int ring_sz = RTE_DIM(ev_array);
	unsigned int i;

	std_ring = rte_event_ring_create("std", ring_sz, rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (std_ring == NULL) {
		printf("%s: error, can't create std ring\n", __func__);
		return -1;
	}
	exact_sz_ring = rte_event_ring_create("exact sz",
			ring_sz, rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
	if (exact_sz_ring == NULL) {
		printf("%s: error, can't create exact size ring\n", __func__);
		return -1;
	}

	/*
	 * Check that the exact size ring is bigger than the standard ring
	 */
	if (rte_event_ring_get_size(std_ring) >=
			rte_event_ring_get_size(exact_sz_ring)) {
		printf("%s: error, std ring (size: %u) is not smaller than exact size one (size %u)\n",
				__func__,
				rte_event_ring_get_size(std_ring),
				rte_event_ring_get_size(exact_sz_ring));
		return -1;
	}
	/*
	 * check that the exact_sz_ring can hold one more element than the
	 * standard ring. (16 vs 15 elements)
	 */
	for (i = 0; i < ring_sz - 1; i++) {
		rte_event_ring_enqueue_burst(std_ring, &ev, 1, NULL);
		rte_event_ring_enqueue_burst(exact_sz_ring, &ev, 1, NULL);
	}
	if (rte_event_ring_enqueue_burst(std_ring, &ev, 1, NULL) != 0) {
		printf("%s: error, unexpected successful enqueue\n", __func__);
		return -1;
	}
	if (rte_event_ring_enqueue_burst(exact_sz_ring, &ev, 1, NULL) != 1) {
		printf("%s: error, enqueue failed\n", __func__);
		return -1;
	}

	/* check that dequeue returns the expected number of elements */
	if (rte_event_ring_dequeue_burst(exact_sz_ring, ev_array,
			RTE_DIM(ev_array), NULL) != ring_sz) {
		printf("%s: error, failed to dequeue expected nb of elements\n",
				__func__);
		return -1;
	}

	/* check that the capacity function returns expected value */
	if (rte_event_ring_get_capacity(exact_sz_ring) != ring_sz) {
		printf("%s: error, incorrect ring capacity reported\n",
				__func__);
		return -1;
	}

	rte_event_ring_free(std_ring);
	rte_event_ring_free(exact_sz_ring);
	return 0;
}

static int
test_event_ring(void)
{
	if (r == NULL)
		r = rte_event_ring_create("ev_test", RING_SIZE,
				SOCKET_ID_ANY, 0);
	if (r == NULL)
		return -1;

	/* retrieve the ring from its name */
	if (rte_event_ring_lookup("ev_test") != r) {
		printf("Cannot lookup ring from its name\n");
		return -1;
	}

	/* basic operations */
	if (test_create_count_odd() < 0) {
		printf("Test failed to detect odd count\n");
		return -1;
	}
	printf("Test detected odd count\n");

	if (test_lookup_null() < 0) {
		printf("Test failed to detect NULL ring lookup\n");
		return -1;
	}
	printf("Test detected NULL ring lookup\n");

	/* test of creating ring with wrong size */
	if (test_event_ring_creation_with_wrong_size() < 0)
		return -1;

	if (test_basic_event_enqueue_dequeue() < 0)
		return -1;

	if (test_event_ring_with_exact_size() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(event_ring_autotest, test_event_ring);
