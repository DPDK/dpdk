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
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>

#include "test.h"

/*
 * Mempool
 * =======
 *
 * Basic tests: done on one core with and without cache:
 *
 *    - Get one object, put one object
 *    - Get two objects, put two objects
 *    - Get all objects, test that their content is not modified and
 *      put them back in the pool.
 */

#define MEMPOOL_ELT_SIZE 2048
#define MAX_KEEP 16
#define MEMPOOL_SIZE ((rte_lcore_count()*(MAX_KEEP+RTE_MEMPOOL_CACHE_MAX_SIZE))-1)

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

static rte_atomic32_t synchro;

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

/* basic tests (done on one core) */
static int
test_mempool_basic(struct rte_mempool *mp, int use_external_cache)
{
	uint32_t *objnum;
	void **objtable;
	void *obj, *obj2;
	char *obj_data;
	int ret = 0;
	unsigned i, j;
	int offset;
	struct rte_mempool_cache *cache;

	if (use_external_cache) {
		/* Create a user-owned mempool cache. */
		cache = rte_mempool_cache_create(RTE_MEMPOOL_CACHE_MAX_SIZE,
						 SOCKET_ID_ANY);
		if (cache == NULL)
			RET_ERR();
	} else {
		/* May be NULL if cache is disabled. */
		cache = rte_mempool_default_cache(mp, rte_lcore_id());
	}

	/* dump the mempool status */
	rte_mempool_dump(stdout, mp);

	printf("get an object\n");
	if (rte_mempool_generic_get(mp, &obj, 1, cache, 0) < 0)
		GOTO_ERR(ret, out);
	rte_mempool_dump(stdout, mp);

	/* tests that improve coverage */
	printf("get object count\n");
	/* We have to count the extra caches, one in this case. */
	offset = use_external_cache ? 1 * cache->len : 0;
	if (rte_mempool_avail_count(mp) + offset != MEMPOOL_SIZE - 1)
		GOTO_ERR(ret, out);

	printf("get private data\n");
	if (rte_mempool_get_priv(mp) != (char *)mp +
			MEMPOOL_HEADER_SIZE(mp, mp->cache_size))
		GOTO_ERR(ret, out);

#ifndef RTE_EXEC_ENV_BSDAPP /* rte_mem_virt2phy() not supported on bsd */
	printf("get physical address of an object\n");
	if (rte_mempool_virt2phy(mp, obj) != rte_mem_virt2phy(obj))
		GOTO_ERR(ret, out);
#endif

	printf("put the object back\n");
	rte_mempool_generic_put(mp, &obj, 1, cache, 0);
	rte_mempool_dump(stdout, mp);

	printf("get 2 objects\n");
	if (rte_mempool_generic_get(mp, &obj, 1, cache, 0) < 0)
		GOTO_ERR(ret, out);
	if (rte_mempool_generic_get(mp, &obj2, 1, cache, 0) < 0) {
		rte_mempool_generic_put(mp, &obj, 1, cache, 0);
		GOTO_ERR(ret, out);
	}
	rte_mempool_dump(stdout, mp);

	printf("put the objects back\n");
	rte_mempool_generic_put(mp, &obj, 1, cache, 0);
	rte_mempool_generic_put(mp, &obj2, 1, cache, 0);
	rte_mempool_dump(stdout, mp);

	/*
	 * get many objects: we cannot get them all because the cache
	 * on other cores may not be empty.
	 */
	objtable = malloc(MEMPOOL_SIZE * sizeof(void *));
	if (objtable == NULL)
		GOTO_ERR(ret, out);

	for (i = 0; i < MEMPOOL_SIZE; i++) {
		if (rte_mempool_generic_get(mp, &objtable[i], 1, cache, 0) < 0)
			break;
	}

	/*
	 * for each object, check that its content was not modified,
	 * and put objects back in pool
	 */
	while (i--) {
		obj = objtable[i];
		obj_data = obj;
		objnum = obj;
		if (*objnum > MEMPOOL_SIZE) {
			printf("bad object number(%d)\n", *objnum);
			ret = -1;
			break;
		}
		for (j = sizeof(*objnum); j < mp->elt_size; j++) {
			if (obj_data[j] != 0)
				ret = -1;
		}

		rte_mempool_generic_put(mp, &objtable[i], 1, cache, 0);
	}

	free(objtable);
	if (ret == -1)
		printf("objects were modified!\n");

out:
	if (use_external_cache) {
		rte_mempool_cache_flush(cache, mp);
		rte_mempool_cache_free(cache);
	}

	return ret;
}

