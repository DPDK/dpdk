/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#ifndef _IPSEC_WORKER_H_
#define _IPSEC_WORKER_H_

#include <rte_acl.h>
#include <rte_ethdev.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>

#include "ipsec.h"

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

enum pkt_type {
	PKT_TYPE_PLAIN_IPV4 = 1,
	PKT_TYPE_IPSEC_IPV4,
	PKT_TYPE_PLAIN_IPV6,
	PKT_TYPE_IPSEC_IPV6,
	PKT_TYPE_INVALID
};

enum {
	PKT_DROPPED = 0,
	PKT_FORWARDED,
	PKT_POSTED	/* for lookaside case */
};

struct route_table {
	struct rt_ctx *rt4_ctx;
	struct rt_ctx *rt6_ctx;
};

/*
 * Conf required by event mode worker with tx internal port
 */
struct lcore_conf_ev_tx_int_port_wrkr {
	struct ipsec_ctx inbound;
	struct ipsec_ctx outbound;
	struct route_table rt;
} __rte_cache_aligned;

void ipsec_poll_mode_worker(void);
void ipsec_poll_mode_wrkr_inl_pr(void);
void ipsec_poll_mode_wrkr_inl_pr_ss(void);

int ipsec_launch_one_lcore(void *args);

/*
 * helper routine for inline and cpu(synchronous) processing
 * this is just to satisfy inbound_sa_check() and get_hop_for_offload_pkt().
 * Should be removed in future.
 */
