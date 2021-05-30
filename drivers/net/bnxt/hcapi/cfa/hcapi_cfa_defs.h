/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/*!
 *   \file
 *   \brief Exported functions for CFA HW programming
 */
#ifndef _HCAPI_CFA_DEFS_H_
#define _HCAPI_CFA_DEFS_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(__GNUC__)
#pragma anon_unions
#endif

#define CFA_BITS_PER_BYTE (8)
#define CFA_BITS_PER_WORD (sizeof(uint32_t) * CFA_BITS_PER_BYTE)
#define __CFA_ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define CFA_ALIGN(x, a) __CFA_ALIGN_MASK((x), (a) - 1)
#define CFA_ALIGN_256(x) CFA_ALIGN(x, 256)
#define CFA_ALIGN_128(x) CFA_ALIGN(x, 128)
#define CFA_ALIGN_32(x) CFA_ALIGN(x, 32)

#define NUM_WORDS_ALIGN_32BIT(x) (CFA_ALIGN_32(x) / CFA_BITS_PER_WORD)
#define NUM_WORDS_ALIGN_128BIT(x) (CFA_ALIGN_128(x) / CFA_BITS_PER_WORD)
#define NUM_WORDS_ALIGN_256BIT(x) (CFA_ALIGN_256(x) / CFA_BITS_PER_WORD)

/* TODO: redefine according to chip variant */
#define CFA_GLOBAL_CFG_DATA_SZ (100)

#ifndef SUPPORT_CFA_HW_P4
#define SUPPORT_CFA_HW_P4 (0)
#endif

#ifndef SUPPORT_CFA_HW_P45
#define SUPPORT_CFA_HW_P45 (0)
#endif

#ifndef SUPPORT_CFA_HW_P58
#define SUPPORT_CFA_HW_P58 (0)
#endif

#if SUPPORT_CFA_HW_ALL
#include "hcapi_cfa_p4.h"
#include "hcapi_cfa_p58.h"
#endif /* SUPPORT_CFA_HW_ALL */

/*
 * Hashing defines
 */
#define HCAPI_CFA_LKUP_SEED_MEM_SIZE 512

/* CRC32i support for Key0 hash */
#define ucrc32(ch, crc) (crc32tbl[((crc) ^ (ch)) & 0xff] ^ ((crc) >> 8))
#define crc32(x, y) crc32i(~0, x, y)

/**
 * CFA HW version definition
 */
enum hcapi_cfa_ver {
	HCAPI_CFA_P40 = 0, /**< CFA phase 4.0 */
	HCAPI_CFA_P45 = 1, /**< CFA phase 4.5 */
	HCAPI_CFA_P58 = 2, /**< CFA phase 5.8 */
	HCAPI_CFA_PMAX = 3
};

/**
 * CFA direction definition
 */
enum hcapi_cfa_dir {
	HCAPI_CFA_DIR_RX = 0, /**< Receive */
	HCAPI_CFA_DIR_TX = 1, /**< Transmit */
	HCAPI_CFA_DIR_MAX = 2
};

/**
 * CFA HW OPCODE definition
 */
enum hcapi_cfa_hwops {
	HCAPI_CFA_HWOPS_PUT,   /**< Write to HW operation */
	HCAPI_CFA_HWOPS_GET,   /**< Read from HW operation */
	HCAPI_CFA_HWOPS_ADD,   /*<
				* For operations which require more then
				* simple writes to HW, this operation is
				* used.  The distinction with this operation
				* when compared to the PUT ops is that this
				* operation is used in conjunction with
				* the HCAPI_CFA_HWOPS_DEL op to remove
				* the operations issued by the ADD OP.
				*/
	HCAPI_CFA_HWOPS_DEL,   /*<
				*  Beside to delete from the hardware, this
				*   operation is also undo the add operation
				*   performed by the HCAPI_CFA_HWOPS_ADD op.
				*/
	HCAPI_CFA_HWOPS_EVICT, /*< This operation is used to evict entries from
				*   CFA cache memories. This operation is only
				*   applicable to tables that use CFA caches.
				*/
	HCAPI_CFA_HWOPS_MAX
};

/**
 * CFA HW KEY CONTROL OPCODE definition
 */
