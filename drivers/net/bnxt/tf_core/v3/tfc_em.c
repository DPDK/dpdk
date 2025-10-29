/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <memory.h>

#include "bnxt.h"
#include "bnxt_mpc.h"

#include "tfc.h"
#include "tfo.h"
#include "tfc_em.h"
#include "tfc_cpm.h"
#include "tfc_msg.h"
#include "tfc_priv.h"
#include "cfa_types.h"
#include "cfa_mm.h"
#include "cfa_bld_mpc_field_ids.h"
#include "tfc_flow_handle.h"
#include "sys_util.h"
#include "tfc_util.h"

#include "tfc_debug.h"

#define TFC_EM_DYNAMIC_BUCKET_RECORD_SIZE 1

/* Enable cache configuration */
#define TFC_EM_CACHE_OPT_EN	 0
/* Enable dynamic bucket support */

#ifdef EM_DEBUG
char const *tfc_mpc_error_string[] = {
	"OK",
	"Unsupported Opcode",
	"Format",
	"Scope",
	"Address",
	"Cache",
	"EM Miss",
	"Duplicate",
	"No Events",
	"EM Abort"
};
#endif

static int tfc_em_insert_response(struct cfa_bld_mpcinfo *mpc_info,
				  struct bnxt_mpc_mbuf *mpc_msg_out,
				  uint8_t *rx_msg,
				  uint32_t *hash)
{
	int i;
	int rc;
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_MAX_FLD];

		/* Process response */
	for (i = 0; i < CFA_BLD_MPC_EM_INSERT_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD;
	fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_BKT_NUM_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMP_BKT_NUM_FLD;
	fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_NUM_ENTRIES_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMP_NUM_ENTRIES_FLD;
	fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX3_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX3_FLD;
	fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_CHAIN_UPD_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMP_CHAIN_UPD_FLD;
	fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_HASH_MSB_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMP_HASH_MSB_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_em_insert(rx_msg,
							   mpc_msg_out->msg_size,
							   fields_cmp);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "EM insert parse failed: %d", rc);
		return -EINVAL;
	}
	if (fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
#ifdef EM_DEBUG
		PMD_DRV_LOG_LINE(ERR, "MPC failed with error:%s",
				 tfc_mpc_error_string[(uint32_t)
				 fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD].val]);
#else
		PMD_DRV_LOG_LINE(ERR, "MPC failed with status code:%d",
				 (uint32_t)
				 fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD].val);
#endif
		rc = ((int)fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_STATUS_FLD].val) * -1;
		return rc;
	}

#if TFC_EM_DYNAMIC_BUCKET_EN
	/* If the dynamic bucket is unused then free it */
	if (bucket_offset && fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_CHAIN_UPD_FLD].val == 0) {
		/* Free allocated resources */
		fparms.record_offset = bucket_offset;
		fparms.num_contig_records = TFC_EM_DYNAMIC_BUCKET_RECORD_SIZE;

		rc = cfa_mm_free(cmm, &fparms);
		bucket_offset = 0;
	}
#endif

	*hash = fields_cmp[CFA_BLD_MPC_EM_INSERT_CMP_TABLE_INDEX3_FLD].val;

	return rc;
}

int tfc_em_insert(struct tfc *tfcp, uint8_t tsid,
		  struct tfc_em_insert_parms *parms)
{
	int rc;
	int cleanup_rc;
	struct tfc_cpm *cpm_lkup = NULL;
	struct tfc_cpm *cpm_act = NULL;
	uint16_t pool_id;
	struct tfc_cmm *cmm = NULL;
	bool is_bs_owner;
	struct tfc_ts_mem_cfg mem_cfg;
	uint32_t entry_offset;
	uint32_t num_contig_records;
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct tfc_ts_pool_info pi;
	struct cfa_mm_free_parms fparms;
	struct cfa_mm_alloc_parms aparms;
	uint32_t buff_len;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES];
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES];
	uint32_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	uint32_t i;
	uint32_t hash = 0;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_MAX_FLD];
	bool is_shared;
	struct cfa_bld_mpcinfo *mpc_info;
	bool valid;
	uint16_t max_pools;
