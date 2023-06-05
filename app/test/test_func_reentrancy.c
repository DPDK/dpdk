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
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>

#ifdef RTE_LIB_HASH
#include <rte_hash.h>
#include <rte_fbk_hash.h>
#include <rte_jhash.h>
#endif /* RTE_LIB_HASH */

#ifdef RTE_LIB_LPM
#include <rte_lpm.h>
#endif /* RTE_LIB_LPM */

#include <rte_string_fns.h>

#include "test.h"

typedef int (*case_func_t)(void* arg);
typedef void (*case_clean_t)(unsigned lcore_id);

#define MAX_STRING_SIZE                     (256)
#define MAX_ITER_MULTI                      (16)
#define MAX_ITER_ONCE                       (4)
#define MAX_LPM_ITER_TIMES                  (6)

#define MEMPOOL_ELT_SIZE                    (sizeof(uint32_t))
#define MEMPOOL_SIZE                        (4)

#define MAX_LCORES	(rte_memzone_max_get() / (MAX_ITER_MULTI * 4U))

static uint32_t obj_count;
static uint32_t synchro;

#define WAIT_SYNCHRO_FOR_WORKERS()   do { \
	if (lcore_self != rte_get_main_lcore())                  \
		rte_wait_until_equal_32(&synchro, 1, __ATOMIC_RELAXED); \
} while(0)

/*
 * rte_eal_init only init once
 */
static int
test_eal_init_once(__rte_unused void *arg)
{
	unsigned lcore_self =  rte_lcore_id();

	WAIT_SYNCHRO_FOR_WORKERS();

	__atomic_store_n(&obj_count, 1, __ATOMIC_RELAXED); /* silent the check in the caller */
	if (rte_eal_init(0, NULL) != -1)
		return -1;

	return 0;
}

/*
 * ring create/lookup reentrancy test
 */
static void
ring_clean(unsigned int lcore_id)
{
	struct rte_ring *rp;
	char ring_name[MAX_STRING_SIZE];
	int i;

	rp = rte_ring_lookup("fr_test_once");
	rte_ring_free(rp);

	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(ring_name, sizeof(ring_name),
				"fr_test_%d_%d", lcore_id, i);
		rp = rte_ring_lookup(ring_name);
		rte_ring_free(rp);
	}
}

static int
ring_create_lookup(__rte_unused void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_ring * rp;
	char ring_name[MAX_STRING_SIZE];
	int i;

	WAIT_SYNCHRO_FOR_WORKERS();

	/* create the same ring simultaneously on all threads */
	for (i = 0; i < MAX_ITER_ONCE; i++) {
		rp = rte_ring_create("fr_test_once", 4096, SOCKET_ID_ANY, 0);
		if (rp != NULL)
			__atomic_fetch_add(&obj_count, 1, __ATOMIC_RELAXED);
	}

	/* create/lookup new ring several times */
	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(ring_name, sizeof(ring_name), "fr_test_%d_%d", lcore_self, i);
		rp = rte_ring_create(ring_name, 4096, SOCKET_ID_ANY, 0);
		if (NULL == rp)
			return -1;
		if (rte_ring_lookup(ring_name) != rp)
			return -1;

		/* verify all ring created successful */
		if (rte_ring_lookup(ring_name) == NULL)
			return -1;
	}

	return 0;
}

static void
my_obj_init(struct rte_mempool *mp, __rte_unused void *arg,
	    void *obj, unsigned i)
{
	uint32_t *objnum = obj;
	memset(obj, 0, mp->elt_size);
	*objnum = i;
}

static void
mempool_clean(unsigned int lcore_id)
{
	struct rte_mempool *mp;
	char mempool_name[MAX_STRING_SIZE];
	int i;

	mp = rte_mempool_lookup("fr_test_once");
	rte_mempool_free(mp);

	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(mempool_name, sizeof(mempool_name), "fr_test_%d_%d",
			 lcore_id, i);
		mp = rte_mempool_lookup(mempool_name);
		rte_mempool_free(mp);
	}
}

static int
mempool_create_lookup(__rte_unused void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_mempool * mp;
	char mempool_name[MAX_STRING_SIZE];
	int i;

	WAIT_SYNCHRO_FOR_WORKERS();

	/* create the same mempool simultaneously on all threads */
	for (i = 0; i < MAX_ITER_ONCE; i++) {
		mp = rte_mempool_create("fr_test_once",  MEMPOOL_SIZE,
					MEMPOOL_ELT_SIZE, 0, 0,
					NULL, NULL,
					my_obj_init, NULL,
					SOCKET_ID_ANY, 0);
		if (mp != NULL)
			__atomic_fetch_add(&obj_count, 1, __ATOMIC_RELAXED);
	}

	/* create/lookup new ring several times */
	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(mempool_name, sizeof(mempool_name), "fr_test_%d_%d", lcore_self, i);
		mp = rte_mempool_create(mempool_name, MEMPOOL_SIZE,
						MEMPOOL_ELT_SIZE, 0, 0,
						NULL, NULL,
						my_obj_init, NULL,
						SOCKET_ID_ANY, 0);
		if (NULL == mp)
			return -1;
		if (rte_mempool_lookup(mempool_name) != mp)
			return -1;

		/* verify all ring created successful */
		if (rte_mempool_lookup(mempool_name) == NULL)
			return -1;
	}

	return 0;
}

