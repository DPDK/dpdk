/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_vect.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
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
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_pause.h>

#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>

#include <lthread_api.h>

#define APP_LOOKUP_EXACT_MATCH          0
#define APP_LOOKUP_LPM                  1
#define DO_RFC_1812_CHECKS

/* Enable cpu-load stats 0-off, 1-on */
#define APP_CPU_LOAD                 1

#ifndef APP_LOOKUP_METHOD
#define APP_LOOKUP_METHOD             APP_LOOKUP_LPM
#endif

#ifndef __GLIBC__ /* sched_getcpu() is glibc specific */
#define sched_getcpu() rte_lcore_id()
#endif

static int
check_ptype(int portid)
{
	int i, ret;
	int ipv4 = 0, ipv6 = 0;

	ret = rte_eth_dev_get_supported_ptypes(portid, RTE_PTYPE_L3_MASK, NULL,
			0);
	if (ret <= 0)
		return 0;

	uint32_t ptypes[ret];

	ret = rte_eth_dev_get_supported_ptypes(portid, RTE_PTYPE_L3_MASK,
			ptypes, ret);
	for (i = 0; i < ret; ++i) {
		if (ptypes[i] & RTE_PTYPE_L3_IPV4)
			ipv4 = 1;
		if (ptypes[i] & RTE_PTYPE_L3_IPV6)
			ipv6 = 1;
	}

	if (ipv4 && ipv6)
		return 1;

	return 0;
}

static inline void
parse_ptype(struct rte_mbuf *m)
{
	struct ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_type = eth_hdr->ether_type;
	if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))
		packet_type |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6))
		packet_type |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;

	m->packet_type = packet_type;
}

static uint16_t
cb_parse_ptype(__rte_unused uint16_t port, __rte_unused uint16_t queue,
		struct rte_mbuf *pkts[], uint16_t nb_pkts,
		__rte_unused uint16_t max_pkts, __rte_unused void *user_param)
{
	unsigned int i;

	for (i = 0; i < nb_pkts; i++)
		parse_ptype(pkts[i]);

	return nb_pkts;
}

/*
 *  When set to zero, simple forwaring path is eanbled.
 *  When set to one, optimized forwarding path is enabled.
 *  Note that LPM optimisation path uses SSE4.1 instructions.
 */
#define ENABLE_MULTI_BUFFER_OPTIMIZE	1

#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)
#include <rte_hash.h>
#elif (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)
#include <rte_lpm.h>
#include <rte_lpm6.h>
#else
#error "APP_LOOKUP_METHOD set to incorrect value"
#endif

#define RTE_LOGTYPE_L3FWD RTE_LOGTYPE_USER1

#define MAX_JUMBO_PKT_LEN  9600

#define IPV6_ADDR_LEN 16

#define MEMPOOL_CACHE_SIZE 256

/*
 * This expression is used to calculate the number of mbufs needed depending on
 * user input, taking into account memory for rx and tx hardware rings, cache
 * per lcore and mtable per port per lcore. RTE_MAX is used to ensure that
 * NB_MBUF never goes below a minimum value of 8192
 */

#define NB_MBUF RTE_MAX(\
		(nb_ports*nb_rx_queue*nb_rxd +      \
		nb_ports*nb_lcores*MAX_PKT_BURST +  \
		nb_ports*n_tx_queue*nb_txd +        \
		nb_lcores*MEMPOOL_CACHE_SIZE),      \
		(unsigned)8192)

#define MAX_PKT_BURST     32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Try to avoid TX buffering if we have at least MAX_TX_BURST packets to send.
 */
#define	MAX_TX_BURST  (MAX_PKT_BURST / 2)
#define BURST_SIZE    MAX_TX_BURST

#define NB_SOCKETS 8

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3

/* Used to mark destination port as 'invalid'. */
#define	BAD_PORT	((uint16_t)-1)

#define FWDSTEP	4

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static uint64_t dest_eth_addr[RTE_MAX_ETHPORTS];
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

static xmm_t val_eth[RTE_MAX_ETHPORTS];

/* replace first 12B of the ethernet header. */
#define	MASK_ETH 0x3f

/* mask of enabled ports */
static uint32_t enabled_port_mask;
static int promiscuous_on; /**< Set in promiscuous mode off by default. */
static int numa_on = 1;    /**< NUMA is enabled by default. */
static int parse_ptype_on;

#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)
static int ipv6;           /**< ipv6 is false by default. */
#endif

#if (APP_CPU_LOAD == 1)

#define MAX_CPU RTE_MAX_LCORE
#define CPU_LOAD_TIMEOUT_US (5 * 1000 * 1000)  /**< Timeout for collecting 5s */

#define CPU_PROCESS     0
#define CPU_POLL        1
#define MAX_CPU_COUNTER 2

struct cpu_load {
	uint16_t       n_cpu;
	uint64_t       counter;
	uint64_t       hits[MAX_CPU_COUNTER][MAX_CPU];
} __rte_cache_aligned;

static struct cpu_load cpu_load;
static int cpu_load_lcore_id = -1;

#define SET_CPU_BUSY(thread, counter) \
		thread->conf.busy[counter] = 1

#define SET_CPU_IDLE(thread, counter) \
		thread->conf.busy[counter] = 0

#define IS_CPU_BUSY(thread, counter) \
		(thread->conf.busy[counter] > 0)

#else

#define SET_CPU_BUSY(thread, counter)
#define SET_CPU_IDLE(thread, counter)
#define IS_CPU_BUSY(thread, counter) 0

#endif

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

struct lcore_rx_queue {
	uint16_t port_id;
	uint8_t queue_id;
} __rte_cache_aligned;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT  RTE_MAX_ETHPORTS
#define MAX_RX_QUEUE_PER_PORT  128

#define MAX_LCORE_PARAMS       1024
struct rx_thread_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
	uint8_t thread_id;
} __rte_cache_aligned;

static struct rx_thread_params rx_thread_params_array[MAX_LCORE_PARAMS];
static struct rx_thread_params rx_thread_params_array_default[] = {
	{0, 0, 2, 0},
	{0, 1, 2, 1},
	{0, 2, 2, 2},
	{1, 0, 2, 3},
	{1, 1, 2, 4},
	{1, 2, 2, 5},
	{2, 0, 2, 6},
	{3, 0, 3, 7},
	{3, 1, 3, 8},
};

static struct rx_thread_params *rx_thread_params =
		rx_thread_params_array_default;
static uint16_t nb_rx_thread_params = RTE_DIM(rx_thread_params_array_default);

struct tx_thread_params {
	uint8_t lcore_id;
	uint8_t thread_id;
} __rte_cache_aligned;

static struct tx_thread_params tx_thread_params_array[MAX_LCORE_PARAMS];
static struct tx_thread_params tx_thread_params_array_default[] = {
	{4, 0},
	{5, 1},
	{6, 2},
	{7, 3},
	{8, 4},
	{9, 5},
	{10, 6},
	{11, 7},
	{12, 8},
};

static struct tx_thread_params *tx_thread_params =
		tx_thread_params_array_default;
static uint16_t nb_tx_thread_params = RTE_DIM(tx_thread_params_array_default);

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.ignore_offload_bitfield = 1,
		.offloads = (DEV_RX_OFFLOAD_CHECKSUM |
			     DEV_RX_OFFLOAD_CRC_STRIP),
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_TCP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static struct rte_mempool *pktmbuf_pool[NB_SOCKETS];

#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)

#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc

struct ipv4_5tuple {
	uint32_t ip_dst;
	uint32_t ip_src;
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
} __attribute__((__packed__));

union ipv4_5tuple_host {
	struct {
		uint8_t  pad0;
		uint8_t  proto;
		uint16_t pad1;
		uint32_t ip_src;
		uint32_t ip_dst;
		uint16_t port_src;
		uint16_t port_dst;
	};
	__m128i xmm;
};

#define XMM_NUM_IN_IPV6_5TUPLE 3

struct ipv6_5tuple {
	uint8_t  ip_dst[IPV6_ADDR_LEN];
	uint8_t  ip_src[IPV6_ADDR_LEN];
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
} __attribute__((__packed__));

union ipv6_5tuple_host {
	struct {
		uint16_t pad0;
		uint8_t  proto;
		uint8_t  pad1;
		uint8_t  ip_src[IPV6_ADDR_LEN];
		uint8_t  ip_dst[IPV6_ADDR_LEN];
		uint16_t port_src;
		uint16_t port_dst;
		uint64_t reserve;
	};
	__m128i xmm[XMM_NUM_IN_IPV6_5TUPLE];
};

struct ipv4_l3fwd_route {
	struct ipv4_5tuple key;
	uint8_t if_out;
};

struct ipv6_l3fwd_route {
	struct ipv6_5tuple key;
	uint8_t if_out;
};

static struct ipv4_l3fwd_route ipv4_l3fwd_route_array[] = {
	{{IPv4(101, 0, 0, 0), IPv4(100, 10, 0, 1),  101, 11, IPPROTO_TCP}, 0},
	{{IPv4(201, 0, 0, 0), IPv4(200, 20, 0, 1),  102, 12, IPPROTO_TCP}, 1},
	{{IPv4(111, 0, 0, 0), IPv4(100, 30, 0, 1),  101, 11, IPPROTO_TCP}, 2},
	{{IPv4(211, 0, 0, 0), IPv4(200, 40, 0, 1),  102, 12, IPPROTO_TCP}, 3},
};

static struct ipv6_l3fwd_route ipv6_l3fwd_route_array[] = {
	{{
	{0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38,
			0x05},
	101, 11, IPPROTO_TCP}, 0},

	{{
	{0xfe, 0x90, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0x90, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38,
			0x05},
	102, 12, IPPROTO_TCP}, 1},

	{{
	{0xfe, 0xa0, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0xa0, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38,
			0x05},
	101, 11, IPPROTO_TCP}, 2},

	{{
	{0xfe, 0xb0, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0xb0, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38,
			0x05},
	102, 12, IPPROTO_TCP}, 3},
};

typedef struct rte_hash lookup_struct_t;
static lookup_struct_t *ipv4_l3fwd_lookup_struct[NB_SOCKETS];
static lookup_struct_t *ipv6_l3fwd_lookup_struct[NB_SOCKETS];

#ifdef RTE_ARCH_X86_64
/* default to 4 million hash entries (approx) */
#define L3FWD_HASH_ENTRIES (1024*1024*4)
#else
/* 32-bit has less address-space for hugepage memory, limit to 1M entries */
#define L3FWD_HASH_ENTRIES (1024*1024*1)
#endif
#define HASH_ENTRY_NUMBER_DEFAULT 4

static uint32_t hash_entry_number = HASH_ENTRY_NUMBER_DEFAULT;

static inline uint32_t
ipv4_hash_crc(const void *data, __rte_unused uint32_t data_len,
		uint32_t init_val)
{
	const union ipv4_5tuple_host *k;
	uint32_t t;
	const uint32_t *p;

	k = data;
	t = k->proto;
	p = (const uint32_t *)&k->port_src;

	init_val = rte_hash_crc_4byte(t, init_val);
	init_val = rte_hash_crc_4byte(k->ip_src, init_val);
	init_val = rte_hash_crc_4byte(k->ip_dst, init_val);
	init_val = rte_hash_crc_4byte(*p, init_val);
	return init_val;
}

static inline uint32_t
ipv6_hash_crc(const void *data, __rte_unused uint32_t data_len,
		uint32_t init_val)
{
	const union ipv6_5tuple_host *k;
	uint32_t t;
	const uint32_t *p;
	const uint32_t *ip_src0, *ip_src1, *ip_src2, *ip_src3;
	const uint32_t *ip_dst0, *ip_dst1, *ip_dst2, *ip_dst3;

	k = data;
	t = k->proto;
	p = (const uint32_t *)&k->port_src;

	ip_src0 = (const uint32_t *) k->ip_src;
	ip_src1 = (const uint32_t *)(k->ip_src + 4);
	ip_src2 = (const uint32_t *)(k->ip_src + 8);
	ip_src3 = (const uint32_t *)(k->ip_src + 12);
	ip_dst0 = (const uint32_t *) k->ip_dst;
	ip_dst1 = (const uint32_t *)(k->ip_dst + 4);
	ip_dst2 = (const uint32_t *)(k->ip_dst + 8);
	ip_dst3 = (const uint32_t *)(k->ip_dst + 12);
	init_val = rte_hash_crc_4byte(t, init_val);
	init_val = rte_hash_crc_4byte(*ip_src0, init_val);
	init_val = rte_hash_crc_4byte(*ip_src1, init_val);
	init_val = rte_hash_crc_4byte(*ip_src2, init_val);
	init_val = rte_hash_crc_4byte(*ip_src3, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst0, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst1, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst2, init_val);
	init_val = rte_hash_crc_4byte(*ip_dst3, init_val);
	init_val = rte_hash_crc_4byte(*p, init_val);
	return init_val;
}

#define IPV4_L3FWD_NUM_ROUTES RTE_DIM(ipv4_l3fwd_route_array)
#define IPV6_L3FWD_NUM_ROUTES RTE_DIM(ipv6_l3fwd_route_array)

static uint8_t ipv4_l3fwd_out_if[L3FWD_HASH_ENTRIES] __rte_cache_aligned;
static uint8_t ipv6_l3fwd_out_if[L3FWD_HASH_ENTRIES] __rte_cache_aligned;

#endif

#if (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)
struct ipv4_l3fwd_route {
	uint32_t ip;
	uint8_t  depth;
	uint8_t  if_out;
};

struct ipv6_l3fwd_route {
	uint8_t ip[16];
	uint8_t depth;
	uint8_t if_out;
};

static struct ipv4_l3fwd_route ipv4_l3fwd_route_array[] = {
	{IPv4(1, 1, 1, 0), 24, 0},
	{IPv4(2, 1, 1, 0), 24, 1},
	{IPv4(3, 1, 1, 0), 24, 2},
	{IPv4(4, 1, 1, 0), 24, 3},
	{IPv4(5, 1, 1, 0), 24, 4},
	{IPv4(6, 1, 1, 0), 24, 5},
	{IPv4(7, 1, 1, 0), 24, 6},
	{IPv4(8, 1, 1, 0), 24, 7},
};

