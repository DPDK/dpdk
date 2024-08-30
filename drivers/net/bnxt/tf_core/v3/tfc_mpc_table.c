/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

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
	uint32_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_READ_CMD_MAX_FLD];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_READ_CMP_MAX_FLD];
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	bool is_shared;
	struct cfa_bld_mpcinfo *mpc_info;
	uint64_t host_address;
	uint8_t discard_data[128];
	uint32_t set;
	uint32_t way;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	rc = tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, &valid, NULL);
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
	uint32_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_WRITE_CMD_MAX_FLD];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_WRITE_CMP_MAX_FLD];
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct cfa_bld_mpcinfo *mpc_info;
	bool is_shared;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	rc = tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, &valid, NULL);
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
		printf("Hash MSB:0x%0x\n",
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
	uint32_t msg_count = BNXT_MPC_COMP_MSG_COUNT;
	int i;
	uint32_t buff_len;
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_INVALIDATE_CMD_MAX_FLD];
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_INVALIDATE_CMP_MAX_FLD];
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct cfa_bld_mpcinfo *mpc_info;
	bool is_shared;
	bool valid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	rc = tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, &valid, NULL);
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
		printf("Hash MSB:0x%0x\n",
		       (uint32_t)fields_cmp[CFA_BLD_MPC_INVALIDATE_CMP_HASH_MSB_FLD].val);
		goto cleanup;
	}

	return 0;

 cleanup:

	return rc;
}

#define TFC_ACTION_SIZE_BYTES  32
#define TFC_BUCKET_SIZE_BYTES  32

struct act_full_info_t {
	bool drop;
	uint8_t vlan_del_rep;
	uint8_t dest_op;
	uint16_t vnic_vport;
	uint8_t decap_func;
	uint16_t mirror;
	uint16_t meter_ptr;
	uint8_t stat0_ctr_type;
	bool stat0_ing_egr;
	uint32_t stat0_ptr;
	uint8_t stat1_ctr_type;
	bool stat1_ing_egr;
	uint32_t stat1_ptr;
	uint32_t mod_ptr;
	uint32_t enc_ptr;
	uint32_t src_ptr;
	char mod_str[512];
};

struct act_mcg_info_t {
	uint8_t src_ko_en;
	uint32_t nxt_ptr;
	uint8_t act_hint0;
	uint32_t act_rec_ptr0;
	uint8_t act_hint1;
	uint32_t act_rec_ptr1;
	uint8_t act_hint2;
	uint32_t act_rec_ptr2;
	uint8_t act_hint3;
	uint32_t act_rec_ptr3;
	uint8_t act_hint4;
	uint32_t act_rec_ptr4;
	uint8_t act_hint5;
	uint32_t act_rec_ptr5;
	uint8_t act_hint6;
	uint32_t act_rec_ptr6;
	uint8_t act_hint7;
	uint32_t act_rec_ptr7;
};

struct act_info_t {
	bool valid;
	uint8_t vector;
	union {
		struct act_full_info_t full;
		struct act_mcg_info_t mcg;
	};
};

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

static uint64_t get_address(struct tfc_ts_mem_cfg *mem, uint32_t offset)
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

static void em_decode(uint32_t *em_ptr, struct em_info_t *em_info)
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

