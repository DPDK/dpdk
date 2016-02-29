/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_hash.h>

#include "l3fwd.h"

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC       rte_jhash
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */

#define IPV6_ADDR_LEN 16

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

struct ipv4_l3fwd_em_route {
	struct ipv4_5tuple key;
	uint8_t if_out;
};

struct ipv6_l3fwd_em_route {
	struct ipv6_5tuple key;
	uint8_t if_out;
};

static struct ipv4_l3fwd_em_route ipv4_l3fwd_em_route_array[] = {
	{{IPv4(101, 0, 0, 0), IPv4(100, 10, 0, 1),  101, 11, IPPROTO_TCP}, 0},
	{{IPv4(201, 0, 0, 0), IPv4(200, 20, 0, 1),  102, 12, IPPROTO_TCP}, 1},
	{{IPv4(111, 0, 0, 0), IPv4(100, 30, 0, 1),  101, 11, IPPROTO_TCP}, 2},
	{{IPv4(211, 0, 0, 0), IPv4(200, 40, 0, 1),  102, 12, IPPROTO_TCP}, 3},
};

static struct ipv6_l3fwd_em_route ipv6_l3fwd_em_route_array[] = {
	{{
	{0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
	101, 11, IPPROTO_TCP}, 0},

	{{
	{0xfe, 0x90, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0x90, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
	102, 12, IPPROTO_TCP}, 1},

	{{
	{0xfe, 0xa0, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0xa0, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
	101, 11, IPPROTO_TCP}, 2},

	{{
	{0xfe, 0xb0, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
	{0xfe, 0xb0, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
	102, 12, IPPROTO_TCP}, 3},
};

struct rte_hash *ipv4_l3fwd_em_lookup_struct[NB_SOCKETS];
struct rte_hash *ipv6_l3fwd_em_lookup_struct[NB_SOCKETS];

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

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
	init_val = rte_hash_crc_4byte(t, init_val);
	init_val = rte_hash_crc_4byte(k->ip_src, init_val);
	init_val = rte_hash_crc_4byte(k->ip_dst, init_val);
	init_val = rte_hash_crc_4byte(*p, init_val);
#else /* RTE_MACHINE_CPUFLAG_SSE4_2 */
	init_val = rte_jhash_1word(t, init_val);
	init_val = rte_jhash_1word(k->ip_src, init_val);
	init_val = rte_jhash_1word(k->ip_dst, init_val);
	init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */

	return init_val;
}

static inline uint32_t
ipv6_hash_crc(const void *data, __rte_unused uint32_t data_len,
		uint32_t init_val)
{
	const union ipv6_5tuple_host *k;
	uint32_t t;
	const uint32_t *p;
#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
	const uint32_t  *ip_src0, *ip_src1, *ip_src2, *ip_src3;
	const uint32_t  *ip_dst0, *ip_dst1, *ip_dst2, *ip_dst3;
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */

	k = data;
	t = k->proto;
	p = (const uint32_t *)&k->port_src;

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
	ip_src0 = (const uint32_t *) k->ip_src;
	ip_src1 = (const uint32_t *)(k->ip_src+4);
	ip_src2 = (const uint32_t *)(k->ip_src+8);
	ip_src3 = (const uint32_t *)(k->ip_src+12);
	ip_dst0 = (const uint32_t *) k->ip_dst;
	ip_dst1 = (const uint32_t *)(k->ip_dst+4);
	ip_dst2 = (const uint32_t *)(k->ip_dst+8);
	ip_dst3 = (const uint32_t *)(k->ip_dst+12);
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
#else /* RTE_MACHINE_CPUFLAG_SSE4_2 */
	init_val = rte_jhash_1word(t, init_val);
	init_val = rte_jhash(k->ip_src,
			sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
	init_val = rte_jhash(k->ip_dst,
			sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
	init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */
	return init_val;
}

#define IPV4_L3FWD_EM_NUM_ROUTES \
	(sizeof(ipv4_l3fwd_em_route_array) / sizeof(ipv4_l3fwd_em_route_array[0]))

#define IPV6_L3FWD_EM_NUM_ROUTES \
	(sizeof(ipv6_l3fwd_em_route_array) / sizeof(ipv6_l3fwd_em_route_array[0]))

static uint8_t ipv4_l3fwd_out_if[L3FWD_HASH_ENTRIES] __rte_cache_aligned;
static uint8_t ipv6_l3fwd_out_if[L3FWD_HASH_ENTRIES] __rte_cache_aligned;

static __m128i mask0;
static __m128i mask1;
static __m128i mask2;

static inline uint8_t
em_get_ipv4_dst_port(void *ipv4_hdr, uint8_t portid, void *lookup_struct)
{
	int ret = 0;
	union ipv4_5tuple_host key;
	struct rte_hash *ipv4_l3fwd_lookup_struct =
		(struct rte_hash *)lookup_struct;

	ipv4_hdr = (uint8_t *)ipv4_hdr + offsetof(struct ipv4_hdr, time_to_live);
	__m128i data = _mm_loadu_si128((__m128i *)(ipv4_hdr));

	/*
	 * Get 5 tuple: dst port, src port, dst IP address,
	 * src IP address and protocol.
	 */
	key.xmm = _mm_and_si128(data, mask0);

	/* Find destination port */
	ret = rte_hash_lookup(ipv4_l3fwd_lookup_struct, (const void *)&key);
	return (uint8_t)((ret < 0) ? portid : ipv4_l3fwd_out_if[ret]);
}

static inline uint8_t
em_get_ipv6_dst_port(void *ipv6_hdr,  uint8_t portid, void *lookup_struct)
{
	int ret = 0;
	union ipv6_5tuple_host key;
	struct rte_hash *ipv6_l3fwd_lookup_struct =
		(struct rte_hash *)lookup_struct;

	ipv6_hdr = (uint8_t *)ipv6_hdr + offsetof(struct ipv6_hdr, payload_len);
	__m128i data0 =
		_mm_loadu_si128((__m128i *)(ipv6_hdr));
	__m128i data1 =
		_mm_loadu_si128((__m128i *)(((uint8_t *)ipv6_hdr)+
					sizeof(__m128i)));
	__m128i data2 =
		_mm_loadu_si128((__m128i *)(((uint8_t *)ipv6_hdr)+
					sizeof(__m128i)+sizeof(__m128i)));

	/* Get part of 5 tuple: src IP address lower 96 bits and protocol */
	key.xmm[0] = _mm_and_si128(data0, mask1);

	/*
	 * Get part of 5 tuple: dst IP address lower 96 bits
	 * and src IP address higher 32 bits.
	 */
	key.xmm[1] = data1;

	/*
	 * Get part of 5 tuple: dst port and src port
	 * and dst IP address higher 32 bits.
	 */
	key.xmm[2] = _mm_and_si128(data2, mask2);

	/* Find destination port */
	ret = rte_hash_lookup(ipv6_l3fwd_lookup_struct, (const void *)&key);
	return (uint8_t)((ret < 0) ? portid : ipv6_l3fwd_out_if[ret]);
}

/*
 * Include header file if SSE4_1 is enabled for
 * buffer optimization i.e. ENABLE_MULTI_BUFFER_OPTIMIZE=1.
 */
#if defined(__SSE4_1__)
#ifndef HASH_MULTI_LOOKUP
#include "l3fwd_em_sse.h"
#else
#include "l3fwd_em_hlm_sse.h"
#endif
#else
#include "l3fwd_em.h"
#endif

static void
convert_ipv4_5tuple(struct ipv4_5tuple *key1,
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

static void
convert_ipv6_5tuple(struct ipv6_5tuple *key1,
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

	mask0 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS,
				ALL_32_BITS, BIT_8_TO_15);

	for (i = 0; i < IPV4_L3FWD_EM_NUM_ROUTES; i++) {
		struct ipv4_l3fwd_em_route  entry;
		union ipv4_5tuple_host newkey;

		entry = ipv4_l3fwd_em_route_array[i];
		convert_ipv4_5tuple(&entry.key, &newkey);
		ret = rte_hash_add_key(h, (void *) &newkey);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
				" to the l3fwd hash.\n", i);
		}
		ipv4_l3fwd_out_if[ret] = entry.if_out;
	}
	printf("Hash: Adding 0x%" PRIx64 " keys\n",
		(uint64_t)IPV4_L3FWD_EM_NUM_ROUTES);
}

#define BIT_16_TO_23 0x00ff0000
static inline void
populate_ipv6_few_flow_into_table(const struct rte_hash *h)
{
	uint32_t i;
	int32_t ret;

	mask1 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS,
				ALL_32_BITS, BIT_16_TO_23);

	mask2 = _mm_set_epi32(0, 0, ALL_32_BITS, ALL_32_BITS);

	for (i = 0; i < IPV6_L3FWD_EM_NUM_ROUTES; i++) {
		struct ipv6_l3fwd_em_route entry;
		union ipv6_5tuple_host newkey;

		entry = ipv6_l3fwd_em_route_array[i];
		convert_ipv6_5tuple(&entry.key, &newkey);
		ret = rte_hash_add_key(h, (void *) &newkey);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
				" to the l3fwd hash.\n", i);
		}
		ipv6_l3fwd_out_if[ret] = entry.if_out;
	}
	printf("Hash: Adding 0x%" PRIx64 "keys\n",
		(uint64_t)IPV6_L3FWD_EM_NUM_ROUTES);
}

#define NUMBER_PORT_USED 4
static inline void
populate_ipv4_many_flow_into_table(const struct rte_hash *h,
		unsigned int nr_flow)
{
	unsigned i;

	mask0 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS,
				ALL_32_BITS, BIT_8_TO_15);

