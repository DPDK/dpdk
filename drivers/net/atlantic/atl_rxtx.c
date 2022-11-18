/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <rte_net.h>

#include "atl_ethdev.h"
#include "atl_hw_regs.h"

#include "atl_logs.h"
#include "hw_atl/hw_atl_llh.h"
#include "hw_atl/hw_atl_b0.h"
#include "hw_atl/hw_atl_b0_internal.h"

#define ATL_TX_CKSUM_OFFLOAD_MASK (			 \
	RTE_MBUF_F_TX_IP_CKSUM |				 \
	RTE_MBUF_F_TX_L4_MASK |				 \
	RTE_MBUF_F_TX_TCP_SEG)

#define ATL_TX_OFFLOAD_MASK (				 \
	RTE_MBUF_F_TX_VLAN |					 \
	RTE_MBUF_F_TX_IPV6 |					 \
	RTE_MBUF_F_TX_IPV4 |					 \
	RTE_MBUF_F_TX_IP_CKSUM |				 \
	RTE_MBUF_F_TX_L4_MASK |				 \
	RTE_MBUF_F_TX_TCP_SEG)

#define ATL_TX_OFFLOAD_NOTSUP_MASK \
	(RTE_MBUF_F_TX_OFFLOAD_MASK ^ ATL_TX_OFFLOAD_MASK)

/**
 * Structure associated with each descriptor of the RX ring of a RX queue.
 */
struct atl_rx_entry {
	struct rte_mbuf *mbuf;
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct atl_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t next_id;
	uint16_t last_id;
};

/**
 * Structure associated with each RX queue.
 */
struct atl_rx_queue {
	struct rte_mempool	*mb_pool;
	struct hw_atl_rxd_s	*hw_ring;
	uint64_t		hw_ring_phys_addr;
	struct atl_rx_entry	*sw_ring;
	uint16_t		nb_rx_desc;
	uint16_t		rx_tail;
	uint16_t		nb_rx_hold;
	uint16_t		rx_free_thresh;
	uint16_t		queue_id;
	uint16_t		port_id;
	uint16_t		buff_size;
	bool			l3_csum_enabled;
	bool			l4_csum_enabled;
};

/**
 * Structure associated with each TX queue.
 */
struct atl_tx_queue {
	struct hw_atl_txd_s	*hw_ring;
	uint64_t		hw_ring_phys_addr;
	struct atl_tx_entry	*sw_ring;
	uint16_t		nb_tx_desc;
	uint16_t		tx_tail;
	uint16_t		tx_head;
	uint16_t		queue_id;
	uint16_t		port_id;
	uint16_t		tx_free_thresh;
	uint16_t		tx_free;
};

static inline void
atl_reset_rx_queue(struct atl_rx_queue *rxq)
{
	struct hw_atl_rxd_s *rxd = NULL;
	int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		rxd = (struct hw_atl_rxd_s *)&rxq->hw_ring[i];
		rxd->buf_addr = 0;
		rxd->hdr_addr = 0;
	}

	rxq->rx_tail = 0;
}

int
atl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		   uint16_t nb_rx_desc, unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf,
		   struct rte_mempool *mb_pool)
{
	struct atl_rx_queue *rxq;
	const struct rte_memzone *mz;

	PMD_INIT_FUNC_TRACE();

	/* make sure a valid number of descriptors have been requested */
	if (nb_rx_desc < AQ_HW_MIN_RX_RING_SIZE ||
			nb_rx_desc > AQ_HW_MAX_RX_RING_SIZE) {
		PMD_INIT_LOG(ERR, "Number of Rx descriptors must be "
		"less than or equal to %d, "
		"greater than or equal to %d", AQ_HW_MAX_RX_RING_SIZE,
		AQ_HW_MIN_RX_RING_SIZE);
		return -EINVAL;
	}

	/*
	 * if this queue existed already, free the associated memory. The
	 * queue cannot be reused in case we need to allocate memory on
	 * different socket than was previously used.
	 */
	if (dev->data->rx_queues[rx_queue_id] != NULL) {
		atl_rx_queue_release(dev, rx_queue_id);
		dev->data->rx_queues[rx_queue_id] = NULL;
	}

	/* allocate memory for the queue structure */
	rxq = rte_zmalloc_socket("atlantic Rx queue", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate queue structure");
		return -ENOMEM;
	}

	/* setup queue */
	rxq->mb_pool = mb_pool;
	rxq->nb_rx_desc = nb_rx_desc;
	rxq->port_id = dev->data->port_id;
	rxq->queue_id = rx_queue_id;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;

