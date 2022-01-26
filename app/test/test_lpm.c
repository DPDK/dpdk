/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "test.h"

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_lpm(void)
{
	printf("lpm not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <rte_ip.h>
#include <rte_lpm.h>
#include <rte_malloc.h>

#include "test_xmmt_ops.h"

#define TEST_LPM_ASSERT(cond) do {                                            \
	if (!(cond)) {                                                        \
		printf("Error at line %d: \n", __LINE__);                     \
		return -1;                                                    \
	}                                                                     \
} while(0)

typedef int32_t (*rte_lpm_test)(void);

static int32_t test0(void);
static int32_t test1(void);
static int32_t test2(void);
static int32_t test3(void);
static int32_t test4(void);
static int32_t test5(void);
static int32_t test6(void);
static int32_t test7(void);
static int32_t test8(void);
static int32_t test9(void);
static int32_t test10(void);
static int32_t test11(void);
static int32_t test12(void);
static int32_t test13(void);
static int32_t test14(void);
static int32_t test15(void);
static int32_t test16(void);
static int32_t test17(void);
static int32_t test18(void);
static int32_t test19(void);
static int32_t test20(void);
static int32_t test21(void);

rte_lpm_test tests[] = {
/* Test Cases */
	test0,
	test1,
	test2,
	test3,
	test4,
	test5,
	test6,
	test7,
	test8,
	test9,
	test10,
	test11,
	test12,
	test13,
	test14,
	test15,
	test16,
	test17,
	test18,
	test19,
	test20,
	test21
};

#define MAX_DEPTH 32
#define MAX_RULES 256
#define NUMBER_TBL8S 256
#define PASS 0

/*
 * Check that rte_lpm_create fails gracefully for incorrect user input
 * arguments
 */
int32_t
test0(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm_create: lpm name == NULL */
	lpm = rte_lpm_create(NULL, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm == NULL);

	/* rte_lpm_create: max_rules = 0 */
	/* Note: __func__ inserts the function name, in this case "test0". */
	config.max_rules = 0;
	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm == NULL);

	/* socket_id < -1 is invalid */
	config.max_rules = MAX_RULES;
	lpm = rte_lpm_create(__func__, -2, &config);
	TEST_LPM_ASSERT(lpm == NULL);

	return PASS;
}

/*
 * Create lpm table then delete lpm table 100 times
 * Use a slightly different rules size each time
 * */
int32_t
test1(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	int32_t i;

	/* rte_lpm_free: Free NULL */
	for (i = 0; i < 100; i++) {
		config.max_rules = MAX_RULES - i;
		lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
		TEST_LPM_ASSERT(lpm != NULL);

		rte_lpm_free(lpm);
	}

	/* Can not test free so return success */
	return PASS;
}

/*
 * Call rte_lpm_free for NULL pointer user input. Note: free has no return and
 * therefore it is impossible to check for failure but this test is added to
 * increase function coverage metrics and to validate that freeing null does
 * not crash.
 */
int32_t
test2(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	rte_lpm_free(lpm);
	rte_lpm_free(NULL);
	return PASS;
}

/*
 * Check that rte_lpm_add fails gracefully for incorrect user input arguments
 */
