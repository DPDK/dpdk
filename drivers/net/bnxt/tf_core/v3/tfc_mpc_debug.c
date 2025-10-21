/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "bnxt.h"
#include "bnxt_mpc.h"

#include "tfc.h"
#include "cfa_bld_mpc_field_ids.h"
#include "cfa_bld_mpcops.h"
#include "tfo.h"
#include "tfc_em.h"
#include "tfc_cpm.h"
#include "tfc_msg.h"
#include "tfc_debug.h"
#include "cfa_types.h"
#include "cfa_mm.h"
#include "sys_util.h"
#include "cfa_bld.h"
#include "tfc_util.h"

int tfc_mpc_table_read(struct tfc *tfcp,
		       uint8_t tsid,
		       enum cfa_dir dir,
		       uint32_t type,
		       uint32_t offset,
		       uint8_t words,
		       uint8_t *data,
		       uint8_t debug)
{
	int rc = 0;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES];
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES];
	uint16_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_READ_CMD_MAX_FLD];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_READ_CMP_MAX_FLD];
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	enum cfa_scope_type scope_type;
	struct cfa_bld_mpcinfo *mpc_info;
	uint64_t host_address;
	uint8_t discard_data[128];
	uint32_t set;
	uint32_t way;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (!valid) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}

	/* Check that data pointer is word aligned */
	if (((uint64_t)data)  & 0x1fULL) {
		PMD_DRV_LOG_LINE(ERR, "Table read data pointer not word aligned");
		return -EINVAL;
	}

	host_address = (uint64_t)rte_mem_virt2iova(data);

	/* Check that MPC APIs are bound */
	if (mpc_info->mpcops == NULL) {
		PMD_DRV_LOG_LINE(ERR, "MPC not initialized");
		return -EINVAL;
	}

	set =  offset & 0x7ff;
	way = (offset >> 12)  & 0xf;

	if (debug)
		PMD_DRV_LOG_LINE(ERR,
				 "Debug read table type:%s %d words32B at way:%d set:%d debug:%d words32B",
				 (type  == 0 ? "Lookup" : "Action"),
				 words, way, set, debug);
	else
		PMD_DRV_LOG_LINE(ERR,
				 "Reading table type:%s %d words32B at offset %d words32B",
				 (type  == 0 ? "Lookup" : "Action"),
				 words, offset);

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_READ_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_READ_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_OPAQUE_FLD].val = 0xAA;

	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD].val = (type == 0 ?
	       CFA_BLD_MPC_HW_TABLE_TYPE_LOOKUP : CFA_BLD_MPC_HW_TABLE_TYPE_ACTION);

	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD].val =
		(debug ? way : tsid);

	fields_cmd[CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD].val = words;

	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD].val =
		(debug ? set : offset);

	fields_cmd[CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD].val = host_address;

	if (debug) {
		fields_cmd[CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD;
		fields_cmd[CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD].val = debug; /* Debug read */
	}

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_read(tx_msg,
							    &buff_len,
							    fields_cmd);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Action read build failed: %d", rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[16];
	mpc_msg_in.msg_size = 16;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[16];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &msg_count,
			  TFC_MPC_TABLE_READ,
			  NULL);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Table read MPC send failed: %d", rc);
		goto cleanup;
	}

		/* Process response */
	for (i = 0; i < CFA_BLD_MPC_READ_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_READ_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_read(rx_msg,
							    mpc_msg_out.msg_size,
							    discard_data,
							    words * TFC_MPC_BYTES_PER_WORD,
							    fields_cmp);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Table read parse failed: %d", rc);
		goto cleanup;
	}

	if (fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
		PMD_DRV_LOG_LINE(ERR, "Table read failed with status code:%d",
				 (uint32_t)fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val);
		rc = -1;
		goto cleanup;
	}

	return 0;

 cleanup:

	return rc;
}

int tfc_mpc_table_write_zero(struct tfc *tfcp,
			     uint8_t tsid,
			     enum cfa_dir dir,
			     uint32_t type,
			     uint32_t offset,
			     uint8_t words,
			     uint8_t *data)
{
	int rc = 0;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES];
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES];
	uint16_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_WRITE_CMD_MAX_FLD];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_WRITE_CMP_MAX_FLD];
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct cfa_bld_mpcinfo *mpc_info;
	enum cfa_scope_type scope_type;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (!valid) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}
	/* Check that MPC APIs are bound */
	if (mpc_info->mpcops == NULL) {
		PMD_DRV_LOG_LINE(ERR, " MPC not initialized");
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
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD].val = (type == 0 ?
	       CFA_BLD_MPC_HW_TABLE_TYPE_LOOKUP : CFA_BLD_MPC_HW_TABLE_TYPE_ACTION);

	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD].val = tsid;

	fields_cmd[CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD].val = words;

	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD].val = offset;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_write(tx_msg,
							     &buff_len,
							     data,
							     fields_cmd);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "write build failed: %d", rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[16];
	mpc_msg_in.msg_size = (words * TFC_MPC_BYTES_PER_WORD) + 16;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[16];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &msg_count,
			  TFC_MPC_TABLE_WRITE,
			  NULL);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "write MPC send failed: %d", rc);
		goto cleanup;
	}

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_WRITE_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_write(rx_msg,
							     mpc_msg_out.msg_size,
							     fields_cmp);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "write parse failed: %d", rc);
		goto cleanup;
	}

	if (fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
		PMD_DRV_LOG_LINE(ERR, "Action write failed with status code:%d",
				 (uint32_t)fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val);
		PMD_DRV_LOG_LINE(ERR, "Hash MSB:0x%0x",
		       (uint32_t)fields_cmp[CFA_BLD_MPC_WRITE_CMP_HASH_MSB_FLD].val);
		goto cleanup;
	}

	return 0;

 cleanup:

	return rc;
}

