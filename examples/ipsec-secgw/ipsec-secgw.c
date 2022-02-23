/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#include <stdbool.h>
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
#include <signal.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_bitmap.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <rte_launch.h>
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
#include <rte_eventdev.h>
#include <rte_ip.h>
#include <rte_ip_frag.h>
#include <rte_alarm.h>
#include <rte_telemetry.h>

#include "event_helper.h"
#include "flow.h"
#include "ipsec.h"
#include "ipsec_worker.h"
#include "parser.h"
#include "sad.h"

volatile bool force_quit;

#define MAX_JUMBO_PKT_LEN  9600

#define MEMPOOL_CACHE_SIZE 256

#define CDEV_QUEUE_DESC 2048
#define CDEV_MAP_ENTRIES 16384
#define CDEV_MP_CACHE_SZ 64
#define CDEV_MP_CACHE_MULTIPLIER 1.5 /* from rte_mempool.c */
#define MAX_QUEUE_PAIRS 1

#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3

#define MAX_RX_QUEUE_PER_LCORE 16

#define MAX_LCORE_PARAMS 1024

/*
 * Configurable number of RX/TX ring descriptors
 */
#define IPSEC_SECGW_RX_DESC_DEFAULT 1024
#define IPSEC_SECGW_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = IPSEC_SECGW_RX_DESC_DEFAULT;
static uint16_t nb_txd = IPSEC_SECGW_TX_DESC_DEFAULT;

#define ETHADDR_TO_UINT64(addr) __BYTES_TO_UINT64( \
		(addr)->addr_bytes[0], (addr)->addr_bytes[1], \
		(addr)->addr_bytes[2], (addr)->addr_bytes[3], \
		(addr)->addr_bytes[4], (addr)->addr_bytes[5], \
		0, 0)

#define	FRAG_TBL_BUCKET_ENTRIES	4
#define	MAX_FRAG_TTL_NS		(10LL * NS_PER_S)

#define MTU_TO_FRAMELEN(x)	((x) + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN)

struct ethaddr_info ethaddr_tbl[RTE_MAX_ETHPORTS] = {
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x7e, 0x94, 0x9a) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x22, 0xa1, 0xd9) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x08, 0x69, 0x26) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x49, 0x9e, 0xdd) }
};

struct flow_info flow_info_tbl[RTE_MAX_ETHPORTS];

#define CMD_LINE_OPT_CONFIG		"config"
#define CMD_LINE_OPT_SINGLE_SA		"single-sa"
#define CMD_LINE_OPT_CRYPTODEV_MASK	"cryptodev_mask"
#define CMD_LINE_OPT_TRANSFER_MODE	"transfer-mode"
#define CMD_LINE_OPT_SCHEDULE_TYPE	"event-schedule-type"
#define CMD_LINE_OPT_RX_OFFLOAD		"rxoffload"
#define CMD_LINE_OPT_TX_OFFLOAD		"txoffload"
#define CMD_LINE_OPT_REASSEMBLE		"reassemble"
#define CMD_LINE_OPT_MTU		"mtu"
#define CMD_LINE_OPT_FRAG_TTL		"frag-ttl"
#define CMD_LINE_OPT_EVENT_VECTOR	"event-vector"
#define CMD_LINE_OPT_VECTOR_SIZE	"vector-size"
#define CMD_LINE_OPT_VECTOR_TIMEOUT	"vector-tmo"
#define CMD_LINE_OPT_VECTOR_POOL_SZ	"vector-pool-sz"
#define CMD_LINE_OPT_PER_PORT_POOL	"per-port-pool"

#define CMD_LINE_ARG_EVENT	"event"
#define CMD_LINE_ARG_POLL	"poll"
#define CMD_LINE_ARG_ORDERED	"ordered"
#define CMD_LINE_ARG_ATOMIC	"atomic"
#define CMD_LINE_ARG_PARALLEL	"parallel"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options
	 */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_CONFIG_NUM,
	CMD_LINE_OPT_SINGLE_SA_NUM,
	CMD_LINE_OPT_CRYPTODEV_MASK_NUM,
	CMD_LINE_OPT_TRANSFER_MODE_NUM,
	CMD_LINE_OPT_SCHEDULE_TYPE_NUM,
	CMD_LINE_OPT_RX_OFFLOAD_NUM,
	CMD_LINE_OPT_TX_OFFLOAD_NUM,
	CMD_LINE_OPT_REASSEMBLE_NUM,
	CMD_LINE_OPT_MTU_NUM,
	CMD_LINE_OPT_FRAG_TTL_NUM,
	CMD_LINE_OPT_EVENT_VECTOR_NUM,
	CMD_LINE_OPT_VECTOR_SIZE_NUM,
	CMD_LINE_OPT_VECTOR_TIMEOUT_NUM,
	CMD_LINE_OPT_VECTOR_POOL_SZ_NUM,
	CMD_LINE_OPT_PER_PORT_POOL_NUM,
};

static const struct option lgopts[] = {
	{CMD_LINE_OPT_CONFIG, 1, 0, CMD_LINE_OPT_CONFIG_NUM},
	{CMD_LINE_OPT_SINGLE_SA, 1, 0, CMD_LINE_OPT_SINGLE_SA_NUM},
	{CMD_LINE_OPT_CRYPTODEV_MASK, 1, 0, CMD_LINE_OPT_CRYPTODEV_MASK_NUM},
	{CMD_LINE_OPT_TRANSFER_MODE, 1, 0, CMD_LINE_OPT_TRANSFER_MODE_NUM},
	{CMD_LINE_OPT_SCHEDULE_TYPE, 1, 0, CMD_LINE_OPT_SCHEDULE_TYPE_NUM},
	{CMD_LINE_OPT_RX_OFFLOAD, 1, 0, CMD_LINE_OPT_RX_OFFLOAD_NUM},
	{CMD_LINE_OPT_TX_OFFLOAD, 1, 0, CMD_LINE_OPT_TX_OFFLOAD_NUM},
	{CMD_LINE_OPT_REASSEMBLE, 1, 0, CMD_LINE_OPT_REASSEMBLE_NUM},
	{CMD_LINE_OPT_MTU, 1, 0, CMD_LINE_OPT_MTU_NUM},
	{CMD_LINE_OPT_FRAG_TTL, 1, 0, CMD_LINE_OPT_FRAG_TTL_NUM},
	{CMD_LINE_OPT_EVENT_VECTOR, 0, 0, CMD_LINE_OPT_EVENT_VECTOR_NUM},
	{CMD_LINE_OPT_VECTOR_SIZE, 1, 0, CMD_LINE_OPT_VECTOR_SIZE_NUM},
	{CMD_LINE_OPT_VECTOR_TIMEOUT, 1, 0, CMD_LINE_OPT_VECTOR_TIMEOUT_NUM},
	{CMD_LINE_OPT_VECTOR_POOL_SZ, 1, 0, CMD_LINE_OPT_VECTOR_POOL_SZ_NUM},
	{CMD_LINE_OPT_PER_PORT_POOL, 0, 0, CMD_LINE_OPT_PER_PORT_POOL_NUM},
	{NULL, 0, 0, 0}
};

uint32_t unprotected_port_mask;
uint32_t single_sa_idx;
/* mask of enabled ports */
static uint32_t enabled_port_mask;
static uint64_t enabled_cryptodev_mask = UINT64_MAX;
static int32_t promiscuous_on = 1;
static int32_t numa_on = 1; /**< NUMA is enabled by default. */
static uint32_t nb_lcores;
static uint32_t single_sa;
uint32_t nb_bufs_in_pool;

/*
 * RX/TX HW offload capabilities to enable/use on ethernet ports.
 * By default all capabilities are enabled.
 */
static uint64_t dev_rx_offload = UINT64_MAX;
static uint64_t dev_tx_offload = UINT64_MAX;

/*
 * global values that determine multi-seg policy
 */
static uint32_t frag_tbl_sz;
static uint32_t frame_buf_size = RTE_MBUF_DEFAULT_BUF_SIZE;
static uint32_t mtu_size = RTE_ETHER_MTU;
static uint64_t frag_ttl_ns = MAX_FRAG_TTL_NS;
static uint32_t stats_interval;

/* application wide librte_ipsec/SA parameters */
struct app_sa_prm app_sa_prm = {
			.enable = 0,
			.cache_sz = SA_CACHE_SZ,
			.udp_encap = 0
		};
static const char *cfgfile;

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
	struct {
		struct rte_ip_frag_tbl *tbl;
		struct rte_mempool *pool_indir;
		struct rte_ip_frag_death_row dr;
	} frag;
} __rte_cache_aligned;

static struct lcore_conf lcore_conf[RTE_MAX_LCORE];

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode	= RTE_ETH_MQ_RX_RSS,
		.split_hdr_size = 0,
		.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP |
				RTE_ETH_RSS_TCP | RTE_ETH_RSS_SCTP,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

struct socket_ctx socket_ctx[NB_SOCKETS];

bool per_port_pool;

/*
 * Determine is multi-segment support required:
 *  - either frame buffer size is smaller then mtu
 *  - or reassemble support is requested
 */
static int
multi_seg_required(void)
{
	return (MTU_TO_FRAMELEN(mtu_size) + RTE_PKTMBUF_HEADROOM >
		frame_buf_size || frag_tbl_sz != 0);
}

static inline void
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

static inline void
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


struct ipsec_core_statistics core_statistics[RTE_MAX_LCORE];

