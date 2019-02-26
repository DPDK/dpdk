/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_efd.h>
#include <rte_byteorder.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ip.h>

#include "test.h"

#define EFD_TEST_KEY_LEN 8
#define TABLE_SIZE (1 << 21)
#define ITERATIONS 3

#if RTE_EFD_VALUE_NUM_BITS == 32
#define VALUE_BITMASK 0xffffffff
#else
#define VALUE_BITMASK ((1 << RTE_EFD_VALUE_NUM_BITS) - 1)
#endif
static unsigned int test_socket_id;

/* 5-tuple key type */
struct flow_key {
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint8_t proto;
} __attribute__((packed));

int efd_logtype_test;

RTE_INIT(test_efd_init_log)
{
	efd_logtype_test = rte_log_register("test.efd");
}

/*
 * Print out result of unit test efd operation.
 */
static void print_key_info(const char *msg, const struct flow_key *key,
		efd_value_t val)
{
	const uint8_t *p = (const uint8_t *) key;
	unsigned int i;

	rte_log(RTE_LOG_DEBUG, efd_logtype_test, "%s key:0x", msg);
	for (i = 0; i < sizeof(struct flow_key); i++)
		rte_log(RTE_LOG_DEBUG, efd_logtype_test, "%02X", p[i]);

	rte_log(RTE_LOG_DEBUG, efd_logtype_test, " @ val %d\n", val);
}

/* Keys used by unit test functions */
static struct flow_key keys[5] = {
	{
		.ip_src = IPv4(0x03, 0x02, 0x01, 0x00),
		.ip_dst = IPv4(0x07, 0x06, 0x05, 0x04),
		.port_src = 0x0908,
		.port_dst = 0x0b0a,
		.proto = 0x0c,
	},
	{
		.ip_src = IPv4(0x13, 0x12, 0x11, 0x10),
		.ip_dst = IPv4(0x17, 0x16, 0x15, 0x14),
		.port_src = 0x1918,
		.port_dst = 0x1b1a,
		.proto = 0x1c,
	},
	{
		.ip_src = IPv4(0x23, 0x22, 0x21, 0x20),
		.ip_dst = IPv4(0x27, 0x26, 0x25, 0x24),
		.port_src = 0x2928,
		.port_dst = 0x2b2a,
		.proto = 0x2c,
	},
	{
		.ip_src = IPv4(0x33, 0x32, 0x31, 0x30),
		.ip_dst = IPv4(0x37, 0x36, 0x35, 0x34),
		.port_src = 0x3938,
		.port_dst = 0x3b3a,
		.proto = 0x3c,
	},
	{
		.ip_src = IPv4(0x43, 0x42, 0x41, 0x40),
		.ip_dst = IPv4(0x47, 0x46, 0x45, 0x44),
		.port_src = 0x4948,
		.port_dst = 0x4b4a,
		.proto = 0x4c,
	}
};
/* Array to store the data */
efd_value_t data[5];

static inline uint8_t efd_get_all_sockets_bitmask(void)
{
	uint8_t all_cpu_sockets_bitmask = 0;
	unsigned int i;
	unsigned int next_lcore = rte_get_master_lcore();
	const int val_true = 1, val_false = 0;
	for (i = 0; i < rte_lcore_count(); i++) {
		all_cpu_sockets_bitmask |= 1 << rte_lcore_to_socket_id(next_lcore);
		next_lcore = rte_get_next_lcore(next_lcore, val_false, val_true);
	}

	return all_cpu_sockets_bitmask;
}

/*
 * Basic sequence of operations for a single key:
 *      - add
 *      - lookup (hit)
 *      - delete
 * Note: lookup (miss) is not applicable since this is a filter
 */
static int test_add_delete(void)
{
	struct rte_efd_table *handle;
	/* test with standard add/lookup/delete functions */
	efd_value_t prev_value;
	printf("Entering %s\n", __func__);

	handle = rte_efd_create("test_add_delete",
			TABLE_SIZE, sizeof(struct flow_key),
			efd_get_all_sockets_bitmask(), test_socket_id);
	TEST_ASSERT_NOT_NULL(handle, "Error creating the EFD table\n");

	data[0] = mrand48() & VALUE_BITMASK;
	TEST_ASSERT_SUCCESS(rte_efd_update(handle, test_socket_id, &keys[0],
			data[0]),
			"Error inserting the key");
	print_key_info("Add", &keys[0], data[0]);

	TEST_ASSERT_EQUAL(rte_efd_lookup(handle, test_socket_id, &keys[0]),
			data[0],
			"failed to find key");

	TEST_ASSERT_SUCCESS(rte_efd_delete(handle, test_socket_id, &keys[0],
			&prev_value),
			"failed to delete key");
	TEST_ASSERT_EQUAL(prev_value, data[0],
			"failed to delete the expected value, got %d, "
			"expected %d", prev_value, data[0]);
	print_key_info("Del", &keys[0], data[0]);

	rte_efd_free(handle);

	return 0;
}

