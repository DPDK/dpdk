/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_rxtx.h"
#include "axgbe_phy.h"

#include <rte_time.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_vect.h>

static void
axgbe_rx_queue_release(struct axgbe_rx_queue *rx_queue)
{
	uint16_t i;
	struct rte_mbuf **sw_ring;

	if (rx_queue) {
		sw_ring = rx_queue->sw_ring;
		if (sw_ring) {
			for (i = 0; i < rx_queue->nb_desc; i++) {
				if (sw_ring[i])
					rte_pktmbuf_free(sw_ring[i]);
			}
			rte_free(sw_ring);
		}
		rte_free(rx_queue);
	}
}

void axgbe_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	axgbe_rx_queue_release(dev->data->rx_queues[queue_idx]);
}

int axgbe_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			     uint16_t nb_desc, unsigned int socket_id,
			     const struct rte_eth_rxconf *rx_conf,
			     struct rte_mempool *mp)
{
	PMD_INIT_FUNC_TRACE();
	uint32_t size;
	const struct rte_memzone *dma;
	struct axgbe_rx_queue *rxq;
	uint32_t rx_desc = nb_desc;
	struct axgbe_port *pdata =  dev->data->dev_private;

	/*
	 * validate Rx descriptors count
	 * should be power of 2 and less than h/w supported
	 */
	if ((!rte_is_power_of_2(rx_desc)) ||
	    rx_desc > pdata->rx_desc_count)
		return -EINVAL;
	/* First allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue",
				 sizeof(struct axgbe_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq) {
		PMD_INIT_LOG(ERR, "rte_zmalloc for rxq failed!");
		return -ENOMEM;
	}

	rxq->cur = 0;
	rxq->dirty = 0;
	rxq->pdata = pdata;
	rxq->mb_pool = mp;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->nb_desc = rx_desc;
	rxq->dma_regs = (void *)((uint8_t *)pdata->xgmac_regs + DMA_CH_BASE +
		(DMA_CH_INC * rxq->queue_id));
	rxq->dma_tail_reg = (volatile uint32_t *)((uint8_t *)rxq->dma_regs +
						  DMA_CH_RDTR_LO);
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

	/* CRC strip in AXGBE supports per port not per queue */
	pdata->crc_strip_enable = (rxq->crc_len == 0) ? 1 : 0;
	rxq->free_thresh = rx_conf->rx_free_thresh ?
		rx_conf->rx_free_thresh : AXGBE_RX_FREE_THRESH;
	if (rxq->free_thresh >  rxq->nb_desc)
		rxq->free_thresh = rxq->nb_desc >> 3;

	/* Allocate RX ring hardware descriptors */
	size = rxq->nb_desc * sizeof(union axgbe_rx_desc);
	dma = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx, size, 128,
				       socket_id);
	if (!dma) {
		PMD_DRV_LOG(ERR, "ring_dma_zone_reserve for rx_ring failed\n");
		axgbe_rx_queue_release(rxq);
		return -ENOMEM;
	}
	rxq->ring_phys_addr = (uint64_t)dma->iova;
	rxq->desc = (volatile union axgbe_rx_desc *)dma->addr;
	memset((void *)rxq->desc, 0, size);
	/* Allocate software ring */
	size = rxq->nb_desc * sizeof(struct rte_mbuf *);
	rxq->sw_ring = rte_zmalloc_socket("sw_ring", size,
					  RTE_CACHE_LINE_SIZE,
					  socket_id);
	if (!rxq->sw_ring) {
		PMD_DRV_LOG(ERR, "rte_zmalloc for sw_ring failed\n");
		axgbe_rx_queue_release(rxq);
		return -ENOMEM;
	}
	dev->data->rx_queues[queue_idx] = rxq;
	if (!pdata->rx_queues)
		pdata->rx_queues = dev->data->rx_queues;

	return 0;
}

