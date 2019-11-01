/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <rte_memory.h>
#include <rte_log.h>
#include <rte_rib6.h>
#include <rte_fib6.h>

#include "test.h"

typedef int32_t (*rte_fib6_test)(void);

static int32_t test_create_invalid(void);
static int32_t test_multiple_create(void);
static int32_t test_free_null(void);
static int32_t test_add_del_invalid(void);
static int32_t test_get_invalid(void);
static int32_t test_lookup(void);

#define MAX_ROUTES	(1 << 16)
/** Maximum number of tbl8 for 2-byte entries */
#define MAX_TBL8	(1 << 15)

/*
 * Check that rte_fib6_create fails gracefully for incorrect user input
 * arguments
 */
int32_t
test_create_invalid(void)
{
	struct rte_fib6 *fib = NULL;
	struct rte_fib6_conf config;

	config.max_routes = MAX_ROUTES;
	config.default_nh = 0;
	config.type = RTE_FIB6_DUMMY;

	/* rte_fib6_create: fib name == NULL */
	fib = rte_fib6_create(NULL, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib == NULL,
		"Call succeeded with invalid parameters\n");

	/* rte_fib6_create: config == NULL */
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, NULL);
	RTE_TEST_ASSERT(fib == NULL,
		"Call succeeded with invalid parameters\n");

	/* socket_id < -1 is invalid */
	fib = rte_fib6_create(__func__, -2, &config);
	RTE_TEST_ASSERT(fib == NULL,
		"Call succeeded with invalid parameters\n");

	/* rte_fib6_create: max_routes = 0 */
	config.max_routes = 0;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib == NULL,
		"Call succeeded with invalid parameters\n");
	config.max_routes = MAX_ROUTES;

	config.type = RTE_FIB6_TYPE_MAX;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib == NULL,
		"Call succeeded with invalid parameters\n");

	config.type = RTE_FIB6_TRIE;
	config.trie.num_tbl8 = MAX_TBL8;

	config.trie.nh_sz = RTE_FIB6_TRIE_8B + 1;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib == NULL,
		"Call succeeded with invalid parameters\n");
	config.trie.nh_sz = RTE_FIB6_TRIE_8B;

	config.trie.num_tbl8 = 0;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib == NULL,
		"Call succeeded with invalid parameters\n");

	return TEST_SUCCESS;
}

/*
 * Create fib table then delete fib table 10 times
 * Use a slightly different rules size each time
 */
int32_t
test_multiple_create(void)
{
	struct rte_fib6 *fib = NULL;
	struct rte_fib6_conf config;
	int32_t i;

	config.default_nh = 0;
	config.type = RTE_FIB6_DUMMY;

	for (i = 0; i < 100; i++) {
		config.max_routes = MAX_ROUTES - i;
		fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
		RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");
		rte_fib6_free(fib);
	}
	/* Can not test free so return success */
	return TEST_SUCCESS;
}

/*
 * Call rte_fib6_free for NULL pointer user input. Note: free has no return and
 * therefore it is impossible to check for failure but this test is added to
 * increase function coverage metrics and to validate that freeing null does
 * not crash.
 */
int32_t
test_free_null(void)
{
	struct rte_fib6 *fib = NULL;
	struct rte_fib6_conf config;

	config.max_routes = MAX_ROUTES;
	config.default_nh = 0;
	config.type = RTE_FIB6_DUMMY;

	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");

	rte_fib6_free(fib);
	rte_fib6_free(NULL);

	return TEST_SUCCESS;
}

/*
 * Check that rte_fib6_add and rte_fib6_delete fails gracefully
 * for incorrect user input arguments
 */
int32_t
test_add_del_invalid(void)
{
	struct rte_fib6 *fib = NULL;
	struct rte_fib6_conf config;
	uint64_t nh = 100;
	uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE] = {0};
	int ret;
	uint8_t depth = 24;

	config.max_routes = MAX_ROUTES;
	config.default_nh = 0;
	config.type = RTE_FIB6_DUMMY;

	/* rte_fib6_add: fib == NULL */
	ret = rte_fib6_add(NULL, ip, depth, nh);
	RTE_TEST_ASSERT(ret < 0,
		"Call succeeded with invalid parameters\n");

	/* rte_fib6_delete: fib == NULL */
	ret = rte_fib6_delete(NULL, ip, depth);
	RTE_TEST_ASSERT(ret < 0,
		"Call succeeded with invalid parameters\n");

	/*Create valid fib to use in rest of test. */
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");

	/* rte_fib6_add: depth > RTE_FIB6_MAXDEPTH */
	ret = rte_fib6_add(fib, ip, RTE_FIB6_MAXDEPTH + 1, nh);
	RTE_TEST_ASSERT(ret < 0,
		"Call succeeded with invalid parameters\n");

	/* rte_fib6_delete: depth > RTE_FIB6_MAXDEPTH */
	ret = rte_fib6_delete(fib, ip, RTE_FIB6_MAXDEPTH + 1);
	RTE_TEST_ASSERT(ret < 0,
		"Call succeeded with invalid parameters\n");

	rte_fib6_free(fib);

	return TEST_SUCCESS;
}

