/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdint.h>
#include <stdalign.h>

#include <rte_net.h>

#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_queue.h"

#define ZXDH_PKT_FORM_CPU                     0x20    /* 1-cpu 0-np */
#define ZXDH_NO_IP_FRAGMENT                   0x2000  /* ip fragment flag */
#define ZXDH_NO_IPID_UPDATE                   0x4000  /* ipid update flag */

#define ZXDH_PI_L3TYPE_IP                     0x00
#define ZXDH_PI_L3TYPE_IPV6                   0x40
#define ZXDH_PI_L3TYPE_NOIP                   0x80
#define ZXDH_PI_L3TYPE_RSV                    0xC0
#define ZXDH_PI_L3TYPE_MASK                   0xC0

#define ZXDH_PCODE_MASK                       0x1F
#define ZXDH_PCODE_IP_PKT_TYPE                0x01
#define ZXDH_PCODE_TCP_PKT_TYPE               0x02
#define ZXDH_PCODE_UDP_PKT_TYPE               0x03
#define ZXDH_PCODE_NO_IP_PKT_TYPE             0x09
#define ZXDH_PCODE_NO_REASSMBLE_TCP_PKT_TYPE  0x0C

#define ZXDH_TX_MAX_SEGS                      31
#define ZXDH_RX_MAX_SEGS                      31

static void
zxdh_xmit_cleanup_inorder_packed(struct zxdh_virtqueue *vq, int32_t num)
{
	uint16_t used_idx = 0;
	uint16_t id       = 0;
	uint16_t curr_id  = 0;
	uint16_t free_cnt = 0;
	uint16_t size     = vq->vq_nentries;
	struct zxdh_vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct zxdh_vq_desc_extra     *dxp  = NULL;

	used_idx = vq->vq_used_cons_idx;
	/* desc_is_used has a load-acquire or rte_io_rmb inside
	 * and wait for used desc in virtqueue.
	 */
	while (num > 0 && zxdh_desc_used(&desc[used_idx], vq)) {
		id = desc[used_idx].id;
		do {
			curr_id = used_idx;
			dxp = &vq->vq_descx[used_idx];
			used_idx += dxp->ndescs;
			free_cnt += dxp->ndescs;
			num -= dxp->ndescs;
			if (used_idx >= size) {
				used_idx -= size;
				vq->vq_packed.used_wrap_counter ^= 1;
			}
			if (dxp->cookie != NULL) {
				rte_pktmbuf_free(dxp->cookie);
				dxp->cookie = NULL;
			}
		} while (curr_id != id);
	}
	vq->vq_used_cons_idx = used_idx;
	vq->vq_free_cnt += free_cnt;
}

static void
zxdh_ring_free_id_packed(struct zxdh_virtqueue *vq, uint16_t id)
{
	struct zxdh_vq_desc_extra *dxp = NULL;

	dxp = &vq->vq_descx[id];
	vq->vq_free_cnt += dxp->ndescs;

	if (vq->vq_desc_tail_idx == ZXDH_VQ_RING_DESC_CHAIN_END)
		vq->vq_desc_head_idx = id;
	else
		vq->vq_descx[vq->vq_desc_tail_idx].next = id;

	vq->vq_desc_tail_idx = id;
	dxp->next = ZXDH_VQ_RING_DESC_CHAIN_END;
}

static void
zxdh_xmit_cleanup_normal_packed(struct zxdh_virtqueue *vq, int32_t num)
{
	uint16_t used_idx = 0;
	uint16_t id = 0;
	uint16_t size = vq->vq_nentries;
	struct zxdh_vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct zxdh_vq_desc_extra *dxp = NULL;

	used_idx = vq->vq_used_cons_idx;
	/* desc_is_used has a load-acquire or rte_io_rmb inside
	 * and wait for used desc in virtqueue.
	 */
	while (num-- && zxdh_desc_used(&desc[used_idx], vq)) {
		id = desc[used_idx].id;
		dxp = &vq->vq_descx[id];
		vq->vq_used_cons_idx += dxp->ndescs;
		if (vq->vq_used_cons_idx >= size) {
			vq->vq_used_cons_idx -= size;
			vq->vq_packed.used_wrap_counter ^= 1;
		}
		zxdh_ring_free_id_packed(vq, id);
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
		used_idx = vq->vq_used_cons_idx;
	}
}

