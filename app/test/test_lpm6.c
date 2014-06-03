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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>

#include <time.h>

#include "test.h"

#ifdef RTE_LIBRTE_LPM

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_random.h>
#include <rte_branch_prediction.h>
#include <rte_ip.h>

#include "rte_lpm6.h"
#include "test_lpm6_routes.h"

#define TEST_LPM_ASSERT(cond) do {                                            \
	if (!(cond)) {                                                        \
		printf("Error at line %d: \n", __LINE__);                     \
		return -1;                                                    \
	}                                                                     \
} while(0)

typedef int32_t (* rte_lpm6_test)(void);

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
static int32_t test22(void);
static int32_t test23(void);
static int32_t test24(void);
static int32_t test25(void);
static int32_t test26(void);
static int32_t test27(void);
static int32_t perf_test(void);

rte_lpm6_test tests6[] = {
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
	test21,
	test22,
	test23,
	test24,
	test25,
	test26,
	test27,
	perf_test,
};

#define NUM_LPM6_TESTS                (sizeof(tests6)/sizeof(tests6[0]))
#define RTE_LPM6_TBL24_NUM_ENTRIES                             (1 << 24)
#define RTE_LPM6_LOOKUP_SUCCESS                               0x04000000
#define MAX_DEPTH                                                    128
#define MAX_RULES                                                1000000
#define NUMBER_TBL8S                                           (1 << 16)
#define MAX_NUM_TBL8S                                          (1 << 21)
#define PASS 0

static void
IPv6(uint8_t *ip, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5,
		uint8_t b6, uint8_t b7, uint8_t b8, uint8_t b9, uint8_t b10,
		uint8_t b11, uint8_t b12, uint8_t b13, uint8_t b14, uint8_t b15,
		uint8_t b16)
{
	ip[0] = b1;
	ip[1] = b2;
	ip[2] = b3;
	ip[3] = b4;
	ip[4] = b5;
	ip[5] = b6;
	ip[6] = b7;
	ip[7] = b8;
	ip[8] = b9;
	ip[9] = b10;
	ip[10] = b11;
	ip[11] = b12;
	ip[12] = b13;
	ip[13] = b14;
	ip[14] = b15;
	ip[15] = b16;
}

/*
 * Check that rte_lpm6_create fails gracefully for incorrect user input
 * arguments
 */
int32_t
test0(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm6_create: lpm name == NULL */
	lpm = rte_lpm6_create(NULL, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm == NULL);

	/* rte_lpm6_create: max_rules = 0 */
	/* Note: __func__ inserts the function name, in this case "test0". */
	config.max_rules = 0;
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm == NULL);

	/* socket_id < -1 is invalid */
	config.max_rules = MAX_RULES;
	lpm = rte_lpm6_create(__func__, -2, &config);
	TEST_LPM_ASSERT(lpm == NULL);

	/* rte_lpm6_create: number_tbl8s is bigger than the maximum */
	config.number_tbl8s = MAX_NUM_TBL8S + 1;
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm == NULL);

	/* rte_lpm6_create: config = NULL */
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, NULL);
	TEST_LPM_ASSERT(lpm == NULL);

	return PASS;
}

/*
 * Creates two different LPM tables. Tries to create a third one with the same
 * name as the first one and expects the create function to return the same
 * pointer.
 */
int32_t
test1(void)
{
	struct rte_lpm6 *lpm1 = NULL, *lpm2 = NULL, *lpm3 = NULL;
	struct rte_lpm6_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm6_create: lpm name == LPM1 */
	lpm1 = rte_lpm6_create("LPM1", SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm1 != NULL);

	/* rte_lpm6_create: lpm name == LPM2 */
	lpm2 = rte_lpm6_create("LPM2", SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm2 != NULL);

	/* rte_lpm6_create: lpm name == LPM2 */
	lpm3 = rte_lpm6_create("LPM1", SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm3 == lpm1);

	rte_lpm6_free(lpm1);
	rte_lpm6_free(lpm2);

	return PASS;
}

/*
 * Create lpm table then delete lpm table 100 times
 * Use a slightly different rules size each time
 */
