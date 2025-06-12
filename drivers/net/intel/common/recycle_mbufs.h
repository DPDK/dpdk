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

#endif
