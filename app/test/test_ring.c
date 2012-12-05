/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 *  version: DPDK.L.1.2.3-3
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

#include <cmdline_parse.h>

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
 * #. Check quota and watermark
 *
 *    - Start a loop on another lcore that will enqueue and dequeue
 *      objects in a ring. It will monitor the value of quota (default
 *      bulk count) and watermark.
 *    - At the same time, change the quota and the watermark on the
 *      master lcore.
 *    - The slave lcore will check that bulk count changes from 4 to
 *      8, and watermark changes from 16 to 32.
 *
 * #. Performance tests.
 *
 *    This test is done on the following configurations:
 *
 *    - One core enqueuing, one core dequeuing
 *    - One core enqueuing, other cores dequeuing
 *    - One core dequeuing, other cores enqueuing
 *    - Half of the cores enqueuing, the other half dequeuing
 *
 *    When only one core enqueues/dequeues, the test is done with the
 *    SP/SC functions in addition to the MP/MC functions.
 *
 *    The test is done with different bulk size.
 *
 *    On each core, the test enqueues or dequeues objects during
 *    TIME_S seconds. The number of successes and failures are stored on
 *    each core, then summed and displayed.
 *
 *    The test checks that the number of enqueues is equal to the
 *    number of dequeues.
 */

#define RING_SIZE 4096
#define MAX_BULK 32
#define N 65536
#define TIME_S 5

static rte_atomic32_t synchro;

static unsigned bulk_enqueue;
static unsigned bulk_dequeue;
static struct rte_ring *r;

struct test_stats {
	unsigned enq_success ;
	unsigned enq_quota;
	unsigned enq_fail;

	unsigned deq_success;
	unsigned deq_fail;
} __rte_cache_aligned;

static struct test_stats test_stats[RTE_MAX_LCORE];

#define DEFINE_ENQUEUE_FUNCTION(name, enq_code)			\
static int							\
name(__attribute__((unused)) void *arg)				\
{								\
	unsigned success = 0;					\
	unsigned quota = 0;					\
	unsigned fail = 0;					\
	unsigned i;						\
	unsigned long dummy_obj;				\
	void *obj_table[MAX_BULK];				\
	int ret;						\
	unsigned lcore_id = rte_lcore_id();			\
	uint64_t start_cycles, end_cycles;			\
	uint64_t time_diff = 0, hz = rte_get_hpet_hz();		\
								\
	/* init dummy object table */				\
	for (i = 0; i< MAX_BULK; i++) {				\
		dummy_obj = lcore_id + 0x1000 + i;		\
		obj_table[i] = (void *)dummy_obj;		\
	}							\
								\
	/* wait synchro for slaves */				\
	if (lcore_id != rte_get_master_lcore())			\
		while (rte_atomic32_read(&synchro) == 0);	\
								\
	start_cycles = rte_get_hpet_cycles();			\
								\
	/* enqueue as many object as possible */		\
	while (time_diff/hz < TIME_S) {				\
		for (i = 0; likely(i < N); i++) {		\
			ret = enq_code;				\
			if (ret == 0)				\
				success++;			\
			else if (ret == -EDQUOT)		\
				quota++;			\
			else					\
				fail++;				\
		}						\
		end_cycles = rte_get_hpet_cycles();		\
		time_diff = end_cycles - start_cycles;		\
	}							\
								\
	/* write statistics in a shared structure */		\
	test_stats[lcore_id].enq_success = success;		\
	test_stats[lcore_id].enq_quota = quota;			\
	test_stats[lcore_id].enq_fail = fail;			\
								\
	return 0;						\
}

#define DEFINE_DEQUEUE_FUNCTION(name, deq_code)			\
static int							\
name(__attribute__((unused)) void *arg)				\
{								\
	unsigned success = 0;					\
	unsigned fail = 0;					\
	unsigned i;						\
	void *obj_table[MAX_BULK];				\
	int ret;						\
	unsigned lcore_id = rte_lcore_id();			\
	uint64_t start_cycles, end_cycles;			\
	uint64_t time_diff = 0, hz = rte_get_hpet_hz();		\
								\
	/* wait synchro for slaves */				\
	if (lcore_id != rte_get_master_lcore())			\
		while (rte_atomic32_read(&synchro) == 0);	\
								\
	start_cycles = rte_get_hpet_cycles();			\
								\
	/* dequeue as many object as possible */		\
	while (time_diff/hz < TIME_S) {				\
		for (i = 0; likely(i < N); i++) {		\
			ret = deq_code;				\
			if (ret == 0)				\
				success++;			\
			else					\
				fail++;				\
		}						\
		end_cycles = rte_get_hpet_cycles();		\
		time_diff = end_cycles - start_cycles;		\
	}							\
								\
	/* write statistics in a shared structure */		\
	test_stats[lcore_id].deq_success = success;		\
	test_stats[lcore_id].deq_fail = fail;			\
								\
	return 0;						\
}

