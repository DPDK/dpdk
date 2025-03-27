/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdint.h>
#include <stdalign.h>

#include <rte_net.h>

#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_common.h"
#include "zxdh_rxtx.h"
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

#define  ZXDH_PD_OFFLOAD_SPEC_PHYPORT         (1 << 15)
#define  ZXDH_PD_OFFLOAD_SVLAN_INSERT         (1 << 14)
#define  ZXDH_PD_OFFLOAD_CVLAN_INSERT         (1 << 13)
#define  ZXDH_PD_OFFLOAD_OUTER_IPCSUM         (1 << 12)
#define  ZXDH_PD_OFFLOAD_PRIO_MASK            (0x7 << 8)
#define  ZXDH_PD_OFFLOAD_DELAY_STAT           (1 << 7)

#define ZXDH_PCODE_MASK                       0x1F
#define ZXDH_PCODE_IP_PKT_TYPE                0x01
#define ZXDH_PCODE_TCP_PKT_TYPE               0x02
#define ZXDH_PCODE_UDP_PKT_TYPE               0x03
#define ZXDH_PCODE_NO_IP_PKT_TYPE             0x09
#define ZXDH_PCODE_NO_REASSMBLE_TCP_PKT_TYPE  0x0C

/* Uplink pd header byte0~1 */
#define ZXDH_MBUF_F_RX_OUTER_L4_CKSUM_GOOD               0x00080000
#define ZXDH_MBUF_F_RX_QINQ                              0x00100000
#define ZXDH_MBUF_F_RX_SEC_OFFLOAD                       0x00200000
#define ZXDH_MBUF_F_RX_QINQ_STRIPPED                     0x00400000
#define FELX_4BYTE                                       0x00800000
#define FELX_8BYTE                                       0x01000000
#define ZXDH_MBUF_F_RX_FDIR_FLX_MASK                     0x01800000
#define ZXDH_MBUF_F_RX_FDIR_ID                           0x02000000
#define ZXDH_MBUF_F_RX_1588_TMST                         0x04000000
#define ZXDH_MBUF_F_RX_1588_PTP                          0x08000000
#define ZXDH_MBUF_F_RX_VLAN_STRIPPED                     0x10000000
#define ZXDH_MBUF_F_RX_OUTER_IP_CKSUM_BAD                0x20000000
#define ZXDH_MBUF_F_RX_FDIR                              0x40000000
#define ZXDH_MBUF_F_RX_RSS_HASH                          0x80000000

/* Outer/Inner L2 type */
#define ZXDH_PD_L2TYPE_MASK                              0xf000
#define ZXDH_PTYPE_L2_ETHER                              0x1000
#define ZXDH_PTYPE_L2_ETHER_TIMESYNC                     0x2000
#define ZXDH_PTYPE_L2_ETHER_ARP                          0x3000
#define ZXDH_PTYPE_L2_ETHER_LLDP                         0x4000
#define ZXDH_PTYPE_L2_ETHER_NSH                          0x5000
#define ZXDH_PTYPE_L2_ETHER_VLAN                         0x6000
#define ZXDH_PTYPE_L2_ETHER_QINQ                         0x7000
#define ZXDH_PTYPE_L2_ETHER_PPPOE                        0x8000
#define ZXDH_PTYPE_L2_ETHER_FCOE                         0x9000
#define ZXDH_PTYPE_L2_ETHER_MPLS                         0xa000

/* Outer/Inner L3 type */
#define ZXDH_PD_L3TYPE_MASK                              0x0f00
#define ZXDH_PTYPE_L3_IPV4                               0x0100
#define ZXDH_PTYPE_L3_IPV4_EXT                           0x0200
#define ZXDH_PTYPE_L3_IPV6                               0x0300
#define ZXDH_PTYPE_L3_IPV4_EXT_UNKNOWN                   0x0400
#define ZXDH_PTYPE_L3_IPV6_EXT                           0x0500
#define ZXDH_PTYPE_L3_IPV6_EXT_UNKNOWN                   0x0600

