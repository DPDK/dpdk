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
#include <rte_malloc.h>

#include "test.h"

typedef int32_t (*rte_fib6_test)(void);

static int32_t test_create_invalid(void);
static int32_t test_multiple_create(void);
static int32_t test_free_null(void);
static int32_t test_add_del_invalid(void);
static int32_t test_get_invalid(void);
static int32_t test_lookup(void);
static int32_t test_invalid_rcu(void);
static int32_t test_fib_rcu_sync_rw(void);

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
	config.rib_ext_sz = 0;
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

	config.type = RTE_FIB6_TRIE + 1;
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

	config.rib_ext_sz = 0;
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
	config.rib_ext_sz = 0;
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
	struct rte_ipv6_addr ip = RTE_IPV6_ADDR_UNSPEC;
	int ret;
	uint8_t depth = 24;

	config.max_routes = MAX_ROUTES;
	config.rib_ext_sz = 0;
	config.default_nh = 0;
	config.type = RTE_FIB6_DUMMY;

	/* rte_fib6_add: fib == NULL */
	ret = rte_fib6_add(NULL, &ip, depth, nh);
	RTE_TEST_ASSERT(ret < 0,
		"Call succeeded with invalid parameters\n");

	/* rte_fib6_delete: fib == NULL */
	ret = rte_fib6_delete(NULL, &ip, depth);
	RTE_TEST_ASSERT(ret < 0,
		"Call succeeded with invalid parameters\n");

	/*Create valid fib to use in rest of test. */
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");

	/* rte_fib6_add: depth > RTE_IPV6_MAX_DEPTH */
	ret = rte_fib6_add(fib, &ip, RTE_IPV6_MAX_DEPTH + 1, nh);
	RTE_TEST_ASSERT(ret < 0,
		"Call succeeded with invalid parameters\n");

	/* rte_fib6_delete: depth > RTE_IPV6_MAX_DEPTH */
	ret = rte_fib6_delete(fib, &ip, RTE_IPV6_MAX_DEPTH + 1);
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
	struct rte_ipv6_addr *ip_arr,
	struct rte_ipv6_addr *ip_missing, uint64_t def_nh,
	uint32_t n)
{
	uint64_t nh_arr[RTE_IPV6_MAX_DEPTH];
	int ret;
	uint32_t i = 0;

	ret = rte_fib6_lookup_bulk(fib, ip_arr, nh_arr, RTE_IPV6_MAX_DEPTH);
	RTE_TEST_ASSERT(ret == 0, "Failed to lookup\n");

	for (; i <= RTE_IPV6_MAX_DEPTH - n; i++)
		RTE_TEST_ASSERT(nh_arr[i] == n,
			"Failed to get proper nexthop\n");

	for (; i < RTE_IPV6_MAX_DEPTH; i++)
		RTE_TEST_ASSERT(nh_arr[i] == --n,
			"Failed to get proper nexthop\n");

	ret = rte_fib6_lookup_bulk(fib, ip_missing, nh_arr, 1);
	RTE_TEST_ASSERT((ret == 0) && (nh_arr[0] == def_nh),
		"Failed to get proper nexthop\n");

	return TEST_SUCCESS;
}

