/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NPC_PRIV_H_
#define _ROC_NPC_PRIV_H_

#define NPC_IH_LENGTH	     8
#define NPC_TPID_LENGTH	     2
#define NPC_HIGIG2_LENGTH    16
#define NPC_MAX_RAW_ITEM_LEN 16
#define NPC_COUNTER_NONE     (-1)

#define NPC_RSS_GRPS 8

#define NPC_ACTION_FLAG_DEFAULT 0xffff

#define NPC_PFVF_FUNC_MASK 0x3FF

/* 32 bytes from LDATA_CFG & 32 bytes from FLAGS_CFG */
#define NPC_MAX_EXTRACT_DATA_LEN (64)
#define NPC_MAX_EXTRACT_HW_LEN	 (4 * NPC_MAX_EXTRACT_DATA_LEN)
#define NPC_LDATA_LFLAG_LEN	 (16)
#define NPC_MAX_KEY_NIBBLES	 (31)

/* Nibble offsets */
#define NPC_LAYER_KEYX_SZ	  (3)
#define NPC_PARSE_KEX_S_LA_OFFSET (7)
#define NPC_PARSE_KEX_S_LID_OFFSET(lid)                                        \
	((((lid) - (NPC_LID_LA)) * NPC_LAYER_KEYX_SZ) +                        \
	 NPC_PARSE_KEX_S_LA_OFFSET)

/* This mark value indicates flag action */
#define NPC_FLOW_FLAG_VAL (0xffff)

#define NPC_RX_ACT_MATCH_OFFSET (40)
#define NPC_RX_ACT_MATCH_MASK	(0xFFFF)

#define NPC_RSS_ACT_GRP_OFFSET (20)
#define NPC_RSS_ACT_ALG_OFFSET (56)
#define NPC_RSS_ACT_GRP_MASK   (0xFFFFF)
#define NPC_RSS_ACT_ALG_MASK   (0x1F)

#define NPC_MCAM_KEX_FIELD_MAX	  23
#define NPC_MCAM_MAX_PROTO_FIELDS (NPC_MCAM_KEX_FIELD_MAX + 1)
#define NPC_MCAM_KEY_X4_WORDS	  7 /* Number of 64-bit words */

#define NPC_RVUPF_MAX_9XXX 0x10 /* HRM: RVU_PRIV_CONST */
#define NPC_RVUPF_MAX_98XX 0x18 /* HRM: RVU_PRIV_CONST */
#define NPC_RVUPF_MAX_10XX 0x20 /* HRM: RVU_PRIV_CONST */
#define NPC_NIXLF_MAX	   0x80 /* HRM: NIX_AF_CONST2 */
#define NPC_MCAME_PER_PF   3	/* DRV: RSVD_MCAM_ENTRIES_PER_PF */
#define NPC_MCAME_PER_LF   1	/* DRV: RSVD_MCAM_ENTRIES_PER_NIXLF */
#define NPC_NIXLF_MAX_98XX (2 * NPC_NIXLF_MAX) /*2 NIXLFs */
#define NPC_MCAME_RESVD_9XXX                                                   \
	(NPC_NIXLF_MAX * NPC_MCAME_PER_LF +                                    \
	 (NPC_RVUPF_MAX_9XXX - 1) * NPC_MCAME_PER_PF)

#define NPC_MCAME_RESVD_10XX                                                   \
	(NPC_NIXLF_MAX * NPC_MCAME_PER_LF +                                    \
	 (NPC_RVUPF_MAX_10XX - 1) * NPC_MCAME_PER_PF)

#define NPC_MCAME_RESVD_98XX                                                   \
	(NPC_NIXLF_MAX_98XX * NPC_MCAME_PER_LF +                               \
	 (NPC_RVUPF_MAX_98XX - 1) * NPC_MCAME_PER_PF)

#define NPC_ACTION_MAX_VLAN_PARAMS    3
#define NPC_ACTION_MAX_VLANS_STRIPPED 2

