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

/* Inline Func to read integer that is stored in big endian format */
static inline void ulp_util_field_int_read(uint8_t *buffer,
					   uint32_t *val)
{
	uint32_t temp_val;

	memcpy(&temp_val, buffer, sizeof(uint32_t));
	*val = rte_be_to_cpu_32(temp_val);
}

/* Inline Func to write integer that is stored in big endian format */
static inline void ulp_util_field_int_write(uint8_t *buffer,
					    uint32_t val)
{
	uint32_t temp_val = rte_cpu_to_be_32(val);

	memcpy(buffer, &temp_val, sizeof(uint32_t));
}

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow items into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_hdr_parse(const struct rte_flow_item pattern[],
			      struct ulp_rte_hdr_bitmap *hdr_bitmap,
			      struct ulp_rte_hdr_field *hdr_field)
{
	const struct rte_flow_item *item = pattern;
	uint32_t field_idx = BNXT_ULP_HDR_FIELD_LAST;
	uint32_t vlan_idx = 0;
	struct bnxt_ulp_rte_hdr_info *hdr_info;

	/* Parse all the items in the pattern */
	while (item && item->type != RTE_FLOW_ITEM_TYPE_END) {
		/* get the header information from the flow_hdr_info table */
		hdr_info = &ulp_hdr_info[item->type];
		if (hdr_info->hdr_type ==
		    BNXT_ULP_HDR_TYPE_NOT_SUPPORTED) {
			BNXT_TF_DBG(ERR,
				    "Truflow parser does not support type %d\n",
				    item->type);
			return BNXT_TF_RC_PARSE_ERR;
		} else if (hdr_info->hdr_type ==
			   BNXT_ULP_HDR_TYPE_SUPPORTED) {
			/* call the registered callback handler */
			if (hdr_info->proto_hdr_func) {
				if (hdr_info->proto_hdr_func(item,
							     hdr_bitmap,
							     hdr_field,
							     &field_idx,
							     &vlan_idx) !=
				    BNXT_TF_RC_SUCCESS) {
					return BNXT_TF_RC_ERROR;
				}
			}
		}
		item++;
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item PF Header. */
static int32_t
ulp_rte_parser_svif_set(struct ulp_rte_hdr_bitmap *hdr_bitmap,
			struct ulp_rte_hdr_field *hdr_field,
			enum rte_flow_item_type proto,
			uint32_t svif,
			uint32_t mask)
{
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_SVIF)) {
		BNXT_TF_DBG(ERR,
			    "SVIF already set,"
			    " multiple sources not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/* TBD: Check for any mapping errors for svif */
	/* Update the hdr_bitmap with BNXT_ULP_HDR_PROTO_SVIF. */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_SVIF);

	if (proto != RTE_FLOW_ITEM_TYPE_PF) {
		memcpy(hdr_field[BNXT_ULP_HDR_FIELD_SVIF_INDEX].spec,
		       &svif, sizeof(svif));
		memcpy(hdr_field[BNXT_ULP_HDR_FIELD_SVIF_INDEX].mask,
		       &mask, sizeof(mask));
		hdr_field[BNXT_ULP_HDR_FIELD_SVIF_INDEX].size = sizeof(svif);
	}

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item PF Header. */
int32_t
ulp_rte_pf_hdr_handler(const struct rte_flow_item *item,
		       struct ulp_rte_hdr_bitmap *hdr_bitmap,
		       struct ulp_rte_hdr_field *hdr_field,
		       uint32_t *field_idx __rte_unused,
		       uint32_t *vlan_idx __rte_unused)
{
	return ulp_rte_parser_svif_set(hdr_bitmap, hdr_field,
				       item->type, 0, 0);
}

/* Function to handle the parsing of RTE Flow item VF Header. */
int32_t
ulp_rte_vf_hdr_handler(const struct rte_flow_item *item,
		       struct ulp_rte_hdr_bitmap *hdr_bitmap,
		       struct ulp_rte_hdr_field	 *hdr_field,
		       uint32_t *field_idx __rte_unused,
		       uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_vf *vf_spec, *vf_mask;
	uint32_t svif = 0, mask = 0;

	vf_spec = item->spec;
	vf_mask = item->mask;

	/*
	 * Copy the rte_flow_item for eth into hdr_field using ethernet
	 * header fields.
	 */
	if (vf_spec)
		svif = vf_spec->id;
	if (vf_mask)
		mask = vf_mask->id;

	return ulp_rte_parser_svif_set(hdr_bitmap, hdr_field,
				       item->type, svif, mask);
}

/* Function to handle the parsing of RTE Flow item port id  Header. */
int32_t
ulp_rte_port_id_hdr_handler(const struct rte_flow_item *item,
			    struct ulp_rte_hdr_bitmap *hdr_bitmap,
			    struct ulp_rte_hdr_field *hdr_field,
			    uint32_t *field_idx __rte_unused,
			    uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_port_id *port_spec, *port_mask;
	uint32_t svif = 0, mask = 0;

	port_spec = item->spec;
	port_mask = item->mask;

	/*
	 * Copy the rte_flow_item for Port into hdr_field using port id
	 * header fields.
	 */
	if (port_spec)
		svif = port_spec->id;
	if (port_mask)
		mask = port_mask->id;

	return ulp_rte_parser_svif_set(hdr_bitmap, hdr_field,
				       item->type, svif, mask);
}

/* Function to handle the parsing of RTE Flow item phy port Header. */
int32_t
ulp_rte_phy_port_hdr_handler(const struct rte_flow_item *item,
			     struct ulp_rte_hdr_bitmap *hdr_bitmap,
			     struct ulp_rte_hdr_field *hdr_field,
			     uint32_t *field_idx __rte_unused,
			     uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_phy_port *port_spec, *port_mask;
	uint32_t svif = 0, mask = 0;

	port_spec = item->spec;
	port_mask = item->mask;

	/* Copy the rte_flow_item for phy port into hdr_field */
	if (port_spec)
		svif = port_spec->index;
	if (port_mask)
		mask = port_mask->index;

	return ulp_rte_parser_svif_set(hdr_bitmap, hdr_field,
				       item->type, svif, mask);
}

/* Function to handle the parsing of RTE Flow item Ethernet Header. */
int32_t
ulp_rte_eth_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_hdr_bitmap *hdr_bitmap,
			struct ulp_rte_hdr_field *hdr_field,
			uint32_t *field_idx,
			uint32_t *vlan_idx)
{
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	uint32_t idx = *field_idx;
	uint32_t mdx = *field_idx;
	uint64_t set_flag = 0;

	eth_spec = item->spec;
	eth_mask = item->mask;

	/*
	 * Copy the rte_flow_item for eth into hdr_field using ethernet
	 * header fields
	 */
	if (eth_spec) {
		hdr_field[idx].size = sizeof(eth_spec->dst.addr_bytes);
		memcpy(hdr_field[idx++].spec, eth_spec->dst.addr_bytes,
		       sizeof(eth_spec->dst.addr_bytes));
		hdr_field[idx].size = sizeof(eth_spec->src.addr_bytes);
		memcpy(hdr_field[idx++].spec, eth_spec->src.addr_bytes,
		       sizeof(eth_spec->src.addr_bytes));
		hdr_field[idx].size = sizeof(eth_spec->type);
		memcpy(hdr_field[idx++].spec, &eth_spec->type,
		       sizeof(eth_spec->type));
	} else {
		idx += BNXT_ULP_PROTO_HDR_ETH_NUM;
	}

	if (eth_mask) {
		memcpy(hdr_field[mdx++].mask, eth_mask->dst.addr_bytes,
		       sizeof(eth_mask->dst.addr_bytes));
		memcpy(hdr_field[mdx++].mask, eth_mask->src.addr_bytes,
		       sizeof(eth_mask->src.addr_bytes));
		memcpy(hdr_field[mdx++].mask, &eth_mask->type,
		       sizeof(eth_mask->type));
	}
	/* Add number of vlan header elements */
	*field_idx = idx + BNXT_ULP_PROTO_HDR_VLAN_NUM;
	*vlan_idx = idx;

	/* Update the hdr_bitmap with BNXT_ULP_HDR_PROTO_I_ETH */
	set_flag = ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ETH);
	if (set_flag)
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_ETH);
	else
		ULP_BITMAP_RESET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_ETH);

	/* update the hdr_bitmap with BNXT_ULP_HDR_PROTO_O_ETH */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ETH);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vlan Header. */
