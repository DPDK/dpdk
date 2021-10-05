/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_FILTER_H_
#define _BNXT_FILTER_H_

#include <rte_ether.h>

#define bnxt_vlan_filter_exists(bp, filter, chk, vlan_id)	\
		(((filter)->enables & (chk)) &&			\
		 ((filter)->l2_ivlan == (vlan_id) &&		\
		  (filter)->l2_ivlan_mask == 0x0FFF) &&		\
		 !memcmp((filter)->l2_addr, (bp)->mac_addr,	\
			 RTE_ETHER_ADDR_LEN))
struct bnxt;

#define BNXT_FLOW_L2_VALID_FLAG			BIT(0)
#define BNXT_FLOW_L2_SRC_VALID_FLAG		BIT(1)
#define BNXT_FLOW_L2_INNER_SRC_VALID_FLAG	BIT(2)
#define BNXT_FLOW_L2_DST_VALID_FLAG		BIT(3)
#define BNXT_FLOW_L2_INNER_DST_VALID_FLAG	BIT(4)
#define BNXT_FLOW_L2_DROP_FLAG			BIT(5)
#define BNXT_FLOW_PARSE_INNER_FLAG		BIT(6)
#define BNXT_FLOW_MARK_FLAG			BIT(7)

struct bnxt_flow_stats {
	uint64_t	packets;
	uint64_t	bytes;
};

struct bnxt_filter_info {
	STAILQ_ENTRY(bnxt_filter_info)	next;
	uint32_t		flow_id;
	uint64_t		fw_l2_filter_id;
	struct bnxt_filter_info *matching_l2_fltr_ptr;
	uint64_t		fw_em_filter_id;
	uint64_t		fw_ntuple_filter_id;
#define INVALID_MAC_INDEX	((uint16_t)-1)
	uint16_t		mac_index;
#define HWRM_CFA_L2_FILTER	0
#define HWRM_CFA_EM_FILTER	1
#define HWRM_CFA_NTUPLE_FILTER	2
#define HWRM_CFA_TUNNEL_REDIRECT_FILTER	3
#define HWRM_CFA_CONFIG		4
	uint8_t                 filter_type;
	uint32_t                dst_id;

	/* Filter Characteristics */
	uint32_t		flags;
	uint32_t		enables;
	uint32_t		l2_ref_cnt;
	uint8_t			l2_addr[RTE_ETHER_ADDR_LEN];
	uint8_t			l2_addr_mask[RTE_ETHER_ADDR_LEN];
	uint32_t		valid_flags;
	uint16_t		l2_ovlan;
	uint16_t		l2_ovlan_mask;
	uint16_t		l2_ivlan;
	uint16_t		l2_ivlan_mask;
	uint8_t			t_l2_addr[RTE_ETHER_ADDR_LEN];
	uint8_t			t_l2_addr_mask[RTE_ETHER_ADDR_LEN];
	uint16_t		t_l2_ovlan;
	uint16_t		t_l2_ovlan_mask;
	uint16_t		t_l2_ivlan;
	uint16_t		t_l2_ivlan_mask;
	uint8_t			tunnel_type;
	uint16_t		mirror_vnic_id;
	uint32_t		vni;
	uint8_t			pri_hint;
	uint64_t		l2_filter_id_hint;
	uint32_t		src_id;
	uint8_t			src_type;
	uint8_t                 src_macaddr[6];
	uint8_t                 dst_macaddr[6];
	uint32_t                dst_ipaddr[4];
	uint32_t                dst_ipaddr_mask[4];
	uint32_t                src_ipaddr[4];
	uint32_t                src_ipaddr_mask[4];
	uint16_t                dst_port;
	uint16_t                dst_port_mask;
	uint16_t                src_port;
	uint16_t                src_port_mask;
	uint16_t                ip_protocol;
	uint16_t                ip_addr_type;
	uint16_t                ethertype;
	uint32_t		priority;
	/* Backptr to vnic. As of now, used only by an L2 filter
	 * to remember which vnic it was created on
	 */
	struct			bnxt_vnic_info *vnic;
	uint32_t		mark;
	struct bnxt_flow_stats	hw_stats;
};

