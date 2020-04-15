/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TF_COMMON_H_
#define _BNXT_TF_COMMON_H_

#define BNXT_TF_DBG(lvl, fmt, args...)	PMD_DRV_LOG(lvl, fmt, ## args)

#define BNXT_ULP_EM_FLOWS			8192
#define BNXT_ULP_1M_FLOWS			1000000
#define BNXT_EEM_RX_GLOBAL_ID_MASK		(BNXT_ULP_1M_FLOWS - 1)
#define BNXT_EEM_TX_GLOBAL_ID_MASK		(BNXT_ULP_1M_FLOWS - 1)
#define BNXT_EEM_HASH_KEY2_USED			0x8000000
#define BNXT_EEM_RX_HW_HASH_KEY2_BIT		BNXT_ULP_1M_FLOWS
#define	BNXT_ULP_DFLT_RX_MAX_KEY		512
#define	BNXT_ULP_DFLT_RX_MAX_ACTN_ENTRY		256
#define	BNXT_ULP_DFLT_RX_MEM			0
#define	BNXT_ULP_RX_NUM_FLOWS			32
#define	BNXT_ULP_RX_TBL_IF_ID			0
#define	BNXT_ULP_DFLT_TX_MAX_KEY		512
#define	BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY		256
#define	BNXT_ULP_DFLT_TX_MEM			0
#define	BNXT_ULP_TX_NUM_FLOWS			32
#define	BNXT_ULP_TX_TBL_IF_ID			0

enum bnxt_tf_rc {
	BNXT_TF_RC_PARSE_ERR	= -2,
	BNXT_TF_RC_ERROR	= -1,
	BNXT_TF_RC_SUCCESS	= 0
};

/* ulp direction Type */
enum ulp_direction_type {
	ULP_DIR_INGRESS,
	ULP_DIR_EGRESS,
};

struct bnxt_ulp_mark_tbl *
bnxt_ulp_cntxt_ptr2_mark_db_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_ptr2_mark_db_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_mark_tbl *mark_tbl);

#endif /* _BNXT_TF_COMMON_H_ */
