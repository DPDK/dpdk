/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "ulp_template_db.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_common.h"
#include "ulp_rte_parser.h"
#include "ulp_utils.h"
#include "tfp.h"
#include "ulp_port_db.h"

/* Utility function to skip the void items. */
static inline int32_t
ulp_rte_item_skip_void(const struct rte_flow_item **item, uint32_t increment)
{
	if (!*item)
		return 0;
	if (increment)
		(*item)++;
	while ((*item) && (*item)->type == RTE_FLOW_ITEM_TYPE_VOID)
		(*item)++;
	if (*item)
		return 1;
	return 0;
}

/* Utility function to update the field_bitmap */
static void
ulp_rte_parser_field_bitmap_update(struct ulp_rte_parser_params *params,
				   uint32_t idx)
{
	struct ulp_rte_hdr_field *field;

	field = &params->hdr_field[idx];
	if (ulp_bitmap_notzero(field->mask, field->size)) {
		ULP_INDEX_BITMAP_SET(params->fld_bitmap.bits, idx);
		/* Not exact match */
		if (!ulp_bitmap_is_ones(field->mask, field->size))
			ULP_BITMAP_SET(params->fld_bitmap.bits,
				       BNXT_ULP_MATCH_TYPE_BITMASK_WM);
	} else {
		ULP_INDEX_BITMAP_RESET(params->fld_bitmap.bits, idx);
	}
}

/* Utility function to copy field spec items */
static struct ulp_rte_hdr_field *
ulp_rte_parser_fld_copy(struct ulp_rte_hdr_field *field,
			const void *buffer,
			uint32_t size)
{
	field->size = size;
	memcpy(field->spec, buffer, field->size);
	field++;
	return field;
}

/* Utility function to copy field masks items */
static void
ulp_rte_prsr_mask_copy(struct ulp_rte_parser_params *params,
		       uint32_t *idx,
		       const void *buffer,
		       uint32_t size)
{
	struct ulp_rte_hdr_field *field = &params->hdr_field[*idx];

	memcpy(field->mask, buffer, size);
	ulp_rte_parser_field_bitmap_update(params, *idx);
	*idx = *idx + 1;
}

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow items into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_hdr_parse(const struct rte_flow_item pattern[],
			      struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item *item = pattern;
	struct bnxt_ulp_rte_hdr_info *hdr_info;

	params->field_idx = BNXT_ULP_PROTO_HDR_SVIF_NUM;
	if (params->dir == ULP_DIR_EGRESS)
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_EGR);

	/* Parse all the items in the pattern */
	while (item && item->type != RTE_FLOW_ITEM_TYPE_END) {
		/* get the header information from the flow_hdr_info table */
		hdr_info = &ulp_hdr_info[item->type];
		if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_NOT_SUPPORTED) {
			BNXT_TF_DBG(ERR,
				    "Truflow parser does not support type %d\n",
				    item->type);
			return BNXT_TF_RC_PARSE_ERR;
		} else if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_SUPPORTED) {
			/* call the registered callback handler */
			if (hdr_info->proto_hdr_func) {
				if (hdr_info->proto_hdr_func(item, params) !=
				    BNXT_TF_RC_SUCCESS) {
					return BNXT_TF_RC_ERROR;
				}
			}
		}
		item++;
	}
	/* update the implied SVIF */
	(void)ulp_rte_parser_svif_process(params);
	return BNXT_TF_RC_SUCCESS;
}

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow actions into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_act_parse(const struct rte_flow_action actions[],
			      struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action *action_item = actions;
	struct bnxt_ulp_rte_act_info *hdr_info;

	/* Parse all the items in the pattern */
	while (action_item && action_item->type != RTE_FLOW_ACTION_TYPE_END) {
		/* get the header information from the flow_hdr_info table */
		hdr_info = &ulp_act_info[action_item->type];
		if (hdr_info->act_type ==
		    BNXT_ULP_ACT_TYPE_NOT_SUPPORTED) {
			BNXT_TF_DBG(ERR,
				    "Truflow parser does not support act %u\n",
				    action_item->type);
			return BNXT_TF_RC_ERROR;
		} else if (hdr_info->act_type ==
		    BNXT_ULP_ACT_TYPE_SUPPORTED) {
			/* call the registered callback handler */
			if (hdr_info->proto_act_func) {
				if (hdr_info->proto_act_func(action_item,
							     params) !=
				    BNXT_TF_RC_SUCCESS) {
					return BNXT_TF_RC_ERROR;
				}
			}
		}
		action_item++;
	}
	/* update the implied VNIC */
	ulp_rte_parser_vnic_process(params);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item PF Header. */
static int32_t
ulp_rte_parser_svif_set(struct ulp_rte_parser_params *params,
			enum rte_flow_item_type proto,
			uint16_t svif,
			uint16_t mask)
{
	uint16_t port_id = svif;
	uint32_t dir = 0;
	struct ulp_rte_hdr_field *hdr_field;
	uint32_t ifindex;
	int32_t rc;

	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_SVIF)) {
		BNXT_TF_DBG(ERR,
			    "SVIF already set,multiple source not support'd\n");
		return BNXT_TF_RC_ERROR;
	}

	/*update the hdr_bitmap with BNXT_ULP_HDR_PROTO_SVIF */
	ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_SVIF);

	if (proto == RTE_FLOW_ITEM_TYPE_PORT_ID) {
		dir = ULP_UTIL_CHF_IDX_RD(params,
					  BNXT_ULP_CHF_IDX_DIRECTION);
		/* perform the conversion from dpdk port to bnxt svif */
		rc = ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx, port_id,
						       &ifindex);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Invalid port id\n");
			return BNXT_TF_RC_ERROR;
		}
		ulp_port_db_svif_get(params->ulp_ctx, ifindex, dir, &svif);
		svif = rte_cpu_to_be_16(svif);
	}
	hdr_field = &params->hdr_field[BNXT_ULP_PROTO_HDR_FIELD_SVIF_IDX];
	memcpy(hdr_field->spec, &svif, sizeof(svif));
	memcpy(hdr_field->mask, &mask, sizeof(mask));
	hdr_field->size = sizeof(svif);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of the RTE port id */