static void em_show(struct em_info_t *em_info)
{
	int i;
	char line1[256];
	char line2[256];
	char line3[256];
	char line4[256];
	char tmp1[256];
	char tmp2[256];
	char tmp3[256];
	char tmp4[256];

	printf(":LREC: opcode:%s\n", opcode_string[em_info->opcode]);

	sprintf(line1, "+-+--+-Epoch-+--+--+--+");
	sprintf(line2, " V|rs|  0  1 |Op|St|ah|");
	sprintf(line3, "+-+--+----+--+--+--+--+");
	sprintf(line4, " %1d %2d %4d %2d %2d %2d %2d ",
		em_info->valid,
		em_info->rec_size,
		em_info->epoch0,
		em_info->epoch1,
		em_info->opcode,
		em_info->strength,
		em_info->act_hint);

	if (em_info->opcode != 2 && em_info->opcode != 3) {
		/* All but FAST */
		sprintf(tmp1, "-Act Rec--+");
		sprintf(tmp2, " Ptr      |");
		sprintf(tmp3, "----------+");
		sprintf(tmp4, "0x%08x ",
		       em_info->act_rec_ptr);
	} else {
		/* Just FAST */
		sprintf(tmp1, "-------+");
		sprintf(tmp2, " Dest  |");
		sprintf(tmp3, "-------+");
		sprintf(tmp4, "0x05%x ",
		       em_info->destination);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	if (em_info->opcode == 4 || em_info->opcode == 6) {
		/* CT only */
		sprintf(tmp1, "--+--+-------------TCP-------+--+---+");
		sprintf(tmp2, "Dr|ue| Win|   lc  |   op  |oi|st|tmr|");
		sprintf(tmp3, "--+--+----+-------+-------+--+--+---+");
		sprintf(tmp4, "%2d %2d %4d %0x5x %0x5x %2d %2d %3d ",
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
		sprintf(tmp1, "--+--+--+-------FC-------+");
		sprintf(tmp2, "RI|as|pm|op|tp|     Ptr  |");
		sprintf(tmp3, "--+--+--+--+--+----------+");
		sprintf(tmp4, "%2d %2d %2d %2d %2d 0x%08x ",
			em_info->ring_table_idx,
			em_info->act_rec_size,
			em_info->paths_m1,
			em_info->fc_op,
			em_info->fc_type,
			em_info->fc_ptr);
	} else {
		sprintf(tmp1, "--+--+--+---------+");
		sprintf(tmp2, "RD|pf|mp| cMData  |");
		sprintf(tmp3, "--+--+--+---------+");
		sprintf(tmp4, "%2d 0x%2x %2d %08x ",
			em_info->recycle_dest,
			em_info->prof_func,
			em_info->meta_prof,
			em_info->metadata);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	sprintf(tmp1, "-----Range-+\n");
	sprintf(tmp2, "Prof|  Idx |\n");
	sprintf(tmp3, "----+------+\n");
	sprintf(tmp4, "0x%02x 0x%04x\n",
		em_info->range_profile,
		em_info->range_index);

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	printf("%s%s%s%s",
	       line1,
	       line2,
	       line3,
	       line4);

	printf("Key:");
	for (i = 0; i < ((em_info->rec_size + 1) * 32); i++) {
		if (i % 32 == 0)
			printf("\n%04d:  ", i);
		printf("%02x", em_info->key[i]);
	}
	i = ((em_info->rec_size + 1) * 32);
	printf("\nKey Reversed:\n%04d:  ", i - 32);
	do {
		i--;
		printf("%02x", em_info->key[i]);
		if (i != 0 && i % 32 == 0)
			printf("\n%04d:  ", i - 32);
	} while (i > 0);
	printf("\n");
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
	{1, "Replace:", {{16,  "DPort"} } },
	{1, "Replace:", {{16,  "SPort"} } },
	{1, "Replace:", {{32,  "IPv4 DIP"} } },
	{1, "Replace:", {{32,  "IPv4 SIP"} } },
	{1, "Replace:", {{128, "IPv6 DIP"} } },
	{1, "Replace:", {{128, "IPv6 SIP"} } },
	{1, "Replace:", {{48,  "SMAC"} } },
	{1, "Replace:", {{48,  "DMAC"} } },
	{2, "Update Field:",  {{16, "uf_vec"}, {32, "uf_data"} } },
	{3, "Tunnel Modify:", {{16, "tun_mv"}, {16, "tun_ex_prot"}, {16, "tun_new_prot"} } },
	{3, "TTL Update:",    {{5,  "alt_pfid"}, {12, "alt_vid"}, {5, "ttl_op"} } },
	{4, "Replace/Add Outer VLAN:", {{16, "tpid"}, {3, "pri"}, {1, "de"}, {12, "vid"} } },
	{4, "Replace/Add Inner:",      {{16, "tpid"}, {3, "pri"}, {1, "de"}, {12, "vid"} } },
	{0, "Remove outer VLAN:", {{0, NULL} } },
	{0, "Remove inner VLAN:", {{0, NULL} } },
	{4, "Metadata Update:",   {{2, "md_op"}, {4, "md_prof"}, {10, "rsvd"}, {32, "md_data"} } },
};

static void mod_decode(uint32_t *data, char *mod_str)
{
	int vindex  = 0;
	int i;
	uint32_t word = 0;
	uint16_t mod_vector;
	uint32_t offset = 64;
	uint32_t val;
	char str[128];

	mod_vector = tfc_getbits(data, 48, 16);
	offset -= 16;
	sprintf(mod_str, "Modify Record: Vector:0x%08x\n", mod_vector);

	vindex = log2(mod_vector);
	sprintf(str, "%s: ", mod_data[vindex].name);
	strcat(mod_str, str);
	for (i = 0; i < mod_data[vindex].num_fields; i++)  {
		val = tfc_getbits(data, offset, mod_data[vindex].field[i].num_bits);
		sprintf(str, "%s:%x ", mod_data[vindex].field[i].name, val);
		strcat(mod_str, str);
		offset -= mod_data[vindex].field[i].num_bits;
		if (offset - (word * 64) == 0) {
			word++;
			offset = (64 + (word * 64));
		}
	}

	sprintf(str, "\n");
	strcat(mod_str, str);
}

static void act_decode(uint32_t *act_ptr,
		       uint64_t base,
		       struct act_info_t *act_info)
{
	act_info->valid = false;
	act_info->vector = tfc_getbits(act_ptr, 0, 3);

	if (act_info->vector == 1 ||
	    act_info->vector == 4)
		act_info->valid = true;

	switch (act_info->vector) {
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
			mod_decode((uint32_t *)((uintptr_t)
						(base + (act_info->full.mod_ptr <<  3))),
				    act_info->full.mod_str);
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

static void act_show(struct act_info_t *act_info, uint32_t offset)
{
	if (act_info->valid) {
		switch (act_info->vector) {
		case 1:
		printf("Full Action Record\n");
		printf("+----------+--+-+--+--+-----+--+-+------+----Stat0-------+------Stat1-----+----------+----------+----------+\n");
		printf("|   Index  |V |d|dr|do|vn/p |df|m| mtp  |ct|ie|    ptr   |ct|ie|    ptr   |   mptr   |   eptr   |   sptr   |\n");
		printf("+----------+--+-+--+--+-----+--+-+------+--+--+----------+--+--+----------+----------+----------+----------+\n");

		printf(" 0x%08x %2d %d %2d %2d 0x%03x %2d %d 0x%04x %2d %2d 0x%08x %2d %2d 0x%08x 0x%08x 0x%08x 0x%08x\n",
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
			printf("%s", act_info->full.mod_str);
		break;
		case 4:
		printf("Multicast Group Record\n");
		printf("+----------+--+----------+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+\n");
		printf("|   Index  |V |  NxtPtr  | ActRPtr0 |ah| ActRPtr1 |ah| ActRPtr2 |ah| ActRPtr3 |ah| ActRPtr4 |ah| ActRPtr5 |ah| ActRPtr6 |ah| ActRPtr7 |ah|\n");
		printf("+----------+--+----------+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+----------+--+\n");

		printf(" 0x%08x %2d 0x%08x 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d 0x%08x %2d\n",
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
			uint32_t timestamp __rte_packed;
			uint16_t tcp_flags __rte_packed;
		} c_24b;
		struct {
			uint64_t meter_pkt_cnt;
			uint64_t meter_byte_cnt;
		} c_32b;
		struct {
			uint64_t timestamp : 32 __rte_packed;
			uint64_t tcp_flags : 16 __rte_packed;
			uint64_t meter_pkt_cnt : 38 __rte_packed;
			uint64_t meter_byte_cnt : 42 __rte_packed;
		} c_32b_all;
	} t;
};

#define STATS_COMMON_FMT    \
	"\tPkt count    : 0x%016" PRIu64 ", Byte count    : 0x%016" PRIu64 "\n"
#define STATS_METER_FMT     \
	"\tMeter pkt cnt: 0x%016" PRIx64 ", Meter byte cnt: 0x%016" PRIx64 "\n"
#define STATS_TCP_FLAGS_FMT \
	"\tTCP flags    : 0x%04x, timestamp     : 0x%08x\n"

static void stat_show(uint8_t stat1_ctr_type,
		      void *stat_ptr)
{
	struct stat_fields_s *stats = stat_ptr;
	uint64_t meter_pkt_cnt;
	uint64_t meter_byte_cnt;
	uint32_t timestamp;

	/* Common fields */
	printf(STATS_COMMON_FMT, stats->pkt_cnt, stats->byte_cnt);

	switch (stat1_ctr_type) {
	case CFA_BLD_STAT_COUNTER_SIZE_16B:
		/* Nothing further to do */
		break;
	case CFA_BLD_STAT_COUNTER_SIZE_24B:
		timestamp = stats->t.c_24b.timestamp;
		printf(STATS_TCP_FLAGS_FMT,
		       stats->t.c_24b.tcp_flags,
		       timestamp);
		break;
	case CFA_BLD_STAT_COUNTER_SIZE_32B:
		printf(STATS_METER_FMT,
		       stats->t.c_32b.meter_pkt_cnt,
		       stats->t.c_32b.meter_byte_cnt);
		break;
	case CFA_BLD_STAT_COUNTER_SIZE_32B_ALL:
		meter_pkt_cnt = stats->t.c_32b_all.meter_pkt_cnt;
		meter_byte_cnt = stats->t.c_32b_all.meter_byte_cnt;
		timestamp = stats->t.c_32b_all.timestamp;
		printf(STATS_METER_FMT STATS_TCP_FLAGS_FMT,
		       meter_pkt_cnt,
		       meter_byte_cnt,
		       stats->t.c_32b_all.tcp_flags,
		       timestamp);
		break;
	default:
		       /* Should never happen since type is 2 bits in size */
		printf("Unknown counter type %d\n", stat1_ctr_type);
		break;
	}
}

static void bucket_decode(uint32_t *bucket_ptr,
			  struct bucket_info_t *bucket_info,
			  struct tfc_ts_mem_cfg *mem_cfg)
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

			em_ptr = (uint8_t *)((uintptr_t)
					     get_address(mem_cfg,
							 bucket_info->entries[i].entry_ptr
							 * 32));
			em_decode((uint32_t *)em_ptr, &bucket_info->em_info[i]);
		}
	}
}

static void bucket_show(struct bucket_info_t *bucket_info, uint32_t offset)
{
	int i;

	if (bucket_info->valid) {
		printf("Static Bucket:0x%08x\n", offset);
		printf("+-+ +---------+ +----------------------------------- Entries --------------------------------------------------------------+\n");
		printf(" C     CPtr     0                 1                 2                 3                 4                 5\n");
		printf("+-+ +---------+ +-----+---------+ +-----+---------+ +-----+---------+ +-----+---------+ +-----+---------+ +------+---------+\n");
		printf(" %d   0x%07x",
		       bucket_info->chain,
		       bucket_info->chain_ptr);
		for (i = 0; i < TFC_BUCKET_ENTRIES; i++) {
			printf("   0x%03x 0x%07x",
			       bucket_info->entries[i].hash_msb,
			       bucket_info->entries[i].entry_ptr);
		}
		printf("\n");

		/*
		 * Now display each valid EM entry from the bucket
		 */
		for (i = 0; i < TFC_BUCKET_ENTRIES; i++) {
			if (bucket_info->entries[i].entry_ptr != 0) {
				if (bucket_info->em_info[i].valid)
					em_show(&bucket_info->em_info[i]);
				else
					printf("<<< Invalid LREC  >>>\n");
			}
		}
	}
}

int tfc_em_show(struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir)
{
	int rc = 0;
	bool is_shared;
	bool is_bs_owner;
	struct tfc_ts_mem_cfg mem_cfg;
	uint32_t bucket_row;
	uint32_t bucket_count;
	uint8_t *bucket_ptr;
	struct bucket_info_t bucket_info;
	uint32_t bucket_offset = 0;
	bool valid;

	rc = tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, &valid, NULL);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %d", rc);
		return -EINVAL;
	}
	if (!valid) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				dir,
				CFA_REGION_TYPE_LKUP,
				&is_bs_owner,
				&mem_cfg);   /* Gets rec_cnt */
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %d", rc);
		return -EINVAL;
	}

	bucket_count = mem_cfg.lkup_rec_start_offset;

	printf(" Lookup Table\n");
	printf(" Static bucket count:%d\n", bucket_count);

	/*
	 * Go through the static buckets looking for valid entries.
	 * If a valid entry is found then  display it and also display
	 * the EM entries it points to.
	 */
	for (bucket_row = 0; bucket_row < bucket_count; ) {
		bucket_ptr = (uint8_t *)((uintptr_t)
					 get_address(&mem_cfg, bucket_offset));
		bucket_decode((uint32_t *)bucket_ptr, &bucket_info, &mem_cfg);

		if (bucket_info.valid)
			bucket_show(&bucket_info, bucket_offset);

		bucket_offset += TFC_BUCKET_SIZE_BYTES;
		bucket_row++;
	}

	return rc;
}

