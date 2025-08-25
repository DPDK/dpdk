/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>
#include <rte_spinlock.h>
#include <rte_mtr.h>
#include <rte_version.h>
#include <rte_hash_crc.h>

#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_ulp_utils.h"
#include "bnxt_ulp_tfc.h"
#include "bnxt_tf_common.h"
#include "hsi_struct_def_dpdk.h"
#include "tf_core.h"
#include "tf_ext_flow_handle.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_mark_mgr.h"
#include "ulp_fc_mgr.h"
#include "ulp_sc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_matcher.h"
#include "ulp_port_db.h"
#include "ulp_tun.h"
#include "ulp_ha_mgr.h"
#include "bnxt_tf_pmd_shim.h"
#include "ulp_template_db_tbl.h"
#include "tfc_resources.h"

/* define to enable shared table scope */
#define TFC_SHARED_TBL_SCOPE_ENABLE 0

bool
bnxt_ulp_cntxt_shared_tbl_scope_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	uint32_t flags = 0;
	int rc;

	rc = bnxt_ulp_cntxt_ptr2_ulp_flags_get(ulp_ctx, &flags);
	if (rc)
		return false;
	return !!(flags & BNXT_ULP_SHARED_TBL_SCOPE_ENABLED);
}

int32_t
bnxt_ulp_cntxt_tfcp_set(struct bnxt_ulp_context *ulp, struct tfc *tfcp)
{
	enum bnxt_ulp_tfo_type tfo_type = BNXT_ULP_TFO_TYPE_TFC;

	if (ulp == NULL)
		return -EINVAL;

	/* If NULL, this is invalidating an entry */
	if (tfcp == NULL)
		tfo_type = BNXT_ULP_TFO_TYPE_INVALID;
	ulp->tfo_type = tfo_type;
	ulp->tfcp = tfcp;

	return 0;
}

struct tfc *
bnxt_ulp_cntxt_tfcp_get(struct bnxt_ulp_context *ulp)
{
	if (ulp == NULL)
		return NULL;

	if (ulp->tfo_type != BNXT_ULP_TFO_TYPE_TFC) {
		BNXT_DRV_DBG(ERR, "Wrong tf type %d != %d\n",
			     ulp->tfo_type, BNXT_ULP_TFO_TYPE_TFC);
		return NULL;
	}

	return (struct tfc *)ulp->tfcp;
}

uint32_t
bnxt_ulp_cntxt_tbl_scope_max_pools_get(struct bnxt_ulp_context *ulp_ctx)
{
	/* Max pools can be 1 or greater, always return workable value */
	if (ulp_ctx != NULL &&
	    ulp_ctx->cfg_data != NULL &&
	    ulp_ctx->cfg_data->max_pools)
		return ulp_ctx->cfg_data->max_pools;
	return 1;
}

int32_t
bnxt_ulp_cntxt_tbl_scope_max_pools_set(struct bnxt_ulp_context *ulp_ctx,
				       uint32_t max)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;

	/* make sure that max is at least 1 */
	if (max == 0)
		max = 1;

	ulp_ctx->cfg_data->max_pools = max;
	return 0;
}

enum tfc_tbl_scope_bucket_factor
bnxt_ulp_cntxt_em_mulitplier_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return TFC_TBL_SCOPE_BUCKET_FACTOR_1;

	return ulp_ctx->cfg_data->em_multiplier;
}

int32_t
bnxt_ulp_cntxt_em_mulitplier_set(struct bnxt_ulp_context *ulp_ctx,
				 enum tfc_tbl_scope_bucket_factor factor)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;
	ulp_ctx->cfg_data->em_multiplier = factor;
	return 0;
}

uint32_t
bnxt_ulp_cntxt_num_rx_flows_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return 0;
	return ulp_ctx->cfg_data->num_rx_flows;
}

int32_t
bnxt_ulp_cntxt_num_rx_flows_set(struct bnxt_ulp_context *ulp_ctx, uint32_t num)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;
	ulp_ctx->cfg_data->num_rx_flows = num;
	return 0;
}

uint32_t
bnxt_ulp_cntxt_num_tx_flows_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return 0;
	return ulp_ctx->cfg_data->num_tx_flows;
}

int32_t
bnxt_ulp_cntxt_num_tx_flows_set(struct bnxt_ulp_context *ulp_ctx, uint32_t num)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;
	ulp_ctx->cfg_data->num_tx_flows = num;
	return 0;
}

uint16_t
bnxt_ulp_cntxt_em_rx_key_max_sz_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return 0;
	return ulp_ctx->cfg_data->em_rx_key_max_sz;
}