int32_t
ulp_rte_parser_svif_process(struct ulp_rte_parser_params *params)
{
	uint16_t port_id = 0;
	uint16_t svif_mask = 0xFFFF;

	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_SVIF))
		return BNXT_TF_RC_SUCCESS;

	/* SVIF not set. So get the port id */
	port_id = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_INCOMING_IF);

	/* Update the SVIF details */
	return ulp_rte_parser_svif_set(params, RTE_FLOW_ITEM_TYPE_PORT_ID,
				       port_id, svif_mask);
}

/* Function to handle the implicit VNIC RTE port id */
int32_t
ulp_rte_parser_vnic_process(struct ulp_rte_parser_params *params)
{
	struct ulp_rte_act_bitmap *act = &params->act_bitmap;

	if (ULP_BITMAP_ISSET(act->bits, BNXT_ULP_ACTION_BIT_VNIC) ||
	    ULP_BITMAP_ISSET(act->bits, BNXT_ULP_ACTION_BIT_VPORT))
		return BNXT_TF_RC_SUCCESS;

	/* Update the vnic details */
	ulp_rte_pf_act_handler(NULL, params);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item PF Header. */
int32_t
ulp_rte_pf_hdr_handler(const struct rte_flow_item *item,
		       struct ulp_rte_parser_params *params)
{
	uint16_t port_id = 0;
	uint16_t svif_mask = 0xFFFF;

	/* Get the port id */
	port_id = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_INCOMING_IF);

	/* Update the SVIF details */
	return ulp_rte_parser_svif_set(params,
				       item->type,
				       port_id, svif_mask);
}

/* Function to handle the parsing of RTE Flow item VF Header. */
int32_t
ulp_rte_vf_hdr_handler(const struct rte_flow_item *item,
		       struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_vf *vf_spec = item->spec;
	const struct rte_flow_item_vf *vf_mask = item->mask;
	uint16_t svif = 0, mask = 0;

	/* Get VF rte_flow_item for Port details */
	if (vf_spec)
		svif = (uint16_t)vf_spec->id;
	if (vf_mask)
		mask = (uint16_t)vf_mask->id;

	return ulp_rte_parser_svif_set(params, item->type, svif, mask);
}

/* Function to handle the parsing of RTE Flow item port id  Header. */
int32_t
ulp_rte_port_id_hdr_handler(const struct rte_flow_item *item,
			    struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_port_id *port_spec = item->spec;
	const struct rte_flow_item_port_id *port_mask = item->mask;
	uint16_t svif = 0, mask = 0;

	/*
	 * Copy the rte_flow_item for Port into hdr_field using port id
	 * header fields.
	 */
	if (port_spec)
		svif = (uint16_t)port_spec->id;
	if (port_mask)
		mask = (uint16_t)port_mask->id;

	/* Update the SVIF details */
	return ulp_rte_parser_svif_set(params, item->type, svif, mask);
}

