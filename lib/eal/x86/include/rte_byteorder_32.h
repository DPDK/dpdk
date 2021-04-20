/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_BYTEORDER_X86_H_
#error do not include this file directly, use <rte_byteorder.h> instead
#endif

#ifndef _RTE_BYTEORDER_I686_H_
#define _RTE_BYTEORDER_I686_H_

#include <stdint.h>
#include <rte_byteorder.h>

/*
 * An architecture-optimized byte swap for a 64-bit value.
 *
  * Do not use this function directly. The preferred function is rte_bswap64().
 */
/* Compat./Leg. mode */
static inline uint64_t rte_arch_bswap64(uint64_t x)
{
	uint64_t ret = 0;
	ret |= ((uint64_t)rte_arch_bswap32(x & 0xffffffffUL) << 32);
	ret |= ((uint64_t)rte_arch_bswap32((x >> 32) & 0xffffffffUL));
	return ret;
}

#endif /* _RTE_BYTEORDER_I686_H_ */
