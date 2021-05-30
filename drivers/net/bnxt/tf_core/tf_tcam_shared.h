/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_TCAM_SHARED_H_
#define _TF_TCAM_SHARED_H_

#include "tf_core.h"
#include "tf_tcam.h"

/**
 * @page tcam_shared TCAM SHARED
 *
 * @ref tf_tcam_shared_bind
 *
 * @ref tf_tcam_shared_unbind
 *
 * @ref tf_tcam_shared_alloc
 *
 * @ref tf_tcam_shared_free
 *
 * @ref tf_tcam_shared_set
 *
 * @ref tf_tcam_shared_get
 *
 * @ref tf_tcam_shared_move_p4
 *
 * @ref tf_tcam_shared_move_p58
 *
 * @ref tf_tcam_shared_clear
 */

/**
 * Initializes the TCAM shared module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_bind(struct tf *tfp,
			struct tf_tcam_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_unbind(struct tf *tfp);

/**
 * Allocates the requested tcam type from the internal RM DB.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_alloc(struct tf *tfp,
			 struct tf_tcam_alloc_parms *parms);

/**
 * Free's the requested table type and returns it to the DB.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_free(struct tf *tfp,
			struct tf_tcam_free_parms *parms);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_set(struct tf *tfp,
		       struct tf_tcam_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_get(struct tf *tfp,
		       struct tf_tcam_get_parms *parms);


/**
 * Moves entries from the WC_TCAM_HI to the WC_TCAM_LO shared pools
 * for the P4 device.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_move_p4(struct tf *tfp,
			   struct tf_move_tcam_shared_entries_parms *parms);

/**
 * Moves entries from the WC_TCAM_HI to the WC_TCAM_LO shared pools
 * for the P58 device.
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_move_p58(struct tf *tfp,
			    struct tf_move_tcam_shared_entries_parms *parms);

/**
 * Allocates and clears the entire WC_TCAM_HI or WC_TCAM_LO shared pools
 *
 * [in] tfp
 *   Pointer to the truflow handle
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_shared_clear(struct tf *tfp,
			 struct tf_clear_tcam_shared_entries_parms *parms);

#endif /* _TF_TCAM_SHARED_H */
