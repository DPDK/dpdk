/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
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
 * Table scope control block content
 */
struct tf_em_caps {
	uint32_t flags;
	uint32_t supported;
	uint32_t max_entries_supported;
	uint16_t key_entry_size;
	uint16_t record_entry_size;
	uint16_t efc_entry_size;
};

/** Invalid table scope id */
#define TF_TBL_SCOPE_INVALID 0xffffffff

/**
 * Table Scope Control Block
 *
 * Holds private data for a table scope. Only one instance of a table
 * scope with Internal EM is supported.
 */
struct tf_tbl_scope_cb {
	uint32_t tbl_scope_id;
	int index;
	struct hcapi_cfa_em_ctx_mem_info em_ctx_info[TF_DIR_MAX];
	struct tf_em_caps em_caps[TF_DIR_MAX];
	struct stack ext_act_pool[TF_DIR_MAX];
	uint32_t *ext_act_pool_mem[TF_DIR_MAX];
};

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
	 * [in] Result data to search for
	 */
	uint8_t *result;
	/**
	 * [in] Result data size in bytes
	 */
	uint16_t result_sz_in_bytes;
	/**
	 * [in] Whether or not to allocate on MISS, 1 is allocate.
	 */
	uint8_t alloc;
	/**
	 * [out] If search_enable, set if matching entry found
	 */
	uint8_t hit;
	/**
	 * [out] The status of the search (REJECT, MISS, HIT)
	 */
	enum tf_search_status search_status;
	/**
	 * [out] Current ref count after allocation
	 */
	uint16_t ref_cnt;
	/**
	 * [out] Idx of allocated entry or found entry
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

#endif /* TF_TBL_TYPE_H */
