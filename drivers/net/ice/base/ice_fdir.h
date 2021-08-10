/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_FDIR_H_
#define _ICE_FDIR_H_

#include "ice_common.h"

#define ICE_FDIR_IP_PROTOCOLS
#define ICE_IP_PROTO_TCP		6
#define ICE_IP_PROTO_UDP		17
#define ICE_IP_PROTO_SCTP		132
#define ICE_IP_PROTO_IP			0
#define ICE_IP_PROTO_ESP		50

#define ICE_FDIR_GTPU_IP_INNER_PKT_OFF 50
#define ICE_FDIR_GTPU_EH_INNER_PKT_OFF 58
#define ICE_FDIR_IPV4_GRE_INNER_PKT_OFF 38
#define ICE_FDIR_IPV6_GRE_INNER_PKT_OFF 58
#define ICE_FDIR_V4_V4_GTPOGRE_PKT_OFF	74
#define ICE_FDIR_V4_V6_GTPOGRE_PKT_OFF	94
#define ICE_FDIR_V6_V4_GTPOGRE_PKT_OFF	94
#define ICE_FDIR_V6_V6_GTPOGRE_PKT_OFF	114
#define ICE_FDIR_V4_V4_GTPOGRE_EH_PKT_OFF	82
#define ICE_FDIR_V4_V6_GTPOGRE_EH_PKT_OFF	102
#define ICE_FDIR_V6_V4_GTPOGRE_EH_PKT_OFF	102
#define ICE_FDIR_V6_V6_GTPOGRE_EH_PKT_OFF	122

#define ICE_FDIR_TUN_PKT_OFF		50
#define ICE_FDIR_MAX_RAW_PKT_SIZE	(512 + ICE_FDIR_TUN_PKT_OFF)
#define ICE_FDIR_BUF_FULL_MARGIN	10

/* macros for offsets into packets for flow director programming */
#define ICE_IPV4_SRC_ADDR_OFFSET	26
#define ICE_IPV4_DST_ADDR_OFFSET	30
#define ICE_IPV4_TCP_SRC_PORT_OFFSET	34
#define ICE_IPV4_TCP_DST_PORT_OFFSET	36
#define ICE_IPV4_UDP_SRC_PORT_OFFSET	34
#define ICE_IPV4_UDP_DST_PORT_OFFSET	36
#define ICE_IPV4_SCTP_SRC_PORT_OFFSET	34
#define ICE_IPV4_SCTP_DST_PORT_OFFSET	36
#define ICE_IPV4_PROTO_OFFSET		23
#define ICE_IPV6_SRC_ADDR_OFFSET	22
#define ICE_IPV6_DST_ADDR_OFFSET	38
#define ICE_IPV6_TCP_SRC_PORT_OFFSET	54
#define ICE_IPV6_TCP_DST_PORT_OFFSET	56
#define ICE_IPV6_UDP_SRC_PORT_OFFSET	54
#define ICE_IPV6_UDP_DST_PORT_OFFSET	56
#define ICE_IPV6_SCTP_SRC_PORT_OFFSET	54
#define ICE_IPV6_SCTP_DST_PORT_OFFSET	56

#define ICE_MAC_ETHTYPE_OFFSET		12
#define ICE_IPV4_TOS_OFFSET		15
#define ICE_IPV4_ID_OFFSET		18
#define ICE_IPV4_TTL_OFFSET		22
#define ICE_IPV6_TC_OFFSET		14
#define ICE_IPV6_HLIM_OFFSET		21
#define ICE_IPV6_PROTO_OFFSET		20
#define ICE_IPV6_ID_OFFSET		58
/* For TUN inner (without inner MAC) */
#define ICE_IPV4_NO_MAC_TOS_OFFSET	1
#define ICE_IPV4_NO_MAC_TTL_OFFSET	8
#define ICE_IPV4_NO_MAC_PROTO_OFFSET	9
#define ICE_IPV4_NO_MAC_SRC_ADDR_OFFSET	12
#define ICE_IPV4_NO_MAC_DST_ADDR_OFFSET	16
#define ICE_TCP4_NO_MAC_SRC_PORT_OFFSET	20
#define ICE_TCP4_NO_MAC_DST_PORT_OFFSET	22
#define ICE_UDP4_NO_MAC_SRC_PORT_OFFSET	20
#define ICE_UDP4_NO_MAC_DST_PORT_OFFSET	22
#define ICE_IPV6_NO_MAC_TC_OFFSET	0
#define ICE_IPV6_NO_MAC_HLIM_OFFSET	7
#define ICE_IPV6_NO_MAC_PROTO_OFFSET	6
#define ICE_IPV6_NO_MAC_SRC_ADDR_OFFSET	8
#define ICE_IPV6_NO_MAC_DST_ADDR_OFFSET	24
#define ICE_TCP6_NO_MAC_SRC_PORT_OFFSET	40
#define ICE_TCP6_NO_MAC_DST_PORT_OFFSET	42
#define ICE_UDP6_NO_MAC_SRC_PORT_OFFSET	40
#define ICE_UDP6_NO_MAC_DST_PORT_OFFSET	42
#define ICE_IPV4_GTPU_TEID_OFFSET	46
#define ICE_IPV4_GTPU_QFI_OFFSET	56
#define ICE_IPV6_GTPU_TEID_OFFSET	66
#define ICE_IPV6_GTPU_QFI_OFFSET	76
#define ICE_IPV4_GTPOGRE_TEID_OFFSET	70
#define ICE_IPV4_GTPOGRE_QFI_OFFSET	80
#define ICE_IPV6_GTPOGRE_TEID_OFFSET	90
#define ICE_IPV6_GTPOGRE_QFI_OFFSET	100
#define ICE_IPV4_L2TPV3_SESS_ID_OFFSET	34
#define ICE_IPV6_L2TPV3_SESS_ID_OFFSET	54
#define ICE_IPV4_ESP_SPI_OFFSET		34
#define ICE_IPV6_ESP_SPI_OFFSET		54
#define ICE_IPV4_AH_SPI_OFFSET		38
#define ICE_IPV6_AH_SPI_OFFSET		58
#define ICE_IPV4_NAT_T_ESP_SPI_OFFSET	42
#define ICE_IPV6_NAT_T_ESP_SPI_OFFSET	62
#define ICE_IPV4_VXLAN_VNI_OFFSET	46
#define ICE_ECPRI_TP0_PC_ID_OFFSET	18
#define ICE_IPV4_UDP_ECPRI_TP0_PC_ID_OFFSET			46

