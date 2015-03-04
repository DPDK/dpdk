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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>

#include "test.h"

/*
 * Mempool performance
 * =======
 *
 *    Each core get *n_keep* objects per bulk of *n_get_bulk*. Then,
 *    objects are put back in the pool per bulk of *n_put_bulk*.
 *
 *    This sequence is done during TIME_S seconds.
 *
 *    This test is done on the following configurations:
 *
 *    - Cores configuration (*cores*)
 *
 *      - One core with cache
 *      - Two cores with cache
 *      - Max. cores with cache
 *      - One core without cache
 *      - Two cores without cache
 *      - Max. cores without cache
 *
 *    - Bulk size (*n_get_bulk*, *n_put_bulk*)
 *
 *      - Bulk get from 1 to 32
 *      - Bulk put from 1 to 32
 *
 *    - Number of kept objects (*n_keep*)
 *
 *      - 32
 *      - 128
 */

#define N 65536
#define TIME_S 5
#define MEMPOOL_ELT_SIZE 2048
#define MAX_KEEP 128
#define MEMPOOL_SIZE ((RTE_MAX_LCORE*(MAX_KEEP+RTE_MEMPOOL_CACHE_MAX_SIZE))-1)

static struct rte_mempool *mp;
static struct rte_mempool *mp_cache, *mp_nocache;

static rte_atomic32_t synchro;

/* number of objects in one bulk operation (get or put) */
static unsigned n_get_bulk;
static unsigned n_put_bulk;

/* number of objects retrived from mempool before putting them back */
static unsigned n_keep;

/* number of enqueues / dequeues */
struct mempool_test_stats {
	unsigned enq_count;
} __rte_cache_aligned;

static struct mempool_test_stats stats[RTE_MAX_LCORE];

/*
 * save the object number in the first 4 bytes of object data. All
 * other bytes are set to 0.
 */
static void
my_obj_init(struct rte_mempool *mp, __attribute__((unused)) void *arg,
	    void *obj, unsigned i)
{
	uint32_t *objnum = obj;
	memset(obj, 0, mp->elt_size);
	*objnum = i;
}

static int
per_lcore_mempool_test(__attribute__((unused)) void *arg)
{
	void *obj_table[MAX_KEEP];
	unsigned i, idx;
	unsigned lcore_id = rte_lcore_id();
	int ret;
	uint64_t start_cycles, end_cycles;
	uint64_t time_diff = 0, hz = rte_get_timer_hz();

	/* n_get_bulk and n_put_bulk must be divisors of n_keep */
	if (((n_keep / n_get_bulk) * n_get_bulk) != n_keep)
		return -1;
	if (((n_keep / n_put_bulk) * n_put_bulk) != n_keep)
		return -1;

	stats[lcore_id].enq_count = 0;

	/* wait synchro for slaves */
	if (lcore_id != rte_get_master_lcore())
		while (rte_atomic32_read(&synchro) == 0);

	start_cycles = rte_get_timer_cycles();

	while (time_diff/hz < TIME_S) {
		for (i = 0; likely(i < (N/n_keep)); i++) {
			/* get n_keep objects by bulk of n_bulk */
			idx = 0;
			while (idx < n_keep) {
				ret = rte_mempool_get_bulk(mp, &obj_table[idx],
							   n_get_bulk);
				if (unlikely(ret < 0)) {
					rte_mempool_dump(stdout, mp);
					rte_ring_dump(stdout, mp->ring);
					/* in this case, objects are lost... */
					return -1;
				}
				idx += n_get_bulk;
			}

			/* put the objects back */
			idx = 0;
			while (idx < n_keep) {
				rte_mempool_put_bulk(mp, &obj_table[idx],
						     n_put_bulk);
				idx += n_put_bulk;
			}
		}
		end_cycles = rte_get_timer_cycles();
		time_diff = end_cycles - start_cycles;
		stats[lcore_id].enq_count += N;
	}

	return 0;
}

