/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define NPC_MAX_FIELD_NAME_SIZE	   80
#define NPC_RX_ACTIONOP_MASK	   GENMASK(3, 0)
#define NPC_RX_ACTION_PFFUNC_MASK  GENMASK(19, 4)
#define NPC_RX_ACTION_INDEX_MASK   GENMASK(39, 20)
#define NPC_RX_ACTION_MATCH_MASK   GENMASK(55, 40)
#define NPC_RX_ACTION_FLOWKEY_MASK GENMASK(60, 56)

#define NPC_TX_ACTION_INDEX_MASK GENMASK(31, 12)
#define NPC_TX_ACTION_MATCH_MASK GENMASK(47, 32)

#define NIX_RX_VTAGACT_VTAG0_RELPTR_MASK GENMASK(7, 0)
#define NIX_RX_VTAGACT_VTAG0_LID_MASK	 GENMASK(10, 8)
#define NIX_RX_VTAGACT_VTAG0_TYPE_MASK	 GENMASK(14, 12)
#define NIX_RX_VTAGACT_VTAG0_VALID_MASK	 BIT_ULL(15)

#define NIX_RX_VTAGACT_VTAG1_RELPTR_MASK GENMASK(39, 32)
#define NIX_RX_VTAGACT_VTAG1_LID_MASK	 GENMASK(42, 40)
#define NIX_RX_VTAGACT_VTAG1_TYPE_MASK	 GENMASK(46, 44)
#define NIX_RX_VTAGACT_VTAG1_VALID_MASK	 BIT_ULL(47)

#define NIX_TX_VTAGACT_VTAG0_RELPTR_MASK GENMASK(7, 0)
#define NIX_TX_VTAGACT_VTAG0_LID_MASK	 GENMASK(10, 8)
#define NIX_TX_VTAGACT_VTAG0_OP_MASK	 GENMASK(13, 12)
#define NIX_TX_VTAGACT_VTAG0_DEF_MASK	 GENMASK(25, 16)

#define NIX_TX_VTAGACT_VTAG1_RELPTR_MASK GENMASK(39, 32)
#define NIX_TX_VTAGACT_VTAG1_LID_MASK	 GENMASK(42, 40)
#define NIX_TX_VTAGACT_VTAG1_OP_MASK	 GENMASK(45, 44)
#define NIX_TX_VTAGACT_VTAG1_DEF_MASK	 GENMASK(57, 48)

struct npc_rx_parse_nibble_s {
	uint16_t chan : 3;
	uint16_t errlev : 1;
	uint16_t errcode : 2;
	uint16_t l2l3bm : 1;
	uint16_t laflags : 2;
	uint16_t latype : 1;
	uint16_t lbflags : 2;
	uint16_t lbtype : 1;
	uint16_t lcflags : 2;
	uint16_t lctype : 1;
	uint16_t ldflags : 2;
	uint16_t ldtype : 1;
	uint16_t leflags : 2;
	uint16_t letype : 1;
	uint16_t lfflags : 2;
	uint16_t lftype : 1;
	uint16_t lgflags : 2;
	uint16_t lgtype : 1;
	uint16_t lhflags : 2;
	uint16_t lhtype : 1;
} __plt_packed;

static const char *const intf_str[] = {
	"NIX-RX",
	"NIX-TX",
};

