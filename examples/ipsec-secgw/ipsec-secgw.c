/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_acl.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_cryptodev.h>

#include "ipsec.h"
#include "parser.h"

#define RTE_LOGTYPE_IPSEC RTE_LOGTYPE_USER1

#define MAX_JUMBO_PKT_LEN  9600

#define MEMPOOL_CACHE_SIZE 256

#define NB_MBUF	(32000)

#define CDEV_QUEUE_DESC 2048
#define CDEV_MAP_ENTRIES 1024
#define CDEV_MP_NB_OBJS 2048
#define CDEV_MP_CACHE_SZ 64
#define MAX_QUEUE_PAIRS 1

#define OPTION_CONFIG		"config"
#define OPTION_SINGLE_SA	"single-sa"

#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

#define NB_SOCKETS 4

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3

#define MAX_RX_QUEUE_PER_LCORE 16

#define MAX_LCORE_PARAMS 1024

#define UNPROTECTED_PORT(port) (unprotected_port_mask & (1 << portid))

/*
 * Configurable number of RX/TX ring descriptors
 */
#define IPSEC_SECGW_RX_DESC_DEFAULT 128
#define IPSEC_SECGW_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = IPSEC_SECGW_RX_DESC_DEFAULT;
static uint16_t nb_txd = IPSEC_SECGW_TX_DESC_DEFAULT;

#if RTE_BYTE_ORDER != RTE_LITTLE_ENDIAN
#define __BYTES_TO_UINT64(a, b, c, d, e, f, g, h) \
	(((uint64_t)((a) & 0xff) << 56) | \
	((uint64_t)((b) & 0xff) << 48) | \
	((uint64_t)((c) & 0xff) << 40) | \
	((uint64_t)((d) & 0xff) << 32) | \
	((uint64_t)((e) & 0xff) << 24) | \
	((uint64_t)((f) & 0xff) << 16) | \
	((uint64_t)((g) & 0xff) << 8)  | \
	((uint64_t)(h) & 0xff))
#else
#define __BYTES_TO_UINT64(a, b, c, d, e, f, g, h) \
	(((uint64_t)((h) & 0xff) << 56) | \
	((uint64_t)((g) & 0xff) << 48) | \
	((uint64_t)((f) & 0xff) << 40) | \
	((uint64_t)((e) & 0xff) << 32) | \
	((uint64_t)((d) & 0xff) << 24) | \
	((uint64_t)((c) & 0xff) << 16) | \
	((uint64_t)((b) & 0xff) << 8) | \
	((uint64_t)(a) & 0xff))
#endif
#define ETHADDR(a, b, c, d, e, f) (__BYTES_TO_UINT64(a, b, c, d, e, f, 0, 0))

#define ETHADDR_TO_UINT64(addr) __BYTES_TO_UINT64( \
		addr.addr_bytes[0], addr.addr_bytes[1], \
		addr.addr_bytes[2], addr.addr_bytes[3], \
		addr.addr_bytes[4], addr.addr_bytes[5], \
		0, 0)

/* port/source ethernet addr and destination ethernet addr */
struct ethaddr_info {
	uint64_t src, dst;
};

struct ethaddr_info ethaddr_tbl[RTE_MAX_ETHPORTS] = {
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x7e, 0x94, 0x9a) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x22, 0xa1, 0xd9) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x08, 0x69, 0x26) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x49, 0x9e, 0xdd) }
};

/* mask of enabled ports */
static uint32_t enabled_port_mask;
static uint32_t unprotected_port_mask;
static int32_t promiscuous_on = 1;
static int32_t numa_on = 1; /**< NUMA is enabled by default. */
static uint32_t nb_lcores;
static uint32_t single_sa;
static uint32_t single_sa_idx;

struct lcore_rx_queue {
	uint8_t port_id;
	uint8_t queue_id;
} __rte_cache_aligned;

struct lcore_params {
	uint8_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];

static struct lcore_params *lcore_params;
static uint16_t nb_lcore_params;

static struct rte_hash *cdev_map_in;
static struct rte_hash *cdev_map_out;

struct buffer {
	uint16_t len;
	struct rte_mbuf *m_table[MAX_PKT_BURST] __rte_aligned(sizeof(void *));
};

struct lcore_conf {
	uint16_t nb_rx_queue;
	struct lcore_rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
	uint16_t tx_queue_id[RTE_MAX_ETHPORTS];
	struct buffer tx_mbufs[RTE_MAX_ETHPORTS];
	struct ipsec_ctx inbound;
	struct ipsec_ctx outbound;
	struct rt_ctx *rt4_ctx;
	struct rt_ctx *rt6_ctx;
} __rte_cache_aligned;

