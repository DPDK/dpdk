/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <sys/queue.h>

#include <stdint.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "ngbe_logs.h"
#include "base/ngbe.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"

/*
 * Prefetch a cache line into all cache levels.
 */
#define rte_ngbe_prefetch(p)   rte_prefetch0(p)

/*********************************************************************
 *
 *  Tx functions
 *
 **********************************************************************/

/*
 * Check for descriptors with their DD bit set and free mbufs.
 * Return the total number of buffers freed.
 */
static __rte_always_inline int
ngbe_tx_free_bufs(struct ngbe_tx_queue *txq)
{
	struct ngbe_tx_entry *txep;
	uint32_t status;
	int i, nb_free = 0;
	struct rte_mbuf *m, *free[RTE_NGBE_TX_MAX_FREE_BUF_SZ];

	/* check DD bit on threshold descriptor */
	status = txq->tx_ring[txq->tx_next_dd].dw3;
	if (!(status & rte_cpu_to_le_32(NGBE_TXD_DD))) {
		if (txq->nb_tx_free >> 1 < txq->tx_free_thresh)
			ngbe_set32_masked(txq->tdc_reg_addr,
				NGBE_TXCFG_FLUSH, NGBE_TXCFG_FLUSH);
		return 0;
	}

	/*
	 * first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_free_thresh-1)
	 */
	txep = &txq->sw_ring[txq->tx_next_dd - (txq->tx_free_thresh - 1)];
	for (i = 0; i < txq->tx_free_thresh; ++i, ++txep) {
		/* free buffers one at a time */
		m = rte_pktmbuf_prefree_seg(txep->mbuf);
		txep->mbuf = NULL;

		if (unlikely(m == NULL))
			continue;

		if (nb_free >= RTE_NGBE_TX_MAX_FREE_BUF_SZ ||
		    (nb_free > 0 && m->pool != free[0]->pool)) {
			rte_mempool_put_bulk(free[0]->pool,
					     (void **)free, nb_free);
			nb_free = 0;
		}

		free[nb_free++] = m;
	}

	if (nb_free > 0)
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);

	/* buffers were freed, update counters */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_free_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_free_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_free_thresh - 1);

	return txq->tx_free_thresh;
}

/* Populate 4 descriptors with data from 4 mbufs */
static inline void
tx4(volatile struct ngbe_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;
	int i;

	for (i = 0; i < 4; ++i, ++txdp, ++pkts) {
		buf_dma_addr = rte_mbuf_data_iova(*pkts);
		pkt_len = (*pkts)->data_len;

		/* write data to descriptor */
		txdp->qw0 = rte_cpu_to_le_64(buf_dma_addr);
		txdp->dw2 = cpu_to_le32(NGBE_TXD_FLAGS |
					NGBE_TXD_DATLEN(pkt_len));
		txdp->dw3 = cpu_to_le32(NGBE_TXD_PAYLEN(pkt_len));

		rte_prefetch0(&(*pkts)->pool);
	}
}

/* Populate 1 descriptor with data from 1 mbuf */
static inline void
tx1(volatile struct ngbe_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;

	buf_dma_addr = rte_mbuf_data_iova(*pkts);
	pkt_len = (*pkts)->data_len;

	/* write data to descriptor */
	txdp->qw0 = cpu_to_le64(buf_dma_addr);
	txdp->dw2 = cpu_to_le32(NGBE_TXD_FLAGS |
				NGBE_TXD_DATLEN(pkt_len));
	txdp->dw3 = cpu_to_le32(NGBE_TXD_PAYLEN(pkt_len));

	rte_prefetch0(&(*pkts)->pool);
}

/*
 * Fill H/W descriptor ring with mbuf data.
 * Copy mbuf pointers to the S/W ring.
 */
