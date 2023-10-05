/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
#include <rte_eal_paging.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_errno.h>
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
#include <rte_mbuf.h>

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
	if (rte_mempool_generic_get(mp, &obj, 1, cache) < 0)
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
			RTE_MEMPOOL_HEADER_SIZE(mp, mp->cache_size))
		GOTO_ERR(ret, out);

#ifndef RTE_EXEC_ENV_FREEBSD /* rte_mem_virt2iova() not supported on bsd */
	printf("get physical address of an object\n");
	if (rte_mempool_virt2iova(obj) != rte_mem_virt2iova(obj))
		GOTO_ERR(ret, out);
#endif

	printf("put the object back\n");
	rte_mempool_generic_put(mp, &obj, 1, cache);
	rte_mempool_dump(stdout, mp);

	printf("get 2 objects\n");
	if (rte_mempool_generic_get(mp, &obj, 1, cache) < 0)
		GOTO_ERR(ret, out);
	if (rte_mempool_generic_get(mp, &obj2, 1, cache) < 0) {
		rte_mempool_generic_put(mp, &obj, 1, cache);
		GOTO_ERR(ret, out);
	}
	rte_mempool_dump(stdout, mp);

	printf("put the objects back\n");
	rte_mempool_generic_put(mp, &obj, 1, cache);
	rte_mempool_generic_put(mp, &obj2, 1, cache);
	rte_mempool_dump(stdout, mp);

	/*
	 * get many objects: we cannot get them all because the cache
	 * on other cores may not be empty.
	 */
	objtable = malloc(MEMPOOL_SIZE * sizeof(void *));
	if (objtable == NULL)
		GOTO_ERR(ret, out);

	for (i = 0; i < MEMPOOL_SIZE; i++) {
		if (rte_mempool_generic_get(mp, &objtable[i], 1, cache) < 0)
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

		rte_mempool_generic_put(mp, &objtable[i], 1, cache);
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

static int test_mempool_creation_with_invalid_flags(void)
{
	struct rte_mempool *mp_cov;

	mp_cov = rte_mempool_create("test_mempool_invalid_flags", MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE, 0, 0,
		NULL, NULL,
		NULL, NULL,
		SOCKET_ID_ANY, ~RTE_MEMPOOL_VALID_USER_FLAGS);

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
 * test function for mempool test based on single consumer and single producer,
 * can run on one lcore only
 */
static int
test_mempool_launch_single_consumer(__rte_unused void *arg)
{
	return test_mempool_single_consumer();
}

static void
my_mp_init(struct rte_mempool *mp, __rte_unused void *arg)
{
	printf("mempool name is %s\n", mp->name);
	/* nothing to be implemented here*/
	return ;
}

/*
 * it tests the mempool operations based on single producer and single consumer
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
			RTE_MEMPOOL_F_NO_CACHE_ALIGN | RTE_MEMPOOL_F_SP_PUT |
			RTE_MEMPOOL_F_SC_GET);
		if (mp_spsc == NULL)
			RET_ERR();
	}
	if (rte_mempool_lookup("test_mempool_sp_sc") != mp_spsc) {
		printf("Cannot lookup mempool from its name\n");
		ret = -1;
		goto err;
	}
	lcore_next = rte_get_next_lcore(lcore_id, 0, 1);
	if (lcore_next >= RTE_MAX_LCORE) {
		ret = -1;
		goto err;
	}
	if (rte_eal_lcore_role(lcore_next) != ROLE_RTE) {
		ret = -1;
		goto err;
	}
	rte_spinlock_init(&scsp_spinlock);
	memset(scsp_obj_table, 0, sizeof(scsp_obj_table));
	rte_eal_remote_launch(test_mempool_launch_single_consumer, NULL,
		lcore_next);
	if (test_mempool_single_producer() < 0)
		ret = -1;

	if (rte_eal_wait_lcore(lcore_next) < 0)
		ret = -1;

err:
	rte_mempool_free(mp_spsc);
	mp_spsc = NULL;

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

static void
walk_cb(struct rte_mempool *mp, void *userdata __rte_unused)
{
	printf("\t%s\n", mp->name);
}

struct mp_data {
	int16_t ret;
};

static void
test_mp_mem_init(struct rte_mempool *mp,
		__rte_unused void *opaque,
		__rte_unused struct rte_mempool_memhdr *memhdr,
		__rte_unused unsigned int mem_idx)
{
	struct mp_data *data = opaque;

	if (mp == NULL) {
		data->ret = -1;
		return;
	}
	/* nothing to be implemented here*/
	data->ret = 0;
}

struct test_mempool_events_data {
	struct rte_mempool *mp;
	enum rte_mempool_event event;
	bool invoked;
};

static void
test_mempool_events_cb(enum rte_mempool_event event,
		       struct rte_mempool *mp, void *user_data)
{
	struct test_mempool_events_data *data = user_data;

	data->mp = mp;
	data->event = event;
	data->invoked = true;
}

static int
test_mempool_events(int (*populate)(struct rte_mempool *mp))
{
#pragma push_macro("RTE_TEST_TRACE_FAILURE")
#undef RTE_TEST_TRACE_FAILURE
#define RTE_TEST_TRACE_FAILURE(...) do { goto fail; } while (0)

	static const size_t callback_num = 3;
	static const size_t mempool_num = 2;
	static const unsigned int mempool_elt_size = 64;
	static const unsigned int mempool_size = 64;

	struct test_mempool_events_data data[callback_num];
	struct rte_mempool *mp[mempool_num], *freed;
	char name[RTE_MEMPOOL_NAMESIZE];
	size_t i, j;
	int ret;

	memset(mp, 0, sizeof(mp));
	for (i = 0; i < callback_num; i++) {
		ret = rte_mempool_event_callback_register
				(test_mempool_events_cb, &data[i]);
		RTE_TEST_ASSERT_EQUAL(ret, 0, "Failed to register the callback %zu: %s",
				      i, rte_strerror(rte_errno));
	}
	ret = rte_mempool_event_callback_unregister(test_mempool_events_cb, mp);
	RTE_TEST_ASSERT_NOT_EQUAL(ret, 0, "Unregistered a non-registered callback");
	/* NULL argument has no special meaning in this API. */
	ret = rte_mempool_event_callback_unregister(test_mempool_events_cb,
						    NULL);
	RTE_TEST_ASSERT_NOT_EQUAL(ret, 0, "Unregistered a non-registered callback with NULL argument");

	/* Create mempool 0 that will be observed by all callbacks. */
	memset(&data, 0, sizeof(data));
	strcpy(name, "empty0");
	mp[0] = rte_mempool_create_empty(name, mempool_size,
					 mempool_elt_size, 0, 0,
					 SOCKET_ID_ANY, 0);
	RTE_TEST_ASSERT_NOT_NULL(mp[0], "Cannot create mempool %s: %s",
				 name, rte_strerror(rte_errno));
	for (j = 0; j < callback_num; j++)
		RTE_TEST_ASSERT_EQUAL(data[j].invoked, false,
				      "Callback %zu invoked on %s mempool creation",
				      j, name);

	rte_mempool_set_ops_byname(mp[0], rte_mbuf_best_mempool_ops(), NULL);
	ret = populate(mp[0]);
	RTE_TEST_ASSERT_EQUAL(ret, (int)mp[0]->size, "Failed to populate mempool %s: %s",
			      name, rte_strerror(-ret));
	for (j = 0; j < callback_num; j++) {
		RTE_TEST_ASSERT_EQUAL(data[j].invoked, true,
					"Callback %zu not invoked on mempool %s population",
					j, name);
		RTE_TEST_ASSERT_EQUAL(data[j].event,
					RTE_MEMPOOL_EVENT_READY,
					"Wrong callback invoked, expected READY");
		RTE_TEST_ASSERT_EQUAL(data[j].mp, mp[0],
					"Callback %zu invoked for a wrong mempool instead of %s",
					j, name);
	}

	/* Check that unregistered callback 0 observes no events. */
	ret = rte_mempool_event_callback_unregister(test_mempool_events_cb,
						    &data[0]);
	RTE_TEST_ASSERT_EQUAL(ret, 0, "Failed to unregister callback 0: %s",
			      rte_strerror(rte_errno));
	memset(&data, 0, sizeof(data));
	strcpy(name, "empty1");
	mp[1] = rte_mempool_create_empty(name, mempool_size,
					 mempool_elt_size, 0, 0,
					 SOCKET_ID_ANY, 0);
	RTE_TEST_ASSERT_NOT_NULL(mp[1], "Cannot create mempool %s: %s",
				 name, rte_strerror(rte_errno));
	rte_mempool_set_ops_byname(mp[1], rte_mbuf_best_mempool_ops(), NULL);
	ret = populate(mp[1]);
	RTE_TEST_ASSERT_EQUAL(ret, (int)mp[1]->size, "Failed to populate mempool %s: %s",
			      name, rte_strerror(-ret));
	RTE_TEST_ASSERT_EQUAL(data[0].invoked, false,
			      "Unregistered callback 0 invoked on %s mempool populaton",
			      name);

	for (i = 0; i < mempool_num; i++) {
		memset(&data, 0, sizeof(data));
		sprintf(name, "empty%zu", i);
		rte_mempool_free(mp[i]);
		/*
		 * Save pointer to check that it was passed to the callback,
		 * but put NULL into the array in case cleanup is called early.
		 */
		freed = mp[i];
		mp[i] = NULL;
		for (j = 1; j < callback_num; j++) {
			RTE_TEST_ASSERT_EQUAL(data[j].invoked, true,
					      "Callback %zu not invoked on mempool %s destruction",
					      j, name);
			RTE_TEST_ASSERT_EQUAL(data[j].event,
					      RTE_MEMPOOL_EVENT_DESTROY,
					      "Wrong callback invoked, expected DESTROY");
			RTE_TEST_ASSERT_EQUAL(data[j].mp, freed,
					      "Callback %zu invoked for a wrong mempool instead of %s",
					      j, name);
		}
		RTE_TEST_ASSERT_EQUAL(data[0].invoked, false,
				      "Unregistered callback 0 invoked on %s mempool destruction",
				      name);
	}

	for (j = 1; j < callback_num; j++) {
		ret = rte_mempool_event_callback_unregister
					(test_mempool_events_cb, &data[j]);
		RTE_TEST_ASSERT_EQUAL(ret, 0, "Failed to unregister the callback %zu: %s",
				      j, rte_strerror(rte_errno));
	}
	return TEST_SUCCESS;

fail:
	for (j = 0; j < callback_num; j++)
		rte_mempool_event_callback_unregister
					(test_mempool_events_cb, &data[j]);
	for (i = 0; i < mempool_num; i++)
		rte_mempool_free(mp[i]);
	return TEST_FAILED;

#pragma pop_macro("RTE_TEST_TRACE_FAILURE")
}

struct test_mempool_events_safety_data {
	bool invoked;
	int (*api_func)(rte_mempool_event_callback *func, void *user_data);
	rte_mempool_event_callback *cb_func;
	void *cb_user_data;
	int ret;
};

static void
test_mempool_events_safety_cb(enum rte_mempool_event event,
			      struct rte_mempool *mp, void *user_data)
{
	struct test_mempool_events_safety_data *data = user_data;

	RTE_SET_USED(event);
	RTE_SET_USED(mp);
	data->invoked = true;
	data->ret = data->api_func(data->cb_func, data->cb_user_data);
}

static int
test_mempool_events_safety(void)
{
#pragma push_macro("RTE_TEST_TRACE_FAILURE")
#undef RTE_TEST_TRACE_FAILURE
#define RTE_TEST_TRACE_FAILURE(...) do { \
		ret = TEST_FAILED; \
		goto exit; \
	} while (0)

	struct test_mempool_events_data data;
	struct test_mempool_events_safety_data sdata[2];
	struct rte_mempool *mp;
	size_t i;
	int ret;

	/* removes itself */
	sdata[0].api_func = rte_mempool_event_callback_unregister;
	sdata[0].cb_func = test_mempool_events_safety_cb;
	sdata[0].cb_user_data = &sdata[0];
	sdata[0].ret = -1;
	rte_mempool_event_callback_register(test_mempool_events_safety_cb,
					    &sdata[0]);
	/* inserts a callback after itself */
	sdata[1].api_func = rte_mempool_event_callback_register;
	sdata[1].cb_func = test_mempool_events_cb;
	sdata[1].cb_user_data = &data;
	sdata[1].ret = -1;
	rte_mempool_event_callback_register(test_mempool_events_safety_cb,
					    &sdata[1]);

	mp = rte_mempool_create_empty("empty", MEMPOOL_SIZE,
				      MEMPOOL_ELT_SIZE, 0, 0,
				      SOCKET_ID_ANY, 0);
	RTE_TEST_ASSERT_NOT_NULL(mp, "Cannot create mempool: %s",
				 rte_strerror(rte_errno));
	memset(&data, 0, sizeof(data));
	ret = rte_mempool_populate_default(mp);
	RTE_TEST_ASSERT_EQUAL(ret, (int)mp->size, "Failed to populate mempool: %s",
			      rte_strerror(-ret));

	RTE_TEST_ASSERT_EQUAL(sdata[0].ret, 0, "Callback failed to unregister itself: %s",
			      rte_strerror(rte_errno));
	RTE_TEST_ASSERT_EQUAL(sdata[1].ret, 0, "Failed to insert a new callback: %s",
			      rte_strerror(rte_errno));
	RTE_TEST_ASSERT_EQUAL(data.invoked, false,
			      "Inserted callback is invoked on mempool population");

	memset(&data, 0, sizeof(data));
	sdata[0].invoked = false;
	rte_mempool_free(mp);
	mp = NULL;
	RTE_TEST_ASSERT_EQUAL(sdata[0].invoked, false,
			      "Callback that unregistered itself was called");
	RTE_TEST_ASSERT_EQUAL(sdata[1].ret, -EEXIST,
			      "New callback inserted twice");
	RTE_TEST_ASSERT_EQUAL(data.invoked, true,
			      "Inserted callback is not invoked on mempool destruction");

	rte_mempool_event_callback_unregister(test_mempool_events_cb, &data);
	for (i = 0; i < RTE_DIM(sdata); i++)
		rte_mempool_event_callback_unregister
				(test_mempool_events_safety_cb, &sdata[i]);
	ret = TEST_SUCCESS;

exit:
	/* cleanup, don't care which callbacks are already removed */
	rte_mempool_event_callback_unregister(test_mempool_events_cb, &data);
	for (i = 0; i < RTE_DIM(sdata); i++)
		rte_mempool_event_callback_unregister
				(test_mempool_events_safety_cb, &sdata[i]);
	/* in case of failure before the planned destruction */
	rte_mempool_free(mp);
	return ret;

#pragma pop_macro("RTE_TEST_TRACE_FAILURE")
}