static struct lcore_conf lcore_conf[RTE_MAX_LCORE];

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode	= ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 1, /**< IP checksum offload enabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP | ETH_RSS_UDP |
				ETH_RSS_TCP | ETH_RSS_SCTP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static struct socket_ctx socket_ctx[NB_SOCKETS];

struct traffic_type {
	const uint8_t *data[MAX_PKT_BURST * 2];
	struct rte_mbuf *pkts[MAX_PKT_BURST * 2];
	uint32_t res[MAX_PKT_BURST * 2];
	uint32_t num;
};

struct ipsec_traffic {
	struct traffic_type ipsec;
	struct traffic_type ip4;
	struct traffic_type ip6;
};

static inline void
prepare_one_packet(struct rte_mbuf *pkt, struct ipsec_traffic *t)
{
	uint8_t *nlp;
	struct ether_hdr *eth;

	eth = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	if (eth->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
		nlp = (uint8_t *)rte_pktmbuf_adj(pkt, ETHER_HDR_LEN);
		nlp = RTE_PTR_ADD(nlp, offsetof(struct ip, ip_p));
		if (*nlp == IPPROTO_ESP)
			t->ipsec.pkts[(t->ipsec.num)++] = pkt;
		else {
			t->ip4.data[t->ip4.num] = nlp;
			t->ip4.pkts[(t->ip4.num)++] = pkt;
		}
	} else if (eth->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6)) {
		nlp = (uint8_t *)rte_pktmbuf_adj(pkt, ETHER_HDR_LEN);
		nlp = RTE_PTR_ADD(nlp, offsetof(struct ip6_hdr, ip6_nxt));
		if (*nlp == IPPROTO_ESP)
			t->ipsec.pkts[(t->ipsec.num)++] = pkt;
		else {
			t->ip6.data[t->ip6.num] = nlp;
			t->ip6.pkts[(t->ip6.num)++] = pkt;
		}
	} else {
		/* Unknown/Unsupported type, drop the packet */
		RTE_LOG(ERR, IPSEC, "Unsupported packet type\n");
		rte_pktmbuf_free(pkt);
	}
}

static inline void
prepare_traffic(struct rte_mbuf **pkts, struct ipsec_traffic *t,
		uint16_t nb_pkts)
{
	int32_t i;

	t->ipsec.num = 0;
	t->ip4.num = 0;
	t->ip6.num = 0;

	for (i = 0; i < (nb_pkts - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts[i + PREFETCH_OFFSET],
					void *));
		prepare_one_packet(pkts[i], t);
	}
	/* Process left packets */
	for (; i < nb_pkts; i++)
		prepare_one_packet(pkts[i], t);
}

static inline void
prepare_tx_pkt(struct rte_mbuf *pkt, uint8_t port)
{
	struct ip *ip;
	struct ether_hdr *ethhdr;

	ip = rte_pktmbuf_mtod(pkt, struct ip *);

	ethhdr = (struct ether_hdr *)rte_pktmbuf_prepend(pkt, ETHER_HDR_LEN);

	if (ip->ip_v == IPVERSION) {
		pkt->ol_flags |= PKT_TX_IP_CKSUM | PKT_TX_IPV4;
		pkt->l3_len = sizeof(struct ip);
		pkt->l2_len = ETHER_HDR_LEN;

		ethhdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	} else {
		pkt->ol_flags |= PKT_TX_IPV6;
		pkt->l3_len = sizeof(struct ip6_hdr);
		pkt->l2_len = ETHER_HDR_LEN;

		ethhdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
	}

	memcpy(&ethhdr->s_addr, &ethaddr_tbl[port].src,
			sizeof(struct ether_addr));
	memcpy(&ethhdr->d_addr, &ethaddr_tbl[port].dst,
			sizeof(struct ether_addr));
}

static inline void
prepare_tx_burst(struct rte_mbuf *pkts[], uint16_t nb_pkts, uint8_t port)
{
	int32_t i;
	const int32_t prefetch_offset = 2;

	for (i = 0; i < (nb_pkts - prefetch_offset); i++) {
		rte_mbuf_prefetch_part2(pkts[i + prefetch_offset]);
		prepare_tx_pkt(pkts[i], port);
	}
	/* Process left packets */
	for (; i < nb_pkts; i++)
		prepare_tx_pkt(pkts[i], port);
}

/* Send burst of packets on an output interface */
static inline int32_t
send_burst(struct lcore_conf *qconf, uint16_t n, uint8_t port)
{
	struct rte_mbuf **m_table;
	int32_t ret;
	uint16_t queueid;

	queueid = qconf->tx_queue_id[port];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

	prepare_tx_burst(m_table, n, port);

	ret = rte_eth_tx_burst(port, queueid, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	return 0;
}

/* Enqueue a single packet, and send burst if queue is filled */
static inline int32_t
send_single_packet(struct rte_mbuf *m, uint8_t port)
{
	uint32_t lcore_id;
	uint16_t len;
	struct lcore_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_conf[lcore_id];
	len = qconf->tx_mbufs[port].len;
	qconf->tx_mbufs[port].m_table[len] = m;
	len++;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}

	qconf->tx_mbufs[port].len = len;
	return 0;
}

