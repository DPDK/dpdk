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
	uint32_t gre_key_mask = 0xffffffff;
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
	case RTE_FLOW_ITEM_TYPE_GRE_KEY:
		lt = NPC_LT_LD_GRE;
		info.def_mask = &gre_key_mask;
		info.len = sizeof(gre_key_mask);
		info.hw_hdr_len = 4;
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		lt = NPC_LT_LD_NVGRE;
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

static inline void
flow_check_lc_ip_tunnel(struct otx2_parse_state *pst)
{
	const struct rte_flow_item *pattern = pst->pattern + 1;

	pattern = otx2_flow_skip_void_and_any_items(pattern);
	if (pattern->type == RTE_FLOW_ITEM_TYPE_MPLS ||
	    pattern->type == RTE_FLOW_ITEM_TYPE_IPV4 ||
	    pattern->type == RTE_FLOW_ITEM_TYPE_IPV6)
		pst->tunnel = 1;
}

/* Outer IPv4, Outer IPv6, MPLS, ARP */
int
otx2_flow_parse_lc(struct otx2_parse_state *pst)
{
	uint8_t hw_mask[NPC_MAX_EXTRACT_DATA_LEN];
	struct otx2_flow_item_info info;
	int lid, lt;
	int rc;

	if (pst->pattern->type == RTE_FLOW_ITEM_TYPE_MPLS)
		return otx2_flow_parse_mpls(pst, NPC_LID_LC);

	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LC;

	switch (pst->pattern->type) {
	case RTE_FLOW_ITEM_TYPE_IPV4:
		lt = NPC_LT_LC_IP;
		info.def_mask = &rte_flow_item_ipv4_mask;
		info.len = sizeof(struct rte_flow_item_ipv4);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		lid = NPC_LID_LC;
		lt = NPC_LT_LC_IP6;
		info.def_mask = &rte_flow_item_ipv6_mask;
		info.len = sizeof(struct rte_flow_item_ipv6);
		break;
	case RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4:
		lt = NPC_LT_LC_ARP;
		info.def_mask = &rte_flow_item_arp_eth_ipv4_mask;
		info.len = sizeof(struct rte_flow_item_arp_eth_ipv4);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6_EXT:
		lid = NPC_LID_LC;
		lt = NPC_LT_LC_IP6_EXT;
		info.def_mask = &rte_flow_item_ipv6_ext_mask;
		info.len = sizeof(struct rte_flow_item_ipv6_ext);
		info.hw_hdr_len = 40;
		break;
	default:
		/* No match at this layer */
		return 0;
	}

	/* Identify if IP tunnels MPLS or IPv4/v6 */
	flow_check_lc_ip_tunnel(pst);

	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	return otx2_flow_update_parse_state(pst, &info, lid, lt, 0);
}

/* VLAN, ETAG */
int
otx2_flow_parse_lb(struct otx2_parse_state *pst)
{
	const struct rte_flow_item *pattern = pst->pattern;
	const struct rte_flow_item *last_pattern;
	char hw_mask[NPC_MAX_EXTRACT_DATA_LEN];
	struct otx2_flow_item_info info;
	int lid, lt, lflags;
	int nr_vlans = 0;
	int rc;

	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = NPC_TPID_LENGTH;

	lid = NPC_LID_LB;
	lflags = 0;
	last_pattern = pattern;

	if (pst->pattern->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		/* RTE vlan is either 802.1q or 802.1ad,
		 * this maps to either CTAG/STAG. We need to decide
		 * based on number of VLANS present. Matching is
		 * supported on first tag only.
		 */
		info.def_mask = &rte_flow_item_vlan_mask;
		info.hw_mask = NULL;
		info.len = sizeof(struct rte_flow_item_vlan);

		pattern = pst->pattern;
		while (pattern->type == RTE_FLOW_ITEM_TYPE_VLAN) {
			nr_vlans++;

			/* Basic validation of 2nd/3rd vlan item */
			if (nr_vlans > 1) {
				otx2_npc_dbg("Vlans  = %d", nr_vlans);
				rc = otx2_flow_parse_item_basic(pattern, &info,
								pst->error);
				if (rc != 0)
					return rc;
			}
			last_pattern = pattern;
			pattern++;
			pattern = otx2_flow_skip_void_and_any_items(pattern);
		}

		switch (nr_vlans) {
		case 1:
			lt = NPC_LT_LB_CTAG;
			break;
		case 2:
			lt = NPC_LT_LB_STAG_QINQ;
			lflags = NPC_F_STAG_CTAG;
			break;
		case 3:
			lt = NPC_LT_LB_STAG_QINQ;
			lflags = NPC_F_STAG_STAG_CTAG;
			break;
		default:
			rte_flow_error_set(pst->error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   last_pattern,
					   "more than 3 vlans not supported");
			return -rte_errno;
		}
	} else if (pst->pattern->type == RTE_FLOW_ITEM_TYPE_E_TAG) {
		/* we can support ETAG and match a subsequent CTAG
		 * without any matching support.
		 */
		lt = NPC_LT_LB_ETAG;
		lflags = 0;

		last_pattern = pst->pattern;
		pattern = otx2_flow_skip_void_and_any_items(pst->pattern + 1);
		if (pattern->type == RTE_FLOW_ITEM_TYPE_VLAN) {
			info.def_mask = &rte_flow_item_vlan_mask;
			/* set supported mask to NULL for vlan tag */
			info.hw_mask = NULL;
			info.len = sizeof(struct rte_flow_item_vlan);
			rc = otx2_flow_parse_item_basic(pattern, &info,
							pst->error);
			if (rc != 0)
				return rc;

			lflags = NPC_F_ETAG_CTAG;
			last_pattern = pattern;
		}

		info.def_mask = &rte_flow_item_e_tag_mask;
		info.len = sizeof(struct rte_flow_item_e_tag);
	} else {
		return 0;
	}

	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);

	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc != 0)
		return rc;

	/* Point pattern to last item consumed */
	pst->pattern = last_pattern;
	return otx2_flow_update_parse_state(pst, &info, lid, lt, lflags);
}