static struct ipv6_l3fwd_route ipv6_l3fwd_route_array[] = {
	{{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 0},
	{{2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 1},
	{{3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 2},
	{{4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 3},
	{{5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 4},
	{{6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 5},
	{{7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 6},
	{{8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 7},
};

#define IPV4_L3FWD_NUM_ROUTES RTE_DIM(ipv4_l3fwd_route_array)
#define IPV6_L3FWD_NUM_ROUTES RTE_DIM(ipv6_l3fwd_route_array)

#define IPV4_L3FWD_LPM_MAX_RULES         1024
#define IPV6_L3FWD_LPM_MAX_RULES         1024
#define IPV6_L3FWD_LPM_NUMBER_TBL8S (1 << 16)

typedef struct rte_lpm lookup_struct_t;
typedef struct rte_lpm6 lookup6_struct_t;
static lookup_struct_t *ipv4_l3fwd_lookup_struct[NB_SOCKETS];
static lookup6_struct_t *ipv6_l3fwd_lookup_struct[NB_SOCKETS];
#endif

struct lcore_conf {
	lookup_struct_t *ipv4_lookup_struct;
#if (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)
	lookup6_struct_t *ipv6_lookup_struct;
#else
	lookup_struct_t *ipv6_lookup_struct;
#endif
	void *data;
} __rte_cache_aligned;

static struct lcore_conf lcore_conf[RTE_MAX_LCORE];
RTE_DEFINE_PER_LCORE(struct lcore_conf *, lcore_conf);

#define MAX_RX_QUEUE_PER_THREAD 16
#define MAX_TX_PORT_PER_THREAD  RTE_MAX_ETHPORTS
#define MAX_TX_QUEUE_PER_PORT   RTE_MAX_ETHPORTS
#define MAX_RX_QUEUE_PER_PORT   128

#define MAX_RX_THREAD 1024
#define MAX_TX_THREAD 1024
#define MAX_THREAD    (MAX_RX_THREAD + MAX_TX_THREAD)

/**
 * Producers and consumers threads configuration
 */
static int lthreads_on = 1; /**< Use lthreads for processing*/

rte_atomic16_t rx_counter;  /**< Number of spawned rx threads */
rte_atomic16_t tx_counter;  /**< Number of spawned tx threads */

struct thread_conf {
	uint16_t lcore_id;      /**< Initial lcore for rx thread */
	uint16_t cpu_id;        /**< Cpu id for cpu load stats counter */
	uint16_t thread_id;     /**< Thread ID */

#if (APP_CPU_LOAD > 0)
	int busy[MAX_CPU_COUNTER];
#endif
};

struct thread_rx_conf {
	struct thread_conf conf;

	uint16_t n_rx_queue;
	struct lcore_rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];

	uint16_t n_ring;        /**< Number of output rings */
	struct rte_ring *ring[RTE_MAX_LCORE];
	struct lthread_cond *ready[RTE_MAX_LCORE];

#if (APP_CPU_LOAD > 0)
	int busy[MAX_CPU_COUNTER];
#endif
} __rte_cache_aligned;

uint16_t n_rx_thread;
struct thread_rx_conf rx_thread[MAX_RX_THREAD];

struct thread_tx_conf {
	struct thread_conf conf;

	uint16_t tx_queue_id[RTE_MAX_LCORE];
	struct mbuf_table tx_mbufs[RTE_MAX_LCORE];

	struct rte_ring *ring;
	struct lthread_cond **ready;

} __rte_cache_aligned;

uint16_t n_tx_thread;
struct thread_tx_conf tx_thread[MAX_TX_THREAD];

/* Send burst of packets on an output interface */
static inline int
send_burst(struct thread_tx_conf *qconf, uint16_t n, uint16_t port)
{
	struct rte_mbuf **m_table;
	int ret;
	uint16_t queueid;

	queueid = qconf->tx_queue_id[port];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

	ret = rte_eth_tx_burst(port, queueid, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	return 0;
}

/* Enqueue a single packet, and send burst if queue is filled */
static inline int
send_single_packet(struct rte_mbuf *m, uint16_t port)
{
	uint16_t len;
	struct thread_tx_conf *qconf;

	if (lthreads_on)
		qconf = (struct thread_tx_conf *)lthread_get_data();
	else
		qconf = (struct thread_tx_conf *)RTE_PER_LCORE(lcore_conf)->data;

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

#if ((APP_LOOKUP_METHOD == APP_LOOKUP_LPM) && \
	(ENABLE_MULTI_BUFFER_OPTIMIZE == 1))
static __rte_always_inline void
send_packetsx4(uint16_t port,
	struct rte_mbuf *m[], uint32_t num)
{
	uint32_t len, j, n;
	struct thread_tx_conf *qconf;

	if (lthreads_on)
		qconf = (struct thread_tx_conf *)lthread_get_data();
	else
		qconf = (struct thread_tx_conf *)RTE_PER_LCORE(lcore_conf)->data;

	len = qconf->tx_mbufs[port].len;

	/*
	 * If TX buffer for that queue is empty, and we have enough packets,
	 * then send them straightway.
	 */
	if (num >= MAX_TX_BURST && len == 0) {
		n = rte_eth_tx_burst(port, qconf->tx_queue_id[port], m, num);
		if (unlikely(n < num)) {
			do {
				rte_pktmbuf_free(m[n]);
			} while (++n < num);
		}
		return;
	}

	/*
	 * Put packets into TX buffer for that queue.
	 */

	n = len + num;
	n = (n > MAX_PKT_BURST) ? MAX_PKT_BURST - len : num;

	j = 0;
	switch (n % FWDSTEP) {
	while (j < n) {
	case 0:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
		/* fall-through */
	case 3:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
		/* fall-through */
	case 2:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
		/* fall-through */
	case 1:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
	}
	}

	len += n;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {

		send_burst(qconf, MAX_PKT_BURST, port);

		/* copy rest of the packets into the TX buffer. */
		len = num - n;
		j = 0;
		switch (len % FWDSTEP) {
		while (j < len) {
		case 0:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
			/* fall-through */
		case 3:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
			/* fall-through */
		case 2:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
			/* fall-through */
		case 1:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
		}
		}
	}

	qconf->tx_mbufs[port].len = len;
}
#endif /* APP_LOOKUP_LPM */

#ifdef DO_RFC_1812_CHECKS
static inline int
is_valid_ipv4_pkt(struct ipv4_hdr *pkt, uint32_t link_len)
{
	/* From http://www.rfc-editor.org/rfc/rfc1812.txt section 5.2.2 */
	/*
	 * 1. The packet length reported by the Link Layer must be large
	 * enough to hold the minimum length legal IP datagram (20 bytes).
	 */
	if (link_len < sizeof(struct ipv4_hdr))
		return -1;

	/* 2. The IP checksum must be correct. */
	/* this is checked in H/W */

	/*
	 * 3. The IP version number must be 4. If the version number is not 4
	 * then the packet may be another version of IP, such as IPng or
	 * ST-II.
	 */
	if (((pkt->version_ihl) >> 4) != 4)
		return -3;
	/*
	 * 4. The IP header length field must be large enough to hold the
	 * minimum length legal IP datagram (20 bytes = 5 words).
	 */
	if ((pkt->version_ihl & 0xf) < 5)
		return -4;

	/*
	 * 5. The IP total length field must be large enough to hold the IP
	 * datagram header, whose length is specified in the IP header length
	 * field.
	 */
	if (rte_cpu_to_be_16(pkt->total_length) < sizeof(struct ipv4_hdr))
		return -5;

	return 0;
}
#endif

#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)

static __m128i mask0;
static __m128i mask1;
static __m128i mask2;
static inline uint16_t
get_ipv4_dst_port(void *ipv4_hdr, uint16_t portid,
		lookup_struct_t *ipv4_l3fwd_lookup_struct)
{
	int ret = 0;
	union ipv4_5tuple_host key;

	ipv4_hdr = (uint8_t *)ipv4_hdr + offsetof(struct ipv4_hdr, time_to_live);
	__m128i data = _mm_loadu_si128((__m128i *)(ipv4_hdr));
	/* Get 5 tuple: dst port, src port, dst IP address, src IP address and
	   protocol */
	key.xmm = _mm_and_si128(data, mask0);
	/* Find destination port */
	ret = rte_hash_lookup(ipv4_l3fwd_lookup_struct, (const void *)&key);
	return ((ret < 0) ? portid : ipv4_l3fwd_out_if[ret]);
}

static inline uint16_t
get_ipv6_dst_port(void *ipv6_hdr, uint16_t portid,
		lookup_struct_t *ipv6_l3fwd_lookup_struct)
{
	int ret = 0;
	union ipv6_5tuple_host key;

	ipv6_hdr = (uint8_t *)ipv6_hdr + offsetof(struct ipv6_hdr, payload_len);
	__m128i data0 = _mm_loadu_si128((__m128i *)(ipv6_hdr));
	__m128i data1 = _mm_loadu_si128((__m128i *)(((uint8_t *)ipv6_hdr) +
			sizeof(__m128i)));
	__m128i data2 = _mm_loadu_si128((__m128i *)(((uint8_t *)ipv6_hdr) +
			sizeof(__m128i) + sizeof(__m128i)));
	/* Get part of 5 tuple: src IP address lower 96 bits and protocol */
	key.xmm[0] = _mm_and_si128(data0, mask1);
	/* Get part of 5 tuple: dst IP address lower 96 bits and src IP address
	   higher 32 bits */
	key.xmm[1] = data1;
	/* Get part of 5 tuple: dst port and src port and dst IP address higher
	   32 bits */
	key.xmm[2] = _mm_and_si128(data2, mask2);

	/* Find destination port */
	ret = rte_hash_lookup(ipv6_l3fwd_lookup_struct, (const void *)&key);
	return ((ret < 0) ? portid : ipv6_l3fwd_out_if[ret]);
}
#endif

#if (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)

static inline uint16_t
get_ipv4_dst_port(void *ipv4_hdr, uint16_t portid,
		lookup_struct_t *ipv4_l3fwd_lookup_struct)
{
	uint32_t next_hop;

	return ((rte_lpm_lookup(ipv4_l3fwd_lookup_struct,
		rte_be_to_cpu_32(((struct ipv4_hdr *)ipv4_hdr)->dst_addr),
		&next_hop) == 0) ? next_hop : portid);
}

static inline uint16_t
get_ipv6_dst_port(void *ipv6_hdr,  uint16_t portid,
		lookup6_struct_t *ipv6_l3fwd_lookup_struct)
{
	uint32_t next_hop;

	return ((rte_lpm6_lookup(ipv6_l3fwd_lookup_struct,
			((struct ipv6_hdr *)ipv6_hdr)->dst_addr, &next_hop) == 0) ?
			next_hop : portid);
}
#endif

static inline void l3fwd_simple_forward(struct rte_mbuf *m, uint16_t portid)
		__attribute__((unused));

#if ((APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH) && \
	(ENABLE_MULTI_BUFFER_OPTIMIZE == 1))

#define MASK_ALL_PKTS   0xff
#define EXCLUDE_1ST_PKT 0xfe
#define EXCLUDE_2ND_PKT 0xfd
#define EXCLUDE_3RD_PKT 0xfb
#define EXCLUDE_4TH_PKT 0xf7
#define EXCLUDE_5TH_PKT 0xef
#define EXCLUDE_6TH_PKT 0xdf
#define EXCLUDE_7TH_PKT 0xbf
#define EXCLUDE_8TH_PKT 0x7f

static inline void
simple_ipv4_fwd_8pkts(struct rte_mbuf *m[8], uint16_t portid)
{
	struct ether_hdr *eth_hdr[8];
	struct ipv4_hdr *ipv4_hdr[8];
	uint16_t dst_port[8];
	int32_t ret[8];
	union ipv4_5tuple_host key[8];
	__m128i data[8];

	eth_hdr[0] = rte_pktmbuf_mtod(m[0], struct ether_hdr *);
	eth_hdr[1] = rte_pktmbuf_mtod(m[1], struct ether_hdr *);
	eth_hdr[2] = rte_pktmbuf_mtod(m[2], struct ether_hdr *);
	eth_hdr[3] = rte_pktmbuf_mtod(m[3], struct ether_hdr *);
	eth_hdr[4] = rte_pktmbuf_mtod(m[4], struct ether_hdr *);
	eth_hdr[5] = rte_pktmbuf_mtod(m[5], struct ether_hdr *);
	eth_hdr[6] = rte_pktmbuf_mtod(m[6], struct ether_hdr *);
	eth_hdr[7] = rte_pktmbuf_mtod(m[7], struct ether_hdr *);

	/* Handle IPv4 headers.*/
	ipv4_hdr[0] = rte_pktmbuf_mtod_offset(m[0], struct ipv4_hdr *,
			sizeof(struct ether_hdr));
	ipv4_hdr[1] = rte_pktmbuf_mtod_offset(m[1], struct ipv4_hdr *,
			sizeof(struct ether_hdr));
	ipv4_hdr[2] = rte_pktmbuf_mtod_offset(m[2], struct ipv4_hdr *,
			sizeof(struct ether_hdr));
	ipv4_hdr[3] = rte_pktmbuf_mtod_offset(m[3], struct ipv4_hdr *,
			sizeof(struct ether_hdr));
	ipv4_hdr[4] = rte_pktmbuf_mtod_offset(m[4], struct ipv4_hdr *,
			sizeof(struct ether_hdr));
	ipv4_hdr[5] = rte_pktmbuf_mtod_offset(m[5], struct ipv4_hdr *,
			sizeof(struct ether_hdr));
	ipv4_hdr[6] = rte_pktmbuf_mtod_offset(m[6], struct ipv4_hdr *,
			sizeof(struct ether_hdr));
	ipv4_hdr[7] = rte_pktmbuf_mtod_offset(m[7], struct ipv4_hdr *,
			sizeof(struct ether_hdr));

#ifdef DO_RFC_1812_CHECKS
	/* Check to make sure the packet is valid (RFC1812) */
	uint8_t valid_mask = MASK_ALL_PKTS;

	if (is_valid_ipv4_pkt(ipv4_hdr[0], m[0]->pkt_len) < 0) {
		rte_pktmbuf_free(m[0]);
		valid_mask &= EXCLUDE_1ST_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[1], m[1]->pkt_len) < 0) {
		rte_pktmbuf_free(m[1]);
		valid_mask &= EXCLUDE_2ND_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[2], m[2]->pkt_len) < 0) {
		rte_pktmbuf_free(m[2]);
		valid_mask &= EXCLUDE_3RD_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[3], m[3]->pkt_len) < 0) {
		rte_pktmbuf_free(m[3]);
		valid_mask &= EXCLUDE_4TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[4], m[4]->pkt_len) < 0) {
		rte_pktmbuf_free(m[4]);
		valid_mask &= EXCLUDE_5TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[5], m[5]->pkt_len) < 0) {
		rte_pktmbuf_free(m[5]);
		valid_mask &= EXCLUDE_6TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[6], m[6]->pkt_len) < 0) {
		rte_pktmbuf_free(m[6]);
		valid_mask &= EXCLUDE_7TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[7], m[7]->pkt_len) < 0) {
		rte_pktmbuf_free(m[7]);
		valid_mask &= EXCLUDE_8TH_PKT;
	}
	if (unlikely(valid_mask != MASK_ALL_PKTS)) {
		if (valid_mask == 0)
			return;

		uint8_t i = 0;

		for (i = 0; i < 8; i++)
			if ((0x1 << i) & valid_mask)
				l3fwd_simple_forward(m[i], portid);
	}
#endif /* End of #ifdef DO_RFC_1812_CHECKS */

