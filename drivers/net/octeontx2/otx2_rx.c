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
		rte_prefetch_non_temporal((void *)(desc +
					(CQE_SZ((head + 2) & qmask))));
		cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));

		mbuf = nix_get_mbuf_from_cqe(cq, data_off);

		otx2_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init,
				     flags);
		otx2_nix_mbuf_to_tstamp(mbuf, rxq->tstamp, flags,
				(uint64_t *)((uint8_t *)mbuf + data_off));
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

#if defined(RTE_ARCH_ARM64)

static __rte_always_inline uint64_t
nix_vlan_update(const uint64_t w2, uint64_t ol_flags, uint8x16_t *f)
{
	if (w2 & BIT_ULL(21) /* vtag0_gone */) {
		ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		*f = vsetq_lane_u16((uint16_t)(w2 >> 32), *f, 5);
	}

	return ol_flags;
}

static __rte_always_inline uint64_t
nix_qinq_update(const uint64_t w2, uint64_t ol_flags, struct rte_mbuf *mbuf)
{
	if (w2 & BIT_ULL(23) /* vtag1_gone */) {
		ol_flags |= RTE_MBUF_F_RX_QINQ | RTE_MBUF_F_RX_QINQ_STRIPPED;
		mbuf->vlan_tci_outer = (uint16_t)(w2 >> 48);
	}

	return ol_flags;
}

