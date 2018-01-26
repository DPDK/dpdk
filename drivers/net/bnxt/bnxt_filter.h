/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BNXT_FILTER_H_
#define _BNXT_FILTER_H_

#include <rte_ether.h>

struct bnxt;
struct bnxt_filter_info {
	STAILQ_ENTRY(bnxt_filter_info)	next;
	uint64_t		fw_l2_filter_id;
	uint64_t		fw_em_filter_id;
	uint64_t		fw_ntuple_filter_id;
#define INVALID_MAC_INDEX	((uint16_t)-1)
	uint16_t		mac_index;
#define HWRM_CFA_L2_FILTER	0
#define HWRM_CFA_EM_FILTER	1
#define HWRM_CFA_NTUPLE_FILTER	2
	uint8_t                 filter_type;    //L2 or EM or NTUPLE filter
	uint32_t                dst_id;

	/* Filter Characteristics */
	uint32_t		flags;
	uint32_t		enables;
	uint8_t			l2_addr[ETHER_ADDR_LEN];
	uint8_t			l2_addr_mask[ETHER_ADDR_LEN];
	uint16_t		l2_ovlan;
	uint16_t		l2_ovlan_mask;
	uint16_t		l2_ivlan;
	uint16_t		l2_ivlan_mask;
	uint8_t			t_l2_addr[ETHER_ADDR_LEN];
	uint8_t			t_l2_addr_mask[ETHER_ADDR_LEN];
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
};

struct bnxt_filter_info *bnxt_alloc_filter(struct bnxt *bp);
struct bnxt_filter_info *bnxt_alloc_vf_filter(struct bnxt *bp, uint16_t vf);
void bnxt_init_filters(struct bnxt *bp);
void bnxt_free_all_filters(struct bnxt *bp);
void bnxt_free_filter_mem(struct bnxt *bp);
int bnxt_alloc_filter_mem(struct bnxt *bp);
struct bnxt_filter_info *bnxt_get_unused_filter(struct bnxt *bp);
void bnxt_free_filter(struct bnxt *bp, struct bnxt_filter_info *filter);
struct bnxt_filter_info *bnxt_get_l2_filter(struct bnxt *bp,
		struct bnxt_filter_info *nf, struct bnxt_vnic_info *vnic);
int check_zero_bytes(const uint8_t *bytes, int len);

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
#endif
