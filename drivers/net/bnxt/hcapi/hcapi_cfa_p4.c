/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "lookup3.h"
#include "rand.h"

#include "hcapi_cfa_defs.h"

#define HCAPI_CFA_LKUP_SEED_MEM_SIZE 512
uint32_t hcapi_cfa_lkup_lkup3_init_cfg;
uint32_t hcapi_cfa_lkup_em_seed_mem[HCAPI_CFA_LKUP_SEED_MEM_SIZE];
bool hcapi_cfa_lkup_init;

static inline uint32_t SWAP_WORDS32(uint32_t val32)
{
	return (((val32 & 0x0000ffff) << 16) |
		((val32 & 0xffff0000) >> 16));
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

/* CRC32i support for Key0 hash */
#define ucrc32(ch, crc) (crc32tbl[((crc) ^ (ch)) & 0xff] ^ ((crc) >> 8))
#define crc32(x, y) crc32i(~0, x, y)

static const uint32_t crc32tbl[] = {	/* CRC polynomial 0xedb88320 */
0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t hcapi_cfa_crc32i(uint32_t crc, const uint8_t *buf, size_t len)
{
	int l;

#ifdef TF_EEM_DEBUG
	TFP_DRV_LOG(DEBUG, "CRC2:");
#endif
	for (l = (len - 1); l >= 0; l--) {
		crc = ucrc32(buf[l], crc);
#ifdef TF_EEM_DEBUG
		TFP_DRV_LOG(DEBUG,
			    "%02X %08X %08X\n",
			    (buf[l] & 0xff),
			    crc,
			    ~crc);
#endif
	}

#ifdef TF_EEM_DEBUG
	TFP_DRV_LOG(DEBUG, "\n");
#endif

	return ~crc;
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

	val1 = hcapi_cfa_crc32i(~val1,
		      (key - (CFA_P4_EEM_KEY_MAX_SIZE - 1)),
		      CFA_P4_EEM_KEY_MAX_SIZE);

	/* End with seed */
	if (val2 & 0x1)
		val1 = hcapi_cfa_crc32i(~val1, temp, 4);

	return val1;
}

static uint32_t hcapi_cfa_lookup3_hash(uint8_t *in_key)
{
	uint32_t val1;

	val1 = hashword(((const uint32_t *)(uintptr_t *)in_key) + 1,
			 CFA_P4_EEM_KEY_MAX_SIZE / (sizeof(uint32_t)),
			 hcapi_cfa_lkup_lkup3_init_cfg);

	return val1;
}


uint64_t hcapi_get_table_page(struct hcapi_cfa_em_table *mem,
			      uint32_t page)
{
	int level = 0;
	uint64_t addr;

	if (mem == NULL)
		return 0;

	/*
	 * Use the level according to the num_level of page table
	 */
	level = mem->num_lvl - 1;

	addr = (uintptr_t)mem->pg_tbl[level].pg_va_tbl[page];

	return addr;
}

/** Approximation of HCAPI hcapi_cfa_key_hash()
 *
 * Return:
 *
 */
uint64_t hcapi_cfa_key_hash(uint64_t *key_data,
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

static int hcapi_cfa_key_hw_op_put(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;

	memcpy((uint8_t *)(uintptr_t)op->hw.base_addr +
	       key_obj->offset,
	       key_obj->data,
	       key_obj->size);

	return rc;
}

static int hcapi_cfa_key_hw_op_get(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;

	memcpy(key_obj->data,
	       (uint8_t *)(uintptr_t)op->hw.base_addr +
	       key_obj->offset,
	       key_obj->size);

	return rc;
}

static int hcapi_cfa_key_hw_op_add(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;
	struct cfa_p4_eem_64b_entry table_entry;

	/*
	 * Is entry free?
	 */
	memcpy(&table_entry,
	       (uint8_t *)(uintptr_t)op->hw.base_addr +
	       key_obj->offset,
	       key_obj->size);

	/*
	 * If this is entry is valid then report failure
	 */
	if (table_entry.hdr.word1 & (1 << CFA_P4_EEM_ENTRY_VALID_SHIFT))
		return -1;

	memcpy((uint8_t *)(uintptr_t)op->hw.base_addr +
	       key_obj->offset,
	       key_obj->data,
	       key_obj->size);

	return rc;
}

static int hcapi_cfa_key_hw_op_del(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_key_data *key_obj)
{
	int rc = 0;
	struct cfa_p4_eem_64b_entry table_entry;

	/*
	 * Read entry
	 */
	memcpy(&table_entry,
	       (uint8_t *)(uintptr_t)op->hw.base_addr +
	       key_obj->offset,
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
			if (memcmp(&table_entry,
				   key_obj->data,
				   key_obj->size) != 0)
				return -1;
		}
	} else {
		return -1;
	}


	/*
	 * Delete entry
	 */
	memset((uint8_t *)(uintptr_t)op->hw.base_addr +
	       key_obj->offset,
	       0,
	       key_obj->size);

	return rc;
}


/** Apporiximation of hcapi_cfa_key_hw_op()
 *
 *
 */
int hcapi_cfa_key_hw_op(struct hcapi_cfa_hwop *op,
			struct hcapi_cfa_key_tbl *key_tbl,
			struct hcapi_cfa_key_data *key_obj,
			struct hcapi_cfa_key_loc *key_loc)
{
	int rc = 0;

	if (op == NULL ||
	    key_tbl == NULL ||
	    key_obj == NULL ||
	    key_loc == NULL)
		return -1;

	op->hw.base_addr =
		hcapi_get_table_page((struct hcapi_cfa_em_table *)
				     key_tbl->base0,
				     key_obj->offset / key_tbl->page_size);
	/* Offset is adjusted to be the offset into the page */
	key_obj->offset = key_obj->offset % key_tbl->page_size;

	if (op->hw.base_addr == 0)
		return -1;

	switch (op->opcode) {
	case HCAPI_CFA_HWOPS_PUT: /**< Write to HW operation */
		rc = hcapi_cfa_key_hw_op_put(op, key_obj);
		break;
	case HCAPI_CFA_HWOPS_GET: /**< Read from HW operation */
		rc = hcapi_cfa_key_hw_op_get(op, key_obj);
		break;
	case HCAPI_CFA_HWOPS_ADD:
		/**< For operations which require more than
		 * simple writes to HW, this operation is used. The
		 * distinction with this operation when compared
		 * to the PUT ops is that this operation is used
		 * in conjunction with the HCAPI_CFA_HWOPS_DEL
		 * op to remove the operations issued by the
		 * ADD OP.
		 */

		rc = hcapi_cfa_key_hw_op_add(op, key_obj);

		break;
	case HCAPI_CFA_HWOPS_DEL:
		rc = hcapi_cfa_key_hw_op_del(op, key_obj);
		break;
	default:
		rc = -1;
		break;
	}

	return rc;
}