	for (i = 0; i < nr_flow; i++) {
		struct ipv4_l3fwd_em_route entry;
		union ipv4_5tuple_host newkey;

		uint8_t a = (uint8_t)
			((i/NUMBER_PORT_USED)%BYTE_VALUE_MAX);
		uint8_t b = (uint8_t)
			(((i/NUMBER_PORT_USED)/BYTE_VALUE_MAX)%BYTE_VALUE_MAX);
		uint8_t c = (uint8_t)
			((i/NUMBER_PORT_USED)/(BYTE_VALUE_MAX*BYTE_VALUE_MAX));

		/* Create the ipv4 exact match flow */
		memset(&entry, 0, sizeof(entry));
		switch (i & (NUMBER_PORT_USED - 1)) {
		case 0:
			entry = ipv4_l3fwd_em_route_array[0];
			entry.key.ip_dst = IPv4(101, c, b, a);
			break;
		case 1:
			entry = ipv4_l3fwd_em_route_array[1];
			entry.key.ip_dst = IPv4(201, c, b, a);
			break;
		case 2:
			entry = ipv4_l3fwd_em_route_array[2];
			entry.key.ip_dst = IPv4(111, c, b, a);
			break;
		case 3:
			entry = ipv4_l3fwd_em_route_array[3];
			entry.key.ip_dst = IPv4(211, c, b, a);
			break;
		};
		convert_ipv4_5tuple(&entry.key, &newkey);
		int32_t ret = rte_hash_add_key(h, (void *) &newkey);

		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);

