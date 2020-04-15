/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef TF_RM_H_
#define TF_RM_H_

#include "tf_resources.h"
#include "tf_core.h"
#include "bitalloc.h"

struct tf;
struct tf_session;

/* Internal macro to determine appropriate allocation pools based on
 * DIRECTION parm, also performs error checking for DIRECTION parm. The
 * SESSION_POOL and SESSION pointers are set appropriately upon
 * successful return (the GLOBAL_POOL is used to globally manage
 * resource allocation and the SESSION_POOL is used to track the
 * resources that have been allocated to the session)
 *
 * parameters:
 *   struct tfp        *tfp
 *   enum tf_dir        direction
 *   struct bitalloc  **session_pool
 *   string             base_pool_name - used to form pointers to the
 *					 appropriate bit allocation
 *					 pools, both directions of the
 *					 session pools must have same
 *					 base name, for example if
 *					 POOL_NAME is feat_pool: - the
 *					 ptr's to the session pools
 *					 are feat_pool_rx feat_pool_tx
 *
 *  int                  rc - return code
 *			      0 - Success
 *			     -1 - invalid DIRECTION parm
 */
#define TF_RM_GET_POOLS(tfs, direction, session_pool, pool_name, rc) do { \
		(rc) = 0;						\
		if ((direction) == TF_DIR_RX) {				\
			*(session_pool) = (tfs)->pool_name ## _RX;	\
		} else if ((direction) == TF_DIR_TX) {			\
			*(session_pool) = (tfs)->pool_name ## _TX;	\
		} else {						\
			rc = -1;					\
		}							\
	} while (0)