int32_t
test3(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip = RTE_IPV4(0, 0, 0, 0), next_hop = 100;
	uint8_t depth = 24;
	int32_t status = 0;

	/* rte_lpm_add: lpm == NULL */
	status = rte_lpm_add(NULL, ip, depth, next_hop);
	TEST_LPM_ASSERT(status < 0);

	/*Create valid lpm to use in rest of test. */
	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm_add: depth < 1 */
	status = rte_lpm_add(lpm, ip, 0, next_hop);
	TEST_LPM_ASSERT(status < 0);

	/* rte_lpm_add: depth > MAX_DEPTH */
	status = rte_lpm_add(lpm, ip, (MAX_DEPTH + 1), next_hop);
	TEST_LPM_ASSERT(status < 0);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Check that rte_lpm_delete fails gracefully for incorrect user input
 * arguments
 */
int32_t
test4(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip = RTE_IPV4(0, 0, 0, 0);
	uint8_t depth = 24;
	int32_t status = 0;

	/* rte_lpm_delete: lpm == NULL */
	status = rte_lpm_delete(NULL, ip, depth);
	TEST_LPM_ASSERT(status < 0);

	/*Create valid lpm to use in rest of test. */
	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm_delete: depth < 1 */
	status = rte_lpm_delete(lpm, ip, 0);
	TEST_LPM_ASSERT(status < 0);

	/* rte_lpm_delete: depth > MAX_DEPTH */
	status = rte_lpm_delete(lpm, ip, (MAX_DEPTH + 1));
	TEST_LPM_ASSERT(status < 0);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Check that rte_lpm_lookup fails gracefully for incorrect user input
 * arguments
 */
int32_t
test5(void)
{
#if defined(RTE_LIBRTE_LPM_DEBUG)
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip = RTE_IPV4(0, 0, 0, 0), next_hop_return = 0;
	int32_t status = 0;

	/* rte_lpm_lookup: lpm == NULL */
	status = rte_lpm_lookup(NULL, ip, &next_hop_return);
	TEST_LPM_ASSERT(status < 0);

	/*Create valid lpm to use in rest of test. */
	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm_lookup: depth < 1 */
	status = rte_lpm_lookup(lpm, ip, NULL);
	TEST_LPM_ASSERT(status < 0);

	rte_lpm_free(lpm);
#endif
	return PASS;
}



/*
 * Call add, lookup and delete for a single rule with depth <= 24
 */
int32_t
test6(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip = RTE_IPV4(0, 0, 0, 0), next_hop_add = 100, next_hop_return = 0;
	uint8_t depth = 24;
	int32_t status = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Call add, lookup and delete for a single rule with depth > 24
 */

int32_t
test7(void)
{
	xmm_t ipx4;
	uint32_t hop[4];
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip = RTE_IPV4(0, 0, 0, 0), next_hop_add = 100, next_hop_return = 0;
	uint8_t depth = 32;
	int32_t status = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	ipx4 = vect_set_epi32(ip, ip + 0x100, ip - 0x100, ip);
	rte_lpm_lookupx4(lpm, ipx4, hop, UINT32_MAX);
	TEST_LPM_ASSERT(hop[0] == next_hop_add);
	TEST_LPM_ASSERT(hop[1] == UINT32_MAX);
	TEST_LPM_ASSERT(hop[2] == UINT32_MAX);
	TEST_LPM_ASSERT(hop[3] == next_hop_add);

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Use rte_lpm_add to add rules which effect only the second half of the lpm
 * table. Use all possible depths ranging from 1..32. Set the next hop = to the
 * depth. Check lookup hit for on every add and check for lookup miss on the
 * first half of the lpm table after each add. Finally delete all rules going
 * backwards (i.e. from depth = 32 ..1) and carry out a lookup after each
 * delete. The lookup should return the next_hop_add value related to the
 * previous depth value (i.e. depth -1).
 */
int32_t
test8(void)
{
	xmm_t ipx4;
	uint32_t hop[4];
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip1 = RTE_IPV4(127, 255, 255, 255), ip2 = RTE_IPV4(128, 0, 0, 0);
	uint32_t next_hop_add, next_hop_return;
	uint8_t depth;
	int32_t status = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Loop with rte_lpm_add. */
	for (depth = 1; depth <= 32; depth++) {
		/* Let the next_hop_add value = depth. Just for change. */
		next_hop_add = depth;

		status = rte_lpm_add(lpm, ip2, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);

		/* Check IP in first half of tbl24 which should be empty. */
		status = rte_lpm_lookup(lpm, ip1, &next_hop_return);
		TEST_LPM_ASSERT(status == -ENOENT);

		status = rte_lpm_lookup(lpm, ip2, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
			(next_hop_return == next_hop_add));

		ipx4 = vect_set_epi32(ip2, ip1, ip2, ip1);
		rte_lpm_lookupx4(lpm, ipx4, hop, UINT32_MAX);
		TEST_LPM_ASSERT(hop[0] == UINT32_MAX);
		TEST_LPM_ASSERT(hop[1] == next_hop_add);
		TEST_LPM_ASSERT(hop[2] == UINT32_MAX);
		TEST_LPM_ASSERT(hop[3] == next_hop_add);
	}

	/* Loop with rte_lpm_delete. */
	for (depth = 32; depth >= 1; depth--) {
		next_hop_add = (uint8_t) (depth - 1);

		status = rte_lpm_delete(lpm, ip2, depth);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm_lookup(lpm, ip2, &next_hop_return);

		if (depth != 1) {
			TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_add));
		} else {
			TEST_LPM_ASSERT(status == -ENOENT);
		}

		status = rte_lpm_lookup(lpm, ip1, &next_hop_return);
		TEST_LPM_ASSERT(status == -ENOENT);

		ipx4 = vect_set_epi32(ip1, ip1, ip2, ip2);
		rte_lpm_lookupx4(lpm, ipx4, hop, UINT32_MAX);
		if (depth != 1) {
			TEST_LPM_ASSERT(hop[0] == next_hop_add);
			TEST_LPM_ASSERT(hop[1] == next_hop_add);
		} else {
			TEST_LPM_ASSERT(hop[0] == UINT32_MAX);
			TEST_LPM_ASSERT(hop[1] == UINT32_MAX);
		}
		TEST_LPM_ASSERT(hop[2] == UINT32_MAX);
		TEST_LPM_ASSERT(hop[3] == UINT32_MAX);
	}

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * - Add & lookup to hit invalid TBL24 entry
 * - Add & lookup to hit valid TBL24 entry not extended
 * - Add & lookup to hit valid extended TBL24 entry with invalid TBL8 entry
 * - Add & lookup to hit valid extended TBL24 entry with valid TBL8 entry
 *
 */
int32_t
test9(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip, ip_1, ip_2;
	uint8_t depth, depth_1, depth_2;
	uint32_t next_hop_add, next_hop_add_1, next_hop_add_2, next_hop_return;
	int32_t status = 0;

	/* Add & lookup to hit invalid TBL24 entry */
	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	/* Add & lookup to hit valid TBL24 entry not extended */
	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 23;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	depth = 24;
	next_hop_add = 101;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	depth = 24;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	depth = 23;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	/* Add & lookup to hit valid extended TBL24 entry with invalid TBL8
	 * entry */
	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	ip = RTE_IPV4(128, 0, 0, 5);
	depth = 32;
	next_hop_add = 101;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	/* Add & lookup to hit valid extended TBL24 entry with valid TBL8
	 * entry */
	ip_1 = RTE_IPV4(128, 0, 0, 0);
	depth_1 = 25;
	next_hop_add_1 = 101;

	ip_2 = RTE_IPV4(128, 0, 0, 5);
	depth_2 = 32;
	next_hop_add_2 = 102;

	next_hop_return = 0;

	status = rte_lpm_add(lpm, ip_1, depth_1, next_hop_add_1);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip_1, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add_1));

	status = rte_lpm_add(lpm, ip_2, depth_2, next_hop_add_2);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip_2, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add_2));

	status = rte_lpm_delete(lpm, ip_2, depth_2);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip_2, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add_1));

	status = rte_lpm_delete(lpm, ip_1, depth_1);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip_1, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_free(lpm);

	return PASS;
}