int32_t
bnxt_ulp_cntxt_em_rx_key_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				    uint16_t max)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;

	ulp_ctx->cfg_data->em_rx_key_max_sz = max;
	return 0;
}

uint16_t
bnxt_ulp_cntxt_em_tx_key_max_sz_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return 0;
	return ulp_ctx->cfg_data->em_tx_key_max_sz;
}

int32_t
bnxt_ulp_cntxt_em_tx_key_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				    uint16_t max)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;

	ulp_ctx->cfg_data->em_tx_key_max_sz = max;
	return 0;
}

uint16_t
bnxt_ulp_cntxt_act_rec_rx_max_sz_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return 0;
	return ulp_ctx->cfg_data->act_rx_max_sz;
}

int32_t
bnxt_ulp_cntxt_act_rec_rx_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				     int16_t max)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;

	ulp_ctx->cfg_data->act_rx_max_sz = max;
	return 0;
}

uint16_t
bnxt_ulp_cntxt_act_rec_tx_max_sz_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return 0;
	return ulp_ctx->cfg_data->act_tx_max_sz;
}

int32_t
bnxt_ulp_cntxt_act_rec_tx_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				     int16_t max)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return -EINVAL;

	ulp_ctx->cfg_data->act_tx_max_sz = max;
	return 0;
}

uint32_t
bnxt_ulp_cntxt_page_sz_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL)
		return 0;

	return ulp_ctx->cfg_data->page_sz;
}

int32_t
bnxt_ulp_cntxt_page_sz_set(struct bnxt_ulp_context *ulp_ctx,
			   uint32_t page_sz)
{
	if (ulp_ctx == NULL)
		return -EINVAL;
	ulp_ctx->cfg_data->page_sz = page_sz;
	return 0;
}

static int32_t
ulp_tfc_dparms_init(struct bnxt *bp,
		    struct bnxt_ulp_context *ulp_ctx,
		    uint32_t dev_id)
{
	struct bnxt_ulp_device_params *dparms;
	uint32_t num_flows = 0, num_rx_flows = 0, num_tx_flows = 0;

	/* The max_num_kflows were set, so move to external */
	if (bnxt_ulp_cntxt_mem_type_set(ulp_ctx, BNXT_ULP_FLOW_MEM_TYPE_EXT))
		return -EINVAL;

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(DEBUG, "Failed to get device parms\n");
		return -EINVAL;
	}

	if (bp->max_num_kflows) {
		num_flows = bp->max_num_kflows * 1024;
		dparms->ext_flow_db_num_entries = bp->max_num_kflows * 1024;
	} else {
		num_rx_flows = bnxt_ulp_cntxt_num_rx_flows_get(ulp_ctx);
		num_tx_flows = bnxt_ulp_cntxt_num_tx_flows_get(ulp_ctx);
		num_flows = num_rx_flows + num_tx_flows;
	}

	dparms->ext_flow_db_num_entries = num_flows;

	/* GFID =  2 * num_flows */
	dparms->mark_db_gfid_entries = dparms->ext_flow_db_num_entries * 2;
	BNXT_DRV_DBG(DEBUG, "Set the number of flows = %" PRIu64 "\n",
		    dparms->ext_flow_db_num_entries);

	return 0;
}

static void
ulp_tfc_tbl_scope_deinit(struct bnxt *bp)
{
	uint16_t fid = 0, fid_cnt = 0;
	struct tfc *tfcp;
	uint8_t tsid = 0;
	int32_t rc;

	tfcp = bnxt_ulp_cntxt_tfcp_get(bp->ulp_ctx);
	if (tfcp == NULL)
		return;

	rc = bnxt_ulp_cntxt_tsid_get(bp->ulp_ctx, &tsid);

	rc = bnxt_ulp_cntxt_fid_get(bp->ulp_ctx, &fid);
	if (rc)
		return;

	rc = tfc_tbl_scope_cpm_free(tfcp, tsid);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed Freeing CPM TSID:%d FID:%d\n",
			     tsid, fid);
	else
		BNXT_DRV_DBG(DEBUG, "Freed CPM TSID:%d FID: %d\n", tsid, fid);

	rc = tfc_tbl_scope_mem_free(tfcp, fid, tsid);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed freeing tscope mem TSID:%d FID:%d\n",
			     tsid, fid);
	else
		BNXT_DRV_DBG(DEBUG, "Freed tscope mem TSID:%d FID:%d\n",
			     tsid, fid);

	rc = tfc_tbl_scope_fid_rem(tfcp, fid, tsid, &fid_cnt);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed removing FID from TSID:%d FID:%d\n",
			     tsid, fid);
	else
		BNXT_DRV_DBG(DEBUG, "Removed FID from TSID:%d FID:%d\n",
			     tsid, fid);
}