static const char *const ltype_str[NPC_MAX_LID][NPC_MAX_LT] = {
	[NPC_LID_LA][0] = "NONE",
	[NPC_LID_LA][NPC_LT_LA_ETHER] = "LA_ETHER",
	[NPC_LID_LA][NPC_LT_LA_IH_NIX_ETHER] = "LA_IH_NIX_ETHER",
	[NPC_LID_LA][NPC_LT_LA_HIGIG2_ETHER] = "LA_HIGIG2_ETHER",
	[NPC_LID_LA][NPC_LT_LA_IH_NIX_HIGIG2_ETHER] = "LA_IH_NIX_HIGIG2_ETHER",
	[NPC_LID_LA][NPC_LT_LA_CUSTOM_L2_90B_ETHER] = "LA_CUSTOM_L2_90B_ETHER",
	[NPC_LID_LA][NPC_LT_LA_CPT_HDR] = "LA_CPT_HDR",
	[NPC_LID_LA][NPC_LT_LA_CUSTOM_L2_24B_ETHER] = "LA_CUSTOM_L2_24B_ETHER",
	[NPC_LID_LA][NPC_LT_LA_CUSTOM_PRE_L2_ETHER] = "NPC_LT_LA_CUSTOM_PRE_L2_ETHER",
	[NPC_LID_LA][NPC_LT_LA_CUSTOM0] = "NPC_LT_LA_CUSTOM0",
	[NPC_LID_LA][NPC_LT_LA_CUSTOM1] = "NPC_LT_LA_CUSTOM1",
	[NPC_LID_LB][0] = "NONE",
	[NPC_LID_LB][NPC_LT_LB_ETAG] = "LB_ETAG",
	[NPC_LID_LB][NPC_LT_LB_CTAG] = "LB_CTAG",
	[NPC_LID_LB][NPC_LT_LB_STAG_QINQ] = "LB_STAG_QINQ",
	[NPC_LID_LB][NPC_LT_LB_BTAG] = "LB_BTAG",
	[NPC_LID_LB][NPC_LT_LB_PPPOE] = "LB_PPPOE",
	[NPC_LID_LB][NPC_LT_LB_DSA] = "LB_DSA",
	[NPC_LID_LB][NPC_LT_LB_DSA_VLAN] = "LB_DSA_VLAN",
	[NPC_LID_LB][NPC_LT_LB_EDSA] = "LB_EDSA",
	[NPC_LID_LB][NPC_LT_LB_EDSA_VLAN] = "LB_EDSA_VLAN",
	[NPC_LID_LB][NPC_LT_LB_EXDSA] = "LB_EXDSA",
	[NPC_LID_LB][NPC_LT_LB_EXDSA_VLAN] = "LB_EXDSA_VLAN",
	[NPC_LID_LB][NPC_LT_LB_FDSA] = "LB_FDSA",
	[NPC_LID_LB][NPC_LT_LB_VLAN_EXDSA] = "LB_VLAN_EXDSA",
	[NPC_LID_LB][NPC_LT_LB_CUSTOM0] = "LB_CUSTOM0",
	[NPC_LID_LB][NPC_LT_LB_CUSTOM1] = "LB_CUSTOM1",
	[NPC_LID_LC][0] = "NONE",
	[NPC_LID_LC][NPC_LT_LC_PTP] = "LC_PTP",
	[NPC_LID_LC][NPC_LT_LC_IP] = "LC_IP",
	[NPC_LID_LC][NPC_LT_LC_IP_OPT] = "LC_IP_OPT",
	[NPC_LID_LC][NPC_LT_LC_IP6] = "LC_IP6",
	[NPC_LID_LC][NPC_LT_LC_IP6_EXT] = "LC_IP6_EXT",
	[NPC_LID_LC][NPC_LT_LC_ARP] = "LC_ARP",
	[NPC_LID_LC][NPC_LT_LC_RARP] = "LC_RARP",
	[NPC_LID_LC][NPC_LT_LC_MPLS] = "LC_MPLS",
	[NPC_LID_LC][NPC_LT_LC_NSH] = "LC_NSH",
	[NPC_LID_LC][NPC_LT_LC_FCOE] = "LC_FCOE",
	[NPC_LID_LC][NPC_LT_LC_NGIO] = "LC_NGIO",
	[NPC_LID_LC][NPC_LT_LC_CUSTOM0] = "LC_CUSTOM0",
	[NPC_LID_LC][NPC_LT_LC_CUSTOM1] = "LC_CUSTOM1",
	[NPC_LID_LD][0] = "NONE",
	[NPC_LID_LD][NPC_LT_LD_TCP] = "LD_TCP",
	[NPC_LID_LD][NPC_LT_LD_UDP] = "LD_UDP",
	[NPC_LID_LD][NPC_LT_LD_SCTP] = "LD_SCTP",
	[NPC_LID_LD][NPC_LT_LD_ICMP6] = "LD_ICMP6",
	[NPC_LID_LD][NPC_LT_LD_CUSTOM0] = "LD_CUSTOM0",
	[NPC_LID_LD][NPC_LT_LD_CUSTOM1] = "LD_CUSTOM1",
	[NPC_LID_LD][NPC_LT_LD_IGMP] = "LD_IGMP",
	[NPC_LID_LD][NPC_LT_LD_AH] = "LD_AH",
	[NPC_LID_LD][NPC_LT_LD_GRE] = "LD_GRE",
	[NPC_LID_LD][NPC_LT_LD_NVGRE] = "LD_NVGRE",
	[NPC_LID_LD][NPC_LT_LD_NSH] = "LD_NSH",
	[NPC_LID_LD][NPC_LT_LD_TU_MPLS_IN_NSH] = "LD_TU_MPLS_IN_NSH",
	[NPC_LID_LD][NPC_LT_LD_TU_MPLS_IN_IP] = "LD_TU_MPLS_IN_IP",
	[NPC_LID_LD][NPC_LT_LD_ICMP] = "LD_ICMP",
	[NPC_LID_LE][0] = "NONE",
	[NPC_LID_LE][NPC_LT_LE_VXLAN] = "LE_VXLAN",
	[NPC_LID_LE][NPC_LT_LE_GENEVE] = "LE_GENEVE",
	[NPC_LID_LE][NPC_LT_LE_ESP] = "LE_ESP",
	[NPC_LID_LE][NPC_LT_LE_GTPU] = "LE_GTPU",
	[NPC_LID_LE][NPC_LT_LE_VXLANGPE] = "LE_VXLANGPE",
	[NPC_LID_LE][NPC_LT_LE_GTPC] = "LE_GTPC",
	[NPC_LID_LE][NPC_LT_LE_NSH] = "LE_NSH",
	[NPC_LID_LE][NPC_LT_LE_TU_MPLS_IN_GRE] = "LE_TU_MPLS_IN_GRE",
	[NPC_LID_LE][NPC_LT_LE_TU_NSH_IN_GRE] = "LE_TU_NSH_IN_GRE",
	[NPC_LID_LE][NPC_LT_LE_TU_MPLS_IN_UDP] = "LE_TU_MPLS_IN_UDP",
	[NPC_LID_LE][NPC_LT_LE_CUSTOM0] = "LE_CUSTOM0",
	[NPC_LID_LE][NPC_LT_LE_CUSTOM1] = "LE_CUSTOM1",
	[NPC_LID_LF][0] = "NONE",
	[NPC_LID_LF][NPC_LT_LF_TU_ETHER] = "LF_TU_ETHER",
	[NPC_LID_LF][NPC_LT_LF_TU_PPP] = "LF_TU_PPP",
	[NPC_LID_LF][NPC_LT_LF_TU_MPLS_IN_VXLANGPE] = "LF_TU_MPLS_IN_VXLANGPE",
	[NPC_LID_LF][NPC_LT_LF_TU_NSH_IN_VXLANGPE] = "LF_TU_NSH_IN_VXLANGPE",
	[NPC_LID_LF][NPC_LT_LF_TU_MPLS_IN_NSH] = "LF_TU_MPLS_IN_NSH",
	[NPC_LID_LF][NPC_LT_LF_TU_3RD_NSH] = "LF_TU_3RD_NSH",
	[NPC_LID_LF][NPC_LT_LF_CUSTOM0] = "LF_CUSTOM0",
	[NPC_LID_LF][NPC_LT_LF_CUSTOM1] = "LF_CUSTOM1",
	[NPC_LID_LG][0] = "NONE",
	[NPC_LID_LG][NPC_LT_LG_TU_IP] = "LG_TU_IP",
	[NPC_LID_LG][NPC_LT_LG_TU_IP6] = "LG_TU_IP6",
	[NPC_LID_LG][NPC_LT_LG_TU_ARP] = "LG_TU_ARP",
	[NPC_LID_LG][NPC_LT_LG_TU_ETHER_IN_NSH] = "LG_TU_ETHER_IN_NSH",
	[NPC_LID_LG][NPC_LT_LG_CUSTOM0] = "LG_CUSTOM0",
	[NPC_LID_LG][NPC_LT_LG_CUSTOM1] = "LG_CUSTOM1",
	[NPC_LID_LH][0] = "NONE",
	[NPC_LID_LH][NPC_LT_LH_TU_TCP] = "LH_TU_TCP",
	[NPC_LID_LH][NPC_LT_LH_TU_UDP] = "LH_TU_UDP",
	[NPC_LID_LH][NPC_LT_LH_TU_SCTP] = "LH_TU_SCTP",
	[NPC_LID_LH][NPC_LT_LH_TU_ICMP6] = "LH_TU_ICMP6",
	[NPC_LID_LH][NPC_LT_LH_CUSTOM0] = "LH_CUSTOM0",
	[NPC_LID_LH][NPC_LT_LH_CUSTOM1] = "LH_CUSTOM1",
	[NPC_LID_LH][NPC_LT_LH_TU_IGMP] = "LH_TU_IGMP",
	[NPC_LID_LH][NPC_LT_LH_TU_ESP] = "LH_TU_ESP",
	[NPC_LID_LH][NPC_LT_LH_TU_AH] = "LH_TU_AH",
	[NPC_LID_LH][NPC_LT_LH_TU_ICMP] = "LH_TU_ICMP",
};

