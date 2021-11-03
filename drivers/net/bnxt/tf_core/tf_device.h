/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_DEVICE_H_
#define _TF_DEVICE_H_

#include "cfa_resource_types.h"
#include "tf_core.h"
#include "tf_identifier.h"
#include "tf_tbl.h"
#include "tf_tcam.h"
#ifdef TF_TCAM_SHARED
#include "tf_tcam_shared.h"
#endif
#include "tf_if_tbl.h"
#include "tf_global_cfg.h"

struct tf;
struct tf_session;

/**
 * The Device module provides a general device template. A supported
 * device type should implement one or more of the listed function
 * pointers according to its capabilities.
 *
 * If a device function pointer is NULL the device capability is not
 * supported.
 */

/**
 * TF device information
 */
struct tf_dev_info {
	enum tf_device_type type;
	const struct tf_dev_ops *ops;
};

/**
 * This structure can be used to translate the CFA resource type to TF type.
 */
struct tf_hcapi_resource_map {
	/**
	 * Truflow module type associated with this resource type.
	 */
	enum tf_module_type module_type;

	/**
	 * Bitmap of TF sub-type for the element.
	 */
	uint32_t type_caps;
};

/**
 * @page device Device
 *
 * @ref tf_dev_bind
 *
 * @ref tf_dev_unbind
 */

/**
 * Device bind handles the initialization of the specified device
 * type.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] type
 *   Device type
 *
 * [in] resources
 *   Pointer to resource allocation information
 *
 * [in] wc_num_slices
 *   Number of slices per row for WC
 *
 * [out] dev_handle
 *   Device handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) parameter failure.
 *   - (-ENODEV) no such device supported.
 */
int tf_dev_bind(struct tf *tfp,
		enum tf_device_type type,
		bool shadow_copy,
		struct tf_session_resources *resources,
		uint16_t wc_num_slices,
		struct tf_dev_info *dev_handle);

/**
 * Device release handles cleanup of the device specific information.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dev_handle
 *   Device handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) parameter failure.
 *   - (-ENODEV) no such device supported.
 */
int tf_dev_unbind(struct tf *tfp,
		  struct tf_dev_info *dev_handle);

int
tf_dev_bind_ops(enum tf_device_type type,
		struct tf_dev_info *dev_handle);

/**
 * Truflow device specific function hooks structure
 *
 * The following device hooks can be defined; unless noted otherwise,
 * they are optional and can be filled with a null pointer. The
 * purpose of these hooks is to support Truflow device operations for
 * different device variants.
 */
struct tf_dev_ops {
	/**
	 * Retrieves the MAX number of resource types that the device
	 * supports.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [out] max_types
	 *   Pointer to MAX number of types the device supports
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_max_types)(struct tf *tfp,
				    uint16_t *max_types);

	/**
	 * Retrieves the string description for the CFA resource
	 * type
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] resource_id
	 *   HCAPI cfa resource type id
	 *
	 * [out] resource_str
	 *   Pointer to a string
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_resource_str)(struct tf *tfp,
				       uint16_t resource_id,
				       const char **resource_str);

	/**
	 * Set the WC TCAM slice information that the device
	 * supports.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] num_slices_per_row
	 *   Number of slices per row the device supports
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_tcam_slice_info)(struct tf *tfp,
					  enum tf_wc_num_slice num_slices_per_row);

	/**
	 * Retrieves the WC TCAM slice information that the device
	 * supports.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] type
	 *   TCAM table type
	 *
	 * [in] key_sz
	 *   Key size
	 *
	 * [out] num_slices_per_row
	 *   Pointer to number of slices per row the device supports
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_tcam_slice_info)(struct tf *tfp,
					  enum tf_tcam_tbl_type type,
					  uint16_t key_sz,
					  uint16_t *num_slices_per_row);

	/**
	 * Allocation of an identifier element.
	 *
	 * This API allocates the specified identifier element from a
	 * device specific identifier DB. The allocated element is
	 * returned.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to identifier allocation parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_ident)(struct tf *tfp,
				  struct tf_ident_alloc_parms *parms);

	/**
	 * Free of an identifier element.
	 *
	 * This API free's a previous allocated identifier element from a
	 * device specific identifier DB.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to identifier free parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_free_ident)(struct tf *tfp,
				 struct tf_ident_free_parms *parms);

	/**
	 * Search of an identifier element.
	 *
	 * This API search the specified identifier element from a
	 * device specific identifier shadow DB. The allocated element
	 * is returned.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to identifier search parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_search_ident)(struct tf *tfp,
				   struct tf_ident_search_parms *parms);

	/**
	 * Retrieves the identifier resource info.
	 *
	 * This API retrieves the identifier resource info from the rm db.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to identifier info
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_ident_resc_info)(struct tf *tfp,
					  struct tf_identifier_resource_info *parms);

	/**
	 * Indicates whether the index table type is SRAM managed
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] type
	 *   Truflow index table type, e.g. TF_TYPE_FULL_ACT_RECORD
	 *
	 * Returns
	 *   - (0) if the table is not managed by the SRAM manager
	 *   - (1) if the table is managed by the SRAM manager
	 */
	bool (*tf_dev_is_sram_managed)(struct tf *tfp,
				       enum tf_tbl_type tbl_type);

