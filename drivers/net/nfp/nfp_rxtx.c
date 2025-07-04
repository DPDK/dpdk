/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

#include "nfp_rxtx.h"

#include <ethdev_pci.h>
#include <rte_security.h>

#include "nfd3/nfp_nfd3.h"
#include "nfdk/nfp_nfdk.h"
#include "nfdk/nfp_nfdk_vec.h"
#include "flower/nfp_flower.h"

#include "nfp_ipsec.h"
#include "nfp_logs.h"
#include "nfp_net_meta.h"
#include "nfp_rxtx_vec.h"

/*
 * The bit format and map of nfp packet type for rxd.offload_info in Rx descriptor.
 *
 * Bit format about nfp packet type refers to the following:
 * ---------------------------------
 *            1                   0
 *  5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |       |ol3|tunnel |  l3 |  l4 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Bit map about nfp packet type refers to the following:
 *
 * L4: bit 0~2, used for layer 4 or inner layer 4.
 * 000: NFP_NET_PTYPE_L4_NONE
 * 001: NFP_NET_PTYPE_L4_TCP
 * 010: NFP_NET_PTYPE_L4_UDP
 * 011: NFP_NET_PTYPE_L4_FRAG
 * 100: NFP_NET_PTYPE_L4_NONFRAG
 * 101: NFP_NET_PTYPE_L4_ICMP
 * 110: NFP_NET_PTYPE_L4_SCTP
 * 111: reserved
 *
 * L3: bit 3~5, used for layer 3 or inner layer 3.
 * 000: NFP_NET_PTYPE_L3_NONE
 * 001: NFP_NET_PTYPE_L3_IPV6
 * 010: NFP_NET_PTYPE_L3_IPV4
 * 011: NFP_NET_PTYPE_L3_IPV4_EXT
 * 100: NFP_NET_PTYPE_L3_IPV6_EXT
 * 101: NFP_NET_PTYPE_L3_IPV4_EXT_UNKNOWN
 * 110: NFP_NET_PTYPE_L3_IPV6_EXT_UNKNOWN
 * 111: reserved
 *
 * Tunnel: bit 6~9, used for tunnel.
 * 0000: NFP_NET_PTYPE_TUNNEL_NONE
 * 0001: NFP_NET_PTYPE_TUNNEL_VXLAN
 * 0100: NFP_NET_PTYPE_TUNNEL_NVGRE
 * 0101: NFP_NET_PTYPE_TUNNEL_GENEVE
 * 0010, 0011, 0110~1111: reserved
 *
 * Outer L3: bit 10~11, used for outer layer 3.
 * 00: NFP_NET_PTYPE_OUTER_L3_NONE
 * 01: NFP_NET_PTYPE_OUTER_L3_IPV6
 * 10: NFP_NET_PTYPE_OUTER_L3_IPV4
 * 11: reserved
 *
 * Reserved: bit 10~15, used for extension.
 */

/* Mask and offset about nfp packet type based on the bit map above. */
#define NFP_NET_PTYPE_L4_MASK                  0x0007
#define NFP_NET_PTYPE_L3_MASK                  0x0038
#define NFP_NET_PTYPE_TUNNEL_MASK              0x03c0
#define NFP_NET_PTYPE_OUTER_L3_MASK            0x0c00

#define NFP_NET_PTYPE_L4_OFFSET                0
#define NFP_NET_PTYPE_L3_OFFSET                3
#define NFP_NET_PTYPE_TUNNEL_OFFSET            6
#define NFP_NET_PTYPE_OUTER_L3_OFFSET          10

/* Case about nfp packet type based on the bit map above. */
#define NFP_NET_PTYPE_L4_NONE                  0
#define NFP_NET_PTYPE_L4_TCP                   1
#define NFP_NET_PTYPE_L4_UDP                   2
#define NFP_NET_PTYPE_L4_FRAG                  3
#define NFP_NET_PTYPE_L4_NONFRAG               4
#define NFP_NET_PTYPE_L4_ICMP                  5
#define NFP_NET_PTYPE_L4_SCTP                  6

