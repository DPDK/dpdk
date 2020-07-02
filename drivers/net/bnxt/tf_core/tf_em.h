/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_EM_H_
#define _TF_EM_H_

#include "tf_core.h"
#include "tf_session.h"

#define SUPPORT_CFA_HW_P4 1
#define SUPPORT_CFA_HW_P58 0
#define SUPPORT_CFA_HW_P59 0
#define SUPPORT_CFA_HW_ALL 0

#include "hcapi/hcapi_cfa_defs.h"

#define TF_HW_EM_KEY_MAX_SIZE 52
#define TF_EM_KEY_RECORD_SIZE 64

/*
 * Used to build GFID:
 *
 *   15           2  0
 *  +--------------+--+
 *  |   Index      |E |
 *  +--------------+--+
 *
 * E = Entry (bucket inndex)
 */
#define TF_EM_INTERNAL_INDEX_SHIFT 2
#define TF_EM_INTERNAL_INDEX_MASK 0xFFFC
#define TF_EM_INTERNAL_ENTRY_MASK  0x3

/** EM Entry
 *  Each EM entry is 512-bit (64-bytes) but ordered differently to
 *  EEM.
 */
struct tf_em_64b_entry {
	/** Header is 8 bytes long */
	struct cfa_p4_eem_entry_hdr hdr;
	/** Key is 448 bits - 56 bytes */
	uint8_t key[TF_EM_KEY_RECORD_SIZE - sizeof(struct cfa_p4_eem_entry_hdr)];
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

void *tf_em_get_table_page(struct tf_tbl_scope_cb *tbl_scope_cb,
			   enum tf_dir dir,
			   uint32_t offset,
			   enum hcapi_cfa_em_table_type table_type);

int tf_em_insert_entry(struct tf *tfp,
		       struct tf_insert_em_entry_parms *parms);

int tf_em_delete_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *parms);
#endif /* _TF_EM_H_ */
