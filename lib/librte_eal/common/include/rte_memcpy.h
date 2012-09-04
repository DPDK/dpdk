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

#ifndef _RTE_MEMCPY_H_
#define _RTE_MEMCPY_H_

/**
 * @file
 *
 * Functions for SSE implementation of memcpy().
 */

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copy 16 bytes from one location to another using optimised SSE
 * instructions. The locations should not overlap.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 */
static inline void
rte_mov16(uint8_t *dst, const uint8_t *src)
{
	asm volatile ("movdqu (%[src]), %%xmm0\n\t"
		      "movdqu %%xmm0, (%[dst])\n\t"
		      :
		      : [src] "r" (src),
			[dst] "r"(dst)
		      : "xmm0", "memory");
}

/**
 * Copy 32 bytes from one location to another using optimised SSE
 * instructions. The locations should not overlap.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 */
static inline void
rte_mov32(uint8_t *dst, const uint8_t *src)
{
	asm volatile ("movdqu (%[src]), %%xmm0\n\t"
		      "movdqu 16(%[src]), %%xmm1\n\t"
		      "movdqu %%xmm0, (%[dst])\n\t"
		      "movdqu %%xmm1, 16(%[dst])"
		      :
		      : [src] "r" (src),
			[dst] "r"(dst)
		      : "xmm0", "xmm1", "memory");
}

/**
 * Copy 48 bytes from one location to another using optimised SSE
 * instructions. The locations should not overlap.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 */
static inline void
rte_mov48(uint8_t *dst, const uint8_t *src)
{
	asm volatile ("movdqu (%[src]), %%xmm0\n\t"
		      "movdqu 16(%[src]), %%xmm1\n\t"
		      "movdqu 32(%[src]), %%xmm2\n\t"
		      "movdqu %%xmm0, (%[dst])\n\t"
		      "movdqu %%xmm1, 16(%[dst])\n\t"
		      "movdqu %%xmm2, 32(%[dst])"
		      :
		      : [src] "r" (src),
			[dst] "r"(dst)
		      : "xmm0", "xmm1", "memory");
}

/**
 * Copy 64 bytes from one location to another using optimised SSE
 * instructions. The locations should not overlap.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 */
static inline void
rte_mov64(uint8_t *dst, const uint8_t *src)
{
	asm volatile ("movdqu (%[src]), %%xmm0\n\t"
		      "movdqu 16(%[src]), %%xmm1\n\t"
		      "movdqu 32(%[src]), %%xmm2\n\t"
		      "movdqu 48(%[src]), %%xmm3\n\t"
		      "movdqu %%xmm0, (%[dst])\n\t"
		      "movdqu %%xmm1, 16(%[dst])\n\t"
		      "movdqu %%xmm2, 32(%[dst])\n\t"
		      "movdqu %%xmm3, 48(%[dst])"
		      :
		      : [src] "r" (src),
			[dst] "r"(dst)
		      : "xmm0", "xmm1", "xmm2", "xmm3","memory");
}

/**
 * Copy 128 bytes from one location to another using optimised SSE
 * instructions. The locations should not overlap.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 */
static inline void
rte_mov128(uint8_t *dst, const uint8_t *src)
{
	asm volatile ("movdqu (%[src]), %%xmm0\n\t"
		      "movdqu 16(%[src]), %%xmm1\n\t"
		      "movdqu 32(%[src]), %%xmm2\n\t"
		      "movdqu 48(%[src]), %%xmm3\n\t"
		      "movdqu 64(%[src]), %%xmm4\n\t"
		      "movdqu 80(%[src]), %%xmm5\n\t"
		      "movdqu 96(%[src]), %%xmm6\n\t"
		      "movdqu 112(%[src]), %%xmm7\n\t"
		      "movdqu %%xmm0, (%[dst])\n\t"
		      "movdqu %%xmm1, 16(%[dst])\n\t"
		      "movdqu %%xmm2, 32(%[dst])\n\t"
		      "movdqu %%xmm3, 48(%[dst])\n\t"
		      "movdqu %%xmm4, 64(%[dst])\n\t"
		      "movdqu %%xmm5, 80(%[dst])\n\t"
		      "movdqu %%xmm6, 96(%[dst])\n\t"
		      "movdqu %%xmm7, 112(%[dst])"
		      :
		      : [src] "r" (src),
			[dst] "r"(dst)
		      : "xmm0", "xmm1", "xmm2", "xmm3",
			"xmm4", "xmm5", "xmm6", "xmm7", "memory");
}

/**
 * Copy 256 bytes from one location to another using optimised SSE
 * instructions. The locations should not overlap.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 */
static inline void
rte_mov256(uint8_t *dst, const uint8_t *src)
{
	/*
	 * There are 16XMM registers, but this function does not use
	 * them all so that it can still be compiled as 32bit
	 * code. The performance increase was neglible if all 16
	 * registers were used.
	 */
	rte_mov128(dst, src);
	rte_mov128(dst + 128, src + 128);
}