#define NFP_NET_PTYPE_L3_NONE                  0
#define NFP_NET_PTYPE_L3_IPV6                  1
#define NFP_NET_PTYPE_L3_IPV4                  2
#define NFP_NET_PTYPE_L3_IPV4_EXT              3
#define NFP_NET_PTYPE_L3_IPV6_EXT              4
#define NFP_NET_PTYPE_L3_IPV4_EXT_UNKNOWN      5
#define NFP_NET_PTYPE_L3_IPV6_EXT_UNKNOWN      6

#define NFP_NET_PTYPE_TUNNEL_NONE              0
#define NFP_NET_PTYPE_TUNNEL_VXLAN             1
#define NFP_NET_PTYPE_TUNNEL_NVGRE             4
#define NFP_NET_PTYPE_TUNNEL_GENEVE            5

#define NFP_NET_PTYPE_OUTER_L3_NONE            0
#define NFP_NET_PTYPE_OUTER_L3_IPV6            1
#define NFP_NET_PTYPE_OUTER_L3_IPV4            2

#define NFP_PTYPE2RTE(tunnel, type) ((tunnel) ? RTE_PTYPE_INNER_##type : RTE_PTYPE_##type)

/* Record NFP packet type parsed from rxd.offload_info. */
struct nfp_ptype_parsed {
	uint8_t l4_ptype;       /**< Packet type of layer 4, or inner layer 4. */
	uint8_t l3_ptype;       /**< Packet type of layer 3, or inner layer 3. */
	uint8_t tunnel_ptype;   /**< Packet type of tunnel. */
	uint8_t outer_l3_ptype; /**< Packet type of outer layer 3. */
};

/* Set mbuf checksum flags based on RX descriptor flags */
void
nfp_net_rx_cksum(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxd,
		struct rte_mbuf *mb)
{
	uint16_t flags;
	struct nfp_net_hw *hw = rxq->hw;

	if ((hw->super.ctrl & NFP_NET_CFG_CTRL_RXCSUM) == 0)
		return;

	flags = rte_le_to_cpu_16(rxd->rxd.flags);

	/* If IPv4 and IP checksum error, fail */
	if (unlikely((flags & PCIE_DESC_RX_IP4_CSUM) != 0 &&
			(flags & PCIE_DESC_RX_IP4_CSUM_OK) == 0))
		mb->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
	else
		mb->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

	/* If neither UDP nor TCP return */
	if ((flags & PCIE_DESC_RX_TCP_CSUM) == 0 &&
			(flags & PCIE_DESC_RX_UDP_CSUM) == 0)
		return;

	if (likely(flags & PCIE_DESC_RX_L4_CSUM_OK) != 0)
		mb->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	else
		mb->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
}

static int
nfp_net_rx_fill_freelist(struct nfp_net_rxq *rxq)
{
	uint16_t i;
	uint64_t dma_addr;
	struct nfp_net_dp_buf *rxe = rxq->rxbufs;

	PMD_RX_LOG(DEBUG, "Fill Rx Freelist for %hu descriptors.",
			rxq->rx_count);

	for (i = 0; i < rxq->rx_count; i++) {
		struct nfp_net_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_pktmbuf_alloc(rxq->mem_pool);

		if (mbuf == NULL) {
			PMD_DRV_LOG(ERR, "RX mbuf alloc failed queue_id=%hu.",
				rxq->qidx);
			return -ENOMEM;
		}

		dma_addr = rte_mbuf_data_iova_default(mbuf);

		rxd = &rxq->rxds[i];
		rxd->fld.dd = 0;
		rxd->fld.dma_addr_hi = rte_cpu_to_le_16((dma_addr >> 32) & 0xffff);
		rxd->fld.dma_addr_lo = rte_cpu_to_le_32(dma_addr & 0xffffffff);

		rxe[i].mbuf = mbuf;
	}

	/* Make sure all writes are flushed before telling the hardware */
	rte_wmb();

	/* Not advertising the whole ring as the firmware gets confused if so */
	PMD_RX_LOG(DEBUG, "Increment FL write pointer in %hu.", rxq->rx_count - 1);

	nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, rxq->rx_count - 1);

	return 0;
}

int
nfp_net_rx_freelist_setup(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (nfp_net_rx_fill_freelist(dev->data->rx_queues[i]) != 0)
			return -1;
	}

	return 0;
}