DEFINE_ENQUEUE_FUNCTION(test_ring_per_core_sp_enqueue,
			rte_ring_sp_enqueue_bulk(r, obj_table, bulk_enqueue))

DEFINE_DEQUEUE_FUNCTION(test_ring_per_core_sc_dequeue,
			rte_ring_sc_dequeue_bulk(r, obj_table, bulk_dequeue))

DEFINE_ENQUEUE_FUNCTION(test_ring_per_core_mp_enqueue,
			rte_ring_mp_enqueue_bulk(r, obj_table, bulk_enqueue))

DEFINE_DEQUEUE_FUNCTION(test_ring_per_core_mc_dequeue,
			rte_ring_mc_dequeue_bulk(r, obj_table, bulk_dequeue))

#define	TEST_RING_VERIFY(exp)						\
	if (!(exp)) {							\
		printf("error at %s:%d\tcondition " #exp " failed\n",	\
		    __func__, __LINE__);				\
		rte_ring_dump(r);					\
		return (-1);						\
	}

#define	TEST_RING_FULL_EMTPY_ITER	8


static int
launch_cores(unsigned enq_core_count, unsigned deq_core_count, int sp, int sc)
{
	void *obj;
	unsigned lcore_id;
	unsigned rate, deq_remain = 0;
	unsigned enq_total, deq_total;
	struct test_stats sum;
	int (*enq_f)(void *);
	int (*deq_f)(void *);
	unsigned cores = enq_core_count + deq_core_count;
	int ret;

	rte_atomic32_set(&synchro, 0);

	printf("ring_autotest e/d_core=%u,%u e/d_bulk=%u,%u ",
	       enq_core_count, deq_core_count, bulk_enqueue, bulk_dequeue);
	printf("sp=%d sc=%d ", sp, sc);

	/* set enqueue function to be used */
	if (sp)
		enq_f = test_ring_per_core_sp_enqueue;
	else
		enq_f = test_ring_per_core_mp_enqueue;

	/* set dequeue function to be used */
	if (sc)
		deq_f = test_ring_per_core_sc_dequeue;
	else
		deq_f = test_ring_per_core_mc_dequeue;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (enq_core_count != 0) {
			enq_core_count--;
			rte_eal_remote_launch(enq_f, NULL, lcore_id);
		}
		if (deq_core_count != 1) {
			deq_core_count--;
			rte_eal_remote_launch(deq_f, NULL, lcore_id);
		}
	}

	memset(test_stats, 0, sizeof(test_stats));

	/* start synchro and launch test on master */
	rte_atomic32_set(&synchro, 1);
	ret = deq_f(NULL);

	/* wait all cores */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (cores == 1)
			break;
		cores--;
		if (rte_eal_wait_lcore(lcore_id) < 0)
			ret = -1;
	}

	memset(&sum, 0, sizeof(sum));
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		sum.enq_success += test_stats[lcore_id].enq_success;
		sum.enq_quota += test_stats[lcore_id].enq_quota;
		sum.enq_fail += test_stats[lcore_id].enq_fail;
		sum.deq_success += test_stats[lcore_id].deq_success;
		sum.deq_fail += test_stats[lcore_id].deq_fail;
	}

	/* empty the ring */
	while (rte_ring_sc_dequeue(r, &obj) == 0)
		deq_remain += 1;

	if (ret < 0) {
		printf("per-lcore test returned -1\n");
		return -1;
	}

	enq_total = (sum.enq_success * bulk_enqueue) +
		(sum.enq_quota * bulk_enqueue);
	deq_total = (sum.deq_success * bulk_dequeue) + deq_remain;

	rate = deq_total/TIME_S;

	printf("rate_persec=%u\n", rate);

	if (enq_total != deq_total) {
		printf("invalid enq/deq_success counter: %u %u\n",
		       enq_total, deq_total);
		return -1;
	}

	return 0;
}