	rxq->l3_csum_enabled = dev->data->dev_conf.rxmode.offloads &
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
	rxq->l4_csum_enabled = dev->data->dev_conf.rxmode.offloads &
		(RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM);
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		PMD_DRV_LOG(ERR, "PMD does not support KEEP_CRC offload");

	/* allocate memory for the software ring */
	rxq->sw_ring = rte_zmalloc_socket("atlantic sw rx ring",
				nb_rx_desc * sizeof(struct atl_rx_entry),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR,
			"Port %d: Cannot allocate software ring for queue %d",
			rxq->port_id, rxq->queue_id);
		rte_free(rxq);
		return -ENOMEM;
	}

	/*
	 * allocate memory for the hardware descriptor ring. A memzone large
	 * enough to hold the maximum ring size is requested to allow for
	 * resizing in later calls to the queue setup function.
	 */
	mz = rte_eth_dma_zone_reserve(dev, "rx hw_ring", rx_queue_id,
				      HW_ATL_B0_MAX_RXD *
					sizeof(struct hw_atl_rxd_s),
				      128, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR,
			"Port %d: Cannot allocate hardware ring for queue %d",
			rxq->port_id, rxq->queue_id);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
		return -ENOMEM;
	}
	rxq->hw_ring = mz->addr;
	rxq->hw_ring_phys_addr = mz->iova;

	atl_reset_rx_queue(rxq);

	dev->data->rx_queues[rx_queue_id] = rxq;
	return 0;
}

static inline void
atl_reset_tx_queue(struct atl_tx_queue *txq)
{
	struct atl_tx_entry *tx_entry;
	union hw_atl_txc_s *txc;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	if (!txq) {
		PMD_DRV_LOG(ERR, "Pointer to txq is NULL");
		return;
	}

	tx_entry = txq->sw_ring;

	for (i = 0; i < txq->nb_tx_desc; i++) {
		txc = (union hw_atl_txc_s *)&txq->hw_ring[i];
		txc->flags1 = 0;
		txc->flags2 = 2;
	}

	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->hw_ring[i].dd = 1;
		tx_entry[i].mbuf = NULL;
	}

	txq->tx_tail = 0;
	txq->tx_head = 0;
	txq->tx_free = txq->nb_tx_desc - 1;
}

int
atl_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		   uint16_t nb_tx_desc, unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf)
{
	struct atl_tx_queue *txq;
	const struct rte_memzone *mz;

	PMD_INIT_FUNC_TRACE();

	/* make sure a valid number of descriptors have been requested */
	if (nb_tx_desc < AQ_HW_MIN_TX_RING_SIZE ||
		nb_tx_desc > AQ_HW_MAX_TX_RING_SIZE) {
		PMD_INIT_LOG(ERR, "Number of Tx descriptors must be "
			"less than or equal to %d, "
			"greater than or equal to %d", AQ_HW_MAX_TX_RING_SIZE,
			AQ_HW_MIN_TX_RING_SIZE);
		return -EINVAL;
	}

	/*
	 * if this queue existed already, free the associated memory. The
	 * queue cannot be reused in case we need to allocate memory on
	 * different socket than was previously used.
	 */
	if (dev->data->tx_queues[tx_queue_id] != NULL) {
		atl_tx_queue_release(dev, tx_queue_id);
		dev->data->tx_queues[tx_queue_id] = NULL;
	}

	/* allocate memory for the queue structure */
	txq = rte_zmalloc_socket("atlantic Tx queue", sizeof(*txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate queue structure");
		return -ENOMEM;
	}

	/* setup queue */
	txq->nb_tx_desc = nb_tx_desc;
	txq->port_id = dev->data->port_id;
	txq->queue_id = tx_queue_id;
	txq->tx_free_thresh = tx_conf->tx_free_thresh;


	/* allocate memory for the software ring */
	txq->sw_ring = rte_zmalloc_socket("atlantic sw tx ring",
				nb_tx_desc * sizeof(struct atl_tx_entry),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR,
			"Port %d: Cannot allocate software ring for queue %d",
			txq->port_id, txq->queue_id);
		rte_free(txq);
		return -ENOMEM;
	}

	/*
	 * allocate memory for the hardware descriptor ring. A memzone large
	 * enough to hold the maximum ring size is requested to allow for
	 * resizing in later calls to the queue setup function.
	 */
	mz = rte_eth_dma_zone_reserve(dev, "tx hw_ring", tx_queue_id,
				HW_ATL_B0_MAX_TXD * sizeof(struct hw_atl_txd_s),
				128, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR,
			"Port %d: Cannot allocate hardware ring for queue %d",
			txq->port_id, txq->queue_id);
		rte_free(txq->sw_ring);
		rte_free(txq);
		return -ENOMEM;
	}
	txq->hw_ring = mz->addr;
	txq->hw_ring_phys_addr = mz->iova;

	atl_reset_tx_queue(txq);

	dev->data->tx_queues[tx_queue_id] = txq;
	return 0;
}

