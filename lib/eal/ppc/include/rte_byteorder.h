/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Inspired from FreeBSD src/sys/powerpc/include/endian.h
 * Copyright (c) 1987, 1991, 1993
 * The Regents of the University of California.  All rights reserved.
 */

#ifndef _RTE_BYTEORDER_PPC_64_H_
#define _RTE_BYTEORDER_PPC_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "generic/rte_byteorder.h"

/*
 * An architecture-optimized byte swap for a 16-bit value.
 *
 * Do not use this function directly. The preferred function is rte_bswap16().
 */
static inline uint16_t rte_arch_bswap16(uint16_t _x)
{
	return (_x >> 8) | ((_x << 8) & 0xff00);
}

/*
 * An architecture-optimized byte swap for a 32-bit value.
 *
 * Do not use this function directly. The preferred function is rte_bswap32().
 */
static inline uint32_t rte_arch_bswap32(uint32_t _x)
{
	return (_x >> 24) | ((_x >> 8) & 0xff00) | ((_x << 8) & 0xff0000) |
		((_x << 24) & 0xff000000);
}

/*
 * An architecture-optimized byte swap for a 64-bit value.
 *
 * Do not use this function directly. The preferred function is rte_bswap64().
 */
/* 64-bit mode */
static inline uint64_t rte_arch_bswap64(uint64_t _x)
{
	return (_x >> 56) | ((_x >> 40) & 0xff00) | ((_x >> 24) & 0xff0000) |
		((_x >> 8) & 0xff000000) | ((_x << 8) & (0xffULL << 32)) |
		((_x << 24) & (0xffULL << 40)) |
		((_x << 40) & (0xffULL << 48)) | ((_x << 56));
}

#ifndef RTE_FORCE_INTRINSICS
#define rte_bswap16(x) ((uint16_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap16(x) :		\
				   rte_arch_bswap16(x)))

#define rte_bswap32(x) ((uint32_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap32(x) :		\
				   rte_arch_bswap32(x)))

#define rte_bswap64(x) ((uint64_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap64(x) :		\
				   rte_arch_bswap64(x)))
#endif

/* Power 8 have both little endian and big endian mode
 * Power 7 only support big endian
 */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define rte_cpu_to_le_16(x) (x)
#define rte_cpu_to_le_32(x) (x)
#define rte_cpu_to_le_64(x) (x)

#define rte_cpu_to_be_16(x) rte_bswap16(x)
#define rte_cpu_to_be_32(x) rte_bswap32(x)
#define rte_cpu_to_be_64(x) rte_bswap64(x)

#define rte_le_to_cpu_16(x) (x)
#define rte_le_to_cpu_32(x) (x)
#define rte_le_to_cpu_64(x) (x)

#define rte_be_to_cpu_16(x) rte_bswap16(x)
#define rte_be_to_cpu_32(x) rte_bswap32(x)
#define rte_be_to_cpu_64(x) rte_bswap64(x)

#else /* RTE_BIG_ENDIAN */

#define rte_cpu_to_le_16(x) rte_bswap16(x)
#define rte_cpu_to_le_32(x) rte_bswap32(x)
#define rte_cpu_to_le_64(x) rte_bswap64(x)

#define rte_cpu_to_be_16(x) (x)
#define rte_cpu_to_be_32(x) (x)
#define rte_cpu_to_be_64(x) (x)

#define rte_le_to_cpu_16(x) rte_bswap16(x)
#define rte_le_to_cpu_32(x) rte_bswap32(x)
#define rte_le_to_cpu_64(x) rte_bswap64(x)

#define rte_be_to_cpu_16(x) (x)
#define rte_be_to_cpu_32(x) (x)
#define rte_be_to_cpu_64(x) (x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BYTEORDER_PPC_64_H_ */