int tfc_mpc_table_invalidate(struct tfc *tfcp,
			     uint8_t tsid,
			     enum cfa_dir dir,
			     uint32_t type,
			     uint32_t offset,
			     uint32_t words)
{
	int rc = 0;
	uint8_t tx_msg[TFC_MPC_MAX_TX_BYTES];
	uint8_t rx_msg[TFC_MPC_MAX_RX_BYTES];
	uint16_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_MAX_FLD];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_INVALIDATE_CMP_MAX_FLD];
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct cfa_bld_mpcinfo *mpc_info;
	enum cfa_scope_type scope_type;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (!valid) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}
	/* Check that MPC APIs are bound */
	if (mpc_info->mpcops == NULL) {
		PMD_DRV_LOG_LINE(ERR, " MPC not initialized");
		return -EINVAL;
	}

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_INVALIDATE_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_INVALIDATE_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_OPAQUE_FLD].val = 0xAA;

	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_INVALIDATE_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_TYPE_FLD].val = (type == 0 ?
	       CFA_BLD_MPC_HW_TABLE_TYPE_LOOKUP : CFA_BLD_MPC_HW_TABLE_TYPE_ACTION);

	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_INVALIDATE_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_SCOPE_FLD].val = tsid;

	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_INVALIDATE_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_DATA_SIZE_FLD].val = words;

	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_INVALIDATE_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_TABLE_INDEX_FLD].val = offset;

	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD;
	fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_CACHE_OPTION_FLD].val =
		CFA_BLD_MPC_EV_EVICT_SCOPE_ADDRESS;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_evict(tx_msg,
							     &buff_len,
							     fields_cmd);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "evict build failed: %d", rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (dir == CFA_DIR_TX ?
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA :
			      HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[16];
	mpc_msg_in.msg_size = 16;
	mpc_msg_out.cmp_type = CMPL_BASE_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[16];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = tfc_mpc_send(tfcp->bp,
			  &mpc_msg_in,
			  &mpc_msg_out,
			  &msg_count,
			  TFC_MPC_INVALIDATE,
			  NULL);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "write MPC send failed: %d", rc);
		goto cleanup;
	}

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_INVALIDATE_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_INVALIDATE_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_INVALIDATE_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_evict(rx_msg,
							     mpc_msg_out.msg_size,
							     fields_cmp);

	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "evict parse failed: %d", rc);
		goto cleanup;
	}

	if (fields_cmp[CFA_BLD_MPC_INVALIDATE_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
		PMD_DRV_LOG_LINE(ERR, "evict failed with status code:%d",
				 (uint32_t)fields_cmp[CFA_BLD_MPC_INVALIDATE_CMP_STATUS_FLD].val);
		PMD_DRV_LOG_LINE(ERR, "Hash MSB:0x%0x",
		       (uint32_t)fields_cmp[CFA_BLD_MPC_INVALIDATE_CMP_HASH_MSB_FLD].val);
		goto cleanup;
	}

	return 0;

 cleanup:

	return rc;
}

#define TFC_ACTION_SIZE_BYTES  32
#define TFC_BUCKET_SIZE_BYTES  32

#define TFC_STRING_LENGTH_32  32
#define TFC_STRING_LENGTH_64  64
#define TFC_STRING_LENGTH_96  96
#define TFC_STRING_LENGTH_256 256

static const char * const opcode_string[] = {
	"NORMAL",
	"NORMAL_RFS",
	"FAST",
	"FAST_RFS",
	"CT_MISS_DEF",
	"INVALID",
	"CT_HIT_DEF",
	"INVALID",
	"RECYCLE"
};

static void act_decode(uint32_t *act_ptr,
		       uintptr_t base,
		       struct act_info_t *act_info);
static void stat_decode(char *str,
			uint8_t stat_num,
			uint8_t stat1_ctr_type,
			uint32_t *stat_ptr);

const char *get_lrec_opcode_str(uint8_t opcode)
{
	return opcode_string[opcode];
}

static uintptr_t get_address(struct tfc_ts_mem_cfg *mem, uint32_t offset)
{
	uint32_t page =  offset / mem->pg_tbl[0].pg_size;
	uint32_t adj_offset = offset % mem->pg_tbl[0].pg_size;
	int level = 0;
	uint64_t addr;

	/*
	 * Use the level according to the num_level of page table
	 */
	level = mem->num_lvl - 1;

	addr = (uint64_t)mem->pg_tbl[level].pg_va_tbl[page] + adj_offset;

	return addr;
}

void act_process(uint32_t act_rec_ptr,
		 struct act_info_t *act_info,
		 struct tfc_ts_mem_cfg *act_mem_cfg)
{
	uint8_t *act_ptr;
	uintptr_t base;
	uint32_t act_offset = act_rec_ptr << 5;

	base = get_address(act_mem_cfg, 0);
	act_ptr = (uint8_t *)get_address(act_mem_cfg, act_offset);
	act_decode((uint32_t *)act_ptr, base, act_info);
}

static void em_decode(uint32_t *em_ptr,
		      struct em_info_t *em_info,
		      struct tfc_ts_mem_cfg *act_mem_cfg)
{
	em_info->key = (uint8_t *)em_ptr;

	em_ptr += (128 / 8) / 4; /* For EM records the LREC follows 128 bits of key */
	em_info->valid = tfc_getbits(em_ptr, 127, 1);
	em_info->rec_size = tfc_getbits(em_ptr, 125, 2);
	em_info->epoch0 = tfc_getbits(em_ptr, 113, 12);
	em_info->epoch1 = tfc_getbits(em_ptr, 107, 6);
	em_info->opcode = tfc_getbits(em_ptr, 103, 4);
	em_info->strength = tfc_getbits(em_ptr, 101, 2);
	em_info->act_hint = tfc_getbits(em_ptr, 99, 2);