#if TFC_EM_DYNAMIC_BUCKET_EN
	struct cfa_mm_alloc_parms bucket_aparms;
	bool shared = false;
	uint32_t bucket_offset;
#endif

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	rc = tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, &valid, &max_pools);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (unlikely(!valid)) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}
	if (unlikely(!max_pools)) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) Max pools must be greater than 0 %d",
				 tsid, max_pools);
		return -EINVAL;
	}

	/* Check that MPC APIs are bound */
	if (unlikely(mpc_info->mpcops == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "MPC not initialized");
		return -EINVAL;
	}

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				parms->dir,
				CFA_REGION_TYPE_LKUP,
				&is_bs_owner,
				&mem_cfg);   /* Gets rec_cnt */
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %s",
				 strerror(-rc));
		return -EINVAL;
	}


	/* Get CPM instances */
	rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, parms->dir, &cpm_lkup, &cpm_act);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "failed to get CPM instances: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	num_contig_records = 1 << next_pow2(parms->lkup_key_sz_words);

	tfo_ts_get_pool_info(tfcp->tfo, tsid, parms->dir, &pi);

	rc = tfc_cpm_get_avail_pool(cpm_lkup, &pool_id);

	/* if no pool available locally or all pools full */
	if (rc) {
		/* Allocate a pool */
		struct cfa_mm_query_parms qparms;
		struct cfa_mm_open_parms oparms;
		uint16_t fid;

		/* There is only 1 pool for a non-shared table scope and
		 * it is full.
		 */
		if (!is_shared) {
			PMD_DRV_LOG_LINE(ERR, "no records remain");
			return -ENOMEM;
		}

		rc = tfc_get_fid(tfcp, &fid);
		if (unlikely(rc))
			return rc;

		rc = tfc_tbl_scope_pool_alloc(tfcp,
					      fid,
					      tsid,
					      CFA_REGION_TYPE_LKUP,
					      parms->dir,
					      NULL,
					      &pool_id);

		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "table scope pool alloc failed: %s",
					 strerror(-rc));
			return -EINVAL;
		}

		/*
		 * Create pool CMM instance.
		 * rec_cnt is the total number of records which includes static buckets,
		 */
		qparms.max_records = (mem_cfg.rec_cnt - mem_cfg.lkup_rec_start_offset) / max_pools;
		qparms.max_contig_records = pi.lkup_max_contig_rec;
		rc = cfa_mm_query(&qparms);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "cfa_mm_query() failed: %s", strerror(-rc));
			rte_free(cmm);
			return -EINVAL;
		}

		cmm = rte_zmalloc("tf", qparms.db_size, 0);
		oparms.db_mem_size = qparms.db_size;
		oparms.max_contig_records = qparms.max_contig_records;
		oparms.max_records = qparms.max_records;
		rc = cfa_mm_open(cmm, &oparms);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "cfa_mm_open() failed: %s", strerror(-rc));
			rte_free(cmm);
			return -EINVAL;
		}

		/* Store CMM instance in the CPM */
		rc = tfc_cpm_set_cmm_inst(cpm_lkup, pool_id, cmm);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "tfc_cpm_set_cmm_inst() failed: %s",
					 strerror(-rc));
			return -EINVAL;
		}

		/* Store the updated pool information */
		tfo_ts_set_pool_info(tfcp->tfo, tsid, parms->dir, &pi);

	} else {
		/* Get the pool instance and allocate an lkup rec index from the pool */
		rc = tfc_cpm_get_cmm_inst(cpm_lkup, pool_id, &cmm);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR, "tfc_cpm_get_cmm_inst() failed: %s",
					 strerror(-rc));
			return -EINVAL;
		}
	}

	aparms.num_contig_records = num_contig_records;
	rc = cfa_mm_alloc(cmm, &aparms);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "cfa_mm_alloc() failed: %s", strerror(-rc));
		return -EINVAL;
	}

