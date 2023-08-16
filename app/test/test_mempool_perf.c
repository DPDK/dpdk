/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2022 SmartShare Systems
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
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>

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
 *      - One core with user-owned cache
 *      - Two cores with user-owned cache
 *      - Max. cores with user-owned cache
 *
 *    - Bulk size (*n_get_bulk*, *n_put_bulk*)
 *
 *      - Bulk get from 1 to 32
 *      - Bulk put from 1 to 32
 *      - Bulk get and put from 1 to 32, compile time constant
 *
 *    - Number of kept objects (*n_keep*)
 *
 *      - 32
 *      - 128
 *      - 512
 */

#define N 65536
#define TIME_S 5
#define MEMPOOL_ELT_SIZE 2048
#define MAX_KEEP 512
#define MEMPOOL_SIZE ((rte_lcore_count()*(MAX_KEEP+RTE_MEMPOOL_CACHE_MAX_SIZE))-1)

/* Number of pointers fitting into one cache line. */
#define CACHE_LINE_BURST (RTE_CACHE_LINE_SIZE / sizeof(uintptr_t))

#define LOG_ERR() printf("test failed at %s():%d\n", __func__, __LINE__)
#define RET_ERR() do {							\
		LOG_ERR();						\
		return -1;						\
	} while (0)
#define GOTO_ERR(var, label) do {					\
		LOG_ERR();						\
		var = -1;						\
		goto label;						\
	} while (0)

static int use_external_cache;
static unsigned external_cache_size = RTE_MEMPOOL_CACHE_MAX_SIZE;

static uint32_t synchro;

/* number of objects in one bulk operation (get or put) */
static unsigned n_get_bulk;
static unsigned n_put_bulk;

/* number of objects retrieved from mempool before putting them back */
static unsigned n_keep;

/* true if we want to test with constant n_get_bulk and n_put_bulk */
static int use_constant_values;

/* number of enqueues / dequeues */
struct mempool_test_stats {
	uint64_t enq_count;
} __rte_cache_aligned;

static struct mempool_test_stats stats[RTE_MAX_LCORE];

/*
 * save the object number in the first 4 bytes of object data. All
 * other bytes are set to 0.
 */
static void
my_obj_init(struct rte_mempool *mp, __rte_unused void *arg,
	    void *obj, unsigned i)
{
	uint32_t *objnum = obj;
	memset(obj, 0, mp->elt_size);
	*objnum = i;
}

static __rte_always_inline int
test_loop(struct rte_mempool *mp, struct rte_mempool_cache *cache,
	  unsigned int x_keep, unsigned int x_get_bulk, unsigned int x_put_bulk)
{
	void *obj_table[MAX_KEEP] __rte_cache_aligned;
	unsigned int idx;
	unsigned int i;
	int ret;

	for (i = 0; likely(i < (N / x_keep)); i++) {
		/* get x_keep objects by bulk of x_get_bulk */
		for (idx = 0; idx < x_keep; idx += x_get_bulk) {
			ret = rte_mempool_generic_get(mp,
						      &obj_table[idx],
						      x_get_bulk,
						      cache);
			if (unlikely(ret < 0)) {
				rte_mempool_dump(stdout, mp);
				return ret;
			}
		}

		/* put the objects back by bulk of x_put_bulk */
		for (idx = 0; idx < x_keep; idx += x_put_bulk) {
			rte_mempool_generic_put(mp,
						&obj_table[idx],
						x_put_bulk,
						cache);
		}
	}

	return 0;
}