static inline void
inbound_sp_sa(struct sp_ctx *sp, struct sa_ctx *sa, struct traffic_type *ip,
		uint16_t lim)
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
		if (res & BYPASS) {
			ip->pkts[j++] = m;
			continue;
		}
		if (res & DISCARD || i < lim) {
			rte_pktmbuf_free(m);
			continue;
		}
		/* Only check SPI match for processed IPSec packets */
		sa_idx = ip->res[i] & PROTECT_MASK;
		if (sa_idx == 0 || !inbound_sa_check(sa, m, sa_idx)) {
			rte_pktmbuf_free(m);
			continue;
		}
		ip->pkts[j++] = m;
	}
	ip->num = j;
}

static inline void
process_pkts_inbound(struct ipsec_ctx *ipsec_ctx,
		struct ipsec_traffic *traffic)
{
	struct rte_mbuf *m;
	uint16_t idx, nb_pkts_in, i, n_ip4, n_ip6;

	nb_pkts_in = ipsec_inbound(ipsec_ctx, traffic->ipsec.pkts,
			traffic->ipsec.num, MAX_PKT_BURST);

	n_ip4 = traffic->ip4.num;
	n_ip6 = traffic->ip6.num;

	/* SP/ACL Inbound check ipsec and ip4 */
	for (i = 0; i < nb_pkts_in; i++) {
		m = traffic->ipsec.pkts[i];
		struct ip *ip = rte_pktmbuf_mtod(m, struct ip *);
		if (ip->ip_v == IPVERSION) {
			idx = traffic->ip4.num++;
			traffic->ip4.pkts[idx] = m;
			traffic->ip4.data[idx] = rte_pktmbuf_mtod_offset(m,
					uint8_t *, offsetof(struct ip, ip_p));
		} else if (ip->ip_v == IP6_VERSION) {
			idx = traffic->ip6.num++;
			traffic->ip6.pkts[idx] = m;
			traffic->ip6.data[idx] = rte_pktmbuf_mtod_offset(m,
					uint8_t *,
					offsetof(struct ip6_hdr, ip6_nxt));
		} else
			rte_pktmbuf_free(m);
	}

	inbound_sp_sa(ipsec_ctx->sp4_ctx, ipsec_ctx->sa_ctx, &traffic->ip4,
			n_ip4);

	inbound_sp_sa(ipsec_ctx->sp6_ctx, ipsec_ctx->sa_ctx, &traffic->ip6,
			n_ip6);
}

static inline void
outbound_sp(struct sp_ctx *sp, struct traffic_type *ip,
		struct traffic_type *ipsec)
{
	struct rte_mbuf *m;
	uint32_t i, j, sa_idx;

	if (ip->num == 0 || sp == NULL)
		return;

	rte_acl_classify((struct rte_acl_ctx *)sp, ip->data, ip->res,
			ip->num, DEFAULT_MAX_CATEGORIES);

	j = 0;
	for (i = 0; i < ip->num; i++) {
		m = ip->pkts[i];
		sa_idx = ip->res[i] & PROTECT_MASK;
		if ((ip->res[i] == 0) || (ip->res[i] & DISCARD))
			rte_pktmbuf_free(m);
		else if (sa_idx != 0) {
			ipsec->res[ipsec->num] = sa_idx;
			ipsec->pkts[ipsec->num++] = m;
		} else /* BYPASS */
			ip->pkts[j++] = m;
	}
	ip->num = j;
}

static inline void
process_pkts_outbound(struct ipsec_ctx *ipsec_ctx,
		struct ipsec_traffic *traffic)
{
	struct rte_mbuf *m;
	uint16_t idx, nb_pkts_out, i;

	/* Drop any IPsec traffic from protected ports */
	for (i = 0; i < traffic->ipsec.num; i++)
		rte_pktmbuf_free(traffic->ipsec.pkts[i]);

	traffic->ipsec.num = 0;

	outbound_sp(ipsec_ctx->sp4_ctx, &traffic->ip4, &traffic->ipsec);

	outbound_sp(ipsec_ctx->sp6_ctx, &traffic->ip6, &traffic->ipsec);

	nb_pkts_out = ipsec_outbound(ipsec_ctx, traffic->ipsec.pkts,
			traffic->ipsec.res, traffic->ipsec.num,
			MAX_PKT_BURST);

	for (i = 0; i < nb_pkts_out; i++) {
		m = traffic->ipsec.pkts[i];
		struct ip *ip = rte_pktmbuf_mtod(m, struct ip *);
		if (ip->ip_v == IPVERSION) {
			idx = traffic->ip4.num++;
			traffic->ip4.pkts[idx] = m;
		} else {
			idx = traffic->ip6.num++;
			traffic->ip6.pkts[idx] = m;
		}
	}
}

static inline void
process_pkts_inbound_nosp(struct ipsec_ctx *ipsec_ctx,
		struct ipsec_traffic *traffic)
{
	struct rte_mbuf *m;
	uint32_t nb_pkts_in, i, idx;

	/* Drop any IPv4 traffic from unprotected ports */
	for (i = 0; i < traffic->ip4.num; i++)
		rte_pktmbuf_free(traffic->ip4.pkts[i]);

	traffic->ip4.num = 0;

