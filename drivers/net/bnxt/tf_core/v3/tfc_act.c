/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>

#include "bnxt.h"
#include "bnxt_mpc.h"

#include "tfc.h"
#include "cfa_bld_mpc_field_ids.h"
#include "cfa_bld_mpcops.h"
#include "tfo.h"
#include "tfc_em.h"
#include "tfc_cpm.h"
#include "tfc_msg.h"
#include "tfc_priv.h"
#include "cfa_types.h"
#include "cfa_mm.h"
#include "tfc_action_handle.h"
#include "sys_util.h"
#include "tfc_util.h"

/*
 * The read/write  granularity is 32B
 */
#define TFC_ACT_RW_GRANULARITY 32

#define TFC_ACT_CACHE_OPT_EN 0

/* Max additional data size in bytes */
#define TFC_ACT_DISCARD_DATA_SIZE 128

int tfc_act_alloc(struct tfc *tfcp,
		  uint8_t tsid,
		  struct tfc_cmm_info *cmm_info,
		  uint16_t num_contig_rec)
{
	int rc = 0;
	struct tfc_cpm *cpm_lkup = NULL;
	struct tfc_cpm *cpm_act = NULL;
	uint16_t pool_id;
	struct tfc_ts_mem_cfg mem_cfg;
	bool is_bs_owner;
	struct tfc_cmm *cmm;
	uint32_t entry_offset;
	struct cfa_mm_alloc_parms aparms;
	enum cfa_scope_type scope_type;
	struct tfc_ts_pool_info pi;
	bool valid;
	uint16_t max_pools;

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, &max_pools);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}

	if (unlikely(!valid)) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) not allocated", tsid);
		return -EINVAL;
	}

	if (unlikely(max_pools == 0)) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) Max pools must be greater than 0 %d",
				 tsid, max_pools);
		return -EINVAL;
	}

	rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, cmm_info->dir, &pi);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "%s: failed to get pool info: %s",
				 __func__, strerror(-rc));
	}

	/* Get CPM instances */
	rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, cmm_info->dir, &cpm_lkup, &cpm_act);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get CPM instances: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				cmm_info->dir,
				CFA_REGION_TYPE_ACT,
				&is_bs_owner,
				&mem_cfg);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %s", strerror(-rc));
		return -EINVAL;
	}

	/* if no pool available locally or all pools full */
	rc = tfc_cpm_get_avail_pool(cpm_act, &pool_id);

	if (rc) {
		/* Allocate a pool */
		struct cfa_mm_query_parms qparms;
		struct cfa_mm_open_parms oparms;
		uint16_t fid;

		/* There is only 1 pool for a non-shared table scope
		 * and it is full.
		 */
		if (unlikely(scope_type == CFA_SCOPE_TYPE_NON_SHARED)) {
			PMD_DRV_LOG_LINE(ERR, "%s: no records remain",
					 __func__);
			return -ENOMEM;
		}
		rc = tfc_get_fid(tfcp, &fid);
		if (unlikely(rc))
			return rc;

		rc = tfc_tbl_scope_pool_alloc(tfcp,
					      fid,
					      tsid,
					      CFA_REGION_TYPE_ACT,
					      cmm_info->dir,
					      NULL,
					      &pool_id);

		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "table scope pool alloc failed: %s",
					 strerror(-rc));
			return -EINVAL;
		}

		/* Create pool CMM instance */
		qparms.max_records = mem_cfg.rec_cnt;
		qparms.max_contig_records = pi.act_max_contig_rec;
		rc = cfa_mm_query(&qparms);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "cfa_mm_query() failed: %s", strerror(-rc));
			return rc;
		}

		cmm = rte_zmalloc("tf", qparms.db_size, 0);
		oparms.db_mem_size = qparms.db_size;
		oparms.max_contig_records = qparms.max_contig_records;
		oparms.max_records = qparms.max_records / max_pools;
		rc = cfa_mm_open(cmm, &oparms);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "cfa_mm_open() failed: %d", rc);
			return -EINVAL;
		}

		/* Store CMM instance in the CPM */
		rc = tfc_cpm_set_cmm_inst(cpm_act, pool_id, cmm);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "tfc_cpm_set_cmm_inst() failed: %d", rc);
			return -EINVAL;
		}
		/* store updated pool info */
		tfo_ts_set_pool_info(tfcp->tfo, tsid, cmm_info->dir, &pi);

	} else {
		/* Get the pool instance and allocate an act rec index from the pool */
		rc = tfc_cpm_get_cmm_inst(cpm_act, pool_id, &cmm);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "tfc_cpm_get_cmm_inst() failed: %d", rc);
			return -EINVAL;
		}
	}
	aparms.num_contig_records = 1 << next_pow2(num_contig_rec);
	rc = cfa_mm_alloc(cmm, &aparms);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "cfa_mm_alloc() failed: %d", rc);
		return -EINVAL;
	}

	/* Update CPM info so it will determine best pool to use next alloc */
	rc = tfc_cpm_set_usage(pi.act_cpm, pool_id, aparms.used_count, aparms.all_used);
	if (unlikely(rc))
		PMD_DRV_LOG_LINE(ERR, "EM insert tfc_cpm_set_usage() failed: %d", rc);

	CREATE_OFFSET(&entry_offset, pi.act_pool_sz_exp, pool_id, aparms.record_offset);

	/* Create Action handle */
	cmm_info->act_handle = tfc_create_action_handle(tsid,
							num_contig_rec,
							entry_offset);
	return rc;
}