/*
 * - Add rule that covers a TBL24 range previously invalid & lookup (& delete &
 *   lookup)
 * - Add rule that extends a TBL24 invalid entry & lookup (& delete & lookup)
 * - Add rule that extends a TBL24 valid entry & lookup for both rules (&
 *   delete & lookup)
 * - Add rule that updates the next hop in TBL24 & lookup (& delete & lookup)
 * - Add rule that updates the next hop in TBL8 & lookup (& delete & lookup)
 * - Delete a rule that is not present in the TBL24 & lookup
 * - Delete a rule that is not present in the TBL8 & lookup
 *
 */
int32_t
test10(void)
{

	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip, next_hop_add, next_hop_return;
	uint8_t depth;
	int32_t status = 0;

	/* Add rule that covers a TBL24 range previously invalid & lookup
	 * (& delete & lookup) */
	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 16;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 25;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	rte_lpm_delete_all(lpm);

	/* Add rule that extends a TBL24 valid entry & lookup for both rules
	 * (& delete & lookup) */

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	ip = RTE_IPV4(128, 0, 0, 10);
	depth = 32;
	next_hop_add = 101;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	ip = RTE_IPV4(128, 0, 0, 0);
	next_hop_add = 100;

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	ip = RTE_IPV4(128, 0, 0, 10);
	depth = 32;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	/* Add rule that updates the next hop in TBL24 & lookup
	 * (& delete & lookup) */

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	next_hop_add = 101;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	/* Add rule that updates the next hop in TBL8 & lookup
	 * (& delete & lookup) */

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	next_hop_add = 101;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	/* Delete a rule that is not present in the TBL24 & lookup */

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status < 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_delete_all(lpm);

	/* Delete a rule that is not present in the TBL8 & lookup */

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 32;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status < 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Add two rules, lookup to hit the more specific one, lookup to hit the less
 * specific one delete the less specific rule and lookup previous values again;
 * add a more specific rule than the existing rule, lookup again
 *
 * */
int32_t
test11(void)
{

	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip, next_hop_add, next_hop_return;
	uint8_t depth;
	int32_t status = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	ip = RTE_IPV4(128, 0, 0, 10);
	depth = 32;
	next_hop_add = 101;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	ip = RTE_IPV4(128, 0, 0, 0);
	next_hop_add = 100;

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	ip = RTE_IPV4(128, 0, 0, 10);
	depth = 32;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Add an extended rule (i.e. depth greater than 24, lookup (hit), delete,
 * lookup (miss) in a for loop of 1000 times. This will check tbl8 extension
 * and contraction.
 *
 * */

int32_t
test12(void)
{
	xmm_t ipx4;
	uint32_t hop[4];
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip, i, next_hop_add, next_hop_return;
	uint8_t depth;
	int32_t status = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	for (i = 0; i < 1000; i++) {
		status = rte_lpm_add(lpm, ip, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_add));

		ipx4 = vect_set_epi32(ip, ip + 1, ip, ip - 1);
		rte_lpm_lookupx4(lpm, ipx4, hop, UINT32_MAX);
		TEST_LPM_ASSERT(hop[0] == UINT32_MAX);
		TEST_LPM_ASSERT(hop[1] == next_hop_add);
		TEST_LPM_ASSERT(hop[2] == UINT32_MAX);
		TEST_LPM_ASSERT(hop[3] == next_hop_add);

		status = rte_lpm_delete(lpm, ip, depth);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT(status == -ENOENT);
	}

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Add a rule to tbl24, lookup (hit), then add a rule that will extend this
 * tbl24 entry, lookup (hit). delete the rule that caused the tbl24 extension,
 * lookup (miss) and repeat for loop of 1000 times. This will check tbl8
 * extension and contraction.
 *
 * */

int32_t
test13(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	uint32_t ip, i, next_hop_add_1, next_hop_add_2, next_hop_return;
	uint8_t depth;
	int32_t status = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	ip = RTE_IPV4(128, 0, 0, 0);
	depth = 24;
	next_hop_add_1 = 100;

	status = rte_lpm_add(lpm, ip, depth, next_hop_add_1);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add_1));

	depth = 32;
	next_hop_add_2 = 101;

	for (i = 0; i < 1000; i++) {
		status = rte_lpm_add(lpm, ip, depth, next_hop_add_2);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_add_2));

		status = rte_lpm_delete(lpm, ip, depth);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_add_1));
	}

	depth = 24;

	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * For TBL8 extension exhaustion. Add 512 rules that require a tbl8 extension.
 * No more tbl8 extensions will be allowed. Now add one more rule that required
 * a tbl8 extension and get fail.
 * */