/* Function to handle the parsing of RTE Flow item phy port Header. */
int32_t
ulp_rte_phy_port_hdr_handler(const struct rte_flow_item *item,
			     struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_phy_port *port_spec = item->spec;
	const struct rte_flow_item_phy_port *port_mask = item->mask;
	uint32_t svif = 0, mask = 0;

	/* Copy the rte_flow_item for phy port into hdr_field */
	if (port_spec)
		svif = port_spec->index;
	if (port_mask)
		mask = port_mask->index;

	/* Update the SVIF details */
	return ulp_rte_parser_svif_set(params, item->type, svif, mask);
}

/* Function to handle the parsing of RTE Flow item Ethernet Header. */
int32_t
ulp_rte_eth_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	uint32_t idx = params->field_idx;
	uint64_t set_flag = 0;
	uint32_t size;

	/*
	 * Copy the rte_flow_item for eth into hdr_field using ethernet
	 * header fields
	 */
	if (eth_spec) {
		size = sizeof(eth_spec->dst.addr_bytes);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						eth_spec->dst.addr_bytes,
						size);
		size = sizeof(eth_spec->src.addr_bytes);
		field = ulp_rte_parser_fld_copy(field,
						eth_spec->src.addr_bytes,
						size);
		field = ulp_rte_parser_fld_copy(field,
						&eth_spec->type,
						sizeof(eth_spec->type));
	}
	if (eth_mask) {
		ulp_rte_prsr_mask_copy(params, &idx, eth_mask->dst.addr_bytes,
				       sizeof(eth_mask->dst.addr_bytes));
		ulp_rte_prsr_mask_copy(params, &idx, eth_mask->src.addr_bytes,
				       sizeof(eth_mask->src.addr_bytes));
		ulp_rte_prsr_mask_copy(params, &idx, &eth_mask->type,
				       sizeof(eth_mask->type));
	}
	/* Add number of vlan header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_ETH_NUM;
	params->vlan_idx = params->field_idx;
	params->field_idx += BNXT_ULP_PROTO_HDR_VLAN_NUM;

	/* Update the hdr_bitmap with BNXT_ULP_HDR_PROTO_I_ETH */
	set_flag = ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
				    BNXT_ULP_HDR_BIT_O_ETH);
	if (set_flag)
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_I_ETH);
	else
		ULP_BITMAP_RESET(params->hdr_bitmap.bits,
				 BNXT_ULP_HDR_BIT_I_ETH);

	/* update the hdr_bitmap with BNXT_ULP_HDR_PROTO_O_ETH */
	ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vlan Header. */