int
atl_tx_init(struct rte_eth_dev *eth_dev)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct atl_tx_queue *txq;
	uint64_t base_addr = 0;
	int i = 0;
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		base_addr = txq->hw_ring_phys_addr;

		err = hw_atl_b0_hw_ring_tx_init(hw, base_addr,
						txq->queue_id,
						txq->nb_tx_desc, 0,
						txq->port_id);

		if (err) {
			PMD_INIT_LOG(ERR,
				"Port %d: Cannot init TX queue %d",
				txq->port_id, txq->queue_id);
			break;
		}
	}

	return err;
}

int
atl_rx_init(struct rte_eth_dev *eth_dev)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct aq_rss_parameters *rss_params = &hw->aq_nic_cfg->aq_rss;
	struct atl_rx_queue *rxq;
	uint64_t base_addr = 0;
	int i = 0;
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		base_addr = rxq->hw_ring_phys_addr;

		/* Take requested pool mbuf size and adapt
		 * descriptor buffer to best fit
		 */
		int buff_size = rte_pktmbuf_data_room_size(rxq->mb_pool) -
				RTE_PKTMBUF_HEADROOM;

		buff_size = RTE_ALIGN_FLOOR(buff_size, 1024);
		if (buff_size > HW_ATL_B0_RXD_BUF_SIZE_MAX) {
			PMD_INIT_LOG(WARNING,
				"Port %d queue %d: mem pool buff size is too big\n",
				rxq->port_id, rxq->queue_id);
			buff_size = HW_ATL_B0_RXD_BUF_SIZE_MAX;
		}
		if (buff_size < 1024) {
			PMD_INIT_LOG(ERR,
				"Port %d queue %d: mem pool buff size is too small\n",
				rxq->port_id, rxq->queue_id);
			return -EINVAL;
		}
		rxq->buff_size = buff_size;

		err = hw_atl_b0_hw_ring_rx_init(hw, base_addr, rxq->queue_id,
						rxq->nb_rx_desc, buff_size, 0,
						rxq->port_id);

		if (err) {
			PMD_INIT_LOG(ERR, "Port %d: Cannot init RX queue %d",
				     rxq->port_id, rxq->queue_id);
			break;
		}
	}

	for (i = rss_params->indirection_table_size; i--;)
		rss_params->indirection_table[i] = i &
			(eth_dev->data->nb_rx_queues - 1);
	hw_atl_b0_hw_rss_set(hw, rss_params);
	return err;
}

static int
atl_alloc_rx_queue_mbufs(struct atl_rx_queue *rxq)
{
	struct atl_rx_entry *rx_entry = rxq->sw_ring;
	struct hw_atl_rxd_s *rxd;
	uint64_t dma_addr = 0;
	uint32_t i = 0;

	PMD_INIT_FUNC_TRACE();

	/* fill Rx ring */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR,
				"Port %d: mbuf alloc failed for rx queue %d",
				rxq->port_id, rxq->queue_id);
			return -ENOMEM;
		}

		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->port = rxq->port_id;

		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxd = (struct hw_atl_rxd_s *)&rxq->hw_ring[i];
		rxd->buf_addr = dma_addr;
		rxd->hdr_addr = 0;
		rx_entry[i].mbuf = mbuf;
	}

	return 0;
}

static void
atl_rx_queue_release_mbufs(struct atl_rx_queue *rxq)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	if (rxq->sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

int
atl_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct atl_rx_queue *rxq = NULL;

	PMD_INIT_FUNC_TRACE();

	if (rx_queue_id < dev->data->nb_rx_queues) {
		rxq = dev->data->rx_queues[rx_queue_id];

		if (atl_alloc_rx_queue_mbufs(rxq) != 0) {
			PMD_INIT_LOG(ERR,
				"Port %d: Allocate mbufs for queue %d failed",
				rxq->port_id, rxq->queue_id);
			return -1;
		}

		hw_atl_b0_hw_ring_rx_start(hw, rx_queue_id);

		rte_wmb();
		hw_atl_reg_rx_dma_desc_tail_ptr_set(hw, rxq->nb_rx_desc - 1,
						    rx_queue_id);
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
	} else {
		return -1;
	}

	return 0;
}