	data[0] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[0], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));
	data[1] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[1], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));
	data[2] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[2], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));
	data[3] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[3], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));
	data[4] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[4], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));
	data[5] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[5], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));
	data[6] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[6], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));
	data[7] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[7], __m128i *,
			sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, time_to_live)));

	key[0].xmm = _mm_and_si128(data[0], mask0);
	key[1].xmm = _mm_and_si128(data[1], mask0);
	key[2].xmm = _mm_and_si128(data[2], mask0);
	key[3].xmm = _mm_and_si128(data[3], mask0);
	key[4].xmm = _mm_and_si128(data[4], mask0);
	key[5].xmm = _mm_and_si128(data[5], mask0);
	key[6].xmm = _mm_and_si128(data[6], mask0);
	key[7].xmm = _mm_and_si128(data[7], mask0);

	const void *key_array[8] = {&key[0], &key[1], &key[2], &key[3],
			&key[4], &key[5], &key[6], &key[7]};

	rte_hash_lookup_bulk(RTE_PER_LCORE(lcore_conf)->ipv4_lookup_struct,
			&key_array[0], 8, ret);
	dst_port[0] = ((ret[0] < 0) ? portid : ipv4_l3fwd_out_if[ret[0]]);
	dst_port[1] = ((ret[1] < 0) ? portid : ipv4_l3fwd_out_if[ret[1]]);
	dst_port[2] = ((ret[2] < 0) ? portid : ipv4_l3fwd_out_if[ret[2]]);
	dst_port[3] = ((ret[3] < 0) ? portid : ipv4_l3fwd_out_if[ret[3]]);
	dst_port[4] = ((ret[4] < 0) ? portid : ipv4_l3fwd_out_if[ret[4]]);
	dst_port[5] = ((ret[5] < 0) ? portid : ipv4_l3fwd_out_if[ret[5]]);
	dst_port[6] = ((ret[6] < 0) ? portid : ipv4_l3fwd_out_if[ret[6]]);
	dst_port[7] = ((ret[7] < 0) ? portid : ipv4_l3fwd_out_if[ret[7]]);

	if (dst_port[0] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[0]) == 0)
		dst_port[0] = portid;
	if (dst_port[1] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[1]) == 0)
		dst_port[1] = portid;
	if (dst_port[2] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[2]) == 0)
		dst_port[2] = portid;
	if (dst_port[3] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[3]) == 0)
		dst_port[3] = portid;
	if (dst_port[4] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[4]) == 0)
		dst_port[4] = portid;
	if (dst_port[5] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[5]) == 0)
		dst_port[5] = portid;
	if (dst_port[6] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[6]) == 0)
		dst_port[6] = portid;
	if (dst_port[7] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[7]) == 0)
		dst_port[7] = portid;

#ifdef DO_RFC_1812_CHECKS
	/* Update time to live and header checksum */
	--(ipv4_hdr[0]->time_to_live);
	--(ipv4_hdr[1]->time_to_live);
	--(ipv4_hdr[2]->time_to_live);
	--(ipv4_hdr[3]->time_to_live);
	++(ipv4_hdr[0]->hdr_checksum);
	++(ipv4_hdr[1]->hdr_checksum);
	++(ipv4_hdr[2]->hdr_checksum);
	++(ipv4_hdr[3]->hdr_checksum);
	--(ipv4_hdr[4]->time_to_live);
	--(ipv4_hdr[5]->time_to_live);
	--(ipv4_hdr[6]->time_to_live);
	--(ipv4_hdr[7]->time_to_live);
	++(ipv4_hdr[4]->hdr_checksum);
	++(ipv4_hdr[5]->hdr_checksum);
	++(ipv4_hdr[6]->hdr_checksum);
	++(ipv4_hdr[7]->hdr_checksum);
#endif

	/* dst addr */
	*(uint64_t *)&eth_hdr[0]->d_addr = dest_eth_addr[dst_port[0]];
	*(uint64_t *)&eth_hdr[1]->d_addr = dest_eth_addr[dst_port[1]];
	*(uint64_t *)&eth_hdr[2]->d_addr = dest_eth_addr[dst_port[2]];
	*(uint64_t *)&eth_hdr[3]->d_addr = dest_eth_addr[dst_port[3]];
	*(uint64_t *)&eth_hdr[4]->d_addr = dest_eth_addr[dst_port[4]];
	*(uint64_t *)&eth_hdr[5]->d_addr = dest_eth_addr[dst_port[5]];
	*(uint64_t *)&eth_hdr[6]->d_addr = dest_eth_addr[dst_port[6]];
	*(uint64_t *)&eth_hdr[7]->d_addr = dest_eth_addr[dst_port[7]];

	/* src addr */
	ether_addr_copy(&ports_eth_addr[dst_port[0]], &eth_hdr[0]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[1]], &eth_hdr[1]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[2]], &eth_hdr[2]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[3]], &eth_hdr[3]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[4]], &eth_hdr[4]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[5]], &eth_hdr[5]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[6]], &eth_hdr[6]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[7]], &eth_hdr[7]->s_addr);

	send_single_packet(m[0], (uint8_t)dst_port[0]);
	send_single_packet(m[1], (uint8_t)dst_port[1]);
	send_single_packet(m[2], (uint8_t)dst_port[2]);
	send_single_packet(m[3], (uint8_t)dst_port[3]);
	send_single_packet(m[4], (uint8_t)dst_port[4]);
	send_single_packet(m[5], (uint8_t)dst_port[5]);
	send_single_packet(m[6], (uint8_t)dst_port[6]);
	send_single_packet(m[7], (uint8_t)dst_port[7]);

}

static inline void get_ipv6_5tuple(struct rte_mbuf *m0, __m128i mask0,
		__m128i mask1, union ipv6_5tuple_host *key)
{
	__m128i tmpdata0 = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m0,
			__m128i *, sizeof(struct ether_hdr) +
			offsetof(struct ipv6_hdr, payload_len)));
	__m128i tmpdata1 = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m0,
			__m128i *, sizeof(struct ether_hdr) +
			offsetof(struct ipv6_hdr, payload_len) + sizeof(__m128i)));
	__m128i tmpdata2 = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m0,
			__m128i *, sizeof(struct ether_hdr) +
			offsetof(struct ipv6_hdr, payload_len) + sizeof(__m128i) +
			sizeof(__m128i)));
	key->xmm[0] = _mm_and_si128(tmpdata0, mask0);
	key->xmm[1] = tmpdata1;
	key->xmm[2] = _mm_and_si128(tmpdata2, mask1);
}

static inline void
simple_ipv6_fwd_8pkts(struct rte_mbuf *m[8], uint16_t portid)
{
	int32_t ret[8];
	uint16_t dst_port[8];
	struct ether_hdr *eth_hdr[8];
	union ipv6_5tuple_host key[8];

	__attribute__((unused)) struct ipv6_hdr *ipv6_hdr[8];

	eth_hdr[0] = rte_pktmbuf_mtod(m[0], struct ether_hdr *);
	eth_hdr[1] = rte_pktmbuf_mtod(m[1], struct ether_hdr *);
	eth_hdr[2] = rte_pktmbuf_mtod(m[2], struct ether_hdr *);
	eth_hdr[3] = rte_pktmbuf_mtod(m[3], struct ether_hdr *);
	eth_hdr[4] = rte_pktmbuf_mtod(m[4], struct ether_hdr *);
	eth_hdr[5] = rte_pktmbuf_mtod(m[5], struct ether_hdr *);
	eth_hdr[6] = rte_pktmbuf_mtod(m[6], struct ether_hdr *);
	eth_hdr[7] = rte_pktmbuf_mtod(m[7], struct ether_hdr *);

	/* Handle IPv6 headers.*/
	ipv6_hdr[0] = rte_pktmbuf_mtod_offset(m[0], struct ipv6_hdr *,
			sizeof(struct ether_hdr));
	ipv6_hdr[1] = rte_pktmbuf_mtod_offset(m[1], struct ipv6_hdr *,
			sizeof(struct ether_hdr));
	ipv6_hdr[2] = rte_pktmbuf_mtod_offset(m[2], struct ipv6_hdr *,
			sizeof(struct ether_hdr));
	ipv6_hdr[3] = rte_pktmbuf_mtod_offset(m[3], struct ipv6_hdr *,
			sizeof(struct ether_hdr));
	ipv6_hdr[4] = rte_pktmbuf_mtod_offset(m[4], struct ipv6_hdr *,
			sizeof(struct ether_hdr));
	ipv6_hdr[5] = rte_pktmbuf_mtod_offset(m[5], struct ipv6_hdr *,
			sizeof(struct ether_hdr));
	ipv6_hdr[6] = rte_pktmbuf_mtod_offset(m[6], struct ipv6_hdr *,
			sizeof(struct ether_hdr));
	ipv6_hdr[7] = rte_pktmbuf_mtod_offset(m[7], struct ipv6_hdr *,
			sizeof(struct ether_hdr));

	get_ipv6_5tuple(m[0], mask1, mask2, &key[0]);
	get_ipv6_5tuple(m[1], mask1, mask2, &key[1]);
	get_ipv6_5tuple(m[2], mask1, mask2, &key[2]);
	get_ipv6_5tuple(m[3], mask1, mask2, &key[3]);
	get_ipv6_5tuple(m[4], mask1, mask2, &key[4]);
	get_ipv6_5tuple(m[5], mask1, mask2, &key[5]);
	get_ipv6_5tuple(m[6], mask1, mask2, &key[6]);
	get_ipv6_5tuple(m[7], mask1, mask2, &key[7]);

	const void *key_array[8] = {&key[0], &key[1], &key[2], &key[3],
			&key[4], &key[5], &key[6], &key[7]};

	rte_hash_lookup_bulk(RTE_PER_LCORE(lcore_conf)->ipv6_lookup_struct,
			&key_array[0], 4, ret);
	dst_port[0] = ((ret[0] < 0) ? portid : ipv6_l3fwd_out_if[ret[0]]);
	dst_port[1] = ((ret[1] < 0) ? portid : ipv6_l3fwd_out_if[ret[1]]);
	dst_port[2] = ((ret[2] < 0) ? portid : ipv6_l3fwd_out_if[ret[2]]);
	dst_port[3] = ((ret[3] < 0) ? portid : ipv6_l3fwd_out_if[ret[3]]);
	dst_port[4] = ((ret[4] < 0) ? portid : ipv6_l3fwd_out_if[ret[4]]);
	dst_port[5] = ((ret[5] < 0) ? portid : ipv6_l3fwd_out_if[ret[5]]);
	dst_port[6] = ((ret[6] < 0) ? portid : ipv6_l3fwd_out_if[ret[6]]);
	dst_port[7] = ((ret[7] < 0) ? portid : ipv6_l3fwd_out_if[ret[7]]);

	if (dst_port[0] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[0]) == 0)
		dst_port[0] = portid;
	if (dst_port[1] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[1]) == 0)
		dst_port[1] = portid;
	if (dst_port[2] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[2]) == 0)
		dst_port[2] = portid;
	if (dst_port[3] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[3]) == 0)
		dst_port[3] = portid;
	if (dst_port[4] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[4]) == 0)
		dst_port[4] = portid;
	if (dst_port[5] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[5]) == 0)
		dst_port[5] = portid;
	if (dst_port[6] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[6]) == 0)
		dst_port[6] = portid;
	if (dst_port[7] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[7]) == 0)
		dst_port[7] = portid;

	/* dst addr */
	*(uint64_t *)&eth_hdr[0]->d_addr = dest_eth_addr[dst_port[0]];
	*(uint64_t *)&eth_hdr[1]->d_addr = dest_eth_addr[dst_port[1]];
	*(uint64_t *)&eth_hdr[2]->d_addr = dest_eth_addr[dst_port[2]];
	*(uint64_t *)&eth_hdr[3]->d_addr = dest_eth_addr[dst_port[3]];
	*(uint64_t *)&eth_hdr[4]->d_addr = dest_eth_addr[dst_port[4]];
	*(uint64_t *)&eth_hdr[5]->d_addr = dest_eth_addr[dst_port[5]];
	*(uint64_t *)&eth_hdr[6]->d_addr = dest_eth_addr[dst_port[6]];
	*(uint64_t *)&eth_hdr[7]->d_addr = dest_eth_addr[dst_port[7]];

	/* src addr */
	ether_addr_copy(&ports_eth_addr[dst_port[0]], &eth_hdr[0]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[1]], &eth_hdr[1]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[2]], &eth_hdr[2]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[3]], &eth_hdr[3]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[4]], &eth_hdr[4]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[5]], &eth_hdr[5]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[6]], &eth_hdr[6]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[7]], &eth_hdr[7]->s_addr);

	send_single_packet(m[0], dst_port[0]);
	send_single_packet(m[1], dst_port[1]);
	send_single_packet(m[2], dst_port[2]);
	send_single_packet(m[3], dst_port[3]);
	send_single_packet(m[4], dst_port[4]);
	send_single_packet(m[5], dst_port[5]);
	send_single_packet(m[6], dst_port[6]);
	send_single_packet(m[7], dst_port[7]);

}
#endif /* APP_LOOKUP_METHOD */

