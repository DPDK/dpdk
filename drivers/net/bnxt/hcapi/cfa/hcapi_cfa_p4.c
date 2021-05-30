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

#include "hcapi_cfa.h"
#include "hcapi_cfa_defs.h"

static uint32_t hcapi_cfa_lkup_lkup3_init_cfg;
static uint32_t hcapi_cfa_lkup_em_seed_mem[HCAPI_CFA_LKUP_SEED_MEM_SIZE];
static bool hcapi_cfa_lkup_init;

static inline uint32_t SWAP_WORDS32(uint32_t val32)
{
	return (((val32 & 0x0000ffff) << 16) | ((val32 & 0xffff0000) >> 16));
}

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
	hcapi_cfa_lkup_lkup3_init_cfg = SWAP_WORDS32(rand32());

	for (i = 0; i < HCAPI_CFA_LKUP_SEED_MEM_SIZE / 2; i++) {
		r = SWAP_WORDS32(rand32());
		hcapi_cfa_lkup_em_seed_mem[i * 2] = r;
		r = SWAP_WORDS32(rand32());
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

	for (i = CFA_P4_EEM_KEY_MAX_SIZE - 2; i >= 0; i--) {
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

	val1 = hcapi_cfa_crc32i(~val1, (key - (CFA_P4_EEM_KEY_MAX_SIZE - 1)),
				CFA_P4_EEM_KEY_MAX_SIZE);

	/* End with seed */
	if (val2 & 0x1)
		val1 = hcapi_cfa_crc32i(~val1, temp, 4);

	return val1;
}

static uint32_t hcapi_cfa_lookup3_hash(uint8_t *in_key)
{
	uint32_t val1;

	val1 = hashword(((uint32_t *)in_key) + 1,
			CFA_P4_EEM_KEY_MAX_SIZE / (sizeof(uint32_t)),
			hcapi_cfa_lkup_lkup3_init_cfg);

	return val1;
}

uint64_t hcapi_get_table_page(struct hcapi_cfa_em_table *mem, uint32_t page)
{
	int level = 0;
	uint64_t addr;

	if (mem == NULL)
		return 0;

	/*
	 * Use the level according to the num_level of page table
	 */
	level = mem->num_lvl - 1;

	addr = (uint64_t)mem->pg_tbl[level].pg_va_tbl[page];

	return addr;
}

/** Approximation of HCAPI hcapi_cfa_key_hash()
 *
 * Return:
 *
 */
uint64_t hcapi_cfa_p4_key_hash(uint64_t *key_data,
			       uint16_t bitlen)
{
	uint32_t key0_hash;
	uint32_t key1_hash;

	/*
	 * Init the seeds if needed
	 */
	if (!hcapi_cfa_lkup_init)
		hcapi_cfa_seeds_init();

	key0_hash =
		hcapi_cfa_crc32_hash(((uint8_t *)key_data) + (bitlen / 8) - 1);

	key1_hash = hcapi_cfa_lookup3_hash((uint8_t *)key_data);

	return ((uint64_t)key0_hash) << 32 | (uint64_t)key1_hash;
}

static int hcapi_cfa_p4_key_hw_op_put(struct hcapi_cfa_hwop *op,
				      struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;

	memcpy((uint8_t *)(uintptr_t)op->hw.base_addr + key_obj->offset,
	       key_obj->data, key_obj->size);

	return rc;
}

static int hcapi_cfa_p4_key_hw_op_get(struct hcapi_cfa_hwop *op,
				      struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;

	memcpy(key_obj->data,
	       (uint8_t *)(uintptr_t)op->hw.base_addr + key_obj->offset,
	       key_obj->size);

	return rc;
}

static int hcapi_cfa_p4_key_hw_op_add(struct hcapi_cfa_hwop *op,
				      struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;
	struct cfa_p4_eem_64b_entry table_entry;

	/*
	 * Is entry free?
	 */
	memcpy(&table_entry,
	       (uint8_t *)(uintptr_t)op->hw.base_addr + key_obj->offset,
	       key_obj->size);

	/*
	 * If this is entry is valid then report failure
	 */
	if (table_entry.hdr.word1 & (1 << CFA_P4_EEM_ENTRY_VALID_SHIFT))
		return -1;

	memcpy((uint8_t *)(uintptr_t)op->hw.base_addr + key_obj->offset,
	       key_obj->data, key_obj->size);

	return rc;
}

static int hcapi_cfa_p4_key_hw_op_del(struct hcapi_cfa_hwop *op,
				      struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;
	struct cfa_p4_eem_64b_entry table_entry;

	/*
	 * Read entry
	 */
	memcpy(&table_entry,
	       (uint8_t *)(uintptr_t)op->hw.base_addr + key_obj->offset,
	       key_obj->size);

	/*
	 * If this is not a valid entry then report failure.
	 */
	if (table_entry.hdr.word1 & (1 << CFA_P4_EEM_ENTRY_VALID_SHIFT)) {
		/*
		 * If a key has been provided then verify the key matches
		 * before deleting the entry.
		 */
		if (key_obj->data != NULL) {
			if (memcmp(&table_entry, key_obj->data,
				   key_obj->size) != 0)
				return -1;
		}
	} else {
		return -1;
	}

	/*
	 * Delete entry
	 */
	memset((uint8_t *)(uintptr_t)op->hw.base_addr + key_obj->offset, 0, key_obj->size);

	return rc;
}

/** Approximation of hcapi_cfa_key_hw_op()
 *
 *
 */
static int hcapi_cfa_p4_key_hw_op(struct hcapi_cfa_hwop *op,
				  struct hcapi_cfa_key_tbl *key_tbl,
				  struct hcapi_cfa_key_data *key_obj,
				  struct hcapi_cfa_key_loc *key_loc)
{
	int rc = 0;
	struct hcapi_cfa_em_table *em_tbl;
	uint32_t page;

	if (op == NULL || key_tbl == NULL || key_obj == NULL || key_loc == NULL)
		return -1;

	page = key_obj->offset / key_tbl->page_size;
	em_tbl = (struct hcapi_cfa_em_table *)key_tbl->base0;
	op->hw.base_addr = hcapi_get_table_page(em_tbl, page);
	/* Offset is adjusted to be the offset into the page */
	key_obj->offset = key_obj->offset % key_tbl->page_size;

	if (op->hw.base_addr == 0)
		return -1;

	switch (op->opcode) {
	case HCAPI_CFA_HWOPS_PUT: /**< Write to HW operation */
		rc = hcapi_cfa_p4_key_hw_op_put(op, key_obj);
		break;
	case HCAPI_CFA_HWOPS_GET: /**< Read from HW operation */
		rc = hcapi_cfa_p4_key_hw_op_get(op, key_obj);
		break;
	case HCAPI_CFA_HWOPS_ADD:
		/**< For operations which require more then simple
		 * writes to HW, this operation is used.  The
		 * distinction with this operation when compared
		 * to the PUT ops is that this operation is used
		 * in conjunction with the HCAPI_CFA_HWOPS_DEL
		 * op to remove the operations issued by the
		 * ADD OP.
		 */

		rc = hcapi_cfa_p4_key_hw_op_add(op, key_obj);

		break;
	case HCAPI_CFA_HWOPS_DEL:
		rc = hcapi_cfa_p4_key_hw_op_del(op, key_obj);
		break;
	default:
		rc = -1;
		break;
	}

	return rc;
}

const struct hcapi_cfa_devops cfa_p4_devops = {
	.hcapi_cfa_key_hash = hcapi_cfa_p4_key_hash,
	.hcapi_cfa_key_hw_op = hcapi_cfa_p4_key_hw_op,
};