static int
per_lcore_mempool_test(void *arg)
{
	struct rte_mempool *mp = arg;
	unsigned lcore_id = rte_lcore_id();
	int ret = 0;
	uint64_t start_cycles, end_cycles;
	uint64_t time_diff = 0, hz = rte_get_timer_hz();
	struct rte_mempool_cache *cache;

	if (use_external_cache) {
		/* Create a user-owned mempool cache. */
		cache = rte_mempool_cache_create(external_cache_size,
						 SOCKET_ID_ANY);
		if (cache == NULL)
			RET_ERR();
	} else {
		/* May be NULL if cache is disabled. */
		cache = rte_mempool_default_cache(mp, lcore_id);
	}

	/* n_get_bulk and n_put_bulk must be divisors of n_keep */
	if (((n_keep / n_get_bulk) * n_get_bulk) != n_keep)
		GOTO_ERR(ret, out);
	if (((n_keep / n_put_bulk) * n_put_bulk) != n_keep)
		GOTO_ERR(ret, out);
	/* for constant n, n_get_bulk and n_put_bulk must be the same */
	if (use_constant_values && n_put_bulk != n_get_bulk)
		GOTO_ERR(ret, out);

	stats[lcore_id].enq_count = 0;

	/* wait synchro for workers */
	if (lcore_id != rte_get_main_lcore())
		rte_wait_until_equal_32(&synchro, 1, __ATOMIC_RELAXED);

	start_cycles = rte_get_timer_cycles();

	while (time_diff/hz < TIME_S) {
		if (!use_constant_values)
			ret = test_loop(mp, cache, n_keep, n_get_bulk, n_put_bulk);
		else if (n_get_bulk == 1)
			ret = test_loop(mp, cache, n_keep, 1, 1);
		else if (n_get_bulk == 4)
			ret = test_loop(mp, cache, n_keep, 4, 4);
		else if (n_get_bulk == CACHE_LINE_BURST)
			ret = test_loop(mp, cache, n_keep,
					CACHE_LINE_BURST, CACHE_LINE_BURST);
		else if (n_get_bulk == 32)
			ret = test_loop(mp, cache, n_keep, 32, 32);
		else
			ret = -1;

		if (ret < 0)
			GOTO_ERR(ret, out);

		end_cycles = rte_get_timer_cycles();
		time_diff = end_cycles - start_cycles;
		stats[lcore_id].enq_count += N;
	}

out:
	if (use_external_cache) {
		rte_mempool_cache_flush(cache, mp);
		rte_mempool_cache_free(cache);
	}

	return ret;
}

/* launch all the per-lcore test, and display the result */
static int
launch_cores(struct rte_mempool *mp, unsigned int cores)
{
	unsigned lcore_id;
	uint64_t rate;
	int ret;
	unsigned cores_save = cores;

	__atomic_store_n(&synchro, 0, __ATOMIC_RELAXED);

	/* reset stats */
	memset(stats, 0, sizeof(stats));

	printf("mempool_autotest cache=%u cores=%u n_get_bulk=%u "
	       "n_put_bulk=%u n_keep=%u constant_n=%u ",
	       use_external_cache ?
		   external_cache_size : (unsigned) mp->cache_size,
	       cores, n_get_bulk, n_put_bulk, n_keep, use_constant_values);

	if (rte_mempool_avail_count(mp) != MEMPOOL_SIZE) {
		printf("mempool is not full\n");
		return -1;
	}

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (cores == 1)
			break;
		cores--;
		rte_eal_remote_launch(per_lcore_mempool_test,
				      mp, lcore_id);
	}

	/* start synchro and launch test on main */
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);

	ret = per_lcore_mempool_test(mp);

	cores = cores_save;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
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

	printf("rate_persec=%" PRIu64 "\n", rate);

	return 0;
}

/* for a given number of core, launch all test cases */
static int
do_one_mempool_test(struct rte_mempool *mp, unsigned int cores)
{
	unsigned int bulk_tab_get[] = { 1, 4, CACHE_LINE_BURST, 32, 0 };
	unsigned int bulk_tab_put[] = { 1, 4, CACHE_LINE_BURST, 32, 0 };
	unsigned int keep_tab[] = { 32, 128, 512, 0 };
	unsigned *get_bulk_ptr;
	unsigned *put_bulk_ptr;
	unsigned *keep_ptr;
	int ret;

	for (get_bulk_ptr = bulk_tab_get; *get_bulk_ptr; get_bulk_ptr++) {
		for (put_bulk_ptr = bulk_tab_put; *put_bulk_ptr; put_bulk_ptr++) {
			for (keep_ptr = keep_tab; *keep_ptr; keep_ptr++) {

				use_constant_values = 0;
				n_get_bulk = *get_bulk_ptr;
				n_put_bulk = *put_bulk_ptr;
				n_keep = *keep_ptr;
				ret = launch_cores(mp, cores);
				if (ret < 0)
					return -1;

				/* replay test with constant values */
				if (n_get_bulk == n_put_bulk) {
					use_constant_values = 1;
					ret = launch_cores(mp, cores);
					if (ret < 0)
						return -1;
				}
			}
		}
	}
	return 0;
}