		ipv4_l3fwd_out_if[ret] = (uint8_t) entry.if_out;

	}
	printf("Hash: Adding 0x%x keys\n", nr_flow);
}

static inline void
populate_ipv6_many_flow_into_table(const struct rte_hash *h,
		unsigned int nr_flow)
{
	unsigned i;

	mask1 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS,
				ALL_32_BITS, BIT_16_TO_23);
	mask2 = _mm_set_epi32(0, 0, ALL_32_BITS, ALL_32_BITS);

	for (i = 0; i < nr_flow; i++) {
		struct ipv6_l3fwd_em_route entry;
		union ipv6_5tuple_host newkey;

		uint8_t a = (uint8_t)
			((i/NUMBER_PORT_USED)%BYTE_VALUE_MAX);
		uint8_t b = (uint8_t)
			(((i/NUMBER_PORT_USED)/BYTE_VALUE_MAX)%BYTE_VALUE_MAX);
		uint8_t c = (uint8_t)
			((i/NUMBER_PORT_USED)/(BYTE_VALUE_MAX*BYTE_VALUE_MAX));

		/* Create the ipv6 exact match flow */
		memset(&entry, 0, sizeof(entry));
		switch (i & (NUMBER_PORT_USED - 1)) {
		case 0:
			entry = ipv6_l3fwd_em_route_array[0];
			break;
		case 1:
			entry = ipv6_l3fwd_em_route_array[1];
			break;
		case 2:
			entry = ipv6_l3fwd_em_route_array[2];
			break;
		case 3:
			entry = ipv6_l3fwd_em_route_array[3];
			break;
		};
		entry.key.ip_dst[13] = c;
		entry.key.ip_dst[14] = b;
		entry.key.ip_dst[15] = a;
		convert_ipv6_5tuple(&entry.key, &newkey);
		int32_t ret = rte_hash_add_key(h, (void *) &newkey);

		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);

		ipv6_l3fwd_out_if[ret] = (uint8_t) entry.if_out;

	}
	printf("Hash: Adding 0x%x keys\n", nr_flow);
}

