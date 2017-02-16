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
	printf("const %"PRIx16" -> %"PRIx16"\n", 0x1337, res_u16);
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

REGISTER_TEST_COMMAND(byteorder_autotest, test_byteorder);
