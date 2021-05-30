/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_malloc.h>

#include "ulp_tun.h"
#include "ulp_rte_parser.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_matcher.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"

/* This function programs the outer tunnel flow in the hardware. */
static int32_t
ulp_install_outer_tun_flow(struct ulp_rte_parser_params *params,
			   struct bnxt_tun_cache_entry *tun_entry,
			   uint16_t tun_idx)
{
	struct bnxt_ulp_mapper_create_parms mparms = { 0 };
	int ret;

	/* Reset the JUMP action bit in the action bitmap as we don't
	 * offload this action.
	 */
	ULP_BITMAP_RESET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_JUMP);

	ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_F1);

	ret = ulp_matcher_pattern_match(params, &params->class_id);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto err;

	ret = ulp_matcher_action_match(params, &params->act_tmpl);
	if (ret != BNXT_TF_RC_SUCCESS)
		goto err;

	params->parent_flow = true;
	bnxt_ulp_init_mapper_params(&mparms, params,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.tun_idx = tun_idx;

	/* Call the ulp mapper to create the flow in the hardware. */
	ret = ulp_mapper_flow_create(params->ulp_ctx, &mparms);
	if (ret)
		goto err;

	/* Store the tunnel dmac in the tunnel cache table and use it while
	 * programming tunnel inner flow.
	 */
	memcpy(tun_entry->t_dmac,
	       &params->hdr_field[ULP_TUN_O_DMAC_HDR_FIELD_INDEX].spec,
	       RTE_ETHER_ADDR_LEN);

	tun_entry->tun_flow_info[params->port_id].state =
				BNXT_ULP_FLOW_STATE_TUN_O_OFFLD;
	tun_entry->outer_tun_flow_id = params->fid;

	/* Tunnel outer flow  and it's related inner flows are correlated
	 * based on Tunnel Destination IP Address.
	 */
	if (tun_entry->t_dst_ip_valid)
		goto done;
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_IPV4))
		memcpy(&tun_entry->t_dst_ip,
		       &params->hdr_field[ULP_TUN_O_IPV4_DIP_INDEX].spec,
		       sizeof(rte_be32_t));
	else
		memcpy(tun_entry->t_dst_ip6,
		       &params->hdr_field[ULP_TUN_O_IPV6_DIP_INDEX].spec,
		       sizeof(tun_entry->t_dst_ip6));
	tun_entry->t_dst_ip_valid = true;

done:
	return BNXT_TF_RC_FID;

err:
	memset(tun_entry, 0, sizeof(struct bnxt_tun_cache_entry));
	return BNXT_TF_RC_ERROR;
}

/* This function programs the inner tunnel flow in the hardware. */
static void
ulp_install_inner_tun_flow(struct bnxt_tun_cache_entry *tun_entry,
			   struct ulp_rte_parser_params *tun_o_params)
{
	struct bnxt_ulp_mapper_create_parms mparms = { 0 };
	struct ulp_per_port_flow_info *flow_info;
	struct ulp_rte_parser_params *inner_params;
	int ret;

	/* Tunnel inner flow doesn't have tunnel dmac, use the tunnel
	 * dmac that was stored during F1 programming.
	 */
	flow_info = &tun_entry->tun_flow_info[tun_o_params->port_id];
	STAILQ_FOREACH(inner_params, &flow_info->tun_i_prms_list, next) {
		memcpy(&inner_params->hdr_field[ULP_TUN_O_DMAC_HDR_FIELD_INDEX],
		       tun_entry->t_dmac, RTE_ETHER_ADDR_LEN);
		inner_params->parent_fid = tun_entry->outer_tun_flow_id;

		bnxt_ulp_init_mapper_params(&mparms, inner_params,
					    BNXT_ULP_FDB_TYPE_REGULAR);

		ret = ulp_mapper_flow_create(inner_params->ulp_ctx, &mparms);
		if (ret)
			PMD_DRV_LOG(ERR,
				    "Failed to create inner tun flow, FID:%u.",
				    inner_params->fid);
	}
}

