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

#ifndef _RTE_BYTEORDER_H_
#define _RTE_BYTEORDER_H_

/**
 * @file
 *
 * Byte Swap Operations
 *
 * This file defines a generic API for byte swap operations. Part of
 * the implementation is architecture-specific.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * An internal function to swap bytes in a 16-bit value.
 *
 * It is used by rte_bswap16() when the value is constant. Do not use
 * this function directly; rte_bswap16() is preferred.
 */
static inline uint16_t
rte_constant_bswap16(uint16_t x)
{
	return (uint16_t)(((x & 0x00ffU) << 8) |
		((x & 0xff00U) >> 8));
}

/*
 * An internal function to swap bytes in a 32-bit value.
 *
 * It is used by rte_bswap32() when the value is constant. Do not use
 * this function directly; rte_bswap32() is preferred.
 */
static inline uint32_t
rte_constant_bswap32(uint32_t x)
{
	return  ((x & 0x000000ffUL) << 24) |
		((x & 0x0000ff00UL) << 8) |
		((x & 0x00ff0000UL) >> 8) |
		((x & 0xff000000UL) >> 24);
}

/*
 * An internal function to swap bytes of a 64-bit value.
 *
 * It is used by rte_bswap64() when the value is constant. Do not use
 * this function directly; rte_bswap64() is preferred.
 */
static inline uint64_t
rte_constant_bswap64(uint64_t x)
{
	return  ((x & 0x00000000000000ffULL) << 56) |
		((x & 0x000000000000ff00ULL) << 40) |
		((x & 0x0000000000ff0000ULL) << 24) |
		((x & 0x00000000ff000000ULL) <<  8) |
		((x & 0x000000ff00000000ULL) >>  8) |
		((x & 0x0000ff0000000000ULL) >> 24) |
		((x & 0x00ff000000000000ULL) >> 40) |
		((x & 0xff00000000000000ULL) >> 56);
}

/*
 * An architecture-optimized byte swap for a 16-bit value.
 *
 * Do not use this function directly. The preferred function is rte_bswap16().
 */
static inline uint16_t rte_arch_bswap16(uint16_t _x)
{
	register uint16_t x = _x;
	asm volatile ("xchgb %b[x1],%h[x2]"
		      : [x1] "=Q" (x)
		      : [x2] "0" (x)
		      );
	return x;
}

/*
 * An architecture-optimized byte swap for a 32-bit value.
 *
 * Do not use this function directly. The preferred function is rte_bswap32().
 */
static inline uint32_t rte_arch_bswap32(uint32_t _x)
{
	register uint32_t x = _x;
	asm volatile ("bswap %[x]"
		      : [x] "+r" (x)
		      );
	return x;
}

/*
 * An architecture-optimized byte swap for a 64-bit value.
 *
  * Do not use this function directly. The preferred function is rte_bswap64().
 */
#ifdef RTE_ARCH_X86_64
/* 64-bit mode */
static inline uint64_t rte_arch_bswap64(uint64_t _x)
{
	register uint64_t x = _x;
	asm volatile ("bswap %[x]"
		      : [x] "+r" (x)
		      );
	return x;
}
#else /* ! RTE_ARCH_X86_64 */
/* Compat./Leg. mode */
static inline uint64_t rte_arch_bswap64(uint64_t x)
{
	uint64_t ret = 0;
	ret |= ((uint64_t)rte_arch_bswap32(x & 0xffffffffUL) << 32);
	ret |= ((uint64_t)rte_arch_bswap32((x >> 32) & 0xffffffffUL));
	return ret;
}
#endif /* RTE_ARCH_X86_64 */


#ifndef RTE_FORCE_INTRINSICS
/**
 * Swap bytes in a 16-bit value.
 */
#define rte_bswap16(x) ((uint16_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap16(x) :		\
				   rte_arch_bswap16(x)))

/**
 * Swap bytes in a 32-bit value.
 */
#define rte_bswap32(x) ((uint32_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap32(x) :		\
				   rte_arch_bswap32(x)))

/**
 * Swap bytes in a 64-bit value.
 */
#define rte_bswap64(x) ((uint64_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap64(x) :		\
				   rte_arch_bswap64(x)))

#else

/**
 * Swap bytes in a 16-bit value.
 * __builtin_bswap16 is only available gcc 4.8 and upwards
 */
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#define rte_bswap16(x) __builtin_bswap16(x)
#else
#define rte_bswap16(x) ((uint16_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap16(x) :		\
				   rte_arch_bswap16(x)))
#endif

/**
 * Swap bytes in a 32-bit value.
 */
#define rte_bswap32(x) __builtin_bswap32(x)

/**
 * Swap bytes in a 64-bit value.
 */
#define rte_bswap64(x) __builtin_bswap64(x)

#endif

/**
 * Convert a 16-bit value from CPU order to little endian.
 */
#define rte_cpu_to_le_16(x) (x)

/**
 * Convert a 32-bit value from CPU order to little endian.
 */
#define rte_cpu_to_le_32(x) (x)

/**
 * Convert a 64-bit value from CPU order to little endian.
 */
#define rte_cpu_to_le_64(x) (x)


/**
 * Convert a 16-bit value from CPU order to big endian.
 */
#define rte_cpu_to_be_16(x) rte_bswap16(x)

/**
 * Convert a 32-bit value from CPU order to big endian.
 */
#define rte_cpu_to_be_32(x) rte_bswap32(x)

/**
 * Convert a 64-bit value from CPU order to big endian.
 */
#define rte_cpu_to_be_64(x) rte_bswap64(x)


/**
 * Convert a 16-bit value from little endian to CPU order.
 */
#define rte_le_to_cpu_16(x) (x)

/**
 * Convert a 32-bit value from little endian to CPU order.
 */
#define rte_le_to_cpu_32(x) (x)

/**
 * Convert a 64-bit value from little endian to CPU order.
 */
#define rte_le_to_cpu_64(x) (x)


/**
 * Convert a 16-bit value from big endian to CPU order.
 */
#define rte_be_to_cpu_16(x) rte_bswap16(x)

/**
 * Convert a 32-bit value from big endian to CPU order.
 */
#define rte_be_to_cpu_32(x) rte_bswap32(x)

/**
 * Convert a 64-bit value from big endian to CPU order.
 */
#define rte_be_to_cpu_64(x) rte_bswap64(x)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BYTEORDER_H_ */