/* Print out statistics on packet distribution */
static void
print_stats_cb(__rte_unused void *param)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	float burst_percent, rx_per_call, tx_per_call;
	unsigned int coreid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nCore statistics ====================================");

	for (coreid = 0; coreid < RTE_MAX_LCORE; coreid++) {
		/* skip disabled cores */
		if (rte_lcore_is_enabled(coreid) == 0)
			continue;
		burst_percent = (float)(core_statistics[coreid].burst_rx * 100)/
					core_statistics[coreid].rx;
		rx_per_call =  (float)(core_statistics[coreid].rx)/
				       core_statistics[coreid].rx_call;
		tx_per_call =  (float)(core_statistics[coreid].tx)/
				       core_statistics[coreid].tx_call;
		printf("\nStatistics for core %u ------------------------------"
			   "\nPackets received: %20"PRIu64
			   "\nPackets sent: %24"PRIu64
			   "\nPackets dropped: %21"PRIu64
			   "\nBurst percent: %23.2f"
			   "\nPackets per Rx call: %17.2f"
			   "\nPackets per Tx call: %17.2f",
			   coreid,
			   core_statistics[coreid].rx,
			   core_statistics[coreid].tx,
			   core_statistics[coreid].dropped,
			   burst_percent,
			   rx_per_call,
			   tx_per_call);

		total_packets_dropped += core_statistics[coreid].dropped;
		total_packets_tx += core_statistics[coreid].tx;
		total_packets_rx += core_statistics[coreid].rx;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_rx,
		   total_packets_tx,
		   total_packets_dropped);
	printf("\n====================================================\n");

	rte_eal_alarm_set(stats_interval * US_PER_S, print_stats_cb, NULL);
}

