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
 *    Each core get *n_keep* objects per bulk of a pseudorandom number
 *    between 1 and *n_max_bulk*.
 *    Objects are put back in the pool per bulk of a similar pseudorandom number.
 *    Note: The very low entropy of the randomization algorithm is harmless, because
 *          the sole purpose of randomization is to prevent the CPU's dynamic branch
 *          predictor from enhancing the test results.
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
 *    - Pseudorandom max bulk size (*n_max_bulk*)
 *
 *      - Max bulk from CACHE_LINE_BURST to 256, and RTE_MEMPOOL_CACHE_MAX_SIZE,
 *        where CACHE_LINE_BURST is the number of pointers fitting into one CPU cache line.
 *
 *    - Fixed bulk size (*n_get_bulk*, *n_put_bulk*)
 *
 *      - Bulk get from 1 to 256, and RTE_MEMPOOL_CACHE_MAX_SIZE
 *      - Bulk put from 1 to 256, and RTE_MEMPOOL_CACHE_MAX_SIZE
 *      - Bulk get and put from 1 to 256, and RTE_MEMPOOL_CACHE_MAX_SIZE, compile time constant
 *
 *    - Number of kept objects (*n_keep*)
 *
 *      - 32
 *      - 128
 *      - 512
 *      - 2048
 *      - 8192
 *      - 32768
 */

#define TIME_S 1
#define MEMPOOL_ELT_SIZE 2048
#define MAX_KEEP 32768
#define N (128 * MAX_KEEP)
#define MEMPOOL_SIZE ((rte_lcore_count()*(MAX_KEEP+RTE_MEMPOOL_CACHE_MAX_SIZE*2))-1)

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
static unsigned int external_cache_size = RTE_MEMPOOL_CACHE_MAX_SIZE;

static RTE_ATOMIC(uint32_t) synchro;

/* max random number of objects in one bulk operation (get and put) */
static unsigned int n_max_bulk;

/* number of objects in one bulk operation (get or put) */
static unsigned int n_get_bulk;
static unsigned int n_put_bulk;

/* number of objects retrieved from mempool before putting them back */
static unsigned int n_keep;

/* true if we want to test with constant n_get_bulk and n_put_bulk */
static int use_constant_values;

/* number of enqueues / dequeues, and time used */
struct __rte_cache_aligned mempool_test_stats {
	uint64_t enq_count;
	uint64_t duration_cycles;
	RTE_CACHE_GUARD;
};

static struct mempool_test_stats stats[RTE_MAX_LCORE];

/*
 * save the object number in the first 4 bytes of object data. All
 * other bytes are set to 0.
 */
static void
my_obj_init(struct rte_mempool *mp, __rte_unused void *arg,
	    void *obj, unsigned int i)
{
	uint32_t *objnum = obj;
	memset(obj, 0, mp->elt_size);
	*objnum = i;
}

