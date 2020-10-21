/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_ip.h>
#include <rte_string_fns.h>

#include "test.h"

#include <rte_hash.h>
#include <rte_fbk_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>

/*******************************************************************************
 * Hash function performance test configuration section. Each performance test
 * will be performed HASHTEST_ITERATIONS times.
 *
 * The five arrays below control what tests are performed. Every combination
 * from the array entries is tested.
 */
static rte_hash_function hashtest_funcs[] = {rte_jhash, rte_hash_crc};
static uint32_t hashtest_initvals[] = {0};
static uint32_t hashtest_key_lens[] = {0, 2, 4, 5, 6, 7, 8, 10, 11, 15, 16, 21, 31, 32, 33, 63, 64};
#define MAX_KEYSIZE 64
/******************************************************************************/
#define LOCAL_FBK_HASH_ENTRIES_MAX (1 << 15)

/*
 * Check condition and return an error if true. Assumes that "handle" is the
 * name of the hash structure pointer to be freed.
 */
#define RETURN_IF_ERROR(cond, str, ...) do {				\
	if (cond) {							\
		printf("ERROR line %d: " str "\n", __LINE__, ##__VA_ARGS__); \
		if (handle) rte_hash_free(handle);			\
		return -1;						\
	}								\
} while(0)

#define RETURN_IF_ERROR_FBK(cond, str, ...) do {				\
	if (cond) {							\
		printf("ERROR line %d: " str "\n", __LINE__, ##__VA_ARGS__); \
		if (handle) rte_fbk_hash_free(handle);			\
		return -1;						\
	}								\
} while(0)

#define RETURN_IF_ERROR_RCU_QSBR(cond, str, ...) do {			\
	if (cond) {							\
		printf("ERROR line %d: " str "\n", __LINE__, ##__VA_ARGS__); \
		if (rcu_cfg.mode == RTE_HASH_QSBR_MODE_SYNC) {		\
			writer_done = 1;				\
			/* Wait until reader exited. */			\
			rte_eal_mp_wait_lcore();			\
		}							\
		rte_hash_free(g_handle);				\
		rte_free(g_qsv);					\
		return -1;						\
	}								\
} while (0)

/* 5-tuple key type */
struct flow_key {
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint8_t proto;
} __rte_packed;

/*
 * Hash function that always returns the same value, to easily test what
 * happens when a bucket is full.
 */
static uint32_t pseudo_hash(__rte_unused const void *keys,
			    __rte_unused uint32_t key_len,
			    __rte_unused uint32_t init_val)
{
	return 3;
}

RTE_LOG_REGISTER(hash_logtype_test, test.hash, INFO);

/*
 * Print out result of unit test hash operation.
 */
static void print_key_info(const char *msg, const struct flow_key *key,
								int32_t pos)
{
	const uint8_t *p = (const uint8_t *)key;
	unsigned int i;

	rte_log(RTE_LOG_DEBUG, hash_logtype_test, "%s key:0x", msg);
	for (i = 0; i < sizeof(struct flow_key); i++)
		rte_log(RTE_LOG_DEBUG, hash_logtype_test, "%02X", p[i]);
	rte_log(RTE_LOG_DEBUG, hash_logtype_test, " @ pos %d\n", pos);
}

/* Keys used by unit test functions */
static struct flow_key keys[5] = { {
	.ip_src = RTE_IPV4(0x03, 0x02, 0x01, 0x00),
	.ip_dst = RTE_IPV4(0x07, 0x06, 0x05, 0x04),
	.port_src = 0x0908,
	.port_dst = 0x0b0a,
	.proto = 0x0c,
}, {
	.ip_src = RTE_IPV4(0x13, 0x12, 0x11, 0x10),
	.ip_dst = RTE_IPV4(0x17, 0x16, 0x15, 0x14),
	.port_src = 0x1918,
	.port_dst = 0x1b1a,
	.proto = 0x1c,
}, {
	.ip_src = RTE_IPV4(0x23, 0x22, 0x21, 0x20),
	.ip_dst = RTE_IPV4(0x27, 0x26, 0x25, 0x24),
	.port_src = 0x2928,
	.port_dst = 0x2b2a,
	.proto = 0x2c,
}, {
	.ip_src = RTE_IPV4(0x33, 0x32, 0x31, 0x30),
	.ip_dst = RTE_IPV4(0x37, 0x36, 0x35, 0x34),
	.port_src = 0x3938,
	.port_dst = 0x3b3a,
	.proto = 0x3c,
}, {
	.ip_src = RTE_IPV4(0x43, 0x42, 0x41, 0x40),
	.ip_dst = RTE_IPV4(0x47, 0x46, 0x45, 0x44),
	.port_src = 0x4948,
	.port_dst = 0x4b4a,
	.proto = 0x4c,
} };

/* Parameters used for hash table in unit test functions. Name set later. */
static struct rte_hash_parameters ut_params = {
	.entries = 64,
	.key_len = sizeof(struct flow_key), /* 13 */
	.hash_func = rte_jhash,
	.hash_func_init_val = 0,
	.socket_id = 0,
};

#define CRC32_ITERATIONS (1U << 10)
#define CRC32_DWORDS (1U << 6)
/*
 * Test if all CRC32 implementations yield the same hash value
 */
static int
test_crc32_hash_alg_equiv(void)
{
	uint32_t hash_val;
	uint32_t init_val;
	uint64_t data64[CRC32_DWORDS];
	unsigned i, j;
	size_t data_len;

	printf("\n# CRC32 implementations equivalence test\n");
	for (i = 0; i < CRC32_ITERATIONS; i++) {
		/* Randomizing data_len of data set */
		data_len = (size_t) ((rte_rand() % sizeof(data64)) + 1);
		init_val = (uint32_t) rte_rand();

		/* Fill the data set */
		for (j = 0; j < CRC32_DWORDS; j++)
			data64[j] = rte_rand();

		/* Calculate software CRC32 */
		rte_hash_crc_set_alg(CRC32_SW);
		hash_val = rte_hash_crc(data64, data_len, init_val);

		/* Check against 4-byte-operand sse4.2 CRC32 if available */
		rte_hash_crc_set_alg(CRC32_SSE42);
		if (hash_val != rte_hash_crc(data64, data_len, init_val)) {
			printf("Failed checking CRC32_SW against CRC32_SSE42\n");
			break;
		}

		/* Check against 8-byte-operand sse4.2 CRC32 if available */
		rte_hash_crc_set_alg(CRC32_SSE42_x64);
		if (hash_val != rte_hash_crc(data64, data_len, init_val)) {
			printf("Failed checking CRC32_SW against CRC32_SSE42_x64\n");
			break;
		}

		/* Check against 8-byte-operand ARM64 CRC32 if available */
		rte_hash_crc_set_alg(CRC32_ARM64);
		if (hash_val != rte_hash_crc(data64, data_len, init_val)) {
			printf("Failed checking CRC32_SW against CRC32_ARM64\n");
			break;
		}
	}

	/* Resetting to best available algorithm */
	rte_hash_crc_set_alg(CRC32_SSE42_x64);

	if (i == CRC32_ITERATIONS)
		return 0;

	printf("Failed test data (hex, %zu bytes total):\n", data_len);
	for (j = 0; j < data_len; j++)
		printf("%02X%c", ((uint8_t *)data64)[j],
				((j+1) % 16 == 0 || j == data_len - 1) ? '\n' : ' ');

	return -1;
}

/*
 * Test a hash function.
 */
static void run_hash_func_test(rte_hash_function f, uint32_t init_val,
		uint32_t key_len)
{
	static uint8_t key[MAX_KEYSIZE];
	unsigned i;


	for (i = 0; i < key_len; i++)
		key[i] = (uint8_t) rte_rand();

	/* just to be on the safe side */
	if (!f)
		return;

	f(key, key_len, init_val);
}