static int
lookup_and_check_desc(struct rte_fib6 *fib,
	struct rte_ipv6_addr *ip_arr,
	struct rte_ipv6_addr *ip_missing, uint64_t def_nh,
	uint32_t n)
{
	uint64_t nh_arr[RTE_IPV6_MAX_DEPTH];
	int ret;
	uint32_t i = 0;

	ret = rte_fib6_lookup_bulk(fib, ip_arr, nh_arr, RTE_IPV6_MAX_DEPTH);
	RTE_TEST_ASSERT(ret == 0, "Failed to lookup\n");

	for (; i < n; i++)
		RTE_TEST_ASSERT(nh_arr[i] == RTE_IPV6_MAX_DEPTH - i,
			"Failed to get proper nexthop\n");

	for (; i < RTE_IPV6_MAX_DEPTH; i++)
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
	struct rte_ipv6_addr ip_arr[RTE_IPV6_MAX_DEPTH];
	struct rte_ipv6_addr ip_add = RTE_IPV6(0x8000, 0, 0, 0, 0, 0, 0, 0);
	struct rte_ipv6_addr ip_missing =
		RTE_IPV6(0x7fff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff);
	uint32_t i, j;
	int ret;

	for (i = 0; i < RTE_IPV6_MAX_DEPTH; i++) {
		ip_arr[i] = ip_add;
		j = (RTE_IPV6_MAX_DEPTH - i) / CHAR_BIT;
		if (j < RTE_IPV6_ADDR_SIZE) {
			ip_arr[i].a[j] |= UINT8_MAX >> ((RTE_IPV6_MAX_DEPTH - i) % CHAR_BIT);
			for (j++; j < RTE_IPV6_ADDR_SIZE; j++)
				ip_arr[i].a[j] = 0xff;
		}
	}

	ret = lookup_and_check_desc(fib, ip_arr, &ip_missing, def_nh, 0);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS, "Lookup and check fails\n");

	for (i = 1; i <= RTE_IPV6_MAX_DEPTH; i++) {
		ret = rte_fib6_add(fib, &ip_add, i, i);
		RTE_TEST_ASSERT(ret == 0, "Failed to add a route\n");
		ret = lookup_and_check_asc(fib, ip_arr, &ip_missing, def_nh, i);
		RTE_TEST_ASSERT(ret == TEST_SUCCESS,
			"Lookup and check fails\n");
	}

	for (i = RTE_IPV6_MAX_DEPTH; i > 1; i--) {
		ret = rte_fib6_delete(fib, &ip_add, i);
		RTE_TEST_ASSERT(ret == 0, "Failed to delete a route\n");
		ret = lookup_and_check_asc(fib, ip_arr, &ip_missing,
			def_nh, i - 1);

		RTE_TEST_ASSERT(ret == TEST_SUCCESS,
			"Lookup and check fails\n");
	}
	ret = rte_fib6_delete(fib, &ip_add, i);
	RTE_TEST_ASSERT(ret == 0, "Failed to delete a route\n");
	ret = lookup_and_check_desc(fib, ip_arr, &ip_missing, def_nh, 0);
	RTE_TEST_ASSERT(ret == TEST_SUCCESS,
		"Lookup and check fails\n");

	for (i = 0; i < RTE_IPV6_MAX_DEPTH; i++) {
		ret = rte_fib6_add(fib, &ip_add, RTE_IPV6_MAX_DEPTH - i,
			RTE_IPV6_MAX_DEPTH - i);
		RTE_TEST_ASSERT(ret == 0, "Failed to add a route\n");
		ret = lookup_and_check_desc(fib, ip_arr, &ip_missing,
			def_nh, i + 1);
		RTE_TEST_ASSERT(ret == TEST_SUCCESS,
			"Lookup and check fails\n");
	}

	for (i = 1; i <= RTE_IPV6_MAX_DEPTH; i++) {
		ret = rte_fib6_delete(fib, &ip_add, i);
		RTE_TEST_ASSERT(ret == 0, "Failed to delete a route\n");
		ret = lookup_and_check_desc(fib, ip_arr, &ip_missing, def_nh,
			RTE_IPV6_MAX_DEPTH - i);
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
	config.rib_ext_sz = 0;
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

/*
 * rte_fib6_rcu_qsbr_add positive and negative tests.
 *  - Add RCU QSBR variable to FIB
 *  - Add another RCU QSBR variable to FIB
 *  - Check returns
 */
int32_t
test_invalid_rcu(void)
{
	struct rte_fib6 *fib = NULL;
	struct rte_fib6_conf config = { 0 };
	size_t sz;
	struct rte_rcu_qsbr *qsv;
	struct rte_rcu_qsbr *qsv2;
	int32_t status;
	struct rte_fib6_rcu_config rcu_cfg = {0};
	uint64_t def_nh = 100;

	config.max_routes = MAX_ROUTES;
	config.rib_ext_sz = 0;
	config.default_nh = def_nh;

	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
	qsv = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz, RTE_CACHE_LINE_SIZE,
		SOCKET_ID_ANY);
	RTE_TEST_ASSERT(qsv != NULL, "Can not allocate memory for RCU\n");

	status = rte_rcu_qsbr_init(qsv, RTE_MAX_LCORE);
	RTE_TEST_ASSERT(status == 0, "Can not initialize RCU\n");

	rcu_cfg.v = qsv;

	/* adding rcu to RTE_FIB6_DUMMY FIB type */
	config.type = RTE_FIB6_DUMMY;
	rcu_cfg.mode = RTE_FIB6_QSBR_MODE_SYNC;
	status = rte_fib6_rcu_qsbr_add(fib, &rcu_cfg);
	RTE_TEST_ASSERT(status == -ENOTSUP,
		"rte_fib6_rcu_qsbr_add returned wrong error status when called with DUMMY type FIB\n");
	rte_fib6_free(fib);

	config.type = RTE_FIB6_TRIE;
	config.trie.nh_sz = RTE_FIB6_TRIE_4B;
	config.trie.num_tbl8 = MAX_TBL8;
	fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(fib != NULL, "Failed to create FIB\n");

	/* Call rte_fib6_rcu_qsbr_add without fib or config */
	status = rte_fib6_rcu_qsbr_add(NULL, &rcu_cfg);
	RTE_TEST_ASSERT(status == -EINVAL, "RCU added without fib\n");
	status = rte_fib6_rcu_qsbr_add(fib, NULL);
	RTE_TEST_ASSERT(status == -EINVAL, "RCU added without config\n");

	/* Invalid QSBR mode */
	rcu_cfg.mode = 2;
	status = rte_fib6_rcu_qsbr_add(fib, &rcu_cfg);
	RTE_TEST_ASSERT(status == -EINVAL, "RCU added with incorrect mode\n");

	rcu_cfg.mode = RTE_FIB6_QSBR_MODE_DQ;

	/* Attach RCU QSBR to FIB to check for double attach */
	status = rte_fib6_rcu_qsbr_add(fib, &rcu_cfg);
	RTE_TEST_ASSERT(status == 0, "Can not attach RCU to FIB\n");

	/* Create and attach another RCU QSBR to FIB table */
	qsv2 = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz, RTE_CACHE_LINE_SIZE,
		SOCKET_ID_ANY);
	RTE_TEST_ASSERT(qsv2 != NULL, "Can not allocate memory for RCU\n");

	rcu_cfg.v = qsv2;
	rcu_cfg.mode = RTE_FIB6_QSBR_MODE_SYNC;
	status = rte_fib6_rcu_qsbr_add(fib, &rcu_cfg);
	RTE_TEST_ASSERT(status == -EEXIST, "Secondary RCU was mistakenly attached\n");

	rte_fib6_free(fib);
	rte_free(qsv);
	rte_free(qsv2);

	return TEST_SUCCESS;
}