	/* Drop any IPv6 traffic from unprotected ports */
	for (i = 0; i < traffic->ip6.num; i++)
		rte_pktmbuf_free(traffic->ip6.pkts[i]);

	traffic->ip6.num = 0;

	nb_pkts_in = ipsec_inbound(ipsec_ctx, traffic->ipsec.pkts,
			traffic->ipsec.num, MAX_PKT_BURST);

	for (i = 0; i < nb_pkts_in; i++) {
		m = traffic->ipsec.pkts[i];
		struct ip *ip = rte_pktmbuf_mtod(m, struct ip *);
		if (ip->ip_v == IPVERSION) {
			idx = traffic->ip4.num++;
			traffic->ip4.pkts[idx] = m;
		} else {
			idx = traffic->ip6.num++;
			traffic->ip6.pkts[idx] = m;
		}
	}
}

static inline void
process_pkts_outbound_nosp(struct ipsec_ctx *ipsec_ctx,
		struct ipsec_traffic *traffic)
{
	struct rte_mbuf *m;
	uint32_t nb_pkts_out, i;
	struct ip *ip;

	/* Drop any IPsec traffic from protected ports */
	for (i = 0; i < traffic->ipsec.num; i++)
		rte_pktmbuf_free(traffic->ipsec.pkts[i]);

	traffic->ipsec.num = 0;

	for (i = 0; i < traffic->ip4.num; i++)
		traffic->ip4.res[i] = single_sa_idx;

	for (i = 0; i < traffic->ip6.num; i++)
		traffic->ip6.res[i] = single_sa_idx;

	nb_pkts_out = ipsec_outbound(ipsec_ctx, traffic->ip4.pkts,
			traffic->ip4.res, traffic->ip4.num,
			MAX_PKT_BURST);

	/* They all sue the same SA (ip4 or ip6 tunnel) */
	m = traffic->ipsec.pkts[i];
	ip = rte_pktmbuf_mtod(m, struct ip *);
	if (ip->ip_v == IPVERSION)
		traffic->ip4.num = nb_pkts_out;
	else
		traffic->ip6.num = nb_pkts_out;
}

static inline void
route4_pkts(struct rt_ctx *rt_ctx, struct rte_mbuf *pkts[], uint8_t nb_pkts)
{
	uint32_t hop[MAX_PKT_BURST * 2];
	uint32_t dst_ip[MAX_PKT_BURST * 2];
	uint16_t i, offset;

	if (nb_pkts == 0)
		return;

	for (i = 0; i < nb_pkts; i++) {
		offset = offsetof(struct ip, ip_dst);
		dst_ip[i] = *rte_pktmbuf_mtod_offset(pkts[i],
				uint32_t *, offset);
		dst_ip[i] = rte_be_to_cpu_32(dst_ip[i]);
	}

	rte_lpm_lookup_bulk((struct rte_lpm *)rt_ctx, dst_ip, hop, nb_pkts);

	for (i = 0; i < nb_pkts; i++) {
		if ((hop[i] & RTE_LPM_LOOKUP_SUCCESS) == 0) {
			rte_pktmbuf_free(pkts[i]);
			continue;
		}
		send_single_packet(pkts[i], hop[i] & 0xff);
	}
}

static inline void
route6_pkts(struct rt_ctx *rt_ctx, struct rte_mbuf *pkts[], uint8_t nb_pkts)
{
	int32_t hop[MAX_PKT_BURST * 2];
	uint8_t dst_ip[MAX_PKT_BURST * 2][16];
	uint8_t *ip6_dst;
	uint16_t i, offset;

	if (nb_pkts == 0)
		return;

	for (i = 0; i < nb_pkts; i++) {
		offset = offsetof(struct ip6_hdr, ip6_dst);
		ip6_dst = rte_pktmbuf_mtod_offset(pkts[i], uint8_t *, offset);
		memcpy(&dst_ip[i][0], ip6_dst, 16);
	}

	rte_lpm6_lookup_bulk_func((struct rte_lpm6 *)rt_ctx, dst_ip,
			hop, nb_pkts);

	for (i = 0; i < nb_pkts; i++) {
		if (hop[i] == -1) {
			rte_pktmbuf_free(pkts[i]);
			continue;
		}
		send_single_packet(pkts[i], hop[i] & 0xff);
	}
}

static inline void
process_pkts(struct lcore_conf *qconf, struct rte_mbuf **pkts,
		uint8_t nb_pkts, uint8_t portid)
{
	struct ipsec_traffic traffic;

	prepare_traffic(pkts, &traffic, nb_pkts);

	if (unlikely(single_sa)) {
		if (UNPROTECTED_PORT(portid))
			process_pkts_inbound_nosp(&qconf->inbound, &traffic);
		else
			process_pkts_outbound_nosp(&qconf->outbound, &traffic);
	} else {
		if (UNPROTECTED_PORT(portid))
			process_pkts_inbound(&qconf->inbound, &traffic);
		else
			process_pkts_outbound(&qconf->outbound, &traffic);
	}

	route4_pkts(qconf->rt4_ctx, traffic.ip4.pkts, traffic.ip4.num);
	route6_pkts(qconf->rt6_ctx, traffic.ip6.pkts, traffic.ip6.num);
}