#define ICE_FDIR_MAX_FLTRS		16384

/* IPv4 has 2 flag bits that enable fragment processing: DF and MF. DF
 * requests that the packet not be fragmented. MF indicates that a packet has
 * been fragmented, except that for the last fragment has a non-zero
 * Fragment Offset field with zero MF.
 */
#define ICE_FDIR_IPV4_PKT_FLAG_MF		0x20
#define ICE_FDIR_IPV4_PKT_FLAG_MF_SHIFT	8
#define ICE_FDIR_IPV4_PKT_FLAG_DF		0x40

/* For IPv6 fragmented packets, all fragments except the last have
 * the MF flag set.
 */
#define ICE_FDIR_IPV6_PKT_FLAG_MF		0x100
#define ICE_FDIR_IPV6_PKT_FLAG_MF_SHIFT	8

enum ice_fltr_prgm_desc_dest {
	ICE_FLTR_PRGM_DESC_DEST_DROP_PKT,
	ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QINDEX,
	ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QGROUP,
	ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_OTHER,
};

enum ice_fltr_prgm_desc_fd_status {
	ICE_FLTR_PRGM_DESC_FD_STATUS_NONE,
	ICE_FLTR_PRGM_DESC_FD_STATUS_FD_ID,
	ICE_FLTR_PRGM_DESC_FD_STATUS_FD_ID_4FLEX_BYTES,
	ICE_FLTR_PRGM_DESC_FD_STATUS_8FLEX_BYTES,
};

/* Flow Director (FD) Filter Programming descriptor */
struct ice_fd_fltr_desc_ctx {
	u32 fdid;
	u16 qindex;
	u16 cnt_index;
	u16 fd_vsi;
	u16 flex_val;
	u8 comp_q;
	u8 comp_report;
	u8 fd_space;
	u8 cnt_ena;
	u8 evict_ena;
	u8 toq;
	u8 toq_prio;
	u8 dpu_recipe;
	u8 drop;
	u8 flex_prio;
	u8 flex_mdid;
	u8 dtype;
	u8 pcmd;
	u8 desc_prof_prio;
	u8 desc_prof;
	u8 swap;
	u8 fdid_prio;
	u8 fdid_mdid;
};

#define ICE_FLTR_PRGM_FLEX_WORD_SIZE	sizeof(__be16)

struct ice_rx_flow_userdef {
	u16 flex_word;
	u16 flex_offset;
	u16 flex_fltr;
};

struct ice_fdir_v4 {
	__be32 dst_ip;
	__be32 src_ip;
	__be16 dst_port;
	__be16 src_port;
	__be32 l4_header;
	__be32 sec_parm_idx;	/* security parameter index */
	u8 tos;
	u8 ip_ver;
	u8 proto;
	u8 ttl;
	__be16 packet_id;
};

#define ICE_IPV6_ADDR_LEN_AS_U32		4

struct ice_fdir_v6 {
	__be32 dst_ip[ICE_IPV6_ADDR_LEN_AS_U32];
	__be32 src_ip[ICE_IPV6_ADDR_LEN_AS_U32];
	__be16 dst_port;
	__be16 src_port;
	__be32 l4_header; /* next header */
	__be32 sec_parm_idx; /* security parameter index */
	u8 tc;
	u8 proto;
	u8 hlim;
	__be32 packet_id;
};