static int32_t
ulp_tfc_tbl_scope_init(struct bnxt *bp)
{
	struct tfc_tbl_scope_mem_alloc_parms mem_parms;
	struct tfc_tbl_scope_size_query_parms qparms =  { 0 };
	uint16_t max_lkup_sz[CFA_DIR_MAX], max_act_sz[CFA_DIR_MAX];
	struct tfc_tbl_scope_cpm_alloc_parms cparms;
	uint16_t fid, max_pools;
	bool first = true, shared = false;
	uint8_t tsid = 0;
	struct tfc *tfcp;
	int32_t rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(bp->ulp_ctx);
	if (tfcp == NULL)
		return -EINVAL;

	fid = bp->fw_fid;

	max_pools = bnxt_ulp_cntxt_tbl_scope_max_pools_get(bp->ulp_ctx);
	max_lkup_sz[CFA_DIR_RX] =
		bnxt_ulp_cntxt_em_rx_key_max_sz_get(bp->ulp_ctx);
	max_lkup_sz[CFA_DIR_TX] =
		bnxt_ulp_cntxt_em_tx_key_max_sz_get(bp->ulp_ctx);
	max_act_sz[CFA_DIR_RX] =
		bnxt_ulp_cntxt_act_rec_rx_max_sz_get(bp->ulp_ctx);
	max_act_sz[CFA_DIR_TX] =
		bnxt_ulp_cntxt_act_rec_tx_max_sz_get(bp->ulp_ctx);

	shared = bnxt_ulp_cntxt_shared_tbl_scope_enabled(bp->ulp_ctx);

#if (TFC_SHARED_TBL_SCOPE_ENABLE == 1)
	/* Temporary code for testing shared table scopes until ULP
	 * usage defined.
	 */
	if (!BNXT_PF(bp)) {
		shared = true;
		max_pools = 8;
	}
#endif
	/* Calculate the sizes for setting up memory */
	qparms.shared = shared;
	qparms.max_pools = max_pools;
	qparms.factor = bnxt_ulp_cntxt_em_mulitplier_get(bp->ulp_ctx);
	qparms.flow_cnt[CFA_DIR_RX] =
		bnxt_ulp_cntxt_num_rx_flows_get(bp->ulp_ctx);
	qparms.flow_cnt[CFA_DIR_TX] =
		bnxt_ulp_cntxt_num_tx_flows_get(bp->ulp_ctx);
	qparms.key_sz_in_bytes[CFA_DIR_RX] = max_lkup_sz[CFA_DIR_RX];
	qparms.key_sz_in_bytes[CFA_DIR_TX] = max_lkup_sz[CFA_DIR_TX];
	qparms.act_rec_sz_in_bytes[CFA_DIR_RX] = max_act_sz[CFA_DIR_RX];
	qparms.act_rec_sz_in_bytes[CFA_DIR_TX] = max_act_sz[CFA_DIR_TX];
	rc = tfc_tbl_scope_size_query(tfcp, &qparms);
	if (rc)
		return rc;



	rc = tfc_tbl_scope_id_alloc(tfcp, shared, CFA_APP_TYPE_TF, &tsid,
				    &first);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to allocate tscope\n");
		return rc;
	}
	BNXT_DRV_DBG(DEBUG, "Allocated tscope TSID:%d\n", tsid);

	rc = bnxt_ulp_cntxt_tsid_set(bp->ulp_ctx, tsid);
	if (rc)
		return rc;

	/* If we are shared and not the first table scope creator
	 */
	if (shared && !first) {
		bool configured;
		#define ULP_SHARED_TSID_WAIT_TIMEOUT 5000
		#define ULP_SHARED_TSID_WAIT_TIME 50
		int32_t timeout = ULP_SHARED_TSID_WAIT_TIMEOUT;
		do {
			rte_delay_ms(ULP_SHARED_TSID_WAIT_TIME);
			rc = tfc_tbl_scope_config_state_get(tfcp, tsid, &configured);
			if (rc) {
				BNXT_DRV_DBG(ERR,
					     "Failed get tsid(%d) config state\n",
					     rc);
				return rc;
			}
			timeout -= ULP_SHARED_TSID_WAIT_TIME;
			BNXT_DRV_DBG(INFO,
				     "Waiting %d ms for shared tsid(%d)\n",
				     timeout, tsid);
		} while (!configured && timeout > 0);
		if (timeout <= 0) {
			BNXT_DRV_DBG(ERR, "Timed out on shared tsid(%d)\n",
				     tsid);
			return -ETIMEDOUT;
		}
	}
	mem_parms.first = first;
	mem_parms.static_bucket_cnt_exp[CFA_DIR_RX] =
		qparms.static_bucket_cnt_exp[CFA_DIR_RX];
	mem_parms.static_bucket_cnt_exp[CFA_DIR_TX] =
		qparms.static_bucket_cnt_exp[CFA_DIR_TX];
	mem_parms.lkup_rec_cnt[CFA_DIR_RX] = qparms.lkup_rec_cnt[CFA_DIR_RX];
	mem_parms.lkup_rec_cnt[CFA_DIR_TX] = qparms.lkup_rec_cnt[CFA_DIR_TX];
	mem_parms.act_rec_cnt[CFA_DIR_RX] = qparms.act_rec_cnt[CFA_DIR_RX];
	mem_parms.act_rec_cnt[CFA_DIR_TX] = qparms.act_rec_cnt[CFA_DIR_TX];
	mem_parms.pbl_page_sz_in_bytes =
		bnxt_ulp_cntxt_page_sz_get(bp->ulp_ctx);
	mem_parms.max_pools = max_pools;

	mem_parms.lkup_pool_sz_exp[CFA_DIR_RX] =
		qparms.lkup_pool_sz_exp[CFA_DIR_RX];
	mem_parms.lkup_pool_sz_exp[CFA_DIR_TX] =
		qparms.lkup_pool_sz_exp[CFA_DIR_TX];

	mem_parms.act_pool_sz_exp[CFA_DIR_RX] =
		qparms.act_pool_sz_exp[CFA_DIR_RX];
	mem_parms.act_pool_sz_exp[CFA_DIR_TX] =
		qparms.act_pool_sz_exp[CFA_DIR_TX];
	mem_parms.local = true;
	rc = tfc_tbl_scope_mem_alloc(tfcp, fid, tsid, &mem_parms);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "Failed to allocate tscope mem TSID:%d on FID:%d\n",
			     tsid, fid);
		return rc;
		}

	BNXT_DRV_DBG(DEBUG, "Allocated or set tscope mem TSID:%d on FID:%d\n",
		     tsid, fid);


	/* The max contiguous is in 32 Bytes records, so convert Bytes to 32
	 * Byte records.
	 */
	cparms.lkup_max_contig_rec[CFA_DIR_RX] = (max_lkup_sz[CFA_DIR_RX] + 31) / 32;
	cparms.lkup_max_contig_rec[CFA_DIR_TX] = (max_lkup_sz[CFA_DIR_TX] + 31) / 32;
	cparms.act_max_contig_rec[CFA_DIR_RX] = (max_act_sz[CFA_DIR_RX] + 31) / 32;
	cparms.act_max_contig_rec[CFA_DIR_TX] = (max_act_sz[CFA_DIR_TX] + 31) / 32;
	cparms.max_pools = max_pools;

	rc = tfc_tbl_scope_cpm_alloc(tfcp, tsid, &cparms);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed to allocate CPM TSID:%d FID:%d\n",
			     tsid, fid);
	else
		BNXT_DRV_DBG(DEBUG, "Allocated CPM TSID:%d FID:%d\n", tsid, fid);

	return rc;
}

