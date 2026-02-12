/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TF_COMMON_H_
#define _BNXT_TF_COMMON_H_

#include <inttypes.h>

#include "bnxt_ulp.h"
#include "ulp_template_db_enum.h"

#define BNXT_DRV_DBG(lvl, fmt, ...) \
	RTE_LOG(lvl, BNXT, "%s(): " fmt, __func__, ## __VA_ARGS__)


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
#define	BNXT_ULP_DFLT_TX_MAX_KEY		512
#define	BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY		256
#define	BNXT_ULP_DFLT_TX_MEM			0
#define	BNXT_ULP_TX_NUM_FLOWS			32

enum bnxt_tf_rc {
	BNXT_TF_RC_PARSE_ERR	= -2,
	BNXT_TF_RC_ERROR	= -1,
	BNXT_TF_RC_SUCCESS	= 0
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

#endif /* _BNXT_TF_COMMON_H_ */