static int test_mempool_creation_with_exceeded_cache_size(void)
{
	struct rte_mempool *mp_cov;

	mp_cov = rte_mempool_create("test_mempool_cache_too_big",
		MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE,
		RTE_MEMPOOL_CACHE_MAX_SIZE + 32, 0,
		NULL, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY, 0);

	if (mp_cov != NULL) {
		rte_mempool_free(mp_cov);
		RET_ERR();
	}

	return 0;
}

static struct rte_mempool *mp_spsc;
static rte_spinlock_t scsp_spinlock;
static void *scsp_obj_table[MAX_KEEP];

/*
 * single producer function
 */
static int test_mempool_single_producer(void)
{
	unsigned int i;
	void *obj = NULL;
	uint64_t start_cycles, end_cycles;
	uint64_t duration = rte_get_timer_hz() / 4;

	start_cycles = rte_get_timer_cycles();
	while (1) {
		end_cycles = rte_get_timer_cycles();
		/* duration uses up, stop producing */
		if (start_cycles + duration < end_cycles)
			break;
		rte_spinlock_lock(&scsp_spinlock);
		for (i = 0; i < MAX_KEEP; i ++) {
			if (NULL != scsp_obj_table[i]) {
				obj = scsp_obj_table[i];
				break;
			}
		}
		rte_spinlock_unlock(&scsp_spinlock);
		if (i >= MAX_KEEP) {
			continue;
		}
		if (rte_mempool_from_obj(obj) != mp_spsc) {
			printf("obj not owned by this mempool\n");
			RET_ERR();
		}
		rte_mempool_put(mp_spsc, obj);
		rte_spinlock_lock(&scsp_spinlock);
		scsp_obj_table[i] = NULL;
		rte_spinlock_unlock(&scsp_spinlock);
	}

	return 0;
}

/*
 * single consumer function
 */
static int test_mempool_single_consumer(void)
{
	unsigned int i;
	void * obj;
	uint64_t start_cycles, end_cycles;
	uint64_t duration = rte_get_timer_hz() / 8;

	start_cycles = rte_get_timer_cycles();
	while (1) {
		end_cycles = rte_get_timer_cycles();
		/* duration uses up, stop consuming */
		if (start_cycles + duration < end_cycles)
			break;
		rte_spinlock_lock(&scsp_spinlock);
		for (i = 0; i < MAX_KEEP; i ++) {
			if (NULL == scsp_obj_table[i])
				break;
		}
		rte_spinlock_unlock(&scsp_spinlock);
		if (i >= MAX_KEEP)
			continue;
		if (rte_mempool_get(mp_spsc, &obj) < 0)
			break;
		rte_spinlock_lock(&scsp_spinlock);
		scsp_obj_table[i] = obj;
		rte_spinlock_unlock(&scsp_spinlock);
	}

	return 0;
}

/*
 * test function for mempool test based on singple consumer and single producer,
 * can run on one lcore only
 */
static int
test_mempool_launch_single_consumer(__attribute__((unused)) void *arg)
{
	return test_mempool_single_consumer();
}

static void
my_mp_init(struct rte_mempool *mp, __attribute__((unused)) void *arg)
{
	printf("mempool name is %s\n", mp->name);
	/* nothing to be implemented here*/
	return ;
}

/*
 * it tests the mempool operations based on singple producer and single consumer
 */
static int
test_mempool_sp_sc(void)
{
	int ret = 0;
	unsigned lcore_id = rte_lcore_id();
	unsigned lcore_next;

	/* create a mempool with single producer/consumer ring */
	if (mp_spsc == NULL) {
		mp_spsc = rte_mempool_create("test_mempool_sp_sc", MEMPOOL_SIZE,
			MEMPOOL_ELT_SIZE, 0, 0,
			my_mp_init, NULL,
			my_obj_init, NULL,
			SOCKET_ID_ANY,
			MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT |
			MEMPOOL_F_SC_GET);
		if (mp_spsc == NULL)
			RET_ERR();
	}
	if (rte_mempool_lookup("test_mempool_sp_sc") != mp_spsc) {
		printf("Cannot lookup mempool from its name\n");
		rte_mempool_free(mp_spsc);
		RET_ERR();
	}
	lcore_next = rte_get_next_lcore(lcore_id, 0, 1);
	if (lcore_next >= RTE_MAX_LCORE) {
		rte_mempool_free(mp_spsc);
		RET_ERR();
	}
	if (rte_eal_lcore_role(lcore_next) != ROLE_RTE) {
		rte_mempool_free(mp_spsc);
		RET_ERR();
	}
	rte_spinlock_init(&scsp_spinlock);
	memset(scsp_obj_table, 0, sizeof(scsp_obj_table));
	rte_eal_remote_launch(test_mempool_launch_single_consumer, NULL,
		lcore_next);
	if (test_mempool_single_producer() < 0)
		ret = -1;

	if (rte_eal_wait_lcore(lcore_next) < 0)
		ret = -1;
	rte_mempool_free(mp_spsc);

	return ret;
}