static uint16_t
npc_get_nibbles(struct roc_npc_flow *flow, uint16_t size, uint32_t bit_offset)
{
	uint32_t byte_index, noffset;
	uint16_t data, mask;
	uint8_t *bytes;

	bytes = (uint8_t *)flow->mcam_data;
	mask = (1ULL << (size * 4)) - 1;
	byte_index = bit_offset / 8;
	noffset = bit_offset % 8;
	data = *(unaligned_uint16_t *)&bytes[byte_index];
	data >>= noffset;
	data &= mask;

	return data;
}

static void
npc_flow_print_parse_nibbles(FILE *file, struct roc_npc_flow *flow,
			     uint64_t parse_nibbles)
{
	struct npc_rx_parse_nibble_s *rx_parse;
	uint32_t data, offset = 0;

	rx_parse = (struct npc_rx_parse_nibble_s *)&parse_nibbles;

	if (rx_parse->chan) {
		data = npc_get_nibbles(flow, 3, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_CHAN:%#03X\n", data);
		offset += 12;
	}

	if (rx_parse->errlev) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_ERRLEV:%#X\n", data);
		offset += 4;
	}

	if (rx_parse->errcode) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_ERRCODE:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->l2l3bm) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_L2L3_BCAST:%#X\n", data);
		offset += 4;
	}

	if (rx_parse->laflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LA_FLAGS:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->latype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LA_LTYPE:%s\n",
			ltype_str[NPC_LID_LA][data]);
		offset += 4;
	}

	if (rx_parse->lbflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LB_FLAGS:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->lbtype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LB_LTYPE:%s\n",
			ltype_str[NPC_LID_LB][data]);
		offset += 4;
	}

	if (rx_parse->lcflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LC_FLAGS:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->lctype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LC_LTYPE:%s\n",
			ltype_str[NPC_LID_LC][data]);
		offset += 4;
	}

	if (rx_parse->ldflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LD_FLAGS:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->ldtype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LD_LTYPE:%s\n",
			ltype_str[NPC_LID_LD][data]);
		offset += 4;
	}

	if (rx_parse->leflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LE_FLAGS:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->letype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LE_LTYPE:%s\n",
			ltype_str[NPC_LID_LE][data]);
		offset += 4;
	}

	if (rx_parse->lfflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LF_FLAGS:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->lftype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LF_LTYPE:%s\n",
			ltype_str[NPC_LID_LF][data]);
		offset += 4;
	}

	if (rx_parse->lgflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LG_FLAGS:%#02X\n", data);
		offset += 8;
	}

	if (rx_parse->lgtype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LG_LTYPE:%s\n",
			ltype_str[NPC_LID_LG][data]);
		offset += 4;
	}

	if (rx_parse->lhflags) {
		data = npc_get_nibbles(flow, 2, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LH_FLAGS:%#02X\n", data);
	}

	if (rx_parse->lhtype) {
		data = npc_get_nibbles(flow, 1, offset);
		fprintf(file, "\tNPC_PARSE_NIBBLE_LH_LTYPE:%s\n",
			ltype_str[NPC_LID_LH][data]);
		offset += 4;
	}
}