#if TFC_EM_DYNAMIC_BUCKET_EN
	if (!shared) {
		/* Allocate dynamic bucket */
		bucket_aparms.num_contig_records = TFC_EM_DYNAMIC_BUCKET_RECORD_SIZE;
		rc = cfa_mm_alloc(cmm, &bucket_aparms);
		if (unlikely(rc)) {
			PMD_DRV_LOG_LINE(ERR,
					 "cfa_mm_alloc() for dynamic bucket failed: %s\n",
					 strerror(-rc));
			return -EINVAL;
		}

		bucket_offset  = bucket_aparms.record_offset;
	}
#endif

	CREATE_OFFSET(&entry_offset, pi.lkup_pool_sz_exp, pool_id, aparms.record_offset);

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_EM_INSERT_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_OPAQUE_FLD].val = 0xAA;

	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_SCOPE_FLD].val = tsid;

	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_DATA_SIZE_FLD].val = parms->lkup_key_sz_words;

		/* LREC address */
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX_FLD].val = entry_offset +
		mem_cfg.lkup_rec_start_offset;

#if TFC_EM_DYNAMIC_BUCKET_EN
		/* Dynamic bucket address */
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX2_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX2_FLD;
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_TABLE_INDEX2_FLD].val = bucket_offset;
#endif
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_REPLACE_FLD].field_id =
		CFA_BLD_MPC_EM_INSERT_CMD_REPLACE_FLD;
	fields_cmd[CFA_BLD_MPC_EM_INSERT_CMD_REPLACE_FLD].val = 0x0;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_em_insert(tx_msg,
							   &buff_len,
							   parms->lkup_key_data,
							   fields_cmd);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "EM insert build failed: %s",
				 strerror(-rc));
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (parms->dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = (parms->lkup_key_sz_words * 32) +
		TFC_MPC_HEADER_SIZE_BYTES;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_LONG;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &msg_count,
			  TFC_MPC_EM_INSERT,
			  parms->batch_info);

	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR, "EM insert send failed: %s", strerror(-rc));
		goto cleanup;
	}

	if (parms->batch_info && !parms->batch_info->enabled) {
		rc = tfc_em_insert_response(mpc_info,
					    &mpc_msg_out,
					    rx_msg,
					    &hash);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR,
					 "EM insert tfc_em_insert_response() failed: %d",
					 rc);
			goto cleanup;
		}
	}

	*parms->flow_handle = tfc_create_flow_handle(tsid,
						     num_contig_records, /* Based on key size */
						     entry_offset,
						     hash);

	/* Update CPM info so it will determine best pool to use next alloc */
	rc = tfc_cpm_set_usage(cpm_lkup, pool_id, aparms.used_count, aparms.all_used);
	if (unlikely(rc)) {
		PMD_DRV_LOG_LINE(ERR,
				 "EM insert tfc_cpm_set_usage() failed: %d",
				 rc);
		goto cleanup;
	}

	if (!rc)
		return 0;

 cleanup:
	/*
	 * Preserve the rc from the actual error rather than
	 * an error during cleanup.
	 */
#if TFC_EM_DYNAMIC_BUCKET_EN
	/* If the dynamic bucket set then free it */
	if (bucket_offset) {
		/* Free allocated resources */
		fparms.record_offset = bucket_offset;
		fparms.num_contig_records = TFC_EM_DYNAMIC_BUCKET_RECORD_SIZE;

		cleanup_rc = cfa_mm_free(cmm, &fparms);
	}
#endif

	/* Free allocated resources */
	fparms.record_offset = aparms.record_offset;
	fparms.num_contig_records = num_contig_records;
	cleanup_rc = cfa_mm_free(cmm, &fparms);
	if (cleanup_rc != 0)
		PMD_DRV_LOG_LINE(ERR, "failed to free entry: %s", strerror(-rc));

	cleanup_rc = tfc_cpm_set_usage(cpm_lkup, pool_id, fparms.used_count, false);
	if (cleanup_rc != 0)
		PMD_DRV_LOG_LINE(ERR, "failed to set usage: %s", strerror(-rc));

	return rc;
}

