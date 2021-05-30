/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_HA_MGR_H_
#define _ULP_HA_MGR_H_

#include "bnxt_ulp.h"

enum ulp_ha_mgr_state {
	ULP_HA_STATE_INIT,
	ULP_HA_STATE_PRIM_RUN,
	ULP_HA_STATE_PRIM_SEC_RUN,
	ULP_HA_STATE_SEC_TIMER_COPY,
	ULP_HA_PRIM_CLOSE
};

enum ulp_ha_mgr_app_type {
	ULP_HA_APP_TYPE_NONE,
	ULP_HA_APP_TYPE_PRIM,
	ULP_HA_APP_TYPE_SEC
};

enum ulp_ha_mgr_region {
	ULP_HA_REGION_LOW,
	ULP_HA_REGION_HI
};

struct bnxt_ulp_ha_mgr_info {
	enum ulp_ha_mgr_app_type app_type;
	enum ulp_ha_mgr_region region;
	uint32_t flags;
	pthread_mutex_t ha_lock;
};

bool
ulp_ha_mgr_is_enabled(struct bnxt_ulp_context *ulp_ctx);

int32_t
ulp_ha_mgr_enable(struct bnxt_ulp_context *ulp_ctx);

int32_t
ulp_ha_mgr_init(struct bnxt_ulp_context *ulp_ctx);

void
ulp_ha_mgr_deinit(struct bnxt_ulp_context *ulp_ctx);

int32_t
ulp_ha_mgr_app_type_get(struct bnxt_ulp_context *ulp_ctx,
			enum ulp_ha_mgr_app_type *app_type);

int32_t
ulp_ha_mgr_state_get(struct bnxt_ulp_context *ulp_ctx,
		     enum ulp_ha_mgr_state *state);

int32_t
ulp_ha_mgr_open(struct bnxt_ulp_context *ulp_ctx);

int32_t
ulp_ha_mgr_close(struct bnxt_ulp_context *ulp_ctx);

int32_t
ulp_ha_mgr_region_get(struct bnxt_ulp_context *ulp_ctx,
		      enum ulp_ha_mgr_region *region);

#endif /* _ULP_HA_MGR_H_*/