static int32_t
ulp_tfc_cntxt_app_caps_init(struct bnxt *bp, uint8_t app_id, uint32_t dev_id)
{
	struct bnxt_ulp_app_capabilities_info *info;
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	uint32_t num = 0, rc;
	bool found = false;
	uint16_t i;

	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(ulp_ctx->cfg_data->ulp_flags)) {
		BNXT_DRV_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			    app_id, dev_id);
		return -EINVAL;
	}

	info = bnxt_ulp_app_cap_list_get(&num);
	if (!info || !num) {
		BNXT_DRV_DBG(ERR, "Failed to get app capabilities.\n");
		return -EINVAL;
	}

	for (i = 0; i < num && !found; i++) {
		if (info[i].app_id != app_id || info[i].device_id != dev_id)
			continue;
		found = true;
		if (info[i].flags & BNXT_ULP_APP_CAP_SHARED_EN)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_SHARED_SESSION_ENABLED;
		if (info[i].flags & BNXT_ULP_APP_CAP_HOT_UPGRADE_EN)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_HIGH_AVAIL_ENABLED;
		if (info[i].flags & BNXT_ULP_APP_CAP_UNICAST_ONLY)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_UNICAST_ONLY;
		if (info[i].flags & BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_TOS_PROTO_SUPPORT;
		if (info[i].flags & BNXT_ULP_APP_CAP_BC_MC_SUPPORT)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_BC_MC_SUPPORT;
		if (info[i].flags & BNXT_ULP_APP_CAP_SOCKET_DIRECT) {
			/* Enable socket direction only if MR is enabled in fw*/
			if (BNXT_MULTIROOT_EN(bp)) {
				ulp_ctx->cfg_data->ulp_flags |=
					BNXT_ULP_APP_SOCKET_DIRECT;
				BNXT_DRV_DBG(DEBUG,
					    "Socket Direct feature is enabled\n");
			}
		}
		/* Update the capability feature bits*/
		if (bnxt_ulp_cap_feat_process(info[i].feature_bits,
					      &ulp_ctx->cfg_data->feature_bits))
			return -EINVAL;

		bnxt_ulp_default_app_priority_set(ulp_ctx,
						  info[i].default_priority);
		bnxt_ulp_max_def_priority_set(ulp_ctx,
					      info[i].max_def_priority);
		bnxt_ulp_min_flow_priority_set(ulp_ctx,
					       info[i].min_flow_priority);
		bnxt_ulp_max_flow_priority_set(ulp_ctx,
					       info[i].max_flow_priority);

		bnxt_ulp_cntxt_ptr2_default_class_bits_set(ulp_ctx,
							   info[i].default_class_bits);
		bnxt_ulp_cntxt_ptr2_default_act_bits_set(ulp_ctx,
							 info[i].default_act_bits);

		rc = bnxt_ulp_cntxt_tbl_scope_max_pools_set(ulp_ctx,
							    info[i].max_pools);
		if (rc)
			return rc;
		rc = bnxt_ulp_cntxt_em_mulitplier_set(ulp_ctx,
						      info[i].em_multiplier);
		if (rc)
			return rc;

		rc = bnxt_ulp_cntxt_num_rx_flows_set(ulp_ctx,
						     info[i].num_rx_flows);
		if (rc)
			return rc;

		rc = bnxt_ulp_cntxt_num_tx_flows_set(ulp_ctx,
						     info[i].num_tx_flows);
		if (rc)
			return rc;

		rc = bnxt_ulp_cntxt_em_rx_key_max_sz_set(ulp_ctx,
							 info[i].em_rx_key_max_sz);
		if (rc)
			return rc;

		rc = bnxt_ulp_cntxt_em_tx_key_max_sz_set(ulp_ctx,
							 info[i].em_tx_key_max_sz);
		if (rc)
			return rc;

		rc = bnxt_ulp_cntxt_act_rec_rx_max_sz_set(ulp_ctx,
							  info[i].act_rx_max_sz);
		if (rc)
			return rc;

		rc = bnxt_ulp_cntxt_act_rec_tx_max_sz_set(ulp_ctx,
							  info[i].act_tx_max_sz);
		if (rc)
			return rc;

		rc = bnxt_ulp_cntxt_page_sz_set(ulp_ctx,
						info[i].pbl_page_sz_in_bytes);
		if (rc)
			return rc;
		bnxt_ulp_num_key_recipes_set(ulp_ctx,
					     info[i].num_key_recipes_per_dir);
	}
	if (!found) {
		BNXT_DRV_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			    app_id, dev_id);
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_APP_DEV_UNSUPPORTED;
		return -EINVAL;
	}

	return 0;
}