int32_t
test14(void)
{

	/* We only use depth = 32 in the loop below so we must make sure
	 * that we have enough storage for all rules at that depth*/

	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = 256 * 32;
	config.number_tbl8s = 512;
	config.flags = 0;
	uint32_t ip, next_hop_base, next_hop_return;
	uint8_t depth;
	int32_t status = 0;
	xmm_t ipx4;
	uint32_t hop[4];

	/* Add enough space for 256 rules for every depth */
	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	depth = 32;
	next_hop_base = 100;
	ip = RTE_IPV4(0, 0, 0, 0);

	/* Add 256 rules that require a tbl8 extension */
	for (; ip <= RTE_IPV4(0, 1, 255, 0); ip += 256) {
		status = rte_lpm_add(lpm, ip, depth, next_hop_base + ip);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_base + ip));

		ipx4 = vect_set_epi32(ip + 3, ip + 2, ip + 1, ip);
		rte_lpm_lookupx4(lpm, ipx4, hop, UINT32_MAX);
		TEST_LPM_ASSERT(hop[0] == next_hop_base + ip);
		TEST_LPM_ASSERT(hop[1] == UINT32_MAX);
		TEST_LPM_ASSERT(hop[2] == UINT32_MAX);
		TEST_LPM_ASSERT(hop[3] == UINT32_MAX);
	}

	/* All tbl8 extensions have been used above. Try to add one more and
	 * we get a fail */
	ip = RTE_IPV4(1, 0, 0, 0);
	depth = 32;

	status = rte_lpm_add(lpm, ip, depth, next_hop_base + ip);
	TEST_LPM_ASSERT(status < 0);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Sequence of operations for find existing lpm table
 *
 *  - create table
 *  - find existing table: hit
 *  - find non-existing table: miss
 *
 */
