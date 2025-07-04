/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "rte_malloc.h"
#include "tfc_msg.h"
#include "tf_msg_common.h"
#include "hsi_struct_def_dpdk.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfo.h"

/* Logging defines */
#define TFC_RM_MSG_DEBUG  0

#define CFA_INVALID_FID UINT16_MAX

/**
 * This is the MAX data we can transport across regular HWRM
 */
#define TFC_PCI_BUF_SIZE_MAX 80
/**
 * If data bigger than TFC_PCI_BUF_SIZE_MAX then use DMA method
 */
struct tfc_msg_dma_buf {
	void *va_addr;
	uint64_t pa_addr;
};

static int tfc_msg_set_fid(struct bnxt *bp, uint16_t req_fid, uint16_t *msg_fid)
{
	/*
	 * Set request FID to 0xffff in case the request FID is the same as the
	 * target FID (bp->fw_fid), or if this is a PF. If we're on a TVF, then
	 * set the FID to the requested FID.
	 *
	 * The firmware validates the FID and accepts/rejects the request based
	 * on these rules:
	 *
	 *   1. (request_fid == 0xffff), final_fid = target_fid, accept
	 *   2. IS_PF(request_fid):
	 *      reject, Only (1) above is allowed
	 *   3. IS_PF(target_fid) && IS_VF(request_fid):
	 *      if(target_fid == parent_of(request_fid)) accept, else reject
	 *   4. IS_VF(target_fid) && IS_VF(request_fid):
	 *      if(parent_of(target_fid) == parent_of(request_fid)) accept, else reject
	 *
	 *   Note: for cases 2..4, final_fid = request_fid
	 */
	if (bp->fw_fid == req_fid || BNXT_PF(bp))
		*msg_fid = CFA_INVALID_FID;
	else if (BNXT_VF_IS_TRUSTED(bp))
		*msg_fid = rte_cpu_to_le_16(req_fid);
	else
		return -EINVAL;
	return 0;
}

/**
 * Allocates a DMA buffer that can be used for message transfer.
 *
 * [in] buf
 *   Pointer to DMA buffer structure
 *
 * [in] size
 *   Requested size of the buffer in bytes
 *
 * Returns:
 *    0	     - Success
 *   -ENOMEM - Unable to allocate buffer, no memory
 */
static int
tfc_msg_alloc_dma_buf(struct tfc_msg_dma_buf *buf, int size)
{
	/* Allocate session */
	buf->va_addr = rte_zmalloc("tfc_msg_dma_buf", size, 4096);
	if (buf->va_addr == NULL)
		return -ENOMEM;

	buf->pa_addr = rte_mem_virt2iova((void *)(uintptr_t)buf->va_addr);
	if (buf->pa_addr == RTE_BAD_IOVA) {
		rte_free(buf->va_addr);
		return -ENOMEM;
	}

	return 0;
}

/**
 * Free's a previous allocated DMA buffer.
 *
 * [in] buf
 *   Pointer to DMA buffer structure
 */
static void
tfc_msg_free_dma_buf(struct tfc_msg_dma_buf *buf)
{
	rte_free(buf->va_addr);
}

/* HWRM Direct messages */

int
tfc_msg_tbl_scope_qcaps(struct tfc *tfcp,
			bool *tbl_scope_capable,
			uint32_t *max_lkup_rec_cnt,
			uint32_t *max_act_rec_cnt,
			uint8_t	*max_lkup_static_buckets_exp)
{
	struct hwrm_tfc_tbl_scope_qcaps_input req = { 0 };
	struct hwrm_tfc_tbl_scope_qcaps_output resp = { 0 };
	struct bnxt *bp;
	int rc;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tbl_scope_capable == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tbl_scope_capable pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;
	*tbl_scope_capable = false;

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TBL_SCOPE_QCAPS,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc)
		return rc;

	if (resp.tbl_scope_capable) {
		*tbl_scope_capable = true;
		if (max_lkup_rec_cnt)
			*max_lkup_rec_cnt =
				rte_le_to_cpu_32(resp.max_lkup_rec_cnt);
		if (max_act_rec_cnt)
			*max_act_rec_cnt =
				rte_le_to_cpu_32(resp.max_act_rec_cnt);
		if (max_lkup_static_buckets_exp)
			*max_lkup_static_buckets_exp =
				resp.max_lkup_static_buckets_exp;
	}

	return rc;
}
int
tfc_msg_tbl_scope_id_alloc(struct tfc *tfcp, uint16_t fid,
			   bool shared, enum cfa_app_type app_type,
			   uint8_t *tsid,
			   bool *first)
{
	struct hwrm_tfc_tbl_scope_id_alloc_input req = { 0 };
	struct hwrm_tfc_tbl_scope_id_alloc_output resp = { 0 };
	struct bnxt *bp;
	int rc;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tsid == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;
	req.app_type = app_type;
	req.shared = shared;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TBL_SCOPE_ID_ALLOC,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0) {
		*tsid = resp.tsid;
		if (first) {
			if (resp.first)
				*first = true;
			else
				*first = false;
		}
	}

	return rc;
}

