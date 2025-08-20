/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#include <rte_bus_vdev.h>
#include <ethdev_driver.h>
#include <rte_kvargs.h>
#include <rte_string_fns.h>
#include <rte_vect.h>

#include <fcntl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <rte_ether.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "macb_rxtx.h"

#define MACB_MAX_TX_BURST    32
#define MACB_TX_MAX_FREE_BUF_SZ 64

/* Default RS bit threshold values */
#ifndef MACB_DEFAULT_TX_RS_THRESH
#define MACB_DEFAULT_TX_RS_THRESH   32
#endif
#ifndef MACB_DEFAULT_TX_FREE_THRESH
#define MACB_DEFAULT_TX_FREE_THRESH 32
#endif

uint16_t macb_eth_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
							uint16_t nb_pkts)
{
	struct macb_tx_queue *queue;
	struct macb *bp;
	struct macb_tx_entry *macb_txe;
	uint32_t tx_head, tx_tail;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint16_t nb_tx;
	uint32_t tx_first;
	uint32_t tx_last;
	uint64_t buf_dma_addr;
	uint16_t free_txds;
	u32 ctrl;
	struct macb_dma_desc *txdesc;

	queue = (struct macb_tx_queue *)tx_queue;
	bp = queue->bp;

	macb_reclaim_txd(queue);
	tx_head = queue->tx_head;
	tx_tail = queue->tx_tail;
	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = *tx_pkts++;
		tx_first = tx_tail;
		tx_last = tx_tail + tx_pkt->nb_segs - 1;
		tx_last = macb_tx_ring_wrap(bp, tx_last);

		/* Make hw descriptor updates visible to CPU */
		rte_rmb();

		if (unlikely(tx_head == tx_tail))
			free_txds = bp->tx_ring_size - 1;
		else if (tx_head > tx_tail)
			free_txds = tx_head - tx_tail - 1;
		else
			free_txds = bp->tx_ring_size - (tx_tail - tx_head) - 1;

		if (free_txds < tx_pkt->nb_segs) {
			if (nb_tx == 0)
				return 0;
			goto end_of_tx;
		}

		m_seg = tx_pkt;
		do {
			txdesc = macb_tx_desc(queue, tx_tail);
			macb_txe = macb_tx_entry(queue, tx_tail);
			if (likely(macb_txe->mbuf != NULL))
				rte_pktmbuf_free_seg(macb_txe->mbuf);
			macb_txe->mbuf = m_seg;

			queue->stats.tx_bytes += m_seg->data_len;
			ctrl = (u32)m_seg->data_len | MACB_BIT(TX_USED);
			if (unlikely(tx_tail == (queue->nb_tx_desc - 1)))
				ctrl |= MACB_BIT(TX_WRAP);

			if (likely(tx_tail == tx_last))
				ctrl |= MACB_BIT(TX_LAST);

			buf_dma_addr = rte_mbuf_data_iova(m_seg);
			/* Set TX buffer descriptor */
			macb_set_addr(bp, txdesc, buf_dma_addr);
			txdesc->ctrl = ctrl;
			m_seg = m_seg->next;

			tx_tail = macb_tx_ring_wrap(bp, ++tx_tail);
		} while (unlikely(m_seg != NULL));

		while (unlikely(tx_last != tx_first)) {
			txdesc = macb_tx_desc(queue, tx_last);
			txdesc->ctrl &= ~MACB_BIT(TX_USED);
			tx_last = macb_tx_ring_wrap(bp, --tx_last);
		}

		txdesc = macb_tx_desc(queue, tx_last);
		rte_wmb();
		txdesc->ctrl &= ~MACB_BIT(TX_USED);

		queue->stats.tx_packets++;
	}

end_of_tx:
	macb_writel(bp, NCR, macb_readl(bp, NCR) | MACB_BIT(TSTART));
	queue->tx_tail = tx_tail;

	return nb_tx;
}

