/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TF_COMMON_H_
#define _BNXT_TF_COMMON_H_

#include <inttypes.h>

#include "bnxt_ulp.h"
#include "ulp_template_db_enum.h"

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
	BNXT_TF_RC_SUCCESS	= 0,
	BNXT_TF_RC_NORMAL	= 1,
	BNXT_TF_RC_FID		= 2,
};

/* eth IPv4 Type */
enum bnxt_ulp_eth_ip_type {
	BNXT_ULP_ETH_IPV4 = 4,
	BNXT_ULP_ETH_IPV6 = 5,
	BNXT_ULP_MAX_ETH_IP_TYPE = 0
};

/* ulp direction Type */
enum bnxt_ulp_direction_type {
	BNXT_ULP_DIR_INVALID,
	BNXT_ULP_DIR_INGRESS,
	BNXT_ULP_DIR_EGRESS,
};

/* enumeration of the interface types */
enum bnxt_ulp_intf_type {
	BNXT_ULP_INTF_TYPE_INVALID = 0,
	BNXT_ULP_INTF_TYPE_PF,
	BNXT_ULP_INTF_TYPE_TRUSTED_VF,
	BNXT_ULP_INTF_TYPE_VF,
	BNXT_ULP_INTF_TYPE_PF_REP,
	BNXT_ULP_INTF_TYPE_VF_REP,
	BNXT_ULP_INTF_TYPE_PHY_PORT,
	BNXT_ULP_INTF_TYPE_LAST
};

struct bnxt_ulp_mark_tbl *
bnxt_ulp_cntxt_ptr2_mark_db_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_ptr2_mark_db_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_mark_tbl *mark_tbl);

#endif /* _BNXT_TF_COMMON_H_ */