/*
 * it tests some more basic of mempool
 */
static int
test_mempool_basic_ex(struct rte_mempool *mp)
{
	unsigned i;
	void **obj;
	void *err_obj;
	int ret = -1;

	if (mp == NULL)
		return ret;

	obj = rte_calloc("test_mempool_basic_ex", MEMPOOL_SIZE,
		sizeof(void *), 0);
	if (obj == NULL) {
		printf("test_mempool_basic_ex fail to rte_malloc\n");
		return ret;
	}
	printf("test_mempool_basic_ex now mempool (%s) has %u free entries\n",
		mp->name, rte_mempool_in_use_count(mp));
	if (rte_mempool_full(mp) != 1) {
		printf("test_mempool_basic_ex the mempool should be full\n");
		goto fail_mp_basic_ex;
	}

	for (i = 0; i < MEMPOOL_SIZE; i ++) {
		if (rte_mempool_get(mp, &obj[i]) < 0) {
			printf("test_mp_basic_ex fail to get object for [%u]\n",
				i);
			goto fail_mp_basic_ex;
		}
	}
	if (rte_mempool_get(mp, &err_obj) == 0) {
		printf("test_mempool_basic_ex get an impossible obj\n");
		goto fail_mp_basic_ex;
	}
	printf("number: %u\n", i);
	if (rte_mempool_empty(mp) != 1) {
		printf("test_mempool_basic_ex the mempool should be empty\n");
		goto fail_mp_basic_ex;
	}

	for (i = 0; i < MEMPOOL_SIZE; i++)
		rte_mempool_put(mp, obj[i]);

	if (rte_mempool_full(mp) != 1) {
		printf("test_mempool_basic_ex the mempool should be full\n");
		goto fail_mp_basic_ex;
	}

	ret = 0;

fail_mp_basic_ex:
	if (obj != NULL)
		rte_free((void *)obj);

	return ret;
}

static int
test_mempool_same_name_twice_creation(void)
{
	struct rte_mempool *mp_tc, *mp_tc2;

	mp_tc = rte_mempool_create("test_mempool_same_name", MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE, 0, 0,
		NULL, NULL,
		NULL, NULL,
		SOCKET_ID_ANY, 0);

	if (mp_tc == NULL)
		RET_ERR();

	mp_tc2 = rte_mempool_create("test_mempool_same_name", MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE, 0, 0,
		NULL, NULL,
		NULL, NULL,
		SOCKET_ID_ANY, 0);

	if (mp_tc2 != NULL) {
		rte_mempool_free(mp_tc);
		rte_mempool_free(mp_tc2);
		RET_ERR();
	}

	rte_mempool_free(mp_tc);
	return 0;
}

/*
 * BAsic test for mempool_xmem functions.
 */
static int
test_mempool_xmem_misc(void)
{
	uint32_t elt_num, total_size;
	size_t sz;
	ssize_t usz;

	elt_num = MAX_KEEP;
	total_size = rte_mempool_calc_obj_size(MEMPOOL_ELT_SIZE, 0, NULL);
	sz = rte_mempool_xmem_size(elt_num, total_size, MEMPOOL_PG_SHIFT_MAX);

	usz = rte_mempool_xmem_usage(NULL, elt_num, total_size, 0, 1,
		MEMPOOL_PG_SHIFT_MAX);

	if (sz != (size_t)usz)  {
		printf("failure @ %s: rte_mempool_xmem_usage(%u, %u) "
			"returns: %#zx, while expected: %#zx;\n",
			__func__, elt_num, total_size, sz, (size_t)usz);
		return -1;
	}

	return 0;
}