static inline void
drain_buffers(struct lcore_conf *qconf)
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

/* main processing loop */
static int32_t
main_loop(__attribute__((unused)) void *dummy)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	uint32_t lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int32_t i, nb_rx;
	uint8_t portid, queueid;
	struct lcore_conf *qconf;
	int32_t socket_id;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1)
			/ US_PER_S * BURST_TX_DRAIN_US;
	struct lcore_rx_queue *rxql;

	prev_tsc = 0;
	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];
	rxql = qconf->rx_queue_list;
	socket_id = rte_lcore_to_socket_id(lcore_id);

	qconf->rt4_ctx = socket_ctx[socket_id].rt_ip4;
	qconf->rt6_ctx = socket_ctx[socket_id].rt_ip6;
	qconf->inbound.sp4_ctx = socket_ctx[socket_id].sp_ip4_in;
	qconf->inbound.sp6_ctx = socket_ctx[socket_id].sp_ip6_in;
	qconf->inbound.sa_ctx = socket_ctx[socket_id].sa_in;
	qconf->inbound.cdev_map = cdev_map_in;
	qconf->inbound.session_pool = socket_ctx[socket_id].session_pool;
	qconf->outbound.sp4_ctx = socket_ctx[socket_id].sp_ip4_out;
	qconf->outbound.sp6_ctx = socket_ctx[socket_id].sp_ip6_out;
	qconf->outbound.sa_ctx = socket_ctx[socket_id].sa_out;
	qconf->outbound.cdev_map = cdev_map_out;
	qconf->outbound.session_pool = socket_ctx[socket_id].session_pool;

	if (qconf->nb_rx_queue == 0) {
		RTE_LOG(INFO, IPSEC, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, IPSEC, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->nb_rx_queue; i++) {
		portid = rxql[i].port_id;
		queueid = rxql[i].queue_id;
		RTE_LOG(INFO, IPSEC,
			" -- lcoreid=%u portid=%hhu rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	while (1) {
		cur_tsc = rte_rdtsc();

		/* TX queue buffer drain */
		diff_tsc = cur_tsc - prev_tsc;

		if (unlikely(diff_tsc > drain_tsc)) {
			drain_buffers(qconf);
			prev_tsc = cur_tsc;
		}

		/* Read packet from RX queues */
		for (i = 0; i < qconf->nb_rx_queue; ++i) {
			portid = rxql[i].port_id;
			queueid = rxql[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid,
					pkts, MAX_PKT_BURST);

			if (nb_rx > 0)
				process_pkts(qconf, pkts, nb_rx, portid);
		}
	}
}

static int32_t
check_params(void)
{
	uint8_t lcore, portid, nb_ports;
	uint16_t i;
	int32_t socket_id;

	if (lcore_params == NULL) {
		printf("Error: No port/queue/core mappings\n");
		return -1;
	}

	nb_ports = rte_eth_dev_count();

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			printf("error: lcore %hhu is not enabled in "
				"lcore mask\n", lcore);
			return -1;
		}
		socket_id = rte_lcore_to_socket_id(lcore);
		if (socket_id != 0 && numa_on == 0) {
			printf("warning: lcore %hhu is on socket %d "
				"with numa off\n",
				lcore, socket_id);
		}
		portid = lcore_params[i].port_id;
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("port %u is not enabled in port mask\n", portid);
			return -1;
		}
		if (portid >= nb_ports) {
			printf("port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static uint8_t
get_port_nb_rx_queues(const uint8_t port)
{
	int32_t queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port &&
				lcore_params[i].queue_id > queue)
			queue = lcore_params[i].queue_id;
	}
	return (uint8_t)(++queue);
}

static int32_t
init_lcore_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t lcore;

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		nb_rx_queue = lcore_conf[lcore].nb_rx_queue;
		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			printf("error: too many queues (%u) for lcore: %u\n",
					nb_rx_queue + 1, lcore);
			return -1;
		}
		lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
			lcore_params[i].port_id;
		lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
			lcore_params[i].queue_id;
		lcore_conf[lcore].nb_rx_queue++;
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK -P -u PORTMASK"
		"  --"OPTION_CONFIG" (port,queue,lcore)[,(port,queue,lcore]"
		" --single-sa SAIDX -f CONFIG_FILE\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
		"  -P : enable promiscuous mode\n"
		"  -u PORTMASK: hexadecimal bitmask of unprotected ports\n"
		"  --"OPTION_CONFIG": (port,queue,lcore): "
		"rx queues configuration\n"
		"  --single-sa SAIDX: use single SA index for outbound, "
		"bypassing the SP\n"
		"  -f CONFIG_FILE: Configuration file path\n",
		prgname);
}

static int32_t
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if ((pm == 0) && errno)
		return -1;

	return pm;
}