int32_t
test15(void)
{
	struct rte_lpm *lpm = NULL, *result = NULL;
	struct rte_lpm_config config;

	config.max_rules = 256 * 32;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* Create lpm  */
	lpm = rte_lpm_create("lpm_find_existing", SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Try to find existing lpm */
	result = rte_lpm_find_existing("lpm_find_existing");
	TEST_LPM_ASSERT(result == lpm);

	/* Try to find non-existing lpm */
	result = rte_lpm_find_existing("lpm_find_non_existing");
	TEST_LPM_ASSERT(result == NULL);

	/* Cleanup. */
	rte_lpm_delete_all(lpm);
	rte_lpm_free(lpm);

	return PASS;
}

/*
 * test failure condition of overloading the tbl8 so no more will fit
 * Check we get an error return value in that case
 */
int32_t
test16(void)
{
	uint32_t ip;
	struct rte_lpm_config config;

	config.max_rules = 256 * 32;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	struct rte_lpm *lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);

	/* ip loops through all possibilities for top 24 bits of address */
	for (ip = 0; ip < 0xFFFFFF; ip++) {
		/* add an entry within a different tbl8 each time, since
		 * depth >24 and the top 24 bits are different */
		if (rte_lpm_add(lpm, (ip << 8) + 0xF0, 30, 0) < 0)
			break;
	}

	if (ip != NUMBER_TBL8S) {
		printf("Error, unexpected failure with filling tbl8 groups\n");
		printf("Failed after %u additions, expected after %u\n",
				(unsigned)ip, (unsigned)NUMBER_TBL8S);
	}

	rte_lpm_free(lpm);
	return 0;
}

/*
 * Test for overwriting of tbl8:
 *  - add rule /32 and lookup
 *  - add new rule /24 and lookup
 *	- add third rule /25 and lookup
 *	- lookup /32 and /24 rule to ensure the table has not been overwritten.
 */
int32_t
test17(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;
	const uint32_t ip_10_32 = RTE_IPV4(10, 10, 10, 2);
	const uint32_t ip_10_24 = RTE_IPV4(10, 10, 10, 0);
	const uint32_t ip_20_25 = RTE_IPV4(10, 10, 20, 2);
	const uint8_t d_ip_10_32 = 32,
			d_ip_10_24 = 24,
			d_ip_20_25 = 25;
	const uint32_t next_hop_ip_10_32 = 100,
			next_hop_ip_10_24 = 105,
			next_hop_ip_20_25 = 111;
	uint32_t next_hop_return = 0;
	int32_t status = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	if ((status = rte_lpm_add(lpm, ip_10_32, d_ip_10_32,
			next_hop_ip_10_32)) < 0)
		return -1;

	status = rte_lpm_lookup(lpm, ip_10_32, &next_hop_return);
	uint32_t test_hop_10_32 = next_hop_return;
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_32);

	if ((status = rte_lpm_add(lpm, ip_10_24, d_ip_10_24,
			next_hop_ip_10_24)) < 0)
			return -1;

	status = rte_lpm_lookup(lpm, ip_10_24, &next_hop_return);
	uint32_t test_hop_10_24 = next_hop_return;
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_24);

	if ((status = rte_lpm_add(lpm, ip_20_25, d_ip_20_25,
			next_hop_ip_20_25)) < 0)
		return -1;

	status = rte_lpm_lookup(lpm, ip_20_25, &next_hop_return);
	uint32_t test_hop_20_25 = next_hop_return;
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_20_25);

	if (test_hop_10_32 == test_hop_10_24) {
		printf("Next hop return equal\n");
		return -1;
	}

	if (test_hop_10_24 == test_hop_20_25) {
		printf("Next hop return equal\n");
		return -1;
	}

	status = rte_lpm_lookup(lpm, ip_10_32, &next_hop_return);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_32);

	status = rte_lpm_lookup(lpm, ip_10_24, &next_hop_return);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_24);

	rte_lpm_free(lpm);

	return PASS;
}