int
otx2_flow_parse_la(struct otx2_parse_state *pst)
{
	struct rte_flow_item_eth hw_mask;
	struct otx2_flow_item_info info;
	int lid, lt;
	int rc;

	/* Identify the pattern type into lid, lt */
	if (pst->pattern->type != RTE_FLOW_ITEM_TYPE_ETH)
		return 0;

	lid = NPC_LID_LA;
	lt = NPC_LT_LA_ETHER;
	info.hw_hdr_len = 0;

	if (pst->flow->nix_intf == NIX_INTF_TX) {
		lt = NPC_LT_LA_IH_NIX_ETHER;
		info.hw_hdr_len = NPC_IH_LENGTH;
		if (pst->npc->switch_header_type == OTX2_PRIV_FLAGS_HIGIG) {
			lt = NPC_LT_LA_IH_NIX_HIGIG2_ETHER;
			info.hw_hdr_len += NPC_HIGIG2_LENGTH;
		}
	} else {
		if (pst->npc->switch_header_type == OTX2_PRIV_FLAGS_HIGIG) {
			lt = NPC_LT_LA_HIGIG2_ETHER;
			info.hw_hdr_len = NPC_HIGIG2_LENGTH;
		}
	}

	/* Prepare for parsing the item */
	info.def_mask = &rte_flow_item_eth_mask;
	info.hw_mask = &hw_mask;
	info.len = sizeof(struct rte_flow_item_eth);
	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	info.spec = NULL;
	info.mask = NULL;

	/* Basic validation of item parameters */
	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc)
		return rc;

	/* Update pst if not validate only? clash check? */
	return otx2_flow_update_parse_state(pst, &info, lid, lt, 0);
}

int
otx2_flow_parse_higig2_hdr(struct otx2_parse_state *pst)
{
	struct rte_flow_item_higig2_hdr hw_mask;
	struct otx2_flow_item_info info;
	int lid, lt;
	int rc;

	/* Identify the pattern type into lid, lt */
	if (pst->pattern->type != RTE_FLOW_ITEM_TYPE_HIGIG2)
		return 0;

	lid = NPC_LID_LA;
	lt = NPC_LT_LA_HIGIG2_ETHER;
	info.hw_hdr_len = 0;

	if (pst->flow->nix_intf == NIX_INTF_TX) {
		lt = NPC_LT_LA_IH_NIX_HIGIG2_ETHER;
		info.hw_hdr_len = NPC_IH_LENGTH;
	}

	/* Prepare for parsing the item */
	info.def_mask = &rte_flow_item_higig2_hdr_mask;
	info.hw_mask = &hw_mask;
	info.len = sizeof(struct rte_flow_item_higig2_hdr);
	otx2_flow_get_hw_supp_mask(pst, &info, lid, lt);
	info.spec = NULL;
	info.mask = NULL;

	/* Basic validation of item parameters */
	rc = otx2_flow_parse_item_basic(pst->pattern, &info, pst->error);
	if (rc)
		return rc;

	/* Update pst if not validate only? clash check? */
	return otx2_flow_update_parse_state(pst, &info, lid, lt, 0);
}

static int
parse_rss_action(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_action *act,
		 struct rte_flow_error *error)
{
	struct otx2_eth_dev *hw = dev->data->dev_private;
	struct otx2_rss_info *rss_info = &hw->rss_info;
	const struct rte_flow_action_rss *rss;
	uint32_t i;

	rss = (const struct rte_flow_action_rss *)act->conf;

