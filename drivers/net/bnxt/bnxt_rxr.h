/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_RXR_H_
#define _BNXT_RXR_H_

#define B_RX_DB(db, prod)						\
		(*(uint32_t *)db = (DB_KEY_RX | prod))

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

#define RX_CMP_L4_CS_BITS	rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_L4_CS_CALC)

#define RX_CMP_L4_CS_ERR_BITS	rte_cpu_to_le_32(RX_PKT_CMPL_ERRORS_L4_CS_ERROR)

#define RX_CMP_L4_CS_OK(rxcmp1)						\
	    (((rxcmp1)->flags2 & RX_CMP_L4_CS_BITS) &&		\
	     !((rxcmp1)->errors_v2 & RX_CMP_L4_CS_ERR_BITS))

#define RX_CMP_IP_CS_ERR_BITS	rte_cpu_to_le_32(RX_PKT_CMPL_ERRORS_IP_CS_ERROR)

#define RX_CMP_IP_CS_BITS	rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_CS_CALC)

#define RX_CMP_IP_CS_OK(rxcmp1)						\
		(((rxcmp1)->flags2 & RX_CMP_IP_CS_BITS) &&	\
		!((rxcmp1)->errors_v2 & RX_CMP_IP_CS_ERR_BITS))

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
	void			*rx_doorbell;
	void			*ag_doorbell;

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
#endif