	if (em_info->opcode != 2 && em_info->opcode != 3) {
		/* All but FAST */
		em_info->act_rec_ptr = tfc_getbits(em_ptr, 73, 26);
		act_process(em_info->act_rec_ptr, &em_info->act_info, act_mem_cfg);
	} else {
		/* Just FAST */
		em_info->destination = tfc_getbits(em_ptr, 73, 17);
	}

	if (em_info->opcode == 4 || em_info->opcode == 6) {
		/* CT only */
		em_info->tcp_direction = tfc_getbits(em_ptr, 72, 1);
		em_info->tcp_update_en = tfc_getbits(em_ptr, 71, 1);
		em_info->tcp_win = tfc_getbits(em_ptr, 66, 5);
		em_info->tcp_msb_loc = tfc_getbits(em_ptr, 48, 18);
		em_info->tcp_msb_opp = tfc_getbits(em_ptr, 30, 18);
		em_info->tcp_msb_opp_init = tfc_getbits(em_ptr, 29, 1);
		em_info->state = tfc_getbits(em_ptr, 24, 5);
		em_info->timer_value  = tfc_getbits(em_ptr, 20, 4);
	} else if (em_info->opcode != 8) {
		/* Not CT and nor RECYCLE */
		em_info->ring_table_idx = tfc_getbits(em_ptr, 64, 9);
		em_info->act_rec_size = tfc_getbits(em_ptr, 59, 5);
		em_info->paths_m1 = tfc_getbits(em_ptr, 55, 4);
		em_info->fc_op  = tfc_getbits(em_ptr, 54, 1);
		em_info->fc_type = tfc_getbits(em_ptr, 52, 2);
		em_info->fc_ptr = tfc_getbits(em_ptr, 24, 28);
	} else {
		em_info->recycle_dest = tfc_getbits(em_ptr, 72, 1); /* Just Recycle */
		em_info->prof_func = tfc_getbits(em_ptr, 64, 8);
		em_info->meta_prof = tfc_getbits(em_ptr, 61, 3);
		em_info->metadata = tfc_getbits(em_ptr, 29, 32);
	}

	em_info->range_profile = tfc_getbits(em_ptr, 16, 4);
	em_info->range_index = tfc_getbits(em_ptr, 0, 16);
}

static void em_show(FILE *fd, struct em_info_t *em_info)
{
	int i;
	char *line1 = NULL;
	char *line2 = NULL;
	char *line3 = NULL;
	char *line4 = NULL;
	char tmp1[TFC_STRING_LENGTH_64];
	char tmp2[TFC_STRING_LENGTH_64];
	char tmp3[TFC_STRING_LENGTH_64];
	char tmp4[TFC_STRING_LENGTH_64];

	line1 = rte_zmalloc("data", TFC_STRING_LENGTH_256, 8);
	line2 = rte_zmalloc("data", TFC_STRING_LENGTH_256, 8);
	line3 = rte_zmalloc("data", TFC_STRING_LENGTH_256, 8);
	line4 = rte_zmalloc("data", TFC_STRING_LENGTH_256, 8);
	if (!line1 || !line2 || !line3 || !line4) {
		rte_free(line1);
		rte_free(line2);
		rte_free(line3);
		rte_free(line4);
		fprintf(fd, "%s: Failed to allocate temp buffer\n",
			   __func__);
		return;
	}

	fprintf(fd, ":LREC: opcode:%s\n", get_lrec_opcode_str(em_info->opcode));

	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+-Epoch-+--+--+--+");
	snprintf(line2, TFC_STRING_LENGTH_256, " V|rs|  0  1 |Op|St|ah|");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+----+--+--+--+--+");
	snprintf(line4, TFC_STRING_LENGTH_256, " %1d %2d %4d %2d %2d %2d %2d ",
		 em_info->valid,
		 em_info->rec_size,
		 em_info->epoch0,
		 em_info->epoch1,
		 em_info->opcode,
		 em_info->strength,
		 em_info->act_hint);

	if (em_info->opcode != 2 && em_info->opcode != 3) {
		/* All but FAST */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "-Act Rec--+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, " Ptr      |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "----------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "0x%08x ",
			 em_info->act_rec_ptr);
	} else {
		/* Just FAST */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "-------+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, " Dest  |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "-------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "0x05%x ",
			 em_info->destination);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	if (em_info->opcode == 4 || em_info->opcode == 6) {
		/* CT only */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "--+--+-------------TCP-------+--+---+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, "Dr|ue| Win|   lc  |   op  |oi|st|tmr|");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "--+--+----+-------+-------+--+--+---+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "%2d %2d %4d %0x5x %0x5x %2d %2d %3d ",
			 em_info->tcp_direction,
			 em_info->tcp_update_en,
			 em_info->tcp_win,
			 em_info->tcp_msb_loc,
			 em_info->tcp_msb_opp,
			 em_info->tcp_msb_opp_init,
			 em_info->state,
			 em_info->timer_value);
	} else if (em_info->opcode != 8) {
		/* Not CT and nor RECYCLE */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "--+--+--+-------FC-------+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, "RI|as|pm|op|tp|     Ptr  |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "--+--+--+--+--+----------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "%2d %2d %2d %2d %2d 0x%08x ",
			 em_info->ring_table_idx,
			 em_info->act_rec_size,
			 em_info->paths_m1,
			 em_info->fc_op,
			 em_info->fc_type,
			 em_info->fc_ptr);
	} else {
		snprintf(tmp1, TFC_STRING_LENGTH_64, "--+--+--+---------+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, "RD|pf|mp| cMData  |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "--+--+--+---------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "%2d 0x%2x %2d %08x ",
			 em_info->recycle_dest,
			 em_info->prof_func,
			 em_info->meta_prof,
			 em_info->metadata);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	snprintf(tmp1, TFC_STRING_LENGTH_64, "-----Range-+\n");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "Prof|  Idx |\n");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "----+------+\n");
	snprintf(tmp4, TFC_STRING_LENGTH_64, "0x%02x 0x%04x\n",
		 em_info->range_profile,
		 em_info->range_index);

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	fprintf(fd, "%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line4);

	fputs("Key:", fd);
	for (i = 0; i < ((em_info->rec_size + 1) * 32); i++) {
		if (i % 32 == 0)
			fprintf(fd, "\n%04d:  ", i);
		fprintf(fd, "%02x", em_info->key[i]);
	}
	i = ((em_info->rec_size + 1) * 32);
	fprintf(fd, "\nKey Reversed:\n%04d:  ", i - 32);
	do {
		i--;
		fprintf(fd, "%02x", em_info->key[i]);
		if (i != 0 && i % 32 == 0)
			fprintf(fd, "\n%04d:  ", i - 32);
	} while (i > 0);
	fputs("\n", fd);

	if (em_info->opcode != 2 && em_info->opcode != 3)
		act_show(fd, &em_info->act_info, em_info->act_rec_ptr << 5);

	rte_free(line1);
	rte_free(line2);
	rte_free(line3);
	rte_free(line4);
}

