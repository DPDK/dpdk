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

#ifdef RTE_LIBRTE_HASH
#include <rte_hash.h>
#include <rte_fbk_hash.h>
#include <rte_jhash.h>
#endif /* RTE_LIBRTE_HASH */

#ifdef RTE_LIBRTE_LPM
#include <rte_lpm.h>
#endif /* RTE_LIBRTE_LPM */

#include <rte_string_fns.h>

#include "test.h"

typedef int (*case_func_t)(void* arg);
typedef void (*case_clean_t)(unsigned lcore_id);

#define MAX_STRING_SIZE                     (256)
#define MAX_ITER_TIMES                      (16)
#define MAX_LPM_ITER_TIMES                  (8)

#define MEMPOOL_ELT_SIZE                    (0)
#define MEMPOOL_SIZE                        (4)

#define MAX_LCORES	RTE_MAX_MEMZONE / (MAX_ITER_TIMES * 4U)

static rte_atomic32_t synchro = RTE_ATOMIC32_INIT(0);

#define WAIT_SYNCHRO_FOR_SLAVES()   do{ \
	if (lcore_self != rte_get_master_lcore())                  \
		while (rte_atomic32_read(&synchro) == 0);        \
} while(0)

/*
 * rte_eal_init only init once
 */
static int
test_eal_init_once(__attribute__((unused)) void *arg)
{
	unsigned lcore_self =  rte_lcore_id();

	WAIT_SYNCHRO_FOR_SLAVES();

	if (rte_eal_init(0, NULL) != -1)
		return -1;

	return 0;
}

/*
 * ring create/lookup reentrancy test
 */
static int
ring_create_lookup(__attribute__((unused)) void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_ring * rp;
	char ring_name[MAX_STRING_SIZE];
	int i;

	WAIT_SYNCHRO_FOR_SLAVES();

	/* create the same ring simultaneously on all threads */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		rp = rte_ring_create("fr_test_once", 4096, SOCKET_ID_ANY, 0);
		if ((NULL == rp) && (rte_ring_lookup("fr_test_once") == NULL))
			return -1;
	}

	/* create/lookup new ring several times */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(ring_name, sizeof(ring_name), "fr_test_%d_%d", lcore_self, i);
		rp = rte_ring_create(ring_name, 4096, SOCKET_ID_ANY, 0);
		if (NULL == rp)
			return -1;
		if (rte_ring_lookup(ring_name) != rp)
			return -1;
	}

	/* verify all ring created sucessful */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(ring_name, sizeof(ring_name), "fr_test_%d_%d", lcore_self, i);
		if (rte_ring_lookup(ring_name) == NULL)
			return -1;
	}

	return 0;
}

static void
my_obj_init(struct rte_mempool *mp, __attribute__((unused)) void *arg,
	    void *obj, unsigned i)
{
	uint32_t *objnum = obj;
	memset(obj, 0, mp->elt_size);
	*objnum = i;
}

static int
mempool_create_lookup(__attribute__((unused)) void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_mempool * mp;
	char mempool_name[MAX_STRING_SIZE];
	int i;

	WAIT_SYNCHRO_FOR_SLAVES();

	/* create the same mempool simultaneously on all threads */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		mp = rte_mempool_create("fr_test_once",  MEMPOOL_SIZE,
					MEMPOOL_ELT_SIZE, 0, 0,
					NULL, NULL,
					my_obj_init, NULL,
					SOCKET_ID_ANY, 0);
		if ((NULL == mp) && (rte_mempool_lookup("fr_test_once") == NULL))
			return -1;
	}

	/* create/lookup new ring several times */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
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
	}

	/* verify all ring created sucessful */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(mempool_name, sizeof(mempool_name), "fr_test_%d_%d", lcore_self, i);
		if (rte_mempool_lookup(mempool_name) == NULL)
			return -1;
	}

	return 0;
}

#ifdef RTE_LIBRTE_HASH
static void
hash_clean(unsigned lcore_id)
{
	char hash_name[MAX_STRING_SIZE];
	struct rte_hash *handle;
	int i;

	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(hash_name, sizeof(hash_name), "fr_test_%d_%d",  lcore_id, i);

		if ((handle = rte_hash_find_existing(hash_name)) != NULL)
			rte_hash_free(handle);
	}
}

static int
hash_create_free(__attribute__((unused)) void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_hash *handle;
	char hash_name[MAX_STRING_SIZE];
	int i;
	struct rte_hash_parameters hash_params = {
		.name = NULL,
		.entries = 16,
		.bucket_entries = 4,
		.key_len = 4,
		.hash_func = (rte_hash_function)rte_jhash2,
		.hash_func_init_val = 0,
		.socket_id = 0,
	};

	WAIT_SYNCHRO_FOR_SLAVES();

	/* create the same hash simultaneously on all threads */
	hash_params.name = "fr_test_once";
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		handle = rte_hash_create(&hash_params);
		if ((NULL == handle) && (rte_hash_find_existing("fr_test_once") == NULL))
			return -1;
	}

	/* create mutiple times simultaneously */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(hash_name, sizeof(hash_name), "fr_test_%d_%d", lcore_self, i);
		hash_params.name = hash_name;

		handle = rte_hash_create(&hash_params);
		if (NULL == handle)
			return -1;

		/* verify correct existing and then free all */
		if (handle != rte_hash_find_existing(hash_name))
			return -1;

		rte_hash_free(handle);
	}

	/* verify free correct */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(hash_name, sizeof(hash_name), "fr_test_%d_%d",  lcore_self, i);

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

	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(fbk_name, sizeof(fbk_name), "fr_test_%d_%d",  lcore_id, i);

		if ((handle = rte_fbk_hash_find_existing(fbk_name)) != NULL)
			rte_fbk_hash_free(handle);
	}
}