static int tfc_em_delete_response(struct cfa_bld_mpcinfo *mpc_info,
				  struct bnxt_mpc_mbuf *mpc_msg_out,
				  uint8_t *rx_msg
#if TFC_EM_DYNAMIC_BUCKET_EN
				  , bool *db_unused,
				  uint32_t *db_offset
#endif
			      )
{
	int i;
	int rc;
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_MAX_FLD];

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_EM_DELETE_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD;
#ifdef EM_DEBUF
	fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_BKT_NUM_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMP_BKT_NUM_FLD;
	fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_NUM_ENTRIES_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMP_NUM_ENTRIES_FLD;
	fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX3_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX3_FLD;
	fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_CHAIN_UPD_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMP_CHAIN_UPD_FLD;
#endif
	rc = mpc_info->mpcops->cfa_bld_mpc_parse_em_delete(rx_msg,
							   mpc_msg_out->msg_size,
							   fields_cmp);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "delete parse failed: %s", strerror(-rc));
		return -EINVAL;
	}

	if (fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
#ifdef EM_DEBUG
		PMD_DRV_LOG_LINE(ERR, "MPC failed with error:%s",
				 tfc_mpc_error_string[(uint32_t)
				 fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD].val]);
#else
		PMD_DRV_LOG_LINE(ERR, "MPC failed with status code:%d",
				 (uint32_t)
				 fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD].val);
#endif
		rc = ((int)fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_STATUS_FLD].val) * -1;
		return rc;
	}

#if TFC_EM_DYNAMIC_BUCKET_EN
	*db_unused = fields_cmp[CFA_BLD_MPC_EM_DELETE_CMP_CHAIN_UPD_FLD].val == 1 ?
		true : false;
	*db_offset = fields[CFA_BLD_MPC_EM_DELETE_CMP_TABLE_INDEX3_FLD].val;
#endif
	return 0;
}

int tfc_em_delete_raw(struct tfc *tfcp,
		      uint8_t tsid,
		      enum cfa_dir dir,
		      uint32_t offset,
		      uint32_t static_bucket,
		      struct tfc_mpc_batch_info_t *batch_info
#if TFC_EM_DYNAMIC_BUCKET_EN
		      , bool *db_unused,
		      uint32_t *db_offset
#endif
		      )
{
	int rc = 0;
	uint32_t buff_len;
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES];
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES];
	uint32_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	int i;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_MAX_FLD];
	struct cfa_bld_mpcinfo *mpc_info;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);
	if (mpc_info->mpcops == NULL) {
		PMD_DRV_LOG_LINE(ERR, "MPC not initialized");
		return -EINVAL;
	}

	/* Create MPC EM delete command using builder */
	for (i = 0; i < CFA_BLD_MPC_EM_DELETE_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_OPAQUE_FLD].val = 0xAA;
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_SCOPE_FLD].val = tsid;

	/* LREC address to delete */
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX_FLD].val = offset;

	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD;
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD].val = static_bucket;

#if TFC_EM_DYNAMIC_BUCKET_EN
	/* Static bucket address */
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD].field_id =
		CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD;
	fields_cmd[CFA_BLD_MPC_EM_DELETE_CMD_TABLE_INDEX2_FLD].val = 0x01222222;
#endif

	/* Create MPC EM delete command using builder */
	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_em_delete(tx_msg,
							   &buff_len,
							   fields_cmd);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "delete mpc build failed: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = 16;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_LONG;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;
	mpc_msg_out.chnl_id = 0;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &msg_count,
			  TFC_MPC_EM_DELETE,
			  batch_info);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "delete MPC send failed: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	if (!batch_info->enabled)
		rc = tfc_em_delete_response(mpc_info,
					    &mpc_msg_out,
					    rx_msg
#if TFC_EM_DYNAMIC_BUCKET_EN
					    , db_unused,
					    db_offset
#endif
					    );
	return rc;
}