struct mod_field_s {
	uint8_t num_bits;
	const char *name;
};

struct mod_data_s {
	uint8_t num_fields;
	const char *name;
	struct mod_field_s field[4];
};

struct mod_data_s mod_data[] = {
	{1, "Replace", {{16,  "DPort"}}},
	{1, "Replace", {{16,  "SPort"}}},
	{1, "Replace", {{32,  "IPv4 DIP"}}},
	{1, "Replace", {{32,  "IPv4 SIP"}}},
	{1, "Replace", {{128, "IPv6 DIP"}}},
	{1, "Replace", {{128, "IPv6 SIP"}}},
	{1, "Replace", {{48,  "SMAC"}}},
	{1, "Replace", {{48,  "DMAC"}}},
	{2, "Update Field",  {{16, "uf_vec"}, {32, "uf_data"}}},
	{3, "Tunnel Modify", {{16, "tun_mv"}, {16, "tun_ex_prot"}, {16, "tun_new_prot"}}},
	{4, "TTL Update",    {{5,  "alt_pfid"}, {12, "alt_vid"}, {10, "rsvd"}, {5, "ttl_op"}}},
	{4, "Replace/Add Outer VLAN", {{16, "tpid"}, {3, "pri"}, {1, "de"}, {12, "vid"}}},
	{4, "Replace/Add Inner",      {{16, "tpid"}, {3, "pri"}, {1, "de"}, {12, "vid"}}},
	{0, "Remove outer VLAN", {{0, NULL}}},
	{0, "Remove inner VLAN", {{0, NULL}}},
	{4, "Metadata Update",   {{2, "md_op"}, {4, "md_prof"}, {10, "rsvd"}, {32, "md_data"}}},
};

static void mod_decode(uint32_t *data, char *mod_str)
{
	int i;
	int j;
	int k;
	uint16_t mod_vector;
	int32_t row_offset = 64;
	int32_t read_offset;
	int32_t row = 0;
	uint32_t val[8];
	char str[256];
	int16_t vect;
	uint16_t bit = 0x8000;

	row_offset -= 16;
	read_offset = row_offset;
	mod_vector = tfc_getbits(data, read_offset, 16);
	snprintf(mod_str,
		 TFC_MOD_STRING_LENGTH,
		 "\nModify Record: Vector:0x%08x\n", mod_vector);

	for (vect = 15; vect >= 0; vect--) {
		if (mod_vector & bit) {
			snprintf(str, TFC_STRING_LENGTH_256, "%s: ", mod_data[vect].name);
			strcat(mod_str, str);

			for (i = 0; i < mod_data[vect].num_fields; i++) {
				row_offset -= mod_data[vect].field[i].num_bits;
				if (row_offset < 0) {
					row++;
					row_offset = 64 + row_offset;
				}
				read_offset = row_offset + (row * 64);

				for (j = 0; j < mod_data[vect].field[i].num_bits / 32; j++) {
					val[j] = tfc_getbits(data, read_offset, 32);
					read_offset -= 32;
				}

				if (mod_data[vect].field[i].num_bits % 32) {
					val[j] = tfc_getbits(data,
							     read_offset,
						     (mod_data[vect].field[i].num_bits % 32));
					j++;
				}

				snprintf(str,
					 TFC_STRING_LENGTH_256,
					 "%s:0x",
					 mod_data[vect].field[i].name);
				strcat(mod_str, str);

				switch (mod_data[vect].field[i].num_bits) {
				case 128:
					for (k = 0; k < 8; k++) {
						snprintf(str,
							 TFC_STRING_LENGTH_256,
							 "%08x",
							 val[k]);
						strcat(mod_str, str);
					}
					break;
				case 48:
					snprintf(str, TFC_STRING_LENGTH_256, "%08x", val[0]);
					strcat(mod_str, str);
					snprintf(str,
						 TFC_STRING_LENGTH_256,
						 "%04x",
						 (val[1] & 0xffff));
					strcat(mod_str, str);
					break;
				case 32:
					snprintf(str, TFC_STRING_LENGTH_256, "%08x ", val[0]);
					strcat(mod_str, str);
					break;
				case 16:
					snprintf(str, TFC_STRING_LENGTH_256, "%04x ", val[0]);
					strcat(mod_str, str);
					break;
				default:
					snprintf(str, TFC_STRING_LENGTH_256, "%04x ",
						 (val[0] &
						  ((1 << mod_data[vect].field[i].num_bits) - 1)));
					strcat(mod_str, str);
					break;
				}
			}

			snprintf(str, TFC_STRING_LENGTH_256, "\n");
			strcat(mod_str, str);
		}

		bit = bit >> 1;
	}

	snprintf(str, TFC_STRING_LENGTH_256, "\n");
	strcat(mod_str, str);
}