uint16_t macb_eth_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
							uint16_t nb_pkts)
{
	struct macb_rx_queue *rxq;
	unsigned int len;
	unsigned int entry, next_entry;
	struct macb_dma_desc *desc, *ndesc;
	uint16_t nb_rx;
	struct macb *bp;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	struct macb_rx_entry *rxe, *rxn;
	uint64_t dma_addr;
	uint8_t rxused_v[MACB_LOOK_AHEAD];
	uint8_t nb_rxused;
	int i;

	nb_rx = 0;
	rxq = rx_queue;
	bp = rxq->bp;

	while (nb_rx < nb_pkts) {
		u32 ctrl;
		bool rxused;
		struct rte_ether_hdr *eth_hdr;
		uint16_t ether_type;

		entry = macb_rx_ring_wrap(bp, rxq->rx_tail);
		desc = macb_rx_desc(rxq, entry);

		/* Make hw descriptor updates visible to CPU */
		rte_rmb();

		rxused = (desc->addr & MACB_BIT(RX_USED)) ? true : false;
		if (!rxused)
			break;

		for (i = 0; i < MACB_LOOK_AHEAD; i++) {
			desc = macb_rx_desc(rxq, (entry + i));
			rxused_v[i] = (desc->addr & MACB_BIT(RX_USED)) ? 1 : 0;
		}

		/* Ensure ctrl is at least as up-to-date as rxused */
		rte_rmb();

		/* Compute how many status bits were set */
		for (i = 0, nb_rxused = 0; i < MACB_LOOK_AHEAD; i++) {
			if (unlikely(rxused_v[i] == 0))
				break;
			nb_rxused += rxused_v[i];
		}

		/* Translate descriptor info to mbuf parameters */
		for (i = 0; i < nb_rxused; i++) {
			rxe = macb_rx_entry(rxq, (entry + i));
			desc = macb_rx_desc(rxq, (entry + i));
			ctrl = desc->ctrl;
			rxq->rx_tail++;
			rte_prefetch0(macb_rx_entry(rxq, rxq->rx_tail)->mbuf);

			if (unlikely((ctrl & (MACB_BIT(RX_SOF) | MACB_BIT(RX_EOF)))
				!= (MACB_BIT(RX_SOF) | MACB_BIT(RX_EOF)))) {
				MACB_LOG(ERR, "not whole frame pointed by descriptor");
				rxq->rx_tail = macb_rx_ring_wrap(bp, rxq->rx_tail);
				rxq->stats.rx_dropped++;

				desc->ctrl = 0;
				rte_wmb();
				desc->addr &= ~MACB_BIT(RX_USED);
				continue;
			}

			nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (unlikely(!nmb)) {
				MACB_LOG(ERR, "RX mbuf alloc failed port_id=%u queue_id=%u",
					 (unsigned int)rxq->port_id, (unsigned int)rxq->queue_id);
				rxq->rx_tail = macb_rx_ring_wrap(bp, rxq->rx_tail);
				rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
				rxq->stats.rx_dropped++;

				desc->ctrl = 0;
				rte_wmb();
				desc->addr &= ~MACB_BIT(RX_USED);
				goto out;
			}
			nmb->data_off = RTE_PKTMBUF_HEADROOM + MACB_RX_DATA_OFFSET;

			next_entry = macb_rx_ring_wrap(bp, (rxq->rx_tail + MACB_NEXT_FETCH));
			rxn = macb_rx_entry(rxq, next_entry);
			rte_prefetch0((char *)rxn->mbuf->buf_addr + rxn->mbuf->data_off);
			ndesc = macb_rx_desc(rxq, next_entry);

			/*
			 * When next RX descriptor is on a cache-line boundary,
			 * prefetch the next 2 RX descriptors.
			 */
			if ((next_entry & 0x3) == 0)
				rte_prefetch0(ndesc);

			rxm = rxe->mbuf;
			rxe->mbuf = nmb;

			len = (ctrl & bp->rx_frm_len_mask) - rxq->crc_len;
			rxq->stats.rx_packets++;
			rxq->stats.rx_bytes += len;

			dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
			rxm->nb_segs = 1;
			rxm->next = NULL;
			rxm->pkt_len = len;
			rxm->data_len = len;
			rxm->port = rxq->port_id;
			rxm->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD;

			eth_hdr = rte_pktmbuf_mtod(rxm, struct rte_ether_hdr *);
			ether_type = eth_hdr->ether_type;

			if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
				rxm->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4;
			else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6))
				rxm->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6;
			else
				rxm->packet_type = RTE_PTYPE_UNKNOWN;

			/*
			 * Store the mbuf address into the next entry of the array
			 * of returned packets.
			 */
			rx_pkts[nb_rx++] = rxm;

			if (unlikely(rxq->rx_tail == rxq->nb_rx_desc)) {
				dma_addr |= MACB_BIT(RX_WRAP);
				rxq->rx_tail = 0;
			}

			desc->ctrl = 0;
			/* Setting addr clears RX_USED and allows reception,
			 * make sure ctrl is cleared first to avoid a race.
			 */
			rte_wmb();
			macb_set_addr(bp, desc, dma_addr);
		}

		if (nb_rxused != MACB_LOOK_AHEAD)
			break;
	}

out:
	return nb_rx;
}