static __rte_always_inline uint16_t
nix_recv_pkts_vector(void *rx_queue, struct rte_mbuf **rx_pkts,
		     uint16_t pkts, const uint16_t flags)
{
	struct otx2_eth_rxq *rxq = rx_queue; uint16_t packets = 0;
	uint64x2_t cq0_w8, cq1_w8, cq2_w8, cq3_w8, mbuf01, mbuf23;
	const uint64_t mbuf_initializer = rxq->mbuf_initializer;
	const uint64x2_t data_off = vdupq_n_u64(rxq->data_off);
	uint64_t ol_flags0, ol_flags1, ol_flags2, ol_flags3;
	uint64x2_t rearm0 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm1 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm2 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm3 = vdupq_n_u64(mbuf_initializer);
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3;
	const uint16_t *lookup_mem = rxq->lookup_mem;
	const uint32_t qmask = rxq->qmask;
	const uint64_t wdata = rxq->wdata;
	const uintptr_t desc = rxq->desc;
	uint8x16_t f0, f1, f2, f3;
	uint32_t head = rxq->head;
	uint16_t pkts_left;

	pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);
	pkts_left = pkts & (NIX_DESCS_PER_LOOP - 1);

	/* Packets has to be floor-aligned to NIX_DESCS_PER_LOOP */
	pkts = RTE_ALIGN_FLOOR(pkts, NIX_DESCS_PER_LOOP);

	while (packets < pkts) {
		/* Exit loop if head is about to wrap and become unaligned */
		if (((head + NIX_DESCS_PER_LOOP - 1) & qmask) <
				NIX_DESCS_PER_LOOP) {
			pkts_left += (pkts - packets);
			break;
		}

		const uintptr_t cq0 = desc + CQE_SZ(head);

		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(8)));
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(9)));
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(10)));
		rte_prefetch_non_temporal((void *)(cq0 + CQE_SZ(11)));

		/* Get NIX_RX_SG_S for size and buffer pointer */
		cq0_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(0) + 64));
		cq1_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(1) + 64));
		cq2_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(2) + 64));
		cq3_w8 = vld1q_u64((uint64_t *)(cq0 + CQE_SZ(3) + 64));

		/* Extract mbuf from NIX_RX_SG_S */
		mbuf01 = vzip2q_u64(cq0_w8, cq1_w8);
		mbuf23 = vzip2q_u64(cq2_w8, cq3_w8);
		mbuf01 = vqsubq_u64(mbuf01, data_off);
		mbuf23 = vqsubq_u64(mbuf23, data_off);

		/* Move mbufs to scalar registers for future use */
		mbuf0 = (struct rte_mbuf *)vgetq_lane_u64(mbuf01, 0);
		mbuf1 = (struct rte_mbuf *)vgetq_lane_u64(mbuf01, 1);
		mbuf2 = (struct rte_mbuf *)vgetq_lane_u64(mbuf23, 0);
		mbuf3 = (struct rte_mbuf *)vgetq_lane_u64(mbuf23, 1);

		/* Mask to get packet len from NIX_RX_SG_S */
		const uint8x16_t shuf_msk = {
			0xFF, 0xFF,   /* pkt_type set as unknown */
			0xFF, 0xFF,   /* pkt_type set as unknown */
			0, 1,         /* octet 1~0, low 16 bits pkt_len */
			0xFF, 0xFF,   /* skip high 16 bits pkt_len, zero out */
			0, 1,         /* octet 1~0, 16 bits data_len */
			0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF
			};

		/* Form the rx_descriptor_fields1 with pkt_len and data_len */
		f0 = vqtbl1q_u8(cq0_w8, shuf_msk);
		f1 = vqtbl1q_u8(cq1_w8, shuf_msk);
		f2 = vqtbl1q_u8(cq2_w8, shuf_msk);
		f3 = vqtbl1q_u8(cq3_w8, shuf_msk);

		/* Load CQE word0 and word 1 */
		uint64_t cq0_w0 = ((uint64_t *)(cq0 + CQE_SZ(0)))[0];
		uint64_t cq0_w1 = ((uint64_t *)(cq0 + CQE_SZ(0)))[1];
		uint64_t cq1_w0 = ((uint64_t *)(cq0 + CQE_SZ(1)))[0];
		uint64_t cq1_w1 = ((uint64_t *)(cq0 + CQE_SZ(1)))[1];
		uint64_t cq2_w0 = ((uint64_t *)(cq0 + CQE_SZ(2)))[0];
		uint64_t cq2_w1 = ((uint64_t *)(cq0 + CQE_SZ(2)))[1];
		uint64_t cq3_w0 = ((uint64_t *)(cq0 + CQE_SZ(3)))[0];
		uint64_t cq3_w1 = ((uint64_t *)(cq0 + CQE_SZ(3)))[1];

		if (flags & NIX_RX_OFFLOAD_RSS_F) {
			/* Fill rss in the rx_descriptor_fields1 */
			f0 = vsetq_lane_u32(cq0_w0, f0, 3);
			f1 = vsetq_lane_u32(cq1_w0, f1, 3);
			f2 = vsetq_lane_u32(cq2_w0, f2, 3);
			f3 = vsetq_lane_u32(cq3_w0, f3, 3);
			ol_flags0 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags1 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags2 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags3 = RTE_MBUF_F_RX_RSS_HASH;
		} else {
			ol_flags0 = 0; ol_flags1 = 0;
			ol_flags2 = 0; ol_flags3 = 0;
		}

		if (flags & NIX_RX_OFFLOAD_PTYPE_F) {
			/* Fill packet_type in the rx_descriptor_fields1 */
			f0 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq0_w1),
					    f0, 0);
			f1 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq1_w1),
					    f1, 0);
			f2 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq2_w1),
					    f2, 0);
			f3 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq3_w1),
					    f3, 0);
		}

		if (flags & NIX_RX_OFFLOAD_CHECKSUM_F) {
			ol_flags0 |= nix_rx_olflags_get(lookup_mem, cq0_w1);
			ol_flags1 |= nix_rx_olflags_get(lookup_mem, cq1_w1);
			ol_flags2 |= nix_rx_olflags_get(lookup_mem, cq2_w1);
			ol_flags3 |= nix_rx_olflags_get(lookup_mem, cq3_w1);
		}

		if (flags & NIX_RX_OFFLOAD_VLAN_STRIP_F) {
			uint64_t cq0_w2 = *(uint64_t *)(cq0 + CQE_SZ(0) + 16);
			uint64_t cq1_w2 = *(uint64_t *)(cq0 + CQE_SZ(1) + 16);
			uint64_t cq2_w2 = *(uint64_t *)(cq0 + CQE_SZ(2) + 16);
			uint64_t cq3_w2 = *(uint64_t *)(cq0 + CQE_SZ(3) + 16);

			ol_flags0 = nix_vlan_update(cq0_w2, ol_flags0, &f0);
			ol_flags1 = nix_vlan_update(cq1_w2, ol_flags1, &f1);
			ol_flags2 = nix_vlan_update(cq2_w2, ol_flags2, &f2);
			ol_flags3 = nix_vlan_update(cq3_w2, ol_flags3, &f3);

			ol_flags0 = nix_qinq_update(cq0_w2, ol_flags0, mbuf0);
			ol_flags1 = nix_qinq_update(cq1_w2, ol_flags1, mbuf1);
			ol_flags2 = nix_qinq_update(cq2_w2, ol_flags2, mbuf2);
			ol_flags3 = nix_qinq_update(cq3_w2, ol_flags3, mbuf3);
		}

		if (flags & NIX_RX_OFFLOAD_MARK_UPDATE_F) {
			ol_flags0 = nix_update_match_id(*(uint16_t *)
				    (cq0 + CQE_SZ(0) + 38), ol_flags0, mbuf0);
			ol_flags1 = nix_update_match_id(*(uint16_t *)
				    (cq0 + CQE_SZ(1) + 38), ol_flags1, mbuf1);
			ol_flags2 = nix_update_match_id(*(uint16_t *)
				    (cq0 + CQE_SZ(2) + 38), ol_flags2, mbuf2);
			ol_flags3 = nix_update_match_id(*(uint16_t *)
				    (cq0 + CQE_SZ(3) + 38), ol_flags3, mbuf3);
		}

		/* Form rearm_data with ol_flags */
		rearm0 = vsetq_lane_u64(ol_flags0, rearm0, 1);
		rearm1 = vsetq_lane_u64(ol_flags1, rearm1, 1);
		rearm2 = vsetq_lane_u64(ol_flags2, rearm2, 1);
		rearm3 = vsetq_lane_u64(ol_flags3, rearm3, 1);

		/* Update rx_descriptor_fields1 */
		vst1q_u64((uint64_t *)mbuf0->rx_descriptor_fields1, f0);
		vst1q_u64((uint64_t *)mbuf1->rx_descriptor_fields1, f1);
		vst1q_u64((uint64_t *)mbuf2->rx_descriptor_fields1, f2);
		vst1q_u64((uint64_t *)mbuf3->rx_descriptor_fields1, f3);

		/* Update rearm_data */
		vst1q_u64((uint64_t *)mbuf0->rearm_data, rearm0);
		vst1q_u64((uint64_t *)mbuf1->rearm_data, rearm1);
		vst1q_u64((uint64_t *)mbuf2->rearm_data, rearm2);
		vst1q_u64((uint64_t *)mbuf3->rearm_data, rearm3);

		/* Update that no more segments */
		mbuf0->next = NULL;
		mbuf1->next = NULL;
		mbuf2->next = NULL;
		mbuf3->next = NULL;

		/* Store the mbufs to rx_pkts */
		vst1q_u64((uint64_t *)&rx_pkts[packets], mbuf01);
		vst1q_u64((uint64_t *)&rx_pkts[packets + 2], mbuf23);

		/* Prefetch mbufs */
		otx2_prefetch_store_keep(mbuf0);
		otx2_prefetch_store_keep(mbuf1);
		otx2_prefetch_store_keep(mbuf2);
		otx2_prefetch_store_keep(mbuf3);

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf0->pool, (void **)&mbuf0, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf1->pool, (void **)&mbuf1, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf2->pool, (void **)&mbuf2, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf3->pool, (void **)&mbuf3, 1, 1);

		/* Advance head pointer and packets */
		head += NIX_DESCS_PER_LOOP; head &= qmask;
		packets += NIX_DESCS_PER_LOOP;
	}

	rxq->head = head;
	rxq->available -= packets;

	rte_io_wmb();
	/* Free all the CQs that we've processed */
	otx2_write64((rxq->wdata | packets), rxq->cq_door);

	if (unlikely(pkts_left))
		packets += nix_recv_pkts(rx_queue, &rx_pkts[packets],
					 pkts_left, flags);

	return packets;
}