static void axgbe_prepare_rx_stop(struct axgbe_port *pdata,
				  unsigned int queue)
{
	unsigned int rx_status;
	unsigned long rx_timeout;

	/* The Rx engine cannot be stopped if it is actively processing
	 * packets. Wait for the Rx queue to empty the Rx fifo.  Don't
	 * wait forever though...
	 */
	rx_timeout = rte_get_timer_cycles() + (AXGBE_DMA_STOP_TIMEOUT *
					       rte_get_timer_hz());

	while (time_before(rte_get_timer_cycles(), rx_timeout)) {
		rx_status = AXGMAC_MTL_IOREAD(pdata, queue, MTL_Q_RQDR);
		if ((AXGMAC_GET_BITS(rx_status, MTL_Q_RQDR, PRXQ) == 0) &&
		    (AXGMAC_GET_BITS(rx_status, MTL_Q_RQDR, RXQSTS) == 0))
			break;

		rte_delay_us(900);
	}

	if (!time_before(rte_get_timer_cycles(), rx_timeout))
		PMD_DRV_LOG(ERR,
			    "timed out waiting for Rx queue %u to empty\n",
			    queue);
}

void axgbe_dev_disable_rx(struct rte_eth_dev *dev)
{
	struct axgbe_rx_queue *rxq;
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int i;

	/* Disable MAC Rx */
	AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, DCRCC, 0);
	AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, CST, 0);
	AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, ACS, 0);
	AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, RE, 0);

	/* Prepare for Rx DMA channel stop */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		axgbe_prepare_rx_stop(pdata, i);
	}
	/* Disable each Rx queue */
	AXGMAC_IOWRITE(pdata, MAC_RQC0R, 0);
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		/* Disable Rx DMA channel */
		AXGMAC_DMA_IOWRITE_BITS(rxq, DMA_CH_RCR, SR, 0);
	}
}

void axgbe_dev_enable_rx(struct rte_eth_dev *dev)
{
	struct axgbe_rx_queue *rxq;
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int i;
	unsigned int reg_val = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		/* Enable Rx DMA channel */
		AXGMAC_DMA_IOWRITE_BITS(rxq, DMA_CH_RCR, SR, 1);
	}

	reg_val = 0;
	for (i = 0; i < pdata->rx_q_count; i++)
		reg_val |= (0x02 << (i << 1));
	AXGMAC_IOWRITE(pdata, MAC_RQC0R, reg_val);

	/* Enable MAC Rx */
	AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, DCRCC, 1);
	/* Frame is forwarded after stripping CRC to application*/
	if (pdata->crc_strip_enable) {
		AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, CST, 1);
		AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, ACS, 1);
	}
	AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, RE, 1);
}