/* main processing loop */
int
em_main_loop(__attribute__((unused)) void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int i, nb_rx;
	uint8_t portid, queueid;
	struct lcore_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];

	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {

		portid = qconf->rx_queue_list[i].port_id;
		queueid = qconf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
			" -- lcoreid=%u portid=%hhu rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	while (!force_quit) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < qconf->n_rx_queue; i++) {
				portid = qconf->rx_queue_list[i].port_id;
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				send_burst(qconf,
					qconf->tx_mbufs[portid].len,
					portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; ++i) {
			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
				MAX_PKT_BURST);
			if (nb_rx == 0)
				continue;

			/*
			 * For SSE4_1 use ENABLE_MULTI_BUFFER_OPTIMIZE=1
			 * code.
			 */
#if defined(__SSE4_1__)
			l3fwd_em_send_packets(nb_rx, pkts_burst,
							portid, qconf);
#else
			l3fwd_em_no_opt_send_packets(nb_rx, pkts_burst,
							portid, qconf);
#endif /* __SSE_4_1__ */
		}
	}

	return 0;
}

/*
 * Initialize exact match (hash) parameters.
 */
void
setup_hash(const int socketid)
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
	ipv4_l3fwd_em_lookup_struct[socketid] =
		rte_hash_create(&ipv4_l3fwd_hash_params);
	if (ipv4_l3fwd_em_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Unable to create the l3fwd hash on socket %d\n",
			socketid);

	/* create ipv6 hash */
	snprintf(s, sizeof(s), "ipv6_l3fwd_hash_%d", socketid);
	ipv6_l3fwd_hash_params.name = s;
	ipv6_l3fwd_hash_params.socket_id = socketid;
	ipv6_l3fwd_em_lookup_struct[socketid] =
		rte_hash_create(&ipv6_l3fwd_hash_params);
	if (ipv6_l3fwd_em_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Unable to create the l3fwd hash on socket %d\n",
			socketid);

	if (hash_entry_number != HASH_ENTRY_NUMBER_DEFAULT) {
		/* For testing hash matching with a large number of flows we
		 * generate millions of IP 5-tuples with an incremented dst
		 * address to initialize the hash table. */
		if (ipv6 == 0) {
			/* populate the ipv4 hash */
			populate_ipv4_many_flow_into_table(
				ipv4_l3fwd_em_lookup_struct[socketid],
				hash_entry_number);
		} else {
			/* populate the ipv6 hash */
			populate_ipv6_many_flow_into_table(
				ipv6_l3fwd_em_lookup_struct[socketid],
				hash_entry_number);
		}
	} else {
		/*
		 * Use data in ipv4/ipv6 l3fwd lookup table
		 * directly to initialize the hash table.
		 */
		if (ipv6 == 0) {
			/* populate the ipv4 hash */
			populate_ipv4_few_flow_into_table(
				ipv4_l3fwd_em_lookup_struct[socketid]);
		} else {
			/* populate the ipv6 hash */
			populate_ipv6_few_flow_into_table(
				ipv6_l3fwd_em_lookup_struct[socketid]);
		}
	}
}

/* Return ipv4/ipv6 em fwd lookup struct. */
void *
em_get_ipv4_l3fwd_lookup_struct(const int socketid)
{
	return ipv4_l3fwd_em_lookup_struct[socketid];
}

void *
em_get_ipv6_l3fwd_lookup_struct(const int socketid)
{
	return ipv6_l3fwd_em_lookup_struct[socketid];
}