static void enc_decode(uint32_t *data, char *enc_str)
{
	uint16_t vector;
	char str[64];
	uint32_t val[16];
	uint32_t offset = 0;
	uint8_t vtag;
	uint8_t l2;
	uint8_t l3;
	uint8_t l4;
	uint8_t tunnel;

	vector = tfc_getbits(data, offset, 16);
	offset += 16;

	vtag = ((vector >> 2) & 0xf);
	l2 = ((vector >> 6) & 0x1);
	l3 = ((vector >> 7) & 0x7);
	l4 = ((vector >> 10) & 0x7);
	tunnel = ((vector >> 13) & 0x7);

	snprintf(enc_str,
		 TFC_ENC_STRING_LENGTH,
		 "Encap Record: vector:0x%04x\n", vector);

	snprintf(str, TFC_STRING_LENGTH_64,
		 "Valid:%d EC:%d VTAG:0x%01x L2:%d L3:0x%01x L4:0x%01x Tunnel:0x%01x\n",
		 (vector & 0x1),
		 ((vector >> 1) & 0x1),
		 vtag,
		 l2,
		 l3,
		 l4,
		 tunnel);

	strcat(enc_str, str);

	if (l2) { /* L2 */
		snprintf(str, TFC_STRING_LENGTH_64, "L2:\n");
		strcat(enc_str, str);

		val[0] = tfc_getbits(data, offset, 32);
		offset += 32;
		val[1] = tfc_getbits(data, offset, 16);
		offset += 16;

		snprintf(str, TFC_STRING_LENGTH_64, "DMAC:0x%08x%04x\n", val[0], val[1]);
		strcat(enc_str, str);
	}

	if (l3) { /* L3 */
		snprintf(str, TFC_STRING_LENGTH_64, "L3:\n");
		strcat(enc_str, str);
	}

	if (l4) { /* L4 */
		snprintf(str, TFC_STRING_LENGTH_64, "L4:\n");
		strcat(enc_str, str);
	}

	if (tunnel) { /* Tunnel */
		snprintf(str, TFC_STRING_LENGTH_64, "Tunnel:\n");
		strcat(enc_str, str);
	}
}

static void act_decode(uint32_t *act_ptr,
		       uintptr_t base,
		       struct act_info_t *act_info)
{
	if (!act_ptr || !act_info) {
		PMD_DRV_LOG_LINE(ERR, "act_ptr %p, act_info %p", act_ptr, act_info);
		return;
	}

	act_info->valid = false;
	act_info->vector = tfc_getbits(act_ptr, 0, 3);

	if (act_info->vector == 0 ||
	    act_info->vector == 1 ||
	    act_info->vector == 4)
		act_info->valid = true;

	switch (act_info->vector) {
	case 0:
		act_info->compact.drop = tfc_getbits(act_ptr, 3, 1);
		act_info->compact.vlan_del_rep = tfc_getbits(act_ptr, 4, 2);
		act_info->compact.vnic_vport = tfc_getbits(act_ptr, 6, 11);
		act_info->compact.dest_op = tfc_getbits(act_ptr, 17, 2);
		act_info->compact.decap_func = tfc_getbits(act_ptr, 19, 5);
		act_info->compact.mirror = tfc_getbits(act_ptr, 24, 5);
		act_info->compact.meter_ptr = tfc_getbits(act_ptr, 29, 10);
		act_info->compact.stat0_offs = tfc_getbits(act_ptr, 39, 3);
		act_info->compact.stat0_ing_egr = tfc_getbits(act_ptr, 42, 1);
		act_info->compact.stat0_ctr_type = tfc_getbits(act_ptr, 43, 2);
		act_info->compact.mod_offs = tfc_getbits(act_ptr, 45, 5);
		act_info->compact.enc_offs = tfc_getbits(act_ptr, 50, 6);
		act_info->compact.src_offs = tfc_getbits(act_ptr, 56, 4);

		if (act_info->compact.mod_offs) {
			mod_decode((uint32_t *)(act_ptr + (act_info->compact.mod_offs <<  3)),
				   act_info->compact.mod_str);
		}
		if (act_info->compact.stat0_offs)
			stat_decode(act_info->compact.stat0_str,
				    0,
				    act_info->compact.stat0_ctr_type,
				    (uint32_t *)(act_ptr + (act_info->compact.stat0_offs <<  3)));
		if (act_info->compact.enc_offs)
			enc_decode((uint32_t *)(act_ptr + (act_info->compact.enc_offs <<  3)),
				   act_info->compact.enc_str);
	break;
	case 1:
		act_info->full.drop = tfc_getbits(act_ptr, 3, 1);
		act_info->full.vlan_del_rep = tfc_getbits(act_ptr, 4, 2);
		act_info->full.vnic_vport = tfc_getbits(act_ptr, 6, 11);
		act_info->full.dest_op = tfc_getbits(act_ptr, 17, 2);
		act_info->full.decap_func = tfc_getbits(act_ptr, 19, 5);
		act_info->full.mirror = tfc_getbits(act_ptr, 24, 5);
		act_info->full.meter_ptr = tfc_getbits(act_ptr, 29, 10);
		act_info->full.stat0_ptr = tfc_getbits(act_ptr, 39, 28);
		act_info->full.stat0_ing_egr = tfc_getbits(act_ptr, 67, 1);
		act_info->full.stat0_ctr_type = tfc_getbits(act_ptr, 68, 2);
		act_info->full.stat1_ptr = tfc_getbits(act_ptr, 70, 28);
		act_info->full.stat1_ing_egr = tfc_getbits(act_ptr, 98, 1);
		act_info->full.stat1_ctr_type = tfc_getbits(act_ptr, 99, 2);
		act_info->full.mod_ptr = tfc_getbits(act_ptr, 101, 28);
		act_info->full.enc_ptr = tfc_getbits(act_ptr, 129, 28);
		act_info->full.src_ptr = tfc_getbits(act_ptr, 157, 28);

		if (act_info->full.mod_ptr)
			mod_decode((uint32_t *)(base + (act_info->full.mod_ptr <<  3)),
				   act_info->full.mod_str);
		if (act_info->full.stat0_ptr)
			stat_decode(act_info->full.stat0_str,
				    0,
				    act_info->full.stat0_ctr_type,
				    (uint32_t *)(base + (act_info->full.stat0_ptr <<  3)));
		if (act_info->full.stat1_ptr)
			stat_decode(act_info->full.stat1_str,
				    1,
				    act_info->full.stat1_ctr_type,
				    (uint32_t *)(base + (act_info->full.stat1_ptr <<  3)));
		if (act_info->full.enc_ptr)
			enc_decode((uint32_t *)(base + (act_info->full.enc_ptr <<  3)),
				   act_info->full.enc_str);
	break;
	case 4:
		act_info->mcg.nxt_ptr = tfc_getbits(act_ptr, 6, 26);
		act_info->mcg.act_hint0    = tfc_getbits(act_ptr, 32, 2);
		act_info->mcg.act_rec_ptr0 = tfc_getbits(act_ptr, 34, 26);
		act_info->mcg.act_hint1    = tfc_getbits(act_ptr, 60, 2);
		act_info->mcg.act_rec_ptr1 = tfc_getbits(act_ptr, 62, 26);
		act_info->mcg.act_hint2    = tfc_getbits(act_ptr, 88, 2);
		act_info->mcg.act_rec_ptr2 = tfc_getbits(act_ptr, 90, 26);
		act_info->mcg.act_hint3    = tfc_getbits(act_ptr, 116, 2);
		act_info->mcg.act_rec_ptr3 = tfc_getbits(act_ptr, 118, 26);
		act_info->mcg.act_hint4    = tfc_getbits(act_ptr, 144, 2);
		act_info->mcg.act_rec_ptr4 = tfc_getbits(act_ptr, 146, 26);
		act_info->mcg.act_hint5    = tfc_getbits(act_ptr, 172, 2);
		act_info->mcg.act_rec_ptr5 = tfc_getbits(act_ptr, 174, 26);
		act_info->mcg.act_hint6    = tfc_getbits(act_ptr, 200, 2);
		act_info->mcg.act_rec_ptr6 = tfc_getbits(act_ptr, 202, 26);
		act_info->mcg.act_hint7    = tfc_getbits(act_ptr, 228, 2);
		act_info->mcg.act_rec_ptr7 = tfc_getbits(act_ptr, 230, 26);
		break;
	}
}