	/* Not supported */
	if (attr->egress) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
					  attr, "No support of RSS in egress");
	}

	if (dev->data->dev_conf.rxmode.mq_mode != ETH_MQ_RX_RSS)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  act, "multi-queue mode is disabled");

	/* Parse RSS related parameters from configuration */
	if (!rss || !rss->queue_num)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  act, "no valid queues");

	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, act,
					  "non-default RSS hash functions"
					  " are not supported");

	if (rss->key_len && rss->key_len > RTE_DIM(rss_info->key))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, act,
					  "RSS hash key too large");

	if (rss->queue_num > rss_info->rss_size)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "too many queues for RSS context");

	for (i = 0; i < rss->queue_num; i++) {
		if (rss->queue[i] >= dev->data->nb_rx_queues)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  act,
						  "queue id > max number"
						  " of queues");
	}

	return 0;
}

int
otx2_flow_parse_actions(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_action actions[],
			struct rte_flow_error *error,
			struct rte_flow *flow)
{
	struct otx2_eth_dev *hw = dev->data->dev_private;
	struct otx2_npc_flow_info *npc = &hw->npc_flow;
	const struct rte_flow_action_count *act_count;
	const struct rte_flow_action_mark *act_mark;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_vf *vf_act;
	const char *errmsg = NULL;
	int sel_act, req_act = 0;
	uint16_t pf_func, vf_id;
	int errcode = 0;
	int mark = 0;
	int rq = 0;

	/* Initialize actions */
	flow->ctr_id = NPC_COUNTER_NONE;
	pf_func = otx2_pfvf_func(hw->pf, hw->vf);

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		otx2_npc_dbg("Action type = %d", actions->type);

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			act_mark =
			    (const struct rte_flow_action_mark *)actions->conf;

			/* We have only 16 bits. Use highest val for flag */
			if (act_mark->id > (OTX2_FLOW_FLAG_VAL - 2)) {
				errmsg = "mark value must be < 0xfffe";
				errcode = ENOTSUP;
				goto err_exit;
			}
			mark = act_mark->id + 1;
			req_act |= OTX2_FLOW_ACT_MARK;
			rte_atomic32_inc(&npc->mark_actions);
			break;

		case RTE_FLOW_ACTION_TYPE_FLAG:
			mark = OTX2_FLOW_FLAG_VAL;
			req_act |= OTX2_FLOW_ACT_FLAG;
			rte_atomic32_inc(&npc->mark_actions);
			break;

		case RTE_FLOW_ACTION_TYPE_COUNT:
			act_count =
				(const struct rte_flow_action_count *)
				actions->conf;

			if (act_count->shared == 1) {
				errmsg = "Shared Counters not supported";
				errcode = ENOTSUP;
				goto err_exit;
			}
			/* Indicates, need a counter */
			flow->ctr_id = 1;
			req_act |= OTX2_FLOW_ACT_COUNT;
			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			req_act |= OTX2_FLOW_ACT_DROP;
			break;

		case RTE_FLOW_ACTION_TYPE_PF:
			req_act |= OTX2_FLOW_ACT_PF;
			pf_func &= (0xfc00);
			break;

		case RTE_FLOW_ACTION_TYPE_VF:
			vf_act = (const struct rte_flow_action_vf *)
				actions->conf;
			req_act |= OTX2_FLOW_ACT_VF;
			if (vf_act->original == 0) {
				vf_id = vf_act->id & RVU_PFVF_FUNC_MASK;
				if (vf_id  >= hw->maxvf) {
					errmsg = "invalid vf specified";
					errcode = EINVAL;
					goto err_exit;
				}
				pf_func &= (0xfc00);
				pf_func = (pf_func | (vf_id + 1));
			}
			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			/* Applicable only to ingress flow */
			act_q = (const struct rte_flow_action_queue *)
				actions->conf;
			rq = act_q->index;
			if (rq >= dev->data->nb_rx_queues) {
				errmsg = "invalid queue index";
				errcode = EINVAL;
				goto err_exit;
			}
			req_act |= OTX2_FLOW_ACT_QUEUE;
			break;

		case RTE_FLOW_ACTION_TYPE_RSS:
			errcode = parse_rss_action(dev,	attr, actions, error);
			if (errcode)
				return -rte_errno;

			req_act |= OTX2_FLOW_ACT_RSS;
			break;

