/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Ericsson AB
 */

#ifdef __RDSEED__
#include <x86intrin.h>
#endif
#include <unistd.h>

#include <rte_bitops.h>
#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_random.h>

struct rte_rand_state {
	uint64_t z1;
	uint64_t z2;
	uint64_t z3;
	uint64_t z4;
	uint64_t z5;
	RTE_CACHE_GUARD;
} __rte_cache_aligned;

/* One instance each for every lcore id-equipped thread, and one
 * additional instance to be shared by all others threads (i.e., all
 * unregistered non-EAL threads).
 */
static struct rte_rand_state rand_states[RTE_MAX_LCORE + 1];

static uint32_t
__rte_rand_lcg32(uint32_t *seed)
{
	*seed = 1103515245U * *seed + 12345U;

	return *seed;
}

static uint64_t
__rte_rand_lcg64(uint32_t *seed)
{
	uint64_t low;
	uint64_t high;

	/* A 64-bit LCG would have been much cleaner, but good
	 * multiplier/increments for such seem hard to come by.
	 */

	low = __rte_rand_lcg32(seed);
	high = __rte_rand_lcg32(seed);

	return low | (high << 32);
}

static uint64_t
__rte_rand_lfsr258_gen_seed(uint32_t *seed, uint64_t min_value)
{
	uint64_t res;

	res = __rte_rand_lcg64(seed);

	if (res < min_value)
		res += min_value;

	return res;
}

static void
__rte_srand_lfsr258(uint64_t seed, struct rte_rand_state *state)
{
	uint32_t lcg_seed;

	lcg_seed = (uint32_t)(seed ^ (seed >> 32));

	state->z1 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 2UL);
	state->z2 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 512UL);
	state->z3 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 4096UL);
	state->z4 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 131072UL);
	state->z5 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 8388608UL);
}

void
rte_srand(uint64_t seed)
{
	unsigned int lcore_id;

	/* add lcore_id to seed to avoid having the same sequence */
	for (lcore_id = 0; lcore_id < RTE_DIM(rand_states); lcore_id++)
		__rte_srand_lfsr258(seed + lcore_id, &rand_states[lcore_id]);
}

static __rte_always_inline uint64_t
__rte_rand_lfsr258_comp(uint64_t z, uint64_t a, uint64_t b, uint64_t c,
			uint64_t d)
{
	return ((z & c) << d) ^ (((z << a) ^ z) >> b);
}

/* Based on L’Ecuyer, P.: Tables of maximally equidistributed combined
 * LFSR generators.
 */

static __rte_always_inline uint64_t
__rte_rand_lfsr258(struct rte_rand_state *state)
{
	state->z1 = __rte_rand_lfsr258_comp(state->z1, 1UL, 53UL,
					    18446744073709551614UL, 10UL);
	state->z2 = __rte_rand_lfsr258_comp(state->z2, 24UL, 50UL,
					    18446744073709551104UL, 5UL);
	state->z3 = __rte_rand_lfsr258_comp(state->z3, 3UL, 23UL,
					    18446744073709547520UL, 29UL);
	state->z4 = __rte_rand_lfsr258_comp(state->z4, 5UL, 24UL,
					    18446744073709420544UL, 23UL);
	state->z5 = __rte_rand_lfsr258_comp(state->z5, 3UL, 33UL,
					    18446744073701163008UL, 8UL);

	return state->z1 ^ state->z2 ^ state->z3 ^ state->z4 ^ state->z5;
}

static __rte_always_inline
struct rte_rand_state *__rte_rand_get_state(void)
{
	unsigned int idx;

	idx = rte_lcore_id();

	/* last instance reserved for unregistered non-EAL threads */
	if (unlikely(idx == LCORE_ID_ANY))
		idx = RTE_MAX_LCORE;

	return &rand_states[idx];
}

uint64_t
rte_rand(void)
{
	struct rte_rand_state *state;

	state = __rte_rand_get_state();

	return __rte_rand_lfsr258(state);
}

uint64_t
rte_rand_max(uint64_t upper_bound)
{
	struct rte_rand_state *state;
	uint8_t ones;
	uint8_t leading_zeros;
	uint64_t mask = ~((uint64_t)0);
	uint64_t res;

	if (unlikely(upper_bound < 2))
		return 0;

	state = __rte_rand_get_state();

	ones = rte_popcount64(upper_bound);

	/* Handle power-of-2 upper_bound as a special case, since it
	 * has no bias issues.
	 */
	if (unlikely(ones == 1))
		return __rte_rand_lfsr258(state) & (upper_bound - 1);

	/* The approach to avoiding bias is to create a mask that
	 * stretches beyond the request value range, and up to the
	 * next power-of-2. In case the masked generated random value
	 * is equal to or greater than the upper bound, just discard
	 * the value and generate a new one.
	 */

	leading_zeros = rte_clz64(upper_bound);
	mask >>= leading_zeros;

	do {
		res = __rte_rand_lfsr258(state) & mask;
	} while (unlikely(res >= upper_bound));

	return res;
}

double
rte_drand(void)
{
	static const uint64_t denom = (uint64_t)1 << 53;
	uint64_t rand64 = rte_rand();

	/*
	 * The double mantissa only has 53 bits, so we uniformly mask off the
	 * high 11 bits and then floating-point divide by 2^53 to achieve a
	 * result in [0, 1).
	 *
	 * We are not allowed to emit 1.0, so denom must be one greater than
	 * the possible range of the preceding step.
	 */

	rand64 &= denom - 1;
	return (double)rand64 / denom;
}

static uint64_t
__rte_random_initial_seed(void)
{
#ifdef RTE_LIBEAL_USE_GETENTROPY
	int ge_rc;
	uint64_t ge_seed;

	ge_rc = getentropy(&ge_seed, sizeof(ge_seed));

	if (ge_rc == 0)
		return ge_seed;
#endif
#ifdef __RDSEED__
	unsigned int rdseed_low;
	unsigned int rdseed_high;

	/* first fallback: rdseed instruction, if available */
	if (_rdseed32_step(&rdseed_low) == 1 &&
	    _rdseed32_step(&rdseed_high) == 1)
		return (uint64_t)rdseed_low | ((uint64_t)rdseed_high << 32);
#endif
	/* second fallback: seed using rdtsc */
	return rte_get_tsc_cycles();
}

RTE_INIT(rte_rand_init)
{
	uint64_t seed;

	seed = __rte_random_initial_seed();

	rte_srand(seed);
}
