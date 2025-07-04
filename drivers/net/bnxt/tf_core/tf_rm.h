/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef TF_RM_NEW_H_
#define TF_RM_NEW_H_

#include "tf_core.h"
#include "bitalloc.h"
#include "tf_device.h"

struct tf;

/** RM return codes */
#define TF_RM_ALLOCATED_ENTRY_FREE        0
#define TF_RM_ALLOCATED_ENTRY_IN_USE      1
#define TF_RM_ALLOCATED_NO_ENTRY_FOUND   -1

/**
 * The Resource Manager (RM) module provides basic DB handling for
 * internal resources. These resources exists within the actual device
 * and are controlled by the HCAPI Resource Manager running on the
 * firmware.
 *
 * The RM DBs are all intended to be indexed using TF types there for
 * a lookup requires no additional conversion. The DB configuration
 * specifies the TF Type to HCAPI Type mapping and it becomes the
 * responsibility of the DB initialization to handle this static
 * mapping.
 *
 * Accessor functions are providing access to the DB, thus hiding the
 * implementation.
 *
 * The RM DB will work on its initial allocated sizes so the
 * capability of dynamically growing a particular resource is not
 * possible. If this capability later becomes a requirement then the
 * MAX pool size of the chip needs to be added to the tf_rm_elem_info
 * structure and several new APIs would need to be added to allow for
 * growth of a single TF resource type.
 *
 * The access functions do not check for NULL pointers as they are a
 * support module, not called directly.
 */

/**
 * RM Element configuration enumeration. Used by the Device to
 * indicate how the RM elements the DB consists off, are to be
 * configured at time of DB creation. The TF may present types to the
 * ULP layer that is not controlled by HCAPI within the Firmware.
 */
enum tf_rm_elem_cfg_type {
	/**
	 * No configuration
	 */
	TF_RM_ELEM_CFG_NULL,
	/** HCAPI 'controlled', no RM storage so the module
	 *  using the RM can chose to handle storage locally.
	 */
	TF_RM_ELEM_CFG_HCAPI,
	/** HCAPI 'controlled', uses a bit allocator pool for internal
	 *  storage in the RM.
	 */
	TF_RM_ELEM_CFG_HCAPI_BA,
	/**
	 * HCAPI 'controlled', uses a bit allocator pool for internal
	 * storage in the RM but multiple TF types map to a single
	 * HCAPI type.  Parent manages the table.
	 */
	TF_RM_ELEM_CFG_HCAPI_BA_PARENT,
	/**
	 * HCAPI 'controlled', uses a bit allocator pool for internal
	 * storage in the RM but multiple TF types map to a single
	 * HCAPI type.  Child accesses the parent db.
	 */
	TF_RM_ELEM_CFG_HCAPI_BA_CHILD,
	TF_RM_TYPE_MAX
};

/**
 * RM Reservation strategy enumeration. Type of strategy comes from
 * the HCAPI RM QCAPS handshake.
 */
enum tf_rm_resc_resv_strategy {
	TF_RM_RESC_RESV_STATIC_PARTITION,
	TF_RM_RESC_RESV_STRATEGY_1,
	TF_RM_RESC_RESV_STRATEGY_2,
	TF_RM_RESC_RESV_STRATEGY_3,
	TF_RM_RESC_RESV_MAX
};

/**
 * RM Element configuration structure, used by the Device to configure
 * how an individual TF type is configured in regard to the HCAPI RM
 * of same type.
 */
struct tf_rm_element_cfg {
	/**
	 * RM Element config controls how the DB for that element is
	 * processed.
	 */
	enum tf_rm_elem_cfg_type cfg_type;

	/**
	 * HCAPI RM Type for the element. Used for TF to HCAPI type
	 * conversion.
	 */
	uint16_t hcapi_type;

	/**
	 * if cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD/PARENT
	 *
	 * Parent Truflow module subtype associated with this resource type.
	 */
	uint16_t parent_subtype;

	/**
	 * if cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD/PARENT
	 *
	 * Resource slices.  How many slices will fit in the
	 * resource pool chunk size.
	 */
	uint8_t slices;
};

/**
 * Allocation information for a single element.
 */
struct tf_rm_alloc_info {
	/**
	 * HCAPI RM allocated range information.
	 *
	 * NOTE:
	 * In case of dynamic allocation support this would have
	 * to be changed to linked list of tf_rm_entry instead.
	 */
	struct tf_resource_info entry;
};