uint32_t
nfp_net_rx_queue_count(void *rx_queue)
{
	uint32_t idx;
	uint32_t count = 0;
	struct nfp_net_rxq *rxq;
	struct nfp_net_rx_desc *rxds;

	rxq = rx_queue;
	idx = rxq->rd_p;

	/*
	 * Other PMDs are just checking the DD bit in intervals of 4
	 * descriptors and counting all four if the first has the DD
	 * bit on. Of course, this is not accurate but can be good for
	 * performance. But ideally that should be done in descriptors
	 * chunks belonging to the same cache line.
	 */
	while (count < rxq->rx_count) {
		rxds = &rxq->rxds[idx];
		if ((rxds->rxd.meta_len_dd & PCIE_DESC_RX_DD) == 0)
			break;

		count++;
		idx++;

		/* Wrapping */
		if ((idx) == rxq->rx_count)
			idx = 0;
	}

	return count;
}

/**
 * Set packet type to mbuf based on parsed structure.
 *
 * @param nfp_ptype
 *   Packet type structure parsing from Rx descriptor.
 * @param mb
 *   Mbuf to set the packet type.
 */
static void
nfp_net_set_ptype(const struct nfp_ptype_parsed *nfp_ptype,
		struct rte_mbuf *mb)
{
	uint32_t mbuf_ptype = RTE_PTYPE_L2_ETHER;
	uint8_t nfp_tunnel_ptype = nfp_ptype->tunnel_ptype;

	if (nfp_tunnel_ptype != NFP_NET_PTYPE_TUNNEL_NONE)
		mbuf_ptype |= RTE_PTYPE_INNER_L2_ETHER;

	switch (nfp_ptype->outer_l3_ptype) {
	case NFP_NET_PTYPE_OUTER_L3_NONE:
		break;
	case NFP_NET_PTYPE_OUTER_L3_IPV4:
		mbuf_ptype |= RTE_PTYPE_L3_IPV4;
		break;
	case NFP_NET_PTYPE_OUTER_L3_IPV6:
		mbuf_ptype |= RTE_PTYPE_L3_IPV6;
		break;
	default:
		PMD_RX_LOG(DEBUG, "Unrecognized nfp outer layer 3 packet type: %u.",
				nfp_ptype->outer_l3_ptype);
		break;
	}

	switch (nfp_tunnel_ptype) {
	case NFP_NET_PTYPE_TUNNEL_NONE:
		break;
	case NFP_NET_PTYPE_TUNNEL_VXLAN:
		mbuf_ptype |= RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L4_UDP;
		break;
	case NFP_NET_PTYPE_TUNNEL_NVGRE:
		mbuf_ptype |= RTE_PTYPE_TUNNEL_NVGRE;
		break;
	case NFP_NET_PTYPE_TUNNEL_GENEVE:
		mbuf_ptype |= RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L4_UDP;
		break;
	default:
		PMD_RX_LOG(DEBUG, "Unrecognized nfp tunnel packet type: %u.",
				nfp_tunnel_ptype);
		break;
	}

	switch (nfp_ptype->l4_ptype) {
	case NFP_NET_PTYPE_L4_NONE:
		break;
	case NFP_NET_PTYPE_L4_TCP:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L4_TCP);
		break;
	case NFP_NET_PTYPE_L4_UDP:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L4_UDP);
		break;
	case NFP_NET_PTYPE_L4_FRAG:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L4_FRAG);
		break;
	case NFP_NET_PTYPE_L4_NONFRAG:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L4_NONFRAG);
		break;
	case NFP_NET_PTYPE_L4_ICMP:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L4_ICMP);
		break;
	case NFP_NET_PTYPE_L4_SCTP:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L4_SCTP);
		break;
	default:
		PMD_RX_LOG(DEBUG, "Unrecognized nfp layer 4 packet type: %u.",
				nfp_ptype->l4_ptype);
		break;
	}

	switch (nfp_ptype->l3_ptype) {
	case NFP_NET_PTYPE_L3_NONE:
		break;
	case NFP_NET_PTYPE_L3_IPV4:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L3_IPV4);
		break;
	case NFP_NET_PTYPE_L3_IPV6:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L3_IPV6);
		break;
	case NFP_NET_PTYPE_L3_IPV4_EXT:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L3_IPV4_EXT);
		break;
	case NFP_NET_PTYPE_L3_IPV6_EXT:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L3_IPV6_EXT);
		break;
	case NFP_NET_PTYPE_L3_IPV4_EXT_UNKNOWN:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L3_IPV4_EXT_UNKNOWN);
		break;
	case NFP_NET_PTYPE_L3_IPV6_EXT_UNKNOWN:
		mbuf_ptype |= NFP_PTYPE2RTE(nfp_tunnel_ptype, L3_IPV6_EXT_UNKNOWN);
		break;
	default:
		PMD_RX_LOG(DEBUG, "Unrecognized nfp layer 3 packet type: %u.",
				nfp_ptype->l3_ptype);
		break;
	}

	mb->packet_type = mbuf_ptype;
}