#define RE_LKUP 0
#define RE_ACT  1
#define TE_LKUP 2
#define TE_ACT  3
/**
 *  Given the direction and the region return the backing store cfg instance
 */
static int tfc_tbl_scope_region_dir_to_inst(enum cfa_region_type region,
					    enum cfa_dir dir,
					    uint16_t *instance)
{
	if (!instance) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	switch (region) {
	case CFA_REGION_TYPE_LKUP:
		if (dir == CFA_DIR_RX)
			*instance = RE_LKUP;
		else
			*instance = TE_LKUP;
		break;
	case CFA_REGION_TYPE_ACT:
		if (dir == CFA_DIR_RX)
			*instance = RE_ACT;
		else
			*instance = TE_ACT;
		break;
	default:
		PMD_DRV_LOG_LINE(ERR, "Invalid region");
		return -EINVAL;
	}
	return 0;
}

/**
 * Given the page_sz_bytes and pbl_level, encode the pg_sz_pbl_level
 */
static int tfc_tbl_scope_pg_sz_pbl_level_encode(uint32_t page_sz_in_bytes,
						uint8_t pbl_level,
						uint8_t *page_sz_pbl_level)
{
	uint8_t page_sz;
	int rc = 0;

	switch (page_sz_in_bytes) {
	case 0x1000: /* 4K */
		page_sz = HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_PAGE_SIZE_PG_4K;
		break;
	case 0x2000: /* 8K */
		page_sz = HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_PAGE_SIZE_PG_8K;
		break;
	case 0x10000: /* 64K */
		page_sz = HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_PAGE_SIZE_PG_64K;
		break;
	case 0x200000: /* 2M */
		page_sz = HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_PAGE_SIZE_PG_2M;
		break;
	case 0x40000000: /* 1G */
		page_sz = HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_PAGE_SIZE_PG_1G;
		break;
	default:
		PMD_DRV_LOG_LINE(ERR, "Unsupported page size (0x%x)",
				 page_sz_in_bytes);
		return -EINVAL;
	}
	/* Page size value is already shifted */
	*page_sz_pbl_level = page_sz;
	if (pbl_level > 2) {
		PMD_DRV_LOG_LINE(ERR, "Invalid pbl_level(%d)", pbl_level);
		return -EINVAL;
	}
	*page_sz_pbl_level |= pbl_level;
	return rc;
}


int
tfc_msg_backing_store_cfg_v2(struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir,
			     enum cfa_region_type region, uint64_t base_addr,
			     uint8_t pbl_level, uint32_t pbl_page_sz_in_bytes,
			     uint32_t rec_cnt, uint8_t static_bkt_cnt_exp,
			     bool cfg_done)
{
	struct hwrm_func_backing_store_cfg_v2_input req = { 0 };
	struct hwrm_func_backing_store_cfg_v2_input resp = { 0 };
	struct bnxt *bp;
	int rc;
	struct ts_split_entries *ts_sp;

	ts_sp = (struct ts_split_entries *)&req.split_entry_0;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	bp = tfcp->bp;
	ts_sp->tsid = tsid;
	ts_sp->lkup_static_bkt_cnt_exp[dir] = static_bkt_cnt_exp;
	ts_sp->region_num_entries = rec_cnt;
	if (cfg_done)
		req.flags |= HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_FLAGS_BS_CFG_ALL_DONE;

	rc = tfc_tbl_scope_region_dir_to_inst(region, dir, &req.instance);
	if (rc)
		return rc;

	req.page_dir = rte_cpu_to_le_64(base_addr);
	req.num_entries = rte_cpu_to_le_32(rec_cnt);

	req.type = HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_TBL_SCOPE;

	rc = tfc_tbl_scope_pg_sz_pbl_level_encode(pbl_page_sz_in_bytes,
						  pbl_level,
						  &req.page_size_pbl_level);
	if (rc)
		return rc;

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_FUNC_BACKING_STORE_CFG_V2,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	return rc;
}

int
tfc_msg_tbl_scope_deconfig(struct tfc *tfcp, uint8_t tsid)
{
	struct hwrm_tfc_tbl_scope_deconfig_input req = { 0 };
	struct hwrm_tfc_tbl_scope_deconfig_output resp = { 0 };
	struct bnxt *bp;
	int rc;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;
	req.tsid = tsid;
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TBL_SCOPE_DECONFIG,
					 &req, sizeof(req), &resp,
					 sizeof(resp));

	return rc;
}

