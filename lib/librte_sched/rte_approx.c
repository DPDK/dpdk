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

#include <stdlib.h>

#include "rte_approx.h"

/*
 * Based on paper "Approximating Rational Numbers by Fractions" by Michal
 * Forisek forisek@dcs.fmph.uniba.sk
 *
 * Given a rational number alpha with 0 < alpha < 1 and a precision d, the goal
 * is to find positive integers p, q such that alpha - d < p/q < alpha + d, and
 * q is minimal.
 *
 * http://people.ksp.sk/~misof/publications/2007approx.pdf
 */

/* fraction comparison: compare (a/b) and (c/d) */
static inline uint32_t
less(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
	return a*d < b*c;
}

static inline uint32_t
less_or_equal(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
	return a*d <= b*c;
}

/* check whether a/b is a valid approximation */
static inline uint32_t
matches(uint32_t a, uint32_t b,
	uint32_t alpha_num, uint32_t d_num, uint32_t denum)
{
	if (less_or_equal(a, b, alpha_num - d_num, denum))
		return 0;

	if (less(a ,b, alpha_num + d_num, denum))
		return 1;

	return 0;
}

static inline void
find_exact_solution_left(uint32_t p_a, uint32_t q_a, uint32_t p_b, uint32_t q_b,
	uint32_t alpha_num, uint32_t d_num, uint32_t denum, uint32_t *p, uint32_t *q)
{
	uint32_t k_num = denum * p_b - (alpha_num + d_num) * q_b;
	uint32_t k_denum = (alpha_num + d_num) * q_a - denum * p_a;
	uint32_t k = (k_num / k_denum) + 1;

	*p = p_b + k * p_a;
	*q = q_b + k * q_a;
}

static inline void
find_exact_solution_right(uint32_t p_a, uint32_t q_a, uint32_t p_b, uint32_t q_b,
	uint32_t alpha_num, uint32_t d_num, uint32_t denum, uint32_t *p, uint32_t *q)
{
	uint32_t k_num = - denum * p_b + (alpha_num - d_num) * q_b;
	uint32_t k_denum = - (alpha_num - d_num) * q_a + denum * p_a;
	uint32_t k = (k_num / k_denum) + 1;

	*p = p_b + k * p_a;
	*q = q_b + k * q_a;
}

static int
find_best_rational_approximation(uint32_t alpha_num, uint32_t d_num, uint32_t denum, uint32_t *p, uint32_t *q)
{
	uint32_t p_a, q_a, p_b, q_b;

	/* check assumptions on the inputs */
	if (!((0 < d_num) && (d_num < alpha_num) && (alpha_num < denum) && (d_num + alpha_num < denum))) {
		return -1;
	}

	/* set initial bounds for the search */
	p_a = 0;
	q_a = 1;
	p_b = 1;
	q_b = 1;

	while (1) {
		uint32_t new_p_a, new_q_a, new_p_b, new_q_b;
		uint32_t x_num, x_denum, x;
		int aa, bb;

		/* compute the number of steps to the left */
		x_num = denum * p_b - alpha_num * q_b;
		x_denum = - denum * p_a + alpha_num * q_a;
		x = (x_num + x_denum - 1) / x_denum; /* x = ceil(x_num / x_denum) */

		/* check whether we have a valid approximation */
		aa = matches(p_b + x * p_a, q_b + x * q_a, alpha_num, d_num, denum);
		bb = matches(p_b + (x-1) * p_a, q_b + (x - 1) * q_a, alpha_num, d_num, denum);
		if (aa || bb) {
			find_exact_solution_left(p_a, q_a, p_b, q_b, alpha_num, d_num, denum, p, q);
			return 0;
		}

		/* update the interval */
		new_p_a = p_b + (x - 1) * p_a ;
		new_q_a = q_b + (x - 1) * q_a;
		new_p_b = p_b + x * p_a ;
		new_q_b = q_b + x * q_a;

		p_a = new_p_a ;
		q_a = new_q_a;
		p_b = new_p_b ;
		q_b = new_q_b;

		/* compute the number of steps to the right */
		x_num = alpha_num * q_b - denum * p_b;
		x_denum = - alpha_num * q_a + denum * p_a;
		x = (x_num + x_denum - 1) / x_denum; /* x = ceil(x_num / x_denum) */

		/* check whether we have a valid approximation */
		aa = matches(p_b + x * p_a, q_b + x * q_a, alpha_num, d_num, denum);
		bb = matches(p_b + (x - 1) * p_a, q_b + (x - 1) * q_a, alpha_num, d_num, denum);
		if (aa || bb) {
			find_exact_solution_right(p_a, q_a, p_b, q_b, alpha_num, d_num, denum, p, q);
			return 0;
		 }

		/* update the interval */
		new_p_a = p_b + (x - 1) * p_a;
		new_q_a = q_b + (x - 1) * q_a;
		new_p_b = p_b + x * p_a;
		new_q_b = q_b + x * q_a;

		p_a = new_p_a;
		q_a = new_q_a;
		p_b = new_p_b;
		q_b = new_q_b;
	}
}

int rte_approx(double alpha, double d, uint32_t *p, uint32_t *q)
{
	uint32_t alpha_num, d_num, denum;

	/* Check input arguments */
	if (!((0.0 < d) && (d < alpha) && (alpha < 1.0))) {
		return -1;
	}

	if ((p == NULL) || (q == NULL)) {
		return -2;
	}

	/* Compute alpha_num, d_num and denum */
	denum = 1;
	while (d < 1) {
		alpha *= 10;
		d *= 10;
		denum *= 10;
	}
	alpha_num = (uint32_t) alpha;
	d_num = (uint32_t) d;

	/* Perform approximation */
	return find_best_rational_approximation(alpha_num, d_num, denum, p, q);
}