/*
 * Sequence of operations for a single key:
 *      - add
 *      - lookup: hit
 *      - add: update
 *      - lookup: hit (updated data)
 *      - delete: hit
 */
static int test_add_update_delete(void)
{
	struct rte_efd_table *handle;
	printf("Entering %s\n", __func__);
	/* test with standard add/lookup/delete functions */
	efd_value_t prev_value;
	data[1] = mrand48() & VALUE_BITMASK;

	handle = rte_efd_create("test_add_update_delete", TABLE_SIZE,
			sizeof(struct flow_key),
			efd_get_all_sockets_bitmask(), test_socket_id);
	TEST_ASSERT_NOT_NULL(handle, "Error creating the efd table\n");

	TEST_ASSERT_SUCCESS(rte_efd_update(handle, test_socket_id, &keys[1],
			data[1]), "Error inserting the key");
	print_key_info("Add", &keys[1], data[1]);

	TEST_ASSERT_EQUAL(rte_efd_lookup(handle, test_socket_id, &keys[1]),
			data[1], "failed to find key");
	print_key_info("Lkp", &keys[1], data[1]);

	data[1] = data[1] + 1;
	TEST_ASSERT_SUCCESS(rte_efd_update(handle, test_socket_id, &keys[1],
			data[1]), "Error re-inserting the key");
	print_key_info("Add", &keys[1], data[1]);

	TEST_ASSERT_EQUAL(rte_efd_lookup(handle, test_socket_id, &keys[1]),
			data[1], "failed to find key");
	print_key_info("Lkp", &keys[1], data[1]);

	TEST_ASSERT_SUCCESS(rte_efd_delete(handle, test_socket_id, &keys[1],
			&prev_value), "failed to delete key");
	TEST_ASSERT_EQUAL(prev_value, data[1],
			"failed to delete the expected value, got %d, "
			"expected %d", prev_value, data[1]);
	print_key_info("Del", &keys[1], data[1]);


	rte_efd_free(handle);
	return 0;
}

/*
 * Sequence of operations for find existing EFD table
 *
 *  - create table
 *  - find existing table: hit
 *  - find non-existing table: miss
 *
 */
static int test_efd_find_existing(void)
{
	struct rte_efd_table *handle = NULL, *result = NULL;

	printf("Entering %s\n", __func__);

	/* Create EFD table. */
	handle = rte_efd_create("efd_find_existing", TABLE_SIZE,
			sizeof(struct flow_key),
			efd_get_all_sockets_bitmask(), test_socket_id);
	TEST_ASSERT_NOT_NULL(handle, "Error creating the efd table\n");

	/* Try to find existing EFD table */
	result = rte_efd_find_existing("efd_find_existing");
	TEST_ASSERT_EQUAL(result, handle, "could not find existing efd table");

	/* Try to find non-existing EFD table */
	result = rte_efd_find_existing("efd_find_non_existing");
	TEST_ASSERT_NULL(result, "found table that shouldn't exist");

	/* Cleanup. */
	rte_efd_free(handle);

	return 0;
}

/*
 * Sequence of operations for 5 keys
 *      - add keys
 *      - lookup keys: hit  (bulk)
 *      - add keys (update)
 *      - lookup keys: hit (updated data)
 *      - delete keys : hit
 */