int
tfc_msg_tbl_scope_fid_add(struct tfc *tfcp, uint16_t fid,
			  uint8_t tsid, uint16_t *fid_cnt)
{
	struct hwrm_tfc_tbl_scope_fid_add_input req = { 0 };
	struct hwrm_tfc_tbl_scope_fid_add_output resp = { 0 };
	struct bnxt *bp;
	int rc;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.tsid = tsid;
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TBL_SCOPE_FID_ADD,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0)
		if (fid_cnt)
			*fid_cnt = rte_le_to_cpu_16(resp.fid_cnt);

	return rc;
}

int
tfc_msg_tbl_scope_fid_rem(struct tfc *tfcp, uint16_t fid,
			  uint8_t tsid, uint16_t *fid_cnt)
{
	struct hwrm_tfc_tbl_scope_fid_rem_input req = { 0 };
	struct hwrm_tfc_tbl_scope_fid_rem_output resp = { 0 };
	struct bnxt *bp;
	int rc;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	bp = tfcp->bp;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.tsid = tsid;
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TBL_SCOPE_FID_REM,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0)
		if (fid_cnt)
			*fid_cnt = rte_le_to_cpu_16(resp.fid_cnt);

	return rc;
}

int
tfc_msg_idx_tbl_alloc(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		      enum cfa_track_type tt, enum cfa_dir dir,
		      enum cfa_resource_subtype_idx_tbl subtype,
		      uint16_t *id)

{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_idx_tbl_alloc_input req = { 0 };
	struct hwrm_tfc_idx_tbl_alloc_output resp = { 0 };

	if (dir == CFA_DIR_RX)
		req.flags |= HWRM_TFC_IDX_TBL_ALLOC_INPUT_FLAGS_DIR_RX &
			     HWRM_TFC_IDX_TBL_ALLOC_INPUT_FLAGS_DIR;
	else
		req.flags |= HWRM_TFC_IDX_TBL_ALLOC_INPUT_FLAGS_DIR_TX &
			     HWRM_TFC_IDX_TBL_ALLOC_INPUT_FLAGS_DIR;

	if (tt == CFA_TRACK_TYPE_FID)
		req.track_type = HWRM_TFC_IDX_TBL_ALLOC_INPUT_TRACK_TYPE_TRACK_TYPE_FID;
	else
		req.track_type = HWRM_TFC_IDX_TBL_ALLOC_INPUT_TRACK_TYPE_TRACK_TYPE_SID;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.subtype = rte_le_to_cpu_16(subtype);

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IDX_TBL_ALLOC,
					 &req, sizeof(req), &resp, sizeof(resp));

	if (rc == 0)
		*id = rte_cpu_to_le_16(resp.idx_tbl_id);

	return rc;
}

int
tfc_msg_idx_tbl_alloc_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
			  enum cfa_track_type tt, enum cfa_dir dir,
			  enum cfa_resource_subtype_idx_tbl subtype,
			  const uint32_t *dev_data, uint8_t data_size,
			  uint16_t *id)

{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_idx_tbl_alloc_set_input req = { 0 };
	struct hwrm_tfc_idx_tbl_alloc_set_output resp = { 0 };
	struct tfc_msg_dma_buf buf = { 0 };
	uint8_t *data = NULL;

	if (dir == CFA_DIR_RX)
		req.flags |= HWRM_TFC_IDX_TBL_ALLOC_SET_INPUT_FLAGS_DIR_RX &
			     HWRM_TFC_IDX_TBL_ALLOC_SET_INPUT_FLAGS_DIR;
	else
		req.flags |= HWRM_TFC_IDX_TBL_ALLOC_SET_INPUT_FLAGS_DIR_TX &
			     HWRM_TFC_IDX_TBL_ALLOC_SET_INPUT_FLAGS_DIR;

	if (tt == CFA_TRACK_TYPE_FID)
		req.track_type = HWRM_TFC_IDX_TBL_ALLOC_SET_INPUT_TRACK_TYPE_TRACK_TYPE_FID;
	else
		req.track_type = HWRM_TFC_IDX_TBL_ALLOC_SET_INPUT_TRACK_TYPE_TRACK_TYPE_SID;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.subtype = rte_le_to_cpu_16(subtype);
	req.data_size = rte_le_to_cpu_16(data_size);

	if (req.data_size >= sizeof(req.dev_data)) {
		req.flags |= HWRM_TFC_IDX_TBL_ALLOC_SET_INPUT_FLAGS_DMA;
		rc = tfc_msg_alloc_dma_buf(&buf, data_size);
		if (rc)
			goto cleanup;
		data = buf.va_addr;
		memcpy(&req.dma_addr, &buf.pa_addr, sizeof(buf.pa_addr));
	} else {
		data = &req.dev_data[0];
	}

	memcpy(&data[0], &dev_data[0], req.data_size);

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IDX_TBL_ALLOC_SET,
					 &req, sizeof(req), &resp, sizeof(resp));

	if (rc == 0)
		*id = rte_cpu_to_le_16(resp.idx_tbl_id);