int32_t
ulp_rte_vlan_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap	*hdr_bit;
	uint32_t idx = params->vlan_idx;
	uint16_t vlan_tag, priority;
	uint32_t outer_vtag_num;
	uint32_t inner_vtag_num;

	/*
	 * Copy the rte_flow_item for vlan into hdr_field using Vlan
	 * header fields
	 */
	if (vlan_spec) {
		vlan_tag = ntohs(vlan_spec->tci);
		priority = htons(vlan_tag >> 13);
		vlan_tag &= 0xfff;
		vlan_tag = htons(vlan_tag);

		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&priority,
						sizeof(priority));
		field = ulp_rte_parser_fld_copy(field,
						&vlan_tag,
						sizeof(vlan_tag));
		field = ulp_rte_parser_fld_copy(field,
						&vlan_spec->inner_type,
						sizeof(vlan_spec->inner_type));
	}

	if (vlan_mask) {
		vlan_tag = ntohs(vlan_mask->tci);
		priority = htons(vlan_tag >> 13);
		vlan_tag &= 0xfff;
		vlan_tag = htons(vlan_tag);

		field = &params->hdr_field[idx];
		memcpy(field->mask, &priority, field->size);
		field++;
		memcpy(field->mask, &vlan_tag, field->size);
		field++;
		memcpy(field->mask, &vlan_mask->inner_type, field->size);
	}
	/* Set the vlan index to new incremented value */
	params->vlan_idx += BNXT_ULP_PROTO_HDR_S_VLAN_NUM;

	/* Get the outer tag and inner tag counts */
	outer_vtag_num = ULP_UTIL_CHF_IDX_RD(params,
					     BNXT_ULP_CHF_IDX_O_VTAG_NUM);
	inner_vtag_num = ULP_UTIL_CHF_IDX_RD(params,
					     BNXT_ULP_CHF_IDX_I_VTAG_NUM);

	/* Update the hdr_bitmap of the vlans */
	hdr_bit = &params->hdr_bitmap;
	if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
	    !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OO_VLAN)) {
		/* Set the outer vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OO_VLAN);
		outer_vtag_num++;
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_VTAG_NUM,
				    outer_vtag_num);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_VTAG_PRESENT, 1);
	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OO_VLAN) &&
		   !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OI_VLAN)) {
		/* Set the outer vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OI_VLAN);
		outer_vtag_num++;
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_VTAG_NUM,
				    outer_vtag_num);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_TWO_VTAGS, 1);
	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OO_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OI_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_IO_VLAN)) {
		/* Set the inner vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bit->bits, BNXT_ULP_HDR_BIT_IO_VLAN);
		inner_vtag_num++;
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_VTAG_NUM,
				    inner_vtag_num);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_VTAG_PRESENT, 1);
	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OO_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_OI_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_IO_VLAN) &&
		   !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_II_VLAN)) {
		/* Set the inner vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bit->bits, BNXT_ULP_HDR_BIT_II_VLAN);
		inner_vtag_num++;
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_VTAG_NUM,
				    inner_vtag_num);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_TWO_VTAGS, 1);
	} else {
		BNXT_TF_DBG(ERR, "Error Parsing:Vlan hdr found withtout eth\n");
		return BNXT_TF_RC_ERROR;
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint32_t inner_l3, outer_l3;

	inner_l3 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_I_L3);
	if (inner_l3) {
		BNXT_TF_DBG(ERR, "Parse Error:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (ipv4_spec) {
		size = sizeof(ipv4_spec->hdr.version_ihl);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&ipv4_spec->hdr.version_ihl,
						size);
		size = sizeof(ipv4_spec->hdr.type_of_service);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.type_of_service,
						size);
		size = sizeof(ipv4_spec->hdr.total_length);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.total_length,
						size);
		size = sizeof(ipv4_spec->hdr.packet_id);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.packet_id,
						size);
		size = sizeof(ipv4_spec->hdr.fragment_offset);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.fragment_offset,
						size);
		size = sizeof(ipv4_spec->hdr.time_to_live);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.time_to_live,
						size);
		size = sizeof(ipv4_spec->hdr.next_proto_id);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.next_proto_id,
						size);
		size = sizeof(ipv4_spec->hdr.hdr_checksum);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.hdr_checksum,
						size);
		size = sizeof(ipv4_spec->hdr.src_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.src_addr,
						size);
		size = sizeof(ipv4_spec->hdr.dst_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.dst_addr,
						size);
	}
	if (ipv4_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.version_ihl,
				       sizeof(ipv4_mask->hdr.version_ihl));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.type_of_service,
				       sizeof(ipv4_mask->hdr.type_of_service));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.total_length,
				       sizeof(ipv4_mask->hdr.total_length));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.packet_id,
				       sizeof(ipv4_mask->hdr.packet_id));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.fragment_offset,
				       sizeof(ipv4_mask->hdr.fragment_offset));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.time_to_live,
				       sizeof(ipv4_mask->hdr.time_to_live));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.next_proto_id,
				       sizeof(ipv4_mask->hdr.next_proto_id));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.hdr_checksum,
				       sizeof(ipv4_mask->hdr.hdr_checksum));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.src_addr,
				       sizeof(ipv4_mask->hdr.src_addr));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.dst_addr,
				       sizeof(ipv4_mask->hdr.dst_addr));
	}
	/* Add the number of ipv4 header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_IPV4_NUM;

	/* Set the ipv4 header bitmap and computed l3 header bitmaps */
	outer_l3 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_O_L3);
	if (outer_l3 ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV4);
		inner_l3++;
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_L3, inner_l3);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4);
		outer_l3++;
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_L3, outer_l3);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item IPV6 Header */
int32_t
ulp_rte_ipv6_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_ipv6	*ipv6_spec = item->spec;
	const struct rte_flow_item_ipv6	*ipv6_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint32_t inner_l3, outer_l3;

	inner_l3 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_I_L3);
	if (inner_l3) {
		BNXT_TF_DBG(ERR, "Parse Error: 3'rd L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (ipv6_spec) {
		size = sizeof(ipv6_spec->hdr.vtc_flow);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&ipv6_spec->hdr.vtc_flow,
						size);
		size = sizeof(ipv6_spec->hdr.payload_len);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.payload_len,
						size);
		size = sizeof(ipv6_spec->hdr.proto);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.proto,
						size);
		size = sizeof(ipv6_spec->hdr.hop_limits);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.hop_limits,
						size);
		size = sizeof(ipv6_spec->hdr.src_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.src_addr,
						size);
		size = sizeof(ipv6_spec->hdr.dst_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.dst_addr,
						size);
	}
	if (ipv6_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.vtc_flow,
				       sizeof(ipv6_mask->hdr.vtc_flow));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.payload_len,
				       sizeof(ipv6_mask->hdr.payload_len));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.proto,
				       sizeof(ipv6_mask->hdr.proto));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.hop_limits,
				       sizeof(ipv6_mask->hdr.hop_limits));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.src_addr,
				       sizeof(ipv6_mask->hdr.src_addr));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.dst_addr,
				       sizeof(ipv6_mask->hdr.dst_addr));
	}
	/* add number of ipv6 header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_IPV6_NUM;

	/* Set the ipv6 header bitmap and computed l3 header bitmaps */
	outer_l3 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_O_L3);
	if (outer_l3 ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV6);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_L3, 1);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_L3, 1);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item UDP Header. */
