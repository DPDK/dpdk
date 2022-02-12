/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "roc_api.h"
#include "roc_priv.h"

const struct roc_npc_item_info *
npc_parse_skip_void_and_any_items(const struct roc_npc_item_info *pattern)
{
	while ((pattern->type == ROC_NPC_ITEM_TYPE_VOID) ||
	       (pattern->type == ROC_NPC_ITEM_TYPE_ANY))
		pattern++;

	return pattern;
}

int
npc_parse_meta_items(struct npc_parse_state *pst)
{
	PLT_SET_USED(pst);
	return 0;
}

static int
npc_flow_raw_item_prepare(const struct roc_npc_flow_item_raw *raw_spec,
			  const struct roc_npc_flow_item_raw *raw_mask,
			  struct npc_parse_item_info *info, uint8_t *spec_buf,
			  uint8_t *mask_buf)
{

	memset(spec_buf, 0, NPC_MAX_RAW_ITEM_LEN);
	memset(mask_buf, 0, NPC_MAX_RAW_ITEM_LEN);

	memcpy(spec_buf + raw_spec->offset, raw_spec->pattern,
	       raw_spec->length);

	if (raw_mask && raw_mask->pattern) {
		memcpy(mask_buf + raw_spec->offset, raw_mask->pattern,
		       raw_spec->length);
	} else {
		memset(mask_buf + raw_spec->offset, 0xFF, raw_spec->length);
	}

	info->len = NPC_MAX_RAW_ITEM_LEN;
	info->spec = spec_buf;
	info->mask = mask_buf;
	return 0;
}

int
npc_parse_pre_l2(struct npc_parse_state *pst)
{
	uint8_t raw_spec_buf[NPC_MAX_RAW_ITEM_LEN] = {0};
	uint8_t raw_mask_buf[NPC_MAX_RAW_ITEM_LEN] = {0};
	uint8_t hw_mask[NPC_MAX_EXTRACT_HW_LEN] = {0};
	const struct roc_npc_flow_item_raw *raw_spec;
	struct npc_parse_item_info info;
	int lid, lt, len;
	int rc;

	if (pst->npc->switch_header_type != ROC_PRIV_FLAGS_PRE_L2)
		return 0;

	/* Identify the pattern type into lid, lt */
	if (pst->pattern->type != ROC_NPC_ITEM_TYPE_RAW)
		return 0;

	lid = NPC_LID_LA;
	lt = NPC_LT_LA_CUSTOM_PRE_L2_ETHER;
	info.hw_hdr_len = 0;

	raw_spec = pst->pattern->spec;
	len = raw_spec->length + raw_spec->offset;
	if (len > NPC_MAX_RAW_ITEM_LEN)
		return -EINVAL;

	if (raw_spec->relative == 0 || raw_spec->search || raw_spec->limit ||
	    raw_spec->offset < 0)
		return -EINVAL;

	npc_flow_raw_item_prepare(
		(const struct roc_npc_flow_item_raw *)pst->pattern->spec,
		(const struct roc_npc_flow_item_raw *)pst->pattern->mask, &info,
		raw_spec_buf, raw_mask_buf);

	info.hw_mask = &hw_mask;
	npc_get_hw_supp_mask(pst, &info, lid, lt);

	/* Basic validation of item parameters */
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc)
		return rc;

	/* Update pst if not validate only? clash check? */
	return npc_update_parse_state(pst, &info, lid, lt, 0);
}

int
npc_parse_cpt_hdr(struct npc_parse_state *pst)
{
	uint8_t hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt;
	int rc;

	/* Identify the pattern type into lid, lt */
	if (pst->pattern->type != ROC_NPC_ITEM_TYPE_CPT_HDR)
		return 0;

	lid = NPC_LID_LA;
	lt = NPC_LT_LA_CPT_HDR;
	info.hw_hdr_len = 0;

	/* Prepare for parsing the item */
	info.def_mask = NULL;
	info.hw_mask = &hw_mask;
	info.len = pst->pattern->size;
	npc_get_hw_supp_mask(pst, &info, lid, lt);
	info.spec = NULL;
	info.mask = NULL;

	/* Basic validation of item parameters */
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc)
		return rc;

	/* Update pst if not validate only? clash check? */
	return npc_update_parse_state(pst, &info, lid, lt, 0);
}

