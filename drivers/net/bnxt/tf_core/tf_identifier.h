/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TF_IDENTIFIER_H_
#define _TF_IDENTIFIER_H_

#include "tf_core.h"

/**
 * The Identifier module provides processing of Identifiers.
 */

struct tf_ident_cfg_parms {
	/**
	 * [in] Number of identifier types in each of the
	 * configuration arrays
	 */
	uint16_t num_elements;
	/**
	 * [in] Identifier configuration array
	 */
	struct tf_rm_element_cfg *cfg;
	/**
	 * [in] Session resource allocations
	 */
	struct tf_session_resources *resources;
};

/**
 * Identifier allocation parameter definition
 */
struct tf_ident_alloc_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Identifier type
	 */
	enum tf_identifier_type type;
	/**
	 * [out] Identifier allocated
	 */
	uint16_t *id;
};

/**
 * Identifier free parameter definition
 */
struct tf_ident_free_parms {
	/**
	 * [in]	 receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Identifier type
	 */
	enum tf_identifier_type type;
	/**
	 * [in] ID to free
	 */
	uint16_t id;
	/**
	 * (experimental)
	 * [out] Current refcnt after free
	 */
	uint32_t *ref_cnt;
};

/**
 * Identifier search parameter definition
 */
struct tf_ident_search_parms {
	/**
	 * [in]  receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Identifier type
	 */
	enum tf_identifier_type type;
	/**
	 * [in] Identifier data to search for
	 */
	uint16_t search_id;
	/**
	 * [out] Set if matching identifier found
	 */
	bool *hit;
	/**
	 * [out] Current ref count after allocation
	 */
	uint32_t *ref_cnt;
};

/**
 * Identifier database
 *
 * Identifier rm database
 *
 */
struct ident_rm_db {
	struct rm_db *ident_db[TF_DIR_MAX];
};

/**
 * @page ident Identity Management
 *
 * @ref tf_ident_bind
 *
 * @ref tf_ident_unbind
 *
 * @ref tf_ident_alloc
 *
 * @ref tf_ident_free
 */

/**
 * Initializes the Identifier module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
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
int tf_ident_bind(struct tf *tfp,
		  struct tf_ident_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
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
int tf_ident_unbind(struct tf *tfp);

/**
 * Allocates a single identifier type.
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
int tf_ident_alloc(struct tf *tfp,
		   struct tf_ident_alloc_parms *parms);

/**
 * Free's a single identifier type.
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
int tf_ident_free(struct tf *tfp,
		  struct tf_ident_free_parms *parms);

/**
 * Search a single identifier type.
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
int tf_ident_search(struct tf *tfp,
		    struct tf_ident_search_parms *parms);

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
int tf_ident_get_resc_info(struct tf *tfp,
			   struct tf_identifier_resource_info *parms);
#endif /* _TF_IDENTIFIER_H_ */