int32_t
ulp_rte_udp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint32_t inner_l4, outer_l4;

	inner_l4 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_I_L4);
	if (inner_l4) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (udp_spec) {
		size = sizeof(udp_spec->hdr.src_port);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&udp_spec->hdr.src_port,
						size);
		size = sizeof(udp_spec->hdr.dst_port);
		field = ulp_rte_parser_fld_copy(field,
						&udp_spec->hdr.dst_port,
						size);
		size = sizeof(udp_spec->hdr.dgram_len);
		field = ulp_rte_parser_fld_copy(field,
						&udp_spec->hdr.dgram_len,
						size);
		size = sizeof(udp_spec->hdr.dgram_cksum);
		field = ulp_rte_parser_fld_copy(field,
						&udp_spec->hdr.dgram_cksum,
						size);
	}
	if (udp_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.src_port,
				       sizeof(udp_mask->hdr.src_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.dst_port,
				       sizeof(udp_mask->hdr.dst_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.dgram_len,
				       sizeof(udp_mask->hdr.dgram_len));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.dgram_cksum,
				       sizeof(udp_mask->hdr.dgram_cksum));
	}

	/* Add number of UDP header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_UDP_NUM;

	/* Set the udp header bitmap and computed l4 header bitmaps */
	outer_l4 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_O_L4);
	if (outer_l4 ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_UDP);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_L4, 1);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_L4, 1);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item TCP Header. */
int32_t
ulp_rte_tcp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_tcp *tcp_spec = item->spec;
	const struct rte_flow_item_tcp *tcp_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint32_t inner_l4, outer_l4;

	inner_l4 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_I_L4);
	if (inner_l4) {
		BNXT_TF_DBG(ERR, "Parse Error:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (tcp_spec) {
		size = sizeof(tcp_spec->hdr.src_port);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&tcp_spec->hdr.src_port,
						size);
		size = sizeof(tcp_spec->hdr.dst_port);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.dst_port,
						size);
		size = sizeof(tcp_spec->hdr.sent_seq);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.sent_seq,
						size);
		size = sizeof(tcp_spec->hdr.recv_ack);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.recv_ack,
						size);
		size = sizeof(tcp_spec->hdr.data_off);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.data_off,
						size);
		size = sizeof(tcp_spec->hdr.tcp_flags);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.tcp_flags,
						size);
		size = sizeof(tcp_spec->hdr.rx_win);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.rx_win,
						size);
		size = sizeof(tcp_spec->hdr.cksum);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.cksum,
						size);
		size = sizeof(tcp_spec->hdr.tcp_urp);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.tcp_urp,
						size);
	} else {
		idx += BNXT_ULP_PROTO_HDR_TCP_NUM;
	}

	if (tcp_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.src_port,
				       sizeof(tcp_mask->hdr.src_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.dst_port,
				       sizeof(tcp_mask->hdr.dst_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.sent_seq,
				       sizeof(tcp_mask->hdr.sent_seq));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.recv_ack,
				       sizeof(tcp_mask->hdr.recv_ack));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.data_off,
				       sizeof(tcp_mask->hdr.data_off));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.tcp_flags,
				       sizeof(tcp_mask->hdr.tcp_flags));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.rx_win,
				       sizeof(tcp_mask->hdr.rx_win));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.cksum,
				       sizeof(tcp_mask->hdr.cksum));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.tcp_urp,
				       sizeof(tcp_mask->hdr.tcp_urp));
	}
	/* add number of TCP header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_TCP_NUM;

	/* Set the udp header bitmap and computed l4 header bitmaps */
	outer_l4 = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_O_L4);
	if (outer_l4 ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_TCP);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_I_L4, 1);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP);
		ULP_UTIL_CHF_IDX_WR(params, BNXT_ULP_CHF_IDX_O_L4, 1);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vxlan Header. */
