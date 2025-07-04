/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
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
#ifndef CFA_ALIGN
#define CFA_ALIGN(x, a) __CFA_ALIGN_MASK((x), (a) - 1)
#endif
#define CFA_ALIGN_256(x) CFA_ALIGN(x, 256)
#define CFA_ALIGN_128(x) CFA_ALIGN(x, 128)
#define CFA_ALIGN_32(x) CFA_ALIGN(x, 32)

/* TODO: redefine according to chip variant */
#define CFA_GLOBAL_CFG_DATA_SZ (100)

#include "hcapi_cfa_p4.h"
#include "hcapi_cfa_p58.h"

#define CFA_PROF_L2CTXT_TCAM_MAX_FIELD_CNT CFA_P58_PROF_L2_CTXT_TCAM_MAX_FLD
#define CFA_PROF_L2CTXT_REMAP_MAX_FIELD_CNT CFA_P58_PROF_L2_CTXT_RMP_DR_MAX_FLD
#define CFA_PROF_MAX_KEY_CFG_SZ sizeof(struct cfa_p58_prof_key_cfg)
#define CFA_KEY_MAX_FIELD_CNT 0
#define CFA_ACT_MAX_TEMPLATE_SZ 0

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
	HCAPI_CFA_P59 = 3, /**< CFA phase 5.9 */
	HCAPI_CFA_PMAX = 4
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
	HCAPI_CFA_HWOPS_EVICT, /*< This operation is used to edit entries from
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
 * CFA HW field structure definition
 */
struct hcapi_cfa_field {
	/** [in] Starting bit position pf the HW field within a HW table
	 *  entry.
	 */
	uint16_t bitpos;
	/** [in] Number of bits for the HW field. */
	uint16_t bitlen;
};

/**
 * CFA HW table entry layout structure definition
 */
struct hcapi_cfa_layout {
	/** [out] Bit order of layout */
	bool is_msb_order;
	/** [out] Size in bits of entry */
	uint32_t total_sz_in_bits;
	/** [out] data pointer of the HW layout fields array */
	struct hcapi_cfa_field *field_array;
	/** [out] number of HW field entries in the HW layout field array */
	uint32_t array_sz;
	/** [out] layout_id - layout id associated with the layout */
	uint16_t layout_id;
};

/**
 * CFA HW data object definition
 */
struct hcapi_cfa_data_obj {
	/** [in] HW field identifier. Used as an index to a HW table layout */
	uint16_t field_id;
	/** [in] Value of the HW field */
	uint64_t val;
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
 * CFA HW layout table definition
 */
struct hcapi_cfa_layout_tbl {
	/** [out] data pointer to an array of fix formatted layouts supported.
	 *  The index to the array is the CFA HW table ID
	 */
	const struct hcapi_cfa_layout *tbl;
	/** [out] number of fix formatted layouts in the layout array */
	uint16_t num_layouts;
};

/**
 * Key template consists of key fields that can be enabled/disabled
 * individually.
 */
struct hcapi_cfa_key_template {
	/** [in] key field enable field array, set 1 to the correspeonding
	 *  field enable to make a field valid
	 */
	uint8_t field_en[CFA_KEY_MAX_FIELD_CNT];
	/** [in] Identify if the key template is for TCAM. If false, the
	 *  key template is for EM. This field is mandantory for device that
	 *  only support fix key formats.
	 */
	bool is_wc_tcam_key;
	/** [in] Identify if the key template will be use for IPv6 Keys.
	 *
	 */
	bool is_ipv6_key;
};

/**
 * key layout consist of field array, key bitlen, key ID, and other meta data
 * pertain to a key
 */
struct hcapi_cfa_key_layout {
	/** [out] key layout data */
	struct hcapi_cfa_layout *layout;
	/** [out] actual key size in number of bits */
	uint16_t bitlen;
	/** [out] key identifier and this field is only valid for device
	 *  that supports fix key formats
	 */
	uint16_t id;
	/** [out] Identified the key layout is WC TCAM key */
	bool is_wc_tcam_key;
	/** [out] Identify if the key template will be use for IPv6 Keys.
	 *
	 */
	bool is_ipv6_key;
	/** [out] total slices size, valid for WC TCAM key only. It can be
	 *  used by the user to determine the total size of WC TCAM key slices
	 *  in bytes.
	 */
	uint16_t slices_size;
};

/**
 * key layout memory contents
 */
struct hcapi_cfa_key_layout_contents {
	/** key layouts */
	struct hcapi_cfa_key_layout key_layout;

	/** layout */
	struct hcapi_cfa_layout layout;