/**
 * Parse the packet type from Rx descriptor and set to mbuf.
 *
 * @param rxq
 *   Rx queue
 * @param rxds
 *   Rx descriptor including the offloading info of packet type.
 * @param mb
 *   Mbuf to set the packet type.
 */
void
nfp_net_parse_ptype(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxds,
		struct rte_mbuf *mb)
{
	uint16_t rxd_ptype;
	struct nfp_net_hw *hw = rxq->hw;
	struct nfp_ptype_parsed nfp_ptype;

	if ((hw->super.ctrl_ext & NFP_NET_CFG_CTRL_PKT_TYPE) == 0)
		return;

	rxd_ptype = rte_le_to_cpu_16(rxds->rxd.offload_info);
	if (rxd_ptype == 0 || (rxds->rxd.flags & PCIE_DESC_RX_VLAN) != 0)
		return;

	nfp_ptype.l4_ptype = (rxd_ptype & NFP_NET_PTYPE_L4_MASK) >>
			NFP_NET_PTYPE_L4_OFFSET;
	nfp_ptype.l3_ptype = (rxd_ptype & NFP_NET_PTYPE_L3_MASK) >>
			NFP_NET_PTYPE_L3_OFFSET;
	nfp_ptype.tunnel_ptype = (rxd_ptype & NFP_NET_PTYPE_TUNNEL_MASK) >>
			NFP_NET_PTYPE_TUNNEL_OFFSET;
	nfp_ptype.outer_l3_ptype = (rxd_ptype & NFP_NET_PTYPE_OUTER_L3_MASK) >>
			NFP_NET_PTYPE_OUTER_L3_OFFSET;

	nfp_net_set_ptype(&nfp_ptype, mb);
}

/*
 * RX path design:
 *
 * There are some decisions to take:
 * 1) How to check DD RX descriptors bit
 * 2) How and when to allocate new mbufs
 *
 * Current implementation checks just one single DD bit each loop. As each
 * descriptor is 8 bytes, it is likely a good idea to check descriptors in
 * a single cache line instead. Tests with this change have not shown any
 * performance improvement but it requires further investigation. For example,
 * depending on which descriptor is next, the number of descriptors could be
 * less than 8 for just checking those in the same cache line. This implies
 * extra work which could be counterproductive by itself. Indeed, last firmware
 * changes are just doing this: writing several descriptors with the DD bit
 * for saving PCIe bandwidth and DMA operations from the NFP.
 *
 * Mbuf allocation is done when a new packet is received. Then the descriptor
 * is automatically linked with the new mbuf and the old one is given to the
 * user. The main drawback with this design is mbuf allocation is heavier than
 * using bulk allocations allowed by DPDK with rte_mempool_get_bulk. From the
 * cache point of view it does not seem allocating the mbuf early on as we are
 * doing now have any benefit at all. Again, tests with this change have not
 * shown any improvement. Also, rte_mempool_get_bulk returns all or nothing
 * so looking at the implications of this type of allocation should be studied
 * deeply.
 */