static __rte_always_inline void
l3fwd_simple_forward(struct rte_mbuf *m, uint16_t portid)
{
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	uint16_t dst_port;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	if (RTE_ETH_IS_IPV4_HDR(m->packet_type)) {
		/* Handle IPv4 headers.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
				sizeof(struct ether_hdr));

#ifdef DO_RFC_1812_CHECKS
		/* Check to make sure the packet is valid (RFC1812) */
		if (is_valid_ipv4_pkt(ipv4_hdr, m->pkt_len) < 0) {
			rte_pktmbuf_free(m);
			return;
		}
#endif

		 dst_port = get_ipv4_dst_port(ipv4_hdr, portid,
			RTE_PER_LCORE(lcore_conf)->ipv4_lookup_struct);
		if (dst_port >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port) == 0)
			dst_port = portid;

#ifdef DO_RFC_1812_CHECKS
		/* Update time to live and header checksum */
		--(ipv4_hdr->time_to_live);
		++(ipv4_hdr->hdr_checksum);
#endif
		/* dst addr */
		*(uint64_t *)&eth_hdr->d_addr = dest_eth_addr[dst_port];

		/* src addr */
		ether_addr_copy(&ports_eth_addr[dst_port], &eth_hdr->s_addr);

		send_single_packet(m, dst_port);
	} else if (RTE_ETH_IS_IPV6_HDR(m->packet_type)) {
		/* Handle IPv6 headers.*/
		struct ipv6_hdr *ipv6_hdr;

		ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct ipv6_hdr *,
				sizeof(struct ether_hdr));

		dst_port = get_ipv6_dst_port(ipv6_hdr, portid,
				RTE_PER_LCORE(lcore_conf)->ipv6_lookup_struct);

		if (dst_port >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port) == 0)
			dst_port = portid;

		/* dst addr */
		*(uint64_t *)&eth_hdr->d_addr = dest_eth_addr[dst_port];

		/* src addr */
		ether_addr_copy(&ports_eth_addr[dst_port], &eth_hdr->s_addr);

		send_single_packet(m, dst_port);
	} else
		/* Free the mbuf that contains non-IPV4/IPV6 packet */
		rte_pktmbuf_free(m);
}

#if ((APP_LOOKUP_METHOD == APP_LOOKUP_LPM) && \
	(ENABLE_MULTI_BUFFER_OPTIMIZE == 1))
#ifdef DO_RFC_1812_CHECKS

#define	IPV4_MIN_VER_IHL	0x45
#define	IPV4_MAX_VER_IHL	0x4f
#define	IPV4_MAX_VER_IHL_DIFF	(IPV4_MAX_VER_IHL - IPV4_MIN_VER_IHL)

/* Minimum value of IPV4 total length (20B) in network byte order. */
#define	IPV4_MIN_LEN_BE	(sizeof(struct ipv4_hdr) << 8)

/*
 * From http://www.rfc-editor.org/rfc/rfc1812.txt section 5.2.2:
 * - The IP version number must be 4.
 * - The IP header length field must be large enough to hold the
 *    minimum length legal IP datagram (20 bytes = 5 words).
 * - The IP total length field must be large enough to hold the IP
 *   datagram header, whose length is specified in the IP header length
 *   field.
 * If we encounter invalid IPV4 packet, then set destination port for it
 * to BAD_PORT value.
 */
static __rte_always_inline void
rfc1812_process(struct ipv4_hdr *ipv4_hdr, uint16_t *dp, uint32_t ptype)
{
	uint8_t ihl;

	if (RTE_ETH_IS_IPV4_HDR(ptype)) {
		ihl = ipv4_hdr->version_ihl - IPV4_MIN_VER_IHL;

		ipv4_hdr->time_to_live--;
		ipv4_hdr->hdr_checksum++;

		if (ihl > IPV4_MAX_VER_IHL_DIFF ||
				((uint8_t)ipv4_hdr->total_length == 0 &&
				ipv4_hdr->total_length < IPV4_MIN_LEN_BE)) {
			dp[0] = BAD_PORT;
		}
	}
}

#else
#define	rfc1812_process(mb, dp, ptype)	do { } while (0)
#endif /* DO_RFC_1812_CHECKS */
#endif /* APP_LOOKUP_LPM && ENABLE_MULTI_BUFFER_OPTIMIZE */


#if ((APP_LOOKUP_METHOD == APP_LOOKUP_LPM) && \
	(ENABLE_MULTI_BUFFER_OPTIMIZE == 1))

static __rte_always_inline uint16_t
get_dst_port(struct rte_mbuf *pkt, uint32_t dst_ipv4, uint16_t portid)
{
	uint32_t next_hop;
	struct ipv6_hdr *ipv6_hdr;
	struct ether_hdr *eth_hdr;

	if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
		return (uint16_t) ((rte_lpm_lookup(
				RTE_PER_LCORE(lcore_conf)->ipv4_lookup_struct, dst_ipv4,
				&next_hop) == 0) ? next_hop : portid);

	} else if (RTE_ETH_IS_IPV6_HDR(pkt->packet_type)) {

		eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
		ipv6_hdr = (struct ipv6_hdr *)(eth_hdr + 1);

		return (uint16_t) ((rte_lpm6_lookup(
				RTE_PER_LCORE(lcore_conf)->ipv6_lookup_struct,
				ipv6_hdr->dst_addr, &next_hop) == 0) ?
				next_hop : portid);

	}

	return portid;
}

static inline void
process_packet(struct rte_mbuf *pkt, uint16_t *dst_port, uint16_t portid)
{
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	uint32_t dst_ipv4;
	uint16_t dp;
	__m128i te, ve;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);

	dst_ipv4 = ipv4_hdr->dst_addr;
	dst_ipv4 = rte_be_to_cpu_32(dst_ipv4);
	dp = get_dst_port(pkt, dst_ipv4, portid);

	te = _mm_load_si128((__m128i *)eth_hdr);
	ve = val_eth[dp];

	dst_port[0] = dp;
	rfc1812_process(ipv4_hdr, dst_port, pkt->packet_type);

	te =  _mm_blend_epi16(te, ve, MASK_ETH);
	_mm_store_si128((__m128i *)eth_hdr, te);
}

/*
 * Read packet_type and destination IPV4 addresses from 4 mbufs.
 */
static inline void
processx4_step1(struct rte_mbuf *pkt[FWDSTEP],
		__m128i *dip,
		uint32_t *ipv4_flag)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ether_hdr *eth_hdr;
	uint32_t x0, x1, x2, x3;

	eth_hdr = rte_pktmbuf_mtod(pkt[0], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x0 = ipv4_hdr->dst_addr;
	ipv4_flag[0] = pkt[0]->packet_type & RTE_PTYPE_L3_IPV4;

	eth_hdr = rte_pktmbuf_mtod(pkt[1], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x1 = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[1]->packet_type;

	eth_hdr = rte_pktmbuf_mtod(pkt[2], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x2 = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[2]->packet_type;

	eth_hdr = rte_pktmbuf_mtod(pkt[3], struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
	x3 = ipv4_hdr->dst_addr;
	ipv4_flag[0] &= pkt[3]->packet_type;

	dip[0] = _mm_set_epi32(x3, x2, x1, x0);
}

/*
 * Lookup into LPM for destination port.
 * If lookup fails, use incoming port (portid) as destination port.
 */
static inline void
processx4_step2(__m128i dip,
		uint32_t ipv4_flag,
		uint16_t portid,
		struct rte_mbuf *pkt[FWDSTEP],
		uint16_t dprt[FWDSTEP])
{
	rte_xmm_t dst;
	const __m128i bswap_mask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11,
			4, 5, 6, 7, 0, 1, 2, 3);

	/* Byte swap 4 IPV4 addresses. */
	dip = _mm_shuffle_epi8(dip, bswap_mask);

	/* if all 4 packets are IPV4. */
	if (likely(ipv4_flag)) {
		rte_lpm_lookupx4(RTE_PER_LCORE(lcore_conf)->ipv4_lookup_struct, dip,
				dst.u32, portid);

		/* get rid of unused upper 16 bit for each dport. */
		dst.x = _mm_packs_epi32(dst.x, dst.x);
		*(uint64_t *)dprt = dst.u64[0];
	} else {
		dst.x = dip;
		dprt[0] = get_dst_port(pkt[0], dst.u32[0], portid);
		dprt[1] = get_dst_port(pkt[1], dst.u32[1], portid);
		dprt[2] = get_dst_port(pkt[2], dst.u32[2], portid);
		dprt[3] = get_dst_port(pkt[3], dst.u32[3], portid);
	}
}

/*
 * Update source and destination MAC addresses in the ethernet header.
 * Perform RFC1812 checks and updates for IPV4 packets.
 */
static inline void
processx4_step3(struct rte_mbuf *pkt[FWDSTEP], uint16_t dst_port[FWDSTEP])
{
	__m128i te[FWDSTEP];
	__m128i ve[FWDSTEP];
	__m128i *p[FWDSTEP];

	p[0] = rte_pktmbuf_mtod(pkt[0], __m128i *);
	p[1] = rte_pktmbuf_mtod(pkt[1], __m128i *);
	p[2] = rte_pktmbuf_mtod(pkt[2], __m128i *);
	p[3] = rte_pktmbuf_mtod(pkt[3], __m128i *);

	ve[0] = val_eth[dst_port[0]];
	te[0] = _mm_load_si128(p[0]);

	ve[1] = val_eth[dst_port[1]];
	te[1] = _mm_load_si128(p[1]);

	ve[2] = val_eth[dst_port[2]];
	te[2] = _mm_load_si128(p[2]);

	ve[3] = val_eth[dst_port[3]];
	te[3] = _mm_load_si128(p[3]);

	/* Update first 12 bytes, keep rest bytes intact. */
	te[0] =  _mm_blend_epi16(te[0], ve[0], MASK_ETH);
	te[1] =  _mm_blend_epi16(te[1], ve[1], MASK_ETH);
	te[2] =  _mm_blend_epi16(te[2], ve[2], MASK_ETH);
	te[3] =  _mm_blend_epi16(te[3], ve[3], MASK_ETH);

	_mm_store_si128(p[0], te[0]);
	_mm_store_si128(p[1], te[1]);
	_mm_store_si128(p[2], te[2]);
	_mm_store_si128(p[3], te[3]);

	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[0] + 1),
			&dst_port[0], pkt[0]->packet_type);
	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[1] + 1),
			&dst_port[1], pkt[1]->packet_type);
	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[2] + 1),
			&dst_port[2], pkt[2]->packet_type);
	rfc1812_process((struct ipv4_hdr *)((struct ether_hdr *)p[3] + 1),
			&dst_port[3], pkt[3]->packet_type);
}

/*
 * We group consecutive packets with the same destionation port into one burst.
 * To avoid extra latency this is done together with some other packet
 * processing, but after we made a final decision about packet's destination.
 * To do this we maintain:
 * pnum - array of number of consecutive packets with the same dest port for
 * each packet in the input burst.
 * lp - pointer to the last updated element in the pnum.
 * dlp - dest port value lp corresponds to.
 */

#define	GRPSZ	(1 << FWDSTEP)
#define	GRPMSK	(GRPSZ - 1)

#define GROUP_PORT_STEP(dlp, dcp, lp, pn, idx)	do { \
	if (likely((dlp) == (dcp)[(idx)])) {         \
		(lp)[0]++;                           \
	} else {                                     \
		(dlp) = (dcp)[idx];                  \
		(lp) = (pn) + (idx);                 \
		(lp)[0] = 1;                         \
	}                                            \
} while (0)

/*
 * Group consecutive packets with the same destination port in bursts of 4.
 * Suppose we have array of destionation ports:
 * dst_port[] = {a, b, c, d,, e, ... }
 * dp1 should contain: <a, b, c, d>, dp2: <b, c, d, e>.
 * We doing 4 comparisons at once and the result is 4 bit mask.
 * This mask is used as an index into prebuild array of pnum values.
 */
static inline uint16_t *
port_groupx4(uint16_t pn[FWDSTEP + 1], uint16_t *lp, __m128i dp1, __m128i dp2)
{
	static const struct {
		uint64_t pnum; /* prebuild 4 values for pnum[]. */
		int32_t  idx;  /* index for new last updated elemnet. */
		uint16_t lpv;  /* add value to the last updated element. */
	} gptbl[GRPSZ] = {
	{
		/* 0: a != b, b != c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100010001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 1: a == b, b != c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100010002),
		.idx = 4,
		.lpv = 1,
	},
	{
		/* 2: a != b, b == c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100020001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 3: a == b, b == c, c != d, d != e */
		.pnum = UINT64_C(0x0001000100020003),
		.idx = 4,
		.lpv = 2,
	},
	{
		/* 4: a != b, b != c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200010001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 5: a == b, b != c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200010002),
		.idx = 4,
		.lpv = 1,
	},
	{
		/* 6: a != b, b == c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200030001),
		.idx = 4,
		.lpv = 0,
	},
	{
		/* 7: a == b, b == c, c == d, d != e */
		.pnum = UINT64_C(0x0001000200030004),
		.idx = 4,
		.lpv = 3,
	},
	{
		/* 8: a != b, b != c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100010001),
		.idx = 3,
		.lpv = 0,
	},
	{
		/* 9: a == b, b != c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100010002),
		.idx = 3,
		.lpv = 1,
	},
	{
		/* 0xa: a != b, b == c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100020001),
		.idx = 3,
		.lpv = 0,
	},
	{
		/* 0xb: a == b, b == c, c != d, d == e */
		.pnum = UINT64_C(0x0002000100020003),
		.idx = 3,
		.lpv = 2,
	},
	{
		/* 0xc: a != b, b != c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300010001),
		.idx = 2,
		.lpv = 0,
	},
	{
		/* 0xd: a == b, b != c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300010002),
		.idx = 2,
		.lpv = 1,
	},
	{
		/* 0xe: a != b, b == c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300040001),
		.idx = 1,
		.lpv = 0,
	},
	{
		/* 0xf: a == b, b == c, c == d, d == e */
		.pnum = UINT64_C(0x0002000300040005),
		.idx = 0,
		.lpv = 4,
	},
	};

	union {
		uint16_t u16[FWDSTEP + 1];
		uint64_t u64;
	} *pnum = (void *)pn;

	int32_t v;

	dp1 = _mm_cmpeq_epi16(dp1, dp2);
	dp1 = _mm_unpacklo_epi16(dp1, dp1);
	v = _mm_movemask_ps((__m128)dp1);

	/* update last port counter. */
	lp[0] += gptbl[v].lpv;

	/* if dest port value has changed. */
	if (v != GRPMSK) {
		pnum->u64 = gptbl[v].pnum;
		pnum->u16[FWDSTEP] = 1;
		lp = pnum->u16 + gptbl[v].idx;
	}

	return lp;
}

