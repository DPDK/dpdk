/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <stdio.h>

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc_vf2pf_msg.h"
#include "tfc_util.h"

/* Logging defines */
#define TFC_VF2PF_MSG_DEBUG  0

int
tfc_vf2pf_mem_alloc(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *req,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *resp)
{
	struct bnxt *bp;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (req == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid req pointer");
		return -EINVAL;
	}

	if (resp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid resp pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;
	return  bnxt_hwrm_tf_oem_cmd(bp, (uint32_t *)req, sizeof(*req),
				  (uint32_t *)resp, sizeof(*resp));
}
int
tfc_vf2pf_mem_free(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_mem_free_cmd *req,
		    struct tfc_vf2pf_tbl_scope_mem_free_resp *resp)
{
	struct bnxt *bp;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (req == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid req pointer");
		return -EINVAL;
	}

	if (resp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid resp pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;
	return  bnxt_hwrm_tf_oem_cmd(bp, (uint32_t *)req, sizeof(*req),
				  (uint32_t *)resp, sizeof(*resp));
}

int
tfc_vf2pf_pool_alloc(struct tfc *tfcp,
		     struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *req,
		     struct tfc_vf2pf_tbl_scope_pool_alloc_resp *resp)
{
	struct bnxt *bp;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (req == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid req pointer");
		return -EINVAL;
	}

	if (resp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid resp pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;
	return  bnxt_hwrm_tf_oem_cmd(bp, (uint32_t *)req, sizeof(*req),
				  (uint32_t *)resp, sizeof(*resp));
}

int
tfc_vf2pf_pool_free(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_pool_free_cmd *req,
		    struct tfc_vf2pf_tbl_scope_pool_free_resp *resp)
{
	struct bnxt *bp;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (req == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid req pointer");
		return -EINVAL;
	}

	if (resp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid resp pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;
	return  bnxt_hwrm_tf_oem_cmd(bp, (uint32_t *)req, sizeof(*req),
				  (uint32_t *)resp, sizeof(*resp));
}

static int
tfc_vf2pf_mem_alloc_process(struct tfc *tfcp,
			    uint32_t *oem_data,
			    uint32_t *resp_data,
			    uint16_t *resp_len)
{
	int dir;
	int rc = 0;
	struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *req =
		(struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *)oem_data;
	struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *resp =
		(struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *)resp_data;
	uint16_t data_len = sizeof(*resp);
	struct tfc_tbl_scope_mem_alloc_parms ma_parms;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (*resp_len < data_len) {
		PMD_DRV_LOG_LINE(ERR, "resp_data buffer is too small");
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	PMD_DRV_LOG_LINE(ERR, "Table scope mem alloc cfg cmd:");
	PMD_DRV_LOG_LINE(ERR, "\ttsid: 0x%x, max_pools: 0x%x", req->tsid, req->max_pools);
	for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
		PMD_DRV_LOG_LINE(ERR, "\tsbuckt_cnt_exp: 0x%x, dbucket_cnt: 0x%x",
				 req->static_bucket_cnt_exp[dir],
				 req->dynamic_bucket_cnt[dir]);
		PMD_DRV_LOG_LINE(ERR, "\tlkup_rec_cnt: 0x%x, lkup_pool_sz_exp: 0x%x",
				 req->lkup_rec_cnt[dir],
				 req->lkup_pool_sz_exp[dir]);
		PMD_DRV_LOG_LINE(ERR, "\tact_pool_sz_exp: 0x%x, lkup_rec_start_offset: 0x%x",
				 req->act_pool_sz_exp[dir],
				 req->lkup_rec_start_offset[dir]);
	}

	memset(&ma_parms, 0, sizeof(struct tfc_tbl_scope_mem_alloc_parms));

	for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
		ma_parms.static_bucket_cnt_exp[dir] = req->static_bucket_cnt_exp[dir];
		ma_parms.dynamic_bucket_cnt[dir] = req->dynamic_bucket_cnt[dir];
		ma_parms.lkup_rec_cnt[dir] = req->lkup_rec_cnt[dir];
		ma_parms.act_rec_cnt[dir] = req->act_rec_cnt[dir];
		ma_parms.act_pool_sz_exp[dir] = req->act_pool_sz_exp[dir];
		ma_parms.lkup_pool_sz_exp[dir] = req->lkup_pool_sz_exp[dir];
		ma_parms.lkup_rec_start_offset[dir] = req->lkup_rec_start_offset[dir];
	}
	/* Obtain from driver page definition (4k for DPDK) */
	ma_parms.pbl_page_sz_in_bytes = BNXT_PAGE_SIZE;
	/* First is meaningless on the PF, set to 0 */
	ma_parms.first = 0;

	/* This is not for local use if we are getting a message from the VF */
	ma_parms.local = false;
	ma_parms.max_pools = req->max_pools;
	ma_parms.scope_type = req->scope_type;
	rc = tfc_tbl_scope_mem_alloc(tfcp, req->hdr.fid, req->tsid, &ma_parms);
	if (rc == 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF allocation succeeds",
				 req->tsid);
	} else {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF allocation fails (%s)",
				 req->tsid, strerror(-rc));
	}
	*resp_len = rte_cpu_to_le_16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD;
	resp->tsid = req->tsid;
	resp->status = rc;
	return rc;
}


static int
tfc_vf2pf_mem_free_process(struct tfc *tfcp,
			   uint32_t *oem_data,
			   uint32_t *resp_data,
			   uint16_t *resp_len)
{
	int rc = 0;
	struct tfc_vf2pf_tbl_scope_mem_free_cmd *req =
		(struct tfc_vf2pf_tbl_scope_mem_free_cmd *)oem_data;
	struct tfc_vf2pf_tbl_scope_mem_free_resp *resp =
		(struct tfc_vf2pf_tbl_scope_mem_free_resp *)resp_data;
	uint16_t data_len = sizeof(*resp);

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (*resp_len < data_len) {
		PMD_DRV_LOG_LINE(ERR, "resp_data buffer is too small");
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	PMD_DRV_LOG_LINE(ERR, "Table scope mem free cfg cmd:");
	PMD_DRV_LOG_LINE(ERR, "\ttsid: 0x%x", req->tsid);

	rc = tfc_tbl_scope_mem_free(tfcp, req->hdr.fid, req->tsid, 0);
	if (rc == 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF free succeeds", req->tsid);
	} else {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF free fails (%s)",
				 req->tsid, strerror(-rc));
	}
	*resp_len = rte_cpu_to_le_16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD;
	resp->tsid = req->tsid;
	resp->status = rc;
	return rc;
}

static int
tfc_vf2pf_pool_alloc_process(struct tfc *tfcp,
			     uint32_t *oem_data,
			     uint32_t *resp_data,
			     uint16_t *resp_len)
{
	int rc = 0;
	struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *req =
		(struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *)oem_data;
	struct tfc_vf2pf_tbl_scope_pool_alloc_resp *resp =
		(struct tfc_vf2pf_tbl_scope_pool_alloc_resp *)resp_data;
	uint16_t data_len = sizeof(*resp);
	uint8_t pool_sz_exp = 0;
	uint16_t pool_id = 0;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (*resp_len < data_len) {
		PMD_DRV_LOG_LINE(ERR, "resp_data buffer is too small");
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	PMD_DRV_LOG_LINE(ERR, "Table scope pool alloc cmd:");
	PMD_DRV_LOG_LINE(ERR, "\ttsid: 0x%x, region:%s fid(%d)", req->tsid,
			 tfc_ts_region_2_str(req->region, req->dir), req->hdr.fid);

	rc = tfc_tbl_scope_pool_alloc(tfcp, req->hdr.fid, req->tsid, req->region,
				      req->dir, &pool_sz_exp, &pool_id);
	if (rc == 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF pool_alloc(%d) succeeds",
				 req->tsid, pool_id);
	} else {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF pool_alloc fails (%s)",
				 req->tsid, strerror(-rc));
	}
	*resp_len = rte_cpu_to_le_16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD;
	resp->tsid = req->tsid;
	resp->pool_sz_exp = pool_sz_exp;
	resp->pool_id = pool_id;
	resp->status = rc;
	return rc;
}


