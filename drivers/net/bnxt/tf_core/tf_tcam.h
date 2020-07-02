/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_TCAM_H_
#define _TF_TCAM_H_

#include "tf_core.h"

/**
 * The TCAM module provides processing of Internal TCAM types.
 */

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
	struct tf_rm_element_cfg *tcam_cfg[TF_DIR_MAX];

	/**
	 * Shadow table type configuration array
	 */
	struct tf_shadow_tcam_cfg *tcam_shadow_cfg[TF_DIR_MAX];
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
	 * [out] Idx of allocated entry or found entry (if search_enable)
	 */
	uint32_t idx;
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
	enum tf_tcam_tbl_type tcam_tbl_type;
	/**
	 * [in] Enable search for matching entry
	 */
	uint8_t search_enable;
	/**
	 * [in] Key data to match on (if search)
	 */
	uint8_t *key;
	/**
	 * [in] key size in bits (if search)
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [in] Mask data to match on (if search)
	 */
	uint8_t *mask;
	/**
	 * [in] Priority of entry requested (definition TBD)
	 */
	uint32_t priority;
	/**
	 * [out] If search, set if matching entry found
	 */
	uint8_t hit;
	/**
	 * [out] Current refcnt after allocation
	 */
	uint16_t ref_cnt;
	/**
	 * [out] Idx allocated
	 *
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

#endif /* _TF_TCAM_H */