void act_show(FILE *fd, struct act_info_t *act_info, uint32_t offset)
{
	if (act_info->valid) {
		switch (act_info->vector) {
		case 0:
			fputs("Compact Action Record\n", fd);
			fputs("+----------+--+-+--+--+-----+--+-+------+---Stat0"
			      "---+------+------+------+\n", fd);
			fputs("|   Index  |V |d|dr|do|vn/p |df|m| mtp  |ct|ie| offs|"
			      " moffs| eoffs| soffs|\n", fd);
			fputs("+----------+--+-+--+--+-----+--+-+------+--+--+-----+"
			      "------+------+------+\n", fd);

			fprintf(fd,
	" 0x%08x %2d %d %2d %2d 0x%03x %2d %d 0x%04x %2d %2d  0x%02x   0x%02x   0x%02x   0x%02x\n",
				offset,
				act_info->vector,
				act_info->compact.drop,
				act_info->compact.vlan_del_rep,
				act_info->compact.dest_op,
				act_info->compact.vnic_vport,
				act_info->compact.decap_func,
				act_info->compact.mirror,
				act_info->compact.meter_ptr,
				act_info->compact.stat0_ctr_type,
				act_info->compact.stat0_ing_egr,
				act_info->compact.stat0_offs,
				act_info->compact.mod_offs,
				act_info->compact.enc_offs,
				act_info->compact.src_offs);

		if (act_info->compact.mod_offs)
			fprintf(fd, "%s", act_info->compact.mod_str);
		if (act_info->compact.stat0_offs)
			fprintf(fd, "%s", act_info->compact.stat0_str);
		if (act_info->compact.enc_offs)
			fprintf(fd, "%s", act_info->compact.enc_str);
		break;
		case 1:
		fputs("Full Action Record\n", fd);
		fputs("+----------+--+-+--+--+-----+--+-+------+----Stat0-------+------Stat1-----+----------+----------+----------+\n",
		      fd);
		fputs("|   Index  |V |d|dr|do|vn/p |df|m| mtp  |ct|ie|    ptr   |ct|ie|    ptr   |   mptr   |   eptr   |   sptr   |\n",
		      fd);
		fputs("+----------+--+-+--+--+-----+--+-+------+--+--+----------+--+--+----------+----------+----------+----------+\n",
		      fd);

		fprintf(fd, " 0x%08x %2d %d %2d %2d 0x%03x %2d %d 0x%04x %2d %2d 0x%08x %2d %2d 0x%08x 0x%08x 0x%08x 0x%08x\n",
			offset,
			act_info->vector,
			act_info->full.drop,
			act_info->full.vlan_del_rep,
			act_info->full.dest_op,
			act_info->full.vnic_vport,
			act_info->full.decap_func,
			act_info->full.mirror,
			act_info->full.meter_ptr,
			act_info->full.stat0_ctr_type,
			act_info->full.stat0_ing_egr,
			act_info->full.stat0_ptr,
			act_info->full.stat1_ctr_type,
			act_info->full.stat1_ing_egr,
			act_info->full.stat1_ptr,
			act_info->full.mod_ptr,
			act_info->full.enc_ptr,
			act_info->full.src_ptr);
		if (act_info->full.mod_ptr)
			fprintf(fd, "%s", act_info->full.mod_str);
		if (act_info->full.stat0_ptr)
			fprintf(fd, "%s", act_info->full.stat0_str);
		if (act_info->full.stat1_ptr)
			fprintf(fd, "%s", act_info->full.stat1_str);
		if (act_info->full.enc_ptr)
			fprintf(fd, "%s", act_info->full.enc_str);

		break;
		case 4:
			fputs("Multicast Group Record\n", fd);
			fputs("+----------+--+----------+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+\n",
			      fd);
			fputs("|   Index  |V |  NxtPtr  | ActRPtr0 |ah| ActRPtr1 |ah| ActRPtr2 |ah| ActRPtr3 |ah| ActRPtr4 |ah| ActRPtr5 |ah| ActRPtr6 |ah| ActRPtr7 |ah|\n",
			      fd);
			fputs("+----------+--+----------+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+\n",
			      fd);

		fprintf(fd, " 0x%08x %2d 0x%08x 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d\n",
			offset,
			act_info->vector,
			act_info->mcg.nxt_ptr,
			act_info->mcg.act_rec_ptr0,
			act_info->mcg.act_hint0,
			act_info->mcg.act_rec_ptr1,
			act_info->mcg.act_hint1,
			act_info->mcg.act_rec_ptr2,
			act_info->mcg.act_hint2,
			act_info->mcg.act_rec_ptr3,
			act_info->mcg.act_hint3,
			act_info->mcg.act_rec_ptr4,
			act_info->mcg.act_hint4,
			act_info->mcg.act_rec_ptr5,
			act_info->mcg.act_hint5,
			act_info->mcg.act_rec_ptr6,
			act_info->mcg.act_hint6,
			act_info->mcg.act_rec_ptr7,
			act_info->mcg.act_hint7);
			break;
		}
	}
}