int tfc_em_delete(struct tfc *tfcp, struct tfc_em_delete_parms *parms)
{
	int rc = 0;
	uint32_t static_bucket;
	uint32_t pool_id;
	struct tfc_cpm *cpm_lkup = NULL;
	struct tfc_cpm *cpm_act = NULL;
	struct tfc_cmm *cmm;
	uint32_t record_offset;
	uint32_t record_size;
	struct cfa_mm_free_parms fparms;
	uint8_t tsid;
	bool is_shared;
	struct tfc_ts_pool_info pi;
	bool is_bs_owner;
	struct tfc_ts_mem_cfg mem_cfg;
	bool valid;

#if TFC_EM_DYNAMIC_BUCKET_EN
	bool db_unused;
	uint32_t db_offset;
#endif
	/* Get fields from MPC Flow handle */
	tfc_get_fields_from_flow_handle(&parms->flow_handle,
					&tsid,
					&record_size,
					&record_offset,
					&static_bucket);

	rc = tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, &valid, NULL);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s",
				 strerror(-rc));
		return -EINVAL;
	}
	if (!valid) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}

	tfo_ts_get_pool_info(tfcp->tfo, tsid, parms->dir, &pi);

	pool_id = TFC_FLOW_GET_POOL_ID(record_offset, pi.lkup_pool_sz_exp);

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				parms->dir,
				CFA_REGION_TYPE_LKUP,
				&is_bs_owner,
				&mem_cfg);   /* Gets rec_cnt */
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	/* Get CPM instance for this table scope */
	rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, parms->dir, &cpm_lkup, &cpm_act);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get CMM instance: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	rc = tfc_em_delete_raw(tfcp,
			       tsid,
			       parms->dir,
			       record_offset +
			       mem_cfg.lkup_rec_start_offset,
			       static_bucket,
			       parms->batch_info
#if TFC_EM_DYNAMIC_BUCKET_EN
			       , &db_unused,
			       &db_offset
#endif
			       );

	record_offset = REMOVE_POOL_FROM_OFFSET(pi.lkup_pool_sz_exp,
						record_offset);
#if TFC_EM_DYNAMIC_BUCKET_EN
	/* If the dynamic bucket is unused then free it */
	if (db_unused) {
		/* Free allocated resources */
		fparms.record_offset = db_offset;
		fparms.num_contig_records = TFC_EM_DYNAMIC_BUCKET_RECORD_SIZE;

		rc = cfa_mm_free(cmm, &fparms);
	}
#endif

	rc = tfc_cpm_get_cmm_inst(cpm_lkup, pool_id, &cmm);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get CMM instance: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	fparms.record_offset = record_offset;
	fparms.num_contig_records = 1 << next_pow2(record_size);

	rc = cfa_mm_free(cmm, &fparms);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to free CMM instance: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	rc = tfc_cpm_set_usage(cpm_lkup, pool_id, fparms.used_count, false);
	if (rc != 0)
		PMD_DRV_LOG_LINE(ERR, "failed to set usage: %s",
				 strerror(-rc));

	return rc;
}

static void bucket_decode(uint32_t *bucket_ptr,
			  struct bucket_info_t *bucket_info)
{
	int i;
	int offset = 0;

	bucket_info->valid = false;
	bucket_info->chain = tfc_getbits(bucket_ptr, 254, 1);
	bucket_info->chain_ptr = tfc_getbits(bucket_ptr, 228, 26);

	if  (bucket_info->chain ||
	     bucket_info->chain_ptr)
		bucket_info->valid = true;

	for (i = 0; i < TFC_BUCKET_ENTRIES; i++) {
		bucket_info->entries[i].entry_ptr = tfc_getbits(bucket_ptr, offset, 26);
		offset +=  26;
		bucket_info->entries[i].hash_msb = tfc_getbits(bucket_ptr, offset, 12);
		offset += 12;
		if  (bucket_info->entries[i].hash_msb ||
		     bucket_info->entries[i].entry_ptr) {
			bucket_info->valid = true;
		}
	}
}