static int
do_one_ring_test2(unsigned enq_core_count, unsigned deq_core_count,
		  unsigned n_enq_bulk, unsigned n_deq_bulk)
{
	int sp, sc;
	int do_sp, do_sc;
	int ret;

	bulk_enqueue = n_enq_bulk;
	bulk_dequeue = n_deq_bulk;

	do_sp = (enq_core_count == 1) ? 1 : 0;
	do_sc = (deq_core_count  == 1) ? 1 : 0;

	for (sp = 0; sp <= do_sp; sp ++) {
		for (sc = 0; sc <= do_sc; sc ++) {
			ret = launch_cores(enq_core_count,
					   deq_core_count,
					   sp, sc);
			if (ret < 0)
				return -1;
		}
	}
	return 0;
}

static int
do_one_ring_test(unsigned enq_core_count, unsigned deq_core_count)
{
	unsigned bulk_enqueue_tab[] = { 1, 2, 4, 32, 0 };
	unsigned bulk_dequeue_tab[] = { 1, 2, 4, 32, 0 };
	unsigned *bulk_enqueue_ptr;
	unsigned *bulk_dequeue_ptr;
	int ret;

	for (bulk_enqueue_ptr = bulk_enqueue_tab;
	     *bulk_enqueue_ptr;
	     bulk_enqueue_ptr++) {

		for (bulk_dequeue_ptr = bulk_dequeue_tab;
		     *bulk_dequeue_ptr;
		     bulk_dequeue_ptr++) {

			ret = do_one_ring_test2(enq_core_count, deq_core_count,
						*bulk_enqueue_ptr,
						*bulk_dequeue_ptr);
			if (ret < 0)
				return -1;
		}
	}
	return 0;
}

static int
check_quota_and_watermark(__attribute__((unused)) void *dummy)
{
	uint64_t hz = rte_get_hpet_hz();
	void *obj_table[MAX_BULK];
	unsigned watermark, watermark_old = 16;
	uint64_t cur_time, end_time;
	int64_t diff = 0;
	int i, ret;
	unsigned quota, quota_old = 4;

	/* init the object table */
	memset(obj_table, 0, sizeof(obj_table));
	end_time = rte_get_hpet_cycles() + (hz * 2);

	/* check that bulk and watermark are 4 and 32 (respectively) */
	while (diff >= 0) {

		/* read quota, the only change allowed is from 4 to 8 */
		quota = rte_ring_get_bulk_count(r);
		if (quota != quota_old && (quota_old != 4 || quota != 8)) {
			printf("Bad quota change %u -> %u\n", quota_old,
			       quota);
			return -1;
		}
		quota_old = quota;

		/* add in ring until we reach watermark */
		ret = 0;
		for (i = 0; i < 16; i ++) {
			if (ret != 0)
				break;
			ret = rte_ring_enqueue_bulk(r, obj_table, quota);
		}

		if (ret != -EDQUOT) {
			printf("Cannot enqueue objects, or watermark not "
			       "reached (ret=%d)\n", ret);
			return -1;
		}

		/* read watermark, the only change allowed is from 16 to 32 */
		watermark = i * quota;
		if (watermark != watermark_old &&
		    (watermark_old != 16 || watermark != 32)) {
			printf("Bad watermark change %u -> %u\n", watermark_old,
			       watermark);
			return -1;
		}
		watermark_old = watermark;

		/* dequeue objects from ring */
		while (i--) {
			ret = rte_ring_dequeue_bulk(r, obj_table, quota);
			if (ret != 0) {
				printf("Cannot dequeue (ret=%d)\n", ret);
				return -1;
			}
		}

		cur_time = rte_get_hpet_cycles();
		diff = end_time - cur_time;
	}

	if (watermark_old != 32 || quota_old != 8) {
		printf("quota or watermark was not updated (q=%u wm=%u)\n",
		       quota_old, watermark_old);
		return -1;
	}

	return 0;
}