int tfc_act_set_response(struct cfa_bld_mpcinfo *mpc_info,
			 struct bnxt_mpc_mbuf *mpc_msg_out,
			 uint8_t *rx_msg)
{
	int rc;
	int i;
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_WRITE_CMP_MAX_FLD];

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_WRITE_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_write(rx_msg,
							     mpc_msg_out->msg_size,
							     fields_cmp);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "write parse failed: %d", rc);
		rc = -EINVAL;
	}

	if (unlikely(fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK)) {
		PMD_DRV_LOG_LINE(ERR, "failed with status code:%d",
				(uint32_t)fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val);
		rc = ((int)fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val) * -1;
	}

	return rc;
}

int tfc_act_set(struct tfc *tfcp,
		struct tfc_mpc_batch_info_t *batch_info,
		const struct tfc_cmm_info *cmm_info,
		const uint8_t *data,
		uint16_t data_sz_words)
{
	int rc = 0;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES];
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES];
	uint16_t opaque;
	uint32_t i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_WRITE_CMD_MAX_FLD];
	uint32_t entry_offset;
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct cfa_bld_mpcinfo *mpc_info;
	uint32_t record_size;
	uint8_t tsid;
	enum cfa_scope_type scope_type;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	/* Check that MPC APIs are bound */
	if (unlikely(mpc_info->mpcops == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "MPC not initialized");
		return -EINVAL;
	}

	tfc_get_fields_from_action_handle(&cmm_info->act_handle,
					  &tsid,
					  &record_size,
					  &entry_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (unlikely(!valid)) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_WRITE_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD].val = 0xAA;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD].val = CFA_BLD_MPC_HW_TABLE_TYPE_ACTION;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD].val = tsid;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD].val = data_sz_words;
#if TFC_ACT_CACHE_OPT_EN
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD].val = 0x01;
#endif
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD].val = entry_offset;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_write(tx_msg,
							     &buff_len,
							     data,
							     fields_cmd);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "write build failed: %d", rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (cmm_info->dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = buff_len - TFC_MPC_HEADER_SIZE_BYTES;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &opaque,
			  TFC_MPC_TABLE_WRITE,
			  batch_info);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "write MPC send failed: %d", rc);
		goto cleanup;
	}

	if (batch_info && !batch_info->enabled)
		rc =  tfc_act_set_response(mpc_info, &mpc_msg_out, rx_msg);

	return rc;

 cleanup:

	return rc;
}

int tfc_act_get_only_response(struct cfa_bld_mpcinfo *mpc_info,
			      struct bnxt_mpc_mbuf *mpc_msg_out,
			      uint8_t *rx_msg,
			      uint16_t *data_sz_words)
{
	int i;
	int rc;
	uint8_t discard_data[TFC_ACT_DISCARD_DATA_SIZE];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_READ_CMP_MAX_FLD] = { {0} };

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_READ_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_READ_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_read(rx_msg,
							    mpc_msg_out->msg_size,
							    discard_data,
							    *data_sz_words * TFC_MPC_BYTES_PER_WORD,
							    fields_cmp);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "Action read parse failed: %d", rc);
		return -1;
	}

	if (fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
		PMD_DRV_LOG_LINE(ERR, "Action read failed with status code:%d",
				 (uint32_t)fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val);
		rc = ((int)fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val) * -1;
		return -1;
	}

	return 0;
}