#endif /* APP_LOOKUP_METHOD */

static void
process_burst(struct rte_mbuf *pkts_burst[MAX_PKT_BURST], int nb_rx,
		uint16_t portid)
{

	int j;

#if ((APP_LOOKUP_METHOD == APP_LOOKUP_LPM) && \
	(ENABLE_MULTI_BUFFER_OPTIMIZE == 1))
	int32_t k;
	uint16_t dlp;
	uint16_t *lp;
	uint16_t dst_port[MAX_PKT_BURST];
	__m128i dip[MAX_PKT_BURST / FWDSTEP];
	uint32_t ipv4_flag[MAX_PKT_BURST / FWDSTEP];
	uint16_t pnum[MAX_PKT_BURST + 1];
#endif


#if (ENABLE_MULTI_BUFFER_OPTIMIZE == 1)
#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)
	{
		/*
		 * Send nb_rx - nb_rx%8 packets
		 * in groups of 8.
		 */
		int32_t n = RTE_ALIGN_FLOOR(nb_rx, 8);

		for (j = 0; j < n; j += 8) {
			uint32_t pkt_type =
				pkts_burst[j]->packet_type &
				pkts_burst[j+1]->packet_type &
				pkts_burst[j+2]->packet_type &
				pkts_burst[j+3]->packet_type &
				pkts_burst[j+4]->packet_type &
				pkts_burst[j+5]->packet_type &
				pkts_burst[j+6]->packet_type &
				pkts_burst[j+7]->packet_type;
			if (pkt_type & RTE_PTYPE_L3_IPV4) {
				simple_ipv4_fwd_8pkts(&pkts_burst[j], portid);
			} else if (pkt_type &
				RTE_PTYPE_L3_IPV6) {
				simple_ipv6_fwd_8pkts(&pkts_burst[j], portid);
			} else {
				l3fwd_simple_forward(pkts_burst[j], portid);
				l3fwd_simple_forward(pkts_burst[j+1], portid);
				l3fwd_simple_forward(pkts_burst[j+2], portid);
				l3fwd_simple_forward(pkts_burst[j+3], portid);
				l3fwd_simple_forward(pkts_burst[j+4], portid);
				l3fwd_simple_forward(pkts_burst[j+5], portid);
				l3fwd_simple_forward(pkts_burst[j+6], portid);
				l3fwd_simple_forward(pkts_burst[j+7], portid);
			}
		}
		for (; j < nb_rx ; j++)
			l3fwd_simple_forward(pkts_burst[j], portid);
	}
#elif (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)

	k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	for (j = 0; j != k; j += FWDSTEP)
		processx4_step1(&pkts_burst[j], &dip[j / FWDSTEP],
				&ipv4_flag[j / FWDSTEP]);

	k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	for (j = 0; j != k; j += FWDSTEP)
		processx4_step2(dip[j / FWDSTEP], ipv4_flag[j / FWDSTEP],
				portid, &pkts_burst[j], &dst_port[j]);

	/*
	 * Finish packet processing and group consecutive
	 * packets with the same destination port.
	 */
	k = RTE_ALIGN_FLOOR(nb_rx, FWDSTEP);
	if (k != 0) {
		__m128i dp1, dp2;

		lp = pnum;
		lp[0] = 1;

		processx4_step3(pkts_burst, dst_port);

		/* dp1: <d[0], d[1], d[2], d[3], ... > */
		dp1 = _mm_loadu_si128((__m128i *)dst_port);

		for (j = FWDSTEP; j != k; j += FWDSTEP) {
			processx4_step3(&pkts_burst[j], &dst_port[j]);

			/*
			 * dp2:
			 * <d[j-3], d[j-2], d[j-1], d[j], ... >
			 */
			dp2 = _mm_loadu_si128(
					(__m128i *)&dst_port[j - FWDSTEP + 1]);
			lp  = port_groupx4(&pnum[j - FWDSTEP], lp, dp1, dp2);

			/*
			 * dp1:
			 * <d[j], d[j+1], d[j+2], d[j+3], ... >
			 */
			dp1 = _mm_srli_si128(dp2, (FWDSTEP - 1) *
					sizeof(dst_port[0]));
		}

		/*
		 * dp2: <d[j-3], d[j-2], d[j-1], d[j-1], ... >
		 */
		dp2 = _mm_shufflelo_epi16(dp1, 0xf9);
		lp  = port_groupx4(&pnum[j - FWDSTEP], lp, dp1, dp2);

		/*
		 * remove values added by the last repeated
		 * dst port.
		 */
		lp[0]--;
		dlp = dst_port[j - 1];
	} else {
		/* set dlp and lp to the never used values. */
		dlp = BAD_PORT - 1;
		lp = pnum + MAX_PKT_BURST;
	}

	/* Process up to last 3 packets one by one. */
	switch (nb_rx % FWDSTEP) {
	case 3:
		process_packet(pkts_burst[j], dst_port + j, portid);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
		/* fall-through */
	case 2:
		process_packet(pkts_burst[j], dst_port + j, portid);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
		/* fall-through */
	case 1:
		process_packet(pkts_burst[j], dst_port + j, portid);
		GROUP_PORT_STEP(dlp, dst_port, lp, pnum, j);
		j++;
	}

	/*
	 * Send packets out, through destination port.
	 * Consecuteve pacekts with the same destination port
	 * are already grouped together.
	 * If destination port for the packet equals BAD_PORT,
	 * then free the packet without sending it out.
	 */
	for (j = 0; j < nb_rx; j += k) {

		int32_t m;
		uint16_t pn;

		pn = dst_port[j];
		k = pnum[j];

		if (likely(pn != BAD_PORT))
			send_packetsx4(pn, pkts_burst + j, k);
		else
			for (m = j; m != j + k; m++)
				rte_pktmbuf_free(pkts_burst[m]);

	}

#endif /* APP_LOOKUP_METHOD */
#else /* ENABLE_MULTI_BUFFER_OPTIMIZE == 0 */

	/* Prefetch first packets */
	for (j = 0; j < PREFETCH_OFFSET && j < nb_rx; j++)
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j], void *));

	/* Prefetch and forward already prefetched packets */
	for (j = 0; j < (nb_rx - PREFETCH_OFFSET); j++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[
				j + PREFETCH_OFFSET], void *));
		l3fwd_simple_forward(pkts_burst[j], portid);
	}

	/* Forward remaining prefetched packets */
	for (; j < nb_rx; j++)
		l3fwd_simple_forward(pkts_burst[j], portid);

#endif /* ENABLE_MULTI_BUFFER_OPTIMIZE */

}

#if (APP_CPU_LOAD > 0)

/*
 * CPU-load stats collector
 */
static int
cpu_load_collector(__rte_unused void *arg) {
	unsigned i, j, k;
	uint64_t hits;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	uint64_t total[MAX_CPU] = { 0 };
	unsigned min_cpu = MAX_CPU;
	unsigned max_cpu = 0;
	unsigned cpu_id;
	int busy_total = 0;
	int busy_flag = 0;

	unsigned int n_thread_per_cpu[MAX_CPU] = { 0 };
	struct thread_conf *thread_per_cpu[MAX_CPU][MAX_THREAD];

	struct thread_conf *thread_conf;

	const uint64_t interval_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * CPU_LOAD_TIMEOUT_US;

	prev_tsc = 0;
	/*
	 * Wait for all threads
	 */

	printf("Waiting for %d rx threads and %d tx threads\n", n_rx_thread,
			n_tx_thread);

	while (rte_atomic16_read(&rx_counter) < n_rx_thread)
		rte_pause();

	while (rte_atomic16_read(&tx_counter) < n_tx_thread)
		rte_pause();

	for (i = 0; i < n_rx_thread; i++) {

		thread_conf = &rx_thread[i].conf;
		cpu_id = thread_conf->cpu_id;
		thread_per_cpu[cpu_id][n_thread_per_cpu[cpu_id]++] = thread_conf;

		if (cpu_id > max_cpu)
			max_cpu = cpu_id;
		if (cpu_id < min_cpu)
			min_cpu = cpu_id;
	}
	for (i = 0; i < n_tx_thread; i++) {

		thread_conf = &tx_thread[i].conf;
		cpu_id = thread_conf->cpu_id;
		thread_per_cpu[cpu_id][n_thread_per_cpu[cpu_id]++] = thread_conf;

		if (thread_conf->cpu_id > max_cpu)
			max_cpu = thread_conf->cpu_id;
		if (thread_conf->cpu_id < min_cpu)
			min_cpu = thread_conf->cpu_id;
	}

	while (1) {

		cpu_load.counter++;
		for (i = min_cpu; i <= max_cpu; i++) {
			for (j = 0; j < MAX_CPU_COUNTER; j++) {
				for (k = 0; k < n_thread_per_cpu[i]; k++)
					if (thread_per_cpu[i][k]->busy[j]) {
						busy_flag = 1;
						break;
					}
				if (busy_flag) {
					cpu_load.hits[j][i]++;
					busy_total = 1;
					busy_flag = 0;
				}
			}

			if (busy_total) {
				total[i]++;
				busy_total = 0;
			}
		}

		cur_tsc = rte_rdtsc();

		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > interval_tsc)) {

			printf("\033c");

			printf("Cpu usage for %d rx threads and %d tx threads:\n\n",
					n_rx_thread, n_tx_thread);

			printf("cpu#     proc%%  poll%%  overhead%%\n\n");

			for (i = min_cpu; i <= max_cpu; i++) {
				hits = 0;
				printf("CPU %d:", i);
				for (j = 0; j < MAX_CPU_COUNTER; j++) {
					printf("%7" PRIu64 "",
							cpu_load.hits[j][i] * 100 / cpu_load.counter);
					hits += cpu_load.hits[j][i];
					cpu_load.hits[j][i] = 0;
				}
				printf("%7" PRIu64 "\n",
						100 - total[i] * 100 / cpu_load.counter);
				total[i] = 0;
			}
			cpu_load.counter = 0;

			prev_tsc = cur_tsc;
		}

	}
}
#endif /* APP_CPU_LOAD */

/*
 * Null processing lthread loop
 *
 * This loop is used to start empty scheduler on lcore.
 */
static void *
lthread_null(__rte_unused void *args)
{
	int lcore_id = rte_lcore_id();

	RTE_LOG(INFO, L3FWD, "Starting scheduler on lcore %d.\n", lcore_id);
	lthread_exit(NULL);
	return NULL;
}

/* main processing loop */
static void *
lthread_tx_per_ring(void *dummy)
{
	int nb_rx;
	uint16_t portid;
	struct rte_ring *ring;
	struct thread_tx_conf *tx_conf;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct lthread_cond *ready;

	tx_conf = (struct thread_tx_conf *)dummy;
	ring = tx_conf->ring;
	ready = *tx_conf->ready;

	lthread_set_data((void *)tx_conf);

	/*
	 * Move this lthread to lcore
	 */
	lthread_set_affinity(tx_conf->conf.lcore_id);

	RTE_LOG(INFO, L3FWD, "entering main tx loop on lcore %u\n", rte_lcore_id());

	nb_rx = 0;
	rte_atomic16_inc(&tx_counter);
	while (1) {

		/*
		 * Read packet from ring
		 */
		SET_CPU_BUSY(tx_conf, CPU_POLL);
		nb_rx = rte_ring_sc_dequeue_burst(ring, (void **)pkts_burst,
				MAX_PKT_BURST, NULL);
		SET_CPU_IDLE(tx_conf, CPU_POLL);

		if (nb_rx > 0) {
			SET_CPU_BUSY(tx_conf, CPU_PROCESS);
			portid = pkts_burst[0]->port;
			process_burst(pkts_burst, nb_rx, portid);
			SET_CPU_IDLE(tx_conf, CPU_PROCESS);
			lthread_yield();
		} else
			lthread_cond_wait(ready, 0);

	}
	return NULL;
}

/*
 * Main tx-lthreads spawner lthread.
 *
 * This lthread is used to spawn one new lthread per ring from producers.
 *
 */
static void *
lthread_tx(void *args)
{
	struct lthread *lt;

	unsigned lcore_id;
	uint16_t portid;
	struct thread_tx_conf *tx_conf;

	tx_conf = (struct thread_tx_conf *)args;
	lthread_set_data((void *)tx_conf);

	/*
	 * Move this lthread to the selected lcore
	 */
	lthread_set_affinity(tx_conf->conf.lcore_id);

	/*
	 * Spawn tx readers (one per input ring)
	 */
	lthread_create(&lt, tx_conf->conf.lcore_id, lthread_tx_per_ring,
			(void *)tx_conf);

	lcore_id = rte_lcore_id();

	RTE_LOG(INFO, L3FWD, "Entering Tx main loop on lcore %u\n", lcore_id);

	tx_conf->conf.cpu_id = sched_getcpu();
	while (1) {

		lthread_sleep(BURST_TX_DRAIN_US * 1000);

		/*
		 * TX burst queue drain
		 */
		for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
			if (tx_conf->tx_mbufs[portid].len == 0)
				continue;
			SET_CPU_BUSY(tx_conf, CPU_PROCESS);
			send_burst(tx_conf, tx_conf->tx_mbufs[portid].len, portid);
			SET_CPU_IDLE(tx_conf, CPU_PROCESS);
			tx_conf->tx_mbufs[portid].len = 0;
		}

	}
	return NULL;
}

