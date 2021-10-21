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
static inline uint32_t
ngbe_rxd_pkt_info_to_pkt_type(uint32_t pkt_info, uint16_t ptid_mask)
{
	uint16_t ptid = NGBE_RXD_PTID(pkt_info);

	ptid &= ptid_mask;

	return ngbe_decode_ptype(ptid);
}

/*
 * LOOK_AHEAD defines how many desc statuses to check beyond the
 * current descriptor.
 * It must be a pound define for optimal performance.
 * Do not change the value of LOOK_AHEAD, as the ngbe_rx_scan_hw_ring
 * function only works with LOOK_AHEAD=8.
 */
#define LOOK_AHEAD 8
#if (LOOK_AHEAD != 8)
#error "PMD NGBE: LOOK_AHEAD must be 8\n"
#endif
static inline int
ngbe_rx_scan_hw_ring(struct ngbe_rx_queue *rxq)
{
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t pkt_len;
	int nb_dd;
	uint32_t s[LOOK_AHEAD];
	uint32_t pkt_info[LOOK_AHEAD];
	int i, j, nb_rx = 0;
	uint32_t status;

	/* get references to current descriptor and S/W ring entry */
	rxdp = &rxq->rx_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	status = rxdp->qw1.lo.status;
	/* check to make sure there is at least 1 packet to receive */
	if (!(status & rte_cpu_to_le_32(NGBE_RXD_STAT_DD)))
		return 0;

	/*
	 * Scan LOOK_AHEAD descriptors at a time to determine which descriptors
	 * reference packets that are ready to be received.
	 */
	for (i = 0; i < RTE_PMD_NGBE_RX_MAX_BURST;
	     i += LOOK_AHEAD, rxdp += LOOK_AHEAD, rxep += LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = 0; j < LOOK_AHEAD; j++)
			s[j] = rte_le_to_cpu_32(rxdp[j].qw1.lo.status);

		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

		/* Compute how many status bits were set */
		for (nb_dd = 0; nb_dd < LOOK_AHEAD &&
				(s[nb_dd] & NGBE_RXD_STAT_DD); nb_dd++)
			;

		for (j = 0; j < nb_dd; j++)
			pkt_info[j] = rte_le_to_cpu_32(rxdp[j].qw0.dw0);

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf format */
		for (j = 0; j < nb_dd; ++j) {
			mb = rxep[j].mbuf;
			pkt_len = rte_le_to_cpu_16(rxdp[j].qw1.hi.len);
			mb->data_len = pkt_len;
			mb->pkt_len = pkt_len;

			mb->packet_type =
				ngbe_rxd_pkt_info_to_pkt_type(pkt_info[j],
				NGBE_PTID_MASK);
		}

		/* Move mbuf pointers from the S/W ring to the stage */
		for (j = 0; j < LOOK_AHEAD; ++j)
			rxq->rx_stage[i + j] = rxep[j].mbuf;

		/* stop if all requested packets could not be received */
		if (nb_dd != LOOK_AHEAD)
			break;
	}

	/* clear software ring entries so we can cleanup correctly */
	for (i = 0; i < nb_rx; ++i)
		rxq->sw_ring[rxq->rx_tail + i].mbuf = NULL;

	return nb_rx;
}

static inline int
ngbe_rx_alloc_bufs(struct ngbe_rx_queue *rxq, bool reset_mbuf)
{
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t alloc_idx;
	__le64 dma_addr;
	int diag, i;

	/* allocate buffers in bulk directly into the S/W ring */
	alloc_idx = rxq->rx_free_trigger - (rxq->rx_free_thresh - 1);
	rxep = &rxq->sw_ring[alloc_idx];
	diag = rte_mempool_get_bulk(rxq->mb_pool, (void *)rxep,
				    rxq->rx_free_thresh);
	if (unlikely(diag != 0))
		return -ENOMEM;

	rxdp = &rxq->rx_ring[alloc_idx];
	for (i = 0; i < rxq->rx_free_thresh; ++i) {
		/* populate the static rte mbuf fields */
		mb = rxep[i].mbuf;
		if (reset_mbuf)
			mb->port = rxq->port_id;

		rte_mbuf_refcnt_set(mb, 1);
		mb->data_off = RTE_PKTMBUF_HEADROOM;

		/* populate the descriptors */
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mb));
		NGBE_RXD_HDRADDR(&rxdp[i], 0);
		NGBE_RXD_PKTADDR(&rxdp[i], dma_addr);
	}

	/* update state of internal queue structure */
	rxq->rx_free_trigger = rxq->rx_free_trigger + rxq->rx_free_thresh;
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = rxq->rx_free_thresh - 1;

	/* no errors */
	return 0;
}