/* This function either install outer tunnel flow & inner tunnel flow
 * or just the outer tunnel flow based on the flow state.
 */
static int32_t
ulp_post_process_outer_tun_flow(struct ulp_rte_parser_params *params,
			     struct bnxt_tun_cache_entry *tun_entry,
			     uint16_t tun_idx)
{
	int ret;

	ret = ulp_install_outer_tun_flow(params, tun_entry, tun_idx);
	if (ret == BNXT_TF_RC_ERROR) {
		PMD_DRV_LOG(ERR, "Failed to create outer tunnel flow.");
		return ret;
	}

	/* Install any cached tunnel inner flows that came before tunnel
	 * outer flow.
	 */
	ulp_install_inner_tun_flow(tun_entry, params);

	return BNXT_TF_RC_FID;
}

/* This function will be called if inner tunnel flow request comes before
 * outer tunnel flow request.
 */
static int32_t
ulp_post_process_cache_inner_tun_flow(struct ulp_rte_parser_params *params,
				      struct bnxt_tun_cache_entry *tun_entry)
{
	struct ulp_rte_parser_params *inner_tun_params;
	struct ulp_per_port_flow_info *flow_info;
	int ret;

	ret = ulp_matcher_pattern_match(params, &params->class_id);
	if (ret != BNXT_TF_RC_SUCCESS)
		return BNXT_TF_RC_ERROR;

	ret = ulp_matcher_action_match(params, &params->act_tmpl);
	if (ret != BNXT_TF_RC_SUCCESS)
		return BNXT_TF_RC_ERROR;

	/* If Tunnel inner flow comes first then we can't install it in the
	 * hardware, because, Tunnel inner flow will not have L2 context
	 * information. So, just cache the Tunnel inner flow information
	 * and program it in the context of F1 flow installation.
	 */
	flow_info = &tun_entry->tun_flow_info[params->port_id];
	inner_tun_params = rte_zmalloc("ulp_inner_tun_params",
				       sizeof(struct ulp_rte_parser_params), 0);
	if (!inner_tun_params)
		return BNXT_TF_RC_ERROR;
	memcpy(inner_tun_params, params, sizeof(struct ulp_rte_parser_params));
	STAILQ_INSERT_TAIL(&flow_info->tun_i_prms_list, inner_tun_params,
			   next);
	flow_info->tun_i_cnt++;

	/* F1 and it's related Tunnel inner flows are correlated based on
	 * Tunnel Destination IP Address. It could be already set, if
	 * the inner flow got offloaded first.
	 */
	if (tun_entry->t_dst_ip_valid)
		goto done;
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_IPV4))
		memcpy(&tun_entry->t_dst_ip,
		       &params->hdr_field[ULP_TUN_O_IPV4_DIP_INDEX].spec,
		       sizeof(rte_be32_t));
	else
		memcpy(tun_entry->t_dst_ip6,
		       &params->hdr_field[ULP_TUN_O_IPV6_DIP_INDEX].spec,
		       sizeof(tun_entry->t_dst_ip6));
	tun_entry->t_dst_ip_valid = true;

done:
	return BNXT_TF_RC_FID;
}

/* This function will be called if inner tunnel flow request comes after
 * the outer tunnel flow request.
 */
static int32_t
ulp_post_process_inner_tun_flow(struct ulp_rte_parser_params *params,
				struct bnxt_tun_cache_entry *tun_entry)
{
	memcpy(&params->hdr_field[ULP_TUN_O_DMAC_HDR_FIELD_INDEX],
	       tun_entry->t_dmac, RTE_ETHER_ADDR_LEN);

	params->parent_fid = tun_entry->outer_tun_flow_id;

	return BNXT_TF_RC_NORMAL;
}