static __rte_always_inline int
test_loop(struct rte_mempool *mp, struct rte_mempool_cache *cache,
	  unsigned int x_keep, unsigned int x_get_bulk, unsigned int x_put_bulk)
{
	alignas(RTE_CACHE_LINE_SIZE) void *obj_table[MAX_KEEP];
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

static __rte_always_inline int
test_loop_random(struct rte_mempool *mp, struct rte_mempool_cache *cache,
	  unsigned int x_keep, unsigned int x_max_bulk)
{
	alignas(RTE_CACHE_LINE_SIZE) void *obj_table[MAX_KEEP];
	unsigned int idx;
	unsigned int i;
	unsigned int r = 0;
	unsigned int x_bulk;
	int ret;

	for (i = 0; likely(i < (N / x_keep)); i++) {
		/* get x_keep objects by bulk of random [1 .. x_max_bulk] */
		for (idx = 0; idx < x_keep; idx += x_bulk, r++) {
			/* Generate a pseudorandom number [1 .. x_max_bulk]. */
			x_bulk = ((r ^ (r >> 2) ^ (r << 3)) & (x_max_bulk - 1)) + 1;
			if (unlikely(idx + x_bulk > x_keep))
				x_bulk = x_keep - idx;
			ret = rte_mempool_generic_get(mp,
						      &obj_table[idx],
						      x_bulk,
						      cache);
			if (unlikely(ret < 0)) {
				rte_mempool_dump(stdout, mp);
				return ret;
			}
		}

		/* put the objects back by bulk of random [1 .. x_max_bulk] */
		for (idx = 0; idx < x_keep; idx += x_bulk, r++) {
			/* Generate a pseudorandom number [1 .. x_max_bulk]. */
			x_bulk = ((r ^ (r >> 2) ^ (r << 3)) & (x_max_bulk - 1)) + 1;
			if (unlikely(idx + x_bulk > x_keep))
				x_bulk = x_keep - idx;
			rte_mempool_generic_put(mp,
						&obj_table[idx],
						x_bulk,
						cache);
		}
	}

	return 0;
}

static int
per_lcore_mempool_test(void *arg)
{
	struct rte_mempool *mp = arg;
	unsigned int lcore_id = rte_lcore_id();
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
	if (n_max_bulk == 0 && (((n_keep / n_get_bulk) * n_get_bulk) != n_keep))
		GOTO_ERR(ret, out);
	if (n_max_bulk == 0 && (((n_keep / n_put_bulk) * n_put_bulk) != n_keep))
		GOTO_ERR(ret, out);
	/* for constant n, n_get_bulk and n_put_bulk must be the same */
	if (use_constant_values && n_put_bulk != n_get_bulk)
		GOTO_ERR(ret, out);

	stats[lcore_id].enq_count = 0;
	stats[lcore_id].duration_cycles = 0;

	/* wait synchro for workers */
	if (lcore_id != rte_get_main_lcore())
		rte_wait_until_equal_32((uint32_t *)(uintptr_t)&synchro, 1,
				rte_memory_order_relaxed);

	start_cycles = rte_get_timer_cycles();

	while (time_diff/hz < TIME_S) {
		if (n_max_bulk != 0)
			ret = test_loop_random(mp, cache, n_keep, n_max_bulk);
		else if (!use_constant_values)
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
		else if (n_get_bulk == 64)
			ret = test_loop(mp, cache, n_keep, 64, 64);
		else if (n_get_bulk == 128)
			ret = test_loop(mp, cache, n_keep, 128, 128);
		else if (n_get_bulk == 256)
			ret = test_loop(mp, cache, n_keep, 256, 256);
		else if (n_get_bulk == RTE_MEMPOOL_CACHE_MAX_SIZE)
			ret = test_loop(mp, cache, n_keep,
					RTE_MEMPOOL_CACHE_MAX_SIZE, RTE_MEMPOOL_CACHE_MAX_SIZE);
		else
			ret = -1;

		if (ret < 0)
			GOTO_ERR(ret, out);

		end_cycles = rte_get_timer_cycles();
		time_diff = end_cycles - start_cycles;
		stats[lcore_id].enq_count += N;
	}

	stats[lcore_id].duration_cycles = time_diff;

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
	unsigned int lcore_id;
	uint64_t rate;
	int ret;
	unsigned int cores_save = cores;
	double hz = rte_get_timer_hz();

	rte_atomic_store_explicit(&synchro, 0, rte_memory_order_relaxed);

	/* reset stats */
	memset(stats, 0, sizeof(stats));

	printf("mempool_autotest cache=%u cores=%u n_keep=%5u ",
	       use_external_cache ? external_cache_size : (unsigned int) mp->cache_size,
	       cores,
	       n_keep);
	if (n_max_bulk != 0)
		printf("n_max_bulk=%3u ",
		       n_max_bulk);
	else
		printf("n_get_bulk=%3u n_put_bulk=%3u constant_n=%u ",
		       n_get_bulk, n_put_bulk,
		       use_constant_values);

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
	rte_atomic_store_explicit(&synchro, 1, rte_memory_order_relaxed);

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
		if (stats[lcore_id].duration_cycles != 0)
			rate += (double)stats[lcore_id].enq_count * hz /
					(double)stats[lcore_id].duration_cycles;

	printf("rate_persec=%10" PRIu64 "\n", rate);

	return 0;
}

/* for a given number of core, launch all test cases */
static int
do_one_mempool_test(struct rte_mempool *mp, unsigned int cores, int external_cache)
{
	unsigned int bulk_tab_max[] = { CACHE_LINE_BURST, 32, 64, 128, 256,
			RTE_MEMPOOL_CACHE_MAX_SIZE, 0 };
	unsigned int bulk_tab_get[] = { 1, 4, CACHE_LINE_BURST, 32, 64, 128, 256,
			RTE_MEMPOOL_CACHE_MAX_SIZE, 0 };
	unsigned int bulk_tab_put[] = { 1, 4, CACHE_LINE_BURST, 32, 64, 128, 256,
			RTE_MEMPOOL_CACHE_MAX_SIZE, 0 };
	unsigned int keep_tab[] = { 32, 128, 512, 2048, 8192, 32768, 0 };
	unsigned int *max_bulk_ptr;
	unsigned int *get_bulk_ptr;
	unsigned int *put_bulk_ptr;
	unsigned int *keep_ptr;
	int ret;

	for (keep_ptr = keep_tab; *keep_ptr; keep_ptr++) {
		for (max_bulk_ptr = bulk_tab_max; *max_bulk_ptr; max_bulk_ptr++) {

			if (*keep_ptr < *max_bulk_ptr)
				continue;

			use_external_cache = external_cache;
			use_constant_values = 0;
			n_max_bulk = *max_bulk_ptr;
			n_get_bulk = 0;
			n_put_bulk = 0;
			n_keep = *keep_ptr;
			ret = launch_cores(mp, cores);
			if (ret < 0)
				return -1;
		}
	}

	for (keep_ptr = keep_tab; *keep_ptr; keep_ptr++) {
		for (get_bulk_ptr = bulk_tab_get; *get_bulk_ptr; get_bulk_ptr++) {
			for (put_bulk_ptr = bulk_tab_put; *put_bulk_ptr; put_bulk_ptr++) {

				if (*keep_ptr < *get_bulk_ptr || *keep_ptr < *put_bulk_ptr)
					continue;

				use_external_cache = external_cache;
				use_constant_values = 0;
				n_max_bulk = 0;
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
do_all_mempool_perf_tests(unsigned int cores)
{
	struct rte_mempool *mp_cache = NULL;
	struct rte_mempool *mp_nocache = NULL;
	struct rte_mempool *default_pool_cache = NULL;
	struct rte_mempool *default_pool_nocache = NULL;
	const char *mp_cache_ops;
	const char *mp_nocache_ops;
	const char *default_pool_ops;
	int ret = -1;

	/* create a mempool (without cache) */
	mp_nocache = rte_mempool_create("perf_test_nocache", MEMPOOL_SIZE,
					MEMPOOL_ELT_SIZE, 0, 0,
					NULL, NULL,
					my_obj_init, NULL,
					SOCKET_ID_ANY, 0);
	if (mp_nocache == NULL) {
		printf("cannot allocate mempool (without cache)\n");
		goto err;
	}
	mp_nocache_ops = rte_mempool_get_ops(mp_nocache->ops_index)->name;

	/* create a mempool (with cache) */
	mp_cache = rte_mempool_create("perf_test_cache", MEMPOOL_SIZE,
				      MEMPOOL_ELT_SIZE,
				      RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
				      NULL, NULL,
				      my_obj_init, NULL,
				      SOCKET_ID_ANY, 0);
	if (mp_cache == NULL) {
		printf("cannot allocate mempool (with cache)\n");
		goto err;
	}
	mp_cache_ops = rte_mempool_get_ops(mp_cache->ops_index)->name;

	default_pool_ops = rte_mbuf_best_mempool_ops();

	/* Create a mempool (without cache) based on Default handler */
	default_pool_nocache = rte_mempool_create_empty("default_pool_nocache",
			MEMPOOL_SIZE,
			MEMPOOL_ELT_SIZE,
			0, 0,
			SOCKET_ID_ANY, 0);
	if (default_pool_nocache == NULL) {
		printf("cannot allocate %s mempool (without cache)\n", default_pool_ops);
		goto err;
	}
	if (rte_mempool_set_ops_byname(default_pool_nocache, default_pool_ops, NULL) < 0) {
		printf("cannot set %s handler\n", default_pool_ops);
		goto err;
	}
	if (rte_mempool_populate_default(default_pool_nocache) < 0) {
		printf("cannot populate %s mempool\n", default_pool_ops);
		goto err;
	}
	rte_mempool_obj_iter(default_pool_nocache, my_obj_init, NULL);

	/* Create a mempool (with cache) based on Default handler */
	default_pool_cache = rte_mempool_create_empty("default_pool_cache",
			MEMPOOL_SIZE,
			MEMPOOL_ELT_SIZE,
			RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
			SOCKET_ID_ANY, 0);
	if (default_pool_cache == NULL) {
		printf("cannot allocate %s mempool (with cache)\n", default_pool_ops);
		goto err;
	}
	if (rte_mempool_set_ops_byname(default_pool_cache, default_pool_ops, NULL) < 0) {
		printf("cannot set %s handler\n", default_pool_ops);
		goto err;
	}
	if (rte_mempool_populate_default(default_pool_cache) < 0) {
		printf("cannot populate %s mempool\n", default_pool_ops);
		goto err;
	}
	rte_mempool_obj_iter(default_pool_cache, my_obj_init, NULL);

	printf("start performance test (using %s, without cache)\n",
	       mp_nocache_ops);
	if (do_one_mempool_test(mp_nocache, cores, 0) < 0)
		goto err;

	if (strcmp(default_pool_ops, mp_nocache_ops) != 0) {
		printf("start performance test for %s (without cache)\n",
		       default_pool_ops);
		if (do_one_mempool_test(default_pool_nocache, cores, 0) < 0)
			goto err;
	}

	printf("start performance test (using %s, with cache)\n",
	       mp_cache_ops);
	if (do_one_mempool_test(mp_cache, cores, 0) < 0)
		goto err;

	if (strcmp(default_pool_ops, mp_cache_ops) != 0) {
		printf("start performance test for %s (with cache)\n",
		       default_pool_ops);
		if (do_one_mempool_test(default_pool_cache, cores, 0) < 0)
			goto err;
	}

	printf("start performance test (using %s, with user-owned cache)\n",
	       mp_nocache_ops);
	if (do_one_mempool_test(mp_nocache, cores, 1) < 0)
		goto err;

	rte_mempool_list_dump(stdout);

	ret = 0;

err:
	rte_mempool_free(mp_cache);
	rte_mempool_free(mp_nocache);
	rte_mempool_free(default_pool_cache);
	rte_mempool_free(default_pool_nocache);
	return ret;
}

static int
test_mempool_perf_1core(void)
{
	return do_all_mempool_perf_tests(1);
}

static int
test_mempool_perf_2cores(void)
{
	if (rte_lcore_count() < 2) {
		printf("not enough lcores\n");
		return -1;
	}
	return do_all_mempool_perf_tests(2);
}

static int
test_mempool_perf_allcores(void)
{
	return do_all_mempool_perf_tests(rte_lcore_count());
}

static int
test_mempool_perf(void)
{
	int ret = -1;

	/* performance test with 1, 2 and max cores */
	if (do_all_mempool_perf_tests(1) < 0)
		goto err;
	if (rte_lcore_count() == 1)
		goto done;

	if (do_all_mempool_perf_tests(2) < 0)
		goto err;
	if (rte_lcore_count() == 2)
		goto done;

	if (do_all_mempool_perf_tests(rte_lcore_count()) < 0)
		goto err;

done:
	ret = 0;

err:
	return ret;
}

REGISTER_PERF_TEST(mempool_perf_autotest, test_mempool_perf);
REGISTER_PERF_TEST(mempool_perf_autotest_1core, test_mempool_perf_1core);
REGISTER_PERF_TEST(mempool_perf_autotest_2cores, test_mempool_perf_2cores);
REGISTER_PERF_TEST(mempool_perf_autotest_allcores, test_mempool_perf_allcores);
