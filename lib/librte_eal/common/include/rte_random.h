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
#include <stdlib.h>

/**
 * Seed the pseudo-random generator.
 *
 * The generator is automatically seeded by the EAL init with a timer
 * value. It may need to be re-seeded by the user with a real random
 * value.
 *
 * @param seedval
 *   The value of the seed.
 */
static inline void
rte_srand(uint64_t seedval)
{
	srand48((long)seedval);
}

/**
 * Get a pseudo-random value.
 *
 * This function generates pseudo-random numbers using the linear
 * congruential algorithm and 48-bit integer arithmetic, called twice
 * to generate a 64-bit value.
 *
 * @return
 *   A pseudo-random value between 0 and (1<<64)-1.
 */
static inline uint64_t
rte_rand(void)
{
	uint64_t val;
	val = (uint64_t)lrand48();
	val <<= 32;
	val += (uint64_t)lrand48();
	return val;
}

#ifdef __cplusplus
}
#endif


#endif /* _RTE_RANDOM_H_ */
