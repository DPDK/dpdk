/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>

#include "atl_ethdev.h"
#include "atl_hw_regs.h"

#include "atl_logs.h"
#include "hw_atl/hw_atl_llh.h"
#include "hw_atl/hw_atl_b0.h"
#include "hw_atl/hw_atl_b0_internal.h"

/**
 * Structure associated with each descriptor of the RX ring of a RX queue.
 */
struct atl_rx_entry {
	struct rte_mbuf *mbuf;
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
		atl_rx_queue_release(dev->data->rx_queues[rx_queue_id]);
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
		DEV_RX_OFFLOAD_IPV4_CKSUM;
	rxq->l4_csum_enabled = dev->data->dev_conf.rxmode.offloads &
		(DEV_RX_OFFLOAD_UDP_CKSUM | DEV_RX_OFFLOAD_TCP_CKSUM);
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_KEEP_CRC)
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

int
atl_tx_init(struct rte_eth_dev *eth_dev __rte_unused)
{
	return 0;
}

int
atl_rx_init(struct rte_eth_dev *eth_dev)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
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
atl_rx_queue_release(void *rx_queue)
{
	PMD_INIT_FUNC_TRACE();

	if (rx_queue != NULL) {
		struct atl_rx_queue *rxq = (struct atl_rx_queue *)rx_queue;

		atl_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

uint16_t
atl_prep_pkts(void *tx_queue __rte_unused,
	      struct rte_mbuf **tx_pkts __rte_unused,
	      uint16_t nb_pkts __rte_unused)
{
	return 0;
}

void
atl_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		atl_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = 0;
	}
	dev->data->nb_rx_queues = 0;
}

int
atl_start_queues(struct rte_eth_dev *dev)
{
	int i;

	PMD_INIT_FUNC_TRACE();

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
			mbuf_flags |= PKT_RX_IP_CKSUM_BAD;
		else
			mbuf_flags |= PKT_RX_IP_CKSUM_GOOD;
	} else {
		mbuf_flags |= PKT_RX_IP_CKSUM_UNKNOWN;
	}

	/* CSUM calculated ? */
	if (rxq->l4_csum_enabled && (rxd_wb->rx_stat & BIT(3))) {
		if (rxd_wb->rx_stat & BIT(2))
			mbuf_flags |= PKT_RX_L4_CKSUM_BAD;
		else
			mbuf_flags |= PKT_RX_L4_CKSUM_GOOD;
	} else {
		mbuf_flags |= PKT_RX_L4_CKSUM_UNKNOWN;
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

		PMD_RX_LOG(ERR, "port_id=%u queue_id=%u tail=%u "
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
				PMD_RX_LOG(ERR,
				   "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", (unsigned int)rxq->port_id,
				   (unsigned int)rxq->queue_id);
				dev->data->rx_mbuf_alloc_failed++;
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

		PMD_RX_LOG(ERR, "add mbuf segs=%d pkt_len=%d",
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
	 * RDH register, which creates a "full" ring situtation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(ERR, "port_id=%u queue_id=%u rx_tail=%u "
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


uint16_t
atl_xmit_pkts(void *tx_queue __rte_unused,
	      struct rte_mbuf **tx_pkts __rte_unused,
	      uint16_t nb_pkts __rte_unused)
{
	return 0;
}