int
atl_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct atl_rx_queue *rxq = NULL;

	PMD_INIT_FUNC_TRACE();

	if (rx_queue_id < dev->data->nb_rx_queues) {
		rxq = dev->data->rx_queues[rx_queue_id];

		hw_atl_b0_hw_ring_rx_stop(hw, rx_queue_id);

		atl_rx_queue_release_mbufs(rxq);
		atl_reset_rx_queue(rxq);

		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STOPPED;
	} else {
		return -1;
	}

	return 0;
}

void
atl_rx_queue_release(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct atl_rx_queue *rxq = dev->data->rx_queues[rx_queue_id];

	PMD_INIT_FUNC_TRACE();

	if (rxq != NULL) {
		atl_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

static void
atl_tx_queue_release_mbufs(struct atl_tx_queue *txq)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	if (txq->sw_ring != NULL) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

int
atl_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	if (tx_queue_id < dev->data->nb_tx_queues) {
		hw_atl_b0_hw_ring_tx_start(hw, tx_queue_id);

		rte_wmb();
		hw_atl_b0_hw_tx_ring_tail_update(hw, 0, tx_queue_id);
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
	} else {
		return -1;
	}

	return 0;
}

int
atl_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct atl_tx_queue *txq;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];

	hw_atl_b0_hw_ring_tx_stop(hw, tx_queue_id);

	atl_tx_queue_release_mbufs(txq);
	atl_reset_tx_queue(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
atl_tx_queue_release(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct atl_tx_queue *txq = dev->data->tx_queues[tx_queue_id];

	PMD_INIT_FUNC_TRACE();

	if (txq != NULL) {
		atl_tx_queue_release_mbufs(txq);
		rte_free(txq->sw_ring);
		rte_free(txq);
	}
}

void
atl_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		atl_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = 0;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		atl_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = 0;
	}
	dev->data->nb_tx_queues = 0;
}

int
atl_start_queues(struct rte_eth_dev *dev)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (atl_tx_queue_start(dev, i) != 0) {
			PMD_DRV_LOG(ERR,
				"Port %d: Start Tx queue %d failed",
				dev->data->port_id, i);
			return -1;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (atl_rx_queue_start(dev, i) != 0) {
			PMD_DRV_LOG(ERR,
				"Port %d: Start Rx queue %d failed",
				dev->data->port_id, i);
			return -1;
		}
	}

	return 0;
}

int
atl_stop_queues(struct rte_eth_dev *dev)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (atl_tx_queue_stop(dev, i) != 0) {
			PMD_DRV_LOG(ERR,
				"Port %d: Stop Tx queue %d failed",
				dev->data->port_id, i);
			return -1;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (atl_rx_queue_stop(dev, i) != 0) {
			PMD_DRV_LOG(ERR,
				"Port %d: Stop Rx queue %d failed",
				dev->data->port_id, i);
			return -1;
		}
	}

	return 0;
}

void
atl_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		 struct rte_eth_rxq_info *qinfo)
{
	struct atl_rx_queue *rxq;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;
}

void
atl_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		 struct rte_eth_txq_info *qinfo)
{
	struct atl_tx_queue *txq;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;
}

/* Return Rx queue avail count */

uint32_t
atl_rx_queue_count(void *rx_queue)
{
	struct atl_rx_queue *rxq;

	PMD_INIT_FUNC_TRACE();

	rxq = rx_queue;

	if (rxq == NULL)
		return 0;

	return rxq->nb_rx_desc - rxq->nb_rx_hold;
}

int
atl_dev_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct atl_rx_queue *rxq = rx_queue;
	struct hw_atl_rxd_wb_s *rxd;
	uint32_t idx;

	PMD_INIT_FUNC_TRACE();

	if (unlikely(offset >= rxq->nb_rx_desc))
		return -EINVAL;

	if (offset >= rxq->nb_rx_desc - rxq->nb_rx_hold)
		return RTE_ETH_RX_DESC_UNAVAIL;

	idx = rxq->rx_tail + offset;

	if (idx >= rxq->nb_rx_desc)
		idx -= rxq->nb_rx_desc;

	rxd = (struct hw_atl_rxd_wb_s *)&rxq->hw_ring[idx];

	if (rxd->dd)
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
atl_dev_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct atl_tx_queue *txq = tx_queue;
	struct hw_atl_txd_s *txd;
	uint32_t idx;

	PMD_INIT_FUNC_TRACE();

	if (unlikely(offset >= txq->nb_tx_desc))
		return -EINVAL;

	idx = txq->tx_tail + offset;

	if (idx >= txq->nb_tx_desc)
		idx -= txq->nb_tx_desc;

	txd = &txq->hw_ring[idx];

	if (txd->dd)
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