/* The function to free and deinit the ulp context data. */
static int32_t
ulp_tfc_ctx_deinit(struct bnxt *bp,
		   struct bnxt_ulp_session_state *session)
{
	/* Free the contents */
	if (session->cfg_data) {
		rte_free(session->cfg_data);
		bp->ulp_ctx->cfg_data = NULL;
		session->cfg_data = NULL;
	}
	return 0;
}

/* The function to allocate and initialize the ulp context data. */
static int32_t
ulp_tfc_ctx_init(struct bnxt *bp,
		 struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_data	*ulp_data;
	enum bnxt_ulp_device_id devid;
	int32_t	rc = 0;

	/* Initialize the context entries list */
	bnxt_ulp_cntxt_list_init();

	/* Allocate memory to hold ulp context data. */
	ulp_data = rte_zmalloc("bnxt_ulp_data",
			       sizeof(struct bnxt_ulp_data), 0);
	if (!ulp_data) {
		BNXT_DRV_DBG(ERR, "Failed to allocate memory for ulp data\n");
		return -ENOMEM;
	}

	/* Increment the ulp context data reference count usage. */
	bp->ulp_ctx->cfg_data = ulp_data;
	session->cfg_data = ulp_data;
	ulp_data->ref_cnt++;
	ulp_data->ulp_flags |= BNXT_ULP_VF_REP_ENABLED;

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add the context list entry\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_devid_get(bp, &devid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to determine device for ULP init.\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_cntxt_dev_id_set(bp->ulp_ctx, devid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to set device for ULP init.\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_cntxt_app_id_set(bp->ulp_ctx, bp->app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to set app_id for ULP init.\n");
		goto error_deinit;
	}
	BNXT_DRV_DBG(DEBUG, "Ulp initialized with app id %d\n", bp->app_id);

	rc = ulp_tfc_dparms_init(bp, bp->ulp_ctx, devid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to init dparms for app(%x)/dev(%x)\n",
			    bp->app_id, devid);
		goto error_deinit;
	}

	rc = ulp_tfc_cntxt_app_caps_init(bp, bp->app_id, devid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to set caps for app(%x)/dev(%x)\n",
			    bp->app_id, devid);
		goto error_deinit;
	}

	if (BNXT_TESTPMD_EN(bp)) {
		ulp_data->ulp_flags &= ~BNXT_ULP_VF_REP_ENABLED;
		BNXT_DRV_DBG(ERR, "Enabled Testpmd forward mode\n");
	}

	return rc;

error_deinit:
	session->session_opened[BNXT_ULP_SESSION_TYPE_DEFAULT] = 1;
	(void)ulp_tfc_ctx_deinit(bp, session);
	return rc;
}