#else

static inline uint16_t
nix_recv_pkts_vector(void *rx_queue, struct rte_mbuf **rx_pkts,
		     uint16_t pkts, const uint16_t flags)
{
	RTE_SET_USED(rx_queue);
	RTE_SET_USED(rx_pkts);
	RTE_SET_USED(pkts);
	RTE_SET_USED(flags);

	return 0;
}

#endif

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			       \
static uint16_t __rte_noinline	__rte_hot					       \
otx2_nix_recv_pkts_ ## name(void *rx_queue,				       \
			struct rte_mbuf **rx_pkts, uint16_t pkts)	       \
{									       \
	return nix_recv_pkts(rx_queue, rx_pkts, pkts, (flags));		       \
}									       \
									       \
static uint16_t __rte_noinline	__rte_hot					       \
otx2_nix_recv_pkts_mseg_ ## name(void *rx_queue,			       \
			struct rte_mbuf **rx_pkts, uint16_t pkts)	       \
{									       \
	return nix_recv_pkts(rx_queue, rx_pkts, pkts,			       \
			     (flags) | NIX_RX_MULTI_SEG_F);		       \
}									       \
									       \
static uint16_t __rte_noinline	__rte_hot					       \
otx2_nix_recv_pkts_vec_ ## name(void *rx_queue,				       \
			struct rte_mbuf **rx_pkts, uint16_t pkts)	       \
{									       \
	/* TSTMP is not supported by vector */				       \
	if ((flags) & NIX_RX_OFFLOAD_TSTAMP_F)				       \
		return 0;						       \
	return nix_recv_pkts_vector(rx_queue, rx_pkts, pkts, (flags));	       \
}									       \