#define TF_RM_GET_POOLS_RX(tfs, session_pool, pool_name)	\
	(*(session_pool) = (tfs)->pool_name ## _RX)

#define TF_RM_GET_POOLS_TX(tfs, session_pool, pool_name)	\
	(*(session_pool) = (tfs)->pool_name ## _TX)

/**
 * Resource query single entry
 */
struct tf_rm_query_entry {
	/** Minimum guaranteed number of elements */
	uint16_t min;
	/** Maximum non-guaranteed number of elements */
	uint16_t max;
};

/**
 * Resource single entry
 */
struct tf_rm_entry {
	/** Starting index of the allocated resource */
	uint16_t start;
	/** Number of allocated elements */
	uint16_t stride;
};

/**
 * Resource query array of HW entities
 */
struct tf_rm_hw_query {
	/** array of HW resource entries */
	struct tf_rm_query_entry hw_query[TF_RESC_TYPE_HW_MAX];
};

/**
 * Resource allocation array of HW entities
 */
struct tf_rm_hw_alloc {
	/** array of HW resource entries */
	uint16_t hw_num[TF_RESC_TYPE_HW_MAX];
};

/**
 * Resource query array of SRAM entities
 */
struct tf_rm_sram_query {
	/** array of SRAM resource entries */
	struct tf_rm_query_entry sram_query[TF_RESC_TYPE_SRAM_MAX];
};

/**
 * Resource allocation array of SRAM entities
 */
struct tf_rm_sram_alloc {
	/** array of SRAM resource entries */
	uint16_t sram_num[TF_RESC_TYPE_SRAM_MAX];
};

/**
 * Resource Manager arrays for a single direction
 */
struct tf_rm_resc {
	/** array of HW resource entries */
	struct tf_rm_entry hw_entry[TF_RESC_TYPE_HW_MAX];
	/** array of SRAM resource entries */
	struct tf_rm_entry sram_entry[TF_RESC_TYPE_SRAM_MAX];
};

/**
 * Resource Manager Database
 */
struct tf_rm_db {
	struct tf_rm_resc rx;
	struct tf_rm_resc tx;
};

/**
 * Helper function converting direction to text string
 */
const char
*tf_dir_2_str(enum tf_dir dir);

/**
 * Helper function converting identifier to text string
 */
const char
*tf_ident_2_str(enum tf_identifier_type id_type);

/**
 * Helper function converting tcam type to text string
 */
const char
*tf_tcam_tbl_2_str(enum tf_tcam_tbl_type tcam_type);

/**
 * Helper function used to convert HW HCAPI resource type to a string.
 */
const char
*tf_hcapi_hw_2_str(enum tf_resource_type_hw hw_type);

/**
 * Helper function used to convert SRAM HCAPI resource type to a string.
 */
const char
*tf_hcapi_sram_2_str(enum tf_resource_type_sram sram_type);

/**
 * Initializes the Resource Manager and the associated database
 * entries for HW and SRAM resources. Must be called before any other
 * Resource Manager functions.
 *
 * [in] tfp
 *   Pointer to TF handle
 */
void tf_rm_init(struct tf *tfp);

/**
 * Allocates and validates both HW and SRAM resources per the NVM
 * configuration. If any allocation fails all resources for the
 * session is deallocated.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_allocate_validate(struct tf *tfp);

/**
 * Closes the Resource Manager and frees all allocated resources per
 * the associated database.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOTEMPTY) if resources are not cleaned up before close
 */
int tf_rm_close(struct tf *tfp);

#if (TF_SHADOW == 1)
/**
 * Initializes Shadow DB of configuration elements
 *
 * [in] tfs
 *   Pointer to TF Session
 *
 * Returns:
 *  0  - Success
 */
int tf_rm_shadow_db_init(struct tf_session *tfs);
#endif /* TF_SHADOW */

/**
 * Perform a Session Pool lookup using the Tcam table type.
 *
 * Function will print error msg if tcam type is unsupported or lookup
 * failed.
 *
 * [in] tfs
 *   Pointer to TF Session
 *
 * [in] type
 *   Type of the object
 *
 * [in] dir
 *    Receive or transmit direction
 *
 * [in/out]  session_pool
 *   Session pool
 *
 * Returns:
 *  0           - Success will set the **pool
 *  -EOPNOTSUPP - Type is not supported
 */
int
tf_rm_lookup_tcam_type_pool(struct tf_session *tfs,
			    enum tf_dir dir,
			    enum tf_tcam_tbl_type type,
			    struct bitalloc **pool);

/**
 * Perform a Session Pool lookup using the Table type.
 *
 * Function will print error msg if table type is unsupported or
 * lookup failed.
 *
 * [in] tfs
 *   Pointer to TF Session
 *
 * [in] type
 *   Type of the object
 *
 * [in] dir
 *    Receive or transmit direction
 *
 * [in/out]  session_pool
 *   Session pool
 *
 * Returns:
 *  0           - Success will set the **pool
 *  -EOPNOTSUPP - Type is not supported
 */
int
tf_rm_lookup_tbl_type_pool(struct tf_session *tfs,
			   enum tf_dir dir,
			   enum tf_tbl_type type,
			   struct bitalloc **pool);

/**
 * Converts the TF Table Type to internal HCAPI_TYPE
 *
 * [in] type
 *   Type to be converted
 *
 * [in/out] hcapi_type
 *   Converted type
 *
 * Returns:
 *  0           - Success will set the *hcapi_type
 *  -EOPNOTSUPP - Type is not supported
 */
int
tf_rm_convert_tbl_type(enum tf_tbl_type type,
		       uint32_t *hcapi_type);

/**
 * TF RM Convert of index methods.
 */
enum tf_rm_convert_type {
	/** Adds the base of the Session Pool to the index */
	TF_RM_CONVERT_ADD_BASE,
	/** Removes the Session Pool base from the index */
	TF_RM_CONVERT_RM_BASE
};

/**
 * Provides conversion of the Table Type index in relation to the
 * Session Pool base.
 *
 * [in] tfs
 *   Pointer to TF Session
 *
 * [in] dir
 *    Receive or transmit direction
 *
 * [in] type
 *   Type of the object
 *
 * [in] c_type
 *   Type of conversion to perform
 *
 * [in] index
 *   Index to be converted
 *
 * [in/out]  convert_index
 *   Pointer to the converted index
 */
int
tf_rm_convert_index(struct tf_session *tfs,
		    enum tf_dir dir,
		    enum tf_tbl_type type,
		    enum tf_rm_convert_type c_type,
		    uint32_t index,
		    uint32_t *convert_index);

#endif /* TF_RM_H_ */
