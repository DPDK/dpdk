/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _TF_UTIL_H_
#define _TF_UTIL_H_

#include "tf_core.h"

/**
 * Helper function converting direction to text string
 *
 * [in] dir
 *   Receive or transmit direction identifier
 *
 * Returns:
 *   Pointer to a char string holding the string for the direction
 */
const char *tf_dir_2_str(enum tf_dir dir);

/**
 * Helper function converting identifier to text string
 *
 * [in] id_type
 *   Identifier type
 *
 * Returns:
 *   Pointer to a char string holding the string for the identifier
 */
const char *tf_ident_2_str(enum tf_identifier_type id_type);

/**
 * Helper function converting tcam type to text string
 *
 * [in] tcam_type
 *   TCAM type
 *
 * Returns:
 *   Pointer to a char string holding the string for the tcam
 */
const char *tf_tcam_tbl_2_str(enum tf_tcam_tbl_type tcam_type);

/**
 * Helper function converting tbl type to text string
 *
 * [in] tbl_type
 *   Table type
 *
 * Returns:
 *   Pointer to a char string holding the string for the table type
 */
const char *tf_tbl_type_2_str(enum tf_tbl_type tbl_type);

/**
 * Helper function converting em tbl type to text string
 *
 * [in] em_type
 *   EM type
 *
 * Returns:
 *   Pointer to a char string holding the string for the EM type
 */
const char *tf_em_tbl_type_2_str(enum tf_em_tbl_type em_type);

#endif /* _TF_UTIL_H_ */
