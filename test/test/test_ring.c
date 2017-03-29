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
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_random.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_hexdump.h>

#include "test.h"

/*
 * Ring
 * ====
 *
 * #. Basic tests: done on one core:
 *
 *    - Using single producer/single consumer functions:
 *
 *      - Enqueue one object, two objects, MAX_BULK objects
 *      - Dequeue one object, two objects, MAX_BULK objects
 *      - Check that dequeued pointers are correct
 *
 *    - Using multi producers/multi consumers functions:
 *
 *      - Enqueue one object, two objects, MAX_BULK objects
 *      - Dequeue one object, two objects, MAX_BULK objects
 *      - Check that dequeued pointers are correct
 *
 * #. Performance tests.
 *
 * Tests done in test_ring_perf.c
 */

#define RING_SIZE 4096
#define MAX_BULK 32

static rte_atomic32_t synchro;

static struct rte_ring *r;

#define	TEST_RING_VERIFY(exp)						\
	if (!(exp)) {							\
		printf("error at %s:%d\tcondition " #exp " failed\n",	\
		    __func__, __LINE__);				\
		rte_ring_dump(stdout, r);				\
		return -1;						\
	}

#define	TEST_RING_FULL_EMTPY_ITER	8

/*
 * helper routine for test_ring_basic
 */
static int
test_ring_basic_full_empty(void * const src[], void *dst[])
{
	unsigned i, rand;
	const unsigned rsz = RING_SIZE - 1;

	printf("Basic full/empty test\n");

	for (i = 0; TEST_RING_FULL_EMTPY_ITER != i; i++) {

		/* random shift in the ring */
		rand = RTE_MAX(rte_rand() % RING_SIZE, 1UL);
		printf("%s: iteration %u, random shift: %u;\n",
		    __func__, i, rand);
		TEST_RING_VERIFY(rte_ring_enqueue_bulk(r, src, rand,
				NULL) != 0);
		TEST_RING_VERIFY(rte_ring_dequeue_bulk(r, dst, rand,
				NULL) == rand);

		/* fill the ring */
		TEST_RING_VERIFY(rte_ring_enqueue_bulk(r, src, rsz, NULL) != 0);
		TEST_RING_VERIFY(0 == rte_ring_free_count(r));
		TEST_RING_VERIFY(rsz == rte_ring_count(r));
		TEST_RING_VERIFY(rte_ring_full(r));
		TEST_RING_VERIFY(0 == rte_ring_empty(r));

		/* empty the ring */
		TEST_RING_VERIFY(rte_ring_dequeue_bulk(r, dst, rsz,
				NULL) == rsz);
		TEST_RING_VERIFY(rsz == rte_ring_free_count(r));
		TEST_RING_VERIFY(0 == rte_ring_count(r));
		TEST_RING_VERIFY(0 == rte_ring_full(r));
		TEST_RING_VERIFY(rte_ring_empty(r));

		/* check data */
		TEST_RING_VERIFY(0 == memcmp(src, dst, rsz));
		rte_ring_dump(stdout, r);
	}
	return 0;
}