/*
 * Test all hash functions.
 */
static void run_hash_func_tests(void)
{
	unsigned i, j, k;

	for (i = 0; i < RTE_DIM(hashtest_funcs); i++) {
		for (j = 0; j < RTE_DIM(hashtest_initvals); j++) {
			for (k = 0; k < RTE_DIM(hashtest_key_lens); k++) {
				run_hash_func_test(hashtest_funcs[i],
						hashtest_initvals[j],
						hashtest_key_lens[k]);
			}
		}
	}
}

/*
 * Basic sequence of operations for a single key:
 *	- add
 *	- lookup (hit)
 *	- delete
 *	- lookup (miss)
 *
 * Repeat the test case when 'free on delete' is disabled.
 *	- add
 *	- lookup (hit)
 *	- delete
 *	- lookup (miss)
 *	- free
 */
static int test_add_delete(void)
{
	struct rte_hash *handle;
	/* test with standard add/lookup/delete functions */
	int pos0, expectedPos0;

	ut_params.name = "test1";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 < 0, "failed to add key (pos0=%d)", pos0);
	expectedPos0 = pos0;

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to delete key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found key after deleting! (pos0=%d)", pos0);

	rte_hash_free(handle);

	/* repeat test with precomputed hash functions */
	hash_sig_t hash_value;
	int pos1, expectedPos1, delPos1;

	ut_params.extra_flag = RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL;
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");
	ut_params.extra_flag = 0;

	hash_value = rte_hash_hash(handle, &keys[0]);
	pos1 = rte_hash_add_key_with_hash(handle, &keys[0], hash_value);
	print_key_info("Add", &keys[0], pos1);
	RETURN_IF_ERROR(pos1 < 0, "failed to add key (pos1=%d)", pos1);
	expectedPos1 = pos1;

	pos1 = rte_hash_lookup_with_hash(handle, &keys[0], hash_value);
	print_key_info("Lkp", &keys[0], pos1);
	RETURN_IF_ERROR(pos1 != expectedPos1,
			"failed to find key (pos1=%d)", pos1);

	pos1 = rte_hash_del_key_with_hash(handle, &keys[0], hash_value);
	print_key_info("Del", &keys[0], pos1);
	RETURN_IF_ERROR(pos1 != expectedPos1,
			"failed to delete key (pos1=%d)", pos1);
	delPos1 = pos1;

	pos1 = rte_hash_lookup_with_hash(handle, &keys[0], hash_value);
	print_key_info("Lkp", &keys[0], pos1);
	RETURN_IF_ERROR(pos1 != -ENOENT,
			"fail: found key after deleting! (pos1=%d)", pos1);

	pos1 = rte_hash_free_key_with_position(handle, delPos1);
	print_key_info("Free", &keys[0], delPos1);
	RETURN_IF_ERROR(pos1 != 0,
			"failed to free key (pos1=%d)", delPos1);

	rte_hash_free(handle);

	return 0;
}

/*
 * Sequence of operations for a single key:
 *	- delete: miss
 *	- add
 *	- lookup: hit
 *	- add: update
 *	- lookup: hit (updated data)
 *	- delete: hit
 *	- delete: miss
 *	- lookup: miss
 */
static int test_add_update_delete(void)
{
	struct rte_hash *handle;
	int pos0, expectedPos0;

	ut_params.name = "test2";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found non-existent key (pos0=%d)", pos0);

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 < 0, "failed to add key (pos0=%d)", pos0);
	expectedPos0 = pos0;

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to re-add key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to delete key (pos0=%d)", pos0);

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: deleted already deleted key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found key after deleting! (pos0=%d)", pos0);

	rte_hash_free(handle);
	return 0;
}

/*
 * Sequence of operations for a single key with 'disable free on del' set:
 *	- delete: miss
 *	- add
 *	- lookup: hit
 *	- add: update
 *	- lookup: hit (updated data)
 *	- delete: hit
 *	- delete: miss
 *	- lookup: miss
 *	- free: hit
 *	- lookup: miss
 */
static int test_add_update_delete_free(void)
{
	struct rte_hash *handle;
	int pos0, expectedPos0, delPos0, result;

	ut_params.name = "test2";
	ut_params.extra_flag = RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL;
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");
	ut_params.extra_flag = 0;

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found non-existent key (pos0=%d)", pos0);

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 < 0, "failed to add key (pos0=%d)", pos0);
	expectedPos0 = pos0;

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to re-add key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	delPos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], delPos0);
	RETURN_IF_ERROR(delPos0 != expectedPos0,
			"failed to delete key (pos0=%d)", delPos0);

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: deleted already deleted key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found key after deleting! (pos0=%d)", pos0);

	result = rte_hash_free_key_with_position(handle, delPos0);
	print_key_info("Free", &keys[0], delPos0);
	RETURN_IF_ERROR(result != 0,
			"failed to free key (pos1=%d)", delPos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found key after deleting! (pos0=%d)", pos0);

	rte_hash_free(handle);
	return 0;
}

/*
 * Sequence of operations for a single key with 'rw concurrency lock free' set:
 *	- add
 *	- delete: hit
 *	- free: hit
 * Repeat the test case when 'multi writer add' is enabled.
 *	- add
 *	- delete: hit
 *	- free: hit
 */
static int test_add_delete_free_lf(void)
{
/* Should match the #define LCORE_CACHE_SIZE value in rte_cuckoo_hash.h */
#define LCORE_CACHE_SIZE	64
	struct rte_hash *handle;
	hash_sig_t hash_value;
	int pos, expectedPos, delPos;
	uint8_t extra_flag;
	uint32_t i, ip_src;

	extra_flag = ut_params.extra_flag;
	ut_params.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF;
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");
	ut_params.extra_flag = extra_flag;

	/*
	 * The number of iterations is at least the same as the number of slots
	 * rte_hash allocates internally. This is to reveal potential issues of
	 * not freeing keys successfully.
	 */
	for (i = 0; i < ut_params.entries + 1; i++) {
		hash_value = rte_hash_hash(handle, &keys[0]);
		pos = rte_hash_add_key_with_hash(handle, &keys[0], hash_value);
		print_key_info("Add", &keys[0], pos);
		RETURN_IF_ERROR(pos < 0, "failed to add key (pos=%d)", pos);
		expectedPos = pos;

		pos = rte_hash_del_key_with_hash(handle, &keys[0], hash_value);
		print_key_info("Del", &keys[0], pos);
		RETURN_IF_ERROR(pos != expectedPos,
				"failed to delete key (pos=%d)", pos);
		delPos = pos;

		pos = rte_hash_free_key_with_position(handle, delPos);
		print_key_info("Free", &keys[0], delPos);
		RETURN_IF_ERROR(pos != 0,
				"failed to free key (pos=%d)", delPos);
	}

	rte_hash_free(handle);

	extra_flag = ut_params.extra_flag;
	ut_params.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF |
				RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");
	ut_params.extra_flag = extra_flag;

	ip_src = keys[0].ip_src;
	/*
	 * The number of iterations is at least the same as the number of slots
	 * rte_hash allocates internally. This is to reveal potential issues of
	 * not freeing keys successfully.
	 */
	for (i = 0; i < ut_params.entries + (RTE_MAX_LCORE - 1) *
					(LCORE_CACHE_SIZE - 1) + 1; i++) {
		keys[0].ip_src++;
		hash_value = rte_hash_hash(handle, &keys[0]);
		pos = rte_hash_add_key_with_hash(handle, &keys[0], hash_value);
		print_key_info("Add", &keys[0], pos);
		RETURN_IF_ERROR(pos < 0, "failed to add key (pos=%d)", pos);
		expectedPos = pos;

		pos = rte_hash_del_key_with_hash(handle, &keys[0], hash_value);
		print_key_info("Del", &keys[0], pos);
		RETURN_IF_ERROR(pos != expectedPos,
			"failed to delete key (pos=%d)", pos);
		delPos = pos;

		pos = rte_hash_free_key_with_position(handle, delPos);
		print_key_info("Free", &keys[0], delPos);
		RETURN_IF_ERROR(pos != 0,
			"failed to free key (pos=%d)", delPos);
	}
	keys[0].ip_src = ip_src;

	rte_hash_free(handle);

	return 0;
}

