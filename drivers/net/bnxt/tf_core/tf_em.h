/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TF_EM_H_
#define _TF_EM_H_

#include "tf_core.h"
#include "tf_session.h"
#include "tf_em_common.h"

#include "hcapi_cfa_defs.h"

#define TF_P4_HW_EM_KEY_MAX_SIZE 52
#define TF_P4_EM_KEY_RECORD_SIZE 64

#define TF_P58_HW_EM_KEY_MAX_SIZE 80

/*
 * Used to build GFID:
 *
 *   15           2  0
 *  +--------------+--+
 *  |   Index      |E |
 *  +--------------+--+
 *
 * E = Entry (bucket index)
 */
#define TF_EM_INTERNAL_INDEX_SHIFT 2
#define TF_EM_INTERNAL_INDEX_MASK 0xFFFC
#define TF_EM_INTERNAL_ENTRY_MASK  0x3


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

/** EM Entry
 *  Each EM entry is 512-bit (64-bytes) but ordered differently to
 *  EEM.
 */
struct tf_em_64b_entry {
	/** Header is 8 bytes long */
	struct cfa_p4_eem_entry_hdr hdr;
	/** Key is 448 bits - 56 bytes */
	uint8_t key[TF_P4_EM_KEY_RECORD_SIZE - sizeof(struct cfa_p4_eem_entry_hdr)];
};

/**
 * EM database
 *
 * EM rm database
 *
 */
struct em_rm_db {
	struct rm_db *em_db[TF_DIR_MAX];
};

/**
 * @page em EM
 *
 * @ref tf_alloc_eem_tbl_scope
 *
 * @ref tf_free_eem_tbl_scope_cb
 *
 * @ref tf_em_insert_int_entry
 *
 * @ref tf_em_delete_int_entry
 *
 * @ref tf_em_insert_ext_entry DEFUNCT
 *
 * @ref tf_em_delete_ext_entry DEFUNCT
 *
 * @ref tf_em_insert_ext_sys_entry DEFUNCT
 *
 * @ref tf_em_delete_ext_sys_entry DEFUNCT
 *
 * @ref tf_em_int_bind
 *
 * @ref tf_em_int_unbind
 *
 * @ref tf_em_ext_common_bind DEFUNCT
 *
 * @ref tf_em_ext_common_unbind DEFUNCT
 *
 * @ref tf_em_ext_host_alloc DEFUNCT
 *
 * @ref tf_em_ext_host_free DEFUNCT
 *
 * @ref tf_em_ext_system_alloc DEFUNCT
 *
 * @ref tf_em_ext_system_free DEFUNCT
 *
 * @ref tf_em_ext_common_free DEFUNCT
 *
 * @ref tf_em_ext_common_alloc DEFUNCT
 */

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
int tf_em_hash_insert_int_entry(struct tf *tfp,
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
int tf_em_hash_delete_int_entry(struct tf *tfp,
				struct tf_delete_em_entry_parms *parms);

/**
 * Move record from internal EM table
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
int tf_em_move_int_entry(struct tf *tfp,
			 struct tf_move_em_entry_parms *parms);

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
 * Retrieves the allocated resource info
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_em_get_resc_info(struct tf *tfp,
		    struct tf_em_resource_info *em);
#endif /* _TF_EM_H_ */