#define NPC_LTYPE_OFFSET_START 7
/* LB OFFSET : START + LA (2b flags + 1b ltype) + LB (2b flags) */
#define NPC_LTYPE_LB_OFFSET (NPC_LTYPE_OFFSET_START + 5)
#define NPC_LFLAG_LB_OFFSET (NPC_LTYPE_OFFSET_START + 3)
/* LC OFFSET : START + LA (2b flags + 1b ltype) + LB (2b flags + 1b ltype) + LC
 * (2b flags)
 */
#define NPC_LFLAG_LC_OFFSET (NPC_LTYPE_OFFSET_START + 6)
#define NPC_LTYPE_LC_OFFSET (NPC_LTYPE_OFFSET_START + 8)

#define CN10K_SDP_CH_START 0x80
#define CN10K_SDP_CH_MASK  0xF80

struct npc_action_vtag_info {
	uint16_t vlan_id;
	uint16_t vlan_ethtype;
	uint8_t vlan_pcp;
};

enum npc_err_status {
	NPC_ERR_PARAM = -1024,
	NPC_ERR_NO_MEM,
	NPC_ERR_INVALID_SPEC,
	NPC_ERR_INVALID_MASK,
	NPC_ERR_INVALID_RANGE,
	NPC_ERR_INVALID_KEX,
	NPC_ERR_INVALID_SIZE,
	NPC_ERR_INTERNAL,
	NPC_ERR_MCAM_ALLOC,
	NPC_ERR_ACTION_NOTSUP,
	NPC_ERR_PATTERN_NOTSUP,
};

enum npc_mcam_intf { NPC_MCAM_RX, NPC_MCAM_TX };

typedef union npc_kex_cap_terms_t {
	/** Packet Matching Rule term fields */
	struct {
		/** Total length of received packet */
		uint64_t len : 1;
		/** Initial (outer) Ethertype only */
		uint64_t ethtype_0 : 1;
		/** Ethertype of most inner VLAN tag */
		uint64_t ethtype_x : 1;
		/** First VLAN ID (outer) */
		uint64_t vlan_id_0 : 1;
		/** Last VLAN ID (inner) */
		uint64_t vlan_id_x : 1;
		/** PCP in the first VLAN header */
		uint64_t vlan_pcp_0 : 1;
		/** destination MAC address */
		uint64_t dmac : 1;
		/** IP Protocol or IPv6 Next Header */
		uint64_t ip_proto : 1;
		/** DSCP in IP header */
		uint64_t ip_dscp : 1;
		/** Destination UDP port, implies IPPROTO=17 */
		uint64_t udp_dport : 1;
		/** Destination TCP port implies IPPROTO=6 */
		uint64_t tcp_dport : 1;
		/** Source UDP Port */
		uint64_t udp_sport : 1;
		/** Source TCP port */
		uint64_t tcp_sport : 1;
		/** Source IP address */
		uint64_t sip_addr : 1;
		/** Destination IP address */
		uint64_t dip_addr : 1;
		/** Source IP address */
		uint64_t sip6_addr : 1;
		/** Destination IP address */
		uint64_t dip6_addr : 1;
		/** IPsec session identifier */
		uint64_t ipsec_spi : 1;
		/** NVGRE/VXLAN network identifier */
		uint64_t ld_vni : 1;
		/** Custom frame match rule. PMR offset is counted from
		 *  the start of the packet.
		 */
		uint64_t custom_frame : 1;
		/** Custom layer 3 match rule. PMR offset is counted from
		 *  the start of layer 3 in the packet.
		 */
		uint64_t custom_l3 : 1;
		/** IGMP Group address */
		uint64_t igmp_grp_addr : 1;
		/** ICMP identifier */
		uint64_t icmp_id : 1;
		/** ICMP type */
		uint64_t icmp_type : 1;
		/** ICMP code */
		uint64_t icmp_code : 1;
		/** Source SCTP port */
		uint64_t sctp_sport : 1;
		/** Destination SCTP port */
		uint64_t sctp_dport : 1;
		/** GTPv1 tunnel endpoint identifier */
		uint64_t gtpv1_teid : 1;
	} bit;

	/** All bits of the bit field structure */
	uint64_t all_bits;

} npc_kex_cap_terms_t;

struct npc_parse_item_info {
	const void *def_mask; /* default mask */
	void *hw_mask;	      /* hardware supported mask */
	int len;	      /* length of item */
	const void *spec;     /* spec to use, NULL implies match any */
	const void *mask;     /* mask to use */
	uint8_t hw_hdr_len;   /* Extra data len at each layer*/
};

