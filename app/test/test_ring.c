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
#include <rte_tailq.h>
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
 *    - Test watermark and default bulk enqueue/dequeue:
 *
 *      - Set watermark
 *      - Set default bulk value
 *      - Enqueue objects, check that -EDQUOT is returned when
 *        watermark is exceeded
 *      - Check that dequeued pointers are correct
 *
 * #. Check live watermark change
 *
 *    - Start a loop on another lcore that will enqueue and dequeue
 *      objects in a ring. It will monitor the value of watermark.
 *    - At the same time, change the watermark on the master lcore.
 *    - The slave lcore will check that watermark changes from 16 to 32.
 *
 * #. Performance tests.
 *
 * Tests done in test_ring_perf.c
 */

#define RING_SIZE 4096
#define MAX_BULK 32
#define N 65536
#define TIME_S 5

static rte_atomic32_t synchro;

static struct rte_ring *r;

#define	TEST_RING_VERIFY(exp)						\
	if (!(exp)) {							\
		printf("error at %s:%d\tcondition " #exp " failed\n",	\
		    __func__, __LINE__);				\
		rte_ring_dump(stdout, r);				\
		return (-1);						\
	}

#define	TEST_RING_FULL_EMTPY_ITER	8

static int
check_live_watermark_change(__attribute__((unused)) void *dummy)
{
	uint64_t hz = rte_get_timer_hz();
	void *obj_table[MAX_BULK];
	unsigned watermark, watermark_old = 16;
	uint64_t cur_time, end_time;
	int64_t diff = 0;
	int i, ret;
	unsigned count = 4;

	/* init the object table */
	memset(obj_table, 0, sizeof(obj_table));
	end_time = rte_get_timer_cycles() + (hz * 2);

	/* check that bulk and watermark are 4 and 32 (respectively) */
	while (diff >= 0) {

		/* add in ring until we reach watermark */
		ret = 0;
		for (i = 0; i < 16; i ++) {
			if (ret != 0)
				break;
			ret = rte_ring_enqueue_bulk(r, obj_table, count);
		}

		if (ret != -EDQUOT) {
			printf("Cannot enqueue objects, or watermark not "
			       "reached (ret=%d)\n", ret);
			return -1;
		}

		/* read watermark, the only change allowed is from 16 to 32 */
		watermark = r->prod.watermark;
		if (watermark != watermark_old &&
		    (watermark_old != 16 || watermark != 32)) {
			printf("Bad watermark change %u -> %u\n", watermark_old,
			       watermark);
			return -1;
		}
		watermark_old = watermark;

		/* dequeue objects from ring */
		while (i--) {
			ret = rte_ring_dequeue_bulk(r, obj_table, count);
			if (ret != 0) {
				printf("Cannot dequeue (ret=%d)\n", ret);
				return -1;
			}
		}

		cur_time = rte_get_timer_cycles();
		diff = end_time - cur_time;
	}

	if (watermark_old != 32 ) {
		printf(" watermark was not updated (wm=%u)\n",
		       watermark_old);
		return -1;
	}

	return 0;
}

static int
test_live_watermark_change(void)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned lcore_id2 = rte_get_next_lcore(lcore_id, 0, 1);

	printf("Test watermark live modification\n");
	rte_ring_set_water_mark(r, 16);

	/* launch a thread that will enqueue and dequeue, checking
	 * watermark and quota */
	rte_eal_remote_launch(check_live_watermark_change, NULL, lcore_id2);

	rte_delay_ms(1000);
	rte_ring_set_water_mark(r, 32);
	rte_delay_ms(1000);

	if (rte_eal_wait_lcore(lcore_id2) < 0)
		return -1;

	return 0;
}