int
npc_parse_higig2_hdr(struct npc_parse_state *pst)
{
	uint8_t hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt;
	int rc;

	/* Identify the pattern type into lid, lt */
	if (pst->pattern->type != ROC_NPC_ITEM_TYPE_HIGIG2)
		return 0;

	lid = NPC_LID_LA;
	lt = NPC_LT_LA_HIGIG2_ETHER;
	info.hw_hdr_len = 0;

	if (pst->flow->nix_intf == NIX_INTF_TX) {
		lt = NPC_LT_LA_IH_NIX_HIGIG2_ETHER;
		info.hw_hdr_len = NPC_IH_LENGTH;
	}

	/* Prepare for parsing the item */
	info.def_mask = NULL;
	info.hw_mask = &hw_mask;
	info.len = pst->pattern->size;
	npc_get_hw_supp_mask(pst, &info, lid, lt);
	info.spec = NULL;
	info.mask = NULL;

	/* Basic validation of item parameters */
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc)
		return rc;

	/* Update pst if not validate only? clash check? */
	return npc_update_parse_state(pst, &info, lid, lt, 0);
}

int
npc_parse_la(struct npc_parse_state *pst)
{
	const struct roc_npc_flow_item_eth *eth_item;
	uint8_t hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt;
	int rc;

	/* Identify the pattern type into lid, lt */
	if (pst->pattern->type != ROC_NPC_ITEM_TYPE_ETH)
		return 0;

	eth_item = pst->pattern->spec;

	lid = NPC_LID_LA;
	lt = NPC_LT_LA_ETHER;
	info.hw_hdr_len = 0;

	if (pst->flow->nix_intf == NIX_INTF_TX) {
		lt = NPC_LT_LA_IH_NIX_ETHER;
		info.hw_hdr_len = NPC_IH_LENGTH;
		if (pst->npc->switch_header_type == ROC_PRIV_FLAGS_HIGIG) {
			lt = NPC_LT_LA_IH_NIX_HIGIG2_ETHER;
			info.hw_hdr_len += NPC_HIGIG2_LENGTH;
		}
	} else {
		if (pst->npc->switch_header_type == ROC_PRIV_FLAGS_HIGIG) {
			lt = NPC_LT_LA_HIGIG2_ETHER;
			info.hw_hdr_len = NPC_HIGIG2_LENGTH;
		}
	}

	/* Prepare for parsing the item */
	info.def_mask = NULL;
	info.hw_mask = &hw_mask;
	info.len = sizeof(eth_item->hdr);
	npc_get_hw_supp_mask(pst, &info, lid, lt);
	info.spec = NULL;
	info.mask = NULL;

	/* Basic validation of item parameters */
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc)
		return rc;

	rc = npc_update_parse_state(pst, &info, lid, lt, 0);
	if (rc)
		return rc;

	if (eth_item && eth_item->has_vlan)
		pst->set_vlan_ltype_mask = true;

	return 0;
}

#define NPC_MAX_SUPPORTED_VLANS 3