static int
test_mempool_perf(void)
{
	struct rte_mempool *mp_cache = NULL;
	struct rte_mempool *mp_nocache = NULL;
	struct rte_mempool *default_pool = NULL;
	const char *default_pool_ops;
	int ret = -1;

	/* create a mempool (without cache) */
	mp_nocache = rte_mempool_create("perf_test_nocache", MEMPOOL_SIZE,
					MEMPOOL_ELT_SIZE, 0, 0,
					NULL, NULL,
					my_obj_init, NULL,
					SOCKET_ID_ANY, 0);
	if (mp_nocache == NULL)
		goto err;

	/* create a mempool (with cache) */
	mp_cache = rte_mempool_create("perf_test_cache", MEMPOOL_SIZE,
				      MEMPOOL_ELT_SIZE,
				      RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
				      NULL, NULL,
				      my_obj_init, NULL,
				      SOCKET_ID_ANY, 0);
	if (mp_cache == NULL)
		goto err;

	default_pool_ops = rte_mbuf_best_mempool_ops();
	/* Create a mempool based on Default handler */
	default_pool = rte_mempool_create_empty("default_pool",
						MEMPOOL_SIZE,
						MEMPOOL_ELT_SIZE,
						0, 0,
						SOCKET_ID_ANY, 0);

	if (default_pool == NULL) {
		printf("cannot allocate %s mempool\n", default_pool_ops);
		goto err;
	}

	if (rte_mempool_set_ops_byname(default_pool, default_pool_ops, NULL)
				       < 0) {
		printf("cannot set %s handler\n", default_pool_ops);
		goto err;
	}

	if (rte_mempool_populate_default(default_pool) < 0) {
		printf("cannot populate %s mempool\n", default_pool_ops);
		goto err;
	}

	rte_mempool_obj_iter(default_pool, my_obj_init, NULL);

	/* performance test with 1, 2 and max cores */
	printf("start performance test (without cache)\n");

	if (do_one_mempool_test(mp_nocache, 1) < 0)
		goto err;

	if (do_one_mempool_test(mp_nocache, 2) < 0)
		goto err;

	if (do_one_mempool_test(mp_nocache, rte_lcore_count()) < 0)
		goto err;

	/* performance test with 1, 2 and max cores */
	printf("start performance test for %s (without cache)\n",
	       default_pool_ops);

	if (do_one_mempool_test(default_pool, 1) < 0)
		goto err;

	if (do_one_mempool_test(default_pool, 2) < 0)
		goto err;

	if (do_one_mempool_test(default_pool, rte_lcore_count()) < 0)
		goto err;

	/* performance test with 1, 2 and max cores */
	printf("start performance test (with cache)\n");

	if (do_one_mempool_test(mp_cache, 1) < 0)
		goto err;

	if (do_one_mempool_test(mp_cache, 2) < 0)
		goto err;

	if (do_one_mempool_test(mp_cache, rte_lcore_count()) < 0)
		goto err;

	/* performance test with 1, 2 and max cores */
	printf("start performance test (with user-owned cache)\n");
	use_external_cache = 1;

	if (do_one_mempool_test(mp_nocache, 1) < 0)
		goto err;

	if (do_one_mempool_test(mp_nocache, 2) < 0)
		goto err;

	if (do_one_mempool_test(mp_nocache, rte_lcore_count()) < 0)
		goto err;

	rte_mempool_list_dump(stdout);

	ret = 0;

err:
	rte_mempool_free(mp_cache);
	rte_mempool_free(mp_nocache);
	rte_mempool_free(default_pool);
	return ret;
}

REGISTER_PERF_TEST(mempool_perf_autotest, test_mempool_perf);
