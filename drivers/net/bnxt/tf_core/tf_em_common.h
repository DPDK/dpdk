/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_EM_COMMON_H_
#define _TF_EM_COMMON_H_

#include "tf_core.h"
#include "tf_session.h"

/**
 * Function to search for table scope control block structure
 * with specified table scope ID.
 *
 * [in] tbl_scope_id
 *   Table scope ID to search for
 *
 * Returns:
 *  Pointer to the found table scope control block struct or NULL if
 *   table scope control block struct not found
 */
struct tf_tbl_scope_cb *tbl_scope_cb_find(uint32_t tbl_scope_id);

/**
 * Table Scope Allocate
 *
 * Allocate a table scope
 *
 * [in/out] pointer to tbl_scope_id
 *
 * Returns:
 *  0 - success
 *  -EINVAL - error
 */
int tf_tbl_scope_alloc(uint32_t *tbl_scope_id);

/**
 * Table Scope Free
 *
 * Free a table scope
 *
 * [in] tbl_scope_id to free
 *
 * Returns:
 *  0 - success
 *  -EINVAL - error
 */
int tf_tbl_scope_free(uint32_t tbl_scope_id);

/**
 * Create and initialize a stack to use for action entries
 *
 * [in] dir
 *   Direction
 * [in] tbl_scope_id
 *   Table scope ID
 * [in] num_entries
 *   Number of EEM entries
 * [in] entry_sz_bytes
 *   Size of the entry
 *
 * Returns:
 *   0       - Success
 *   -ENOMEM - Out of memory
 *   -EINVAL - Failure
 */
int tf_create_tbl_pool_external(enum tf_dir dir,
				struct tf_tbl_scope_cb *tbl_scope_cb,
				uint32_t num_entries,
				uint32_t entry_sz_bytes);

/**
 * Delete and cleanup action record allocation stack
 *
 * [in] dir
 *   Direction
 * [in] tbl_scope_id
 *   Table scope ID
 *
 */
void tf_destroy_tbl_pool_external(enum tf_dir dir,
				  struct tf_tbl_scope_cb *tbl_scope_cb);

/**
 * Get hash mask for current EEM table size
 *
 * [in] num_entries
 *   Number of EEM entries
 */
uint32_t tf_em_get_key_mask(int num_entries);

/**
 * Populate key_entry
 *
 * [in] result
 *   Entry data
 * [in] in_key
 *   Key data
 * [out] key_entry
 *   Completed key record
 */
void tf_em_create_key_entry(struct cfa_p4_eem_entry_hdr *result,
			    uint8_t	       *in_key,
			    struct cfa_p4_eem_64b_entry *key_entry);

/**
 * Find base page address for offset into specified table type
 *
 * [in] tbl_scope_cb
 *   Table scope
 * [in] dir
 *   Direction
 * [in] Offset
 *   Offset in to table
 * [in] table_type
 *   Table type
 *
 * Returns:
 *
 * 0                                 - Failure
 * Void pointer to page base address - Success
 */
void *tf_em_get_table_page(struct tf_tbl_scope_cb *tbl_scope_cb,
			   enum tf_dir dir,
			   uint32_t offset,
			   enum hcapi_cfa_em_table_type table_type);

/**
 * Validates EM number of entries requested
 *
 * [in] tbl_scope_cb
 *   Pointer to table scope control block to be populated
 *
 * [in] parms
 *   Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_validate_num_entries(struct tf_tbl_scope_cb *tbl_scope_cb,
			       struct tf_alloc_tbl_scope_parms *parms);

/**
 * Size the EM table based on capabilities
 *
 * [in] tbl
 *   EM table to size
 *
 * Returns:
 *   0        - Success
 *   - EINVAL - Parameter error
 *   - ENOMEM - Out of memory
 */
int tf_em_size_table(struct hcapi_cfa_em_table *tbl,
		     uint32_t page_size);

#endif /* _TF_EM_COMMON_H_ */
