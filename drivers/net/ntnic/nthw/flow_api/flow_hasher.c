/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <math.h>

#include "flow_hasher.h"

static uint32_t shuffle(uint32_t x)
{
	return ((x & 0x00000002) << 29) | ((x & 0xAAAAAAA8) >> 3) | ((x & 0x15555555) << 3) |
		((x & 0x40000000) >> 29);
}

static uint32_t ror_inv(uint32_t x, const int s)
{
	return (x >> s) | ((~x) << (32 - s));
}

static uint32_t combine(uint32_t x, uint32_t y)
{
	uint32_t x1 = ror_inv(x, 15);
	uint32_t x2 = ror_inv(x, 13);
	uint32_t y1 = ror_inv(y, 3);
	uint32_t y2 = ror_inv(y, 27);

	return x ^ y ^
		((x1 & y1 & ~x2 & ~y2) | (x1 & ~y1 & x2 & ~y2) | (x1 & ~y1 & ~x2 & y2) |
		(~x1 & y1 & x2 & ~y2) | (~x1 & y1 & ~x2 & y2) | (~x1 & ~y1 & x2 & y2));
}

static uint32_t mix(uint32_t x, uint32_t y)
{
	return shuffle(combine(x, y));
}

static uint64_t ror_inv3(uint64_t x)
{
	const uint64_t m = 0xE0000000E0000000ULL;

	return ((x >> 3) | m) ^ ((x << 29) & m);
}

static uint64_t ror_inv13(uint64_t x)
{
	const uint64_t m = 0xFFF80000FFF80000ULL;

	return ((x >> 13) | m) ^ ((x << 19) & m);
}

static uint64_t ror_inv15(uint64_t x)
{
	const uint64_t m = 0xFFFE0000FFFE0000ULL;

	return ((x >> 15) | m) ^ ((x << 17) & m);
}

static uint64_t ror_inv27(uint64_t x)
{
	const uint64_t m = 0xFFFFFFE0FFFFFFE0ULL;

	return ((x >> 27) | m) ^ ((x << 5) & m);
}

static uint64_t shuffle64(uint64_t x)
{
	return ((x & 0x0000000200000002) << 29) | ((x & 0xAAAAAAA8AAAAAAA8) >> 3) |
		((x & 0x1555555515555555) << 3) | ((x & 0x4000000040000000) >> 29);
}

static uint64_t pair(uint32_t x, uint32_t y)
{
	return ((uint64_t)x << 32) | y;
}

static uint64_t combine64(uint64_t x, uint64_t y)
{
	uint64_t x1 = ror_inv15(x);
	uint64_t x2 = ror_inv13(x);
	uint64_t y1 = ror_inv3(y);
	uint64_t y2 = ror_inv27(y);

	return x ^ y ^
		((x1 & y1 & ~x2 & ~y2) | (x1 & ~y1 & x2 & ~y2) | (x1 & ~y1 & ~x2 & y2) |
		(~x1 & y1 & x2 & ~y2) | (~x1 & y1 & ~x2 & y2) | (~x1 & ~y1 & x2 & y2));
}

static uint64_t mix64(uint64_t x, uint64_t y)
{
	return shuffle64(combine64(x, y));
}

static uint32_t calc16(const uint32_t key[16])
{
	/*
	 * 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15   Layer 0
	 *  \./     \./     \./     \./     \./     \./     \./     \./
	 *   0       1       2       3       4       5       6       7     Layer 1
	 *    \__.__/         \__.__/         \__.__/         \__.__/
	 *       0               1               2               3         Layer 2
	 *        \______.______/                 \______.______/
	 *               0                               1                 Layer 3
	 *                \______________.______________/
	 *                               0                                 Layer 4
	 *                              / \
	 *                              \./
	 *                               0                                 Layer 5
	 *                              / \
	 *                              \./                                Layer 6
	 *                             value
	 */

	uint64_t z;
	uint32_t x;

	z = mix64(mix64(mix64(pair(key[0], key[8]), pair(key[1], key[9])),
		mix64(pair(key[2], key[10]), pair(key[3], key[11]))),
		mix64(mix64(pair(key[4], key[12]), pair(key[5], key[13])),
		mix64(pair(key[6], key[14]), pair(key[7], key[15]))));

	x = mix((uint32_t)(z >> 32), (uint32_t)z);
	x = mix(x, ror_inv(x, 17));
	x = combine(x, ror_inv(x, 17));

	return x;
}

uint32_t nthw_gethash(struct hasher_s *hsh, const uint32_t key[16], int *result)
{
	uint64_t val;
	uint32_t res;

	val = calc16(key);
	res = (uint32_t)val;

	if (hsh->cam_bw > 32)
		val = (val << (hsh->cam_bw - 32)) ^ val;

	for (int i = 0; i < hsh->banks; i++) {
		result[i] = (unsigned int)(val & hsh->cam_records_bw_mask);
		val = val >> hsh->cam_records_bw;
	}

	return res;
}

int nthw_init_hasher(struct hasher_s *hsh, int banks, int nb_records)
{
	hsh->banks = banks;
	hsh->cam_records_bw = (int)(log2(nb_records - 1) + 1);
	hsh->cam_records_bw_mask = (1U << hsh->cam_records_bw) - 1;
	hsh->cam_bw = hsh->banks * hsh->cam_records_bw;

	return 0;
}
