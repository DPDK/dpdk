/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_SCHED_COMMON_H__
#define __INCLUDE_RTE_SCHED_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

#define __rte_aligned_16 __attribute__((__aligned__(16)))

static inline uint32_t
rte_sched_min_val_2_u32(uint32_t x, uint32_t y)
{
	return (x < y)? x : y;
}

#if 0
static inline uint32_t
rte_min_pos_4_u16(uint16_t *x)
{
	uint32_t pos0, pos1;

	pos0 = (x[0] <= x[1])? 0 : 1;
	pos1 = (x[2] <= x[3])? 2 : 3;

	return (x[pos0] <= x[pos1])? pos0 : pos1;
}

#else

/* simplified version to remove branches with CMOV instruction */
static inline uint32_t
rte_min_pos_4_u16(uint16_t *x)
{
	uint32_t pos0 = 0;
	uint32_t pos1 = 2;

	if (x[1] <= x[0]) pos0 = 1;
	if (x[3] <= x[2]) pos1 = 3;
	if (x[pos1] <= x[pos0]) pos0 = pos1;

	return pos0;
}

#endif

/*
 * Compute the Greatest Common Divisor (GCD) of two numbers.
 * This implementation uses Euclid's algorithm:
 *    gcd(a, 0) = a
 *    gcd(a, b) = gcd(b, a mod b)
 *
 */
static inline uint32_t
rte_get_gcd(uint32_t a, uint32_t b)
{
	uint32_t c;

	if (a == 0)
		return b;
	if (b == 0)
		return a;

	if (a < b) {
		c = a;
		a = b;
		b = c;
	}

	while (b != 0) {
		c = a % b;
		a = b;
		b = c;
	}

	return a;
}

/*
 * Compute the Lowest Common Denominator (LCD) of two numbers.
 * This implementation computes GCD first:
 *    LCD(a, b) = (a * b) / GCD(a, b)
 *
 */
static inline uint32_t
rte_get_lcd(uint32_t a, uint32_t b)
{
	return (a * b) / rte_get_gcd(a, b);
}

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_SCHED_COMMON_H__ */
