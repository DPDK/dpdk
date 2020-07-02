/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_DEVICE_H_
#define _TF_DEVICE_H_

#include "tf_core.h"
#include "tf_identifier.h"
#include "tf_tbl_type.h"
#include "tf_tcam.h"

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
	const struct tf_dev_ops *ops;
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
 * [out] dev_handle
 *   Device handle
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int dev_bind(struct tf *tfp,
	     enum tf_device_type type,
	     struct tf_session_resources *resources,
	     struct tf_dev_info *dev_handle);

/**
 * Device release handles cleanup of the device specific information.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] dev_handle
 *   Device handle
 */
int dev_unbind(struct tf *tfp,
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
	 * Allocation of a table type element.
	 *
	 * This API allocates the specified table type element from a
	 * device specific table type DB. The allocated element is
	 * returned.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table type allocation parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_tbl_type)(struct tf *tfp,
				     struct tf_tbl_type_alloc_parms *parms);

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
	 *   Pointer to table type free parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_free_tbl_type)(struct tf *tfp,
				    struct tf_tbl_type_free_parms *parms);

	/**
	 * Searches for the specified table type element in a shadow DB.
	 *
	 * This API searches for the specified table type element in a
	 * device specific shadow DB. If the element is found the
	 * reference count for the element is updated. If the element
	 * is not found a new element is allocated from the table type
	 * DB and then inserted into the shadow DB.
	 *
	 * [in] tfp
	 *   Pointer to TF handle
	 *
	 * [in] parms
	 *   Pointer to table type allocation and search parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_alloc_search_tbl_type)
			(struct tf *tfp,
			struct tf_tbl_type_alloc_search_parms *parms);

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
	 *   Pointer to table type set parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_set_tbl_type)(struct tf *tfp,
				   struct tf_tbl_type_set_parms *parms);

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
	 *   Pointer to table type get parameters
	 *
	 * Returns
	 *   - (0) if successful.
	 *   - (-EINVAL) on failure.
	 */
	int (*tf_dev_get_tbl_type)(struct tf *tfp,
				   struct tf_tbl_type_get_parms *parms);

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
};

/**
 * Supported device operation structures
 */
extern const struct tf_dev_ops tf_dev_ops_p4;

#endif /* _TF_DEVICE_H_ */