cleanup:
	tfc_msg_free_dma_buf(&buf);

	return rc;
}

int
tfc_msg_idx_tbl_set(struct tfc *tfcp, uint16_t fid,
		    uint16_t sid, enum cfa_dir dir,
		    enum cfa_resource_subtype_idx_tbl subtype, uint16_t id,
		    const uint32_t *dev_data, uint8_t data_size)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_idx_tbl_set_input req = { 0 };
	struct hwrm_tfc_idx_tbl_set_output resp = { 0 };
	struct tfc_msg_dma_buf buf = { 0 };
	uint8_t *data = NULL;

	if (dir == CFA_DIR_RX)
		req.flags |= HWRM_TFC_IDX_TBL_SET_INPUT_FLAGS_DIR_RX &
			     HWRM_TFC_IDX_TBL_SET_INPUT_FLAGS_DIR;
	else
		req.flags |= HWRM_TFC_IDX_TBL_SET_INPUT_FLAGS_DIR_TX &
			     HWRM_TFC_IDX_TBL_SET_INPUT_FLAGS_DIR;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.idx_tbl_id = rte_le_to_cpu_16(id);
	req.subtype = rte_le_to_cpu_16(subtype);
	req.data_size = rte_le_to_cpu_16(data_size);
	rc = tfc_msg_alloc_dma_buf(&buf, data_size);
	if (rc)
		goto cleanup;

	if (req.data_size >= sizeof(req.dev_data)) {
		req.flags |= HWRM_TFC_IDX_TBL_SET_INPUT_FLAGS_DMA;
		rc = tfc_msg_alloc_dma_buf(&buf, data_size);
		if (rc)
			goto cleanup;
		data = buf.va_addr;
		memcpy(&req.dma_addr, &buf.pa_addr, sizeof(buf.pa_addr));
	} else {
		data = &req.dev_data[0];
	}

	memcpy(&data[0], &dev_data[0], req.data_size);
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IDX_TBL_SET,
					 &req, sizeof(req), &resp, sizeof(resp));
cleanup:
	tfc_msg_free_dma_buf(&buf);

	return rc;
}

int
tfc_msg_idx_tbl_get(struct tfc *tfcp, uint16_t fid,
		    uint16_t sid, enum cfa_dir dir,
		    enum cfa_resource_subtype_idx_tbl subtype, uint16_t id,
		    uint32_t *dev_data, uint8_t *data_size)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_idx_tbl_get_input req = { 0 };
	struct hwrm_tfc_idx_tbl_get_output resp = { 0 };
	struct tfc_msg_dma_buf buf = { 0 };

	if (dir == CFA_DIR_RX)
		req.flags |= HWRM_TFC_IDX_TBL_GET_INPUT_FLAGS_DIR_RX &
			     HWRM_TFC_IDX_TBL_GET_INPUT_FLAGS_DIR;
	else
		req.flags |= HWRM_TFC_IDX_TBL_GET_INPUT_FLAGS_DIR_TX &
			     HWRM_TFC_IDX_TBL_GET_INPUT_FLAGS_DIR;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_cpu_to_le_16(sid);
	req.idx_tbl_id = rte_cpu_to_le_16(id);
	req.subtype = rte_cpu_to_le_16(subtype);
	req.buffer_size = rte_cpu_to_le_16(*data_size);

	rc = tfc_msg_alloc_dma_buf(&buf, *data_size);
	if (rc)
		goto cleanup;

	memcpy(&req.dma_addr, &buf.pa_addr, sizeof(buf.pa_addr));
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IDX_TBL_GET,
					 &req, sizeof(req), &resp, sizeof(resp));

	if (rc == 0) {
		memcpy(dev_data, buf.va_addr, resp.data_size);
		*data_size = rte_le_to_cpu_16(resp.data_size);
	}

cleanup:
	tfc_msg_free_dma_buf(&buf);

	return rc;
}

int
tfc_msg_idx_tbl_free(struct tfc *tfcp, uint16_t fid,
		     uint16_t sid, enum cfa_dir dir,
		     enum cfa_resource_subtype_idx_tbl subtype, uint16_t id)
{
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_idx_tbl_free_input req = { 0 };
	struct hwrm_tfc_idx_tbl_free_output resp = { 0 };
	int rc = 0;

	if (dir == CFA_DIR_RX)
		req.flags |= HWRM_TFC_IDX_TBL_FREE_INPUT_FLAGS_DIR_RX &
			     HWRM_TFC_IDX_TBL_FREE_INPUT_FLAGS_DIR;
	else
		req.flags |= HWRM_TFC_IDX_TBL_FREE_INPUT_FLAGS_DIR_TX &
			     HWRM_TFC_IDX_TBL_FREE_INPUT_FLAGS_DIR;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_cpu_to_le_16(sid);
	req.idx_tbl_id = rte_cpu_to_le_16(id);
	req.subtype = rte_cpu_to_le_16(subtype);