static int
test_ring_basic(void)
{
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned i, num_elems;

	/* alloc dummy object pointers */
	src = malloc(RING_SIZE*2*sizeof(void *));
	if (src == NULL)
		goto fail;

	for (i = 0; i < RING_SIZE*2 ; i++) {
		src[i] = (void *)(unsigned long)i;
	}
	cur_src = src;

	/* alloc some room for copied objects */
	dst = malloc(RING_SIZE*2*sizeof(void *));
	if (dst == NULL)
		goto fail;

	memset(dst, 0, RING_SIZE*2*sizeof(void *));
	cur_dst = dst;

	printf("enqueue 1 obj\n");
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, 1, NULL);
	cur_src += 1;
	if (ret == 0)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, 2, NULL);
	cur_src += 2;
	if (ret == 0)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, MAX_BULK, NULL);
	cur_src += MAX_BULK;
	if (ret == 0)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_sc_dequeue_bulk(r, cur_dst, 1, NULL);
	cur_dst += 1;
	if (ret == 0)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_sc_dequeue_bulk(r, cur_dst, 2, NULL);
	cur_dst += 2;
	if (ret == 0)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_sc_dequeue_bulk(r, cur_dst, MAX_BULK, NULL);
	cur_dst += MAX_BULK;
	if (ret == 0)
		goto fail;

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}
	cur_src = src;
	cur_dst = dst;

	printf("enqueue 1 obj\n");
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, 1, NULL);
	cur_src += 1;
	if (ret == 0)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, 2, NULL);
	cur_src += 2;
	if (ret == 0)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, MAX_BULK, NULL);
	cur_src += MAX_BULK;
	if (ret == 0)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_mc_dequeue_bulk(r, cur_dst, 1, NULL);
	cur_dst += 1;
	if (ret == 0)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_mc_dequeue_bulk(r, cur_dst, 2, NULL);
	cur_dst += 2;
	if (ret == 0)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_mc_dequeue_bulk(r, cur_dst, MAX_BULK, NULL);
	cur_dst += MAX_BULK;
	if (ret == 0)
		goto fail;

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}
	cur_src = src;
	cur_dst = dst;

	printf("fill and empty the ring\n");
	for (i = 0; i<RING_SIZE/MAX_BULK; i++) {
		ret = rte_ring_mp_enqueue_bulk(r, cur_src, MAX_BULK, NULL);
		cur_src += MAX_BULK;
		if (ret == 0)
			goto fail;
		ret = rte_ring_mc_dequeue_bulk(r, cur_dst, MAX_BULK, NULL);
		cur_dst += MAX_BULK;
		if (ret == 0)
			goto fail;
	}

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	if (test_ring_basic_full_empty(src, dst) != 0)
		goto fail;

	cur_src = src;
	cur_dst = dst;

	printf("test default bulk enqueue / dequeue\n");
	num_elems = 16;

	cur_src = src;
	cur_dst = dst;

	ret = rte_ring_enqueue_bulk(r, cur_src, num_elems, NULL);
	cur_src += num_elems;
	if (ret == 0) {
		printf("Cannot enqueue\n");
		goto fail;
	}
	ret = rte_ring_enqueue_bulk(r, cur_src, num_elems, NULL);
	cur_src += num_elems;
	if (ret == 0) {
		printf("Cannot enqueue\n");
		goto fail;
	}
	ret = rte_ring_dequeue_bulk(r, cur_dst, num_elems, NULL);
	cur_dst += num_elems;
	if (ret == 0) {
		printf("Cannot dequeue\n");
		goto fail;
	}
	ret = rte_ring_dequeue_bulk(r, cur_dst, num_elems, NULL);
	cur_dst += num_elems;
	if (ret == 0) {
		printf("Cannot dequeue2\n");
		goto fail;
	}

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;

	ret = rte_ring_mp_enqueue(r, cur_src);
	if (ret != 0)
		goto fail;

	ret = rte_ring_mc_dequeue(r, cur_dst);
	if (ret != 0)
		goto fail;

	free(src);
	free(dst);
	return 0;

 fail:
	free(src);
	free(dst);
	return -1;
}

