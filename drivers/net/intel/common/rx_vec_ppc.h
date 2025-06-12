/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#ifndef _COMMON_INTEL_RX_VEC_PPC_H_
#define _COMMON_INTEL_RX_VEC_PPC_H_

#include <stdint.h>

#include <ethdev_driver.h>
#include <rte_io.h>
#include <rte_altivec.h>

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
			__vector unsigned long dma_addr0 = (__vector unsigned long){};

			for (i = 0; i < CI_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i].mbuf = &rxq->fake_mbuf;
				vec_st(dma_addr0, 0,
					RTE_CAST_PTR(__vector unsigned long *, &rxdp[i]));
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed += rearm_thresh;
		return -1;
	}
	return 0;
}

/*
 * SSE code path can handle both 16-byte and 32-byte descriptors with one code
 * path, as we only ever write 16 bytes at a time.
 */
static __rte_always_inline void
_ci_rxq_rearm_altivec(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	const uint16_t rearm_thresh = CI_VPMD_RX_REARM_THRESH;
	__vector unsigned long hdroom =
			(__vector unsigned long){RTE_PKTMBUF_HEADROOM, RTE_PKTMBUF_HEADROOM};
	int i;

	volatile union ci_rx_desc *rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < rearm_thresh; i += 2, rxep += 2) {
		__vector unsigned long vaddr0, vaddr1;
		struct rte_mbuf *mb0 = rxep[0].mbuf;
		struct rte_mbuf *mb1 = rxep[1].mbuf;

		/* Flush mbuf with pkt template.
		 * Data to be rearmed is 6 bytes long.
		 * Though, RX will overwrite ol_flags that are coming next
		 * anyway. So overwrite whole 8 bytes with one load:
		 * 6 bytes of rearm_data plus first 2 bytes of ol_flags.
		 */
		*(uint64_t *)&mb0->rearm_data = rxq->mbuf_initializer;
		*(uint64_t *)&mb1->rearm_data = rxq->mbuf_initializer;

		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		vaddr0 = vec_ld(0, (__vector unsigned long *)&mb0->buf_addr);
		vaddr1 = vec_ld(0, (__vector unsigned long *)&mb1->buf_addr);

#if RTE_IOVA_IN_MBUF
		/* convert pa to dma_addr hdr/data */
		vaddr0 = vec_mergel(vaddr0, vaddr0);
		vaddr1 = vec_mergel(vaddr1, vaddr1);
#else
		/* convert va to dma_addr hdr/data */
		vaddr0 = vec_mergeh(vaddr0, vaddr0);
		vaddr1 = vec_mergeh(vaddr1, vaddr1);
#endif

		/* add headroom to pa values */
		vaddr0 = vec_add(vaddr0, hdroom);
		vaddr1 = vec_add(vaddr1, hdroom);

		/* flush desc with pa dma_addr */
		vec_st(vaddr0, 0, RTE_CAST_PTR(__vector unsigned long *, rxdp++));
		vec_st(vaddr1, 0, RTE_CAST_PTR(__vector unsigned long *, rxdp++));
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

	_ci_rxq_rearm_altivec(rxq);

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