uint16_t macb_eth_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
									  uint16_t nb_pkts)
{
	struct macb_rx_queue *rxq;
	unsigned int len;
	unsigned int entry, next_entry;
	struct macb_dma_desc *desc, *ndesc;
	uint16_t nb_rx;
	struct macb *bp;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	struct macb_rx_entry *rxe, *rxn;
	uint64_t dma_addr;
	uint8_t rxused_v[MACB_LOOK_AHEAD];
	uint8_t nb_rxused;
	uint16_t data_bus_width_mask;
	int i;

	nb_rx = 0;
	rxq = rx_queue;
	bp = rxq->bp;

	/*
	 * Retrieve RX context of current packet, if any.
	 */
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;
	data_bus_width_mask = MACB_DATA_BUS_WIDTH_MASK(bp->data_bus_width);

	while (nb_rx < nb_pkts) {
		u32 ctrl;
		bool rxused;
		struct rte_ether_hdr *eth_hdr;
		uint16_t ether_type;

		entry = macb_rx_ring_wrap(bp, rxq->rx_tail);
		desc = macb_rx_desc(rxq, entry);

		/* Make hw descriptor updates visible to CPU */
		rte_rmb();

		rxused = (desc->addr & MACB_BIT(RX_USED)) ? true : false;
		if (!rxused)
			break;

		for (i = 0; i < MACB_LOOK_AHEAD; i++) {
			desc = macb_rx_desc(rxq, (entry + i));
			rxused_v[i] = (desc->addr & MACB_BIT(RX_USED)) ? 1 : 0;
		}

		/* Ensure ctrl is at least as up-to-date as rxused */
		rte_rmb();

		/* Compute how many status bits were set */
		for (i = 0, nb_rxused = 0; i < MACB_LOOK_AHEAD; i++) {
			if (unlikely(rxused_v[i] == 0))
				break;
			nb_rxused += rxused_v[i];
		}

		/* Translate descriptor info to mbuf parameters */
		for (i = 0; i < nb_rxused; i++) {
			rxe = macb_rx_entry(rxq, (entry + i));
			desc = macb_rx_desc(rxq, (entry + i));
			ctrl = desc->ctrl;
			rxq->rx_tail++;
			rte_prefetch0(macb_rx_entry(rxq, rxq->rx_tail)->mbuf);

			nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (unlikely(!nmb)) {
				MACB_LOG(ERR, "RX mbuf alloc failed port_id=%u queue_id=%u",
					 (unsigned int)rxq->port_id, (unsigned int)rxq->queue_id);
				rxq->rx_tail = macb_rx_ring_wrap(bp, rxq->rx_tail);
				rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
				rxq->stats.rx_dropped++;

				desc->ctrl = 0;
				rte_wmb();
				desc->addr &= ~MACB_BIT(RX_USED);
				goto out;
			}
			nmb->data_off = RTE_PKTMBUF_HEADROOM + MACB_RX_DATA_OFFSET;

			next_entry = macb_rx_ring_wrap(bp, (rxq->rx_tail + MACB_NEXT_FETCH));
			rxn = macb_rx_entry(rxq, next_entry);
			rte_prefetch0((char *)rxn->mbuf->buf_addr + rxn->mbuf->data_off);
			ndesc = macb_rx_desc(rxq, next_entry);

			/*
			 * When next RX descriptor is on a cache-line boundary,
			 * prefetch the next 2 RX descriptors.
			 */
			if ((next_entry & 0x3) == 0)
				rte_prefetch0(ndesc);

			rxm = rxe->mbuf;
			rxe->mbuf = nmb;

			dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
			if (unlikely(rxq->rx_tail == rxq->nb_rx_desc)) {
				dma_addr |= MACB_BIT(RX_WRAP);
				rxq->rx_tail = 0;
			}
			desc->ctrl = 0;
			/* Setting addr clears RX_USED and allows reception,
			 * make sure ctrl is cleared first to avoid a race.
			 */
			rte_wmb();
			macb_set_addr(bp, desc, dma_addr);

			len = ctrl & bp->rx_frm_len_mask;
			rxq->stats.rx_bytes += len;

			/*
			 * If this is the first buffer of the received packet,
			 * set the pointer to the first mbuf of the packet and
			 * initialize its context.
			 * Otherwise, update the total length and the number of segments
			 * of the current scattered packet, and update the pointer to
			 * the last mbuf of the current packet.
			 */
			if (!first_seg) {
				first_seg = rxm;
				first_seg->nb_segs = 1;
				first_seg->pkt_len =
					len ? len : (bp->rx_buffer_size - MACB_RX_DATA_OFFSET -
					(RTE_PKTMBUF_HEADROOM & data_bus_width_mask));
				rxm->data_len = first_seg->pkt_len;

				eth_hdr = rte_pktmbuf_mtod(rxm, struct rte_ether_hdr *);
				ether_type = eth_hdr->ether_type;

				if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
					rxm->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4;
				else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6))
					rxm->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6;
				else
					rxm->packet_type = RTE_PTYPE_UNKNOWN;
			} else {
				rxm->data_len =
					len ? (len - first_seg->pkt_len) : bp->rx_buffer_size;
				rxm->data_off = RTE_PKTMBUF_HEADROOM & ~data_bus_width_mask;
				if (likely(rxm->data_len > 0)) {
					first_seg->pkt_len += rxm->data_len;
					first_seg->nb_segs++;
					last_seg->next = rxm;
				}
			}

			/*
			 * If this is not the last buffer of the received packet,
			 * update the pointer to the last mbuf of the current scattered
			 * packet and continue to parse the RX ring.
			 */
			if (!(ctrl & MACB_BIT(RX_EOF))) {
				last_seg = rxm;
				continue;
			}

			/*
			 * This is the last buffer of the received packet.
			 * If the CRC is not stripped by the hardware:
			 *   - Subtract the CRC	length from the total packet length.
			 *   - If the last buffer only contains the whole CRC or a part
			 *     of it, free the mbuf associated to the last buffer.
			 *     If part of the CRC is also contained in the previous
			 *     mbuf, subtract the length of that CRC part from the
			 *     data length of the previous mbuf.
			 */
			rxm->next = NULL;
			if (unlikely(rxq->crc_len > 0)) {
				first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
				if (rxm->data_len <= RTE_ETHER_CRC_LEN) {
					rte_pktmbuf_free_seg(rxm);
					first_seg->nb_segs--;
					last_seg->data_len = (uint16_t)(last_seg->data_len -
									(RTE_ETHER_CRC_LEN - len));
					last_seg->next = NULL;
				} else {
					rxm->data_len = rxm->data_len - RTE_ETHER_CRC_LEN;
				}
			}

			first_seg->port = rxq->port_id;
			first_seg->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD |
					      RTE_MBUF_F_RX_L4_CKSUM_GOOD;
			/*
			 * Store the mbuf address into the next entry of the array
			 * of returned packets.
			 */
			rx_pkts[nb_rx++] = first_seg;
			rxq->stats.rx_packets++;
			/*
			 * Setup receipt context for a new packet.
			 */
			first_seg = NULL;
			last_seg = NULL;
		}

		if (nb_rxused != MACB_LOOK_AHEAD)
			break;
	}

out:
	/*
	 * Save receive context.
	 */
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	return nb_rx;
}

void __rte_cold macb_tx_queue_release_mbufs(struct macb_tx_queue *txq)
{
	unsigned int i;

	if (txq->tx_sw_ring != NULL) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->tx_sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->tx_sw_ring[i].mbuf);
				txq->tx_sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void __rte_cold macb_tx_queue_release(struct macb_tx_queue *txq)
{
	if (txq != NULL) {
		macb_tx_queue_release_mbufs(txq);
		rte_free(txq->tx_sw_ring);
		rte_free(txq);
	}
}

