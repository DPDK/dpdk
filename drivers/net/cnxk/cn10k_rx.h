/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN10K_RX_H__
#define __CN10K_RX_H__

#include <rte_ether.h>
#include <rte_vect.h>

#include <cnxk_ethdev.h>

#define NIX_RX_OFFLOAD_NONE	     (0)
#define NIX_RX_OFFLOAD_RSS_F	     BIT(0)
#define NIX_RX_OFFLOAD_PTYPE_F	     BIT(1)
#define NIX_RX_OFFLOAD_CHECKSUM_F    BIT(2)
#define NIX_RX_OFFLOAD_MARK_UPDATE_F BIT(3)
#define NIX_RX_OFFLOAD_TSTAMP_F	     BIT(4)
#define NIX_RX_OFFLOAD_VLAN_STRIP_F  BIT(5)

/* Flags to control cqe_to_mbuf conversion function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_RX_MULTI_SEG_F BIT(15)

#define CNXK_NIX_CQ_ENTRY_SZ 128
#define NIX_DESCS_PER_LOOP   4
#define CQE_CAST(x)	     ((struct nix_cqe_hdr_s *)(x))
#define CQE_SZ(x)	     ((x) * CNXK_NIX_CQ_ENTRY_SZ)

union mbuf_initializer {
	struct {
		uint16_t data_off;
		uint16_t refcnt;
		uint16_t nb_segs;
		uint16_t port;
	} fields;
	uint64_t value;
};

static __rte_always_inline uint64_t
nix_clear_data_off(uint64_t oldval)
{
	union mbuf_initializer mbuf_init = {.value = oldval};

	mbuf_init.fields.data_off = 0;
	return mbuf_init.value;
}

static __rte_always_inline struct rte_mbuf *
nix_get_mbuf_from_cqe(void *cq, const uint64_t data_off)
{
	rte_iova_t buff;

	/* Skip CQE, NIX_RX_PARSE_S and SG HDR(9 DWORDs) and peek buff addr */
	buff = *((rte_iova_t *)((uint64_t *)cq + 9));
	return (struct rte_mbuf *)(buff - data_off);
}

static __rte_always_inline uint32_t
nix_ptype_get(const void *const lookup_mem, const uint64_t in)
{
	const uint16_t *const ptype = lookup_mem;
	const uint16_t lh_lg_lf = (in & 0xFFF0000000000000) >> 52;
	const uint16_t tu_l2 = ptype[(in & 0x000FFFF000000000) >> 36];
	const uint16_t il4_tu = ptype[PTYPE_NON_TUNNEL_ARRAY_SZ + lh_lg_lf];

	return (il4_tu << PTYPE_NON_TUNNEL_WIDTH) | tu_l2;
}

static __rte_always_inline uint32_t
nix_rx_olflags_get(const void *const lookup_mem, const uint64_t in)
{
	const uint32_t *const ol_flags =
		(const uint32_t *)((const uint8_t *)lookup_mem +
				   PTYPE_ARRAY_SZ);

	return ol_flags[(in & 0xfff00000) >> 20];
}

static inline uint64_t
nix_update_match_id(const uint16_t match_id, uint64_t ol_flags,
		    struct rte_mbuf *mbuf)
{
	/* There is no separate bit to check match_id
	 * is valid or not? and no flag to identify it is an
	 * RTE_FLOW_ACTION_TYPE_FLAG vs RTE_FLOW_ACTION_TYPE_MARK
	 * action. The former case addressed through 0 being invalid
	 * value and inc/dec match_id pair when MARK is activated.
	 * The later case addressed through defining
	 * CNXK_FLOW_MARK_DEFAULT as value for
	 * RTE_FLOW_ACTION_TYPE_MARK.
	 * This would translate to not use
	 * CNXK_FLOW_ACTION_FLAG_DEFAULT - 1 and
	 * CNXK_FLOW_ACTION_FLAG_DEFAULT for match_id.
	 * i.e valid mark_id's are from
	 * 0 to CNXK_FLOW_ACTION_FLAG_DEFAULT - 2
	 */
	if (likely(match_id)) {
		ol_flags |= PKT_RX_FDIR;
		if (match_id != CNXK_FLOW_ACTION_FLAG_DEFAULT) {
			ol_flags |= PKT_RX_FDIR_ID;
			mbuf->hash.fdir.hi = match_id - 1;
		}
	}

	return ol_flags;
}