static int
test_ring_burst_basic(void)
{
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned i;

	/* alloc dummy object pointers */
	src = malloc(RING_SIZE*2*sizeof(void *));
	if (src == NULL)
		goto fail;

	for (i = 0; i < RING_SIZE*2 ; i++) {
		src[i] = (void *)(unsigned long)i;
	}
	cur_src = src;

	/* alloc some room for copied objects */
	dst = malloc(RING_SIZE*2*sizeof(void *));
	if (dst == NULL)
		goto fail;

	memset(dst, 0, RING_SIZE*2*sizeof(void *));
	cur_dst = dst;

	printf("Test SP & SC basic functions \n");
	printf("enqueue 1 obj\n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, 1, NULL);
	cur_src += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, 2, NULL);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
	cur_src += MAX_BULK;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, 1, NULL);
	cur_dst += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, 2, NULL);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK, NULL);
	cur_dst += MAX_BULK;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
		goto fail;

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;

	printf("Test enqueue without enough memory space \n");
	for (i = 0; i< (RING_SIZE/MAX_BULK - 1); i++) {
		ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
		cur_src += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK) {
			goto fail;
		}
	}

	printf("Enqueue 2 objects, free entries = MAX_BULK - 2  \n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, 2, NULL);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("Enqueue the remaining entries = MAX_BULK - 2  \n");
	/* Always one free entry left */
	ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
	cur_src += MAX_BULK - 3;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK - 3)
		goto fail;

	printf("Test if ring is full  \n");
	if (rte_ring_full(r) != 1)
		goto fail;

	printf("Test enqueue for a full entry  \n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
	if ((ret & RTE_RING_SZ_MASK) != 0)
		goto fail;

	printf("Test dequeue without enough objects \n");
	for (i = 0; i<RING_SIZE/MAX_BULK - 1; i++) {
		ret = rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK, NULL);
		cur_dst += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
	}

	/* Available memory space for the exact MAX_BULK entries */
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, 2, NULL);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK, NULL);
	cur_dst += MAX_BULK - 3;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK - 3)
		goto fail;

	printf("Test if ring is empty \n");
	/* Check if ring is empty */
	if (1 != rte_ring_empty(r))
		goto fail;

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;

	printf("Test MP & MC basic functions \n");

	printf("enqueue 1 obj\n");
	ret = rte_ring_mp_enqueue_burst(r, cur_src, 1, NULL);
	cur_src += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_mp_enqueue_burst(r, cur_src, 2, NULL);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
	cur_src += MAX_BULK;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, 1, NULL);
	cur_dst += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, 2, NULL);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK, NULL);
	cur_dst += MAX_BULK;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
		goto fail;

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;

	printf("fill and empty the ring\n");
	for (i = 0; i<RING_SIZE/MAX_BULK; i++) {
		ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
		cur_src += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
		ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK, NULL);
		cur_dst += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
	}

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;

	printf("Test enqueue without enough memory space \n");
	for (i = 0; i<RING_SIZE/MAX_BULK - 1; i++) {
		ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
		cur_src += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
	}

	/* Available memory space for the exact MAX_BULK objects */
	ret = rte_ring_mp_enqueue_burst(r, cur_src, 2, NULL);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK, NULL);
	cur_src += MAX_BULK - 3;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK - 3)
		goto fail;


	printf("Test dequeue without enough objects \n");
	for (i = 0; i<RING_SIZE/MAX_BULK - 1; i++) {
		ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK, NULL);
		cur_dst += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
	}

	/* Available objects - the exact MAX_BULK */
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, 2, NULL);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK, NULL);
	cur_dst += MAX_BULK - 3;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK - 3)
		goto fail;

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		rte_hexdump(stdout, "src", src, cur_src - src);
		rte_hexdump(stdout, "dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;

	printf("Covering rte_ring_enqueue_burst functions \n");

	ret = rte_ring_enqueue_burst(r, cur_src, 2, NULL);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_dequeue_burst(r, cur_dst, 2, NULL);
	cur_dst += 2;
	if (ret != 2)
		goto fail;

	/* Free memory before test completed */
	free(src);
	free(dst);
	return 0;

 fail:
	free(src);
	free(dst);
	return -1;
}

/*
 * it will always fail to create ring with a wrong ring size number in this function
 */
static int
test_ring_creation_with_wrong_size(void)
{
	struct rte_ring * rp = NULL;

	/* Test if ring size is not power of 2 */
	rp = rte_ring_create("test_bad_ring_size", RING_SIZE + 1, SOCKET_ID_ANY, 0);
	if (NULL != rp) {
		return -1;
	}

	/* Test if ring size is exceeding the limit */
	rp = rte_ring_create("test_bad_ring_size", (RTE_RING_SZ_MASK + 1), SOCKET_ID_ANY, 0);
	if (NULL != rp) {
		return -1;
	}
	return 0;
}

/*
 * it tests if it would always fail to create ring with an used ring name
 */
static int
test_ring_creation_with_an_used_name(void)
{
	struct rte_ring * rp;

	rp = rte_ring_create("test", RING_SIZE, SOCKET_ID_ANY, 0);
	if (NULL != rp)
		return -1;

	return 0;
}

/*
 * Test to if a non-power of 2 count causes the create
 * function to fail correctly
 */