static int
atl_rx_enable_intr(struct rte_eth_dev *dev, uint16_t queue_id, bool enable)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct atl_rx_queue *rxq;

	PMD_INIT_FUNC_TRACE();

	if (queue_id >= dev->data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid RX queue id=%d", queue_id);
		return -EINVAL;
	}

	rxq = dev->data->rx_queues[queue_id];

	if (rxq == NULL)
		return 0;

	/* Mapping interrupt vector */
	hw_atl_itr_irq_map_en_rx_set(hw, enable, queue_id);

	return 0;
}

int
atl_dev_rx_queue_intr_enable(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	return atl_rx_enable_intr(eth_dev, queue_id, true);
}

int
atl_dev_rx_queue_intr_disable(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	return atl_rx_enable_intr(eth_dev, queue_id, false);
}

uint16_t
atl_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	      uint16_t nb_pkts)
{
	int i, ret;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		if (m->nb_segs > AQ_HW_MAX_SEGS_SIZE) {
			rte_errno = EINVAL;
			return i;
		}

		if (ol_flags & ATL_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = ENOTSUP;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
	}

	return i;
}

static uint64_t
atl_desc_to_offload_flags(struct atl_rx_queue *rxq,
			  struct hw_atl_rxd_wb_s *rxd_wb)
{
	uint64_t mbuf_flags = 0;

	PMD_INIT_FUNC_TRACE();

	/* IPv4 ? */
	if (rxq->l3_csum_enabled && ((rxd_wb->pkt_type & 0x3) == 0)) {
		/* IPv4 csum error ? */
		if (rxd_wb->rx_stat & BIT(1))
			mbuf_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		else
			mbuf_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
	} else {
		mbuf_flags |= RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;
	}

	/* CSUM calculated ? */
	if (rxq->l4_csum_enabled && (rxd_wb->rx_stat & BIT(3))) {
		if (rxd_wb->rx_stat & BIT(2))
			mbuf_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
		else
			mbuf_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	} else {
		mbuf_flags |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;
	}

	return mbuf_flags;
}

static uint32_t
atl_desc_to_pkt_type(struct hw_atl_rxd_wb_s *rxd_wb)
{
	uint32_t type = RTE_PTYPE_UNKNOWN;
	uint16_t l2_l3_type = rxd_wb->pkt_type & 0x3;
	uint16_t l4_type = (rxd_wb->pkt_type & 0x1C) >> 2;

	switch (l2_l3_type) {
	case 0:
		type = RTE_PTYPE_L3_IPV4;
		break;
	case 1:
		type = RTE_PTYPE_L3_IPV6;
		break;
	case 2:
		type = RTE_PTYPE_L2_ETHER;
		break;
	case 3:
		type = RTE_PTYPE_L2_ETHER_ARP;
		break;
	}

	switch (l4_type) {
	case 0:
		type |= RTE_PTYPE_L4_TCP;
		break;
	case 1:
		type |= RTE_PTYPE_L4_UDP;
		break;
	case 2:
		type |= RTE_PTYPE_L4_SCTP;
		break;
	case 3:
		type |= RTE_PTYPE_L4_ICMP;
		break;
	}

	if (rxd_wb->pkt_type & BIT(5))
		type |= RTE_PTYPE_L2_ETHER_VLAN;

	return type;
}