static int
tfc_vf2pf_pool_free_process(struct tfc *tfcp,
			   uint32_t *oem_data,
			   uint32_t *resp_data,
			   uint16_t *resp_len)
{
	int rc = 0;
	struct tfc_vf2pf_tbl_scope_pool_free_cmd *req =
		(struct tfc_vf2pf_tbl_scope_pool_free_cmd *)oem_data;
	struct tfc_vf2pf_tbl_scope_pool_free_resp *resp =
		(struct tfc_vf2pf_tbl_scope_pool_free_resp *)resp_data;
	uint16_t data_len = sizeof(*resp);

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (*resp_len < data_len) {
		PMD_DRV_LOG_LINE(ERR, "resp_data buffer is too small");
		return -EINVAL;
	}

	/* This block of code is for testing purpose. Will be removed later */
	PMD_DRV_LOG_LINE(ERR, "Table scope pool free cfg cmd:");
	PMD_DRV_LOG_LINE(ERR, "\ttsid: 0x%x", req->tsid);

	rc = tfc_tbl_scope_pool_free(tfcp, req->hdr.fid, req->tsid, req->region,
				     req->dir, req->pool_id);
	if (rc == 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF free succeeds", req->tsid);
	} else {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) PF free fails (%s)",
				 req->tsid, strerror(-rc));
	}
	*resp_len = rte_cpu_to_le_16(data_len);
	resp->hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD;
	resp->tsid = req->tsid;
	resp->status = rc;
	return rc;
}

int
tfc_oem_cmd_process(struct tfc *tfcp, uint32_t *oem_data, uint32_t *resp, uint16_t *resp_len)
{
	struct tfc_vf2pf_hdr *oem = (struct tfc_vf2pf_hdr *)oem_data;
	int rc = 0;

	switch (oem->type) {
	case TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD:
		rc = tfc_vf2pf_mem_alloc_process(tfcp, oem_data, resp, resp_len);
		break;
	case TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD:
		rc = tfc_vf2pf_mem_free_process(tfcp, oem_data, resp, resp_len);
		break;

	case TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD:
		rc = tfc_vf2pf_pool_alloc_process(tfcp, oem_data, resp, resp_len);
		break;

	case TFC_VF2PF_TYPE_TBL_SCOPE_POOL_FREE_CMD:
		rc = tfc_vf2pf_pool_free_process(tfcp, oem_data, resp, resp_len);
		break;
	case TFC_VF2PF_TYPE_TBL_SCOPE_PFID_QUERY_CMD:
	default:
		rc = -EPERM;
		break;
	}

	return rc;
}