/* Test for catch on invalid watermark values */
static int
test_set_watermark( void ){
	unsigned count;
	int setwm;

	struct rte_ring *r = rte_ring_lookup("test_ring_basic_ex");
	if(r == NULL){
		printf( " ring lookup failed\n" );
		goto error;
	}
	count = r->prod.size*2;
	setwm = rte_ring_set_water_mark(r, count);
	if (setwm != -EINVAL){
		printf("Test failed to detect invalid watermark count value\n");
		goto error;
	}

	count = 0;
	rte_ring_set_water_mark(r, count);
	if (r->prod.watermark != r->prod.size) {
		printf("Test failed to detect invalid watermark count value\n");
		goto error;
	}
	return 0;

error:
	return -1;
}

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
		TEST_RING_VERIFY(-ENOBUFS != rte_ring_enqueue_bulk(r, src,
		    rand));
		TEST_RING_VERIFY(0 == rte_ring_dequeue_bulk(r, dst, rand));

		/* fill the ring */
		TEST_RING_VERIFY(-ENOBUFS != rte_ring_enqueue_bulk(r, src,
		    rsz));
		TEST_RING_VERIFY(0 == rte_ring_free_count(r));
		TEST_RING_VERIFY(rsz == rte_ring_count(r));
		TEST_RING_VERIFY(rte_ring_full(r));
		TEST_RING_VERIFY(0 == rte_ring_empty(r));

		/* empty the ring */
		TEST_RING_VERIFY(0 == rte_ring_dequeue_bulk(r, dst, rsz));
		TEST_RING_VERIFY(rsz == rte_ring_free_count(r));
		TEST_RING_VERIFY(0 == rte_ring_count(r));
		TEST_RING_VERIFY(0 == rte_ring_full(r));
		TEST_RING_VERIFY(rte_ring_empty(r));

		/* check data */
		TEST_RING_VERIFY(0 == memcmp(src, dst, rsz));
		rte_ring_dump(stdout, r);
	}
	return (0);
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
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, 1);
	cur_src += 1;
	if (ret != 0)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, 2);
	cur_src += 2;
	if (ret != 0)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, MAX_BULK);
	cur_src += MAX_BULK;
	if (ret != 0)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_sc_dequeue_bulk(r, cur_dst, 1);
	cur_dst += 1;
	if (ret != 0)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_sc_dequeue_bulk(r, cur_dst, 2);
	cur_dst += 2;
	if (ret != 0)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_sc_dequeue_bulk(r, cur_dst, MAX_BULK);
	cur_dst += MAX_BULK;
	if (ret != 0)
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
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, 1);
	cur_src += 1;
	if (ret != 0)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, 2);
	cur_src += 2;
	if (ret != 0)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, MAX_BULK);
	cur_src += MAX_BULK;
	if (ret != 0)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_mc_dequeue_bulk(r, cur_dst, 1);
	cur_dst += 1;
	if (ret != 0)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_mc_dequeue_bulk(r, cur_dst, 2);
	cur_dst += 2;
	if (ret != 0)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_mc_dequeue_bulk(r, cur_dst, MAX_BULK);
	cur_dst += MAX_BULK;
	if (ret != 0)
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
		ret = rte_ring_mp_enqueue_bulk(r, cur_src, MAX_BULK);
		cur_src += MAX_BULK;
		if (ret != 0)
			goto fail;
		ret = rte_ring_mc_dequeue_bulk(r, cur_dst, MAX_BULK);
		cur_dst += MAX_BULK;
		if (ret != 0)
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

	printf("test watermark and default bulk enqueue / dequeue\n");
	rte_ring_set_water_mark(r, 20);
	num_elems = 16;

	cur_src = src;
	cur_dst = dst;

	ret = rte_ring_enqueue_bulk(r, cur_src, num_elems);
	cur_src += num_elems;
	if (ret != 0) {
		printf("Cannot enqueue\n");
		goto fail;
	}
	ret = rte_ring_enqueue_bulk(r, cur_src, num_elems);
	cur_src += num_elems;
	if (ret != -EDQUOT) {
		printf("Watermark not exceeded\n");
		goto fail;
	}
	ret = rte_ring_dequeue_bulk(r, cur_dst, num_elems);
	cur_dst += num_elems;
	if (ret != 0) {
		printf("Cannot dequeue\n");
		goto fail;
	}
	ret = rte_ring_dequeue_bulk(r, cur_dst, num_elems);
	cur_dst += num_elems;
	if (ret != 0) {
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

	if (src)
		free(src);
	if (dst)
		free(dst);
	return 0;

 fail:
	if (src)
		free(src);
	if (dst)
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
	ret = rte_ring_sp_enqueue_burst(r, cur_src, 1);
	cur_src += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, 2);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK) ;
	cur_src += MAX_BULK;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, 1) ;
	cur_dst += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, 2);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK);
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
		ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK);
		cur_src += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK) {
			goto fail;
		}
	}

	printf("Enqueue 2 objects, free entries = MAX_BULK - 2  \n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, 2);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("Enqueue the remaining entries = MAX_BULK - 2  \n");
	/* Always one free entry left */
	ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK);
	cur_src += MAX_BULK - 3;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK - 3)
		goto fail;

	printf("Test if ring is full  \n");
	if (rte_ring_full(r) != 1)
		goto fail;

	printf("Test enqueue for a full entry  \n");
	ret = rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK);
	if ((ret & RTE_RING_SZ_MASK) != 0)
		goto fail;

	printf("Test dequeue without enough objects \n");
	for (i = 0; i<RING_SIZE/MAX_BULK - 1; i++) {
		ret = rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK);
		cur_dst += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
	}

	/* Available memory space for the exact MAX_BULK entries */
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, 2);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK);
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
	ret = rte_ring_mp_enqueue_burst(r, cur_src, 1);
	cur_src += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("enqueue 2 objs\n");
	ret = rte_ring_mp_enqueue_burst(r, cur_src, 2);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("enqueue MAX_BULK objs\n");
	ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK);
	cur_src += MAX_BULK;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
		goto fail;

	printf("dequeue 1 obj\n");
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, 1);
	cur_dst += 1;
	if ((ret & RTE_RING_SZ_MASK) != 1)
		goto fail;

	printf("dequeue 2 objs\n");
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, 2);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	printf("dequeue MAX_BULK objs\n");
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK);
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
		ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK);
		cur_src += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
		ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK);
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
		ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK);
		cur_src += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
	}

	/* Available memory space for the exact MAX_BULK objects */
	ret = rte_ring_mp_enqueue_burst(r, cur_src, 2);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_mp_enqueue_burst(r, cur_src, MAX_BULK);
	cur_src += MAX_BULK - 3;
	if ((ret & RTE_RING_SZ_MASK) != MAX_BULK - 3)
		goto fail;


	printf("Test dequeue without enough objects \n");
	for (i = 0; i<RING_SIZE/MAX_BULK - 1; i++) {
		ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK);
		cur_dst += MAX_BULK;
		if ((ret & RTE_RING_SZ_MASK) != MAX_BULK)
			goto fail;
	}

	/* Available objects - the exact MAX_BULK */
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, 2);
	cur_dst += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_mc_dequeue_burst(r, cur_dst, MAX_BULK);
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

	ret = rte_ring_enqueue_burst(r, cur_src, 2);
	cur_src += 2;
	if ((ret & RTE_RING_SZ_MASK) != 2)
		goto fail;

	ret = rte_ring_dequeue_burst(r, cur_dst, 2);
	cur_dst += 2;
	if (ret != 2)
		goto fail;

	/* Free memory before test completed */
	if (src)
		free(src);
	if (dst)
		free(dst);
	return 0;

 fail:
	if (src)
		free(src);
	if (dst)
		free(dst);
	return -1;
}

