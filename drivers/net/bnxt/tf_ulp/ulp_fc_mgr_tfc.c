/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
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
#include "bnxt_ulp_tfc.h"
#include "bnxt_tf_common.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tfc.h"
#include "tfc_debug.h"
#include "tfc_action_handle.h"

/* Need to create device parms for these values and handle
 * alignment dynamically.
 */
#define ULP_FC_TFC_PKT_CNT_OFFS 0
#define ULP_FC_TFC_BYTE_CNT_OFFS 1
#define ULP_TFC_CNTR_READ_BYTES 32
#define ULP_TFC_CNTR_ALIGN 32
#define ULP_TFC_ACT_WORD_SZ 32

struct ulp_fc_tfc_stats_cache_entry {
	uint32_t flags;
	uint64_t timestamp;
	uint8_t tsid;
	uint32_t record_size;
	uint32_t offset;
	uint8_t dir;
	uint64_t packet_count;
	uint64_t byte_count;
	uint16_t tcp_flags;
	uint32_t tcp_timestamp;
};

static int32_t
ulp_fc_tfc_update_accum_stats(__rte_unused struct bnxt_ulp_context *ctxt,
			      __rte_unused struct bnxt_ulp_fc_info *fc_info,
			      __rte_unused struct bnxt_ulp_device_params *dparms)
{
	/* Accumulation is not supported, just return success */
	return 0;
}

static uint8_t *data;
static uint64_t virt2iova_data;

static int32_t
ulp_fc_tfc_flow_stat_get(struct bnxt_ulp_context *ctxt,
			 uint8_t direction,
			 uint32_t session_type __rte_unused,
			 uint64_t handle,
			 struct rte_flow_query_count *count)
{
	uint16_t data_size = ULP_TFC_CNTR_READ_BYTES;
	struct tfc_cmm_clr cmm_clr = { 0 };
	struct tfc_cmm_info cmm_info;
	uint16_t word_size;
	uint64_t *data64;
	struct tfc *tfcp;
	int32_t rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ctxt);
	if (tfcp == NULL) {
		BNXT_DRV_DBG(ERR, "Failed to get tf object\n");
		return -EINVAL;
	}

	if (data == NULL) {
		data = rte_zmalloc("dma data",
				   ULP_TFC_CNTR_READ_BYTES,
				   ULP_TFC_CNTR_ALIGN);

		if (data == NULL) {
			BNXT_DRV_DBG(ERR, "Failed to allocate dma buffer\n");
			return -EINVAL;
		}

		virt2iova_data = (uint64_t)rte_mem_virt2iova(data);
	}

	/* Ensure that data is large enough to read words */
	word_size = (data_size + ULP_TFC_ACT_WORD_SZ - 1) / ULP_TFC_ACT_WORD_SZ;
	if (word_size * ULP_TFC_ACT_WORD_SZ > data_size) {
		BNXT_DRV_DBG(ERR, "Insufficient size %d for stat get\n",
			     data_size);
		return -EINVAL;
	}

	data64 = (uint64_t *)data;
	cmm_info.rsubtype = CFA_RSUBTYPE_CMM_ACT;
	cmm_info.act_handle = handle;
	cmm_info.dir = (enum cfa_dir)direction;
	/* Read and Clear the hw stat if requested */
	if (count->reset) {
		cmm_clr.clr = true;
		cmm_clr.offset_in_byte = 0;
		cmm_clr.sz_in_byte = sizeof(data64[ULP_FC_TFC_PKT_CNT_OFFS]) +
			sizeof(data64[ULP_FC_TFC_BYTE_CNT_OFFS]);
	}
	rc = tfc_act_get(tfcp, NULL, &cmm_info, &cmm_clr, &virt2iova_data, &word_size);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "Failed to read stat memory hndl=0x%" PRIx64 "\n",
			     handle);
		return rc;
	}
	if (data64[ULP_FC_TFC_PKT_CNT_OFFS]) {
		count->hits_set = 1;
		count->hits = data64[ULP_FC_TFC_PKT_CNT_OFFS];
	}
	if (data64[ULP_FC_TFC_BYTE_CNT_OFFS]) {
		count->bytes_set = 1;
		count->bytes = data64[ULP_FC_TFC_BYTE_CNT_OFFS];
	}

	return rc;
}

const struct bnxt_ulp_fc_core_ops ulp_fc_tfc_core_ops = {
	.ulp_flow_stat_get = ulp_fc_tfc_flow_stat_get,
	.ulp_flow_stats_accum_update = ulp_fc_tfc_update_accum_stats
};
