/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "bnxt_ulp.h"
#include "ulp_tun.h"
#include "ulp_utils.h"

/* returns negative on error, 1 if new entry is allocated or zero if old */
int32_t
ulp_app_tun_search_entry(struct bnxt_ulp_context *ulp_ctx,
			 struct rte_flow_tunnel *app_tunnel,
			 struct bnxt_flow_app_tun_ent **tun_entry)
{
	struct bnxt_flow_app_tun_ent *tun_ent_list;
	int32_t i, rc = 0, free_entry = -1;

	tun_ent_list = bnxt_ulp_cntxt_ptr2_app_tun_list_get(ulp_ctx);
	if (!tun_ent_list) {
		BNXT_TF_DBG(ERR, "unable to get the app tunnel list\n");
		return -EINVAL;
	}

	for (i = 0; i < BNXT_ULP_MAX_TUN_CACHE_ENTRIES; i++) {
		if (!tun_ent_list[i].ref_cnt) {
			if (free_entry < 0)
				free_entry = i;
		} else {
			if (!memcmp(&tun_ent_list[i].app_tunnel,
				    app_tunnel,
				    sizeof(struct rte_flow_tunnel))) {
				*tun_entry =  &tun_ent_list[i];
				tun_ent_list[free_entry].ref_cnt++;
				return rc;
			}
		}
	}
	if (free_entry >= 0) {
		*tun_entry =  &tun_ent_list[free_entry];
		memcpy(&tun_ent_list[free_entry].app_tunnel, app_tunnel,
		       sizeof(struct rte_flow_tunnel));
		tun_ent_list[free_entry].ref_cnt = 1;
		rc = 1;
	} else {
		BNXT_TF_DBG(ERR, "ulp app tunnel list is full\n");
		return -ENOMEM;
	}

	return rc;
}

void
ulp_app_tun_entry_delete(struct bnxt_flow_app_tun_ent *tun_entry)
{
	if (tun_entry) {
		if (tun_entry->ref_cnt) {
			tun_entry->ref_cnt--;
			if (!tun_entry->ref_cnt)
				memset(tun_entry, 0,
				       sizeof(struct bnxt_flow_app_tun_ent));
		}
	}
}

int32_t
ulp_app_tun_entry_set_decap_action(struct bnxt_flow_app_tun_ent *tun_entry)
{
	if (!tun_entry)
		return -EINVAL;

	tun_entry->action.type = (typeof(tun_entry->action.type))
			      BNXT_RTE_FLOW_ACTION_TYPE_VXLAN_DECAP;
	tun_entry->action.conf = tun_entry;
	return 0;
}

int32_t
ulp_app_tun_entry_set_decap_item(struct bnxt_flow_app_tun_ent *tun_entry)
{
	if (!tun_entry)
		return -EINVAL;

	tun_entry->item.type = (typeof(tun_entry->item.type))
			      BNXT_RTE_FLOW_ITEM_TYPE_VXLAN_DECAP;
	tun_entry->item.spec = tun_entry;
	tun_entry->item.last = NULL;
	tun_entry->item.mask = NULL;
	return 0;
}

struct bnxt_flow_app_tun_ent *
ulp_app_tun_match_entry(struct bnxt_ulp_context *ulp_ctx,
			const void *ctx)
{
	struct bnxt_flow_app_tun_ent *tun_ent_list;
	int32_t i;

	tun_ent_list = bnxt_ulp_cntxt_ptr2_app_tun_list_get(ulp_ctx);
	if (!tun_ent_list) {
		BNXT_TF_DBG(ERR, "unable to get the app tunnel list\n");
		return NULL;
	}

	for (i = 0; i < BNXT_ULP_MAX_TUN_CACHE_ENTRIES; i++) {
		if (&tun_ent_list[i] == ctx)
			return &tun_ent_list[i];
	}
	return NULL;
}

