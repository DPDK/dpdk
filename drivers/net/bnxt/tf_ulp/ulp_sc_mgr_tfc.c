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
#include "ulp_sc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tfc.h"
#include "tfc_debug.h"
#include "tfc_action_handle.h"

static int32_t
ulp_sc_tfc_stats_cache_update(struct tfc *tfcp,
			      int dir,
			      uint64_t *host_address,
			      uint64_t handle,
			      uint16_t *words,
			      struct tfc_mpc_batch_info_t *batch_info,
			      bool reset)
{
	struct tfc_cmm_info cmm_info;
	struct tfc_cmm_clr cmm_clr;
	int rc;

	cmm_info.dir = dir;
	cmm_info.rsubtype = CFA_RSUBTYPE_CMM_ACT;
	cmm_info.act_handle = handle;
	cmm_clr.clr = reset;

	if (reset) {
		cmm_clr.offset_in_byte = 0;
		cmm_clr.sz_in_byte = 16;
	}

	rc = tfc_act_get(tfcp,
			 batch_info,
			 &cmm_info,
			 &cmm_clr,
			 host_address,
			 words);

	return rc;
}


const struct bnxt_ulp_sc_core_ops ulp_sc_tfc_core_ops = {
	.ulp_stats_cache_update = ulp_sc_tfc_stats_cache_update
};