int32_t
ulp_rte_vxlan_hdr_handler(const struct rte_flow_item *item,
			  struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_vxlan *vxlan_spec = item->spec;
	const struct rte_flow_item_vxlan *vxlan_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;

	/*
	 * Copy the rte_flow_item for vxlan into hdr_field using vxlan
	 * header fields
	 */
	if (vxlan_spec) {
		size = sizeof(vxlan_spec->flags);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&vxlan_spec->flags,
						size);
		size = sizeof(vxlan_spec->rsvd0);
		field = ulp_rte_parser_fld_copy(field,
						&vxlan_spec->rsvd0,
						size);
		size = sizeof(vxlan_spec->vni);
		field = ulp_rte_parser_fld_copy(field,
						&vxlan_spec->vni,
						size);
		size = sizeof(vxlan_spec->rsvd1);
		field = ulp_rte_parser_fld_copy(field,
						&vxlan_spec->rsvd1,
						size);
	}
	if (vxlan_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->flags,
				       sizeof(vxlan_mask->flags));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->rsvd0,
				       sizeof(vxlan_mask->rsvd0));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->vni,
				       sizeof(vxlan_mask->vni));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->rsvd1,
				       sizeof(vxlan_mask->rsvd1));
	}
	/* Add number of vxlan header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_VXLAN_NUM;

	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item void Header */
int32_t
ulp_rte_void_hdr_handler(const struct rte_flow_item *item __rte_unused,
			 struct ulp_rte_parser_params *params __rte_unused)
{
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action void Header. */
int32_t
ulp_rte_void_act_handler(const struct rte_flow_action *action_item __rte_unused,
			 struct ulp_rte_parser_params *params __rte_unused)
{
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action Mark Header. */
int32_t
ulp_rte_mark_act_handler(const struct rte_flow_action *action_item,
			 struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_mark *mark;
	struct ulp_rte_act_bitmap *act = &param->act_bitmap;
	uint32_t mark_id;

	mark = action_item->conf;
	if (mark) {
		mark_id = tfp_cpu_to_be_32(mark->id);
		memcpy(&param->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_MARK],
		       &mark_id, BNXT_ULP_ACT_PROP_SZ_MARK);

		/* Update the hdr_bitmap with vxlan */
		ULP_BITMAP_SET(act->bits, BNXT_ULP_ACTION_BIT_MARK);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: Mark arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action RSS Header. */
int32_t
ulp_rte_rss_act_handler(const struct rte_flow_action *action_item,
			struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_rss *rss = action_item->conf;

	if (rss) {
		/* Update the hdr_bitmap with vxlan */
		ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACTION_BIT_RSS);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: RSS arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action vxlan_encap Header. */
int32_t
ulp_rte_vxlan_encap_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_vxlan_encap *vxlan_encap;
	const struct rte_flow_item *item;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv6 *ipv6_spec;
	struct rte_flow_item_vxlan vxlan_spec;
	uint32_t vlan_num = 0, vlan_size = 0;
	uint32_t ip_size = 0, ip_type = 0;
	uint32_t vxlan_size = 0;
	uint8_t *buff;
	/* IP header per byte - ver/hlen, TOS, ID, ID, FRAG, FRAG, TTL, PROTO */
	const uint8_t def_ipv4_hdr[] = {0x45, 0x00, 0x00, 0x01, 0x00,
				    0x00, 0x40, 0x11};
	struct ulp_rte_act_bitmap *act = &params->act_bitmap;
	struct ulp_rte_act_prop *ap = &params->act_prop;

	vxlan_encap = action_item->conf;
	if (!vxlan_encap) {
		BNXT_TF_DBG(ERR, "Parse Error: Vxlan_encap arg is invalid\n");
		return BNXT_TF_RC_ERROR;
	}

	item = vxlan_encap->definition;
	if (!item) {
		BNXT_TF_DBG(ERR, "Parse Error: definition arg is invalid\n");
		return BNXT_TF_RC_ERROR;
	}

	if (!ulp_rte_item_skip_void(&item, 0))
		return BNXT_TF_RC_ERROR;

	/* must have ethernet header */
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH) {
		BNXT_TF_DBG(ERR, "Parse Error:vxlan encap does not have eth\n");
		return BNXT_TF_RC_ERROR;
	}
	eth_spec = item->spec;
	buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC];
	ulp_encap_buffer_copy(buff,
			      eth_spec->dst.addr_bytes,
			      BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC);

