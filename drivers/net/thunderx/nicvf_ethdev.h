/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __THUNDERX_NICVF_ETHDEV_H__
#define __THUNDERX_NICVF_ETHDEV_H__

#include <rte_ethdev_driver.h>

#define THUNDERX_NICVF_PMD_VERSION      "2.0"
#define THUNDERX_REG_BYTES		8

#define NICVF_INTR_POLL_INTERVAL_MS	50
#define NICVF_HALF_DUPLEX		0x00
#define NICVF_FULL_DUPLEX		0x01
#define NICVF_UNKNOWN_DUPLEX		0xff

#define NICVF_RSS_OFFLOAD_PASS1 ( \
	ETH_RSS_PORT | \
	ETH_RSS_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP)

#define NICVF_RSS_OFFLOAD_TUNNEL ( \
	ETH_RSS_VXLAN | \
	ETH_RSS_GENEVE | \
	ETH_RSS_NVGRE)

#define NICVF_TX_OFFLOAD_CAPA ( \
	DEV_TX_OFFLOAD_IPV4_CKSUM       | \
	DEV_TX_OFFLOAD_UDP_CKSUM        | \
	DEV_TX_OFFLOAD_TCP_CKSUM        | \
	DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM | \
	DEV_TX_OFFLOAD_MBUF_FAST_FREE   | \
	DEV_TX_OFFLOAD_MULTI_SEGS)

#define NICVF_RX_OFFLOAD_CAPA ( \
	DEV_RX_OFFLOAD_VLAN_STRIP  | \
	DEV_RX_OFFLOAD_CRC_STRIP   | \
	DEV_RX_OFFLOAD_JUMBO_FRAME | \
	DEV_RX_OFFLOAD_SCATTER)

#define NICVF_DEFAULT_RX_FREE_THRESH    224
#define NICVF_DEFAULT_TX_FREE_THRESH    224
#define NICVF_TX_FREE_MPOOL_THRESH      16
#define NICVF_MAX_RX_FREE_THRESH        1024
#define NICVF_MAX_TX_FREE_THRESH        1024

#define VLAN_TAG_SIZE                   4	/* 802.3ac tag */

static inline struct nicvf *
nicvf_pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

static inline uint64_t
nicvf_mempool_phy_offset(struct rte_mempool *mp)
{
	struct rte_mempool_memhdr *hdr;

	hdr = STAILQ_FIRST(&mp->mem_list);
	assert(hdr != NULL);
	return (uint64_t)((uintptr_t)hdr->addr - hdr->iova);
}

static inline uint16_t
nicvf_mbuff_meta_length(struct rte_mbuf *mbuf)
{
	return (uint16_t)((uintptr_t)mbuf->buf_addr - (uintptr_t)mbuf);
}

static inline uint16_t
nicvf_netdev_qidx(struct nicvf *nic, uint8_t local_qidx)
{
	uint16_t global_qidx = local_qidx;

	if (nic->sqs_mode)
		global_qidx += ((nic->sqs_id + 1) * MAX_CMP_QUEUES_PER_QS);

	return global_qidx;
}

/*
 * Simple phy2virt functions assuming mbufs are in a single huge page
 * V = P + offset
 * P = V - offset
 */
static inline uintptr_t
nicvf_mbuff_phy2virt(rte_iova_t phy, uint64_t mbuf_phys_off)
{
	return (uintptr_t)(phy + mbuf_phys_off);
}

static inline uintptr_t
nicvf_mbuff_virt2phy(uintptr_t virt, uint64_t mbuf_phys_off)
{
	return (rte_iova_t)(virt - mbuf_phys_off);
}

static inline void
nicvf_tx_range(struct rte_eth_dev *dev, struct nicvf *nic, uint16_t *tx_start,
	       uint16_t *tx_end)
{
	uint16_t tmp;

	*tx_start = RTE_ALIGN_FLOOR(nicvf_netdev_qidx(nic, 0),
				    MAX_SND_QUEUES_PER_QS);
	tmp = RTE_ALIGN_CEIL(nicvf_netdev_qidx(nic, 0) + 1,
			     MAX_SND_QUEUES_PER_QS) - 1;
	*tx_end = dev->data->nb_tx_queues ?
		RTE_MIN(tmp, dev->data->nb_tx_queues - 1) : 0;
}

static inline void
nicvf_rx_range(struct rte_eth_dev *dev, struct nicvf *nic, uint16_t *rx_start,
	       uint16_t *rx_end)
{
	uint16_t tmp;

	*rx_start = RTE_ALIGN_FLOOR(nicvf_netdev_qidx(nic, 0),
				    MAX_RCV_QUEUES_PER_QS);
	tmp = RTE_ALIGN_CEIL(nicvf_netdev_qidx(nic, 0) + 1,
			     MAX_RCV_QUEUES_PER_QS) - 1;
	*rx_end = dev->data->nb_rx_queues ?
		RTE_MIN(tmp, dev->data->nb_rx_queues - 1) : 0;
}

#endif /* __THUNDERX_NICVF_ETHDEV_H__  */