int tfc_em_delete_entries_by_pool_id(struct tfc *tfcp,
				     uint8_t tsid,
				     enum cfa_dir dir,
				     uint16_t pool_id,
				     uint8_t debug,
				     uint8_t *data)
{
	uint32_t offset;
	int rc;
	int i;
	int j;
	struct bucket_info_t bucket;
	struct tfc_ts_pool_info pi;
	struct tfc_ts_mem_cfg mem_cfg;
	bool is_bs_owner;
#if TFC_EM_DYNAMIC_BUCKET_EN
	bool db_unused;
	uint32_t db_offset;
#endif
	struct tfc_mpc_batch_info_t batch_info;

	memset(&batch_info, 0, sizeof(batch_info));

	/* Get memory info */
	rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, &pi);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR,
				 "Failed to get pool info for tsid:%d",
				 tsid);
		return -EINVAL;
	}

	rc = tfo_ts_get_mem_cfg(tfcp->tfo,
				tsid,
				dir,
				CFA_REGION_TYPE_LKUP,
				&is_bs_owner,
				&mem_cfg);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %s",
				 strerror(-rc));
		return -EINVAL;
	}

	/* Read static bucket entries */
	for (offset = 0; offset < mem_cfg.lkup_rec_start_offset; ) {
		/*
		 * Read static bucket region of lookup table.
		 * A static bucket is 32B in size and must be 32B aligned.
		 * A table read can read up to 4 * 32B so in the interest
		 * of efficiency the max read size will be used.
		 */
		rc = tfc_mpc_table_read(tfcp,
					tsid,
					dir,
					CFA_REGION_TYPE_LKUP,
					offset,
					TFC_MPC_MAX_TABLE_READ_WORDS,
					data,
					debug);

		if (rc != 0) {
			PMD_DRV_LOG_LINE(ERR,
					 "tfc_mpc_table_read() failed for offset:%d: %s",
					 offset, strerror(-rc));
			return -EINVAL;
		}

		for (i = 0; (i < TFC_MPC_MAX_TABLE_READ_WORDS) &&
			     (offset < mem_cfg.lkup_rec_start_offset); i++) {
			/* Walk static bucket entry pointers */
			bucket_decode((uint32_t *)&data[i * TFC_MPC_BYTES_PER_WORD],
				      &bucket);

			for (j = 0; j < TFC_BUCKET_ENTRIES; j++) {
				if (bucket.entries[j].entry_ptr != 0 &&
		    pool_id == (bucket.entries[j].entry_ptr >> pi.lkup_pool_sz_exp)) {
					/* Delete EM entry */
					rc = tfc_em_delete_raw(tfcp,
							       tsid,
							       dir,
							       bucket.entries[j].entry_ptr,
							       offset,
							       &batch_info
#if TFC_EM_DYNAMIC_BUCKET_EN
							       , &db_unused,
							       &db_offset
#endif
							       );
					if (rc) {
						PMD_DRV_LOG_LINE(ERR,
								 "EM delete failed for offset:0x%08x %d",
								 offset, rc);
						return -1;
					}
				}
			}

			offset++;
		}
	}

	return rc;
}

int tfc_mpc_send(struct bnxt *bp,
		 struct bnxt_mpc_mbuf *in_msg,
		 struct bnxt_mpc_mbuf *out_msg,
		 uint32_t *opaque,
		 int type,
		 struct tfc_mpc_batch_info_t *batch_info)
{
	int rc;
	bool enabled = false;

	if (batch_info)
		enabled = batch_info->enabled;

	rc = bnxt_mpc_send(bp, in_msg, out_msg, opaque, enabled);

	if (rc)
		return rc;

	if (batch_info && batch_info->enabled) {
		memcpy(&batch_info->comp_info[batch_info->count].out_msg,
		       out_msg,
		       sizeof(*out_msg));
		batch_info->comp_info[batch_info->count].mpc_queue =
			bp->mpc->mpc_txq[in_msg->chnl_id];
		batch_info->comp_info[batch_info->count].type = type;
		batch_info->count++;
	}

	return 0;
}

static int tfc_mpc_process_completions(uint8_t *rx_msg,
				       struct tfc_mpc_comp_info_t *comp_info)
{
	int rc;
	int retry = BNXT_MPC_RX_RETRY;

	comp_info->out_msg.msg_data = rx_msg;