struct stat_fields_s {
	uint64_t pkt_cnt;
	uint64_t byte_cnt;
	union {
		struct {
			uint32_t timestamp;
			uint16_t tcp_flags;
		} c_24b;
		struct {
			uint64_t meter_pkt_cnt;
			uint64_t meter_byte_cnt;
		} c_32b;
		struct {
			uint64_t timestamp : 32;
			uint64_t tcp_flags : 16;
			uint64_t meter_pkt_cnt : 38;
			uint64_t meter_byte_cnt : 42;
		} c_32b_all;
	} t;
};

#define STATS_COMMON_FMT    \
	"Stats:%d Pkt count:%016" PRIu64 " Byte count:%016" PRIu64 "\n"
#define STATS_METER_FMT     \
	"\tMeter pkt count:%016" PRIu64 " Meter byte count:%016" PRIu64 "\n"
#define STATS_TCP_FLAGS_FMT \
	"\tTCP flags:0x%04x timestamp:0x%08x\n"

static void stat_decode(char *str,
			uint8_t stat_num,
			uint8_t stat_ctr_type,
			uint32_t *stat_ptr)
{
	struct stat_fields_s *stats = (struct stat_fields_s *)stat_ptr;
	uint64_t meter_pkt_cnt;
	uint64_t meter_byte_cnt;
	uint32_t timestamp;
	char tmp0[96];

	/* Common fields */
	snprintf(str,
		 TFC_STAT_STRING_LENGTH,
		 STATS_COMMON_FMT,
		 stat_num, stats->pkt_cnt, stats->byte_cnt);

	switch (stat_ctr_type) {
	case CFA_BLD_STAT_COUNTER_SIZE_16B:
		/* Nothing further to do */
		break;
	case CFA_BLD_STAT_COUNTER_SIZE_24B:
		timestamp = stats->t.c_24b.timestamp;
		snprintf(tmp0,
			 TFC_STRING_LENGTH_96,
			 STATS_TCP_FLAGS_FMT,
			 stats->t.c_24b.tcp_flags,
			 timestamp);
		strcat(str, tmp0);
		break;
	case CFA_BLD_STAT_COUNTER_SIZE_32B:
		snprintf(tmp0,
			 TFC_STRING_LENGTH_96,
			 STATS_METER_FMT,
			 stats->t.c_32b.meter_pkt_cnt,
			 stats->t.c_32b.meter_byte_cnt);
		strcat(str, tmp0);
		break;
	case CFA_BLD_STAT_COUNTER_SIZE_32B_ALL:
		meter_pkt_cnt = stats->t.c_32b_all.meter_pkt_cnt;
		meter_byte_cnt = stats->t.c_32b_all.meter_byte_cnt;
		timestamp = stats->t.c_32b_all.timestamp;
		snprintf(tmp0,
			 TFC_STRING_LENGTH_96,
			 STATS_METER_FMT STATS_TCP_FLAGS_FMT,
			 meter_pkt_cnt,
			 meter_byte_cnt,
			 stats->t.c_32b_all.tcp_flags,
			 timestamp);
		strcat(str, tmp0);
		break;
	default:
		       /* Should never happen since type is 2 bits in size */
		snprintf(tmp0,
			 TFC_STRING_LENGTH_96,
			 "Unknown counter type %d\n", stat_ctr_type);
		strcat(str, tmp0);
		break;
	}
}

static void bucket_decode(uint32_t *bucket_ptr,
			  struct bucket_info_t *bucket_info,
			  struct tfc_ts_mem_cfg *lkup_mem_cfg,
			  struct tfc_ts_mem_cfg *act_mem_cfg)
{
	int i;
	int offset = 0;
	uint8_t *em_ptr;

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

			em_ptr = (uint8_t *)get_address(lkup_mem_cfg,
							bucket_info->entries[i].entry_ptr * 32);
			em_decode((uint32_t *)em_ptr, &bucket_info->em_info[i], act_mem_cfg);
		}
	}
}

