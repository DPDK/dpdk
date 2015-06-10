/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>

#include "test.h"

/*******************************************************************************
 * Hash function performance test configuration section. Each performance test
 * will be performed HASHTEST_ITERATIONS times.
 *
 * The three arrays below control what tests are performed. Every combination
 * from the array entries is tested.
 */
#define HASHTEST_ITERATIONS 1000000

static rte_hash_function hashtest_funcs[] = {rte_jhash, rte_hash_crc};
static uint32_t hashtest_initvals[] = {0};
static uint32_t hashtest_key_lens[] = {2, 4, 5, 6, 7, 8, 10, 11, 15, 16, 21, 31, 32, 33, 63, 64};
/******************************************************************************/

/*
 * To help print out name of hash functions.
 */
static const char *
get_hash_name(rte_hash_function f)
{
	if (f == rte_jhash)
		return "jhash";

	if (f == rte_hash_crc)
		return "rte_hash_crc";

	return "UnknownHash";
}

/*
 * Test a hash function.
 */
static void
run_hash_func_perf_test(rte_hash_function f, uint32_t init_val,
		uint32_t key_len)
{
	static uint8_t key[HASHTEST_ITERATIONS][RTE_HASH_KEY_LENGTH_MAX];
	uint64_t ticks, start, end;
	unsigned i, j;

	for (i = 0; i < HASHTEST_ITERATIONS; i++) {
		for (j = 0; j < key_len; j++)
			key[i][j] = (uint8_t) rte_rand();
	}

	start = rte_rdtsc();
	for (i = 0; i < HASHTEST_ITERATIONS; i++)
		f(key[i], key_len, init_val);
	end = rte_rdtsc();
	ticks = end - start;

	printf("%-12s, %-18u, %-13u, %.02f\n", get_hash_name(f), (unsigned) key_len,
			(unsigned) init_val, (double)ticks / HASHTEST_ITERATIONS);
}

/*
 * Test all hash functions.
 */
static void
run_hash_func_perf_tests(void)
{
	unsigned i, j, k;

	printf(" *** Hash function performance test results ***\n");
	printf(" Number of iterations for each test = %d\n",
			HASHTEST_ITERATIONS);
	printf("Hash Func.  , Key Length (bytes), Initial value, Ticks/Op.\n");

	for (i = 0;
	     i < sizeof(hashtest_funcs) / sizeof(rte_hash_function);
	     i++) {
		for (j = 0;
		     j < sizeof(hashtest_initvals) / sizeof(uint32_t);
		     j++) {
			for (k = 0;
			     k < sizeof(hashtest_key_lens) / sizeof(uint32_t);
			     k++) {
				run_hash_func_perf_test(hashtest_funcs[i],
						hashtest_initvals[j],
						hashtest_key_lens[k]);
			}
		}
	}
}

static int
test_hash_functions(void)
{
	run_hash_func_perf_tests();

	return 0;
}

static struct test_command hash_functions_cmd = {
	.command = "hash_functions_autotest",
	.callback = test_hash_functions,
};
REGISTER_TEST_COMMAND(hash_functions_cmd);
