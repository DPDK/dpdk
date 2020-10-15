/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _RTE_PREFETCH_H_
#define _RTE_PREFETCH_H_

#include <rte_compat.h>

/**
 * @file
 *
 * Prefetch operations.
 *
 * This file defines an API for prefetch macros / inline-functions,
 * which are architecture-dependent. Prefetching occurs when a
 * processor requests an instruction or data from memory to cache
 * before it is actually needed, potentially speeding up the execution of the
 * program.
 */

/**
 * Prefetch a cache line into all cache levels.
 * @param p
 *   Address to prefetch
 */
static inline void rte_prefetch0(const volatile void *p);

/**
 * Prefetch a cache line into all cache levels except the 0th cache level.
 * @param p
 *   Address to prefetch
 */
static inline void rte_prefetch1(const volatile void *p);

/**
 * Prefetch a cache line into all cache levels except the 0th and 1th cache
 * levels.
 * @param p
 *   Address to prefetch
 */
static inline void rte_prefetch2(const volatile void *p);

/**
 * Prefetch a cache line into all cache levels (non-temporal/transient version)
 *
 * The non-temporal prefetch is intended as a prefetch hint that processor will
 * use the prefetched data only once or short period, unlike the
 * rte_prefetch0() function which imply that prefetched data to use repeatedly.
 *
 * @param p
 *   Address to prefetch
 */
static inline void rte_prefetch_non_temporal(const volatile void *p);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Prefetch a cache line into all cache levels, with intention to write. This
 * prefetch variant hints to the CPU that the program is expecting to write to
 * the cache line being prefetched.
 *
 * @param p Address to prefetch
 */
__rte_experimental
static inline void
rte_prefetch0_write(const void *p)
{
	/* 1 indicates intention to write, 3 sets target cache level to L1. See
	 * GCC docs where these integer constants are described in more detail:
	 *  https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
	 */
	__builtin_prefetch(p, 1, 3);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Prefetch a cache line into all cache levels, except the 0th, with intention
 * to write. This prefetch variant hints to the CPU that the program is
 * expecting to write to the cache line being prefetched.
 *
 * @param p Address to prefetch
 */
__rte_experimental
static inline void
rte_prefetch1_write(const void *p)
{
	/* 1 indicates intention to write, 2 sets target cache level to L2. See
	 * GCC docs where these integer constants are described in more detail:
	 *  https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
	 */
	__builtin_prefetch(p, 1, 2);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Prefetch a cache line into all cache levels, except the 0th and 1st, with
 * intention to write. This prefetch variant hints to the CPU that the program
 * is expecting to write to the cache line being prefetched.
 *
 * @param p Address to prefetch
 */
__rte_experimental
static inline void
rte_prefetch2_write(const void *p)
{
	/* 1 indicates intention to write, 1 sets target cache level to L3. See
	 * GCC docs where these integer constants are described in more detail:
	 *  https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
	 */
	__builtin_prefetch(p, 1, 1);
}

#endif /* _RTE_PREFETCH_H_ */