static int
test_ring_stats(void)
{

#ifndef RTE_LIBRTE_RING_DEBUG
	printf("Enable RTE_LIBRTE_RING_DEBUG to test ring stats.\n");
	return 0;
#else
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned i;
	unsigned num_items            = 0;
	unsigned failed_enqueue_ops   = 0;
	unsigned failed_enqueue_items = 0;
	unsigned failed_dequeue_ops   = 0;
	unsigned failed_dequeue_items = 0;
	unsigned last_enqueue_ops     = 0;
	unsigned last_enqueue_items   = 0;
	unsigned last_quota_ops       = 0;
	unsigned last_quota_items     = 0;
	unsigned lcore_id = rte_lcore_id();
	struct rte_ring_debug_stats *ring_stats = &r->stats[lcore_id];

	printf("Test the ring stats.\n");

	/* Reset the watermark in case it was set in another test. */
	rte_ring_set_water_mark(r, 0);

	/* Reset the ring stats. */
	memset(&r->stats[lcore_id], 0, sizeof(r->stats[lcore_id]));

	/* Allocate some dummy object pointers. */
	src = malloc(RING_SIZE*2*sizeof(void *));
	if (src == NULL)
		goto fail;

	for (i = 0; i < RING_SIZE*2 ; i++) {
		src[i] = (void *)(unsigned long)i;
	}

	/* Allocate some memory for copied objects. */
	dst = malloc(RING_SIZE*2*sizeof(void *));
	if (dst == NULL)
		goto fail;

	memset(dst, 0, RING_SIZE*2*sizeof(void *));

	/* Set the head and tail pointers. */
	cur_src = src;
	cur_dst = dst;

	/* Do Enqueue tests. */
	printf("Test the dequeue stats.\n");

	/* Fill the ring up to RING_SIZE -1. */
	printf("Fill the ring.\n");
	for (i = 0; i< (RING_SIZE/MAX_BULK); i++) {
		rte_ring_sp_enqueue_burst(r, cur_src, MAX_BULK);
		cur_src += MAX_BULK;
	}

	/* Adjust for final enqueue = MAX_BULK -1. */
	cur_src--;

	printf("Verify that the ring is full.\n");
	if (rte_ring_full(r) != 1)
		goto fail;


	printf("Verify the enqueue success stats.\n");
	/* Stats should match above enqueue operations to fill the ring. */
	if (ring_stats->enq_success_bulk != (RING_SIZE/MAX_BULK))
		goto fail;

	/* Current max objects is RING_SIZE -1. */
	if (ring_stats->enq_success_objs != RING_SIZE -1)
		goto fail;

	/* Shouldn't have any failures yet. */
	if (ring_stats->enq_fail_bulk != 0)
		goto fail;
	if (ring_stats->enq_fail_objs != 0)
		goto fail;


	printf("Test stats for SP burst enqueue to a full ring.\n");
	num_items = 2;
	ret = rte_ring_sp_enqueue_burst(r, cur_src, num_items);
	if ((ret & RTE_RING_SZ_MASK) != 0)
		goto fail;

	failed_enqueue_ops   += 1;
	failed_enqueue_items += num_items;

	/* The enqueue should have failed. */
	if (ring_stats->enq_fail_bulk != failed_enqueue_ops)
		goto fail;
	if (ring_stats->enq_fail_objs != failed_enqueue_items)
		goto fail;


	printf("Test stats for SP bulk enqueue to a full ring.\n");
	num_items = 4;
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, num_items);
	if (ret != -ENOBUFS)
		goto fail;

	failed_enqueue_ops   += 1;
	failed_enqueue_items += num_items;

	/* The enqueue should have failed. */
	if (ring_stats->enq_fail_bulk != failed_enqueue_ops)
		goto fail;
	if (ring_stats->enq_fail_objs != failed_enqueue_items)
		goto fail;


	printf("Test stats for MP burst enqueue to a full ring.\n");
	num_items = 8;
	ret = rte_ring_mp_enqueue_burst(r, cur_src, num_items);
	if ((ret & RTE_RING_SZ_MASK) != 0)
		goto fail;

	failed_enqueue_ops   += 1;
	failed_enqueue_items += num_items;

	/* The enqueue should have failed. */
	if (ring_stats->enq_fail_bulk != failed_enqueue_ops)
		goto fail;
	if (ring_stats->enq_fail_objs != failed_enqueue_items)
		goto fail;


	printf("Test stats for MP bulk enqueue to a full ring.\n");
	num_items = 16;
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, num_items);
	if (ret != -ENOBUFS)
		goto fail;

	failed_enqueue_ops   += 1;
	failed_enqueue_items += num_items;

	/* The enqueue should have failed. */
	if (ring_stats->enq_fail_bulk != failed_enqueue_ops)
		goto fail;
	if (ring_stats->enq_fail_objs != failed_enqueue_items)
		goto fail;


	/* Do Dequeue tests. */
	printf("Test the dequeue stats.\n");

	printf("Empty the ring.\n");
	for (i = 0; i<RING_SIZE/MAX_BULK; i++) {
		rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK);
		cur_dst += MAX_BULK;
	}

	/* There was only RING_SIZE -1 objects to dequeue. */
	cur_dst++;

	printf("Verify ring is empty.\n");
	if (1 != rte_ring_empty(r))
		goto fail;

	printf("Verify the dequeue success stats.\n");
	/* Stats should match above dequeue operations. */
	if (ring_stats->deq_success_bulk != (RING_SIZE/MAX_BULK))
		goto fail;

	/* Objects dequeued is RING_SIZE -1. */
	if (ring_stats->deq_success_objs != RING_SIZE -1)
		goto fail;

	/* Shouldn't have any dequeue failure stats yet. */
	if (ring_stats->deq_fail_bulk != 0)
		goto fail;

	printf("Test stats for SC burst dequeue with an empty ring.\n");
	num_items = 2;
	ret = rte_ring_sc_dequeue_burst(r, cur_dst, num_items);
	if ((ret & RTE_RING_SZ_MASK) != 0)
		goto fail;

	failed_dequeue_ops   += 1;
	failed_dequeue_items += num_items;

	/* The dequeue should have failed. */
	if (ring_stats->deq_fail_bulk != failed_dequeue_ops)
		goto fail;
	if (ring_stats->deq_fail_objs != failed_dequeue_items)
		goto fail;


	printf("Test stats for SC bulk dequeue with an empty ring.\n");
	num_items = 4;
	ret = rte_ring_sc_dequeue_bulk(r, cur_dst, num_items);
	if (ret != -ENOENT)
		goto fail;

	failed_dequeue_ops   += 1;
	failed_dequeue_items += num_items;

	/* The dequeue should have failed. */
	if (ring_stats->deq_fail_bulk != failed_dequeue_ops)
		goto fail;
	if (ring_stats->deq_fail_objs != failed_dequeue_items)
		goto fail;


	printf("Test stats for MC burst dequeue with an empty ring.\n");
	num_items = 8;
	ret = rte_ring_mc_dequeue_burst(r, cur_dst, num_items);
	if ((ret & RTE_RING_SZ_MASK) != 0)
		goto fail;
	failed_dequeue_ops   += 1;
	failed_dequeue_items += num_items;

	/* The dequeue should have failed. */
	if (ring_stats->deq_fail_bulk != failed_dequeue_ops)
		goto fail;
	if (ring_stats->deq_fail_objs != failed_dequeue_items)
		goto fail;


	printf("Test stats for MC bulk dequeue with an empty ring.\n");
	num_items = 16;
	ret = rte_ring_mc_dequeue_bulk(r, cur_dst, num_items);
	if (ret != -ENOENT)
		goto fail;

	failed_dequeue_ops   += 1;
	failed_dequeue_items += num_items;

	/* The dequeue should have failed. */
	if (ring_stats->deq_fail_bulk != failed_dequeue_ops)
		goto fail;
	if (ring_stats->deq_fail_objs != failed_dequeue_items)
		goto fail;


	printf("Test total enqueue/dequeue stats.\n");
	/* At this point the enqueue and dequeue stats should be the same. */
	if (ring_stats->enq_success_bulk != ring_stats->deq_success_bulk)
		goto fail;
	if (ring_stats->enq_success_objs != ring_stats->deq_success_objs)
		goto fail;
	if (ring_stats->enq_fail_bulk    != ring_stats->deq_fail_bulk)
		goto fail;
	if (ring_stats->enq_fail_objs    != ring_stats->deq_fail_objs)
		goto fail;


	/* Watermark Tests. */
	printf("Test the watermark/quota stats.\n");

	printf("Verify the initial watermark stats.\n");
	/* Watermark stats should be 0 since there is no watermark. */
	if (ring_stats->enq_quota_bulk != 0)
		goto fail;
	if (ring_stats->enq_quota_objs != 0)
		goto fail;

	/* Set a watermark. */
	rte_ring_set_water_mark(r, 16);

	/* Reset pointers. */
	cur_src = src;
	cur_dst = dst;

	last_enqueue_ops   = ring_stats->enq_success_bulk;
	last_enqueue_items = ring_stats->enq_success_objs;


	printf("Test stats for SP burst enqueue below watermark.\n");
	num_items = 8;
	ret = rte_ring_sp_enqueue_burst(r, cur_src, num_items);
	if ((ret & RTE_RING_SZ_MASK) != num_items)
		goto fail;

	/* Watermark stats should still be 0. */
	if (ring_stats->enq_quota_bulk != 0)
		goto fail;
	if (ring_stats->enq_quota_objs != 0)
		goto fail;

	/* Success stats should have increased. */
	if (ring_stats->enq_success_bulk != last_enqueue_ops + 1)
		goto fail;
	if (ring_stats->enq_success_objs != last_enqueue_items + num_items)
		goto fail;

	last_enqueue_ops   = ring_stats->enq_success_bulk;
	last_enqueue_items = ring_stats->enq_success_objs;


	printf("Test stats for SP burst enqueue at watermark.\n");
	num_items = 8;
	ret = rte_ring_sp_enqueue_burst(r, cur_src, num_items);
	if ((ret & RTE_RING_SZ_MASK) != num_items)
		goto fail;

	/* Watermark stats should have changed. */
	if (ring_stats->enq_quota_bulk != 1)
		goto fail;
	if (ring_stats->enq_quota_objs != num_items)
		goto fail;

	last_quota_ops   = ring_stats->enq_quota_bulk;
	last_quota_items = ring_stats->enq_quota_objs;


	printf("Test stats for SP burst enqueue above watermark.\n");
	num_items = 1;
	ret = rte_ring_sp_enqueue_burst(r, cur_src, num_items);
	if ((ret & RTE_RING_SZ_MASK) != num_items)
		goto fail;

	/* Watermark stats should have changed. */
	if (ring_stats->enq_quota_bulk != last_quota_ops +1)
		goto fail;
	if (ring_stats->enq_quota_objs != last_quota_items + num_items)
		goto fail;

	last_quota_ops   = ring_stats->enq_quota_bulk;
	last_quota_items = ring_stats->enq_quota_objs;


	printf("Test stats for MP burst enqueue above watermark.\n");
	num_items = 2;
	ret = rte_ring_mp_enqueue_burst(r, cur_src, num_items);
	if ((ret & RTE_RING_SZ_MASK) != num_items)
		goto fail;

	/* Watermark stats should have changed. */
	if (ring_stats->enq_quota_bulk != last_quota_ops +1)
		goto fail;
	if (ring_stats->enq_quota_objs != last_quota_items + num_items)
		goto fail;

	last_quota_ops   = ring_stats->enq_quota_bulk;
	last_quota_items = ring_stats->enq_quota_objs;


	printf("Test stats for SP bulk enqueue above watermark.\n");
	num_items = 4;
	ret = rte_ring_sp_enqueue_bulk(r, cur_src, num_items);
	if (ret != -EDQUOT)
		goto fail;

	/* Watermark stats should have changed. */
	if (ring_stats->enq_quota_bulk != last_quota_ops +1)
		goto fail;
	if (ring_stats->enq_quota_objs != last_quota_items + num_items)
		goto fail;

	last_quota_ops   = ring_stats->enq_quota_bulk;
	last_quota_items = ring_stats->enq_quota_objs;


	printf("Test stats for MP bulk enqueue above watermark.\n");
	num_items = 8;
	ret = rte_ring_mp_enqueue_bulk(r, cur_src, num_items);
	if (ret != -EDQUOT)
		goto fail;

	/* Watermark stats should have changed. */
	if (ring_stats->enq_quota_bulk != last_quota_ops +1)
		goto fail;
	if (ring_stats->enq_quota_objs != last_quota_items + num_items)
		goto fail;

	printf("Test watermark success stats.\n");
	/* Success stats should be same as last non-watermarked enqueue. */
	if (ring_stats->enq_success_bulk != last_enqueue_ops)
		goto fail;
	if (ring_stats->enq_success_objs != last_enqueue_items)
		goto fail;


	/* Cleanup. */

	/* Empty the ring. */
	for (i = 0; i<RING_SIZE/MAX_BULK; i++) {
		rte_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK);
		cur_dst += MAX_BULK;
	}

	/* Reset the watermark. */
	rte_ring_set_water_mark(r, 0);

	/* Reset the ring stats. */
	memset(&r->stats[lcore_id], 0, sizeof(r->stats[lcore_id]));

	/* Free memory before test completed */
	if (src)
		free(src);
	if (dst)
		free(dst);
	return 0;

fail:
	if (src)
		free(src);
	if (dst)
		free(dst);
	return -1;
#endif
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

	obj = (void **)rte_zmalloc("test_ring_basic_ex_malloc", (RING_SIZE * sizeof(void *)), 0);
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
	ret = rte_ring_enqueue_burst(rp, obj, 2);
	if ((ret & RTE_RING_SZ_MASK) != 2) {
		printf("test_ring_basic_ex: rte_ring_enqueue_burst fails \n");
		goto fail_test;
	}

	ret = rte_ring_dequeue_burst(rp, obj, 2);
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

int
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

	/* ring stats */
	if (test_ring_stats() < 0)
		return -1;

	/* basic operations */
	if (test_live_watermark_change() < 0)
		return -1;

	if ( test_set_watermark() < 0){
		printf ("Test failed to detect invalid parameter\n");
		return -1;
	}
	else
		printf ( "Test detected forced bad watermark values\n");

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