void __rte_cold macb_eth_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	macb_tx_queue_release(dev->data->tx_queues[qid]);
}

void __rte_cold macb_reset_tx_queue(struct macb_tx_queue *txq, struct rte_eth_dev *dev)
{
	struct macb_tx_entry *txe = txq->tx_sw_ring;
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	uint16_t i;
	struct macb_dma_desc *desc = NULL;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++) {
		desc = macb_tx_desc(txq, i);
		macb_set_addr(bp, desc, 0);
		desc->ctrl = MACB_BIT(TX_USED);
	}

	desc->ctrl |= MACB_BIT(TX_WRAP);
	txq->tx_head = 0;
	txq->tx_tail = 0;
	memset((void *)&txq->stats, 0, sizeof(struct macb_tx_queue_stats));

	/* Initialize ring entries */
	for (i = 0; i < txq->nb_tx_desc; i++)
		txe[i].mbuf = NULL;
}

static void __rte_cold
macb_set_tx_function(struct macb_tx_queue *txq, struct rte_eth_dev *dev)
{
	if (txq->tx_rs_thresh >= MACB_MAX_TX_BURST) {
		if (txq->tx_rs_thresh <= MACB_TX_MAX_FREE_BUF_SZ &&
		    (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128)) {
			MACB_LOG(DEBUG, "Vector tx enabled.");
			dev->tx_pkt_burst = macb_eth_xmit_pkts_vec;
		}
	} else {
		dev->tx_pkt_burst = macb_eth_xmit_pkts;
	}
}