static void
zxdh_xmit_cleanup_packed(struct zxdh_virtqueue *vq, int32_t num, int32_t in_order)
{
	if (in_order)
		zxdh_xmit_cleanup_inorder_packed(vq, num);
	else
		zxdh_xmit_cleanup_normal_packed(vq, num);
}

static uint8_t
zxdh_xmit_get_ptype(struct rte_mbuf *m)
{
	uint8_t pcode = ZXDH_PCODE_NO_IP_PKT_TYPE;
	uint8_t l3_ptype = ZXDH_PI_L3TYPE_NOIP;

	if ((m->packet_type & RTE_PTYPE_INNER_L3_MASK) == RTE_PTYPE_INNER_L3_IPV4 ||
			((!(m->packet_type & RTE_PTYPE_TUNNEL_MASK)) &&
			(m->packet_type & RTE_PTYPE_L3_MASK) == RTE_PTYPE_L3_IPV4)) {
		l3_ptype = ZXDH_PI_L3TYPE_IP;
		pcode = ZXDH_PCODE_IP_PKT_TYPE;
	} else if ((m->packet_type & RTE_PTYPE_INNER_L3_MASK) == RTE_PTYPE_INNER_L3_IPV6 ||
			((!(m->packet_type & RTE_PTYPE_TUNNEL_MASK)) &&
			(m->packet_type & RTE_PTYPE_L3_MASK) == RTE_PTYPE_L3_IPV6)) {
		l3_ptype = ZXDH_PI_L3TYPE_IPV6;
		pcode = ZXDH_PCODE_IP_PKT_TYPE;
	} else {
		goto end;
	}

	if ((m->packet_type & RTE_PTYPE_INNER_L4_MASK) == RTE_PTYPE_INNER_L4_TCP ||
			((!(m->packet_type & RTE_PTYPE_TUNNEL_MASK)) &&
			(m->packet_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP))
		pcode = ZXDH_PCODE_TCP_PKT_TYPE;
	else if ((m->packet_type & RTE_PTYPE_INNER_L4_MASK) == RTE_PTYPE_INNER_L4_UDP ||
				((!(m->packet_type & RTE_PTYPE_TUNNEL_MASK)) &&
				(m->packet_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_UDP))
		pcode = ZXDH_PCODE_UDP_PKT_TYPE;

end:
	return  l3_ptype | ZXDH_PKT_FORM_CPU | pcode;
}

static void zxdh_xmit_fill_net_hdr(struct rte_mbuf *cookie,
				struct zxdh_net_hdr_dl *hdr)
{
	uint16_t pkt_flag_lw16 = ZXDH_NO_IPID_UPDATE;
	uint16_t l3_offset;
	uint32_t ol_flag = 0;

	hdr->pi_hdr.pkt_flag_lw16 = rte_be_to_cpu_16(pkt_flag_lw16);

	hdr->pi_hdr.pkt_type = zxdh_xmit_get_ptype(cookie);
	l3_offset = ZXDH_DL_NET_HDR_SIZE + cookie->outer_l2_len +
				cookie->outer_l3_len + cookie->l2_len;
	hdr->pi_hdr.l3_offset = rte_be_to_cpu_16(l3_offset);
	hdr->pi_hdr.l4_offset = rte_be_to_cpu_16(l3_offset + cookie->l3_len);

	hdr->pd_hdr.ol_flag = rte_be_to_cpu_32(ol_flag);
}

static inline void zxdh_enqueue_xmit_packed_fast(struct zxdh_virtnet_tx *txvq,
						struct rte_mbuf *cookie, int32_t in_order)
{
	struct zxdh_virtqueue *vq = txvq->vq;
	uint16_t id = in_order ? vq->vq_avail_idx : vq->vq_desc_head_idx;
	struct zxdh_vq_desc_extra *dxp = &vq->vq_descx[id];
	uint16_t flags = vq->vq_packed.cached_flags;
	struct zxdh_net_hdr_dl *hdr = NULL;

	dxp->ndescs = 1;
	dxp->cookie = cookie;
	hdr = rte_pktmbuf_mtod_offset(cookie, struct zxdh_net_hdr_dl *, -ZXDH_DL_NET_HDR_SIZE);
	zxdh_xmit_fill_net_hdr(cookie, hdr);

	uint16_t idx = vq->vq_avail_idx;
	struct zxdh_vring_packed_desc *dp = &vq->vq_packed.ring.desc[idx];

	dp->addr = rte_pktmbuf_iova(cookie) - ZXDH_DL_NET_HDR_SIZE;
	dp->len  = cookie->data_len + ZXDH_DL_NET_HDR_SIZE;
	dp->id   = id;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
	}
	vq->vq_free_cnt--;
	if (!in_order) {
		vq->vq_desc_head_idx = dxp->next;
		if (vq->vq_desc_head_idx == ZXDH_VQ_RING_DESC_CHAIN_END)
			vq->vq_desc_tail_idx = ZXDH_VQ_RING_DESC_CHAIN_END;
		}
		zxdh_queue_store_flags_packed(dp, flags, vq->hw->weak_barriers);
}

static inline void zxdh_enqueue_xmit_packed(struct zxdh_virtnet_tx *txvq,
						struct rte_mbuf *cookie,
						uint16_t needed,
						int32_t use_indirect,
						int32_t in_order)
{
	struct zxdh_tx_region *txr = txvq->zxdh_net_hdr_mz->addr;
	struct zxdh_virtqueue *vq = txvq->vq;
	struct zxdh_vring_packed_desc *start_dp = vq->vq_packed.ring.desc;
	void *hdr = NULL;
	uint16_t head_idx = vq->vq_avail_idx;
	uint16_t idx = head_idx;
	uint16_t prev = head_idx;
	uint16_t head_flags = cookie->next ? ZXDH_VRING_DESC_F_NEXT : 0;
	uint16_t seg_num = cookie->nb_segs;
	uint16_t id = in_order ? vq->vq_avail_idx : vq->vq_desc_head_idx;
	struct zxdh_vring_packed_desc *head_dp = &vq->vq_packed.ring.desc[idx];
	struct zxdh_vq_desc_extra *dxp = &vq->vq_descx[id];

	dxp->ndescs = needed;
	dxp->cookie = cookie;
	head_flags |= vq->vq_packed.cached_flags;
	/* if offload disabled, it is not zeroed below, do it now */

	if (use_indirect) {
		/**
		 * setup tx ring slot to point to indirect
		 * descriptor list stored in reserved region.
		 * the first slot in indirect ring is already
		 * preset to point to the header in reserved region
		 **/
		start_dp[idx].addr =
			txvq->zxdh_net_hdr_mem + RTE_PTR_DIFF(&txr[idx].tx_packed_indir, txr);
		start_dp[idx].len  = (seg_num + 1) * sizeof(struct zxdh_vring_packed_desc);
		/* Packed descriptor id needs to be restored when inorder. */
		if (in_order)
			start_dp[idx].id = idx;

		/* reset flags for indirect desc */
		head_flags = ZXDH_VRING_DESC_F_INDIRECT;
		head_flags |= vq->vq_packed.cached_flags;
		hdr = (void *)&txr[idx].tx_hdr;
		/* loop below will fill in rest of the indirect elements */
		start_dp = txr[idx].tx_packed_indir;
		start_dp->len = ZXDH_DL_NET_HDR_SIZE; /* update actual net or type hdr size */
		idx = 1;
	} else {
		/* setup first tx ring slot to point to header stored in reserved region. */
		start_dp[idx].addr = txvq->zxdh_net_hdr_mem + RTE_PTR_DIFF(&txr[idx].tx_hdr, txr);
		start_dp[idx].len  = ZXDH_DL_NET_HDR_SIZE;
		head_flags |= ZXDH_VRING_DESC_F_NEXT;
		hdr = (void *)&txr[idx].tx_hdr;
		idx++;
		if (idx >= vq->vq_nentries) {
			idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
		}
	}
	zxdh_xmit_fill_net_hdr(cookie, (struct zxdh_net_hdr_dl *)hdr);

	do {
		start_dp[idx].addr = rte_pktmbuf_iova(cookie);
		start_dp[idx].len  = cookie->data_len;
		if (likely(idx != head_idx)) {
			uint16_t flags = cookie->next ? ZXDH_VRING_DESC_F_NEXT : 0;
			flags |= vq->vq_packed.cached_flags;
			start_dp[idx].flags = flags;
		}
		prev = idx;
		idx++;
		if (idx >= vq->vq_nentries) {
			idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
		}
	} while ((cookie = cookie->next) != NULL);
	start_dp[prev].id = id;
	if (use_indirect) {
		idx = head_idx;
		if (++idx >= vq->vq_nentries) {
			idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
		}
	}
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - needed);
	vq->vq_avail_idx = idx;
	if (!in_order) {
		vq->vq_desc_head_idx = dxp->next;
		if (vq->vq_desc_head_idx == ZXDH_VQ_RING_DESC_CHAIN_END)
			vq->vq_desc_tail_idx = ZXDH_VQ_RING_DESC_CHAIN_END;
	}
	zxdh_queue_store_flags_packed(head_dp, head_flags, vq->hw->weak_barriers);
}