static __rte_always_inline void
nix_cqe_xtract_mseg(const union nix_rx_parse_u *rx, struct rte_mbuf *mbuf,
		    uint64_t rearm)
{
	const rte_iova_t *iova_list;
	struct rte_mbuf *head;
	const rte_iova_t *eol;
	uint8_t nb_segs;
	uint64_t sg;

	sg = *(const uint64_t *)(rx + 1);
	nb_segs = (sg >> 48) & 0x3;
	mbuf->nb_segs = nb_segs;
	mbuf->data_len = sg & 0xFFFF;
	sg = sg >> 16;

	eol = ((const rte_iova_t *)(rx + 1) + ((rx->desc_sizem1 + 1) << 1));
	/* Skip SG_S and first IOVA*/
	iova_list = ((const rte_iova_t *)(rx + 1)) + 2;
	nb_segs--;

	rearm = rearm & ~0xFFFF;

	head = mbuf;
	while (nb_segs) {
		mbuf->next = ((struct rte_mbuf *)*iova_list) - 1;
		mbuf = mbuf->next;

		__mempool_check_cookies(mbuf->pool, (void **)&mbuf, 1, 1);

		mbuf->data_len = sg & 0xFFFF;
		sg = sg >> 16;
		*(uint64_t *)(&mbuf->rearm_data) = rearm;
		nb_segs--;
		iova_list++;

		if (!nb_segs && (iova_list + 1 < eol)) {
			sg = *(const uint64_t *)(iova_list);
			nb_segs = (sg >> 48) & 0x3;
			head->nb_segs += nb_segs;
			iova_list = (const rte_iova_t *)(iova_list + 1);
		}
	}
	mbuf->next = NULL;
}

static __rte_always_inline void
cn10k_nix_cqe_to_mbuf(const struct nix_cqe_hdr_s *cq, const uint32_t tag,
		      struct rte_mbuf *mbuf, const void *lookup_mem,
		      const uint64_t val, const uint16_t flag)
{
	const union nix_rx_parse_u *rx =
		(const union nix_rx_parse_u *)((const uint64_t *)cq + 1);
	const uint16_t len = rx->pkt_lenm1 + 1;
	const uint64_t w1 = *(const uint64_t *)rx;
	uint64_t ol_flags = 0;

	/* Mark mempool obj as "get" as it is alloc'ed by NIX */
	__mempool_check_cookies(mbuf->pool, (void **)&mbuf, 1, 1);

	if (flag & NIX_RX_OFFLOAD_PTYPE_F)
		mbuf->packet_type = nix_ptype_get(lookup_mem, w1);
	else
		mbuf->packet_type = 0;

	if (flag & NIX_RX_OFFLOAD_RSS_F) {
		mbuf->hash.rss = tag;
		ol_flags |= PKT_RX_RSS_HASH;
	}

	if (flag & NIX_RX_OFFLOAD_CHECKSUM_F)
		ol_flags |= nix_rx_olflags_get(lookup_mem, w1);

	if (flag & NIX_RX_OFFLOAD_VLAN_STRIP_F) {
		if (rx->vtag0_gone) {
			ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
			mbuf->vlan_tci = rx->vtag0_tci;
		}
		if (rx->vtag1_gone) {
			ol_flags |= PKT_RX_QINQ | PKT_RX_QINQ_STRIPPED;
			mbuf->vlan_tci_outer = rx->vtag1_tci;
		}
	}

	if (flag & NIX_RX_OFFLOAD_MARK_UPDATE_F)
		ol_flags = nix_update_match_id(rx->match_id, ol_flags, mbuf);

	mbuf->ol_flags = ol_flags;
	*(uint64_t *)(&mbuf->rearm_data) = val;
	mbuf->pkt_len = len;

	if (flag & NIX_RX_MULTI_SEG_F) {
		nix_cqe_xtract_mseg(rx, mbuf, val);
	} else {
		mbuf->data_len = len;
		mbuf->next = NULL;
	}
}

static inline uint16_t
nix_rx_nb_pkts(struct cn10k_eth_rxq *rxq, const uint64_t wdata,
	       const uint16_t pkts, const uint32_t qmask)
{
	uint32_t available = rxq->available;

	/* Update the available count if cached value is not enough */
	if (unlikely(available < pkts)) {
		uint64_t reg, head, tail;

		/* Use LDADDA version to avoid reorder */
		reg = roc_atomic64_add_sync(wdata, rxq->cq_status);
		/* CQ_OP_STATUS operation error */
		if (reg & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) ||
		    reg & BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR))
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
cn10k_nix_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts,
		    const uint16_t flags)
{
	struct cn10k_eth_rxq *rxq = rx_queue;
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
		rte_prefetch_non_temporal(
			(void *)(desc + (CQE_SZ((head + 2) & qmask))));
		cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));

		mbuf = nix_get_mbuf_from_cqe(cq, data_off);

		cn10k_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init,
				      flags);
		cnxk_nix_mbuf_to_tstamp(mbuf, rxq->tstamp,
					(flags & NIX_RX_OFFLOAD_TSTAMP_F),
					(uint64_t *)((uint8_t *)mbuf + data_off)
					);
		rx_pkts[packets++] = mbuf;
		roc_prefetch_store_keep(mbuf);
		head++;
		head &= qmask;
	}

	rxq->head = head;
	rxq->available -= nb_pkts;

	/* Free all the CQs that we've processed */
	plt_write64((wdata | nb_pkts), rxq->cq_door);

	return nb_pkts;
}

