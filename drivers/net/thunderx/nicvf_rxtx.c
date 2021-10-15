/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <ethdev_driver.h>
#include <rte_ether.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>

#include "base/nicvf_plat.h"

#include "nicvf_ethdev.h"
#include "nicvf_rxtx.h"
#include "nicvf_logs.h"

static inline void __rte_hot
fill_sq_desc_header(union sq_entry_t *entry, struct rte_mbuf *pkt)
{
	/* Local variable sqe to avoid read from sq desc memory*/
	union sq_entry_t sqe;
	uint64_t ol_flags;

	/* Fill SQ header descriptor */
	sqe.buff[0] = 0;
	sqe.hdr.subdesc_type = SQ_DESC_TYPE_HEADER;
	/* Number of sub-descriptors following this one */
	sqe.hdr.subdesc_cnt = pkt->nb_segs;
	sqe.hdr.tot_len = pkt->pkt_len;

	ol_flags = pkt->ol_flags & NICVF_TX_OFFLOAD_MASK;
	if (unlikely(ol_flags)) {
		/* L4 cksum */
		uint64_t l4_flags = ol_flags & RTE_MBUF_F_TX_L4_MASK;
		if (l4_flags == RTE_MBUF_F_TX_TCP_CKSUM)
			sqe.hdr.csum_l4 = SEND_L4_CSUM_TCP;
		else if (l4_flags == RTE_MBUF_F_TX_UDP_CKSUM)
			sqe.hdr.csum_l4 = SEND_L4_CSUM_UDP;
		else
			sqe.hdr.csum_l4 = SEND_L4_CSUM_DISABLE;

		sqe.hdr.l3_offset = pkt->l2_len;
		sqe.hdr.l4_offset = pkt->l3_len + pkt->l2_len;

		/* L3 cksum */
		if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
			sqe.hdr.csum_l3 = 1;
	}

	entry->buff[0] = sqe.buff[0];
}

static inline void __rte_hot
fill_sq_desc_header_zero_w1(union sq_entry_t *entry,
				struct rte_mbuf *pkt)
{
	fill_sq_desc_header(entry, pkt);
	entry->buff[1] = 0ULL;
}

void __rte_hot
nicvf_single_pool_free_xmited_buffers(struct nicvf_txq *sq)
{
	int j = 0;
	uint32_t curr_head;
	uint32_t head = sq->head;
	struct rte_mbuf **txbuffs = sq->txbuffs;
	void *obj_p[NICVF_MAX_TX_FREE_THRESH] __rte_cache_aligned;

	curr_head = nicvf_addr_read(sq->sq_head) >> 4;
	while (head != curr_head) {
		if (txbuffs[head])
			obj_p[j++] = txbuffs[head];

		head = (head + 1) & sq->qlen_mask;
	}

	rte_mempool_put_bulk(sq->pool, obj_p, j);
	sq->head = curr_head;
	sq->xmit_bufs -= j;
	NICVF_TX_ASSERT(sq->xmit_bufs >= 0);
}

void __rte_hot
nicvf_multi_pool_free_xmited_buffers(struct nicvf_txq *sq)
{
	uint32_t n = 0;
	uint32_t curr_head;
	uint32_t head = sq->head;
	struct rte_mbuf **txbuffs = sq->txbuffs;

	curr_head = nicvf_addr_read(sq->sq_head) >> 4;
	while (head != curr_head) {
		if (txbuffs[head]) {
			rte_pktmbuf_free_seg(txbuffs[head]);
			n++;
		}

		head = (head + 1) & sq->qlen_mask;
	}

	sq->head = curr_head;
	sq->xmit_bufs -= n;
	NICVF_TX_ASSERT(sq->xmit_bufs >= 0);
}

static inline uint32_t __rte_hot
nicvf_free_tx_desc(struct nicvf_txq *sq)
{
	return ((sq->head - sq->tail - 1) & sq->qlen_mask);
}

/* Send Header + Packet */
#define TX_DESC_PER_PKT 2

