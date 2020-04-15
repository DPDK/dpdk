/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_EM_H_
#define _TF_EM_H_

#include "tf_core.h"
#include "tf_session.h"

#define TF_HW_EM_KEY_MAX_SIZE 52
#define TF_EM_KEY_RECORD_SIZE 64

/** EEM Entry header
 *
 */
struct tf_eem_entry_hdr {
	uint32_t pointer;
	uint32_t word1;  /*
			  * The header is made up of two words,
			  * this is the first word. This field has multiple
			  * subfields, there is no suitable single name for
			  * it so just going with word1.
			  */
#define TF_LKUP_RECORD_VALID_SHIFT 31
#define TF_LKUP_RECORD_VALID_MASK 0x80000000
#define TF_LKUP_RECORD_L1_CACHEABLE_SHIFT 30
#define TF_LKUP_RECORD_L1_CACHEABLE_MASK 0x40000000
#define TF_LKUP_RECORD_STRENGTH_SHIFT 28
#define TF_LKUP_RECORD_STRENGTH_MASK 0x30000000
#define TF_LKUP_RECORD_RESERVED_SHIFT 17
#define TF_LKUP_RECORD_RESERVED_MASK 0x0FFE0000
#define TF_LKUP_RECORD_KEY_SIZE_SHIFT 8
#define TF_LKUP_RECORD_KEY_SIZE_MASK 0x0001FF00
#define TF_LKUP_RECORD_ACT_REC_SIZE_SHIFT 3
#define TF_LKUP_RECORD_ACT_REC_SIZE_MASK 0x000000F8
#define TF_LKUP_RECORD_ACT_REC_INT_SHIFT 2
#define TF_LKUP_RECORD_ACT_REC_INT_MASK 0x00000004
#define TF_LKUP_RECORD_EXT_FLOW_CTR_SHIFT 1
#define TF_LKUP_RECORD_EXT_FLOW_CTR_MASK 0x00000002
#define TF_LKUP_RECORD_ACT_PTR_MSB_SHIFT 0
#define TF_LKUP_RECORD_ACT_PTR_MSB_MASK 0x00000001
};

/** EEM Entry
 *  Each EEM entry is 512-bit (64-bytes)
 */
struct tf_eem_64b_entry {
	/** Key is 448 bits - 56 bytes */
	uint8_t key[TF_EM_KEY_RECORD_SIZE - sizeof(struct tf_eem_entry_hdr)];
	/** Header is 8 bytes long */
	struct tf_eem_entry_hdr hdr;
};

/**
 * Allocates EEM Table scope
 *
 * [in] tfp
 *   Pointer to TruFlow handle
 *
 * [in] parms
 *   Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 *   -ENOMEM - Out of memory
 */
int tf_alloc_eem_tbl_scope(struct tf *tfp,
			   struct tf_alloc_tbl_scope_parms *parms);

/**
 * Free's EEM Table scope control block
 *
 * [in] tfp
 *   Pointer to TruFlow handle
 *
 * [in] parms
 *   Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_free_eem_tbl_scope_cb(struct tf *tfp,
			     struct tf_free_tbl_scope_parms *parms);

/**
 * Function to search for table scope control block structure
 * with specified table scope ID.
 *
 * [in] session
 *   Session to use for the search of the table scope control block
 * [in] tbl_scope_id
 *   Table scope ID to search for
 *
 * Returns:
 *  Pointer to the found table scope control block struct or NULL if
 *  table scope control block struct not found
 */
struct tf_tbl_scope_cb *tbl_scope_cb_find(struct tf_session *session,
					  uint32_t tbl_scope_id);

int tf_insert_eem_entry(struct tf_session *session,
			struct tf_tbl_scope_cb *tbl_scope_cb,
			struct tf_insert_em_entry_parms *parms);

int tf_delete_eem_entry(struct tf *tfp,
			struct tf_delete_em_entry_parms *parms);

void *tf_em_get_table_page(struct tf_tbl_scope_cb *tbl_scope_cb,
			   enum tf_dir dir,
			   uint32_t offset,
			   enum tf_em_table_type table_type);

#endif /* _TF_EM_H_ */