#if defined(RTE_ARCH_ARM64)

static __rte_always_inline uint64_t
nix_vlan_update(const uint64_t w2, uint64_t ol_flags, uint8x16_t *f)
{
	if (w2 & BIT_ULL(21) /* vtag0_gone */) {
		ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		*f = vsetq_lane_u16((uint16_t)(w2 >> 32), *f, 5);
	}

	return ol_flags;
}

static __rte_always_inline uint64_t
nix_qinq_update(const uint64_t w2, uint64_t ol_flags, struct rte_mbuf *mbuf)
{
	if (w2 & BIT_ULL(23) /* vtag1_gone */) {
		ol_flags |= PKT_RX_QINQ | PKT_RX_QINQ_STRIPPED;
		mbuf->vlan_tci_outer = (uint16_t)(w2 >> 48);
	}

	return ol_flags;
}

static __rte_always_inline uint16_t
cn10k_nix_recv_pkts_vector(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t pkts, const uint16_t flags)
{
	struct cn10k_eth_rxq *rxq = rx_queue;
	uint16_t packets = 0;
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
			0xFF, 0xFF, /* pkt_type set as unknown */
			0xFF, 0xFF, /* pkt_type set as unknown */
			0,    1,    /* octet 1~0, low 16 bits pkt_len */
			0xFF, 0xFF, /* skip high 16 bits pkt_len, zero out */
			0,    1,    /* octet 1~0, 16 bits data_len */
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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
			ol_flags0 = PKT_RX_RSS_HASH;
			ol_flags1 = PKT_RX_RSS_HASH;
			ol_flags2 = PKT_RX_RSS_HASH;
			ol_flags3 = PKT_RX_RSS_HASH;
		} else {
			ol_flags0 = 0;
			ol_flags1 = 0;
			ol_flags2 = 0;
			ol_flags3 = 0;
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
			ol_flags0 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(0) + 38), ol_flags0,
				mbuf0);
			ol_flags1 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(1) + 38), ol_flags1,
				mbuf1);
			ol_flags2 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(2) + 38), ol_flags2,
				mbuf2);
			ol_flags3 = nix_update_match_id(
				*(uint16_t *)(cq0 + CQE_SZ(3) + 38), ol_flags3,
				mbuf3);
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
		roc_prefetch_store_keep(mbuf0);
		roc_prefetch_store_keep(mbuf1);
		roc_prefetch_store_keep(mbuf2);
		roc_prefetch_store_keep(mbuf3);

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		__mempool_check_cookies(mbuf0->pool, (void **)&mbuf0, 1, 1);
		__mempool_check_cookies(mbuf1->pool, (void **)&mbuf1, 1, 1);
		__mempool_check_cookies(mbuf2->pool, (void **)&mbuf2, 1, 1);
		__mempool_check_cookies(mbuf3->pool, (void **)&mbuf3, 1, 1);

		/* Advance head pointer and packets */
		head += NIX_DESCS_PER_LOOP;
		head &= qmask;
		packets += NIX_DESCS_PER_LOOP;
	}

	rxq->head = head;
	rxq->available -= packets;

	rte_io_wmb();
	/* Free all the CQs that we've processed */
	plt_write64((rxq->wdata | packets), rxq->cq_door);

	if (unlikely(pkts_left))
		packets += cn10k_nix_recv_pkts(rx_queue, &rx_pkts[packets],
					       pkts_left, flags);

	return packets;
}

#else

static inline uint16_t
cn10k_nix_recv_pkts_vector(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t pkts, const uint16_t flags)
{
	RTE_SET_USED(rx_queue);
	RTE_SET_USED(rx_pkts);
	RTE_SET_USED(pkts);
	RTE_SET_USED(flags);

	return 0;
}

#endif