static int32_t
parse_decimal(const char *str)
{
	char *end = NULL;
	unsigned long num;

	num = strtoul(str, &end, 10);
	if ((str[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	return num;
}

static int32_t
parse_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int32_t i;
	uint32_t size;

	nb_lcore_params = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') !=
				_NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of lcore params: %hu\n",
				nb_lcore_params);
			return -1;
		}
		lcore_params_array[nb_lcore_params].port_id =
			(uint8_t)int_fld[FLD_PORT];
		lcore_params_array[nb_lcore_params].queue_id =
			(uint8_t)int_fld[FLD_QUEUE];
		lcore_params_array[nb_lcore_params].lcore_id =
			(uint8_t)int_fld[FLD_LCORE];
		++nb_lcore_params;
	}
	lcore_params = lcore_params_array;
	return 0;
}

#define __STRNCMP(name, opt) (!strncmp(name, opt, sizeof(opt)))
static int32_t
parse_args_long_options(struct option *lgopts, int32_t option_index)
{
	int32_t ret = -1;
	const char *optname = lgopts[option_index].name;

	if (__STRNCMP(optname, OPTION_CONFIG)) {
		ret = parse_config(optarg);
		if (ret)
			printf("invalid config\n");
	}

	if (__STRNCMP(optname, OPTION_SINGLE_SA)) {
		ret = parse_decimal(optarg);
		if (ret != -1) {
			single_sa = 1;
			single_sa_idx = ret;
			printf("Configured with single SA index %u\n",
					single_sa_idx);
			ret = 0;
		}
	}

	return ret;
}
#undef __STRNCMP