#pragma push_macro("RTE_TEST_TRACE_FAILURE")
#undef RTE_TEST_TRACE_FAILURE
#define RTE_TEST_TRACE_FAILURE(...) do { \
		ret = TEST_FAILED; \
		goto exit; \
	} while (0)

static int
test_mempool_flag_non_io_set_when_no_iova_contig_set(void)
{
	const struct rte_memzone *mz = NULL;
	void *virt;
	rte_iova_t iova;
	size_t size = MEMPOOL_ELT_SIZE * 16;
	struct rte_mempool *mp = NULL;
	int ret;

	mz = rte_memzone_reserve("test_mempool", size, SOCKET_ID_ANY, 0);
	RTE_TEST_ASSERT_NOT_NULL(mz, "Cannot allocate memory");
	virt = mz->addr;
	iova = mz->iova;
	mp = rte_mempool_create_empty("empty", MEMPOOL_SIZE,
				      MEMPOOL_ELT_SIZE, 0, 0,
				      SOCKET_ID_ANY, RTE_MEMPOOL_F_NO_IOVA_CONTIG);
	RTE_TEST_ASSERT_NOT_NULL(mp, "Cannot create mempool: %s",
				 rte_strerror(rte_errno));
	rte_mempool_set_ops_byname(mp, rte_mbuf_best_mempool_ops(), NULL);

	RTE_TEST_ASSERT(mp->flags & RTE_MEMPOOL_F_NON_IO,
			"NON_IO flag is not set on an empty mempool");

	/*
	 * Always use valid IOVA so that populate() has no other reason
	 * to infer that the mempool cannot be used for IO.
	 */
	ret = rte_mempool_populate_iova(mp, virt, iova, size, NULL, NULL);
	RTE_TEST_ASSERT(ret > 0, "Failed to populate mempool: %s",
			rte_strerror(-ret));
	RTE_TEST_ASSERT(mp->flags & RTE_MEMPOOL_F_NON_IO,
			"NON_IO flag is not set when NO_IOVA_CONTIG is set");
	ret = TEST_SUCCESS;
exit:
	rte_mempool_free(mp);
	rte_memzone_free(mz);
	return ret;
}