/*
 * Check that rte_fib6_get_dp and rte_fib6_get_rib fails gracefully
 * for incorrect user input arguments
 */
int32_t
test_get_invalid(void)
{
	void *p;

	p = rte_fib6_get_dp(NULL);
	RTE_TEST_ASSERT(p == NULL,
		"Call succeeded with invalid parameters\n");

	p = rte_fib6_get_rib(NULL);
	RTE_TEST_ASSERT(p == NULL,
		"Call succeeded with invalid parameters\n");

	return TEST_SUCCESS;
}

/*
 * Add routes for one supernet with all possible depths and do lookup
 * on each step
 * After delete routes with doing lookup on each step
 */
static int
lookup_and_check_asc(struct rte_fib6 *fib,
	uint8_t ip_arr[RTE_FIB6_MAXDEPTH][RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t ip_missing[][RTE_FIB6_IPV6_ADDR_SIZE], uint64_t def_nh,
	uint32_t n)
{
	uint64_t nh_arr[RTE_FIB6_MAXDEPTH];
	int ret;
	uint32_t i = 0;

	ret = rte_fib6_lookup_bulk(fib, ip_arr, nh_arr, RTE_FIB6_MAXDEPTH);
	RTE_TEST_ASSERT(ret == 0, "Failed to lookup\n");

	for (; i <= RTE_FIB6_MAXDEPTH - n; i++)
		RTE_TEST_ASSERT(nh_arr[i] == n,
			"Failed to get proper nexthop\n");

	for (; i < RTE_FIB6_MAXDEPTH; i++)
		RTE_TEST_ASSERT(nh_arr[i] == --n,
			"Failed to get proper nexthop\n");

	ret = rte_fib6_lookup_bulk(fib, ip_missing, nh_arr, 1);
	RTE_TEST_ASSERT((ret == 0) && (nh_arr[0] == def_nh),
		"Failed to get proper nexthop\n");

	return TEST_SUCCESS;
}

static int
lookup_and_check_desc(struct rte_fib6 *fib,
	uint8_t ip_arr[RTE_FIB6_MAXDEPTH][RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t ip_missing[][RTE_FIB6_IPV6_ADDR_SIZE], uint64_t def_nh,
	uint32_t n)
{
	uint64_t nh_arr[RTE_FIB6_MAXDEPTH];
	int ret;
	uint32_t i = 0;

	ret = rte_fib6_lookup_bulk(fib, ip_arr, nh_arr, RTE_FIB6_MAXDEPTH);
	RTE_TEST_ASSERT(ret == 0, "Failed to lookup\n");

	for (; i < n; i++)
		RTE_TEST_ASSERT(nh_arr[i] == RTE_FIB6_MAXDEPTH - i,
			"Failed to get proper nexthop\n");

	for (; i < RTE_FIB6_MAXDEPTH; i++)
		RTE_TEST_ASSERT(nh_arr[i] == def_nh,
			"Failed to get proper nexthop\n");

	ret = rte_fib6_lookup_bulk(fib, ip_missing, nh_arr, 1);
	RTE_TEST_ASSERT((ret == 0) && (nh_arr[0] == def_nh),
		"Failed to get proper nexthop\n");

	return TEST_SUCCESS;
}

static int
check_fib(struct rte_fib6 *fib)
{
	uint64_t def_nh = 100;
	uint8_t ip_arr[RTE_FIB6_MAXDEPTH][RTE_FIB6_IPV6_ADDR_SIZE];
	uint8_t ip_add[RTE_FIB6_IPV6_ADDR_SIZE] = {0};
	uint8_t ip_missing[1][RTE_FIB6_IPV6_ADDR_SIZE] = { {255} };
	uint32_t i, j;
	int ret;

	ip_add[0] = 128;
	ip_missing[0][0] = 127;
	for (i = 0; i < RTE_FIB6_MAXDEPTH; i++) {
		for (j = 0; j < RTE_FIB6_IPV6_ADDR_SIZE; j++) {
			ip_arr[i][j] = ip_add[j] |
				~get_msk_part(RTE_FIB6_MAXDEPTH - i, j);
		}
	}

	ret = lookup_and_check_desc(fib, ip_arr, ip_missing, def_nh, 0);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS, "Lookup and check fails\n");

	for (i = 1; i <= RTE_FIB6_MAXDEPTH; i++) {
		ret = rte_fib6_add(fib, ip_add, i, i);
		RTE_TEST_ASSERT(ret == 0, "Failed to add a route\n");
		ret = lookup_and_check_asc(fib, ip_arr, ip_missing, def_nh, i);
		RTE_TEST_ASSERT(ret == TEST_SUCCESS,
			"Lookup and check fails\n");
	}

	for (i = RTE_FIB6_MAXDEPTH; i > 1; i--) {
		ret = rte_fib6_delete(fib, ip_add, i);
		RTE_TEST_ASSERT(ret == 0, "Failed to delete a route\n");
		ret = lookup_and_check_asc(fib, ip_arr, ip_missing,
			def_nh, i - 1);

		RTE_TEST_ASSERT(ret == TEST_SUCCESS,
			"Lookup and check fails\n");
	}
	ret = rte_fib6_delete(fib, ip_add, i);
	RTE_TEST_ASSERT(ret == 0, "Failed to delete a route\n");
	ret = lookup_and_check_desc(fib, ip_arr, ip_missing, def_nh, 0);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS,
		"Lookup and check fails\n");

	for (i = 0; i < RTE_FIB6_MAXDEPTH; i++) {
		ret = rte_fib6_add(fib, ip_add, RTE_FIB6_MAXDEPTH - i,
			RTE_FIB6_MAXDEPTH - i);
		RTE_TEST_ASSERT(ret == 0, "Failed to add a route\n");
		ret = lookup_and_check_desc(fib, ip_arr, ip_missing,
			def_nh, i + 1);
		RTE_TEST_ASSERT(ret == TEST_SUCCESS,
			"Lookup and check fails\n");
	}

	for (i = 1; i <= RTE_FIB6_MAXDEPTH; i++) {
		ret = rte_fib6_delete(fib, ip_add, i);
		RTE_TEST_ASSERT(ret == 0, "Failed to delete a route\n");
		ret = lookup_and_check_desc(fib, ip_arr, ip_missing, def_nh,
			RTE_FIB6_MAXDEPTH - i);
		RTE_TEST_ASSERT(ret == TEST_SUCCESS,
			"Lookup and check fails\n");
	}

	return TEST_SUCCESS;
}