static int
test_quota_and_watermark(void)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned lcore_id2 = rte_get_next_lcore(lcore_id, 0, 1);

	printf("Test quota and watermark live modification\n");

	rte_ring_set_bulk_count(r, 4);
	rte_ring_set_water_mark(r, 16);

	/* launch a thread that will enqueue and dequeue, checking
	 * watermark and quota */
	rte_eal_remote_launch(check_quota_and_watermark, NULL, lcore_id2);

	rte_delay_ms(1000);
	rte_ring_set_bulk_count(r, 8);
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
		rte_ring_dump(r);
	}
	return (0);
}

static int
test_ring_basic(void)
{
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned i, n;

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
		test_hexdump("src", src, cur_src - src);
		test_hexdump("dst", dst, cur_dst - dst);
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
		test_hexdump("src", src, cur_src - src);
		test_hexdump("dst", dst, cur_dst - dst);
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
		test_hexdump("src", src, cur_src - src);
		test_hexdump("dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

	if (test_ring_basic_full_empty(src, dst) != 0)
		goto fail;

	cur_src = src;
	cur_dst = dst;

	printf("test watermark and default bulk enqueue / dequeue\n");
	rte_ring_set_bulk_count(r, 16);
	rte_ring_set_water_mark(r, 20);
	n = rte_ring_get_bulk_count(r);
	if (n != 16) {
		printf("rte_ring_get_bulk_count() returned %u instead "
		       "of 16\n", n);
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;
	ret = rte_ring_enqueue_bulk(r, cur_src, n);
	cur_src += 16;
	if (ret != 0) {
		printf("Cannot enqueue\n");
		goto fail;
	}
	ret = rte_ring_enqueue_bulk(r, cur_src, n);
	cur_src += 16;
	if (ret != -EDQUOT) {
		printf("Watermark not exceeded\n");
		goto fail;
	}
	ret = rte_ring_dequeue_bulk(r, cur_dst, n);
	cur_dst += 16;
	if (ret != 0) {
		printf("Cannot dequeue\n");
		goto fail;
	}
	ret = rte_ring_dequeue_bulk(r, cur_dst, n);
	cur_dst += 16;
	if (ret != 0) {
		printf("Cannot dequeue2\n");
		goto fail;
	}

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		test_hexdump("src", src, cur_src - src);
		test_hexdump("dst", dst, cur_dst - dst);
		printf("data after dequeue is not the same\n");
		goto fail;
	}

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

/*
 * it will always fail to create ring with a wrong ring size number in this function
 */
static int
test_ring_creation_with_wrong_size(void)
{
	struct rte_ring * rp = NULL;

	rp = rte_ring_create("test_bad_ring_size", RING_SIZE+1, SOCKET_ID_ANY, 0);
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

	rp = rte_ring_create("test_ring_basic_ex", RING_SIZE, SOCKET_ID_ANY, 0);
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

	ret = 0;
fail_test:
	if (obj != NULL)
		rte_free(obj);

	return ret;
}

int
test_ring(void)
{
	unsigned enq_core_count, deq_core_count;

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

	/* basic operations */
	if (test_ring_basic() < 0)
		return -1;

	/* basic operations */
	if (test_quota_and_watermark() < 0)
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


	printf("start performance tests\n");

	/* one lcore for enqueue, one for dequeue */
	enq_core_count = 1;
	deq_core_count = 1;
	if (do_one_ring_test(enq_core_count, deq_core_count) < 0)
		return -1;

	/* max cores for enqueue, one for dequeue */
	enq_core_count = rte_lcore_count() - 1;
	deq_core_count = 1;
	if (do_one_ring_test(enq_core_count, deq_core_count) < 0)
		return -1;

	/* max cores for dequeue, one for enqueue */
	enq_core_count = 1;
	deq_core_count = rte_lcore_count() - 1;
	if (do_one_ring_test(enq_core_count, deq_core_count) < 0)
		return -1;

	/* half for enqueue and half for dequeue */
	enq_core_count = rte_lcore_count() / 2;
	deq_core_count = rte_lcore_count() / 2;
	if (do_one_ring_test(enq_core_count, deq_core_count) < 0)
		return -1;

	/* test of creating ring with wrong size */
	if (test_ring_creation_with_wrong_size() < 0)
		return -1;

	/* test of creation ring with an used name */
	if (test_ring_creation_with_an_used_name() < 0)
		return -1;

	/* dump the ring status */
	rte_ring_list_dump();

	return 0;
}