int
npc_parse_lb(struct npc_parse_state *pst)
{
	const struct roc_npc_flow_item_vlan *vlan_item[NPC_MAX_SUPPORTED_VLANS];
	const struct roc_npc_item_info *pattern = pst->pattern;
	const struct roc_npc_item_info *last_pattern;
	const struct roc_npc_flow_item_raw *raw_spec;
	uint8_t raw_spec_buf[NPC_MAX_RAW_ITEM_LEN];
	uint8_t raw_mask_buf[NPC_MAX_RAW_ITEM_LEN];
	char hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt, lflags, len = 0;
	int nr_vlans = 0;
	int rc;

	info.def_mask = NULL;
	info.spec = NULL;
	info.mask = NULL;
	info.def_mask = NULL;
	info.hw_hdr_len = NPC_TPID_LENGTH;

	lid = NPC_LID_LB;
	lflags = 0;
	last_pattern = pattern;

	if (pst->pattern->type == ROC_NPC_ITEM_TYPE_VLAN) {
		/* RTE vlan is either 802.1q or 802.1ad,
		 * this maps to either CTAG/STAG. We need to decide
		 * based on number of VLANS present. Matching is
		 * supported on first tag only.
		 */
		info.hw_mask = NULL;
		info.len = sizeof(vlan_item[0]->hdr);

		pattern = pst->pattern;
		while (pattern->type == ROC_NPC_ITEM_TYPE_VLAN) {
			if (nr_vlans > NPC_MAX_SUPPORTED_VLANS - 1)
				return NPC_ERR_PATTERN_NOTSUP;

			vlan_item[nr_vlans] = pattern->spec;
			nr_vlans++;

			/* Basic validation of Second/Third vlan item */
			if (nr_vlans > 1) {
				rc = npc_parse_item_basic(pattern, &info);
				if (rc != 0)
					return rc;
			}
			last_pattern = pattern;
			pattern++;
			pattern = npc_parse_skip_void_and_any_items(pattern);
		}

		switch (nr_vlans) {
		case 1:
			lt = NPC_LT_LB_CTAG;
			if (vlan_item[0] && vlan_item[0]->has_more_vlan)
				lt = NPC_LT_LB_STAG_QINQ;
			break;
		case 2:
			if (vlan_item[1] && vlan_item[1]->has_more_vlan) {
				if (!(pst->npc->keyx_supp_nmask[pst->nix_intf] &
				      0x3ULL << NPC_LFLAG_LB_OFFSET))
					return NPC_ERR_PATTERN_NOTSUP;

				/* This lflag value will match either one of
				 * NPC_F_LB_L_WITH_STAG_STAG,
				 * NPC_F_LB_L_WITH_QINQ_CTAG,
				 * NPC_F_LB_L_WITH_QINQ_QINQ and
				 * NPC_F_LB_L_WITH_ITAG (0b0100 to 0b0111). For
				 * NPC_F_LB_L_WITH_ITAG, ltype is NPC_LT_LB_ETAG
				 * hence will not match.
				 */

				lflags = NPC_F_LB_L_WITH_QINQ_CTAG &
					 NPC_F_LB_L_WITH_QINQ_QINQ &
					 NPC_F_LB_L_WITH_STAG_STAG;
			} else {
				lflags = NPC_F_LB_L_WITH_CTAG;
			}
			lt = NPC_LT_LB_STAG_QINQ;
			break;
		case 3:
			if (vlan_item[2] && vlan_item[2]->has_more_vlan)
				return NPC_ERR_PATTERN_NOTSUP;
			lt = NPC_LT_LB_STAG_QINQ;
			lflags = NPC_F_STAG_STAG_CTAG;
			break;
		default:
			return NPC_ERR_PATTERN_NOTSUP;
		}
	} else if (pst->pattern->type == ROC_NPC_ITEM_TYPE_E_TAG) {
		/* we can support ETAG and match a subsequent CTAG
		 * without any matching support.
		 */
		lt = NPC_LT_LB_ETAG;
		lflags = 0;

		last_pattern = pst->pattern;
		pattern = npc_parse_skip_void_and_any_items(pst->pattern + 1);
		if (pattern->type == ROC_NPC_ITEM_TYPE_VLAN) {
			/* set supported mask to NULL for vlan tag */
			info.hw_mask = NULL;
			info.len = pattern->size;
			rc = npc_parse_item_basic(pattern, &info);
			if (rc != 0)
				return rc;

			lflags = NPC_F_ETAG_CTAG;
			last_pattern = pattern;
		}
		info.len = pattern->size;
	} else if (pst->pattern->type == ROC_NPC_ITEM_TYPE_QINQ) {
		vlan_item[0] = pst->pattern->spec;
		info.hw_mask = NULL;
		info.len = sizeof(vlan_item[0]->hdr);
		lt = NPC_LT_LB_STAG_QINQ;
		lflags = NPC_F_STAG_CTAG;
		if (vlan_item[0] && vlan_item[0]->has_more_vlan) {
			lflags = NPC_F_LB_L_WITH_QINQ_CTAG &
				 NPC_F_LB_L_WITH_QINQ_QINQ;
		}
	} else if (pst->pattern->type == ROC_NPC_ITEM_TYPE_RAW) {
		raw_spec = pst->pattern->spec;
		if (raw_spec->relative)
			return 0;
		len = raw_spec->length + raw_spec->offset;
		if (len > NPC_MAX_RAW_ITEM_LEN)
			return -EINVAL;

		if (pst->npc->switch_header_type == ROC_PRIV_FLAGS_VLAN_EXDSA) {
			lt = NPC_LT_LB_VLAN_EXDSA;
		} else if (pst->npc->switch_header_type ==
			   ROC_PRIV_FLAGS_EXDSA) {
			lt = NPC_LT_LB_EXDSA;
		} else {
			return -EINVAL;
		}

		npc_flow_raw_item_prepare((const struct roc_npc_flow_item_raw *)
						  pst->pattern->spec,
					  (const struct roc_npc_flow_item_raw *)
						  pst->pattern->mask,
					  &info, raw_spec_buf, raw_mask_buf);

		info.hw_hdr_len = 0;
	} else {
		return 0;
	}

	info.hw_mask = &hw_mask;
	npc_get_hw_supp_mask(pst, &info, lid, lt);

	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc != 0)
		return rc;

	/* Point pattern to last item consumed */
	pst->pattern = last_pattern;
	return npc_update_parse_state(pst, &info, lid, lt, lflags);
}

