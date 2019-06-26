/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"
#include "otx2_flow.h"

const struct rte_flow_item *
otx2_flow_skip_void_and_any_items(const struct rte_flow_item *pattern)
{
	while ((pattern->type == RTE_FLOW_ITEM_TYPE_VOID) ||
	       (pattern->type == RTE_FLOW_ITEM_TYPE_ANY))
		pattern++;

	return pattern;
}

/*
 * Tunnel+ESP, Tunnel+ICMP4/6, Tunnel+TCP, Tunnel+UDP,
 * Tunnel+SCTP
 */
int
otx2_flow_parse_lh(struct otx2_parse_state *pst)
{
	struct otx2_flow_item_info info;
	char hw_mask[64];
	int lid, lt;
	int rc;

	if (!pst->tunnel)
		return 0;

	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LH;

	switch (pst->pattern->type) {
	case RTE_FLOW_ITEM_TYPE_UDP:
		lt = NPC_LT_LH_TU_UDP;
		info.def_mask = &rte_flow_item_udp_mask;
		info.len = sizeof(struct rte_flow_item_udp);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		lt = NPC_LT_LH_TU_TCP;
		info.def_mask = &rte_flow_item_tcp_mask;
		info.len = sizeof(struct rte_flow_item_tcp);
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		lt = NPC_LT_LH_TU_SCTP;
		info.def_mask = &rte_flow_item_sctp_mask;
		info.len = sizeof(struct rte_flow_item_sctp);
		break;
	case RTE_FLOW_ITEM_TYPE_ESP:
		lt = NPC_LT_LH_TU_ESP;
		info.def_mask = &rte_flow_item_esp_mask;
		info.len = sizeof(struct rte_flow_item_esp);
		break;
	default:
		return 0;
	}

	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	return otx2_flow_update_parse_state(pst, &info, lid, lt, 0);
}

/* Tunnel+IPv4, Tunnel+IPv6 */
int
otx2_flow_parse_lg(struct otx2_parse_state *pst)
{
	struct otx2_flow_item_info info;
	char hw_mask[64];
	int lid, lt;
	int rc;

	if (!pst->tunnel)
		return 0;

	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LG;

	if (pst->pattern->type == RTE_FLOW_ITEM_TYPE_IPV4) {
		lt = NPC_LT_LG_TU_IP;
		info.def_mask = &rte_flow_item_ipv4_mask;
		info.len = sizeof(struct rte_flow_item_ipv4);
	} else if (pst->pattern->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		lt = NPC_LT_LG_TU_IP6;
		info.def_mask = &rte_flow_item_ipv6_mask;
		info.len = sizeof(struct rte_flow_item_ipv6);
	} else {
		/* There is no tunneled IP header */
		return 0;
	}

	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	return otx2_flow_update_parse_state(pst, &info, lid, lt, 0);
}

/* Tunnel+Ether */
int
otx2_flow_parse_lf(struct otx2_parse_state *pst)
{
	const struct rte_flow_item *pattern, *last_pattern;
	struct rte_flow_item_eth hw_mask;
	struct otx2_flow_item_info info;
	int lid, lt, lflags;
	int nr_vlans = 0;
	int rc;

	/* We hit this layer if there is a tunneling protocol */
	if (!pst->tunnel)
		return 0;

	if (pst->pattern->type != RTE_FLOW_ITEM_TYPE_ETH)
		return 0;

	lid = NPC_LID_LF;
	lt = NPC_LT_LF_TU_ETHER;
	lflags = 0;

	info.def_mask = &rte_flow_item_vlan_mask;
	/* No match support for vlan tags */
	info.hw_mask = NULL;
	info.len = sizeof(struct rte_flow_item_vlan);
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;

	/* Look ahead and find out any VLAN tags. These can be
	 * detected but no data matching is available.
	 */
	last_pattern = pst->pattern;
	pattern = pst->pattern + 1;
	pattern = otx2_flow_skip_void_and_any_items(pattern);
	while (pattern->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		nr_vlans++;
		rc = otx2_flow_parse_item_basic(pattern, &info, pst->error);
		if (rc != 0)
			return rc;
		last_pattern = pattern;
		pattern++;
		pattern = otx2_flow_skip_void_and_any_items(pattern);
	}
	otx2_npc_dbg("Nr_vlans = %d", nr_vlans);
	switch (nr_vlans) {
	case 0:
		break;
	case 1:
		lflags = NPC_F_TU_ETHER_CTAG;
		break;
	case 2:
		lflags = NPC_F_TU_ETHER_STAG_CTAG;
		break;
	default:
		rte_flow_error_set(pst->error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   last_pattern,
				   "more than 2 vlans with tunneled Ethernet "
				   "not supported");
		return -rte_errno;
	}

	info.def_mask = &rte_flow_item_eth_mask;
	info.hw_mask = &hw_mask;
	info.len = sizeof(struct rte_flow_item_eth);
	info.hw_hdr_len = 0;
	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	info.spec = NULL;
	info.mask = NULL;

	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	pst->pattern = last_pattern;

	return otx2_flow_update_parse_state(pst, &info, lid, lt, lflags);
}