struct bnxt_filter_info *bnxt_alloc_filter(struct bnxt *bp);
struct bnxt_filter_info *bnxt_alloc_vf_filter(struct bnxt *bp, uint16_t vf);
void bnxt_free_all_filters(struct bnxt *bp);
void bnxt_free_filter_mem(struct bnxt *bp);
int bnxt_alloc_filter_mem(struct bnxt *bp);
struct bnxt_filter_info *bnxt_get_unused_filter(struct bnxt *bp);
void bnxt_free_filter(struct bnxt *bp, struct bnxt_filter_info *filter);
struct bnxt_filter_info *bnxt_get_l2_filter(struct bnxt *bp,
		struct bnxt_filter_info *nf, struct bnxt_vnic_info *vnic);

#define NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_MACADDR	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_MACADDR
#define EM_FLOW_ALLOC_INPUT_EN_SRC_MACADDR	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_MACADDR
#define NTUPLE_FLTR_ALLOC_INPUT_EN_DST_MACADDR	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_MACADDR
#define EM_FLOW_ALLOC_INPUT_EN_DST_MACADDR	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_MACADDR
#define NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE   \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_ETHERTYPE
#define EM_FLOW_ALLOC_INPUT_EN_ETHERTYPE       \
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_ETHERTYPE
#define EM_FLOW_ALLOC_INPUT_EN_OVLAN_VID       \
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_OVLAN_VID
#define NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR  \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_IPADDR
#define NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK     \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_IPADDR_MASK
#define NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR  \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_IPADDR
#define NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK     \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_IPADDR_MASK
#define NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT    \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_PORT
#define NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK       \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_PORT_MASK
#define NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT    \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_PORT
#define NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK       \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_PORT_MASK
#define NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_IP_PROTOCOL
#define EM_FLOW_ALLOC_INPUT_EN_SRC_IPADDR	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_IPADDR
#define EM_FLOW_ALLOC_INPUT_EN_DST_IPADDR	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_IPADDR
#define EM_FLOW_ALLOC_INPUT_EN_SRC_PORT	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_PORT
#define EM_FLOW_ALLOC_INPUT_EN_DST_PORT	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_PORT
#define EM_FLOW_ALLOC_INPUT_EN_IP_PROTO	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_IP_PROTOCOL
#define EM_FLOW_ALLOC_INPUT_IP_ADDR_TYPE_IPV6	\
	HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_ADDR_TYPE_IPV6
#define NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV6	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_ADDR_TYPE_IPV6
#define CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_VXLAN	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_VXLAN
#define CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_NVGRE	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NVGRE
#define CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_IPGRE  \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPGRE
#define L2_FILTER_ALLOC_INPUT_EN_L2_ADDR_MASK	\
	HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK
#define NTUPLE_FLTR_ALLOC_INPUT_IP_PROTOCOL_UDP	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_PROTOCOL_UDP
#define NTUPLE_FLTR_ALLOC_INPUT_IP_PROTOCOL_TCP	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_PROTOCOL_TCP
#define NTUPLE_FLTR_ALLOC_INPUT_IP_PROTOCOL_UNKNOWN	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_PROTOCOL_UNKNOWN
#define NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV4	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_ADDR_TYPE_IPV4
#define NTUPLE_FLTR_ALLOC_INPUT_EN_MIRROR_VNIC_ID	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID
#define NTUPLE_FLTR_ALLOC_INPUT_EN_MIRROR_VNIC_ID	\
	HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID
#define L2_FILTER_ALLOC_INPUT_EN_T_NUM_VLANS \
	HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_NUM_VLANS
#define L2_FILTER_ALLOC_INPUT_EN_NUM_VLANS \
	HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_NUM_VLANS
#endif