static inline uint32_t __rte_hot
nicvf_free_xmitted_buffers(struct nicvf_txq *sq, struct rte_mbuf **tx_pkts,
			    uint16_t nb_pkts)
{
	uint32_t free_desc = nicvf_free_tx_desc(sq);

	if (free_desc < nb_pkts * TX_DESC_PER_PKT ||
			sq->xmit_bufs > sq->tx_free_thresh) {
		if (unlikely(sq->pool == NULL))
			sq->pool = tx_pkts[0]->pool;

		sq->pool_free(sq);
		/* Freed now, let see the number of free descs again */
		free_desc = nicvf_free_tx_desc(sq);
	}
	return free_desc;
}

uint16_t __rte_hot
nicvf_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;
	uint32_t free_desc;
	uint32_t tail;
	struct nicvf_txq *sq = tx_queue;
	union sq_entry_t *desc_ptr = sq->desc;
	struct rte_mbuf **txbuffs = sq->txbuffs;
	struct rte_mbuf *pkt;
	uint32_t qlen_mask = sq->qlen_mask;

	tail = sq->tail;
	free_desc = nicvf_free_xmitted_buffers(sq, tx_pkts, nb_pkts);

	for (i = 0; i < nb_pkts && (int)free_desc >= TX_DESC_PER_PKT; i++) {
		pkt = tx_pkts[i];

		txbuffs[tail] = NULL;
		fill_sq_desc_header(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;

		txbuffs[tail] = pkt;
		fill_sq_desc_gather(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;
		free_desc -= TX_DESC_PER_PKT;
	}

	if (likely(i)) {
		sq->tail = tail;
		sq->xmit_bufs += i;
		rte_wmb();

		/* Inform HW to xmit the packets */
		nicvf_addr_write(sq->sq_door, i * TX_DESC_PER_PKT);
	}
	return i;
}

uint16_t __rte_hot
nicvf_xmit_pkts_multiseg(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts)
{
	int i, k;
	uint32_t used_desc, next_used_desc, used_bufs, free_desc, tail;
	struct nicvf_txq *sq = tx_queue;
	union sq_entry_t *desc_ptr = sq->desc;
	struct rte_mbuf **txbuffs = sq->txbuffs;
	struct rte_mbuf *pkt, *seg;
	uint32_t qlen_mask = sq->qlen_mask;
	uint16_t nb_segs;

	tail = sq->tail;
	used_desc = 0;
	used_bufs = 0;

	free_desc = nicvf_free_xmitted_buffers(sq, tx_pkts, nb_pkts);

	for (i = 0; i < nb_pkts; i++) {
		pkt = tx_pkts[i];

		nb_segs = pkt->nb_segs;

		next_used_desc = used_desc + nb_segs + 1;
		if (next_used_desc > free_desc)
			break;
		used_desc = next_used_desc;
		used_bufs += nb_segs;

		txbuffs[tail] = NULL;
		fill_sq_desc_header_zero_w1(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;

		txbuffs[tail] = pkt;
		fill_sq_desc_gather(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;

		seg = pkt->next;
		for (k = 1; k < nb_segs; k++) {
			txbuffs[tail] = seg;
			fill_sq_desc_gather(desc_ptr + tail, seg);
			tail = (tail + 1) & qlen_mask;
			seg = seg->next;
		}
	}

	if (likely(used_desc)) {
		sq->tail = tail;
		sq->xmit_bufs += used_bufs;
		rte_wmb();

		/* Inform HW to xmit the packets */
		nicvf_addr_write(sq->sq_door, used_desc);
	}
	return i;
}

static const uint32_t ptype_table[16][16] __rte_cache_aligned = {
	[L3_NONE][L4_NONE] = RTE_PTYPE_UNKNOWN,
	[L3_NONE][L4_IPSEC_ESP] = RTE_PTYPE_UNKNOWN,
	[L3_NONE][L4_IPFRAG] = RTE_PTYPE_L4_FRAG,
	[L3_NONE][L4_IPCOMP] = RTE_PTYPE_UNKNOWN,
	[L3_NONE][L4_TCP] = RTE_PTYPE_L4_TCP,
	[L3_NONE][L4_UDP_PASS1] = RTE_PTYPE_L4_UDP,
	[L3_NONE][L4_GRE] = RTE_PTYPE_TUNNEL_GRE,
	[L3_NONE][L4_UDP_PASS2] = RTE_PTYPE_L4_UDP,
	[L3_NONE][L4_UDP_GENEVE] = RTE_PTYPE_TUNNEL_GENEVE,
	[L3_NONE][L4_UDP_VXLAN] = RTE_PTYPE_TUNNEL_VXLAN,
	[L3_NONE][L4_NVGRE] = RTE_PTYPE_TUNNEL_NVGRE,

	[L3_IPV4][L4_NONE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_UNKNOWN,
	[L3_IPV4][L4_IPSEC_ESP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV4,
	[L3_IPV4][L4_IPFRAG] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_FRAG,
	[L3_IPV4][L4_IPCOMP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_UNKNOWN,
	[L3_IPV4][L4_TCP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
	[L3_IPV4][L4_UDP_PASS1] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
	[L3_IPV4][L4_GRE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_GRE,
	[L3_IPV4][L4_UDP_PASS2] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
	[L3_IPV4][L4_UDP_GENEVE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_GENEVE,
	[L3_IPV4][L4_UDP_VXLAN] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_VXLAN,
	[L3_IPV4][L4_NVGRE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_NVGRE,

	[L3_IPV4_OPT][L4_NONE] = RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_UNKNOWN,
	[L3_IPV4_OPT][L4_IPSEC_ESP] =  RTE_PTYPE_L3_IPV4_EXT |
				RTE_PTYPE_L3_IPV4,
	[L3_IPV4_OPT][L4_IPFRAG] = RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_FRAG,
	[L3_IPV4_OPT][L4_IPCOMP] = RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_UNKNOWN,
	[L3_IPV4_OPT][L4_TCP] = RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_TCP,
	[L3_IPV4_OPT][L4_UDP_PASS1] = RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_UDP,
	[L3_IPV4_OPT][L4_GRE] = RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_GRE,
	[L3_IPV4_OPT][L4_UDP_PASS2] = RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_UDP,
	[L3_IPV4_OPT][L4_UDP_GENEVE] = RTE_PTYPE_L3_IPV4_EXT |
				RTE_PTYPE_TUNNEL_GENEVE,
	[L3_IPV4_OPT][L4_UDP_VXLAN] = RTE_PTYPE_L3_IPV4_EXT |
				RTE_PTYPE_TUNNEL_VXLAN,
	[L3_IPV4_OPT][L4_NVGRE] = RTE_PTYPE_L3_IPV4_EXT |
				RTE_PTYPE_TUNNEL_NVGRE,

	[L3_IPV6][L4_NONE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_UNKNOWN,
	[L3_IPV6][L4_IPSEC_ESP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L3_IPV4,
	[L3_IPV6][L4_IPFRAG] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_FRAG,
	[L3_IPV6][L4_IPCOMP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_UNKNOWN,
	[L3_IPV6][L4_TCP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
	[L3_IPV6][L4_UDP_PASS1] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
	[L3_IPV6][L4_GRE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_GRE,
	[L3_IPV6][L4_UDP_PASS2] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
	[L3_IPV6][L4_UDP_GENEVE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_GENEVE,
	[L3_IPV6][L4_UDP_VXLAN] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_VXLAN,
	[L3_IPV6][L4_NVGRE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_NVGRE,

	[L3_IPV6_OPT][L4_NONE] = RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_UNKNOWN,
	[L3_IPV6_OPT][L4_IPSEC_ESP] =  RTE_PTYPE_L3_IPV6_EXT |
					RTE_PTYPE_L3_IPV4,
	[L3_IPV6_OPT][L4_IPFRAG] = RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_FRAG,
	[L3_IPV6_OPT][L4_IPCOMP] = RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_UNKNOWN,
	[L3_IPV6_OPT][L4_TCP] = RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_TCP,
	[L3_IPV6_OPT][L4_UDP_PASS1] = RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_UDP,
	[L3_IPV6_OPT][L4_GRE] = RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_GRE,
	[L3_IPV6_OPT][L4_UDP_PASS2] = RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_UDP,
	[L3_IPV6_OPT][L4_UDP_GENEVE] = RTE_PTYPE_L3_IPV6_EXT |
					RTE_PTYPE_TUNNEL_GENEVE,
	[L3_IPV6_OPT][L4_UDP_VXLAN] = RTE_PTYPE_L3_IPV6_EXT |
					RTE_PTYPE_TUNNEL_VXLAN,
	[L3_IPV6_OPT][L4_NVGRE] = RTE_PTYPE_L3_IPV6_EXT |
					RTE_PTYPE_TUNNEL_NVGRE,

	[L3_ET_STOP][L4_NONE] = RTE_PTYPE_UNKNOWN,
	[L3_ET_STOP][L4_IPSEC_ESP] = RTE_PTYPE_UNKNOWN,
	[L3_ET_STOP][L4_IPFRAG] = RTE_PTYPE_L4_FRAG,
	[L3_ET_STOP][L4_IPCOMP] = RTE_PTYPE_UNKNOWN,
	[L3_ET_STOP][L4_TCP] = RTE_PTYPE_L4_TCP,
	[L3_ET_STOP][L4_UDP_PASS1] = RTE_PTYPE_L4_UDP,
	[L3_ET_STOP][L4_GRE] = RTE_PTYPE_TUNNEL_GRE,
	[L3_ET_STOP][L4_UDP_PASS2] = RTE_PTYPE_L4_UDP,
	[L3_ET_STOP][L4_UDP_GENEVE] = RTE_PTYPE_TUNNEL_GENEVE,
	[L3_ET_STOP][L4_UDP_VXLAN] = RTE_PTYPE_TUNNEL_VXLAN,
	[L3_ET_STOP][L4_NVGRE] = RTE_PTYPE_TUNNEL_NVGRE,

	[L3_OTHER][L4_NONE] = RTE_PTYPE_UNKNOWN,
	[L3_OTHER][L4_IPSEC_ESP] = RTE_PTYPE_UNKNOWN,
	[L3_OTHER][L4_IPFRAG] = RTE_PTYPE_L4_FRAG,
	[L3_OTHER][L4_IPCOMP] = RTE_PTYPE_UNKNOWN,
	[L3_OTHER][L4_TCP] = RTE_PTYPE_L4_TCP,
	[L3_OTHER][L4_UDP_PASS1] = RTE_PTYPE_L4_UDP,
	[L3_OTHER][L4_GRE] = RTE_PTYPE_TUNNEL_GRE,
	[L3_OTHER][L4_UDP_PASS2] = RTE_PTYPE_L4_UDP,
	[L3_OTHER][L4_UDP_GENEVE] = RTE_PTYPE_TUNNEL_GENEVE,
	[L3_OTHER][L4_UDP_VXLAN] = RTE_PTYPE_TUNNEL_VXLAN,
	[L3_OTHER][L4_NVGRE] = RTE_PTYPE_TUNNEL_NVGRE,
};

static inline uint32_t __rte_hot
nicvf_rx_classify_pkt(cqe_rx_word0_t cqe_rx_w0)
{
	return ptype_table[cqe_rx_w0.l3_type][cqe_rx_w0.l4_type];
}

static inline uint64_t __rte_hot
nicvf_set_olflags(const cqe_rx_word0_t cqe_rx_w0)
{
	static const uint64_t flag_table[3] __rte_cache_aligned = {
		RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD,
		RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN,
		RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
	};

	const uint8_t idx = (cqe_rx_w0.err_opcode == CQE_RX_ERR_L4_CHK) << 1 |
		(cqe_rx_w0.err_opcode == CQE_RX_ERR_IP_CHK);
	return flag_table[idx];
}

static inline int __rte_hot
nicvf_fill_rbdr(struct nicvf_rxq *rxq, int to_fill)
{
	int i;
	uint32_t ltail, next_tail;
	struct nicvf_rbdr *rbdr = rxq->shared_rbdr;
	uint64_t mbuf_phys_off = rxq->mbuf_phys_off;
	struct rbdr_entry_t *desc = rbdr->desc;
	uint32_t qlen_mask = rbdr->qlen_mask;
	uintptr_t door = rbdr->rbdr_door;
	void *obj_p[NICVF_MAX_RX_FREE_THRESH] __rte_cache_aligned;

	if (unlikely(rte_mempool_get_bulk(rxq->pool, obj_p, to_fill) < 0)) {
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			to_fill;
		return 0;
	}

	NICVF_RX_ASSERT((unsigned int)to_fill <= (qlen_mask -
		(nicvf_addr_read(rbdr->rbdr_status) & NICVF_RBDR_COUNT_MASK)));

	next_tail = __atomic_fetch_add(&rbdr->next_tail, to_fill,
					__ATOMIC_ACQUIRE);
	ltail = next_tail;
	for (i = 0; i < to_fill; i++) {
		struct rbdr_entry_t *entry = desc + (ltail & qlen_mask);

		entry->full_addr = nicvf_mbuff_virt2phy((uintptr_t)obj_p[i],
							mbuf_phys_off);
		ltail++;
	}

	rte_wait_until_equal_32(&rbdr->tail, next_tail, __ATOMIC_RELAXED);

	__atomic_store_n(&rbdr->tail, ltail, __ATOMIC_RELEASE);
	nicvf_addr_write(door, to_fill);
	return to_fill;
}

static inline int32_t __rte_hot
nicvf_rx_pkts_to_process(struct nicvf_rxq *rxq, uint16_t nb_pkts,
			 int32_t available_space)
{
	if (unlikely(available_space < nb_pkts))
		rxq->available_space = nicvf_addr_read(rxq->cq_status)
						& NICVF_CQ_CQE_COUNT_MASK;

	return RTE_MIN(nb_pkts, available_space);
}

static inline void __rte_hot
nicvf_rx_offload(cqe_rx_word0_t cqe_rx_w0, cqe_rx_word2_t cqe_rx_w2,
		 struct rte_mbuf *pkt)
{
	if (likely(cqe_rx_w0.rss_alg)) {
		pkt->hash.rss = cqe_rx_w2.rss_tag;
		pkt->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;

	}
}

static __rte_always_inline uint16_t
nicvf_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts,
		const uint32_t flag)
{
	uint32_t i, to_process;
	struct cqe_rx_t *cqe_rx;
	struct rte_mbuf *pkt;
	cqe_rx_word0_t cqe_rx_w0;
	cqe_rx_word1_t cqe_rx_w1;
	cqe_rx_word2_t cqe_rx_w2;
	cqe_rx_word3_t cqe_rx_w3;
	struct nicvf_rxq *rxq = rx_queue;
	union cq_entry_t *desc = rxq->desc;
	const uint64_t cqe_mask = rxq->qlen_mask;
	uint64_t rb0_ptr, mbuf_phys_off = rxq->mbuf_phys_off;
	const uint64_t mbuf_init = rxq->mbuf_initializer.value;
	uint32_t cqe_head = rxq->head & cqe_mask;
	int32_t available_space = rxq->available_space;
	const uint8_t rbptr_offset = rxq->rbptr_offset;

	to_process = nicvf_rx_pkts_to_process(rxq, nb_pkts, available_space);

	for (i = 0; i < to_process; i++) {
		rte_prefetch_non_temporal(&desc[cqe_head + 2]);
		cqe_rx = (struct cqe_rx_t *)&desc[cqe_head];
		NICVF_RX_ASSERT(((struct cq_entry_type_t *)cqe_rx)->cqe_type
						 == CQE_TYPE_RX);

		NICVF_LOAD_PAIR(cqe_rx_w0.u64, cqe_rx_w1.u64, cqe_rx);
		NICVF_LOAD_PAIR(cqe_rx_w2.u64, cqe_rx_w3.u64, &cqe_rx->word2);
		rb0_ptr = *((uint64_t *)cqe_rx + rbptr_offset);
		pkt = (struct rte_mbuf *)nicvf_mbuff_phy2virt
				(rb0_ptr - cqe_rx_w1.align_pad, mbuf_phys_off);

		if (flag & NICVF_RX_OFFLOAD_NONE)
			pkt->ol_flags = 0;
		if (flag & NICVF_RX_OFFLOAD_CKSUM)
			pkt->ol_flags = nicvf_set_olflags(cqe_rx_w0);
		if (flag & NICVF_RX_OFFLOAD_VLAN_STRIP) {
			if (unlikely(cqe_rx_w0.vlan_stripped)) {
				pkt->ol_flags |= RTE_MBUF_F_RX_VLAN
							| RTE_MBUF_F_RX_VLAN_STRIPPED;
				pkt->vlan_tci =
					rte_cpu_to_be_16(cqe_rx_w2.vlan_tci);
			}
		}
		pkt->data_len = cqe_rx_w3.rb0_sz;
		pkt->pkt_len = cqe_rx_w3.rb0_sz;
		pkt->packet_type = nicvf_rx_classify_pkt(cqe_rx_w0);
		nicvf_mbuff_init_update(pkt, mbuf_init, cqe_rx_w1.align_pad);
		nicvf_rx_offload(cqe_rx_w0, cqe_rx_w2, pkt);
		rx_pkts[i] = pkt;
		cqe_head = (cqe_head + 1) & cqe_mask;
		nicvf_prefetch_store_keep(pkt);
	}

	if (likely(to_process)) {
		rxq->available_space -= to_process;
		rxq->head = cqe_head;
		nicvf_addr_write(rxq->cq_door, to_process);
		rxq->recv_buffers += to_process;
	}
	if (rxq->recv_buffers > rxq->rx_free_thresh) {
		rxq->recv_buffers -= nicvf_fill_rbdr(rxq, rxq->rx_free_thresh);
		NICVF_RX_ASSERT(rxq->recv_buffers >= 0);
	}

	return to_process;
}

uint16_t __rte_hot
nicvf_recv_pkts_no_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return nicvf_recv_pkts(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_NONE);
}

uint16_t __rte_hot
nicvf_recv_pkts_cksum(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return nicvf_recv_pkts(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_CKSUM);
}

uint16_t __rte_hot
nicvf_recv_pkts_vlan_strip(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return nicvf_recv_pkts(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_NONE | NICVF_RX_OFFLOAD_VLAN_STRIP);
}

uint16_t __rte_hot
nicvf_recv_pkts_cksum_vlan_strip(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return nicvf_recv_pkts(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_CKSUM | NICVF_RX_OFFLOAD_VLAN_STRIP);
}

static __rte_always_inline uint16_t __rte_hot
nicvf_process_cq_mseg_entry(struct cqe_rx_t *cqe_rx,
			uint64_t mbuf_phys_off,
			struct rte_mbuf **rx_pkt, uint8_t rbptr_offset,
			uint64_t mbuf_init, const uint32_t flag)
{
	struct rte_mbuf *pkt, *seg, *prev;
	cqe_rx_word0_t cqe_rx_w0;
	cqe_rx_word1_t cqe_rx_w1;
	cqe_rx_word2_t cqe_rx_w2;
	uint16_t *rb_sz, nb_segs, seg_idx;
	uint64_t *rb_ptr;

	NICVF_LOAD_PAIR(cqe_rx_w0.u64, cqe_rx_w1.u64, cqe_rx);
	NICVF_RX_ASSERT(cqe_rx_w0.cqe_type == CQE_TYPE_RX);
	cqe_rx_w2 = cqe_rx->word2;
	rb_sz = &cqe_rx->word3.rb0_sz;
	rb_ptr = (uint64_t *)cqe_rx + rbptr_offset;
	nb_segs = cqe_rx_w0.rb_cnt;
	pkt = (struct rte_mbuf *)nicvf_mbuff_phy2virt
			(rb_ptr[0] - cqe_rx_w1.align_pad, mbuf_phys_off);

	pkt->pkt_len = cqe_rx_w1.pkt_len;
	pkt->data_len = rb_sz[nicvf_frag_num(0)];
	nicvf_mbuff_init_mseg_update(
				pkt, mbuf_init, cqe_rx_w1.align_pad, nb_segs);
	pkt->packet_type = nicvf_rx_classify_pkt(cqe_rx_w0);
	if (flag & NICVF_RX_OFFLOAD_NONE)
		pkt->ol_flags = 0;
	if (flag & NICVF_RX_OFFLOAD_CKSUM)
		pkt->ol_flags = nicvf_set_olflags(cqe_rx_w0);
	if (flag & NICVF_RX_OFFLOAD_VLAN_STRIP) {
		if (unlikely(cqe_rx_w0.vlan_stripped)) {
			pkt->ol_flags |= RTE_MBUF_F_RX_VLAN
				| RTE_MBUF_F_RX_VLAN_STRIPPED;
			pkt->vlan_tci = rte_cpu_to_be_16(cqe_rx_w2.vlan_tci);
		}
	}
	nicvf_rx_offload(cqe_rx_w0, cqe_rx_w2, pkt);

	*rx_pkt = pkt;
	prev = pkt;
	for (seg_idx = 1; seg_idx < nb_segs; seg_idx++) {
		seg = (struct rte_mbuf *)nicvf_mbuff_phy2virt
			(rb_ptr[seg_idx], mbuf_phys_off);

		prev->next = seg;
		seg->data_len = rb_sz[nicvf_frag_num(seg_idx)];
		nicvf_mbuff_init_update(seg, mbuf_init, 0);

		prev = seg;
	}
	prev->next = NULL;
	return nb_segs;
}

static __rte_always_inline uint16_t __rte_hot
nicvf_recv_pkts_multiseg(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts, const uint32_t flag)
{
	union cq_entry_t *cq_entry;
	struct cqe_rx_t *cqe_rx;
	struct nicvf_rxq *rxq = rx_queue;
	union cq_entry_t *desc = rxq->desc;
	const uint64_t cqe_mask = rxq->qlen_mask;
	uint64_t mbuf_phys_off = rxq->mbuf_phys_off;
	uint32_t i, to_process, cqe_head, buffers_consumed = 0;
	int32_t available_space = rxq->available_space;
	uint16_t nb_segs;
	const uint64_t mbuf_init = rxq->mbuf_initializer.value;
	const uint8_t rbptr_offset = rxq->rbptr_offset;

	cqe_head = rxq->head & cqe_mask;
	to_process = nicvf_rx_pkts_to_process(rxq, nb_pkts, available_space);

	for (i = 0; i < to_process; i++) {
		rte_prefetch_non_temporal(&desc[cqe_head + 2]);
		cq_entry = &desc[cqe_head];
		cqe_rx = (struct cqe_rx_t *)cq_entry;
		nb_segs = nicvf_process_cq_mseg_entry(cqe_rx, mbuf_phys_off,
			rx_pkts + i, rbptr_offset, mbuf_init, flag);
		buffers_consumed += nb_segs;
		cqe_head = (cqe_head + 1) & cqe_mask;
		nicvf_prefetch_store_keep(rx_pkts[i]);
	}

	if (likely(to_process)) {
		rxq->available_space -= to_process;
		rxq->head = cqe_head;
		nicvf_addr_write(rxq->cq_door, to_process);
		rxq->recv_buffers += buffers_consumed;
	}
	if (rxq->recv_buffers > rxq->rx_free_thresh) {
		rxq->recv_buffers -= nicvf_fill_rbdr(rxq, rxq->rx_free_thresh);
		NICVF_RX_ASSERT(rxq->recv_buffers >= 0);
	}

	return to_process;
}

uint16_t __rte_hot
nicvf_recv_pkts_multiseg_no_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return nicvf_recv_pkts_multiseg(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_NONE);
}

uint16_t __rte_hot
nicvf_recv_pkts_multiseg_cksum(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return nicvf_recv_pkts_multiseg(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_CKSUM);
}

uint16_t __rte_hot
nicvf_recv_pkts_multiseg_vlan_strip(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	return nicvf_recv_pkts_multiseg(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_NONE | NICVF_RX_OFFLOAD_VLAN_STRIP);
}

uint16_t __rte_hot
nicvf_recv_pkts_multiseg_cksum_vlan_strip(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	return nicvf_recv_pkts_multiseg(rx_queue, rx_pkts, nb_pkts,
			NICVF_RX_OFFLOAD_CKSUM | NICVF_RX_OFFLOAD_VLAN_STRIP);
}

uint32_t
nicvf_dev_rx_queue_count(void *rx_queue)
{
	struct nicvf_rxq *rxq;

	rxq = rx_queue;
	return nicvf_addr_read(rxq->cq_status) & NICVF_CQ_CQE_COUNT_MASK;
}

uint32_t
nicvf_dev_rbdr_refill(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	struct nicvf_rxq *rxq;
	uint32_t to_process;
	uint32_t rx_free;

	rxq = dev->data->rx_queues[queue_idx];
	to_process = rxq->recv_buffers;
	while (rxq->recv_buffers > 0) {
		rx_free = RTE_MIN(rxq->recv_buffers, NICVF_MAX_RX_FREE_THRESH);
		rxq->recv_buffers -= nicvf_fill_rbdr(rxq, rx_free);
	}

	assert(rxq->recv_buffers == 0);
	return to_process;
}
