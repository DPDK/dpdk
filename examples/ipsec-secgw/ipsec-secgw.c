/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
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
#include <rte_security.h>

#include "ipsec.h"
#include "parser.h"

#define RTE_LOGTYPE_IPSEC RTE_LOGTYPE_USER1

#define MAX_JUMBO_PKT_LEN  9600

#define MEMPOOL_CACHE_SIZE 256

#define NB_MBUF	(32000)

#define CDEV_QUEUE_DESC 2048
#define CDEV_MAP_ENTRIES 16384
#define CDEV_MP_NB_OBJS 1024
#define CDEV_MP_CACHE_SZ 64
#define MAX_QUEUE_PAIRS 1

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
#define IPSEC_SECGW_RX_DESC_DEFAULT 1024
#define IPSEC_SECGW_TX_DESC_DEFAULT 1024
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
		(addr)->addr_bytes[0], (addr)->addr_bytes[1], \
		(addr)->addr_bytes[2], (addr)->addr_bytes[3], \
		(addr)->addr_bytes[4], (addr)->addr_bytes[5], \
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

#define CMD_LINE_OPT_CONFIG		"config"
#define CMD_LINE_OPT_SINGLE_SA		"single-sa"
#define CMD_LINE_OPT_CRYPTODEV_MASK	"cryptodev_mask"
#define CMD_LINE_OPT_RX_OFFLOAD		"rxoffload"
#define CMD_LINE_OPT_TX_OFFLOAD		"txoffload"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options
	 */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_CONFIG_NUM,
	CMD_LINE_OPT_SINGLE_SA_NUM,
	CMD_LINE_OPT_CRYPTODEV_MASK_NUM,
	CMD_LINE_OPT_RX_OFFLOAD_NUM,
	CMD_LINE_OPT_TX_OFFLOAD_NUM,
};

static const struct option lgopts[] = {
	{CMD_LINE_OPT_CONFIG, 1, 0, CMD_LINE_OPT_CONFIG_NUM},
	{CMD_LINE_OPT_SINGLE_SA, 1, 0, CMD_LINE_OPT_SINGLE_SA_NUM},
	{CMD_LINE_OPT_CRYPTODEV_MASK, 1, 0, CMD_LINE_OPT_CRYPTODEV_MASK_NUM},
	{CMD_LINE_OPT_RX_OFFLOAD, 1, 0, CMD_LINE_OPT_RX_OFFLOAD_NUM},
	{CMD_LINE_OPT_TX_OFFLOAD, 1, 0, CMD_LINE_OPT_TX_OFFLOAD_NUM},
	{NULL, 0, 0, 0}
};

/* mask of enabled ports */
static uint32_t enabled_port_mask;
static uint64_t enabled_cryptodev_mask = UINT64_MAX;
static uint32_t unprotected_port_mask;
static int32_t promiscuous_on = 1;
static int32_t numa_on = 1; /**< NUMA is enabled by default. */
static uint32_t nb_lcores;
static uint32_t single_sa;
static uint32_t single_sa_idx;
static uint32_t frame_size;

/*
 * RX/TX HW offload capabilities to enable/use on ethernet ports.
 * By default all capabilities are enabled.
 */
static uint64_t dev_rx_offload = UINT64_MAX;
static uint64_t dev_tx_offload = UINT64_MAX;

/* application wide librte_ipsec/SA parameters */
struct app_sa_prm app_sa_prm = {.enable = 0};

struct lcore_rx_queue {
	uint16_t port_id;
	uint8_t queue_id;
} __rte_cache_aligned;