#define RSS_F	  NIX_RX_OFFLOAD_RSS_F
#define PTYPE_F	  NIX_RX_OFFLOAD_PTYPE_F
#define CKSUM_F	  NIX_RX_OFFLOAD_CHECKSUM_F
#define MARK_F	  NIX_RX_OFFLOAD_MARK_UPDATE_F
#define TS_F      NIX_RX_OFFLOAD_TSTAMP_F
#define RX_VLAN_F NIX_RX_OFFLOAD_VLAN_STRIP_F

/* [RX_VLAN_F] [TS] [MARK] [CKSUM] [PTYPE] [RSS] */
#define NIX_RX_FASTPATH_MODES						       \
R(no_offload,			0, 0, 0, 0, 0, 0, NIX_RX_OFFLOAD_NONE)	       \
R(rss,				0, 0, 0, 0, 0, 1, RSS_F)		       \
R(ptype,			0, 0, 0, 0, 1, 0, PTYPE_F)		       \
R(ptype_rss,			0, 0, 0, 0, 1, 1, PTYPE_F | RSS_F)	       \
R(cksum,			0, 0, 0, 1, 0, 0, CKSUM_F)		       \
R(cksum_rss,			0, 0, 0, 1, 0, 1, CKSUM_F | RSS_F)	       \
R(cksum_ptype,			0, 0, 0, 1, 1, 0, CKSUM_F | PTYPE_F)	       \
R(cksum_ptype_rss,		0, 0, 0, 1, 1, 1, CKSUM_F | PTYPE_F | RSS_F)   \
R(mark,				0, 0, 1, 0, 0, 0, MARK_F)		       \
R(mark_rss,			0, 0, 1, 0, 0, 1, MARK_F | RSS_F)	       \
R(mark_ptype,			0, 0, 1, 0, 1, 0, MARK_F | PTYPE_F)	       \
R(mark_ptype_rss,		0, 0, 1, 0, 1, 1, MARK_F | PTYPE_F | RSS_F)    \
R(mark_cksum,			0, 0, 1, 1, 0, 0, MARK_F | CKSUM_F)	       \
R(mark_cksum_rss,		0, 0, 1, 1, 0, 1, MARK_F | CKSUM_F | RSS_F)    \
R(mark_cksum_ptype,		0, 0, 1, 1, 1, 0, MARK_F | CKSUM_F | PTYPE_F)  \
R(mark_cksum_ptype_rss,		0, 0, 1, 1, 1, 1,			       \
			MARK_F | CKSUM_F | PTYPE_F | RSS_F)		       \
R(ts,				0, 1, 0, 0, 0, 0, TS_F)			       \
R(ts_rss,			0, 1, 0, 0, 0, 1, TS_F | RSS_F)		       \
R(ts_ptype,			0, 1, 0, 0, 1, 0, TS_F | PTYPE_F)	       \
R(ts_ptype_rss,			0, 1, 0, 0, 1, 1, TS_F | PTYPE_F | RSS_F)      \
R(ts_cksum,			0, 1, 0, 1, 0, 0, TS_F | CKSUM_F)	       \
R(ts_cksum_rss,			0, 1, 0, 1, 0, 1, TS_F | CKSUM_F | RSS_F)      \
R(ts_cksum_ptype,		0, 1, 0, 1, 1, 0, TS_F | CKSUM_F | PTYPE_F)    \
R(ts_cksum_ptype_rss,		0, 1, 0, 1, 1, 1,			       \
			TS_F | CKSUM_F | PTYPE_F | RSS_F)		       \
R(ts_mark,			0, 1, 1, 0, 0, 0, TS_F | MARK_F)	       \
R(ts_mark_rss,			0, 1, 1, 0, 0, 1, TS_F | MARK_F | RSS_F)       \
R(ts_mark_ptype,		0, 1, 1, 0, 1, 0, TS_F | MARK_F | PTYPE_F)     \
R(ts_mark_ptype_rss,		0, 1, 1, 0, 1, 1,			       \
			TS_F | MARK_F | PTYPE_F | RSS_F)		       \
R(ts_mark_cksum,		0, 1, 1, 1, 0, 0, TS_F | MARK_F | CKSUM_F)     \
R(ts_mark_cksum_rss,		0, 1, 1, 1, 0, 1,			       \
			TS_F | MARK_F | CKSUM_F | RSS_F)		       \
R(ts_mark_cksum_ptype,		0, 1, 1, 1, 1, 0,			       \
			TS_F | MARK_F | CKSUM_F | PTYPE_F)		       \