int32_t
ulp_rte_vlan_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_hdr_bitmap *hdr_bitmap,
			 struct ulp_rte_hdr_field *hdr_field,
			 uint32_t *field_idx __rte_unused,
			 uint32_t *vlan_idx)
{
	const struct rte_flow_item_vlan *vlan_spec, *vlan_mask;
	uint32_t idx = *vlan_idx;
	uint32_t mdx = *vlan_idx;
	uint16_t vlan_tag, priority;
	uint32_t outer_vtag_num = 0, inner_vtag_num = 0;
	uint8_t *outer_tag_buffer;
	uint8_t *inner_tag_buffer;

	vlan_spec = item->spec;
	vlan_mask = item->mask;
	outer_tag_buffer = hdr_field[BNXT_ULP_HDR_FIELD_O_VTAG_NUM].spec;
	inner_tag_buffer = hdr_field[BNXT_ULP_HDR_FIELD_I_VTAG_NUM].spec;

	/*
	 * Copy the rte_flow_item for vlan into hdr_field using Vlan
	 * header fields
	 */
	if (vlan_spec) {
		vlan_tag = ntohs(vlan_spec->tci);
		priority = htons(vlan_tag >> 13);
		vlan_tag &= 0xfff;
		vlan_tag = htons(vlan_tag);

		hdr_field[idx].size = sizeof(priority);
		memcpy(hdr_field[idx++].spec, &priority, sizeof(priority));
		hdr_field[idx].size = sizeof(vlan_tag);
		memcpy(hdr_field[idx++].spec, &vlan_tag, sizeof(vlan_tag));
		hdr_field[idx].size = sizeof(vlan_spec->inner_type);
		memcpy(hdr_field[idx++].spec, &vlan_spec->inner_type,
		       sizeof(vlan_spec->inner_type));
	} else {
		idx += BNXT_ULP_PROTO_HDR_S_VLAN_NUM;
	}

	if (vlan_mask) {
		vlan_tag = ntohs(vlan_mask->tci);
		priority = htons(vlan_tag >> 13);
		vlan_tag &= 0xfff;
		vlan_tag = htons(vlan_tag);

		memcpy(hdr_field[mdx++].mask, &priority, sizeof(priority));
		memcpy(hdr_field[mdx++].mask, &vlan_tag, sizeof(vlan_tag));
		memcpy(hdr_field[mdx++].mask, &vlan_mask->inner_type,
		       sizeof(vlan_mask->inner_type));
	}
	/* Set the vlan index to new incremented value */
	*vlan_idx = idx;

	/* Get the outer tag and inner tag counts */
	ulp_util_field_int_read(outer_tag_buffer, &outer_vtag_num);
	ulp_util_field_int_read(inner_tag_buffer, &inner_vtag_num);

	/* Update the hdr_bitmap of the vlans */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
	    !ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_OO_VLAN)) {
		/* Set the outer vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_OO_VLAN);
		outer_vtag_num++;
		ulp_util_field_int_write(outer_tag_buffer, outer_vtag_num);
		hdr_field[BNXT_ULP_HDR_FIELD_O_VTAG_NUM].size =
							sizeof(uint32_t);
	} else if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_OO_VLAN) &&
		   !ULP_BITMAP_ISSET(hdr_bitmap->bits,
				     BNXT_ULP_HDR_BIT_OI_VLAN)) {
		/* Set the outer vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_OI_VLAN);
		outer_vtag_num++;
		ulp_util_field_int_write(outer_tag_buffer, outer_vtag_num);
		hdr_field[BNXT_ULP_HDR_FIELD_O_VTAG_NUM].size =
							    sizeof(uint32_t);
	} else if (ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_OO_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_OI_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_I_ETH) &&
		   !ULP_BITMAP_ISSET(hdr_bitmap->bits,
				     BNXT_ULP_HDR_BIT_IO_VLAN)) {
		/* Set the inner vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_IO_VLAN);
		inner_vtag_num++;
		ulp_util_field_int_write(inner_tag_buffer, inner_vtag_num);
		hdr_field[BNXT_ULP_HDR_FIELD_I_VTAG_NUM].size =
							    sizeof(uint32_t);
	} else if (ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_OO_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_OI_VLAN) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_I_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bitmap->bits,
				    BNXT_ULP_HDR_BIT_IO_VLAN) &&
		   !ULP_BITMAP_ISSET(hdr_bitmap->bits,
				     BNXT_ULP_HDR_BIT_II_VLAN)) {
		/* Set the inner vlan bit and update the vlan tag num */
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_II_VLAN);
		inner_vtag_num++;
		ulp_util_field_int_write(inner_tag_buffer, inner_vtag_num);
		hdr_field[BNXT_ULP_HDR_FIELD_I_VTAG_NUM].size =
							    sizeof(uint32_t);
	} else {
		BNXT_TF_DBG(ERR, "Error Parsing:Vlan hdr found withtout eth\n");
		return BNXT_TF_RC_ERROR;
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_hdr_bitmap *hdr_bitmap,
			 struct ulp_rte_hdr_field *hdr_field,
			 uint32_t *field_idx,
			 uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	uint32_t idx = *field_idx;
	uint32_t mdx = *field_idx;

	ipv4_spec = item->spec;
	ipv4_mask = item->mask;

	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L3)) {
		BNXT_TF_DBG(ERR, "Parse Error:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (ipv4_spec) {
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.version_ihl);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.version_ihl,
		       sizeof(ipv4_spec->hdr.version_ihl));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.type_of_service);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.type_of_service,
		       sizeof(ipv4_spec->hdr.type_of_service));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.total_length);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.total_length,
		       sizeof(ipv4_spec->hdr.total_length));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.packet_id);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.packet_id,
		       sizeof(ipv4_spec->hdr.packet_id));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.fragment_offset);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.fragment_offset,
		       sizeof(ipv4_spec->hdr.fragment_offset));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.time_to_live);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.time_to_live,
		       sizeof(ipv4_spec->hdr.time_to_live));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.next_proto_id);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.next_proto_id,
		       sizeof(ipv4_spec->hdr.next_proto_id));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.hdr_checksum);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.hdr_checksum,
		       sizeof(ipv4_spec->hdr.hdr_checksum));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.src_addr);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.src_addr,
		       sizeof(ipv4_spec->hdr.src_addr));
		hdr_field[idx].size = sizeof(ipv4_spec->hdr.dst_addr);
		memcpy(hdr_field[idx++].spec, &ipv4_spec->hdr.dst_addr,
		       sizeof(ipv4_spec->hdr.dst_addr));
	} else {
		idx += BNXT_ULP_PROTO_HDR_IPV4_NUM;
	}

	if (ipv4_mask) {
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.version_ihl,
		       sizeof(ipv4_mask->hdr.version_ihl));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.type_of_service,
		       sizeof(ipv4_mask->hdr.type_of_service));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.total_length,
		       sizeof(ipv4_mask->hdr.total_length));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.packet_id,
		       sizeof(ipv4_mask->hdr.packet_id));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.fragment_offset,
		       sizeof(ipv4_mask->hdr.fragment_offset));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.time_to_live,
		       sizeof(ipv4_mask->hdr.time_to_live));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.next_proto_id,
		       sizeof(ipv4_mask->hdr.next_proto_id));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.hdr_checksum,
		       sizeof(ipv4_mask->hdr.hdr_checksum));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.src_addr,
		       sizeof(ipv4_mask->hdr.src_addr));
		memcpy(hdr_field[mdx++].mask, &ipv4_mask->hdr.dst_addr,
		       sizeof(ipv4_mask->hdr.dst_addr));
	}
	*field_idx = idx; /* Number of ipv4 header elements */

	/* Set the ipv4 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L3) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV4);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L3);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L3);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item IPV6 Header */