struct npc_parse_state {
	struct npc *npc;
	const struct roc_npc_item_info *pattern;
	const struct roc_npc_item_info *last_pattern;
	struct roc_npc_flow *flow;
	uint8_t nix_intf;
	uint8_t tunnel;
	uint8_t terminate;
	uint8_t layer_mask;
	uint8_t lt[NPC_MAX_LID];
	uint8_t flags[NPC_MAX_LID];
	uint8_t *mcam_data; /* point to flow->mcam_data + key_len */
	uint8_t *mcam_mask; /* point to flow->mcam_mask + key_len */
	bool is_vf;
	/* adjust ltype in MCAM to match at least one vlan */
	bool set_vlan_ltype_mask;
	bool set_ipv6ext_ltype_mask;
	bool is_second_pass_rule;
	bool has_eth_type;
};

enum npc_kpu_parser_flag {
	NPC_F_NA = 0,
	NPC_F_PKI,
	NPC_F_PKI_VLAN,
	NPC_F_PKI_ETAG,
	NPC_F_PKI_ITAG,
	NPC_F_PKI_MPLS,
	NPC_F_PKI_NSH,
	NPC_F_ETYPE_UNK,
	NPC_F_ETHER_VLAN,
	NPC_F_ETHER_ETAG,
	NPC_F_ETHER_ITAG,
	NPC_F_ETHER_MPLS,
	NPC_F_ETHER_NSH,
	NPC_F_STAG_CTAG,
	NPC_F_STAG_CTAG_UNK,
	NPC_F_STAG_STAG_CTAG,
	NPC_F_STAG_STAG_STAG,
	NPC_F_QINQ_CTAG,
	NPC_F_QINQ_CTAG_UNK,
	NPC_F_QINQ_QINQ_CTAG,
	NPC_F_QINQ_QINQ_QINQ,
	NPC_F_BTAG_ITAG,
	NPC_F_BTAG_ITAG_STAG,
	NPC_F_BTAG_ITAG_CTAG,
	NPC_F_BTAG_ITAG_UNK,
	NPC_F_ETAG_CTAG,
	NPC_F_ETAG_BTAG_ITAG,
	NPC_F_ETAG_STAG,
	NPC_F_ETAG_QINQ,
	NPC_F_ETAG_ITAG,
	NPC_F_ETAG_ITAG_STAG,
	NPC_F_ETAG_ITAG_CTAG,
	NPC_F_ETAG_ITAG_UNK,
	NPC_F_ITAG_STAG_CTAG,
	NPC_F_ITAG_STAG,
	NPC_F_ITAG_CTAG,
	NPC_F_MPLS_4_LABELS,
	NPC_F_MPLS_3_LABELS,
	NPC_F_MPLS_2_LABELS,
	NPC_F_IP_HAS_OPTIONS,
	NPC_F_IP_IP_IN_IP,
	NPC_F_IP_6TO4,
	NPC_F_IP_MPLS_IN_IP,
	NPC_F_IP_UNK_PROTO,
	NPC_F_IP_IP_IN_IP_HAS_OPTIONS,
	NPC_F_IP_6TO4_HAS_OPTIONS,
	NPC_F_IP_MPLS_IN_IP_HAS_OPTIONS,
	NPC_F_IP_UNK_PROTO_HAS_OPTIONS,
	NPC_F_IP6_HAS_EXT,
	NPC_F_IP6_TUN_IP6,
	NPC_F_IP6_MPLS_IN_IP,
	NPC_F_TCP_HAS_OPTIONS,
	NPC_F_TCP_HTTP,
	NPC_F_TCP_HTTPS,
	NPC_F_TCP_PPTP,
	NPC_F_TCP_UNK_PORT,
	NPC_F_TCP_HTTP_HAS_OPTIONS,
	NPC_F_TCP_HTTPS_HAS_OPTIONS,
	NPC_F_TCP_PPTP_HAS_OPTIONS,
	NPC_F_TCP_UNK_PORT_HAS_OPTIONS,
	NPC_F_UDP_VXLAN,
	NPC_F_UDP_VXLAN_NOVNI,
	NPC_F_UDP_VXLAN_NOVNI_NSH,
	NPC_F_UDP_VXLANGPE,
	NPC_F_UDP_VXLANGPE_NSH,
	NPC_F_UDP_VXLANGPE_MPLS,
	NPC_F_UDP_VXLANGPE_NOVNI,
	NPC_F_UDP_VXLANGPE_NOVNI_NSH,
	NPC_F_UDP_VXLANGPE_NOVNI_MPLS,
	NPC_F_UDP_VXLANGPE_UNK,
	NPC_F_UDP_VXLANGPE_NONP,
	NPC_F_UDP_GTP_GTPC,
	NPC_F_UDP_GTP_GTPU_G_PDU,
	NPC_F_UDP_GTP_GTPU_UNK,
	NPC_F_UDP_UNK_PORT,
	NPC_F_UDP_GENEVE,
	NPC_F_UDP_GENEVE_OAM,
	NPC_F_UDP_GENEVE_CRI_OPT,
	NPC_F_UDP_GENEVE_OAM_CRI_OPT,
	NPC_F_GRE_NVGRE,
	NPC_F_GRE_HAS_SRE,
	NPC_F_GRE_HAS_CSUM,
	NPC_F_GRE_HAS_KEY,
	NPC_F_GRE_HAS_SEQ,
	NPC_F_GRE_HAS_CSUM_KEY,
	NPC_F_GRE_HAS_CSUM_SEQ,
	NPC_F_GRE_HAS_KEY_SEQ,
	NPC_F_GRE_HAS_CSUM_KEY_SEQ,
	NPC_F_GRE_HAS_ROUTE,
	NPC_F_GRE_UNK_PROTO,
	NPC_F_GRE_VER1,
	NPC_F_GRE_VER1_HAS_SEQ,
	NPC_F_GRE_VER1_HAS_ACK,
	NPC_F_GRE_VER1_HAS_SEQ_ACK,
	NPC_F_GRE_VER1_UNK_PROTO,
	NPC_F_TU_ETHER_UNK,
	NPC_F_TU_ETHER_CTAG,
	NPC_F_TU_ETHER_CTAG_UNK,
	NPC_F_TU_ETHER_STAG_CTAG,
	NPC_F_TU_ETHER_STAG_CTAG_UNK,
	NPC_F_TU_ETHER_STAG,
	NPC_F_TU_ETHER_STAG_UNK,
	NPC_F_TU_ETHER_QINQ_CTAG,
	NPC_F_TU_ETHER_QINQ_CTAG_UNK,
	NPC_F_TU_ETHER_QINQ,
	NPC_F_TU_ETHER_QINQ_UNK,
	NPC_F_LAST /* has to be the last item */
};