int32_t
test2(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	int32_t i;

	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm6_free: Free NULL */
	for (i = 0; i < 100; i++) {
		config.max_rules = MAX_RULES - i;
		lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
		TEST_LPM_ASSERT(lpm != NULL);

		rte_lpm6_free(lpm);
	}

	/* Can not test free so return success */
	return PASS;
}

/*
 * Call rte_lpm6_free for NULL pointer user input. Note: free has no return and
 * therefore it is impossible to check for failure but this test is added to
 * increase function coverage metrics and to validate that freeing null does
 * not crash.
 */
int32_t
test3(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	rte_lpm6_free(lpm);
	rte_lpm6_free(NULL);
	return PASS;
}

/*
 * Check that rte_lpm6_add fails gracefully for incorrect user input arguments
 */
int32_t
test4(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;

	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth = 24, next_hop = 100;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm6_add: lpm == NULL */
	status = rte_lpm6_add(NULL, ip, depth, next_hop);
	TEST_LPM_ASSERT(status < 0);

	/*Create vaild lpm to use in rest of test. */
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm6_add: depth < 1 */
	status = rte_lpm6_add(lpm, ip, 0, next_hop);
	TEST_LPM_ASSERT(status < 0);

	/* rte_lpm6_add: depth > MAX_DEPTH */
	status = rte_lpm6_add(lpm, ip, (MAX_DEPTH + 1), next_hop);
	TEST_LPM_ASSERT(status < 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Check that rte_lpm6_delete fails gracefully for incorrect user input
 * arguments
 */
int32_t
test5(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth = 24;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm_delete: lpm == NULL */
	status = rte_lpm6_delete(NULL, ip, depth);
	TEST_LPM_ASSERT(status < 0);

	/*Create vaild lpm to use in rest of test. */
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm_delete: depth < 1 */
	status = rte_lpm6_delete(lpm, ip, 0);
	TEST_LPM_ASSERT(status < 0);

	/* rte_lpm_delete: depth > MAX_DEPTH */
	status = rte_lpm6_delete(lpm, ip, (MAX_DEPTH + 1));
	TEST_LPM_ASSERT(status < 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Check that rte_lpm6_lookup fails gracefully for incorrect user input
 * arguments
 */
int32_t
test6(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t next_hop_return = 0;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm6_lookup: lpm == NULL */
	status = rte_lpm6_lookup(NULL, ip, &next_hop_return);
	TEST_LPM_ASSERT(status < 0);

	/*Create vaild lpm to use in rest of test. */
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm6_lookup: ip = NULL */
	status = rte_lpm6_lookup(lpm, NULL, &next_hop_return);
	TEST_LPM_ASSERT(status < 0);

	/* rte_lpm6_lookup: next_hop = NULL */
	status = rte_lpm6_lookup(lpm, ip, NULL);
	TEST_LPM_ASSERT(status < 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Checks that rte_lpm6_lookup_bulk_func fails gracefully for incorrect user
 * input arguments
 */
int32_t
test7(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[10][16];
	int16_t next_hop_return[10];
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm6_lookup: lpm == NULL */
	status = rte_lpm6_lookup_bulk_func(NULL, ip, next_hop_return, 10);
	TEST_LPM_ASSERT(status < 0);

	/*Create vaild lpm to use in rest of test. */
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm6_lookup: ip = NULL */
	status = rte_lpm6_lookup_bulk_func(lpm, NULL, next_hop_return, 10);
	TEST_LPM_ASSERT(status < 0);

	/* rte_lpm6_lookup: next_hop = NULL */
	status = rte_lpm6_lookup_bulk_func(lpm, ip, NULL, 10);
	TEST_LPM_ASSERT(status < 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Checks that rte_lpm6_delete_bulk_func fails gracefully for incorrect user
 * input arguments
 */
int32_t
test8(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[10][16];
	uint8_t depth[10];
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* rte_lpm6_delete: lpm == NULL */
	status = rte_lpm6_delete_bulk_func(NULL, ip, depth, 10);
	TEST_LPM_ASSERT(status < 0);

	/*Create vaild lpm to use in rest of test. */
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* rte_lpm6_delete: ip = NULL */
	status = rte_lpm6_delete_bulk_func(lpm, NULL, depth, 10);
	TEST_LPM_ASSERT(status < 0);

	/* rte_lpm6_delete: next_hop = NULL */
	status = rte_lpm6_delete_bulk_func(lpm, ip, NULL, 10);
	TEST_LPM_ASSERT(status < 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Call add, lookup and delete for a single rule with depth < 24.
 * Check all the combinations for the first three bytes that result in a hit.
 * Delete the rule and check that the same test returs a miss.
 */
int32_t
test9(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth = 16, next_hop_add = 100, next_hop_return = 0;
	int32_t status = 0;
	uint8_t i;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	for (i = 0; i < UINT8_MAX; i++) {
		ip[2] = i;
		status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));
	}

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	for (i = 0; i < UINT8_MAX; i++) {
		ip[2] = i;
		status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT(status == -ENOENT);
	}

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Adds max_rules + 1 and expects a failure. Deletes a rule, then adds
 * another one and expects success.
 */
int32_t
test10(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth, next_hop_add = 100;
	int32_t status = 0;
	int i;

	config.max_rules = 127;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	for (i = 1; i < 128; i++) {
		depth = (uint8_t)i;
		status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);
	}

	depth = 128;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == -ENOSPC);

	depth = 127;
	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	depth = 128;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Creates an LPM table with a small number of tbl8s and exhaust them in the
 * middle of the process of creating a rule.
 */
int32_t
test11(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth, next_hop_add = 100;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = 16;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	depth = 128;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	ip[0] = 1;
	depth = 25;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	depth = 33;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	depth = 41;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	depth = 49;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == -ENOSPC);

	depth = 41;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Creates an LPM table with a small number of tbl8s and exhaust them in the
 * middle of the process of adding a rule when there is already an existing rule
 * in that position and needs to be extended.
 */
int32_t
test12(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth, next_hop_add = 100;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = 16;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	depth = 128;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	ip[0] = 1;
	depth = 41;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	depth = 49;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == -ENOSPC);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Creates an LPM table with max_rules = 2 and tries to add 3 rules.
 * Delete one of the rules and tries to add the third one again.
 */
int32_t
test13(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth, next_hop_add = 100;
	int32_t status = 0;

	config.max_rules = 2;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	depth = 1;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	depth = 2;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	depth = 3;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == -ENOSPC);

	depth = 2;
	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	depth = 3;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Add 2^16 routes with different first 16 bits and depth 25.
 * Add one more route with the same depth and check that results in a failure.
 * After that delete the last rule and create the one that was attempted to be
 * created. This checks tbl8 exhaustion.
 */
int32_t
test14(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth = 25, next_hop_add = 100;
	int32_t status = 0;
	int i, j;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	for (i = 0; i < 256; i++) {
		ip[0] = (uint8_t)i;
		for (j = 0; j < 256; j++) {
			ip[1] = (uint8_t)j;
			status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
			TEST_LPM_ASSERT(status == 0);
		}
	}

	ip[0] = 255;
	ip[1] = 255;
	ip[2] = 1;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == -ENOSPC);

	ip[0] = 255;
	ip[1] = 255;
	ip[2] = 0;
	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	ip[0] = 255;
	ip[1] = 255;
	ip[2] = 1;
	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Call add, lookup and delete for a single rule with depth = 24
 */
int32_t
test15(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth = 24, next_hop_add = 100, next_hop_return = 0;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Call add, lookup and delete for a single rule with depth > 24
 */
int32_t
test16(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[] = {12,12,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth = 128, next_hop_add = 100, next_hop_return = 0;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Use rte_lpm6_add to add rules which effect only the second half of the lpm
 * table. Use all possible depths ranging from 1..32. Set the next hop = to the
 * depth. Check lookup hit for on every add and check for lookup miss on the
 * first half of the lpm table after each add. Finally delete all rules going
 * backwards (i.e. from depth = 32 ..1) and carry out a lookup after each
 * delete. The lookup should return the next_hop_add value related to the
 * previous depth value (i.e. depth -1).
 */
int32_t
test17(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip1[] = {127,255,255,255,255,255,255,255,255,
			255,255,255,255,255,255,255};
	uint8_t ip2[] = {128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t depth, next_hop_add, next_hop_return;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Loop with rte_lpm6_add. */
	for (depth = 1; depth <= 128; depth++) {
		/* Let the next_hop_add value = depth. Just for change. */
		next_hop_add = depth;

		status = rte_lpm6_add(lpm, ip2, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);

		/* Check IP in first half of tbl24 which should be empty. */
		status = rte_lpm6_lookup(lpm, ip1, &next_hop_return);
		TEST_LPM_ASSERT(status == -ENOENT);

		status = rte_lpm6_lookup(lpm, ip2, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
			(next_hop_return == next_hop_add));
	}

	/* Loop with rte_lpm6_delete. */
	for (depth = 128; depth >= 1; depth--) {
		next_hop_add = (uint8_t) (depth - 1);

		status = rte_lpm6_delete(lpm, ip2, depth);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm6_lookup(lpm, ip2, &next_hop_return);

		if (depth != 1) {
			TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_add));
		}
		else {
			TEST_LPM_ASSERT(status == -ENOENT);
		}

		status = rte_lpm6_lookup(lpm, ip1, &next_hop_return);
		TEST_LPM_ASSERT(status == -ENOENT);
	}

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * - Add & lookup to hit invalid TBL24 entry
 * - Add & lookup to hit valid TBL24 entry not extended
 * - Add & lookup to hit valid extended TBL24 entry with invalid TBL8 entry
 * - Add & lookup to hit valid extended TBL24 entry with valid TBL8 entry
 */
int32_t
test18(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[16], ip_1[16], ip_2[16];
	uint8_t depth, depth_1, depth_2, next_hop_add, next_hop_add_1,
		next_hop_add_2, next_hop_return;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* Add & lookup to hit invalid TBL24 entry */
	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	/* Add & lookup to hit valid TBL24 entry not extended */
	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 23;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	depth = 24;
	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	depth = 24;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	depth = 23;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	/* Add & lookup to hit valid extended TBL24 entry with invalid TBL8
	 * entry.
	 */
	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	IPv6(ip, 128, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 32;
	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	/* Add & lookup to hit valid extended TBL24 entry with valid TBL8
	 * entry
	 */
	IPv6(ip_1, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth_1 = 25;
	next_hop_add_1 = 101;

	IPv6(ip_2, 128, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth_2 = 32;
	next_hop_add_2 = 102;

	next_hop_return = 0;

	status = rte_lpm6_add(lpm, ip_1, depth_1, next_hop_add_1);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip_1, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add_1));

	status = rte_lpm6_add(lpm, ip_2, depth_2, next_hop_add_2);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip_2, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add_2));

	status = rte_lpm6_delete(lpm, ip_2, depth_2);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip_2, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add_1));

	status = rte_lpm6_delete(lpm, ip_1, depth_1);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip_1, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_free(lpm);

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
 */
int32_t
test19(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[16];
	uint8_t depth, next_hop_add, next_hop_return;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* Add rule that covers a TBL24 range previously invalid & lookup
	 * (& delete & lookup)
	 */
	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 16;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 25;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	rte_lpm6_delete_all(lpm);

	/*
	 * Add rule that extends a TBL24 valid entry & lookup for both rules
	 * (& delete & lookup)
	 */

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip, 128, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 32;
	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	next_hop_add = 100;

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 24;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	IPv6(ip, 128, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 32;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	/*
	 * Add rule that updates the next hop in TBL24 & lookup
	 * (& delete & lookup)
	 */

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	/*
	 * Add rule that updates the next hop in TBL8 & lookup
	 * (& delete & lookup)
	 */

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	/* Delete a rule that is not present in the TBL24 & lookup */

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status < 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_delete_all(lpm);

	/* Delete a rule that is not present in the TBL8 & lookup */

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 32;
	next_hop_add = 100;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status < 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Add two rules, lookup to hit the more specific one, lookup to hit the less
 * specific one delete the less specific rule and lookup previous values again;
 * add a more specific rule than the existing rule, lookup again
 */
int32_t
test20(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[16];
	uint8_t depth, next_hop_add, next_hop_return;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 24;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10);
	depth = 128;
	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	next_hop_add = 100;

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT((status == 0) && (next_hop_return == next_hop_add));

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 24;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10);
	depth = 128;

	status = rte_lpm6_delete(lpm, ip, depth);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
	TEST_LPM_ASSERT(status == -ENOENT);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Adds 3 rules and look them up through the lookup_bulk function.
 * Includes in the lookup a fourth IP address that won't match
 * and checks that the result is as expected.
 */
int32_t
test21(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip_batch[4][16];
	uint8_t depth, next_hop_add;
	int16_t next_hop_return[4];
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	IPv6(ip_batch[0], 128, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 48;
	next_hop_add = 100;

	status = rte_lpm6_add(lpm, ip_batch[0], depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[1], 128, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 48;
	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip_batch[1], depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[2], 128, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 48;
	next_hop_add = 102;

	status = rte_lpm6_add(lpm, ip_batch[2], depth, next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[3], 128, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	status = rte_lpm6_lookup_bulk_func(lpm, ip_batch,
			next_hop_return, 4);
	TEST_LPM_ASSERT(status == 0 && next_hop_return[0] == 100
			&& next_hop_return[1] == 101 && next_hop_return[2] == 102
			&& next_hop_return[3] == -1);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Adds 5 rules and look them up.
 * Use the delete_bulk function to delete two of them. Lookup again.
 * Use the delete_bulk function to delete one more. Lookup again.
 * Use the delete_bulk function to delete two more, one invalid. Lookup again.
 * Use the delete_bulk function to delete the remaining one. Lookup again.
 */
int32_t
test22(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip_batch[5][16];
	uint8_t depth[5], next_hop_add;
	int16_t next_hop_return[5];
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Adds 5 rules and look them up */

	IPv6(ip_batch[0], 128, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth[0] = 48;
	next_hop_add = 101;

	status = rte_lpm6_add(lpm, ip_batch[0], depth[0], next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[1], 128, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth[1] = 48;
	next_hop_add = 102;

	status = rte_lpm6_add(lpm, ip_batch[1], depth[1], next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[2], 128, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth[2] = 48;
	next_hop_add = 103;

	status = rte_lpm6_add(lpm, ip_batch[2], depth[2], next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[3], 128, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth[3] = 48;
	next_hop_add = 104;

	status = rte_lpm6_add(lpm, ip_batch[3], depth[3], next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[4], 128, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth[4] = 48;
	next_hop_add = 105;

	status = rte_lpm6_add(lpm, ip_batch[4], depth[4], next_hop_add);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup_bulk_func(lpm, ip_batch,
			next_hop_return, 5);
	TEST_LPM_ASSERT(status == 0 && next_hop_return[0] == 101
			&& next_hop_return[1] == 102 && next_hop_return[2] == 103
			&& next_hop_return[3] == 104 && next_hop_return[4] == 105);

	/* Use the delete_bulk function to delete two of them. Lookup again */

	status = rte_lpm6_delete_bulk_func(lpm, &ip_batch[0], depth, 2);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup_bulk_func(lpm, ip_batch,
			next_hop_return, 5);
	TEST_LPM_ASSERT(status == 0 && next_hop_return[0] == -1
			&& next_hop_return[1] == -1 && next_hop_return[2] == 103
			&& next_hop_return[3] == 104 && next_hop_return[4] == 105);

	/* Use the delete_bulk function to delete one more. Lookup again */

	status = rte_lpm6_delete_bulk_func(lpm, &ip_batch[2], depth, 1);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup_bulk_func(lpm, ip_batch,
			next_hop_return, 5);
	TEST_LPM_ASSERT(status == 0 && next_hop_return[0] == -1
			&& next_hop_return[1] == -1 && next_hop_return[2] == -1
			&& next_hop_return[3] == 104 && next_hop_return[4] == 105);

	/* Use the delete_bulk function to delete two, one invalid. Lookup again */

	IPv6(ip_batch[4], 128, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	status = rte_lpm6_delete_bulk_func(lpm, &ip_batch[3], depth, 2);
	TEST_LPM_ASSERT(status == 0);

	IPv6(ip_batch[4], 128, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	status = rte_lpm6_lookup_bulk_func(lpm, ip_batch,
			next_hop_return, 5);
	TEST_LPM_ASSERT(status == 0 && next_hop_return[0] == -1
			&& next_hop_return[1] == -1 && next_hop_return[2] == -1
			&& next_hop_return[3] == -1 && next_hop_return[4] == 105);

	/* Use the delete_bulk function to delete the remaining one. Lookup again */

	status = rte_lpm6_delete_bulk_func(lpm, &ip_batch[4], depth, 1);
	TEST_LPM_ASSERT(status == 0);

	status = rte_lpm6_lookup_bulk_func(lpm, ip_batch,
			next_hop_return, 5);
	TEST_LPM_ASSERT(status == 0 && next_hop_return[0] == -1
			&& next_hop_return[1] == -1 && next_hop_return[2] == -1
			&& next_hop_return[3] == -1 && next_hop_return[4] == -1);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Add an extended rule (i.e. depth greater than 24, lookup (hit), delete,
 * lookup (miss) in a for loop of 1000 times. This will check tbl8 extension
 * and contraction.
 */
int32_t
test23(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint32_t i;
	uint8_t ip[16];
	uint8_t depth, next_hop_add, next_hop_return;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	IPv6(ip, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	depth = 128;
	next_hop_add = 100;

	for (i = 0; i < 1000; i++) {
		status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_add));

		status = rte_lpm6_delete(lpm, ip, depth);
		TEST_LPM_ASSERT(status == 0);

		status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT(status == -ENOENT);
	}

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Sequence of operations for find existing lpm table
 *
 *  - create table
 *  - find existing table: hit
 *  - find non-existing table: miss
 */
int32_t
test24(void)
{
	struct rte_lpm6 *lpm = NULL, *result = NULL;
	struct rte_lpm6_config config;

	config.max_rules = 256 * 32;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	/* Create lpm  */
	lpm = rte_lpm6_create("lpm_find_existing", SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Try to find existing lpm */
	result = rte_lpm6_find_existing("lpm_find_existing");
	TEST_LPM_ASSERT(result == lpm);

	/* Try to find non-existing lpm */
	result = rte_lpm6_find_existing("lpm_find_non_existing");
	TEST_LPM_ASSERT(result == NULL);

	/* Cleanup. */
	rte_lpm6_delete_all(lpm);
	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Add a set of random routes with random depths.
 * Lookup different IP addresses that match the routes previously added.
 * Checks that the next hop is the expected one.
 * The routes, IP addresses and expected result for every case have been
 * precalculated by using a python script and stored in a .h file.
 */
int32_t
test25(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip[16];
	uint32_t i;
	uint8_t depth, next_hop_add, next_hop_return, next_hop_expected;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	for (i = 0; i < 1000; i++) {
		memcpy(ip, large_route_table[i].ip, 16);
		depth = large_route_table[i].depth;
		next_hop_add = large_route_table[i].next_hop;
		status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);
	}

	for (i = 0; i < 100000; i++) {
		memcpy(ip, large_ips_table[i].ip, 16);
		next_hop_expected = large_ips_table[i].next_hop;

		status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
		TEST_LPM_ASSERT((status == 0) &&
				(next_hop_return == next_hop_expected));
	}

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Test for overwriting of tbl8:
 *  - add rule /32 and lookup
 *  - add new rule /24 and lookup
 *	- add third rule /25 and lookup
 *	- lookup /32 and /24 rule to ensure the table has not been overwritten.
 */
int32_t
test26(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint8_t ip_10_32[] = {10, 10, 10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint8_t ip_10_24[] = {10, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint8_t ip_20_25[] = {10, 10, 20, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint8_t d_ip_10_32 = 32;
	uint8_t	d_ip_10_24 = 24;
	uint8_t	d_ip_20_25 = 25;
	uint8_t next_hop_ip_10_32 = 100;
	uint8_t	next_hop_ip_10_24 = 105;
	uint8_t	next_hop_ip_20_25 = 111;
	uint8_t next_hop_return = 0;
	int32_t status = 0;

	config.max_rules = MAX_RULES;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	if ((status = rte_lpm6_add(lpm, ip_10_32, d_ip_10_32,
			next_hop_ip_10_32)) < 0)
		return -1;

	status = rte_lpm6_lookup(lpm, ip_10_32, &next_hop_return);
	uint8_t test_hop_10_32 = next_hop_return;
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_32);

	if ((status = rte_lpm6_add(lpm, ip_10_24, d_ip_10_24,
			next_hop_ip_10_24)) < 0)
			return -1;

	status = rte_lpm6_lookup(lpm, ip_10_24, &next_hop_return);
	uint8_t test_hop_10_24 = next_hop_return;
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_24);

	if ((status = rte_lpm6_add(lpm, ip_20_25, d_ip_20_25,
			next_hop_ip_20_25)) < 0)
		return -1;

	status = rte_lpm6_lookup(lpm, ip_20_25, &next_hop_return);
	uint8_t test_hop_20_25 = next_hop_return;
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_20_25);

	if (test_hop_10_32 == test_hop_10_24) {
		printf("Next hop return equal\n");
		return -1;
	}

	if (test_hop_10_24 == test_hop_20_25){
		printf("Next hop return equal\n");
		return -1;
	}

	status = rte_lpm6_lookup(lpm, ip_10_32, &next_hop_return);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_32);

	status = rte_lpm6_lookup(lpm, ip_10_24, &next_hop_return);
	TEST_LPM_ASSERT(status == 0);
	TEST_LPM_ASSERT(next_hop_return == next_hop_ip_10_24);

	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Add a rule that reaches the end of the tree.
 * Add a rule that is more generic than the first one.
 * Check every possible combination that produces a match for the second rule.
 * This tests tbl expansion.
 */
int32_t
test27(void)
{
		struct rte_lpm6 *lpm = NULL;
		struct rte_lpm6_config config;
		uint8_t ip[] = {128,128,128,128,128,128,128,128,128,128,128,128,128,128,0,0};
		uint8_t depth = 128, next_hop_add = 100, next_hop_return;
		int32_t status = 0;
		int i, j;

		config.max_rules = MAX_RULES;
		config.number_tbl8s = NUMBER_TBL8S;
		config.flags = 0;

		lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
		TEST_LPM_ASSERT(lpm != NULL);

		depth = 128;
		next_hop_add = 128;
		status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);

		depth = 112;
		next_hop_add = 112;
		status = rte_lpm6_add(lpm, ip, depth, next_hop_add);
		TEST_LPM_ASSERT(status == 0);

		for (i = 0; i < 256; i++) {
			ip[14] = (uint8_t)i;
			for (j = 0; j < 256; j++) {
				ip[15] = (uint8_t)j;
				status = rte_lpm6_lookup(lpm, ip, &next_hop_return);
				if (i == 0 && j == 0)
					TEST_LPM_ASSERT(status == 0 && next_hop_return == 128);
				else
					TEST_LPM_ASSERT(status == 0 && next_hop_return == 112);
				}
		}

		rte_lpm6_free(lpm);

		return PASS;
}

/*
 * Lookup performance test
 */

#define ITERATIONS (1 << 10)
#define BATCH_SIZE 100000

static void
print_route_distribution(const struct rules_tbl_entry *table, uint32_t n)
{
	unsigned i, j;

	printf("Route distribution per prefix width: \n");
	printf("DEPTH    QUANTITY (PERCENT)\n");
	printf("--------------------------- \n");

	/* Count depths. */
	for(i = 1; i <= 128; i++) {
		unsigned depth_counter = 0;
		double percent_hits;

		for (j = 0; j < n; j++)
			if (table[j].depth == (uint8_t) i)
				depth_counter++;

		percent_hits = ((double)depth_counter)/((double)n) * 100;
		printf("%.2u%15u (%.2f)\n", i, depth_counter, percent_hits);
	}
	printf("\n");
}

int32_t
perf_test(void)
{
	struct rte_lpm6 *lpm = NULL;
	struct rte_lpm6_config config;
	uint64_t begin, total_time;
	unsigned i, j;
	uint8_t next_hop_add = 0xAA, next_hop_return = 0;
	int status = 0;
	int64_t count = 0;

	config.max_rules = 1000000;
	config.number_tbl8s = NUMBER_TBL8S;
	config.flags = 0;

	rte_srand(rte_rdtsc());

	printf("No. routes = %u\n", (unsigned) NUM_ROUTE_ENTRIES);

	print_route_distribution(large_route_table, (uint32_t) NUM_ROUTE_ENTRIES);

	lpm = rte_lpm6_create(__func__, SOCKET_ID_ANY, &config);
	TEST_LPM_ASSERT(lpm != NULL);

	/* Measure add. */
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		if (rte_lpm6_add(lpm, large_route_table[i].ip,
				large_route_table[i].depth, next_hop_add) == 0)
			status++;
	}
	/* End Timer. */
	total_time = rte_rdtsc() - begin;

	printf("Unique added entries = %d\n", status);
	printf("Average LPM Add: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	/* Measure single Lookup */
	total_time = 0;
	count = 0;

	for (i = 0; i < ITERATIONS; i ++) {
		begin = rte_rdtsc();

		for (j = 0; j < NUM_IPS_ENTRIES; j ++) {
			if (rte_lpm6_lookup(lpm, large_ips_table[j].ip,
					&next_hop_return) != 0)
				count++;
		}

		total_time += rte_rdtsc() - begin;

	}
	printf("Average LPM Lookup: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Measure bulk Lookup */
	total_time = 0;
	count = 0;

	uint8_t ip_batch[NUM_IPS_ENTRIES][16];
	int16_t next_hops[NUM_IPS_ENTRIES];

	for (i = 0; i < NUM_IPS_ENTRIES; i++)
		memcpy(ip_batch[i], large_ips_table[i].ip, 16);

	for (i = 0; i < ITERATIONS; i ++) {

		/* Lookup per batch */
		begin = rte_rdtsc();
		rte_lpm6_lookup_bulk_func(lpm, ip_batch, next_hops, NUM_IPS_ENTRIES);
		total_time += rte_rdtsc() - begin;

		for (j = 0; j < NUM_IPS_ENTRIES; j++)
			if (next_hops[j] < 0)
				count++;
	}
	printf("BULK LPM Lookup: %.1f cycles (fails = %.1f%%)\n",
			(double)total_time / ((double)ITERATIONS * BATCH_SIZE),
			(count * 100.0) / (double)(ITERATIONS * BATCH_SIZE));

	/* Delete */
	status = 0;
	begin = rte_rdtsc();

	for (i = 0; i < NUM_ROUTE_ENTRIES; i++) {
		/* rte_lpm_delete(lpm, ip, depth) */
		status += rte_lpm6_delete(lpm, large_route_table[i].ip,
				large_route_table[i].depth);
	}

	total_time += rte_rdtsc() - begin;

	printf("Average LPM Delete: %g cycles\n",
			(double)total_time / NUM_ROUTE_ENTRIES);

	rte_lpm6_delete_all(lpm);
	rte_lpm6_free(lpm);

	return PASS;
}

/*
 * Do all unit and performance tests.
 */
int
test_lpm6(void)
{
	unsigned i;
	int status = -1, global_status = 0;

	for (i = 0; i < NUM_LPM6_TESTS; i++) {
		status = tests6[i]();

		if (status < 0) {
			printf("ERROR: LPM Test %s: FAIL\n", RTE_STR(tests6[i]));
			global_status = status;
		}
	}

	return global_status;
}

#else /* RTE_LIBRTE_LPM */

int
test_lpm6(void)
{
        printf("The LPM library is not included in this build\n");
        return 0;
}

#endif /* RTE_LIBRTE_LPM */
