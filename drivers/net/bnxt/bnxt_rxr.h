/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_RXR_H_
#define _BNXT_RXR_H_

#define B_RX_DB(db, prod)						\
		(*(uint32_t *)db = (DB_KEY_RX | (prod)))

#define BNXT_TPA_L4_SIZE(x)	\
	{ \
		typeof(x) hdr_info = (x); \
		(((hdr_info) & 0xf8000000) ? ((hdr_info) >> 27) : 32) \
	}

#define BNXT_TPA_INNER_L3_OFF(hdr_info)	\
	(((hdr_info) >> 18) & 0x1ff)

#define BNXT_TPA_INNER_L2_OFF(hdr_info)	\
	(((hdr_info) >> 9) & 0x1ff)

#define BNXT_TPA_OUTER_L3_OFF(hdr_info)	\
	((hdr_info) & 0x1ff)

#define flags2_0xf(rxcmp1)	\
	(((rxcmp1)->flags2) & 0xf)

/* IP non tunnel can be with or without L4-
 * Ether / (vlan) / IP|IP6 / UDP|TCP|SCTP Or
 * Ether / (vlan) / outer IP|IP6 / ICMP
 * we use '==' instead of '&' because tunnel pkts have all 4 fields set.
 */
#define IS_IP_NONTUNNEL_PKT(flags2_f)	\
	(	\
	 ((flags2_f) == \
	  (rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_CS_CALC))) ||	\
	 ((flags2_f) ==	\
	  (rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_CS_CALC | \
			    RX_PKT_CMPL_FLAGS2_L4_CS_CALC))) \
	)

/* IP Tunnel pkt must have atleast tunnel-IP-calc set.
 * again tunnel ie outer L4 is optional bcoz of
 * Ether / (vlan) / outer IP|IP6 / GRE / Ether / IP|IP6 / UDP|TCP|SCTP
 * Ether / (vlan) / outer IP|IP6 / outer UDP / VxLAN / Ether / IP|IP6 /
 *           UDP|TCP|SCTP
 * Ether / (vlan) / outer IP|IP6 / outer UDP / VXLAN-GPE / Ether / IP|IP6 /
 *           UDP|TCP|SCTP
 * Ether / (vlan) / outer IP|IP6 / outer UDP / VXLAN-GPE / IP|IP6 /
 *           UDP|TCP|SCTP
 * Ether / (vlan) / outer IP|IP6 / GRE / IP|IP6 / UDP|TCP|SCTP
 * Ether / (vlan) / outer IP|IP6 / IP|IP6 / UDP|TCP|SCTP
 * also inner L3 chksum error is not taken into consideration by DPDK.
 */
#define IS_IP_TUNNEL_PKT(flags2_f)	\
	((flags2_f) & rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC))

/* RX_PKT_CMPL_ERRORS_IP_CS_ERROR only for Non-tunnel pkts.
 * For tunnel pkts RX_PKT_CMPL_ERRORS_IP_CS_ERROR is not accounted and treated
 * as good csum pkt.
 */
#define RX_CMP_IP_CS_ERROR(rxcmp1)	\
	((rxcmp1)->errors_v2 &	\
	 rte_cpu_to_le_32(RX_PKT_CMPL_ERRORS_IP_CS_ERROR))

#define RX_CMP_IP_OUTER_CS_ERROR(rxcmp1)	\
	((rxcmp1)->errors_v2 &	\
	 rte_cpu_to_le_32(RX_PKT_CMPL_ERRORS_T_IP_CS_ERROR))

#define RX_CMP_IP_CS_BITS	\
	rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_CS_CALC | \
			 RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC)

#define RX_CMP_IP_CS_UNKNOWN(rxcmp1)	\
		!((rxcmp1)->flags2 & RX_CMP_IP_CS_BITS)

/* L4 non tunnel pkt-
 * Ether / (vlan) / IP6 / UDP|TCP|SCTP
 */
#define IS_L4_NONTUNNEL_PKT(flags2_f)	\
	( \
	  ((flags2_f) == \
	   (rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_CS_CALC |	\
			     RX_PKT_CMPL_FLAGS2_L4_CS_CALC))))

/* L4 tunnel pkt-
 * Outer L4 is not mandatory. Eg: GRE-
 * Ether / (vlan) / outer IP|IP6 / GRE / Ether / IP|IP6 / UDP|TCP|SCTP
 * Ether / (vlan) / outer IP|IP6 / outer UDP / VxLAN / Ether / IP|IP6 /
 *           UDP|TCP|SCTP
 */