/* Rx function one to one refresh */
uint16_t
axgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	PMD_INIT_FUNC_TRACE();
	uint16_t nb_rx = 0;
	struct axgbe_rx_queue *rxq = rx_queue;
	volatile union axgbe_rx_desc *desc;
	uint64_t old_dirty = rxq->dirty;
	struct rte_mbuf *mbuf, *tmbuf;
	unsigned int err, etlt;
	uint32_t error_status;
	uint16_t idx, pidx, pkt_len;
	uint64_t offloads;

	idx = AXGBE_GET_DESC_IDX(rxq, rxq->cur);
	while (nb_rx < nb_pkts) {
		if (unlikely(idx == rxq->nb_desc))
			idx = 0;

		desc = &rxq->desc[idx];

		if (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, OWN))
			break;
		tmbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (unlikely(!tmbuf)) {
			PMD_DRV_LOG(ERR, "RX mbuf alloc failed port_id = %u"
				    " queue_id = %u\n",
				    (unsigned int)rxq->port_id,
				    (unsigned int)rxq->queue_id);
			rte_eth_devices[
				rxq->port_id].data->rx_mbuf_alloc_failed++;
			rxq->rx_mbuf_alloc_failed++;
			break;
		}
		pidx = idx + 1;
		if (unlikely(pidx == rxq->nb_desc))
			pidx = 0;

		rte_prefetch0(rxq->sw_ring[pidx]);
		if ((pidx & 0x3) == 0) {
			rte_prefetch0(&rxq->desc[pidx]);
			rte_prefetch0(&rxq->sw_ring[pidx]);
		}

		mbuf = rxq->sw_ring[idx];
		/* Check for any errors and free mbuf*/
		err = AXGMAC_GET_BITS_LE(desc->write.desc3,
					 RX_NORMAL_DESC3, ES);
		error_status = 0;
		if (unlikely(err)) {
			error_status = desc->write.desc3 & AXGBE_ERR_STATUS;
			if ((error_status != AXGBE_L3_CSUM_ERR) &&
			    (error_status != AXGBE_L4_CSUM_ERR)) {
				rxq->errors++;
				rte_pktmbuf_free(mbuf);
				goto err_set;
			}
		}
		if (rxq->pdata->rx_csum_enable) {
			mbuf->ol_flags = 0;
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
			if (unlikely(error_status == AXGBE_L3_CSUM_ERR)) {
				mbuf->ol_flags &= ~RTE_MBUF_F_RX_IP_CKSUM_GOOD;
				mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
				mbuf->ol_flags &= ~RTE_MBUF_F_RX_L4_CKSUM_GOOD;
				mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;
			} else if (
				unlikely(error_status == AXGBE_L4_CSUM_ERR)) {
				mbuf->ol_flags &= ~RTE_MBUF_F_RX_L4_CKSUM_GOOD;
				mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
			}
		}
		rte_prefetch1(rte_pktmbuf_mtod(mbuf, void *));
		/* Get the RSS hash */
		if (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, RSV))
			mbuf->hash.rss = rte_le_to_cpu_32(desc->write.desc1);
		etlt = AXGMAC_GET_BITS_LE(desc->write.desc3,
				RX_NORMAL_DESC3, ETLT);
		offloads = rxq->pdata->eth_dev->data->dev_conf.rxmode.offloads;
		if (!err || !etlt) {
			if (etlt == RX_CVLAN_TAG_PRESENT) {
				mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN;
				mbuf->vlan_tci =
					AXGMAC_GET_BITS_LE(desc->write.desc0,
							RX_NORMAL_DESC0, OVT);
				if (offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
					mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN_STRIPPED;
				else
					mbuf->ol_flags &= ~RTE_MBUF_F_RX_VLAN_STRIPPED;
			} else {
				mbuf->ol_flags &=
					~(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED);
				mbuf->vlan_tci = 0;
			}
		}
		/* Indicate if a Context Descriptor is next */
		if (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, CDA))
			mbuf->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP
					| RTE_MBUF_F_RX_IEEE1588_TMST;
		pkt_len = AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3,
					     PL) - rxq->crc_len;
		/* Mbuf populate */
		mbuf->next = NULL;
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->port_id;
		mbuf->pkt_len = pkt_len;
		mbuf->data_len = pkt_len;
		rxq->bytes += pkt_len;
		rx_pkts[nb_rx++] = mbuf;
err_set:
		rxq->cur++;
		rxq->sw_ring[idx++] = tmbuf;
		desc->read.baddr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(tmbuf));
		memset((void *)(&desc->read.desc2), 0, 8);
		AXGMAC_SET_BITS_LE(desc->read.desc3, RX_NORMAL_DESC3, OWN, 1);
		rxq->dirty++;
	}
	rxq->pkts += nb_rx;
	if (rxq->dirty != old_dirty) {
		rte_wmb();
		idx = AXGBE_GET_DESC_IDX(rxq, rxq->dirty - 1);
		AXGMAC_DMA_IOWRITE(rxq, DMA_CH_RDTR_LO,
				   low32_value(rxq->ring_phys_addr +
				   (idx * sizeof(union axgbe_rx_desc))));
	}

	return nb_rx;
}