static inline void
ngbe_tx_fill_hw_ring(struct ngbe_tx_queue *txq, struct rte_mbuf **pkts,
		      uint16_t nb_pkts)
{
	volatile struct ngbe_tx_desc *txdp = &txq->tx_ring[txq->tx_tail];
	struct ngbe_tx_entry *txep = &txq->sw_ring[txq->tx_tail];
	const int N_PER_LOOP = 4;
	const int N_PER_LOOP_MASK = N_PER_LOOP - 1;
	int mainpart, leftover;
	int i, j;

	/*
	 * Process most of the packets in chunks of N pkts.  Any
	 * leftover packets will get processed one at a time.
	 */
	mainpart = (nb_pkts & ((uint32_t)~N_PER_LOOP_MASK));
	leftover = (nb_pkts & ((uint32_t)N_PER_LOOP_MASK));
	for (i = 0; i < mainpart; i += N_PER_LOOP) {
		/* Copy N mbuf pointers to the S/W ring */
		for (j = 0; j < N_PER_LOOP; ++j)
			(txep + i + j)->mbuf = *(pkts + i + j);
		tx4(txdp + i, pkts + i);
	}

	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; ++i) {
			(txep + mainpart + i)->mbuf = *(pkts + mainpart + i);
			tx1(txdp + mainpart + i, pkts + mainpart + i);
		}
	}
}

static inline uint16_t
tx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	     uint16_t nb_pkts)
{
	struct ngbe_tx_queue *txq = (struct ngbe_tx_queue *)tx_queue;
	uint16_t n = 0;

	/*
	 * Begin scanning the H/W ring for done descriptors when the
	 * number of available descriptors drops below tx_free_thresh.
	 * For each done descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		ngbe_tx_free_bufs(txq);

	/* Only use descriptors that are available */
	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	/* Use exactly nb_pkts descriptors */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	/*
	 * At this point, we know there are enough descriptors in the
	 * ring to transmit all the packets.  This assumes that each
	 * mbuf contains a single segment, and that no new offloads
	 * are expected, which would require a new context descriptor.
	 */

	/*
	 * See if we're going to wrap-around. If so, handle the top
	 * of the descriptor ring first, then do the bottom.  If not,
	 * the processing looks just like the "bottom" part anyway...
	 */
	if ((txq->tx_tail + nb_pkts) > txq->nb_tx_desc) {
		n = (uint16_t)(txq->nb_tx_desc - txq->tx_tail);
		ngbe_tx_fill_hw_ring(txq, tx_pkts, n);
		txq->tx_tail = 0;
	}

	/* Fill H/W descriptor ring with mbuf data */
	ngbe_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
	txq->tx_tail = (uint16_t)(txq->tx_tail + (nb_pkts - n));

	/*
	 * Check for wrap-around. This would only happen if we used
	 * up to the last descriptor in the ring, no more, no less.
	 */
	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;

	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   (uint16_t)txq->port_id, (uint16_t)txq->queue_id,
		   (uint16_t)txq->tx_tail, (uint16_t)nb_pkts);

	/* update tail pointer */
	rte_wmb();
	ngbe_set32_relaxed(txq->tdt_reg_addr, txq->tx_tail);

	return nb_pkts;
}

uint16_t
ngbe_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	uint16_t nb_tx;

	/* Try to transmit at least chunks of TX_MAX_BURST pkts */
	if (likely(nb_pkts <= RTE_PMD_NGBE_TX_MAX_BURST))
		return tx_xmit_pkts(tx_queue, tx_pkts, nb_pkts);

	/* transmit more than the max burst, in chunks of TX_MAX_BURST */
	nb_tx = 0;
	while (nb_pkts != 0) {
		uint16_t ret, n;

		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_NGBE_TX_MAX_BURST);
		ret = tx_xmit_pkts(tx_queue, &tx_pkts[nb_tx], n);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_tx;
}

/*********************************************************************
 *
 *  Rx functions
 *
 **********************************************************************/