int32_t
test_lookup(void)
{
	struct rte_fib6 *fib = NULL;
	struct rte_fib6_conf config;
	uint64_t def_nh = 100;
	int ret;

	config.max_routes = MAX_ROUTES;
	config.default_nh = def_nh;
	config.type = RTE_FIB6_DUMMY;

	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");
	ret = check_fib(fib);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS,
		"Check_fib fails for DUMMY type\n");
	rte_fib6_free(fib);

	config.type = RTE_FIB6_TRIE;

	config.trie.nh_sz = RTE_FIB6_TRIE_2B;
	config.trie.num_tbl8 = MAX_TBL8 - 1;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");
	ret = check_fib(fib);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS,
		"Check_fib fails for TRIE_2B type\n");
	rte_fib6_free(fib);

	config.trie.nh_sz = RTE_FIB6_TRIE_4B;
	config.trie.num_tbl8 = MAX_TBL8;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");
	ret = check_fib(fib);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS,
		"Check_fib fails for TRIE_4B type\n");
	rte_fib6_free(fib);

	config.trie.nh_sz = RTE_FIB6_TRIE_8B;
	config.trie.num_tbl8 = MAX_TBL8;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");
	ret = check_fib(fib);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS,
		"Check_fib fails for TRIE_8B type\n");
	rte_fib6_free(fib);

	return TEST_SUCCESS;
}

static struct unit_test_suite fib6_fast_tests = {
	.suite_name = "fib6 autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
	TEST_CASE(test_create_invalid),
	TEST_CASE(test_free_null),
	TEST_CASE(test_add_del_invalid),
	TEST_CASE(test_get_invalid),
	TEST_CASE(test_lookup),
	TEST_CASES_END()
	}
};

static struct unit_test_suite fib6_slow_tests = {
	.suite_name = "fib6 slow autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
	TEST_CASE(test_multiple_create),
	TEST_CASES_END()
	}
};

/*
 * Do all unit tests.
 */
static int
test_fib6(void)
{
	return unit_test_suite_runner(&fib6_fast_tests);
}

static int
test_slow_fib6(void)
{
	return unit_test_suite_runner(&fib6_slow_tests);
}

REGISTER_TEST_COMMAND(fib6_autotest, test_fib6);
REGISTER_TEST_COMMAND(fib6_slow_autotest, test_slow_fib6);