uint16_t eth_axgbe_recv_scattered_pkts(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	PMD_INIT_FUNC_TRACE();
	uint16_t nb_rx = 0;
	struct axgbe_rx_queue *rxq = rx_queue;
	volatile union axgbe_rx_desc *desc;

	uint64_t old_dirty = rxq->dirty;
	struct rte_mbuf *first_seg = NULL;
	struct rte_mbuf *mbuf, *tmbuf;
	unsigned int err, etlt;
	uint32_t error_status;
	uint16_t idx, pidx, data_len = 0, pkt_len = 0;
	uint64_t offloads;

	idx = AXGBE_GET_DESC_IDX(rxq, rxq->cur);
	while (nb_rx < nb_pkts) {
		bool eop = 0;
next_desc:
		if (unlikely(idx == rxq->nb_desc))
			idx = 0;

		desc = &rxq->desc[idx];

		if (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, OWN))
			break;

		tmbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (unlikely(!tmbuf)) {
			PMD_DRV_LOG(ERR, "RX mbuf alloc failed port_id = %u"
				    " queue_id = %u\n",
				    (unsigned int)rxq->port_id,
				    (unsigned int)rxq->queue_id);
			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
			break;
		}

		pidx = idx + 1;
		if (unlikely(pidx == rxq->nb_desc))
			pidx = 0;

		rte_prefetch0(rxq->sw_ring[pidx]);
		if ((pidx & 0x3) == 0) {
			rte_prefetch0(&rxq->desc[pidx]);
			rte_prefetch0(&rxq->sw_ring[pidx]);
		}

		mbuf = rxq->sw_ring[idx];
		/* Check for any errors and free mbuf*/
		err = AXGMAC_GET_BITS_LE(desc->write.desc3,
					 RX_NORMAL_DESC3, ES);
		error_status = 0;
		if (unlikely(err)) {
			error_status = desc->write.desc3 & AXGBE_ERR_STATUS;
			if ((error_status != AXGBE_L3_CSUM_ERR)
					&& (error_status != AXGBE_L4_CSUM_ERR)) {
				rxq->errors++;
				rte_pktmbuf_free(mbuf);
				goto err_set;
			}
		}
		rte_prefetch1(rte_pktmbuf_mtod(mbuf, void *));

		if (!AXGMAC_GET_BITS_LE(desc->write.desc3,
					RX_NORMAL_DESC3, LD)) {
			eop = 0;
			pkt_len = rxq->buf_size;
			data_len = pkt_len;
		} else {
			eop = 1;
			pkt_len = AXGMAC_GET_BITS_LE(desc->write.desc3,
					RX_NORMAL_DESC3, PL);
			data_len = pkt_len - rxq->crc_len;
		}

		if (first_seg != NULL) {
			if (rte_pktmbuf_chain(first_seg, mbuf) != 0)
				rte_mempool_put(rxq->mb_pool,
						first_seg);
		} else {
			first_seg = mbuf;
		}

		/* Get the RSS hash */
		if (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, RSV))
			mbuf->hash.rss = rte_le_to_cpu_32(desc->write.desc1);
		etlt = AXGMAC_GET_BITS_LE(desc->write.desc3,
				RX_NORMAL_DESC3, ETLT);
		offloads = rxq->pdata->eth_dev->data->dev_conf.rxmode.offloads;
		if (!err || !etlt) {
			if (etlt == RX_CVLAN_TAG_PRESENT) {
				mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN;
				mbuf->vlan_tci =
					AXGMAC_GET_BITS_LE(desc->write.desc0,
							RX_NORMAL_DESC0, OVT);
				if (offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
					mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN_STRIPPED;
				else
					mbuf->ol_flags &= ~RTE_MBUF_F_RX_VLAN_STRIPPED;
			} else {
				mbuf->ol_flags &=
					~(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED);
				mbuf->vlan_tci = 0;
			}
		}
		/* Mbuf populate */
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->data_len = data_len;

err_set:
		rxq->cur++;
		rxq->sw_ring[idx++] = tmbuf;
		desc->read.baddr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(tmbuf));
		memset((void *)(&desc->read.desc2), 0, 8);
		AXGMAC_SET_BITS_LE(desc->read.desc3, RX_NORMAL_DESC3, OWN, 1);
		rxq->dirty++;

		if (!eop) {
			rte_pktmbuf_free(mbuf);
			goto next_desc;
		}

		first_seg->pkt_len = pkt_len;
		rxq->bytes += pkt_len;
		mbuf->next = NULL;

		first_seg->port = rxq->port_id;
		if (rxq->pdata->rx_csum_enable) {
			mbuf->ol_flags = 0;
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
			if (unlikely(error_status == AXGBE_L3_CSUM_ERR)) {
				mbuf->ol_flags &= ~RTE_MBUF_F_RX_IP_CKSUM_GOOD;
				mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
				mbuf->ol_flags &= ~RTE_MBUF_F_RX_L4_CKSUM_GOOD;
				mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;
			} else if (unlikely(error_status
						== AXGBE_L4_CSUM_ERR)) {
				mbuf->ol_flags &= ~RTE_MBUF_F_RX_L4_CKSUM_GOOD;
				mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
			}
		}

		rx_pkts[nb_rx++] = first_seg;

		 /* Setup receipt context for a new packet.*/
		first_seg = NULL;
	}

	/* Save receive context.*/
	rxq->pkts += nb_rx;

	if (rxq->dirty != old_dirty) {
		rte_wmb();
		idx = AXGBE_GET_DESC_IDX(rxq, rxq->dirty - 1);
		AXGMAC_DMA_IOWRITE(rxq, DMA_CH_RDTR_LO,
				   low32_value(rxq->ring_phys_addr +
				   (idx * sizeof(union axgbe_rx_desc))));
	}
	return nb_rx;
}

