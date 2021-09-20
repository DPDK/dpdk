/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef TF_TBL_TYPE_H_
#define TF_TBL_TYPE_H_

#include "tf_core.h"
#include "stack.h"

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
	 * [in] Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [out] Idx of allocated entry or found entry (if search_enable)
	 */
	uint32_t *idx;
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
	 * [in] Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] Index to free
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
	 * [in] Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
	 */
	uint32_t tbl_scope_id;
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
 * Table get bulk parameters
 */
struct tf_tbl_get_bulk_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to get
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Starting index to read from
	 */
	uint32_t starting_idx;
	/**
	 * [in] Number of sequential entries
	 */
	uint16_t num_entries;
	/**
	 * [in] Size of the single entry
	 */
	uint16_t entry_sz_in_bytes;
	/**
	 * [out] Host physical address, where the data
	 * will be copied to by the firmware.
	 * Use tfp_calloc() API and mem_pa
	 * variable of the tfp_calloc_parms
	 * structure for the physical address.
	 */
	uint64_t physical_mem_addr;
};

/**
 * Table RM database
 *
 * Table rm database
 *
 */
struct tbl_rm_db {
	struct rm_db *tbl_db[TF_DIR_MAX];
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
 * @ref tf_tbl_set
 *
 * @ref tf_tbl_get
 *
 * @ref tf_tbl_bulk_get
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
 * Frees the requested table type and returns it to the DB.
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

/**
 * Retrieves bulk block of elements by sending a firmware request to
 * get the elements.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table get bulk parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_bulk_get(struct tf *tfp,
		    struct tf_tbl_get_bulk_parms *parms);

/**
 * Retrieves the allocated resource info
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table resource info parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_tbl_get_resc_info(struct tf *tfp,
		     struct tf_tbl_resource_info *tbl);

#endif /* TF_TBL_TYPE_H */
