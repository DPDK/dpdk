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

void axgbe_dev_rx_queue_release(void *rxq)
{
	axgbe_rx_queue_release(rxq);
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
	rxq->dma_regs = pdata->xgmac_regs + DMA_CH_BASE +
		(DMA_CH_INC * rxq->queue_id);
	rxq->dma_tail_reg = (volatile uint32_t *)(rxq->dma_regs +
						  DMA_CH_RDTR_LO);
	rxq->crc_len = (uint8_t)((dev->data->dev_conf.rxmode.offloads &
			DEV_RX_OFFLOAD_CRC_STRIP) ? 0 : ETHER_CRC_LEN);

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
	rxq->ring_phys_addr = (uint64_t)dma->phys_addr;
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

void axgbe_dev_tx_queue_release(void *txq)
{
	axgbe_tx_queue_release(txq);
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

	tx_desc = nb_desc;
	pdata = (struct axgbe_port *)dev->data->dev_private;

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

	txq->nb_desc = tx_desc;
	txq->free_thresh = tx_conf->tx_free_thresh ?
		tx_conf->tx_free_thresh : AXGBE_TX_FREE_THRESH;
	if (txq->free_thresh > txq->nb_desc)
		txq->free_thresh = (txq->nb_desc >> 1);
	txq->free_batch_cnt = txq->free_thresh;

	if ((tx_conf->txq_flags & (uint32_t)ETH_TXQ_FLAGS_NOOFFLOADS) !=
	    ETH_TXQ_FLAGS_NOOFFLOADS) {
		txq->vector_disable = 1;
	}

	/* Allocate TX ring hardware descriptors */
	tsize = txq->nb_desc * sizeof(struct axgbe_tx_desc);
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
				      tsize, AXGBE_DESC_ALIGN, socket_id);
	if (!tz) {
		axgbe_tx_queue_release(txq);
		return -ENOMEM;
	}
	memset(tz->addr, 0, tsize);
	txq->ring_phys_addr = (uint64_t)tz->phys_addr;
	txq->desc = tz->addr;
	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->dma_regs = pdata->xgmac_regs + DMA_CH_BASE +
		(DMA_CH_INC * txq->queue_id);
	txq->dma_tail_reg = (volatile uint32_t *)(txq->dma_regs +
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

	return 0;
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