enum hcapi_cfa_key_ctrlops {
	HCAPI_CFA_KEY_CTRLOPS_INSERT, /**< insert control bits */
	HCAPI_CFA_KEY_CTRLOPS_STRIP,  /**< strip control bits */
	HCAPI_CFA_KEY_CTRLOPS_MAX
};

/**
 * CFA HW definition
 */
struct hcapi_cfa_hw {
	/** [in] HW table base address for the operation with optional device
	 *  handle. For on-chip HW table operation, this is the either the TX
	 *  or RX CFA HW base address. For off-chip table, this field is the
	 *  base memory address of the off-chip table.
	 */
	uint64_t base_addr;
	/** [in] Optional opaque device handle. It is generally used to access
	 *  an GRC register space through PCIE BAR and passed to the BAR memory
	 *  accessor routine.
	 */
	void *handle;
};

/**
 * CFA HW operation definition
 *
 */
struct hcapi_cfa_hwop {
	/** [in] HW opcode */
	enum hcapi_cfa_hwops opcode;
	/** [in] CFA HW information used by accessor routines.
	 */
	struct hcapi_cfa_hw hw;
};

/**
 * CFA HW data structure definition
 */
struct hcapi_cfa_data {
	/** [in] physical offset to the HW table for the data to be
	 *  written to.  If this is an array of registers, this is the
	 *  index into the array of registers.  For writing keys, this
	 *  is the byte pointer into the memory where the key should be
	 *  written.
	 */
	union {
		uint32_t index;
		uint32_t byte_offset;
	};
	/** [in] HW data buffer pointer */
	uint8_t *data;
	/** [in] HW data mask buffer pointer.
	 *  When the CFA data is a FKB and  data_mask pointer
	 *  is NULL, then the default mask to enable all bit will
	 *  be used.
	 */
	uint8_t *data_mask;
	/** [in/out] size of the HW data buffer in bytes
	 */
	uint16_t data_sz;
};

/*********************** Truflow start ***************************/
enum hcapi_cfa_pg_tbl_lvl {
	TF_PT_LVL_0,
	TF_PT_LVL_1,
	TF_PT_LVL_2,
	TF_PT_LVL_MAX
};

enum hcapi_cfa_em_table_type {
	TF_KEY0_TABLE,
	TF_KEY1_TABLE,
	TF_RECORD_TABLE,
	TF_EFC_TABLE,
	TF_ACTION_TABLE,
	TF_EM_LKUP_TABLE,
	TF_MAX_TABLE
};

struct hcapi_cfa_em_page_tbl {
	uint32_t pg_count;
	uint32_t pg_size;
	void **pg_va_tbl;
	uint64_t *pg_pa_tbl;
};

struct hcapi_cfa_em_table {
	int type;
	uint32_t num_entries;
	uint16_t ctx_id;
	uint32_t entry_size;
	int num_lvl;
	uint32_t page_cnt[TF_PT_LVL_MAX];
	uint64_t num_data_pages;
	void *l0_addr;
	uint64_t l0_dma_addr;
	struct hcapi_cfa_em_page_tbl pg_tbl[TF_PT_LVL_MAX];
};

struct hcapi_cfa_em_ctx_mem_info {
	struct hcapi_cfa_em_table em_tables[TF_MAX_TABLE];
};

/*********************** Truflow end ****************************/
/**
 * CFA HW key table definition
 *
 * Applicable to EEM and off-chip EM table only.
 */
struct hcapi_cfa_key_tbl {
	/** [in] For EEM, this is the KEY0 base mem pointer. For off-chip EM,
	 *  this is the base mem pointer of the key table.
	 */
	uint8_t *base0;
	/** [in] total size of the key table in bytes. For EEM, this size is
	 *  same for both KEY0 and KEY1 table.
	 */
	uint32_t size;
	/** [in] number of key buckets, applicable for newer chips */
	uint32_t num_buckets;
	/** [in] For EEM, this is KEY1 base mem pointer. For off-chip EM,
	 *  this is the key record memory base pointer within the key table,
	 *  applicable for newer chip
	 */
	uint8_t *base1;
	/** [in] Optional - If the table is managed by a Backing Store
	 *  database, then this object can be use to configure the EM Key.
	 */
	struct hcapi_cfa_bs_db *bs_db;
	/** [in] Page size for EEM tables */
	uint32_t page_size;
};