		case RTE_FLOW_ACTION_TYPE_SECURITY:
			/* Assumes user has already configured security
			 * session for this flow. Associated conf is
			 * opaque. When RTE security is implemented for otx2,
			 * we need to verify that for specified security
			 * session:
			 *  action_type ==
			 *    RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL &&
			 *  session_protocol ==
			 *    RTE_SECURITY_PROTOCOL_IPSEC
			 *
			 * RSS is not supported with inline ipsec. Get the
			 * rq from associated conf, or make
			 * RTE_FLOW_ACTION_TYPE_QUEUE compulsory with this
			 * action.
			 * Currently, rq = 0 is assumed.
			 */
			req_act |= OTX2_FLOW_ACT_SEC;
			rq = 0;
			break;
		default:
			errmsg = "Unsupported action specified";
			errcode = ENOTSUP;
			goto err_exit;
		}
	}

	/* Check if actions specified are compatible */
	if (attr->egress) {
		/* Only DROP/COUNT is supported */
		if (!(req_act & OTX2_FLOW_ACT_DROP)) {
			errmsg = "DROP is required action for egress";
			errcode = EINVAL;
			goto err_exit;
		} else if (req_act & ~(OTX2_FLOW_ACT_DROP |
				       OTX2_FLOW_ACT_COUNT)) {
			errmsg = "Unsupported action specified";
			errcode = ENOTSUP;
			goto err_exit;
		}
		flow->npc_action = NIX_TX_ACTIONOP_DROP;
		goto set_pf_func;
	}

	/* We have already verified the attr, this is ingress.
	 * - Exactly one terminating action is supported
	 * - Exactly one of MARK or FLAG is supported
	 * - If terminating action is DROP, only count is valid.
	 */
	sel_act = req_act & OTX2_FLOW_ACT_TERM;
	if ((sel_act & (sel_act - 1)) != 0) {
		errmsg = "Only one terminating action supported";
		errcode = EINVAL;
		goto err_exit;
	}

	if (req_act & OTX2_FLOW_ACT_DROP) {
		sel_act = req_act & ~OTX2_FLOW_ACT_COUNT;
		if ((sel_act & (sel_act - 1)) != 0) {
			errmsg = "Only COUNT action is supported "
				"with DROP ingress action";
			errcode = ENOTSUP;
			goto err_exit;
		}
	}

	if ((req_act & (OTX2_FLOW_ACT_FLAG | OTX2_FLOW_ACT_MARK))
	    == (OTX2_FLOW_ACT_FLAG | OTX2_FLOW_ACT_MARK)) {
		errmsg = "Only one of FLAG or MARK action is supported";
		errcode = ENOTSUP;
		goto err_exit;
	}

	/* Set NIX_RX_ACTIONOP */
	if (req_act & (OTX2_FLOW_ACT_PF | OTX2_FLOW_ACT_VF)) {
		flow->npc_action = NIX_RX_ACTIONOP_UCAST;
		if (req_act & OTX2_FLOW_ACT_QUEUE)
			flow->npc_action |= (uint64_t)rq << 20;
	} else if (req_act & OTX2_FLOW_ACT_DROP) {
		flow->npc_action = NIX_RX_ACTIONOP_DROP;
	} else if (req_act & OTX2_FLOW_ACT_QUEUE) {
		flow->npc_action = NIX_RX_ACTIONOP_UCAST;
		flow->npc_action |= (uint64_t)rq << 20;
	} else if (req_act & OTX2_FLOW_ACT_RSS) {
		/* When user added a rule for rss, first we will add the
		 *rule in MCAM and then update the action, once if we have
		 *FLOW_KEY_ALG index. So, till we update the action with
		 *flow_key_alg index, set the action to drop.
		 */
		if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS)
			flow->npc_action = NIX_RX_ACTIONOP_DROP;
		else
			flow->npc_action = NIX_RX_ACTIONOP_UCAST;
	} else if (req_act & OTX2_FLOW_ACT_SEC) {
		flow->npc_action = NIX_RX_ACTIONOP_UCAST_IPSEC;
		flow->npc_action |= (uint64_t)rq << 20;
	} else if (req_act & (OTX2_FLOW_ACT_FLAG | OTX2_FLOW_ACT_MARK)) {
		flow->npc_action = NIX_RX_ACTIONOP_UCAST;
	} else if (req_act & OTX2_FLOW_ACT_COUNT) {
		/* Keep OTX2_FLOW_ACT_COUNT always at the end
		 * This is default action, when user specify only
		 * COUNT ACTION
		 */
		flow->npc_action = NIX_RX_ACTIONOP_UCAST;
	} else {
		/* Should never reach here */
		errmsg = "Invalid action specified";
		errcode = EINVAL;
		goto err_exit;
	}

	if (mark)
		flow->npc_action |= (uint64_t)mark << 40;

	if (rte_atomic32_read(&npc->mark_actions) == 1) {
		hw->rx_offload_flags |=
			NIX_RX_OFFLOAD_MARK_UPDATE_F;
		otx2_eth_set_rx_function(dev);
	}

set_pf_func:
	/* Ideally AF must ensure that correct pf_func is set */
	flow->npc_action |= (uint64_t)pf_func << 4;

	return 0;

err_exit:
	rte_flow_error_set(error, errcode,
			   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
			   errmsg);
	return -rte_errno;
}