static void bucket_show(FILE *fd, struct bucket_info_t *bucket_info, uint32_t offset)
{
	int i;

	if (bucket_info->valid) {
		fprintf(fd, "Static Bucket:0x%08x\n", offset);
		fputs("+-+ +---------+ +----------------------------------- Entries --------------------------------------------------------------+\n",
		      fd);
		fputs(" C     CPtr     0                 1                 2                 3                 4                 5\n",
		      fd);
		fputs("+-+ +---------+ +-----+---------+ +-----+---------+ +-----+---------+ +-----+---------+ +-----+---------+ +------+---------+\n",
		      fd);
		fprintf(fd, " %d   0x%07x",
			   bucket_info->chain,
			   bucket_info->chain_ptr);
		for (i = 0; i < TFC_BUCKET_ENTRIES; i++) {
			fprintf(fd, "   0x%03x 0x%07x",
				   bucket_info->entries[i].hash_msb,
				   bucket_info->entries[i].entry_ptr);
		}
		fputs("\n", fd);

		/*
		 * Now display each valid EM entry from the bucket
		 */
		for (i = 0; i < TFC_BUCKET_ENTRIES; i++) {
			if (bucket_info->entries[i].entry_ptr != 0) {
				if (bucket_info->em_info[i].valid)
					em_show(fd, &bucket_info->em_info[i]);
				else
					fputs("<<< Invalid LREC  >>>\n", fd);
			}
		}

		fputs("\n", fd);
	}
}

int tfc_em_show(FILE *fd, struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir)
{
	int rc = 0;
	enum cfa_scope_type scope_type;
	bool is_bs_owner;
	struct tfc_ts_mem_cfg *lkup_mem_cfg;
	struct tfc_ts_mem_cfg *act_mem_cfg;
	uint32_t bucket_row;
	uint32_t bucket_count;
	uint8_t *bucket_ptr;
	struct bucket_info_t *bucket_info;
	uint32_t bucket_offset = 0;
	bool valid;

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc != 0) {
		fprintf(fd, "%s: failed to get tsid: %d\n",
			   __func__, rc);
		return -EINVAL;
	}
	if (!valid) {
		fprintf(fd, "%s: tsid not allocated %d\n",
			   __func__, tsid);
		return -EINVAL;
	}

	lkup_mem_cfg = rte_zmalloc("data", sizeof(*lkup_mem_cfg), 8);
	if (!lkup_mem_cfg)
		return -ENOMEM;

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				dir,
				CFA_REGION_TYPE_LKUP,
				&is_bs_owner,
				lkup_mem_cfg);   /* Gets rec_cnt */
	if (rc != 0) {
		fprintf(fd, "%s: tfo_ts_get_mem_cfg() failed for LKUP: %d\n",
			   __func__, rc);
		rte_free(lkup_mem_cfg);
		return -EINVAL;
	}

	act_mem_cfg = rte_zmalloc("data", sizeof(*act_mem_cfg), 8);
	if (!act_mem_cfg) {
		rte_free(lkup_mem_cfg);
		return -ENOMEM;
	}

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				dir,
				CFA_REGION_TYPE_ACT,
				&is_bs_owner,
				act_mem_cfg);   /* Gets rec_cnt */
	if (rc != 0) {
		fprintf(fd, "%s: tfo_ts_get_mem_cfg() failed for ACT: %d\n",
			   __func__, rc);
		rte_free(lkup_mem_cfg);
		rte_free(act_mem_cfg);
		return -EINVAL;
	}

	bucket_count = lkup_mem_cfg->lkup_rec_start_offset;

	fputs(" Lookup Table\n", fd);
	fprintf(fd, " Static bucket count:%d\n", bucket_count);

	bucket_info = rte_zmalloc("data", sizeof(*bucket_info), 8);
	if (!bucket_info) {
		fprintf(fd, "%s: Failed to allocate bucket info struct\n",
			   __func__);
		rte_free(lkup_mem_cfg);
		rte_free(act_mem_cfg);
		return -ENOMEM;
	}

	/*
	 * Go through the static buckets looking for valid entries.
	 * If a valid entry is found then  display it and also display
	 * the EM entries it points to.
	 */
	for (bucket_row = 0; bucket_row < bucket_count; ) {
		bucket_ptr = (uint8_t *)get_address(lkup_mem_cfg, bucket_offset);
		bucket_decode((uint32_t *)bucket_ptr, bucket_info, lkup_mem_cfg, act_mem_cfg);

		if (bucket_info->valid)
			bucket_show(fd, bucket_info, bucket_offset);

		bucket_offset += TFC_BUCKET_SIZE_BYTES;
		bucket_row++;
	}

	rte_free(bucket_info);
	rte_free(lkup_mem_cfg);
	rte_free(act_mem_cfg);

	return rc;
}

void tfc_backing_store_dump(FILE *fd)
{
	uint16_t port;
	struct bnxt *bp;
	uint8_t tsid;
	int dir;

	RTE_ETH_FOREACH_DEV(port) {
		bp = bnxt_pmd_get_bp(port);
		if (bp && !tfo_tsid_get(bp->tfcp.tfo, &tsid)) {
			for (dir = 0; dir <= 1; dir++) {
				fputs("+------------------- EM ------------------+\n", fd);
				fprintf(fd, "+--------- Port:%d TSID:%d DIR:%s ----------+\n",
					port, tsid, (dir == CFA_DIR_RX ? "rx" : "tx"));
				fputs("+-----------------------------------------+\n", fd);
				tfc_em_show(fd, &bp->tfcp, tsid, dir);

				fputs("+------------------- WC ------------------+\n", fd);
				fprintf(fd, "+--------- Port:%d TSID:%d DIR:%s ----------+\n",
					port, tsid, (dir == CFA_DIR_RX ? "rx" : "tx"));
				fputs("+-----------------------------------------+\n", fd);
				tfc_wc_show(fd, &bp->tfcp, tsid, dir);
			}
		}
	}
}