	/**
	 * Get SRAM table information.
	 *
	 * Converts an internal RM allocated element offset to
	 * a user address and vice versa.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] type
	 *   Truflow index table type, e.g. TF_TYPE_FULL_ACT_RECORD
	 *
	 * [in/out] base
	 *   Pointer to the base address of the associated table type.
	 *
	 * [in/out] shift
	 *   Pointer to any shift required for the associated table type.
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_tbl_info)(struct tf *tfp,
				   void *tbl_db,
				   enum tf_tbl_type type,
				   uint16_t *base,
				   uint16_t *shift);

	/**
	 * Allocation of an index table type element.
	 *
	 * This API allocates the specified table type element from a
	 * device specific table type DB. The allocated element is
	 * returned.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table allocation parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_tbl)(struct tf *tfp,
				struct tf_tbl_alloc_parms *parms);

	/**
	 * Allocation of an SRAM index table type element.
	 *
	 * This API allocates the specified table type element from a
	 * device specific table type DB. The allocated element is
	 * returned.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table allocation parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_sram_tbl)(struct tf *tfp,
				     struct tf_tbl_alloc_parms *parms);
	/**
	 * Allocation of a external table type element.
	 *
	 * This API allocates the specified table type element from a
	 * device specific table type DB. The allocated element is
	 * returned.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table allocation parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_ext_tbl)(struct tf *tfp,
				    struct tf_tbl_alloc_parms *parms);

	/**
	 * Free of a table type element.
	 *
	 * This API free's a previous allocated table type element from a
	 * device specific table type DB.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table free parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_free_tbl)(struct tf *tfp,
			       struct tf_tbl_free_parms *parms);
	/**
	 * Free of an SRAM table type element.
	 *
	 * This API free's a previous allocated table type element from a
	 * device specific table type DB.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table free parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_free_sram_tbl)(struct tf *tfp,
				    struct tf_tbl_free_parms *parms);
	/**
	 * Free of a external table type element.
	 *
	 * This API free's a previous allocated table type element from a
	 * device specific table type DB.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table free parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_free_ext_tbl)(struct tf *tfp,
				   struct tf_tbl_free_parms *parms);

	/**
	 * Sets the specified table type element.
	 *
	 * This API sets the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table set parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_tbl)(struct tf *tfp,
			      struct tf_tbl_set_parms *parms);

	/**
	 * Sets the specified external table type element.
	 *
	 * This API sets the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table set parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_ext_tbl)(struct tf *tfp,
				  struct tf_tbl_set_parms *parms);

	/**
	 * Sets the specified SRAM table type element.
	 *
	 * This API sets the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table set parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_sram_tbl)(struct tf *tfp,
				   struct tf_tbl_set_parms *parms);

	/**
	 * Retrieves the specified table type element.
	 *
	 * This API retrieves the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table get parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_tbl)(struct tf *tfp,
			      struct tf_tbl_get_parms *parms);

	/**
	 * Retrieves the specified SRAM table type element.
	 *
	 * This API retrieves the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table get parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_sram_tbl)(struct tf *tfp,
				   struct tf_tbl_get_parms *parms);

	/**
	 * Retrieves the specified table type element using 'bulk'
	 * mechanism.
	 *
	 * This API retrieves the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table get bulk parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_bulk_tbl)(struct tf *tfp,
				   struct tf_tbl_get_bulk_parms *parms);

	/**
	 * Retrieves the specified SRAM table type element using 'bulk'
	 * mechanism.
	 *
	 * This API retrieves the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table get bulk parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_bulk_sram_tbl)(struct tf *tfp,
					struct tf_tbl_get_bulk_parms *parms);

	/**
	 * Gets the increment value to add to the shared session resource
	 * start offset by for each count in the "stride"
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to get shared tbl increment parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_shared_tbl_increment)(struct tf *tfp,
				struct tf_get_shared_tbl_increment_parms *parms);

	/**
	 * Retrieves the table resource info.
	 *
	 * This API retrieves the table resource info from the rm db.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to tbl info
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_tbl_resc_info)(struct tf *tfp,
					 struct tf_tbl_resource_info *parms);

	/**
	 * Allocation of a tcam element.
	 *
	 * This API allocates the specified tcam element from a device
	 * specific tcam DB. The allocated element is returned.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to tcam allocation parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_tcam)(struct tf *tfp,
				 struct tf_tcam_alloc_parms *parms);

	/**
	 * Free of a tcam element.
	 *
	 * This API free's a previous allocated tcam element from a
	 * device specific tcam DB.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to tcam free parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_free_tcam)(struct tf *tfp,
				struct tf_tcam_free_parms *parms);

	/**
	 * Searches for the specified tcam element in a shadow DB.
	 *
	 * This API searches for the specified tcam element in a
	 * device specific shadow DB. If the element is found the
	 * reference count for the element is updated. If the element
	 * is not found a new element is allocated from the tcam DB
	 * and then inserted into the shadow DB.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to tcam allocation and search parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_search_tcam)
			(struct tf *tfp,
			struct tf_tcam_alloc_search_parms *parms);

	/**
	 * Sets the specified tcam element.
	 *
	 * This API sets the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to tcam set parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_tcam)(struct tf *tfp,
			       struct tf_tcam_set_parms *parms);

	/**
	 * Retrieves the specified tcam element.
	 *
	 * This API retrieves the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to tcam get parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_tcam)(struct tf *tfp,
			       struct tf_tcam_get_parms *parms);

#ifdef TF_TCAM_SHARED
	/**
	 * Move TCAM shared entries
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_move_tcam)(struct tf *tfp,
			       struct tf_move_tcam_shared_entries_parms *parms);

	/**
	 * Move TCAM shared entries
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_clear_tcam)(struct tf *tfp,
			      struct tf_clear_tcam_shared_entries_parms *parms);

#endif /* TF_TCAM_SHARED */

