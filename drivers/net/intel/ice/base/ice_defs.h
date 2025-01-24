/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _ICE_DEFS_H_
#define _ICE_DEFS_H_

#define ETH_ALEN	6

#define ETH_HEADER_LEN	14

#define BIT(a) (1UL << (a))
#define BIT_ULL(a) (1ULL << (a))

#define BITS_PER_BYTE	8

#define _FORCE_

#define ICE_BYTES_PER_WORD	2
#define ICE_BYTES_PER_DWORD	4
#define ICE_MAX_TRAFFIC_CLASS	8

/**
 * ROUND_UP - round up to next arbitrary multiple (not a power of 2)
 * @a: value to round up
 * @b: arbitrary multiple
 *
 * Round up to the next multiple of the arbitrary b.
 * Note, when b is a power of 2 use ICE_ALIGN() instead.
 */
#define ROUND_UP(a, b)	((b) * DIVIDE_AND_ROUND_UP((a), (b)))

#define MIN_T(_t, _a, _b)	min((_t)(_a), (_t)(_b))

#define IS_ASCII(_ch)	((_ch) < 0x80)

#define STRUCT_HACK_VAR_LEN
/**
 * ice_struct_size - size of struct with C99 flexible array member
 * @ptr: pointer to structure
 * @field: flexible array member (last member of the structure)
 * @num: number of elements of that flexible array member
 */
#define ice_struct_size(ptr, field, num) \
	(sizeof(*(ptr)) + sizeof(*(ptr)->field) * (num))

#define FLEX_ARRAY_SIZE(_ptr, _mem, cnt) ((cnt) * sizeof(_ptr->_mem[0]))

#endif /* _ICE_DEFS_H_ */