/**
 * CFA HW key buffer definition
 */
struct hcapi_cfa_key_obj {
	/** [in] pointer to the key data buffer */
	uint32_t *data;
	/** [in] buffer len in bytes */
	uint32_t len;
	/** [in] Pointer to the key layout */
	struct hcapi_cfa_key_layout *layout;
};

/**
 * CFA HW key data definition
 */
struct hcapi_cfa_key_data {
	/** [in] For on-chip key table, it is the offset in unit of smallest
	 *  key. For off-chip key table, it is the byte offset relative
	 *  to the key record memory base and adjusted for page and entry size.
	 */
	uint32_t offset;
	/** [in] HW key data buffer pointer */
	uint8_t *data;
	/** [in] size of the key in bytes */
	uint16_t size;
	/** [in] optional table scope ID */
	uint8_t tbl_scope;
	/** [in] the fid owner of the key */
	uint64_t metadata;
	/** [in] stored with the bucket which can be used by
	 *       the caller to retrieve later via the GET HW OP.
	 */
};

/**
 * CFA HW key location definition
 */
struct hcapi_cfa_key_loc {
	/** [out] on-chip EM bucket offset or off-chip EM bucket mem pointer */
	uint64_t bucket_mem_ptr;
	/** [out] off-chip EM key offset mem pointer */
	uint64_t mem_ptr;
	/** [out] index within the array of the EM buckets */
	uint32_t bucket_mem_idx;
	/** [out] index within the EM bucket */
	uint8_t bucket_idx;
	/** [out] index within the EM records */
	uint32_t mem_idx;
};

/**
 *  Action record info
 */
struct hcapi_cfa_action_addr {
	/** [in] action SRAM block ID for on-chip action records or table
	 *  scope of the action backing store
	 */
	uint16_t blk_id;
	/** [in] ar_id or cache line aligned address offset for the action
	 *  record
	 */
	uint32_t offset;
};

/**
 * Action data definition
 */
struct hcapi_cfa_action_data {
	/** [in] action record addr info for on-chip action records */
	struct hcapi_cfa_action_addr addr;
	/** [in/out] pointer to the action data buffer */
	uint32_t *data;
	/** [in] action data buffer len in bytes */
	uint32_t len;
};

/**
 * Action object definition
 */
struct hcapi_cfa_action_obj {
	/** [in] pointer to the action data buffer */
	uint32_t *data;
	/** [in] buffer len in bytes */
	uint32_t len;
	/** [in] pointer to the action layout */
	struct hcapi_cfa_action_layout *layout;
};

/**
 * This function is used to hash E/EM keys
 *
 *
 * @param[in] key_data
 *  A pointer of the key
 *
 * @param[in] bitlen
 *  Number of bits in the key
 *
 * @return
 *   CRC32 and Lookup3 hashes of the input key
 */
uint64_t hcapi_cfa_key_hash(uint64_t *key_data,
			    uint16_t bitlen);

/**
 * This function is used to execute an operation
 *
 *
 * @param[in] op
 *  Operation
 *
 * @param[in] key_tbl
 *  Table
 *
 * @param[in] key_obj
 *  Key data
 *
 * @param[in] key_key_loc
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_key_hw_op(struct hcapi_cfa_hwop *op,
			struct hcapi_cfa_key_tbl *key_tbl,
			struct hcapi_cfa_key_data *key_obj,
			struct hcapi_cfa_key_loc *key_loc);

uint64_t hcapi_get_table_page(struct hcapi_cfa_em_table *mem,
			      uint32_t page);
uint32_t hcapi_cfa_crc32i(uint32_t crc, const uint8_t *buf, size_t len);
uint64_t hcapi_cfa_p4_key_hash(uint64_t *key_data,
			       uint16_t bitlen);
uint64_t hcapi_cfa_p58_key_hash(uint64_t *key_data,
				uint16_t bitlen);
#endif /* HCAPI_CFA_DEFS_H_ */