static inline void
prep_process_group(void *sa, struct rte_mbuf *mb[], uint32_t cnt)
{
	uint32_t j;
	struct ipsec_mbuf_metadata *priv;

	for (j = 0; j != cnt; j++) {
		priv = get_priv(mb[j]);
		priv->sa = sa;
		/* setup TSO related fields if TSO enabled*/
		if (priv->sa->mss) {
			uint32_t ptype = mb[j]->packet_type;
			/* only TCP is supported */
			if ((ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP) {
				mb[j]->tso_segsz = priv->sa->mss;
				if ((IS_TUNNEL(priv->sa->flags))) {
					mb[j]->outer_l3_len = mb[j]->l3_len;
					mb[j]->outer_l2_len = mb[j]->l2_len;
					mb[j]->ol_flags |=
						RTE_MBUF_F_TX_TUNNEL_ESP;
					if (RTE_ETH_IS_IPV4_HDR(ptype))
						mb[j]->ol_flags |=
						RTE_MBUF_F_TX_OUTER_IP_CKSUM;
				}
				mb[j]->l4_len = sizeof(struct rte_tcp_hdr);
				mb[j]->ol_flags |= (RTE_MBUF_F_TX_TCP_SEG |
						RTE_MBUF_F_TX_TCP_CKSUM);
				if (RTE_ETH_IS_IPV4_HDR(ptype))
					mb[j]->ol_flags |=
						RTE_MBUF_F_TX_OUTER_IPV4;
				else
					mb[j]->ol_flags |=
						RTE_MBUF_F_TX_OUTER_IPV6;
			}
		}
	}
}

static __rte_always_inline void
adjust_ipv4_pktlen(struct rte_mbuf *m, const struct rte_ipv4_hdr *iph,
	uint32_t l2_len)
{
	uint32_t plen, trim;

	plen = rte_be_to_cpu_16(iph->total_length) + l2_len;
	if (plen < m->pkt_len) {
		trim = m->pkt_len - plen;
		rte_pktmbuf_trim(m, trim);
	}
}

static __rte_always_inline void
adjust_ipv6_pktlen(struct rte_mbuf *m, const struct rte_ipv6_hdr *iph,
	uint32_t l2_len)
{
	uint32_t plen, trim;

	plen = rte_be_to_cpu_16(iph->payload_len) + sizeof(*iph) + l2_len;
	if (plen < m->pkt_len) {
		trim = m->pkt_len - plen;
		rte_pktmbuf_trim(m, trim);
	}
}

static __rte_always_inline void
prepare_one_packet(void *ctx, struct rte_mbuf *pkt,
		   struct ipsec_traffic *t)
{
	uint32_t ptype = pkt->packet_type;
	const struct rte_ipv4_hdr *iph4;
	const struct rte_ipv6_hdr *iph6;
	uint32_t tun_type, l3_type;
	uint64_t tx_offload;
	uint16_t l3len;

	if (is_ip_reassembly_incomplete(pkt) > 0) {
		free_reassembly_fail_pkt(pkt);
		return;
	}

	tun_type = ptype & RTE_PTYPE_TUNNEL_MASK;
	l3_type = ptype & RTE_PTYPE_L3_MASK;

	if (RTE_ETH_IS_IPV4_HDR(l3_type)) {
		iph4 = (const struct rte_ipv4_hdr *)rte_pktmbuf_adj(pkt,
			RTE_ETHER_HDR_LEN);
		adjust_ipv4_pktlen(pkt, iph4, 0);

		if (tun_type == RTE_PTYPE_TUNNEL_ESP) {
			t->ipsec.pkts[(t->ipsec.num)++] = pkt;
		} else {
			t->ip4.data[t->ip4.num] = &iph4->next_proto_id;
			t->ip4.pkts[(t->ip4.num)++] = pkt;
		}
		tx_offload = sizeof(*iph4) << RTE_MBUF_L2_LEN_BITS;
	} else if (RTE_ETH_IS_IPV6_HDR(l3_type)) {
		int next_proto;
		size_t ext_len;
		uint8_t *p;

		/* get protocol type */
		iph6 = (const struct rte_ipv6_hdr *)rte_pktmbuf_adj(pkt,
			RTE_ETHER_HDR_LEN);
		adjust_ipv6_pktlen(pkt, iph6, 0);

		l3len = sizeof(struct ip6_hdr);

		if (tun_type == RTE_PTYPE_TUNNEL_ESP) {
			t->ipsec.pkts[(t->ipsec.num)++] = pkt;
		} else {
			t->ip6.data[t->ip6.num] = &iph6->proto;
			t->ip6.pkts[(t->ip6.num)++] = pkt;
		}

		/* Determine l3 header size up to ESP extension by walking
		 * through extension headers.
		 */
		if (l3_type == RTE_PTYPE_L3_IPV6_EXT ||
		     l3_type == RTE_PTYPE_L3_IPV6_EXT_UNKNOWN) {
			p = rte_pktmbuf_mtod(pkt, uint8_t *);
			next_proto = iph6->proto;
			while (next_proto != IPPROTO_ESP &&
			       l3len < pkt->data_len &&
			       (next_proto = rte_ipv6_get_next_ext(p + l3len,
						next_proto, &ext_len)) >= 0)
				l3len += ext_len;

			/* Drop pkt when IPv6 header exceeds first seg size */
			if (unlikely(l3len > pkt->data_len)) {
				free_pkts(&pkt, 1);
				return;
			}
		}
		tx_offload = l3len << RTE_MBUF_L2_LEN_BITS;
	} else {
		/* Unknown/Unsupported type, drop the packet */
		RTE_LOG_DP(DEBUG, IPSEC, "Unsupported packet type 0x%x\n", ptype);
		free_pkts(&pkt, 1);
		return;
	}

	if  ((ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP)
		tx_offload |= (sizeof(struct rte_tcp_hdr) <<
			       (RTE_MBUF_L2_LEN_BITS + RTE_MBUF_L3_LEN_BITS));
	else if ((ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_UDP)
		tx_offload |= (sizeof(struct rte_udp_hdr) <<
			       (RTE_MBUF_L2_LEN_BITS + RTE_MBUF_L3_LEN_BITS));
	pkt->tx_offload = tx_offload;

	/* Check if the packet has been processed inline. For inline protocol
	 * processed packets, the metadata in the mbuf can be used to identify
	 * the security processing done on the packet. The metadata will be
	 * used to retrieve the application registered userdata associated
	 * with the security session.
	 */

	if (ctx && pkt->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD) {
		struct ipsec_sa *sa;
		struct ipsec_mbuf_metadata *priv;

		sa = *(struct ipsec_sa **)rte_security_dynfield(pkt);
		if (sa == NULL) {
			/* userdata could not be retrieved */
			return;
		}

		/* Save SA as priv member in mbuf. This will be used in the
		 * IPsec selector(SP-SA) check.
		 */

		priv = get_priv(pkt);
		priv->sa = sa;
	}
}

static __rte_always_inline void
prepare_traffic(void *ctx, struct rte_mbuf **pkts,
		struct ipsec_traffic *t, uint16_t nb_pkts)
{
	int32_t i;

	t->ipsec.num = 0;
	t->ip4.num = 0;
	t->ip6.num = 0;

	for (i = 0; i < (nb_pkts - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts[i + PREFETCH_OFFSET],
					void *));
		prepare_one_packet(ctx, pkts[i], t);
	}
	/* Process left packets */
	for (; i < nb_pkts; i++)
		prepare_one_packet(ctx, pkts[i], t);
}

/* Send burst of packets on an output interface */
static __rte_always_inline int32_t
send_burst(struct lcore_conf *qconf, uint16_t n, uint16_t port)
{
	struct rte_mbuf **m_table;
	int32_t ret;
	uint16_t queueid;

	queueid = qconf->tx_queue_id[port];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

	ret = rte_eth_tx_burst(port, queueid, m_table, n);

	core_stats_update_tx(ret);

	if (unlikely(ret < n)) {
		do {
			free_pkts(&m_table[ret], 1);
		} while (++ret < n);
	}

	return 0;
}

/*
 * Helper function to fragment and queue for TX one packet.
 */
static __rte_always_inline uint32_t
send_fragment_packet(struct lcore_conf *qconf, struct rte_mbuf *m,
	uint16_t port, uint8_t proto)
{
	struct rte_ether_hdr *ethhdr;
	struct rte_ipv4_hdr *ip;
	struct rte_mbuf *pkt;
	struct buffer *tbl;
	uint32_t len, n, i;
	int32_t rc;

	tbl =  qconf->tx_mbufs + port;
	len = tbl->len;

	/* free space for new fragments */
	if (len + RTE_LIBRTE_IP_FRAG_MAX_FRAG >=  RTE_DIM(tbl->m_table)) {
		send_burst(qconf, len, port);
		len = 0;
	}

	n = RTE_DIM(tbl->m_table) - len;

	/* Strip the ethernet header that was prepended earlier */
	rte_pktmbuf_adj(m, RTE_ETHER_HDR_LEN);

	if (proto == IPPROTO_IP)
		rc = rte_ipv4_fragment_packet(m, tbl->m_table + len,
			n, mtu_size, m->pool, qconf->frag.pool_indir);
	else
		rc = rte_ipv6_fragment_packet(m, tbl->m_table + len,
			n, mtu_size, m->pool, qconf->frag.pool_indir);

	if (rc < 0) {
		RTE_LOG(ERR, IPSEC,
			"%s: failed to fragment packet with size %u, "
			"error code: %d\n",
			__func__, m->pkt_len, rte_errno);
		rc = 0;
	}

	i = len;
	len += rc;
	for (; i < len; i++) {
		pkt = tbl->m_table[i];

		/* Update Ethernet header */
		ethhdr = (struct rte_ether_hdr *)
			rte_pktmbuf_prepend(pkt, RTE_ETHER_HDR_LEN);
		pkt->l2_len = RTE_ETHER_HDR_LEN;

		if (proto == IPPROTO_IP) {
			ethhdr->ether_type =
				rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
			/* Update minimum offload data */
			pkt->l3_len = sizeof(struct rte_ipv4_hdr);
			pkt->ol_flags |= qconf->outbound.ipv4_offloads;

			ip = (struct rte_ipv4_hdr *)(ethhdr + 1);
			ip->hdr_checksum = 0;

			/* calculate IPv4 cksum in SW */
			if ((pkt->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) == 0)
				ip->hdr_checksum = rte_ipv4_cksum(ip);
		} else {
			ethhdr->ether_type =
				rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);

			/* Update minimum offload data */
			pkt->l3_len = sizeof(struct rte_ipv6_hdr);
			pkt->ol_flags |= qconf->outbound.ipv6_offloads;
		}

		memcpy(&ethhdr->src_addr, &ethaddr_tbl[port].src,
		       sizeof(struct rte_ether_addr));
		memcpy(&ethhdr->dst_addr, &ethaddr_tbl[port].dst,
		       sizeof(struct rte_ether_addr));
	}

	free_pkts(&m, 1);
	return len;
}

/* Enqueue a single packet, and send burst if queue is filled */
static __rte_always_inline int32_t
send_single_packet(struct rte_mbuf *m, uint16_t port, uint8_t proto)
{
	uint32_t lcore_id;
	uint16_t len;
	struct lcore_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_conf[lcore_id];
	len = qconf->tx_mbufs[port].len;

	/* L2 header is already part of packet */
	if (m->pkt_len - RTE_ETHER_HDR_LEN <= mtu_size) {
		qconf->tx_mbufs[port].m_table[len] = m;
		len++;

	/* need to fragment the packet */
	} else if (frag_tbl_sz > 0)
		len = send_fragment_packet(qconf, m, port, proto);
	else
		free_pkts(&m, 1);

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}

	qconf->tx_mbufs[port].len = len;
	return 0;
}