int tfc_act_show(struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir)
{
	int rc = 0;
	bool is_shared;
	bool is_bs_owner;
	struct tfc_ts_mem_cfg mem_cfg;
	uint32_t act_row;
	uint32_t act_count;
	uint8_t *act_ptr;
	uint8_t *stat_ptr;
	struct act_info_t act_info;
	uint32_t act_offset = 0;
	uint64_t base;
	bool valid;

	rc = tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, &valid, NULL);
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "failed to get tsid: %s", strerror(-rc));
		return -EINVAL;
	}
	if (!valid) {
		PMD_DRV_LOG_LINE(ERR, "tsid not allocated %d", tsid);
		return -EINVAL;
	}
	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				dir,
				CFA_REGION_TYPE_ACT,
				&is_bs_owner,
				&mem_cfg);   /* Gets rec_cnt */
	if (rc != 0) {
		PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %d", rc);
		return -EINVAL;
	}

	base = get_address(&mem_cfg, 0);
	act_count = mem_cfg.rec_cnt;
	printf(" Action Table\n");

	/*
	 * Go through the action table looking for valid entries.
	 * If a valid entry is found then display it.
	 */
	for (act_row = 0; act_row < act_count; ) {
		act_ptr = (uint8_t *)((uintptr_t)
				      get_address(&mem_cfg, act_offset));
		act_decode((uint32_t *)act_ptr, base,  &act_info);

		if (act_info.valid) {
			act_show(&act_info, act_offset);

			switch (act_info.vector) {
			case 1:
				/*
				 * If there is a stats ptr, it must be non-zero since
				 * the record at offset zero will be taken by the action
				 * record.
				 */
				if (act_info.full.stat0_ptr) {
					uint32_t stats_offset;
					/*
					 * There is a stats ptr, should be next 32B
					 * record.
					 */
					act_offset += TFC_ACTION_SIZE_BYTES;
					act_row++;

					stats_offset = act_offset;
					/* stat0_ptr is an 8B pointer */
					if ((stats_offset >> 3) != act_info.full.stat0_ptr) {
						printf("8B act_offset (0x%x) != stat0_ptr (0x%x)\n",
						       stats_offset >> 3,
						       act_info.full.stat0_ptr);
						continue;
					}
					stat_ptr = (uint8_t *)((uintptr_t)
							       get_address(&mem_cfg, stats_offset));
					stat_show(act_info.full.stat0_ctr_type, stat_ptr);
				}
				break;
			}
		}

		act_offset += TFC_ACTION_SIZE_BYTES;
		act_row++;
	}
	return rc;
}