/* Tx Apis */
static void axgbe_tx_queue_release(struct axgbe_tx_queue *tx_queue)
{
	uint16_t i;
	struct rte_mbuf **sw_ring;

	if (tx_queue) {
		sw_ring = tx_queue->sw_ring;
		if (sw_ring) {
			for (i = 0; i < tx_queue->nb_desc; i++) {
				if (sw_ring[i])
					rte_pktmbuf_free(sw_ring[i]);
			}
			rte_free(sw_ring);
		}
		rte_free(tx_queue);
	}
}

void axgbe_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	axgbe_tx_queue_release(dev->data->tx_queues[queue_idx]);
}

int axgbe_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			     uint16_t nb_desc, unsigned int socket_id,
			     const struct rte_eth_txconf *tx_conf)
{
	PMD_INIT_FUNC_TRACE();
	uint32_t tx_desc;
	struct axgbe_port *pdata;
	struct axgbe_tx_queue *txq;
	unsigned int tsize;
	const struct rte_memzone *tz;
	uint64_t offloads;

	tx_desc = nb_desc;
	pdata = dev->data->dev_private;

	/*
	 * validate tx descriptors count
	 * should be power of 2 and less than h/w supported
	 */
	if ((!rte_is_power_of_2(tx_desc)) ||
	    tx_desc > pdata->tx_desc_count ||
	    tx_desc < AXGBE_MIN_RING_DESC)
		return -EINVAL;

	/* First allocate the tx queue data structure */
	txq = rte_zmalloc("ethdev TX queue", sizeof(struct axgbe_tx_queue),
			  RTE_CACHE_LINE_SIZE);
	if (!txq)
		return -ENOMEM;
	txq->pdata = pdata;
	offloads = tx_conf->offloads |
		txq->pdata->eth_dev->data->dev_conf.txmode.offloads;
	txq->nb_desc = tx_desc;
	txq->free_thresh = tx_conf->tx_free_thresh ?
		tx_conf->tx_free_thresh : AXGBE_TX_FREE_THRESH;
	if (txq->free_thresh > txq->nb_desc)
		txq->free_thresh = (txq->nb_desc >> 1);
	txq->free_batch_cnt = txq->free_thresh;

	/* In vector_tx path threshold should be multiple of queue_size*/
	if (txq->nb_desc % txq->free_thresh != 0)
		txq->vector_disable = 1;

	if (offloads != 0)
		txq->vector_disable = 1;

	/* Allocate TX ring hardware descriptors */
	tsize = txq->nb_desc * sizeof(struct axgbe_tx_desc);
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
				      tsize, AXGBE_DESC_ALIGN, socket_id);
	if (!tz) {
		axgbe_tx_queue_release(txq);
		return -ENOMEM;
	}
	memset(tz->addr, 0, tsize);
	txq->ring_phys_addr = (uint64_t)tz->iova;
	txq->desc = tz->addr;
	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->dma_regs = (void *)((uint8_t *)pdata->xgmac_regs + DMA_CH_BASE +
		(DMA_CH_INC * txq->queue_id));
	txq->dma_tail_reg = (volatile uint32_t *)((uint8_t *)txq->dma_regs +
						  DMA_CH_TDTR_LO);
	txq->cur = 0;
	txq->dirty = 0;
	txq->nb_desc_free = txq->nb_desc;
	/* Allocate software ring */
	tsize = txq->nb_desc * sizeof(struct rte_mbuf *);
	txq->sw_ring = rte_zmalloc("tx_sw_ring", tsize,
				   RTE_CACHE_LINE_SIZE);
	if (!txq->sw_ring) {
		axgbe_tx_queue_release(txq);
		return -ENOMEM;
	}
	dev->data->tx_queues[queue_idx] = txq;
	if (!pdata->tx_queues)
		pdata->tx_queues = dev->data->tx_queues;

	if (txq->vector_disable ||
			rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_128)
		dev->tx_pkt_burst = &axgbe_xmit_pkts;
	else