static int test_five_keys(void)
{
	struct rte_efd_table *handle;
	const void *key_array[5] = {0};
	efd_value_t result[5] = {0};
	efd_value_t prev_value;
	unsigned int i;
	printf("Entering %s\n", __func__);

	handle = rte_efd_create("test_five_keys", TABLE_SIZE,
			sizeof(struct flow_key),
			efd_get_all_sockets_bitmask(), test_socket_id);
	TEST_ASSERT_NOT_NULL(handle, "Error creating the efd table\n");

	/* Setup data */
	for (i = 0; i < 5; i++)
		data[i] = mrand48() & VALUE_BITMASK;

	/* Add */
	for (i = 0; i < 5; i++) {
		TEST_ASSERT_SUCCESS(rte_efd_update(handle, test_socket_id,
				&keys[i], data[i]),
				"Error inserting the key");
		print_key_info("Add", &keys[i], data[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++)
		key_array[i] = &keys[i];

	rte_efd_lookup_bulk(handle, test_socket_id, 5,
			(void *) &key_array, result);

	for (i = 0; i < 5; i++) {
		TEST_ASSERT_EQUAL(result[i], data[i],
				"bulk: failed to find key. Expected %d, got %d",
				data[i], result[i]);
		print_key_info("Lkp", &keys[i], data[i]);
	}

	/* Modify data (bulk) */
	for (i = 0; i < 5; i++)
		data[i] = data[i] + 1;

	/* Add - update */
	for (i = 0; i < 5; i++) {
		TEST_ASSERT_SUCCESS(rte_efd_update(handle, test_socket_id,
				&keys[i], data[i]),
				"Error inserting the key");
		print_key_info("Add", &keys[i], data[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++) {
		TEST_ASSERT_EQUAL(rte_efd_lookup(handle, test_socket_id,
				&keys[i]), data[i],
				"failed to find key");
		print_key_info("Lkp", &keys[i], data[i]);
	}

	/* Delete */
	for (i = 0; i < 5; i++) {
		TEST_ASSERT_SUCCESS(rte_efd_delete(handle, test_socket_id,
				&keys[i], &prev_value),
				"failed to delete key");
		TEST_ASSERT_EQUAL(prev_value, data[i],
				"failed to delete the expected value, got %d, "
				"expected %d", prev_value, data[i]);
		print_key_info("Del", &keys[i], data[i]);
	}


	rte_efd_free(handle);

	return 0;
}

/*
 * Test to see the average table utilization (entries added/max entries)
 * before hitting a random entry that cannot be added
 */
static int test_average_table_utilization(void)
{
	struct rte_efd_table *handle = NULL;
	uint32_t num_rules_in = TABLE_SIZE;
	uint8_t simple_key[EFD_TEST_KEY_LEN];
	unsigned int i, j;
	unsigned int added_keys, average_keys_added = 0;

	printf("Evaluating table utilization and correctness, please wait\n");
	fflush(stdout);

	for (j = 0; j < ITERATIONS; j++) {
		handle = rte_efd_create("test_efd", num_rules_in,
				EFD_TEST_KEY_LEN, efd_get_all_sockets_bitmask(),
				test_socket_id);
		if (handle == NULL) {
			printf("efd table creation failed\n");
			return -1;
		}

		unsigned int succeeded = 0;
		unsigned int lost_keys = 0;

		/* Add random entries until key cannot be added */
		for (added_keys = 0; added_keys < num_rules_in; added_keys++) {

			for (i = 0; i < EFD_TEST_KEY_LEN; i++)
				simple_key[i] = rte_rand() & 0xFF;

			efd_value_t val = simple_key[0];

			if (rte_efd_update(handle, test_socket_id, simple_key,
						val))
				break; /* continue;*/
			if (rte_efd_lookup(handle, test_socket_id, simple_key)
					!= val)
				lost_keys++;
			else
				succeeded++;
		}

		average_keys_added += succeeded;

		/* Reset the table */
		rte_efd_free(handle);

		/* Print progress on operations */
		printf("Added %10u	Succeeded %10u	Lost %10u\n",
				added_keys, succeeded, lost_keys);
		fflush(stdout);
	}

	average_keys_added /= ITERATIONS;

	printf("\nAverage table utilization = %.2f%% (%u/%u)\n",
			((double) average_keys_added / num_rules_in * 100),
			average_keys_added, num_rules_in);

	return 0;
}

/*
 * Do tests for EFD creation with bad parameters.
 */
static int test_efd_creation_with_bad_parameters(void)
{
	struct rte_efd_table *handle, *tmp;
	printf("Entering %s, **Errors are expected **\n", __func__);

	handle = rte_efd_create("creation_with_bad_parameters_0", TABLE_SIZE, 0,
			efd_get_all_sockets_bitmask(), test_socket_id);
	if (handle != NULL) {
		rte_efd_free(handle);
		printf("Impossible creating EFD table successfully "
			"if key_len in parameter is zero\n");
		return -1;
	}

	handle = rte_efd_create("creation_with_bad_parameters_1", TABLE_SIZE,
			sizeof(struct flow_key), 0, test_socket_id);
	if (handle != NULL) {
		rte_efd_free(handle);
		printf("Impossible creating EFD table successfully "
			"with invalid socket bitmask\n");
		return -1;
	}

	handle = rte_efd_create("creation_with_bad_parameters_2", TABLE_SIZE,
			sizeof(struct flow_key), efd_get_all_sockets_bitmask(),
			255);
	if (handle != NULL) {
		rte_efd_free(handle);
		printf("Impossible creating EFD table successfully "
			"with invalid socket\n");
		return -1;
	}

	/* test with same name should fail */
	handle = rte_efd_create("same_name", TABLE_SIZE,
			sizeof(struct flow_key),
			efd_get_all_sockets_bitmask(), 0);
	if (handle == NULL) {
		printf("Cannot create first EFD table with 'same_name'\n");
		return -1;
	}
	tmp = rte_efd_create("same_name", TABLE_SIZE, sizeof(struct flow_key),
			efd_get_all_sockets_bitmask(), 0);
	if (tmp != NULL) {
		printf("Creation of EFD table with same name should fail\n");
		rte_efd_free(handle);
		rte_efd_free(tmp);
		return -1;
	}
	rte_efd_free(handle);

	printf("# Test successful. No more errors expected\n");

	return 0;
}

static int
test_efd(void)
{

	/* Unit tests */
	if (test_add_delete() < 0)
		return -1;
	if (test_efd_find_existing() < 0)
		return -1;
	if (test_add_update_delete() < 0)
		return -1;
	if (test_five_keys() < 0)
		return -1;
	if (test_efd_creation_with_bad_parameters() < 0)
		return -1;
	if (test_average_table_utilization() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(efd_autotest, test_efd);
