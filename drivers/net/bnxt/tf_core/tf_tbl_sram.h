/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef TF_TBL_SRAM_H_
#define TF_TBL_SRAM_H_

#include "tf_core.h"
#include "stack.h"

/**
 * The SRAM Table module provides processing of managed SRAM types.
 */

/**
 * @page  tblsram SRAM Table
 *
 * @ref tf_tbl_sram_bind
 *
 * @ref tf_tbl_sram_unbind
 *
 * @ref tf_tbl_sram_alloc
 *
 * @ref tf_tbl_sram_free
 *
 * @ref tf_tbl_sram_set
 *
 * @ref tf_tbl_sram_get
 *
 * @ref tf_tbl_sram_bulk_get
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
int tf_tbl_sram_bind(struct tf *tfp);

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
int tf_tbl_sram_unbind(struct tf *tfp);

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
int tf_tbl_sram_alloc(struct tf *tfp,
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
int tf_tbl_sram_free(struct tf *tfp,
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
int tf_tbl_sram_set(struct tf *tfp,
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
int tf_tbl_sram_get(struct tf *tfp,
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
int tf_tbl_sram_bulk_get(struct tf *tfp,
			 struct tf_tbl_get_bulk_parms *parms);
#endif /* TF_TBL_SRAM_H */