	/**
	 * Retrieves the tcam resource info.
	 *
	 * This API retrieves the tcam resource info from the rm db.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to tcam info
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_tcam_resc_info)(struct tf *tfp,
					 struct tf_tcam_resource_info *parms);

	/**
	 * Insert EM hash entry API
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to E/EM insert parameters
	 *
	 *  Returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_insert_int_em_entry)(struct tf *tfp,
					  struct tf_insert_em_entry_parms *parms);

	/**
	 * Delete EM hash entry API
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to E/EM delete parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_delete_int_em_entry)(struct tf *tfp,
					  struct tf_delete_em_entry_parms *parms);

	/**
	 * Move EM hash entry API
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to E/EM move parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_move_int_em_entry)(struct tf *tfp,
					struct tf_move_em_entry_parms *parms);

	/**
	 * Insert EEM hash entry API
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to E/EM insert parameters
	 *
	 *  Returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_insert_ext_em_entry)(struct tf *tfp,
					  struct tf_insert_em_entry_parms *parms);

	/**
	 * Delete EEM hash entry API
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to E/EM delete parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_delete_ext_em_entry)(struct tf *tfp,
					  struct tf_delete_em_entry_parms *parms);

	/**
	 * Retrieves the em resource info.
	 *
	 * This API retrieves the em resource info from the rm db.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to em info
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_em_resc_info)(struct tf *tfp,
				       struct tf_em_resource_info *parms);

	/**
	 * Move EEM hash entry API
	 *
	 *   Pointer to E/EM move parameters
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to em info
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_move_ext_em_entry)(struct tf *tfp,
					struct tf_move_em_entry_parms *parms);

	/**
	 * Allocate EEM table scope
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table scope alloc parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_alloc_tbl_scope)(struct tf *tfp,
				      struct tf_alloc_tbl_scope_parms *parms);
	/**
	 * Map EEM parif
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] pf
	 * PF associated with the table scope
	 *
	 * [in] parif_bitmask
	 * Bitmask of PARIFs to enable
	 *
	 * [in/out] pointer to the parif_2_pf data to be updated
	 *
	 * [in/out] pointer to the parif_2_pf mask to be updated
	 *
	 * [in] sz_in_bytes - number of bytes to be written
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_map_parif)(struct tf *tfp,
				uint16_t parif_bitmask,
				uint16_t pf,
				uint8_t *data,
				uint8_t *mask,
				uint16_t sz_in_bytes);
	/**
	 * Map EEM table scope
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table scope map parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_map_tbl_scope)(struct tf *tfp,
				    struct tf_map_tbl_scope_parms *parms);

	/**
	 * Free EEM table scope
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table scope free parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_free_tbl_scope)(struct tf *tfp,
				     struct tf_free_tbl_scope_parms *parms);

	/**
	 * Sets the specified interface table type element.
	 *
	 * This API sets the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to interface table set parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_if_tbl)(struct tf *tfp,
				 struct tf_if_tbl_set_parms *parms);

	/**
	 * Retrieves the specified interface table type element.
	 *
	 * This API retrieves the specified element data by invoking the
	 * firmware.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table get parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_if_tbl)(struct tf *tfp,
				 struct tf_if_tbl_get_parms *parms);

	/**
	 * Update global cfg
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to global cfg parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_set_global_cfg)(struct tf *tfp,
				     struct tf_global_cfg_parms *parms);

	/**
	 * Get global cfg
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to global cfg parameters
	 *
	 *    returns:
	 *    0       - Success
	 *    -EINVAL - Error
	 */
	int (*tf_dev_get_global_cfg)(struct tf *tfp,
				     struct tf_global_cfg_parms *parms);