static void *
lthread_rx(void *dummy)
{
	int ret;
	uint16_t nb_rx;
	int i;
	uint16_t portid;
	uint8_t queueid;
	int worker_id;
	int len[RTE_MAX_LCORE] = { 0 };
	int old_len, new_len;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct thread_rx_conf *rx_conf;

	rx_conf = (struct thread_rx_conf *)dummy;
	lthread_set_data((void *)rx_conf);

	/*
	 * Move this lthread to lcore
	 */
	lthread_set_affinity(rx_conf->conf.lcore_id);

	if (rx_conf->n_rx_queue == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", rte_lcore_id());
		return NULL;
	}

	RTE_LOG(INFO, L3FWD, "Entering main Rx loop on lcore %u\n", rte_lcore_id());

	for (i = 0; i < rx_conf->n_rx_queue; i++) {

		portid = rx_conf->rx_queue_list[i].port_id;
		queueid = rx_conf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
			" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
				rte_lcore_id(), portid, queueid);
	}

	/*
	 * Init all condition variables (one per rx thread)
	 */
	for (i = 0; i < rx_conf->n_rx_queue; i++)
		lthread_cond_init(NULL, &rx_conf->ready[i], NULL);

	worker_id = 0;

	rx_conf->conf.cpu_id = sched_getcpu();
	rte_atomic16_inc(&rx_counter);
	while (1) {

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < rx_conf->n_rx_queue; ++i) {
			portid = rx_conf->rx_queue_list[i].port_id;
			queueid = rx_conf->rx_queue_list[i].queue_id;

			SET_CPU_BUSY(rx_conf, CPU_POLL);
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
				MAX_PKT_BURST);
			SET_CPU_IDLE(rx_conf, CPU_POLL);

			if (nb_rx != 0) {
				worker_id = (worker_id + 1) % rx_conf->n_ring;
				old_len = len[worker_id];

				SET_CPU_BUSY(rx_conf, CPU_PROCESS);
				ret = rte_ring_sp_enqueue_burst(
						rx_conf->ring[worker_id],
						(void **) pkts_burst,
						nb_rx, NULL);

				new_len = old_len + ret;

				if (new_len >= BURST_SIZE) {
					lthread_cond_signal(rx_conf->ready[worker_id]);
					new_len = 0;
				}

				len[worker_id] = new_len;

				if (unlikely(ret < nb_rx)) {
					uint32_t k;

					for (k = ret; k < nb_rx; k++) {
						struct rte_mbuf *m = pkts_burst[k];

						rte_pktmbuf_free(m);
					}
				}
				SET_CPU_IDLE(rx_conf, CPU_PROCESS);
			}

			lthread_yield();
		}
	}
	return NULL;
}

/*
 * Start scheduler with initial lthread on lcore
 *
 * This lthread loop spawns all rx and tx lthreads on master lcore
 */

static void *
lthread_spawner(__rte_unused void *arg)
{
	struct lthread *lt[MAX_THREAD];
	int i;
	int n_thread = 0;

	printf("Entering lthread_spawner\n");

	/*
	 * Create producers (rx threads) on default lcore
	 */
	for (i = 0; i < n_rx_thread; i++) {
		rx_thread[i].conf.thread_id = i;
		lthread_create(&lt[n_thread], -1, lthread_rx,
				(void *)&rx_thread[i]);
		n_thread++;
	}

	/*
	 * Wait for all producers. Until some producers can be started on the same
	 * scheduler as this lthread, yielding is required to let them to run and
	 * prevent deadlock here.
	 */
	while (rte_atomic16_read(&rx_counter) < n_rx_thread)
		lthread_sleep(100000);

	/*
	 * Create consumers (tx threads) on default lcore_id
	 */
	for (i = 0; i < n_tx_thread; i++) {
		tx_thread[i].conf.thread_id = i;
		lthread_create(&lt[n_thread], -1, lthread_tx,
				(void *)&tx_thread[i]);
		n_thread++;
	}

	/*
	 * Wait for all threads finished
	 */
	for (i = 0; i < n_thread; i++)
		lthread_join(lt[i], NULL);

	return NULL;
}

/*
 * Start master scheduler with initial lthread spawning rx and tx lthreads
 * (main_lthread_master).
 */
static int
lthread_master_spawner(__rte_unused void *arg) {
	struct lthread *lt;
	int lcore_id = rte_lcore_id();

	RTE_PER_LCORE(lcore_conf) = &lcore_conf[lcore_id];
	lthread_create(&lt, -1, lthread_spawner, NULL);
	lthread_run();

	return 0;
}

/*
 * Start scheduler on lcore.
 */
static int
sched_spawner(__rte_unused void *arg) {
	struct lthread *lt;
	int lcore_id = rte_lcore_id();

#if (APP_CPU_LOAD)
	if (lcore_id == cpu_load_lcore_id) {
		cpu_load_collector(arg);
		return 0;
	}
#endif /* APP_CPU_LOAD */

	RTE_PER_LCORE(lcore_conf) = &lcore_conf[lcore_id];
	lthread_create(&lt, -1, lthread_null, NULL);
	lthread_run();

	return 0;
}

/* main processing loop */
static int
pthread_tx(void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int nb_rx;
	uint16_t portid;
	struct thread_tx_conf *tx_conf;

	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;

	tx_conf = (struct thread_tx_conf *)dummy;

	RTE_LOG(INFO, L3FWD, "Entering main Tx loop on lcore %u\n", rte_lcore_id());

	tx_conf->conf.cpu_id = sched_getcpu();
	rte_atomic16_inc(&tx_counter);
	while (1) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			/*
			 * This could be optimized (use queueid instead of
			 * portid), but it is not called so often
			 */
			SET_CPU_BUSY(tx_conf, CPU_PROCESS);
			for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
				if (tx_conf->tx_mbufs[portid].len == 0)
					continue;
				send_burst(tx_conf, tx_conf->tx_mbufs[portid].len, portid);
				tx_conf->tx_mbufs[portid].len = 0;
			}
			SET_CPU_IDLE(tx_conf, CPU_PROCESS);

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from ring
		 */
		SET_CPU_BUSY(tx_conf, CPU_POLL);
		nb_rx = rte_ring_sc_dequeue_burst(tx_conf->ring,
				(void **)pkts_burst, MAX_PKT_BURST, NULL);
		SET_CPU_IDLE(tx_conf, CPU_POLL);

		if (unlikely(nb_rx == 0)) {
			sched_yield();
			continue;
		}

		SET_CPU_BUSY(tx_conf, CPU_PROCESS);
		portid = pkts_burst[0]->port;
		process_burst(pkts_burst, nb_rx, portid);
		SET_CPU_IDLE(tx_conf, CPU_PROCESS);

	}
}

static int
pthread_rx(void *dummy)
{
	int i;
	int worker_id;
	uint32_t n;
	uint32_t nb_rx;
	unsigned lcore_id;
	uint8_t queueid;
	uint16_t portid;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	struct thread_rx_conf *rx_conf;

	lcore_id = rte_lcore_id();
	rx_conf = (struct thread_rx_conf *)dummy;

	if (rx_conf->n_rx_queue == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main rx loop on lcore %u\n", lcore_id);

	for (i = 0; i < rx_conf->n_rx_queue; i++) {

		portid = rx_conf->rx_queue_list[i].port_id;
		queueid = rx_conf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
			" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
				lcore_id, portid, queueid);
	}

	worker_id = 0;
	rx_conf->conf.cpu_id = sched_getcpu();
	rte_atomic16_inc(&rx_counter);
	while (1) {

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < rx_conf->n_rx_queue; ++i) {
			portid = rx_conf->rx_queue_list[i].port_id;
			queueid = rx_conf->rx_queue_list[i].queue_id;

			SET_CPU_BUSY(rx_conf, CPU_POLL);
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
				MAX_PKT_BURST);
			SET_CPU_IDLE(rx_conf, CPU_POLL);

			if (nb_rx == 0) {
				sched_yield();
				continue;
			}

			SET_CPU_BUSY(rx_conf, CPU_PROCESS);
			worker_id = (worker_id + 1) % rx_conf->n_ring;
			n = rte_ring_sp_enqueue_burst(rx_conf->ring[worker_id],
					(void **)pkts_burst, nb_rx, NULL);

			if (unlikely(n != nb_rx)) {
				uint32_t k;

				for (k = n; k < nb_rx; k++) {
					struct rte_mbuf *m = pkts_burst[k];

					rte_pktmbuf_free(m);
				}
			}

			SET_CPU_IDLE(rx_conf, CPU_PROCESS);

		}
	}
}

/*
 * P-Thread spawner.
 */
static int
pthread_run(__rte_unused void *arg) {
	int lcore_id = rte_lcore_id();
	int i;

	for (i = 0; i < n_rx_thread; i++)
		if (rx_thread[i].conf.lcore_id == lcore_id) {
			printf("Start rx thread on %d...\n", lcore_id);
			RTE_PER_LCORE(lcore_conf) = &lcore_conf[lcore_id];
			RTE_PER_LCORE(lcore_conf)->data = (void *)&rx_thread[i];
			pthread_rx((void *)&rx_thread[i]);
			return 0;
		}

	for (i = 0; i < n_tx_thread; i++)
		if (tx_thread[i].conf.lcore_id == lcore_id) {
			printf("Start tx thread on %d...\n", lcore_id);
			RTE_PER_LCORE(lcore_conf) = &lcore_conf[lcore_id];
			RTE_PER_LCORE(lcore_conf)->data = (void *)&tx_thread[i];
			pthread_tx((void *)&tx_thread[i]);
			return 0;
		}

#if (APP_CPU_LOAD)
	if (lcore_id == cpu_load_lcore_id)
		cpu_load_collector(arg);
#endif /* APP_CPU_LOAD */

	return 0;
}

static int
check_lcore_params(void)
{
	uint8_t queue, lcore;
	uint16_t i;
	int socketid;

	for (i = 0; i < nb_rx_thread_params; ++i) {
		queue = rx_thread_params[i].queue_id;
		if (queue >= MAX_RX_QUEUE_PER_PORT) {
			printf("invalid queue number: %hhu\n", queue);
			return -1;
		}
		lcore = rx_thread_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			printf("error: lcore %hhu is not enabled in lcore mask\n", lcore);
			return -1;
		}
		socketid = rte_lcore_to_socket_id(lcore);
		if ((socketid != 0) && (numa_on == 0))
			printf("warning: lcore %hhu is on socket %d with numa off\n",
				lcore, socketid);
	}
	return 0;
}

static int
check_port_config(void)
{
	unsigned portid;
	uint16_t i;

	for (i = 0; i < nb_rx_thread_params; ++i) {
		portid = rx_thread_params[i].port_id;
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
get_port_n_rx_queues(const uint16_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_rx_thread_params; ++i)
		if (rx_thread_params[i].port_id == port &&
				rx_thread_params[i].queue_id > queue)
			queue = rx_thread_params[i].queue_id;

	return (uint8_t)(++queue);
}

static int
init_rx_rings(void)
{
	unsigned socket_io;
	struct thread_rx_conf *rx_conf;
	struct thread_tx_conf *tx_conf;
	unsigned rx_thread_id, tx_thread_id;
	char name[256];
	struct rte_ring *ring = NULL;

	for (tx_thread_id = 0; tx_thread_id < n_tx_thread; tx_thread_id++) {

		tx_conf = &tx_thread[tx_thread_id];

		printf("Connecting tx-thread %d with rx-thread %d\n", tx_thread_id,
				tx_conf->conf.thread_id);

		rx_thread_id = tx_conf->conf.thread_id;
		if (rx_thread_id > n_tx_thread) {
			printf("connection from tx-thread %u to rx-thread %u fails "
					"(rx-thread not defined)\n", tx_thread_id, rx_thread_id);
			return -1;
		}

		rx_conf = &rx_thread[rx_thread_id];
		socket_io = rte_lcore_to_socket_id(rx_conf->conf.lcore_id);

		snprintf(name, sizeof(name), "app_ring_s%u_rx%u_tx%u",
				socket_io, rx_thread_id, tx_thread_id);

		ring = rte_ring_create(name, 1024 * 4, socket_io,
				RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (ring == NULL) {
			rte_panic("Cannot create ring to connect rx-thread %u "
					"with tx-thread %u\n", rx_thread_id, tx_thread_id);
		}

		rx_conf->ring[rx_conf->n_ring] = ring;

		tx_conf->ring = ring;
		tx_conf->ready = &rx_conf->ready[rx_conf->n_ring];

		rx_conf->n_ring++;
	}
	return 0;
}

static int
init_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t thread;

	n_rx_thread = 0;

	for (i = 0; i < nb_rx_thread_params; ++i) {
		thread = rx_thread_params[i].thread_id;
		nb_rx_queue = rx_thread[thread].n_rx_queue;

		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			printf("error: too many queues (%u) for thread: %u\n",
				(unsigned)nb_rx_queue + 1, (unsigned)thread);
			return -1;
		}

		rx_thread[thread].conf.thread_id = thread;
		rx_thread[thread].conf.lcore_id = rx_thread_params[i].lcore_id;
		rx_thread[thread].rx_queue_list[nb_rx_queue].port_id =
			rx_thread_params[i].port_id;
		rx_thread[thread].rx_queue_list[nb_rx_queue].queue_id =
			rx_thread_params[i].queue_id;
		rx_thread[thread].n_rx_queue++;

		if (thread >= n_rx_thread)
			n_rx_thread = thread + 1;

	}
	return 0;
}

static int
init_tx_threads(void)
{
	int i;

	n_tx_thread = 0;
	for (i = 0; i < nb_tx_thread_params; ++i) {
		tx_thread[n_tx_thread].conf.thread_id = tx_thread_params[i].thread_id;
		tx_thread[n_tx_thread].conf.lcore_id = tx_thread_params[i].lcore_id;
		n_tx_thread++;
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK -P"
		"  [--rx (port,queue,lcore,thread)[,(port,queue,lcore,thread]]"
		"  [--tx (lcore,thread)[,(lcore,thread]]"
		"  [--enable-jumbo [--max-pkt-len PKTLEN]]\n"
		"  [--parse-ptype]\n\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
		"  -P : enable promiscuous mode\n"
		"  --rx (port,queue,lcore,thread): rx queues configuration\n"
		"  --tx (lcore,thread): tx threads configuration\n"
		"  --stat-lcore LCORE: use lcore for stat collector\n"
		"  --eth-dest=X,MM:MM:MM:MM:MM:MM: optional, ethernet destination for port X\n"
		"  --no-numa: optional, disable numa awareness\n"
		"  --ipv6: optional, specify it if running ipv6 packets\n"
		"  --enable-jumbo: enable jumbo frame"
		" which max packet len is PKTLEN in decimal (64-9600)\n"
		"  --hash-entry-num: specify the hash entry number in hexadecimal to be setup\n"
		"  --no-lthreads: turn off lthread model\n"
		"  --parse-ptype: set to use software to analyze packet type\n\n",
		prgname);
}