	do {
		rc =  bnxt_mpc_cmd_cmpl(comp_info->mpc_queue,
					&comp_info->out_msg);

		if (likely(rc == 1)) {
#ifdef MPC_DEBUG
			if (unlikely(retry != BNXT_MPC_RX_RETRY))
				PMD_DRV_LOG_LINE(ERR, "Retrys:%d",
						 BNXT_MPC_RX_RETRY - retry);
#endif
			return 0;
		}
#ifdef MPC_DEBUG
		PMD_DRV_LOG_LINE(ERR,
				 "Received zero or more than one completion:%d",
				 rc);
#endif
		retry--;
	} while (retry);

	PMD_DRV_LOG_LINE(ERR, "Retry timeout rc:%d", rc);
	return -1;
}

int tfc_mpc_batch_start(struct tfc_mpc_batch_info_t *batch_info)
{
	if (unlikely(batch_info->enabled))
		return -EBUSY;

	batch_info->enabled = true;
	batch_info->count = 0;
	batch_info->error = false;
	return 0;
}

bool tfc_mpc_batch_started(struct tfc_mpc_batch_info_t *batch_info)
{
	if (unlikely(!batch_info))
		return false;

	return (batch_info->enabled && batch_info->count > 0);
}

int tfc_mpc_batch_end(struct tfc *tfcp,
		      struct tfc_mpc_batch_info_t *batch_info)
{
	uint32_t i;
	int rc;
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES];
	struct cfa_bld_mpcinfo *mpc_info;
	uint32_t hash = 0;
#if TFC_EM_DYNAMIC_BUCKET_EN
	bool *db_unused;
	uint32_t *db_offset;
#endif

	if (unlikely(!batch_info->enabled))
		return -EBUSY;

	if (unlikely(!batch_info->count)) {
		batch_info->enabled = false;
		return 0;
	}

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	if (unlikely(mpc_info->mpcops == NULL)) {
		PMD_DRV_LOG_LINE(ERR, "MPC not initialized");
		return -EINVAL;
	}

	if (batch_info->count < (BNXT_MPC_COMP_MAX_COUNT / 4))
		rte_delay_us_block(BNXT_MPC_RX_US_DELAY * 4);

	for (i = 0; i < batch_info->count; i++) {
		rc = tfc_mpc_process_completions(&rx_msg[TFC_MPC_HEADER_SIZE_BYTES],
						 &batch_info->comp_info[i]);
		if (unlikely(rc))
			return -1;


		switch (batch_info->comp_info[i].type) {
		case TFC_MPC_EM_INSERT:
			rc = tfc_em_insert_response(mpc_info,
						    &batch_info->comp_info[i].out_msg,
						    rx_msg,
						    &hash);
			/*
			 * If the handle is non NULL it should reference a
			 * flow DB entry that requires the flow_handle
			 * contained within to be updated.
			 */
			batch_info->em_hdl[i] =
				tfc_create_flow_handle2(batch_info->em_hdl[i],
							hash);
			batch_info->em_error = rc;
			break;
		case TFC_MPC_EM_DELETE:
			rc = tfc_em_delete_response(mpc_info,
						    &batch_info->comp_info[i].out_msg,
						    rx_msg
#if TFC_EM_DYNAMIC_BUCKET_EN
						    , bool *db_unused,
						    uint32_t *db_offset
#endif
						    );
			break;
		case TFC_MPC_TABLE_WRITE:
			rc = tfc_act_set_response(mpc_info,
						  &batch_info->comp_info[i].out_msg,
						  rx_msg);
			break;
		case TFC_MPC_TABLE_READ:
			rc = tfc_act_get_only_response(mpc_info,
						       &batch_info->comp_info[i].out_msg,
						       rx_msg,
						       &batch_info->comp_info[i].read_words);
			break;

		case TFC_MPC_TABLE_READ_CLEAR:
			rc = tfc_act_get_clear_response(mpc_info,
							&batch_info->comp_info[i].out_msg,
							rx_msg,
							&batch_info->comp_info[i].read_words);
			break;

		default:
			PMD_DRV_LOG_LINE(ERR, "MPC Batch not supported for type: %d",
					 batch_info->comp_info[i].type);
			return -1;
		}

		batch_info->result[i] = rc;
		if (rc)
			batch_info->error = true;
	}

	batch_info->enabled = false;
	batch_info->count = 0;

	return 0;
}