#ifdef RTE_MEMCPY_BUILTIN_CONSTANT_P
/**
 * Choose between compiler built-in implementation of memcpy or DPDK
 * implementation depending if size is a compile-time constant
 */
#define rte_memcpy(dst, src, n) \
	(__builtin_constant_p (n) ? \
	memcpy(dst, src, n) : rte_memcpy_func(dst, src, n))
#else
/**
 * Always use DPDK implementation.
 */
#define rte_memcpy rte_memcpy_func
#endif

/**
 * Copy bytes from one location to another. The locations must not overlap.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 * @param n
 *   Number of bytes to copy.
 * @return
 *   Pointer to the destination data.
 */
static inline void *
rte_memcpy_func(void *dst, const void *src, size_t n)
{
	void *ret = dst;

	/* We can't copy < 16 bytes using XMM registers so do it manually. */
	if (n < 16) {
		if (n & 0x01) {
			*(uint8_t *)dst = *(const uint8_t *)src;
			dst = (uint8_t *)dst + 1;
			src = (const uint8_t *)src + 1;
		}
		if (n & 0x02) {
			*(uint16_t *)dst = *(const uint16_t *)src;
			dst = (uint16_t *)dst + 1;
			src = (const uint16_t *)src + 1;
		}
		if (n & 0x04) {
			/*
			 * NOTE: doing this as a 32bit copy causes "strict
			 * aliasing" compile errors, but worked fine for 64bit
			 * copy below, for unknown reasons.
			 */
			*(uint16_t *)dst = *(const uint16_t *)src;
			*((uint16_t *)dst + 1) = *((const uint16_t *)src + 1);
			dst = (uint32_t *)dst + 1;
			src = (const uint32_t *)src + 1;
		}
		if (n & 0x08) {
			*(uint64_t *)dst = *(const uint64_t *)src;
		}
		return ret;
	}

	/* Special fast cases for <= 128 bytes */
	if (n <= 32) {
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		rte_mov16((uint8_t *)dst - 16 + n, (const uint8_t *)src - 16 + n);
		return ret;
	}

	if (n <= 64) {
		rte_mov32((uint8_t *)dst, (const uint8_t *)src);
		rte_mov32((uint8_t *)dst - 32 + n, (const uint8_t *)src - 32 + n);
		return ret;
	}

	if (n <= 128) {
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		rte_mov64((uint8_t *)dst - 64 + n, (const uint8_t *)src - 64 + n);
		return ret;
	}

	/*
	 * For large copies > 128 bytes. This combination of 256, 64 and 16 byte
	 * copies was found to be faster than doing 128 and 32 byte copies as
	 * well.
	 */
	for ( ; n >= 256; n -= 256) {
		rte_mov256((uint8_t *)dst, (const uint8_t *)src);
		dst = (uint8_t *)dst + 256;
		src = (const uint8_t *)src + 256;
	}

	/*
	 * We split the remaining bytes (which will be less than 256) into
	 * 64byte (2^6) chunks.
	 * Using incrementing integers in the case labels of a switch statement
	 * enourages the compiler to use a jump table. To get incrementing
	 * integers, we shift the 2 relevant bits to the LSB position to first
	 * get decrementing integers, and then subtract.
	 */
	switch (3 - (n >> 6)) {
	case 0x00:
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		n -= 64;
		dst = (uint8_t *)dst + 64;
		src = (const uint8_t *)src + 64;      /* fallthrough */
	case 0x01:
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		n -= 64;
		dst = (uint8_t *)dst + 64;
		src = (const uint8_t *)src + 64;      /* fallthrough */
	case 0x02:
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		n -= 64;
		dst = (uint8_t *)dst + 64;
		src = (const uint8_t *)src + 64;      /* fallthrough */
	default:
		;
	}

	/*
	 * We split the remaining bytes (which will be less than 64) into
	 * 16byte (2^4) chunks, using the same switch structure as above.
	 */
	switch (3 - (n >> 4)) {
	case 0x00:
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		n -= 16;
		dst = (uint8_t *)dst + 16;
		src = (const uint8_t *)src + 16;      /* fallthrough */
	case 0x01:
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		n -= 16;
		dst = (uint8_t *)dst + 16;
		src = (const uint8_t *)src + 16;      /* fallthrough */
	case 0x02:
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		n -= 16;
		dst = (uint8_t *)dst + 16;
		src = (const uint8_t *)src + 16;      /* fallthrough */
	default:
		;
	}

	/* Copy any remaining bytes, without going beyond end of buffers */
	if (n != 0) {
		rte_mov16((uint8_t *)dst - 16 + n, (const uint8_t *)src - 16 + n);
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMCPY_H_ */