/*
 * Test for recycle of tbl8
 *  - step 1: add a rule with depth=28 (> 24)
 *  - step 2: add a rule with same 24-bit prefix and depth=23 (< 24)
 *  - step 3: delete the first rule
 *  - step 4: check tbl8 is freed
 *  - step 5: add a rule same as the first one (depth=28)
 *  - step 6: check same tbl8 is allocated
 *  - step 7: add a rule with same 24-bit prefix and depth=24
 *  - step 8: delete the rule (depth=28) added in step 5
 *  - step 9: check tbl8 is freed
 *  - step 10: add a rule with same 24-bit prefix and depth = 28
 *  - setp 11: check same tbl8 is allocated again
 */
int32_t
test18(void)
{
#define group_idx next_hop
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;
	uint32_t ip, next_hop;
	uint8_t depth;
	uint32_t tbl8_group_index;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	ip = RTE_IPV4(192, 168, 100, 100);
	depth = 28;
	next_hop = 1;
	rte_lpm_add(lpm, ip, depth, next_hop);

	TEST_LPM_ASSERT(lpm->tbl24[ip>>8].valid_group);
	tbl8_group_index = lpm->tbl24[ip>>8].group_idx;

	depth = 23;
	next_hop = 2;
	rte_lpm_add(lpm, ip, depth, next_hop);
	TEST_LPM_ASSERT(lpm->tbl24[ip>>8].valid_group);

	depth = 28;
	rte_lpm_delete(lpm, ip, depth);

	TEST_LPM_ASSERT(!lpm->tbl24[ip>>8].valid_group);

	next_hop = 3;
	rte_lpm_add(lpm, ip, depth, next_hop);

	TEST_LPM_ASSERT(lpm->tbl24[ip>>8].valid_group);
	TEST_LPM_ASSERT(tbl8_group_index == lpm->tbl24[ip>>8].group_idx);

	depth = 24;
	next_hop = 4;
	rte_lpm_add(lpm, ip, depth, next_hop);
	TEST_LPM_ASSERT(lpm->tbl24[ip>>8].valid_group);

	depth = 28;
	rte_lpm_delete(lpm, ip, depth);

	TEST_LPM_ASSERT(!lpm->tbl24[ip>>8].valid_group);

	next_hop = 5;
	rte_lpm_add(lpm, ip, depth, next_hop);

	TEST_LPM_ASSERT(lpm->tbl24[ip>>8].valid_group);
	TEST_LPM_ASSERT(tbl8_group_index == lpm->tbl24[ip>>8].group_idx);

	rte_lpm_free(lpm);
#undef group_idx
	return PASS;
}

/*
 * rte_lpm_rcu_qsbr_add positive and negative tests.
 *  - Add RCU QSBR variable to LPM
 *  - Add another RCU QSBR variable to LPM
 *  - Check returns
 */