int
otx2_flow_parse_le(struct otx2_parse_state *pst)
{
	/*
	 * We are positioned at UDP. Scan ahead and look for
	 * UDP encapsulated tunnel protocols. If available,
	 * parse them. In that case handle this:
	 *	- RTE spec assumes we point to tunnel header.
	 *	- NPC parser provides offset from UDP header.
	 */

	/*
	 * Note: Add support to GENEVE, VXLAN_GPE when we
	 * upgrade DPDK
	 *
	 * Note: Better to split flags into two nibbles:
	 *	- Higher nibble can have flags
	 *	- Lower nibble to further enumerate protocols
	 *	  and have flags based extraction
	 */
	const struct rte_flow_item *pattern = pst->pattern;
	struct otx2_flow_item_info info;
	int lid, lt, lflags;
	char hw_mask[64];
	int rc;

	if (pst->tunnel)
		return 0;

	if (pst->pattern->type == RTE_FLOW_ITEM_TYPE_MPLS)
		return otx2_flow_parse_mpls(pst, NPC_LID_LE);

	info.spec = NULL;
	info.mask = NULL;
	info.hw_mask = NULL;
	info.def_mask = NULL;
	info.len = 0;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LE;
	lflags = 0;

	/* Ensure we are not matching anything in UDP */
	rc = otx2_flow_parse_item_basic(pattern, &info, pst->error);
	if (rc)
		return rc;

	info.hw_mask = &hw_mask;
	pattern = otx2_flow_skip_void_and_any_items(pattern);
	otx2_npc_dbg("Pattern->type = %d", pattern->type);
	switch (pattern->type) {
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		lflags = NPC_F_UDP_VXLAN;
		info.def_mask = &rte_flow_item_vxlan_mask;
		info.len = sizeof(struct rte_flow_item_vxlan);
		lt = NPC_LT_LE_VXLAN;
		break;
	case RTE_FLOW_ITEM_TYPE_GTPC:
		lflags = NPC_F_UDP_GTP_GTPC;
		info.def_mask = &rte_flow_item_gtp_mask;
		info.len = sizeof(struct rte_flow_item_gtp);
		lt = NPC_LT_LE_GTPC;
		break;
	case RTE_FLOW_ITEM_TYPE_GTPU:
		lflags = NPC_F_UDP_GTP_GTPU_G_PDU;
		info.def_mask = &rte_flow_item_gtp_mask;
		info.len = sizeof(struct rte_flow_item_gtp);
		lt = NPC_LT_LE_GTPU;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		lflags = NPC_F_UDP_GENEVE;
		info.def_mask = &rte_flow_item_geneve_mask;
		info.len = sizeof(struct rte_flow_item_geneve);
		lt = NPC_LT_LE_GENEVE;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		lflags = NPC_F_UDP_VXLANGPE;
		info.def_mask = &rte_flow_item_vxlan_gpe_mask;
		info.len = sizeof(struct rte_flow_item_vxlan_gpe);
		lt = NPC_LT_LE_VXLANGPE;
		break;
	default:
		return 0;
	}

	pst->tunnel = 1;

	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	rc = otx2_flow_parse_item_basic(pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	return otx2_flow_update_parse_state(pst, &info, lid, lt, lflags);
}

static int
flow_parse_mpls_label_stack(struct otx2_parse_state *pst, int *flag)
{
	int nr_labels = 0;
	const struct rte_flow_item *pattern = pst->pattern;
	struct otx2_flow_item_info info;
	int rc;
	uint8_t flag_list[] = {0, NPC_F_MPLS_2_LABELS,
		NPC_F_MPLS_3_LABELS, NPC_F_MPLS_4_LABELS};

	/*
	 * pst->pattern points to first MPLS label. We only check
	 * that subsequent labels do not have anything to match.
	 */
	info.def_mask = &rte_flow_item_mpls_mask;
	info.hw_mask = NULL;
	info.len = sizeof(struct rte_flow_item_mpls);
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;

	while (pattern->type == RTE_FLOW_ITEM_TYPE_MPLS) {
		nr_labels++;

		/* Basic validation of 2nd/3rd/4th mpls item */
		if (nr_labels > 1) {
			rc = otx2_flow_parse_item_basic(pattern, &info,
							pst->error);
			if (rc != 0)
				return rc;
		}
		pst->last_pattern = pattern;
		pattern++;
		pattern = otx2_flow_skip_void_and_any_items(pattern);
	}

	if (nr_labels > 4) {
		rte_flow_error_set(pst->error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   pst->last_pattern,
				   "more than 4 mpls labels not supported");
		return -rte_errno;
	}

	*flag = flag_list[nr_labels - 1];
	return 0;
}

int
otx2_flow_parse_mpls(struct otx2_parse_state *pst, int lid)
{
	/* Find number of MPLS labels */
	struct rte_flow_item_mpls hw_mask;
	struct otx2_flow_item_info info;
	int lt, lflags;
	int rc;

	lflags = 0;

	if (lid == NPC_LID_LC)
		lt = NPC_LT_LC_MPLS;
	else if (lid == NPC_LID_LD)
		lt = NPC_LT_LD_TU_MPLS_IN_IP;
	else
		lt = NPC_LT_LE_TU_MPLS_IN_UDP;

	/* Prepare for parsing the first item */
	info.def_mask = &rte_flow_item_mpls_mask;
	info.hw_mask = &hw_mask;
	info.len = sizeof(struct rte_flow_item_mpls);
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;

	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	/*
	 * Parse for more labels.
	 * This sets lflags and pst->last_pattern correctly.
	 */
	rc = flow_parse_mpls_label_stack(pst, &lflags);
	if (rc != 0)
		return rc;

	pst->tunnel = 1;
	pst->pattern = pst->last_pattern;

	return otx2_flow_update_parse_state(pst, &info, lid, lt, lflags);
}

/*
 * ICMP, ICMP6, UDP, TCP, SCTP, VXLAN, GRE, NVGRE,
 * GTP, GTPC, GTPU, ESP
 *
 * Note: UDP tunnel protocols are identified by flags.
 *       LPTR for these protocol still points to UDP
 *       header. Need flag based extraction to support
 *       this.
 */
int
otx2_flow_parse_ld(struct otx2_parse_state *pst)
{
	char hw_mask[NPC_MAX_EXTRACT_DATA_LEN];
	struct otx2_flow_item_info info;
	int lid, lt, lflags;
	int rc;

	if (pst->tunnel) {
		/* We have already parsed MPLS or IPv4/v6 followed
		 * by MPLS or IPv4/v6. Subsequent TCP/UDP etc
		 * would be parsed as tunneled versions. Skip
		 * this layer, except for tunneled MPLS. If LC is
		 * MPLS, we have anyway skipped all stacked MPLS
		 * labels.
		 */
		if (pst->pattern->type == RTE_FLOW_ITEM_TYPE_MPLS)
			return otx2_flow_parse_mpls(pst, NPC_LID_LD);
		return 0;
	}
	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.def_mask = NULL;
	info.len = 0;
	info.hw_hdr_len = 0;

	lid = NPC_LID_LD;
	lflags = 0;

	otx2_npc_dbg("Pst->pattern->type = %d", pst->pattern->type);
	switch (pst->pattern->type) {
	case RTE_FLOW_ITEM_TYPE_ICMP:
		if (pst->lt[NPC_LID_LC] == NPC_LT_LC_IP6)
			lt = NPC_LT_LD_ICMP6;
		else
			lt = NPC_LT_LD_ICMP;
		info.def_mask = &rte_flow_item_icmp_mask;
		info.len = sizeof(struct rte_flow_item_icmp);
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		lt = NPC_LT_LD_UDP;
		info.def_mask = &rte_flow_item_udp_mask;
		info.len = sizeof(struct rte_flow_item_udp);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		lt = NPC_LT_LD_TCP;
		info.def_mask = &rte_flow_item_tcp_mask;
		info.len = sizeof(struct rte_flow_item_tcp);
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		lt = NPC_LT_LD_SCTP;
		info.def_mask = &rte_flow_item_sctp_mask;
		info.len = sizeof(struct rte_flow_item_sctp);
		break;
	case RTE_FLOW_ITEM_TYPE_ESP:
		lt = NPC_LT_LD_ESP;
		info.def_mask = &rte_flow_item_esp_mask;
		info.len = sizeof(struct rte_flow_item_esp);
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		lt = NPC_LT_LD_GRE;
		info.def_mask = &rte_flow_item_gre_mask;
		info.len = sizeof(struct rte_flow_item_gre);
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		lt = NPC_LT_LD_GRE;
		lflags = NPC_F_GRE_NVGRE;
		info.def_mask = &rte_flow_item_nvgre_mask;
		info.len = sizeof(struct rte_flow_item_nvgre);
		/* Further IP/Ethernet are parsed as tunneled */
		pst->tunnel = 1;
		break;
	default:
		return 0;
	}

	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	return otx2_flow_update_parse_state(pst, &info, lid, lt, lflags);
}