NIX_RX_FASTPATH_MODES
#undef R

static inline void
pick_rx_func(struct rte_eth_dev *eth_dev,
	     const eth_rx_burst_t rx_burst[2][2][2][2][2][2][2])
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* [SEC] [TSTMP] [MARK] [VLAN] [CKSUM] [PTYPE] [RSS] */
	eth_dev->rx_pkt_burst = rx_burst
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_SECURITY_F)]
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

	const eth_rx_burst_t nix_eth_rx_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
	[f6][f5][f4][f3][f2][f1][f0] =  otx2_nix_recv_pkts_ ## name,

NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_burst_mseg[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
	[f6][f5][f4][f3][f2][f1][f0] =  otx2_nix_recv_pkts_mseg_ ## name,

NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_vec_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
	[f6][f5][f4][f3][f2][f1][f0] =  otx2_nix_recv_pkts_vec_ ## name,

NIX_RX_FASTPATH_MODES
#undef R
	};

	/* For PTP enabled, scalar rx function should be chosen as most of the
	 * PTP apps are implemented to rx burst 1 pkt.
	 */
	if (dev->scalar_ena || dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
		pick_rx_func(eth_dev, nix_eth_rx_burst);
	else
		pick_rx_func(eth_dev, nix_eth_rx_vec_burst);

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
		pick_rx_func(eth_dev, nix_eth_rx_burst_mseg);

	/* Copy multi seg version with no offload for tear down sequence */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		dev->rx_pkt_burst_no_offload =
			nix_eth_rx_burst_mseg[0][0][0][0][0][0][0];
	rte_mb();
}
