/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include "tf_device.h"
#include "tf_identifier.h"
#include "tf_tbl_type.h"
#include "tf_tcam.h"

const struct tf_dev_ops tf_dev_ops_p4 = {
	.tf_dev_alloc_ident = tf_ident_alloc,
	.tf_dev_free_ident = tf_ident_free,
	.tf_dev_alloc_tbl_type = tf_tbl_type_alloc,
	.tf_dev_free_tbl_type = tf_tbl_type_free,
	.tf_dev_alloc_search_tbl_type = tf_tbl_type_alloc_search,
	.tf_dev_set_tbl_type = tf_tbl_type_set,
	.tf_dev_get_tbl_type = tf_tbl_type_get,
	.tf_dev_alloc_tcam = tf_tcam_alloc,
	.tf_dev_free_tcam = tf_tcam_free,
	.tf_dev_alloc_search_tcam = tf_tcam_alloc_search,
	.tf_dev_set_tcam = tf_tcam_set,
	.tf_dev_get_tcam = tf_tcam_get,
};
