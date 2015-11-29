/*
 * Reciprocal divide
 *
 * Used with permission from original authors
 *  Hannes Frederic Sowa and Daniel Borkmann
 *
 * This algorithm is based on the paper "Division by Invariant
 * Integers Using Multiplication" by Torbjörn Granlund and Peter
 * L. Montgomery.
 *
 * The assembler implementation from Agner Fog, which this code is
 * based on, can be found here:
 * http://www.agner.org/optimize/asmlib.zip
 *
 * This optimization for A/B is helpful if the divisor B is mostly
 * runtime invariant. The reciprocal of B is calculated in the
 * slow-path with reciprocal_value(). The fast-path can then just use
 * a much faster multiplication operation with a variable dividend A
 * to calculate the division A/B.
 */

#ifndef _RTE_RECIPROCAL_H_
#define _RTE_RECIPROCAL_H_

struct rte_reciprocal {
	uint32_t m;
	uint8_t sh1, sh2;
};

static inline uint32_t rte_reciprocal_divide(uint32_t a, struct rte_reciprocal R)
{
	uint32_t t = (uint32_t)(((uint64_t)a * R.m) >> 32);

	return (t + ((a - t) >> R.sh1)) >> R.sh2;
}

struct rte_reciprocal rte_reciprocal_value(uint32_t d);

#endif /* _RTE_RECIPROCAL_H_ */