int32_t
test19(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;
	size_t sz;
	struct rte_rcu_qsbr *qsv;
	struct rte_rcu_qsbr *qsv2;
	int32_t status;
	struct rte_lpm_rcu_config rcu_cfg = {0};

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
	qsv = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
					RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	TEST_LPM_ASSERT(qsv != NULL);

	status = rte_rcu_qsbr_init(qsv, RTE_MAX_LCORE);
	TEST_LPM_ASSERT(status == 0);

	rcu_cfg.v = qsv;
	/* Invalid QSBR mode */
	rcu_cfg.mode = 2;
	status = rte_lpm_rcu_qsbr_add(lpm, &rcu_cfg);
	TEST_LPM_ASSERT(status != 0);

	rcu_cfg.mode = RTE_LPM_QSBR_MODE_DQ;
	/* Attach RCU QSBR to LPM table */
	status = rte_lpm_rcu_qsbr_add(lpm, &rcu_cfg);
	TEST_LPM_ASSERT(status == 0);

	/* Create and attach another RCU QSBR to LPM table */
	qsv2 = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
					RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	TEST_LPM_ASSERT(qsv2 != NULL);

	rcu_cfg.v = qsv2;
	rcu_cfg.mode = RTE_LPM_QSBR_MODE_SYNC;
	status = rte_lpm_rcu_qsbr_add(lpm, &rcu_cfg);
	TEST_LPM_ASSERT(status != 0);

	rte_lpm_free(lpm);
	rte_free(qsv);
	rte_free(qsv2);

	return PASS;
}

/*
 * rte_lpm_rcu_qsbr_add DQ mode functional test.
 * Reader and writer are in the same thread in this test.
 *  - Create LPM which supports 1 tbl8 group at max
 *  - Add RCU QSBR variable to LPM
 *  - Add a rule with depth=28 (> 24)
 *  - Register a reader thread (not a real thread)
 *  - Reader lookup existing rule
 *  - Writer delete the rule
 *  - Reader lookup the rule
 *  - Writer re-add the rule (no available tbl8 group)
 *  - Reader report quiescent state and unregister
 *  - Writer re-add the rule
 *  - Reader lookup the rule
 */
int32_t
test20(void)
{
	struct rte_lpm *lpm = NULL;
	struct rte_lpm_config config;
	size_t sz;
	struct rte_rcu_qsbr *qsv;
	int32_t status;
	uint32_t ip, next_hop, next_hop_return;
	uint8_t depth;
	struct rte_lpm_rcu_config rcu_cfg = {0};

	config.max_rules = MAX_RULES;
	config.number_tbl8s = 1;
	config.flags = 0;

	lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(1);
	qsv = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
				RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	TEST_LPM_ASSERT(qsv != NULL);

	status = rte_rcu_qsbr_init(qsv, 1);
	TEST_LPM_ASSERT(status == 0);

	rcu_cfg.v = qsv;
	rcu_cfg.mode = RTE_LPM_QSBR_MODE_DQ;
	/* Attach RCU QSBR to LPM table */
	status = rte_lpm_rcu_qsbr_add(lpm, &rcu_cfg);
	TEST_LPM_ASSERT(status == 0);

	ip = RTE_IPV4(192, 0, 2, 100);
	depth = 28;
	next_hop = 1;
	status = rte_lpm_add(lpm, ip, depth, next_hop);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(lpm->tbl24[ip>>8].valid_group);

	/* Register pseudo reader */
	status = rte_rcu_qsbr_thread_register(qsv, 0);
	TEST_LPM_ASSERT(status == 0);
	rte_rcu_qsbr_thread_online(qsv, 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop);

	/* Writer update */
	status = rte_lpm_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(!lpm->tbl24[ip>>8].valid);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status != 0);

	status = rte_lpm_add(lpm, ip, depth, next_hop);
	TEST_LPM_ASSERT(status != 0);

	/* Reader quiescent */
	rte_rcu_qsbr_quiescent(qsv, 0);

	status = rte_lpm_add(lpm, ip, depth, next_hop);
	TEST_LPM_ASSERT(status == 0);

	rte_rcu_qsbr_thread_offline(qsv, 0);
	status = rte_rcu_qsbr_thread_unregister(qsv, 0);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop);

	rte_lpm_free(lpm);
	rte_free(qsv);

	return PASS;
}

static struct rte_lpm *g_lpm;
static struct rte_rcu_qsbr *g_v;
static uint32_t g_ip = RTE_IPV4(192, 0, 2, 100);
static volatile uint8_t writer_done;
/* Report quiescent state interval every 1024 lookups. Larger critical
 * sections in reader will result in writer polling multiple times.
 */
#define QSBR_REPORTING_INTERVAL 1024
#define WRITER_ITERATIONS	512

