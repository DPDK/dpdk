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

struct stat_fields_s {
	uint64_t pkt_cnt;
	uint64_t byte_cnt;
	union {
		struct __rte_packed_begin {
			uint32_t timestamp;
			uint16_t tcp_flags;
		} c_24b __rte_packed_end;
		struct {
			uint64_t meter_pkt_cnt;
			uint64_t meter_byte_cnt;
		} c_32b;
		struct __rte_packed_begin {
			uint64_t timestamp:32;
			uint64_t tcp_flags:16;
			uint64_t meter_pkt_cnt:38;
			uint64_t meter_byte_cnt:42;
		} c_32b_all __rte_packed_end;
	} t;
};

#define STATS_COMMON_FMT    \
	"\tPkt count    : 0x%016" PRIu64 ", Byte count    : 0x%016" PRIu64 "\n"
#define STATS_METER_FMT     \
	"\tMeter pkt cnt: 0x%016" PRIx64 ", Meter byte cnt: 0x%016" PRIx64 "\n"
#define STATS_TCP_FLAGS_FMT \
	"\tTCP flags    : 0x%04x, timestamp     : 0x%08x\n"
