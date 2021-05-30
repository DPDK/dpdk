/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "lookup3.h"
#include "rand.h"

#include "hcapi_cfa_defs.h"

static uint32_t hcapi_cfa_lkup_lkup3_init_cfg;
static uint32_t hcapi_cfa_lkup_em_seed_mem[HCAPI_CFA_LKUP_SEED_MEM_SIZE];
static bool hcapi_cfa_lkup_init;

static void hcapi_cfa_seeds_init(void)
{
	int i;
	uint32_t r;

	if (hcapi_cfa_lkup_init)
		return;

	hcapi_cfa_lkup_init = true;

	/* Initialize the lfsr */
	rand_init();

	/* RX and TX use the same seed values */
	hcapi_cfa_lkup_lkup3_init_cfg = rand32();

	for (i = 0; i < HCAPI_CFA_LKUP_SEED_MEM_SIZE / 2; i++) {
		r = rand32();
		hcapi_cfa_lkup_em_seed_mem[i * 2] = r;
		r = rand32();
		hcapi_cfa_lkup_em_seed_mem[i * 2 + 1] = (r & 0x1);
	}
}

static uint32_t hcapi_cfa_crc32_hash(uint8_t *key)
{
	int i;
	uint32_t index;
	uint32_t val1, val2;
	uint8_t temp[4];
	uint8_t *kptr = key;

	/* Do byte-wise XOR of the 52-byte HASH key first. */
	index = *key;
	kptr--;

	for (i = CFA_P58_EEM_KEY_MAX_SIZE - 2; i >= 0; i--) {
		index = index ^ *kptr;
		kptr--;
	}

	/* Get seeds */
	val1 = hcapi_cfa_lkup_em_seed_mem[index * 2];
	val2 = hcapi_cfa_lkup_em_seed_mem[index * 2 + 1];

	temp[3] = (uint8_t)(val1 >> 24);
	temp[2] = (uint8_t)(val1 >> 16);
	temp[1] = (uint8_t)(val1 >> 8);
	temp[0] = (uint8_t)(val1 & 0xff);
	val1 = 0;

	/* Start with seed */
	if (!(val2 & 0x1))
		val1 = hcapi_cfa_crc32i(~val1, temp, 4);

	val1 = hcapi_cfa_crc32i(~val1,
		      (key - (CFA_P58_EEM_KEY_MAX_SIZE - 1)),
		      CFA_P58_EEM_KEY_MAX_SIZE);

	/* End with seed */
	if (val2 & 0x1)
		val1 = hcapi_cfa_crc32i(~val1, temp, 4);

	return val1;
}

static uint32_t hcapi_cfa_lookup3_hash(uint8_t *in_key)
{
	uint32_t val1;

	val1 = hashword(((uint32_t *)in_key),
			 CFA_P58_EEM_KEY_MAX_SIZE / (sizeof(uint32_t)),
			 hcapi_cfa_lkup_lkup3_init_cfg);

	return val1;
}


/** Approximation of HCAPI hcapi_cfa_key_hash()
 *
 * Return:
 *
 */
uint64_t hcapi_cfa_p58_key_hash(uint64_t *key_data,
				uint16_t bitlen)
{
	uint32_t key0_hash;
	uint32_t key1_hash;

	/*
	 * Init the seeds if needed
	 */
	if (!hcapi_cfa_lkup_init)
		hcapi_cfa_seeds_init();

	key0_hash = hcapi_cfa_crc32_hash(((uint8_t *)key_data) +
					      (bitlen / 8) - 1);

	key1_hash = hcapi_cfa_lookup3_hash((uint8_t *)key_data);

	return ((uint64_t)key0_hash) << 32 | (uint64_t)key1_hash;
}
