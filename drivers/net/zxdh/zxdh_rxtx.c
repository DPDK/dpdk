/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdint.h>
#include <stdalign.h>

#include <rte_net.h>

#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_queue.h"

#define ZXDH_SVLAN_TPID                       0x88a8
#define ZXDH_CVLAN_TPID                       0x8100

#define ZXDH_PKT_FORM_CPU                     0x20    /* 1-cpu 0-np */
#define ZXDH_NO_IP_FRAGMENT                   0x2000  /* ip fragment flag */
#define ZXDH_NO_IPID_UPDATE                   0x4000  /* ipid update flag */

#define ZXDH_PI_L3TYPE_IP                     0x00
#define ZXDH_PI_L3TYPE_IPV6                   0x40
#define ZXDH_PI_L3TYPE_NOIP                   0x80
#define ZXDH_PI_L3TYPE_RSV                    0xC0
#define ZXDH_PI_L3TYPE_MASK                   0xC0

#define  ZXDH_PD_OFFLOAD_SVLAN_INSERT         (1 << 14)
#define  ZXDH_PD_OFFLOAD_CVLAN_INSERT         (1 << 13)

#define ZXDH_PCODE_MASK                       0x1F
#define ZXDH_PCODE_IP_PKT_TYPE                0x01
#define ZXDH_PCODE_TCP_PKT_TYPE               0x02
#define ZXDH_PCODE_UDP_PKT_TYPE               0x03
#define ZXDH_PCODE_NO_IP_PKT_TYPE             0x09
#define ZXDH_PCODE_NO_REASSMBLE_TCP_PKT_TYPE  0x0C

#define ZXDH_TX_MAX_SEGS                      31
#define ZXDH_RX_MAX_SEGS                      31

uint32_t zxdh_outer_l2_type[16] = {
	0,
	RTE_PTYPE_L2_ETHER,
	RTE_PTYPE_L2_ETHER_TIMESYNC,
	RTE_PTYPE_L2_ETHER_ARP,
	RTE_PTYPE_L2_ETHER_LLDP,
	RTE_PTYPE_L2_ETHER_NSH,
	RTE_PTYPE_L2_ETHER_VLAN,
	RTE_PTYPE_L2_ETHER_QINQ,
	RTE_PTYPE_L2_ETHER_PPPOE,
	RTE_PTYPE_L2_ETHER_FCOE,
	RTE_PTYPE_L2_ETHER_MPLS,
};

uint32_t zxdh_outer_l3_type[16] = {
	0,
	RTE_PTYPE_L3_IPV4,
	RTE_PTYPE_L3_IPV4_EXT,
	RTE_PTYPE_L3_IPV6,
	RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
	RTE_PTYPE_L3_IPV6_EXT,
	RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
};

uint32_t zxdh_outer_l4_type[16] = {
	0,
	RTE_PTYPE_L4_TCP,
	RTE_PTYPE_L4_UDP,
	RTE_PTYPE_L4_FRAG,
	RTE_PTYPE_L4_SCTP,
	RTE_PTYPE_L4_ICMP,
	RTE_PTYPE_L4_NONFRAG,
	RTE_PTYPE_L4_IGMP,
};

uint32_t zxdh_tunnel_type[16] = {
	0,
	RTE_PTYPE_TUNNEL_IP,
	RTE_PTYPE_TUNNEL_GRE,
	RTE_PTYPE_TUNNEL_VXLAN,
	RTE_PTYPE_TUNNEL_NVGRE,
	RTE_PTYPE_TUNNEL_GENEVE,
	RTE_PTYPE_TUNNEL_GRENAT,
	RTE_PTYPE_TUNNEL_GTPC,
	RTE_PTYPE_TUNNEL_GTPU,
	RTE_PTYPE_TUNNEL_ESP,
	RTE_PTYPE_TUNNEL_L2TP,
	RTE_PTYPE_TUNNEL_VXLAN_GPE,
	RTE_PTYPE_TUNNEL_MPLS_IN_GRE,
	RTE_PTYPE_TUNNEL_MPLS_IN_UDP,
};

uint32_t zxdh_inner_l2_type[16] = {
	0,
	RTE_PTYPE_INNER_L2_ETHER,
	0,
	0,
	0,
	0,
	RTE_PTYPE_INNER_L2_ETHER_VLAN,
	RTE_PTYPE_INNER_L2_ETHER_QINQ,
	0,
	0,
	0,
};

uint32_t zxdh_inner_l3_type[16] = {
	0,
	RTE_PTYPE_INNER_L3_IPV4,
	RTE_PTYPE_INNER_L3_IPV4_EXT,
	RTE_PTYPE_INNER_L3_IPV6,
	RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
	RTE_PTYPE_INNER_L3_IPV6_EXT,
	RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
};