static int
npc_parse_mpls_label_stack(struct npc_parse_state *pst, int *flag)
{
	uint8_t flag_list[] = {0, NPC_F_MPLS_2_LABELS, NPC_F_MPLS_3_LABELS,
			       NPC_F_MPLS_4_LABELS};
	const struct roc_npc_item_info *pattern = pst->pattern;
	struct npc_parse_item_info info;
	int nr_labels = 0;
	int rc;

	/*
	 * pst->pattern points to first MPLS label. We only check
	 * that subsequent labels do not have anything to match.
	 */
	info.def_mask = NULL;
	info.hw_mask = NULL;
	info.len = pattern->size;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;

	while (pattern->type == ROC_NPC_ITEM_TYPE_MPLS) {
		nr_labels++;

		/* Basic validation of Second/Third/Fourth mpls item */
		if (nr_labels > 1) {
			rc = npc_parse_item_basic(pattern, &info);
			if (rc != 0)
				return rc;
		}
		pst->last_pattern = pattern;
		pattern++;
		pattern = npc_parse_skip_void_and_any_items(pattern);
	}

	if (nr_labels < 1 || nr_labels > 4)
		return NPC_ERR_PATTERN_NOTSUP;

	*flag = flag_list[nr_labels - 1];
	return 0;
}

static int
npc_parse_mpls(struct npc_parse_state *pst, int lid)
{
	/* Find number of MPLS labels */
	uint8_t hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
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
	info.hw_mask = &hw_mask;
	info.len = pst->pattern->size;
	info.spec = NULL;
	info.mask = NULL;
	info.def_mask = NULL;
	info.hw_hdr_len = 0;

	npc_get_hw_supp_mask(pst, &info, lid, lt);
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc != 0)
		return rc;

	/*
	 * Parse for more labels.
	 * This sets lflags and pst->last_pattern correctly.
	 */
	rc = npc_parse_mpls_label_stack(pst, &lflags);
	if (rc != 0)
		return rc;

	pst->tunnel = 1;
	pst->pattern = pst->last_pattern;

	return npc_update_parse_state(pst, &info, lid, lt, lflags);
}

static inline void
npc_check_lc_ip_tunnel(struct npc_parse_state *pst)
{
	const struct roc_npc_item_info *pattern = pst->pattern + 1;

	pattern = npc_parse_skip_void_and_any_items(pattern);
	if (pattern->type == ROC_NPC_ITEM_TYPE_MPLS ||
	    pattern->type == ROC_NPC_ITEM_TYPE_IPV4 ||
	    pattern->type == ROC_NPC_ITEM_TYPE_IPV6)
		pst->tunnel = 1;
}