#define NPC_ACTION_TERM                                                        \
	(ROC_NPC_ACTION_TYPE_DROP | ROC_NPC_ACTION_TYPE_QUEUE |                \
	 ROC_NPC_ACTION_TYPE_RSS | ROC_NPC_ACTION_TYPE_DUP |                   \
	 ROC_NPC_ACTION_TYPE_SEC)

struct npc_xtract_info {
	/* Length in bytes of pkt data extracted. len = 0
	 * indicates that extraction is disabled.
	 */
	uint8_t len;
	uint8_t hdr_off;      /* Byte offset of proto hdr: extract_src */
	uint8_t key_off;      /* Byte offset in MCAM key where data is placed */
	uint8_t enable;	      /* Extraction enabled or disabled */
	uint8_t flags_enable; /* Flags extraction enabled */
	uint8_t use_hash;     /* Use field hash */
};

/* Information for a given {LAYER, LTYPE} */
struct npc_lid_lt_xtract_info {
	/* Info derived from parser configuration */
	uint16_t npc_proto;	    /* Network protocol identified */
	uint8_t valid_flags_mask;   /* Flags applicable */
	uint8_t is_terminating : 1; /* No more parsing */
	struct npc_xtract_info xtract[NPC_MAX_LD];
};

union npc_kex_ldata_flags_cfg {
	struct {
		uint64_t lid : 3;
		uint64_t rvsd_62_1 : 61;
	} s;