uint16_t
zxdh_xmit_pkts_packed(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct zxdh_virtnet_tx *txvq = tx_queue;
	struct zxdh_virtqueue  *vq   = txvq->vq;
	struct zxdh_hw    *hw   = vq->hw;
	uint16_t nb_tx = 0;

	bool in_order = zxdh_pci_with_feature(hw, ZXDH_F_IN_ORDER);

	if (nb_pkts > vq->vq_free_cnt)
		zxdh_xmit_cleanup_packed(vq, nb_pkts - vq->vq_free_cnt, in_order);
	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *txm = tx_pkts[nb_tx];
		int32_t can_push     = 0;
		int32_t use_indirect = 0;
		int32_t slots        = 0;
		int32_t need         = 0;

		/* optimize ring usage */
		if ((zxdh_pci_with_feature(hw, ZXDH_F_ANY_LAYOUT) ||
			zxdh_pci_with_feature(hw, ZXDH_F_VERSION_1)) &&
			rte_mbuf_refcnt_read(txm) == 1 &&
			RTE_MBUF_DIRECT(txm) &&
			txm->nb_segs == 1 &&
			rte_pktmbuf_headroom(txm) >= ZXDH_DL_NET_HDR_SIZE &&
			rte_is_aligned(rte_pktmbuf_mtod(txm, char *),
			alignof(struct zxdh_net_hdr_dl))) {
			can_push = 1;
		} else if (zxdh_pci_with_feature(hw, ZXDH_RING_F_INDIRECT_DESC) &&
					txm->nb_segs < ZXDH_MAX_TX_INDIRECT) {
			use_indirect = 1;
		}
		/**
		 * How many main ring entries are needed to this Tx?
		 * indirect   => 1
		 * any_layout => number of segments
		 * default    => number of segments + 1
		 **/
		slots = use_indirect ? 1 : (txm->nb_segs + !can_push);
		need = slots - vq->vq_free_cnt;
		/* Positive value indicates it need free vring descriptors */
		if (unlikely(need > 0)) {
			zxdh_xmit_cleanup_packed(vq, need, in_order);
			need = slots - vq->vq_free_cnt;
			if (unlikely(need > 0)) {
				PMD_TX_LOG(ERR, "port[ep:%d, pf:%d, vf:%d, vfid:%d, pcieid:%d], queue:%d[pch:%d]. No free desc to xmit",
					hw->vport.epid, hw->vport.pfid, hw->vport.vfid,
					hw->vfid, hw->pcie_id, txvq->queue_id,
					hw->channel_context[txvq->queue_id].ph_chno);
				break;
			}
		}
		/* Enqueue Packet buffers */
		if (can_push)
			zxdh_enqueue_xmit_packed_fast(txvq, txm, in_order);
		else
			zxdh_enqueue_xmit_packed(txvq, txm, slots, use_indirect, in_order);
	}
	if (likely(nb_tx)) {
		if (unlikely(zxdh_queue_kick_prepare_packed(vq))) {
			zxdh_queue_notify(vq);
			PMD_TX_LOG(DEBUG, "Notified backend after xmit");
		}
	}
	return nb_tx;
}

uint16_t zxdh_xmit_pkts_prepare(void *tx_queue __rte_unused, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts)
{
	uint16_t nb_tx;

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *m = tx_pkts[nb_tx];
		int32_t error;

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		error = rte_validate_tx_offload(m);
		if (unlikely(error)) {
			rte_errno = -error;
			break;
		}
#endif

		error = rte_net_intel_cksum_prepare(m);
		if (unlikely(error)) {
			rte_errno = -error;
			break;
		}
	}
	return nb_tx;
}
