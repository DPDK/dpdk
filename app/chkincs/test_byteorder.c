/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 *  version: DPDK.L.1.2.3-3
 */

#include <rte_byteorder.h>

#include "test.h"

static volatile uint16_t u16 = 0x1337;
static volatile uint32_t u32 = 0xdeadbeefUL;
static volatile uint64_t u64 = 0xdeadcafebabefaceULL;

/*
 *      ^
 *     / \
 *    / | \     WARNING: this test program does *not* show how to use the
 *   /  .  \    API. Its only goal is to check dependencies of include files.
 *  /_______\
 */

int
test_byteorder(void)
{
	uint16_t res_u16;
	uint32_t res_u32;
	uint64_t res_u64;

	res_u16 = rte_bswap16(u16);
	res_u32 = rte_bswap32(u32);
	res_u64 = rte_bswap64(u64);

	res_u16 = rte_cpu_to_le_16(u16);
	res_u32 = rte_cpu_to_le_32(u32);
	res_u64 = rte_cpu_to_le_64(u64);

	res_u16 = rte_cpu_to_be_16(u16);
	res_u32 = rte_cpu_to_be_32(u32);
	res_u64 = rte_cpu_to_be_64(u64);

	res_u16 = rte_le_to_cpu_16(u16);
	res_u32 = rte_le_to_cpu_32(u32);
	res_u64 = rte_le_to_cpu_64(u64);

	res_u16 = rte_be_to_cpu_16(u16);
	res_u32 = rte_be_to_cpu_32(u32);
	res_u64 = rte_be_to_cpu_64(u64);

	(void)res_u16;
	(void)res_u32;
	(void)res_u64;

	return 1;
}
