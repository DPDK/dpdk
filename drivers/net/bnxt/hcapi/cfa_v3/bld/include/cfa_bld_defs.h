/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_DEFS_H_
#define _CFA_BLD_DEFS_H_

#include <stdint.h>
#include <stdbool.h>

#include "cfa_resources.h"
#include "cfa_types.h"

/**
 *  @addtogroup CFA_BLD CFA Builder Library
 *  \ingroup CFA_V3
 *  The CFA builder library is a set of APIs provided the following services:
 *
 *  1. Provide users generic put service to convert software programming data
 *     into a hardware data bit stream according to a HW layout representation,
 *     or generic get service to extract value of a field or values of a number
 *     of fields from the raw hardware data bit stream according to a HW layout.
 *
 *     - A software programming data is represented in {field_idx, val}
 *        structure.
 *     - A HW layout is represented with array of CFA field structures with
 *        {bitpos, bitlen} and identified by a layout id corresponding to a CFA
 *        HW table.
 *     - A HW data bit stream are bits that is formatted according to a HW
 *        layout representation.
 *
 *  2. Provide EM/WC key and action related service APIs to compile layout,
 *     init, and manipulate key and action data objects.
 *
 *  3. Provide CFA mid-path message building APIs. (TBD)
 *
 *  The CFA builder library is designed to run in the primate firmware and also
 *  as part of the following host base diagnostic software.
 *  - Lcdiag
 *  - Truflow CLI
 *  - coredump decoder
 *
 *  @{
 */

/** @name CFA Builder Common Definition
 * CFA builder common structures and enumerations
 */

/**@{*/
/**
 * CFA HW KEY CONTROL OPCODE definition
 */
enum cfa_key_ctrlops {
	CFA_KEY_CTRLOPS_INSERT, /**< insert WC control bits */
	CFA_KEY_CTRLOPS_STRIP,  /**< strip WC control bits */
	CFA_KEY_CTRLOPS_SWAP,   /**< swap EM cache lines */
	CFA_KEY_CTRLOPS_MAX
};

/**
 * CFA HW field structure definition
 */