uint16_t
nfp_net_recv_pkts(void *rx_queue,
		struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	uint16_t data_len;
	uint64_t dma_addr;
	uint16_t avail = 0;
	struct rte_mbuf *mb;
	uint16_t nb_hold = 0;
	struct nfp_net_hw *hw;
	struct rte_mbuf *new_mb;
	struct nfp_net_rxq *rxq;
	struct nfp_pf_dev *pf_dev;
	struct nfp_net_dp_buf *rxb;
	struct nfp_net_rx_desc *rxds;
	uint16_t avail_multiplexed = 0;

	rxq = rx_queue;
	if (unlikely(rxq == NULL)) {
		/*
		 * DPDK just checks the queue is lower than max queues
		 * enabled. But the queue needs to be configured.
		 */
		PMD_RX_LOG(ERR, "RX Bad queue.");
		return 0;
	}

	hw = rxq->hw;
	pf_dev = rxq->hw_priv->pf_dev;

	while (avail + avail_multiplexed < nb_pkts) {
		rxb = &rxq->rxbufs[rxq->rd_p];
		if (unlikely(rxb == NULL)) {
			PMD_RX_LOG(ERR, "The rxb does not exist!");
			break;
		}

		rxds = &rxq->rxds[rxq->rd_p];
		if ((rxds->rxd.meta_len_dd & PCIE_DESC_RX_DD) == 0)
			break;

		/*
		 * Memory barrier to ensure that we won't do other
		 * reads before the DD bit.
		 */
		rte_rmb();

		/*
		 * We got a packet. Let's alloc a new mbuf for refilling the
		 * free descriptor ring as soon as possible.
		 */
		new_mb = rte_pktmbuf_alloc(rxq->mem_pool);
		if (unlikely(new_mb == NULL)) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u queue_id=%hu.",
					rxq->port_id, rxq->qidx);
			nfp_net_mbuf_alloc_failed(rxq);
			break;
		}

		/*
		 * Grab the mbuf and refill the descriptor with the
		 * previously allocated mbuf.
		 */
		mb = rxb->mbuf;
		rxb->mbuf = new_mb;
		data_len = rte_le_to_cpu_16(rxds->rxd.data_len);

		PMD_RX_LOG(DEBUG, "Packet len: %u, mbuf_size: %u.",
				data_len, rxq->mbuf_size);

		/* Size of this segment */
		mb->data_len = data_len - NFP_DESC_META_LEN(rxds);
		/* Size of the whole packet. We just support 1 segment */
		mb->pkt_len = data_len - NFP_DESC_META_LEN(rxds);

		if (unlikely((mb->data_len + hw->rx_offset) > rxq->mbuf_size)) {
			/*
			 * This should not happen and the user has the
			 * responsibility of avoiding it. But we have
			 * to give some info about the error.
			 */
			PMD_RX_LOG(ERR, "The mbuf overflow likely due to the RX offset.");
			rte_pktmbuf_free(mb);
			break;
		}

		/* Filling the received mbuf with packet info */
		if (hw->rx_offset != 0)
			mb->data_off = RTE_PKTMBUF_HEADROOM + hw->rx_offset;
		else
			mb->data_off = RTE_PKTMBUF_HEADROOM + NFP_DESC_META_LEN(rxds);

		/* No scatter mode supported */
		mb->nb_segs = 1;
		mb->next = NULL;
		mb->port = rxq->port_id;

		struct nfp_net_meta_parsed meta;
		nfp_net_meta_parse(rxds, rxq, hw, mb, &meta);

		nfp_net_parse_ptype(rxq, rxds, mb);

		/* Checking the checksum flag */
		nfp_net_rx_cksum(rxq, rxds, mb);

		/* Now resetting and updating the descriptor */
		rxds->vals[0] = 0;
		rxds->vals[1] = 0;
		dma_addr = rte_mbuf_data_iova_default(new_mb);
		rxds->fld.dd = 0;
		rxds->fld.dma_addr_hi = rte_cpu_to_le_16((dma_addr >> 32) & 0xffff);
		rxds->fld.dma_addr_lo = rte_cpu_to_le_32(dma_addr & 0xffffffff);
		nb_hold++;

		rxq->rd_p++;
		if (unlikely(rxq->rd_p == rxq->rx_count)) /* Wrapping */
			rxq->rd_p = 0;

		if (pf_dev->recv_pkt_meta_check_t(&meta)) {
			rx_pkts[avail++] = mb;
		} else {
			if (nfp_flower_pf_dispatch_pkts(rxq, mb, meta.port_id)) {
				avail_multiplexed++;
			} else {
				rte_pktmbuf_free(mb);
				break;
			}
		}
	}

	if (nb_hold == 0)
		return nb_hold;

	PMD_RX_LOG(DEBUG, "RX  port_id=%hu queue_id=%hu, %hu packets received.",
			rxq->port_id, rxq->qidx, avail);

	nb_hold += rxq->nb_rx_hold;

	/*
	 * FL descriptors needs to be written before incrementing the
	 * FL queue WR pointer.
	 */
	rte_wmb();
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "The port=%hu queue=%hu nb_hold=%hu avail=%hu.",
				rxq->port_id, rxq->qidx, nb_hold, avail);
		nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, nb_hold);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	return avail;
}

