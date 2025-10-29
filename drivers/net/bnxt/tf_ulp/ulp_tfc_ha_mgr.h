/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2024 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TFC_HA_MGR_H_
#define _ULP_TFC_HA_MGR_H_

#include "bnxt_ulp.h"

enum ulp_tfc_ha_mgr_state {
	ULP_TFC_HA_STATE_INVALID = 0,
	ULP_TFC_HA_STATE_PRIMARY,
	ULP_TFC_HA_STATE_SECONDARY,
	ULP_TFC_HA_STATE_MAX,
};

struct bnxt_ulp_tfc_ha_mgr_info {
	enum ulp_tfc_ha_mgr_state ha_state;
	uint32_t app_inst_id;
	uint32_t app_inst_cnt;
};

bool
bnxt_ulp_tfc_hot_upgrade_enabled(struct bnxt_ulp_context *ulp_ctx);

bool
bnxt_ulp_tfc_hot_upgrade_is_secondary(struct bnxt_ulp_context *ulp_ctx);

int32_t
ulp_tfc_hot_upgrade_mgr_init(struct bnxt_ulp_context *ulp_ctx);

void
ulp_tfc_hot_upgrade_mgr_deinit(struct bnxt_ulp_context *ulp_ctx);

int32_t
ulp_ha_mgr_state_get(struct bnxt_ulp_context *ulp_ctx,
		     enum ulp_ha_mgr_state *state);

#endif /* _ULP_TFC_HA_MGR_H_*/