static void
npc_flow_print_xtractinfo(FILE *file, struct npc_xtract_info *lfinfo,
			  struct roc_npc_flow *flow, int lid, int lt)
{
	uint8_t *datastart, *maskstart;
	int i;

	datastart = (uint8_t *)&flow->mcam_data + lfinfo->key_off;
	maskstart = (uint8_t *)&flow->mcam_mask + lfinfo->key_off;

	fprintf(file, "\t%s, hdr offset:%#X, len:%#X, key offset:%#X, ",
		ltype_str[lid][lt], lfinfo->hdr_off, lfinfo->len,
		lfinfo->key_off);

	fprintf(file, "Data:0X");
	for (i = lfinfo->len - 1; i >= 0; i--)
		fprintf(file, "%02X", datastart[i]);

	fprintf(file, ", Mask:0X");

	for (i = lfinfo->len - 1; i >= 0; i--)
		fprintf(file, "%02X", maskstart[i]);

	fprintf(file, "\n");
}

static void
npc_flow_print_item(FILE *file, struct npc *npc, struct npc_xtract_info *xinfo,
		    struct roc_npc_flow *flow, int intf, int lid, int lt,
		    int ld)
{
	struct npc_xtract_info *lflags_info;
	int i, lf_cfg = 0;

	npc_flow_print_xtractinfo(file, xinfo, flow, lid, lt);

	if (xinfo->flags_enable) {
		lf_cfg = npc->prx_lfcfg[ld].i;

		if (lf_cfg != lid)
			return;

		for (i = 0; i < NPC_MAX_LFL; i++) {
			lflags_info = npc->prx_fxcfg[intf][ld][i].xtract;

			if (!lflags_info->enable)
				continue;

			npc_flow_print_xtractinfo(file, lflags_info, flow, lid, lt);
		}
	}
}

