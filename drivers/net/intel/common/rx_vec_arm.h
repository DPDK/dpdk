/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#ifndef _COMMON_INTEL_RX_VEC_ARM_H_
#define _COMMON_INTEL_RX_VEC_ARM_H_

#include <stdint.h>

#include <ethdev_driver.h>
#include <rte_io.h>
#include <rte_vect.h>

#include "rx.h"

static inline int
_ci_rxq_rearm_get_bufs(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	volatile union ci_rx_desc *rxdp;
	int i;

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	if (rte_mempool_get_bulk(rxq->mp, (void **)rxp, rearm_thresh) < 0) {
		if (rxq->rxrearm_nb + rearm_thresh >= rxq->nb_rx_desc) {
			uint64x2_t zero = vdupq_n_u64(0);

			for (i = 0; i < CI_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i].mbuf = &rxq->fake_mbuf;
				vst1q_u64(RTE_CAST_PTR(uint64_t *, &rxdp[i]), zero);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed += rearm_thresh;
		return -1;
	}
	return 0;
}

static __rte_always_inline void
_ci_rxq_rearm_neon(struct ci_rx_queue *rxq)
{
	const uint64x2_t zero = vdupq_n_u64(0);
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	struct ci_rx_entry *rxp = &rxq->sw_ring[rxq->rxrearm_start];
	volatile union ci_rx_desc *rxdp;
	int i;

	const uint8x8_t mbuf_init = vld1_u8((uint8_t *)&rxq->mbuf_initializer);

	rxdp = &rxq->rx_ring[rxq->rxrearm_start];

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < rearm_thresh; i += 2, rxp += 2, rxdp += 2) {
		struct rte_mbuf *mb0 = rxp[0].mbuf;
		struct rte_mbuf *mb1 = rxp[1].mbuf;

		/*
		 * Flush mbuf with pkt template.
		 * Data to be rearmed is 6 bytes long.
		 */
		vst1_u8((uint8_t *)&mb0->rearm_data, mbuf_init);
		vst1_u8((uint8_t *)&mb1->rearm_data, mbuf_init);
#if RTE_IOVA_IN_MBUF
		const uint64_t addr0 = mb0->buf_iova + RTE_PKTMBUF_HEADROOM;
		const uint64_t addr1 = mb1->buf_iova + RTE_PKTMBUF_HEADROOM;
#else
		const uint64_t addr0 = (uintptr_t)RTE_PTR_ADD(mb0->buf_addr, RTE_PKTMBUF_HEADROOM);
		const uint64_t addr1 = (uintptr_t)RTE_PTR_ADD(mb1->buf_addr, RTE_PKTMBUF_HEADROOM);
#endif
		uint64x2_t dma_addr0 = vsetq_lane_u64(addr0, zero, 0);
		uint64x2_t dma_addr1 = vsetq_lane_u64(addr1, zero, 0);
		/* flush desc with pa dma_addr */
		vst1q_u64(RTE_CAST_PTR(uint64_t *, &rxdp[0]), dma_addr0);
		vst1q_u64(RTE_CAST_PTR(uint64_t *, &rxdp[1]), dma_addr1);
	}
}

static __rte_always_inline void
ci_rxq_rearm(struct ci_rx_queue *rxq)
{
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	uint16_t rx_id;

	/* Pull 'n' more MBUFs into the software ring */
	if (_ci_rxq_rearm_get_bufs(rxq) < 0)
		return;

	_ci_rxq_rearm_neon(rxq);

	rxq->rxrearm_start += rearm_thresh;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= rearm_thresh;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	rte_write32_wc(rte_cpu_to_le_32(rx_id), rxq->qrx_tail);
}

#endif /* _COMMON_INTEL_RX_VEC_ARM_H_ */