R(ts_mark_cksum_ptype_rss,	0, 1, 1, 1, 1, 1,			       \
			TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(vlan,				1, 0, 0, 0, 0, 0, RX_VLAN_F)		       \
R(vlan_rss,			1, 0, 0, 0, 0, 1, RX_VLAN_F | RSS_F)	       \
R(vlan_ptype,			1, 0, 0, 0, 1, 0, RX_VLAN_F | PTYPE_F)	       \
R(vlan_ptype_rss,		1, 0, 0, 0, 1, 1, RX_VLAN_F | PTYPE_F | RSS_F) \
R(vlan_cksum,			1, 0, 0, 1, 0, 0, RX_VLAN_F | CKSUM_F)	       \
R(vlan_cksum_rss,		1, 0, 0, 1, 0, 1, RX_VLAN_F | CKSUM_F | RSS_F) \
R(vlan_cksum_ptype,		1, 0, 0, 1, 1, 0,			       \
			RX_VLAN_F | CKSUM_F | PTYPE_F)			       \
R(vlan_cksum_ptype_rss,		1, 0, 0, 1, 1, 1,			       \
			RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)		       \
R(vlan_mark,			1, 0, 1, 0, 0, 0, RX_VLAN_F | MARK_F)	       \
R(vlan_mark_rss,		1, 0, 1, 0, 0, 1, RX_VLAN_F | MARK_F | RSS_F)  \
R(vlan_mark_ptype,		1, 0, 1, 0, 1, 0, RX_VLAN_F | MARK_F | PTYPE_F)\
R(vlan_mark_ptype_rss,		1, 0, 1, 0, 1, 1,			       \
			RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)		       \
R(vlan_mark_cksum,		1, 0, 1, 1, 0, 0, RX_VLAN_F | MARK_F | CKSUM_F)\
R(vlan_mark_cksum_rss,		1, 0, 1, 1, 0, 1,			       \
			RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)		       \
R(vlan_mark_cksum_ptype,	1, 0, 1, 1, 1, 0,			       \
			RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)		       \
R(vlan_mark_cksum_ptype_rss,	1, 0, 1, 1, 1, 1,			       \
			RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(vlan_ts,			1, 1, 0, 0, 0, 0, RX_VLAN_F | TS_F)	       \
R(vlan_ts_rss,			1, 1, 0, 0, 0, 1, RX_VLAN_F | TS_F | RSS_F)    \
R(vlan_ts_ptype,		1, 1, 0, 0, 1, 0, RX_VLAN_F | TS_F | PTYPE_F)  \
R(vlan_ts_ptype_rss,		1, 1, 0, 0, 1, 1,			       \
			RX_VLAN_F | TS_F | PTYPE_F | RSS_F)		       \
R(vlan_ts_cksum,		1, 1, 0, 1, 0, 0, RX_VLAN_F | TS_F | CKSUM_F)  \
R(vlan_ts_cksum_rss,		1, 1, 0, 1, 0, 1,			       \
			RX_VLAN_F | TS_F | CKSUM_F | RSS_F)		       \
R(vlan_ts_cksum_ptype,		1, 1, 0, 1, 1, 0,			       \
			RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)		       \
R(vlan_ts_cksum_ptype_rss,	1, 1, 0, 1, 1, 1,			       \
			RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(vlan_ts_mark,			1, 1, 1, 0, 0, 0, RX_VLAN_F | TS_F | MARK_F)   \
R(vlan_ts_mark_rss,		1, 1, 1, 0, 0, 1,			       \
			RX_VLAN_F | TS_F | MARK_F | RSS_F)		       \
R(vlan_ts_mark_ptype,		1, 1, 1, 0, 1, 0,			       \
			RX_VLAN_F | TS_F | MARK_F | PTYPE_F)		       \
R(vlan_ts_mark_ptype_rss,	1, 1, 1, 0, 1, 1,			       \
			RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F)	       \
R(vlan_ts_mark_cksum,		1, 1, 1, 1, 0, 0,			       \
			RX_VLAN_F | TS_F | MARK_F | CKSUM_F)		       \
R(vlan_ts_mark_cksum_rss,	1, 1, 1, 1, 0, 1,			       \
			RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F)	       \
R(vlan_ts_mark_cksum_ptype,	1, 1, 1, 1, 1, 0,			       \
			RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)	       \
R(vlan_ts_mark_cksum_ptype_rss,	1, 1, 1, 1, 1, 1,			       \
			RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define R(name, f5, f4, f3, f2, f1, f0, flags)				       \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_##name(          \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
									       \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_mseg_##name(     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
									       \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_vec_##name(      \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);

NIX_RX_FASTPATH_MODES
#undef R

#endif /* __CN10K_RX_H__ */
