/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */
#include <eal_export.h>
#include <rte_random.h>
#include <rte_common.h>
#include <rte_bitops.h>
#include <rte_debug.h>
#include <rte_thash.h>
#include <rte_log.h>

#define MAX_POLY_DEGREE 32
#define MAX_TOEPLITZ_KEY_LENGTH 64
RTE_LOG_REGISTER_SUFFIX(thash_poly_logtype, thash_poly, INFO);
#define RTE_LOGTYPE_HASH thash_poly_logtype
#define HASH_LOG(level, ...) \
	RTE_LOG_LINE(level, HASH, "" __VA_ARGS__)

/*
 * Finite field math for field extensions with irreducing polynomial
 * of degree <= 32.
 * Polynomials are represented with 2 arguments - uint32_t poly and int degree.
 * poly argument does not hold the highest degree coefficient,
 * so full polynomial can be expressed as poly|(1ULL << degree).
 * The algorithm to produce random irreducible polynomial was inspired by:
 * "Computation in finite fields" by John Kerl, Chapter 10.4.
 */

static const uint32_t irreducible_poly_table[][3] = {
	{0, 0, 0},		/* degree 0 */
	{0x1, 0x1, 0x1},	/* degree 1 */
	{0x3, 0x3, 0x3},	/* degree 2 and so on.. */
	{0x3, 0x5, 0x3},	/* x^3+x^2+1(0x5) is reverted x^3+x+1(0x3) */
	{0x3, 0x9, 0x3},	/* x^4+x^3+1(0x9) is reverted x^4+x+1(0x3) */
	{0x5, 0xf, 0x17},	/* 0x5 <-> 0x9; 0xf <-> 0x1d; 0x17 <-> 0x1b */
	{0x3, 0x27, 0x1b},	/* 0x3 <-> 0x21; 0x27 <-> 0x33; 0x1b <-> 0x2d*/
};

/*
 * Table of the monic lowest weight irreducible polynomials over GF(2)
 * starting from degree 7 up to degree 32.
 * Taken from Handbook of Finite Fields by Gary L. Mullen and
 * Daniel Penario p.33 Table 2.2.1.
 * https://people.math.carleton.ca/~daniel/hff/irred/F2-2to10000.txt
 */
static const uint32_t default_irreducible_poly[] = {
	0x3,    /* x^7 + x + 1*/
	0x1b,   /* x^8 + x^4 + x^3 + x + 1 */
	0x3,    /* x^9 + x + 1*/
	0x9,    /* x^10 + x^3 + 1 */
	0x5,    /* x^11 + x^2 + 1 */
	0x9,    /* x^12 + x^3 + 1 */
	0x1b,   /* x^13 + x^4 + x^3 + x + 1 */
	0x33,   /* x^14 + x^5 + 1 */
	0x3,    /* x^15 + x + 1 */
	0x2b,   /* x^16 + x^5 + x^3 + x + 1 */
	0x9,    /* x^17 + x^3 + 1 */
	0x9,    /* x^18 + x^3 + 1 */
	0x27,   /* x^19 + x^5 + x^2 + x + 1 */
	0x9,    /* x^20 + x^3 + 1 */
	0x5,    /* x^21 + x^2 + 1 */
	0x3,    /* x^22 + x + 1 */
	0x21,   /* x^23 + x^5 + 1 */
	0x1b,   /* x^24 + x^4 + x^3 + x + 1 */
	0x9,    /* x^25 + x^3 + 1 */
	0x1b,   /* x^26 + x^4 + x^3 + x + 1 */
	0x27,   /* x^27 + x^5 + x^2 + x + 1 */
	0x3,    /* x^28 + x + 1 */
	0x5,    /* x^29 + x^2 + 1 */
	0x3,    /* x^30 + x + 1 */
	0x9,    /* x^31 + x^3 + 1 */
	0x8d,   /* x^32 + x^7 + x^3 + x^2 + 1 */
};

#define MAX_DIVISORS 28 /* 2^24 - 1 */

struct divisors {
	uint32_t n; /* number of divisors */
	uint32_t div_arr[MAX_DIVISORS];
};

/* divisors of (2^n - 1) less than MIN(512, 2^n - 1) for all n in [7, 32] */
static const struct divisors divisors[] = {
	{ .n = 0, .div_arr = {} }, /* 2^7-1 is Mersenne prime */
	{ .n = 6, .div_arr = {3, 5, 15, 17, 51, 85} },
	{ .n = 2, .div_arr = {7, 73} },
	{ .n = 6, .div_arr = {3, 11, 31, 33, 93, 341} },
	{ .n = 2, .div_arr = {23, 89} },
	{ .n = 19, .div_arr = {3, 5, 7, 9, 13, 15, 21, 35, 39, 45, 63, 65, 91,
		105, 117, 195, 273, 315, 455} },
	{ .n = 0, .div_arr = {} }, /* 2^13-1 is Mersenne prime */
	{ .n = 5, .div_arr = {3, 43, 127, 129, 381} },
	{ .n = 4, .div_arr = {7, 31, 151, 217} },
	{ .n = 8, .div_arr = {3, 5, 15, 17, 51, 85, 255, 257} },
	{ .n = 0, .div_arr = {} }, /* 2^17-1 is Mersenne prime */
	{ .n = 14, .div_arr = {3, 7, 9, 19, 21, 27, 57, 63, 73, 133, 171, 189,
		219, 399} },
	{ .n = 0, .div_arr = {0} }, /* 2^19-1 is Mersenne prime */
	{ .n = 19, .div_arr = {3, 5, 11, 15, 25, 31, 33, 41, 55, 75, 93, 123,
		155, 165, 205, 275, 341, 451, 465} },
	{ .n = 4, .div_arr = {7, 49, 127, 337} },
	{ .n = 5, .div_arr = {3, 23, 69, 89, 267} },
	{ .n = 1, .div_arr = {47} },
	{ .n = 28, .div_arr = {3, 5, 7, 9, 13, 15, 17, 21, 35, 39, 45, 51, 63,
		65, 85, 91, 105, 117, 119, 153, 195, 221, 241, 255, 273, 315,
		357, 455} },
	{ .n = 1, .div_arr = {31} },
	{ .n = 1, .div_arr = {3} },
	{ .n = 2, .div_arr = {7, 73} },
	{ .n = 14, .div_arr = {3, 5, 15, 29, 43, 87, 113, 127, 129, 145, 215,
		339, 381, 435} },
	{ .n = 1, .div_arr = {233} },
	{ .n = 18, .div_arr = {3, 7, 9, 11, 21, 31, 33, 63, 77, 93, 99, 151,
		217, 231, 279, 331, 341, 453} },
	{ .n = 0, .div_arr = {} },/* 2^31-1 is Mersenne prime */
	{ .n = 8, .div_arr = {3, 5, 15, 17, 51, 85, 255, 257} },
};

