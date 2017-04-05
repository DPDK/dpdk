/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation.
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
#define LINE_LEN           75

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
static const uint16_t crc16_vec1_res = 0x8cdd;
static const uint16_t crc16_vec2_res = 0xec5b;

static int
crc_calc(const uint8_t *vec,
	uint32_t vec_len,
	enum rte_net_crc_type type)
{
	/* compute CRC */
	uint32_t ret = rte_net_crc_calc(vec, vec_len, type);

	/* dump data on console */
	TEST_HEXDUMP(stdout, NULL, vec, vec_len);

	return  ret;
}

static int
test_crc_calc(void)
{
	uint32_t i;
	enum rte_net_crc_type type;
	uint8_t *test_data;
	uint32_t result;
	int error;

	/* 32-bit ethernet CRC: Test 1 */
	type = RTE_NET_CRC32_ETH;

	result = crc_calc(crc_vec, CRC_VEC_LEN, type);
	if (result != crc32_vec_res)
		return -1;

	/* 32-bit ethernet CRC: Test 2 */
	test_data = rte_zmalloc(NULL, CRC32_VEC_LEN1, 0);

	for (i = 0; i < CRC32_VEC_LEN1; i += 12)
		rte_memcpy(&test_data[i], crc32_vec1, 12);

	result = crc_calc(test_data, CRC32_VEC_LEN1, type);
	if (result != crc32_vec1_res) {
		error = -2;
		goto fail;
	}

	/* 32-bit ethernet CRC: Test 3 */
	for (i = 0; i < CRC32_VEC_LEN2; i += 12)
		rte_memcpy(&test_data[i], crc32_vec1, 12);

	result = crc_calc(test_data, CRC32_VEC_LEN2, type);
	if (result != crc32_vec2_res) {
		error = -3;
		goto fail;
	}

	/* 16-bit CCITT CRC:  Test 4 */
	type = RTE_NET_CRC16_CCITT;
	result = crc_calc(crc_vec, CRC_VEC_LEN, type);
	if (result != crc16_vec_res) {
		error = -4;
		goto fail;
	}
	/* 16-bit CCITT CRC:  Test 5 */
	result = crc_calc(crc16_vec1, CRC16_VEC_LEN1, type);
	if (result != crc16_vec1_res) {
		error = -5;
		goto fail;
	}
	/* 16-bit CCITT CRC:  Test 6 */
	result = crc_calc(crc16_vec2, CRC16_VEC_LEN2, type);
	if (result != crc16_vec2_res) {
		error = -6;
		goto fail;
	}

	rte_free(test_data);
	return 0;

fail:
	rte_free(test_data);
	return error;
}

static int
test_crc(void)
{
	int ret;
	/* set CRC scalar mode */
	rte_net_crc_set_alg(RTE_NET_CRC_SCALAR);

	ret = test_crc_calc();
	if (ret < 0) {
		printf("test_crc (scalar): failed (%d)\n", ret);
		return ret;
	}
	/* set CRC sse4.2 mode */
	rte_net_crc_set_alg(RTE_NET_CRC_SSE42);

	ret = test_crc_calc();
	if (ret < 0) {
		printf("test_crc (x86_64_SSE4.2): failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

REGISTER_TEST_COMMAND(crc_autotest, test_crc);