static void
nfp_net_rx_queue_release_mbufs(struct nfp_net_rxq *rxq)
{
	uint16_t i;

	if (rxq->rxbufs == NULL)
		return;

	for (i = 0; i < rxq->rx_count; i++) {
		if (rxq->rxbufs[i].mbuf != NULL) {
			rte_pktmbuf_free_seg(rxq->rxbufs[i].mbuf);
			rxq->rxbufs[i].mbuf = NULL;
		}
	}
}

void
nfp_net_rx_queue_release(struct rte_eth_dev *dev,
		uint16_t queue_idx)
{
	struct nfp_net_rxq *rxq = dev->data->rx_queues[queue_idx];

	if (rxq != NULL) {
		nfp_net_rx_queue_release_mbufs(rxq);
		rte_eth_dma_zone_free(dev, "rx_ring", queue_idx);
		rte_free(rxq->rxbufs);
		rte_free(rxq);
	}
}

void
nfp_net_reset_rx_queue(struct nfp_net_rxq *rxq)
{
	nfp_net_rx_queue_release_mbufs(rxq);
	rxq->rd_p = 0;
	rxq->nb_rx_hold = 0;
}

static void
nfp_rx_queue_setup_flbufsz(struct nfp_net_hw *hw,
		struct nfp_net_rxq *rxq)
{
	if (!hw->flbufsz_set_flag) {
		hw->flbufsz_set_flag = true;
		hw->flbufsz = rxq->mbuf_size;
		return;
	}

	if (hw->flbufsz < rxq->mbuf_size)
		hw->flbufsz = rxq->mbuf_size;
}

int
nfp_net_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mp)
{
	uint32_t rx_desc_sz;
	uint16_t min_rx_desc;
	uint16_t max_rx_desc;
	struct nfp_net_hw *hw;
	struct nfp_net_rxq *rxq;
	const struct rte_memzone *tz;
	struct nfp_net_hw_priv *hw_priv;

	hw = nfp_net_get_hw(dev);
	hw_priv = dev->process_private;

	nfp_net_rx_desc_limits(hw_priv, &min_rx_desc, &max_rx_desc);

	/* Validating number of descriptors */
	rx_desc_sz = nb_desc * sizeof(struct nfp_net_rx_desc);
	if (rx_desc_sz % NFP_ALIGN_RING_DESC != 0 ||
			nb_desc > max_rx_desc || nb_desc < min_rx_desc) {
		PMD_DRV_LOG(ERR, "Wrong nb_desc value.");
		return -EINVAL;
	}

	/*
	 * Free memory prior to re-allocation if needed. This is the case after
	 * calling @nfp_net_stop().
	 */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		nfp_net_rx_queue_release(dev, queue_idx);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocating rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue", sizeof(struct nfp_net_rxq),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		return -ENOMEM;

	dev->data->rx_queues[queue_idx] = rxq;

	/* Hw queues mapping based on firmware configuration */
	rxq->qidx = queue_idx;
	rxq->fl_qcidx = queue_idx * hw->stride_rx;
	rxq->qcp_fl = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->fl_qcidx);

	/*
	 * Tracking mbuf size for detecting a potential mbuf overflow due to
	 * RX offset.
	 */
	rxq->mem_pool = mp;
	rxq->mbuf_size = rxq->mem_pool->elt_size;
	rxq->mbuf_size -= (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM);
	nfp_rx_queue_setup_flbufsz(hw, rxq);

	rxq->rx_count = nb_desc;
	rxq->port_id = dev->data->port_id;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;

	/*
	 * Allocate RX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
			sizeof(struct nfp_net_rx_desc) * max_rx_desc,
			NFP_MEMZONE_ALIGN, socket_id);
	if (tz == NULL) {
		PMD_DRV_LOG(ERR, "Error allocating rx dma.");
		nfp_net_rx_queue_release(dev, queue_idx);
		dev->data->rx_queues[queue_idx] = NULL;
		return -ENOMEM;
	}

	/* Saving physical and virtual addresses for the RX ring */
	rxq->dma = (uint64_t)tz->iova;
	rxq->rxds = tz->addr;

	/* Mbuf pointers array for referencing mbufs linked to RX descriptors */
	rxq->rxbufs = rte_zmalloc_socket("rxq->rxbufs",
			sizeof(*rxq->rxbufs) * nb_desc, RTE_CACHE_LINE_SIZE,
			socket_id);
	if (rxq->rxbufs == NULL) {
		nfp_net_rx_queue_release(dev, queue_idx);
		dev->data->rx_queues[queue_idx] = NULL;
		return -ENOMEM;
	}

	nfp_net_reset_rx_queue(rxq);

	rxq->hw = hw;
	rxq->hw_priv = dev->process_private;

	/*
	 * Telling the HW about the physical address of the RX ring and number
	 * of descriptors in log2 format.
	 */
	nn_cfg_writeq(&hw->super, NFP_NET_CFG_RXR_ADDR(queue_idx), rxq->dma);
	nn_cfg_writeb(&hw->super, NFP_NET_CFG_RXR_SZ(queue_idx), rte_log2_u32(nb_desc));

	return 0;
}