static __rte_always_inline void
inbound_sp_sa(struct sp_ctx *sp, struct sa_ctx *sa, struct traffic_type *ip,
		uint16_t lim, struct ipsec_spd_stats *stats)
{
	struct rte_mbuf *m;
	uint32_t i, j, res, sa_idx;

	if (ip->num == 0 || sp == NULL)
		return;

	rte_acl_classify((struct rte_acl_ctx *)sp, ip->data, ip->res,
			ip->num, DEFAULT_MAX_CATEGORIES);

	j = 0;
	for (i = 0; i < ip->num; i++) {
		m = ip->pkts[i];
		res = ip->res[i];
		if (res == BYPASS) {
			ip->pkts[j++] = m;
			stats->bypass++;
			continue;
		}
		if (res == DISCARD) {
			free_pkts(&m, 1);
			stats->discard++;
			continue;
		}

		/* Only check SPI match for processed IPSec packets */
		if (i < lim && ((m->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD) == 0)) {
			stats->discard++;
			free_pkts(&m, 1);
			continue;
		}

		sa_idx = res - 1;
		if (!inbound_sa_check(sa, m, sa_idx)) {
			stats->discard++;
			free_pkts(&m, 1);
			continue;
		}
		ip->pkts[j++] = m;
		stats->protect++;
	}
	ip->num = j;
}