static int32_t
parse_args(int32_t argc, char **argv)
{
	int32_t opt, ret;
	char **argvopt;
	int32_t option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{OPTION_CONFIG, 1, 0, 0},
		{OPTION_SINGLE_SA, 1, 0, 0},
		{NULL, 0, 0, 0}
	};
	int32_t f_present = 0;

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:Pu:f:",
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				printf("invalid portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;
		case 'P':
			printf("Promiscuous mode selected\n");
			promiscuous_on = 1;
			break;
		case 'u':
			unprotected_port_mask = parse_portmask(optarg);
			if (unprotected_port_mask == 0) {
				printf("invalid unprotected portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;
		case 'f':
			if (f_present == 1) {
				printf("\"-f\" option present more than "
					"once!\n");
				print_usage(prgname);
				return -1;
			}
			if (parse_cfg_file(optarg) < 0) {
				printf("parsing file \"%s\" failed\n",
					optarg);
				print_usage(prgname);
				return -1;
			}
			f_present = 1;
			break;
		case 0:
			if (parse_args_long_options(lgopts, option_index)) {
				print_usage(prgname);
				return -1;
			}
			break;
		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (f_present == 0) {
		printf("Mandatory option \"-f\" not present\n");
		return -1;
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, const struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(uint32_t)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static int32_t
add_mapping(struct rte_hash *map, const char *str, uint16_t cdev_id,
		uint16_t qp, struct lcore_params *params,
		struct ipsec_ctx *ipsec_ctx,
		const struct rte_cryptodev_capabilities *cipher,
		const struct rte_cryptodev_capabilities *auth)
{
	int32_t ret = 0;
	unsigned long i;
	struct cdev_key key = { 0 };

	key.lcore_id = params->lcore_id;
	if (cipher)
		key.cipher_algo = cipher->sym.cipher.algo;
	if (auth)
		key.auth_algo = auth->sym.auth.algo;

	ret = rte_hash_lookup(map, &key);
	if (ret != -ENOENT)
		return 0;

	for (i = 0; i < ipsec_ctx->nb_qps; i++)
		if (ipsec_ctx->tbl[i].id == cdev_id)
			break;

	if (i == ipsec_ctx->nb_qps) {
		if (ipsec_ctx->nb_qps == MAX_QP_PER_LCORE) {
			printf("Maximum number of crypto devices assigned to "
				"a core, increase MAX_QP_PER_LCORE value\n");
			return 0;
		}
		ipsec_ctx->tbl[i].id = cdev_id;
		ipsec_ctx->tbl[i].qp = qp;
		ipsec_ctx->nb_qps++;
		printf("%s cdev mapping: lcore %u using cdev %u qp %u "
				"(cdev_id_qp %lu)\n", str, key.lcore_id,
				cdev_id, qp, i);
	}

	ret = rte_hash_add_key_data(map, &key, (void *)i);
	if (ret < 0) {
		printf("Faled to insert cdev mapping for (lcore %u, "
				"cdev %u, qp %u), errno %d\n",
				key.lcore_id, ipsec_ctx->tbl[i].id,
				ipsec_ctx->tbl[i].qp, ret);
		return 0;
	}

	return 1;
}

static int32_t
add_cdev_mapping(struct rte_cryptodev_info *dev_info, uint16_t cdev_id,
		uint16_t qp, struct lcore_params *params)
{
	int32_t ret = 0;
	const struct rte_cryptodev_capabilities *i, *j;
	struct rte_hash *map;
	struct lcore_conf *qconf;
	struct ipsec_ctx *ipsec_ctx;
	const char *str;

	qconf = &lcore_conf[params->lcore_id];

	if ((unprotected_port_mask & (1 << params->port_id)) == 0) {
		map = cdev_map_out;
		ipsec_ctx = &qconf->outbound;
		str = "Outbound";
	} else {
		map = cdev_map_in;
		ipsec_ctx = &qconf->inbound;
		str = "Inbound";
	}

	/* Required cryptodevs with operation chainning */
	if (!(dev_info->feature_flags &
				RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING))
		return ret;

	for (i = dev_info->capabilities;
			i->op != RTE_CRYPTO_OP_TYPE_UNDEFINED; i++) {
		if (i->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
			continue;

		if (i->sym.xform_type != RTE_CRYPTO_SYM_XFORM_CIPHER)
			continue;

		for (j = dev_info->capabilities;
				j->op != RTE_CRYPTO_OP_TYPE_UNDEFINED; j++) {
			if (j->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
				continue;

			if (j->sym.xform_type != RTE_CRYPTO_SYM_XFORM_AUTH)
				continue;

			ret |= add_mapping(map, str, cdev_id, qp, params,
					ipsec_ctx, i, j);
		}
	}

	return ret;
}

static int32_t
cryptodevs_init(void)
{
	struct rte_cryptodev_config dev_conf;
	struct rte_cryptodev_qp_conf qp_conf;
	uint16_t idx, max_nb_qps, qp, i;
	int16_t cdev_id;
	struct rte_hash_parameters params = { 0 };

	params.entries = CDEV_MAP_ENTRIES;
	params.key_len = sizeof(struct cdev_key);
	params.hash_func = rte_jhash;
	params.hash_func_init_val = 0;
	params.socket_id = rte_socket_id();

	params.name = "cdev_map_in";
	cdev_map_in = rte_hash_create(&params);
	if (cdev_map_in == NULL)
		rte_panic("Failed to create cdev_map hash table, errno = %d\n",
				rte_errno);

	params.name = "cdev_map_out";
	cdev_map_out = rte_hash_create(&params);
	if (cdev_map_out == NULL)
		rte_panic("Failed to create cdev_map hash table, errno = %d\n",
				rte_errno);

	printf("lcore/cryptodev/qp mappings:\n");

	uint32_t max_sess_sz = 0, sess_sz;
	for (cdev_id = 0; cdev_id < rte_cryptodev_count(); cdev_id++) {
		sess_sz = rte_cryptodev_get_private_session_size(cdev_id);
		if (sess_sz > max_sess_sz)
			max_sess_sz = sess_sz;
	}

	idx = 0;
	/* Start from last cdev id to give HW priority */
	for (cdev_id = rte_cryptodev_count() - 1; cdev_id >= 0; cdev_id--) {
		struct rte_cryptodev_info cdev_info;

		rte_cryptodev_info_get(cdev_id, &cdev_info);

		if (nb_lcore_params > cdev_info.max_nb_queue_pairs)
			max_nb_qps = cdev_info.max_nb_queue_pairs;
		else
			max_nb_qps = nb_lcore_params;

		qp = 0;
		i = 0;
		while (qp < max_nb_qps && i < nb_lcore_params) {
			if (add_cdev_mapping(&cdev_info, cdev_id, qp,
						&lcore_params[idx]))
				qp++;
			idx++;
			idx = idx % nb_lcore_params;
			i++;
		}

		if (qp == 0)
			continue;

		dev_conf.socket_id = rte_cryptodev_socket_id(cdev_id);
		dev_conf.nb_queue_pairs = qp;

		if (!socket_ctx[dev_conf.socket_id].session_pool) {
			char mp_name[RTE_MEMPOOL_NAMESIZE];
			struct rte_mempool *sess_mp;

			snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
					"sess_mp_%u", dev_conf.socket_id);
			sess_mp = rte_mempool_create(mp_name,
					CDEV_MP_NB_OBJS,
					max_sess_sz,
					CDEV_MP_CACHE_SZ,
					0, NULL, NULL, NULL,
					NULL, dev_conf.socket_id,
					0);
			if (sess_mp == NULL)
				rte_exit(EXIT_FAILURE,
					"Cannot create session pool on socket %d\n",
					dev_conf.socket_id);
			else
				printf("Allocated session pool on socket %d\n",
					dev_conf.socket_id);
			socket_ctx[dev_conf.socket_id].session_pool = sess_mp;
		}

		if (rte_cryptodev_configure(cdev_id, &dev_conf))
			rte_panic("Failed to initialize cryptodev %u\n",
					cdev_id);

		qp_conf.nb_descriptors = CDEV_QUEUE_DESC;
		for (qp = 0; qp < dev_conf.nb_queue_pairs; qp++)
			if (rte_cryptodev_queue_pair_setup(cdev_id, qp,
					&qp_conf, dev_conf.socket_id,
					socket_ctx[dev_conf.socket_id].session_pool))
				rte_panic("Failed to setup queue %u for "
						"cdev_id %u\n",	0, cdev_id);

		if (rte_cryptodev_start(cdev_id))
			rte_panic("Failed to start cryptodev %u\n",
					cdev_id);
	}

	printf("\n");

	return 0;
}

static void
port_init(uint8_t portid)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	uint16_t nb_tx_queue, nb_rx_queue;
	uint16_t tx_queueid, rx_queueid, queue, lcore_id;
	int32_t ret, socket_id;
	struct lcore_conf *qconf;
	struct ether_addr ethaddr;

	rte_eth_dev_info_get(portid, &dev_info);

	printf("Configuring device port %u:\n", portid);

	rte_eth_macaddr_get(portid, &ethaddr);
	ethaddr_tbl[portid].src = ETHADDR_TO_UINT64(ethaddr);
	print_ethaddr("Address: ", &ethaddr);
	printf("\n");

	nb_rx_queue = get_port_nb_rx_queues(portid);
	nb_tx_queue = nb_lcores;

	if (nb_rx_queue > dev_info.max_rx_queues)
		rte_exit(EXIT_FAILURE, "Error: queue %u not available "
				"(max rx queue is %u)\n",
				nb_rx_queue, dev_info.max_rx_queues);

	if (nb_tx_queue > dev_info.max_tx_queues)
		rte_exit(EXIT_FAILURE, "Error: queue %u not available "
				"(max tx queue is %u)\n",
				nb_tx_queue, dev_info.max_tx_queues);

	printf("Creating queues: nb_rx_queue=%d nb_tx_queue=%u...\n",
			nb_rx_queue, nb_tx_queue);

	ret = rte_eth_dev_configure(portid, nb_rx_queue, nb_tx_queue,
			&port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure device: "
				"err=%d, port=%d\n", ret, portid);

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd, &nb_txd);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot adjust number of descriptors: "
				"err=%d, port=%d\n", ret, portid);

	/* init one TX queue per lcore */
	tx_queueid = 0;
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socket_id = (uint8_t)rte_lcore_to_socket_id(lcore_id);
		else
			socket_id = 0;

		/* init TX queue */
		printf("Setup txq=%u,%d,%d\n", lcore_id, tx_queueid, socket_id);

		txconf = &dev_info.default_txconf;
		txconf->txq_flags = 0;

		ret = rte_eth_tx_queue_setup(portid, tx_queueid, nb_txd,
				socket_id, txconf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
					"err=%d, port=%d\n", ret, portid);

		qconf = &lcore_conf[lcore_id];
		qconf->tx_queue_id[portid] = tx_queueid;
		tx_queueid++;

		/* init RX queues */
		for (queue = 0; queue < qconf->nb_rx_queue; ++queue) {
			if (portid != qconf->rx_queue_list[queue].port_id)
				continue;

			rx_queueid = qconf->rx_queue_list[queue].queue_id;

			printf("Setup rxq=%d,%d,%d\n", portid, rx_queueid,
					socket_id);

			ret = rte_eth_rx_queue_setup(portid, rx_queueid,
					nb_rxd,	socket_id, NULL,
					socket_ctx[socket_id].mbuf_pool);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_rx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);
		}
	}
	printf("\n");
}