static inline uint16_t
ngbe_rx_fill_from_stage(struct ngbe_rx_queue *rxq, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct rte_mbuf **stage = &rxq->rx_stage[rxq->rx_next_avail];
	int i;

	/* how many packets are ready to return? */
	nb_pkts = (uint16_t)RTE_MIN(nb_pkts, rxq->rx_nb_avail);

	/* copy mbuf pointers to the application's packet list */
	for (i = 0; i < nb_pkts; ++i)
		rx_pkts[i] = stage[i];

	/* update internal queue state */
	rxq->rx_nb_avail = (uint16_t)(rxq->rx_nb_avail - nb_pkts);
	rxq->rx_next_avail = (uint16_t)(rxq->rx_next_avail + nb_pkts);

	return nb_pkts;
}

static inline uint16_t
ngbe_rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	     uint16_t nb_pkts)
{
	struct ngbe_rx_queue *rxq = (struct ngbe_rx_queue *)rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	uint16_t nb_rx = 0;

	/* Any previously recv'd pkts will be returned from the Rx stage */
	if (rxq->rx_nb_avail)
		return ngbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	/* Scan the H/W ring for packets to receive */
	nb_rx = (uint16_t)ngbe_rx_scan_hw_ring(rxq);

	/* update internal queue state */
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	/* if required, allocate new buffers to replenish descriptors */
	if (rxq->rx_tail > rxq->rx_free_trigger) {
		uint16_t cur_free_trigger = rxq->rx_free_trigger;

		if (ngbe_rx_alloc_bufs(rxq, true) != 0) {
			int i, j;

			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", (uint16_t)rxq->port_id,
				   (uint16_t)rxq->queue_id);

			dev->data->rx_mbuf_alloc_failed +=
				rxq->rx_free_thresh;

			/*
			 * Need to rewind any previous receives if we cannot
			 * allocate new buffers to replenish the old ones.
			 */
			rxq->rx_nb_avail = 0;
			rxq->rx_tail = (uint16_t)(rxq->rx_tail - nb_rx);
			for (i = 0, j = rxq->rx_tail; i < nb_rx; ++i, ++j)
				rxq->sw_ring[j].mbuf = rxq->rx_stage[i];

			return 0;
		}

		/* update tail pointer */
		rte_wmb();
		ngbe_set32_relaxed(rxq->rdt_reg_addr, cur_free_trigger);
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	/* received any packets this loop? */
	if (rxq->rx_nb_avail)
		return ngbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

/* split requests into chunks of size RTE_PMD_NGBE_RX_MAX_BURST */
uint16_t
ngbe_recv_pkts_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts)
{
	uint16_t nb_rx;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (likely(nb_pkts <= RTE_PMD_NGBE_RX_MAX_BURST))
		return ngbe_rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	/* request is relatively large, chunk it up */
	nb_rx = 0;
	while (nb_pkts) {
		uint16_t ret, n;

		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_NGBE_RX_MAX_BURST);
		ret = ngbe_rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_rx;
}

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
	uint32_t pkt_info;
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

		pkt_info = rte_le_to_cpu_32(rxd.qw0.dw0);
		rxm->packet_type = ngbe_rxd_pkt_info_to_pkt_type(pkt_info,
						       NGBE_PTID_MASK);

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

static inline void
ngbe_fill_cluster_head_buf(struct rte_mbuf *head, struct ngbe_rx_desc *desc,
		struct ngbe_rx_queue *rxq, uint32_t staterr)
{
	uint32_t pkt_info;

	RTE_SET_USED(staterr);
	head->port = rxq->port_id;

	pkt_info = rte_le_to_cpu_32(desc->qw0.dw0);
	head->packet_type = ngbe_rxd_pkt_info_to_pkt_type(pkt_info,
						NGBE_PTID_MASK);
}

