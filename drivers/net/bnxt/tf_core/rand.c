/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

/* Random Number Functions */

#include <stdio.h>
#include <stdint.h>
#include "rand.h"

#define TF_RAND_LFSR_INIT_VALUE 0xACE1u

uint16_t lfsr = TF_RAND_LFSR_INIT_VALUE;
uint32_t bit;

/**
 * Generates a 16 bit pseudo random number
 *
 * Returns:
 *   uint16_t number
 */
uint16_t rand16(void)
{
	bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
	return lfsr = (lfsr >> 1) | (bit << 15);
}

/**
 * Generates a 32 bit pseudo random number
 *
 * Returns:
 *   uint32_t number
 */
uint32_t rand32(void)
{
	return (rand16() << 16) | rand16();
}

/**
 * Resets the seed used by the pseudo random number generator
 */
void rand_init(void)
{
	lfsr = TF_RAND_LFSR_INIT_VALUE;
	bit = 0;
}