#ifdef RTE_ARCH_X86
		dev->tx_pkt_burst = &axgbe_xmit_pkts_vec;
#else
		dev->tx_pkt_burst = &axgbe_xmit_pkts;
#endif

	return 0;
}

int axgbe_dev_fw_version_get(struct rte_eth_dev *eth_dev,
		char *fw_version, size_t fw_size)
{
	struct axgbe_port *pdata;
	struct axgbe_hw_features *hw_feat;
	int ret;

	pdata = (struct axgbe_port *)eth_dev->data->dev_private;
	hw_feat = &pdata->hw_feat;

	ret = snprintf(fw_version, fw_size, "%d.%d.%d",
			AXGMAC_GET_BITS(hw_feat->version, MAC_VR, USERVER),
			AXGMAC_GET_BITS(hw_feat->version, MAC_VR, DEVID),
			AXGMAC_GET_BITS(hw_feat->version, MAC_VR, SNPSVER));
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

static void axgbe_txq_prepare_tx_stop(struct axgbe_port *pdata,
				      unsigned int queue)
{
	unsigned int tx_status;
	unsigned long tx_timeout;

	/* The Tx engine cannot be stopped if it is actively processing
	 * packets. Wait for the Tx queue to empty the Tx fifo.  Don't
	 * wait forever though...
	 */
	tx_timeout = rte_get_timer_cycles() + (AXGBE_DMA_STOP_TIMEOUT *
					       rte_get_timer_hz());
	while (time_before(rte_get_timer_cycles(), tx_timeout)) {
		tx_status = AXGMAC_MTL_IOREAD(pdata, queue, MTL_Q_TQDR);
		if ((AXGMAC_GET_BITS(tx_status, MTL_Q_TQDR, TRCSTS) != 1) &&
		    (AXGMAC_GET_BITS(tx_status, MTL_Q_TQDR, TXQSTS) == 0))
			break;

		rte_delay_us(900);
	}

	if (!time_before(rte_get_timer_cycles(), tx_timeout))
		PMD_DRV_LOG(ERR,
			    "timed out waiting for Tx queue %u to empty\n",
			    queue);
}

static void axgbe_prepare_tx_stop(struct axgbe_port *pdata,
				  unsigned int queue)
{
	unsigned int tx_dsr, tx_pos, tx_qidx;
	unsigned int tx_status;
	unsigned long tx_timeout;

	if (AXGMAC_GET_BITS(pdata->hw_feat.version, MAC_VR, SNPSVER) > 0x20)
		return axgbe_txq_prepare_tx_stop(pdata, queue);

	/* Calculate the status register to read and the position within */
	if (queue < DMA_DSRX_FIRST_QUEUE) {
		tx_dsr = DMA_DSR0;
		tx_pos = (queue * DMA_DSR_Q_WIDTH) + DMA_DSR0_TPS_START;
	} else {
		tx_qidx = queue - DMA_DSRX_FIRST_QUEUE;

		tx_dsr = DMA_DSR1 + ((tx_qidx / DMA_DSRX_QPR) * DMA_DSRX_INC);
		tx_pos = ((tx_qidx % DMA_DSRX_QPR) * DMA_DSR_Q_WIDTH) +
			DMA_DSRX_TPS_START;
	}

	/* The Tx engine cannot be stopped if it is actively processing
	 * descriptors. Wait for the Tx engine to enter the stopped or
	 * suspended state.  Don't wait forever though...
	 */
	tx_timeout = rte_get_timer_cycles() + (AXGBE_DMA_STOP_TIMEOUT *
					       rte_get_timer_hz());
	while (time_before(rte_get_timer_cycles(), tx_timeout)) {
		tx_status = AXGMAC_IOREAD(pdata, tx_dsr);
		tx_status = GET_BITS(tx_status, tx_pos, DMA_DSR_TPS_WIDTH);
		if ((tx_status == DMA_TPS_STOPPED) ||
		    (tx_status == DMA_TPS_SUSPENDED))
			break;

		rte_delay_us(900);
	}

	if (!time_before(rte_get_timer_cycles(), tx_timeout))
		PMD_DRV_LOG(ERR,
			    "timed out waiting for Tx DMA channel %u to stop\n",
			    queue);
}

void axgbe_dev_disable_tx(struct rte_eth_dev *dev)
{
	struct axgbe_tx_queue *txq;
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int i;

	/* Prepare for stopping DMA channel */
	for (i = 0; i < pdata->tx_q_count; i++) {
		txq = dev->data->tx_queues[i];
		axgbe_prepare_tx_stop(pdata, i);
	}
	/* Disable MAC Tx */
	AXGMAC_IOWRITE_BITS(pdata, MAC_TCR, TE, 0);
	/* Disable each Tx queue*/
	for (i = 0; i < pdata->tx_q_count; i++)
		AXGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, TXQEN,
					0);
	/* Disable each  Tx DMA channel */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		AXGMAC_DMA_IOWRITE_BITS(txq, DMA_CH_TCR, ST, 0);
	}
}

