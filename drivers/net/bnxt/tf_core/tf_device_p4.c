/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include <cfa_resource_types.h>

#include "tf_device.h"
#include "tf_identifier.h"
#include "tf_tbl_type.h"
#include "tf_tcam.h"
#include "tf_em.h"

/**
 * Device specific function that retrieves the MAX number of HCAPI
 * types the device supports.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] max_types
 *   Pointer to the MAX number of HCAPI types supported
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p4_get_max_types(struct tf *tfp __rte_unused,
			uint16_t *max_types)
{
	if (max_types == NULL)
		return -EINVAL;

	*max_types = CFA_RESOURCE_TYPE_P4_LAST + 1;

	return 0;
}

/**
 * Device specific function that retrieves the WC TCAM slices the
 * device supports.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] slice_size
 *   Pointer to the WC TCAM slice size
 *
 * [out] num_slices_per_row
 *   Pointer to the WC TCAM row slice configuration
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p4_get_tcam_slice_info(struct tf *tfp __rte_unused,
			      enum tf_tcam_tbl_type type,
			      uint16_t key_sz,
			      uint16_t *num_slices_per_row)
{
#define CFA_P4_WC_TCAM_SLICES_PER_ROW 2
#define CFA_P4_WC_TCAM_SLICE_SIZE     12

	if (type == TF_TCAM_TBL_TYPE_WC_TCAM) {
		*num_slices_per_row = CFA_P4_WC_TCAM_SLICES_PER_ROW;
		if (key_sz > *num_slices_per_row * CFA_P4_WC_TCAM_SLICE_SIZE)
			return -ENOTSUP;
	} else { /* for other type of tcam */
		*num_slices_per_row = 1;
	}

	return 0;
}

/**
 * Truflow P4 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p4_init = {
	.tf_dev_get_max_types = tf_dev_p4_get_max_types,
	.tf_dev_get_tcam_slice_info = tf_dev_p4_get_tcam_slice_info,
	.tf_dev_alloc_ident = NULL,
	.tf_dev_free_ident = NULL,
	.tf_dev_alloc_tbl = NULL,
	.tf_dev_free_tbl = NULL,
	.tf_dev_alloc_search_tbl = NULL,
	.tf_dev_set_tbl = NULL,
	.tf_dev_get_tbl = NULL,
	.tf_dev_alloc_tcam = NULL,
	.tf_dev_free_tcam = NULL,
	.tf_dev_alloc_search_tcam = NULL,
	.tf_dev_set_tcam = NULL,
	.tf_dev_get_tcam = NULL,
};

/**
 * Truflow P4 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p4 = {
	.tf_dev_get_max_types = tf_dev_p4_get_max_types,
	.tf_dev_get_tcam_slice_info = tf_dev_p4_get_tcam_slice_info,
	.tf_dev_alloc_ident = tf_ident_alloc,
	.tf_dev_free_ident = tf_ident_free,
	.tf_dev_alloc_tbl = tf_tbl_alloc,
	.tf_dev_free_tbl = tf_tbl_free,
	.tf_dev_alloc_search_tbl = NULL,
	.tf_dev_set_tbl = tf_tbl_set,
	.tf_dev_get_tbl = tf_tbl_get,
	.tf_dev_alloc_tcam = tf_tcam_alloc,
	.tf_dev_free_tcam = tf_tcam_free,
	.tf_dev_alloc_search_tcam = NULL,
	.tf_dev_set_tcam = tf_tcam_set,
	.tf_dev_get_tcam = NULL,
	.tf_dev_insert_em_entry = tf_em_insert_entry,
	.tf_dev_delete_em_entry = tf_em_delete_entry,
};
