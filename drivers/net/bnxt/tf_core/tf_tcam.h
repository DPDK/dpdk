/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_TCAM_H_
#define _TF_TCAM_H_

#include "tf_core.h"

/**
 * The TCAM module provides processing of Internal TCAM types.
 */

/* Number of slices per row for WC TCAM */
extern uint16_t g_wc_num_slices_per_row;

/**
 * TCAM configuration parameters
 */
struct tf_tcam_cfg_parms {
	/**
	 * Number of tcam types in each of the configuration arrays
	 */
	uint16_t num_elements;
	/**
	 * TCAM configuration array
	 */
	struct tf_rm_element_cfg *cfg;
	/**
	 * Shadow table type configuration array
	 */
	struct tf_shadow_tcam_cfg *shadow_cfg;
	/**
	 * Boolean controlling the request shadow copy.
	 */
	bool shadow_copy;
	/**
	 * Session resource allocations
	 */
	struct tf_session_resources *resources;
	/**
	 * WC number of slices per row.
	 */
	enum tf_wc_num_slice wc_num_slices;
};

/**
 * TCAM allocation parameters
 */
struct tf_tcam_alloc_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation
	 */
	enum tf_tcam_tbl_type type;
	/**
	 * [in] key size
	 */
	uint16_t key_size;
	/**
	 * [in] Priority of entry requested (definition TBD)
	 */
	uint32_t priority;
	/**
	 * [out] Idx of allocated entry or found entry (if search_enable)
	 */
	uint16_t idx;
};

/**
 * TCAM free parameters
 */
struct tf_tcam_free_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation type
	 */
	enum tf_tcam_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Index to free
	 */
	uint16_t idx;
	/**
	 * [out] Reference count after free, only valid if session has been
	 * created with shadow_copy.
	 */
	uint16_t ref_cnt;
};

/**
 * TCAM allocate search parameters
 */
struct tf_tcam_alloc_search_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Key data to match on
	 */
	uint8_t *key;
	/**
	 * [in] key size in bits
	 */
	uint16_t key_size;
	/**
	 * [in] Mask data to match on
	 */
	uint8_t *mask;
	/**
	 * [in] Priority of entry requested (definition TBD)
	 */
	uint32_t priority;
	/**
	 * [in] Allocate on miss.
	 */
	uint8_t alloc;
	/**
	 * [out] Set if matching entry found
	 */
	uint8_t hit;
	/**
	 * [out] Search result status (hit, miss, reject)
	 */
	enum tf_search_status search_status;
	/**
	 * [out] Current refcnt after allocation
	 */
	uint16_t ref_cnt;
	/**
	 * [in,out] The result data from the search is copied here
	 */
	uint8_t *result;
	/**
	 * [in,out] result size in bits for the result data
	 */
	uint16_t result_size;
	/**
	 * [out] Index found
	 */
	uint16_t idx;
};

/**
 * TCAM set parameters
 */
struct tf_tcam_set_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to set
	 */
	enum tf_tcam_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Entry index to write to
	 */
	uint32_t idx;
	/**
	 * [in] array containing key
	 */
	uint8_t *key;
	/**
	 * [in] array containing mask fields
	 */
	uint8_t *mask;
	/**
	 * [in] key size
	 */
	uint16_t key_size;
	/**
	 * [in] array containing result
	 */
	uint8_t *result;
	/**
	 * [in] result size
	 */
	uint16_t result_size;
};

/**
 * TCAM get parameters
 */
struct tf_tcam_get_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to get
	 */
	enum tf_tcam_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Entry index to read
	 */
	uint32_t idx;
	/**
	 * [out] array containing key
	 */
	uint8_t *key;
	/**
	 * [out] array containing mask fields
	 */
	uint8_t *mask;
	/**
	 * [out] key size
	 */
	uint16_t key_size;
	/**
	 * [out] array containing result
	 */
	uint8_t *result;
	/**
	 * [out] result size
	 */
	uint16_t result_size;
};

/**
 * TCAM database
 *
 * Tcam rm database
 *
 */
struct tcam_rm_db {
	struct rm_db *tcam_db[TF_DIR_MAX];
};

/**
 * @page tcam TCAM
 *
 * @ref tf_tcam_bind
 *
 * @ref tf_tcam_unbind
 *
 * @ref tf_tcam_alloc
 *
 * @ref tf_tcam_free
 *
 * @ref tf_tcam_alloc_search
 *
 * @ref tf_tcam_set
 *
 * @ref tf_tcam_get
 *
 */

/**
 * Initializes the TCAM module with the requested DBs. Must be
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
int tf_tcam_bind(struct tf *tfp,
		 struct tf_tcam_cfg_parms *parms);

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
int tf_tcam_unbind(struct tf *tfp);

/**
 * Allocates the requested tcam type from the internal RM DB.
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
int tf_tcam_alloc(struct tf *tfp,
		  struct tf_tcam_alloc_parms *parms);

/**
 * Free's the requested table type and returns it to the DB. If shadow
 * DB is enabled its searched first and if found the element refcount
 * is decremented. If refcount goes to 0 then its returned to the
 * table type DB.
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
int tf_tcam_free(struct tf *tfp,
		 struct tf_tcam_free_parms *parms);

/**
 * Supported if Shadow DB is configured. Searches the Shadow DB for
 * any matching element. If found the refcount in the shadow DB is
 * updated accordingly. If not found a new element is allocated and
 * installed into the shadow DB.
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
int tf_tcam_alloc_search(struct tf *tfp,
			 struct tf_tcam_alloc_search_parms *parms);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
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
int tf_tcam_set(struct tf *tfp,
		struct tf_tcam_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
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
int tf_tcam_get(struct tf *tfp,
		struct tf_tcam_get_parms *parms);

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
int tf_tcam_get_resc_info(struct tf *tfp,
			  struct tf_tcam_resource_info *parms);

#endif /* _TF_TCAM_H */