uint32_t zxdh_inner_l4_type[16] = {
	0,
	RTE_PTYPE_INNER_L4_TCP,
	RTE_PTYPE_INNER_L4_UDP,
	RTE_PTYPE_INNER_L4_FRAG,
	RTE_PTYPE_INNER_L4_SCTP,
	RTE_PTYPE_INNER_L4_ICMP,
	0,
	0,
};

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

	if (cookie->ol_flags & RTE_MBUF_F_TX_VLAN) {
		ol_flag |= ZXDH_PD_OFFLOAD_CVLAN_INSERT;
		hdr->pi_hdr.vlan_id = rte_be_to_cpu_16(cookie->vlan_tci);
		hdr->pd_hdr.cvlan_insert =
			rte_be_to_cpu_32((ZXDH_CVLAN_TPID << 16) | cookie->vlan_tci);
	}
	if (cookie->ol_flags & RTE_MBUF_F_TX_QINQ) {
		ol_flag |= ZXDH_PD_OFFLOAD_SVLAN_INSERT;
		hdr->pd_hdr.svlan_insert =
			rte_be_to_cpu_32((ZXDH_SVLAN_TPID << 16) | cookie->vlan_tci_outer);
	}

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

static void
zxdh_update_packet_stats(struct zxdh_virtnet_stats *stats, struct rte_mbuf *mbuf)
{
	uint32_t s = mbuf->pkt_len;
	struct rte_ether_addr *ea = NULL;

	stats->bytes += s;

	if (s == 64) {
		stats->size_bins[1]++;
	} else if (s > 64 && s < 1024) {
		uint32_t bin;

		/* count zeros, and offset into correct bin */
		bin = (sizeof(s) * 8) - rte_clz32(s) - 5;
		stats->size_bins[bin]++;
	} else {
		if (s < 64)
			stats->size_bins[0]++;
		else if (s < 1519)
			stats->size_bins[6]++;
		else
			stats->size_bins[7]++;
	}

	ea = rte_pktmbuf_mtod(mbuf, struct rte_ether_addr *);
	if (rte_is_multicast_ether_addr(ea)) {
		if (rte_is_broadcast_ether_addr(ea))
			stats->broadcast++;
		else
			stats->multicast++;
	}
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
		if (txm->nb_segs > ZXDH_TX_MAX_SEGS) {
			PMD_TX_LOG(ERR, "%d segs dropped", txm->nb_segs);
			txvq->stats.truncated_err += nb_pkts - nb_tx;
			break;
		}
		/* Enqueue Packet buffers */
		if (can_push)
			zxdh_enqueue_xmit_packed_fast(txvq, txm, in_order);
		else
			zxdh_enqueue_xmit_packed(txvq, txm, slots, use_indirect, in_order);
		zxdh_update_packet_stats(&txvq->stats, txm);
	}
	txvq->stats.packets += nb_tx;
	if (likely(nb_tx)) {
		if (unlikely(zxdh_queue_kick_prepare_packed(vq))) {
			zxdh_queue_notify(vq);
			PMD_TX_LOG(DEBUG, "Notified backend after xmit");
		}
	}
	return nb_tx;
}

uint16_t zxdh_xmit_pkts_prepare(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts)
{
	struct zxdh_virtnet_tx *txvq = tx_queue;
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
		if (m->nb_segs > ZXDH_TX_MAX_SEGS) {
			PMD_TX_LOG(ERR, "%d segs dropped", m->nb_segs);
			txvq->stats.truncated_err += nb_pkts - nb_tx;
			rte_errno = ENOMEM;
			break;
		}
	}
	return nb_tx;
}

static uint16_t zxdh_dequeue_burst_rx_packed(struct zxdh_virtqueue *vq,
					struct rte_mbuf **rx_pkts,
					uint32_t *len,
					uint16_t num)
{
	struct zxdh_vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct rte_mbuf *cookie = NULL;
	uint16_t i, used_idx;
	uint16_t id;

	for (i = 0; i < num; i++) {
		used_idx = vq->vq_used_cons_idx;
		/**
		 * desc_is_used has a load-acquire or rte_io_rmb inside
		 * and wait for used desc in virtqueue.
		 */
		if (!zxdh_desc_used(&desc[used_idx], vq))
			return i;
		len[i] = desc[used_idx].len;
		id = desc[used_idx].id;
		cookie = (struct rte_mbuf *)vq->vq_descx[id].cookie;
		vq->vq_descx[id].cookie = NULL;
		if (unlikely(cookie == NULL)) {
			PMD_RX_LOG(ERR,
				"vring descriptor with no mbuf cookie at %u", vq->vq_used_cons_idx);
			break;
		}
		rx_pkts[i] = cookie;
		vq->vq_free_cnt++;
		vq->vq_used_cons_idx++;
		if (vq->vq_used_cons_idx >= vq->vq_nentries) {
			vq->vq_used_cons_idx -= vq->vq_nentries;
			vq->vq_packed.used_wrap_counter ^= 1;
		}
	}
	return i;
}