static struct rte_fib6 *g_fib;
static struct rte_rcu_qsbr *g_v;
static struct rte_ipv6_addr g_ip = RTE_IPV6(0x2001, 0xabcd, 0, 0, 0, 0, 0, 1);
static volatile uint8_t writer_done;
/* Report quiescent state interval every 1024 lookups. Larger critical
 * sections in reader will result in writer polling multiple times.
 */
#define QSBR_REPORTING_INTERVAL 1024
#define WRITER_ITERATIONS	512

/*
 * Reader thread using rte_fib6 data structure with RCU.
 */
static int
test_fib_rcu_qsbr_reader(void *arg)
{
	int i;
	uint64_t next_hop_return = 0;

	RTE_SET_USED(arg);
	/* Register this thread to report quiescent state */
	rte_rcu_qsbr_thread_register(g_v, 0);
	rte_rcu_qsbr_thread_online(g_v, 0);

	do {
		for (i = 0; i < QSBR_REPORTING_INTERVAL; i++)
			rte_fib6_lookup_bulk(g_fib, &g_ip, &next_hop_return, 1);

		/* Update quiescent state */
		rte_rcu_qsbr_quiescent(g_v, 0);
	} while (!writer_done);

	rte_rcu_qsbr_thread_offline(g_v, 0);
	rte_rcu_qsbr_thread_unregister(g_v, 0);

	return 0;
}

