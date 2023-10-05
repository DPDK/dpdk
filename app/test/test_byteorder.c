/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_byteorder.h>

#include "test.h"

static volatile uint16_t u16 = 0x1337;
static volatile uint32_t u32 = 0xdeadbeefUL;
static volatile uint64_t u64 = 0xdeadcafebabefaceULL;

/*
 * Byteorder functions
 * ===================
 *
 * - check that optimized byte swap functions are working for each
 *   size (16, 32, 64 bits)
 */

static int
test_byteorder(void)
{
	uint16_t res_u16;
	uint32_t res_u32;
	uint64_t res_u64;

	res_u16 = rte_bswap16(u16);
	printf("%"PRIx16" -> %"PRIx16"\n", u16, res_u16);
	if (res_u16 != 0x3713)
		return -1;

	res_u32 = rte_bswap32(u32);
	printf("%"PRIx32" -> %"PRIx32"\n", u32, res_u32);
	if (res_u32 != 0xefbeaddeUL)
		return -1;

	res_u64 = rte_bswap64(u64);
	printf("%"PRIx64" -> %"PRIx64"\n", u64, res_u64);
	if (res_u64 != 0xcefabebafecaaddeULL)
		return -1;

	res_u16 = rte_bswap16(0x1337);
	printf("const %"PRIx16" -> %"PRIx16"\n", (uint16_t)0x1337, res_u16);
	if (res_u16 != 0x3713)
		return -1;

	res_u32 = rte_bswap32(0xdeadbeefUL);
	printf("const %"PRIx32" -> %"PRIx32"\n", (uint32_t) 0xdeadbeef, res_u32);
	if (res_u32 != 0xefbeaddeUL)
		return -1;

	res_u64 = rte_bswap64(0xdeadcafebabefaceULL);
	printf("const %"PRIx64" -> %"PRIx64"\n", (uint64_t) 0xdeadcafebabefaceULL, res_u64);
	if (res_u64 != 0xcefabebafecaaddeULL)
		return -1;

	return 0;
}

REGISTER_FAST_TEST(byteorder_autotest, true, true, test_byteorder);