	uint64_t i;
};

typedef struct npc_lid_lt_xtract_info npc_dxcfg_t[NPC_MAX_INTF][NPC_MAX_LID]
						 [NPC_MAX_LT];
typedef struct npc_lid_lt_xtract_info npc_fxcfg_t[NPC_MAX_INTF][NPC_MAX_LD]
						 [NPC_MAX_LFL];
typedef union npc_kex_ldata_flags_cfg npc_ld_flags_t[NPC_MAX_LD];

/* MBOX_MSG_NPC_GET_DATAX_CFG Response */
struct npc_get_datax_cfg {
	/* NPC_AF_KEX_LDATA(0..1)_FLAGS_CFG */
	union npc_kex_ldata_flags_cfg ld_flags[NPC_MAX_LD];
	/* Extract information indexed with [LID][LTYPE] */
	struct npc_lid_lt_xtract_info lid_lt_xtract[NPC_MAX_LID][NPC_MAX_LT];
	/* Flags based extract indexed with [LDATA][FLAGS_LOWER_NIBBLE]
	 * Fields flags_ena_ld0, flags_ena_ld1 in
	 * struct npc_lid_lt_xtract_info indicate if this is applicable
	 * for a given {LAYER, LTYPE}
	 */
	struct npc_xtract_info flag_xtract[NPC_MAX_LD][NPC_MAX_LT];
};

TAILQ_HEAD(npc_flow_list, roc_npc_flow);

struct npc_prio_flow_entry {
	struct roc_npc_flow *flow;
	TAILQ_ENTRY(npc_prio_flow_entry) next;
};

TAILQ_HEAD(npc_prio_flow_list_head, npc_prio_flow_entry);

struct npc {
	struct mbox *mbox;			/* Mbox */
	uint32_t keyx_supp_nmask[NPC_MAX_INTF]; /* nibble mask */
	uint8_t hash_extract_cap;		/* hash extract support */
	uint8_t profile_name[MKEX_NAME_LEN];	/* KEX profile name */
	uint32_t keyx_len[NPC_MAX_INTF];	/* per intf key len in bits */
	uint32_t datax_len[NPC_MAX_INTF];	/* per intf data len in bits */
	uint32_t keyw[NPC_MAX_INTF];		/* max key + data len bits */
	uint32_t mcam_entries;			/* mcam entries supported */
	uint16_t channel;			/* RX Channel number */
	bool is_sdp_link;
	uint16_t sdp_channel;
	uint16_t sdp_channel_mask;
	uint32_t rss_grps;			/* rss groups supported */
	uint16_t flow_prealloc_size;		/* Pre allocated mcam size */
	uint16_t flow_max_priority;		/* Max priority for flow */
	uint16_t switch_header_type; /* Supported switch header type */
	uint32_t mark_actions;
	uint32_t vtag_strip_actions; /* vtag insert/strip actions */
	uint16_t pf_func;	     /* pf_func of device */
	npc_dxcfg_t prx_dxcfg;	     /* intf, lid, lt, extract */
	npc_fxcfg_t prx_fxcfg;	     /* Flag extract */
	npc_ld_flags_t prx_lfcfg;    /* KEX LD_Flags CFG */
	struct npc_flow_list *flow_list;
	struct npc_prio_flow_list_head *prio_flow_list;
	struct plt_bitmap *rss_grp_entries;
	struct npc_flow_list ipsec_list;
};

#define NPC_HASH_FIELD_LEN 16

struct npc_hash_cfg {
	uint64_t secret_key[3];
	/* NPC_AF_INTF(0..1)_HASH(0..1)_MASK(0..1) */
	uint64_t hash_mask[NPC_MAX_INTF][NPC_MAX_HASH][NPC_MAX_HASH_MASK];
	/* NPC_AF_INTF(0..1)_HASH(0..1)_RESULT_CTRL */
	uint64_t hash_ctrl[NPC_MAX_INTF][NPC_MAX_HASH];
};

static inline struct npc *
roc_npc_to_npc_priv(struct roc_npc *npc)
{
	return (struct npc *)npc->reserved;
}

