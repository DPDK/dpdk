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
	srand48((long unsigned int)seedval);
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
	val = lrand48();
	val <<= 32;
	val += lrand48();
	return val;
}

#ifdef __cplusplus
}
#endif


#endif /* _RTE_PER_LCORE_H_ */