static int parse_max_pkt_len(const char *pktlen)
{
	char *end = NULL;
	unsigned long len;

	/* parse decimal string */
	len = strtoul(pktlen, &end, 10);
	if ((pktlen[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (len == 0)
		return -1;

	return len;
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)
static int
parse_hash_entry_number(const char *hash_entry_num)
{
	char *end = NULL;
	unsigned long hash_en;

	/* parse hexadecimal string */
	hash_en = strtoul(hash_entry_num, &end, 16);
	if ((hash_entry_num[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (hash_en == 0)
		return -1;

	return hash_en;
}
#endif

static int
parse_rx_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		FLD_THREAD,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_rx_thread_params = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_rx_thread_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of rx params: %hu\n",
					nb_rx_thread_params);
			return -1;
		}
		rx_thread_params_array[nb_rx_thread_params].port_id =
				int_fld[FLD_PORT];
		rx_thread_params_array[nb_rx_thread_params].queue_id =
				(uint8_t)int_fld[FLD_QUEUE];
		rx_thread_params_array[nb_rx_thread_params].lcore_id =
				(uint8_t)int_fld[FLD_LCORE];
		rx_thread_params_array[nb_rx_thread_params].thread_id =
				(uint8_t)int_fld[FLD_THREAD];
		++nb_rx_thread_params;
	}
	rx_thread_params = rx_thread_params_array;
	return 0;
}

static int
parse_tx_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_LCORE = 0,
		FLD_THREAD,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_tx_thread_params = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_tx_thread_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of tx params: %hu\n",
				nb_tx_thread_params);
			return -1;
		}
		tx_thread_params_array[nb_tx_thread_params].lcore_id =
				(uint8_t)int_fld[FLD_LCORE];
		tx_thread_params_array[nb_tx_thread_params].thread_id =
				(uint8_t)int_fld[FLD_THREAD];
		++nb_tx_thread_params;
	}
	tx_thread_params = tx_thread_params_array;

	return 0;
}

#if (APP_CPU_LOAD > 0)
static int
parse_stat_lcore(const char *stat_lcore)
{
	char *end = NULL;
	unsigned long lcore_id;

	lcore_id = strtoul(stat_lcore, &end, 10);
	if ((stat_lcore[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	return lcore_id;
}
#endif

static void
parse_eth_dest(const char *optarg)
{
	uint16_t portid;
	char *port_end;
	uint8_t c, *dest, peer_addr[6];

	errno = 0;
	portid = strtoul(optarg, &port_end, 10);
	if (errno != 0 || port_end == optarg || *port_end++ != ',')
		rte_exit(EXIT_FAILURE,
		"Invalid eth-dest: %s", optarg);
	if (portid >= RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE,
		"eth-dest: port %d >= RTE_MAX_ETHPORTS(%d)\n",
		portid, RTE_MAX_ETHPORTS);

	if (cmdline_parse_etheraddr(NULL, port_end,
		&peer_addr, sizeof(peer_addr)) < 0)
		rte_exit(EXIT_FAILURE,
		"Invalid ethernet address: %s\n",
		port_end);
	dest = (uint8_t *)&dest_eth_addr[portid];
	for (c = 0; c < 6; c++)
		dest[c] = peer_addr[c];
	*(uint64_t *)(val_eth + portid) = dest_eth_addr[portid];
}

#define CMD_LINE_OPT_RX_CONFIG "rx"
#define CMD_LINE_OPT_TX_CONFIG "tx"
#define CMD_LINE_OPT_STAT_LCORE "stat-lcore"
#define CMD_LINE_OPT_ETH_DEST "eth-dest"
#define CMD_LINE_OPT_NO_NUMA "no-numa"
#define CMD_LINE_OPT_IPV6 "ipv6"
#define CMD_LINE_OPT_ENABLE_JUMBO "enable-jumbo"
#define CMD_LINE_OPT_HASH_ENTRY_NUM "hash-entry-num"
#define CMD_LINE_OPT_NO_LTHREADS "no-lthreads"
#define CMD_LINE_OPT_PARSE_PTYPE "parse-ptype"

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{CMD_LINE_OPT_RX_CONFIG, 1, 0, 0},
		{CMD_LINE_OPT_TX_CONFIG, 1, 0, 0},
		{CMD_LINE_OPT_STAT_LCORE, 1, 0, 0},
		{CMD_LINE_OPT_ETH_DEST, 1, 0, 0},
		{CMD_LINE_OPT_NO_NUMA, 0, 0, 0},
		{CMD_LINE_OPT_IPV6, 0, 0, 0},
		{CMD_LINE_OPT_ENABLE_JUMBO, 0, 0, 0},
		{CMD_LINE_OPT_HASH_ENTRY_NUM, 1, 0, 0},
		{CMD_LINE_OPT_NO_LTHREADS, 0, 0, 0},
		{CMD_LINE_OPT_PARSE_PTYPE, 0, 0, 0},
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:P",
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
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

		/* long options */
		case 0:
			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_RX_CONFIG,
				sizeof(CMD_LINE_OPT_RX_CONFIG))) {
				ret = parse_rx_config(optarg);
				if (ret) {
					printf("invalid rx-config\n");
					print_usage(prgname);
					return -1;
				}
			}

			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_TX_CONFIG,
				sizeof(CMD_LINE_OPT_TX_CONFIG))) {
				ret = parse_tx_config(optarg);
				if (ret) {
					printf("invalid tx-config\n");
					print_usage(prgname);
					return -1;
				}
			}

#if (APP_CPU_LOAD > 0)
			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_STAT_LCORE,
					sizeof(CMD_LINE_OPT_STAT_LCORE))) {
				cpu_load_lcore_id = parse_stat_lcore(optarg);
			}
#endif

			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_ETH_DEST,
				sizeof(CMD_LINE_OPT_ETH_DEST)))
					parse_eth_dest(optarg);

			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_NO_NUMA,
				sizeof(CMD_LINE_OPT_NO_NUMA))) {
				printf("numa is disabled\n");
				numa_on = 0;
			}

#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)
			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_IPV6,
				sizeof(CMD_LINE_OPT_IPV6))) {
				printf("ipv6 is specified\n");
				ipv6 = 1;
			}
#endif

			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_NO_LTHREADS,
					sizeof(CMD_LINE_OPT_NO_LTHREADS))) {
				printf("l-threads model is disabled\n");
				lthreads_on = 0;
			}

			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_PARSE_PTYPE,
					sizeof(CMD_LINE_OPT_PARSE_PTYPE))) {
				printf("software packet type parsing enabled\n");
				parse_ptype_on = 1;
			}

			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_ENABLE_JUMBO,
				sizeof(CMD_LINE_OPT_ENABLE_JUMBO))) {
				struct option lenopts = {"max-pkt-len", required_argument, 0,
						0};

				printf("jumbo frame is enabled - disabling simple TX path\n");
				port_conf.rxmode.offloads |=
						DEV_RX_OFFLOAD_JUMBO_FRAME;
				port_conf.txmode.offloads |=
						DEV_TX_OFFLOAD_MULTI_SEGS;

				/* if no max-pkt-len set, use the default value ETHER_MAX_LEN */
				if (0 == getopt_long(argc, argvopt, "", &lenopts,
						&option_index)) {

					ret = parse_max_pkt_len(optarg);
					if ((ret < 64) || (ret > MAX_JUMBO_PKT_LEN)) {
						printf("invalid packet length\n");
						print_usage(prgname);
						return -1;
					}
					port_conf.rxmode.max_rx_pkt_len = ret;
				}
				printf("set jumbo frame max packet length to %u\n",
						(unsigned int)port_conf.rxmode.max_rx_pkt_len);
			}
#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)
			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_HASH_ENTRY_NUM,
				sizeof(CMD_LINE_OPT_HASH_ENTRY_NUM))) {
				ret = parse_hash_entry_number(optarg);
				if ((ret > 0) && (ret <= L3FWD_HASH_ENTRIES)) {
					hash_entry_number = ret;
				} else {
					printf("invalid hash entry number\n");
					print_usage(prgname);
					return -1;
				}
			}
#endif
			break;

		default:
			print_usage(prgname);
			return -1;
		}
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

#if (APP_LOOKUP_METHOD == APP_LOOKUP_EXACT_MATCH)

static void convert_ipv4_5tuple(struct ipv4_5tuple *key1,
		union ipv4_5tuple_host *key2)
{
	key2->ip_dst = rte_cpu_to_be_32(key1->ip_dst);
	key2->ip_src = rte_cpu_to_be_32(key1->ip_src);
	key2->port_dst = rte_cpu_to_be_16(key1->port_dst);
	key2->port_src = rte_cpu_to_be_16(key1->port_src);
	key2->proto = key1->proto;
	key2->pad0 = 0;
	key2->pad1 = 0;
}

static void convert_ipv6_5tuple(struct ipv6_5tuple *key1,
		union ipv6_5tuple_host *key2)
{
	uint32_t i;

	for (i = 0; i < 16; i++) {
		key2->ip_dst[i] = key1->ip_dst[i];
		key2->ip_src[i] = key1->ip_src[i];
	}
	key2->port_dst = rte_cpu_to_be_16(key1->port_dst);
	key2->port_src = rte_cpu_to_be_16(key1->port_src);
	key2->proto = key1->proto;
	key2->pad0 = 0;
	key2->pad1 = 0;
	key2->reserve = 0;
}

#define BYTE_VALUE_MAX 256
#define ALL_32_BITS 0xffffffff
#define BIT_8_TO_15 0x0000ff00
static inline void
populate_ipv4_few_flow_into_table(const struct rte_hash *h)
{
	uint32_t i;
	int32_t ret;
	uint32_t array_len = RTE_DIM(ipv4_l3fwd_route_array);

	mask0 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_8_TO_15);
	for (i = 0; i < array_len; i++) {
		struct ipv4_l3fwd_route  entry;
		union ipv4_5tuple_host newkey;

		entry = ipv4_l3fwd_route_array[i];
		convert_ipv4_5tuple(&entry.key, &newkey);
		ret = rte_hash_add_key(h, (void *)&newkey);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
				" to the l3fwd hash.\n", i);
		}
		ipv4_l3fwd_out_if[ret] = entry.if_out;
	}
	printf("Hash: Adding 0x%" PRIx32 " keys\n", array_len);
}

#define BIT_16_TO_23 0x00ff0000
static inline void
populate_ipv6_few_flow_into_table(const struct rte_hash *h)
{
	uint32_t i;
	int32_t ret;
	uint32_t array_len = RTE_DIM(ipv6_l3fwd_route_array);

	mask1 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_16_TO_23);
	mask2 = _mm_set_epi32(0, 0, ALL_32_BITS, ALL_32_BITS);
	for (i = 0; i < array_len; i++) {
		struct ipv6_l3fwd_route entry;
		union ipv6_5tuple_host newkey;

		entry = ipv6_l3fwd_route_array[i];
		convert_ipv6_5tuple(&entry.key, &newkey);
		ret = rte_hash_add_key(h, (void *)&newkey);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
				" to the l3fwd hash.\n", i);
		}
		ipv6_l3fwd_out_if[ret] = entry.if_out;
	}
	printf("Hash: Adding 0x%" PRIx32 "keys\n", array_len);
}

#define NUMBER_PORT_USED 4
static inline void
populate_ipv4_many_flow_into_table(const struct rte_hash *h,
		unsigned int nr_flow)
{
	unsigned i;

	mask0 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_8_TO_15);

	for (i = 0; i < nr_flow; i++) {
		struct ipv4_l3fwd_route entry;
		union ipv4_5tuple_host newkey;
		uint8_t a = (uint8_t)((i / NUMBER_PORT_USED) % BYTE_VALUE_MAX);
		uint8_t b = (uint8_t)(((i / NUMBER_PORT_USED) / BYTE_VALUE_MAX) %
				BYTE_VALUE_MAX);
		uint8_t c = (uint8_t)((i / NUMBER_PORT_USED) / (BYTE_VALUE_MAX *
				BYTE_VALUE_MAX));
		/* Create the ipv4 exact match flow */
		memset(&entry, 0, sizeof(entry));
		switch (i & (NUMBER_PORT_USED - 1)) {
		case 0:
			entry = ipv4_l3fwd_route_array[0];
			entry.key.ip_dst = IPv4(101, c, b, a);
			break;
		case 1:
			entry = ipv4_l3fwd_route_array[1];
			entry.key.ip_dst = IPv4(201, c, b, a);
			break;
		case 2:
			entry = ipv4_l3fwd_route_array[2];
			entry.key.ip_dst = IPv4(111, c, b, a);
			break;
		case 3:
			entry = ipv4_l3fwd_route_array[3];
			entry.key.ip_dst = IPv4(211, c, b, a);
			break;
		};
		convert_ipv4_5tuple(&entry.key, &newkey);
		int32_t ret = rte_hash_add_key(h, (void *)&newkey);

		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);

		ipv4_l3fwd_out_if[ret] = (uint8_t)entry.if_out;

	}
	printf("Hash: Adding 0x%x keys\n", nr_flow);
}