#define	IS_L4_TUNNEL_PKT_INNER_OUTER_L4_CS(flags2_f)	\
	 ((flags2_f) == \
	  (rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_CS_CALC |	\
			    RX_PKT_CMPL_FLAGS2_L4_CS_CALC |	\
			    RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC |	\
			    RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC)))

#define IS_L4_TUNNEL_PKT_ONLY_INNER_L4_CS(flags2_f)	\
	 ((flags2_f) == \
	  (rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_CS_CALC |	\
			    RX_PKT_CMPL_FLAGS2_L4_CS_CALC |	\
			    RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC)))

#define IS_L4_TUNNEL_PKT(flags2_f)	\
	(	\
		IS_L4_TUNNEL_PKT_INNER_OUTER_L4_CS(flags2_f) || \
		IS_L4_TUNNEL_PKT_ONLY_INNER_L4_CS(flags2_f)	\
	)

#define RX_CMP_L4_CS_BITS	\
	rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_L4_CS_CALC)

#define RX_CMP_L4_CS_UNKNOWN(rxcmp1)					\
	    !((rxcmp1)->flags2 & RX_CMP_L4_CS_BITS)

#define RX_CMP_T_L4_CS_BITS	\
	rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC)

#define RX_CMP_T_L4_CS_UNKNOWN(rxcmp1)					\
	    !((rxcmp1)->flags2 & RX_CMP_T_L4_CS_BITS)

/* Outer L4 chksum error
 */
#define RX_CMP_L4_OUTER_CS_ERR2(rxcmp1)	\
	 ((rxcmp1)->errors_v2 & \
	  rte_cpu_to_le_32(RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR))

/* Inner L4 chksum error
 */
#define RX_CMP_L4_INNER_CS_ERR2(rxcmp1)	\
	 ((rxcmp1)->errors_v2 & \
	  rte_cpu_to_le_32(RX_PKT_CMPL_ERRORS_L4_CS_ERROR))

#define BNXT_RX_POST_THRESH	32

enum pkt_hash_types {
	PKT_HASH_TYPE_NONE,	/* Undefined type */
	PKT_HASH_TYPE_L2,	/* Input: src_MAC, dest_MAC */
	PKT_HASH_TYPE_L3,	/* Input: src_IP, dst_IP */
	PKT_HASH_TYPE_L4,	/* Input: src_IP, dst_IP, src_port, dst_port */
};

struct bnxt_tpa_info {
	struct rte_mbuf		*mbuf;
	uint16_t			len;
	unsigned short		gso_type;
	uint32_t			flags2;
	uint32_t			metadata;
	enum pkt_hash_types	hash_type;
	uint32_t			rss_hash;
	uint32_t			hdr_info;
};

struct bnxt_sw_rx_bd {
	struct rte_mbuf		*mbuf; /* data associated with RX descriptor */
};

struct bnxt_rx_ring_info {
	uint16_t		rx_prod;
	uint16_t		ag_prod;
	struct bnxt_db_info     rx_db;
	struct bnxt_db_info     ag_db;

	struct rx_prod_pkt_bd	*rx_desc_ring;
	struct rx_prod_pkt_bd	*ag_desc_ring;
	struct bnxt_sw_rx_bd	*rx_buf_ring; /* sw ring */
	struct bnxt_sw_rx_bd	*ag_buf_ring; /* sw ring */

	rte_iova_t		rx_desc_mapping;
	rte_iova_t		ag_desc_mapping;

	struct bnxt_ring	*rx_ring_struct;
	struct bnxt_ring	*ag_ring_struct;

	/*
	 * To deal with out of order return from TPA, use free buffer indicator
	 */
	struct rte_bitmap	*ag_bitmap;

	struct bnxt_tpa_info *tpa_info;
};

uint16_t bnxt_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts);
void bnxt_free_rx_rings(struct bnxt *bp);
int bnxt_init_rx_ring_struct(struct bnxt_rx_queue *rxq, unsigned int socket_id);
int bnxt_init_one_rx_ring(struct bnxt_rx_queue *rxq);
int bnxt_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int bnxt_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

#ifdef RTE_ARCH_X86
uint16_t bnxt_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts);
int bnxt_rxq_vec_setup(struct bnxt_rx_queue *rxq);
#endif

#endif