static int32_t
ulp_tfc_vfr_session_fid_add(struct bnxt_ulp_context *ulp_ctx, uint16_t rep_fid)
{
	uint16_t fid_cnt = 0, sid = 0;
	struct tfc *tfcp = NULL;
	int rc;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (!tfcp) {
		PMD_DRV_LOG_LINE(ERR, "Unable to get tfcp from ulp_ctx");
		return -EINVAL;
	}

	/* Get the session id */
	rc = bnxt_ulp_cntxt_sid_get(ulp_ctx, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Unable to get SID for VFR FID=%d", rep_fid);
		return rc;
	}

	rc = tfc_session_fid_add(tfcp, rep_fid, sid, &fid_cnt);
	if (!rc)
		PMD_DRV_LOG_LINE(DEBUG,
				 "EFID=%d added to SID=%d, %d total",
				 rep_fid, sid, fid_cnt);
	else
		PMD_DRV_LOG_LINE(ERR,
				 "Failed to add EFID=%d to SID=%d",
				 rep_fid, sid);
	return rc;
}

static int32_t
ulp_tfc_vfr_session_fid_rem(struct bnxt_ulp_context *ulp_ctx, uint16_t rep_fid)
{
	uint16_t fid_cnt = 0, sid = 0;
	struct tfc *tfcp = NULL;
	int rc;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx);
	if (!tfcp) {
		PMD_DRV_LOG_LINE(ERR, "Unable tfcp from ulp_ctx");
		return -EINVAL;
	}

	/* Get the session id */
	rc = bnxt_ulp_cntxt_sid_get(ulp_ctx, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Unable to get SID for VFR FID=%d", rep_fid);
		return rc;
	}

	rc = tfc_session_fid_rem(tfcp, rep_fid, &fid_cnt);
	if (!rc)
		PMD_DRV_LOG_LINE(DEBUG,
				 "Removed EFID=%d from SID=%d, %d remain",
				 rep_fid, sid, fid_cnt);
	else
		PMD_DRV_LOG_LINE(ERR,
				 "Failed to remove EFID=%d from SID=%d",
				 rep_fid, sid);

	return rc;
}

static int32_t
ulp_tfc_ctx_attach(struct bnxt *bp,
		   struct bnxt_ulp_session_state *session)
{
	uint32_t flags, dev_id = BNXT_ULP_DEVICE_ID_LAST;
	uint16_t fid_cnt = 0;
	int32_t rc = 0;
	uint8_t app_id;

	bp->tfcp.bp = bp;
	rc = tfc_open(&bp->tfcp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize the tfc object\n");
		return rc;
	}

	rc = bnxt_ulp_cntxt_tfcp_set(bp->ulp_ctx, &bp->tfcp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add tfcp to ulp ctxt\n");
		return rc;
	}