static inline uint32_t
nfp_net_read_tx_free_qcp(struct nfp_net_txq *txq)
{
	/*
	 * If TX ring pointer write back is not supported, do a PCIe read.
	 * Otherwise read qcp value from write back dma address.
	 */
	if (txq->txrwb == NULL)
		return nfp_qcp_read(txq->qcp_q, NFP_QCP_READ_PTR);

	/*
	 * In most cases the TX count is a power of two and the costly modulus
	 * operation can be substituted with a subtraction and an AND operation.
	 */
	if (rte_is_power_of_2(txq->tx_count) == 1)
		return (*txq->txrwb) & (txq->tx_count - 1);
	else
		return (*txq->txrwb) % txq->tx_count;
}

/**
 * Check for descriptors with a complete status
 *
 * @param txq
 *   TX queue to work with
 *
 * @return
 *   Number of descriptors freed
 */
uint32_t
nfp_net_tx_free_bufs(struct nfp_net_txq *txq)
{
	uint32_t todo;
	uint32_t qcp_rd_p;

	PMD_TX_LOG(DEBUG, "Queue %hu. Check for descriptor with a complete"
			" status.", txq->qidx);

	/* Work out how many packets have been sent */
	qcp_rd_p = nfp_net_read_tx_free_qcp(txq);

	if (qcp_rd_p == txq->rd_p) {
		PMD_TX_LOG(DEBUG, "Queue %hu: It seems harrier is not sending "
				"packets (%u, %u).", txq->qidx,
				qcp_rd_p, txq->rd_p);
		return 0;
	}

	if (qcp_rd_p > txq->rd_p)
		todo = qcp_rd_p - txq->rd_p;
	else
		todo = qcp_rd_p + txq->tx_count - txq->rd_p;

	PMD_TX_LOG(DEBUG, "The qcp_rd_p %u, txq->rd_p: %u, qcp->rd_p: %u.",
			qcp_rd_p, txq->rd_p, txq->rd_p);

	if (todo == 0)
		return todo;

	txq->rd_p += todo;
	if (unlikely(txq->rd_p >= txq->tx_count))
		txq->rd_p -= txq->tx_count;

	return todo;
}

static void
nfp_net_tx_queue_release_mbufs(struct nfp_net_txq *txq)
{
	uint32_t i;

	if (txq->txbufs == NULL)
		return;

	for (i = 0; i < txq->tx_count; i++) {
		if (txq->txbufs[i].mbuf != NULL) {
			rte_pktmbuf_free_seg(txq->txbufs[i].mbuf);
			txq->txbufs[i].mbuf = NULL;
		}
	}
}

void
nfp_net_tx_queue_release(struct rte_eth_dev *dev,
		uint16_t queue_idx)
{
	struct nfp_net_hw *net_hw;
	struct nfp_net_txq *txq = dev->data->tx_queues[queue_idx];

	if (txq != NULL) {
		net_hw = nfp_net_get_hw(dev);
		if (net_hw->txrwb_mz != NULL)
			nn_cfg_writeq(&net_hw->super, NFP_NET_CFG_TXR_WB_ADDR(queue_idx), 0);
		nfp_net_tx_queue_release_mbufs(txq);
		rte_eth_dma_zone_free(dev, "tx_ring", queue_idx);
		rte_free(txq->txbufs);
		rte_free(txq);
	}
}