	return bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IDX_TBL_FREE,
					   &req, sizeof(req), &resp, sizeof(resp));
}

int tfc_msg_global_id_alloc(struct tfc *tfcp, uint16_t fid, uint16_t sid,
			    enum tfc_domain_id domain_id, uint16_t req_cnt,
			    const struct tfc_global_id_req *glb_id_req,
			    struct tfc_global_id *rsp, uint16_t *rsp_cnt,
			    bool *first)
{
	int rc = 0;
	int i = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_global_id_alloc_input hwrm_req;
	struct hwrm_tfc_global_id_alloc_output hwrm_resp;
	struct tfc_global_id_hwrm_req *req_data;
	struct tfc_global_id_hwrm_rsp *rsp_data;
	struct tfc_msg_dma_buf req_buf = { 0 };
	struct tfc_msg_dma_buf rsp_buf = { 0 };
	int dma_size;
	int resp_cnt = 0;

	/* Prepare DMA buffers */
	dma_size = req_cnt * sizeof(struct tfc_global_id_req);
	rc = tfc_msg_alloc_dma_buf(&req_buf, dma_size);
	if (rc)
		return rc;

	for (i = 0; i < req_cnt; i++)
		resp_cnt += glb_id_req->cnt;
	dma_size = resp_cnt * sizeof(struct tfc_global_id);
	*rsp_cnt = resp_cnt;
	rc = tfc_msg_alloc_dma_buf(&rsp_buf, dma_size);
	if (rc) {
		tfc_msg_free_dma_buf(&req_buf);
		return rc;
	}

	/* Populate the request */
	rc = tfc_msg_set_fid(bp, fid, &hwrm_req.fid);
	if (rc)
		goto cleanup;

	hwrm_req.sid = rte_cpu_to_le_16(sid);
	hwrm_req.global_id = rte_cpu_to_le_16(domain_id);
	hwrm_req.req_cnt = req_cnt;
	hwrm_req.req_addr = rte_cpu_to_le_64(req_buf.pa_addr);
	hwrm_req.resc_addr = rte_cpu_to_le_64(rsp_buf.pa_addr);
	req_data = (struct tfc_global_id_hwrm_req *)req_buf.va_addr;
	for (i = 0; i < req_cnt; i++) {
		req_data[i].rtype = rte_cpu_to_le_16(glb_id_req[i].rtype);
		req_data[i].dir = rte_cpu_to_le_16(glb_id_req[i].dir);
		req_data[i].subtype = rte_cpu_to_le_16(glb_id_req[i].rsubtype);
		req_data[i].cnt = rte_cpu_to_le_16(glb_id_req[i].cnt);
	}

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_GLOBAL_ID_ALLOC,
					 &hwrm_req, sizeof(hwrm_req), &hwrm_resp,
					 sizeof(hwrm_resp));
	if (rc == 0) {
		if (first) {
			if (hwrm_resp.first)
				*first = true;
			else
				*first = false;
		}
	}

	/* Process the response
	 * Should always get expected number of entries
	 */
	if (rte_le_to_cpu_32(hwrm_resp.rsp_cnt) != *rsp_cnt) {
		PMD_DRV_LOG_LINE(ERR, "Alloc message size error, rc:%s",
				 strerror(-EINVAL));
		rc = -EINVAL;
		goto cleanup;
	}

	rsp_data = (struct tfc_global_id_hwrm_rsp *)rsp_buf.va_addr;
	for (i = 0; i < *rsp_cnt; i++) {
		rsp[i].rtype = rte_le_to_cpu_32(rsp_data[i].rtype);
		rsp[i].dir = rte_le_to_cpu_32(rsp_data[i].dir);
		rsp[i].rsubtype = rte_le_to_cpu_32(rsp_data[i].subtype);
		rsp[i].id = rte_le_to_cpu_32(rsp_data[i].id);
	}

cleanup:
	tfc_msg_free_dma_buf(&req_buf);
	tfc_msg_free_dma_buf(&rsp_buf);
	return rc;
}

int
tfc_msg_tbl_scope_config_get(struct tfc *tfcp, uint8_t tsid, bool *configured)
{
	struct bnxt *bp;
	struct hwrm_tfc_tbl_scope_config_get_input req = { 0 };
	struct hwrm_tfc_tbl_scope_config_get_output resp = { 0 };
	int rc = 0;

	bp = tfcp->bp;
	req.tsid = tsid;
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TBL_SCOPE_CONFIG_GET,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0)
		*configured = rte_le_to_cpu_16(resp.configured) ? true : false;

	return rc;
}

int
tfc_msg_session_id_alloc(struct tfc *tfcp, uint16_t fid,
			 uint16_t *sid)
{
	struct bnxt *bp;
	struct hwrm_tfc_session_id_alloc_input req = { 0 };
	struct hwrm_tfc_session_id_alloc_output resp = { 0 };
	int rc = 0;

	/* TBD: Parameters are checked by caller, is this enough? */
	bp = tfcp->bp;
	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_SESSION_ID_ALLOC,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0)
		*sid = rte_le_to_cpu_16(resp.sid);

	return rc;
}

