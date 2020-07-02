/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_EM_H_
#define _TF_EM_H_

#include "tf_core.h"
#include "tf_session.h"

#define TF_HACK_TBL_SCOPE_BASE 68
#define SUPPORT_CFA_HW_P4 1
#define SUPPORT_CFA_HW_P58 0
#define SUPPORT_CFA_HW_P59 0
#define SUPPORT_CFA_HW_ALL 0

#include "hcapi/hcapi_cfa_defs.h"

#define TF_HW_EM_KEY_MAX_SIZE 52
#define TF_EM_KEY_RECORD_SIZE 64

#define TF_EM_MAX_MASK 0x7FFF
#define TF_EM_MAX_ENTRY (128 * 1024 * 1024)

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

/** EEM Memory Type
 *
 */
enum tf_mem_type {
	TF_EEM_MEM_TYPE_INVALID,
	TF_EEM_MEM_TYPE_HOST,
	TF_EEM_MEM_TYPE_SYSTEM
};

/**
 * tf_em_cfg_parms definition
 */
struct tf_em_cfg_parms {
	/**
	 * [in] Num entries in resource config
	 */
	uint16_t num_elements;
	/**
	 * [in] Resource config
	 */
	struct tf_rm_element_cfg *cfg;
	/**
	 * Session resource allocations
	 */
	struct tf_session_resources *resources;
	/**
	 * [in] Memory type.
	 */
	enum tf_mem_type mem_type;
};

/**
 * @page table Table
 *
 * @ref tf_alloc_eem_tbl_scope
 *
 * @ref tf_free_eem_tbl_scope_cb
 *
 * @ref tbl_scope_cb_find
 */

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
 * Insert record in to internal EM table
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
int tf_em_insert_int_entry(struct tf *tfp,
			   struct tf_insert_em_entry_parms *parms);

/**
 * Delete record from internal EM table
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
int tf_em_delete_int_entry(struct tf *tfp,
			   struct tf_delete_em_entry_parms *parms);

/**
 * Insert record in to external EEM table
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
int tf_em_insert_ext_entry(struct tf *tfp,
			   struct tf_insert_em_entry_parms *parms);

/**
 * Insert record from external EEM table
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
int tf_em_delete_ext_entry(struct tf *tfp,
			   struct tf_delete_em_entry_parms *parms);

/**
 * Insert record in to external system EEM table
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
int tf_em_insert_ext_sys_entry(struct tf *tfp,
			       struct tf_insert_em_entry_parms *parms);

/**
 * Delete record from external system EEM table
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
int tf_em_delete_ext_sys_entry(struct tf *tfp,
			       struct tf_delete_em_entry_parms *parms);

/**
 * Bind internal EM device interface
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
int tf_em_int_bind(struct tf *tfp,
		   struct tf_em_cfg_parms *parms);

/**
 * Unbind internal EM device interface
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
int tf_em_int_unbind(struct tf *tfp);

/**
 * Common bind for EEM device interface. Used for both host and
 * system memory
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
int tf_em_ext_common_bind(struct tf *tfp,
			  struct tf_em_cfg_parms *parms);

/**
 * Common unbind for EEM device interface. Used for both host and
 * system memory
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
int tf_em_ext_common_unbind(struct tf *tfp);

/**
 * Alloc for external EEM using host memory
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
int tf_em_ext_host_alloc(struct tf *tfp,
			 struct tf_alloc_tbl_scope_parms *parms);

/**
 * Free for external EEM using host memory
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
int tf_em_ext_host_free(struct tf *tfp,
			struct tf_free_tbl_scope_parms *parms);

/**
 * Alloc for external EEM using system memory
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
int tf_em_ext_system_alloc(struct tf *tfp,
			 struct tf_alloc_tbl_scope_parms *parms);

/**
 * Free for external EEM using system memory
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
int tf_em_ext_system_free(struct tf *tfp,
			struct tf_free_tbl_scope_parms *parms);

/**
 * Common free for external EEM using host or system memory
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
int tf_em_ext_common_free(struct tf *tfp,
			  struct tf_free_tbl_scope_parms *parms);

/**
 * Common alloc for external EEM using host or system memory
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
int tf_em_ext_common_alloc(struct tf *tfp,
			   struct tf_alloc_tbl_scope_parms *parms);
#endif /* _TF_EM_H_ */