static int
npc_handle_ipv6ext_attr(const struct roc_npc_flow_item_ipv6 *ipv6_spec,
			struct npc_parse_state *pst, uint8_t *flags)
{
	int flags_count = 0;

	if (ipv6_spec->has_hop_ext) {
		*flags = NPC_F_LC_L_EXT_HOP;
		flags_count++;
	}
	if (ipv6_spec->has_route_ext) {
		*flags = NPC_F_LC_L_EXT_ROUT;
		flags_count++;
	}
	if (ipv6_spec->has_frag_ext) {
		*flags = NPC_F_LC_U_IP6_FRAG;
		flags_count++;
	}
	if (ipv6_spec->has_dest_ext) {
		*flags = NPC_F_LC_L_EXT_DEST;
		flags_count++;
	}
	if (ipv6_spec->has_mobil_ext) {
		*flags = NPC_F_LC_L_EXT_MOBILITY;
		flags_count++;
	}
	if (ipv6_spec->has_hip_ext) {
		*flags = NPC_F_LC_L_EXT_HOSTID;
		flags_count++;
	}
	if (ipv6_spec->has_shim6_ext) {
		*flags = NPC_F_LC_L_EXT_SHIM6;
		flags_count++;
	}
	if (ipv6_spec->has_auth_ext) {
		pst->lt[NPC_LID_LD] = NPC_LT_LD_AH;
		flags_count++;
	}
	if (ipv6_spec->has_esp_ext) {
		pst->lt[NPC_LID_LE] = NPC_LT_LE_ESP;
		flags_count++;
	}

	if (flags_count > 1)
		return -EINVAL;

	if (flags_count)
		pst->set_ipv6ext_ltype_mask = true;

	return 0;
}

int
npc_parse_lc(struct npc_parse_state *pst)
{
	const struct roc_npc_flow_item_ipv6 *ipv6_spec;
	const struct roc_npc_flow_item_raw *raw_spec;
	uint8_t raw_spec_buf[NPC_MAX_RAW_ITEM_LEN];
	uint8_t raw_mask_buf[NPC_MAX_RAW_ITEM_LEN];
	uint8_t hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int rc, lid, lt, len = 0;
	uint8_t flags = 0;

	if (pst->pattern->type == ROC_NPC_ITEM_TYPE_MPLS)
		return npc_parse_mpls(pst, NPC_LID_LC);

	info.def_mask = NULL;
	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LC;

	switch (pst->pattern->type) {
	case ROC_NPC_ITEM_TYPE_IPV4:
		lt = NPC_LT_LC_IP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_IPV6:
		ipv6_spec = pst->pattern->spec;
		lid = NPC_LID_LC;
		lt = NPC_LT_LC_IP6;
		if (ipv6_spec) {
			rc = npc_handle_ipv6ext_attr(ipv6_spec, pst, &flags);
			if (rc)
				return rc;
		}
		info.len = sizeof(ipv6_spec->hdr);
		break;
	case ROC_NPC_ITEM_TYPE_ARP_ETH_IPV4:
		lt = NPC_LT_LC_ARP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_IPV6_EXT:
		lid = NPC_LID_LC;
		lt = NPC_LT_LC_IP6_EXT;
		info.len = pst->pattern->size;
		info.hw_hdr_len = 40;
		break;
	case ROC_NPC_ITEM_TYPE_L3_CUSTOM:
		lt = NPC_LT_LC_CUSTOM0;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_RAW:
		raw_spec = pst->pattern->spec;
		if (!raw_spec->relative)
			return 0;

		len = raw_spec->length + raw_spec->offset;
		if (len > NPC_MAX_RAW_ITEM_LEN)
			return -EINVAL;

		npc_flow_raw_item_prepare((const struct roc_npc_flow_item_raw *)
						  pst->pattern->spec,
					  (const struct roc_npc_flow_item_raw *)
						  pst->pattern->mask,
					  &info, raw_spec_buf, raw_mask_buf);

		lid = NPC_LID_LC;
		lt = NPC_LT_LC_NGIO;
		info.hw_mask = &hw_mask;
		npc_get_hw_supp_mask(pst, &info, lid, lt);
		break;
	default:
		/* No match at this layer */
		return 0;
	}

	/* Identify if IP tunnels MPLS or IPv4/v6 */
	npc_check_lc_ip_tunnel(pst);

	npc_get_hw_supp_mask(pst, &info, lid, lt);
	rc = npc_parse_item_basic(pst->pattern, &info);

	if (rc != 0)
		return rc;

	return npc_update_parse_state(pst, &info, lid, lt, flags);
}