static int32_t
ulp_get_tun_entry(struct ulp_rte_parser_params *params,
		  struct bnxt_tun_cache_entry **tun_entry,
		  uint16_t *tun_idx)
{
	int i, first_free_entry = BNXT_ULP_TUN_ENTRY_INVALID;
	struct bnxt_tun_cache_entry *tun_tbl;
	bool tun_entry_found = false, free_entry_found = false;

	tun_tbl = bnxt_ulp_cntxt_ptr2_tun_tbl_get(params->ulp_ctx);
	if (!tun_tbl)
		return BNXT_TF_RC_ERROR;

	for (i = 0; i < BNXT_ULP_MAX_TUN_CACHE_ENTRIES; i++) {
		if (!memcmp(&tun_tbl[i].t_dst_ip,
			    &params->hdr_field[ULP_TUN_O_IPV4_DIP_INDEX].spec,
			    sizeof(rte_be32_t)) ||
		    !memcmp(&tun_tbl[i].t_dst_ip6,
			    &params->hdr_field[ULP_TUN_O_IPV6_DIP_INDEX].spec,
			    16)) {
			tun_entry_found = true;
			break;
		}

		if (!tun_tbl[i].t_dst_ip_valid && !free_entry_found) {
			first_free_entry = i;
			free_entry_found = true;
		}
	}

	if (tun_entry_found) {
		*tun_entry = &tun_tbl[i];
		*tun_idx = i;
	} else {
		if (first_free_entry == BNXT_ULP_TUN_ENTRY_INVALID)
			return BNXT_TF_RC_ERROR;
		*tun_entry = &tun_tbl[first_free_entry];
		*tun_idx = first_free_entry;
	}

	return 0;
}

int32_t
ulp_post_process_tun_flow(struct ulp_rte_parser_params *params)
{
	bool inner_tun_sig, cache_inner_tun_flow;
	bool outer_tun_reject, outer_tun_flow, inner_tun_flow;
	enum bnxt_ulp_tun_flow_state flow_state;
	struct bnxt_tun_cache_entry *tun_entry;
	uint32_t l3_tun, l3_tun_decap;
	uint16_t tun_idx;
	int rc;

	/* Computational fields that indicate it's a TUNNEL DECAP flow */
	l3_tun = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN);
	l3_tun_decap = ULP_COMP_FLD_IDX_RD(params,
					   BNXT_ULP_CF_IDX_L3_TUN_DECAP);
	if (!l3_tun)
		return BNXT_TF_RC_NORMAL;

	rc = ulp_get_tun_entry(params, &tun_entry, &tun_idx);
	if (rc == BNXT_TF_RC_ERROR)
		return rc;

	if (params->port_id >= RTE_MAX_ETHPORTS)
		return BNXT_TF_RC_ERROR;
	flow_state = tun_entry->tun_flow_info[params->port_id].state;
	/* Outer tunnel flow validation */
	outer_tun_flow = BNXT_OUTER_TUN_FLOW(l3_tun, params);
	outer_tun_reject = BNXT_REJECT_OUTER_TUN_FLOW(flow_state,
						      outer_tun_flow);

	/* Inner tunnel flow validation */
	inner_tun_sig = BNXT_INNER_TUN_SIGNATURE(l3_tun, l3_tun_decap, params);
	cache_inner_tun_flow = BNXT_CACHE_INNER_TUN_FLOW(flow_state,
							 inner_tun_sig);
	inner_tun_flow = BNXT_INNER_TUN_FLOW(flow_state, inner_tun_sig);

	if (outer_tun_reject) {
		tun_entry->outer_tun_rej_cnt++;
		BNXT_TF_DBG(ERR,
			    "Tunnel F1 flow rejected, COUNT: %d\n",
			    tun_entry->outer_tun_rej_cnt);
	}

	if (outer_tun_reject)
		return BNXT_TF_RC_ERROR;
	else if (cache_inner_tun_flow)
		return ulp_post_process_cache_inner_tun_flow(params, tun_entry);
	else if (outer_tun_flow)
		return ulp_post_process_outer_tun_flow(params, tun_entry,
						       tun_idx);
	else if (inner_tun_flow)
		return ulp_post_process_inner_tun_flow(params, tun_entry);
	else
		return BNXT_TF_RC_NORMAL;
}

