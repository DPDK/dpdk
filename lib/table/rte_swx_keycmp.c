/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#include <rte_common.h>

#include "rte_swx_keycmp.h"

static uint32_t
keycmp_generic(void *key1, void *key2, uint32_t key_size)
{
	return memcmp(key1, key2, key_size) ? 0 : 1;
}

#define KEYCMP(N)                                                 \
static uint32_t                                                   \
keycmp##N(void *key1, void *key2, uint32_t key_size __rte_unused) \
{                                                                 \
	return memcmp(key1, key2, N) ? 0 : 1;                     \
}

KEYCMP(1)
KEYCMP(2)
KEYCMP(3)
KEYCMP(4)
KEYCMP(5)
KEYCMP(6)
KEYCMP(7)
KEYCMP(8)
KEYCMP(9)

KEYCMP(10)
KEYCMP(11)
KEYCMP(12)
KEYCMP(13)
KEYCMP(14)
KEYCMP(15)
KEYCMP(16)
KEYCMP(17)
KEYCMP(18)
KEYCMP(19)

KEYCMP(20)
KEYCMP(21)
KEYCMP(22)
KEYCMP(23)
KEYCMP(24)
KEYCMP(25)
KEYCMP(26)
KEYCMP(27)
KEYCMP(28)
KEYCMP(29)

KEYCMP(30)
KEYCMP(31)
KEYCMP(32)
KEYCMP(33)
KEYCMP(34)
KEYCMP(35)
KEYCMP(36)
KEYCMP(37)
KEYCMP(38)
KEYCMP(39)

KEYCMP(40)
KEYCMP(41)
KEYCMP(42)
KEYCMP(43)
KEYCMP(44)
KEYCMP(45)
KEYCMP(46)
KEYCMP(47)
KEYCMP(48)
KEYCMP(49)

KEYCMP(50)
KEYCMP(51)
KEYCMP(52)
KEYCMP(53)
KEYCMP(54)
KEYCMP(55)
KEYCMP(56)
KEYCMP(57)
KEYCMP(58)
KEYCMP(59)

KEYCMP(60)
KEYCMP(61)
KEYCMP(62)
KEYCMP(63)
KEYCMP(64)

static rte_swx_keycmp_func_t keycmp_funcs[] = {
	keycmp1,
	keycmp2,
	keycmp3,
	keycmp4,
	keycmp5,
	keycmp6,
	keycmp7,
	keycmp8,
	keycmp9,
	keycmp10,
	keycmp11,
	keycmp12,
	keycmp13,
	keycmp14,
	keycmp15,
	keycmp16,
	keycmp17,
	keycmp18,
	keycmp19,
	keycmp20,
	keycmp21,
	keycmp22,
	keycmp23,
	keycmp24,
	keycmp25,
	keycmp26,
	keycmp27,
	keycmp28,
	keycmp29,
	keycmp30,
	keycmp31,
	keycmp32,
	keycmp33,
	keycmp34,
	keycmp35,
	keycmp36,
	keycmp37,
	keycmp38,
	keycmp39,
	keycmp40,
	keycmp41,
	keycmp42,
	keycmp43,
	keycmp44,
	keycmp45,
	keycmp46,
	keycmp47,
	keycmp48,
	keycmp49,
	keycmp50,
	keycmp51,
	keycmp52,
	keycmp53,
	keycmp54,
	keycmp55,
	keycmp56,
	keycmp57,
	keycmp58,
	keycmp59,
	keycmp60,
	keycmp61,
	keycmp62,
	keycmp63,
	keycmp64,
};

rte_swx_keycmp_func_t
rte_swx_keycmp_func_get(uint32_t key_size)
{
	if (key_size && key_size <= 64)
		return keycmp_funcs[key_size - 1];

	return keycmp_generic;
}