#ifdef RTE_LIB_HASH
static void
hash_clean(unsigned lcore_id)
{
	char hash_name[MAX_STRING_SIZE];
	struct rte_hash *handle;
	int i;

	handle = rte_hash_find_existing("fr_test_once");
	rte_hash_free(handle);

	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(hash_name, sizeof(hash_name), "fr_test_%d_%d",  lcore_id, i);

		if ((handle = rte_hash_find_existing(hash_name)) != NULL)
			rte_hash_free(handle);
	}
}

static int
hash_create_free(__rte_unused void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_hash *handle;
	char hash_name[MAX_STRING_SIZE];
	int i;
	struct rte_hash_parameters hash_params = {
		.name = NULL,
		.entries = 16,
		.key_len = 4,
		.hash_func = (rte_hash_function)rte_jhash_32b,
		.hash_func_init_val = 0,
		.socket_id = 0,
	};

	WAIT_SYNCHRO_FOR_WORKERS();

	/* create the same hash simultaneously on all threads */
	hash_params.name = "fr_test_once";
	for (i = 0; i < MAX_ITER_ONCE; i++) {
		handle = rte_hash_create(&hash_params);
		if (handle != NULL)
			__atomic_fetch_add(&obj_count, 1, __ATOMIC_RELAXED);
	}

	/* create multiple times simultaneously */
	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(hash_name, sizeof(hash_name), "fr_test_%d_%d", lcore_self, i);
		hash_params.name = hash_name;

		handle = rte_hash_create(&hash_params);
		if (NULL == handle)
			return -1;

		/* verify correct existing and then free all */
		if (handle != rte_hash_find_existing(hash_name))
			return -1;

		rte_hash_free(handle);

		/* verify free correct */
		if (NULL != rte_hash_find_existing(hash_name))
			return -1;
	}

	return 0;
}

static void
fbk_clean(unsigned lcore_id)
{
	char fbk_name[MAX_STRING_SIZE];
	struct rte_fbk_hash_table *handle;
	int i;

	handle = rte_fbk_hash_find_existing("fr_test_once");
	rte_fbk_hash_free(handle);

	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(fbk_name, sizeof(fbk_name), "fr_test_%d_%d",  lcore_id, i);

		if ((handle = rte_fbk_hash_find_existing(fbk_name)) != NULL)
			rte_fbk_hash_free(handle);
	}
}

static int
fbk_create_free(__rte_unused void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_fbk_hash_table *handle;
	char fbk_name[MAX_STRING_SIZE];
	int i;
	struct rte_fbk_hash_params fbk_params = {
		.name = NULL,
		.entries = 4,
		.entries_per_bucket = 4,
		.socket_id = 0,
		.hash_func = rte_jhash_1word,
		.init_val = RTE_FBK_HASH_INIT_VAL_DEFAULT,
	};

	WAIT_SYNCHRO_FOR_WORKERS();

	/* create the same fbk hash table simultaneously on all threads */
	fbk_params.name = "fr_test_once";
	for (i = 0; i < MAX_ITER_ONCE; i++) {
		handle = rte_fbk_hash_create(&fbk_params);
		if (handle != NULL)
			__atomic_fetch_add(&obj_count, 1, __ATOMIC_RELAXED);
	}

	/* create multiple fbk tables simultaneously */
	for (i = 0; i < MAX_ITER_MULTI; i++) {
		snprintf(fbk_name, sizeof(fbk_name), "fr_test_%d_%d", lcore_self, i);
		fbk_params.name = fbk_name;

		handle = rte_fbk_hash_create(&fbk_params);
		if (NULL == handle)
			return -1;

		/* verify correct existing and then free all */
		if (handle != rte_fbk_hash_find_existing(fbk_name))
			return -1;

		rte_fbk_hash_free(handle);

		/* verify free correct */
		if (NULL != rte_fbk_hash_find_existing(fbk_name))
			return -1;
	}

	return 0;
}
#endif /* RTE_LIB_HASH */

#ifdef RTE_LIB_LPM
static void
lpm_clean(unsigned int lcore_id)
{
	char lpm_name[MAX_STRING_SIZE];
	struct rte_lpm *lpm;
	int i;

	lpm = rte_lpm_find_existing("fr_test_once");
	rte_lpm_free(lpm);

	for (i = 0; i < MAX_LPM_ITER_TIMES; i++) {
		snprintf(lpm_name, sizeof(lpm_name), "fr_test_%d_%d",  lcore_id, i);

		if ((lpm = rte_lpm_find_existing(lpm_name)) != NULL)
			rte_lpm_free(lpm);
	}
}