int
tfc_msg_session_fid_add(struct tfc *tfcp, uint16_t fid,
			uint16_t sid, uint16_t *fid_cnt)
{
	struct bnxt *bp;
	struct hwrm_tfc_session_fid_add_input req = { 0 };
	struct hwrm_tfc_session_fid_add_output resp = { 0 };
	int rc;

	/* TBD: Parameters are checked by caller, is this enough? */
	bp = tfcp->bp;
	req.sid = rte_cpu_to_le_16(sid);
	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_SESSION_FID_ADD,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0)
		if (fid_cnt)
			*fid_cnt = rte_le_to_cpu_16(resp.fid_cnt);

	return rc;
}

int
tfc_msg_session_fid_rem(struct tfc *tfcp, uint16_t fid,
			uint16_t sid, uint16_t *fid_cnt)
{
	struct bnxt *bp;
	struct hwrm_tfc_session_fid_rem_input req = { 0 };
	struct hwrm_tfc_session_fid_rem_output resp = { 0 };
	int rc;

	/* TBD: Parameters are checked by caller, is this enough? */
	bp = tfcp->bp;
	req.sid = rte_cpu_to_le_16(sid);
	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_SESSION_FID_REM,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0)
		if (fid_cnt)
			*fid_cnt = rte_le_to_cpu_16(resp.fid_cnt);
	return rc;
}

static int tfc_msg_set_tt(enum cfa_track_type tt, uint8_t *ptt)
{
	switch (tt) {
	case CFA_TRACK_TYPE_SID:
		*ptt = HWRM_TFC_IDENT_ALLOC_INPUT_TRACK_TYPE_TRACK_TYPE_SID;
		break;
	case CFA_TRACK_TYPE_FID:
		*ptt = HWRM_TFC_IDENT_ALLOC_INPUT_TRACK_TYPE_TRACK_TYPE_FID;
		break;
	default:
		PMD_DRV_LOG_LINE(ERR, "Invalid tt[%u]", tt);
		return -EINVAL;
	}

	return 0;
}

int tfc_msg_identifier_alloc(struct tfc *tfcp, enum cfa_dir dir,
			     enum cfa_resource_subtype_ident subtype,
			     enum cfa_track_type tt, uint16_t fid,
			     uint16_t sid, uint16_t *ident_id)
{
	struct bnxt *bp;
	struct hwrm_tfc_ident_alloc_input req = { 0 };
	struct hwrm_tfc_ident_alloc_output resp = { 0 };
	int rc;

	bp = tfcp->bp;
	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_IDENT_ALLOC_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_IDENT_ALLOC_INPUT_FLAGS_DIR_RX);

	rc = tfc_msg_set_tt(tt, &req.track_type);
	if (rc)
		return rc;

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.subtype = subtype;

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IDENT_ALLOC,
					 &req, sizeof(req), &resp,
					 sizeof(resp));
	if (rc == 0)
		*ident_id = rte_le_to_cpu_16(resp.ident_id);
	return rc;
}

int tfc_msg_identifier_free(struct tfc *tfcp, enum cfa_dir dir,
			    enum cfa_resource_subtype_ident subtype,
			    uint16_t fid, uint16_t sid,
			    uint16_t ident_id)
{
	struct bnxt *bp;
	struct hwrm_tfc_ident_free_input req = { 0 };
	struct hwrm_tfc_ident_free_output resp = { 0 };
	int rc;

	bp = tfcp->bp;
	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_IDENT_FREE_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_IDENT_FREE_INPUT_FLAGS_DIR_RX);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.subtype = subtype;
	req.ident_id = ident_id;

	return bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IDENT_FREE,
					  &req, sizeof(req), &resp,
					  sizeof(resp));
}

int
tfc_msg_tcam_alloc(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		   enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		   enum cfa_track_type tt, uint16_t pri, uint16_t key_sz_bytes,
		   uint16_t *tcam_id)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_tcam_alloc_input req = { 0 };
	struct hwrm_tfc_tcam_alloc_output resp = { 0 };

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_TCAM_ALLOC_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_TCAM_ALLOC_INPUT_FLAGS_DIR_RX);

	req.track_type = (tt == CFA_TRACK_TYPE_FID ?
			 HWRM_TFC_TCAM_ALLOC_INPUT_TRACK_TYPE_TRACK_TYPE_FID :
			 HWRM_TFC_TCAM_ALLOC_INPUT_TRACK_TYPE_TRACK_TYPE_SID);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.subtype = rte_le_to_cpu_16(subtype);
	req.priority = rte_le_to_cpu_16(pri);
	req.key_size = rte_le_to_cpu_16(key_sz_bytes);
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TCAM_ALLOC,
					 &req, sizeof(req), &resp, sizeof(resp));
	if (rc == 0)
		*tcam_id = resp.idx;

	return rc;
}