	/** fields */
	struct hcapi_cfa_field field_array[CFA_KEY_MAX_FIELD_CNT];
};

/**
 * Action template consists of action fields that can be enabled/disabled
 * individually.
 */
struct hcapi_cfa_action_template {
	/** [in] CFA version for the action template */
	enum hcapi_cfa_ver hw_ver;
	/** [in] action field enable field array, set 1 to the correspeonding
	 *  field enable to make a field valid
	 */
	uint8_t data[CFA_ACT_MAX_TEMPLATE_SZ];
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
 * action layout consist of field array, action wordlen and action format ID
 */
struct hcapi_cfa_action_layout {
	/** [in] action identifier */
	uint16_t id;
	/** [out] action layout data */
	struct hcapi_cfa_layout *layout;
	/** [out] actual action record size in number of bits */
	uint16_t bitlen;
};

/**
 * CFA backing store type definition
 */
enum hcapi_cfa_bs_type {
	HCAPI_CFA_BS_TYPE_LKUP, /**< EM LKUP backing store type */
	HCAPI_CFA_BS_TYPE_ACT,  /**< Action backing store type */
	HCAPI_CFA_BS_TYPE_MAX
};

/**
 * CFA backing store configuration data object
 */
struct hcapi_cfa_bs_cfg {
	enum hcapi_cfa_bs_type type;
	uint16_t tbl_scope;
	struct hcapi_cfa_bs_db *bs_db;
};

/**
 * CFA backing store data base object
 */
struct hcapi_cfa_bs_db {
	/** [in] memory manager database signature */
	uint32_t signature;
#define HCAPI_CFA_BS_SIGNATURE 0xCFA0B300
	/** [in] memory manager database base pointer  (VA) */
	void *mgmt_db;
	/** [in] memory manager database size in bytes */
	uint32_t mgmt_db_sz;
	/** [in] Backing store memory pool base pointer
	 *  (VA – backed by IOVA which is DMA accessible))
	 */
	void *bs_ptr;
	/** [in] bs_offset - byte offset to the section of the
	 *  backing store memory managed by the backing store
	 *  memory manager.
	 *  For EM backing store, this is the starting byte
	 *  offset to the EM record memory.
	 *  For Action backing store, this offset is 0.
	 */
	uint32_t offset;
	/** [in] backing store memory pool size in bytes
	 */
	uint32_t bs_sz;
};

/**
 *  \defgroup CFA_HCAPI_PUT_API
 *  HCAPI used for writing to the hardware
 *  @{
 */

/**
 * This API provides the functionality to program a specified value to a
 * HW field based on the provided programming layout.
 *
 * @param[in,out] obj_data
 *   A data pointer to a CFA HW key/mask data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_id
 *   ID of the HW field to be programmed
 *
 * @param[in] val
 *   Value of the HW field to be programmed
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_field(uint64_t *data_buf,
			const struct hcapi_cfa_layout *layout,
			uint16_t field_id, uint64_t val);

/**
 * This API provides the functionality to program an array of field values
 * with corresponding field IDs to a number of profiler sub-block fields
 * based on the fixed profiler sub-block hardware programming layout.
 *
 * @param[in, out] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_fields(uint64_t *obj_data,
			 const struct hcapi_cfa_layout *layout,
			 struct hcapi_cfa_data_obj *field_tbl,
			 uint16_t field_tbl_sz);
/**
 * This API provides the functionality to program an array of field values
 * with corresponding field IDs to a number of profiler sub-block fields
 * based on the fixed profiler sub-block hardware programming layout. This
 * API will swap the n byte blocks before programming the field array.
 *
 * @param[in, out] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @param[in] data_size
 *   size of the data in bytes
 *
 * @param[in] n
 *   block size in bytes
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_fields_swap(uint64_t *obj_data,
			      const struct hcapi_cfa_layout *layout,
			      struct hcapi_cfa_data_obj *field_tbl,
			      uint16_t field_tbl_sz, uint16_t data_size,
			      uint16_t n);
/**
 * This API provides the functionality to write a value to a
 * field within the bit position and bit length of a HW data
 * object based on a provided programming layout.
 *
 * @param[in, out] act_obj
 *   A pointer of the action object to be initialized
 *
 * @param[in] layout
 *   A pointer of the programming layout
 *
 * @param field_id
 *   [in] Identifier of the HW field
 *
 * @param[in] bitpos_adj
 *   Bit position adjustment value
 *
 * @param[in] bitlen_adj
 *   Bit length adjustment value
 *
 * @param[in] val
 *   HW field value to be programmed
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_field_rel(uint64_t *obj_data,
			    const struct hcapi_cfa_layout *layout,
			    uint16_t field_id, int16_t bitpos_adj,
			    int16_t bitlen_adj, uint64_t val);

/*@}*/

/**
 *  \defgroup CFA_HCAPI_GET_API
 *  HCAPI used for writing to the hardware
 *  @{
 */

/**
 * This API provides the functionality to get the word length of
 * a layout object.
 *
 * @param[in] layout
 *   A pointer of the HW layout
 *
 * @return
 *   Word length of the layout object
 */
uint16_t hcapi_cfa_get_wordlen(const struct hcapi_cfa_layout *layout);

/**
 * The API provides the functionality to get bit offset and bit
 * length information of a field from a programming layout.
 *
 * @param[in] layout
 *   A pointer of the action layout
 *
 * @param[out] slice
 *   A pointer to the action offset info data structure
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_slice(const struct hcapi_cfa_layout *layout,
			uint16_t field_id, struct hcapi_cfa_field *slice);

/**
 * This API provides the functionality to read the value of a
 * CFA HW field from CFA HW data object based on the hardware
 * programming layout.
 *
 * @param[in] obj_data
 *   A pointer to a CFA HW key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_id
 *   ID of the HW field to be programmed
 *
 * @param[out] val
 *   Value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_field(uint64_t *obj_data,
			const struct hcapi_cfa_layout *layout,
			uint16_t field_id, uint64_t *val);

/**
 * This API provides the functionality to read 128-bit value of
 * a CFA HW field from CFA HW data object based on the hardware
 * programming layout.
 *
 * @param[in] obj_data
 *   A pointer to a CFA HW key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_id
 *   ID of the HW field to be programmed
 *
 * @param[out] val_msb
 *   Msb value of the HW field
 *
 * @param[out] val_lsb
 *   Lsb value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get128_field(uint64_t *obj_data,
			   const struct hcapi_cfa_layout *layout,
			   uint16_t field_id, uint64_t *val_msb,
			   uint64_t *val_lsb);

/**
 * This API provides the functionality to read a number of
 * HW fields from a CFA HW data object based on the hardware
 * programming layout.
 *
 * @param[in] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in, out] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_fields(uint64_t *obj_data,
			 const struct hcapi_cfa_layout *layout,
			 struct hcapi_cfa_data_obj *field_tbl,
			 uint16_t field_tbl_sz);

/**
 * This API provides the functionality to read a number of
 * HW fields from a CFA HW data object based on the hardware
 * programming layout.This API will swap the n byte blocks before
 * retrieving the field array.
 *
 * @param[in] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in, out] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @param[in] data_size
 *   size of the data in bytes
 *
 * @param[in] n
 *   block size in bytes
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_fields_swap(uint64_t *obj_data,
			      const struct hcapi_cfa_layout *layout,
			      struct hcapi_cfa_data_obj *field_tbl,
			      uint16_t field_tbl_sz, uint16_t data_size,
			      uint16_t n);

/**
 * Get a value to a specific location relative to a HW field
 *
 * This API provides the functionality to read HW field from
 * a section of a HW data object identified by the bit position
 * and bit length from a given programming layout in order to avoid
 * reading the entire HW data object.
 *
 * @param[in] obj_data
 *   A pointer of the data object to read from
 *
 * @param[in] layout
 *   A pointer of the programming layout
 *
 * @param[in] field_id
 *   Identifier of the HW field
 *
 * @param[in] bitpos_adj
 *   Bit position adjustment value
 *
 * @param[in] bitlen_adj
 *   Bit length adjustment value
 *
 * @param[out] val
 *   Value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_field_rel(uint64_t *obj_data,
			    const struct hcapi_cfa_layout *layout,
			    uint16_t field_id, int16_t bitpos_adj,
			    int16_t bitlen_adj, uint64_t *val);

/**
 * Get the length of the layout in words
 *
 * @param[in] layout
 *   A pointer to the layout to determine the number of words
 *   required
 *
 * @return
 *   number of words needed for the given layout
 */
uint16_t cfa_hw_get_wordlen(const struct hcapi_cfa_layout *layout);

/**
 * This function is used to initialize a layout_contents structure
 *
 * The struct hcapi_cfa_key_layout is complex as there are three
 * layers of abstraction.  Each of those layer need to be properly
 * initialized.
 *
 * @param[in] contents
 *  A pointer of the layout contents to initialize
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_init_key_contents(struct hcapi_cfa_key_layout_contents *contents);

/**
 * This function is used to validate a key template
 *
 * The struct hcapi_cfa_key_template is complex as there are three
 * layers of abstraction.  Each of those layer need to be properly
 * validated.
 *
 * @param[in] key_template
 *  A pointer of the key template contents to validate
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_is_valid_key_template(struct hcapi_cfa_key_template *key_template);

/**
 * This function is used to validate a key layout
 *
 * The struct hcapi_cfa_key_layout is complex as there are three
 * layers of abstraction.  Each of those layer need to be properly
 * validated.
 *
 * @param[in] key_layout
 *  A pointer of the key layout contents to validate
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_is_valid_key_layout(struct hcapi_cfa_key_layout *key_layout);

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