static __rte_always_inline uint32_t
get_hop_for_offload_pkt(struct rte_mbuf *pkt, int is_ipv6)
{
	struct ipsec_mbuf_metadata *priv;
	struct ipsec_sa *sa;

	priv = get_priv(pkt);

	sa = priv->sa;
	if (unlikely(sa == NULL)) {
		RTE_LOG(ERR, IPSEC, "SA not saved in private data\n");
		goto fail;
	}

	if (is_ipv6)
		return sa->portid;

	/* else */
	return (sa->portid | RTE_LPM_LOOKUP_SUCCESS);

fail:
	if (is_ipv6)
		return BAD_PORT;

	/* else */
	return 0;
}

static __rte_always_inline void
route4_pkts(struct rt_ctx *rt_ctx, struct rte_mbuf *pkts[],
	    uint32_t nb_pkts, uint64_t tx_offloads, bool ip_cksum)
{
	uint32_t hop[MAX_PKT_BURST * 2];
	uint32_t dst_ip[MAX_PKT_BURST * 2];
	struct rte_ether_hdr *ethhdr;
	uint32_t pkt_hop = 0;
	uint16_t i, offset;
	uint16_t lpm_pkts = 0;
	unsigned int lcoreid = rte_lcore_id();
	struct rte_mbuf *pkt;
	uint16_t port;

	if (nb_pkts == 0)
		return;

	/* Need to do an LPM lookup for non-inline packets. Inline packets will
	 * have port ID in the SA
	 */

	for (i = 0; i < nb_pkts; i++) {
		pkt = pkts[i];
		if (!(pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD)) {
			/* Security offload not enabled. So an LPM lookup is
			 * required to get the hop
			 */
			offset = offsetof(struct ip, ip_dst);
			dst_ip[lpm_pkts] = *rte_pktmbuf_mtod_offset(pkt,
					uint32_t *, offset);
			dst_ip[lpm_pkts] = rte_be_to_cpu_32(dst_ip[lpm_pkts]);
			lpm_pkts++;
		}
	}

	rte_lpm_lookup_bulk((struct rte_lpm *)rt_ctx, dst_ip, hop, lpm_pkts);

	lpm_pkts = 0;

	for (i = 0; i < nb_pkts; i++) {
		pkt = pkts[i];
		if (pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
			/* Read hop from the SA */
			pkt_hop = get_hop_for_offload_pkt(pkt, 0);
		} else {
			/* Need to use hop returned by lookup */
			pkt_hop = hop[lpm_pkts++];
		}

		if ((pkt_hop & RTE_LPM_LOOKUP_SUCCESS) == 0) {
			core_statistics[lcoreid].lpm4.miss++;
			free_pkts(&pkt, 1);
			continue;
		}

		port = pkt_hop & 0xff;

		/* Update minimum offload data */
		pkt->l3_len = sizeof(struct rte_ipv4_hdr);
		pkt->l2_len = RTE_ETHER_HDR_LEN;
		pkt->ol_flags |= RTE_MBUF_F_TX_IPV4;

		/* Update Ethernet header */
		ethhdr = (struct rte_ether_hdr *)
			rte_pktmbuf_prepend(pkt, RTE_ETHER_HDR_LEN);

		if (ip_cksum) {
			struct rte_ipv4_hdr *ip;

			pkt->ol_flags |= tx_offloads;

			ip = (struct rte_ipv4_hdr *)(ethhdr + 1);
			ip->hdr_checksum = 0;

			/* calculate IPv4 cksum in SW */
			if ((pkt->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) == 0)
				ip->hdr_checksum = rte_ipv4_cksum(ip);
		}

		ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		memcpy(&ethhdr->src_addr, &ethaddr_tbl[port].src,
		       sizeof(struct rte_ether_addr));
		memcpy(&ethhdr->dst_addr, &ethaddr_tbl[port].dst,
		       sizeof(struct rte_ether_addr));

		send_single_packet(pkt, port, IPPROTO_IP);
	}
}