/**
 * ngbe_recv_pkts_sc - receive handler for scatter case.
 *
 * @rx_queue Rx queue handle
 * @rx_pkts table of received packets
 * @nb_pkts size of rx_pkts table
 * @bulk_alloc if TRUE bulk allocation is used for a HW ring refilling
 *
 * Returns the number of received packets/clusters (according to the "bulk
 * receive" interface).
 */
static inline uint16_t
ngbe_recv_pkts_sc(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts,
		    bool bulk_alloc)
{
	struct ngbe_rx_queue *rxq = rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	volatile struct ngbe_rx_desc *rx_ring = rxq->rx_ring;
	struct ngbe_rx_entry *sw_ring = rxq->sw_ring;
	struct ngbe_scattered_rx_entry *sw_sc_ring = rxq->sw_sc_ring;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = rxq->nb_rx_hold;
	uint16_t prev_id = rxq->rx_tail;

	while (nb_rx < nb_pkts) {
		bool eop;
		struct ngbe_rx_entry *rxe;
		struct ngbe_scattered_rx_entry *sc_entry;
		struct ngbe_scattered_rx_entry *next_sc_entry = NULL;
		struct ngbe_rx_entry *next_rxe = NULL;
		struct rte_mbuf *first_seg;
		struct rte_mbuf *rxm;
		struct rte_mbuf *nmb = NULL;
		struct ngbe_rx_desc rxd;
		uint16_t data_len;
		uint16_t next_id;
		volatile struct ngbe_rx_desc *rxdp;
		uint32_t staterr;

next_desc:
		rxdp = &rx_ring[rx_id];
		staterr = rte_le_to_cpu_32(rxdp->qw1.lo.status);

		if (!(staterr & NGBE_RXD_STAT_DD))
			break;

		rxd = *rxdp;

		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_id=%u "
				  "staterr=0x%x data_len=%u",
			   rxq->port_id, rxq->queue_id, rx_id, staterr,
			   rte_le_to_cpu_16(rxd.qw1.hi.len));

		if (!bulk_alloc) {
			nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (nmb == NULL) {
				PMD_RX_LOG(DEBUG, "Rx mbuf alloc failed "
						  "port_id=%u queue_id=%u",
					   rxq->port_id, rxq->queue_id);

				dev->data->rx_mbuf_alloc_failed++;
				break;
			}
		} else if (nb_hold > rxq->rx_free_thresh) {
			uint16_t next_rdt = rxq->rx_free_trigger;

			if (!ngbe_rx_alloc_bufs(rxq, false)) {
				rte_wmb();
				ngbe_set32_relaxed(rxq->rdt_reg_addr,
							    next_rdt);
				nb_hold -= rxq->rx_free_thresh;
			} else {
				PMD_RX_LOG(DEBUG, "Rx bulk alloc failed "
						  "port_id=%u queue_id=%u",
					   rxq->port_id, rxq->queue_id);

				dev->data->rx_mbuf_alloc_failed++;
				break;
			}
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		eop = staterr & NGBE_RXD_STAT_EOP;

		next_id = rx_id + 1;
		if (next_id == rxq->nb_rx_desc)
			next_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_ngbe_prefetch(sw_ring[next_id].mbuf);

		/*
		 * When next Rx descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 4 pointers
		 * to mbufs.
		 */
		if ((next_id & 0x3) == 0) {
			rte_ngbe_prefetch(&rx_ring[next_id]);
			rte_ngbe_prefetch(&sw_ring[next_id]);
		}

		rxm = rxe->mbuf;

		if (!bulk_alloc) {
			__le64 dma =
			  rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
			/*
			 * Update Rx descriptor with the physical address of the
			 * new data buffer of the new allocated mbuf.
			 */
			rxe->mbuf = nmb;

			rxm->data_off = RTE_PKTMBUF_HEADROOM;
			NGBE_RXD_HDRADDR(rxdp, 0);
			NGBE_RXD_PKTADDR(rxdp, dma);
		} else {
			rxe->mbuf = NULL;
		}

		/*
		 * Set data length & data buffer address of mbuf.
		 */
		data_len = rte_le_to_cpu_16(rxd.qw1.hi.len);
		rxm->data_len = data_len;

		if (!eop) {
			uint16_t nextp_id;

			nextp_id = next_id;
			next_sc_entry = &sw_sc_ring[nextp_id];
			next_rxe = &sw_ring[nextp_id];
			rte_ngbe_prefetch(next_rxe);
		}

		sc_entry = &sw_sc_ring[rx_id];
		first_seg = sc_entry->fbuf;
		sc_entry->fbuf = NULL;

		/*
		 * If this is the first buffer of the received packet,
		 * set the pointer to the first mbuf of the packet and
		 * initialize its context.
		 * Otherwise, update the total length and the number of segments
		 * of the current scattered packet, and update the pointer to
		 * the last mbuf of the current packet.
		 */
		if (first_seg == NULL) {
			first_seg = rxm;
			first_seg->pkt_len = data_len;
			first_seg->nb_segs = 1;
		} else {
			first_seg->pkt_len += data_len;
			first_seg->nb_segs++;
		}

		prev_id = rx_id;
		rx_id = next_id;

		/*
		 * If this is not the last buffer of the received packet, update
		 * the pointer to the first mbuf at the NEXTP entry in the
		 * sw_sc_ring and continue to parse the Rx ring.
		 */
		if (!eop && next_rxe) {
			rxm->next = next_rxe->mbuf;
			next_sc_entry->fbuf = first_seg;
			goto next_desc;
		}

		/* Initialize the first mbuf of the returned packet */
		ngbe_fill_cluster_head_buf(first_seg, &rxd, rxq, staterr);

		/* Prefetch data of first segment, if configured to do so. */
		rte_packet_prefetch((char *)first_seg->buf_addr +
			first_seg->data_off);

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = first_seg;
	}

	/*
	 * Record index of the next Rx descriptor to probe.
	 */
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
	if (!bulk_alloc && nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u",
			   rxq->port_id, rxq->queue_id, rx_id, nb_hold, nb_rx);

		rte_wmb();
		ngbe_set32_relaxed(rxq->rdt_reg_addr, prev_id);
		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

uint16_t
ngbe_recv_pkts_sc_single_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts)
{
	return ngbe_recv_pkts_sc(rx_queue, rx_pkts, nb_pkts, false);
}

