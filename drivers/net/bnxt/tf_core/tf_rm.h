/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
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
 * MAX pool size of the Chip œneeds to be added to the tf_rm_elem_info
 * structure and several new APIs would need to be added to allow for
 * growth of a single TF resource type.
 *
 * The access functions does not check for NULL pointers as it's a
 * support module, not called directly.
 */

/**
 * Resource reservation single entry result. Used when accessing HCAPI
 * RM on the firmware.
 */
struct tf_rm_new_entry {
	/** Starting index of the allocated resource */
	uint16_t start;
	/** Number of allocated elements */
	uint16_t stride;
};

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
	/** HCAPI 'controlled', no RM storage thus the Device Module
	 *  using the RM can chose to handle storage locally.
	 */
	TF_RM_ELEM_CFG_HCAPI,
	/** HCAPI 'controlled', uses a Bit Allocator Pool for internal
	 *  storage in the RM.
	 */
	TF_RM_ELEM_CFG_HCAPI_BA,
	/**
	 * Shared element thus it belongs to a shared FW Session and
	 * is not controlled by the Host.
	 */
	TF_RM_ELEM_CFG_SHARED,
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

	/* If a HCAPI to TF type conversion is required then TF type
	 * can be added here.
	 */

	/**
	 * HCAPI RM Type for the element. Used for TF to HCAPI type
	 * conversion.
	 */
	uint16_t hcapi_type;
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
	struct tf_rm_new_entry entry;
};

/**
 * Create RM DB parameters
 */
struct tf_rm_create_db_parms {
	/**
	 * [in] Device module type. Used for logging purposes.
	 */
	enum tf_device_module_type type;
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
	 * on session open.
	 * Array size is num_elements.
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
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
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
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
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
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
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
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
	/**
	 * [out] Pointer to the requested allocation information for
	 * the specified db_index
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
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
	/**
	 * [out] Pointer to the hcapi type for the specified db_index
	 */
	uint16_t *hcapi_type;
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
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
	/**
	 * [out] Pointer to the inuse count for the specified db_index
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
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
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


#endif /* TF_RM_NEW_H_ */