/* launch all the per-lcore test, and display the result */
static int
launch_cores(unsigned cores)
{
	unsigned lcore_id;
	unsigned rate;
	int ret;
	unsigned cores_save = cores;

	rte_atomic32_set(&synchro, 0);

	/* reset stats */
	memset(stats, 0, sizeof(stats));

	printf("mempool_autotest cache=%u cores=%u n_get_bulk=%u "
	       "n_put_bulk=%u n_keep=%u ",
	       (unsigned) mp->cache_size, cores, n_get_bulk, n_put_bulk, n_keep);

	if (rte_mempool_count(mp) != MEMPOOL_SIZE) {
		printf("mempool is not full\n");
		return -1;
	}

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (cores == 1)
			break;
		cores--;
		rte_eal_remote_launch(per_lcore_mempool_test,
				      NULL, lcore_id);
	}

	/* start synchro and launch test on master */
	rte_atomic32_set(&synchro, 1);

	ret = per_lcore_mempool_test(NULL);

	cores = cores_save;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (cores == 1)
			break;
		cores--;
		if (rte_eal_wait_lcore(lcore_id) < 0)
			ret = -1;
	}

	if (ret < 0) {
		printf("per-lcore test returned -1\n");
		return -1;
	}

	rate = 0;
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
		rate += (stats[lcore_id].enq_count / TIME_S);

	printf("rate_persec=%u\n", rate);

	return 0;
}

/* for a given number of core, launch all test cases */
static int
do_one_mempool_test(unsigned cores)
{
	unsigned bulk_tab_get[] = { 1, 4, 32, 0 };
	unsigned bulk_tab_put[] = { 1, 4, 32, 0 };
	unsigned keep_tab[] = { 32, 128, 0 };
	unsigned *get_bulk_ptr;
	unsigned *put_bulk_ptr;
	unsigned *keep_ptr;
	int ret;

	for (get_bulk_ptr = bulk_tab_get; *get_bulk_ptr; get_bulk_ptr++) {
		for (put_bulk_ptr = bulk_tab_put; *put_bulk_ptr; put_bulk_ptr++) {
			for (keep_ptr = keep_tab; *keep_ptr; keep_ptr++) {

				n_get_bulk = *get_bulk_ptr;
				n_put_bulk = *put_bulk_ptr;
				n_keep = *keep_ptr;
				ret = launch_cores(cores);

				if (ret < 0)
					return -1;
			}
		}
	}
	return 0;
}

static int
test_mempool_perf(void)
{
	rte_atomic32_init(&synchro);

	/* create a mempool (without cache) */
	if (mp_nocache == NULL)
		mp_nocache = rte_mempool_create("perf_test_nocache", MEMPOOL_SIZE,
						MEMPOOL_ELT_SIZE, 0, 0,
						NULL, NULL,
						my_obj_init, NULL,
						SOCKET_ID_ANY, 0);
	if (mp_nocache == NULL)
		return -1;

	/* create a mempool (with cache) */
	if (mp_cache == NULL)
		mp_cache = rte_mempool_create("perf_test_cache", MEMPOOL_SIZE,
					      MEMPOOL_ELT_SIZE,
					      RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
					      NULL, NULL,
					      my_obj_init, NULL,
					      SOCKET_ID_ANY, 0);
	if (mp_cache == NULL)
		return -1;

	/* performance test with 1, 2 and max cores */
	printf("start performance test (without cache)\n");
	mp = mp_nocache;

	if (do_one_mempool_test(1) < 0)
		return -1;

	if (do_one_mempool_test(2) < 0)
		return -1;

	if (do_one_mempool_test(rte_lcore_count()) < 0)
		return -1;

	/* performance test with 1, 2 and max cores */
	printf("start performance test (with cache)\n");
	mp = mp_cache;

	if (do_one_mempool_test(1) < 0)
		return -1;

	if (do_one_mempool_test(2) < 0)
		return -1;

	if (do_one_mempool_test(rte_lcore_count()) < 0)
		return -1;

	rte_mempool_list_dump(stdout);

	return 0;
}

static struct test_command mempool_perf_cmd = {
	.command = "mempool_perf_autotest",
	.callback = test_mempool_perf,
};
REGISTER_TEST_COMMAND(mempool_perf_cmd);