	/* Goto the next item */
	if (!ulp_rte_item_skip_void(&item, 1))
		return BNXT_TF_RC_ERROR;

	/* May have vlan header */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_num++;
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG];
		ulp_encap_buffer_copy(buff,
				      item->spec,
				      sizeof(struct rte_flow_item_vlan));

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	}

	/* may have two vlan headers */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_num++;
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG +
		       sizeof(struct rte_flow_item_vlan)],
		       item->spec,
		       sizeof(struct rte_flow_item_vlan));
		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	}
	/* Update the vlan count and size of more than one */
	if (vlan_num) {
		vlan_size = vlan_num * sizeof(struct rte_flow_item_vlan);
		vlan_num = tfp_cpu_to_be_32(vlan_num);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_NUM],
		       &vlan_num,
		       sizeof(uint32_t));
		vlan_size = tfp_cpu_to_be_32(vlan_size);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ],
		       &vlan_size,
		       sizeof(uint32_t));
	}

	/* L3 must be IPv4, IPv6 */
	if (item->type == RTE_FLOW_ITEM_TYPE_IPV4) {
		ipv4_spec = item->spec;
		ip_size = BNXT_ULP_ENCAP_IPV4_SIZE;

		/* copy the ipv4 details */
		if (ulp_buffer_is_empty(&ipv4_spec->hdr.version_ihl,
					BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS)) {
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP];
			ulp_encap_buffer_copy(buff,
					      def_ipv4_hdr,
					      BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS +
					      BNXT_ULP_ENCAP_IPV4_ID_PROTO);
		} else {
			const uint8_t *tmp_buff;

			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP];
			ulp_encap_buffer_copy(buff,
					      &ipv4_spec->hdr.version_ihl,
					      BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS);
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
			     BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS];
			tmp_buff = (const uint8_t *)&ipv4_spec->hdr.packet_id;
			ulp_encap_buffer_copy(buff,
					      tmp_buff,
					      BNXT_ULP_ENCAP_IPV4_ID_PROTO);
		}
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
		    BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS +
		    BNXT_ULP_ENCAP_IPV4_ID_PROTO];
		ulp_encap_buffer_copy(buff,
				      (const uint8_t *)&ipv4_spec->hdr.dst_addr,
				      BNXT_ULP_ENCAP_IPV4_DEST_IP);

		/* Update the ip size details */
		ip_size = tfp_cpu_to_be_32(ip_size);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ],
		       &ip_size, sizeof(uint32_t));

		/* update the ip type */
		ip_type = rte_cpu_to_be_32(BNXT_ULP_ETH_IPV4);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE],
		       &ip_type, sizeof(uint32_t));

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		ipv6_spec = item->spec;
		ip_size = BNXT_ULP_ENCAP_IPV6_SIZE;

		/* copy the ipv4 details */
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP],
		       ipv6_spec, BNXT_ULP_ENCAP_IPV6_SIZE);

		/* Update the ip size details */
		ip_size = tfp_cpu_to_be_32(ip_size);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ],
		       &ip_size, sizeof(uint32_t));

		 /* update the ip type */
		ip_type = rte_cpu_to_be_32(BNXT_ULP_ETH_IPV6);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE],
		       &ip_type, sizeof(uint32_t));

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	} else {
		BNXT_TF_DBG(ERR, "Parse Error: Vxlan Encap expects L3 hdr\n");
		return BNXT_TF_RC_ERROR;
	}

	/* L4 is UDP */
	if (item->type != RTE_FLOW_ITEM_TYPE_UDP) {
		BNXT_TF_DBG(ERR, "vxlan encap does not have udp\n");
		return BNXT_TF_RC_ERROR;
	}
	/* copy the udp details */
	ulp_encap_buffer_copy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP],
			      item->spec, BNXT_ULP_ENCAP_UDP_SIZE);

	if (!ulp_rte_item_skip_void(&item, 1))
		return BNXT_TF_RC_ERROR;

	/* Finally VXLAN */
	if (item->type != RTE_FLOW_ITEM_TYPE_VXLAN) {
		BNXT_TF_DBG(ERR, "vxlan encap does not have vni\n");
		return BNXT_TF_RC_ERROR;
	}
	vxlan_size = sizeof(struct rte_flow_item_vxlan);
	/* copy the vxlan details */
	memcpy(&vxlan_spec, item->spec, vxlan_size);
	vxlan_spec.flags = 0x08;
	ulp_encap_buffer_copy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN],
			      (const uint8_t *)&vxlan_spec,
			      vxlan_size);
	vxlan_size = tfp_cpu_to_be_32(vxlan_size);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ],
	       &vxlan_size, sizeof(uint32_t));

	/*update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(act->bits, BNXT_ULP_ACTION_BIT_VXLAN_ENCAP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action vxlan_encap Header */