int npc_mcam_free_counter(struct mbox *mbox, uint16_t ctr_id);
int npc_mcam_read_counter(struct mbox *mbox, uint32_t ctr_id, uint64_t *count);
int npc_mcam_clear_counter(struct mbox *mbox, uint32_t ctr_id);
int npc_mcam_free_entry(struct mbox *mbox, uint32_t entry);
int npc_mcam_free_all_entries(struct npc *npc);
int npc_mcam_alloc_and_write(struct npc *npc, struct roc_npc_flow *flow,
			     struct npc_parse_state *pst);
int npc_mcam_alloc_entry(struct npc *npc, struct roc_npc_flow *mcam,
			 struct roc_npc_flow *ref_mcam, int prio,
			 int *resp_count);
int npc_mcam_alloc_entries(struct mbox *mbox, int ref_mcam, int *alloc_entry, int req_count,
			   int prio, int *resp_count, bool is_conti);

int npc_mcam_ena_dis_entry(struct npc *npc, struct roc_npc_flow *mcam, bool enable);
int npc_mcam_write_entry(struct mbox *mbox, struct roc_npc_flow *mcam);
int npc_flow_enable_all_entries(struct npc *npc, bool enable);
int npc_update_parse_state(struct npc_parse_state *pst, struct npc_parse_item_info *info, int lid,
			   int lt, uint8_t flags);
void npc_get_hw_supp_mask(struct npc_parse_state *pst, struct npc_parse_item_info *info, int lid,
			  int lt);
int npc_mask_is_supported(const char *mask, const char *hw_mask, int len);
int npc_parse_item_basic(const struct roc_npc_item_info *item, struct npc_parse_item_info *info);
int npc_parse_meta_items(struct npc_parse_state *pst);
int npc_parse_mark_item(struct npc_parse_state *pst);
int npc_parse_pre_l2(struct npc_parse_state *pst);
int npc_parse_higig2_hdr(struct npc_parse_state *pst);
int npc_parse_cpt_hdr(struct npc_parse_state *pst);
int npc_parse_la(struct npc_parse_state *pst);
int npc_parse_lb(struct npc_parse_state *pst);
int npc_parse_lc(struct npc_parse_state *pst);
int npc_parse_ld(struct npc_parse_state *pst);
int npc_parse_le(struct npc_parse_state *pst);
int npc_parse_lf(struct npc_parse_state *pst);
int npc_parse_lg(struct npc_parse_state *pst);
int npc_parse_lh(struct npc_parse_state *pst);
int npc_mcam_fetch_kex_cfg(struct npc *npc);
int npc_mcam_fetch_hw_cap(struct npc *npc, uint8_t *npc_hw_cap);
int npc_get_free_mcam_entry(struct mbox *mbox, struct roc_npc_flow *flow, struct npc *npc);
void npc_delete_prio_list_entry(struct npc *npc, struct roc_npc_flow *flow);
int npc_flow_free_all_resources(struct npc *npc);
const struct roc_npc_item_info *
npc_parse_skip_void_and_any_items(const struct roc_npc_item_info *pattern);
int npc_program_mcam(struct npc *npc, struct npc_parse_state *pst, bool mcam_alloc);
uint64_t npc_get_kex_capability(struct npc *npc);
int npc_process_ipv6_field_hash(const struct roc_npc_flow_item_ipv6 *ipv6_spec,
				const struct roc_npc_flow_item_ipv6 *ipv6_mask,
				struct npc_parse_state *pst, uint8_t type);
int npc_rss_free_grp_get(struct npc *npc, uint32_t *grp);
int npc_rss_action_configure(struct roc_npc *roc_npc, const struct roc_npc_action_rss *rss,
			     uint8_t *alg_idx, uint32_t *rss_grp, uint32_t mcam_id);
int npc_rss_action_program(struct roc_npc *roc_npc, const struct roc_npc_action actions[],
			   struct roc_npc_flow *flow);
int npc_rss_group_free(struct npc *npc, struct roc_npc_flow *flow);
int npc_mcam_init(struct npc *npc, struct roc_npc_flow *flow, int mcam_id);
int npc_mcam_move(struct mbox *mbox, uint16_t old_ent, uint16_t new_ent);
#endif /* _ROC_NPC_PRIV_H_ */
