/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_TF_H_
#define _BNXT_ULP_TF_H_

#include "bnxt.h"
#include <inttypes.h>
#include "ulp_template_db_enum.h"

struct tf *
bnxt_ulp_bp_tfp_get(struct bnxt *bp, enum bnxt_ulp_session_type type);

struct tf *
bnxt_get_tfp_session(struct bnxt *bp, enum bnxt_session_type type);

/* Function to set the tfp session details in the ulp context. */
int32_t
bnxt_ulp_cntxt_tfp_set(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_session_type s_type,
		       struct tf *tfp);

/* Function to get the tfp session details from ulp context. */
struct tf *
bnxt_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_session_type s_type);
#endif