int
tfc_msg_tcam_alloc_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		       enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		       enum cfa_track_type tt, uint16_t *tcam_id, uint16_t pri,
		       const uint8_t *key, uint8_t key_size, const uint8_t *mask,
		       const uint8_t *remap, uint8_t remap_size)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_tcam_alloc_set_input req = { 0 };
	struct hwrm_tfc_tcam_alloc_set_output resp = { 0 };
	struct tfc_msg_dma_buf buf = { 0 };
	uint8_t *data = NULL;
	int data_size = 0;

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_TCAM_ALLOC_SET_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_TCAM_ALLOC_SET_INPUT_FLAGS_DIR_RX);

	req.track_type = (tt == CFA_TRACK_TYPE_FID ?
			 HWRM_TFC_TCAM_ALLOC_SET_INPUT_TRACK_TYPE_TRACK_TYPE_FID :
			 HWRM_TFC_TCAM_ALLOC_SET_INPUT_TRACK_TYPE_TRACK_TYPE_SID);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.subtype = rte_le_to_cpu_16(subtype);
	req.key_size = rte_le_to_cpu_16(key_size);
	req.priority = rte_le_to_cpu_16(pri);
	req.result_size = rte_le_to_cpu_16(remap_size);
	data_size = 2 * req.key_size + req.result_size;

	if (data_size > TFC_PCI_BUF_SIZE_MAX) {
		req.flags |= HWRM_TFC_TCAM_ALLOC_SET_INPUT_FLAGS_DMA;
		rc = tfc_msg_alloc_dma_buf(&buf, data_size);
		if (rc)
			goto cleanup;
		data = buf.va_addr;
		memcpy(&req.dma_addr, &buf.pa_addr, sizeof(buf.pa_addr));
	} else {
		data = &req.dev_data[0];
	}

	memcpy(&data[0], &key, key_size * sizeof(uint32_t));
	memcpy(&data[key_size], &mask, key_size * sizeof(uint32_t));
	memcpy(&data[key_size * 2], &remap, remap_size * sizeof(uint32_t));
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TCAM_ALLOC_SET,
					 &req, sizeof(req), &resp, sizeof(resp));

	if (rc == 0)
		*tcam_id = resp.tcam_id;

cleanup:
	tfc_msg_free_dma_buf(&buf);

	return rc;
}

int
tfc_msg_tcam_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		 enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		 uint16_t tcam_id, const uint8_t *key, uint8_t key_size,
		 const uint8_t *mask, const uint8_t *remap,
		 uint8_t remap_size)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_tcam_set_input req = { 0 };
	struct hwrm_tfc_tcam_set_output resp = { 0 };
	struct tfc_msg_dma_buf buf = { 0 };
	uint8_t *data = NULL;
	int data_size = 0;

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_TCAM_SET_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_TCAM_SET_INPUT_FLAGS_DIR_RX);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.tcam_id = rte_le_to_cpu_16(tcam_id);
	req.subtype = rte_le_to_cpu_16(subtype);
	req.key_size = rte_le_to_cpu_16(key_size);
	req.result_size = rte_le_to_cpu_16(remap_size);
	data_size = 2 * req.key_size + req.result_size;

	if (data_size > TFC_PCI_BUF_SIZE_MAX) {
		req.flags |= HWRM_TFC_TCAM_SET_INPUT_FLAGS_DMA;
		rc = tfc_msg_alloc_dma_buf(&buf, data_size);
		if (rc)
			goto cleanup;
		data = buf.va_addr;
		memcpy(&req.dma_addr, &buf.pa_addr, sizeof(buf.pa_addr));
	} else {
		data = &req.dev_data[0];
	}

	memcpy(&data[0], key, key_size);
	memcpy(&data[key_size], mask, key_size);
	memcpy(&data[key_size * 2], remap, remap_size);
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TCAM_SET,
					 &req, sizeof(req), &resp, sizeof(resp));
cleanup:
	tfc_msg_free_dma_buf(&buf);

	return rc;
}

int
tfc_msg_tcam_get(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		 enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		 uint16_t tcam_id, uint8_t *key, uint8_t *key_size,
		 uint8_t *mask, uint8_t *remap, uint8_t *remap_size)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_tcam_get_input req = { 0 };
	struct hwrm_tfc_tcam_get_output resp = { 0 };

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_TCAM_GET_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_TCAM_GET_INPUT_FLAGS_DIR_RX);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.tcam_id = rte_le_to_cpu_16(tcam_id);
	req.subtype = rte_le_to_cpu_16(subtype);
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TCAM_GET,
					 &req, sizeof(req), &resp, sizeof(resp));

	if (rc ||
	    *key_size < rte_le_to_cpu_16(resp.key_size) ||
	    *remap_size < rte_le_to_cpu_16(resp.result_size)) {
		PMD_DRV_LOG_LINE(ERR, "Key buffer is too small, rc:%s",
				 strerror(EINVAL));
		rc = -EINVAL;
	}
	*key_size = resp.key_size;
	*remap_size = resp.result_size;
	memcpy(key, &resp.dev_data[0], resp.key_size);
	memcpy(mask, &resp.dev_data[resp.key_size], resp.key_size);
	memcpy(remap, &resp.dev_data[resp.key_size * 2], resp.result_size);

	return rc;
}

