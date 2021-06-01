/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_FLOW_H__
#define __OTX2_FLOW_H__

#include <stdint.h>

#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "otx2_common.h"
#include "otx2_ethdev.h"
#include "otx2_mbox.h"

struct otx2_eth_dev;

int otx2_flow_init(struct otx2_eth_dev *hw);
int otx2_flow_fini(struct otx2_eth_dev *hw);
extern const struct rte_flow_ops otx2_flow_ops;

enum {
	OTX2_INTF_RX = 0,
	OTX2_INTF_TX = 1,
	OTX2_INTF_MAX = 2,
};

#define NPC_IH_LENGTH			8
#define NPC_TPID_LENGTH			2
#define NPC_HIGIG2_LENGTH		16
#define NPC_COUNTER_NONE		(-1)
/* 32 bytes from LDATA_CFG & 32 bytes from FLAGS_CFG */
#define NPC_MAX_EXTRACT_DATA_LEN	(64)
#define NPC_LDATA_LFLAG_LEN		(16)
#define NPC_MAX_KEY_NIBBLES		(31)
/* Nibble offsets */
#define NPC_LAYER_KEYX_SZ		(3)
#define NPC_PARSE_KEX_S_LA_OFFSET	(7)
#define NPC_PARSE_KEX_S_LID_OFFSET(lid)		\
	((((lid) - NPC_LID_LA) * NPC_LAYER_KEYX_SZ)  \
	+ NPC_PARSE_KEX_S_LA_OFFSET)


/* supported flow actions flags */
#define OTX2_FLOW_ACT_MARK    (1 << 0)
#define OTX2_FLOW_ACT_FLAG    (1 << 1)
#define OTX2_FLOW_ACT_DROP    (1 << 2)
#define OTX2_FLOW_ACT_QUEUE   (1 << 3)
#define OTX2_FLOW_ACT_RSS     (1 << 4)
#define OTX2_FLOW_ACT_DUP     (1 << 5)
#define OTX2_FLOW_ACT_SEC     (1 << 6)
#define OTX2_FLOW_ACT_COUNT   (1 << 7)
#define OTX2_FLOW_ACT_PF      (1 << 8)
#define OTX2_FLOW_ACT_VF      (1 << 9)
#define OTX2_FLOW_ACT_VLAN_STRIP (1 << 10)
#define OTX2_FLOW_ACT_VLAN_INSERT (1 << 11)
#define OTX2_FLOW_ACT_VLAN_ETHTYPE_INSERT (1 << 12)
#define OTX2_FLOW_ACT_VLAN_PCP_INSERT (1 << 13)

/* terminating actions */
#define OTX2_FLOW_ACT_TERM    (OTX2_FLOW_ACT_DROP  | \
			       OTX2_FLOW_ACT_QUEUE | \
			       OTX2_FLOW_ACT_RSS   | \
			       OTX2_FLOW_ACT_DUP   | \
			       OTX2_FLOW_ACT_SEC)

/* This mark value indicates flag action */
#define OTX2_FLOW_FLAG_VAL    (0xffff)

#define NIX_RX_ACT_MATCH_OFFSET		(40)
#define NIX_RX_ACT_MATCH_MASK		(0xFFFF)

#define NIX_RSS_ACT_GRP_OFFSET		(20)
#define NIX_RSS_ACT_ALG_OFFSET		(56)
#define NIX_RSS_ACT_GRP_MASK		(0xFFFFF)
#define NIX_RSS_ACT_ALG_MASK		(0x1F)

/* PMD-specific definition of the opaque struct rte_flow */
#define OTX2_MAX_MCAM_WIDTH_DWORDS	7

enum npc_mcam_intf {
	NPC_MCAM_RX,
	NPC_MCAM_TX
};