static int tfc_act_get_only(struct tfc *tfcp,
			    struct tfc_mpc_batch_info_t *batch_info,
			    const struct tfc_cmm_info *cmm_info,
			    uint64_t *host_address,
			    uint16_t *data_sz_words)
{
	int rc = 0;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES] = { 0 };
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES] = { 0 };
	uint16_t opaque;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_READ_CMD_MAX_FLD] = { {0} };
	uint32_t entry_offset;
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	uint32_t record_size;
	uint8_t tsid;
	enum cfa_scope_type scope_type;
	struct cfa_bld_mpcinfo *mpc_info;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	tfc_get_fields_from_action_handle(&cmm_info->act_handle,
					  &tsid,
					  &record_size,
					  &entry_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (unlikely(!valid)) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}

	/* Check that data pointer is word aligned */
	if (unlikely(*host_address  & 0x3ULL)) {
		PMD_DRV_LOG_LINE(ERR, "data pointer not word aligned");
		return -EINVAL;
	}

	/* Check that MPC APIs are bound */
	if (unlikely(mpc_info->mpcops == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "MPC not initialized");
		return -EINVAL;
	}

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_READ_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_READ_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_OPAQUE_FLD].val = 0xAA;

	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD].val =
		CFA_BLD_MPC_HW_TABLE_TYPE_ACTION;

	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD].val = tsid;

	fields_cmd[CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD].val = *data_sz_words;

#if TFC_ACT_CACHE_OPT_EN
	fields_cmd[CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD].val = 0x0;
#endif
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD].val = entry_offset;

	fields_cmd[CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD].val = *host_address;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_read(tx_msg,
							    &buff_len,
							    fields_cmd);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "read build failed: %d", rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (cmm_info->dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = buff_len - TFC_MPC_HEADER_SIZE_BYTES;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &opaque,
			  TFC_MPC_TABLE_READ,
			  batch_info);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "read MPC send failed: %d", rc);
		goto cleanup;
	}

	if ((batch_info && !batch_info->enabled) || !batch_info) {
		rc = tfc_act_get_only_response(mpc_info,
					       &mpc_msg_out,
					       rx_msg,
					       data_sz_words);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "Action response failed: %d", rc);
			goto cleanup;
		}
	} else {
		batch_info->comp_info[batch_info->count - 1].read_words = *data_sz_words;
	}

	return 0;

 cleanup:

	return rc;
}

int tfc_act_get_clear_response(struct cfa_bld_mpcinfo *mpc_info,
			       struct bnxt_mpc_mbuf *mpc_msg_out,
			       uint8_t *rx_msg,
			       uint16_t *data_sz_words)
{
	int i;
	int rc;
	uint8_t discard_data[TFC_ACT_DISCARD_DATA_SIZE];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_MAX_FLD] = { {0} };

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_READ_CLR_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_read_clr(rx_msg,
								mpc_msg_out->msg_size,
								discard_data,
								*data_sz_words *
								TFC_MPC_BYTES_PER_WORD,
								fields_cmp);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "Action read clear parse failed: %d", rc);
		return -1;
	}

	if (fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
		PMD_DRV_LOG_LINE(ERR, "Action read clear failed with status code:%d",
				 (uint32_t)fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].val);
		rc = ((int)fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].val) * -1;
		return rc;
	}

	return 0;
}

static int tfc_act_get_clear(struct tfc *tfcp,
			     struct tfc_mpc_batch_info_t *batch_info,
			     const struct tfc_cmm_info *cmm_info,
			     uint64_t *host_address,
			     uint16_t *data_sz_words,
			     uint8_t clr_offset,
			     uint8_t clr_size)
{
	int rc = 0;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES] = { 0 };
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES] = { 0 };
	uint16_t opaque;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_MAX_FLD] = { {0} };
	uint32_t entry_offset;
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	uint32_t record_size;
	uint8_t tsid;
	enum cfa_scope_type scope_type;
	struct cfa_bld_mpcinfo *mpc_info;
	bool valid;
	uint16_t mask = 0;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	tfc_get_fields_from_action_handle(&cmm_info->act_handle,
					  &tsid,
					  &record_size,
					  &entry_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s",
				 strerror(-rc));
		return -EINVAL;
	}
	if (unlikely(!valid)) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}

	/* Check that data pointer is word aligned */
	if (unlikely(*host_address  & 0x3ULL)) {
		PMD_DRV_LOG_LINE(ERR, "data pointer not word aligned");
		return -EINVAL;
	}

	/* Check that MPC APIs are bound */
	if (unlikely(mpc_info->mpcops == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "MPC not initialized");
		return -EINVAL;
	}

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_READ_CLR_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD].val = 0xAA;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD].val =
		CFA_BLD_MPC_HW_TABLE_TYPE_ACTION;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD].val = tsid;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD].val = *data_sz_words;