/*
 * Reader thread using rte_lpm data structure with RCU.
 */
static int
test_lpm_rcu_qsbr_reader(void *arg)
{
	int i;
	uint32_t next_hop_return = 0;

	RTE_SET_USED(arg);
	/* Register this thread to report quiescent state */
	rte_rcu_qsbr_thread_register(g_v, 0);
	rte_rcu_qsbr_thread_online(g_v, 0);

	do {
		for (i = 0; i < QSBR_REPORTING_INTERVAL; i++)
			rte_lpm_lookup(g_lpm, g_ip, &next_hop_return);

		/* Update quiescent state */
		rte_rcu_qsbr_quiescent(g_v, 0);
	} while (!writer_done);

	rte_rcu_qsbr_thread_offline(g_v, 0);
	rte_rcu_qsbr_thread_unregister(g_v, 0);

	return 0;
}

/*
 * rte_lpm_rcu_qsbr_add sync mode functional test.
 * 1 Reader and 1 writer. They cannot be in the same thread in this test.
 *  - Create LPM which supports 1 tbl8 group at max
 *  - Add RCU QSBR variable with sync mode to LPM
 *  - Register a reader thread. Reader keeps looking up a specific rule.
 *  - Writer keeps adding and deleting a specific rule with depth=28 (> 24)
 */
int32_t
test21(void)
{
	struct rte_lpm_config config;
	size_t sz;
	int32_t status;
	uint32_t i, next_hop;
	uint8_t depth;
	struct rte_lpm_rcu_config rcu_cfg = {0};

	if (rte_lcore_count() < 2) {
		printf("Not enough cores for %s, expecting at least 2\n",
			__func__);
		return TEST_SKIPPED;
	}

	config.max_rules = MAX_RULES;
	config.number_tbl8s = 1;
	config.flags = 0;

	g_lpm = rte_lpm_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(g_lpm != NULL);

	/* Create RCU QSBR variable */
	sz = rte_rcu_qsbr_get_memsize(1);
	g_v = (struct rte_rcu_qsbr *)rte_zmalloc_socket(NULL, sz,
				RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	TEST_LPM_ASSERT(g_v != NULL);

	status = rte_rcu_qsbr_init(g_v, 1);
	TEST_LPM_ASSERT(status == 0);

	rcu_cfg.v = g_v;
	rcu_cfg.mode = RTE_LPM_QSBR_MODE_SYNC;
	/* Attach RCU QSBR to LPM table */
	status = rte_lpm_rcu_qsbr_add(g_lpm, &rcu_cfg);
	TEST_LPM_ASSERT(status == 0);

	writer_done = 0;
	/* Launch reader thread */
	rte_eal_remote_launch(test_lpm_rcu_qsbr_reader, NULL,
				rte_get_next_lcore(-1, 1, 0));

	depth = 28;
	next_hop = 1;
	status = rte_lpm_add(g_lpm, g_ip, depth, next_hop);
	if (status != 0) {
		printf("%s: Failed to add rule\n", __func__);
		goto error;
	}

	/* Writer update */
	for (i = 0; i < WRITER_ITERATIONS; i++) {
		status = rte_lpm_delete(g_lpm, g_ip, depth);
		if (status != 0) {
			printf("%s: Failed to delete rule at iteration %d\n",
				__func__, i);
			goto error;
		}

		status = rte_lpm_add(g_lpm, g_ip, depth, next_hop);
		if (status != 0) {
			printf("%s: Failed to add rule at iteration %d\n",
				__func__, i);
			goto error;
		}
	}

error:
	writer_done = 1;
	/* Wait until reader exited. */
	rte_eal_mp_wait_lcore();

	rte_lpm_free(g_lpm);
	rte_free(g_v);

	return (status == 0) ? PASS : -1;
}

/*
 * Do all unit tests.
 */

static int
test_lpm(void)
{
	unsigned i;
	int status, global_status = 0;

	for (i = 0; i < RTE_DIM(tests); i++) {
		status = tests[i]();
		if (status < 0) {
			printf("ERROR: LPM Test %u: FAIL\n", i);
			global_status = status;
		}
	}

	return global_status;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(lpm_autotest, test_lpm);