int32_t
ulp_rte_vxlan_decap_act_handler(const struct rte_flow_action *action_item
				__rte_unused,
				struct ulp_rte_parser_params *params)
{
	/* update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(params->act_bitmap.bits,
		       BNXT_ULP_ACTION_BIT_VXLAN_DECAP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action drop Header. */
int32_t
ulp_rte_drop_act_handler(const struct rte_flow_action *action_item __rte_unused,
			 struct ulp_rte_parser_params *params)
{
	/* Update the hdr_bitmap with drop */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACTION_BIT_DROP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action count. */
int32_t
ulp_rte_count_act_handler(const struct rte_flow_action *action_item,
			  struct ulp_rte_parser_params *params)

{
	const struct rte_flow_action_count *act_count;
	struct ulp_rte_act_prop *act_prop = &params->act_prop;

	act_count = action_item->conf;
	if (act_count) {
		if (act_count->shared) {
			BNXT_TF_DBG(ERR,
				    "Parse Error:Shared count not supported\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_COUNT],
		       &act_count->id,
		       BNXT_ULP_ACT_PROP_SZ_COUNT);
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACTION_BIT_COUNT);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action PF. */
int32_t
ulp_rte_pf_act_handler(const struct rte_flow_action *action_item __rte_unused,
		       struct ulp_rte_parser_params *params)
{
	uint32_t svif;

	/* Update the hdr_bitmap with vnic bit */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACTION_BIT_VNIC);

	/* copy the PF of the current device into VNIC Property */
	svif = ULP_UTIL_CHF_IDX_RD(params, BNXT_ULP_CHF_IDX_INCOMING_IF);
	svif = bnxt_get_vnic_id(svif);
	svif = rte_cpu_to_be_32(svif);
	memcpy(&params->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_VNIC],
	       &svif, BNXT_ULP_ACT_PROP_SZ_VNIC);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action VF. */
int32_t
ulp_rte_vf_act_handler(const struct rte_flow_action *action_item,
		       struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_vf *vf_action;
	uint32_t pid;

	vf_action = action_item->conf;
	if (vf_action) {
		if (vf_action->original) {
			BNXT_TF_DBG(ERR,
				    "Parse Error:VF Original not supported\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		/* TBD: Update the computed VNIC using VF conversion */
		pid = bnxt_get_vnic_id(vf_action->id);
		pid = rte_cpu_to_be_32(pid);
		memcpy(&param->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_VNIC],
		       &pid, BNXT_ULP_ACT_PROP_SZ_VNIC);
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACTION_BIT_VNIC);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action port_id. */
int32_t
ulp_rte_port_id_act_handler(const struct rte_flow_action *act_item,
			    struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_port_id *port_id;
	uint32_t pid;

	port_id = act_item->conf;
	if (port_id) {
		if (port_id->original) {
			BNXT_TF_DBG(ERR,
				    "ParseErr:Portid Original not supported\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		/* TBD: Update the computed VNIC using port conversion */
		pid = bnxt_get_vnic_id(port_id->id);
		pid = rte_cpu_to_be_32(pid);
		memcpy(&param->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_VNIC],
		       &pid, BNXT_ULP_ACT_PROP_SZ_VNIC);
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACTION_BIT_VNIC);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action phy_port. */
int32_t
ulp_rte_phy_port_act_handler(const struct rte_flow_action *action_item,
			     struct ulp_rte_parser_params *prm)
{
	const struct rte_flow_action_phy_port *phy_port;
	uint32_t pid;

	phy_port = action_item->conf;
	if (phy_port) {
		if (phy_port->original) {
			BNXT_TF_DBG(ERR,
				    "Parse Err:Port Original not supported\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		pid = bnxt_get_vnic_id(phy_port->index);
		pid = rte_cpu_to_be_32(pid);
		memcpy(&prm->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_VPORT],
		       &pid, BNXT_ULP_ACT_PROP_SZ_VPORT);
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(prm->act_bitmap.bits, BNXT_ULP_ACTION_BIT_VPORT);
	return BNXT_TF_RC_SUCCESS;
}