static int
lpm_create_free(__rte_unused void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_lpm *lpm;
	struct rte_lpm_config config;

	config.max_rules = 4;
	config.number_tbl8s = 256;
	config.flags = 0;
	char lpm_name[MAX_STRING_SIZE];
	int i;

	WAIT_SYNCHRO_FOR_WORKERS();

	/* create the same lpm simultaneously on all threads */
	for (i = 0; i < MAX_ITER_ONCE; i++) {
		lpm = rte_lpm_create("fr_test_once",  SOCKET_ID_ANY, &config);
		if (lpm != NULL)
			__atomic_fetch_add(&obj_count, 1, __ATOMIC_RELAXED);
	}

	/* create multiple fbk tables simultaneously */
	for (i = 0; i < MAX_LPM_ITER_TIMES; i++) {
		snprintf(lpm_name, sizeof(lpm_name), "fr_test_%d_%d", lcore_self, i);
		lpm = rte_lpm_create(lpm_name, SOCKET_ID_ANY, &config);
		if (NULL == lpm)
			return -1;

		/* verify correct existing and then free all */
		if (lpm != rte_lpm_find_existing(lpm_name))
			return -1;

		rte_lpm_free(lpm);

		/* verify free correct */
		if (NULL != rte_lpm_find_existing(lpm_name))
			return -1;
	}

	return 0;
}
#endif /* RTE_LIB_LPM */

struct test_case{
	case_func_t    func;
	void*          arg;
	case_clean_t   clean;
	char           name[MAX_STRING_SIZE];
};

/* All test cases in the test suite */
struct test_case test_cases[] = {
	{ test_eal_init_once,     NULL,  NULL,         "eal init once" },
	{ ring_create_lookup,     NULL,  ring_clean,   "ring create/lookup" },
	{ mempool_create_lookup,  NULL,  mempool_clean,
			"mempool create/lookup" },
#ifdef RTE_LIB_HASH
	{ hash_create_free,       NULL,  hash_clean,   "hash create/free" },
	{ fbk_create_free,        NULL,  fbk_clean,    "fbk create/free" },
#endif /* RTE_LIB_HASH */
#ifdef RTE_LIB_LPM
	{ lpm_create_free,        NULL,  lpm_clean,    "lpm create/free" },
#endif /* RTE_LIB_LPM */
};

/**
 * launch test case in two separate thread
 */
static int
launch_test(struct test_case *pt_case)
{
	unsigned int lcore_id;
	unsigned int cores;
	unsigned int count;
	int ret = 0;

	if (pt_case->func == NULL)
		return -1;

	__atomic_store_n(&obj_count, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&synchro, 0, __ATOMIC_RELAXED);

	cores = RTE_MIN(rte_lcore_count(), MAX_LCORES);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (cores == 1)
			break;
		cores--;
		rte_eal_remote_launch(pt_case->func, pt_case->arg, lcore_id);
	}

	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);

	if (pt_case->func(pt_case->arg) < 0)
		ret = -1;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			ret = -1;
	}

	RTE_LCORE_FOREACH(lcore_id) {
		if (pt_case->clean != NULL)
			pt_case->clean(lcore_id);
	}

	count = __atomic_load_n(&obj_count, __ATOMIC_RELAXED);
	if (count != 1) {
		printf("%s: common object allocated %d times (should be 1)\n",
			pt_case->name, count);
		ret = -1;
	}

	return ret;
}

/**
 * Main entry of func_reentrancy test
 */
static int
test_func_reentrancy(void)
{
	uint32_t case_id;
	struct test_case *pt_case = NULL;

	if (RTE_EXEC_ENV_IS_WINDOWS)
		return TEST_SKIPPED;

	if (rte_lcore_count() < 2) {
		printf("Not enough cores for func_reentrancy_autotest, expecting at least 2\n");
		return TEST_SKIPPED;
	}
	else if (rte_lcore_count() > MAX_LCORES)
		printf("Too many lcores, some cores will be disabled\n");

	for (case_id = 0; case_id < RTE_DIM(test_cases); case_id++) {
		pt_case = &test_cases[case_id];
		if (pt_case->func == NULL)
			continue;

		if (launch_test(pt_case) < 0) {
			printf("Func-ReEnt CASE %"PRIu32": %s FAIL\n", case_id, pt_case->name);
			return -1;
		}
		printf("Func-ReEnt CASE %"PRIu32": %s PASS\n", case_id, pt_case->name);
	}

	return 0;
}

REGISTER_TEST_COMMAND(func_reentrancy_autotest, test_func_reentrancy);