uint16_t
ngbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct ngbe_rx_queue *rxq;
	volatile struct ngbe_rx_desc *rx_ring;
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *sw_ring;
	struct ngbe_rx_entry *rxe;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	struct ngbe_rx_desc rxd;
	uint64_t dma_addr;
	uint32_t staterr;
	uint16_t pkt_len;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	while (nb_rx < nb_pkts) {
		/*
		 * The order of operations here is important as the DD status
		 * bit must not be read after any other descriptor fields.
		 * rx_ring and rxdp are pointing to volatile data so the order
		 * of accesses cannot be reordered by the compiler. If they were
		 * not volatile, they could be reordered which could lead to
		 * using invalid descriptor fields when read from rxd.
		 */
		rxdp = &rx_ring[rx_id];
		staterr = rxdp->qw1.lo.status;
		if (!(staterr & rte_cpu_to_le_32(NGBE_RXD_STAT_DD)))
			break;
		rxd = *rxdp;

		/*
		 * End of packet.
		 *
		 * If the NGBE_RXD_STAT_EOP flag is not set, the Rx packet
		 * is likely to be invalid and to be dropped by the various
		 * validation checks performed by the network stack.
		 *
		 * Allocate a new mbuf to replenish the RX ring descriptor.
		 * If the allocation fails:
		 *    - arrange for that Rx descriptor to be the first one
		 *      being parsed the next time the receive function is
		 *      invoked [on the same queue].
		 *
		 *    - Stop parsing the Rx ring and return immediately.
		 *
		 * This policy do not drop the packet received in the Rx
		 * descriptor for which the allocation of a new mbuf failed.
		 * Thus, it allows that packet to be later retrieved if
		 * mbuf have been freed in the mean time.
		 * As a side effect, holding Rx descriptors instead of
		 * systematically giving them back to the NIC may lead to
		 * Rx ring exhaustion situations.
		 * However, the NIC can gracefully prevent such situations
		 * to happen by sending specific "back-pressure" flow control
		 * frames to its peer(s).
		 */
		PMD_RX_LOG(DEBUG,
			   "port_id=%u queue_id=%u rx_id=%u ext_err_stat=0x%08x pkt_len=%u",
			   (uint16_t)rxq->port_id, (uint16_t)rxq->queue_id,
			   (uint16_t)rx_id, (uint32_t)staterr,
			   (uint16_t)rte_le_to_cpu_16(rxd.qw1.hi.len));

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			PMD_RX_LOG(DEBUG,
				   "Rx mbuf alloc failed port_id=%u queue_id=%u",
				   (uint16_t)rxq->port_id,
				   (uint16_t)rxq->queue_id);
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_ngbe_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next Rx descriptor is on a cache-line boundary,
		 * prefetch the next 4 Rx descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_ngbe_prefetch(&rx_ring[rx_id]);
			rte_ngbe_prefetch(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		NGBE_RXD_HDRADDR(rxdp, 0);
		NGBE_RXD_PKTADDR(rxdp, dma_addr);

		/*
		 * Initialize the returned mbuf.
		 * setup generic mbuf fields:
		 *    - number of segments,
		 *    - next segment,
		 *    - packet length,
		 *    - Rx port identifier.
		 */
		pkt_len = (uint16_t)(rte_le_to_cpu_16(rxd.qw1.hi.len));
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_packet_prefetch((char *)rxm->buf_addr + rxm->data_off);
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = pkt_len;
		rxm->data_len = pkt_len;
		rxm->port = rxq->port_id;

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	/*
	 * If the number of free Rx descriptors is greater than the Rx free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed Rx descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG,
			   "port_id=%u queue_id=%u rx_tail=%u nb_hold=%u nb_rx=%u",
			   (uint16_t)rxq->port_id, (uint16_t)rxq->queue_id,
			   (uint16_t)rx_id, (uint16_t)nb_hold,
			   (uint16_t)nb_rx);
		rx_id = (uint16_t)((rx_id == 0) ?
				(rxq->nb_rx_desc - 1) : (rx_id - 1));
		ngbe_set32(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}


/*********************************************************************
 *
 *  Queue management functions
 *
 **********************************************************************/

static void
ngbe_tx_queue_release_mbufs(struct ngbe_tx_queue *txq)
{
	unsigned int i;

	if (txq->sw_ring != NULL) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
ngbe_tx_free_swring(struct ngbe_tx_queue *txq)
{
	if (txq != NULL)
		rte_free(txq->sw_ring);
}

static void
ngbe_tx_queue_release(struct ngbe_tx_queue *txq)
{
	if (txq != NULL) {
		if (txq->ops != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->free_swring(txq);
		}
		rte_free(txq);
	}
}

void
ngbe_dev_tx_queue_release(void *txq)
{
	ngbe_tx_queue_release(txq);
}

/* (Re)set dynamic ngbe_tx_queue fields to defaults */
static void
ngbe_reset_tx_queue(struct ngbe_tx_queue *txq)
{
	static const struct ngbe_tx_desc zeroed_desc = {0};
	struct ngbe_tx_entry *txe = txq->sw_ring;
	uint16_t prev, i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++)
		txq->tx_ring[i] = zeroed_desc;

	/* Initialize SW ring entries */
	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		/* the ring can also be modified by hardware */
		volatile struct ngbe_tx_desc *txd = &txq->tx_ring[i];

		txd->dw3 = rte_cpu_to_le_32(NGBE_TXD_DD);
		txe[i].mbuf = NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_free_thresh - 1);
	txq->tx_tail = 0;

	/*
	 * Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
	txq->ctx_curr = 0;
	memset((void *)&txq->ctx_cache, 0,
		NGBE_CTX_NUM * sizeof(struct ngbe_ctx_info));
}

static const struct ngbe_txq_ops def_txq_ops = {
	.release_mbufs = ngbe_tx_queue_release_mbufs,
	.free_swring = ngbe_tx_free_swring,
	.reset = ngbe_reset_tx_queue,
};

int
ngbe_dev_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct ngbe_tx_queue *txq;
	struct ngbe_hw     *hw;
	uint16_t tx_free_thresh;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	/*
	 * The Tx descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free Tx
	 * descriptors.
	 * One descriptor in the Tx ring is used as a sentinel to avoid a
	 * H/W race condition, hence the maximum threshold constraints.
	 * When set to zero use default values.
	 */
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
			tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR,
			     "tx_free_thresh must be less than the number of TX descriptors minus 3. (tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	if (nb_desc % tx_free_thresh != 0) {
		PMD_INIT_LOG(ERR,
			     "tx_free_thresh must be a divisor of the number of Tx descriptors. (tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		ngbe_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* First allocate the Tx queue data structure */
	txq = rte_zmalloc_socket("ethdev Tx queue",
				 sizeof(struct ngbe_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		return -ENOMEM;

	/*
	 * Allocate Tx ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
			sizeof(struct ngbe_tx_desc) * NGBE_RING_DESC_MAX,
			NGBE_ALIGN, socket_id);
	if (tz == NULL) {
		ngbe_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->tx_free_thresh = tx_free_thresh;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	txq->queue_id = queue_idx;
	txq->reg_idx = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->ops = &def_txq_ops;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	txq->tdt_reg_addr = NGBE_REG_ADDR(hw, NGBE_TXWP(txq->reg_idx));
	txq->tdc_reg_addr = NGBE_REG_ADDR(hw, NGBE_TXCFG(txq->reg_idx));

	txq->tx_ring_phys_addr = TMZ_PADDR(tz);
	txq->tx_ring = (struct ngbe_tx_desc *)TMZ_VADDR(tz);

	/* Allocate software ring */
	txq->sw_ring = rte_zmalloc_socket("txq->sw_ring",
				sizeof(struct ngbe_tx_entry) * nb_desc,
				RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		ngbe_tx_queue_release(txq);
		return -ENOMEM;
	}
	PMD_INIT_LOG(DEBUG,
		     "sw_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		     txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr);

	txq->ops->reset(txq);

	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

/**
 * ngbe_free_sc_cluster - free the not-yet-completed scattered cluster
 *
 * The "next" pointer of the last segment of (not-yet-completed) RSC clusters
 * in the sw_sc_ring is not set to NULL but rather points to the next
 * mbuf of this RSC aggregation (that has not been completed yet and still
 * resides on the HW ring). So, instead of calling for rte_pktmbuf_free() we
 * will just free first "nb_segs" segments of the cluster explicitly by calling
 * an rte_pktmbuf_free_seg().
 *
 * @m scattered cluster head
 */
static void
ngbe_free_sc_cluster(struct rte_mbuf *m)
{
	uint16_t i, nb_segs = m->nb_segs;
	struct rte_mbuf *next_seg;

	for (i = 0; i < nb_segs; i++) {
		next_seg = m->next;
		rte_pktmbuf_free_seg(m);
		m = next_seg;
	}
}

static void
ngbe_rx_queue_release_mbufs(struct ngbe_rx_queue *rxq)
{
	unsigned int i;

	if (rxq->sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
		for (i = 0; i < rxq->rx_nb_avail; ++i) {
			struct rte_mbuf *mb;

			mb = rxq->rx_stage[rxq->rx_next_avail + i];
			rte_pktmbuf_free_seg(mb);
		}
		rxq->rx_nb_avail = 0;
	}

	if (rxq->sw_sc_ring != NULL)
		for (i = 0; i < rxq->nb_rx_desc; i++)
			if (rxq->sw_sc_ring[i].fbuf != NULL) {
				ngbe_free_sc_cluster(rxq->sw_sc_ring[i].fbuf);
				rxq->sw_sc_ring[i].fbuf = NULL;
			}
}

static void
ngbe_rx_queue_release(struct ngbe_rx_queue *rxq)
{
	if (rxq != NULL) {
		ngbe_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq->sw_sc_ring);
		rte_free(rxq);
	}
}

void
ngbe_dev_rx_queue_release(void *rxq)
{
	ngbe_rx_queue_release(rxq);
}

/*
 * Check if Rx Burst Bulk Alloc function can be used.
 * Return
 *        0: the preconditions are satisfied and the bulk allocation function
 *           can be used.
 *  -EINVAL: the preconditions are NOT satisfied and the default Rx burst
 *           function must be used.
 */
static inline int
check_rx_burst_bulk_alloc_preconditions(struct ngbe_rx_queue *rxq)
{
	int ret = 0;

	/*
	 * Make sure the following pre-conditions are satisfied:
	 *   rxq->rx_free_thresh >= RTE_PMD_NGBE_RX_MAX_BURST
	 *   rxq->rx_free_thresh < rxq->nb_rx_desc
	 *   (rxq->nb_rx_desc % rxq->rx_free_thresh) == 0
	 * Scattered packets are not supported.  This should be checked
	 * outside of this function.
	 */
	if (rxq->rx_free_thresh < RTE_PMD_NGBE_RX_MAX_BURST) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=%d, RTE_PMD_NGBE_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, RTE_PMD_NGBE_RX_MAX_BURST);
		ret = -EINVAL;
	} else if (rxq->rx_free_thresh >= rxq->nb_rx_desc) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=%d, rxq->nb_rx_desc=%d",
			     rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if ((rxq->nb_rx_desc % rxq->rx_free_thresh) != 0) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->nb_rx_desc=%d, rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}

	return ret;
}

/* Reset dynamic ngbe_rx_queue fields back to defaults */
static void
ngbe_reset_rx_queue(struct ngbe_adapter *adapter, struct ngbe_rx_queue *rxq)
{
	static const struct ngbe_rx_desc zeroed_desc = {
						{{0}, {0} }, {{0}, {0} } };
	unsigned int i;
	uint16_t len = rxq->nb_rx_desc;

	/*
	 * By default, the Rx queue setup function allocates enough memory for
	 * NGBE_RING_DESC_MAX.  The Rx Burst bulk allocation function requires
	 * extra memory at the end of the descriptor ring to be zero'd out.
	 */
	if (adapter->rx_bulk_alloc_allowed)
		/* zero out extra memory */
		len += RTE_PMD_NGBE_RX_MAX_BURST;

	/*
	 * Zero out HW ring memory. Zero out extra memory at the end of
	 * the H/W ring so look-ahead logic in Rx Burst bulk alloc function
	 * reads extra memory as zeros.
	 */
	for (i = 0; i < len; i++)
		rxq->rx_ring[i] = zeroed_desc;

	/*
	 * initialize extra software ring entries. Space for these extra
	 * entries is always allocated
	 */
	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = rxq->nb_rx_desc; i < len; ++i)
		rxq->sw_ring[i].mbuf = &rxq->fake_mbuf;

	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);
	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

int
ngbe_dev_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct ngbe_rx_queue *rxq;
	struct ngbe_hw     *hw;
	uint16_t len;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		ngbe_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the Rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue",
				 sizeof(struct ngbe_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		return -ENOMEM;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;

	/*
	 * Allocate Rx ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      RX_RING_SZ, NGBE_ALIGN, socket_id);
	if (rz == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Zero init all the descriptors in the ring.
	 */
	memset(rz->addr, 0, RX_RING_SZ);

	rxq->rdt_reg_addr = NGBE_REG_ADDR(hw, NGBE_RXWP(rxq->reg_idx));
	rxq->rdh_reg_addr = NGBE_REG_ADDR(hw, NGBE_RXRP(rxq->reg_idx));

	rxq->rx_ring_phys_addr = TMZ_PADDR(rz);
	rxq->rx_ring = (struct ngbe_rx_desc *)TMZ_VADDR(rz);

	/*
	 * Certain constraints must be met in order to use the bulk buffer
	 * allocation Rx burst function. If any of Rx queues doesn't meet them
	 * the feature should be disabled for the whole port.
	 */
	if (check_rx_burst_bulk_alloc_preconditions(rxq)) {
		PMD_INIT_LOG(DEBUG,
			     "queue[%d] doesn't meet Rx Bulk Alloc preconditions - canceling the feature for the whole port[%d]",
			     rxq->queue_id, rxq->port_id);
		adapter->rx_bulk_alloc_allowed = false;
	}

	/*
	 * Allocate software ring. Allow for space at the end of the
	 * S/W ring to make sure look-ahead logic in bulk alloc Rx burst
	 * function does not access an invalid memory region.
	 */
	len = nb_desc;
	if (adapter->rx_bulk_alloc_allowed)
		len += RTE_PMD_NGBE_RX_MAX_BURST;

	rxq->sw_ring = rte_zmalloc_socket("rxq->sw_ring",
					  sizeof(struct ngbe_rx_entry) * len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Always allocate even if it's not going to be needed in order to
	 * simplify the code.
	 *
	 * This ring is used in Scattered Rx cases and Scattered Rx may
	 * be requested in ngbe_dev_rx_init(), which is called later from
	 * dev_start() flow.
	 */
	rxq->sw_sc_ring =
		rte_zmalloc_socket("rxq->sw_sc_ring",
				  sizeof(struct ngbe_scattered_rx_entry) * len,
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_sc_ring == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	PMD_INIT_LOG(DEBUG,
		     "sw_ring=%p sw_sc_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		     rxq->sw_ring, rxq->sw_sc_ring, rxq->rx_ring,
		     rxq->rx_ring_phys_addr);

	dev->data->rx_queues[queue_idx] = rxq;

	ngbe_reset_rx_queue(adapter, rxq);

	return 0;
}

void
ngbe_dev_clear_queues(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct ngbe_tx_queue *txq = dev->data->tx_queues[i];

		if (txq != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->reset(txq);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct ngbe_rx_queue *rxq = dev->data->rx_queues[i];

		if (rxq != NULL) {
			ngbe_rx_queue_release_mbufs(rxq);
			ngbe_reset_rx_queue(adapter, rxq);
		}
	}
}

void
ngbe_dev_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		ngbe_dev_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ngbe_dev_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

static int
ngbe_alloc_rx_queue_mbufs(struct ngbe_rx_queue *rxq)
{
	struct ngbe_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	unsigned int i;

	/* Initialize software ring entries */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		/* the ring can also be modified by hardware */
		volatile struct ngbe_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "Rx mbuf alloc failed queue_id=%u port_id=%u",
				     (unsigned int)rxq->queue_id,
				     (unsigned int)rxq->port_id);
			return -ENOMEM;
		}

		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxd = &rxq->rx_ring[i];
		NGBE_RXD_HDRADDR(rxd, 0);
		NGBE_RXD_PKTADDR(rxd, dma_addr);
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/*
 * Initializes Receive Unit.
 */
int
ngbe_dev_rx_init(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw;
	struct ngbe_rx_queue *rxq;
	uint64_t bus_addr;
	uint32_t fctrl;
	uint32_t hlreg0;
	uint32_t srrctl;
	uint16_t buf_size;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	/*
	 * Make sure receives are disabled while setting
	 * up the Rx context (registers, descriptor rings, etc.).
	 */
	wr32m(hw, NGBE_MACRXCFG, NGBE_MACRXCFG_ENA, 0);
	wr32m(hw, NGBE_PBRXCTL, NGBE_PBRXCTL_ENA, 0);

	/* Enable receipt of broadcasted frames */
	fctrl = rd32(hw, NGBE_PSRCTL);
	fctrl |= NGBE_PSRCTL_BCA;
	wr32(hw, NGBE_PSRCTL, fctrl);

	hlreg0 = rd32(hw, NGBE_SECRXCTL);
	hlreg0 &= ~NGBE_SECRXCTL_XDSA;
	wr32(hw, NGBE_SECRXCTL, hlreg0);

	wr32m(hw, NGBE_FRMSZ, NGBE_FRMSZ_MAX_MASK,
			NGBE_FRMSZ_MAX(NGBE_FRAME_SIZE_DFT));

	/* Setup Rx queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		/* Setup the Base and Length of the Rx Descriptor Rings */
		bus_addr = rxq->rx_ring_phys_addr;
		wr32(hw, NGBE_RXBAL(rxq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, NGBE_RXBAH(rxq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32(hw, NGBE_RXRP(rxq->reg_idx), 0);
		wr32(hw, NGBE_RXWP(rxq->reg_idx), 0);

		srrctl = NGBE_RXCFG_RNGLEN(rxq->nb_rx_desc);

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= NGBE_RXCFG_DROP;

		/*
		 * Configure the Rx buffer size in the PKTLEN field of
		 * the RXCFG register of the queue.
		 * The value is in 1 KB resolution. Valid values can be from
		 * 1 KB to 16 KB.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM);
		buf_size = ROUND_DOWN(buf_size, 0x1 << 10);
		srrctl |= NGBE_RXCFG_PKTLEN(buf_size);

		wr32(hw, NGBE_RXCFG(rxq->reg_idx), srrctl);
	}

	return 0;
}

/*
 * Initializes Transmit Unit.
 */
void
ngbe_dev_tx_init(struct rte_eth_dev *dev)
{
	struct ngbe_hw     *hw;
	struct ngbe_tx_queue *txq;
	uint64_t bus_addr;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	wr32m(hw, NGBE_SECTXCTL, NGBE_SECTXCTL_ODSA, NGBE_SECTXCTL_ODSA);
	wr32m(hw, NGBE_SECTXCTL, NGBE_SECTXCTL_XDSA, 0);

	/* Setup the Base and Length of the Tx Descriptor Rings */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		bus_addr = txq->tx_ring_phys_addr;
		wr32(hw, NGBE_TXBAL(txq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, NGBE_TXBAH(txq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32m(hw, NGBE_TXCFG(txq->reg_idx), NGBE_TXCFG_BUFLEN_MASK,
			NGBE_TXCFG_BUFLEN(txq->nb_tx_desc));
		/* Setup the HW Tx Head and TX Tail descriptor pointers */
		wr32(hw, NGBE_TXRP(txq->reg_idx), 0);
		wr32(hw, NGBE_TXWP(txq->reg_idx), 0);
	}
}

/*
 * Start Transmit and Receive Units.
 */
int
ngbe_dev_rxtx_start(struct rte_eth_dev *dev)
{
	struct ngbe_hw     *hw;
	struct ngbe_tx_queue *txq;
	struct ngbe_rx_queue *rxq;
	uint32_t dmatxctl;
	uint32_t rxctrl;
	uint16_t i;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		/* Setup Transmit Threshold Registers */
		wr32m(hw, NGBE_TXCFG(txq->reg_idx),
		      NGBE_TXCFG_HTHRESH_MASK |
		      NGBE_TXCFG_WTHRESH_MASK,
		      NGBE_TXCFG_HTHRESH(txq->hthresh) |
		      NGBE_TXCFG_WTHRESH(txq->wthresh));
	}

	dmatxctl = rd32(hw, NGBE_DMATXCTRL);
	dmatxctl |= NGBE_DMATXCTRL_ENA;
	wr32(hw, NGBE_DMATXCTRL, dmatxctl);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq->tx_deferred_start == 0) {
			ret = ngbe_dev_tx_queue_start(dev, i);
			if (ret < 0)
				return ret;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq->rx_deferred_start == 0) {
			ret = ngbe_dev_rx_queue_start(dev, i);
			if (ret < 0)
				return ret;
		}
	}

	/* Enable Receive engine */
	rxctrl = rd32(hw, NGBE_PBRXCTL);
	rxctrl |= NGBE_PBRXCTL_ENA;
	hw->mac.enable_rx_dma(hw, rxctrl);

	return 0;
}

void
ngbe_dev_save_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id)
{
	u32 *reg = &hw->q_rx_regs[rx_queue_id * 8];
	*(reg++) = rd32(hw, NGBE_RXBAL(rx_queue_id));
	*(reg++) = rd32(hw, NGBE_RXBAH(rx_queue_id));
	*(reg++) = rd32(hw, NGBE_RXCFG(rx_queue_id));
}

void
ngbe_dev_store_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id)
{
	u32 *reg = &hw->q_rx_regs[rx_queue_id * 8];
	wr32(hw, NGBE_RXBAL(rx_queue_id), *(reg++));
	wr32(hw, NGBE_RXBAH(rx_queue_id), *(reg++));
	wr32(hw, NGBE_RXCFG(rx_queue_id), *(reg++) & ~NGBE_RXCFG_ENA);
}

void
ngbe_dev_save_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id)
{
	u32 *reg = &hw->q_tx_regs[tx_queue_id * 8];
	*(reg++) = rd32(hw, NGBE_TXBAL(tx_queue_id));
	*(reg++) = rd32(hw, NGBE_TXBAH(tx_queue_id));
	*(reg++) = rd32(hw, NGBE_TXCFG(tx_queue_id));
}

void
ngbe_dev_store_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id)
{
	u32 *reg = &hw->q_tx_regs[tx_queue_id * 8];
	wr32(hw, NGBE_TXBAL(tx_queue_id), *(reg++));
	wr32(hw, NGBE_TXBAH(tx_queue_id), *(reg++));
	wr32(hw, NGBE_TXCFG(tx_queue_id), *(reg++) & ~NGBE_TXCFG_ENA);
}

/*
 * Start Receive Units for specified queue.
 */
int
ngbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];

	/* Allocate buffers for descriptor rings */
	if (ngbe_alloc_rx_queue_mbufs(rxq) != 0) {
		PMD_INIT_LOG(ERR, "Could not alloc mbuf for queue:%d",
			     rx_queue_id);
		return -1;
	}
	rxdctl = rd32(hw, NGBE_RXCFG(rxq->reg_idx));
	rxdctl |= NGBE_RXCFG_ENA;
	wr32(hw, NGBE_RXCFG(rxq->reg_idx), rxdctl);

	/* Wait until Rx Enable ready */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		rxdctl = rd32(hw, NGBE_RXCFG(rxq->reg_idx));
	} while (--poll_ms && !(rxdctl & NGBE_RXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not enable Rx Queue %d", rx_queue_id);
	rte_wmb();
	wr32(hw, NGBE_RXRP(rxq->reg_idx), 0);
	wr32(hw, NGBE_RXWP(rxq->reg_idx), rxq->nb_rx_desc - 1);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/*
 * Stop Receive Units for specified queue.
 */
int
ngbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	struct ngbe_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];

	ngbe_dev_save_rx_queue(hw, rxq->reg_idx);
	wr32m(hw, NGBE_RXCFG(rxq->reg_idx), NGBE_RXCFG_ENA, 0);

	/* Wait until Rx Enable bit clear */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		rxdctl = rd32(hw, NGBE_RXCFG(rxq->reg_idx));
	} while (--poll_ms && (rxdctl & NGBE_RXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not disable Rx Queue %d", rx_queue_id);

	rte_delay_us(RTE_NGBE_WAIT_100_US);
	ngbe_dev_store_rx_queue(hw, rxq->reg_idx);

	ngbe_rx_queue_release_mbufs(rxq);
	ngbe_reset_rx_queue(adapter, rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

/*
 * Start Transmit Units for specified queue.
 */
int
ngbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_tx_queue *txq;
	uint32_t txdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];
	wr32m(hw, NGBE_TXCFG(txq->reg_idx), NGBE_TXCFG_ENA, NGBE_TXCFG_ENA);

	/* Wait until Tx Enable ready */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		txdctl = rd32(hw, NGBE_TXCFG(txq->reg_idx));
	} while (--poll_ms && !(txdctl & NGBE_TXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not enable Tx Queue %d",
			     tx_queue_id);

	rte_wmb();
	wr32(hw, NGBE_TXWP(txq->reg_idx), txq->tx_tail);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/*
 * Stop Transmit Units for specified queue.
 */
int
ngbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_tx_queue *txq;
	uint32_t txdctl;
	uint32_t txtdh, txtdt;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];

	/* Wait until Tx queue is empty */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_us(RTE_NGBE_WAIT_100_US);
		txtdh = rd32(hw, NGBE_TXRP(txq->reg_idx));
		txtdt = rd32(hw, NGBE_TXWP(txq->reg_idx));
	} while (--poll_ms && (txtdh != txtdt));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Tx Queue %d is not empty when stopping.",
			     tx_queue_id);

	ngbe_dev_save_tx_queue(hw, txq->reg_idx);
	wr32m(hw, NGBE_TXCFG(txq->reg_idx), NGBE_TXCFG_ENA, 0);

	/* Wait until Tx Enable bit clear */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		txdctl = rd32(hw, NGBE_TXCFG(txq->reg_idx));
	} while (--poll_ms && (txdctl & NGBE_TXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not disable Tx Queue %d",
			     tx_queue_id);

	rte_delay_us(RTE_NGBE_WAIT_100_US);
	ngbe_dev_store_tx_queue(hw, txq->reg_idx);

	if (txq->ops != NULL) {
		txq->ops->release_mbufs(txq);
		txq->ops->reset(txq);
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}