/* Outer/Inner L4 type */
#define ZXDH_PD_L4TYPE_MASK    0x00f0
#define ZXDH_PTYPE_L4_TCP      0x0010
#define ZXDH_PTYPE_L4_UDP      0x0020
#define ZXDH_PTYPE_L4_FRAG     0x0030
#define ZXDH_PTYPE_L4_SCTP     0x0040
#define ZXDH_PTYPE_L4_ICMP     0x0050
#define ZXDH_PTYPE_L4_NONFRAG  0x0060
#define ZXDH_PTYPE_L4_IGMP     0x0070

#define ZXDH_TX_MAX_SEGS                      31
#define ZXDH_RX_MAX_SEGS                      31

#define ZXDH_PI_LRO_FLAG    0x00000001

#define ZXDH_MIN_MSS                                     64
#define ZXDH_VLAN_ID_MASK                                0xfff

#define ZXDH_MTU_MSS_UNIT_SHIFTBIT                       2
#define ZXDH_MTU_MSS_MASK                                0xFFF
#define ZXDH_PD_HDR_SIZE_MAX                             256

/* error code */
#define ZXDH_UDP_CSUM_ERR  0x0020
#define ZXDH_TCP_CSUM_ERR  0x0040
#define ZXDH_IPV4_CSUM_ERR 0x0100

#define ZXDH_DTPOFFLOAD_MASK ( \
		RTE_MBUF_F_TX_IP_CKSUM |        \
		RTE_MBUF_F_TX_L4_MASK |         \
		RTE_MBUF_F_TX_TCP_SEG |         \
		RTE_MBUF_F_TX_SEC_OFFLOAD |     \
		RTE_MBUF_F_TX_UDP_SEG)

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

static inline uint16_t
zxdh_get_mtu(struct zxdh_virtqueue *vq)
{
	struct rte_eth_dev *eth_dev = vq->hw->eth_dev;

	return eth_dev->data->mtu;
}

static void
zxdh_xmit_fill_net_hdr(struct zxdh_virtqueue *vq, struct rte_mbuf *cookie,
				struct zxdh_net_hdr_dl *hdr)
{
	uint16_t mtu_or_mss = 0;
	uint16_t pkt_flag_lw16 = ZXDH_NO_IPID_UPDATE;
	uint16_t l3_offset;
	uint8_t pcode = ZXDH_PCODE_NO_IP_PKT_TYPE;
	uint8_t l3_ptype = ZXDH_PI_L3TYPE_NOIP;
	struct zxdh_pi_hdr *pi_hdr = NULL;
	struct zxdh_pd_hdr_dl *pd_hdr = NULL;
	struct zxdh_hw *hw = vq->hw;
	struct zxdh_net_hdr_dl *net_hdr_dl = &g_net_hdr_dl[hw->port_id];
	uint8_t hdr_len = hw->dl_net_hdr_len;
	uint32_t ol_flag = 0;