int __rte_cold macb_eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
							uint16_t nb_desc, unsigned int socket_id,
							const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct macb_tx_queue *txq;
	uint32_t size;
	struct macb_priv *priv;
	struct macb *bp;
	uint16_t tx_free_thresh, tx_rs_thresh;

	priv = dev->data->dev_private;
	bp = priv->bp;
	/*
	 * The following two parameters control the setting of the RS bit on
	 * transmit descriptors.
	 * TX descriptors will have their RS bit set after txq->tx_rs_thresh
	 * descriptors have been used.
	 * The TX descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free TX
	 * descriptors.
	 * The following constraints must be satisfied:
	 *  tx_rs_thresh must be greater than 0.
	 *  tx_rs_thresh must be less than the size of the ring minus 2.
	 *  tx_rs_thresh must be less than or equal to tx_free_thresh.
	 *  tx_rs_thresh must be a divisor of the ring size.
	 *  tx_free_thresh must be greater than 0.
	 *  tx_free_thresh must be less than the size of the ring minus 3.
	 *  tx_free_thresh + tx_rs_thresh must not exceed nb_desc.
	 * One descriptor in the TX ring is used as a sentinel to avoid a
	 * H/W race condition, hence the maximum threshold constraints.
	 * When set to zero use default values.
	 */
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
			tx_conf->tx_free_thresh : MACB_DEFAULT_TX_FREE_THRESH);
	/* force tx_rs_thresh to adapt an aggressive tx_free_thresh */
	tx_rs_thresh = (MACB_DEFAULT_TX_RS_THRESH + tx_free_thresh > nb_desc) ?
			nb_desc - tx_free_thresh : MACB_DEFAULT_TX_RS_THRESH;
	if (tx_conf->tx_rs_thresh > 0)
		tx_rs_thresh = tx_conf->tx_rs_thresh;
	if (tx_rs_thresh + tx_free_thresh > nb_desc) {
		MACB_LOG(ERR, "tx_rs_thresh + tx_free_thresh must not "
			     "exceed nb_desc. (tx_rs_thresh=%u "
			     "tx_free_thresh=%u nb_desc=%u port = %d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (unsigned int)tx_free_thresh,
			     (unsigned int)nb_desc,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -(EINVAL);
	}
	if (tx_rs_thresh >= (nb_desc - 2)) {
		MACB_LOG(ERR, "tx_rs_thresh must be less than the number "
			"of TX descriptors minus 2. (tx_rs_thresh=%u "
			"port=%d queue=%d)", (unsigned int)tx_rs_thresh,
			(int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}
	if (tx_rs_thresh > MACB_DEFAULT_TX_RS_THRESH) {
		MACB_LOG(ERR, "tx_rs_thresh must be less or equal than %u. "
			"(tx_rs_thresh=%u port=%d queue=%d)",
			MACB_DEFAULT_TX_RS_THRESH, (unsigned int)tx_rs_thresh,
			(int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		MACB_LOG(ERR, "tx_rs_thresh must be less than the "
			     "tx_free_thresh must be less than the number of "
			     "TX descriptors minus 3. (tx_free_thresh=%u "
			     "port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}
	if (tx_rs_thresh > tx_free_thresh) {
		MACB_LOG(ERR, "tx_rs_thresh must be less than or equal to "
			     "tx_free_thresh. (tx_free_thresh=%u "
			     "tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -(EINVAL);
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		MACB_LOG(ERR, "tx_rs_thresh must be a divisor of the "
			     "number of TX descriptors. (tx_rs_thresh=%u "
			     "port=%d queue=%d)", (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/*
	 * If rs_bit_thresh is greater than 1, then TX WTHRESH should be
	 * set to 0. If WTHRESH is greater than zero, the RS bit is ignored
	 * by the NIC and all descriptors are written back after the NIC
	 * accumulates WTHRESH descriptors.
	 */
	if (tx_rs_thresh > 1 && tx_conf->tx_thresh.wthresh != 0) {
		MACB_LOG(ERR, "TX WTHRESH must be set to 0 if "
			     "tx_rs_thresh is greater than 1. (tx_rs_thresh=%u "
			     "port=%d queue=%d)", (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/*
	 * Validate number of transmit descriptors.
	 * It must not exceed hardware maximum.
	 */
	if ((nb_desc % MACB_TX_LEN_ALIGN) != 0 || nb_desc > MACB_MAX_RING_DESC ||
		nb_desc < MACB_MIN_RING_DESC) {
		MACB_LOG(ERR, "number of descriptors exceeded.");
		return -EINVAL;
	}

	bp->tx_ring_size = nb_desc;

	/* Free memory prior to re-allocation if needed */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		macb_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* First allocate the tx queue data structure */
	txq = rte_zmalloc("ethdev TX queue", sizeof(struct macb_tx_queue),
					  RTE_CACHE_LINE_SIZE);
	if (txq == NULL) {
		MACB_LOG(ERR, "failed to alloc txq.");
		return -ENOMEM;
	}

	if (queue_idx) {
		txq->ISR = GEM_ISR(queue_idx - 1);
		txq->IER = GEM_IER(queue_idx - 1);
		txq->IDR = GEM_IDR(queue_idx - 1);
		txq->IMR = GEM_IMR(queue_idx - 1);
		txq->TBQP = GEM_TBQP(queue_idx - 1);
		if (bp->hw_dma_cap & HW_DMA_CAP_64B)
			txq->TBQPH = GEM_TBQPH(queue_idx - 1);
	} else {
		/* queue0 uses legacy registers */
		txq->ISR = MACB_ISR;
		txq->IER = MACB_IER;
		txq->IDR = MACB_IDR;
		txq->IMR = MACB_IMR;
		txq->TBQP = MACB_TBQP;
		if (bp->hw_dma_cap & HW_DMA_CAP_64B)
			txq->TBQPH = MACB_TBQPH;
	}

	size = TX_RING_BYTES(bp) + bp->tx_bd_rd_prefetch;

	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx, size,
								  RTE_CACHE_LINE_SIZE, socket_id);
	if (tz == NULL) {
		macb_tx_queue_release(txq);
		MACB_LOG(ERR, "failed to alloc tx_ring.");
		return -ENOMEM;
	}

	txq->bp = bp;
	txq->nb_tx_desc = nb_desc;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->tx_free_thresh = tx_free_thresh;
	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->tx_ring_dma = tz->iova;

	txq->tx_ring = (struct macb_dma_desc *)tz->addr;
	/* Allocate software ring */
	txq->tx_sw_ring =
		rte_zmalloc("txq->sw_ring", sizeof(struct macb_tx_entry) * nb_desc,
					RTE_CACHE_LINE_SIZE);

	if (txq->tx_sw_ring == NULL) {
		macb_tx_queue_release(txq);
		MACB_LOG(ERR, "failed to alloc tx_sw_ring.");
		return -ENOMEM;
	}

	macb_set_tx_function(txq, dev);
	macb_reset_tx_queue(txq, dev);
	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

int macb_tx_phyaddr_check(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint32_t bus_addr_high;
	struct macb_tx_queue *txq;

	if (dev->data->tx_queues == NULL) {
		MACB_LOG(ERR, "tx queue is null.");
		return -ENOMEM;
	}
	txq = dev->data->tx_queues[0];
	bus_addr_high = upper_32_bits(txq->tx_ring_dma);

	/* Check the high address of the tx queue. */
	for (i = 1; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (bus_addr_high != upper_32_bits(txq->tx_ring_dma))
			return -EFAULT;
	}

	return 0;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
void __rte_cold macb_eth_tx_init(struct rte_eth_dev *dev)
{
	struct macb_tx_queue *txq;
	uint16_t i;
	struct macb_priv *priv;
	struct macb *bp;

	priv = dev->data->dev_private;
	bp = priv->bp;

	/* Setup the Base of the Tx Descriptor Rings. */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		uint64_t bus_addr;
		txq = dev->data->tx_queues[i];
		bus_addr = txq->tx_ring_dma;

		/* Disable tx interrupts */
		queue_writel(txq, IDR, -1);
		queue_readl(txq, ISR);
		if (bp->caps & MACB_CAPS_ISR_CLEAR_ON_WRITE)
			queue_writel(txq, ISR, -1);
		queue_writel(txq, IDR, MACB_TX_INT_FLAGS | MACB_BIT(HRESP));

		queue_writel(txq, TBQP, lower_32_bits(bus_addr));
		if (bp->hw_dma_cap & HW_DMA_CAP_64B)
			queue_writel(txq, TBQPH, upper_32_bits(bus_addr));
	}

	/* Start tx queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
}

void __rte_cold macb_rx_queue_release_mbufs_vec(struct macb_rx_queue *rxq)
{
	const unsigned int mask = rxq->nb_rx_desc - 1;
	unsigned int i;

	if (rxq->rx_sw_ring == NULL || rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->rx_sw_ring[i].mbuf != NULL)
				rte_pktmbuf_free_seg(rxq->rx_sw_ring[i].mbuf);
		}
	} else {
		for (i = rxq->rx_tail;
		     i != rxq->rxrearm_start;
		     i = (i + 1) & mask) {
			if (rxq->rx_sw_ring[i].mbuf != NULL)
				rte_pktmbuf_free_seg(rxq->rx_sw_ring[i].mbuf);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->rx_sw_ring, 0, sizeof(rxq->rx_sw_ring[0]) * rxq->nb_rx_desc);
}

void __rte_cold macb_rx_queue_release_mbufs(struct macb_rx_queue *rxq)
{
	unsigned int i;
	struct macb *bp = rxq->bp;

	if (rxq->pkt_first_seg != NULL) {
		rte_pktmbuf_free(rxq->pkt_first_seg);
		rxq->pkt_first_seg = NULL;
	}

	if (bp->rx_vec_allowed) {
		macb_rx_queue_release_mbufs_vec(rxq);
		return;
	}

	if (rxq->rx_sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->rx_sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->rx_sw_ring[i].mbuf);
				rxq->rx_sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void __rte_cold macb_rx_queue_release(struct macb_rx_queue *rxq)
{
	if (rxq != NULL) {
		macb_rx_queue_release_mbufs(rxq);
		rte_free(rxq->rx_sw_ring);
		rte_free(rxq);
	}
}

void __rte_cold macb_eth_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	macb_rx_queue_release(dev->data->rx_queues[qid]);
}

void __rte_cold macb_dev_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		macb_eth_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
		rte_eth_dma_zone_free(dev, "rx_ring", i);
	}

	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		macb_eth_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
		rte_eth_dma_zone_free(dev, "tx_ring", i);
	}
	dev->data->nb_tx_queues = 0;
}

void __rte_cold macb_reset_rx_queue(struct macb_rx_queue *rxq)
{
	unsigned int i;
	struct macb_dma_desc *rxdesc;

	uint16_t len = rxq->nb_rx_desc;

	if (rxq->bp->rx_bulk_alloc_allowed)
		len += MACB_MAX_RX_BURST;

	/* Zero out HW ring memory */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		rxdesc = macb_rx_desc(rxq, i);
		rxdesc->ctrl = 0;
		macb_set_addr(rxq->bp, rxdesc, 0);
	}

	if (rxq->bp->rx_bulk_alloc_allowed) {
		rxdesc = macb_rx_desc(rxq, rxq->nb_rx_desc - 1);

		for (i = 0; i < MACB_MAX_RX_BURST; i++) {
			rxdesc += MACB_DESC_ADDR_INTERVAL;
			rxdesc->ctrl = 0;
			macb_set_addr(rxq->bp, rxdesc, 0);
		}
	}

	/*
	 * initialize extra software ring entries. Space for these extra
	 * entries is always allocated
	 */
	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = rxq->nb_rx_desc; i < len; ++i) {
		if (rxq->rx_sw_ring[i].mbuf == NULL)
			rxq->rx_sw_ring[i].mbuf = &rxq->fake_mbuf;
	}

	rxq->rx_tail = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;

	rxq->rxrearm_start = 0;
	rxq->rxrearm_nb = 0;
}

uint64_t __rte_cold macb_get_rx_port_offloads_capa(struct rte_eth_dev *dev __rte_unused)
{
	uint64_t rx_offload_capa;

	rx_offload_capa = RTE_ETH_RX_OFFLOAD_IPV4_CKSUM | RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
			  RTE_ETH_RX_OFFLOAD_TCP_CKSUM | RTE_ETH_RX_OFFLOAD_SCATTER |
			  RTE_ETH_RX_OFFLOAD_KEEP_CRC;

	return rx_offload_capa;
}

uint64_t __rte_cold macb_get_rx_queue_offloads_capa(struct rte_eth_dev *dev)
{
	uint64_t rx_queue_offload_capa;

	/*
	 * As only one Rx queue can be used, let per queue offloading
	 * capability be same to per port queue offloading capability
	 * for better convenience.
	 */
	rx_queue_offload_capa = macb_get_rx_port_offloads_capa(dev);

	return rx_queue_offload_capa;
}

/*
 * Check if Rx Burst Bulk Alloc function can be used.
 * Return
 *        0: the preconditions are satisfied and the bulk allocation function
 *           can be used.
 *  -EINVAL: the preconditions are NOT satisfied and the default Rx burst
 *           function must be used.
 */
static inline int __rte_cold
macb_rx_burst_bulk_alloc_preconditions(struct macb_rx_queue *rxq)
{
	int ret = 0;

	/*
	 * Make sure the following pre-conditions are satisfied:
	 *   rxq->rx_free_thresh >= MACB_MAX_RX_BURST
	 *   rxq->rx_free_thresh < rxq->nb_rx_desc
	 *   (rxq->nb_rx_desc % rxq->rx_free_thresh) == 0
	 */
	if (!(rxq->rx_free_thresh >= MACB_MAX_RX_BURST)) {
		MACB_INFO("Rx Burst Bulk Alloc Preconditions: "
				"rxq->rx_free_thresh=%d, "
				"MACB_MAX_RX_BURST=%d",
				rxq->rx_free_thresh, MACB_MAX_RX_BURST);
		ret = -EINVAL;
	} else if (!(rxq->rx_free_thresh < rxq->nb_rx_desc)) {
		MACB_INFO("Rx Burst Bulk Alloc Preconditions: "
				"rxq->rx_free_thresh=%d, "
				"rxq->nb_rx_desc=%d",
				rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if (!((rxq->nb_rx_desc % rxq->rx_free_thresh) == 0)) {
		MACB_INFO("Rx Burst Bulk Alloc Preconditions: "
				"rxq->nb_rx_desc=%d, "
				"rxq->rx_free_thresh=%d",
				rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}
	return ret;
}

int __rte_cold macb_eth_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
							uint16_t nb_desc, unsigned int socket_id,
							const struct rte_eth_rxconf *rx_conf,
							struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct macb_rx_queue *rxq;
	unsigned int size;
	struct macb_priv *priv;
	struct macb *bp;
	uint64_t offloads;
	uint16_t len = nb_desc;

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	priv = dev->data->dev_private;
	bp = priv->bp;

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of MACB_RX_LEN_ALIGN.
	 */
	if (nb_desc % MACB_RX_LEN_ALIGN != 0 || nb_desc > MACB_MAX_RING_DESC ||
		nb_desc < MACB_MIN_RING_DESC) {
		return -EINVAL;
	}

	bp->rx_ring_size = nb_desc;

	/* Free memory prior to re-allocation if needed */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		macb_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the RX queue data structure. */
	rxq = rte_zmalloc("ethdev RX queue", sizeof(struct macb_rx_queue),
					  RTE_CACHE_LINE_SIZE);
	if (rxq == NULL) {
		MACB_LOG(ERR, "failed to alloc rxq.");
		return -ENOMEM;
	}

	if (queue_idx) {
		rxq->ISR = GEM_ISR(queue_idx - 1);
		rxq->IER = GEM_IER(queue_idx - 1);
		rxq->IDR = GEM_IDR(queue_idx - 1);
		rxq->IMR = GEM_IMR(queue_idx - 1);
		rxq->RBQP = GEM_RBQP(queue_idx - 1);
		rxq->RBQS = GEM_RBQS(queue_idx - 1);
		if (bp->hw_dma_cap & HW_DMA_CAP_64B)
			rxq->RBQPH = GEM_RBQPH(queue_idx - 1);
	} else {
		/* queue0 uses legacy registers */
		rxq->ISR = MACB_ISR;
		rxq->IER = MACB_IER;
		rxq->IDR = MACB_IDR;
		rxq->IMR = MACB_IMR;
		rxq->RBQP = MACB_RBQP;
		if (bp->hw_dma_cap & HW_DMA_CAP_64B)
			rxq->RBQPH = MACB_RBQPH;
	}

	rxq->bp = bp;
	rxq->offloads = offloads;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

	/*
	 * Certain constraints must be met in order to use the bulk buffer
	 * allocation Rx burst function. If any of Rx queues doesn't meet them
	 * the feature should be disabled for the whole port.
	 */
	if (macb_rx_burst_bulk_alloc_preconditions(rxq)) {
		MACB_INFO("queue[%d] doesn't meet Rx Bulk Alloc "
				    "preconditions - canceling the feature for "
				    "port[%d]",
			     rxq->queue_id, rxq->port_id);
		bp->rx_bulk_alloc_allowed = false;
	}

	/*
	 *  Allocate RX ring hardware descriptors. A memzone large enough to
	 *  handle the maximum ring size is allocated in order to allow for
	 *  resizing in later calls to the queue setup function.
	 */
	size = RX_RING_BYTES(bp) + bp->rx_bd_rd_prefetch;

	if (rxq->bp->rx_bulk_alloc_allowed)
		size += macb_dma_desc_get_size(bp) * MACB_MAX_RX_BURST;

	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx, size,
								  RTE_CACHE_LINE_SIZE, socket_id);

	if (rz == NULL) {
		macb_rx_queue_release(rxq);
		MACB_LOG(ERR, "failed to alloc rx_ring.");
		return -ENOMEM;
	}

	rxq->rx_ring_dma = rz->iova;
	rxq->rx_ring = (struct macb_dma_desc *)rz->addr;

	if (rxq->bp->rx_bulk_alloc_allowed)
		len += MACB_MAX_RX_BURST;

	/* Allocate software ring. */
	rxq->rx_sw_ring =
		rte_zmalloc("rxq->sw_ring", sizeof(struct macb_rx_entry) * len,
					RTE_CACHE_LINE_SIZE);
	if (rxq->rx_sw_ring == NULL) {
		macb_rx_queue_release(rxq);
		MACB_LOG(ERR, "failed to alloc rx_sw_ring.");
		return -ENOMEM;
	}
	/* MACB_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64,
	 * rxq->rx_sw_ring, rxq->rx_ring, rxq->rx_ring_dma);
	 */

	dev->data->rx_queues[queue_idx] = rxq;
	macb_reset_rx_queue(rxq);

	return 0;
}

void __rte_cold macb_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
					   struct rte_eth_rxq_info *qinfo)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	struct macb_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->rx_buf_size = bp->rx_buffer_size;
	qinfo->nb_desc = rxq->nb_rx_desc;
	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.offloads = rxq->offloads;
}

void __rte_cold macb_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
					   struct rte_eth_txq_info *qinfo)
{
	struct macb_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];
	qinfo->nb_desc = txq->nb_tx_desc;
	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
}

static int __rte_cold macb_alloc_rx_queue_mbufs(struct macb_rx_queue *rxq)
{
	struct macb_rx_entry *rxe = rxq->rx_sw_ring;
	uint64_t dma_addr;
	unsigned int i;
	struct macb *bp;

	bp = rxq->bp;

	/* Initialize software ring entries. */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		struct macb_dma_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			MACB_LOG(ERR, "RX mbuf alloc failed "
				     "queue_id=%hu", rxq->queue_id);
			return -ENOMEM;
		}
		mbuf->data_off = RTE_PKTMBUF_HEADROOM + MACB_RX_DATA_OFFSET;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxd = macb_rx_desc(rxq, i);
		if (i == rxq->nb_rx_desc - 1)
			dma_addr |= MACB_BIT(RX_WRAP);
		rxd->ctrl = 0;
		/* Setting addr clears RX_USED and allows reception,
		 * make sure ctrl is cleared first to avoid a race.
		 */
		rte_wmb();
		macb_set_addr(bp, rxd, dma_addr);
		rxe[i].mbuf = mbuf;
	}

	rte_wmb();
	return 0;
}

void __rte_cold macb_init_rx_buffer_size(struct macb *bp, size_t size)
{
	if (!macb_is_gem(bp)) {
		bp->rx_buffer_size = MACB_RX_BUFFER_SIZE;
	} else {
		bp->rx_buffer_size = size;

		if (bp->rx_buffer_size % RX_BUFFER_MULTIPLE) {
			bp->rx_buffer_size =
				roundup(bp->rx_buffer_size, RX_BUFFER_MULTIPLE);
		}
	}
}

static void __rte_cold
macb_set_rx_function(struct macb_rx_queue *rxq, struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	u32	max_len;
	uint16_t buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
							RTE_PKTMBUF_HEADROOM);

	max_len = dev->data->mtu + MACB_ETH_OVERHEAD;
	if (max_len > buf_size ||
	    dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER) {
		if (!dev->data->scattered_rx)
			MACB_INFO("forcing scatter mode");
		dev->data->scattered_rx = 1;
	}

	/*
	 * In order to allow Vector Rx there are a few configuration
	 * conditions to be met and Rx Bulk Allocation should be allowed.
	 */
	if (!bp->rx_bulk_alloc_allowed ||
			rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_128) {
		MACB_INFO("Port[%d] doesn't meet Vector Rx "
				    "preconditions",
			     dev->data->port_id);
		bp->rx_vec_allowed = false;
	}

	if (dev->data->scattered_rx) {
		if (bp->rx_vec_allowed) {
			MACB_INFO("Using Vector Scattered Rx "
						"callback (port=%d).",
						dev->data->port_id);
			dev->rx_pkt_burst = macb_eth_recv_scattered_pkts_vec;
		} else {
			MACB_INFO("Using Regular (non-vector) "
					    "Scattered Rx callback "
					    "(port=%d).",
				     dev->data->port_id);
			dev->rx_pkt_burst = macb_eth_recv_scattered_pkts;
		}
	} else {
		if (bp->rx_vec_allowed) {
			MACB_INFO("Vector rx enabled");
			dev->rx_pkt_burst = macb_eth_recv_pkts_vec;
		} else {
			dev->rx_pkt_burst = macb_eth_recv_pkts;
		}
	}
}

int macb_rx_phyaddr_check(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint32_t bus_addr_high;
	struct macb_rx_queue *rxq;

	if (dev->data->rx_queues == NULL) {
		MACB_LOG(ERR, "rx queue is null.");
		return -ENOMEM;
	}
	rxq = dev->data->rx_queues[0];
	bus_addr_high = upper_32_bits(rxq->rx_ring_dma);

	/* Check the high address of the rx queue. */
	for (i = 1; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (bus_addr_high != upper_32_bits(rxq->rx_ring_dma))
			return -EFAULT;
	}

	return 0;
}

int __rte_cold macb_eth_rx_init(struct rte_eth_dev *dev)
{
	int ret;
	uint16_t i;
	uint32_t rxcsum;
	struct macb_rx_queue *rxq;
	struct rte_eth_rxmode *rxmode;

	struct macb_priv *priv;
	struct macb *bp;
	uint16_t buf_size;

	priv = dev->data->dev_private;
	bp = priv->bp;

	rxcsum = gem_readl(bp, NCFGR);
	/* Enable both L3/L4 rx checksum offload */
	rxmode = &dev->data->dev_conf.rxmode;
	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM)
		rxcsum |= GEM_BIT(RXCOEN);
	else
		rxcsum &= ~GEM_BIT(RXCOEN);
	gem_writel(bp, NCFGR, rxcsum);

	/* Configure and enable each RX queue. */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		uint64_t bus_addr;

		rxq = dev->data->rx_queues[i];
		rxq->flags = 0;

		/* Disable rx interrupts */
		queue_writel(rxq, IDR, -1);
		queue_readl(rxq, ISR);
		if (bp->caps & MACB_CAPS_ISR_CLEAR_ON_WRITE)
			queue_writel(rxq, ISR, -1);
		queue_writel(rxq, IDR, MACB_RX_INT_FLAGS | MACB_BIT(HRESP));

		/* Allocate buffers for descriptor rings and set up queue */
		ret = macb_alloc_rx_queue_mbufs(rxq);
		if (ret)
			return ret;

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 *  call to configure
		 */
		if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
			rxq->crc_len = RTE_ETHER_CRC_LEN;
		else
			rxq->crc_len = 0;

		bus_addr = rxq->rx_ring_dma;
		queue_writel(rxq, RBQP, lower_32_bits(bus_addr));
		if (bp->hw_dma_cap & HW_DMA_CAP_64B)
			queue_writel(rxq, RBQPH, upper_32_bits(bus_addr));

		/*
		 * Configure RX buffer size.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
							  RTE_PKTMBUF_HEADROOM);

		macb_init_rx_buffer_size(bp, buf_size);
		macb_set_rx_function(rxq, dev);
	}

	/* Start rx queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/* Stubs needed for linkage when RTE_ARCH_ARM64 is not set. */
#if !defined(RTE_ARCH_ARM64)
uint16_t
macb_eth_recv_pkts_vec(void *rx_queue __rte_unused,
		       struct rte_mbuf **rx_pkts __rte_unused,
		       uint16_t nb_pkts __rte_unused)
{
	return 0;
}

uint16_t
macb_eth_recv_scattered_pkts_vec(void *rx_queue __rte_unused,
				  struct rte_mbuf **rx_pkts __rte_unused,
				  uint16_t nb_pkts __rte_unused)
{
	return 0;
}

uint16_t
macb_eth_xmit_pkts_vec(void *tx_queue __rte_unused,
				struct rte_mbuf **tx_pkts __rte_unused,
				uint16_t nb_pkts __rte_unused)
{
	return 0;
}
#endif
