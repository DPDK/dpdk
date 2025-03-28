/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_BDQ_IF_H_
#define _RNP_BDQ_IF_H_

#include "rnp_hw.h"

struct rnp_rx_queue;
struct rnp_tx_queue;
#pragma pack(push)
#pragma pack(1)
/* receive descriptor */
struct rnp_rx_desc {
	/* rx buffer descriptor */
	union {
		struct {
			u64 pkt_addr;
			u16 rsvd[3];
			u16 cmd;
		} d;
		struct {
			struct {
				u32 rss_hash;
				u32 mark_data;
			} qword0;
			struct {
				u32 lens;
				u16 vlan_tci;
				u16 cmd;
			} qword1;
		} wb;
	};
};
/* tx buffer descriptors (BD) */
struct rnp_tx_desc {
	union {
		struct {
			u64 addr;       /* pkt dma address */
			u16 blen;       /* pkt data Len */
			u16 mac_ip_len; /* mac ip header len */
			u16 vlan_tci;   /* vlan_tci */
			u16 cmd;        /* ctrl command */
		} d;
		struct {
			struct {
				u16 mss;        /* tso sz */
				u8 vf_num;      /* vf num */
				u8 l4_len;      /* l4 header size */
				u8 tunnel_len;  /* tunnel header size */
				u16 vlan_tci;   /* vlan_tci */
				u8 veb_tran;    /* mark pkt is transmit by veb */
			} qword0;
			struct {
				u16 rsvd[3];
				u16 cmd;        /* ctrl command */
			} qword1;
		} c;
	};
};
#pragma pack(pop)
/* common command */
#define RNP_CMD_EOP              RTE_BIT32(0) /* End Of Packet */
#define RNP_CMD_DD               RTE_BIT32(1)
#define RNP_CMD_RS               RTE_BIT32(2)
#define RNP_DESC_TYPE_S		(3)
#define RNP_DATA_DESC		(0x00UL << RNP_DESC_TYPE_S)
#define RNP_CTRL_DESC		(0x01UL << RNP_DESC_TYPE_S)
/* rx data cmd */
#define RNP_RX_PTYPE_PTP	RTE_BIT32(4)
#define RNP_RX_L3TYPE_S		(5)
#define RNP_RX_L3TYPE_IPV4	(0x00UL << RNP_RX_L3TYPE_S)
#define RNP_RX_L3TYPE_IPV6	(0x01UL << RNP_RX_L3TYPE_S)
#define RNP_RX_L4TYPE_S		(6)
#define RNP_RX_L4TYPE_MASK	RTE_GENMASK32(7, 6)
#define RNP_RX_L4TYPE_TCP	(0x01UL << RNP_RX_L4TYPE_S)
#define RNP_RX_L4TYPE_SCTP	(0x02UL << RNP_RX_L4TYPE_S)
#define RNP_RX_L4TYPE_UDP	(0x03UL << RNP_RX_L4TYPE_S)
#define RNP_RX_ERR_MASK		RTE_GENMASK32(12, 8)
#define RNP_RX_L3_ERR		RTE_BIT32(8)
#define RNP_RX_L4_ERR		RTE_BIT32(9)
#define RNP_RX_SCTP_ERR		RTE_BIT32(10)
#define RNP_RX_IN_L3_ERR	RTE_BIT32(11)
#define RNP_RX_IN_L4_ERR	RTE_BIT32(12)
#define RNP_RX_TUNNEL_TYPE_S	(13)
#define RNP_RX_TUNNEL_MASK	RTE_GENMASK32(14, 13)
#define RNP_RX_PTYPE_VXLAN	(0x01UL << RNP_RX_TUNNEL_TYPE_S)
#define RNP_RX_PTYPE_NVGRE	(0x02UL << RNP_RX_TUNNEL_TYPE_S)
#define RNP_RX_STRIP_VLAN	RTE_BIT32(15)
/* mark_data */
#define RNP_RX_L3TYPE_VALID	RTE_BIT32(31)
/* tx data cmd */
#define RNP_TX_TSO_EN		RTE_BIT32(4)
#define RNP_TX_L3TYPE_S		(5)
#define RNP_TX_L3TYPE_IPV6	(0x01UL << RNP_TX_L3TYPE_S)
#define RNP_TX_L3TYPE_IPV4	(0x00UL << RNP_TX_L3TYPE_S)
#define RNP_TX_L4TYPE_S		(6)
#define RNP_TX_L4TYPE_TCP	(0x01UL << RNP_TX_L4TYPE_S)
#define RNP_TX_L4TYPE_SCTP	(0x02UL << RNP_TX_L4TYPE_S)
#define RNP_TX_L4TYPE_UDP	(0x03UL << RNP_TX_L4TYPE_S)
#define RNP_TX_TUNNEL_TYPE_S	(8)
#define RNP_TX_VXLAN_TUNNEL	(0x01UL << RNP_TX_TUNNEL_TYPE_S)
#define RNP_TX_NVGRE_TUNNEL	(0x02UL << RNP_TX_TUNNEL_TYPE_S)
#define RNP_TX_PTP_EN		RTE_BIT32(10)
#define RNP_TX_IP_CKSUM_EN	RTE_BIT32(11)
#define RNP_TX_L4CKSUM_EN	RTE_BIT32(12)
#define RNP_TX_VLAN_CTRL_S	(13)
#define RNP_TX_VLAN_STRIP	(0x01UL << RNP_TX_VLAN_CTRL_S)
#define RNP_TX_VLAN_INSERT	(0x02UL << RNP_TX_VLAN_CTRL_S)
#define RNP_TX_VLAN_VALID	RTE_BIT32(15)
/* tx data mac_ip len */
#define RNP_TX_MAC_LEN_S	(9)
#define RNP_TX_MAC_LEN_MASK	RTE_GENMASK32(15, 9)
/* tx ctrl cmd */
#define RNP_TX_LEN_PAD_S	(8)
#define RNP_TX_OFF_MAC_PAD	(0x01UL << RNP_TX_LEN_PAD_S)
#define RNP_TX_QINQ_CTRL_S	(10)
#define RNP_TX_QINQ_INSERT	(0x02UL << RNP_TX_QINQ_CTRL_S)
#define RNP_TX_QINQ_STRIP	(0x01UL << RNP_TX_QINQ_CTRL_S)
#define RNP_TX_TO_NPU_EN	RTE_BIT32(15)
/* descript op end */
struct rnp_rxq_reset_res {
	u64 rx_pkt_addr;
	u64 tx_pkt_addr;
	u8 *eth_hdr;
};
struct rnp_veb_cfg {
	uint32_t mac_hi;
	uint32_t mac_lo;
	uint32_t vid;
	uint16_t vf_id;
	uint16_t ring;
};
void
rnp_rxq_flow_enable(struct rnp_hw *hw,
		    u16 hw_idx);
void
rnp_rxq_flow_disable(struct rnp_hw *hw,
		     u16 hw_idx);
void
rnp_reset_hw_rxq_op(struct rnp_hw *hw,
		    struct rnp_rx_queue *rxq,
		    struct rnp_tx_queue *txq,
		    struct rnp_rxq_reset_res *res);
void rnp_reset_hw_txq_op(struct rnp_hw *hw,
			 struct rnp_tx_queue *txq);
void rnp_setup_rxbdr(struct rnp_hw *hw,
		     struct rnp_rx_queue *rxq);
void rnp_setup_txbdr(struct rnp_hw *hw,
		     struct rnp_tx_queue *txq);
int rnp_get_dma_ring_index(struct rnp_eth_port *port, u16 queue_idx);

#endif /* _RNP_BDQ_IF_H_ */