	rte_memcpy(hdr, net_hdr_dl, hdr_len);
	if (hw->has_tx_offload) {
		pi_hdr = &hdr->pipd_hdr_dl.pi_hdr;
		pd_hdr = &hdr->pipd_hdr_dl.pd_hdr;

		pcode = ZXDH_PCODE_IP_PKT_TYPE;
		if (cookie->ol_flags & RTE_MBUF_F_TX_IPV6)
			l3_ptype = ZXDH_PI_L3TYPE_IPV6;
		else if (cookie->ol_flags & RTE_MBUF_F_TX_IPV4)
			l3_ptype = ZXDH_PI_L3TYPE_IP;
		else
			pcode = ZXDH_PCODE_NO_IP_PKT_TYPE;

		if (cookie->ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
			mtu_or_mss = (cookie->tso_segsz >= ZXDH_MIN_MSS)
				? cookie->tso_segsz
				: ZXDH_MIN_MSS;
			pi_hdr->pkt_flag_hi8  |= ZXDH_TX_TCPUDP_CKSUM_CAL;
			pkt_flag_lw16 |= ZXDH_NO_IP_FRAGMENT | ZXDH_TX_IP_CKSUM_CAL;
			pcode = ZXDH_PCODE_TCP_PKT_TYPE;
		} else if (cookie->ol_flags & RTE_MBUF_F_TX_UDP_SEG) {
			mtu_or_mss = zxdh_get_mtu(vq);
			mtu_or_mss = (mtu_or_mss >= ZXDH_MIN_MSS) ? mtu_or_mss : ZXDH_MIN_MSS;
			pkt_flag_lw16 |= ZXDH_TX_IP_CKSUM_CAL;
			pi_hdr->pkt_flag_hi8 |= ZXDH_NO_TCP_FRAGMENT | ZXDH_TX_TCPUDP_CKSUM_CAL;
			pcode = ZXDH_PCODE_UDP_PKT_TYPE;
		} else {
			pkt_flag_lw16 |= ZXDH_NO_IP_FRAGMENT;
			pi_hdr->pkt_flag_hi8 |= ZXDH_NO_TCP_FRAGMENT;
		}

		if (cookie->ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
			pkt_flag_lw16 |= ZXDH_TX_IP_CKSUM_CAL;

		if ((cookie->ol_flags & RTE_MBUF_F_TX_UDP_CKSUM) ==
			RTE_MBUF_F_TX_UDP_CKSUM){
			pcode = ZXDH_PCODE_UDP_PKT_TYPE;
			pi_hdr->pkt_flag_hi8 |= ZXDH_TX_TCPUDP_CKSUM_CAL;
		} else if ((cookie->ol_flags & RTE_MBUF_F_TX_TCP_CKSUM) ==
				 RTE_MBUF_F_TX_TCP_CKSUM) {
			pcode = ZXDH_PCODE_TCP_PKT_TYPE;
			pi_hdr->pkt_flag_hi8 |= ZXDH_TX_TCPUDP_CKSUM_CAL;
		}

		pkt_flag_lw16 |= (mtu_or_mss >> ZXDH_MTU_MSS_UNIT_SHIFTBIT) & ZXDH_MTU_MSS_MASK;
		pi_hdr->pkt_flag_lw16 = rte_be_to_cpu_16(pkt_flag_lw16);
		pi_hdr->pkt_type = l3_ptype | ZXDH_PKT_FORM_CPU | pcode;

		l3_offset = hdr_len + cookie->l2_len;
		l3_offset += (cookie->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
					cookie->outer_l2_len + cookie->outer_l3_len : 0;
		pi_hdr->l3_offset = rte_be_to_cpu_16(l3_offset);
		pi_hdr->l4_offset = rte_be_to_cpu_16(l3_offset + cookie->l3_len);
		if (cookie->ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM)
			ol_flag |= ZXDH_PD_OFFLOAD_OUTER_IPCSUM;
	} else {
		pd_hdr = &hdr->pd_hdr;
	}

	if (cookie->ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
		ol_flag |= ZXDH_PD_OFFLOAD_CVLAN_INSERT;
		pd_hdr->cvlan_insert = rte_be_to_cpu_16(cookie->vlan_tci);
		if (unlikely(cookie->ol_flags & RTE_MBUF_F_TX_QINQ)) {
			ol_flag |= ZXDH_PD_OFFLOAD_SVLAN_INSERT;
			pd_hdr->svlan_insert = rte_be_to_cpu_16(cookie->vlan_tci_outer);
		}
	}

	pd_hdr->ol_flag = rte_be_to_cpu_16(ol_flag);
}

static inline void
zxdh_enqueue_xmit_packed_fast(struct zxdh_virtnet_tx *txvq,
						struct rte_mbuf *cookie)
{
	struct zxdh_virtqueue *vq = txvq->vq;
	uint16_t id = vq->vq_avail_idx;
	struct zxdh_vq_desc_extra *dxp = &vq->vq_descx[id];
	uint16_t flags = vq->vq_packed.cached_flags;
	struct zxdh_net_hdr_dl *hdr = NULL;
	uint8_t hdr_len = vq->hw->dl_net_hdr_len;
	struct zxdh_vring_packed_desc *dp = &vq->vq_packed.ring.desc[id];

	dxp->ndescs = 1;
	dxp->cookie = cookie;
	hdr = rte_pktmbuf_mtod_offset(cookie, struct zxdh_net_hdr_dl *, -hdr_len);
	zxdh_xmit_fill_net_hdr(vq, cookie, hdr);

	dp->addr = rte_pktmbuf_iova(cookie) - hdr_len;
	dp->len  = cookie->data_len + hdr_len;
	dp->id   = id;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
	}
	vq->vq_free_cnt--;
	zxdh_queue_store_flags_packed(dp, flags);
}

static inline void
zxdh_enqueue_xmit_packed(struct zxdh_virtnet_tx *txvq,
						struct rte_mbuf *cookie,
						uint16_t needed)
{
	struct zxdh_tx_region *txr = txvq->zxdh_net_hdr_mz->addr;
	struct zxdh_virtqueue *vq = txvq->vq;
	uint16_t id = vq->vq_avail_idx;
	struct zxdh_vq_desc_extra *dxp = &vq->vq_descx[id];
	uint16_t head_idx = vq->vq_avail_idx;
	uint16_t idx = head_idx;
	struct zxdh_vring_packed_desc *start_dp = vq->vq_packed.ring.desc;
	struct zxdh_vring_packed_desc *head_dp = &vq->vq_packed.ring.desc[idx];
	struct zxdh_net_hdr_dl *hdr = NULL;

	uint16_t head_flags = cookie->next ? ZXDH_VRING_DESC_F_NEXT : 0;
	uint8_t hdr_len = vq->hw->dl_net_hdr_len;

	dxp->ndescs = needed;
	dxp->cookie = cookie;
	head_flags |= vq->vq_packed.cached_flags;

	start_dp[idx].addr = txvq->zxdh_net_hdr_mem + RTE_PTR_DIFF(&txr[idx].tx_hdr, txr);
	start_dp[idx].len  = hdr_len;
	head_flags |= ZXDH_VRING_DESC_F_NEXT;
	hdr = (void *)&txr[idx].tx_hdr;

	rte_prefetch1(hdr);
	idx++;
	if (idx >= vq->vq_nentries) {
		idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
	}

	zxdh_xmit_fill_net_hdr(vq, cookie, hdr);

	do {
		start_dp[idx].addr = rte_pktmbuf_iova(cookie);
		start_dp[idx].len  = cookie->data_len;
		start_dp[idx].id = id;
		if (likely(idx != head_idx)) {
			uint16_t flags = cookie->next ? ZXDH_VRING_DESC_F_NEXT : 0;

			flags |= vq->vq_packed.cached_flags;
			start_dp[idx].flags = flags;
		}

		idx++;
		if (idx >= vq->vq_nentries) {
			idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
		}
	} while ((cookie = cookie->next) != NULL);

	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - needed);
	vq->vq_avail_idx = idx;