void axgbe_dev_enable_tx(struct rte_eth_dev *dev)
{
	struct axgbe_tx_queue *txq;
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		/* Enable Tx DMA channel */
		AXGMAC_DMA_IOWRITE_BITS(txq, DMA_CH_TCR, ST, 1);
	}
	/* Enable Tx queue*/
	for (i = 0; i < pdata->tx_q_count; i++)
		AXGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, TXQEN,
					MTL_Q_ENABLED);
	/* Enable MAC Tx */
	AXGMAC_IOWRITE_BITS(pdata, MAC_TCR, TE, 1);
}

/* Free Tx conformed mbufs */
static void axgbe_xmit_cleanup(struct axgbe_tx_queue *txq)
{
	volatile struct axgbe_tx_desc *desc;
	uint16_t idx;

	idx = AXGBE_GET_DESC_IDX(txq, txq->dirty);
	while (txq->cur != txq->dirty) {
		if (unlikely(idx == txq->nb_desc))
			idx = 0;
		desc = &txq->desc[idx];
		/* Check for ownership */
		if (AXGMAC_GET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, OWN))
			return;
		memset((void *)&desc->desc2, 0, 8);
		/* Free mbuf */
		rte_pktmbuf_free(txq->sw_ring[idx]);
		txq->sw_ring[idx++] = NULL;
		txq->dirty++;
	}
}

/* Tx Descriptor formation
 * Considering each mbuf requires one desc
 * mbuf is linear
 */
static int axgbe_xmit_hw(struct axgbe_tx_queue *txq,
			 struct rte_mbuf *mbuf)
{
	volatile struct axgbe_tx_desc *desc;
	uint16_t idx;
	uint64_t mask;

	idx = AXGBE_GET_DESC_IDX(txq, txq->cur);
	desc = &txq->desc[idx];

	/* Update buffer address  and length */
	desc->baddr = rte_mbuf_data_iova(mbuf);
	AXGMAC_SET_BITS_LE(desc->desc2, TX_NORMAL_DESC2, HL_B1L,
			   mbuf->pkt_len);
	/* Total msg length to transmit */
	AXGMAC_SET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, FL,
			   mbuf->pkt_len);
	/* Timestamp enablement check */
	if (mbuf->ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
		AXGMAC_SET_BITS_LE(desc->desc2, TX_NORMAL_DESC2, TTSE, 1);
	rte_wmb();
	/* Mark it as First and Last Descriptor */
	AXGMAC_SET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, FD, 1);
	AXGMAC_SET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, LD, 1);
	/* Mark it as a NORMAL descriptor */
	AXGMAC_SET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, CTXT, 0);
	/* configure h/w Offload */
	mask = mbuf->ol_flags & RTE_MBUF_F_TX_L4_MASK;
	if (mask == RTE_MBUF_F_TX_TCP_CKSUM || mask == RTE_MBUF_F_TX_UDP_CKSUM)
		AXGMAC_SET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, CIC, 0x3);
	else if (mbuf->ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
		AXGMAC_SET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, CIC, 0x1);
	rte_wmb();

	if (mbuf->ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
		/* Mark it as a CONTEXT descriptor */
		AXGMAC_SET_BITS_LE(desc->desc3, TX_CONTEXT_DESC3,
				  CTXT, 1);
		/* Set the VLAN tag */
		AXGMAC_SET_BITS_LE(desc->desc3, TX_CONTEXT_DESC3,
				  VT, mbuf->vlan_tci);
		/* Indicate this descriptor contains the VLAN tag */
		AXGMAC_SET_BITS_LE(desc->desc3, TX_CONTEXT_DESC3,
					  VLTV, 1);
		AXGMAC_SET_BITS_LE(desc->desc2, TX_NORMAL_DESC2, VTIR,
				TX_NORMAL_DESC2_VLAN_INSERT);
	} else {
		AXGMAC_SET_BITS_LE(desc->desc2, TX_NORMAL_DESC2, VTIR, 0x0);
	}
	rte_wmb();

	/* Set OWN bit */
	AXGMAC_SET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, OWN, 1);
	rte_wmb();


	/* Save mbuf */
	txq->sw_ring[idx] = mbuf;
	/* Update current index*/
	txq->cur++;
	/* Update stats */
	txq->bytes += mbuf->pkt_len;

	return 0;
}