/*
 * Sequence of operations for retrieving a key with its position
 *
 *  - create table
 *  - add key
 *  - get the key with its position: hit
 *  - delete key
 *  - try to get the deleted key: miss
 *
 * Repeat the test case when 'free on delete' is disabled.
 *  - create table
 *  - add key
 *  - get the key with its position: hit
 *  - delete key
 *  - try to get the deleted key: hit
 *  - free key
 *  - try to get the deleted key: miss
 *
 */
static int test_hash_get_key_with_position(void)
{
	struct rte_hash *handle = NULL;
	int pos, expectedPos, delPos, result;
	void *key;

	ut_params.name = "hash_get_key_w_pos";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	pos = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos);
	RETURN_IF_ERROR(pos < 0, "failed to add key (pos0=%d)", pos);
	expectedPos = pos;

	result = rte_hash_get_key_with_position(handle, pos, &key);
	RETURN_IF_ERROR(result != 0, "error retrieving a key");

	pos = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos);
	RETURN_IF_ERROR(pos != expectedPos,
			"failed to delete key (pos0=%d)", pos);

	result = rte_hash_get_key_with_position(handle, pos, &key);
	RETURN_IF_ERROR(result != -ENOENT, "non valid key retrieved");

	rte_hash_free(handle);

	ut_params.name = "hash_get_key_w_pos";
	ut_params.extra_flag = RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL;
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");
	ut_params.extra_flag = 0;

	pos = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos);
	RETURN_IF_ERROR(pos < 0, "failed to add key (pos0=%d)", pos);
	expectedPos = pos;

	result = rte_hash_get_key_with_position(handle, pos, &key);
	RETURN_IF_ERROR(result != 0, "error retrieving a key");

	delPos = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], delPos);
	RETURN_IF_ERROR(delPos != expectedPos,
			"failed to delete key (pos0=%d)", delPos);

	result = rte_hash_get_key_with_position(handle, delPos, &key);
	RETURN_IF_ERROR(result != -ENOENT, "non valid key retrieved");

	result = rte_hash_free_key_with_position(handle, delPos);
	print_key_info("Free", &keys[0], delPos);
	RETURN_IF_ERROR(result != 0,
			"failed to free key (pos1=%d)", delPos);

	result = rte_hash_get_key_with_position(handle, delPos, &key);
	RETURN_IF_ERROR(result != -ENOENT, "non valid key retrieved");

	rte_hash_free(handle);
	return 0;
}

/*
 * Sequence of operations for find existing hash table
 *
 *  - create table
 *  - find existing table: hit
 *  - find non-existing table: miss
 *
 */
static int test_hash_find_existing(void)
{
	struct rte_hash *handle = NULL, *result = NULL;

	/* Create hash table. */
	ut_params.name = "hash_find_existing";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Try to find existing hash table */
	result = rte_hash_find_existing("hash_find_existing");
	RETURN_IF_ERROR(result != handle, "could not find existing hash table");

	/* Try to find non-existing hash table */
	result = rte_hash_find_existing("hash_find_non_existing");
	RETURN_IF_ERROR(!(result == NULL), "found table that shouldn't exist");

	/* Cleanup. */
	rte_hash_free(handle);

	return 0;
}

/*
 * Sequence of operations for 5 keys
 *	- add keys
 *	- lookup keys: hit
 *	- add keys (update)
 *	- lookup keys: hit (updated data)
 *	- delete keys : hit
 *	- lookup keys: miss
 */
static int test_five_keys(void)
{
	struct rte_hash *handle;
	const void *key_array[5] = {0};
	int pos[5];
	int expected_pos[5];
	unsigned i;
	int ret;

	ut_params.name = "test3";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Add */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] < 0,
				"failed to add key (pos[%u]=%d)", i, pos[i]);
		expected_pos[i] = pos[i];
	}

	/* Lookup */
	for(i = 0; i < 5; i++)
		key_array[i] = &keys[i];

	ret = rte_hash_lookup_bulk(handle, &key_array[0], 5, (int32_t *)pos);
	if(ret == 0)
		for(i = 0; i < 5; i++) {
			print_key_info("Lkp", key_array[i], pos[i]);
			RETURN_IF_ERROR(pos[i] != expected_pos[i],
					"failed to find key (pos[%u]=%d)", i, pos[i]);
		}

	/* Add - update */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
				"failed to add key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
				"failed to find key (pos[%u]=%d)", i, pos[i]);
	}

	/* Delete */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_del_key(handle, &keys[i]);
		print_key_info("Del", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
				"failed to delete key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != -ENOENT,
				"found non-existent key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup multi */
	ret = rte_hash_lookup_bulk(handle, &key_array[0], 5, (int32_t *)pos);
	if (ret == 0)
		for (i = 0; i < 5; i++) {
			print_key_info("Lkp", key_array[i], pos[i]);
			RETURN_IF_ERROR(pos[i] != -ENOENT,
					"found not-existent key (pos[%u]=%d)", i, pos[i]);
		}

	rte_hash_free(handle);

	return 0;
}

/*
 * Add keys to the same bucket until bucket full.
 *	- add 5 keys to the same bucket (hash created with 4 keys per bucket):
 *	  first 4 successful, 5th successful, pushing existing item in bucket
 *	- lookup the 5 keys: 5 hits
 *	- add the 5 keys again: 5 OK
 *	- lookup the 5 keys: 5 hits (updated data)
 *	- delete the 5 keys: 5 OK
 *	- lookup the 5 keys: 5 misses
 */
static int test_full_bucket(void)
{
	struct rte_hash_parameters params_pseudo_hash = {
		.name = "test4",
		.entries = 64,
		.key_len = sizeof(struct flow_key), /* 13 */
		.hash_func = pseudo_hash,
		.hash_func_init_val = 0,
		.socket_id = 0,
	};
	struct rte_hash *handle;
	int pos[5];
	int expected_pos[5];
	unsigned i;

	handle = rte_hash_create(&params_pseudo_hash);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Fill bucket */
	for (i = 0; i < 4; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] < 0,
			"failed to add key (pos[%u]=%d)", i, pos[i]);
		expected_pos[i] = pos[i];
	}
	/*
	 * This should work and will push one of the items
	 * in the bucket because it is full
	 */
	pos[4] = rte_hash_add_key(handle, &keys[4]);
	print_key_info("Add", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] < 0,
			"failed to add key (pos[4]=%d)", pos[4]);
	expected_pos[4] = pos[4];

	/* Lookup */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to find key (pos[%u]=%d)", i, pos[i]);
	}

	/* Add - update */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to add key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to find key (pos[%u]=%d)", i, pos[i]);
	}

	/* Delete 1 key, check other keys are still found */
	pos[1] = rte_hash_del_key(handle, &keys[1]);
	print_key_info("Del", &keys[1], pos[1]);
	RETURN_IF_ERROR(pos[1] != expected_pos[1],
			"failed to delete key (pos[1]=%d)", pos[1]);
	pos[3] = rte_hash_lookup(handle, &keys[3]);
	print_key_info("Lkp", &keys[3], pos[3]);
	RETURN_IF_ERROR(pos[3] != expected_pos[3],
			"failed lookup after deleting key from same bucket "
			"(pos[3]=%d)", pos[3]);

	/* Go back to previous state */
	pos[1] = rte_hash_add_key(handle, &keys[1]);
	print_key_info("Add", &keys[1], pos[1]);
	expected_pos[1] = pos[1];
	RETURN_IF_ERROR(pos[1] < 0, "failed to add key (pos[1]=%d)", pos[1]);

	/* Delete */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_del_key(handle, &keys[i]);
		print_key_info("Del", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to delete key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != -ENOENT,
			"fail: found non-existent key (pos[%u]=%d)", i, pos[i]);
	}

	rte_hash_free(handle);

	/* Cover the NULL case. */
	rte_hash_free(0);
	return 0;
}

