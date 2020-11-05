/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP
 *
 */

#ifndef __RTA_COMPAT_H__
#define __RTA_COMPAT_H__

#include <stdint.h>
#include <errno.h>

#ifdef RTE_EXEC_ENV_LINUX
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <rte_byteorder.h>
#include <rte_common.h>

#ifndef __BYTE_ORDER__
#error "Undefined endianness"
#endif

#else /* !RTE_EXEC_ENV_LINUX */
#error Environment not supported!
#endif

#ifndef __always_inline
#define __always_inline __rte_always_inline
#endif

#ifndef __always_unused
#define __always_unused __rte_unused
#endif

#ifndef __maybe_unused
#define __maybe_unused __rte_unused
#endif

#if defined(SUPPRESS_PRINTS)
#define pr_msg(l, fmt, ...) do { } while (0)
#else
#define pr_msg(l, fmt, ...) \
	RTE_LOG(l, PMD, "%s(): " fmt "\n", __func__, ##__VA_ARGS__)
#endif

#if !defined(pr_debug)
#if defined(RTA_DEBUG)
#define pr_debug(fmt, ...) pr_msg(DEBUG, fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) do { } while (0)
#endif
#endif /* pr_debug */

#if !defined(pr_err)
#define pr_err(fmt, ...) pr_msg(ERR, fmt, ##__VA_ARGS__)
#endif /* pr_err */

#if !defined(pr_warn)
#define pr_warn(fmt, ...) pr_msg(WARNING, fmt, ##__VA_ARGS__)
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
#if defined(RTE_EXEC_ENV_LINUX) || defined(__GLIBC__)
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
