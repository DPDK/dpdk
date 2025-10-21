/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2024 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_alarm.h>
#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_ulp_utils.h"
#include "ulp_tfc_ha_mgr.h"
#include "bnxt_ulp_tfc.h"

#define ULP_HOT_UPGRADE_TIMER_SEC 2 /* in seconds */

bool
bnxt_ulp_tfc_hot_upgrade_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	uint64_t feat_bits;

	if (unlikely(ulp_ctx == NULL || ulp_ctx->cfg_data == NULL))
		return false;

	feat_bits = bnxt_ulp_feature_bits_get(ulp_ctx);

	/* Both hot upgrade feature and devarg instance id must be configured */
	if ((feat_bits & BNXT_ULP_FEATURE_BIT_HOT_UPGRADE) &&
	    bnxt_ulp_app_instance_id_get(ulp_ctx))
		return true;
	return false;
}

bool
bnxt_ulp_tfc_hot_upgrade_is_secondary(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_tfc_ha_mgr_info *ha_info;

	if (unlikely(ulp_ctx == NULL || ulp_ctx->cfg_data == NULL))
		return false;

	ha_info = bnxt_ulp_cntxt_ptr2_tfc_ha_info_get(ulp_ctx);
	if (ha_info == NULL)
		return false;

	if (ha_info->ha_state == ULP_TFC_HA_STATE_SECONDARY)
		return true;
	return false;
}

static void
ulp_tfc_hot_upgrade_mgr_timer_cb(void *arg)
{
	struct bnxt_ulp_tfc_ha_mgr_info *ha_info;
	struct bnxt_ulp_context *ulp_ctx = NULL;
	uint8_t inst_id, inst_count = 0;
	int32_t restart_timer = 1;
	struct tfc *tfcp = NULL;
	uint16_t fw_fid = 0;
	int32_t rc = 0;

	ulp_ctx = bnxt_ulp_cntxt_entry_acquire(arg);
	if (ulp_ctx == NULL) {
		rte_eal_alarm_set(US_PER_S * ULP_HOT_UPGRADE_TIMER_SEC,
				  ulp_tfc_hot_upgrade_mgr_timer_cb, arg);
		return;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		goto cleanup;
	}

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		goto cleanup;
	}

	inst_id = bnxt_ulp_app_instance_id_get(ulp_ctx);
	if (unlikely(!inst_id)) {
		BNXT_DRV_DBG(ERR, "Invalid instance id\n");
		goto cleanup;
	}

	rc = tfc_hot_up_app_inst_count(tfcp, fw_fid, inst_id, &inst_count);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get hot upgrade status\n");
		goto cleanup;
	}

	/* Primary is no longer present */
	if (inst_count == 1) {
		ha_info = bnxt_ulp_cntxt_ptr2_tfc_ha_info_get(ulp_ctx);
		if (ha_info == NULL) {
			BNXT_DRV_DBG(ERR, "Failed to get ha_info\n");
			goto cleanup;
		}

		BNXT_DRV_DBG(DEBUG, "Transition to Primary\n");
		/* set the current instance as Primary */
		ha_info->ha_state = ULP_TFC_HA_STATE_PRIMARY;
		ha_info->app_inst_cnt = inst_count;

		/* Move the tcam entries to lower priority */
		rc = bnxt_ulp_hot_upgrade_process(ulp_ctx->bp);
		if (rc)
			BNXT_DRV_DBG(ERR, "Failed to update tcam entries\n");

		/* set the primary session */
		rc = tfc_hot_up_app_inst_set(tfcp, fw_fid, inst_id);
		if (rc)
			BNXT_DRV_DBG(ERR, "Failed to set as primary\n");
		restart_timer = 0;
	} else if (!inst_count) {
		BNXT_DRV_DBG(ERR, "Invalid instance count\n");
		restart_timer = 0;
	}

cleanup:
	bnxt_ulp_cntxt_entry_release();
	if (restart_timer)
		rte_eal_alarm_set(US_PER_S * ULP_HOT_UPGRADE_TIMER_SEC,
				  ulp_tfc_hot_upgrade_mgr_timer_cb, arg);
	else
		BNXT_DRV_DBG(DEBUG, "exiting tfc hot upgrade timer\n");
}