static int32_t zxdh_rx_update_mbuf(struct rte_mbuf *m, struct zxdh_net_hdr_ul *hdr)
{
	struct zxdh_pd_hdr_ul *pd_hdr = &hdr->pd_hdr;
	struct zxdh_pi_hdr *pi_hdr = &hdr->pi_hdr;
	uint32_t idx = 0;

	m->pkt_len = rte_be_to_cpu_16(pi_hdr->ul.pkt_len);

	uint16_t pkt_type_outer = rte_be_to_cpu_16(pd_hdr->pkt_type_out);

	idx = (pkt_type_outer >> 12) & 0xF;
	m->packet_type  = zxdh_outer_l2_type[idx];
	idx = (pkt_type_outer >> 8)  & 0xF;
	m->packet_type |= zxdh_outer_l3_type[idx];
	idx = (pkt_type_outer >> 4)  & 0xF;
	m->packet_type |= zxdh_outer_l4_type[idx];
	idx = pkt_type_outer         & 0xF;
	m->packet_type |= zxdh_tunnel_type[idx];

	uint16_t pkt_type_inner = rte_be_to_cpu_16(pd_hdr->pkt_type_in);

	if (pkt_type_inner) {
		idx = (pkt_type_inner >> 12) & 0xF;
		m->packet_type |= zxdh_inner_l2_type[idx];
		idx = (pkt_type_inner >> 8)  & 0xF;
		m->packet_type |= zxdh_inner_l3_type[idx];
		idx = (pkt_type_inner >> 4)  & 0xF;
		m->packet_type |= zxdh_inner_l4_type[idx];
	}

	return 0;
}

static void zxdh_discard_rxbuf(struct zxdh_virtqueue *vq, struct rte_mbuf *m)
{
	int32_t error = 0;
	/*
	 * Requeue the discarded mbuf. This should always be
	 * successful since it was just dequeued.
	 */
	error = zxdh_enqueue_recv_refill_packed(vq, &m, 1);
	if (unlikely(error)) {
		PMD_RX_LOG(ERR, "cannot enqueue discarded mbuf");
		rte_pktmbuf_free(m);
	}
}