/* Eal supported tx wrapper*/
uint16_t
axgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	PMD_INIT_FUNC_TRACE();

	if (unlikely(nb_pkts == 0))
		return nb_pkts;

	struct axgbe_tx_queue *txq;
	uint16_t nb_desc_free;
	uint16_t nb_pkt_sent = 0;
	uint16_t idx;
	uint32_t tail_addr;
	struct rte_mbuf *mbuf;

	txq  = (struct axgbe_tx_queue *)tx_queue;
	nb_desc_free = txq->nb_desc - (txq->cur - txq->dirty);

	if (unlikely(nb_desc_free <= txq->free_thresh)) {
		axgbe_xmit_cleanup(txq);
		nb_desc_free = txq->nb_desc - (txq->cur - txq->dirty);
		if (unlikely(nb_desc_free == 0))
			return 0;
	}
	nb_pkts = RTE_MIN(nb_desc_free, nb_pkts);
	while (nb_pkts--) {
		mbuf = *tx_pkts++;
		if (axgbe_xmit_hw(txq, mbuf))
			goto out;
		nb_pkt_sent++;
	}
out:
	/* Sync read and write */
	rte_mb();
	idx = AXGBE_GET_DESC_IDX(txq, txq->cur);
	tail_addr = low32_value(txq->ring_phys_addr +
				idx * sizeof(struct axgbe_tx_desc));
	/* Update tail reg with next immediate address to kick Tx DMA channel*/
	AXGMAC_DMA_IOWRITE(txq, DMA_CH_TDTR_LO, tail_addr);
	txq->pkts += nb_pkt_sent;
	return nb_pkt_sent;
}

void axgbe_dev_clear_queues(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	uint8_t i;
	struct axgbe_rx_queue *rxq;
	struct axgbe_tx_queue *txq;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		if (rxq) {
			axgbe_rx_queue_release(rxq);
			dev->data->rx_queues[i] = NULL;
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		if (txq) {
			axgbe_tx_queue_release(txq);
			dev->data->tx_queues[i] = NULL;
		}
	}
}

int
axgbe_dev_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct axgbe_rx_queue *rxq = rx_queue;
	volatile union axgbe_rx_desc *desc;
	uint16_t idx;


	if (unlikely(offset >= rxq->nb_desc))
		return -EINVAL;

	if (offset >= rxq->nb_desc - rxq->dirty)
		return RTE_ETH_RX_DESC_UNAVAIL;

	idx = AXGBE_GET_DESC_IDX(rxq, rxq->cur);
	desc = &rxq->desc[idx + offset];

	if (!AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, OWN))
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
axgbe_dev_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct axgbe_tx_queue *txq = tx_queue;
	volatile struct axgbe_tx_desc *desc;
	uint16_t idx;


	if (unlikely(offset >= txq->nb_desc))
		return -EINVAL;

	if (offset >= txq->nb_desc - txq->dirty)
		return RTE_ETH_TX_DESC_UNAVAIL;

	idx = AXGBE_GET_DESC_IDX(txq, txq->dirty + txq->free_batch_cnt - 1);
	desc = &txq->desc[idx + offset];

	if (!AXGMAC_GET_BITS_LE(desc->desc3, TX_NORMAL_DESC3, OWN))
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}