uint16_t
ngbe_recv_pkts_sc_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts)
{
	return ngbe_recv_pkts_sc(rx_queue, rx_pkts, nb_pkts, true);
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
ngbe_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	ngbe_tx_queue_release(dev->data->tx_queues[qid]);
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
ngbe_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	ngbe_rx_queue_release(dev->data->rx_queues[qid]);
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

uint64_t
ngbe_get_rx_port_offloads(struct rte_eth_dev *dev __rte_unused)
{
	return RTE_ETH_RX_OFFLOAD_SCATTER;
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
		ngbe_dev_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ngbe_dev_tx_queue_release(dev, i);
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

void
ngbe_set_rx_function(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	if (dev->data->scattered_rx) {
		/*
		 * Set the scattered callback: there are bulk and
		 * single allocation versions.
		 */
		if (adapter->rx_bulk_alloc_allowed) {
			PMD_INIT_LOG(DEBUG, "Using a Scattered with bulk "
					   "allocation callback (port=%d).",
				     dev->data->port_id);
			dev->rx_pkt_burst = ngbe_recv_pkts_sc_bulk_alloc;
		} else {
			PMD_INIT_LOG(DEBUG, "Using Regular (non-vector, "
					    "single allocation) "
					    "Scattered Rx callback "
					    "(port=%d).",
				     dev->data->port_id);

			dev->rx_pkt_burst = ngbe_recv_pkts_sc_single_alloc;
		}
	/*
	 * Below we set "simple" callbacks according to port/queues parameters.
	 * If parameters allow we are going to choose between the following
	 * callbacks:
	 *    - Bulk Allocation
	 *    - Single buffer allocation (the simplest one)
	 */
	} else if (adapter->rx_bulk_alloc_allowed) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
				    "satisfied. Rx Burst Bulk Alloc function "
				    "will be used on port=%d.",
			     dev->data->port_id);

		dev->rx_pkt_burst = ngbe_recv_pkts_bulk_alloc;
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are not "
				    "satisfied, or Scattered Rx is requested "
				    "(port=%d).",
			     dev->data->port_id);

		dev->rx_pkt_burst = ngbe_recv_pkts;
	}
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
	struct rte_eth_rxmode *rx_conf = &dev->data->dev_conf.rxmode;

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

	if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
		dev->data->scattered_rx = 1;

	ngbe_set_rx_function(dev);

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
