/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include "bnxt.h"
#include "bnxt_ulp_tfc.h"
#include "tfc.h"
#include "cfa_p70.h"
#include "tfc_resources.h"
#include "tfc_msg.h"

#if (TFC_V3_RESOURCE_API_ENABLE == 1)
int tfc_resource_types_query(struct tfc *tfcp,
			     struct tfc_resources *resources)
{
	int rc = 0;
	/* TODO: implement API */
	return rc;
}
#endif /* (TFC_V3_RESOURCE_API_ENABLE == 1) */

#ifdef TF_FLOW_SCALE_QUERY
/****************************************************************************************
 * TruFlow Debug Feature: Flow scale query
 * It is deisabled by default and to be enabled with build option -DTF_FLOW_SCALE_QUERY
 ****************************************************************************************/

/**
 * CFA TCAM instance statistics information
 */
struct cfa_tcm_stats_info {
	/* [out] Maximum number of TCAM entries */
	uint16_t max_slices;
	/* [out[ number of partially used 1-slice rows */
	uint16_t row_1_slice_p_used;
	/* [out] number of fully used 1-slice rows */
	uint16_t row_1_slice_f_used;
	/* [out] number of partially used 2-slice rows */
	uint16_t row_2_slice_p_used;
	/* [out] number of fully used 2-slice rows */
	uint16_t row_2_slice_f_used;
	/* [out] number of fully used 4-slice rows */
	uint16_t row_4_slice_used;
	/* [out] number of unused rows */
	uint16_t unused_row_number;
	/* [out] number of used slices */
	uint16_t used_slices_number;
};

/* Query TF resource usage state with firmware */
static int
tfc_query_resc_usage(struct tfc *tfcp, enum cfa_dir dir)
{
	int rc = 0;
	uint16_t sid;
	struct cfa_tcm_stats_info stats_info;
	uint16_t data_size = sizeof(stats_info);

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to retrieve SID, rc:%s\n",
				 strerror(-rc));
		return rc;
	}
	tfc_msg_resc_usage_query(tfcp, sid, dir, &data_size, &stats_info);
	PMD_DRV_LOG_LINE(ERR,
			 "dir:%s, %d - %d %d(1-slice) - %d %d(2-slices) - %d(4-slices) - %d(unused row) - %d(used_slices)\n",
			 dir ? "TX" : "RX", stats_info.max_slices,
			 stats_info.row_1_slice_p_used,
			 stats_info.row_1_slice_f_used,
			 stats_info.row_2_slice_p_used,
			 stats_info.row_2_slice_f_used,
			 stats_info.row_4_slice_used,
			 stats_info.unused_row_number,
			 stats_info.used_slices_number);

	return 0;
}

/* query all resource usage state with firmware for both direction */
void tfc_resc_usage_query_all(struct bnxt *bp)
{
	struct tfc *tfcp;
	enum cfa_dir dir;

	tfcp = bnxt_ulp_cntxt_tfcp_get(bp->ulp_ctx);
	if (!tfcp) {
		BNXT_DRV_DBG(ERR, "Failed to get truflow pointer\n");
		return;
	}

	/* query usage state with firmware for each direction */
	for (dir = 0; dir < CFA_DIR_MAX; dir++)
		tfc_query_resc_usage(tfcp, dir);
}

#endif /* TF_FLOW_SCALE_QUERY */