int32_t
ulp_rte_ipv6_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_hdr_bitmap *hdr_bitmap,
			 struct ulp_rte_hdr_field *hdr_field,
			 uint32_t *field_idx,
			 uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	uint32_t idx = *field_idx;
	uint32_t mdx = *field_idx;

	ipv6_spec = item->spec;
	ipv6_mask = item->mask;

	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L3)) {
		BNXT_TF_DBG(ERR, "Parse Error: 3'rd L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (ipv6_spec) {
		hdr_field[idx].size = sizeof(ipv6_spec->hdr.vtc_flow);
		memcpy(hdr_field[idx++].spec, &ipv6_spec->hdr.vtc_flow,
		       sizeof(ipv6_spec->hdr.vtc_flow));
		hdr_field[idx].size = sizeof(ipv6_spec->hdr.payload_len);
		memcpy(hdr_field[idx++].spec, &ipv6_spec->hdr.payload_len,
		       sizeof(ipv6_spec->hdr.payload_len));
		hdr_field[idx].size = sizeof(ipv6_spec->hdr.proto);
		memcpy(hdr_field[idx++].spec, &ipv6_spec->hdr.proto,
		       sizeof(ipv6_spec->hdr.proto));
		hdr_field[idx].size = sizeof(ipv6_spec->hdr.hop_limits);
		memcpy(hdr_field[idx++].spec, &ipv6_spec->hdr.hop_limits,
		       sizeof(ipv6_spec->hdr.hop_limits));
		hdr_field[idx].size = sizeof(ipv6_spec->hdr.src_addr);
		memcpy(hdr_field[idx++].spec, &ipv6_spec->hdr.src_addr,
		       sizeof(ipv6_spec->hdr.src_addr));
		hdr_field[idx].size = sizeof(ipv6_spec->hdr.dst_addr);
		memcpy(hdr_field[idx++].spec, &ipv6_spec->hdr.dst_addr,
		       sizeof(ipv6_spec->hdr.dst_addr));
	} else {
		idx += BNXT_ULP_PROTO_HDR_IPV6_NUM;
	}

	if (ipv6_mask) {
		memcpy(hdr_field[mdx++].mask, &ipv6_mask->hdr.vtc_flow,
		       sizeof(ipv6_mask->hdr.vtc_flow));
		memcpy(hdr_field[mdx++].mask, &ipv6_mask->hdr.payload_len,
		       sizeof(ipv6_mask->hdr.payload_len));
		memcpy(hdr_field[mdx++].mask, &ipv6_mask->hdr.proto,
		       sizeof(ipv6_mask->hdr.proto));
		memcpy(hdr_field[mdx++].mask, &ipv6_mask->hdr.hop_limits,
		       sizeof(ipv6_mask->hdr.hop_limits));
		memcpy(hdr_field[mdx++].mask, &ipv6_mask->hdr.src_addr,
		       sizeof(ipv6_mask->hdr.src_addr));
		memcpy(hdr_field[mdx++].mask, &ipv6_mask->hdr.dst_addr,
		       sizeof(ipv6_mask->hdr.dst_addr));
	}
	*field_idx = idx; /* add number of ipv6 header elements */

	/* Set the ipv6 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L3) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV6);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L3);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L3);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item UDP Header. */
int32_t
ulp_rte_udp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_hdr_bitmap *hdr_bitmap,
			struct ulp_rte_hdr_field *hdr_field,
			uint32_t *field_idx,
			uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	uint32_t idx = *field_idx;
	uint32_t mdx = *field_idx;

	udp_spec = item->spec;
	udp_mask = item->mask;

	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L4)) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (udp_spec) {
		hdr_field[idx].size = sizeof(udp_spec->hdr.src_port);
		memcpy(hdr_field[idx++].spec, &udp_spec->hdr.src_port,
		       sizeof(udp_spec->hdr.src_port));
		hdr_field[idx].size = sizeof(udp_spec->hdr.dst_port);
		memcpy(hdr_field[idx++].spec, &udp_spec->hdr.dst_port,
		       sizeof(udp_spec->hdr.dst_port));
		hdr_field[idx].size = sizeof(udp_spec->hdr.dgram_len);
		memcpy(hdr_field[idx++].spec, &udp_spec->hdr.dgram_len,
		       sizeof(udp_spec->hdr.dgram_len));
		hdr_field[idx].size = sizeof(udp_spec->hdr.dgram_cksum);
		memcpy(hdr_field[idx++].spec, &udp_spec->hdr.dgram_cksum,
		       sizeof(udp_spec->hdr.dgram_cksum));
	} else {
		idx += BNXT_ULP_PROTO_HDR_UDP_NUM;
	}

	if (udp_mask) {
		memcpy(hdr_field[mdx++].mask, &udp_mask->hdr.src_port,
		       sizeof(udp_mask->hdr.src_port));
		memcpy(hdr_field[mdx++].mask, &udp_mask->hdr.dst_port,
		       sizeof(udp_mask->hdr.dst_port));
		memcpy(hdr_field[mdx++].mask, &udp_mask->hdr.dgram_len,
		       sizeof(udp_mask->hdr.dgram_len));
		memcpy(hdr_field[mdx++].mask, &udp_mask->hdr.dgram_cksum,
		       sizeof(udp_mask->hdr.dgram_cksum));
	}
	*field_idx = idx; /* Add number of UDP header elements */

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_UDP);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L4);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L4);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item TCP Header. */
int32_t
ulp_rte_tcp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_hdr_bitmap *hdr_bitmap,
			struct ulp_rte_hdr_field *hdr_field,
			uint32_t *field_idx,
			uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	uint32_t idx = *field_idx;
	uint32_t mdx = *field_idx;

	tcp_spec = item->spec;
	tcp_mask = item->mask;

	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L4)) {
		BNXT_TF_DBG(ERR, "Parse Error:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (tcp_spec) {
		hdr_field[idx].size = sizeof(tcp_spec->hdr.src_port);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.src_port,
		       sizeof(tcp_spec->hdr.src_port));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.dst_port);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.dst_port,
		       sizeof(tcp_spec->hdr.dst_port));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.sent_seq);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.sent_seq,
		       sizeof(tcp_spec->hdr.sent_seq));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.recv_ack);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.recv_ack,
		       sizeof(tcp_spec->hdr.recv_ack));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.data_off);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.data_off,
		       sizeof(tcp_spec->hdr.data_off));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.tcp_flags);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.tcp_flags,
		       sizeof(tcp_spec->hdr.tcp_flags));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.rx_win);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.rx_win,
		       sizeof(tcp_spec->hdr.rx_win));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.cksum);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.cksum,
		       sizeof(tcp_spec->hdr.cksum));
		hdr_field[idx].size = sizeof(tcp_spec->hdr.tcp_urp);
		memcpy(hdr_field[idx++].spec, &tcp_spec->hdr.tcp_urp,
		       sizeof(tcp_spec->hdr.tcp_urp));
	} else {
		idx += BNXT_ULP_PROTO_HDR_TCP_NUM;
	}

	if (tcp_mask) {
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.src_port,
		       sizeof(tcp_mask->hdr.src_port));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.dst_port,
		       sizeof(tcp_mask->hdr.dst_port));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.sent_seq,
		       sizeof(tcp_mask->hdr.sent_seq));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.recv_ack,
		       sizeof(tcp_mask->hdr.recv_ack));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.data_off,
		       sizeof(tcp_mask->hdr.data_off));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.tcp_flags,
		       sizeof(tcp_mask->hdr.tcp_flags));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.rx_win,
		       sizeof(tcp_mask->hdr.rx_win));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.cksum,
		       sizeof(tcp_mask->hdr.cksum));
		memcpy(hdr_field[mdx++].mask, &tcp_mask->hdr.tcp_urp,
		       sizeof(tcp_mask->hdr.tcp_urp));
	}
	*field_idx = idx; /* add number of TCP header elements */

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_TCP);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_L4);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP);
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_L4);
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vxlan Header. */
int32_t
ulp_rte_vxlan_hdr_handler(const struct rte_flow_item *item,
			  struct ulp_rte_hdr_bitmap *hdrbitmap,
			  struct ulp_rte_hdr_field *hdr_field,
			  uint32_t *field_idx,
			  uint32_t *vlan_idx __rte_unused)
{
	const struct rte_flow_item_vxlan *vxlan_spec, *vxlan_mask;
	uint32_t idx = *field_idx;
	uint32_t mdx = *field_idx;

	vxlan_spec = item->spec;
	vxlan_mask = item->mask;

	/*
	 * Copy the rte_flow_item for vxlan into hdr_field using vxlan
	 * header fields
	 */
	if (vxlan_spec) {
		hdr_field[idx].size = sizeof(vxlan_spec->flags);
		memcpy(hdr_field[idx++].spec, &vxlan_spec->flags,
		       sizeof(vxlan_spec->flags));
		hdr_field[idx].size = sizeof(vxlan_spec->rsvd0);
		memcpy(hdr_field[idx++].spec, &vxlan_spec->rsvd0,
		       sizeof(vxlan_spec->rsvd0));
		hdr_field[idx].size = sizeof(vxlan_spec->vni);
		memcpy(hdr_field[idx++].spec, &vxlan_spec->vni,
		       sizeof(vxlan_spec->vni));
		hdr_field[idx].size = sizeof(vxlan_spec->rsvd1);
		memcpy(hdr_field[idx++].spec, &vxlan_spec->rsvd1,
		       sizeof(vxlan_spec->rsvd1));
	} else {
		idx += BNXT_ULP_PROTO_HDR_VXLAN_NUM;
	}

	if (vxlan_mask) {
		memcpy(hdr_field[mdx++].mask, &vxlan_mask->flags,
		       sizeof(vxlan_mask->flags));
		memcpy(hdr_field[mdx++].mask, &vxlan_mask->rsvd0,
		       sizeof(vxlan_mask->rsvd0));
		memcpy(hdr_field[mdx++].mask, &vxlan_mask->vni,
		       sizeof(vxlan_mask->vni));
		memcpy(hdr_field[mdx++].mask, &vxlan_mask->rsvd1,
		       sizeof(vxlan_mask->rsvd1));
	}
	*field_idx = idx; /* Add number of vxlan header elements */

	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(hdrbitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item void Header */
int32_t
ulp_rte_void_hdr_handler(const struct rte_flow_item *item __rte_unused,
			 struct ulp_rte_hdr_bitmap *hdr_bit __rte_unused,
			 struct ulp_rte_hdr_field *hdr_field __rte_unused,
			 uint32_t *field_idx __rte_unused,
			 uint32_t *vlan_idx __rte_unused)
{
	return BNXT_TF_RC_SUCCESS;
}
