/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _TF_UTIL_H_
#define _TF_UTIL_H_

#include "tf_core.h"

/**
 * Helper function converting direction to text string
 */
const char
*tf_dir_2_str(enum tf_dir dir);

/**
 * Helper function converting identifier to text string
 */
const char
*tf_ident_2_str(enum tf_identifier_type id_type);

/**
 * Helper function converting tcam type to text string
 */
const char
*tf_tcam_tbl_2_str(enum tf_tcam_tbl_type tcam_type);

/**
 * Helper function converting tbl type to text string
 */
const char
*tf_tbl_type_2_str(enum tf_tbl_type tbl_type);

/**
 * Helper function converting em tbl type to text string
 */
const char
*tf_em_tbl_type_2_str(enum tf_em_tbl_type em_type);

#endif /* _TF_UTIL_H_ */
