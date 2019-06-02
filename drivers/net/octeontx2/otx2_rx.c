/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_vect.h>

#include "otx2_ethdev.h"
#include "otx2_rx.h"

#define NIX_DESCS_PER_LOOP	4
#define CQE_CAST(x)		((struct nix_cqe_hdr_s *)(x))
#define CQE_SZ(x)		((x) * NIX_CQ_ENTRY_SZ)

static inline uint16_t
nix_rx_nb_pkts(struct otx2_eth_rxq *rxq, const uint64_t wdata,
	       const uint16_t pkts, const uint32_t qmask)
{
	uint32_t available = rxq->available;

	/* Update the available count if cached value is not enough */
	if (unlikely(available < pkts)) {
		uint64_t reg, head, tail;

		/* Use LDADDA version to avoid reorder */
		reg = otx2_atomic64_add_sync(wdata, rxq->cq_status);
		/* CQ_OP_STATUS operation error */
		if (reg & BIT_ULL(CQ_OP_STAT_OP_ERR) ||
		    reg & BIT_ULL(CQ_OP_STAT_CQ_ERR))
			return 0;

		tail = reg & 0xFFFFF;
		head = (reg >> 20) & 0xFFFFF;
		if (tail < head)
			available = tail - head + qmask + 1;
		else
			available = tail - head;

		rxq->available = available;
	}

	return RTE_MIN(pkts, available);
}

static __rte_always_inline uint16_t
nix_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	      uint16_t pkts, const uint16_t flags)
{
	struct otx2_eth_rxq *rxq = rx_queue;
	const uint64_t mbuf_init = rxq->mbuf_initializer;
	const void *lookup_mem = rxq->lookup_mem;
	const uint64_t data_off = rxq->data_off;
	const uintptr_t desc = rxq->desc;
	const uint64_t wdata = rxq->wdata;
	const uint32_t qmask = rxq->qmask;
	uint16_t packets = 0, nb_pkts;
	uint32_t head = rxq->head;
	struct nix_cqe_hdr_s *cq;
	struct rte_mbuf *mbuf;

	nb_pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);

	while (packets < nb_pkts) {
		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal((void *)(desc + (CQE_SZ(head + 2))));
		cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));

		mbuf = nix_get_mbuf_from_cqe(cq, data_off);

		otx2_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init,
				     flags);
		otx2_nix_mbuf_to_tstamp(mbuf, rxq->tstamp, flags);
		rx_pkts[packets++] = mbuf;
		otx2_prefetch_store_keep(mbuf);
		head++;
		head &= qmask;
	}

	rxq->head = head;
	rxq->available -= nb_pkts;

	/* Free all the CQs that we've processed */
	otx2_write64((wdata | nb_pkts), rxq->cq_door);

	return nb_pkts;
}


#define R(name, f5, f4, f3, f2, f1, f0, flags)				\
static uint16_t __rte_noinline	__hot					       \
otx2_nix_recv_pkts_ ## name(void *rx_queue,				       \
			struct rte_mbuf **rx_pkts, uint16_t pkts)	       \
{									       \
	return nix_recv_pkts(rx_queue, rx_pkts, pkts, (flags));		       \
}									       \
									       \
static uint16_t __rte_noinline	__hot					       \
otx2_nix_recv_pkts_mseg_ ## name(void *rx_queue,			       \
			struct rte_mbuf **rx_pkts, uint16_t pkts)	       \
{									       \
	return nix_recv_pkts(rx_queue, rx_pkts, pkts,			       \
			     (flags) | NIX_RX_MULTI_SEG_F);		       \
}									       \

NIX_RX_FASTPATH_MODES
#undef R

static inline void
pick_rx_func(struct rte_eth_dev *eth_dev,
	     const eth_rx_burst_t rx_burst[2][2][2][2][2][2])
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* [TSTMP] [MARK] [VLAN] [CKSUM] [PTYPE] [RSS] */
	eth_dev->rx_pkt_burst = rx_burst
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_TSTAMP_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_CHECKSUM_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_PTYPE_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_RSS_F)];
}

void
otx2_eth_set_rx_function(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	const eth_rx_burst_t nix_eth_rx_burst[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)				\
	[f5][f4][f3][f2][f1][f0] =  otx2_nix_recv_pkts_ ## name,

NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_burst_mseg[2][2][2][2][2][2] = {
#define R(name, f5, f4, f3, f2, f1, f0, flags)				\
	[f5][f4][f3][f2][f1][f0] =  otx2_nix_recv_pkts_mseg_ ## name,

NIX_RX_FASTPATH_MODES
#undef R
	};

	pick_rx_func(eth_dev, nix_eth_rx_burst);

	if (dev->rx_offloads & DEV_RX_OFFLOAD_SCATTER)
		pick_rx_func(eth_dev, nix_eth_rx_burst_mseg);

	/* Copy multi seg version with no offload for tear down sequence */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		dev->rx_pkt_burst_no_offload =
			nix_eth_rx_burst_mseg[0][0][0][0][0][0];
	rte_mb();
}