int32_t ulp_tfc_hot_upgrade_mgr_init(struct bnxt_ulp_context *ulp_ctx)
{
	uint8_t inst_id, inst_count = 0, session_cnt = 0;
	struct bnxt_ulp_tfc_ha_mgr_info *ha_info;
	struct tfc *tfcp = NULL;
	uint16_t fw_fid = 0;
	int32_t rc = 0;

	if (!bnxt_ulp_tfc_hot_upgrade_enabled(ulp_ctx))
		return rc;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return -EINVAL;
	}

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return -EINVAL;
	}

	ha_info = rte_zmalloc("ulp_ha_mgr_info", sizeof(*ha_info), 0);
	if (!ha_info)
		return -ENOMEM;

	/* Add the HA info tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_tfc_ha_info_set(ulp_ctx, ha_info);

	inst_id = bnxt_ulp_app_instance_id_get(ulp_ctx);
	if (!inst_id) {
		BNXT_DRV_DBG(ERR, "invalid instance id\n");
		goto cleanup;
	}

	/* get the application instance count */
	ha_info->app_inst_id = inst_id;
	rc = tfc_hot_up_app_inst_count(tfcp, fw_fid, inst_id, &inst_count);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get hot upgrade status\n");
		goto cleanup;
	}

	/* if instance count is 2 or more then just exit */
	if (inst_count >= 2) {
		BNXT_DRV_DBG(ERR, "More than 2 instances, invalid setup\n");
		goto cleanup;
	}

	/* Allocate the hot upgrade instance */
	rc = tfc_hot_up_app_inst_alloc(tfcp, fw_fid, inst_id, inst_count,
				       &session_cnt);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Fail to alloc hot upgrade session\n");
		goto cleanup;
	}
	/* If count is 1 then set as primary app */
	if (session_cnt == 1) {
		/* set the current instance as Primary */
		ha_info->ha_state = ULP_TFC_HA_STATE_PRIMARY;
		ha_info->app_inst_cnt = session_cnt;
		BNXT_DRV_DBG(DEBUG, "setting application as Primary\n");
	} else {
		/* set the current instance as secondary */
		ha_info->ha_state = ULP_TFC_HA_STATE_SECONDARY;
		ha_info->app_inst_cnt = session_cnt;
		BNXT_DRV_DBG(DEBUG, "setting application as Secondary\n");

		/* Start the Hot upgrade poll timer */
		rc = rte_eal_alarm_set(US_PER_S * ULP_HOT_UPGRADE_TIMER_SEC,
				       ulp_tfc_hot_upgrade_mgr_timer_cb,
				       (void *)ulp_ctx->cfg_data);
	}

	return rc;
cleanup:
	if (ha_info != NULL)
		ulp_tfc_hot_upgrade_mgr_deinit(ulp_ctx);
	return -EAGAIN;
}

static void
ulp_tfc_ha_mgr_timer_cancel(struct bnxt_ulp_context *ulp_ctx)
{
	rte_eal_alarm_cancel(ulp_tfc_hot_upgrade_mgr_timer_cb,
			     ulp_ctx->cfg_data);
}

void
ulp_tfc_hot_upgrade_mgr_deinit(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_tfc_ha_mgr_info *ha_info;
	struct tfc *tfcp = NULL;
	uint16_t fw_fid = 0;
	uint8_t inst_id;
	int32_t rc = 0;

	ha_info = bnxt_ulp_cntxt_ptr2_tfc_ha_info_get(ulp_ctx);
	if (ha_info == NULL)
		return;
	bnxt_ulp_cntxt_ptr2_tfc_ha_info_set(ulp_ctx, NULL);

	if (!bnxt_ulp_tfc_hot_upgrade_enabled(ulp_ctx))
		return;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (unlikely(tfcp == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get tfcp pointer");
		return;
	}

	if (unlikely(bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid))) {
		BNXT_DRV_DBG(ERR, "Failed to get func_id\n");
		return;
	}

	inst_id = bnxt_ulp_app_instance_id_get(ulp_ctx);
	if (unlikely(!inst_id)) {
		BNXT_DRV_DBG(ERR, "Failed to get instance id\n");
		return;
	}

	rc = tfc_hot_up_app_inst_free(tfcp, fw_fid, inst_id);
	if (rc)
		BNXT_DRV_DBG(ERR, "Fail to free hot upgrade session:%u\n",
			     inst_id);

	/* disable the hot upgrade timer */
	if (ha_info->ha_state == ULP_TFC_HA_STATE_SECONDARY)
		ulp_tfc_ha_mgr_timer_cancel(ulp_ctx);

	rte_free(ha_info);
}