int
npc_parse_ld(struct npc_parse_state *pst)
{
	char hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
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
		if (pst->pattern->type == ROC_NPC_ITEM_TYPE_MPLS)
			return npc_parse_mpls(pst, NPC_LID_LD);
		return 0;
	}
	info.def_mask = NULL;
	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.len = 0;
	info.hw_hdr_len = 0;

	lid = NPC_LID_LD;
	lflags = 0;

	switch (pst->pattern->type) {
	case ROC_NPC_ITEM_TYPE_ICMP:
		if (pst->lt[NPC_LID_LC] == NPC_LT_LC_IP6)
			lt = NPC_LT_LD_ICMP6;
		else
			lt = NPC_LT_LD_ICMP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_UDP:
		lt = NPC_LT_LD_UDP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_IGMP:
		lt = NPC_LT_LD_IGMP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_TCP:
		lt = NPC_LT_LD_TCP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_SCTP:
		lt = NPC_LT_LD_SCTP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_GRE:
		lt = NPC_LT_LD_GRE;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_GRE_KEY:
		lt = NPC_LT_LD_GRE;
		info.len = pst->pattern->size;
		info.hw_hdr_len = 4;
		break;
	case ROC_NPC_ITEM_TYPE_NVGRE:
		lt = NPC_LT_LD_NVGRE;
		lflags = NPC_F_GRE_NVGRE;
		info.len = pst->pattern->size;
		/* Further IP/Ethernet are parsed as tunneled */
		pst->tunnel = 1;
		break;
	default:
		return 0;
	}

	npc_get_hw_supp_mask(pst, &info, lid, lt);
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc != 0)
		return rc;

	return npc_update_parse_state(pst, &info, lid, lt, lflags);
}

int
npc_parse_le(struct npc_parse_state *pst)
{
	const struct roc_npc_item_info *pattern = pst->pattern;
	char hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt, lflags;
	int rc;

	if (pst->tunnel)
		return 0;

	if (pst->pattern->type == ROC_NPC_ITEM_TYPE_MPLS)
		return npc_parse_mpls(pst, NPC_LID_LE);

	info.spec = NULL;
	info.mask = NULL;
	info.hw_mask = NULL;
	info.def_mask = NULL;
	info.len = 0;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LE;
	lflags = 0;

	/* Ensure we are not matching anything in UDP */
	rc = npc_parse_item_basic(pattern, &info);
	if (rc)
		return rc;

	info.hw_mask = &hw_mask;
	pattern = npc_parse_skip_void_and_any_items(pattern);
	switch (pattern->type) {
	case ROC_NPC_ITEM_TYPE_VXLAN:
		lflags = NPC_F_UDP_VXLAN;
		info.len = pattern->size;
		lt = NPC_LT_LE_VXLAN;
		break;
	case ROC_NPC_ITEM_TYPE_GTPC:
		lflags = NPC_F_UDP_GTP_GTPC;
		info.len = pattern->size;
		lt = NPC_LT_LE_GTPC;
		break;
	case ROC_NPC_ITEM_TYPE_GTPU:
		lflags = NPC_F_UDP_GTP_GTPU_G_PDU;
		info.len = pattern->size;
		lt = NPC_LT_LE_GTPU;
		break;
	case ROC_NPC_ITEM_TYPE_GENEVE:
		lflags = NPC_F_UDP_GENEVE;
		info.len = pattern->size;
		lt = NPC_LT_LE_GENEVE;
		break;
	case ROC_NPC_ITEM_TYPE_VXLAN_GPE:
		lflags = NPC_F_UDP_VXLANGPE;
		info.len = pattern->size;
		lt = NPC_LT_LE_VXLANGPE;
		break;
	case ROC_NPC_ITEM_TYPE_ESP:
		lt = NPC_LT_LE_ESP;
		info.len = pst->pattern->size;
		break;
	default:
		return 0;
	}

	pst->tunnel = 1;

	npc_get_hw_supp_mask(pst, &info, lid, lt);
	rc = npc_parse_item_basic(pattern, &info);
	if (rc != 0)
		return rc;

	return npc_update_parse_state(pst, &info, lid, lt, lflags);
}