static inline void
populate_ipv6_many_flow_into_table(const struct rte_hash *h,
		unsigned int nr_flow)
{
	unsigned i;

	mask1 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_16_TO_23);
	mask2 = _mm_set_epi32(0, 0, ALL_32_BITS, ALL_32_BITS);
	for (i = 0; i < nr_flow; i++) {
		struct ipv6_l3fwd_route entry;
		union ipv6_5tuple_host newkey;

		uint8_t a = (uint8_t) ((i / NUMBER_PORT_USED) % BYTE_VALUE_MAX);
		uint8_t b = (uint8_t) (((i / NUMBER_PORT_USED) / BYTE_VALUE_MAX) %
				BYTE_VALUE_MAX);
		uint8_t c = (uint8_t) ((i / NUMBER_PORT_USED) / (BYTE_VALUE_MAX *
				BYTE_VALUE_MAX));

		/* Create the ipv6 exact match flow */
		memset(&entry, 0, sizeof(entry));
		switch (i & (NUMBER_PORT_USED - 1)) {
		case 0:
			entry = ipv6_l3fwd_route_array[0];
			break;
		case 1:
			entry = ipv6_l3fwd_route_array[1];
			break;
		case 2:
			entry = ipv6_l3fwd_route_array[2];
			break;
		case 3:
			entry = ipv6_l3fwd_route_array[3];
			break;
		};
		entry.key.ip_dst[13] = c;
		entry.key.ip_dst[14] = b;
		entry.key.ip_dst[15] = a;
		convert_ipv6_5tuple(&entry.key, &newkey);
		int32_t ret = rte_hash_add_key(h, (void *)&newkey);

		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);

		ipv6_l3fwd_out_if[ret] = (uint8_t) entry.if_out;

	}
	printf("Hash: Adding 0x%x keys\n", nr_flow);
}

static void
setup_hash(int socketid)
{
	struct rte_hash_parameters ipv4_l3fwd_hash_params = {
		.name = NULL,
		.entries = L3FWD_HASH_ENTRIES,
		.key_len = sizeof(union ipv4_5tuple_host),
		.hash_func = ipv4_hash_crc,
		.hash_func_init_val = 0,
	};

	struct rte_hash_parameters ipv6_l3fwd_hash_params = {
		.name = NULL,
		.entries = L3FWD_HASH_ENTRIES,
		.key_len = sizeof(union ipv6_5tuple_host),
		.hash_func = ipv6_hash_crc,
		.hash_func_init_val = 0,
	};

	char s[64];

	/* create ipv4 hash */
	snprintf(s, sizeof(s), "ipv4_l3fwd_hash_%d", socketid);
	ipv4_l3fwd_hash_params.name = s;
	ipv4_l3fwd_hash_params.socket_id = socketid;
	ipv4_l3fwd_lookup_struct[socketid] =
			rte_hash_create(&ipv4_l3fwd_hash_params);
	if (ipv4_l3fwd_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create the l3fwd hash on "
				"socket %d\n", socketid);

	/* create ipv6 hash */
	snprintf(s, sizeof(s), "ipv6_l3fwd_hash_%d", socketid);
	ipv6_l3fwd_hash_params.name = s;
	ipv6_l3fwd_hash_params.socket_id = socketid;
	ipv6_l3fwd_lookup_struct[socketid] =
			rte_hash_create(&ipv6_l3fwd_hash_params);
	if (ipv6_l3fwd_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create the l3fwd hash on "
				"socket %d\n", socketid);

	if (hash_entry_number != HASH_ENTRY_NUMBER_DEFAULT) {
		/* For testing hash matching with a large number of flows we
		 * generate millions of IP 5-tuples with an incremented dst
		 * address to initialize the hash table. */
		if (ipv6 == 0) {
			/* populate the ipv4 hash */
			populate_ipv4_many_flow_into_table(
				ipv4_l3fwd_lookup_struct[socketid], hash_entry_number);
		} else {
			/* populate the ipv6 hash */
			populate_ipv6_many_flow_into_table(
				ipv6_l3fwd_lookup_struct[socketid], hash_entry_number);
		}
	} else {
		/* Use data in ipv4/ipv6 l3fwd lookup table directly to initialize
		 * the hash table */
		if (ipv6 == 0) {
			/* populate the ipv4 hash */
			populate_ipv4_few_flow_into_table(
					ipv4_l3fwd_lookup_struct[socketid]);
		} else {
			/* populate the ipv6 hash */
			populate_ipv6_few_flow_into_table(
					ipv6_l3fwd_lookup_struct[socketid]);
		}
	}
}
#endif

#if (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)
static void
setup_lpm(int socketid)
{
	struct rte_lpm6_config config;
	struct rte_lpm_config lpm_ipv4_config;
	unsigned i;
	int ret;
	char s[64];

	/* create the LPM table */
	snprintf(s, sizeof(s), "IPV4_L3FWD_LPM_%d", socketid);
	lpm_ipv4_config.max_rules = IPV4_L3FWD_LPM_MAX_RULES;
	lpm_ipv4_config.number_tbl8s = 256;
	lpm_ipv4_config.flags = 0;
	ipv4_l3fwd_lookup_struct[socketid] =
			rte_lpm_create(s, socketid, &lpm_ipv4_config);
	if (ipv4_l3fwd_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create the l3fwd LPM table"
				" on socket %d\n", socketid);

	/* populate the LPM table */
	for (i = 0; i < IPV4_L3FWD_NUM_ROUTES; i++) {

		/* skip unused ports */
		if ((1 << ipv4_l3fwd_route_array[i].if_out &
				enabled_port_mask) == 0)
			continue;

		ret = rte_lpm_add(ipv4_l3fwd_lookup_struct[socketid],
			ipv4_l3fwd_route_array[i].ip,
			ipv4_l3fwd_route_array[i].depth,
			ipv4_l3fwd_route_array[i].if_out);

		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %u to the "
				"l3fwd LPM table on socket %d\n",
				i, socketid);
		}

		printf("LPM: Adding route 0x%08x / %d (%d)\n",
			(unsigned)ipv4_l3fwd_route_array[i].ip,
			ipv4_l3fwd_route_array[i].depth,
			ipv4_l3fwd_route_array[i].if_out);
	}

	/* create the LPM6 table */
	snprintf(s, sizeof(s), "IPV6_L3FWD_LPM_%d", socketid);

	config.max_rules = IPV6_L3FWD_LPM_MAX_RULES;
	config.number_tbl8s = IPV6_L3FWD_LPM_NUMBER_TBL8S;
	config.flags = 0;
	ipv6_l3fwd_lookup_struct[socketid] = rte_lpm6_create(s, socketid,
				&config);
	if (ipv6_l3fwd_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create the l3fwd LPM table"
				" on socket %d\n", socketid);

	/* populate the LPM table */
	for (i = 0; i < IPV6_L3FWD_NUM_ROUTES; i++) {

		/* skip unused ports */
		if ((1 << ipv6_l3fwd_route_array[i].if_out &
				enabled_port_mask) == 0)
			continue;

		ret = rte_lpm6_add(ipv6_l3fwd_lookup_struct[socketid],
			ipv6_l3fwd_route_array[i].ip,
			ipv6_l3fwd_route_array[i].depth,
			ipv6_l3fwd_route_array[i].if_out);

		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %u to the "
				"l3fwd LPM table on socket %d\n",
				i, socketid);
		}

		printf("LPM: Adding route %s / %d (%d)\n",
			"IPV6",
			ipv6_l3fwd_route_array[i].depth,
			ipv6_l3fwd_route_array[i].if_out);
	}
}
#endif

static int
init_mem(unsigned nb_mbuf)
{
	struct lcore_conf *qconf;
	int socketid;
	unsigned lcore_id;
	char s[64];

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socketid = rte_lcore_to_socket_id(lcore_id);
		else
			socketid = 0;

		if (socketid >= NB_SOCKETS) {
			rte_exit(EXIT_FAILURE, "Socket %d of lcore %u is out of range %d\n",
				socketid, lcore_id, NB_SOCKETS);
		}
		if (pktmbuf_pool[socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d", socketid);
			pktmbuf_pool[socketid] =
				rte_pktmbuf_pool_create(s, nb_mbuf,
					MEMPOOL_CACHE_SIZE, 0,
					RTE_MBUF_DEFAULT_BUF_SIZE, socketid);
			if (pktmbuf_pool[socketid] == NULL)
				rte_exit(EXIT_FAILURE,
						"Cannot init mbuf pool on socket %d\n", socketid);
			else
				printf("Allocated mbuf pool on socket %d\n", socketid);

#if (APP_LOOKUP_METHOD == APP_LOOKUP_LPM)
			setup_lpm(socketid);
#else
			setup_hash(socketid);
#endif
		}
		qconf = &lcore_conf[lcore_id];
		qconf->ipv4_lookup_struct = ipv4_l3fwd_lookup_struct[socketid];
		qconf->ipv6_lookup_struct = ipv6_l3fwd_lookup_struct[socketid];
	}
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
					"Port%d Link Up. Speed %u Mbps - %s\n",
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

int
main(int argc, char **argv)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	int ret;
	int i;
	unsigned nb_ports;
	uint16_t queueid, portid;
	unsigned lcore_id;
	uint32_t n_tx_queue, nb_lcores;
	uint8_t nb_rx_queue, queue, socketid;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	/* pre-init dst MACs for all ports to 02:00:00:00:00:xx */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		dest_eth_addr[portid] = ETHER_LOCAL_ADMIN_ADDR +
				((uint64_t)portid << 40);
		*(uint64_t *)(val_eth + portid) = dest_eth_addr[portid];
	}

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L3FWD parameters\n");

	if (check_lcore_params() < 0)
		rte_exit(EXIT_FAILURE, "check_lcore_params failed\n");

	printf("Initializing rx-queues...\n");
	ret = init_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_rx_queues failed\n");

	printf("Initializing tx-threads...\n");
	ret = init_tx_threads();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_tx_threads failed\n");

	printf("Initializing rings...\n");
	ret = init_rx_rings();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_rx_rings failed\n");

	nb_ports = rte_eth_dev_count_avail();

	if (check_port_config() < 0)
		rte_exit(EXIT_FAILURE, "check_port_config failed\n");

	nb_lcores = rte_lcore_count();

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_conf local_port_conf = port_conf;

		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* init port */
		printf("Initializing port %d ... ", portid);
		fflush(stdout);

		nb_rx_queue = get_port_n_rx_queues(portid);
		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		printf("Creating queues: nb_rxq=%d nb_txq=%u... ",
			nb_rx_queue, (unsigned)n_tx_queue);
		rte_eth_dev_info_get(portid, &dev_info);
		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
		ret = rte_eth_dev_configure(portid, nb_rx_queue,
					(uint16_t)n_tx_queue, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%d\n",
				ret, portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_dev_adjust_nb_rx_tx_desc: err=%d, port=%d\n",
				 ret, portid);

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");
		print_ethaddr("Destination:",
			(const struct ether_addr *)&dest_eth_addr[portid]);
		printf(", ");

		/*
		 * prepare src MACs for each port.
		 */
		ether_addr_copy(&ports_eth_addr[portid],
			(struct ether_addr *)(val_eth + portid) + 1);

		/* init memory */
		ret = init_mem(NB_MBUF);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "init_mem failed\n");

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			if (numa_on)
				socketid = (uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("txq=%u,%d,%d ", lcore_id, queueid, socketid);
			fflush(stdout);

			txconf = &dev_info.default_txconf;
			txconf->txq_flags = ETH_TXQ_FLAGS_IGNORE;
			txconf->offloads = local_port_conf.txmode.offloads;
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     socketid, txconf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);

			tx_thread[lcore_id].tx_queue_id[portid] = queueid;
			queueid++;
		}
		printf("\n");
	}

	for (i = 0; i < n_rx_thread; i++) {
		lcore_id = rx_thread[i].conf.lcore_id;

		if (rte_lcore_is_enabled(lcore_id) == 0) {
			rte_exit(EXIT_FAILURE,
					"Cannot start Rx thread on lcore %u: lcore disabled\n",
					lcore_id
				);
		}

		printf("\nInitializing rx queues for Rx thread %d on lcore %u ... ",
				i, lcore_id);
		fflush(stdout);

		/* init RX queues */
		for (queue = 0; queue < rx_thread[i].n_rx_queue; ++queue) {
			struct rte_eth_dev *dev;
			struct rte_eth_conf *conf;
			struct rte_eth_rxconf rxq_conf;

			portid = rx_thread[i].rx_queue_list[queue].port_id;
			queueid = rx_thread[i].rx_queue_list[queue].queue_id;
			dev = &rte_eth_devices[portid];
			conf = &dev->data->dev_conf;

			if (numa_on)
				socketid = (uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("rxq=%d,%d,%d ", portid, queueid, socketid);
			fflush(stdout);

			rte_eth_dev_info_get(portid, &dev_info);
			rxq_conf = dev_info.default_rxconf;
			rxq_conf.offloads = conf->rxmode.offloads;
			ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd,
					socketid,
					&rxq_conf,
					pktmbuf_pool[socketid]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: err=%d, "
						"port=%d\n", ret, portid);
		}
	}

	printf("\n");

	/* start ports */
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%d\n",
				ret, portid);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(portid);
	}

	for (i = 0; i < n_rx_thread; i++) {
		lcore_id = rx_thread[i].conf.lcore_id;
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		/* check if hw packet type is supported */
		for (queue = 0; queue < rx_thread[i].n_rx_queue; ++queue) {
			portid = rx_thread[i].rx_queue_list[queue].port_id;
			queueid = rx_thread[i].rx_queue_list[queue].queue_id;

			if (parse_ptype_on) {
				if (!rte_eth_add_rx_callback(portid, queueid,
						cb_parse_ptype, NULL))
					rte_exit(EXIT_FAILURE,
						"Failed to add rx callback: "
						"port=%d\n", portid);
			} else if (!check_ptype(portid))
				rte_exit(EXIT_FAILURE,
					"Port %d cannot parse packet type.\n\n"
					"Please add --parse-ptype to use sw "
					"packet type analyzer.\n\n",
					portid);
		}
	}

	check_all_ports_link_status(enabled_port_mask);

	if (lthreads_on) {
		printf("Starting L-Threading Model\n");

#if (APP_CPU_LOAD > 0)
		if (cpu_load_lcore_id > 0)
			/* Use one lcore for cpu load collector */
			nb_lcores--;
#endif

		lthread_num_schedulers_set(nb_lcores);
		rte_eal_mp_remote_launch(sched_spawner, NULL, SKIP_MASTER);
		lthread_master_spawner(NULL);

	} else {
		printf("Starting P-Threading Model\n");
		/* launch per-lcore init on every lcore */
		rte_eal_mp_remote_launch(pthread_run, NULL, CALL_MASTER);
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			if (rte_eal_wait_lcore(lcore_id) < 0)
				return -1;
		}
	}

	return 0;
}