uint16_t
atl_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct atl_rx_queue *rxq = (struct atl_rx_queue *)rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	struct atl_adapter *adapter =
		ATL_DEV_TO_ADAPTER(&rte_eth_devices[rxq->port_id]);
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(adapter);
	struct aq_hw_cfg_s *cfg =
		ATL_DEV_PRIVATE_TO_CFG(dev->data->dev_private);
	struct atl_rx_entry *sw_ring = rxq->sw_ring;

	struct rte_mbuf *new_mbuf;
	struct rte_mbuf *rx_mbuf, *rx_mbuf_prev, *rx_mbuf_first;
	struct atl_rx_entry *rx_entry;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;
	struct hw_atl_rxd_wb_s rxd_wb;
	struct hw_atl_rxd_s *rxd = NULL;
	uint16_t tail = rxq->rx_tail;
	uint64_t dma_addr;
	uint16_t pkt_len = 0;

	while (nb_rx < nb_pkts) {
		uint16_t eop_tail = tail;

		rxd = (struct hw_atl_rxd_s *)&rxq->hw_ring[tail];
		rxd_wb = *(struct hw_atl_rxd_wb_s *)rxd;

		if (!rxd_wb.dd) { /* RxD is not done */
			break;
		}

		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u tail=%u "
			   "eop=0x%x pkt_len=%u hash=0x%x hash_type=0x%x",
			   (unsigned int)rxq->port_id,
			   (unsigned int)rxq->queue_id,
			   (unsigned int)tail, (unsigned int)rxd_wb.eop,
			   (unsigned int)rte_le_to_cpu_16(rxd_wb.pkt_len),
			rxd_wb.rss_hash, rxd_wb.rss_type);

		/* RxD is not done */
		if (!rxd_wb.eop) {
			while (true) {
				struct hw_atl_rxd_wb_s *eop_rxwbd;

				eop_tail = (eop_tail + 1) % rxq->nb_rx_desc;
				eop_rxwbd = (struct hw_atl_rxd_wb_s *)
					&rxq->hw_ring[eop_tail];
				if (!eop_rxwbd->dd) {
					/* no EOP received yet */
					eop_tail = tail;
					break;
				}
				if (eop_rxwbd->dd && eop_rxwbd->eop)
					break;
			}
			/* No EOP in ring */
			if (eop_tail == tail)
				break;
		}
		rx_mbuf_prev = NULL;
		rx_mbuf_first = NULL;

		/* Run through packet segments */
		while (true) {
			new_mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (new_mbuf == NULL) {
				PMD_RX_LOG(DEBUG,
				   "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", (unsigned int)rxq->port_id,
				   (unsigned int)rxq->queue_id);
				dev->data->rx_mbuf_alloc_failed++;
				adapter->sw_stats.rx_nombuf++;
				goto err_stop;
			}

			nb_hold++;
			rx_entry = &sw_ring[tail];

			rx_mbuf = rx_entry->mbuf;
			rx_entry->mbuf = new_mbuf;
			dma_addr = rte_cpu_to_le_64(
				rte_mbuf_data_iova_default(new_mbuf));

			/* setup RX descriptor */
			rxd->hdr_addr = 0;
			rxd->buf_addr = dma_addr;

			/*
			 * Initialize the returned mbuf.
			 * 1) setup generic mbuf fields:
			 *	  - number of segments,
			 *	  - next segment,
			 *	  - packet length,
			 *	  - RX port identifier.
			 * 2) integrate hardware offload data, if any:
			 *	<  - RSS flag & hash,
			 *	  - IP checksum flag,
			 *	  - VLAN TCI, if any,
			 *	  - error flags.
			 */
			pkt_len = (uint16_t)rte_le_to_cpu_16(rxd_wb.pkt_len);
			rx_mbuf->data_off = RTE_PKTMBUF_HEADROOM;
			rte_prefetch1((char *)rx_mbuf->buf_addr +
				rx_mbuf->data_off);
			rx_mbuf->nb_segs = 0;
			rx_mbuf->next = NULL;
			rx_mbuf->pkt_len = pkt_len;
			rx_mbuf->data_len = pkt_len;
			if (rxd_wb.eop) {
				u16 remainder_len = pkt_len % rxq->buff_size;
				if (!remainder_len)
					remainder_len = rxq->buff_size;
				rx_mbuf->data_len = remainder_len;
			} else {
				rx_mbuf->data_len = pkt_len > rxq->buff_size ?
						rxq->buff_size : pkt_len;
			}
			rx_mbuf->port = rxq->port_id;

			rx_mbuf->hash.rss = rxd_wb.rss_hash;

			rx_mbuf->vlan_tci = rxd_wb.vlan;

			rx_mbuf->ol_flags =
				atl_desc_to_offload_flags(rxq, &rxd_wb);

			rx_mbuf->packet_type = atl_desc_to_pkt_type(&rxd_wb);

			if (rx_mbuf->packet_type & RTE_PTYPE_L2_ETHER_VLAN) {
				rx_mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN;
				rx_mbuf->vlan_tci = rxd_wb.vlan;

				if (cfg->vlan_strip)
					rx_mbuf->ol_flags |=
						RTE_MBUF_F_RX_VLAN_STRIPPED;
			}

			if (!rx_mbuf_first)
				rx_mbuf_first = rx_mbuf;
			rx_mbuf_first->nb_segs++;

			if (rx_mbuf_prev)
				rx_mbuf_prev->next = rx_mbuf;
			rx_mbuf_prev = rx_mbuf;

			tail = (tail + 1) % rxq->nb_rx_desc;
			/* Prefetch next mbufs */
			rte_prefetch0(sw_ring[tail].mbuf);
			if ((tail & 0x3) == 0) {
				rte_prefetch0(&sw_ring[tail]);
				rte_prefetch0(&sw_ring[tail]);
			}

			/* filled mbuf_first */
			if (rxd_wb.eop)
				break;
			rxd = (struct hw_atl_rxd_s *)&rxq->hw_ring[tail];
			rxd_wb = *(struct hw_atl_rxd_wb_s *)rxd;
		};

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = rx_mbuf_first;
		adapter->sw_stats.q_ipackets[rxq->queue_id]++;
		adapter->sw_stats.q_ibytes[rxq->queue_id] +=
			rx_mbuf_first->pkt_len;

		PMD_RX_LOG(DEBUG, "add mbuf segs=%d pkt_len=%d",
			rx_mbuf_first->nb_segs,
			rx_mbuf_first->pkt_len);
	}