static void
walk_cb(struct rte_mempool *mp, void *userdata __rte_unused)
{
	printf("\t%s\n", mp->name);
}

static int
test_mempool(void)
{
	int ret = -1;
	struct rte_mempool *mp_cache = NULL;
	struct rte_mempool *mp_nocache = NULL;
	struct rte_mempool *mp_stack = NULL;
	struct rte_mempool *default_pool = NULL;

	rte_atomic32_init(&synchro);

	/* create a mempool (without cache) */
	mp_nocache = rte_mempool_create("test_nocache", MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE, 0, 0,
		NULL, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY, 0);

	if (mp_nocache == NULL) {
		printf("cannot allocate mp_nocache mempool\n");
		goto err;
	}

	/* create a mempool (with cache) */
	mp_cache = rte_mempool_create("test_cache", MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE,
		RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
		NULL, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY, 0);

	if (mp_cache == NULL) {
		printf("cannot allocate mp_cache mempool\n");
		goto err;
	}

	/* create a mempool with an external handler */
	mp_stack = rte_mempool_create_empty("test_stack",
		MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE,
		RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
		SOCKET_ID_ANY, 0);

	if (mp_stack == NULL) {
		printf("cannot allocate mp_stack mempool\n");
		goto err;
	}
	if (rte_mempool_set_ops_byname(mp_stack, "stack", NULL) < 0) {
		printf("cannot set stack handler\n");
		goto err;
	}
	if (rte_mempool_populate_default(mp_stack) < 0) {
		printf("cannot populate mp_stack mempool\n");
		goto err;
	}
	rte_mempool_obj_iter(mp_stack, my_obj_init, NULL);

	/* Create a mempool based on Default handler */
	printf("Testing %s mempool handler\n",
	       RTE_MBUF_DEFAULT_MEMPOOL_OPS);
	default_pool = rte_mempool_create_empty("default_pool",
						MEMPOOL_SIZE,
						MEMPOOL_ELT_SIZE,
						RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
						SOCKET_ID_ANY, 0);

	if (default_pool == NULL) {
		printf("cannot allocate default mempool\n");
		goto err;
	}
	if (rte_mempool_set_ops_byname(default_pool,
				RTE_MBUF_DEFAULT_MEMPOOL_OPS, NULL) < 0) {
		printf("cannot set %s handler\n",
			RTE_MBUF_DEFAULT_MEMPOOL_OPS);
		goto err;
	}
	if (rte_mempool_populate_default(default_pool) < 0) {
		printf("cannot populate %s mempool\n",
			RTE_MBUF_DEFAULT_MEMPOOL_OPS);
		goto err;
	}
	rte_mempool_obj_iter(default_pool, my_obj_init, NULL);

	/* retrieve the mempool from its name */
	if (rte_mempool_lookup("test_nocache") != mp_nocache) {
		printf("Cannot lookup mempool from its name\n");
		goto err;
	}

	printf("Walk into mempools:\n");
	rte_mempool_walk(walk_cb, NULL);

	rte_mempool_list_dump(stdout);

	/* basic tests without cache */
	if (test_mempool_basic(mp_nocache, 0) < 0)
		goto err;

	/* basic tests with cache */
	if (test_mempool_basic(mp_cache, 0) < 0)
		goto err;

	/* basic tests with user-owned cache */
	if (test_mempool_basic(mp_nocache, 1) < 0)
		goto err;

	/* more basic tests without cache */
	if (test_mempool_basic_ex(mp_nocache) < 0)
		goto err;

	/* mempool operation test based on single producer and single comsumer */
	if (test_mempool_sp_sc() < 0)
		goto err;

	if (test_mempool_creation_with_exceeded_cache_size() < 0)
		goto err;

	if (test_mempool_same_name_twice_creation() < 0)
		goto err;

	if (test_mempool_xmem_misc() < 0)
		goto err;

	/* test the stack handler */
	if (test_mempool_basic(mp_stack, 1) < 0)
		goto err;

	if (test_mempool_basic(default_pool, 1) < 0)
		goto err;

	rte_mempool_list_dump(stdout);

	ret = 0;

err:
	rte_mempool_free(mp_nocache);
	rte_mempool_free(mp_cache);
	rte_mempool_free(mp_stack);
	rte_mempool_free(default_pool);

	return ret;
}

REGISTER_TEST_COMMAND(mempool_autotest, test_mempool);