void
nfp_net_reset_tx_queue(struct nfp_net_txq *txq)
{
	nfp_net_tx_queue_release_mbufs(txq);
	txq->wr_p = 0;
	txq->rd_p = 0;
	if (txq->txrwb != NULL)
		*txq->txrwb = 0;
}

int
nfp_net_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	struct nfp_net_hw_priv *hw_priv;

	hw_priv = dev->process_private;

	if (hw_priv->pf_dev->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		return nfp_net_nfd3_tx_queue_setup(dev, queue_idx,
				nb_desc, socket_id, tx_conf);
	else
		return nfp_net_nfdk_tx_queue_setup(dev, queue_idx,
				nb_desc, socket_id, tx_conf);
}

void
nfp_net_rx_queue_info_get(struct rte_eth_dev *dev,
		uint16_t queue_id,
		struct rte_eth_rxq_info *info)
{
	struct rte_eth_dev_info dev_info;
	struct nfp_net_rxq *rxq = dev->data->rx_queues[queue_id];

	info->mp = rxq->mem_pool;
	info->nb_desc = rxq->rx_count;

	info->conf.rx_free_thresh = rxq->rx_free_thresh;

	nfp_net_infos_get(dev, &dev_info);
	info->conf.offloads = dev_info.rx_offload_capa &
			dev->data->dev_conf.rxmode.offloads;
	info->conf.rx_thresh = dev_info.default_rxconf.rx_thresh;
}

void
nfp_net_tx_queue_info_get(struct rte_eth_dev *dev,
		uint16_t queue_id,
		struct rte_eth_txq_info *info)
{
	struct rte_eth_dev_info dev_info;
	struct nfp_net_hw_priv *hw_priv = dev->process_private;
	struct nfp_net_txq *txq = dev->data->tx_queues[queue_id];

	if (hw_priv->pf_dev->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		info->nb_desc = txq->tx_count / NFD3_TX_DESC_PER_PKT;
	else
		info->nb_desc = txq->tx_count / NFDK_TX_DESC_PER_SIMPLE_PKT;

	info->conf.tx_free_thresh = txq->tx_free_thresh;

	nfp_net_infos_get(dev, &dev_info);
	info->conf.offloads = dev_info.tx_offload_capa &
			dev->data->dev_conf.txmode.offloads;
	info->conf.tx_thresh = dev_info.default_txconf.tx_thresh;
}

void
nfp_net_recv_pkts_set(struct rte_eth_dev *eth_dev)
{
	if (nfp_net_get_avx2_supported())
		eth_dev->rx_pkt_burst = nfp_net_vec_avx2_recv_pkts;
	else
		eth_dev->rx_pkt_burst = nfp_net_recv_pkts;
}

int
nfp_net_rx_burst_mode_get(struct rte_eth_dev *eth_dev,
		uint16_t queue_id __rte_unused,
		struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst;

	pkt_burst = eth_dev->rx_pkt_burst;
	if (pkt_burst == nfp_net_recv_pkts) {
		strlcpy(mode->info, "Scalar",
				RTE_ETH_BURST_MODE_INFO_SIZE);
	} else if (pkt_burst == nfp_net_vec_avx2_recv_pkts) {
		strlcpy(mode->info, "Vector AVX2",
				RTE_ETH_BURST_MODE_INFO_SIZE);
	} else {
		return -EINVAL;
	}

	return 0;
}

int
nfp_net_tx_burst_mode_get(struct rte_eth_dev *eth_dev,
		uint16_t queue_id __rte_unused,
		struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst;

	pkt_burst = eth_dev->tx_pkt_burst;
	if (pkt_burst == nfp_net_nfd3_xmit_pkts) {
		strlcpy(mode->info, "NFD3 Scalar",
				RTE_ETH_BURST_MODE_INFO_SIZE);
	} else if (pkt_burst == nfp_net_nfdk_xmit_pkts) {
		strlcpy(mode->info, "NFDk Scalar",
				RTE_ETH_BURST_MODE_INFO_SIZE);
	} else if (pkt_burst == nfp_net_nfdk_vec_avx2_xmit_pkts) {
		strlcpy(mode->info, "NFDk Vector AVX2",
				RTE_ETH_BURST_MODE_INFO_SIZE);
	} else {
		return -EINVAL;
	}

	return 0;
}