err_stop:

	rxq->rx_tail = tail;

	/*
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed RX descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			"nb_hold=%u nb_rx=%u",
			(unsigned int)rxq->port_id, (unsigned int)rxq->queue_id,
			(unsigned int)tail, (unsigned int)nb_hold,
			(unsigned int)nb_rx);
		tail = (uint16_t)((tail == 0) ?
			(rxq->nb_rx_desc - 1) : (tail - 1));

		hw_atl_reg_rx_dma_desc_tail_ptr_set(hw, tail, rxq->queue_id);

		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

static void
atl_xmit_cleanup(struct atl_tx_queue *txq)
{
	struct atl_tx_entry *sw_ring;
	struct hw_atl_txd_s *txd;
	int to_clean = 0;

	if (txq != NULL) {
		sw_ring = txq->sw_ring;
		int head = txq->tx_head;
		int cnt = head;

		while (true) {
			txd = &txq->hw_ring[cnt];

			if (txd->dd)
				to_clean++;

			cnt = (cnt + 1) % txq->nb_tx_desc;
			if (cnt == txq->tx_tail)
				break;
		}

		if (to_clean == 0)
			return;

		while (to_clean) {
			txd = &txq->hw_ring[head];

			struct atl_tx_entry *rx_entry = &sw_ring[head];

			if (rx_entry->mbuf) {
				rte_pktmbuf_free_seg(rx_entry->mbuf);
				rx_entry->mbuf = NULL;
			}

			if (txd->dd)
				to_clean--;

			txd->buf_addr = 0;
			txd->flags = 0;

			head = (head + 1) % txq->nb_tx_desc;
			txq->tx_free++;
		}

		txq->tx_head = head;
	}
}

static int
atl_tso_setup(struct rte_mbuf *tx_pkt, union hw_atl_txc_s *txc)
{
	uint32_t tx_cmd = 0;
	uint64_t ol_flags = tx_pkt->ol_flags;

	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		tx_cmd |= tx_desc_cmd_lso | tx_desc_cmd_l4cs;

		txc->cmd = 0x4;

		if (ol_flags & RTE_MBUF_F_TX_IPV6)
			txc->cmd |= 0x2;

		txc->l2_len = tx_pkt->l2_len;
		txc->l3_len = tx_pkt->l3_len;
		txc->l4_len = tx_pkt->l4_len;

		txc->mss_len = tx_pkt->tso_segsz;
	}

	if (ol_flags & RTE_MBUF_F_TX_VLAN) {
		tx_cmd |= tx_desc_cmd_vlan;
		txc->vlan_tag = tx_pkt->vlan_tci;
	}

	if (tx_cmd) {
		txc->type = tx_desc_type_ctx;
		txc->idx = 0;
	}

	return tx_cmd;
}

static inline void
atl_setup_csum_offload(struct rte_mbuf *mbuf, struct hw_atl_txd_s *txd,
		       uint32_t tx_cmd)
{
	txd->cmd |= tx_desc_cmd_fcs;
	txd->cmd |= (mbuf->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) ? tx_desc_cmd_ipv4 : 0;
	/* L4 csum requested */
	txd->cmd |= (mbuf->ol_flags & RTE_MBUF_F_TX_L4_MASK) ? tx_desc_cmd_l4cs : 0;
	txd->cmd |= tx_cmd;
}

