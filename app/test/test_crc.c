/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2020 Intel Corporation
 */

#include "test.h"

#include <rte_hexdump.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_net_crc.h>

#define CRC_VEC_LEN        32
#define CRC32_VEC_LEN1     1512
#define CRC32_VEC_LEN2     348
#define CRC16_VEC_LEN1     12
#define CRC16_VEC_LEN2     2

/* CRC test vector */
static const uint8_t crc_vec[CRC_VEC_LEN] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'A', 'B', 'C', 'D',
	'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
};

/* 32-bit CRC test vector */
static const uint8_t crc32_vec1[12] = {
	0xBE, 0xD7, 0x23, 0x47, 0x6B, 0x8F,
	0xB3, 0x14, 0x5E, 0xFB, 0x35, 0x59,
};

/* 16-bit CRC test vector 1 */
static const uint8_t crc16_vec1[CRC16_VEC_LEN1] = {
	0x0D, 0x01, 0x01, 0x23, 0x45, 0x67,
	0x89, 0x01, 0x23, 0x45, 0x00, 0x01,
};

/* 16-bit CRC test vector 2 */
static const uint8_t crc16_vec2[CRC16_VEC_LEN2] = {
	0x03, 0x3f,
};
/** CRC results */
static const uint32_t crc32_vec_res = 0xb491aab4;
static const uint32_t crc32_vec1_res = 0xac54d294;
static const uint32_t crc32_vec2_res = 0xefaae02f;
static const uint32_t crc16_vec_res = 0x6bec;
static const uint32_t crc16_vec1_res = 0x8cdd;
static const uint32_t crc16_vec2_res = 0xec5b;

static int
crc_all_algs(const char *desc, enum rte_net_crc_type type,
	const uint8_t *data, int data_len, uint32_t res)
{
	struct rte_net_crc *ctx;
	uint32_t crc;
	int ret = TEST_SUCCESS;

	ctx = rte_net_crc_set_alg(RTE_NET_CRC_SCALAR, type);
	TEST_ASSERT_NOT_NULL(ctx, "cannot allocate the CRC context");
	crc = rte_net_crc_calc(ctx, data, data_len);
	if (crc != res) {
		RTE_LOG(ERR, USER1, "TEST FAILED: %s SCALAR\n", desc);
		debug_hexdump(stdout, "SCALAR", &crc, 4);
		ret = TEST_FAILED;
	}
	rte_net_crc_free(ctx);

	ctx = rte_net_crc_set_alg(RTE_NET_CRC_SSE42, type);
	TEST_ASSERT_NOT_NULL(ctx, "cannot allocate the CRC context");
	crc = rte_net_crc_calc(ctx, data, data_len);
	if (crc != res) {
		RTE_LOG(ERR, USER1, "TEST FAILED: %s SSE42\n", desc);
		debug_hexdump(stdout, "SSE", &crc, 4);
		ret = TEST_FAILED;
	}

	rte_net_crc_free(ctx);

	ctx = rte_net_crc_set_alg(RTE_NET_CRC_AVX512, type);
	TEST_ASSERT_NOT_NULL(ctx, "cannot allocate the CRC context");
	crc = rte_net_crc_calc(ctx, data, data_len);
	if (crc != res) {
		RTE_LOG(ERR, USER1, "TEST FAILED: %s AVX512\n", desc);
		debug_hexdump(stdout, "AVX512", &crc, 4);
		ret = TEST_FAILED;
	}
	rte_net_crc_free(ctx);

	ctx = rte_net_crc_set_alg(RTE_NET_CRC_NEON, type);
	TEST_ASSERT_NOT_NULL(ctx, "cannot allocate the CRC context");
	crc = rte_net_crc_calc(ctx, data, data_len);
	if (crc != res) {
		RTE_LOG(ERR, USER1, "TEST FAILED: %s NEON\n", desc);
		debug_hexdump(stdout, "NEON", &crc, 4);
		ret = TEST_FAILED;
	}
	rte_net_crc_free(ctx);

	return ret;
}

static int
crc_autotest(void)
{	uint8_t *test_data;
	uint32_t i;
	int ret = TEST_SUCCESS;

	/* 32-bit ethernet CRC: Test 1 */
	ret = crc_all_algs("32-bit ethernet CRC: Test 1", RTE_NET_CRC32_ETH, crc_vec,
		sizeof(crc_vec), crc32_vec_res);

	/* 32-bit ethernet CRC: Test 2 */
	test_data = rte_zmalloc(NULL, CRC32_VEC_LEN1, 0);
	if (test_data == NULL)
		return -7;
	for (i = 0; i < CRC32_VEC_LEN1; i += 12)
		rte_memcpy(&test_data[i], crc32_vec1, 12);
	ret |= crc_all_algs("32-bit ethernet CRC: Test 2", RTE_NET_CRC32_ETH, test_data,
		CRC32_VEC_LEN1, crc32_vec1_res);

	/* 32-bit ethernet CRC: Test 3 */
	memset(test_data, 0, CRC32_VEC_LEN1);
	for (i = 0; i < CRC32_VEC_LEN2; i += 12)
		rte_memcpy(&test_data[i], crc32_vec1, 12);
	ret |= crc_all_algs("32-bit ethernet CRC: Test 3", RTE_NET_CRC32_ETH, test_data,
		CRC32_VEC_LEN2, crc32_vec2_res);

	/* 16-bit CCITT CRC:  Test 4 */
	crc_all_algs("16-bit CCITT CRC:  Test 4", RTE_NET_CRC16_CCITT, crc_vec,
		sizeof(crc_vec), crc16_vec_res);

	/* 16-bit CCITT CRC:  Test 5 */
	ret |= crc_all_algs("16-bit CCITT CRC:  Test 5", RTE_NET_CRC16_CCITT, crc16_vec1,
		CRC16_VEC_LEN1, crc16_vec1_res);

	/* 16-bit CCITT CRC:  Test 6 */
	ret |= crc_all_algs("16-bit CCITT CRC:  Test 6", RTE_NET_CRC16_CCITT, crc16_vec2,
		CRC16_VEC_LEN2, crc16_vec2_res);

	return ret;
}

REGISTER_FAST_TEST(crc_autotest, true, true, crc_autotest);