static int
test_create_count_odd(void)
{
	struct rte_ring *r = rte_ring_create("test_ring_count",
			4097, SOCKET_ID_ANY, 0 );
	if(r != NULL){
		return -1;
	}
	return 0;
}

static int
test_lookup_null(void)
{
	struct rte_ring *rlp = rte_ring_lookup("ring_not_found");
	if (rlp ==NULL)
	if (rte_errno != ENOENT){
		printf( "test failed to returnn error on null pointer\n");
		return -1;
	}
	return 0;
}

/*
 * it tests some more basic ring operations
 */
static int
test_ring_basic_ex(void)
{
	int ret = -1;
	unsigned i;
	struct rte_ring * rp;
	void **obj = NULL;

	obj = rte_calloc("test_ring_basic_ex_malloc", RING_SIZE, sizeof(void *), 0);
	if (obj == NULL) {
		printf("test_ring_basic_ex fail to rte_malloc\n");
		goto fail_test;
	}

	rp = rte_ring_create("test_ring_basic_ex", RING_SIZE, SOCKET_ID_ANY,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (rp == NULL) {
		printf("test_ring_basic_ex fail to create ring\n");
		goto fail_test;
	}

	if (rte_ring_lookup("test_ring_basic_ex") != rp) {
		goto fail_test;
	}

	if (rte_ring_empty(rp) != 1) {
		printf("test_ring_basic_ex ring is not empty but it should be\n");
		goto fail_test;
	}

	printf("%u ring entries are now free\n", rte_ring_free_count(rp));

	for (i = 0; i < RING_SIZE; i ++) {
		rte_ring_enqueue(rp, obj[i]);
	}

	if (rte_ring_full(rp) != 1) {
		printf("test_ring_basic_ex ring is not full but it should be\n");
		goto fail_test;
	}

	for (i = 0; i < RING_SIZE; i ++) {
		rte_ring_dequeue(rp, &obj[i]);
	}

	if (rte_ring_empty(rp) != 1) {
		printf("test_ring_basic_ex ring is not empty but it should be\n");
		goto fail_test;
	}

	/* Covering the ring burst operation */
	ret = rte_ring_enqueue_burst(rp, obj, 2, NULL);
	if ((ret & RTE_RING_SZ_MASK) != 2) {
		printf("test_ring_basic_ex: rte_ring_enqueue_burst fails \n");
		goto fail_test;
	}

	ret = rte_ring_dequeue_burst(rp, obj, 2, NULL);
	if (ret != 2) {
		printf("test_ring_basic_ex: rte_ring_dequeue_burst fails \n");
		goto fail_test;
	}

	ret = 0;
fail_test:
	if (obj != NULL)
		rte_free(obj);

	return ret;
}

static int
test_ring(void)
{
	/* some more basic operations */
	if (test_ring_basic_ex() < 0)
		return -1;

	rte_atomic32_init(&synchro);

	if (r == NULL)
		r = rte_ring_create("test", RING_SIZE, SOCKET_ID_ANY, 0);
	if (r == NULL)
		return -1;

	/* retrieve the ring from its name */
	if (rte_ring_lookup("test") != r) {
		printf("Cannot lookup ring from its name\n");
		return -1;
	}

	/* burst operations */
	if (test_ring_burst_basic() < 0)
		return -1;

	/* basic operations */
	if (test_ring_basic() < 0)
		return -1;

	/* basic operations */
	if ( test_create_count_odd() < 0){
			printf ("Test failed to detect odd count\n");
			return -1;
		}
		else
			printf ( "Test detected odd count\n");

	if ( test_lookup_null() < 0){
				printf ("Test failed to detect NULL ring lookup\n");
				return -1;
			}
			else
				printf ( "Test detected NULL ring lookup \n");

	/* test of creating ring with wrong size */
	if (test_ring_creation_with_wrong_size() < 0)
		return -1;

	/* test of creation ring with an used name */
	if (test_ring_creation_with_an_used_name() < 0)
		return -1;

	/* dump the ring status */
	rte_ring_list_dump(stdout);

	return 0;
}

REGISTER_TEST_COMMAND(ring_autotest, test_ring);
