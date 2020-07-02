/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef TF_TBL_TYPE_H_
#define TF_TBL_TYPE_H_

#include "tf_core.h"

struct tf;

/**
 * The Table module provides processing of Internal TF table types.
 */

/**
 * Table configuration parameters
 */
struct tf_tbl_cfg_parms {
	/**
	 * Number of table types in each of the configuration arrays
	 */
	uint16_t num_elements;
	/**
	 * Table Type element configuration array
	 */
	struct tf_rm_element_cfg *cfg;
	/**
	 * Shadow table type configuration array
	 */
	struct tf_shadow_tbl_cfg *shadow_cfg;
	/**
	 * Boolean controlling the request shadow copy.
	 */
	bool shadow_copy;
	/**
	 * Session resource allocations
	 */
	struct tf_session_resources *resources;
};

/**
 * Table allocation parameters
 */
struct tf_tbl_alloc_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation
	 */
	enum tf_tbl_type type;
	/**
	 * [out] Idx of allocated entry or found entry (if search_enable)
	 */
	uint32_t idx;
};

/**
 * Table free parameters
 */
struct tf_tbl_free_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation type
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Index to free
	 */
	uint32_t idx;
	/**
	 * [out] Reference count after free, only valid if session has been
	 * created with shadow_copy.
	 */
	uint16_t ref_cnt;
};

/**
 * Table allocate search parameters
 */
struct tf_tbl_alloc_search_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] Enable search for matching entry. If the table type is
	 * internal the shadow copy will be searched before
	 * alloc. Session must be configured with shadow copy enabled.
	 */
	uint8_t search_enable;
	/**
	 * [in] Result data to search for (if search_enable)
	 */
	uint8_t *result;
	/**
	 * [in] Result data size in bytes (if search_enable)
	 */
	uint16_t result_sz_in_bytes;
	/**
	 * [out] If search_enable, set if matching entry found
	 */
	uint8_t hit;
	/**
	 * [out] Current ref count after allocation (if search_enable)
	 */
	uint16_t ref_cnt;
	/**
	 * [out] Idx of allocated entry or found entry (if search_enable)
	 */
	uint32_t idx;
};

/**
 * Table set parameters
 */
struct tf_tbl_set_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to set
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Entry data
	 */
	uint8_t *data;
	/**
	 * [in] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] Entry index to write to
	 */
	uint32_t idx;
};

/**
 * Table get parameters
 */
struct tf_tbl_get_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to get
	 */
	enum tf_tbl_type type;
	/**
	 * [out] Entry data
	 */
	uint8_t *data;
	/**
	 * [out] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] Entry index to read
	 */
	uint32_t idx;
};

/**
 * @page tbl Table
 *
 * @ref tf_tbl_bind
 *
 * @ref tf_tbl_unbind
 *
 * @ref tf_tbl_alloc
 *
 * @ref tf_tbl_free
 *
 * @ref tf_tbl_alloc_search
 *
 * @ref tf_tbl_set
 *
 * @ref tf_tbl_get
 */

/**
 * Initializes the Table module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table configuration parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_bind(struct tf *tfp,
		struct tf_tbl_cfg_parms *parms);

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
int tf_tbl_unbind(struct tf *tfp);

/**
 * Allocates the requested table type from the internal RM DB.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table allocation parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_alloc(struct tf *tfp,
		 struct tf_tbl_alloc_parms *parms);

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
 *   Pointer to Table free parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_free(struct tf *tfp,
		struct tf_tbl_free_parms *parms);

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
int tf_tbl_alloc_search(struct tf *tfp,
			struct tf_tbl_alloc_search_parms *parms);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table set parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_set(struct tf *tfp,
	       struct tf_tbl_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table get parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_get(struct tf *tfp,
	       struct tf_tbl_get_parms *parms);

#endif /* TF_TBL_TYPE_H */