struct npc_xtract_info {
	/* Length in bytes of pkt data extracted. len = 0
	 * indicates that extraction is disabled.
	 */
	uint8_t len;
	uint8_t hdr_off; /* Byte offset of proto hdr: extract_src */
	uint8_t key_off; /* Byte offset in MCAM key where data is placed */
	uint8_t enable; /* Extraction enabled or disabled */
	uint8_t flags_enable; /* Flags extraction enabled */
};

/* Information for a given {LAYER, LTYPE} */
struct npc_lid_lt_xtract_info {
	/* Info derived from parser configuration */
	uint16_t npc_proto;              /* Network protocol identified */
	uint8_t  valid_flags_mask;       /* Flags applicable */
	uint8_t  is_terminating:1;       /* No more parsing */
	struct npc_xtract_info xtract[NPC_MAX_LD];
};

union npc_kex_ldata_flags_cfg {
	struct {
	#if defined(__BIG_ENDIAN_BITFIELD)
		uint64_t rvsd_62_1	: 61;
		uint64_t lid		: 3;
	#else
		uint64_t lid		: 3;
		uint64_t rvsd_62_1	: 61;
	#endif
	} s;

	uint64_t i;
};

typedef struct npc_lid_lt_xtract_info
	otx2_dxcfg_t[NPC_MAX_INTF][NPC_MAX_LID][NPC_MAX_LT];
typedef struct npc_lid_lt_xtract_info
	otx2_fxcfg_t[NPC_MAX_INTF][NPC_MAX_LD][NPC_MAX_LFL];
typedef union npc_kex_ldata_flags_cfg otx2_ld_flags_t[NPC_MAX_LD];


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

struct otx2_mcam_ents_info {
	/* Current max & min values of mcam index */
	uint32_t max_id;
	uint32_t min_id;
	uint32_t free_ent;
	uint32_t live_ent;
};

struct rte_flow {
	uint8_t  nix_intf;
	uint32_t  mcam_id;
	int32_t ctr_id;
	uint32_t priority;
	/* Contiguous match string */
	uint64_t mcam_data[OTX2_MAX_MCAM_WIDTH_DWORDS];
	uint64_t mcam_mask[OTX2_MAX_MCAM_WIDTH_DWORDS];
	uint64_t npc_action;
	uint64_t vtag_action;
	TAILQ_ENTRY(rte_flow) next;
};

TAILQ_HEAD(otx2_flow_list, rte_flow);

/* Accessed from ethdev private - otx2_eth_dev */
struct otx2_npc_flow_info {
	rte_atomic32_t mark_actions;
	uint32_t vtag_actions;
	uint32_t keyx_supp_nmask[NPC_MAX_INTF];/* nibble mask */
	uint32_t keyx_len[NPC_MAX_INTF];	/* per intf key len in bits */
	uint32_t datax_len[NPC_MAX_INTF];	/* per intf data len in bits */
	uint32_t keyw[NPC_MAX_INTF];		/* max key + data len bits */
	uint32_t mcam_entries;			/* mcam entries supported */
	otx2_dxcfg_t prx_dxcfg;			/* intf, lid, lt, extract */
	otx2_fxcfg_t prx_fxcfg;			/* Flag extract */
	otx2_ld_flags_t prx_lfcfg;		/* KEX LD_Flags CFG */
	/* mcam entry info per priority level: both free & in-use */
	struct otx2_mcam_ents_info *flow_entry_info;
	/* Bitmap of free preallocated entries in ascending index &
	 * descending priority
	 */
	struct rte_bitmap **free_entries;
	/* Bitmap of free preallocated entries in descending index &
	 * ascending priority
	 */
	struct rte_bitmap **free_entries_rev;
	/* Bitmap of live entries in ascending index & descending priority */
	struct rte_bitmap **live_entries;
	/* Bitmap of live entries in descending index & ascending priority */
	struct rte_bitmap **live_entries_rev;
	/* Priority bucket wise tail queue of all rte_flow resources */
	struct otx2_flow_list *flow_list;
	uint32_t rss_grps;  /* rss groups supported */
	struct rte_bitmap *rss_grp_entries;
	uint16_t channel; /*rx channel */
	uint16_t flow_prealloc_size;
	uint16_t flow_max_priority;
	uint16_t switch_header_type;
};