void
ulp_tun_tbl_init(struct bnxt_tun_cache_entry *tun_tbl)
{
	struct ulp_per_port_flow_info *flow_info;
	int i, j;

	for (i = 0; i < BNXT_ULP_MAX_TUN_CACHE_ENTRIES; i++) {
		for (j = 0; j < RTE_MAX_ETHPORTS; j++) {
			flow_info = &tun_tbl[i].tun_flow_info[j];
			STAILQ_INIT(&flow_info->tun_i_prms_list);
		}
	}
}

void
ulp_clear_tun_entry(struct bnxt_tun_cache_entry *tun_tbl, uint8_t tun_idx)
{
	struct ulp_rte_parser_params *inner_params;
	struct ulp_per_port_flow_info *flow_info;
	int j;

	for (j = 0; j < RTE_MAX_ETHPORTS; j++) {
		flow_info = &tun_tbl[tun_idx].tun_flow_info[j];
		STAILQ_FOREACH(inner_params,
			       &flow_info->tun_i_prms_list,
			       next) {
			STAILQ_REMOVE(&flow_info->tun_i_prms_list,
				      inner_params,
				      ulp_rte_parser_params, next);
			rte_free(inner_params);
		}
	}

	memset(&tun_tbl[tun_idx], 0,
			sizeof(struct bnxt_tun_cache_entry));

	for (j = 0; j < RTE_MAX_ETHPORTS; j++) {
		flow_info = &tun_tbl[tun_idx].tun_flow_info[j];
		STAILQ_INIT(&flow_info->tun_i_prms_list);
	}
}

static bool
ulp_chk_and_rem_tun_i_flow(struct bnxt_tun_cache_entry *tun_entry,
			   struct ulp_per_port_flow_info *flow_info,
			   uint32_t fid)
{
	struct ulp_rte_parser_params *inner_params;
	int j;

	STAILQ_FOREACH(inner_params,
		       &flow_info->tun_i_prms_list,
		       next) {
		if (inner_params->fid == fid) {
			STAILQ_REMOVE(&flow_info->tun_i_prms_list,
				      inner_params,
				      ulp_rte_parser_params,
				      next);
			rte_free(inner_params);
			flow_info->tun_i_cnt--;
			/* When a dpdk application offloads a duplicate
			 * tunnel inner flow on a port that it is not
			 * destined to, there won't be a tunnel outer flow
			 * associated with these duplicate tunnel inner flows.
			 * So, when the last tunnel inner flow ages out, the
			 * driver has to clear the tunnel entry, otherwise
			 * the tunnel entry cannot be reused.
			 */
			if (!flow_info->tun_i_cnt &&
			    flow_info->state != BNXT_ULP_FLOW_STATE_TUN_O_OFFLD) {
				memset(tun_entry, 0,
				       sizeof(struct bnxt_tun_cache_entry));
				for (j = 0; j < RTE_MAX_ETHPORTS; j++)
					STAILQ_INIT(&flow_info->tun_i_prms_list);
			}
			return true;
		}
	}

	return false;
}

/* When a dpdk application offloads the same tunnel inner flow
 * on all the uplink ports, a tunnel inner flow entry is cached
 * even if it is not for the right uplink port. Such tunnel
 * inner flows will eventually get aged out as there won't be
 * any traffic on these ports. When such a flow destroy is
 * called, cleanup the tunnel inner flow entry.
 */
void
ulp_clear_tun_inner_entry(struct bnxt_tun_cache_entry *tun_tbl, uint32_t fid)
{
	struct ulp_per_port_flow_info *flow_info;
	int i, j;

	for (i = 0; i < BNXT_ULP_MAX_TUN_CACHE_ENTRIES; i++) {
		if (!tun_tbl[i].t_dst_ip_valid)
			continue;
		for (j = 0; j < RTE_MAX_ETHPORTS; j++) {
			flow_info = &tun_tbl[i].tun_flow_info[j];
			if (ulp_chk_and_rem_tun_i_flow(&tun_tbl[i],
						       flow_info, fid) == true)
				return;
		}
	}
}