static void
npc_flow_dump_patterns(FILE *file, struct npc *npc, struct roc_npc_flow *flow)
{
	struct npc_lid_lt_xtract_info *lt_xinfo;
	struct npc_xtract_info *xinfo;
	uint32_t intf, lid, ld, i;
	uint64_t parse_nibbles;
	uint16_t ltype;

	intf = flow->nix_intf;
	parse_nibbles = npc->keyx_supp_nmask[intf];
	npc_flow_print_parse_nibbles(file, flow, parse_nibbles);

	for (i = 0; i < flow->num_patterns; i++) {
		lid = flow->dump_data[i].lid;
		ltype = flow->dump_data[i].ltype;
		lt_xinfo = &npc->prx_dxcfg[intf][lid][ltype];

		for (ld = 0; ld < NPC_MAX_LD; ld++) {
			xinfo = &lt_xinfo->xtract[ld];
			if (!xinfo->enable)
				continue;
			npc_flow_print_item(file, npc, xinfo, flow, intf, lid,
					    ltype, ld);
		}
	}
}

static void
npc_flow_dump_tx_action(FILE *file, uint64_t npc_action)
{
	char index_name[NPC_MAX_FIELD_NAME_SIZE] = "Index:";
	uint32_t tx_op, index, match_id;

	tx_op = npc_action & NPC_RX_ACTIONOP_MASK;

	fprintf(file, "\tActionOp:");

	switch (tx_op) {
	case NIX_TX_ACTIONOP_DROP:
		fprintf(file, "NIX_TX_ACTIONOP_DROP (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_DROP);
		break;
	case NIX_TX_ACTIONOP_UCAST_DEFAULT:
		fprintf(file, "NIX_TX_ACTIONOP_UCAST_DEFAULT (%" PRIu64 ")\n",
			(uint64_t)NIX_TX_ACTIONOP_UCAST_DEFAULT);
		break;
	case NIX_TX_ACTIONOP_UCAST_CHAN:
		fprintf(file, "NIX_TX_ACTIONOP_UCAST_DEFAULT (%" PRIu64 ")\n",
			(uint64_t)NIX_TX_ACTIONOP_UCAST_CHAN);
		plt_strlcpy(index_name,
			    "Transmit Channel:", NPC_MAX_FIELD_NAME_SIZE);
		break;
	case NIX_TX_ACTIONOP_MCAST:
		fprintf(file, "NIX_TX_ACTIONOP_MCAST (%" PRIu64 ")\n",
			(uint64_t)NIX_TX_ACTIONOP_MCAST);
		plt_strlcpy(index_name,
			    "Multicast Table Index:", NPC_MAX_FIELD_NAME_SIZE);
		break;
	case NIX_TX_ACTIONOP_DROP_VIOL:
		fprintf(file, "NIX_TX_ACTIONOP_DROP_VIOL (%" PRIu64 ")\n",
			(uint64_t)NIX_TX_ACTIONOP_DROP_VIOL);
		break;
	default:
		plt_err("Unknown NIX_TX_ACTIONOP found");
		return;
	}

	index = ((npc_action & NPC_TX_ACTION_INDEX_MASK) >> 12) &
		GENMASK(19, 0);

	fprintf(file, "\t%s:%#05X\n", index_name, index);

	match_id = ((npc_action & NPC_TX_ACTION_MATCH_MASK) >> 32) &
		   GENMASK(15, 0);

	fprintf(file, "\tMatch Id:%#04X\n", match_id);
}

static void
npc_flow_dump_rx_action(FILE *file, uint64_t npc_action)
{
	uint32_t rx_op, pf_func, index, match_id, flowkey_alg;
	char index_name[NPC_MAX_FIELD_NAME_SIZE] = "Index:";

	rx_op = npc_action & NPC_RX_ACTIONOP_MASK;

	fprintf(file, "\tActionOp:");

	switch (rx_op) {
	case NIX_RX_ACTIONOP_DROP:
		fprintf(file, "NIX_RX_ACTIONOP_DROP (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_DROP);
		break;
	case NIX_RX_ACTIONOP_UCAST:
		fprintf(file, "NIX_RX_ACTIONOP_UCAST (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_UCAST);
		plt_strlcpy(index_name, "RQ Index", NPC_MAX_FIELD_NAME_SIZE);
		break;
	case NIX_RX_ACTIONOP_UCAST_IPSEC:
		fprintf(file, "NIX_RX_ACTIONOP_UCAST_IPSEC (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_UCAST_IPSEC);
		plt_strlcpy(index_name, "RQ Index:", NPC_MAX_FIELD_NAME_SIZE);
		break;
	case NIX_RX_ACTIONOP_MCAST:
		fprintf(file, "NIX_RX_ACTIONOP_MCAST (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_MCAST);
		plt_strlcpy(index_name, "Multicast/mirror table index",
			    NPC_MAX_FIELD_NAME_SIZE);
		break;
	case NIX_RX_ACTIONOP_RSS:
		fprintf(file, "NIX_RX_ACTIONOP_RSS (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_RSS);
		plt_strlcpy(index_name, "RSS Group Index",
			    NPC_MAX_FIELD_NAME_SIZE);
		break;
	case NIX_RX_ACTIONOP_PF_FUNC_DROP:
		fprintf(file, "NIX_RX_ACTIONOP_PF_FUNC_DROP (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_PF_FUNC_DROP);
		break;
	case NIX_RX_ACTIONOP_MIRROR:
		fprintf(file, "NIX_RX_ACTIONOP_MIRROR (%" PRIu64 ")\n",
			(uint64_t)NIX_RX_ACTIONOP_MIRROR);
		plt_strlcpy(index_name, "Multicast/mirror table index",
			    NPC_MAX_FIELD_NAME_SIZE);
		break;
	default:
		plt_err("Unknown NIX_RX_ACTIONOP found");
		return;
	}

	pf_func = ((npc_action & NPC_RX_ACTION_PFFUNC_MASK) >> 4) &
		  GENMASK(15, 0);

	fprintf(file, "\tPF_FUNC: %#04X\n", pf_func);

	index = ((npc_action & NPC_RX_ACTION_INDEX_MASK) >> 20) &
		GENMASK(19, 0);

	fprintf(file, "\t%s:%#05X\n", index_name, index);

	match_id = ((npc_action & NPC_RX_ACTION_MATCH_MASK) >> 40) &
		   GENMASK(15, 0);

	fprintf(file, "\tMatch Id:%#04X\n", match_id);

	flowkey_alg = ((npc_action & NPC_RX_ACTION_FLOWKEY_MASK) >> 56) &
		      GENMASK(4, 0);

	fprintf(file, "\tFlow Key Alg:%#X\n", flowkey_alg);
}

static void
npc_flow_dump_parsed_action(FILE *file, uint64_t npc_action, bool is_rx)
{
	if (is_rx) {
		fprintf(file, "NPC RX Action:%#016lX\n", npc_action);
		npc_flow_dump_rx_action(file, npc_action);
	} else {
		fprintf(file, "NPC TX Action:%#016lX\n", npc_action);
		npc_flow_dump_tx_action(file, npc_action);
	}
}

static void
npc_flow_dump_rx_vtag_action(FILE *file, uint64_t vtag_action)
{
	uint32_t type, lid, relptr;

	if (vtag_action & NIX_RX_VTAGACT_VTAG0_VALID_MASK) {
		relptr = vtag_action & NIX_RX_VTAGACT_VTAG0_RELPTR_MASK;
		lid = ((vtag_action & NIX_RX_VTAGACT_VTAG0_LID_MASK) >> 8) &
		      GENMASK(2, 0);
		type = ((vtag_action & NIX_RX_VTAGACT_VTAG0_TYPE_MASK) >> 12) &
		       GENMASK(2, 0);

		fprintf(file, "\tVTAG0:relptr:%#X\n", relptr);
		fprintf(file, "\tlid:%#X\n", lid);
		fprintf(file, "\ttype:%#X\n", type);
	}

	if (vtag_action & NIX_RX_VTAGACT_VTAG1_VALID_MASK) {
		relptr = ((vtag_action & NIX_RX_VTAGACT_VTAG1_RELPTR_MASK) >>
			  32) &
			 GENMASK(7, 0);
		lid = ((vtag_action & NIX_RX_VTAGACT_VTAG1_LID_MASK) >> 40) &
		      GENMASK(2, 0);
		type = ((vtag_action & NIX_RX_VTAGACT_VTAG1_TYPE_MASK) >> 44) &
		       GENMASK(2, 0);

		fprintf(file, "\tVTAG1:relptr:%#X\n", relptr);
		fprintf(file, "\tlid:%#X\n", lid);
		fprintf(file, "\ttype:%#X\n", type);
	}
}

static void
npc_get_vtag_opname(uint32_t op, char *opname, int len)
{
	switch (op) {
	case 0x0:
		plt_strlcpy(opname, "NOP", len - 1);
		break;
	case 0x1:
		plt_strlcpy(opname, "INSERT", len - 1);
		break;
	case 0x2:
		plt_strlcpy(opname, "REPLACE", len - 1);
		break;
	default:
		plt_err("Unknown vtag op found");
		break;
	}
}

static void
npc_flow_dump_tx_vtag_action(FILE *file, uint64_t vtag_action)
{
	uint32_t relptr, lid, op, vtag_def;
	char opname[10];

	relptr = vtag_action & NIX_TX_VTAGACT_VTAG0_RELPTR_MASK;
	lid = ((vtag_action & NIX_TX_VTAGACT_VTAG0_LID_MASK) >> 8) &
	      GENMASK(2, 0);
	op = ((vtag_action & NIX_TX_VTAGACT_VTAG0_OP_MASK) >> 12) &
	     GENMASK(1, 0);
	vtag_def = ((vtag_action & NIX_TX_VTAGACT_VTAG0_DEF_MASK) >> 16) &
		   GENMASK(9, 0);

	npc_get_vtag_opname(op, opname, sizeof(opname));

	fprintf(file, "\tVTAG0 relptr:%#X\n", relptr);
	fprintf(file, "\tlid:%#X\n", lid);
	fprintf(file, "\top:%s\n", opname);
	fprintf(file, "\tvtag_def:%#X\n", vtag_def);

	relptr = ((vtag_action & NIX_TX_VTAGACT_VTAG1_RELPTR_MASK) >> 32) &
		 GENMASK(7, 0);
	lid = ((vtag_action & NIX_TX_VTAGACT_VTAG1_LID_MASK) >> 40) &
	      GENMASK(2, 0);
	op = ((vtag_action & NIX_TX_VTAGACT_VTAG1_OP_MASK) >> 44) &
	     GENMASK(1, 0);
	vtag_def = ((vtag_action & NIX_TX_VTAGACT_VTAG1_DEF_MASK) >> 48) &
		   GENMASK(9, 0);

	npc_get_vtag_opname(op, opname, sizeof(opname));

	fprintf(file, "\tVTAG1:relptr:%#X\n", relptr);
	fprintf(file, "\tlid:%#X\n", lid);
	fprintf(file, "\top:%s\n", opname);
	fprintf(file, "\tvtag_def:%#X\n", vtag_def);
}

static void
npc_flow_dump_vtag_action(FILE *file, uint64_t vtag_action, bool is_rx)
{
	if (is_rx) {
		fprintf(file, "NPC RX VTAG Action:%#016lX\n", vtag_action);
		npc_flow_dump_rx_vtag_action(file, vtag_action);
	} else {
		fprintf(file, "NPC TX VTAG Action:%#016lX\n", vtag_action);
		npc_flow_dump_tx_vtag_action(file, vtag_action);
	}
}

static void
npc_flow_hw_mcam_entry_dump(FILE *file, struct npc *npc, struct roc_npc_flow *flow)
{
	uint64_t mcam_data[ROC_NPC_MAX_MCAM_WIDTH_DWORDS];
	uint64_t mcam_mask[ROC_NPC_MAX_MCAM_WIDTH_DWORDS];
	struct npc_mcam_read_entry_req *mcam_read_req;
	struct npc_mcam_read_entry_rsp *mcam_read_rsp;
	struct nix_inl_dev *inl_dev = NULL;
	struct idev_cfg *idev;
	struct mbox *mbox;
	int rc = 0, i;

	idev = idev_get_cfg();
	if (idev)
		inl_dev = idev->nix_inl_dev;

	if (inl_dev && flow->use_pre_alloc)
		mbox = inl_dev->dev.mbox;
	else
		mbox = npc->mbox;

	mcam_read_req = mbox_alloc_msg_npc_mcam_read_entry(mbox_get(mbox));
	if (mcam_read_req == NULL) {
		plt_err("Failed to alloc msg");
		mbox_put(mbox);
		return;
	}

	mcam_read_req->entry = flow->mcam_id;
	rc = mbox_process_msg(mbox, (void *)&mcam_read_rsp);
	if (rc) {
		mbox_put(mbox);
		plt_err("Failed to fetch MCAM entry:%d", flow->mcam_id);
		return;
	}

	mbox_memcpy(mcam_data, mcam_read_rsp->entry_data.kw, sizeof(mcam_data));
	mbox_memcpy(mcam_mask, mcam_read_rsp->entry_data.kw_mask, sizeof(mcam_data));

	fprintf(file, "HW MCAM Data :\n");

	for (i = 0; i < ROC_NPC_MAX_MCAM_WIDTH_DWORDS; i++) {
		fprintf(file, "\tDW%d     :%016lX\n", i, mcam_data[i]);
		fprintf(file, "\tDW%d_Mask:%016lX\n", i, mcam_mask[i]);
	}

	fprintf(file, "\n");
	mbox_put(mbox);
}

void
roc_npc_flow_mcam_dump(FILE *file, struct roc_npc *roc_npc, struct roc_npc_flow *flow)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	uint64_t count = 0;
	bool is_rx = 0;
	int i, rc = 0;

	fprintf(file, "MCAM Index:%d\n", flow->mcam_id);
	if (flow->ctr_id != NPC_COUNTER_NONE && flow->use_ctr) {
		if (flow->use_pre_alloc)
			rc = roc_npc_inl_mcam_read_counter(flow->ctr_id, &count);
		else
			rc = roc_npc_mcam_read_counter(roc_npc, flow->ctr_id, &count);

		if (rc)
			return;
		fprintf(file, "Hit count: %" PRIu64 "\n", count);
	}

	fprintf(file, "Interface :%s (%d)\n", intf_str[flow->nix_intf], flow->nix_intf);
	fprintf(file, "Priority  :%d\n", flow->priority);

	if (flow->nix_intf == NIX_INTF_RX)
		is_rx = 1;

	npc_flow_dump_parsed_action(file, flow->npc_action, is_rx);
	npc_flow_dump_vtag_action(file, flow->vtag_action, is_rx);
	fprintf(file, "Patterns:\n");
	npc_flow_dump_patterns(file, npc, flow);

	fprintf(file, "MCAM Raw Data :\n");

	for (i = 0; i < ROC_NPC_MAX_MCAM_WIDTH_DWORDS; i++) {
		fprintf(file, "\tDW%d     :%016lX\n", i, flow->mcam_data[i]);
		fprintf(file, "\tDW%d_Mask:%016lX\n", i, flow->mcam_mask[i]);
	}

	npc_flow_hw_mcam_entry_dump(file, npc, flow);
}