struct cfa_field {
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
struct cfa_layout {
	/** [out] Bit order of layout
	 * if swap_order_bitpos is non-zero, the bit order of the layout
	 * will be swapped after this bit.  swap_order_bitpos must be a
	 * multiple of 64.  This is currently only used for inlined action
	 * records where the AR is lsb and the following inlined actions
	 * must be msb.
	 */
	bool is_msb_order;
	/** [out] Reverse is_msb_order after this bit if non-zero */
	uint16_t swap_order_bitpos;
	/** [out] Size in bits of entry */
	uint32_t total_sz_in_bits;
	/** [in/out] data pointer of the HW layout fields array */
	struct cfa_field *field_array;
	/** [out] number of HW field entries in the HW layout field array */
	uint32_t array_sz;
	/** [out] layout_id - layout id associated with the layout */
	uint16_t layout_id;
};

/**
 * CFA HW data object definition
 */
struct cfa_data_obj {
	/** [in] HW field identifier. Used as an index to a HW table layout */
	uint16_t field_id;
	/** [in] Value of the HW field */
	uint64_t val;
};

/**
 * CFA HW key buffer definition
 */
struct cfa_key_obj {
	/** [in] pointer to the key data buffer */
	uint32_t *data;
	/** [in] buffer len in bytes */
	uint32_t data_len_bytes;
	/** [out] Data length in bits
	 * When cfa_key_obj is passed as an output parameter, the updated
	 * key length (if the key length changes) is returned in this field by the
	 * key processing api (e.g cfa_bld_key_transform)
	 * When cfa_key_obj is passed as an input parameter, this field is unused
	 * and need not be initialized by the caller.
	 */
	uint32_t data_len_bits;
	/** [in] Pointer to the key layout */
	struct cfa_key_layout *layout;
};

/**
 * CFA HW layout table definition
 */
struct cfa_layout_tbl {
	/** [out] data pointer to an array of fix formatted layouts supported.
	 *  The index to the array is either the CFA resource subtype or
	 *  remap table ID
	 */
	const struct cfa_layout *layouts;
	/** [out] number of fix formatted layouts in the layout array */
	uint16_t num_layouts;
};

/**
 * key layout consist of field array, key bitlen, key ID, and other meta data
 * pertain to a key
 */
struct cfa_key_layout {
	/** [in/out] key layout data */
	struct cfa_layout *layout;
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
	 *  Note: This is important for Thor2 as the field length for the FlowId
	 *  is dependent on the L3 flow type.  For Thor2 for IPv4 Keys, the Flow
	 *  Id field is 16 bits, for all other types (IPv6, ARP, PTP, EAP, RoCE,
	 *  FCoE, UPAR), the Flow Id field length is 20 bits.
	 */
	bool is_ipv6_key;
	/** [out] total number of slices, valid for WC TCAM key only. It can be
	 *  used by the user to pass in the num_slices to write to the hardware.
	 */
	uint16_t num_slices;
};

/**
 * CFA HW key table definition
 *
 * Applicable to EEM and on-chip EM table only.
 */
struct cfa_key_tbl {
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
	/** [in] For EEM, this is KEY1 base mem pointer. Fo on-chip EM,
	 *  this is the key record memory base pointer within the key table,
	 *  applicable for newer chip
	 */
	uint8_t *base1;
	/** [in] Optional - If the table is managed by a Backing Store
	 *  database, then this object can be use to configure the EM Key.
	 */
	struct cfa_bs_db *bs_db;
	/** [in] Page size for EEM tables */
	uint32_t page_size;
};

/**
 * CFA HW key data definition
 */
struct cfa_key_data {
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
	/** [in] stored with the bucket which can be used to by
	 *       the caller to retrieved later via the GET HW OP.
	 */
};

/**
 * CFA HW key location definition
 */
struct cfa_key_loc {
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
struct cfa_action_addr {
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
 * Action object definition
 */
struct cfa_action_obj {
	/** [in] pointer to the action data buffer */
	uint64_t *data;
	/** [in] buffer len in bytes */
	uint32_t len;
	/** [in] pointer to the action layout */
	struct cfa_action_layout *layout;
};

/**
 * action layout consist of field array, action wordlen and action format ID
 */
struct cfa_action_layout {
	/** [in] action identifier */
	uint16_t id;
	/** [out] action layout data */
	struct cfa_layout *layout;
	/** [out] actual action record size in number of bits */
	uint16_t bitlen;
};

/**@}*/

/** @name CFA Builder PUT_FIELD APIs
 *  CFA Manager apis used for generating hw layout specific data objects that
 *  can be programmed to the hardware
 */

/**@{*/
/**
 * @brief This API provides the functionality to program a specified value to a
 * HW field based on the provided programming layout.
 *
 * @param[in,out] data_buf
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
int cfa_put_field(uint64_t *data_buf, const struct cfa_layout *layout,
		  uint16_t field_id, uint64_t val);

/**
 * @brief This API provides the functionality to program an array of field
 * values with corresponding field IDs to a number of profiler sub-block fields
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
int cfa_put_fields(uint64_t *obj_data, const struct cfa_layout *layout,
		   struct cfa_data_obj *field_tbl, uint16_t field_tbl_sz);

/**
 * @brief This API provides the functionality to program an array of field
 * values with corresponding field IDs to a number of profiler sub-block fields
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
int cfa_put_fields_swap(uint64_t *obj_data, const struct cfa_layout *layout,
			struct cfa_data_obj *field_tbl, uint16_t field_tbl_sz,
			uint16_t data_size, uint16_t n);

/**
 * @brief This API provides the functionality to write a value to a
 * field within the bit position and bit length of a HW data
 * object based on a provided programming layout.
 *
 * @param[in, out] obj_data
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
int cfa_put_field_rel(uint64_t *obj_data, const struct cfa_layout *layout,
		      uint16_t field_id, int16_t bitpos_adj, int16_t bitlen_adj,
		      uint64_t val);

/**@}*/

/** @name CFA Builder GET_FIELD APIs
 *  CFA Manager apis used for extract hw layout specific fields from CFA HW
 *  data objects
 */

/**@{*/
/**
 * @brief The API provides the functionality to get bit offset and bit
 * length information of a field from a programming layout.
 *
 * @param[in] layout
 *   A pointer of the action layout
 *
 * @param[in] field_id
 *   The field for which to retrieve the slice
 *
 * @param[out] slice
 *   A pointer to the action offset info data structure
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_get_slice(const struct cfa_layout *layout, uint16_t field_id,
		  struct cfa_field *slice);

/**
 * @brief This API provides the functionality to read the value of a
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
int cfa_get_field(uint64_t *obj_data, const struct cfa_layout *layout,
		  uint16_t field_id, uint64_t *val);

/**
 * @brief This API provides the functionality to read 128-bit value of
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
int cfa_get128_field(uint64_t *obj_data, const struct cfa_layout *layout,
		     uint16_t field_id, uint64_t *val_msb, uint64_t *val_lsb);

/**
 * @brief This API provides the functionality to read a number of
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
int cfa_get_fields(uint64_t *obj_data, const struct cfa_layout *layout,
		   struct cfa_data_obj *field_tbl, uint16_t field_tbl_sz);

/**
 * @brief This API provides the functionality to read a number of
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
int cfa_get_fields_swap(uint64_t *obj_data, const struct cfa_layout *layout,
			struct cfa_data_obj *field_tbl, uint16_t field_tbl_sz,
			uint16_t data_size, uint16_t n);

/**
 * @brief Get a value to a specific location relative to a HW field
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
int cfa_get_field_rel(uint64_t *obj_data, const struct cfa_layout *layout,
		      uint16_t field_id, int16_t bitpos_adj, int16_t bitlen_adj,
		      uint64_t *val);

/**
 * @brief Get the length of the layout in words
 *
 * @param[in] layout
 *   A pointer to the layout to determine the number of words
 *   required
 *
 * @return
 *   number of words needed for the given layout
 */
uint16_t cfa_get_wordlen(const struct cfa_layout *layout);

/**@}*/

/**@}*/
#endif /* _CFA_BLD_DEFS_H_*/
