/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

 #ifndef _COMMON_INTEL_RECYCLE_MBUFS_H_
#define _COMMON_INTEL_RECYCLE_MBUFS_H_

#include <stdint.h>
#include <unistd.h>

#include <rte_mbuf.h>
#include <rte_io.h>
#include <ethdev_driver.h>

#include "rx.h"
#include "tx.h"

/**
 * Recycle mbufs for Rx queue.
 *
 * @param rxq Rx queue pointer
 * @param nb_mbufs number of mbufs to recycle
 * @param desc_len length of Rx descriptor
 */
static __rte_always_inline void
ci_rx_recycle_mbufs(struct ci_rx_queue *rxq, const uint16_t nb_mbufs)
{
	struct ci_rx_entry *rxep;
	volatile union ci_rx_desc *rxdp;
	uint16_t rx_id;
	uint16_t i;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;
	rxep = &rxq->sw_ring[rxq->rxrearm_start];

	for (i = 0; i < nb_mbufs; i++) {
		struct rte_mbuf *mb = rxep[i].mbuf;

#if RTE_IOVA_IN_MBUF
		const uint64_t iova = mb->buf_iova + RTE_PKTMBUF_HEADROOM;
		const uint64_t dma_addr = rte_cpu_to_le_64(iova);
#else
		const uint64_t vaddr = (uintptr_t)mb->buf_addr + RTE_PKTMBUF_HEADROOM;
		const uint64_t dma_addr = rte_cpu_to_le_64(vaddr);
#endif

		rxdp[i].read.hdr_addr = 0;
		rxdp[i].read.pkt_addr = dma_addr;
	}

	/* Update the descriptor initializer index */
	rxq->rxrearm_start += nb_mbufs;
	rx_id = rxq->rxrearm_start - 1;

	if (unlikely(rxq->rxrearm_start >= rxq->nb_rx_desc)) {
		rxq->rxrearm_start = 0;
		rx_id = rxq->nb_rx_desc - 1;
	}

	rxq->rxrearm_nb -= nb_mbufs;

	rte_io_wmb();

	/* Update the tail pointer on the NIC */
	rte_write32_wc_relaxed(rte_cpu_to_le_32(rx_id), rxq->qrx_tail);
}

/**
 * Recycle buffers on Tx.
 *
 * @param txq Tx queue pointer
 * @param desc_done function to check if the Tx descriptor is done
 * @param recycle_rxq_info recycling mbuf information
 *
 * @return how many buffers were recycled
 */
static __rte_always_inline uint16_t
ci_tx_recycle_mbufs(struct ci_tx_queue *txq, ci_desc_done_fn desc_done,
	struct rte_eth_recycle_rxq_info *recycle_rxq_info)
{
	struct ci_tx_entry *txep;
	struct rte_mbuf **rxep;
	int i, n;
	uint16_t nb_recycle_mbufs;
	uint16_t avail = 0;
	uint16_t mbuf_ring_size = recycle_rxq_info->mbuf_ring_size;
	uint16_t mask = recycle_rxq_info->mbuf_ring_size - 1;
	uint16_t refill_requirement = recycle_rxq_info->refill_requirement;
	uint16_t refill_head = *recycle_rxq_info->refill_head;
	uint16_t receive_tail = *recycle_rxq_info->receive_tail;

	/* Get available recycling Rx buffers. */
	avail = (mbuf_ring_size - (refill_head - receive_tail)) & mask;

	/* Check Tx free thresh and Rx available space. */
	if (txq->nb_tx_free > txq->tx_free_thresh || avail <= txq->tx_rs_thresh)
		return 0;

	if (!desc_done(txq, txq->tx_next_dd)) {
		/* If the Tx descriptor is not done, we can not recycle
		 * buffers.
		 */
		return 0;
	}

	n = txq->tx_rs_thresh;
	nb_recycle_mbufs = n;

	/* Mbufs recycle mode can only support no ring buffer wrapping around.
	 * Two case for this:
	 *
	 * case 1: The refill head of Rx buffer ring needs to be aligned with
	 * mbuf ring size. In this case, the number of Tx freeing buffers
	 * should be equal to refill_requirement.
	 *
	 * case 2: The refill head of Rx ring buffer does not need to be aligned
	 * with mbuf ring size. In this case, the update of refill head can not
	 * exceed the Rx mbuf ring size.
	 */
	if ((refill_requirement && refill_requirement != n) ||
		(!refill_requirement && (refill_head + n > mbuf_ring_size)))
		return 0;

	/* First buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_rs_thresh-1).
	 */
	txep = &txq->sw_ring[txq->tx_next_dd - (n - 1)];
	rxep = recycle_rxq_info->mbuf_ring;
	rxep += refill_head;

	/* is fast-free enabled in offloads? */
	struct rte_mempool *fast_free_mp =
			likely(txq->fast_free_mp != (void *)UINTPTR_MAX) ?
			txq->fast_free_mp :
			(txq->fast_free_mp = txep[0].mbuf->pool);

	if (fast_free_mp != NULL) {
		/* Avoid txq containing buffers from unexpected mempool. */
		if (unlikely(recycle_rxq_info->mp != fast_free_mp))
			return 0;

		/* Directly put mbufs from Tx to Rx. */
		for (i = 0; i < n; i++)
			rxep[i] = txep[i].mbuf;
	} else {
		for (i = 0; i < n; i++) {
			rxep[i] = rte_pktmbuf_prefree_seg(txep[i].mbuf);

			/* If Tx buffers are not the last reference or from
			 * unexpected mempool, previous copied buffers are
			 * considered as invalid.
			 */
			if (unlikely(rxep[i] == NULL ||
				recycle_rxq_info->mp != txep[i].mbuf->pool))
				nb_recycle_mbufs = 0;
		}
		/* If Tx buffers are not the last reference or
		 * from unexpected mempool, all recycled buffers
		 * are put into mempool.
		 */
		if (nb_recycle_mbufs == 0)
			for (i = 0; i < n; i++) {
				if (rxep[i] != NULL)
					rte_mempool_put(rxep[i]->pool, rxep[i]);
			}
	}

	/* Update counters for Tx. */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return nb_recycle_mbufs;
}

#endif