static int
fbk_create_free(__attribute__((unused)) void *arg)
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

	WAIT_SYNCHRO_FOR_SLAVES();

	/* create the same fbk hash table simultaneously on all threads */
	fbk_params.name = "fr_test_once";
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		handle = rte_fbk_hash_create(&fbk_params);
		if ((NULL == handle) && (rte_fbk_hash_find_existing("fr_test_once") == NULL))
			return -1;
	}

	/* create mutiple fbk tables simultaneously */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(fbk_name, sizeof(fbk_name), "fr_test_%d_%d", lcore_self, i);
		fbk_params.name = fbk_name;

		handle = rte_fbk_hash_create(&fbk_params);
		if (NULL == handle)
			return -1;

		/* verify correct existing and then free all */
		if (handle != rte_fbk_hash_find_existing(fbk_name))
			return -1;

		rte_fbk_hash_free(handle);
	}

	/* verify free correct */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		snprintf(fbk_name, sizeof(fbk_name), "fr_test_%d_%d",  lcore_self, i);

		if (NULL != rte_fbk_hash_find_existing(fbk_name))
			return -1;
	}

	return 0;
}
#endif /* RTE_LIBRTE_HASH */

#ifdef RTE_LIBRTE_LPM
static void
lpm_clean(unsigned lcore_id)
{
	char lpm_name[MAX_STRING_SIZE];
	struct rte_lpm *lpm;
	int i;

	for (i = 0; i < MAX_LPM_ITER_TIMES; i++) {
		snprintf(lpm_name, sizeof(lpm_name), "fr_test_%d_%d",  lcore_id, i);

		if ((lpm = rte_lpm_find_existing(lpm_name)) != NULL)
			rte_lpm_free(lpm);
	}
}

static int
lpm_create_free(__attribute__((unused)) void *arg)
{
	unsigned lcore_self = rte_lcore_id();
	struct rte_lpm *lpm;
	char lpm_name[MAX_STRING_SIZE];
	int i;

	WAIT_SYNCHRO_FOR_SLAVES();

	/* create the same lpm simultaneously on all threads */
	for (i = 0; i < MAX_ITER_TIMES; i++) {
		lpm = rte_lpm_create("fr_test_once",  SOCKET_ID_ANY, 4, RTE_LPM_HEAP);
		if ((NULL == lpm) && (rte_lpm_find_existing("fr_test_once") == NULL))
			return -1;
	}

	/* create mutiple fbk tables simultaneously */
	for (i = 0; i < MAX_LPM_ITER_TIMES; i++) {
		snprintf(lpm_name, sizeof(lpm_name), "fr_test_%d_%d", lcore_self, i);
		lpm = rte_lpm_create(lpm_name, SOCKET_ID_ANY, 4, RTE_LPM_HEAP);
		if (NULL == lpm)
			return -1;

		/* verify correct existing and then free all */
		if (lpm != rte_lpm_find_existing(lpm_name))
			return -1;

		rte_lpm_free(lpm);
	}

	/* verify free correct */
	for (i = 0; i < MAX_LPM_ITER_TIMES; i++) {
		snprintf(lpm_name, sizeof(lpm_name), "fr_test_%d_%d",  lcore_self, i);
		if (NULL != rte_lpm_find_existing(lpm_name))
			return -1;
	}

	return 0;
}
#endif /* RTE_LIBRTE_LPM */

struct test_case{
	case_func_t    func;
	void*          arg;
	case_clean_t   clean;
	char           name[MAX_STRING_SIZE];
};

/* All test cases in the test suite */
struct test_case test_cases[] = {
	{ test_eal_init_once,     NULL,  NULL,         "eal init once" },
	{ ring_create_lookup,     NULL,  NULL,         "ring create/lookup" },
	{ mempool_create_lookup,  NULL,  NULL,         "mempool create/lookup" },
#ifdef RTE_LIBRTE_HASH
	{ hash_create_free,       NULL,  hash_clean,   "hash create/free" },
	{ fbk_create_free,        NULL,  fbk_clean,    "fbk create/free" },
#endif /* RTE_LIBRTE_HASH */
#ifdef RTE_LIBRTE_LPM
	{ lpm_create_free,        NULL,  lpm_clean,    "lpm create/free" },
#endif /* RTE_LIBRTE_LPM */
};

/**
 * launch test case in two separate thread
 */
static int
launch_test(struct test_case *pt_case)
{
	int ret = 0;
	unsigned lcore_id;
	unsigned cores_save = rte_lcore_count();
	unsigned cores = RTE_MIN(cores_save, MAX_LCORES);

	if (pt_case->func == NULL)
		return -1;

	rte_atomic32_set(&synchro, 0);

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (cores == 1)
			break;
		cores--;
		rte_eal_remote_launch(pt_case->func, pt_case->arg, lcore_id);
	}

	rte_atomic32_set(&synchro, 1);

	if (pt_case->func(pt_case->arg) < 0)
		ret = -1;

	cores = cores_save;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (cores == 1)
			break;
		cores--;
		if (rte_eal_wait_lcore(lcore_id) < 0)
			ret = -1;

		if (pt_case->clean != NULL)
			pt_case->clean(lcore_id);
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

	if (rte_lcore_count() <= 1) {
		printf("Not enough lcore for testing\n");
		return -1;
	}
	else if (rte_lcore_count() > MAX_LCORES)
		printf("Too many lcores, some cores will be disabled\n");

	for (case_id = 0; case_id < sizeof(test_cases)/sizeof(struct test_case); case_id ++) {
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

static struct test_command func_reentrancy_cmd = {
	.command = "func_reentrancy_autotest",
	.callback = test_func_reentrancy,
};
REGISTER_TEST_COMMAND(func_reentrancy_cmd);