static int
test_mempool_flag_non_io_unset_when_populated_with_valid_iova(void)
{
	const struct rte_memzone *mz = NULL;
	void *virt;
	rte_iova_t iova;
	size_t total_size = MEMPOOL_ELT_SIZE * MEMPOOL_SIZE;
	size_t block_size = total_size / 3;
	struct rte_mempool *mp = NULL;
	int ret;

	/*
	 * Since objects from the pool are never used in the test,
	 * we don't care for contiguous IOVA, on the other hand,
	 * requiring it could cause spurious test failures.
	 */
	mz = rte_memzone_reserve("test_mempool", total_size, SOCKET_ID_ANY, 0);
	RTE_TEST_ASSERT_NOT_NULL(mz, "Cannot allocate memory");
	virt = mz->addr;
	iova = mz->iova;
	mp = rte_mempool_create_empty("empty", MEMPOOL_SIZE,
				      MEMPOOL_ELT_SIZE, 0, 0,
				      SOCKET_ID_ANY, 0);
	RTE_TEST_ASSERT_NOT_NULL(mp, "Cannot create mempool: %s",
				 rte_strerror(rte_errno));

	RTE_TEST_ASSERT(mp->flags & RTE_MEMPOOL_F_NON_IO,
			"NON_IO flag is not set on an empty mempool");

	ret = rte_mempool_populate_iova(mp, RTE_PTR_ADD(virt, 1 * block_size),
					RTE_BAD_IOVA, block_size, NULL, NULL);
	RTE_TEST_ASSERT(ret > 0, "Failed to populate mempool: %s",
			rte_strerror(-ret));
	RTE_TEST_ASSERT(mp->flags & RTE_MEMPOOL_F_NON_IO,
			"NON_IO flag is not set when mempool is populated with only RTE_BAD_IOVA");

	ret = rte_mempool_populate_iova(mp, virt, iova, block_size, NULL, NULL);
	RTE_TEST_ASSERT(ret > 0, "Failed to populate mempool: %s",
			rte_strerror(-ret));
	RTE_TEST_ASSERT(!(mp->flags & RTE_MEMPOOL_F_NON_IO),
			"NON_IO flag is not unset when mempool is populated with valid IOVA");

	ret = rte_mempool_populate_iova(mp, RTE_PTR_ADD(virt, 2 * block_size),
					RTE_BAD_IOVA, block_size, NULL, NULL);
	RTE_TEST_ASSERT(ret > 0, "Failed to populate mempool: %s",
			rte_strerror(-ret));
	RTE_TEST_ASSERT(!(mp->flags & RTE_MEMPOOL_F_NON_IO),
			"NON_IO flag is set even when some objects have valid IOVA");
	ret = TEST_SUCCESS;

exit:
	rte_mempool_free(mp);
	rte_memzone_free(mz);
	return ret;
}