struct lcore_params {
	uint16_t port_id;
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
		.offloads = DEV_RX_OFFLOAD_CHECKSUM,
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
		pkt->l2_len = 0;
		pkt->l3_len = sizeof(struct ip);
	} else if (eth->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6)) {
		nlp = (uint8_t *)rte_pktmbuf_adj(pkt, ETHER_HDR_LEN);
		nlp = RTE_PTR_ADD(nlp, offsetof(struct ip6_hdr, ip6_nxt));
		if (*nlp == IPPROTO_ESP)
			t->ipsec.pkts[(t->ipsec.num)++] = pkt;
		else {
			t->ip6.data[t->ip6.num] = nlp;
			t->ip6.pkts[(t->ip6.num)++] = pkt;
		}
		pkt->l2_len = 0;
		pkt->l3_len = sizeof(struct ip6_hdr);
	} else {
		/* Unknown/Unsupported type, drop the packet */
		RTE_LOG(ERR, IPSEC, "Unsupported packet type 0x%x\n",
			rte_be_to_cpu_16(eth->ether_type));
		rte_pktmbuf_free(pkt);
	}

	/* Check if the packet has been processed inline. For inline protocol
	 * processed packets, the metadata in the mbuf can be used to identify
	 * the security processing done on the packet. The metadata will be
	 * used to retrieve the application registered userdata associated
	 * with the security session.
	 */

	if (pkt->ol_flags & PKT_RX_SEC_OFFLOAD) {
		struct ipsec_sa *sa;
		struct ipsec_mbuf_metadata *priv;
		struct rte_security_ctx *ctx = (struct rte_security_ctx *)
						rte_eth_dev_get_sec_ctx(
						pkt->port);

		/* Retrieve the userdata registered. Here, the userdata
		 * registered is the SA pointer.
		 */

		sa = (struct ipsec_sa *)
				rte_security_get_userdata(ctx, pkt->udata64);

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
prepare_tx_pkt(struct rte_mbuf *pkt, uint16_t port,
		const struct lcore_conf *qconf)
{
	struct ip *ip;
	struct ether_hdr *ethhdr;

	ip = rte_pktmbuf_mtod(pkt, struct ip *);

	ethhdr = (struct ether_hdr *)rte_pktmbuf_prepend(pkt, ETHER_HDR_LEN);

	if (ip->ip_v == IPVERSION) {
		pkt->ol_flags |= qconf->outbound.ipv4_offloads;
		pkt->l3_len = sizeof(struct ip);
		pkt->l2_len = ETHER_HDR_LEN;

		ip->ip_sum = 0;

		/* calculate IPv4 cksum in SW */
		if ((pkt->ol_flags & PKT_TX_IP_CKSUM) == 0)
			ip->ip_sum = rte_ipv4_cksum((struct ipv4_hdr *)ip);

		ethhdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	} else {
		pkt->ol_flags |= qconf->outbound.ipv6_offloads;
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
prepare_tx_burst(struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t port,
		const struct lcore_conf *qconf)
{
	int32_t i;
	const int32_t prefetch_offset = 2;

	for (i = 0; i < (nb_pkts - prefetch_offset); i++) {
		rte_mbuf_prefetch_part2(pkts[i + prefetch_offset]);
		prepare_tx_pkt(pkts[i], port, qconf);
	}
	/* Process left packets */
	for (; i < nb_pkts; i++)
		prepare_tx_pkt(pkts[i], port, qconf);
}

/* Send burst of packets on an output interface */
static inline int32_t
send_burst(struct lcore_conf *qconf, uint16_t n, uint16_t port)
{
	struct rte_mbuf **m_table;
	int32_t ret;
	uint16_t queueid;

	queueid = qconf->tx_queue_id[port];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

	prepare_tx_burst(m_table, n, port, qconf);

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
send_single_packet(struct rte_mbuf *m, uint16_t port)
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
		if (res == BYPASS) {
			ip->pkts[j++] = m;
			continue;
		}
		if (res == DISCARD) {
			rte_pktmbuf_free(m);
			continue;
		}

		/* Only check SPI match for processed IPSec packets */
		if (i < lim && ((m->ol_flags & PKT_RX_SEC_OFFLOAD) == 0)) {
			rte_pktmbuf_free(m);
			continue;
		}

		sa_idx = SPI2IDX(res);
		if (!inbound_sa_check(sa, m, sa_idx)) {
			rte_pktmbuf_free(m);
			continue;
		}
		ip->pkts[j++] = m;
	}
	ip->num = j;
}

static void
split46_traffic(struct ipsec_traffic *trf, struct rte_mbuf *mb[], uint32_t num)
{
	uint32_t i, n4, n6;
	struct ip *ip;
	struct rte_mbuf *m;

	n4 = trf->ip4.num;
	n6 = trf->ip6.num;

	for (i = 0; i < num; i++) {

		m = mb[i];
		ip = rte_pktmbuf_mtod(m, struct ip *);

		if (ip->ip_v == IPVERSION) {
			trf->ip4.pkts[n4] = m;
			trf->ip4.data[n4] = rte_pktmbuf_mtod_offset(m,
					uint8_t *, offsetof(struct ip, ip_p));
			n4++;
		} else if (ip->ip_v == IP6_VERSION) {
			trf->ip6.pkts[n6] = m;
			trf->ip6.data[n6] = rte_pktmbuf_mtod_offset(m,
					uint8_t *,
					offsetof(struct ip6_hdr, ip6_nxt));
			n6++;
		} else
			rte_pktmbuf_free(m);
	}

	trf->ip4.num = n4;
	trf->ip6.num = n6;
}


static inline void
process_pkts_inbound(struct ipsec_ctx *ipsec_ctx,
		struct ipsec_traffic *traffic)
{
	uint16_t nb_pkts_in, n_ip4, n_ip6;

	n_ip4 = traffic->ip4.num;
	n_ip6 = traffic->ip6.num;

	if (app_sa_prm.enable == 0) {
		nb_pkts_in = ipsec_inbound(ipsec_ctx, traffic->ipsec.pkts,
				traffic->ipsec.num, MAX_PKT_BURST);
		split46_traffic(traffic, traffic->ipsec.pkts, nb_pkts_in);
	} else {
		inbound_sa_lookup(ipsec_ctx->sa_ctx, traffic->ipsec.pkts,
			traffic->ipsec.saptr, traffic->ipsec.num);
		ipsec_process(ipsec_ctx, traffic);
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
		sa_idx = SPI2IDX(ip->res[i]);
		if (ip->res[i] == DISCARD)
			rte_pktmbuf_free(m);
		else if (ip->res[i] == BYPASS)
			ip->pkts[j++] = m;
		else {
			ipsec->res[ipsec->num] = sa_idx;
			ipsec->pkts[ipsec->num++] = m;
		}
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

	if (app_sa_prm.enable == 0) {

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
	} else {
		outbound_sa_lookup(ipsec_ctx->sa_ctx, traffic->ipsec.res,
			traffic->ipsec.saptr, traffic->ipsec.num);
		ipsec_process(ipsec_ctx, traffic);
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

	if (app_sa_prm.enable == 0) {

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
	} else {
		inbound_sa_lookup(ipsec_ctx->sa_ctx, traffic->ipsec.pkts,
			traffic->ipsec.saptr, traffic->ipsec.num);
		ipsec_process(ipsec_ctx, traffic);
	}
}

static inline void
process_pkts_outbound_nosp(struct ipsec_ctx *ipsec_ctx,
		struct ipsec_traffic *traffic)
{
	struct rte_mbuf *m;
	uint32_t nb_pkts_out, i, n;
	struct ip *ip;

	/* Drop any IPsec traffic from protected ports */
	for (i = 0; i < traffic->ipsec.num; i++)
		rte_pktmbuf_free(traffic->ipsec.pkts[i]);

	n = 0;

	for (i = 0; i < traffic->ip4.num; i++) {
		traffic->ipsec.pkts[n] = traffic->ip4.pkts[i];
		traffic->ipsec.res[n++] = single_sa_idx;
	}

	for (i = 0; i < traffic->ip6.num; i++) {
		traffic->ipsec.pkts[n] = traffic->ip6.pkts[i];
		traffic->ipsec.res[n++] = single_sa_idx;
	}

	traffic->ip4.num = 0;
	traffic->ip6.num = 0;
	traffic->ipsec.num = n;

	if (app_sa_prm.enable == 0) {

		nb_pkts_out = ipsec_outbound(ipsec_ctx, traffic->ipsec.pkts,
				traffic->ipsec.res, traffic->ipsec.num,
				MAX_PKT_BURST);

		/* They all sue the same SA (ip4 or ip6 tunnel) */
		m = traffic->ipsec.pkts[0];
		ip = rte_pktmbuf_mtod(m, struct ip *);
		if (ip->ip_v == IPVERSION) {
			traffic->ip4.num = nb_pkts_out;
			for (i = 0; i < nb_pkts_out; i++)
				traffic->ip4.pkts[i] = traffic->ipsec.pkts[i];
		} else {
			traffic->ip6.num = nb_pkts_out;
			for (i = 0; i < nb_pkts_out; i++)
				traffic->ip6.pkts[i] = traffic->ipsec.pkts[i];
		}
	} else {
		outbound_sa_lookup(ipsec_ctx->sa_ctx, traffic->ipsec.res,
			traffic->ipsec.saptr, traffic->ipsec.num);
		ipsec_process(ipsec_ctx, traffic);
	}
}

static inline int32_t
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
		return -1;

	/* else */
	return 0;
}

static inline void
route4_pkts(struct rt_ctx *rt_ctx, struct rte_mbuf *pkts[], uint8_t nb_pkts)
{
	uint32_t hop[MAX_PKT_BURST * 2];
	uint32_t dst_ip[MAX_PKT_BURST * 2];
	int32_t pkt_hop = 0;
	uint16_t i, offset;
	uint16_t lpm_pkts = 0;

	if (nb_pkts == 0)
		return;

	/* Need to do an LPM lookup for non-inline packets. Inline packets will
	 * have port ID in the SA
	 */

	for (i = 0; i < nb_pkts; i++) {
		if (!(pkts[i]->ol_flags & PKT_TX_SEC_OFFLOAD)) {
			/* Security offload not enabled. So an LPM lookup is
			 * required to get the hop
			 */
			offset = offsetof(struct ip, ip_dst);
			dst_ip[lpm_pkts] = *rte_pktmbuf_mtod_offset(pkts[i],
					uint32_t *, offset);
			dst_ip[lpm_pkts] = rte_be_to_cpu_32(dst_ip[lpm_pkts]);
			lpm_pkts++;
		}
	}

	rte_lpm_lookup_bulk((struct rte_lpm *)rt_ctx, dst_ip, hop, lpm_pkts);

	lpm_pkts = 0;

	for (i = 0; i < nb_pkts; i++) {
		if (pkts[i]->ol_flags & PKT_TX_SEC_OFFLOAD) {
			/* Read hop from the SA */
			pkt_hop = get_hop_for_offload_pkt(pkts[i], 0);
		} else {
			/* Need to use hop returned by lookup */
			pkt_hop = hop[lpm_pkts++];
		}

		if ((pkt_hop & RTE_LPM_LOOKUP_SUCCESS) == 0) {
			rte_pktmbuf_free(pkts[i]);
			continue;
		}
		send_single_packet(pkts[i], pkt_hop & 0xff);
	}
}

static inline void
route6_pkts(struct rt_ctx *rt_ctx, struct rte_mbuf *pkts[], uint8_t nb_pkts)
{
	int32_t hop[MAX_PKT_BURST * 2];
	uint8_t dst_ip[MAX_PKT_BURST * 2][16];
	uint8_t *ip6_dst;
	int32_t pkt_hop = 0;
	uint16_t i, offset;
	uint16_t lpm_pkts = 0;

	if (nb_pkts == 0)
		return;

	/* Need to do an LPM lookup for non-inline packets. Inline packets will
	 * have port ID in the SA
	 */

	for (i = 0; i < nb_pkts; i++) {
		if (!(pkts[i]->ol_flags & PKT_TX_SEC_OFFLOAD)) {
			/* Security offload not enabled. So an LPM lookup is
			 * required to get the hop
			 */
			offset = offsetof(struct ip6_hdr, ip6_dst);
			ip6_dst = rte_pktmbuf_mtod_offset(pkts[i], uint8_t *,
					offset);
			memcpy(&dst_ip[lpm_pkts][0], ip6_dst, 16);
			lpm_pkts++;
		}
	}

	rte_lpm6_lookup_bulk_func((struct rte_lpm6 *)rt_ctx, dst_ip, hop,
			lpm_pkts);

	lpm_pkts = 0;

	for (i = 0; i < nb_pkts; i++) {
		if (pkts[i]->ol_flags & PKT_TX_SEC_OFFLOAD) {
			/* Read hop from the SA */
			pkt_hop = get_hop_for_offload_pkt(pkts[i], 1);
		} else {
			/* Need to use hop returned by lookup */
			pkt_hop = hop[lpm_pkts++];
		}

		if (pkt_hop == -1) {
			rte_pktmbuf_free(pkts[i]);
			continue;
		}
		send_single_packet(pkts[i], pkt_hop & 0xff);
	}
}

static inline void
process_pkts(struct lcore_conf *qconf, struct rte_mbuf **pkts,
		uint8_t nb_pkts, uint16_t portid)
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

static inline void
drain_crypto_buffers(struct lcore_conf *qconf)
{
	uint32_t i;
	struct ipsec_ctx *ctx;

	/* drain inbound buffers*/
	ctx = &qconf->inbound;
	for (i = 0; i != ctx->nb_qps; i++) {
		if (ctx->tbl[i].len != 0)
			enqueue_cop_burst(ctx->tbl  + i);
	}

	/* drain outbound buffers*/
	ctx = &qconf->outbound;
	for (i = 0; i != ctx->nb_qps; i++) {
		if (ctx->tbl[i].len != 0)
			enqueue_cop_burst(ctx->tbl  + i);
	}
}

static void
drain_inbound_crypto_queues(const struct lcore_conf *qconf,
		struct ipsec_ctx *ctx)
{
	uint32_t n;
	struct ipsec_traffic trf;

	if (app_sa_prm.enable == 0) {

		/* dequeue packets from crypto-queue */
		n = ipsec_inbound_cqp_dequeue(ctx, trf.ipsec.pkts,
			RTE_DIM(trf.ipsec.pkts));

		trf.ip4.num = 0;
		trf.ip6.num = 0;

		/* split traffic by ipv4-ipv6 */
		split46_traffic(&trf, trf.ipsec.pkts, n);
	} else
		ipsec_cqp_process(ctx, &trf);

	/* process ipv4 packets */
	if (trf.ip4.num != 0) {
		inbound_sp_sa(ctx->sp4_ctx, ctx->sa_ctx, &trf.ip4, 0);
		route4_pkts(qconf->rt4_ctx, trf.ip4.pkts, trf.ip4.num);
	}

	/* process ipv6 packets */
	if (trf.ip6.num != 0) {
		inbound_sp_sa(ctx->sp6_ctx, ctx->sa_ctx, &trf.ip6, 0);
		route6_pkts(qconf->rt6_ctx, trf.ip6.pkts, trf.ip6.num);
	}
}

static void
drain_outbound_crypto_queues(const struct lcore_conf *qconf,
		struct ipsec_ctx *ctx)
{
	uint32_t n;
	struct ipsec_traffic trf;

	if (app_sa_prm.enable == 0) {

		/* dequeue packets from crypto-queue */
		n = ipsec_outbound_cqp_dequeue(ctx, trf.ipsec.pkts,
			RTE_DIM(trf.ipsec.pkts));

		trf.ip4.num = 0;
		trf.ip6.num = 0;

		/* split traffic by ipv4-ipv6 */
		split46_traffic(&trf, trf.ipsec.pkts, n);
	} else
		ipsec_cqp_process(ctx, &trf);

	/* process ipv4 packets */
	if (trf.ip4.num != 0)
		route4_pkts(qconf->rt4_ctx, trf.ip4.pkts, trf.ip4.num);

	/* process ipv6 packets */
	if (trf.ip6.num != 0)
		route6_pkts(qconf->rt6_ctx, trf.ip6.pkts, trf.ip6.num);
}

/* main processing loop */
static int32_t
main_loop(__attribute__((unused)) void *dummy)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	uint32_t lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int32_t i, nb_rx;
	uint16_t portid;
	uint8_t queueid;
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
	qconf->inbound.session_priv_pool =
			socket_ctx[socket_id].session_priv_pool;
	qconf->outbound.sp4_ctx = socket_ctx[socket_id].sp_ip4_out;
	qconf->outbound.sp6_ctx = socket_ctx[socket_id].sp_ip6_out;
	qconf->outbound.sa_ctx = socket_ctx[socket_id].sa_out;
	qconf->outbound.cdev_map = cdev_map_out;
	qconf->outbound.session_pool = socket_ctx[socket_id].session_pool;
	qconf->outbound.session_priv_pool =
			socket_ctx[socket_id].session_priv_pool;

	if (qconf->nb_rx_queue == 0) {
		RTE_LOG(DEBUG, IPSEC, "lcore %u has nothing to do\n",
			lcore_id);
		return 0;
	}

	RTE_LOG(INFO, IPSEC, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->nb_rx_queue; i++) {
		portid = rxql[i].port_id;
		queueid = rxql[i].queue_id;
		RTE_LOG(INFO, IPSEC,
			" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	while (1) {
		cur_tsc = rte_rdtsc();

		/* TX queue buffer drain */
		diff_tsc = cur_tsc - prev_tsc;

		if (unlikely(diff_tsc > drain_tsc)) {
			drain_tx_buffers(qconf);
			drain_crypto_buffers(qconf);
			prev_tsc = cur_tsc;
		}

		for (i = 0; i < qconf->nb_rx_queue; ++i) {

			/* Read packets from RX queues */
			portid = rxql[i].port_id;
			queueid = rxql[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid,
					pkts, MAX_PKT_BURST);

			if (nb_rx > 0)
				process_pkts(qconf, pkts, nb_rx, portid);

			/* dequeue and process completed crypto-ops */
			if (UNPROTECTED_PORT(portid))
				drain_inbound_crypto_queues(qconf,
					&qconf->inbound);
			else
				drain_outbound_crypto_queues(qconf,
					&qconf->outbound);
		}
	}
}

static int32_t
check_params(void)
{
	uint8_t lcore;
	uint16_t portid;
	uint16_t i;
	int32_t socket_id;

	if (lcore_params == NULL) {
		printf("Error: No port/queue/core mappings\n");
		return -1;
	}

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
		if (!rte_eth_dev_is_valid_port(portid)) {
			printf("port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static uint8_t
get_port_nb_rx_queues(const uint16_t port)
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
	fprintf(stderr, "%s [EAL options] --"
		" -p PORTMASK"
		" [-P]"
		" [-u PORTMASK]"
		" [-j FRAMESIZE]"
		" [-l]"
		" [-w REPLAY_WINDOW_SIZE]"
		" [-e]"
		" [-a]"
		" -f CONFIG_FILE"
		" --config (port,queue,lcore)[,(port,queue,lcore)]"
		" [--single-sa SAIDX]"
		" [--cryptodev_mask MASK]"
		" [--" CMD_LINE_OPT_RX_OFFLOAD " RX_OFFLOAD_MASK]"
		" [--" CMD_LINE_OPT_TX_OFFLOAD " TX_OFFLOAD_MASK]"
		"\n\n"
		"  -p PORTMASK: Hexadecimal bitmask of ports to configure\n"
		"  -P : Enable promiscuous mode\n"
		"  -u PORTMASK: Hexadecimal bitmask of unprotected ports\n"
		"  -j FRAMESIZE: Enable jumbo frame with 'FRAMESIZE' as maximum\n"
		"                packet size\n"
		"  -l enables code-path that uses librte_ipsec\n"
		"  -w REPLAY_WINDOW_SIZE specifies IPsec SQN replay window\n"
		"     size for each SA\n"
		"  -e enables ESN\n"
		"  -a enables SA SQN atomic behaviour\n"
		"  -f CONFIG_FILE: Configuration file\n"
		"  --config (port,queue,lcore): Rx queue configuration\n"
		"  --single-sa SAIDX: Use single SA index for outbound traffic,\n"
		"                     bypassing the SP\n"
		"  --cryptodev_mask MASK: Hexadecimal bitmask of the crypto\n"
		"                         devices to configure\n"
		"  --" CMD_LINE_OPT_RX_OFFLOAD
		": bitmask of the RX HW offload capabilities to enable/use\n"
		"                         (DEV_RX_OFFLOAD_*)\n"
		"  --" CMD_LINE_OPT_TX_OFFLOAD
		": bitmask of the TX HW offload capabilities to enable/use\n"
		"                         (DEV_TX_OFFLOAD_*)\n"
		"\n",
		prgname);
}

static int
parse_mask(const char *str, uint64_t *val)
{
	char *end;
	unsigned long t;

	errno = 0;
	t = strtoul(str, &end, 0);
	if (errno != 0 || end[0] != 0)
		return -EINVAL;

	*val = t;
	return 0;
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

static void
print_app_sa_prm(const struct app_sa_prm *prm)
{
	printf("librte_ipsec usage: %s\n",
		(prm->enable == 0) ? "disabled" : "enabled");

	if (prm->enable == 0)
		return;

	printf("replay window size: %u\n", prm->window_size);
	printf("ESN: %s\n", (prm->enable_esn == 0) ? "disabled" : "enabled");
	printf("SA flags: %#" PRIx64 "\n", prm->flags);
}

static int32_t
parse_args(int32_t argc, char **argv)
{
	int32_t opt, ret;
	char **argvopt;
	int32_t option_index;
	char *prgname = argv[0];
	int32_t f_present = 0;

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "aelp:Pu:f:j:w:",
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
		case 'j':
			{
				int32_t size = parse_decimal(optarg);
				if (size <= 1518) {
					printf("Invalid jumbo frame size\n");
					if (size < 0) {
						print_usage(prgname);
						return -1;
					}
					printf("Using default value 9000\n");
					frame_size = 9000;
				} else {
					frame_size = size;
				}
			}
			printf("Enabled jumbo frames size %u\n", frame_size);
			break;
		case 'l':
			app_sa_prm.enable = 1;
			break;
		case 'w':
			app_sa_prm.enable = 1;
			app_sa_prm.window_size = parse_decimal(optarg);
			break;
		case 'e':
			app_sa_prm.enable = 1;
			app_sa_prm.enable_esn = 1;
			break;
		case 'a':
			app_sa_prm.enable = 1;
			app_sa_prm.flags |= RTE_IPSEC_SAFLAG_SQN_ATOM;
			break;
		case CMD_LINE_OPT_CONFIG_NUM:
			ret = parse_config(optarg);
			if (ret) {
				printf("Invalid config\n");
				print_usage(prgname);
				return -1;
			}
			break;
		case CMD_LINE_OPT_SINGLE_SA_NUM:
			ret = parse_decimal(optarg);
			if (ret == -1) {
				printf("Invalid argument[sa_idx]\n");
				print_usage(prgname);
				return -1;
			}

			/* else */
			single_sa = 1;
			single_sa_idx = ret;
			printf("Configured with single SA index %u\n",
					single_sa_idx);
			break;
		case CMD_LINE_OPT_CRYPTODEV_MASK_NUM:
			ret = parse_portmask(optarg);
			if (ret == -1) {
				printf("Invalid argument[portmask]\n");
				print_usage(prgname);
				return -1;
			}

			/* else */
			enabled_cryptodev_mask = ret;
			break;
		case CMD_LINE_OPT_RX_OFFLOAD_NUM:
			ret = parse_mask(optarg, &dev_rx_offload);
			if (ret != 0) {
				printf("Invalid argument for \'%s\': %s\n",
					CMD_LINE_OPT_RX_OFFLOAD, optarg);
				print_usage(prgname);
				return -1;
			}
			break;
		case CMD_LINE_OPT_TX_OFFLOAD_NUM:
			ret = parse_mask(optarg, &dev_tx_offload);
			if (ret != 0) {
				printf("Invalid argument for \'%s\': %s\n",
					CMD_LINE_OPT_TX_OFFLOAD, optarg);
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

	print_app_sa_prm(&app_sa_prm);

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

/*
 * Update destination ethaddr for the port.
 */
int
add_dst_ethaddr(uint16_t port, const struct ether_addr *addr)
{
	if (port >= RTE_DIM(ethaddr_tbl))
		return -EINVAL;

	ethaddr_tbl[port].dst = ETHADDR_TO_UINT64(addr);
	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf(
					"Port%d Link Up - speed %u Mbps -%s\n",
						portid, link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n", portid);
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
		const struct rte_cryptodev_capabilities *auth,
		const struct rte_cryptodev_capabilities *aead)
{
	int32_t ret = 0;
	unsigned long i;
	struct cdev_key key = { 0 };

	key.lcore_id = params->lcore_id;
	if (cipher)
		key.cipher_algo = cipher->sym.cipher.algo;
	if (auth)
		key.auth_algo = auth->sym.auth.algo;
	if (aead)
		key.aead_algo = aead->sym.aead.algo;

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

		if (i->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			ret |= add_mapping(map, str, cdev_id, qp, params,
					ipsec_ctx, NULL, NULL, i);
			continue;
		}

		if (i->sym.xform_type != RTE_CRYPTO_SYM_XFORM_CIPHER)
			continue;

		for (j = dev_info->capabilities;
				j->op != RTE_CRYPTO_OP_TYPE_UNDEFINED; j++) {
			if (j->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
				continue;

			if (j->sym.xform_type != RTE_CRYPTO_SYM_XFORM_AUTH)
				continue;

			ret |= add_mapping(map, str, cdev_id, qp, params,
						ipsec_ctx, i, j, NULL);
		}
	}

	return ret;
}

/* Check if the device is enabled by cryptodev_mask */
static int
check_cryptodev_mask(uint8_t cdev_id)
{
	if (enabled_cryptodev_mask & (1 << cdev_id))
		return 0;

	return -1;
}

static int32_t
cryptodevs_init(void)
{
	struct rte_cryptodev_config dev_conf;
	struct rte_cryptodev_qp_conf qp_conf;
	uint16_t idx, max_nb_qps, qp, i;
	int16_t cdev_id, port_id;
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
		void *sec_ctx;

		/* Get crypto priv session size */
		sess_sz = rte_cryptodev_sym_get_private_session_size(cdev_id);
		if (sess_sz > max_sess_sz)
			max_sess_sz = sess_sz;

		/*
		 * If crypto device is security capable, need to check the
		 * size of security session as well.
		 */

		/* Get security context of the crypto device */
		sec_ctx = rte_cryptodev_get_sec_ctx(cdev_id);
		if (sec_ctx == NULL)
			continue;

		/* Get size of security session */
		sess_sz = rte_security_session_get_size(sec_ctx);
		if (sess_sz > max_sess_sz)
			max_sess_sz = sess_sz;
	}
	RTE_ETH_FOREACH_DEV(port_id) {
		void *sec_ctx;

		if ((enabled_port_mask & (1 << port_id)) == 0)
			continue;

		sec_ctx = rte_eth_dev_get_sec_ctx(port_id);
		if (sec_ctx == NULL)
			continue;

		sess_sz = rte_security_session_get_size(sec_ctx);
		if (sess_sz > max_sess_sz)
			max_sess_sz = sess_sz;
	}

	idx = 0;
	for (cdev_id = 0; cdev_id < rte_cryptodev_count(); cdev_id++) {
		struct rte_cryptodev_info cdev_info;

		if (check_cryptodev_mask((uint8_t)cdev_id))
			continue;

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

		uint32_t dev_max_sess = cdev_info.sym.max_nb_sessions;
		if (dev_max_sess != 0 && dev_max_sess < CDEV_MP_NB_OBJS)
			rte_exit(EXIT_FAILURE,
				"Device does not support at least %u "
				"sessions", CDEV_MP_NB_OBJS);

		if (!socket_ctx[dev_conf.socket_id].session_pool) {
			char mp_name[RTE_MEMPOOL_NAMESIZE];
			struct rte_mempool *sess_mp;

			snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
					"sess_mp_%u", dev_conf.socket_id);
			sess_mp = rte_cryptodev_sym_session_pool_create(
					mp_name, CDEV_MP_NB_OBJS,
					0, CDEV_MP_CACHE_SZ, 0,
					dev_conf.socket_id);
			socket_ctx[dev_conf.socket_id].session_pool = sess_mp;
		}

		if (!socket_ctx[dev_conf.socket_id].session_priv_pool) {
			char mp_name[RTE_MEMPOOL_NAMESIZE];
			struct rte_mempool *sess_mp;

			snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
					"sess_mp_priv_%u", dev_conf.socket_id);
			sess_mp = rte_mempool_create(mp_name,
					CDEV_MP_NB_OBJS,
					max_sess_sz,
					CDEV_MP_CACHE_SZ,
					0, NULL, NULL, NULL,
					NULL, dev_conf.socket_id,
					0);
			socket_ctx[dev_conf.socket_id].session_priv_pool =
					sess_mp;
		}

		if (!socket_ctx[dev_conf.socket_id].session_priv_pool ||
				!socket_ctx[dev_conf.socket_id].session_pool)
			rte_exit(EXIT_FAILURE,
				"Cannot create session pool on socket %d\n",
				dev_conf.socket_id);
		else
			printf("Allocated session pool on socket %d\n",
					dev_conf.socket_id);

		if (rte_cryptodev_configure(cdev_id, &dev_conf))
			rte_panic("Failed to initialize cryptodev %u\n",
					cdev_id);

		qp_conf.nb_descriptors = CDEV_QUEUE_DESC;
		qp_conf.mp_session =
			socket_ctx[dev_conf.socket_id].session_pool;
		qp_conf.mp_session_private =
			socket_ctx[dev_conf.socket_id].session_priv_pool;
		for (qp = 0; qp < dev_conf.nb_queue_pairs; qp++)
			if (rte_cryptodev_queue_pair_setup(cdev_id, qp,
					&qp_conf, dev_conf.socket_id))
				rte_panic("Failed to setup queue %u for "
						"cdev_id %u\n",	0, cdev_id);

		if (rte_cryptodev_start(cdev_id))
			rte_panic("Failed to start cryptodev %u\n",
					cdev_id);
	}

	/* create session pools for eth devices that implement security */
	RTE_ETH_FOREACH_DEV(port_id) {
		if ((enabled_port_mask & (1 << port_id)) &&
				rte_eth_dev_get_sec_ctx(port_id)) {
			int socket_id = rte_eth_dev_socket_id(port_id);

			if (!socket_ctx[socket_id].session_priv_pool) {
				char mp_name[RTE_MEMPOOL_NAMESIZE];
				struct rte_mempool *sess_mp;

				snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
						"sess_mp_%u", socket_id);
				sess_mp = rte_mempool_create(mp_name,
						(CDEV_MP_NB_OBJS * 2),
						max_sess_sz,
						CDEV_MP_CACHE_SZ,
						0, NULL, NULL, NULL,
						NULL, socket_id,
						0);
				if (sess_mp == NULL)
					rte_exit(EXIT_FAILURE,
						"Cannot create session pool "
						"on socket %d\n", socket_id);
				else
					printf("Allocated session pool "
						"on socket %d\n", socket_id);
				socket_ctx[socket_id].session_priv_pool =
						sess_mp;
			}
		}
	}


	printf("\n");

	return 0;
}

static void
port_init(uint16_t portid, uint64_t req_rx_offloads, uint64_t req_tx_offloads)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	uint16_t nb_tx_queue, nb_rx_queue;
	uint16_t tx_queueid, rx_queueid, queue, lcore_id;
	int32_t ret, socket_id;
	struct lcore_conf *qconf;
	struct ether_addr ethaddr;
	struct rte_eth_conf local_port_conf = port_conf;

	rte_eth_dev_info_get(portid, &dev_info);

	/* limit allowed HW offloafs, as user requested */
	dev_info.rx_offload_capa &= dev_rx_offload;
	dev_info.tx_offload_capa &= dev_tx_offload;

	printf("Configuring device port %u:\n", portid);

	rte_eth_macaddr_get(portid, &ethaddr);
	ethaddr_tbl[portid].src = ETHADDR_TO_UINT64(&ethaddr);
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

	if (frame_size) {
		local_port_conf.rxmode.max_rx_pkt_len = frame_size;
		local_port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_JUMBO_FRAME;
	}

	local_port_conf.rxmode.offloads |= req_rx_offloads;
	local_port_conf.txmode.offloads |= req_tx_offloads;

	/* Check that all required capabilities are supported */
	if ((local_port_conf.rxmode.offloads & dev_info.rx_offload_capa) !=
			local_port_conf.rxmode.offloads)
		rte_exit(EXIT_FAILURE,
			"Error: port %u required RX offloads: 0x%" PRIx64
			", avaialbe RX offloads: 0x%" PRIx64 "\n",
			portid, local_port_conf.rxmode.offloads,
			dev_info.rx_offload_capa);

	if ((local_port_conf.txmode.offloads & dev_info.tx_offload_capa) !=
			local_port_conf.txmode.offloads)
		rte_exit(EXIT_FAILURE,
			"Error: port %u required TX offloads: 0x%" PRIx64
			", avaialbe TX offloads: 0x%" PRIx64 "\n",
			portid, local_port_conf.txmode.offloads,
			dev_info.tx_offload_capa);

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM)
		local_port_conf.txmode.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;

	printf("port %u configurng rx_offloads=0x%" PRIx64
		", tx_offloads=0x%" PRIx64 "\n",
		portid, local_port_conf.rxmode.offloads,
		local_port_conf.txmode.offloads);

	local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
		dev_info.flow_type_rss_offloads;
	if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
			port_conf.rx_adv_conf.rss_conf.rss_hf) {
		printf("Port %u modified RSS hash function based on hardware support,"
			"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			portid,
			port_conf.rx_adv_conf.rss_conf.rss_hf,
			local_port_conf.rx_adv_conf.rss_conf.rss_hf);
	}

	ret = rte_eth_dev_configure(portid, nb_rx_queue, nb_tx_queue,
			&local_port_conf);
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
		txconf->offloads = local_port_conf.txmode.offloads;

		ret = rte_eth_tx_queue_setup(portid, tx_queueid, nb_txd,
				socket_id, txconf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
					"err=%d, port=%d\n", ret, portid);

		qconf = &lcore_conf[lcore_id];
		qconf->tx_queue_id[portid] = tx_queueid;

		/* Pre-populate pkt offloads based on capabilities */
		qconf->outbound.ipv4_offloads = PKT_TX_IPV4;
		qconf->outbound.ipv6_offloads = PKT_TX_IPV6;
		if (local_port_conf.txmode.offloads & DEV_TX_OFFLOAD_IPV4_CKSUM)
			qconf->outbound.ipv4_offloads |= PKT_TX_IP_CKSUM;

		tx_queueid++;

		/* init RX queues */
		for (queue = 0; queue < qconf->nb_rx_queue; ++queue) {
			struct rte_eth_rxconf rxq_conf;

			if (portid != qconf->rx_queue_list[queue].port_id)
				continue;

			rx_queueid = qconf->rx_queue_list[queue].queue_id;

			printf("Setup rxq=%d,%d,%d\n", portid, rx_queueid,
					socket_id);

			rxq_conf = dev_info.default_rxconf;
			rxq_conf.offloads = local_port_conf.rxmode.offloads;
			ret = rte_eth_rx_queue_setup(portid, rx_queueid,
					nb_rxd,	socket_id, &rxq_conf,
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
	uint32_t buff_size = frame_size ? (frame_size + RTE_PKTMBUF_HEADROOM) :
			RTE_MBUF_DEFAULT_BUF_SIZE;


	snprintf(s, sizeof(s), "mbuf_pool_%d", socket_id);
	ctx->mbuf_pool = rte_pktmbuf_pool_create(s, nb_mbuf,
			MEMPOOL_CACHE_SIZE, ipsec_metadata_size(),
			buff_size,
			socket_id);
	if (ctx->mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool on socket %d\n",
				socket_id);
	else
		printf("Allocated mbuf pool on socket %d\n", socket_id);
}

static inline int
inline_ipsec_event_esn_overflow(struct rte_security_ctx *ctx, uint64_t md)
{
	struct ipsec_sa *sa;

	/* For inline protocol processing, the metadata in the event will
	 * uniquely identify the security session which raised the event.
	 * Application would then need the userdata it had registered with the
	 * security session to process the event.
	 */

	sa = (struct ipsec_sa *)rte_security_get_userdata(ctx, md);

	if (sa == NULL) {
		/* userdata could not be retrieved */
		return -1;
	}

	/* Sequence number over flow. SA need to be re-established */
	RTE_SET_USED(sa);
	return 0;
}

static int
inline_ipsec_event_callback(uint16_t port_id, enum rte_eth_event_type type,
		 void *param, void *ret_param)
{
	uint64_t md;
	struct rte_eth_event_ipsec_desc *event_desc = NULL;
	struct rte_security_ctx *ctx = (struct rte_security_ctx *)
					rte_eth_dev_get_sec_ctx(port_id);

	RTE_SET_USED(param);

	if (type != RTE_ETH_EVENT_IPSEC)
		return -1;

	event_desc = ret_param;
	if (event_desc == NULL) {
		printf("Event descriptor not set\n");
		return -1;
	}

	md = event_desc->metadata;

	if (event_desc->subtype == RTE_ETH_EVENT_IPSEC_ESN_OVERFLOW)
		return inline_ipsec_event_esn_overflow(ctx, md);
	else if (event_desc->subtype >= RTE_ETH_EVENT_IPSEC_MAX) {
		printf("Invalid IPsec event reported\n");
		return -1;
	}

	return -1;
}

int32_t
main(int32_t argc, char **argv)
{
	int32_t ret;
	uint32_t lcore_id;
	uint8_t socket_id;
	uint16_t portid;
	uint64_t req_rx_offloads, req_tx_offloads;

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

		/* initilaze SPD */
		sp4_init(&socket_ctx[socket_id], socket_id);

		sp6_init(&socket_ctx[socket_id], socket_id);

		/* initilaze SAD */
		sa_init(&socket_ctx[socket_id], socket_id);

		rt_init(&socket_ctx[socket_id], socket_id);

		pool_init(&socket_ctx[socket_id], socket_id, NB_MBUF);
	}

	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		sa_check_offloads(portid, &req_rx_offloads, &req_tx_offloads);
		port_init(portid, req_rx_offloads, req_tx_offloads);
	}

	cryptodevs_init();

	/* start ports */
	RTE_ETH_FOREACH_DEV(portid) {
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

		rte_eth_dev_callback_register(portid,
			RTE_ETH_EVENT_IPSEC, inline_ipsec_event_callback, NULL);
	}

	check_all_ports_link_status(enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