static __rte_always_inline void
route6_pkts(struct rt_ctx *rt_ctx, struct rte_mbuf *pkts[], uint32_t nb_pkts)
{
	int32_t hop[MAX_PKT_BURST * 2];
	uint8_t dst_ip[MAX_PKT_BURST * 2][16];
	struct rte_ether_hdr *ethhdr;
	uint8_t *ip6_dst;
	uint32_t pkt_hop = 0;
	uint16_t i, offset;
	uint16_t lpm_pkts = 0;
	unsigned int lcoreid = rte_lcore_id();
	struct rte_mbuf *pkt;
	uint16_t port;

	if (nb_pkts == 0)
		return;

	/* Need to do an LPM lookup for non-inline packets. Inline packets will
	 * have port ID in the SA
	 */

	for (i = 0; i < nb_pkts; i++) {
		pkt = pkts[i];
		if (!(pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD)) {
			/* Security offload not enabled. So an LPM lookup is
			 * required to get the hop
			 */
			offset = offsetof(struct ip6_hdr, ip6_dst);
			ip6_dst = rte_pktmbuf_mtod_offset(pkt, uint8_t *,
					offset);
			memcpy(&dst_ip[lpm_pkts][0], ip6_dst, 16);
			lpm_pkts++;
		}
	}

	rte_lpm6_lookup_bulk_func((struct rte_lpm6 *)rt_ctx, dst_ip, hop,
			lpm_pkts);

	lpm_pkts = 0;

	for (i = 0; i < nb_pkts; i++) {
		pkt = pkts[i];
		if (pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
			/* Read hop from the SA */
			pkt_hop = get_hop_for_offload_pkt(pkt, 1);
		} else {
			/* Need to use hop returned by lookup */
			pkt_hop = (uint16_t)hop[lpm_pkts++];
		}

		if (pkt_hop == BAD_PORT) {
			core_statistics[lcoreid].lpm6.miss++;
			free_pkts(&pkt, 1);
			continue;
		}

		port = pkt_hop & 0xff;

		/* Update minimum offload data */
		pkt->ol_flags |= RTE_MBUF_F_TX_IPV6;
		pkt->l3_len = sizeof(struct ip6_hdr);
		pkt->l2_len = RTE_ETHER_HDR_LEN;

		/* Update Ethernet header */
		ethhdr = (struct rte_ether_hdr *)
			rte_pktmbuf_prepend(pkt, RTE_ETHER_HDR_LEN);

		ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(&ethhdr->src_addr, &ethaddr_tbl[port].src,
		       sizeof(struct rte_ether_addr));
		memcpy(&ethhdr->dst_addr, &ethaddr_tbl[port].dst,
		       sizeof(struct rte_ether_addr));

		send_single_packet(pkt, port, IPPROTO_IPV6);
	}
}

static __rte_always_inline void
drain_tx_buffers(struct lcore_conf *qconf)
{
	struct buffer *buf;
	uint32_t portid;

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		buf = &qconf->tx_mbufs[portid];
		if (buf->len == 0)
			continue;
		send_burst(qconf, buf->len, portid);
		buf->len = 0;
	}
}

#endif /* _IPSEC_WORKER_H_ */