static int32_t
ulp_get_tun_entry(struct ulp_rte_parser_params *params,
		  struct bnxt_tun_cache_entry **tun_entry,
		  uint16_t *tun_idx)
{
	int32_t i, first_free_entry = BNXT_ULP_TUN_ENTRY_INVALID;
	struct bnxt_tun_cache_entry *tun_tbl;
	uint32_t dip_idx, dmac_idx, use_ipv4 = 0;

	tun_tbl = bnxt_ulp_cntxt_ptr2_tun_tbl_get(params->ulp_ctx);
	if (!tun_tbl) {
		BNXT_TF_DBG(ERR, "Error: could not get Tunnel table\n");
		return BNXT_TF_RC_ERROR;
	}

	/* get the outer destination ip field index */
	dip_idx = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_TUN_OFF_DIP_ID);
	dmac_idx = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_TUN_OFF_DMAC_ID);
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_IPV4))
		use_ipv4 = 1;

	for (i = 0; i < BNXT_ULP_MAX_TUN_CACHE_ENTRIES; i++) {
		if (!tun_tbl[i].t_dst_ip_valid) {
			if (first_free_entry == BNXT_ULP_TUN_ENTRY_INVALID)
				first_free_entry = i;
			continue;
		}
		/* match on the destination ip of the tunnel */
		if ((use_ipv4 && !memcmp(&tun_tbl[i].t_dst_ip,
					 params->hdr_field[dip_idx].spec,
					 sizeof(rte_be32_t))) ||
		    (!use_ipv4 &&
		     !memcmp(tun_tbl[i].t_dst_ip6,
			     params->hdr_field[dip_idx].spec,
			     sizeof(((struct bnxt_tun_cache_entry *)
				     NULL)->t_dst_ip6)))) {
			*tun_entry = &tun_tbl[i];
			*tun_idx = i;
			return 0;
		}
	}
	if (first_free_entry == BNXT_ULP_TUN_ENTRY_INVALID) {
		BNXT_TF_DBG(ERR, "Error: No entry available in tunnel table\n");
		return BNXT_TF_RC_ERROR;
	}

	*tun_idx = first_free_entry;
	*tun_entry = &tun_tbl[first_free_entry];
	tun_tbl[first_free_entry].t_dst_ip_valid = true;

	/* Update the destination ip and mac */
	if (use_ipv4)
		memcpy(&tun_tbl[first_free_entry].t_dst_ip,
		       params->hdr_field[dip_idx].spec, sizeof(rte_be32_t));
	else
		memcpy(tun_tbl[first_free_entry].t_dst_ip6,
		       params->hdr_field[dip_idx].spec,
		       sizeof(((struct bnxt_tun_cache_entry *)
				     NULL)->t_dst_ip6));
	memcpy(tun_tbl[first_free_entry].t_dmac,
	       params->hdr_field[dmac_idx].spec, RTE_ETHER_ADDR_LEN);

	return 0;
}

/* Tunnel API to delete the tunnel entry */
void
ulp_tunnel_offload_entry_clear(struct bnxt_tun_cache_entry *tun_tbl,
			       uint8_t tun_idx)
{
	memset(&tun_tbl[tun_idx], 0, sizeof(struct bnxt_tun_cache_entry));
}

/* Tunnel API to perform tunnel offload process when there is F1/F2 flows */
int32_t
ulp_tunnel_offload_process(struct ulp_rte_parser_params *params)
{
	struct bnxt_tun_cache_entry *tun_entry;
	uint16_t tun_idx;
	int32_t rc = BNXT_TF_RC_SUCCESS;

	/* Perform the tunnel offload only for F1 and F2 flows */
	if (!ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			      BNXT_ULP_HDR_BIT_F1) &&
	    !ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			      BNXT_ULP_HDR_BIT_F2))
		return rc;

	/* search for the tunnel entry if not found create one */
	rc = ulp_get_tun_entry(params, &tun_entry, &tun_idx);
	if (rc == BNXT_TF_RC_ERROR)
		return rc;

	/* Tunnel offload for the outer Tunnel flow */
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_F1)) {
		/* Reset the JUMP action bit in the action bitmap as we don't
		 * offload this action.
		 */
		ULP_BITMAP_RESET(params->act_bitmap.bits,
				 BNXT_ULP_ACT_BIT_JUMP);
		params->parent_flow = true;
		params->tun_idx = tun_idx;
		tun_entry->outer_tun_flow_id = params->fid;
	} else if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_F2)) {
		ULP_BITMAP_RESET(params->hdr_bitmap.bits,
				 BNXT_ULP_HDR_BIT_F2);
		/* add the vxlan decap action for F2 flows */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_VXLAN_DECAP);
		params->child_flow = true;
		params->tun_idx = tun_idx;
		params->parent_flow = false;
	}
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUNNEL_ID, tun_idx);
	return rc;
}