static void
pool_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t nb_mbuf)
{
	char s[64];

	snprintf(s, sizeof(s), "mbuf_pool_%d", socket_id);
	ctx->mbuf_pool = rte_pktmbuf_pool_create(s, nb_mbuf,
			MEMPOOL_CACHE_SIZE, ipsec_metadata_size(),
			RTE_MBUF_DEFAULT_BUF_SIZE,
			socket_id);
	if (ctx->mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool on socket %d\n",
				socket_id);
	else
		printf("Allocated mbuf pool on socket %d\n", socket_id);
}

int32_t
main(int32_t argc, char **argv)
{
	int32_t ret;
	uint32_t lcore_id, nb_ports;
	uint8_t portid, socket_id;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid parameters\n");

	if ((unprotected_port_mask & enabled_port_mask) !=
			unprotected_port_mask)
		rte_exit(EXIT_FAILURE, "Invalid unprotected portmask 0x%x\n",
				unprotected_port_mask);

	nb_ports = rte_eth_dev_count();

	if (check_params() < 0)
		rte_exit(EXIT_FAILURE, "check_params failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");

	nb_lcores = rte_lcore_count();

	/* Replicate each context per socket */
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socket_id = (uint8_t)rte_lcore_to_socket_id(lcore_id);
		else
			socket_id = 0;

		if (socket_ctx[socket_id].mbuf_pool)
			continue;

		sa_init(&socket_ctx[socket_id], socket_id);

		sp4_init(&socket_ctx[socket_id], socket_id);

		sp6_init(&socket_ctx[socket_id], socket_id);

		rt_init(&socket_ctx[socket_id], socket_id);

		pool_init(&socket_ctx[socket_id], socket_id, NB_MBUF);
	}

	for (portid = 0; portid < nb_ports; portid++) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		port_init(portid);
	}

	cryptodevs_init();

	/* start ports */
	for (portid = 0; portid < nb_ports; portid++) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: "
					"err=%d, port=%d\n", ret, portid);
		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(portid);
	}

	check_all_ports_link_status((uint8_t)nb_ports, enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