/*
 * Similar to the test above (full bucket test), but for extendable buckets.
 */
static int test_extendable_bucket(void)
{
	struct rte_hash_parameters params_pseudo_hash = {
		.name = "test5",
		.entries = 64,
		.key_len = sizeof(struct flow_key), /* 13 */
		.hash_func = pseudo_hash,
		.hash_func_init_val = 0,
		.socket_id = 0,
		.extra_flag = RTE_HASH_EXTRA_FLAGS_EXT_TABLE
	};
	struct rte_hash *handle;
	int pos[64];
	int expected_pos[64];
	unsigned int i;
	struct flow_key rand_keys[64];

	for (i = 0; i < 64; i++) {
		rand_keys[i].port_dst = i;
		rand_keys[i].port_src = i+1;
	}

	handle = rte_hash_create(&params_pseudo_hash);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Fill bucket */
	for (i = 0; i < 64; i++) {
		pos[i] = rte_hash_add_key(handle, &rand_keys[i]);
		print_key_info("Add", &rand_keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] < 0,
			"failed to add key (pos[%u]=%d)", i, pos[i]);
		expected_pos[i] = pos[i];
	}

	/* Lookup */
	for (i = 0; i < 64; i++) {
		pos[i] = rte_hash_lookup(handle, &rand_keys[i]);
		print_key_info("Lkp", &rand_keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to find key (pos[%u]=%d)", i, pos[i]);
	}

	/* Add - update */
	for (i = 0; i < 64; i++) {
		pos[i] = rte_hash_add_key(handle, &rand_keys[i]);
		print_key_info("Add", &rand_keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to add key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 64; i++) {
		pos[i] = rte_hash_lookup(handle, &rand_keys[i]);
		print_key_info("Lkp", &rand_keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to find key (pos[%u]=%d)", i, pos[i]);
	}

	/* Delete 1 key, check other keys are still found */
	pos[35] = rte_hash_del_key(handle, &rand_keys[35]);
	print_key_info("Del", &rand_keys[35], pos[35]);
	RETURN_IF_ERROR(pos[35] != expected_pos[35],
			"failed to delete key (pos[1]=%d)", pos[35]);
	pos[20] = rte_hash_lookup(handle, &rand_keys[20]);
	print_key_info("Lkp", &rand_keys[20], pos[20]);
	RETURN_IF_ERROR(pos[20] != expected_pos[20],
			"failed lookup after deleting key from same bucket "
			"(pos[20]=%d)", pos[20]);

	/* Go back to previous state */
	pos[35] = rte_hash_add_key(handle, &rand_keys[35]);
	print_key_info("Add", &rand_keys[35], pos[35]);
	expected_pos[35] = pos[35];
	RETURN_IF_ERROR(pos[35] < 0, "failed to add key (pos[1]=%d)", pos[35]);

	/* Delete */
	for (i = 0; i < 64; i++) {
		pos[i] = rte_hash_del_key(handle, &rand_keys[i]);
		print_key_info("Del", &rand_keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to delete key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 64; i++) {
		pos[i] = rte_hash_lookup(handle, &rand_keys[i]);
		print_key_info("Lkp", &rand_keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != -ENOENT,
			"fail: found non-existent key (pos[%u]=%d)", i, pos[i]);
	}

	/* Add again */
	for (i = 0; i < 64; i++) {
		pos[i] = rte_hash_add_key(handle, &rand_keys[i]);
		print_key_info("Add", &rand_keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] < 0,
			"failed to add key (pos[%u]=%d)", i, pos[i]);
		expected_pos[i] = pos[i];
	}

	rte_hash_free(handle);

	/* Cover the NULL case. */
	rte_hash_free(0);
	return 0;
}

/******************************************************************************/
static int
fbk_hash_unit_test(void)
{
	struct rte_fbk_hash_params params = {
		.name = "fbk_hash_test",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_1 = {
		.name = "invalid_1",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX + 1, /* Not power of 2 */
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_2 = {
		.name = "invalid_2",
		.entries = 4,
		.entries_per_bucket = 3,         /* Not power of 2 */
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_3 = {
		.name = "invalid_3",
		.entries = 0,                    /* Entries is 0 */
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_4 = {
		.name = "invalid_4",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 0,         /* Entries per bucket is 0 */
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_5 = {
		.name = "invalid_5",
		.entries = 4,
		.entries_per_bucket = 8,         /* Entries per bucket > entries */
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_6 = {
		.name = "invalid_6",
		.entries = RTE_FBK_HASH_ENTRIES_MAX * 2,   /* Entries > max allowed */
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_7 = {
		.name = "invalid_7",
		.entries = RTE_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = RTE_FBK_HASH_ENTRIES_PER_BUCKET_MAX * 2,	/* Entries > max allowed */
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_8 = {
		.name = "invalid_7",
		.entries = RTE_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = RTE_MAX_NUMA_NODES + 1, /* invalid socket */
	};

	/* try to create two hashes with identical names
	 * in this case, trying to create a second one will not
	 * fail but will simply return pointer to the existing
	 * hash with that name. sort of like a "find hash by name" :-)
	 */
	struct rte_fbk_hash_params invalid_params_same_name_1 = {
		.name = "same_name",				/* hash with identical name */
		.entries = 4,
		.entries_per_bucket = 2,
		.socket_id = 0,
	};

	/* trying to create this hash should return a pointer to an existing hash */
	struct rte_fbk_hash_params invalid_params_same_name_2 = {
		.name = "same_name",				/* hash with identical name */
		.entries = RTE_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	/* this is a sanity check for "same name" test
	 * creating this hash will check if we are actually able to create
	 * multiple hashes with different names (instead of having just one).
	 */
	struct rte_fbk_hash_params different_name = {
		.name = "different_name",			/* different name */
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params params_jhash = {
		.name = "valid",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
		.hash_func = rte_jhash_1word,              /* Tests for different hash_func */
		.init_val = RTE_FBK_HASH_INIT_VAL_DEFAULT,
	};

	struct rte_fbk_hash_params params_nohash = {
		.name = "valid nohash",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
		.hash_func = NULL,                            /* Tests for null hash_func */
		.init_val = RTE_FBK_HASH_INIT_VAL_DEFAULT,
	};

	struct rte_fbk_hash_table *handle, *tmp;
	uint32_t keys[5] =
		{0xc6e18639, 0xe67c201c, 0xd4c8cffd, 0x44728691, 0xd5430fa9};
	uint16_t vals[5] = {28108, 5699, 38490, 2166, 61571};
	int status;
	unsigned i;
	double used_entries;

	/* Try creating hashes with invalid parameters */
	printf("# Testing hash creation with invalid parameters "
			"- expect error msgs\n");
	handle = rte_fbk_hash_create(&invalid_params_1);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_2);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_3);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_4);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_5);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_6);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_7);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	if (rte_eal_has_hugepages()) {
		handle = rte_fbk_hash_create(&invalid_params_8);
		RETURN_IF_ERROR_FBK(handle != NULL,
					"fbk hash creation should have failed");
	}

	handle = rte_fbk_hash_create(&invalid_params_same_name_1);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk hash creation should have succeeded");

	tmp = rte_fbk_hash_create(&invalid_params_same_name_2);
	if (tmp != NULL)
		rte_fbk_hash_free(tmp);
	RETURN_IF_ERROR_FBK(tmp != NULL, "fbk hash creation should have failed");

	/* we are not freeing  handle here because we need a hash list
	 * to be not empty for the next test */

	/* create a hash in non-empty list - good for coverage */
	tmp = rte_fbk_hash_create(&different_name);
	RETURN_IF_ERROR_FBK(tmp == NULL, "fbk hash creation should have succeeded");

	/* free both hashes */
	rte_fbk_hash_free(handle);
	rte_fbk_hash_free(tmp);

	/* Create empty jhash hash. */
	handle = rte_fbk_hash_create(&params_jhash);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk jhash hash creation failed");

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	/* Create empty jhash hash. */
	handle = rte_fbk_hash_create(&params_nohash);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk nohash hash creation failed");

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	/* Create empty hash. */
	handle = rte_fbk_hash_create(&params);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk hash creation failed");

	used_entries = rte_fbk_hash_get_load_factor(handle) * LOCAL_FBK_HASH_ENTRIES_MAX;
	RETURN_IF_ERROR_FBK((unsigned)used_entries != 0, \
				"load factor right after creation is not zero but it should be");
	/* Add keys. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_add_key(handle, keys[i], vals[i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash add failed");
	}

	used_entries = rte_fbk_hash_get_load_factor(handle) * LOCAL_FBK_HASH_ENTRIES_MAX;
	RETURN_IF_ERROR_FBK((unsigned)used_entries != (unsigned)((((double)5)/LOCAL_FBK_HASH_ENTRIES_MAX)*LOCAL_FBK_HASH_ENTRIES_MAX), \
				"load factor now is not as expected");
	/* Find value of added keys. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != vals[i],
				"fbk hash lookup failed");
	}

	/* Change value of added keys. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_add_key(handle, keys[i], vals[4 - i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash update failed");
	}

	/* Find new values. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != vals[4-i],
				"fbk hash lookup failed");
	}

	/* Delete keys individually. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_delete_key(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash delete failed");
	}

	used_entries = rte_fbk_hash_get_load_factor(handle) * LOCAL_FBK_HASH_ENTRIES_MAX;
	RETURN_IF_ERROR_FBK((unsigned)used_entries != 0, \
				"load factor right after deletion is not zero but it should be");
	/* Lookup should now fail. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status == 0,
				"fbk hash lookup should have failed");
	}

	/* Add keys again. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_add_key(handle, keys[i], vals[i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash add failed");
	}

	/* Make sure they were added. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != vals[i],
				"fbk hash lookup failed");
	}

	/* Clear all entries. */
	rte_fbk_hash_clear_all(handle);

	/* Lookup should fail. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status == 0,
				"fbk hash lookup should have failed");
	}

	/* coverage */

	/* fill up the hash_table */
	for (i = 0; i < RTE_FBK_HASH_ENTRIES_MAX + 1; i++)
		rte_fbk_hash_add_key(handle, i, (uint16_t) i);

	/* Find non-existent key in a full hashtable */
	status = rte_fbk_hash_lookup(handle, RTE_FBK_HASH_ENTRIES_MAX + 1);
	RETURN_IF_ERROR_FBK(status != -ENOENT,
			"fbk hash lookup succeeded");

	/* Delete non-existent key in a full hashtable */
	status = rte_fbk_hash_delete_key(handle, RTE_FBK_HASH_ENTRIES_MAX + 1);
	RETURN_IF_ERROR_FBK(status != -ENOENT,
			"fbk hash delete succeeded");

	/* Delete one key from a full hashtable */
	status = rte_fbk_hash_delete_key(handle, 1);
	RETURN_IF_ERROR_FBK(status != 0,
			"fbk hash delete failed");

	/* Clear all entries. */
	rte_fbk_hash_clear_all(handle);

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	/* Cover the NULL case. */
	rte_fbk_hash_free(0);

	return 0;
}

/*
 * Sequence of operations for find existing fbk hash table
 *
 *  - create table
 *  - find existing table: hit
 *  - find non-existing table: miss
 *
 */
static int test_fbk_hash_find_existing(void)
{
	struct rte_fbk_hash_params params = {
			.name = "fbk_hash_find_existing",
			.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
			.entries_per_bucket = 4,
			.socket_id = 0,
	};
	struct rte_fbk_hash_table *handle = NULL, *result = NULL;

	/* Create hash table. */
	handle = rte_fbk_hash_create(&params);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk hash creation failed");

	/* Try to find existing fbk hash table */
	result = rte_fbk_hash_find_existing("fbk_hash_find_existing");
	RETURN_IF_ERROR_FBK(result != handle, "could not find existing fbk hash table");

	/* Try to find non-existing fbk hash table */
	result = rte_fbk_hash_find_existing("fbk_hash_find_non_existing");
	RETURN_IF_ERROR_FBK(!(result == NULL), "found fbk table that shouldn't exist");

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	return 0;
}

#define BUCKET_ENTRIES 4
/*
 * Do tests for hash creation with bad parameters.
 */
static int test_hash_creation_with_bad_parameters(void)
{
	struct rte_hash *handle, *tmp;
	struct rte_hash_parameters params;

	handle = rte_hash_create(NULL);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash successfully without any parameter\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_0";
	params.entries = RTE_HASH_ENTRIES_MAX + 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash successfully with entries in parameter exceeded\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_2";
	params.entries = BUCKET_ENTRIES - 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash successfully if entries less than bucket_entries in parameter\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_3";
	params.key_len = 0;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash successfully if key_len in parameter is zero\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_4";
	params.socket_id = RTE_MAX_NUMA_NODES + 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash successfully with invalid socket\n");
		return -1;
	}

	/* test with same name should fail */
	memcpy(&params, &ut_params, sizeof(params));
	params.name = "same_name";
	handle = rte_hash_create(&params);
	if (handle == NULL) {
		printf("Cannot create first hash table with 'same_name'\n");
		return -1;
	}
	tmp = rte_hash_create(&params);
	if (tmp != NULL) {
		printf("Creation of hash table with same name should fail\n");
		rte_hash_free(handle);
		rte_hash_free(tmp);
		return -1;
	}
	rte_hash_free(handle);

	printf("# Test successful. No more errors expected\n");

	return 0;
}

/*
 * Do tests for hash creation with parameters that look incorrect
 * but are actually valid.
 */
static int
test_hash_creation_with_good_parameters(void)
{
	struct rte_hash *handle;
	struct rte_hash_parameters params;

	/* create with null hash function - should choose DEFAULT_HASH_FUNC */
	memcpy(&params, &ut_params, sizeof(params));
	params.name = "name";
	params.hash_func = NULL;
	handle = rte_hash_create(&params);
	if (handle == NULL) {
		printf("Creating hash with null hash_func failed\n");
		return -1;
	}

	rte_hash_free(handle);

	return 0;
}

#define ITERATIONS 3
/*
 * Test to see the average table utilization (entries added/max entries)
 * before hitting a random entry that cannot be added
 */
static int test_average_table_utilization(uint32_t ext_table)
{
	struct rte_hash *handle;
	uint8_t simple_key[MAX_KEYSIZE];
	unsigned i, j;
	unsigned added_keys, average_keys_added = 0;
	int ret;
	unsigned int cnt;

	printf("\n# Running test to determine average utilization"
	       "\n  before adding elements begins to fail\n");
	if (ext_table)
		printf("ext table is enabled\n");
	else
		printf("ext table is disabled\n");

	printf("Measuring performance, please wait");
	fflush(stdout);
	ut_params.entries = 1 << 16;
	ut_params.name = "test_average_utilization";
	ut_params.hash_func = rte_jhash;
	if (ext_table)
		ut_params.extra_flag |= RTE_HASH_EXTRA_FLAGS_EXT_TABLE;
	else
		ut_params.extra_flag &= ~RTE_HASH_EXTRA_FLAGS_EXT_TABLE;

	handle = rte_hash_create(&ut_params);

	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	for (j = 0; j < ITERATIONS; j++) {
		ret = 0;
		/* Add random entries until key cannot be added */
		for (added_keys = 0; ret >= 0; added_keys++) {
			for (i = 0; i < ut_params.key_len; i++)
				simple_key[i] = rte_rand() % 255;
			ret = rte_hash_add_key(handle, simple_key);
			if (ret < 0)
				break;
		}

		if (ret != -ENOSPC) {
			printf("Unexpected error when adding keys\n");
			rte_hash_free(handle);
			return -1;
		}

		cnt = rte_hash_count(handle);
		if (cnt != added_keys) {
			printf("rte_hash_count returned wrong value %u, %u,"
					"%u\n", j, added_keys, cnt);
			rte_hash_free(handle);
			return -1;
		}
		if (ext_table) {
			if (cnt != ut_params.entries) {
				printf("rte_hash_count returned wrong value "
					"%u, %u, %u\n", j, added_keys, cnt);
				rte_hash_free(handle);
				return -1;
			}
		}

		average_keys_added += added_keys;

		/* Reset the table */
		rte_hash_reset(handle);

		/* Print a dot to show progress on operations */
		printf(".");
		fflush(stdout);
	}

	average_keys_added /= ITERATIONS;

	printf("\nAverage table utilization = %.2f%% (%u/%u)\n",
		((double) average_keys_added / ut_params.entries * 100),
		average_keys_added, ut_params.entries);
	rte_hash_free(handle);

	return 0;
}

#define NUM_ENTRIES 256
static int test_hash_iteration(uint32_t ext_table)
{
	struct rte_hash *handle;
	unsigned i;
	uint8_t keys[NUM_ENTRIES][MAX_KEYSIZE];
	const void *next_key;
	void *next_data;
	void *data[NUM_ENTRIES];
	unsigned added_keys;
	uint32_t iter = 0;
	int ret = 0;

	ut_params.entries = NUM_ENTRIES;
	ut_params.name = "test_hash_iteration";
	ut_params.hash_func = rte_jhash;
	ut_params.key_len = 16;
	if (ext_table)
		ut_params.extra_flag |= RTE_HASH_EXTRA_FLAGS_EXT_TABLE;
	else
		ut_params.extra_flag &= ~RTE_HASH_EXTRA_FLAGS_EXT_TABLE;

	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Add random entries until key cannot be added */
	for (added_keys = 0; added_keys < NUM_ENTRIES; added_keys++) {
		data[added_keys] = (void *) ((uintptr_t) rte_rand());
		for (i = 0; i < ut_params.key_len; i++)
			keys[added_keys][i] = rte_rand() % 255;
		ret = rte_hash_add_key_data(handle, keys[added_keys], data[added_keys]);
		if (ret < 0) {
			if (ext_table) {
				printf("Insertion failed for ext table\n");
				goto err;
			}
			break;
		}
	}

	/* Iterate through the hash table */
	while (rte_hash_iterate(handle, &next_key, &next_data, &iter) >= 0) {
		/* Search for the key in the list of keys added */
		for (i = 0; i < NUM_ENTRIES; i++) {
			if (memcmp(next_key, keys[i], ut_params.key_len) == 0) {
				if (next_data != data[i]) {
					printf("Data found in the hash table is"
					       "not the data added with the key\n");
					goto err;
				}
				added_keys--;
				break;
			}
		}
		if (i == NUM_ENTRIES) {
			printf("Key found in the hash table was not added\n");
			goto err;
		}
	}

	/* Check if all keys have been iterated */
	if (added_keys != 0) {
		printf("There were still %u keys to iterate\n", added_keys);
		goto err;
	}

	rte_hash_free(handle);
	return 0;

err:
	rte_hash_free(handle);
	return -1;
}

static uint8_t key[16] = {0x00, 0x01, 0x02, 0x03,
			0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b,
			0x0c, 0x0d, 0x0e, 0x0f};
static struct rte_hash_parameters hash_params_ex = {
	.name = NULL,
	.entries = 64,
	.key_len = 0,
	.hash_func = NULL,
	.hash_func_init_val = 0,
	.socket_id = 0,
};

/*
 * add/delete key with jhash2
 */
static int
test_hash_add_delete_jhash2(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash2";
	hash_params_ex.key_len = 4;
	hash_params_ex.hash_func = (rte_hash_function)rte_jhash_32b;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL) {
		printf("test_hash_add_delete_jhash2 fail to create hash\n");
		goto fail_jhash2;
	}
	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0) {
		printf("test_hash_add_delete_jhash2 fail to add hash key\n");
		goto fail_jhash2;
	}

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2) {
		printf("test_hash_add_delete_jhash2 delete different key from being added\n");
		goto fail_jhash2;
	}
	ret = 0;

fail_jhash2:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

/*
 * add/delete (2) key with jhash2
 */
static int
test_hash_add_delete_2_jhash2(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_2_jhash2";
	hash_params_ex.key_len = 8;
	hash_params_ex.hash_func = (rte_hash_function)rte_jhash_32b;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_2_jhash2;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_2_jhash2;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_2_jhash2;

	ret = 0;

fail_2_jhash2:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

static uint32_t
test_hash_jhash_1word(const void *key, uint32_t length, uint32_t initval)
{
	const uint32_t *k = key;

	RTE_SET_USED(length);

	return rte_jhash_1word(k[0], initval);
}

static uint32_t
test_hash_jhash_2word(const void *key, uint32_t length, uint32_t initval)
{
	const uint32_t *k = key;

	RTE_SET_USED(length);

	return rte_jhash_2words(k[0], k[1], initval);
}

static uint32_t
test_hash_jhash_3word(const void *key, uint32_t length, uint32_t initval)
{
	const uint32_t *k = key;

	RTE_SET_USED(length);

	return rte_jhash_3words(k[0], k[1], k[2], initval);
}

/*
 * add/delete key with jhash 1word
 */
static int
test_hash_add_delete_jhash_1word(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash_1word";
	hash_params_ex.key_len = 4;
	hash_params_ex.hash_func = test_hash_jhash_1word;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_jhash_1word;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_jhash_1word;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_jhash_1word;

	ret = 0;

fail_jhash_1word:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

/*
 * add/delete key with jhash 2word
 */
static int
test_hash_add_delete_jhash_2word(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash_2word";
	hash_params_ex.key_len = 8;
	hash_params_ex.hash_func = test_hash_jhash_2word;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_jhash_2word;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_jhash_2word;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_jhash_2word;

	ret = 0;

fail_jhash_2word:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

/*
 * add/delete key with jhash 3word
 */
static int
test_hash_add_delete_jhash_3word(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash_3word";
	hash_params_ex.key_len = 12;
	hash_params_ex.hash_func = test_hash_jhash_3word;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_jhash_3word;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_jhash_3word;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_jhash_3word;

	ret = 0;

fail_jhash_3word:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

static struct rte_hash *g_handle;
static struct rte_rcu_qsbr *g_qsv;
static volatile uint8_t writer_done;
struct flow_key g_rand_keys[9];

/*
 * rte_hash_rcu_qsbr_add positive and negative tests.
 *  - Add RCU QSBR variable to Hash
 *  - Add another RCU QSBR variable to Hash
 *  - Check returns
 */
static int
test_hash_rcu_qsbr_add(void)
{
	size_t sz;
	struct rte_rcu_qsbr *qsv2 = NULL;
	int32_t status;
	struct rte_hash_rcu_config rcu_cfg = {0};
	struct rte_hash_parameters params;

	printf("\n# Running RCU QSBR add tests\n");
	memcpy(&params, &ut_params, sizeof(params));
	params.name = "test_hash_rcu_qsbr_add";
	params.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF |
				RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;
	g_handle = rte_hash_create(&params);
	RETURN_IF_ERROR_RCU_QSBR(g_handle == NULL, "Hash creation failed");

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
	g_qsv = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
					RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	RETURN_IF_ERROR_RCU_QSBR(g_qsv == NULL,
				 "RCU QSBR variable creation failed");

	status = rte_rcu_qsbr_init(g_qsv, RTE_MAX_LCORE);
	RETURN_IF_ERROR_RCU_QSBR(status != 0,
				 "RCU QSBR variable initialization failed");

	rcu_cfg.v = g_qsv;
	/* Invalid QSBR mode */
	rcu_cfg.mode = 0xff;
	status = rte_hash_rcu_qsbr_add(g_handle, &rcu_cfg);
	RETURN_IF_ERROR_RCU_QSBR(status == 0, "Invalid QSBR mode test failed");

	rcu_cfg.mode = RTE_HASH_QSBR_MODE_DQ;
	/* Attach RCU QSBR to hash table */
	status = rte_hash_rcu_qsbr_add(g_handle, &rcu_cfg);
	RETURN_IF_ERROR_RCU_QSBR(status != 0,
				 "Attach RCU QSBR to hash table failed");

	/* Create and attach another RCU QSBR to hash table */
	qsv2 = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
					RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	RETURN_IF_ERROR_RCU_QSBR(qsv2 == NULL,
				 "RCU QSBR variable creation failed");

	rcu_cfg.v = qsv2;
	rcu_cfg.mode = RTE_HASH_QSBR_MODE_SYNC;
	status = rte_hash_rcu_qsbr_add(g_handle, &rcu_cfg);
	rte_free(qsv2);
	RETURN_IF_ERROR_RCU_QSBR(status == 0,
			"Attach RCU QSBR to hash table succeeded where failure"
			" is expected");

	rte_hash_free(g_handle);
	rte_free(g_qsv);

	return 0;
}

/*
 * rte_hash_rcu_qsbr_add DQ mode functional test.
 * Reader and writer are in the same thread in this test.
 *  - Create hash which supports maximum 8 (9 if ext bkt is enabled) entries
 *  - Add RCU QSBR variable to hash
 *  - Add 8 hash entries and fill the bucket
 *  - If ext bkt is enabled, add 1 extra entry that is available in the ext bkt
 *  - Register a reader thread (not a real thread)
 *  - Reader lookup existing entry
 *  - Writer deletes the entry
 *  - Reader lookup the entry
 *  - Writer re-add the entry (no available free index)
 *  - Reader report quiescent state and unregister
 *  - Writer re-add the entry
 *  - Reader lookup the entry
 */
static int
test_hash_rcu_qsbr_dq_mode(uint8_t ext_bkt)
{
	uint32_t total_entries = (ext_bkt == 0) ? 8 : 9;

	uint8_t hash_extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF;

	if (ext_bkt)
		hash_extra_flag |= RTE_HASH_EXTRA_FLAGS_EXT_TABLE;

	struct rte_hash_parameters params_pseudo_hash = {
		.name = "test_hash_rcu_qsbr_dq_mode",
		.entries = total_entries,
		.key_len = sizeof(struct flow_key), /* 13 */
		.hash_func = pseudo_hash,
		.hash_func_init_val = 0,
		.socket_id = 0,
		.extra_flag = hash_extra_flag,
	};
	int pos[total_entries];
	int expected_pos[total_entries];
	unsigned int i;
	size_t sz;
	int32_t status;
	struct rte_hash_rcu_config rcu_cfg = {0};

	g_qsv = NULL;
	g_handle = NULL;

	for (i = 0; i < total_entries; i++) {
		g_rand_keys[i].port_dst = i;
		g_rand_keys[i].port_src = i+1;
	}

	if (ext_bkt)
		printf("\n# Running RCU QSBR DQ mode functional test with"
		       " ext bkt\n");
	else
		printf("\n# Running RCU QSBR DQ mode functional test\n");

	g_handle = rte_hash_create(&params_pseudo_hash);
	RETURN_IF_ERROR_RCU_QSBR(g_handle == NULL, "Hash creation failed");

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
	g_qsv = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
					RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	RETURN_IF_ERROR_RCU_QSBR(g_qsv == NULL,
				 "RCU QSBR variable creation failed");

	status = rte_rcu_qsbr_init(g_qsv, RTE_MAX_LCORE);
	RETURN_IF_ERROR_RCU_QSBR(status != 0,
				 "RCU QSBR variable initialization failed");

	rcu_cfg.v = g_qsv;
	rcu_cfg.mode = RTE_HASH_QSBR_MODE_DQ;
	/* Attach RCU QSBR to hash table */
	status = rte_hash_rcu_qsbr_add(g_handle, &rcu_cfg);
	RETURN_IF_ERROR_RCU_QSBR(status != 0,
				 "Attach RCU QSBR to hash table failed");

	/* Fill bucket */
	for (i = 0; i < total_entries; i++) {
		pos[i] = rte_hash_add_key(g_handle, &g_rand_keys[i]);
		print_key_info("Add", &g_rand_keys[i], pos[i]);
		RETURN_IF_ERROR_RCU_QSBR(pos[i] < 0,
					 "failed to add key (pos[%u]=%d)", i,
					 pos[i]);
		expected_pos[i] = pos[i];
	}

	/* Register pseudo reader */
	status = rte_rcu_qsbr_thread_register(g_qsv, 0);
	RETURN_IF_ERROR_RCU_QSBR(status != 0,
				 "RCU QSBR thread registration failed");
	rte_rcu_qsbr_thread_online(g_qsv, 0);

	/* Lookup */
	pos[0] = rte_hash_lookup(g_handle, &g_rand_keys[0]);
	print_key_info("Lkp", &g_rand_keys[0], pos[0]);
	RETURN_IF_ERROR_RCU_QSBR(pos[0] != expected_pos[0],
				 "failed to find correct key (pos[%u]=%d)", 0,
				 pos[0]);

	/* Writer update */
	pos[0] = rte_hash_del_key(g_handle, &g_rand_keys[0]);
	print_key_info("Del", &g_rand_keys[0], pos[0]);
	RETURN_IF_ERROR_RCU_QSBR(pos[0] != expected_pos[0],
				 "failed to del correct key (pos[%u]=%d)", 0,
				 pos[0]);

	/* Lookup */
	pos[0] = rte_hash_lookup(g_handle, &g_rand_keys[0]);
	print_key_info("Lkp", &g_rand_keys[0], pos[0]);
	RETURN_IF_ERROR_RCU_QSBR(pos[0] != -ENOENT,
				 "found deleted key (pos[%u]=%d)", 0, pos[0]);

	/* Fill bucket */
	pos[0] = rte_hash_add_key(g_handle, &g_rand_keys[0]);
	print_key_info("Add", &g_rand_keys[0], pos[0]);
	RETURN_IF_ERROR_RCU_QSBR(pos[0] != -ENOSPC,
				 "Added key successfully (pos[%u]=%d)", 0, pos[0]);

	/* Reader quiescent */
	rte_rcu_qsbr_quiescent(g_qsv, 0);

	/* Fill bucket */
	pos[0] = rte_hash_add_key(g_handle, &g_rand_keys[0]);
	print_key_info("Add", &g_rand_keys[0], pos[0]);
	RETURN_IF_ERROR_RCU_QSBR(pos[0] < 0,
				 "failed to add key (pos[%u]=%d)", 0, pos[0]);
	expected_pos[0] = pos[0];

	rte_rcu_qsbr_thread_offline(g_qsv, 0);
	(void)rte_rcu_qsbr_thread_unregister(g_qsv, 0);

	/* Lookup */
	pos[0] = rte_hash_lookup(g_handle, &g_rand_keys[0]);
	print_key_info("Lkp", &g_rand_keys[0], pos[0]);
	RETURN_IF_ERROR_RCU_QSBR(pos[0] != expected_pos[0],
				 "failed to find correct key (pos[%u]=%d)", 0,
				 pos[0]);

	rte_hash_free(g_handle);
	rte_free(g_qsv);
	return 0;

}

/* Report quiescent state interval every 1024 lookups. Larger critical
 * sections in reader will result in writer polling multiple times.
 */
#define QSBR_REPORTING_INTERVAL 1024
#define WRITER_ITERATIONS	512

/*
 * Reader thread using rte_hash data structure with RCU.
 */
static int
test_hash_rcu_qsbr_reader(void *arg)
{
	int i;

	RTE_SET_USED(arg);
	/* Register this thread to report quiescent state */
	(void)rte_rcu_qsbr_thread_register(g_qsv, 0);
	rte_rcu_qsbr_thread_online(g_qsv, 0);

	do {
		for (i = 0; i < QSBR_REPORTING_INTERVAL; i++)
			rte_hash_lookup(g_handle, &g_rand_keys[0]);

		/* Update quiescent state */
		rte_rcu_qsbr_quiescent(g_qsv, 0);
	} while (!writer_done);

	rte_rcu_qsbr_thread_offline(g_qsv, 0);
	(void)rte_rcu_qsbr_thread_unregister(g_qsv, 0);

	return 0;
}

/*
 * rte_hash_rcu_qsbr_add sync mode functional test.
 * 1 Reader and 1 writer. They cannot be in the same thread in this test.
 *  - Create hash which supports maximum 8 (9 if ext bkt is enabled) entries
 *  - Add RCU QSBR variable to hash
 *  - Register a reader thread. Reader keeps looking up a specific key.
 *  - Writer keeps adding and deleting a specific key.
 */
static int
test_hash_rcu_qsbr_sync_mode(uint8_t ext_bkt)
{
	uint32_t total_entries = (ext_bkt == 0) ? 8 : 9;

	uint8_t hash_extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF;

	if (ext_bkt)
		hash_extra_flag |= RTE_HASH_EXTRA_FLAGS_EXT_TABLE;

	struct rte_hash_parameters params_pseudo_hash = {
		.name = "test_hash_rcu_qsbr_sync_mode",
		.entries = total_entries,
		.key_len = sizeof(struct flow_key), /* 13 */
		.hash_func = pseudo_hash,
		.hash_func_init_val = 0,
		.socket_id = 0,
		.extra_flag = hash_extra_flag,
	};
	int pos[total_entries];
	int expected_pos[total_entries];
	unsigned int i;
	size_t sz;
	int32_t status;
	struct rte_hash_rcu_config rcu_cfg = {0};

	g_qsv = NULL;
	g_handle = NULL;

	for (i = 0; i < total_entries; i++) {
		g_rand_keys[i].port_dst = i;
		g_rand_keys[i].port_src = i+1;
	}

	if (ext_bkt)
		printf("\n# Running RCU QSBR sync mode functional test with"
		       " ext bkt\n");
	else
		printf("\n# Running RCU QSBR sync mode functional test\n");

	g_handle = rte_hash_create(&params_pseudo_hash);
	RETURN_IF_ERROR_RCU_QSBR(g_handle == NULL, "Hash creation failed");

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
	g_qsv = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
					RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	RETURN_IF_ERROR_RCU_QSBR(g_qsv == NULL,
				 "RCU QSBR variable creation failed");

	status = rte_rcu_qsbr_init(g_qsv, RTE_MAX_LCORE);
	RETURN_IF_ERROR_RCU_QSBR(status != 0,
				 "RCU QSBR variable initialization failed");

	rcu_cfg.v = g_qsv;
	rcu_cfg.mode = RTE_HASH_QSBR_MODE_SYNC;
	/* Attach RCU QSBR to hash table */
	status = rte_hash_rcu_qsbr_add(g_handle, &rcu_cfg);
	RETURN_IF_ERROR_RCU_QSBR(status != 0,
				 "Attach RCU QSBR to hash table failed");

	/* Launch reader thread */
	rte_eal_remote_launch(test_hash_rcu_qsbr_reader, NULL,
				rte_get_next_lcore(-1, 1, 0));

	/* Fill bucket */
	for (i = 0; i < total_entries; i++) {
		pos[i] = rte_hash_add_key(g_handle, &g_rand_keys[i]);
		print_key_info("Add", &g_rand_keys[i], pos[i]);
		RETURN_IF_ERROR_RCU_QSBR(pos[i] < 0,
				"failed to add key (pos[%u]=%d)", i, pos[i]);
		expected_pos[i] = pos[i];
	}
	writer_done = 0;

	/* Writer Update */
	for (i = 0; i < WRITER_ITERATIONS; i++) {
		expected_pos[0] = pos[0];
		pos[0] = rte_hash_del_key(g_handle, &g_rand_keys[0]);
		print_key_info("Del", &g_rand_keys[0], status);
		RETURN_IF_ERROR_RCU_QSBR(pos[0] != expected_pos[0],
					 "failed to del correct key (pos[%u]=%d)"
					 , 0, pos[0]);

		pos[0] = rte_hash_add_key(g_handle, &g_rand_keys[0]);
		print_key_info("Add", &g_rand_keys[0], pos[0]);
		RETURN_IF_ERROR_RCU_QSBR(pos[0] < 0,
					 "failed to add key (pos[%u]=%d)", 0,
					 pos[0]);
	}

	writer_done = 1;
	/* Wait until reader exited. */
	rte_eal_mp_wait_lcore();

	rte_hash_free(g_handle);
	rte_free(g_qsv);

	return  0;

}

/*
 * Do all unit and performance tests.
 */
static int
test_hash(void)
{
	if (test_add_delete() < 0)
		return -1;
	if (test_hash_add_delete_jhash2() < 0)
		return -1;
	if (test_hash_add_delete_2_jhash2() < 0)
		return -1;
	if (test_hash_add_delete_jhash_1word() < 0)
		return -1;
	if (test_hash_add_delete_jhash_2word() < 0)
		return -1;
	if (test_hash_add_delete_jhash_3word() < 0)
		return -1;
	if (test_hash_get_key_with_position() < 0)
		return -1;
	if (test_hash_find_existing() < 0)
		return -1;
	if (test_add_update_delete() < 0)
		return -1;
	if (test_add_update_delete_free() < 0)
		return -1;
	if (test_add_delete_free_lf() < 0)
		return -1;
	if (test_five_keys() < 0)
		return -1;
	if (test_full_bucket() < 0)
		return -1;
	if (test_extendable_bucket() < 0)
		return -1;

	if (test_fbk_hash_find_existing() < 0)
		return -1;
	if (fbk_hash_unit_test() < 0)
		return -1;
	if (test_hash_creation_with_bad_parameters() < 0)
		return -1;
	if (test_hash_creation_with_good_parameters() < 0)
		return -1;

	/* ext table disabled */
	if (test_average_table_utilization(0) < 0)
		return -1;
	if (test_hash_iteration(0) < 0)
		return -1;

	/* ext table enabled */
	if (test_average_table_utilization(1) < 0)
		return -1;
	if (test_hash_iteration(1) < 0)
		return -1;

	run_hash_func_tests();

	if (test_crc32_hash_alg_equiv() < 0)
		return -1;

	if (test_hash_rcu_qsbr_add() < 0)
		return -1;

	if (test_hash_rcu_qsbr_dq_mode(0) < 0)
		return -1;

	if (test_hash_rcu_qsbr_dq_mode(1) < 0)
		return -1;

	if (test_hash_rcu_qsbr_sync_mode(0) < 0)
		return -1;

	if (test_hash_rcu_qsbr_sync_mode(1) < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(hash_autotest, test_hash);
