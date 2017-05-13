/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright (c) 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __RTA_COMPAT_H__
#define __RTA_COMPAT_H__

#include <stdint.h>
#include <errno.h>

#ifdef __GLIBC__
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <rte_byteorder.h>
#include <rte_common.h>

#ifndef __BYTE_ORDER__
#error "Undefined endianness"
#endif

#else
#error Environment not supported!
#endif

#ifndef __always_inline
#define __always_inline __rte_always_inline
#endif

#ifndef __always_unused
#define __always_unused __attribute__((unused))
#endif

#ifndef __maybe_unused
#define __maybe_unused __attribute__((unused))
#endif

#if defined(__GLIBC__) && !defined(pr_debug)
#if !defined(SUPPRESS_PRINTS) && defined(RTA_DEBUG)
#define pr_debug(fmt, ...) \
	RTE_LOG(DEBUG, PMD, "%s(): " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)     do { } while (0)
#endif
#endif /* pr_debug */

#if defined(__GLIBC__) && !defined(pr_err)
#if !defined(SUPPRESS_PRINTS)
#define pr_err(fmt, ...) \
	RTE_LOG(ERR, PMD, "%s(): " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define pr_err(fmt, ...)    do { } while (0)
#endif
#endif /* pr_err */

#if defined(__GLIBC__) && !defined(pr_warn)
#if !defined(SUPPRESS_PRINTS)
#define pr_warn(fmt, ...) \
	RTE_LOG(WARNING, PMD, "%s(): " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define pr_warn(fmt, ...)    do { } while (0)
#endif
#endif /* pr_warn */

/**
 * ARRAY_SIZE - returns the number of elements in an array
 * @x: array
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef ALIGN
#define ALIGN(x, a) (((x) + ((__typeof__(x))(a) - 1)) & \
			~((__typeof__(x))(a) - 1))
#endif

#ifndef BIT
#define BIT(nr)		(1UL << (nr))
#endif

#ifndef upper_32_bits
/**
 * upper_32_bits - return bits 32-63 of a number
 * @n: the number we're accessing
 */
#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))
#endif

#ifndef lower_32_bits
/**
 * lower_32_bits - return bits 0-31 of a number
 * @n: the number we're accessing
 */
#define lower_32_bits(n) ((uint32_t)(n))
#endif

/* Use Linux naming convention */
#ifdef __GLIBC__
	#define swab16(x) rte_bswap16(x)
	#define swab32(x) rte_bswap32(x)
	#define swab64(x) rte_bswap64(x)
	/* Define cpu_to_be32 macro if not defined in the build environment */
	#if !defined(cpu_to_be32)
		#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			#define cpu_to_be32(x)	(x)
		#else
			#define cpu_to_be32(x)	swab32(x)
		#endif
	#endif
	/* Define cpu_to_le32 macro if not defined in the build environment */
	#if !defined(cpu_to_le32)
		#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			#define cpu_to_le32(x)	swab32(x)
		#else
			#define cpu_to_le32(x)	(x)
		#endif
	#endif
#endif

#endif /* __RTA_COMPAT_H__ */