#if TFC_ACT_CACHE_OPT_EN
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD].val = 0x0;
#endif
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD].val = entry_offset;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD].val = *host_address;

	for (i = clr_offset; i < clr_size; i++)
		mask |= (1 << i);

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD].val = mask;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_read_clr(tx_msg,
								&buff_len,
								fields_cmd);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "read clear build failed: %d", rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (cmm_info->dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = buff_len - TFC_MPC_HEADER_SIZE_BYTES;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &opaque,
			  TFC_MPC_TABLE_READ_CLEAR,
			  batch_info);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "read clear MPC send failed: %d", rc);
		goto cleanup;
	}

	if ((batch_info && !batch_info->enabled) || !batch_info) {
		rc = tfc_act_get_clear_response(mpc_info,
						&mpc_msg_out,
						rx_msg,
						data_sz_words);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "Action response failed: %d", rc);
			goto cleanup;
		}
	} else {
		batch_info->comp_info[batch_info->count - 1].read_words = *data_sz_words;
	}

	return 0;

 cleanup:

	return rc;
}

int tfc_act_get(struct tfc *tfcp,
		struct tfc_mpc_batch_info_t *batch_info,
		const struct tfc_cmm_info *cmm_info,
		struct tfc_cmm_clr *clr,
		uint64_t *host_address,
		uint16_t *data_sz_words)
{
	/* It's not an error to pass clr as a Null pointer, just means that read
	 * and clear is not being requested.  Also allow the user to manage
	 * clear via the clr flag.
	 */
	if (clr && clr->clr) {
		/* Clear offset and size have to be two bytes aligned */
		if (clr->offset_in_byte % 2 || clr->sz_in_byte % 2) {
			PMD_DRV_LOG_LINE(ERR,
					 "clr offset(%d) or size(%d) is not two bytes aligned",
					 clr->offset_in_byte, clr->sz_in_byte);
			return -EINVAL;
		}

		return tfc_act_get_clear(tfcp,
					 batch_info,
					 cmm_info,
					 host_address,
					 data_sz_words,
					 clr->offset_in_byte / 2,
					 clr->sz_in_byte / 2);
	} else {
		return tfc_act_get_only(tfcp,
					batch_info,
					cmm_info,
					host_address,
					data_sz_words);
	}
}

int tfc_act_free(struct tfc *tfcp,
		 const struct tfc_cmm_info *cmm_info)
{
	int rc = 0;
	struct tfc_cpm *cpm_lkup = NULL;
	struct tfc_cpm *cpm_act = NULL;
	struct tfc_cmm *cmm;
	uint32_t pool_id;
	struct tfc_ts_pool_info pi;
	uint32_t record_size;
	uint32_t record_offset;
	struct cfa_mm_free_parms fparms;
	uint8_t tsid;
	enum cfa_scope_type scope_type;
	bool valid;
	bool is_bs_owner;
	struct tfc_ts_mem_cfg mem_cfg;

	/* Get fields from MPC Action handle */
	tfc_get_fields_from_action_handle(&cmm_info->act_handle,
					  &tsid,
					  &record_size,
					  &record_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (unlikely(!valid)) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}
	tfo_ts_get_pool_info(tfcp->tfo, tsid, cmm_info->dir, &pi);

	pool_id = TFC_ACTION_GET_POOL_ID(record_offset, pi.act_pool_sz_exp);

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				cmm_info->dir,
				CFA_REGION_TYPE_ACT,
				&is_bs_owner,
				&mem_cfg);   /* Gets rec_cnt */
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	/* Get CPM instance for this table scope */
	rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, cmm_info->dir, &cpm_lkup, &cpm_act);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get CPM instance: %d", rc);
		return -EINVAL;
	}

	rc = tfc_cpm_get_cmm_inst(cpm_act, pool_id, &cmm);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get CMM instance: %d", rc);
		return -EINVAL;
	}

	fparms.record_offset = REMOVE_POOL_FROM_OFFSET(pi.act_pool_sz_exp,
						       record_offset);
	fparms.num_contig_records = 1 << next_pow2(record_size);
	rc = cfa_mm_free(cmm, &fparms);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to free record: %d", rc);
		return -EINVAL;
	}

	rc = tfc_cpm_set_usage(cpm_act, pool_id, 0, false);
	if (unlikely(rc))
		PMD_DRV_LOG_LINE(ERR, "failed to set usage: %d", rc);

	return rc;
}
