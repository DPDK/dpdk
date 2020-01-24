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

#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>

#include "test.h"

/*
 * Hash values calculated for key sizes from array "hashtest_key_lens"
 * and for initial values from array "hashtest_initvals.
 * Each key will be formed by increasing each byte by 1:
 * e.g.: key size = 4, key = 0x03020100
 *       key size = 8, key = 0x0706050403020100
 */
static uint32_t hash_values_jhash[2][12] = {{
	0x8ba9414b, 0xdf0d39c9,
	0xe4cf1d42, 0xd4ccb93c, 0x5e84eafc, 0x21362cfe,
	0x2f4775ab, 0x9ff036cc, 0xeca51474, 0xbc9d6816,
	0x12926a31, 0x1c9fa888
},
{
	0x5c62c303, 0x1b8cf784,
	0x8270ac65, 0x05fa6668, 0x762df861, 0xda088f2f,
	0x59614cd4, 0x7a94f690, 0xdc1e4993, 0x30825494,
	0x91d0e462, 0x768087fc
}
};
static uint32_t hash_values_crc[2][12] = {{
	0x00000000, 0xf26b8303,
	0x91545164, 0x06040eb1, 0x9bb99201, 0xcc4c4fe4,
	0x14a90993, 0xf8a5dd8c, 0xcaa1ad0b, 0x7ac1e03e,
	0x43f44466, 0x4a11475e
},
{
	0xbdfd3980, 0x70204542,
	0x98cd4c70, 0xd52c702f, 0x41fc0e1c, 0x3905f65c,
	0x94bff47f, 0x1bab102d, 0xf4a2c645, 0xbf441539,
	0x789c104f, 0x53028d3e
}
};

/*******************************************************************************
 * Hash function performance test configuration section. Each performance test
 * will be performed HASHTEST_ITERATIONS times.
 *
 * The three arrays below control what tests are performed. Every combination
 * from the array entries is tested.
 */
#define HASHTEST_ITERATIONS 1000000
#define MAX_KEYSIZE 64
static rte_hash_function hashtest_funcs[] = {rte_jhash, rte_hash_crc};
static uint32_t hashtest_initvals[] = {0, 0xdeadbeef};
static uint32_t hashtest_key_lens[] = {
	1, 2,                 /* Unusual key sizes */
	4, 8, 16, 32, 48, 64, /* standard key sizes */
	9,                    /* IPv4 SRC + DST + protocol, unpadded */
	13,                   /* IPv4 5-tuple, unpadded */
	37,                   /* IPv6 5-tuple, unpadded */
	40                    /* IPv6 5-tuple, padded to 8-byte boundary */
};
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
run_hash_func_perf_test(uint32_t key_len, uint32_t init_val,
		rte_hash_function f)
{
	static uint8_t key[HASHTEST_ITERATIONS][MAX_KEYSIZE];
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

	for (i = 0; i < RTE_DIM(hashtest_initvals); i++) {
		for (j = 0; j < RTE_DIM(hashtest_key_lens); j++) {
			for (k = 0; k < RTE_DIM(hashtest_funcs); k++) {
				run_hash_func_perf_test(hashtest_key_lens[j],
						hashtest_initvals[i],
						hashtest_funcs[k]);
			}
		}
	}
}

/*
 * Verify that hash functions return what they are expected to return
 * (using precalculated values stored above)
 */