struct otx2_parse_state {
	struct otx2_npc_flow_info *npc;
	const struct rte_flow_item *pattern;
	const struct rte_flow_item *last_pattern; /* Temp usage */
	struct rte_flow_error *error;
	struct rte_flow *flow;
	uint8_t tunnel;
	uint8_t terminate;
	uint8_t layer_mask;
	uint8_t lt[NPC_MAX_LID];
	uint8_t flags[NPC_MAX_LID];
	uint8_t *mcam_data; /* point to flow->mcam_data + key_len */
	uint8_t *mcam_mask; /* point to flow->mcam_mask + key_len */
	bool is_vf;
};

struct otx2_flow_item_info {
	const void *def_mask; /* rte_flow default mask */
	void *hw_mask;        /* hardware supported mask */
	int  len;             /* length of item */
	const void *spec;     /* spec to use, NULL implies match any */
	const void *mask;     /* mask to use */
	uint8_t hw_hdr_len;  /* Extra data len at each layer*/
};

struct otx2_idev_kex_cfg {
	struct npc_get_kex_cfg_rsp kex_cfg;
	rte_atomic16_t kex_refcnt;
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


int otx2_flow_mcam_free_counter(struct otx2_mbox *mbox, uint16_t ctr_id);

int otx2_flow_mcam_read_counter(struct otx2_mbox *mbox, uint32_t ctr_id,
				uint64_t *count);

int otx2_flow_mcam_clear_counter(struct otx2_mbox *mbox, uint32_t ctr_id);

int otx2_flow_mcam_free_entry(struct otx2_mbox *mbox, uint32_t entry);

int otx2_flow_mcam_free_all_entries(struct otx2_mbox *mbox);

int otx2_flow_update_parse_state(struct otx2_parse_state *pst,
				 struct otx2_flow_item_info *info,
				 int lid, int lt, uint8_t flags);

int otx2_flow_parse_item_basic(const struct rte_flow_item *item,
			       struct otx2_flow_item_info *info,
			       struct rte_flow_error *error);

void otx2_flow_keyx_compress(uint64_t *data, uint32_t nibble_mask);

int otx2_flow_mcam_alloc_and_write(struct rte_flow *flow,
				   struct otx2_mbox *mbox,
				   struct otx2_parse_state *pst,
				   struct otx2_npc_flow_info *flow_info);

void otx2_flow_get_hw_supp_mask(struct otx2_parse_state *pst,
				struct otx2_flow_item_info *info,
				int lid, int lt);

const struct rte_flow_item *
otx2_flow_skip_void_and_any_items(const struct rte_flow_item *pattern);

int otx2_flow_parse_lh(struct otx2_parse_state *pst);

int otx2_flow_parse_lg(struct otx2_parse_state *pst);

int otx2_flow_parse_lf(struct otx2_parse_state *pst);

int otx2_flow_parse_le(struct otx2_parse_state *pst);

int otx2_flow_parse_ld(struct otx2_parse_state *pst);

int otx2_flow_parse_lc(struct otx2_parse_state *pst);

int otx2_flow_parse_lb(struct otx2_parse_state *pst);

int otx2_flow_parse_la(struct otx2_parse_state *pst);

int otx2_flow_parse_higig2_hdr(struct otx2_parse_state *pst);

int otx2_flow_parse_actions(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_action actions[],
			    struct rte_flow_error *error,
			    struct rte_flow *flow);

int otx2_flow_free_all_resources(struct otx2_eth_dev *hw);

int otx2_flow_parse_mpls(struct otx2_parse_state *pst, int lid);
#endif /* __OTX2_FLOW_H__ */