static uint32_t
gf2_mul(uint32_t a, uint32_t b, uint32_t r, int degree)
{
	uint64_t product = 0;
	uint64_t r_poly = r | RTE_BIT64(degree);

	for (; b; b &= (b - 1))
		product ^= (uint64_t)a << (rte_bsf32(b));

	for (int i = degree * 2 - 1; i >= degree; i--)
		if (product & RTE_BIT64(i))
			product ^= r_poly << (i - degree);

	return (uint32_t)product;
}

static uint32_t
gf2_pow(uint32_t a, uint32_t pow, uint32_t r, int degree)
{
	uint32_t result = 1;
	unsigned int i;

	for (i = 0; i < (sizeof(pow)*CHAR_BIT - rte_clz32(pow)); i++) {
		if (pow & (1 << i))
			result = gf2_mul(result, a, r, degree);

		a = gf2_mul(a, a, r, degree);
	}

	return result;
}

static uint32_t
__thash_get_rand_poly(int poly_degree)
{
	uint32_t roots[MAX_POLY_DEGREE];
	uint32_t rnd;
	uint32_t ret_poly = 0;
	int i, j;
	bool short_orbit = false;

	/* special case for low degree */
	if (poly_degree < 7)
		return irreducible_poly_table[poly_degree][rte_rand() %
			RTE_DIM(irreducible_poly_table[poly_degree])];

	uint32_t r = default_irreducible_poly[poly_degree - 7];

	do {
		short_orbit = false;
		do {
			rnd = rte_rand() & ((1 << poly_degree) - 1);
		} while ((rnd == 0) || (rnd == 1));

		/*
		 * Quick check if random returned one of the roots of
		 * the initial polynomial.
		 * In other words if we randomy got x, x^2, x^4, x^8 or x^16
		 */
#define ROOT_POLY_MSK ((1 << 1)|(1 << 2)|(1 << 4)|(1 << 8)|(1 << 16))
		if ((rte_popcount32(rnd) == 1) && (rnd & ROOT_POLY_MSK))
			return default_irreducible_poly[poly_degree - 7];

		/*
		 * init array with some random polynomial roots
		 * applying Frobenius automorphism (i.e. squaring them)
		 * also checking for short orbits (i.e. if there are repeated roots)
		 */
		roots[0] = rnd;
		for (i = 1; i < poly_degree; i++) {
			roots[i] = gf2_pow(roots[i - 1], 2, r, poly_degree);
			if (roots[i] == roots[0])
				short_orbit = true;
		}
	} while (short_orbit);

	/*
	 * Get coefficients of the polynomial for
	 * (x - roots[0])(x - roots[1])...(x - roots[n])
	 */
	uint32_t poly_coefficients[MAX_POLY_DEGREE + 1] = {0};

	poly_coefficients[0] = 1; /* highest degree term coefficient in the end */
	for (i = 0; i < (int)poly_degree; i++) {
		/* multiply by x */
		for (j = i; j >= 0; j--)
			poly_coefficients[j + 1] = poly_coefficients[j];

		poly_coefficients[0] = 0;

		/* multiply by root */
		for (j = 0; j <= i; j++)
			poly_coefficients[j] ^=
				gf2_mul(poly_coefficients[j + 1],
				roots[i], r, poly_degree);
	}

	for (i = 0; i < poly_degree; i++) {
		if (poly_coefficients[i]) {
			RTE_ASSERT(poly_coefficients[i] == 1);
			ret_poly |= 1 << i;
		}
	}

	return ret_poly;
}

/* test an order of the multiplicative subgroup generated by x */
static int
thash_test_poly_order(uint32_t poly, int degree)
{
	unsigned int i;
	int div_idx = degree - 7;

	if (degree < 7)
		return 0;

	for (i = 0; i < divisors[div_idx].n; i++) {
		if (gf2_pow(0x2, divisors[div_idx].div_arr[i],
				poly, degree) == 1)
			return 1;
	}

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(thash_get_rand_poly)
uint32_t
thash_get_rand_poly(uint32_t poly_degree)
{
	uint32_t ret_poly;

	if (poly_degree > MAX_POLY_DEGREE) {
		HASH_LOG(ERR, "Wrong polynomial degree %d, must be in range [1, 32]", poly_degree);
		return 0;
	}

	do
		ret_poly = __thash_get_rand_poly(poly_degree);
	while (thash_test_poly_order(ret_poly, poly_degree));

	return ret_poly;
}