	rc = bnxt_ulp_devid_get(bp, &dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	/* Increment the ulp context data reference count usage. */
	bp->ulp_ctx->cfg_data = session->cfg_data;
	bp->ulp_ctx->cfg_data->ref_cnt++;

	rc = tfc_session_fid_add(&bp->tfcp, bp->fw_fid,
				 session->session_id, &fid_cnt);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add RFID:%d to SID:%d.\n",
			     bp->fw_fid, session->session_id);
		return rc;
	}
	BNXT_DRV_DBG(DEBUG, "RFID:%d added to SID:%d\n",
		     bp->fw_fid, session->session_id);

	rc = bnxt_ulp_cntxt_sid_set(bp->ulp_ctx, session->session_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add fid to session.\n");
		return rc;
	}

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add the context list entry\n");
		return -EINVAL;
	}

	/*
	 * The supported flag will be set during the init. Use it now to
	 * know if we should go through the attach.
	 */
	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}
	flags = bp->ulp_ctx->cfg_data->ulp_flags;
	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(flags)) {
		BNXT_DRV_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			    app_id, dev_id);
		return -EINVAL;
	}

	rc = ulp_tfc_tbl_scope_init(bp);

	return rc;
}

static void
ulp_tfc_ctx_detach(struct bnxt *bp,
		   struct bnxt_ulp_session_state *session)
{
	uint16_t fid_cnt = 0;
	int32_t rc;

	ulp_tfc_tbl_scope_deinit(bp);

	rc = tfc_session_fid_rem(&bp->tfcp, bp->fw_fid, &fid_cnt);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed to remove RFID:%d from SID:%d\n",
			     bp->fw_fid, session->session_id);
	else
		BNXT_DRV_DBG(DEBUG, "Removed RFID:%d from SID:%d CNT:%d\n",
			     bp->fw_fid, session->session_id, fid_cnt);
	bnxt_ulp_cntxt_sid_reset(bp->ulp_ctx);
	(void)tfc_close(&bp->tfcp);
}

/*
 * When a port is deinit'ed by dpdk. This function is called
 * and this function clears the ULP context and rest of the
 * infrastructure associated with it.
 */
static void
ulp_tfc_deinit(struct bnxt *bp,
	       struct bnxt_ulp_session_state *session)
{
	bool ha_enabled;
	uint16_t fid_cnt = 0;
	int32_t rc;

	if (!bp->ulp_ctx || !bp->ulp_ctx->cfg_data)
		return;

	ha_enabled = bnxt_ulp_cntxt_ha_enabled(bp->ulp_ctx);
	if (ha_enabled) {
		rc = ulp_ha_mgr_close(bp->ulp_ctx);
		if (rc)
			BNXT_DRV_DBG(ERR, "Failed to close HA (%d)\n", rc);
	}

	/* Delete the Stats Counter Manager */
	ulp_sc_mgr_deinit(bp->ulp_ctx);

	/* cleanup the flow database */
	ulp_flow_db_deinit(bp->ulp_ctx);

	/* Delete the Mark database */
	ulp_mark_db_deinit(bp->ulp_ctx);

	/* cleanup the ulp mapper */
	ulp_mapper_deinit(bp->ulp_ctx);

	/* cleanup the ulp matcher */
	ulp_matcher_deinit(bp->ulp_ctx);

	/* Delete the Flow Counter Manager */
	ulp_fc_mgr_deinit(bp->ulp_ctx);

	/* Delete the Port database */
	ulp_port_db_deinit(bp->ulp_ctx);

	/* free the flow db lock */
	pthread_mutex_destroy(&bp->ulp_ctx->cfg_data->flow_db_lock);

	ulp_tfc_tbl_scope_deinit(bp);

	rc = tfc_session_fid_rem(&bp->tfcp, bp->fw_fid, &fid_cnt);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed to remove RFID:%d from SID:%d\n",
			     bp->fw_fid, session->session_id);
	else
		BNXT_DRV_DBG(DEBUG, "Removed RFID:%d from SID:%d CNT:%d\n",
			     bp->fw_fid, session->session_id, fid_cnt);
	bnxt_ulp_cntxt_sid_reset(bp->ulp_ctx);
	(void)tfc_close(&bp->tfcp);

	/* Delete the ulp context and tf session and free the ulp context */
	ulp_tfc_ctx_deinit(bp, session);

	BNXT_DRV_DBG(DEBUG, "ulp ctx has been deinitialized\n");
}

/*
 * When a port is initialized by dpdk. This functions is called
 * and this function initializes the ULP context and rest of the
 * infrastructure associated with it.
 */
static int32_t
ulp_tfc_init(struct bnxt *bp,
	     struct bnxt_ulp_session_state *session)
{
	uint32_t ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	uint16_t sid;
	int rc;

	/* Select 64bit SSE4.2 intrinsic if available */
	rte_hash_crc_set_alg(CRC32_SSE42_x64);