int
tfc_msg_tcam_free(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		  enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		  uint16_t tcam_id)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_tcam_free_input req = { 0 };
	struct hwrm_tfc_tcam_free_output resp = { 0 };

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_TCAM_FREE_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_TCAM_FREE_INPUT_FLAGS_DIR_RX);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);
	req.tcam_id = rte_le_to_cpu_16(tcam_id);
	req.subtype = rte_le_to_cpu_16(subtype);
	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_TCAM_FREE,
					 &req, sizeof(req), &resp, sizeof(resp));

	return rc;
}

int
tfc_msg_if_tbl_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		   enum cfa_dir dir, enum cfa_resource_subtype_if_tbl subtype,
		   uint16_t index, uint8_t data_size, const uint8_t *data)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_if_tbl_set_input req = { 0 };
	struct hwrm_tfc_if_tbl_set_output resp = { 0 };

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_IF_TBL_SET_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_IF_TBL_SET_INPUT_FLAGS_DIR_RX);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);

	req.index = rte_le_to_cpu_16(index);
	req.subtype = rte_le_to_cpu_16(subtype);
	req.data_size = data_size;
	memcpy(req.data, data, data_size);

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IF_TBL_SET,
					 &req, sizeof(req), &resp, sizeof(resp));

	return rc;
}

int
tfc_msg_if_tbl_get(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		   enum cfa_dir dir, enum cfa_resource_subtype_if_tbl subtype,
		   uint16_t index, uint8_t *data_size, uint8_t *data)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_if_tbl_get_input req = { 0 };
	struct hwrm_tfc_if_tbl_get_output resp = { 0 };

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_IF_TBL_GET_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_IF_TBL_GET_INPUT_FLAGS_DIR_RX);

	rc = tfc_msg_set_fid(bp, fid, &req.fid);
	if (rc)
		return rc;
	req.sid = rte_le_to_cpu_16(sid);

	req.index = rte_le_to_cpu_16(index);
	req.subtype = rte_le_to_cpu_16(subtype);
	req.data_size = rte_le_to_cpu_16(*data_size);

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_IF_TBL_GET,
					 &req, sizeof(req), &resp, sizeof(resp));
	if (rc)
		return rc;

	if (*data_size < rte_le_to_cpu_16(resp.data_size)) {
		PMD_DRV_LOG_LINE(ERR, "Table buffer is too small, rc:%s",
				 strerror(EINVAL));
		rc = -EINVAL;
	}

	*data_size = resp.data_size;
	memcpy(data, resp.data, *data_size);

	return rc;
}

#ifdef TF_FLOW_SCALE_QUERY
int tfc_msg_resc_usage_query(struct tfc *tfcp, uint16_t sid, enum cfa_dir dir,
			     uint16_t *data_size, void *data)
{
	int rc = 0;
	struct bnxt *bp = tfcp->bp;
	struct hwrm_tfc_resc_usage_query_input req = { 0 };
	struct hwrm_tfc_resc_usage_query_output resp = { 0 };

	req.flags = (dir == CFA_DIR_TX ?
		    HWRM_TFC_RESC_USAGE_QUERY_INPUT_FLAGS_DIR_TX :
		    HWRM_TFC_RESC_USAGE_QUERY_INPUT_FLAGS_DIR_RX);

	req.sid = rte_le_to_cpu_16(sid);
	req.fid = CFA_INVALID_FID; /* For WC-TCAM, ignore FID */
	req.data_size = rte_le_to_cpu_16(*data_size);
	req.track_type = HWRM_TFC_RESC_USAGE_QUERY_INPUT_TRACK_TYPE_TRACK_TYPE_SID;

	rc = bnxt_hwrm_tf_message_direct(bp, false, HWRM_TFC_RESC_USAGE_QUERY,
					 &req, sizeof(req), &resp, sizeof(resp));
	if (rc)
		return rc;

	if (*data_size < rte_le_to_cpu_16(resp.data_size)) {
		PMD_DRV_LOG_LINE(ERR, "Table buffer is too small, rc:%s",
				 strerror(EINVAL));
		rc = -EINVAL;
	} else {
		*data_size = resp.data_size;
		memcpy(data, resp.data, *data_size);
	}
	return rc;
}
#endif /* TF_FLOW_SCALE_QUERY */