static inline void
atl_xmit_pkt(struct aq_hw_s *hw, struct atl_tx_queue *txq,
	     struct rte_mbuf *tx_pkt)
{
	struct atl_adapter *adapter =
		ATL_DEV_TO_ADAPTER(&rte_eth_devices[txq->port_id]);
	uint32_t pay_len = 0;
	int tail = 0;
	struct atl_tx_entry *tx_entry;
	uint64_t buf_dma_addr;
	struct rte_mbuf *m_seg;
	union hw_atl_txc_s *txc = NULL;
	struct hw_atl_txd_s *txd = NULL;
	u32 tx_cmd = 0U;
	int desc_count = 0;

	tail = txq->tx_tail;

	txc = (union hw_atl_txc_s *)&txq->hw_ring[tail];

	txc->flags1 = 0U;
	txc->flags2 = 0U;

	tx_cmd = atl_tso_setup(tx_pkt, txc);

	if (tx_cmd) {
		/* We've consumed the first desc, adjust counters */
		tail = (tail + 1) % txq->nb_tx_desc;
		txq->tx_tail = tail;
		txq->tx_free -= 1;

		txd = &txq->hw_ring[tail];
		txd->flags = 0U;
	} else {
		txd = (struct hw_atl_txd_s *)txc;
	}

	txd->ct_en = !!tx_cmd;

	txd->type = tx_desc_type_desc;

	atl_setup_csum_offload(tx_pkt, txd, tx_cmd);

	if (tx_cmd)
		txd->ct_idx = 0;

	pay_len = tx_pkt->pkt_len;

	txd->pay_len = pay_len;

	for (m_seg = tx_pkt; m_seg; m_seg = m_seg->next) {
		if (desc_count > 0) {
			txd = &txq->hw_ring[tail];
			txd->flags = 0U;
		}

		buf_dma_addr = rte_mbuf_data_iova(m_seg);
		txd->buf_addr = rte_cpu_to_le_64(buf_dma_addr);

		txd->type = tx_desc_type_desc;
		txd->len = m_seg->data_len;
		txd->pay_len = pay_len;

		/* Store mbuf for freeing later */
		tx_entry = &txq->sw_ring[tail];

		if (tx_entry->mbuf)
			rte_pktmbuf_free_seg(tx_entry->mbuf);
		tx_entry->mbuf = m_seg;

		tail = (tail + 1) % txq->nb_tx_desc;

		desc_count++;
	}

	// Last descriptor requires EOP and WB
	txd->eop = 1U;
	txd->cmd |= tx_desc_cmd_wb;

	hw_atl_b0_hw_tx_ring_tail_update(hw, tail, txq->queue_id);

	txq->tx_tail = tail;

	txq->tx_free -= desc_count;

	adapter->sw_stats.q_opackets[txq->queue_id]++;
	adapter->sw_stats.q_obytes[txq->queue_id] += pay_len;
}

uint16_t
atl_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_dev *dev = NULL;
	struct aq_hw_s *hw = NULL;
	struct atl_tx_queue *txq = tx_queue;
	struct rte_mbuf *tx_pkt;
	uint16_t nb_tx;

	dev = &rte_eth_devices[txq->port_id];
	hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_TX_LOG(DEBUG,
		"port %d txq %d pkts: %d tx_free=%d tx_tail=%d tx_head=%d",
		txq->port_id, txq->queue_id, nb_pkts, txq->tx_free,
		txq->tx_tail, txq->tx_head);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = *tx_pkts++;

		/* Clean Tx queue if needed */
		if (txq->tx_free < txq->tx_free_thresh)
			atl_xmit_cleanup(txq);

		/* Check if we have enough free descriptors */
		if (txq->tx_free < tx_pkt->nb_segs)
			break;

		/* check mbuf is valid */
		if ((tx_pkt->nb_segs == 0) ||
			((tx_pkt->nb_segs > 1) && (tx_pkt->next == NULL)))
			break;

		/* Send the packet */
		atl_xmit_pkt(hw, txq, tx_pkt);
	}

	PMD_TX_LOG(DEBUG, "atl_xmit_pkts %d transmitted", nb_tx);

	return nb_tx;
}