#pragma pop_macro("RTE_TEST_TRACE_FAILURE")

static int
test_mempool(void)
{
	int ret = -1;
	uint32_t nb_objs = 0;
	uint32_t nb_mem_chunks = 0;
	struct rte_mempool *mp_cache = NULL;
	struct rte_mempool *mp_nocache = NULL;
	struct rte_mempool *mp_stack_anon = NULL;
	struct rte_mempool *mp_stack_mempool_iter = NULL;
	struct rte_mempool *mp_stack = NULL;
	struct rte_mempool *default_pool = NULL;
	struct mp_data cb_arg = {
		.ret = -1
	};
	const char *default_pool_ops = rte_mbuf_best_mempool_ops();

	/* create a mempool (without cache) */
	mp_nocache = rte_mempool_create("test_nocache", MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE, 0, 0,
		NULL, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY, 0);

	if (mp_nocache == NULL) {
		printf("cannot allocate mp_nocache mempool\n");
		GOTO_ERR(ret, err);
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
		GOTO_ERR(ret, err);
	}

	/* create an empty mempool  */
	mp_stack_anon = rte_mempool_create_empty("test_stack_anon",
		MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE,
		RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
		SOCKET_ID_ANY, 0);

	if (mp_stack_anon == NULL)
		GOTO_ERR(ret, err);

	/* populate an empty mempool */
	ret = rte_mempool_populate_anon(mp_stack_anon);
	printf("%s ret = %d\n", __func__, ret);
	if (ret < 0)
		GOTO_ERR(ret, err);

	/* Try to populate when already populated */
	ret = rte_mempool_populate_anon(mp_stack_anon);
	if (ret != 0)
		GOTO_ERR(ret, err);

	/* create a mempool  */
	mp_stack_mempool_iter = rte_mempool_create("test_iter_obj",
		MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE,
		RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
		NULL, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY, 0);

	if (mp_stack_mempool_iter == NULL)
		GOTO_ERR(ret, err);

	/* test to initialize mempool objects and memory */
	nb_objs = rte_mempool_obj_iter(mp_stack_mempool_iter, my_obj_init,
			NULL);
	if (nb_objs == 0)
		GOTO_ERR(ret, err);

	nb_mem_chunks = rte_mempool_mem_iter(mp_stack_mempool_iter,
			test_mp_mem_init, &cb_arg);
	if (nb_mem_chunks == 0 || cb_arg.ret < 0)
		GOTO_ERR(ret, err);

	/* create a mempool with an external handler */
	mp_stack = rte_mempool_create_empty("test_stack",
		MEMPOOL_SIZE,
		MEMPOOL_ELT_SIZE,
		RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
		SOCKET_ID_ANY, 0);

	if (mp_stack == NULL) {
		printf("cannot allocate mp_stack mempool\n");
		GOTO_ERR(ret, err);
	}
	if (rte_mempool_set_ops_byname(mp_stack, "stack", NULL) < 0) {
		printf("cannot set stack handler\n");
		GOTO_ERR(ret, err);
	}
	if (rte_mempool_populate_default(mp_stack) < 0) {
		printf("cannot populate mp_stack mempool\n");
		GOTO_ERR(ret, err);
	}
	rte_mempool_obj_iter(mp_stack, my_obj_init, NULL);

	/* Create a mempool based on Default handler */
	printf("Testing %s mempool handler\n", default_pool_ops);
	default_pool = rte_mempool_create_empty("default_pool",
						MEMPOOL_SIZE,
						MEMPOOL_ELT_SIZE,
						RTE_MEMPOOL_CACHE_MAX_SIZE, 0,
						SOCKET_ID_ANY, 0);

	if (default_pool == NULL) {
		printf("cannot allocate default mempool\n");
		GOTO_ERR(ret, err);
	}
	if (rte_mempool_set_ops_byname(default_pool,
				default_pool_ops, NULL) < 0) {
		printf("cannot set %s handler\n", default_pool_ops);
		GOTO_ERR(ret, err);
	}
	if (rte_mempool_populate_default(default_pool) < 0) {
		printf("cannot populate %s mempool\n", default_pool_ops);
		GOTO_ERR(ret, err);
	}
	rte_mempool_obj_iter(default_pool, my_obj_init, NULL);

	/* retrieve the mempool from its name */
	if (rte_mempool_lookup("test_nocache") != mp_nocache) {
		printf("Cannot lookup mempool from its name\n");
		GOTO_ERR(ret, err);
	}

	printf("Walk into mempools:\n");
	rte_mempool_walk(walk_cb, NULL);

	rte_mempool_list_dump(stdout);

	/* basic tests without cache */
	if (test_mempool_basic(mp_nocache, 0) < 0)
		GOTO_ERR(ret, err);

	/* basic tests with cache */
	if (test_mempool_basic(mp_cache, 0) < 0)
		GOTO_ERR(ret, err);

	/* basic tests with user-owned cache */
	if (test_mempool_basic(mp_nocache, 1) < 0)
		GOTO_ERR(ret, err);

	/* more basic tests without cache */
	if (test_mempool_basic_ex(mp_nocache) < 0)
		GOTO_ERR(ret, err);

	/* mempool operation test based on single producer and single consumer */
	if (test_mempool_sp_sc() < 0)
		GOTO_ERR(ret, err);

	if (test_mempool_creation_with_exceeded_cache_size() < 0)
		GOTO_ERR(ret, err);

	if (test_mempool_creation_with_invalid_flags() < 0)
		GOTO_ERR(ret, err);

	if (test_mempool_same_name_twice_creation() < 0)
		GOTO_ERR(ret, err);

	/* test the stack handler */
	if (test_mempool_basic(mp_stack, 1) < 0)
		GOTO_ERR(ret, err);

	if (test_mempool_basic(default_pool, 1) < 0)
		GOTO_ERR(ret, err);

	/* test mempool event callbacks */
	if (test_mempool_events(rte_mempool_populate_default) < 0)
		GOTO_ERR(ret, err);
	if (test_mempool_events(rte_mempool_populate_anon) < 0)
		GOTO_ERR(ret, err);
	if (test_mempool_events_safety() < 0)
		GOTO_ERR(ret, err);

	/* test NON_IO flag inference */
	if (test_mempool_flag_non_io_set_when_no_iova_contig_set() < 0)
		GOTO_ERR(ret, err);
	if (test_mempool_flag_non_io_unset_when_populated_with_valid_iova() < 0)
		GOTO_ERR(ret, err);

	rte_mempool_list_dump(stdout);

	ret = 0;

err:
	rte_mempool_free(mp_nocache);
	rte_mempool_free(mp_cache);
	rte_mempool_free(mp_stack_anon);
	rte_mempool_free(mp_stack_mempool_iter);
	rte_mempool_free(mp_stack);
	rte_mempool_free(default_pool);

	return ret;
}

REGISTER_FAST_TEST(mempool_autotest, false, true, test_mempool);