/*
 * rte_fib6_rcu_qsbr_add sync mode functional test.
 * 1 Reader and 1 writer. They cannot be in the same thread in this test.
 *  - Create FIB which supports 1 tbl8 group at max
 *  - Add RCU QSBR variable with sync mode to FIB
 *  - Register a reader thread. Reader keeps looking up a specific rule.
 *  - Writer keeps adding and deleting a specific rule with depth=28 (> 24)
 */
int32_t
test_fib_rcu_sync_rw(void)
{
	struct rte_fib6_conf config = { 0 };
	size_t sz;
	int32_t status;
	uint32_t i, next_hop;
	uint8_t depth;
	struct rte_fib6_rcu_config rcu_cfg = {0};
	uint64_t def_nh = 100;

	if (rte_lcore_count() < 2) {
		printf("Not enough cores for %s, expecting at least 2\n", __func__);
		return TEST_SKIPPED;
	}

	config.max_routes = MAX_ROUTES;
	config.rib_ext_sz = 0;
	config.default_nh = def_nh;
	config.type = RTE_FIB6_TRIE;
	config.trie.nh_sz = RTE_FIB6_TRIE_4B;
	config.trie.num_tbl8 = 1;

	g_fib = rte_fib6_create(__func__, SOCKET_ID_ANY, &config);
	RTE_TEST_ASSERT(g_fib != NULL, "Failed to create FIB\n");

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(1);
	g_v = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz, RTE_CACHE_LINE_SIZE,
		SOCKET_ID_ANY);
	RTE_TEST_ASSERT(g_v != NULL, "Can not allocate memory for RCU\n");

	status = rte_rcu_qsbr_init(g_v, 1);
	RTE_TEST_ASSERT(status == 0, "Can not initialize RCU\n");

	rcu_cfg.v = g_v;
	rcu_cfg.mode = RTE_FIB6_QSBR_MODE_SYNC;
	/* Attach RCU QSBR to FIB table */
	status = rte_fib6_rcu_qsbr_add(g_fib, &rcu_cfg);
	RTE_TEST_ASSERT(status == 0, "Can not attach RCU to FIB\n");

	writer_done = 0;
	/* Launch reader thread */
	rte_eal_remote_launch(test_fib_rcu_qsbr_reader, NULL, rte_get_next_lcore(-1, 1, 0));

	depth = 28;
	next_hop = 1;
	status = rte_fib6_add(g_fib, &g_ip, depth, next_hop);
	if (status != 0) {
		printf("%s: Failed to add rule\n", __func__);
		goto error;
	}

	/* Writer update */
	for (i = 0; i < WRITER_ITERATIONS; i++) {
		status = rte_fib6_delete(g_fib, &g_ip, depth);
		if (status != 0) {
			printf("%s: Failed to delete rule at iteration %d\n", __func__, i);
			goto error;
		}

		status = rte_fib6_add(g_fib, &g_ip, depth, next_hop);
		if (status != 0) {
			printf("%s: Failed to add rule at iteration %d\n", __func__, i);
			goto error;
		}
	}

error:
	writer_done = 1;
	/* Wait until reader exited. */
	rte_eal_mp_wait_lcore();

	rte_fib6_free(g_fib);
	rte_free(g_v);

	return status == 0 ? TEST_SUCCESS : TEST_FAILED;
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
	TEST_CASE(test_invalid_rcu),
	TEST_CASE(test_fib_rcu_sync_rw),
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

REGISTER_FAST_TEST(fib6_autotest, true, true, test_fib6);
REGISTER_PERF_TEST(fib6_slow_autotest, test_slow_fib6);