struct ice_fdir_udp_gtp {
	u8 flags;
	u8 msg_type;
	__be16 rsrvd_len;
	__be32 teid;
	__be16 rsrvd_seq_nbr;
	u8 rsrvd_n_pdu_nbr;
	u8 rsrvd_next_ext_type;
	u8 rsvrd_ext_len;
	u8	pdu_type:4,
		spare:4;
	u8	ppp:1,
		rqi:1,
		qfi:6;
	u32 rsvrd;
	u8 next_ext;
};

struct ice_fdir_l2tpv3 {
	__be32 session_id;
};

struct ice_fdir_udp_vxlan {
	__be32 vni; /* 8 bits reserved, always be zero */
};

struct ice_fdir_ecpri {
	__be16 pc_id;
};

struct ice_fdir_extra {
	u8 dst_mac[ETH_ALEN];	/* dest MAC address */
	u8 src_mac[ETH_ALEN];	/* src MAC address */
	__be16 ether_type;      /* for NON_IP_L2 */
	u32 usr_def[2];		/* user data */
	__be16 vlan_type;	/* VLAN ethertype */
	__be16 vlan_tag;	/* VLAN tag info */
};

struct ice_fdir_fltr {
	struct LIST_ENTRY_TYPE fltr_node;
	enum ice_fltr_ptype flow_type;

	union {
		struct ice_fdir_v4 v4;
		struct ice_fdir_v6 v6;
	} ip, mask;

	/* for tunnel outer part */
	union {
		struct ice_fdir_v4 v4;
		struct ice_fdir_v6 v6;
	} ip_outer, mask_outer;

	struct ice_fdir_extra ext_data_outer;
	struct ice_fdir_extra ext_mask_outer;

	struct ice_fdir_udp_vxlan vxlan_data;
	struct ice_fdir_udp_vxlan vxlan_mask;

	struct ice_fdir_udp_gtp gtpu_data;
	struct ice_fdir_udp_gtp gtpu_mask;

	struct ice_fdir_l2tpv3 l2tpv3_data;
	struct ice_fdir_l2tpv3 l2tpv3_mask;

	struct ice_fdir_ecpri ecpri_data;
	struct ice_fdir_ecpri ecpri_mask;

	struct ice_fdir_extra ext_data;
	struct ice_fdir_extra ext_mask;

	/* flex byte filter data */
	__be16 flex_word;
	/* queue region size (=2^q_region) */
	u8 q_region;
	u16 flex_offset;
	u16 flex_fltr;

	/* filter control */
	u16 q_index;
	u16 dest_vsi;
	u8 dest_ctl;
	u8 cnt_ena;
	u8 fltr_status;
	u16 cnt_index;
	u32 fltr_id;
	u8 fdid_prio;
	u8 comp_report;
	/* Set to true for an ACL filter */
	bool acl_fltr;
};

/* Dummy packet filter definition structure */
struct ice_fdir_base_pkt {
	enum ice_fltr_ptype flow;
	u16 pkt_len;
	const u8 *pkt;
	u16 tun_pkt_len;
	const u8 *tun_pkt;
};

enum ice_status ice_alloc_fd_res_cntr(struct ice_hw *hw, u16 *cntr_id);
enum ice_status ice_free_fd_res_cntr(struct ice_hw *hw, u16 cntr_id);
enum ice_status
ice_alloc_fd_guar_item(struct ice_hw *hw, u16 *cntr_id, u16 num_fltr);
enum ice_status
ice_free_fd_guar_item(struct ice_hw *hw, u16 cntr_id, u16 num_fltr);
enum ice_status
ice_alloc_fd_shrd_item(struct ice_hw *hw, u16 *cntr_id, u16 num_fltr);
enum ice_status
ice_free_fd_shrd_item(struct ice_hw *hw, u16 cntr_id, u16 num_fltr);
enum ice_status ice_clear_pf_fd_table(struct ice_hw *hw);
void
ice_fdir_get_prgm_desc(struct ice_hw *hw, struct ice_fdir_fltr *input,
		       struct ice_fltr_desc *fdesc, bool add);
enum ice_status
ice_fdir_get_gen_prgm_pkt(struct ice_hw *hw, struct ice_fdir_fltr *input,
			  u8 *pkt, bool frag, bool tun);
enum ice_status
ice_fdir_get_prgm_pkt(struct ice_fdir_fltr *input, u8 *pkt, bool frag);
int ice_get_fdir_cnt_all(struct ice_hw *hw);
bool ice_fdir_is_dup_fltr(struct ice_hw *hw, struct ice_fdir_fltr *input);
bool ice_fdir_has_frag(enum ice_fltr_ptype flow);
struct ice_fdir_fltr *
ice_fdir_find_fltr_by_idx(struct ice_hw *hw, u32 fltr_idx);
void
ice_fdir_update_cntrs(struct ice_hw *hw, enum ice_fltr_ptype flow,
		      bool acl_fltr, bool add);
void ice_fdir_list_add_fltr(struct ice_hw *hw, struct ice_fdir_fltr *input);
#endif /* _ICE_FDIR_H_ */
