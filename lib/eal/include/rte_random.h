/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_RANDOM_H_
#define _RTE_RANDOM_H_

/**
 * @file
 *
 * Pseudo-random Generators in RTE
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>

/**
 * Seed the pseudo-random generator.
 *
 * The generator is automatically seeded by the EAL init with a timer
 * value. It may need to be re-seeded by the user with a real random
 * value.
 *
 * This function is not multi-thread safe in regards to other
 * rte_srand() calls, nor is it in relation to concurrent rte_rand(),
 * rte_rand_max() or rte_drand() calls.
 *
 * @param seedval
 *   The value of the seed.
 */
void
rte_srand(uint64_t seedval);

/**
 * Get a pseudo-random value.
 *
 * The generator is not cryptographically secure.
 *
 * rte_rand(), rte_rand_max() and rte_drand() are multi-thread safe,
 * with the exception that they may not be called by multiple
 * _unregistered_ non-EAL threads in parallel.
 *
 * @return
 *   A pseudo-random value between 0 and (1<<64)-1.
 */
uint64_t
rte_rand(void);

/**
 * Generates a pseudo-random number with an upper bound.
 *
 * This function returns an uniformly distributed (unbiased) random
 * number less than a user-specified maximum value.
 *
 * rte_rand(), rte_rand_max() and rte_drand() are multi-thread safe,
 * with the exception that they may not be called by multiple
 * _unregistered_ non-EAL threads in parallel.
 *
 * @param upper_bound
 *   The upper bound of the generated number.
 * @return
 *   A pseudo-random value between 0 and (upper_bound-1).
 */
uint64_t
rte_rand_max(uint64_t upper_bound);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Generates a pseudo-random floating point number.
 *
 * This function returns a non-negative double-precision floating random
 * number uniformly distributed over the interval [0.0, 1.0).
 *
 * The generator is not cryptographically secure.
 *
 * rte_rand(), rte_rand_max() and rte_drand() are multi-thread safe,
 * with the exception that they may not be called by multiple
 * _unregistered_ non-EAL threads in parallel.
 *
 * @return
 *   A pseudo-random value between 0 and 1.0.
 */
__rte_experimental
double rte_drand(void);

#ifdef __cplusplus
}
#endif


#endif /* _RTE_RANDOM_H_ */