/**
 * Create RM DB parameters
 */
struct tf_rm_create_db_parms {
	/**
	 * [in] Module type. Used for logging purposes.
	 */
	enum tf_module_type module;
	/**
	 * [in] Receive or transmit direction.
	 */
	enum tf_dir dir;
	/**
	 * [in] Number of elements.
	 */
	uint16_t num_elements;
	/**
	 * [in] Parameter structure array. Array size is num_elements.
	 */
	struct tf_rm_element_cfg *cfg;
	/**
	 * Resource allocation count array. This array content
	 * originates from the tf_session_resources that is passed in
	 * on session open. Array size is num_elements.
	 */
	uint16_t *alloc_cnt;
	/**
	 * [out] RM DB Handle
	 */
	void **rm_db;
};

/**
 * Free RM DB parameters
 */
struct tf_rm_free_db_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
};

/**
 * Allocate RM parameters for a single element
 */
struct tf_rm_allocate_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] Module subtype indicates which DB entry to perform the
	 * action on.  (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
	 * TF_MODULE_TYPE_TCAM)
	 */
	uint16_t subtype;
	/**
	 * [in] Pointer to the allocated index in normalized
	 * form. Normalized means the index has been adjusted,
	 * i.e. Full Action Record offsets.
	 */
	uint32_t *index;
	/**
	 * [in] Priority, indicates the priority of the entry
	 * priority  0: allocate from top of the tcam (from index 0
	 *              or lowest available index)
	 * priority !0: allocate from bottom of the tcam (from highest
	 *              available index)
	 */
	uint32_t priority;
	/**
	 * [in] Pointer to the allocated index before adjusted.
	 */
	uint32_t *base_index;
};

/**
 * Free RM parameters for a single element
 */
struct tf_rm_free_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] TF subtype indicates which DB entry to perform the
	 * action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
	 * TF_MODULE_TYPE_TCAM)
	 */
	uint16_t subtype;
	/**
	 * [in] Index to free
	 */
	uint16_t index;
};

/**
 * Is Allocated parameters for a single element
 */
struct tf_rm_is_allocated_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] TF subtype indicates which DB entry to perform the
	 * action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
	 * TF_MODULE_TYPE_TCAM)
	 */
	uint16_t subtype;
	/**
	 * [in] Index to free
	 */
	uint32_t index;
	/**
	 * [in] Pointer to flag that indicates the state of the query
	 */
	int *allocated;
	/**
	 * [in] Pointer to the allocated index before adjusted.
	 */
	uint32_t *base_index;
};

/**
 * Get Allocation information for a single element
 */
struct tf_rm_get_alloc_info_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] TF subtype indicates which DB entry to perform the
	 * action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
	 * TF_MODULE_TYPE_TCAM)
	 */
	uint16_t subtype;
	/**
	 * [out] Pointer to the requested allocation information for
	 * the specified subtype
	 */
	struct tf_rm_alloc_info *info;
};

/**
 * Get HCAPI type parameters for a single element
 */
struct tf_rm_get_hcapi_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] TF subtype indicates which DB entry to perform the
	 * action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
	 * TF_MODULE_TYPE_TCAM)
	 */
	uint16_t subtype;
	/**
	 * [out] Pointer to the hcapi type for the specified subtype
	 */
	uint16_t *hcapi_type;
};
/**
 * Get Slices parameters for a single element
 */
struct tf_rm_get_slices_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] TF subtype indicates which DB entry to perform the
	 * action on. (e.g. TF_TBL_TYPE_FULL_ACTION subtype of module
	 * TF_MODULE_TYPE_TABLE)
	 */
	uint16_t subtype;
	/**
	 * [in/out] Pointer to number of slices for the given type
	 */
	uint16_t *slices;
};

/**
 * Get InUse count parameters for single element
 */
struct tf_rm_get_inuse_count_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] TF subtype indicates which DB entry to perform the
	 * action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
	 * TF_MODULE_TYPE_TCAM)
	 */
	uint16_t subtype;
	/**
	 * [out] Pointer to the inuse count for the specified subtype
	 */
	uint16_t *count;
};

/**
 * Check if the indexes are in the range of reserved resource
 */