static int
verify_precalculated_hash_func_tests(void)
{
	unsigned i, j;
	uint8_t key[64];
	uint32_t hash;

	for (i = 0; i < 64; i++)
		key[i] = (uint8_t) i;

	for (i = 0; i < RTE_DIM(hashtest_key_lens); i++) {
		for (j = 0; j < RTE_DIM(hashtest_initvals); j++) {
			hash = rte_jhash(key, hashtest_key_lens[i],
					 hashtest_initvals[j]);
			if (hash != hash_values_jhash[j][i]) {
				printf("jhash for %u bytes with initial value 0x%x."
				       "Expected 0x%x, but got 0x%x\n",
				       hashtest_key_lens[i], hashtest_initvals[j],
				       hash_values_jhash[j][i], hash);
				return -1;
			}

			hash = rte_hash_crc(key, hashtest_key_lens[i],
					hashtest_initvals[j]);
			if (hash != hash_values_crc[j][i]) {
				printf("CRC for %u bytes with initial value 0x%x."
				       "Expected 0x%x, but got 0x%x\n",
				       hashtest_key_lens[i], hashtest_initvals[j],
				       hash_values_crc[j][i], hash);
				return -1;
			}
		}
	}

	return 0;
}

/*
 * Verify that rte_jhash and rte_jhash_32b return the same
 */
static int
verify_jhash_32bits(void)
{
	unsigned i, j;
	uint8_t key[64];
	uint32_t hash, hash32;

	for (i = 0; i < 64; i++)
		key[i] = rand() & 0xff;

	for (i = 0; i < RTE_DIM(hashtest_key_lens); i++) {
		for (j = 0; j < RTE_DIM(hashtest_initvals); j++) {
			/* Key size must be multiple of 4 (32 bits) */
			if ((hashtest_key_lens[i] & 0x3) == 0) {
				hash = rte_jhash(key, hashtest_key_lens[i],
						hashtest_initvals[j]);
				/* Divide key length by 4 in rte_jhash for 32 bits */
				hash32 = rte_jhash_32b((const unaligned_uint32_t *)key,
						hashtest_key_lens[i] >> 2,
						hashtest_initvals[j]);
				if (hash != hash32) {
					printf("rte_jhash returns different value (0x%x)"
					       "than rte_jhash_32b (0x%x)\n",
					       hash, hash32);
					return -1;
				}
			}
		}
	}

	return 0;
}

/*
 * Verify that rte_jhash and rte_jhash_1word, rte_jhash_2words
 * and rte_jhash_3words return the same
 */
static int
verify_jhash_words(void)
{
	unsigned i;
	uint32_t key[3];
	uint32_t hash, hash_words;

	for (i = 0; i < 3; i++)
		key[i] = rand();

	/* Test rte_jhash_1word */
	hash = rte_jhash(key, 4, 0);
	hash_words = rte_jhash_1word(key[0], 0);
	if (hash != hash_words) {
		printf("rte_jhash returns different value (0x%x)"
		       "than rte_jhash_1word (0x%x)\n",
		       hash, hash_words);
		return -1;
	}
	/* Test rte_jhash_2words */
	hash = rte_jhash(key, 8, 0);
	hash_words = rte_jhash_2words(key[0], key[1], 0);
	if (hash != hash_words) {
		printf("rte_jhash returns different value (0x%x)"
		       "than rte_jhash_2words (0x%x)\n",
		       hash, hash_words);
		return -1;
	}
	/* Test rte_jhash_3words */
	hash = rte_jhash(key, 12, 0);
	hash_words = rte_jhash_3words(key[0], key[1], key[2], 0);
	if (hash != hash_words) {
		printf("rte_jhash returns different value (0x%x)"
		       "than rte_jhash_3words (0x%x)\n",
		       hash, hash_words);
		return -1;
	}

	return 0;
}

/*
 * Run all functional tests for hash functions
 */
static int
run_hash_func_tests(void)
{
	if (verify_precalculated_hash_func_tests() != 0)
		return -1;

	if (verify_jhash_32bits() != 0)
		return -1;

	if (verify_jhash_words() != 0)
		return -1;

	return 0;

}

static int
test_hash_functions(void)
{
	if (run_hash_func_tests() != 0)
		return -1;

	run_hash_func_perf_tests();

	return 0;
}

REGISTER_TEST_COMMAND(hash_functions_autotest, test_hash_functions);