int
npc_parse_lf(struct npc_parse_state *pst)
{
	const struct roc_npc_item_info *pattern, *last_pattern;
	char hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt, lflags;
	int nr_vlans = 0;
	int rc;

	/* We hit this layer if there is a tunneling protocol */
	if (!pst->tunnel)
		return 0;

	if (pst->pattern->type != ROC_NPC_ITEM_TYPE_ETH)
		return 0;

	lid = NPC_LID_LF;
	lt = NPC_LT_LF_TU_ETHER;
	lflags = 0;

	/* No match support for vlan tags */
	info.def_mask = NULL;
	info.hw_mask = NULL;
	info.len = pst->pattern->size;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;

	/* Look ahead and find out any VLAN tags. These can be
	 * detected but no data matching is available.
	 */
	last_pattern = pst->pattern;
	pattern = pst->pattern + 1;
	pattern = npc_parse_skip_void_and_any_items(pattern);
	while (pattern->type == ROC_NPC_ITEM_TYPE_VLAN) {
		nr_vlans++;
		last_pattern = pattern;
		pattern++;
		pattern = npc_parse_skip_void_and_any_items(pattern);
	}
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
		return NPC_ERR_PATTERN_NOTSUP;
	}

	info.hw_mask = &hw_mask;
	info.len = pst->pattern->size;
	info.hw_hdr_len = 0;
	npc_get_hw_supp_mask(pst, &info, lid, lt);
	info.spec = NULL;
	info.mask = NULL;

	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc != 0)
		return rc;

	pst->pattern = last_pattern;

	return npc_update_parse_state(pst, &info, lid, lt, lflags);
}

int
npc_parse_lg(struct npc_parse_state *pst)
{
	char hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt;
	int rc;

	if (!pst->tunnel)
		return 0;

	info.def_mask = NULL;
	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LG;

	if (pst->pattern->type == ROC_NPC_ITEM_TYPE_IPV4) {
		lt = NPC_LT_LG_TU_IP;
		info.len = pst->pattern->size;
	} else if (pst->pattern->type == ROC_NPC_ITEM_TYPE_IPV6) {
		lt = NPC_LT_LG_TU_IP6;
		info.len = pst->pattern->size;
	} else {
		/* There is no tunneled IP header */
		return 0;
	}

	npc_get_hw_supp_mask(pst, &info, lid, lt);
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc != 0)
		return rc;

	return npc_update_parse_state(pst, &info, lid, lt, 0);
}

int
npc_parse_lh(struct npc_parse_state *pst)
{
	char hw_mask[NPC_MAX_EXTRACT_HW_LEN];
	struct npc_parse_item_info info;
	int lid, lt;
	int rc;

	if (!pst->tunnel)
		return 0;

	info.def_mask = NULL;
	info.hw_mask = &hw_mask;
	info.spec = NULL;
	info.mask = NULL;
	info.hw_hdr_len = 0;
	lid = NPC_LID_LH;

	switch (pst->pattern->type) {
	case ROC_NPC_ITEM_TYPE_UDP:
		lt = NPC_LT_LH_TU_UDP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_TCP:
		lt = NPC_LT_LH_TU_TCP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_SCTP:
		lt = NPC_LT_LH_TU_SCTP;
		info.len = pst->pattern->size;
		break;
	case ROC_NPC_ITEM_TYPE_ESP:
		lt = NPC_LT_LH_TU_ESP;
		info.len = pst->pattern->size;
		break;
	default:
		return 0;
	}

	npc_get_hw_supp_mask(pst, &info, lid, lt);
	rc = npc_parse_item_basic(pst->pattern, &info);
	if (rc != 0)
		return rc;

	return npc_update_parse_state(pst, &info, lid, lt, 0);
}