struct tf_rm_check_indexes_in_range_parms {
	/**
	 * [in] RM DB Handle
	 */
	void *rm_db;
	/**
	 * [in] TF subtype indicates which DB entry to perform the
	 * action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
	 * TF_MODULE_TYPE_TCAM)
	 */
	uint16_t subtype;
	/**
	 * [in] Starting index
	 */
	uint16_t starting_index;
	/**
	 * [in] number of entries
	 */
	uint16_t num_entries;
};

/**
 * @page rm Resource Manager
 *
 * @ref tf_rm_create_db
 *
 * @ref tf_rm_free_db
 *
 * @ref tf_rm_allocate
 *
 * @ref tf_rm_free
 *
 * @ref tf_rm_is_allocated
 *
 * @ref tf_rm_get_info
 *
 * @ref tf_rm_get_hcapi_type
 *
 * @ref tf_rm_get_inuse_count
 *
 * @ref tf_rm_get_slice_size
 */

/**
 * Creates and fills a Resource Manager (RM) DB with requested
 * elements. The DB is indexed per the parms structure.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to create parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
/*
 * NOTE:
 * - Fail on parameter check
 * - Fail on DB creation, i.e. alloc amount is not possible or validation fails
 * - Fail on DB creation if DB already exist
 *
 * - Allocs local DB
 * - Does hcapi qcaps
 * - Does hcapi reservation
 * - Populates the pool with allocated elements
 * - Returns handle to the created DB
 */
int tf_rm_create_db(struct tf *tfp,
		    struct tf_rm_create_db_parms *parms);

/**
 * Creates and fills a Resource Manager (RM) DB with requested
 * elements. The DB is indexed per the parms structure. It only retrieve
 * allocated resource information for a exist session.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to create parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_create_db_no_reservation(struct tf *tfp,
		    struct tf_rm_create_db_parms *parms);

/**
 * Closes the Resource Manager (RM) DB and frees all allocated
 * resources per the associated database.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to free parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_free_db(struct tf *tfp,
		  struct tf_rm_free_db_parms *parms);

/**
 * Allocates a single element for the type specified, within the DB.
 *
 * [in] parms
 *   Pointer to allocate parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOMEM) if pool is empty
 */
int tf_rm_allocate(struct tf_rm_allocate_parms *parms);

/**
 * Free's a single element for the type specified, within the DB.
 *
 * [in] parms
 *   Pointer to free parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_free(struct tf_rm_free_parms *parms);

/**
 * Performs an allocation verification check on a specified element.
 *
 * [in] parms
 *   Pointer to is allocated parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
/*
 * NOTE:
 *  - If pool is set to Chip MAX, then the query index must be checked
 *    against the allocated range and query index must be allocated as well.
 *  - If pool is allocated size only, then check if query index is allocated.
 */
int tf_rm_is_allocated(struct tf_rm_is_allocated_parms *parms);

/**
 * Retrieves an elements allocation information from the Resource
 * Manager (RM) DB.
 *
 * [in] parms
 *   Pointer to get info parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_info(struct tf_rm_get_alloc_info_parms *parms);

/**
 * Retrieves all elements allocation information from the Resource
 * Manager (RM) DB.
 *
 * [in] parms
 *   Pointer to get info parameters
 *
 * [in] size
 *   number of the elements for the specific module
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_all_info(struct tf_rm_get_alloc_info_parms *parms, int size);

/**
 * Performs a lookup in the Resource Manager DB and retrieves the
 * requested HCAPI RM type.
 *
 * [in] parms
 *   Pointer to get hcapi parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_hcapi_type(struct tf_rm_get_hcapi_parms *parms);

/**
 * Performs a lookup in the Resource Manager DB and retrieves the
 * requested HCAPI RM type inuse count.
 *
 * [in] parms
 *   Pointer to get inuse parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_inuse_count(struct tf_rm_get_inuse_count_parms *parms);

/**
 * Check if the requested indexes are in the range of reserved resource.
 *
 * [in] parms
 *   Pointer to get inuse parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_rm_check_indexes_in_range(struct tf_rm_check_indexes_in_range_parms *parms);

/**
 * Get the number of slices per resource bit allocator for the resource type
 *
 * [in] parms
 *   Pointer to get inuse parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_rm_get_slices(struct tf_rm_get_slices_parms *parms);
#endif /* TF_RM_NEW_H_ */