	rc = bnxt_ulp_devid_get(bp, &ulp_dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	bp->tfcp.bp = bp;
	rc = tfc_open(&bp->tfcp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize the tfc object\n");
		return rc;
	}

	rc = bnxt_ulp_cntxt_tfcp_set(bp->ulp_ctx, &bp->tfcp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add tfcp to ulp cntxt\n");
		return rc;
	}

	/* First time, so allocate a session and save it. */
	rc = tfc_session_id_alloc(&bp->tfcp, bp->fw_fid, &sid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to allocate a session id\n");
		return rc;
	}
	BNXT_DRV_DBG(DEBUG, "SID:%d allocated with RFID:%d\n", sid, bp->fw_fid);
	session->session_id = sid;
	rc = bnxt_ulp_cntxt_sid_set(bp->ulp_ctx, sid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to sid to ulp cntxt\n");
		return rc;
	}

	/* Allocate and Initialize the ulp context. */
	rc = ulp_tfc_ctx_init(bp, session);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the ulp context\n");
		goto jump_to_error;
	}

	rc = ulp_tfc_tbl_scope_init(bp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the ulp context\n");
		goto jump_to_error;
	}

	pthread_mutex_init(&bp->ulp_ctx->cfg_data->flow_db_lock, NULL);

	rc = ulp_tfc_dparms_init(bp, bp->ulp_ctx, ulp_dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize the dparms\n");
		goto jump_to_error;
	}

	/* create the port database */
	rc = ulp_port_db_init(bp->ulp_ctx, bp->port_cnt);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the port database\n");
		goto jump_to_error;
	}

	/* BAUCOM TODO: Mark database assumes LFID/GFID Parms, need to look at
	 * alternatives.
	 */
	/* Create the Mark database. */
	rc = ulp_mark_db_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the mark database\n");
		goto jump_to_error;
	}

	/* Create the flow database. */
	rc = ulp_flow_db_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the flow database\n");
		goto jump_to_error;
	}

	rc = ulp_matcher_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize ulp matcher\n");
		goto jump_to_error;
	}

	rc = ulp_mapper_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize ulp mapper\n");
		goto jump_to_error;
	}

	/* BAUCOM TODO: need to make FC Mgr not start the thread. */
	rc = ulp_fc_mgr_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize ulp flow counter mgr\n");
		goto jump_to_error;
	}

	rc = ulp_sc_mgr_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize ulp stats cache mgr\n");
		goto jump_to_error;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	if (ulp_dev_id == BNXT_ULP_DEVICE_ID_THOR2) {
		rc = bnxt_flow_mtr_init(bp);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Failed to config meter\n");
			goto jump_to_error;
		}
	}

#ifdef TF_FLOW_SCALE_QUERY
	/* Query resource statstics from firmware */
	tfc_resc_usage_query_all(bp);
#endif /* TF_FLOW_SCALE_QUERY */

	BNXT_DRV_DBG(DEBUG, "ulp ctx has been initialized\n");
	return rc;

jump_to_error:
	bp->ulp_ctx->ops->ulp_deinit(bp, session);
	return rc;
}

/**
 * Get meter capabilities.
 */
#define MAX_FLOW_PER_METER 1024
#define MAX_NUM_METER 1024
#define MAX_METER_RATE_200GBPS ((1ULL << 31) * 100 / 8)
static int
ulp_tfc_mtr_cap_get(struct bnxt *bp __rte_unused,
		    struct rte_mtr_capabilities *cap)
{
	cap->srtcm_rfc2697_byte_mode_supported = 1;
	cap->n_max = MAX_NUM_METER;
	cap->n_shared_max = cap->n_max;
	/* No meter is identical */
	cap->identical = 1;
	cap->shared_identical = 1;
	cap->shared_n_flows_per_mtr_max = MAX_FLOW_PER_METER;
	cap->chaining_n_mtrs_per_flow_max = 1; /* Chaining is not supported. */
	cap->meter_srtcm_rfc2697_n_max = cap->n_max;
	cap->meter_rate_max = MAX_METER_RATE_200GBPS;
	/* No stats supported now */
	cap->stats_mask = 0;

	return 0;
}

const struct bnxt_ulp_core_ops bnxt_ulp_tfc_core_ops = {
	.ulp_ctx_attach = ulp_tfc_ctx_attach,
	.ulp_ctx_detach = ulp_tfc_ctx_detach,
	.ulp_deinit =  ulp_tfc_deinit,
	.ulp_init =  ulp_tfc_init,
	.ulp_vfr_session_fid_add = ulp_tfc_vfr_session_fid_add,
	.ulp_vfr_session_fid_rem = ulp_tfc_vfr_session_fid_rem,
	.ulp_mtr_cap_get = ulp_tfc_mtr_cap_get
};