uint16_t zxdh_recv_pkts_packed(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts)
{
	struct zxdh_virtnet_rx *rxvq = rx_queue;
	struct zxdh_virtqueue *vq = rxvq->vq;
	struct zxdh_hw *hw = vq->hw;
	struct rte_eth_dev *dev = hw->eth_dev;
	struct rte_mbuf *rxm = NULL;
	struct rte_mbuf *prev = NULL;
	uint32_t len[ZXDH_MBUF_BURST_SZ] = {0};
	struct rte_mbuf *rcv_pkts[ZXDH_MBUF_BURST_SZ] = {NULL};
	uint32_t nb_enqueued = 0;
	uint32_t seg_num = 0;
	uint32_t seg_res = 0;
	uint16_t hdr_size = 0;
	int32_t error = 0;
	uint16_t nb_rx = 0;
	uint16_t num = nb_pkts;

	if (unlikely(num > ZXDH_MBUF_BURST_SZ))
		num = ZXDH_MBUF_BURST_SZ;

	num = zxdh_dequeue_burst_rx_packed(vq, rcv_pkts, len, num);
	uint16_t i;
	uint16_t rcvd_pkt_len = 0;

	for (i = 0; i < num; i++) {
		rxm = rcv_pkts[i];
		if (unlikely(len[i] < ZXDH_UL_NET_HDR_SIZE)) {
			nb_enqueued++;
			PMD_RX_LOG(ERR, "RX, len:%u err", len[i]);
			zxdh_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}
		struct zxdh_net_hdr_ul *header =
			(struct zxdh_net_hdr_ul *)((char *)rxm->buf_addr +
			RTE_PKTMBUF_HEADROOM);

		seg_num  = header->type_hdr.num_buffers;
		if (seg_num == 0) {
			PMD_RX_LOG(ERR, "dequeue %d pkt, No.%d pkt seg_num is %d", num, i, seg_num);
			seg_num = 1;
		}
		if (seg_num > ZXDH_RX_MAX_SEGS) {
			PMD_RX_LOG(ERR, "dequeue %d pkt, No.%d pkt seg_num is %d", num, i, seg_num);
			nb_enqueued++;
			zxdh_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}
		/* bit[0:6]-pd_len unit:2B */
		uint16_t pd_len = header->type_hdr.pd_len << 1;
		if (pd_len > ZXDH_PD_HDR_SIZE_MAX || pd_len < ZXDH_PD_HDR_SIZE_MIN) {
			PMD_RX_LOG(ERR, "pd_len:%d is invalid", pd_len);
			nb_enqueued++;
			zxdh_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}
		/* Private queue only handle type hdr */
		hdr_size = pd_len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM + hdr_size;
		rxm->nb_segs = seg_num;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;
		rcvd_pkt_len = (uint32_t)(len[i] - hdr_size);
		rxm->data_len = (uint16_t)(len[i] - hdr_size);
		rxm->port = rxvq->port_id;
		rx_pkts[nb_rx] = rxm;
		prev = rxm;
		/* Update rte_mbuf according to pi/pd header */
		if (zxdh_rx_update_mbuf(rxm, header) < 0) {
			zxdh_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}
		seg_res = seg_num - 1;
		/* Merge remaining segments */
		while (seg_res != 0 && i < (num - 1)) {
			i++;
			rxm = rcv_pkts[i];
			rxm->data_off = RTE_PKTMBUF_HEADROOM;
			rxm->data_len = (uint16_t)(len[i]);

			rcvd_pkt_len += (uint32_t)(len[i]);
			prev->next = rxm;
			prev = rxm;
			rxm->next = NULL;
			seg_res -= 1;
		}

		if (!seg_res) {
			if (rcvd_pkt_len != rx_pkts[nb_rx]->pkt_len) {
				PMD_RX_LOG(ERR, "dropped rcvd_pkt_len %d pktlen %d",
					rcvd_pkt_len, rx_pkts[nb_rx]->pkt_len);
				zxdh_discard_rxbuf(vq, rx_pkts[nb_rx]);
				rxvq->stats.errors++;
				rxvq->stats.truncated_err++;
				continue;
			}
			zxdh_update_packet_stats(&rxvq->stats, rx_pkts[nb_rx]);
			nb_rx++;
		}
	}
	/* Last packet still need merge segments */
	while (seg_res != 0) {
		uint16_t rcv_cnt = RTE_MIN((uint16_t)seg_res, ZXDH_MBUF_BURST_SZ);
		uint16_t extra_idx = 0;

		rcv_cnt = zxdh_dequeue_burst_rx_packed(vq, rcv_pkts, len, rcv_cnt);
		if (unlikely(rcv_cnt == 0)) {
			PMD_RX_LOG(ERR, "No enough segments for packet");
			rte_pktmbuf_free(rx_pkts[nb_rx]);
			rxvq->stats.errors++;
			break;
		}
		while (extra_idx < rcv_cnt) {
			rxm = rcv_pkts[extra_idx];
			rxm->data_off = RTE_PKTMBUF_HEADROOM;
			rxm->pkt_len = (uint32_t)(len[extra_idx]);
			rxm->data_len = (uint16_t)(len[extra_idx]);
			prev->next = rxm;
			prev = rxm;
			rxm->next = NULL;
			rcvd_pkt_len += len[extra_idx];
			extra_idx += 1;
		}
		seg_res -= rcv_cnt;
		if (!seg_res) {
			if (rcvd_pkt_len != rx_pkts[nb_rx]->pkt_len) {
				PMD_RX_LOG(ERR, "dropped rcvd_pkt_len %d pktlen %d",
					rcvd_pkt_len, rx_pkts[nb_rx]->pkt_len);
				zxdh_discard_rxbuf(vq, rx_pkts[nb_rx]);
				rxvq->stats.errors++;
				rxvq->stats.truncated_err++;
				continue;
			}
			zxdh_update_packet_stats(&rxvq->stats, rx_pkts[nb_rx]);
			nb_rx++;
		}
	}
	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	if (likely(!zxdh_queue_full(vq))) {
		/* free_cnt may include mrg descs */
		uint16_t free_cnt = vq->vq_free_cnt;
		struct rte_mbuf *new_pkts[free_cnt];

		if (!rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts, free_cnt)) {
			error = zxdh_enqueue_recv_refill_packed(vq, new_pkts, free_cnt);
			if (unlikely(error)) {
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}
			nb_enqueued += free_cnt;
		} else {
			dev->data->rx_mbuf_alloc_failed += free_cnt;
		}
	}
	if (likely(nb_enqueued)) {
		if (unlikely(zxdh_queue_kick_prepare_packed(vq))) {
			zxdh_queue_notify(vq);
			PMD_RX_LOG(DEBUG, "Notified");
		}
	}
	return nb_rx;
}