	/**
	 * Get mailbox
	 *
	 *    returns:
	 *      mailbox
	 */
	int (*tf_dev_get_mailbox)(void);

	/**
	 * Convert length in bit to length in byte and align to word.
	 * The word length depends on device type.
	 *
	 * [in] size
	 *   Size in bit
	 *
	 * Returns
	 *   Size in byte
	 */
	int (*tf_dev_word_align)(uint16_t size);

	/**
	 * Hash key using crc32 and lookup3
	 *
	 * [in] key_data
	 *   Pointer to key
	 *
	 * [in] bitlen
	 *   Number of key bits
	 *
	 * Returns
	 *   Hashes
	 */
	uint64_t (*tf_dev_cfa_key_hash)(uint64_t *key_data,
					  uint16_t bitlen);

	/**
	 * Translate the CFA resource type to Truflow type
	 *
	 * [in] hcapi_types
	 *   CFA resource type bitmap
	 *
	 * [out] ident_types
	 *   Pointer to identifier type bitmap
	 *
	 * [out] tcam_types
	 *   Pointer to tcam type bitmap
	 *
	 * [out] tbl_types
	 *   Pointer to table type bitmap
	 *
	 * [out] em_types
	 *   Pointer to em type bitmap
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_map_hcapi_caps)(uint64_t hcapi_caps,
				     uint32_t *ident_caps,
				     uint32_t *tcam_caps,
				     uint32_t *tbl_caps,
				     uint32_t *em_caps);

	/**
	 * Device specific function that retrieves the sram resource
	 *
	 * [in] query
	 *   Point to resources query result
	 *
	 * [out] sram_bank_caps
	 *   Pointer to SRAM bank capabilities
	 *
	 * [out] dynamic_sram_capable
	 *   Pointer to dynamic sram capable
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_sram_resources)(void *query,
					 uint32_t *sram_bank_caps,
					 bool *dynamic_sram_capable);

	/**
	 * Device specific function that sets the sram policy
	 *
	 * [in] dir
	 *   Receive or transmit direction
	 *
	 * [in] band_id
	 *   SRAM bank id
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_sram_policy)(enum tf_dir dir,
				      enum tf_sram_bank_id *bank_id);

	/**
	 * Device specific function that gets the sram policy
	 *
	 * [in] dir
	 *   Receive or transmit direction
	 *
	 * [in] band_id
	 *   pointer to SRAM bank id
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_sram_policy)(enum tf_dir dir,
				      enum tf_sram_bank_id *bank_id);
};

/**
 * Supported device operation structures
 */
extern const struct tf_dev_ops tf_dev_ops_p4_init;
extern const struct tf_dev_ops tf_dev_ops_p4;
extern const struct tf_dev_ops tf_dev_ops_p58_init;
extern const struct tf_dev_ops tf_dev_ops_p58;

/**
 * Supported device resource type mapping structures
 */
extern const struct tf_hcapi_resource_map tf_hcapi_res_map_p4[CFA_RESOURCE_TYPE_P4_LAST + 1];
extern const struct tf_hcapi_resource_map tf_hcapi_res_map_p58[CFA_RESOURCE_TYPE_P58_LAST + 1];

#endif /* _TF_DEVICE_H_ */