static inline void
prepare_one_packet(struct rte_mbuf *pkt, struct ipsec_traffic *t)
{
	const struct rte_ether_hdr *eth;
	const struct rte_ipv4_hdr *iph4;
	const struct rte_ipv6_hdr *iph6;
	const struct rte_udp_hdr *udp;
	uint16_t ip4_hdr_len;
	uint16_t nat_port;

	eth = rte_pktmbuf_mtod(pkt, const struct rte_ether_hdr *);
	if (eth->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {

		iph4 = (const struct rte_ipv4_hdr *)rte_pktmbuf_adj(pkt,
			RTE_ETHER_HDR_LEN);
		adjust_ipv4_pktlen(pkt, iph4, 0);

		switch (iph4->next_proto_id) {
		case IPPROTO_ESP:
			t->ipsec.pkts[(t->ipsec.num)++] = pkt;
			break;
		case IPPROTO_UDP:
			if (app_sa_prm.udp_encap == 1) {
				ip4_hdr_len = ((iph4->version_ihl &
					RTE_IPV4_HDR_IHL_MASK) *
					RTE_IPV4_IHL_MULTIPLIER);
				udp = rte_pktmbuf_mtod_offset(pkt,
					struct rte_udp_hdr *, ip4_hdr_len);
				nat_port = rte_cpu_to_be_16(IPSEC_NAT_T_PORT);
				if (udp->src_port == nat_port ||
					udp->dst_port == nat_port){
					t->ipsec.pkts[(t->ipsec.num)++] = pkt;
					pkt->packet_type |=
						MBUF_PTYPE_TUNNEL_ESP_IN_UDP;
					break;
				}
			}
		/* Fall through */
		default:
			t->ip4.data[t->ip4.num] = &iph4->next_proto_id;
			t->ip4.pkts[(t->ip4.num)++] = pkt;
		}
		pkt->l2_len = 0;
		pkt->l3_len = sizeof(*iph4);
		pkt->packet_type |= RTE_PTYPE_L3_IPV4;
		if  (pkt->packet_type & RTE_PTYPE_L4_TCP)
			pkt->l4_len = sizeof(struct rte_tcp_hdr);
		else if (pkt->packet_type & RTE_PTYPE_L4_UDP)
			pkt->l4_len = sizeof(struct rte_udp_hdr);
	} else if (eth->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {
		int next_proto;
		size_t l3len, ext_len;
		uint8_t *p;

		/* get protocol type */
		iph6 = (const struct rte_ipv6_hdr *)rte_pktmbuf_adj(pkt,
			RTE_ETHER_HDR_LEN);
		adjust_ipv6_pktlen(pkt, iph6, 0);

		next_proto = iph6->proto;

		/* determine l3 header size up to ESP extension */
		l3len = sizeof(struct ip6_hdr);
		p = rte_pktmbuf_mtod(pkt, uint8_t *);
		while (next_proto != IPPROTO_ESP && l3len < pkt->data_len &&
			(next_proto = rte_ipv6_get_next_ext(p + l3len,
						next_proto, &ext_len)) >= 0)
			l3len += ext_len;

		/* drop packet when IPv6 header exceeds first segment length */
		if (unlikely(l3len > pkt->data_len)) {
			free_pkts(&pkt, 1);
			return;
		}

		switch (next_proto) {
		case IPPROTO_ESP:
			t->ipsec.pkts[(t->ipsec.num)++] = pkt;
			break;
		case IPPROTO_UDP:
			if (app_sa_prm.udp_encap == 1) {
				udp = rte_pktmbuf_mtod_offset(pkt,
					struct rte_udp_hdr *, l3len);
				nat_port = rte_cpu_to_be_16(IPSEC_NAT_T_PORT);
				if (udp->src_port == nat_port ||
					udp->dst_port == nat_port){
					t->ipsec.pkts[(t->ipsec.num)++] = pkt;
					pkt->packet_type |=
						MBUF_PTYPE_TUNNEL_ESP_IN_UDP;
					break;
				}
			}
		/* Fall through */
		default:
			t->ip6.data[t->ip6.num] = &iph6->proto;
			t->ip6.pkts[(t->ip6.num)++] = pkt;
		}
		pkt->l2_len = 0;
		pkt->l3_len = l3len;
		pkt->packet_type |= RTE_PTYPE_L3_IPV6;
	} else {
		/* Unknown/Unsupported type, drop the packet */
		RTE_LOG(ERR, IPSEC, "Unsupported packet type 0x%x\n",
			rte_be_to_cpu_16(eth->ether_type));
		free_pkts(&pkt, 1);
		return;
	}

	/* Check if the packet has been processed inline. For inline protocol
	 * processed packets, the metadata in the mbuf can be used to identify
	 * the security processing done on the packet. The metadata will be
	 * used to retrieve the application registered userdata associated
	 * with the security session.
	 */

	if (pkt->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD &&
			rte_security_dynfield_is_registered()) {
		struct ipsec_sa *sa;
		struct ipsec_mbuf_metadata *priv;
		struct rte_security_ctx *ctx = (struct rte_security_ctx *)
						rte_eth_dev_get_sec_ctx(
						pkt->port);

		/* Retrieve the userdata registered. Here, the userdata
		 * registered is the SA pointer.
		 */
		sa = (struct ipsec_sa *)rte_security_get_userdata(ctx,
				*rte_security_dynfield(pkt));
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
	struct rte_ether_hdr *ethhdr;

	ip = rte_pktmbuf_mtod(pkt, struct ip *);

	ethhdr = (struct rte_ether_hdr *)
		rte_pktmbuf_prepend(pkt, RTE_ETHER_HDR_LEN);

	if (ip->ip_v == IPVERSION) {
		pkt->ol_flags |= qconf->outbound.ipv4_offloads;
		pkt->l3_len = sizeof(struct ip);
		pkt->l2_len = RTE_ETHER_HDR_LEN;

		ip->ip_sum = 0;

		/* calculate IPv4 cksum in SW */
		if ((pkt->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) == 0)
			ip->ip_sum = rte_ipv4_cksum((struct rte_ipv4_hdr *)ip);

		ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	} else {
		pkt->ol_flags |= qconf->outbound.ipv6_offloads;
		pkt->l3_len = sizeof(struct ip6_hdr);
		pkt->l2_len = RTE_ETHER_HDR_LEN;

		ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	}

	memcpy(&ethhdr->src_addr, &ethaddr_tbl[port].src,
			sizeof(struct rte_ether_addr));
	memcpy(&ethhdr->dst_addr, &ethaddr_tbl[port].dst,
			sizeof(struct rte_ether_addr));
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
static inline uint32_t
send_fragment_packet(struct lcore_conf *qconf, struct rte_mbuf *m,
	uint16_t port, uint8_t proto)
{
	struct buffer *tbl;
	uint32_t len, n;
	int32_t rc;

	tbl =  qconf->tx_mbufs + port;
	len = tbl->len;

	/* free space for new fragments */
	if (len + RTE_LIBRTE_IP_FRAG_MAX_FRAG >=  RTE_DIM(tbl->m_table)) {
		send_burst(qconf, len, port);
		len = 0;
	}

	n = RTE_DIM(tbl->m_table) - len;

	if (proto == IPPROTO_IP)
		rc = rte_ipv4_fragment_packet(m, tbl->m_table + len,
			n, mtu_size, m->pool, qconf->frag.pool_indir);
	else
		rc = rte_ipv6_fragment_packet(m, tbl->m_table + len,
			n, mtu_size, m->pool, qconf->frag.pool_indir);

	if (rc >= 0)
		len += rc;
	else
		RTE_LOG(ERR, IPSEC,
			"%s: failed to fragment packet with size %u, "
			"error code: %d\n",
			__func__, m->pkt_len, rte_errno);

	free_pkts(&m, 1);
	return len;
}

/* Enqueue a single packet, and send burst if queue is filled */
static inline int32_t
send_single_packet(struct rte_mbuf *m, uint16_t port, uint8_t proto)
{
	uint32_t lcore_id;
	uint16_t len;
	struct lcore_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_conf[lcore_id];
	len = qconf->tx_mbufs[port].len;

	if (m->pkt_len <= mtu_size) {
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

static inline void
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
			free_pkts(&m, 1);
	}

	trf->ip4.num = n4;
	trf->ip6.num = n6;
}


static inline void
process_pkts_inbound(struct ipsec_ctx *ipsec_ctx,
		struct ipsec_traffic *traffic)
{
	unsigned int lcoreid = rte_lcore_id();
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

	inbound_sp_sa(ipsec_ctx->sp4_ctx,
		ipsec_ctx->sa_ctx, &traffic->ip4, n_ip4,
		&core_statistics[lcoreid].inbound.spd4);

	inbound_sp_sa(ipsec_ctx->sp6_ctx,
		ipsec_ctx->sa_ctx, &traffic->ip6, n_ip6,
		&core_statistics[lcoreid].inbound.spd6);
}

static inline void
outbound_spd_lookup(struct sp_ctx *sp,
		struct traffic_type *ip,
		struct traffic_type *ipsec,
		struct ipsec_spd_stats *stats)
{
	struct rte_mbuf *m;
	uint32_t i, j, sa_idx;

	if (ip->num == 0 || sp == NULL)
		return;

	rte_acl_classify((struct rte_acl_ctx *)sp, ip->data, ip->res,
			ip->num, DEFAULT_MAX_CATEGORIES);

	for (i = 0, j = 0; i < ip->num; i++) {
		m = ip->pkts[i];
		sa_idx = ip->res[i] - 1;

		if (unlikely(ip->res[i] == DISCARD)) {
			free_pkts(&m, 1);

			stats->discard++;
		} else if (unlikely(ip->res[i] == BYPASS)) {
			ip->pkts[j++] = m;

			stats->bypass++;
		} else {
			ipsec->res[ipsec->num] = sa_idx;
			ipsec->pkts[ipsec->num++] = m;

			stats->protect++;
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
	unsigned int lcoreid = rte_lcore_id();

	/* Drop any IPsec traffic from protected ports */
	free_pkts(traffic->ipsec.pkts, traffic->ipsec.num);

	traffic->ipsec.num = 0;

	outbound_spd_lookup(ipsec_ctx->sp4_ctx,
		&traffic->ip4, &traffic->ipsec,
		&core_statistics[lcoreid].outbound.spd4);

	outbound_spd_lookup(ipsec_ctx->sp6_ctx,
		&traffic->ip6, &traffic->ipsec,
		&core_statistics[lcoreid].outbound.spd6);

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
	free_pkts(traffic->ipsec.pkts, traffic->ipsec.num);

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
	unsigned int lcoreid = rte_lcore_id();

	if (nb_pkts == 0)
		return;

	/* Need to do an LPM lookup for non-inline packets. Inline packets will
	 * have port ID in the SA
	 */

	for (i = 0; i < nb_pkts; i++) {
		if (!(pkts[i]->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD)) {
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
		if (pkts[i]->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
			/* Read hop from the SA */
			pkt_hop = get_hop_for_offload_pkt(pkts[i], 0);
		} else {
			/* Need to use hop returned by lookup */
			pkt_hop = hop[lpm_pkts++];
		}

		if ((pkt_hop & RTE_LPM_LOOKUP_SUCCESS) == 0) {
			core_statistics[lcoreid].lpm4.miss++;
			free_pkts(&pkts[i], 1);
			continue;
		}
		send_single_packet(pkts[i], pkt_hop & 0xff, IPPROTO_IP);
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
	unsigned int lcoreid = rte_lcore_id();

	if (nb_pkts == 0)
		return;

	/* Need to do an LPM lookup for non-inline packets. Inline packets will
	 * have port ID in the SA
	 */

	for (i = 0; i < nb_pkts; i++) {
		if (!(pkts[i]->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD)) {
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
		if (pkts[i]->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
			/* Read hop from the SA */
			pkt_hop = get_hop_for_offload_pkt(pkts[i], 1);
		} else {
			/* Need to use hop returned by lookup */
			pkt_hop = hop[lpm_pkts++];
		}

		if (pkt_hop == -1) {
			core_statistics[lcoreid].lpm6.miss++;
			free_pkts(&pkts[i], 1);
			continue;
		}
		send_single_packet(pkts[i], pkt_hop & 0xff, IPPROTO_IPV6);
	}
}

static inline void
process_pkts(struct lcore_conf *qconf, struct rte_mbuf **pkts,
		uint8_t nb_pkts, uint16_t portid)
{
	struct ipsec_traffic traffic;

	prepare_traffic(pkts, &traffic, nb_pkts);

	if (unlikely(single_sa)) {
		if (is_unprotected_port(portid))
			process_pkts_inbound_nosp(&qconf->inbound, &traffic);
		else
			process_pkts_outbound_nosp(&qconf->outbound, &traffic);
	} else {
		if (is_unprotected_port(portid))
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
	unsigned int lcoreid = rte_lcore_id();

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
		inbound_sp_sa(ctx->sp4_ctx, ctx->sa_ctx, &trf.ip4, 0,
			&core_statistics[lcoreid].inbound.spd4);
		route4_pkts(qconf->rt4_ctx, trf.ip4.pkts, trf.ip4.num);
	}

	/* process ipv6 packets */
	if (trf.ip6.num != 0) {
		inbound_sp_sa(ctx->sp6_ctx, ctx->sa_ctx, &trf.ip6, 0,
			&core_statistics[lcoreid].inbound.spd6);
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
void
ipsec_poll_mode_worker(void)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	uint32_t lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int32_t i, nb_rx;
	uint16_t portid;
	uint8_t queueid;
	struct lcore_conf *qconf;
	int32_t rc, socket_id;
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
	qconf->frag.pool_indir = socket_ctx[socket_id].mbuf_pool_indir;

	rc = ipsec_sad_lcore_cache_init(app_sa_prm.cache_sz);
	if (rc != 0) {
		RTE_LOG(ERR, IPSEC,
			"SAD cache init on lcore %u, failed with code: %d\n",
			lcore_id, rc);
		return;
	}

	if (qconf->nb_rx_queue == 0) {
		RTE_LOG(DEBUG, IPSEC, "lcore %u has nothing to do\n",
			lcore_id);
		return;
	}

	RTE_LOG(INFO, IPSEC, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->nb_rx_queue; i++) {
		portid = rxql[i].port_id;
		queueid = rxql[i].queue_id;
		RTE_LOG(INFO, IPSEC,
			" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	while (!force_quit) {
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

			if (nb_rx > 0) {
				core_stats_update_rx(nb_rx);
				process_pkts(qconf, pkts, nb_rx, portid);
			}

			/* dequeue and process completed crypto-ops */
			if (is_unprotected_port(portid))
				drain_inbound_crypto_queues(qconf,
					&qconf->inbound);
			else
				drain_outbound_crypto_queues(qconf,
					&qconf->outbound);
		}
	}
}

int
check_flow_params(uint16_t fdir_portid, uint8_t fdir_qid)
{
	uint16_t i;
	uint16_t portid;
	uint8_t queueid;

	for (i = 0; i < nb_lcore_params; ++i) {
		portid = lcore_params_array[i].port_id;
		if (portid == fdir_portid) {
			queueid = lcore_params_array[i].queue_id;
			if (queueid == fdir_qid)
				break;
		}

		if (i == nb_lcore_params - 1)
			return -1;
	}

	return 1;
}

static int32_t
check_poll_mode_params(struct eh_conf *eh_conf)
{
	uint8_t lcore;
	uint16_t portid;
	uint16_t i;
	int32_t socket_id;

	if (!eh_conf)
		return -EINVAL;

	if (eh_conf->mode != EH_PKT_TRANSFER_MODE_POLL)
		return 0;

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
		" [-c]"
		" [-t STATS_INTERVAL]"
		" [-s NUMBER_OF_MBUFS_IN_PKT_POOL]"
		" -f CONFIG_FILE"
		" --config (port,queue,lcore)[,(port,queue,lcore)]"
		" [--single-sa SAIDX]"
		" [--cryptodev_mask MASK]"
		" [--transfer-mode MODE]"
		" [--event-schedule-type TYPE]"
		" [--" CMD_LINE_OPT_RX_OFFLOAD " RX_OFFLOAD_MASK]"
		" [--" CMD_LINE_OPT_TX_OFFLOAD " TX_OFFLOAD_MASK]"
		" [--" CMD_LINE_OPT_REASSEMBLE " REASSEMBLE_TABLE_SIZE]"
		" [--" CMD_LINE_OPT_MTU " MTU]"
		" [--event-vector]"
		" [--vector-size SIZE]"
		" [--vector-tmo TIMEOUT in ns]"
		"\n\n"
		"  -p PORTMASK: Hexadecimal bitmask of ports to configure\n"
		"  -P : Enable promiscuous mode\n"
		"  -u PORTMASK: Hexadecimal bitmask of unprotected ports\n"
		"  -j FRAMESIZE: Data buffer size, minimum (and default)\n"
		"     value: RTE_MBUF_DEFAULT_BUF_SIZE\n"
		"  -l enables code-path that uses librte_ipsec\n"
		"  -w REPLAY_WINDOW_SIZE specifies IPsec SQN replay window\n"
		"     size for each SA\n"
		"  -e enables ESN\n"
		"  -a enables SA SQN atomic behaviour\n"
		"  -c specifies inbound SAD cache size,\n"
		"     zero value disables the cache (default value: 128)\n"
		"  -t specifies statistics screen update interval,\n"
		"     zero disables statistics screen (default value: 0)\n"
		"  -s number of mbufs in packet pool, if not specified number\n"
		"     of mbufs will be calculated based on number of cores,\n"
		"     ports and crypto queues\n"
		"  -f CONFIG_FILE: Configuration file\n"
		"  --config (port,queue,lcore): Rx queue configuration. In poll\n"
		"                               mode determines which queues from\n"
		"                               which ports are mapped to which cores.\n"
		"                               In event mode this option is not used\n"
		"                               as packets are dynamically scheduled\n"
		"                               to cores by HW.\n"
		"  --single-sa SAIDX: In poll mode use single SA index for\n"
		"                     outbound traffic, bypassing the SP\n"
		"                     In event mode selects driver submode,\n"
		"                     SA index value is ignored\n"
		"  --cryptodev_mask MASK: Hexadecimal bitmask of the crypto\n"
		"                         devices to configure\n"
		"  --transfer-mode MODE\n"
		"               \"poll\"  : Packet transfer via polling (default)\n"
		"               \"event\" : Packet transfer via event device\n"
		"  --event-schedule-type TYPE queue schedule type, used only when\n"
		"                             transfer mode is set to event\n"
		"               \"ordered\"  : Ordered (default)\n"
		"               \"atomic\"   : Atomic\n"
		"               \"parallel\" : Parallel\n"
		"  --" CMD_LINE_OPT_RX_OFFLOAD
		": bitmask of the RX HW offload capabilities to enable/use\n"
		"                         (RTE_ETH_RX_OFFLOAD_*)\n"
		"  --" CMD_LINE_OPT_TX_OFFLOAD
		": bitmask of the TX HW offload capabilities to enable/use\n"
		"                         (RTE_ETH_TX_OFFLOAD_*)\n"
		"  --" CMD_LINE_OPT_REASSEMBLE " NUM"
		": max number of entries in reassemble(fragment) table\n"
		"    (zero (default value) disables reassembly)\n"
		"  --" CMD_LINE_OPT_MTU " MTU"
		": MTU value on all ports (default value: 1500)\n"
		"    outgoing packets with bigger size will be fragmented\n"
		"    incoming packets with bigger size will be discarded\n"
		"  --" CMD_LINE_OPT_FRAG_TTL " FRAG_TTL_NS"
		": fragments lifetime in nanoseconds, default\n"
		"    and maximum value is 10.000.000.000 ns (10 s)\n"
		"  --event-vector enables event vectorization\n"
		"  --vector-size Max vector size (default value: 16)\n"
		"  --vector-tmo Max vector timeout in nanoseconds"
		"    (default value: 102400)\n"
		"  --" CMD_LINE_OPT_PER_PORT_POOL " Enable per port mbuf pool\n"
		"  --" CMD_LINE_OPT_VECTOR_POOL_SZ " Vector pool size\n"
		"                    (default value is based on mbuf count)\n"
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

	errno = 0;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if ((pm == 0) && errno)
		return -1;

	return pm;
}

static int64_t
parse_decimal(const char *str)
{
	char *end = NULL;
	uint64_t num;

	num = strtoull(str, &end, 10);
	if ((str[0] == '\0') || (end == NULL) || (*end != '\0')
		|| num > INT64_MAX)
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

	printf("replay window size: %u\n", prm->window_size);
	printf("ESN: %s\n", (prm->enable_esn == 0) ? "disabled" : "enabled");
	printf("SA flags: %#" PRIx64 "\n", prm->flags);
	printf("Frag TTL: %" PRIu64 " ns\n", frag_ttl_ns);
}

static int
parse_transfer_mode(struct eh_conf *conf, const char *optarg)
{
	if (!strcmp(CMD_LINE_ARG_POLL, optarg))
		conf->mode = EH_PKT_TRANSFER_MODE_POLL;
	else if (!strcmp(CMD_LINE_ARG_EVENT, optarg))
		conf->mode = EH_PKT_TRANSFER_MODE_EVENT;
	else {
		printf("Unsupported packet transfer mode\n");
		return -EINVAL;
	}

	return 0;
}

static int
parse_schedule_type(struct eh_conf *conf, const char *optarg)
{
	struct eventmode_conf *em_conf = NULL;

	/* Get eventmode conf */
	em_conf = conf->mode_params;

	if (!strcmp(CMD_LINE_ARG_ORDERED, optarg))
		em_conf->ext_params.sched_type = RTE_SCHED_TYPE_ORDERED;
	else if (!strcmp(CMD_LINE_ARG_ATOMIC, optarg))
		em_conf->ext_params.sched_type = RTE_SCHED_TYPE_ATOMIC;
	else if (!strcmp(CMD_LINE_ARG_PARALLEL, optarg))
		em_conf->ext_params.sched_type = RTE_SCHED_TYPE_PARALLEL;
	else {
		printf("Unsupported queue schedule type\n");
		return -EINVAL;
	}

	return 0;
}

static int32_t
parse_args(int32_t argc, char **argv, struct eh_conf *eh_conf)
{
	int opt;
	int64_t ret;
	char **argvopt;
	int32_t option_index;
	char *prgname = argv[0];
	int32_t f_present = 0;
	struct eventmode_conf *em_conf = NULL;

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "aelp:Pu:f:j:w:c:t:s:",
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
			cfgfile = optarg;
			f_present = 1;
			break;

		case 's':
			ret = parse_decimal(optarg);
			if (ret < 0) {
				printf("Invalid number of buffers in a pool: "
					"%s\n", optarg);
				print_usage(prgname);
				return -1;
			}

			nb_bufs_in_pool = ret;
			break;

		case 'j':
			ret = parse_decimal(optarg);
			if (ret < RTE_MBUF_DEFAULT_BUF_SIZE ||
					ret > UINT16_MAX) {
				printf("Invalid frame buffer size value: %s\n",
					optarg);
				print_usage(prgname);
				return -1;
			}
			frame_buf_size = ret;
			printf("Custom frame buffer size %u\n", frame_buf_size);
			break;
		case 'l':
			app_sa_prm.enable = 1;
			break;
		case 'w':
			app_sa_prm.window_size = parse_decimal(optarg);
			break;
		case 'e':
			app_sa_prm.enable_esn = 1;
			break;
		case 'a':
			app_sa_prm.enable = 1;
			app_sa_prm.flags |= RTE_IPSEC_SAFLAG_SQN_ATOM;
			break;
		case 'c':
			ret = parse_decimal(optarg);
			if (ret < 0) {
				printf("Invalid SA cache size: %s\n", optarg);
				print_usage(prgname);
				return -1;
			}
			app_sa_prm.cache_sz = ret;
			break;
		case 't':
			ret = parse_decimal(optarg);
			if (ret < 0) {
				printf("Invalid interval value: %s\n", optarg);
				print_usage(prgname);
				return -1;
			}
			stats_interval = ret;
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
			if (ret == -1 || ret > UINT32_MAX) {
				printf("Invalid argument[sa_idx]\n");
				print_usage(prgname);
				return -1;
			}

			/* else */
			single_sa = 1;
			single_sa_idx = ret;
			eh_conf->ipsec_mode = EH_IPSEC_MODE_TYPE_DRIVER;
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

		case CMD_LINE_OPT_TRANSFER_MODE_NUM:
			ret = parse_transfer_mode(eh_conf, optarg);
			if (ret < 0) {
				printf("Invalid packet transfer mode\n");
				print_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_SCHEDULE_TYPE_NUM:
			ret = parse_schedule_type(eh_conf, optarg);
			if (ret < 0) {
				printf("Invalid queue schedule type\n");
				print_usage(prgname);
				return -1;
			}
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
		case CMD_LINE_OPT_REASSEMBLE_NUM:
			ret = parse_decimal(optarg);
			if (ret < 0 || ret > UINT32_MAX) {
				printf("Invalid argument for \'%s\': %s\n",
					CMD_LINE_OPT_REASSEMBLE, optarg);
				print_usage(prgname);
				return -1;
			}
			frag_tbl_sz = ret;
			break;
		case CMD_LINE_OPT_MTU_NUM:
			ret = parse_decimal(optarg);
			if (ret < 0 || ret > RTE_IPV4_MAX_PKT_LEN) {
				printf("Invalid argument for \'%s\': %s\n",
					CMD_LINE_OPT_MTU, optarg);
				print_usage(prgname);
				return -1;
			}
			mtu_size = ret;
			break;
		case CMD_LINE_OPT_FRAG_TTL_NUM:
			ret = parse_decimal(optarg);
			if (ret < 0 || ret > MAX_FRAG_TTL_NS) {
				printf("Invalid argument for \'%s\': %s\n",
					CMD_LINE_OPT_MTU, optarg);
				print_usage(prgname);
				return -1;
			}
			frag_ttl_ns = ret;
			break;
		case CMD_LINE_OPT_EVENT_VECTOR_NUM:
			em_conf = eh_conf->mode_params;
			em_conf->ext_params.event_vector = 1;
			break;
		case CMD_LINE_OPT_VECTOR_SIZE_NUM:
			ret = parse_decimal(optarg);

			if (ret > MAX_PKT_BURST) {
				printf("Invalid argument for \'%s\': %s\n",
					CMD_LINE_OPT_VECTOR_SIZE, optarg);
				print_usage(prgname);
				return -1;
			}
			em_conf = eh_conf->mode_params;
			em_conf->ext_params.vector_size = ret;
			break;
		case CMD_LINE_OPT_VECTOR_TIMEOUT_NUM:
			ret = parse_decimal(optarg);

			em_conf = eh_conf->mode_params;
			em_conf->vector_tmo_ns = ret;
			break;
		case CMD_LINE_OPT_VECTOR_POOL_SZ_NUM:
			ret = parse_decimal(optarg);

			em_conf = eh_conf->mode_params;
			em_conf->vector_pool_sz = ret;
			break;
		case CMD_LINE_OPT_PER_PORT_POOL_NUM:
			per_port_pool = 1;
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

	/* check do we need to enable multi-seg support */
	if (multi_seg_required()) {
		/* legacy mode doesn't support multi-seg */
		app_sa_prm.enable = 1;
		printf("frame buf size: %u, mtu: %u, "
			"number of reassemble entries: %u\n"
			"multi-segment support is required\n",
			frame_buf_size, mtu_size, frag_tbl_sz);
	}

	print_app_sa_prm(&app_sa_prm);

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

/*
 * Update destination ethaddr for the port.
 */
int
add_dst_ethaddr(uint16_t port, const struct rte_ether_addr *addr)
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
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", portid,
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
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
		printf("Failed to insert cdev mapping for (lcore %u, "
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

	/* Required cryptodevs with operation chaining */
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

static uint16_t
cryptodevs_init(uint16_t req_queue_num)
{
	struct rte_cryptodev_config dev_conf;
	struct rte_cryptodev_qp_conf qp_conf;
	uint16_t idx, max_nb_qps, qp, total_nb_qps, i;
	int16_t cdev_id;
	struct rte_hash_parameters params = { 0 };

	const uint64_t mseg_flag = multi_seg_required() ?
				RTE_CRYPTODEV_FF_IN_PLACE_SGL : 0;

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

	idx = 0;
	total_nb_qps = 0;
	for (cdev_id = 0; cdev_id < rte_cryptodev_count(); cdev_id++) {
		struct rte_cryptodev_info cdev_info;

		if (check_cryptodev_mask((uint8_t)cdev_id))
			continue;

		rte_cryptodev_info_get(cdev_id, &cdev_info);

		if ((mseg_flag & cdev_info.feature_flags) != mseg_flag)
			rte_exit(EXIT_FAILURE,
				"Device %hd does not support \'%s\' feature\n",
				cdev_id,
				rte_cryptodev_get_feature_name(mseg_flag));

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

		qp = RTE_MIN(max_nb_qps, RTE_MAX(req_queue_num, qp));
		if (qp == 0)
			continue;

		total_nb_qps += qp;
		dev_conf.socket_id = rte_cryptodev_socket_id(cdev_id);
		dev_conf.nb_queue_pairs = qp;
		dev_conf.ff_disable = RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO;

		uint32_t dev_max_sess = cdev_info.sym.max_nb_sessions;
		if (dev_max_sess != 0 &&
				dev_max_sess < get_nb_crypto_sessions())
			rte_exit(EXIT_FAILURE,
				"Device does not support at least %u "
				"sessions", get_nb_crypto_sessions());

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

	printf("\n");

	return total_nb_qps;
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
	struct rte_ether_addr ethaddr;
	struct rte_eth_conf local_port_conf = port_conf;

	ret = rte_eth_dev_info_get(portid, &dev_info);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			"Error during getting device (port %u) info: %s\n",
			portid, strerror(-ret));

	/* limit allowed HW offloads, as user requested */
	dev_info.rx_offload_capa &= dev_rx_offload;
	dev_info.tx_offload_capa &= dev_tx_offload;

	printf("Configuring device port %u:\n", portid);

	ret = rte_eth_macaddr_get(portid, &ethaddr);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			"Error getting MAC address (port %u): %s\n",
			portid, rte_strerror(-ret));

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

	local_port_conf.rxmode.mtu = mtu_size;

	if (multi_seg_required()) {
		local_port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
		local_port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
	}

	local_port_conf.rxmode.offloads |= req_rx_offloads;
	local_port_conf.txmode.offloads |= req_tx_offloads;

	/* Check that all required capabilities are supported */
	if ((local_port_conf.rxmode.offloads & dev_info.rx_offload_capa) !=
			local_port_conf.rxmode.offloads)
		rte_exit(EXIT_FAILURE,
			"Error: port %u required RX offloads: 0x%" PRIx64
			", available RX offloads: 0x%" PRIx64 "\n",
			portid, local_port_conf.rxmode.offloads,
			dev_info.rx_offload_capa);

	if ((local_port_conf.txmode.offloads & dev_info.tx_offload_capa) !=
			local_port_conf.txmode.offloads)
		rte_exit(EXIT_FAILURE,
			"Error: port %u required TX offloads: 0x%" PRIx64
			", available TX offloads: 0x%" PRIx64 "\n",
			portid, local_port_conf.txmode.offloads,
			dev_info.tx_offload_capa);

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
		local_port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;

	printf("port %u configuring rx_offloads=0x%" PRIx64
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
		qconf->outbound.ipv4_offloads = RTE_MBUF_F_TX_IPV4;
		qconf->outbound.ipv6_offloads = RTE_MBUF_F_TX_IPV6;
		if (local_port_conf.txmode.offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
			qconf->outbound.ipv4_offloads |= RTE_MBUF_F_TX_IP_CKSUM;

		tx_queueid++;

		/* init RX queues */
		for (queue = 0; queue < qconf->nb_rx_queue; ++queue) {
			struct rte_eth_rxconf rxq_conf;
			struct rte_mempool *pool;

			if (portid != qconf->rx_queue_list[queue].port_id)
				continue;

			rx_queueid = qconf->rx_queue_list[queue].queue_id;

			printf("Setup rxq=%d,%d,%d\n", portid, rx_queueid,
					socket_id);

			rxq_conf = dev_info.default_rxconf;
			rxq_conf.offloads = local_port_conf.rxmode.offloads;

			if (per_port_pool)
				pool = socket_ctx[socket_id].mbuf_pool[portid];
			else
				pool = socket_ctx[socket_id].mbuf_pool[0];

			ret = rte_eth_rx_queue_setup(portid, rx_queueid,
					nb_rxd,	socket_id, &rxq_conf, pool);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_rx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);
		}
	}
	printf("\n");
}

static size_t
max_session_size(void)
{
	size_t max_sz, sz;
	void *sec_ctx;
	int16_t cdev_id, port_id, n;

	max_sz = 0;
	n =  rte_cryptodev_count();
	for (cdev_id = 0; cdev_id != n; cdev_id++) {
		sz = rte_cryptodev_sym_get_private_session_size(cdev_id);
		if (sz > max_sz)
			max_sz = sz;
		/*
		 * If crypto device is security capable, need to check the
		 * size of security session as well.
		 */

		/* Get security context of the crypto device */
		sec_ctx = rte_cryptodev_get_sec_ctx(cdev_id);
		if (sec_ctx == NULL)
			continue;

		/* Get size of security session */
		sz = rte_security_session_get_size(sec_ctx);
		if (sz > max_sz)
			max_sz = sz;
	}

	RTE_ETH_FOREACH_DEV(port_id) {
		if ((enabled_port_mask & (1 << port_id)) == 0)
			continue;

		sec_ctx = rte_eth_dev_get_sec_ctx(port_id);
		if (sec_ctx == NULL)
			continue;

		sz = rte_security_session_get_size(sec_ctx);
		if (sz > max_sz)
			max_sz = sz;
	}

	return max_sz;
}

static void
session_pool_init(struct socket_ctx *ctx, int32_t socket_id, size_t sess_sz)
{
	char mp_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *sess_mp;
	uint32_t nb_sess;

	snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
			"sess_mp_%u", socket_id);
	nb_sess = (get_nb_crypto_sessions() + CDEV_MP_CACHE_SZ *
		rte_lcore_count());
	nb_sess = RTE_MAX(nb_sess, CDEV_MP_CACHE_SZ *
			CDEV_MP_CACHE_MULTIPLIER);
	sess_mp = rte_cryptodev_sym_session_pool_create(
			mp_name, nb_sess, sess_sz, CDEV_MP_CACHE_SZ, 0,
			socket_id);
	ctx->session_pool = sess_mp;

	if (ctx->session_pool == NULL)
		rte_exit(EXIT_FAILURE,
			"Cannot init session pool on socket %d\n", socket_id);
	else
		printf("Allocated session pool on socket %d\n",	socket_id);
}

static void
session_priv_pool_init(struct socket_ctx *ctx, int32_t socket_id,
	size_t sess_sz)
{
	char mp_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *sess_mp;
	uint32_t nb_sess;

	snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
			"sess_mp_priv_%u", socket_id);
	nb_sess = (get_nb_crypto_sessions() + CDEV_MP_CACHE_SZ *
		rte_lcore_count());
	nb_sess = RTE_MAX(nb_sess, CDEV_MP_CACHE_SZ *
			CDEV_MP_CACHE_MULTIPLIER);
	sess_mp = rte_mempool_create(mp_name,
			nb_sess,
			sess_sz,
			CDEV_MP_CACHE_SZ,
			0, NULL, NULL, NULL,
			NULL, socket_id,
			0);
	ctx->session_priv_pool = sess_mp;

	if (ctx->session_priv_pool == NULL)
		rte_exit(EXIT_FAILURE,
			"Cannot init session priv pool on socket %d\n",
			socket_id);
	else
		printf("Allocated session priv pool on socket %d\n",
			socket_id);
}

static void
pool_init(struct socket_ctx *ctx, int32_t socket_id, int portid,
	  uint32_t nb_mbuf)
{
	char s[64];
	int32_t ms;


	/* mbuf_pool is initialised by the pool_init() function*/
	if (socket_ctx[socket_id].mbuf_pool[portid])
		return;

	snprintf(s, sizeof(s), "mbuf_pool_%d_%d", socket_id, portid);
	ctx->mbuf_pool[portid] = rte_pktmbuf_pool_create(s, nb_mbuf,
							 MEMPOOL_CACHE_SIZE,
							 ipsec_metadata_size(),
							 frame_buf_size,
							 socket_id);

	/*
	 * if multi-segment support is enabled, then create a pool
	 * for indirect mbufs. This is not per-port but global.
	 */
	ms = multi_seg_required();
	if (ms != 0 && !ctx->mbuf_pool_indir) {
		snprintf(s, sizeof(s), "mbuf_pool_indir_%d", socket_id);
		ctx->mbuf_pool_indir = rte_pktmbuf_pool_create(s, nb_mbuf,
			MEMPOOL_CACHE_SIZE, 0, 0, socket_id);
	}

	if (ctx->mbuf_pool[portid] == NULL ||
	    (ms != 0 && ctx->mbuf_pool_indir == NULL))
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

static int
ethdev_reset_event_callback(uint16_t port_id,
		enum rte_eth_event_type type,
		 void *param __rte_unused, void *ret_param __rte_unused)
{
	printf("Reset Event on port id %d type %d\n", port_id, type);
	printf("Force quit application");
	force_quit = true;
	return 0;
}

static uint16_t
rx_callback(__rte_unused uint16_t port, __rte_unused uint16_t queue,
	struct rte_mbuf *pkt[], uint16_t nb_pkts,
	__rte_unused uint16_t max_pkts, void *user_param)
{
	uint64_t tm;
	uint32_t i, k;
	struct lcore_conf *lc;
	struct rte_mbuf *mb;
	struct rte_ether_hdr *eth;

	lc = user_param;
	k = 0;
	tm = 0;

	for (i = 0; i != nb_pkts; i++) {

		mb = pkt[i];
		eth = rte_pktmbuf_mtod(mb, struct rte_ether_hdr *);
		if (eth->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {

			struct rte_ipv4_hdr *iph;

			iph = (struct rte_ipv4_hdr *)(eth + 1);
			if (rte_ipv4_frag_pkt_is_fragmented(iph)) {

				mb->l2_len = sizeof(*eth);
				mb->l3_len = sizeof(*iph);
				tm = (tm != 0) ? tm : rte_rdtsc();
				mb = rte_ipv4_frag_reassemble_packet(
					lc->frag.tbl, &lc->frag.dr,
					mb, tm, iph);

				if (mb != NULL) {
					/* fix ip cksum after reassemble. */
					iph = rte_pktmbuf_mtod_offset(mb,
						struct rte_ipv4_hdr *,
						mb->l2_len);
					iph->hdr_checksum = 0;
					iph->hdr_checksum = rte_ipv4_cksum(iph);
				}
			}
		} else if (eth->ether_type ==
				rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {

			struct rte_ipv6_hdr *iph;
			struct rte_ipv6_fragment_ext *fh;

			iph = (struct rte_ipv6_hdr *)(eth + 1);
			fh = rte_ipv6_frag_get_ipv6_fragment_header(iph);
			if (fh != NULL) {
				mb->l2_len = sizeof(*eth);
				mb->l3_len = (uintptr_t)fh - (uintptr_t)iph +
					sizeof(*fh);
				tm = (tm != 0) ? tm : rte_rdtsc();
				mb = rte_ipv6_frag_reassemble_packet(
					lc->frag.tbl, &lc->frag.dr,
					mb, tm, iph, fh);
				if (mb != NULL)
					/* fix l3_len after reassemble. */
					mb->l3_len = mb->l3_len - sizeof(*fh);
			}
		}

		pkt[k] = mb;
		k += (mb != NULL);
	}

	/* some fragments were encountered, drain death row */
	if (tm != 0)
		rte_ip_frag_free_death_row(&lc->frag.dr, 0);

	return k;
}


static int
reassemble_lcore_init(struct lcore_conf *lc, uint32_t cid)
{
	int32_t sid;
	uint32_t i;
	uint64_t frag_cycles;
	const struct lcore_rx_queue *rxq;
	const struct rte_eth_rxtx_callback *cb;

	/* create fragment table */
	sid = rte_lcore_to_socket_id(cid);
	frag_cycles = (rte_get_tsc_hz() + NS_PER_S - 1) /
		NS_PER_S * frag_ttl_ns;

	lc->frag.tbl = rte_ip_frag_table_create(frag_tbl_sz,
		FRAG_TBL_BUCKET_ENTRIES, frag_tbl_sz, frag_cycles, sid);
	if (lc->frag.tbl == NULL) {
		printf("%s(%u): failed to create fragment table of size: %u, "
			"error code: %d\n",
			__func__, cid, frag_tbl_sz, rte_errno);
		return -ENOMEM;
	}

	/* setup reassemble RX callbacks for all queues */
	for (i = 0; i != lc->nb_rx_queue; i++) {

		rxq = lc->rx_queue_list + i;
		cb = rte_eth_add_rx_callback(rxq->port_id, rxq->queue_id,
			rx_callback, lc);
		if (cb == NULL) {
			printf("%s(%u): failed to install RX callback for "
				"portid=%u, queueid=%u, error code: %d\n",
				__func__, cid,
				rxq->port_id, rxq->queue_id, rte_errno);
			return -ENOMEM;
		}
	}

	return 0;
}

static int
reassemble_init(void)
{
	int32_t rc;
	uint32_t i, lc;

	rc = 0;
	for (i = 0; i != nb_lcore_params; i++) {
		lc = lcore_params[i].lcore_id;
		rc = reassemble_lcore_init(lcore_conf + lc, lc);
		if (rc != 0)
			break;
	}

	return rc;
}

static void
create_default_ipsec_flow(uint16_t port_id, uint64_t rx_offloads)
{
	struct rte_flow_action action[2];
	struct rte_flow_item pattern[2];
	struct rte_flow_attr attr = {0};
	struct rte_flow_error err;
	struct rte_flow *flow;
	int ret;

	if (!(rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY))
		return;

	/* Add the default rte_flow to enable SECURITY for all ESP packets */

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ESP;
	pattern[0].spec = NULL;
	pattern[0].mask = NULL;
	pattern[0].last = NULL;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

	action[0].type = RTE_FLOW_ACTION_TYPE_SECURITY;
	action[0].conf = NULL;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
	action[1].conf = NULL;

	attr.ingress = 1;

	ret = rte_flow_validate(port_id, &attr, pattern, action, &err);
	if (ret)
		return;

	flow = rte_flow_create(port_id, &attr, pattern, action, &err);
	if (flow == NULL)
		return;

	flow_info_tbl[port_id].rx_def_flow = flow;
	RTE_LOG(INFO, IPSEC,
		"Created default flow enabling SECURITY for all ESP traffic on port %d\n",
		port_id);
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

static void
ev_mode_sess_verify(struct ipsec_sa *sa, int nb_sa)
{
	struct rte_ipsec_session *ips;
	int32_t i;

	if (!sa || !nb_sa)
		return;

	for (i = 0; i < nb_sa; i++) {
		ips = ipsec_get_primary_session(&sa[i]);
		if (ips->type != RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
			rte_exit(EXIT_FAILURE, "Event mode supports only "
				 "inline protocol sessions\n");
	}

}

static int32_t
check_event_mode_params(struct eh_conf *eh_conf)
{
	struct eventmode_conf *em_conf = NULL;
	struct lcore_params *params;
	uint16_t portid;

	if (!eh_conf || !eh_conf->mode_params)
		return -EINVAL;

	/* Get eventmode conf */
	em_conf = eh_conf->mode_params;

	if (eh_conf->mode == EH_PKT_TRANSFER_MODE_POLL &&
	    em_conf->ext_params.sched_type != SCHED_TYPE_NOT_SET) {
		printf("error: option --event-schedule-type applies only to "
		       "event mode\n");
		return -EINVAL;
	}

	if (eh_conf->mode != EH_PKT_TRANSFER_MODE_EVENT)
		return 0;

	/* Set schedule type to ORDERED if it wasn't explicitly set by user */
	if (em_conf->ext_params.sched_type == SCHED_TYPE_NOT_SET)
		em_conf->ext_params.sched_type = RTE_SCHED_TYPE_ORDERED;

	/*
	 * Event mode currently supports only inline protocol sessions.
	 * If there are other types of sessions configured then exit with
	 * error.
	 */
	ev_mode_sess_verify(sa_in, nb_sa_in);
	ev_mode_sess_verify(sa_out, nb_sa_out);


	/* Option --config does not apply to event mode */
	if (nb_lcore_params > 0) {
		printf("error: option --config applies only to poll mode\n");
		return -EINVAL;
	}

	/*
	 * In order to use the same port_init routine for both poll and event
	 * modes initialize lcore_params with one queue for each eth port
	 */
	lcore_params = lcore_params_array;
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		params = &lcore_params[nb_lcore_params++];
		params->port_id = portid;
		params->queue_id = 0;
		params->lcore_id = rte_get_next_lcore(0, 0, 1);
	}

	return 0;
}

static void
inline_sessions_free(struct sa_ctx *sa_ctx)
{
	struct rte_ipsec_session *ips;
	struct ipsec_sa *sa;
	int32_t ret;
	uint32_t i;

	if (!sa_ctx)
		return;

	for (i = 0; i < sa_ctx->nb_sa; i++) {

		sa = &sa_ctx->sa[i];
		if (!sa->spi)
			continue;

		ips = ipsec_get_primary_session(sa);
		if (ips->type != RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL &&
		    ips->type != RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO)
			continue;

		if (!rte_eth_dev_is_valid_port(sa->portid))
			continue;

		ret = rte_security_session_destroy(
				rte_eth_dev_get_sec_ctx(sa->portid),
				ips->security.ses);
		if (ret)
			RTE_LOG(ERR, IPSEC, "Failed to destroy security "
					    "session type %d, spi %d\n",
					    ips->type, sa->spi);
	}
}

static uint32_t
calculate_nb_mbufs(uint16_t nb_ports, uint16_t nb_crypto_qp, uint32_t nb_rxq,
		uint32_t nb_txq)
{
	return RTE_MAX((nb_rxq * nb_rxd +
			nb_ports * nb_lcores * MAX_PKT_BURST +
			nb_ports * nb_txq * nb_txd +
			nb_lcores * MEMPOOL_CACHE_SIZE +
			nb_crypto_qp * CDEV_QUEUE_DESC +
			nb_lcores * frag_tbl_sz *
			FRAG_TBL_BUCKET_ENTRIES),
		       8192U);
}


static int
handle_telemetry_cmd_ipsec_secgw_stats(const char *cmd __rte_unused,
		const char *params, struct rte_tel_data *data)
{
	uint64_t total_pkts_dropped = 0, total_pkts_tx = 0, total_pkts_rx = 0;
	unsigned int coreid;

	rte_tel_data_start_dict(data);

	if (params) {
		coreid = (uint32_t)atoi(params);
		if (rte_lcore_is_enabled(coreid) == 0)
			return -EINVAL;

		total_pkts_dropped = core_statistics[coreid].dropped;
		total_pkts_tx = core_statistics[coreid].tx;
		total_pkts_rx = core_statistics[coreid].rx;

	} else {
		for (coreid = 0; coreid < RTE_MAX_LCORE; coreid++) {

			/* skip disabled cores */
			if (rte_lcore_is_enabled(coreid) == 0)
				continue;

			total_pkts_dropped += core_statistics[coreid].dropped;
			total_pkts_tx += core_statistics[coreid].tx;
			total_pkts_rx += core_statistics[coreid].rx;
		}
	}

	/* add telemetry key/values pairs */
	rte_tel_data_add_dict_u64(data, "packets received",
				total_pkts_rx);

	rte_tel_data_add_dict_u64(data, "packets transmitted",
				total_pkts_tx);

	rte_tel_data_add_dict_u64(data, "packets dropped",
				total_pkts_dropped);


	return 0;
}

static void
update_lcore_statistics(struct ipsec_core_statistics *total, uint32_t coreid)
{
	struct ipsec_core_statistics *lcore_stats;

	/* skip disabled cores */
	if (rte_lcore_is_enabled(coreid) == 0)
		return;

	lcore_stats = &core_statistics[coreid];

	total->rx = lcore_stats->rx;
	total->dropped = lcore_stats->dropped;
	total->tx = lcore_stats->tx;

	/* outbound stats */
	total->outbound.spd6.protect += lcore_stats->outbound.spd6.protect;
	total->outbound.spd6.bypass += lcore_stats->outbound.spd6.bypass;
	total->outbound.spd6.discard += lcore_stats->outbound.spd6.discard;

	total->outbound.spd4.protect += lcore_stats->outbound.spd4.protect;
	total->outbound.spd4.bypass += lcore_stats->outbound.spd4.bypass;
	total->outbound.spd4.discard += lcore_stats->outbound.spd4.discard;

	total->outbound.sad.miss += lcore_stats->outbound.sad.miss;

	/* inbound stats */
	total->inbound.spd6.protect += lcore_stats->inbound.spd6.protect;
	total->inbound.spd6.bypass += lcore_stats->inbound.spd6.bypass;
	total->inbound.spd6.discard += lcore_stats->inbound.spd6.discard;

	total->inbound.spd4.protect += lcore_stats->inbound.spd4.protect;
	total->inbound.spd4.bypass += lcore_stats->inbound.spd4.bypass;
	total->inbound.spd4.discard += lcore_stats->inbound.spd4.discard;

	total->inbound.sad.miss += lcore_stats->inbound.sad.miss;


	/* routing stats */
	total->lpm4.miss += lcore_stats->lpm4.miss;
	total->lpm6.miss += lcore_stats->lpm6.miss;
}

static void
update_statistics(struct ipsec_core_statistics *total, uint32_t coreid)
{
	memset(total, 0, sizeof(*total));

	if (coreid != UINT32_MAX) {
		update_lcore_statistics(total, coreid);
	} else {
		for (coreid = 0; coreid < RTE_MAX_LCORE; coreid++)
			update_lcore_statistics(total, coreid);
	}
}

static int
handle_telemetry_cmd_ipsec_secgw_stats_outbound(const char *cmd __rte_unused,
		const char *params, struct rte_tel_data *data)
{
	struct ipsec_core_statistics total_stats;

	struct rte_tel_data *spd4_data = rte_tel_data_alloc();
	struct rte_tel_data *spd6_data = rte_tel_data_alloc();
	struct rte_tel_data *sad_data = rte_tel_data_alloc();
	unsigned int coreid = UINT32_MAX;
	int rc = 0;

	/* verify allocated telemetry data structures */
	if (!spd4_data || !spd6_data || !sad_data) {
		rc = -ENOMEM;
		goto exit;
	}

	/* initialize telemetry data structs as dicts */
	rte_tel_data_start_dict(data);

	rte_tel_data_start_dict(spd4_data);
	rte_tel_data_start_dict(spd6_data);
	rte_tel_data_start_dict(sad_data);

	if (params) {
		coreid = (uint32_t)atoi(params);
		if (rte_lcore_is_enabled(coreid) == 0) {
			rc = -EINVAL;
			goto exit;
		}
	}

	update_statistics(&total_stats, coreid);

	/* add spd 4 telemetry key/values pairs */

	rte_tel_data_add_dict_u64(spd4_data, "protect",
		total_stats.outbound.spd4.protect);
	rte_tel_data_add_dict_u64(spd4_data, "bypass",
		total_stats.outbound.spd4.bypass);
	rte_tel_data_add_dict_u64(spd4_data, "discard",
		total_stats.outbound.spd4.discard);

	rte_tel_data_add_dict_container(data, "spd4", spd4_data, 0);

	/* add spd 6 telemetry key/values pairs */

	rte_tel_data_add_dict_u64(spd6_data, "protect",
		total_stats.outbound.spd6.protect);
	rte_tel_data_add_dict_u64(spd6_data, "bypass",
		total_stats.outbound.spd6.bypass);
	rte_tel_data_add_dict_u64(spd6_data, "discard",
		total_stats.outbound.spd6.discard);

	rte_tel_data_add_dict_container(data, "spd6", spd6_data, 0);

	/* add sad telemetry key/values pairs */

	rte_tel_data_add_dict_u64(sad_data, "miss",
		total_stats.outbound.sad.miss);

	rte_tel_data_add_dict_container(data, "sad", sad_data, 0);

exit:
	if (rc) {
		rte_tel_data_free(spd4_data);
		rte_tel_data_free(spd6_data);
		rte_tel_data_free(sad_data);
	}
	return rc;
}

static int
handle_telemetry_cmd_ipsec_secgw_stats_inbound(const char *cmd __rte_unused,
		const char *params, struct rte_tel_data *data)
{
	struct ipsec_core_statistics total_stats;

	struct rte_tel_data *spd4_data = rte_tel_data_alloc();
	struct rte_tel_data *spd6_data = rte_tel_data_alloc();
	struct rte_tel_data *sad_data = rte_tel_data_alloc();
	unsigned int coreid = UINT32_MAX;
	int rc = 0;

	/* verify allocated telemetry data structures */
	if (!spd4_data || !spd6_data || !sad_data) {
		rc = -ENOMEM;
		goto exit;
	}

	/* initialize telemetry data structs as dicts */
	rte_tel_data_start_dict(data);
	rte_tel_data_start_dict(spd4_data);
	rte_tel_data_start_dict(spd6_data);
	rte_tel_data_start_dict(sad_data);

	/* add children dicts to parent dict */

	if (params) {
		coreid = (uint32_t)atoi(params);
		if (rte_lcore_is_enabled(coreid) == 0) {
			rc = -EINVAL;
			goto exit;
		}
	}

	update_statistics(&total_stats, coreid);

	/* add sad telemetry key/values pairs */

	rte_tel_data_add_dict_u64(sad_data, "miss",
		total_stats.inbound.sad.miss);

	rte_tel_data_add_dict_container(data, "sad", sad_data, 0);

	/* add spd 4 telemetry key/values pairs */

	rte_tel_data_add_dict_u64(spd4_data, "protect",
		total_stats.inbound.spd4.protect);
	rte_tel_data_add_dict_u64(spd4_data, "bypass",
		total_stats.inbound.spd4.bypass);
	rte_tel_data_add_dict_u64(spd4_data, "discard",
		total_stats.inbound.spd4.discard);

	rte_tel_data_add_dict_container(data, "spd4", spd4_data, 0);

	/* add spd 6 telemetry key/values pairs */

	rte_tel_data_add_dict_u64(spd6_data, "protect",
		total_stats.inbound.spd6.protect);
	rte_tel_data_add_dict_u64(spd6_data, "bypass",
		total_stats.inbound.spd6.bypass);
	rte_tel_data_add_dict_u64(spd6_data, "discard",
		total_stats.inbound.spd6.discard);

	rte_tel_data_add_dict_container(data, "spd6", spd6_data, 0);

exit:
	if (rc) {
		rte_tel_data_free(spd4_data);
		rte_tel_data_free(spd6_data);
		rte_tel_data_free(sad_data);
	}
	return rc;
}

static int
handle_telemetry_cmd_ipsec_secgw_stats_routing(const char *cmd __rte_unused,
		const char *params, struct rte_tel_data *data)
{
	struct ipsec_core_statistics total_stats;

	struct rte_tel_data *lpm4_data = rte_tel_data_alloc();
	struct rte_tel_data *lpm6_data = rte_tel_data_alloc();
	unsigned int coreid = UINT32_MAX;
	int rc = 0;

	/* verify allocated telemetry data structures */
	if (!lpm4_data || !lpm6_data) {
		rc = -ENOMEM;
		goto exit;
	}

	/* initialize telemetry data structs as dicts */
	rte_tel_data_start_dict(data);
	rte_tel_data_start_dict(lpm4_data);
	rte_tel_data_start_dict(lpm6_data);


	if (params) {
		coreid = (uint32_t)atoi(params);
		if (rte_lcore_is_enabled(coreid) == 0) {
			rc = -EINVAL;
			goto exit;
		}
	}

	update_statistics(&total_stats, coreid);

	/* add lpm 4 telemetry key/values pairs */
	rte_tel_data_add_dict_u64(lpm4_data, "miss",
		total_stats.lpm4.miss);

	rte_tel_data_add_dict_container(data, "IPv4 LPM", lpm4_data, 0);

	/* add lpm 6 telemetry key/values pairs */
	rte_tel_data_add_dict_u64(lpm6_data, "miss",
		total_stats.lpm6.miss);

	rte_tel_data_add_dict_container(data, "IPv6 LPM", lpm6_data, 0);

exit:
	if (rc) {
		rte_tel_data_free(lpm4_data);
		rte_tel_data_free(lpm6_data);
	}
	return rc;
}

static void
ipsec_secgw_telemetry_init(void)
{
	rte_telemetry_register_cmd("/examples/ipsec-secgw/stats",
		handle_telemetry_cmd_ipsec_secgw_stats,
		"Returns global stats. "
		"Optional Parameters: int <logical core id>");

	rte_telemetry_register_cmd("/examples/ipsec-secgw/stats/outbound",
		handle_telemetry_cmd_ipsec_secgw_stats_outbound,
		"Returns outbound global stats. "
		"Optional Parameters: int <logical core id>");

	rte_telemetry_register_cmd("/examples/ipsec-secgw/stats/inbound",
		handle_telemetry_cmd_ipsec_secgw_stats_inbound,
		"Returns inbound global stats. "
		"Optional Parameters: int <logical core id>");

	rte_telemetry_register_cmd("/examples/ipsec-secgw/stats/routing",
		handle_telemetry_cmd_ipsec_secgw_stats_routing,
		"Returns routing stats. "
		"Optional Parameters: int <logical core id>");
}


int32_t
main(int32_t argc, char **argv)
{
	int32_t ret;
	uint32_t lcore_id, nb_txq, nb_rxq = 0;
	uint32_t cdev_id;
	uint32_t i;
	uint8_t socket_id;
	uint16_t portid, nb_crypto_qp, nb_ports = 0;
	uint64_t req_rx_offloads[RTE_MAX_ETHPORTS];
	uint64_t req_tx_offloads[RTE_MAX_ETHPORTS];
	struct eh_conf *eh_conf = NULL;
	size_t sess_sz;

	nb_bufs_in_pool = 0;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* initialize event helper configuration */
	eh_conf = eh_conf_init();
	if (eh_conf == NULL)
		rte_exit(EXIT_FAILURE, "Failed to init event helper config");

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv, eh_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid parameters\n");

	ipsec_secgw_telemetry_init();

	/* parse configuration file */
	if (parse_cfg_file(cfgfile) < 0) {
		printf("parsing file \"%s\" failed\n",
			optarg);
		print_usage(argv[0]);
		return -1;
	}

	if ((unprotected_port_mask & enabled_port_mask) !=
			unprotected_port_mask)
		rte_exit(EXIT_FAILURE, "Invalid unprotected portmask 0x%x\n",
				unprotected_port_mask);

	if (unprotected_port_mask && !nb_sa_in)
		rte_exit(EXIT_FAILURE, "Cannot use unprotected portmask without configured SA inbound\n");

	if (check_poll_mode_params(eh_conf) < 0)
		rte_exit(EXIT_FAILURE, "check_poll_mode_params failed\n");

	if (check_event_mode_params(eh_conf) < 0)
		rte_exit(EXIT_FAILURE, "check_event_mode_params failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");

	nb_lcores = rte_lcore_count();

	sess_sz = max_session_size();

	/*
	 * In event mode request minimum number of crypto queues
	 * to be reserved equal to number of ports.
	 */
	if (eh_conf->mode == EH_PKT_TRANSFER_MODE_EVENT)
		nb_crypto_qp = rte_eth_dev_count_avail();
	else
		nb_crypto_qp = 0;

	nb_crypto_qp = cryptodevs_init(nb_crypto_qp);

	if (nb_bufs_in_pool == 0) {
		RTE_ETH_FOREACH_DEV(portid) {
			if ((enabled_port_mask & (1 << portid)) == 0)
				continue;
			nb_ports++;
			nb_rxq += get_port_nb_rx_queues(portid);
		}

		nb_txq = nb_lcores;

		nb_bufs_in_pool = calculate_nb_mbufs(nb_ports, nb_crypto_qp,
						nb_rxq, nb_txq);
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socket_id = (uint8_t)rte_lcore_to_socket_id(lcore_id);
		else
			socket_id = 0;

		if (per_port_pool) {
			RTE_ETH_FOREACH_DEV(portid) {
				if ((enabled_port_mask & (1 << portid)) == 0)
					continue;

				pool_init(&socket_ctx[socket_id], socket_id,
					  portid, nb_bufs_in_pool);
			}
		} else {
			pool_init(&socket_ctx[socket_id], socket_id, 0,
				  nb_bufs_in_pool);
		}

		if (socket_ctx[socket_id].session_pool)
			continue;

		session_pool_init(&socket_ctx[socket_id], socket_id, sess_sz);
		session_priv_pool_init(&socket_ctx[socket_id], socket_id,
			sess_sz);
	}
	printf("Number of mbufs in packet pool %d\n", nb_bufs_in_pool);

	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		sa_check_offloads(portid, &req_rx_offloads[portid],
				&req_tx_offloads[portid]);
		port_init(portid, req_rx_offloads[portid],
				req_tx_offloads[portid]);
	}

	/*
	 * Set the enabled port mask in helper config for use by helper
	 * sub-system. This will be used while initializing devices using
	 * helper sub-system.
	 */
	eh_conf->eth_portmask = enabled_port_mask;

	/* Initialize eventmode components */
	ret = eh_devs_init(eh_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "eh_devs_init failed, err=%d\n", ret);

	/* start ports */
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: "
					"err=%d, port=%d\n", ret, portid);

		/* Create flow after starting the device */
		create_default_ipsec_flow(portid, req_rx_offloads[portid]);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on) {
			ret = rte_eth_promiscuous_enable(portid);
			if (ret != 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_promiscuous_enable: err=%s, port=%d\n",
					rte_strerror(-ret), portid);
		}

		rte_eth_dev_callback_register(portid, RTE_ETH_EVENT_INTR_RESET,
			ethdev_reset_event_callback, NULL);

		rte_eth_dev_callback_register(portid,
			RTE_ETH_EVENT_IPSEC, inline_ipsec_event_callback, NULL);
	}

	/* fragment reassemble is enabled */
	if (frag_tbl_sz != 0) {
		ret = reassemble_init();
		if (ret != 0)
			rte_exit(EXIT_FAILURE, "failed at reassemble init");
	}

	/* Replicate each context per socket */
	for (i = 0; i < NB_SOCKETS && i < rte_socket_count(); i++) {
		socket_id = rte_socket_id_by_idx(i);
		if ((socket_ctx[socket_id].session_pool != NULL) &&
			(socket_ctx[socket_id].sa_in == NULL) &&
			(socket_ctx[socket_id].sa_out == NULL)) {
			sa_init(&socket_ctx[socket_id], socket_id);
			sp4_init(&socket_ctx[socket_id], socket_id);
			sp6_init(&socket_ctx[socket_id], socket_id);
			rt_init(&socket_ctx[socket_id], socket_id);
		}
	}

	flow_init();

	check_all_ports_link_status(enabled_port_mask);

	if (stats_interval > 0)
		rte_eal_alarm_set(stats_interval * US_PER_S,
				print_stats_cb, NULL);
	else
		RTE_LOG(INFO, IPSEC, "Stats display disabled\n");

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(ipsec_launch_one_lcore, eh_conf, CALL_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	/* Uninitialize eventmode components */
	ret = eh_devs_uninit(eh_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "eh_devs_uninit failed, err=%d\n", ret);

	/* Free eventmode configuration memory */
	eh_conf_uninit(eh_conf);

	/* Destroy inline inbound and outbound sessions */
	for (i = 0; i < NB_SOCKETS && i < rte_socket_count(); i++) {
		socket_id = rte_socket_id_by_idx(i);
		inline_sessions_free(socket_ctx[socket_id].sa_in);
		inline_sessions_free(socket_ctx[socket_id].sa_out);
	}

	for (cdev_id = 0; cdev_id < rte_cryptodev_count(); cdev_id++) {
		printf("Closing cryptodev %d...", cdev_id);
		rte_cryptodev_stop(cdev_id);
		rte_cryptodev_close(cdev_id);
		printf(" Done\n");
	}

	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		printf("Closing port %d...", portid);
		if (flow_info_tbl[portid].rx_def_flow) {
			struct rte_flow_error err;

			ret = rte_flow_destroy(portid,
				flow_info_tbl[portid].rx_def_flow, &err);
			if (ret)
				RTE_LOG(ERR, IPSEC, "Failed to destroy flow "
					" for port %u, err msg: %s\n", portid,
					err.message);
		}
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			RTE_LOG(ERR, IPSEC,
				"rte_eth_dev_stop: err=%s, port=%u\n",
				rte_strerror(-ret), portid);

		rte_eth_dev_close(portid);
		printf(" Done\n");
	}

	/* clean up the EAL */
	rte_eal_cleanup();
	printf("Bye...\n");

	return 0;
}