	zxdh_queue_store_flags_packed(head_dp, head_flags);
}

static void
zxdh_update_packet_stats(struct zxdh_virtnet_stats *stats, struct rte_mbuf *mbuf)
{
	uint32_t s = mbuf->pkt_len;

	stats->bytes += s;
	#ifdef QUEUE_XSTAT
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
	#endif
}

static void
zxdh_xmit_flush(struct zxdh_virtqueue *vq)
{
	uint16_t id       = 0;
	uint16_t curr_id  = 0;
	uint16_t free_cnt = 0;
	uint16_t size     = vq->vq_nentries;
	struct zxdh_vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct zxdh_vq_desc_extra     *dxp  = NULL;
	uint16_t used_idx = vq->vq_used_cons_idx;

	/*
	 * The function desc_is_used performs a load-acquire operation
	 * or calls rte_io_rmb to ensure memory consistency. It waits
	 * for a used descriptor in the virtqueue.
	 */
	while (desc_is_used(&desc[used_idx], vq)) {
		id = desc[used_idx].id;
		do {
			curr_id = used_idx;
			dxp = &vq->vq_descx[used_idx];
			used_idx += dxp->ndescs;
			free_cnt += dxp->ndescs;
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

uint16_t
zxdh_xmit_pkts_packed(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct zxdh_virtnet_tx *txvq = tx_queue;
	struct zxdh_virtqueue  *vq   = txvq->vq;
	uint16_t nb_tx = 0;

	zxdh_xmit_flush(vq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *txm = tx_pkts[nb_tx];
		int32_t can_push     = 0;
		int32_t slots        = 0;
		int32_t need         = 0;

		rte_prefetch0(txm);
		/* optimize ring usage */
		if (rte_mbuf_refcnt_read(txm) == 1 &&
			RTE_MBUF_DIRECT(txm) &&
			txm->nb_segs == 1 &&
			txm->data_off >= ZXDH_DL_NET_HDR_SIZE) {
			can_push = 1;
		}
		/**
		 * How many main ring entries are needed to this Tx?
		 * indirect   => 1
		 * any_layout => number of segments
		 * default    => number of segments + 1
		 **/
		slots = txm->nb_segs + !can_push;
		need = slots - vq->vq_free_cnt;
		/* Positive value indicates it need free vring descriptors */
		if (unlikely(need > 0)) {
			zxdh_xmit_cleanup_inorder_packed(vq, need);
			need = slots - vq->vq_free_cnt;
			if (unlikely(need > 0)) {
				PMD_TX_LOG(ERR,
						" No enough %d free tx descriptors to transmit."
						"freecnt %d",
						need,
						vq->vq_free_cnt);
				break;
			}
		}

		/* Enqueue Packet buffers */
		if (can_push)
			zxdh_enqueue_xmit_packed_fast(txvq, txm);
		else
			zxdh_enqueue_xmit_packed(txvq, txm, slots);
		zxdh_update_packet_stats(&txvq->stats, txm);
	}
	txvq->stats.packets += nb_tx;
	if (likely(nb_tx))
		zxdh_queue_notify(vq);
	return nb_tx;
}

static inline int dl_net_hdr_check(struct rte_mbuf *m, struct zxdh_hw *hw)
{
	if ((m->ol_flags & ZXDH_DTPOFFLOAD_MASK) && !hw->has_tx_offload) {
		PMD_TX_LOG(ERR, "port:[%d], vfid[%d]. "
					"not support tx_offload", hw->port_id, hw->vfid);
		return -EINVAL;
	}
	return 0;
}

uint16_t zxdh_xmit_pkts_prepare(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts)
{
	struct zxdh_virtnet_tx *txvq = tx_queue;
	struct zxdh_hw *hw = txvq->vq->hw;
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

		error = dl_net_hdr_check(m, hw);
		if (unlikely(error)) {
			rte_errno = ENOTSUP;
			txvq->stats.errors += nb_pkts - nb_tx;
			txvq->stats.offload_cfg_err += nb_pkts - nb_tx;
			break;
		}
	}
	return nb_tx;
}

static uint16_t
zxdh_dequeue_burst_rx_packed(struct zxdh_virtqueue *vq,
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
		rte_prefetch0(cookie);
		rte_packet_prefetch(rte_pktmbuf_mtod(cookie, void *));
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

static inline void
zxdh_rx_update_mbuf(struct rte_mbuf *m, struct zxdh_net_hdr_ul *hdr)
{
	uint8_t has_pi = (uint64_t)(hdr->type_hdr.pd_len << 1) > ZXDH_UL_NOPI_HDR_SIZE_MAX;
	struct zxdh_pd_hdr_ul *pd_hdr = has_pi ? &hdr->pipd_hdr_ul.pd_hdr : &hdr->pd_hdr;
	uint32_t pkt_flag = ntohl(pd_hdr->pkt_flag);
	uint32_t idx = 0;
	uint32_t striped_vlan_tci = rte_be_to_cpu_32(pd_hdr->striped_vlan_tci);
	uint16_t pkt_type_outer = rte_be_to_cpu_16(pd_hdr->pkt_type_out);
	uint16_t pkt_type_inner = rte_be_to_cpu_16(pd_hdr->pkt_type_in);

	if (unlikely(pkt_flag & (ZXDH_MBUF_F_RX_1588_PTP | ZXDH_MBUF_F_RX_1588_TMST))) {
		if (pkt_flag & ZXDH_MBUF_F_RX_1588_PTP)
			m->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP;
		if (pkt_flag & ZXDH_MBUF_F_RX_1588_TMST)
			m->ol_flags |= RTE_MBUF_F_RX_IEEE1588_TMST;
	}

	if (pkt_flag & ZXDH_MBUF_F_RX_VLAN_STRIPPED) {
		m->ol_flags |= (RTE_MBUF_F_RX_VLAN_STRIPPED | RTE_MBUF_F_RX_VLAN);
		m->vlan_tci = (unlikely(pkt_flag & ZXDH_MBUF_F_RX_QINQ))
				? (striped_vlan_tci >> 16) & ZXDH_VLAN_ID_MASK
				: striped_vlan_tci & ZXDH_VLAN_ID_MASK;
	}

	if (unlikely(pkt_flag & ZXDH_MBUF_F_RX_QINQ_STRIPPED)) {
		/*
		 * When PKT_RX_QINQ_STRIPPED is set and PKT_RX_VLAN_STRIPPED is unset:
		 * - Only the outer VLAN is removed from the packet data.
		 * - Both TCI values are saved: the inner TCI in mbuf->vlan_tci and
		 *   the outer TCI in mbuf->vlan_tci_outer.
		 *
		 * When PKT_RX_QINQ is set, PKT_RX_VLAN must also be set, and the inner
		 * TCI is saved in mbuf->vlan_tci.
		 */
		m->ol_flags |= (RTE_MBUF_F_RX_QINQ_STRIPPED | RTE_MBUF_F_RX_QINQ);
		m->ol_flags |= (RTE_MBUF_F_RX_VLAN_STRIPPED | RTE_MBUF_F_RX_VLAN);
		m->vlan_tci = striped_vlan_tci & ZXDH_VLAN_ID_MASK;
		m->vlan_tci_outer = (striped_vlan_tci >> 16) & ZXDH_VLAN_ID_MASK;
	}

	/* rss hash/fd handle */
	if (pkt_flag & ZXDH_MBUF_F_RX_RSS_HASH) {
		m->hash.rss = rte_be_to_cpu_32(pd_hdr->rss_hash);
		m->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	}
	if (pkt_flag & ZXDH_MBUF_F_RX_FDIR) {
		m->ol_flags |= RTE_MBUF_F_RX_FDIR;
		if (pkt_flag & ZXDH_MBUF_F_RX_FDIR_ID) {
			m->hash.fdir.hi = rte_be_to_cpu_32(pd_hdr->fd);
			m->ol_flags |= RTE_MBUF_F_RX_FDIR_ID;
		} else if ((pkt_flag & ZXDH_MBUF_F_RX_FDIR_FLX_MASK) == FELX_4BYTE) {
			m->hash.fdir.hi = rte_be_to_cpu_32(pd_hdr->fd);
			m->ol_flags |= RTE_MBUF_F_RX_FDIR_FLX;
		} else if (((pkt_flag & ZXDH_MBUF_F_RX_FDIR_FLX_MASK) == FELX_8BYTE)) {
			m->hash.fdir.hi = rte_be_to_cpu_32(pd_hdr->rss_hash);
			m->hash.fdir.lo = rte_be_to_cpu_32(pd_hdr->fd);
			m->ol_flags |= RTE_MBUF_F_RX_FDIR_FLX;
		}
	}
	/* checksum handle */
	if (pkt_flag & ZXDH_MBUF_F_RX_OUTER_IP_CKSUM_BAD)
		m->ol_flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;
	if (pkt_flag & ZXDH_MBUF_F_RX_OUTER_L4_CKSUM_GOOD)
		m->ol_flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD;

	if (has_pi) {
		struct zxdh_pi_hdr *pi_hdr = &hdr->pipd_hdr_ul.pi_hdr;
		uint16_t pkt_type_masked = pi_hdr->pkt_type & ZXDH_PCODE_MASK;
		uint16_t err_code = rte_be_to_cpu_16(pi_hdr->ul.err_code);

		bool is_ip_pkt =
				(pi_hdr->pkt_type == ZXDH_PCODE_IP_PKT_TYPE) ||
				((pi_hdr->pkt_type & ZXDH_PI_L3TYPE_MASK) == ZXDH_PI_L3TYPE_IP);

		bool is_l4_pkt =
				(pkt_type_masked == ZXDH_PCODE_UDP_PKT_TYPE) ||
				(pkt_type_masked == ZXDH_PCODE_NO_REASSMBLE_TCP_PKT_TYPE) ||
				(pkt_type_masked == ZXDH_PCODE_TCP_PKT_TYPE);

		if (is_ip_pkt && (pi_hdr->pkt_flag_hi8 & ZXDH_RX_IP_CKSUM_VERIFY)) {
			if (err_code & ZXDH_IPV4_CSUM_ERR)
				m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
			else
				m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		}

		if (is_l4_pkt && (pi_hdr->pkt_flag_hi8 & ZXDH_RX_TCPUDP_CKSUM_VERIFY)) {
			if (err_code & (ZXDH_TCP_CSUM_ERR | ZXDH_UDP_CSUM_ERR))
				m->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
			else
				m->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		}

		if (ntohl(pi_hdr->ul.lro_flag) & ZXDH_PI_LRO_FLAG)
			m->ol_flags |= RTE_MBUF_F_RX_LRO;

		m->pkt_len = rte_be_to_cpu_16(pi_hdr->ul.pkt_len);
	} else {
		m->pkt_len = rte_be_to_cpu_16(pd_hdr->pkt_len);
	}

	idx = (pkt_type_outer >> 12) & 0xF;
	m->packet_type  = zxdh_outer_l2_type[idx];
	idx = (pkt_type_outer >> 8)  & 0xF;
	m->packet_type |= zxdh_outer_l3_type[idx];
	idx = (pkt_type_outer >> 4)  & 0xF;
	m->packet_type |= zxdh_outer_l4_type[idx];
	idx = pkt_type_outer         & 0xF;
	m->packet_type |= zxdh_tunnel_type[idx];

	if (pkt_type_inner) {
		idx = (pkt_type_inner >> 12) & 0xF;
		m->packet_type |= zxdh_inner_l2_type[idx];
		idx = (pkt_type_inner >> 8)  & 0xF;
		m->packet_type |= zxdh_inner_l3_type[idx];
		idx = (pkt_type_inner >> 4)  & 0xF;
		m->packet_type |= zxdh_inner_l4_type[idx];
	}
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

uint16_t
zxdh_recv_pkts_packed(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts)
{
	struct zxdh_virtnet_rx *rxvq = rx_queue;
	struct zxdh_virtqueue *vq = rxvq->vq;
	struct zxdh_hw *hw = vq->hw;
	struct rte_mbuf *rxm = NULL;
	struct rte_mbuf *prev = NULL;
	struct zxdh_net_hdr_ul *header = NULL;
	uint32_t lens[ZXDH_MBUF_BURST_SZ] = {0};
	struct rte_mbuf *rcv_pkts[ZXDH_MBUF_BURST_SZ] = {NULL};
	uint16_t len = 0;
	uint32_t seg_num = 0;
	uint32_t seg_res = 0;
	uint32_t error = 0;
	uint16_t hdr_size = 0;
	uint16_t nb_rx = 0;
	uint16_t i;
	uint16_t rcvd_pkt_len = 0;
	uint16_t num = nb_pkts;

	if (unlikely(num > ZXDH_MBUF_BURST_SZ))
		num = ZXDH_MBUF_BURST_SZ;

	num = zxdh_dequeue_burst_rx_packed(vq, rcv_pkts, lens, num);
	if (num == 0) {
		rxvq->stats.idle++;
		goto refill;
	}

	for (i = 0; i < num; i++) {
		rxm = rcv_pkts[i];
		rx_pkts[nb_rx] = rxm;
		prev = rxm;
		len = lens[i];
		header = rte_pktmbuf_mtod(rxm, struct zxdh_net_hdr_ul *);

		seg_num  = header->type_hdr.num_buffers;

		/* Private queue only handle type hdr */
		hdr_size = header->type_hdr.pd_len << 1;
		if (unlikely(hdr_size > lens[i] || hdr_size < ZXDH_TYPE_HDR_SIZE)) {
			PMD_RX_LOG(ERR, "hdr_size:%u is invalid", hdr_size);
			rte_pktmbuf_free(rxm);
			rxvq->stats.errors++;
			rxvq->stats.invalid_hdr_len_err++;
			continue;
		}
		rxm->data_off += hdr_size;
		rxm->nb_segs = seg_num;
		rxm->ol_flags = 0;
		rcvd_pkt_len = len - hdr_size;
		rxm->data_len = rcvd_pkt_len;
		rxm->port = rxvq->port_id;

		/* Update rte_mbuf according to pi/pd header */
		zxdh_rx_update_mbuf(rxm, header);
		seg_res = seg_num - 1;
		/* Merge remaining segments */
		while (seg_res != 0 && i < (num - 1)) {
			i++;
			len = lens[i];
			rxm = rcv_pkts[i];
			rxm->data_len = len;
			rcvd_pkt_len += len;
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

		rcv_cnt = zxdh_dequeue_burst_rx_packed(vq, rcv_pkts, lens, rcv_cnt);
		if (unlikely(rcv_cnt == 0)) {
			PMD_RX_LOG(ERR, "No enough segments for packet");
			rte_pktmbuf_free(rx_pkts[nb_rx]);
			rxvq->stats.errors++;
			rxvq->stats.no_segs_err++;
			break;
		}
		while (extra_idx < rcv_cnt) {
			rxm = rcv_pkts[extra_idx];
			rcvd_pkt_len += (uint16_t)(lens[extra_idx]);
			rxm->data_len = lens[extra_idx];
			prev->next = rxm;
			prev = rxm;
			rxm->next = NULL;
			extra_idx += 1;
		}
		seg_res -= rcv_cnt;
		if (!seg_res) {
			if (unlikely(rcvd_pkt_len != rx_pkts[nb_rx]->pkt_len)) {
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

refill:
	/* Allocate new mbuf for the used descriptor */
	if (likely(!zxdh_queue_full(vq))) {
		struct rte_mbuf *new_pkts[ZXDH_MBUF_BURST_SZ];
		/* free_cnt may include mrg descs */
		uint16_t free_cnt = RTE_MIN(vq->vq_free_cnt, ZXDH_MBUF_BURST_SZ);

		if (!rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts, free_cnt)) {
			error = zxdh_enqueue_recv_refill_packed(vq, new_pkts, free_cnt);
			if (unlikely(error)) {
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}

			if (unlikely(zxdh_queue_kick_prepare_packed(vq)))
				zxdh_queue_notify(vq);
		} else {
			struct rte_eth_dev *dev = hw->eth_dev;

			dev->data->rx_mbuf_alloc_failed += free_cnt;
		}
	}
	return nb_rx;
}
