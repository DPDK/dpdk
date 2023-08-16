/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
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
#include <netinet/in.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_hash.h>

#include "l3fwd.h"
#include "l3fwd_event.h"
#include "em_route_parse.c"

#if defined(RTE_ARCH_X86) || defined(__ARM_FEATURE_CRC32)
#define EM_HASH_CRC 1
#endif

#ifdef EM_HASH_CRC
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC       rte_jhash
#endif

#define IPV6_ADDR_LEN 16

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
	xmm_t xmm;
};

#define XMM_NUM_IN_IPV6_5TUPLE 3

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
	xmm_t xmm[XMM_NUM_IN_IPV6_5TUPLE];
};

/* 198.18.0.0/16 are set aside for RFC2544 benchmarking (RFC5735).
 * Use RFC863 Discard Protocol.
 */
const struct ipv4_l3fwd_em_route ipv4_l3fwd_em_route_array[] = {
	{{RTE_IPV4(198, 18, 0, 0), RTE_IPV4(198, 18, 0, 1),  9, 9, IPPROTO_UDP}, 0},
	{{RTE_IPV4(198, 18, 1, 0), RTE_IPV4(198, 18, 1, 1),  9, 9, IPPROTO_UDP}, 1},
	{{RTE_IPV4(198, 18, 2, 0), RTE_IPV4(198, 18, 2, 1),  9, 9, IPPROTO_UDP}, 2},
	{{RTE_IPV4(198, 18, 3, 0), RTE_IPV4(198, 18, 3, 1),  9, 9, IPPROTO_UDP}, 3},
	{{RTE_IPV4(198, 18, 4, 0), RTE_IPV4(198, 18, 4, 1),  9, 9, IPPROTO_UDP}, 4},
	{{RTE_IPV4(198, 18, 5, 0), RTE_IPV4(198, 18, 5, 1),  9, 9, IPPROTO_UDP}, 5},
	{{RTE_IPV4(198, 18, 6, 0), RTE_IPV4(198, 18, 6, 1),  9, 9, IPPROTO_UDP}, 6},
	{{RTE_IPV4(198, 18, 7, 0), RTE_IPV4(198, 18, 7, 1),  9, 9, IPPROTO_UDP}, 7},
	{{RTE_IPV4(198, 18, 8, 0), RTE_IPV4(198, 18, 8, 1),  9, 9, IPPROTO_UDP}, 8},
	{{RTE_IPV4(198, 18, 9, 0), RTE_IPV4(198, 18, 9, 1),  9, 9, IPPROTO_UDP}, 9},
	{{RTE_IPV4(198, 18, 10, 0), RTE_IPV4(198, 18, 10, 1),  9, 9, IPPROTO_UDP}, 10},
	{{RTE_IPV4(198, 18, 11, 0), RTE_IPV4(198, 18, 11, 1),  9, 9, IPPROTO_UDP}, 11},
	{{RTE_IPV4(198, 18, 12, 0), RTE_IPV4(198, 18, 12, 1),  9, 9, IPPROTO_UDP}, 12},
	{{RTE_IPV4(198, 18, 13, 0), RTE_IPV4(198, 18, 13, 1),  9, 9, IPPROTO_UDP}, 13},
	{{RTE_IPV4(198, 18, 14, 0), RTE_IPV4(198, 18, 14, 1),  9, 9, IPPROTO_UDP}, 14},
	{{RTE_IPV4(198, 18, 15, 0), RTE_IPV4(198, 18, 15, 1),  9, 9, IPPROTO_UDP}, 15},
};

/* 2001:0200::/48 is IANA reserved range for IPv6 benchmarking (RFC5180).
 * Use RFC863 Discard Protocol.
 */
const struct ipv6_l3fwd_em_route ipv6_l3fwd_em_route_array[] = {
	{{{32, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 0},
	{{{32, 1, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 1},
	{{{32, 1, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 2},
	{{{32, 1, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 3},
	{{{32, 1, 2, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 4},
	{{{32, 1, 2, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 5},
	{{{32, 1, 2, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 6},
	{{{32, 1, 2, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 7},
	{{{32, 1, 2, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 8},
	{{{32, 1, 2, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 9},
	{{{32, 1, 2, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 10},
	{{{32, 1, 2, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 11},
	{{{32, 1, 2, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 12},
	{{{32, 1, 2, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 13},
	{{{32, 1, 2, 0, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 14},
	{{{32, 1, 2, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0},
	  {32, 1, 2, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 1}, 9, 9, IPPROTO_UDP}, 15},
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

#ifdef EM_HASH_CRC
	init_val = rte_hash_crc_4byte(t, init_val);
	init_val = rte_hash_crc_4byte(k->ip_src, init_val);
	init_val = rte_hash_crc_4byte(k->ip_dst, init_val);
	init_val = rte_hash_crc_4byte(*p, init_val);
#else
	init_val = rte_jhash_1word(t, init_val);
	init_val = rte_jhash_1word(k->ip_src, init_val);
	init_val = rte_jhash_1word(k->ip_dst, init_val);
	init_val = rte_jhash_1word(*p, init_val);
#endif

	return init_val;
}

static inline uint32_t
ipv6_hash_crc(const void *data, __rte_unused uint32_t data_len,
		uint32_t init_val)
{
	const union ipv6_5tuple_host *k;
	uint32_t t;
	const uint32_t *p;
#ifdef EM_HASH_CRC
	const uint32_t  *ip_src0, *ip_src1, *ip_src2, *ip_src3;
	const uint32_t  *ip_dst0, *ip_dst1, *ip_dst2, *ip_dst3;
#endif

	k = data;
	t = k->proto;
	p = (const uint32_t *)&k->port_src;

#ifdef EM_HASH_CRC
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
#else
	init_val = rte_jhash_1word(t, init_val);
	init_val = rte_jhash(k->ip_src,
			sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
	init_val = rte_jhash(k->ip_dst,
			sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
	init_val = rte_jhash_1word(*p, init_val);
#endif
	return init_val;
}

static uint8_t ipv4_l3fwd_out_if[L3FWD_HASH_ENTRIES] __rte_cache_aligned;
static uint8_t ipv6_l3fwd_out_if[L3FWD_HASH_ENTRIES] __rte_cache_aligned;

static rte_xmm_t mask0;
static rte_xmm_t mask1;
static rte_xmm_t mask2;

#if defined(__SSE2__)
static inline xmm_t
em_mask_key(void *key, xmm_t mask)
{
	__m128i data = _mm_loadu_si128((__m128i *)(key));

	return _mm_and_si128(data, mask);
}
#elif defined(__ARM_NEON)
static inline xmm_t
em_mask_key(void *key, xmm_t mask)
{
	int32x4_t data = vld1q_s32((int32_t *)key);

	return vandq_s32(data, mask);
}
#elif defined(__ALTIVEC__)
static inline xmm_t
em_mask_key(void *key, xmm_t mask)
{
	xmm_t data = vec_ld(0, (xmm_t *)(key));

	return vec_and(data, mask);
}
#elif defined(RTE_ARCH_RISCV)
static inline xmm_t
em_mask_key(void *key, xmm_t mask)
{
	xmm_t data = vect_load_128(key);

	return vect_and(data, mask);
}
#elif defined(RTE_ARCH_LOONGARCH)
static inline xmm_t
em_mask_key(void *key, xmm_t mask)
{
	xmm_t data = vect_load_128(key);

	return vect_and(data, mask);
}
#else
#error No vector engine (SSE, NEON, ALTIVEC) available, check your toolchain
#endif

/* Performing hash-based lookups. 8< */
static inline uint16_t
em_get_ipv4_dst_port(void *ipv4_hdr, uint16_t portid, void *lookup_struct)
{
	int ret = 0;
	union ipv4_5tuple_host key;
	struct rte_hash *ipv4_l3fwd_lookup_struct =
		(struct rte_hash *)lookup_struct;

	ipv4_hdr = (uint8_t *)ipv4_hdr +
		offsetof(struct rte_ipv4_hdr, time_to_live);

	/*
	 * Get 5 tuple: dst port, src port, dst IP address,
	 * src IP address and protocol.
	 */
	key.xmm = em_mask_key(ipv4_hdr, mask0.x);

	/* Find destination port */
	ret = rte_hash_lookup(ipv4_l3fwd_lookup_struct, (const void *)&key);
	return (ret < 0) ? portid : ipv4_l3fwd_out_if[ret];
}
/* >8 End of performing hash-based lookups. */

static inline uint16_t
em_get_ipv6_dst_port(void *ipv6_hdr, uint16_t portid, void *lookup_struct)
{
	int ret = 0;
	union ipv6_5tuple_host key;
	struct rte_hash *ipv6_l3fwd_lookup_struct =
		(struct rte_hash *)lookup_struct;

	ipv6_hdr = (uint8_t *)ipv6_hdr +
		offsetof(struct rte_ipv6_hdr, payload_len);
	void *data0 = ipv6_hdr;
	void *data1 = ((uint8_t *)ipv6_hdr) + sizeof(xmm_t);
	void *data2 = ((uint8_t *)ipv6_hdr) + sizeof(xmm_t) + sizeof(xmm_t);

	/* Get part of 5 tuple: src IP address lower 96 bits and protocol */
	key.xmm[0] = em_mask_key(data0, mask1.x);

	/*
	 * Get part of 5 tuple: dst IP address lower 96 bits
	 * and src IP address higher 32 bits.
	 */
#if defined RTE_ARCH_X86
	key.xmm[1] = _mm_loadu_si128(data1);
#else
	key.xmm[1] = *(xmm_t *)data1;
#endif

	/*
	 * Get part of 5 tuple: dst port and src port
	 * and dst IP address higher 32 bits.
	 */
	key.xmm[2] = em_mask_key(data2, mask2.x);

	/* Find destination port */
	ret = rte_hash_lookup(ipv6_l3fwd_lookup_struct, (const void *)&key);
	return (ret < 0) ? portid : ipv6_l3fwd_out_if[ret];
}

#if defined RTE_ARCH_X86 || defined __ARM_NEON
#if defined(NO_HASH_MULTI_LOOKUP)
#include "l3fwd_em_sequential.h"
#else
#include "l3fwd_em_hlm.h"
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
populate_ipv4_flow_into_table(const struct rte_hash *h)
{
	int i;
	int32_t ret;
	struct rte_eth_dev_info dev_info;
	char srcbuf[INET6_ADDRSTRLEN];
	char dstbuf[INET6_ADDRSTRLEN];

	mask0 = (rte_xmm_t){.u32 = {BIT_8_TO_15, ALL_32_BITS,
				ALL_32_BITS, ALL_32_BITS} };

	for (i = 0; i < route_num_v4; i++) {
		struct em_rule *entry;
		union ipv4_5tuple_host newkey;
		struct in_addr src;
		struct in_addr dst;

		if ((1 << em_route_base_v4[i].if_out &
				enabled_port_mask) == 0)
			continue;

		entry = &em_route_base_v4[i];
		convert_ipv4_5tuple(&(entry->v4_key), &newkey);
		ret = rte_hash_add_key(h, (void *) &newkey);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
				" to the l3fwd hash.\n", i);
		}
		ipv4_l3fwd_out_if[ret] = entry->if_out;
		ret = rte_eth_dev_info_get(em_route_base_v4[i].if_out,
				     &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device (port %u) info: %s\n",
				em_route_base_v4[i].if_out, strerror(-ret));

		src.s_addr = htonl(em_route_base_v4[i].v4_key.ip_src);
		dst.s_addr = htonl(em_route_base_v4[i].v4_key.ip_dst);
		printf("EM: Adding route %s, %s, %d, %d, %d (%d) [%s]\n",
			   inet_ntop(AF_INET, &dst, dstbuf, sizeof(dstbuf)),
		       inet_ntop(AF_INET, &src, srcbuf, sizeof(srcbuf)),
			   em_route_base_v4[i].v4_key.port_dst,
			   em_route_base_v4[i].v4_key.port_src,
			   em_route_base_v4[i].v4_key.proto,
		       em_route_base_v4[i].if_out, rte_dev_name(dev_info.device));
	}
	printf("Hash: Adding 0x%" PRIx64 " keys\n",
		(uint64_t)route_num_v4);
}

#define BIT_16_TO_23 0x00ff0000
static inline void
populate_ipv6_flow_into_table(const struct rte_hash *h)
{
	int i;
	int32_t ret;
	struct rte_eth_dev_info dev_info;
	char srcbuf[INET6_ADDRSTRLEN];
	char dstbuf[INET6_ADDRSTRLEN];

	mask1 = (rte_xmm_t){.u32 = {BIT_16_TO_23, ALL_32_BITS,
				ALL_32_BITS, ALL_32_BITS} };

	mask2 = (rte_xmm_t){.u32 = {ALL_32_BITS, ALL_32_BITS, 0, 0} };

	for (i = 0; i < route_num_v6; i++) {
		struct em_rule *entry;
		union ipv6_5tuple_host newkey;

		if ((1 << em_route_base_v6[i].if_out &
				enabled_port_mask) == 0)
			continue;

		entry = &em_route_base_v6[i];
		convert_ipv6_5tuple(&(entry->v6_key), &newkey);
		ret = rte_hash_add_key(h, (void *) &newkey);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
				" to the l3fwd hash.\n", i);
		}
		ipv6_l3fwd_out_if[ret] = entry->if_out;
		ret = rte_eth_dev_info_get(em_route_base_v6[i].if_out,
				     &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device (port %u) info: %s\n",
				em_route_base_v6[i].if_out, strerror(-ret));

		printf("EM: Adding route %s, %s, %d, %d, %d (%d) [%s]\n",
			   inet_ntop(AF_INET6, em_route_base_v6[i].v6_key.ip_dst,
			   dstbuf, sizeof(dstbuf)),
		       inet_ntop(AF_INET6, em_route_base_v6[i].v6_key.ip_src,
			   srcbuf, sizeof(srcbuf)),
			   em_route_base_v6[i].v6_key.port_dst,
			   em_route_base_v6[i].v6_key.port_src,
			   em_route_base_v6[i].v6_key.proto,
		       em_route_base_v6[i].if_out, rte_dev_name(dev_info.device));
	}
	printf("Hash: Adding 0x%" PRIx64 "keys\n",
		(uint64_t)route_num_v6);
}

/* Requirements:
 * 1. IP packets without extension;
 * 2. L4 payload should be either TCP or UDP.
 */
int
em_check_ptype(int portid)
{
	int i, ret;
	int ptype_l3_ipv4_ext = 0;
	int ptype_l3_ipv6_ext = 0;
	int ptype_l4_tcp = 0;
	int ptype_l4_udp = 0;
	uint32_t ptype_mask = RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK;

	ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, NULL, 0);
	if (ret <= 0)
		return 0;

	uint32_t ptypes[ret];

	ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, ptypes, ret);
	for (i = 0; i < ret; ++i) {
		switch (ptypes[i]) {
		case RTE_PTYPE_L3_IPV4_EXT:
			ptype_l3_ipv4_ext = 1;
			break;
		case RTE_PTYPE_L3_IPV6_EXT:
			ptype_l3_ipv6_ext = 1;
			break;
		case RTE_PTYPE_L4_TCP:
			ptype_l4_tcp = 1;
			break;
		case RTE_PTYPE_L4_UDP:
			ptype_l4_udp = 1;
			break;
		}
	}

	if (!ipv6 && !ptype_l3_ipv4_ext) {
		printf("port %d cannot parse RTE_PTYPE_L3_IPV4_EXT\n", portid);
		return 0;
	}
	if (ipv6 && !ptype_l3_ipv6_ext) {
		printf("port %d cannot parse RTE_PTYPE_L3_IPV6_EXT\n", portid);
		return 0;
	}

	if (ptype_l4_tcp == 0)
		printf("port %d cannot parse RTE_PTYPE_L4_TCP\n", portid);
	if (ptype_l4_udp == 0)
		printf("port %d cannot parse RTE_PTYPE_L4_UDP\n", portid);
	if (ptype_l4_tcp && ptype_l4_udp)
		return 1;

	return 0;
}

static inline void
em_parse_ptype(struct rte_mbuf *m)
{
	struct rte_ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;
	void *l3;
	int hdr_len;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	ether_type = eth_hdr->ether_type;
	l3 = (uint8_t *)eth_hdr + sizeof(struct rte_ether_hdr);
	if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
		ipv4_hdr = (struct rte_ipv4_hdr *)l3;
		hdr_len = rte_ipv4_hdr_len(ipv4_hdr);
		if (hdr_len == sizeof(struct rte_ipv4_hdr)) {
			packet_type |= RTE_PTYPE_L3_IPV4;
			if (ipv4_hdr->next_proto_id == IPPROTO_TCP)
				packet_type |= RTE_PTYPE_L4_TCP;
			else if (ipv4_hdr->next_proto_id == IPPROTO_UDP)
				packet_type |= RTE_PTYPE_L4_UDP;
		} else
			packet_type |= RTE_PTYPE_L3_IPV4_EXT;
	} else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {
		ipv6_hdr = (struct rte_ipv6_hdr *)l3;
		if (ipv6_hdr->proto == IPPROTO_TCP)
			packet_type |= RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP;
		else if (ipv6_hdr->proto == IPPROTO_UDP)
			packet_type |= RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP;
		else
			packet_type |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	}

	m->packet_type = packet_type;
}

uint16_t
em_cb_parse_ptype(uint16_t port __rte_unused, uint16_t queue __rte_unused,
		  struct rte_mbuf *pkts[], uint16_t nb_pkts,
		  uint16_t max_pkts __rte_unused,
		  void *user_param __rte_unused)
{
	unsigned i;

	for (i = 0; i < nb_pkts; ++i)
		em_parse_ptype(pkts[i]);

	return nb_pkts;
}

/* main processing loop */
int
em_main_loop(__rte_unused void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int i, nb_rx;
	uint8_t queueid;
	uint16_t portid;
	struct lcore_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * BURST_TX_DRAIN_US;

	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];

	const uint16_t n_rx_q = qconf->n_rx_queue;
	const uint16_t n_tx_p = qconf->n_tx_port;
	if (n_rx_q == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < n_rx_q; i++) {

		portid = qconf->rx_queue_list[i].port_id;
		queueid = qconf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
			" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	cur_tsc = rte_rdtsc();
	prev_tsc = cur_tsc;

	while (!force_quit) {

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < n_tx_p; ++i) {
				portid = qconf->tx_port_id[i];
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
		for (i = 0; i < n_rx_q; ++i) {
			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
				MAX_PKT_BURST);
			if (nb_rx == 0)
				continue;

#if defined RTE_ARCH_X86 || defined __ARM_NEON
			l3fwd_em_send_packets(nb_rx, pkts_burst,
							portid, qconf);
#else
			l3fwd_em_no_opt_send_packets(nb_rx, pkts_burst,
							portid, qconf);
#endif
		}

		cur_tsc = rte_rdtsc();
	}

	return 0;
}

#ifdef RTE_LIB_EVENTDEV
static __rte_always_inline void
em_event_loop_single(struct l3fwd_event_resources *evt_rsrc,
		const uint8_t flags)
{
	const int event_p_id = l3fwd_get_free_event_port(evt_rsrc);
	const uint8_t tx_q_id = evt_rsrc->evq.event_q_id[
		evt_rsrc->evq.nb_queues - 1];
	const uint8_t event_d_id = evt_rsrc->event_d_id;
	uint8_t deq = 0, enq = 0;
	struct lcore_conf *lconf;
	unsigned int lcore_id;
	struct rte_event ev;

	if (event_p_id < 0)
		return;

	lcore_id = rte_lcore_id();
	lconf = &lcore_conf[lcore_id];

	RTE_LOG(INFO, L3FWD, "entering %s on lcore %u\n", __func__, lcore_id);
	while (!force_quit) {
		deq = rte_event_dequeue_burst(event_d_id, event_p_id, &ev, 1,
					      0);
		if (!deq)
			continue;

		struct rte_mbuf *mbuf = ev.mbuf;

#if defined RTE_ARCH_X86 || defined __ARM_NEON
		mbuf->port = em_get_dst_port(lconf, mbuf, mbuf->port);
		process_packet(mbuf, &mbuf->port);
#else
		l3fwd_em_simple_process(mbuf, lconf);
#endif
		if (mbuf->port == BAD_PORT) {
			rte_pktmbuf_free(mbuf);
			continue;
		}

		if (flags & L3FWD_EVENT_TX_ENQ) {
			ev.queue_id = tx_q_id;
			ev.op = RTE_EVENT_OP_FORWARD;
			do {
				enq = rte_event_enqueue_burst(
					event_d_id, event_p_id, &ev, 1);
			} while (!enq && !force_quit);
		}

		if (flags & L3FWD_EVENT_TX_DIRECT) {
			rte_event_eth_tx_adapter_txq_set(mbuf, 0);
			do {
				enq = rte_event_eth_tx_adapter_enqueue(
					event_d_id, event_p_id, &ev, 1, 0);
			} while (!enq && !force_quit);
		}
	}

	l3fwd_event_worker_cleanup(event_d_id, event_p_id, &ev, enq, deq, 0);
}

static __rte_always_inline void
em_event_loop_burst(struct l3fwd_event_resources *evt_rsrc,
		const uint8_t flags)
{
	const int event_p_id = l3fwd_get_free_event_port(evt_rsrc);
	const uint8_t tx_q_id = evt_rsrc->evq.event_q_id[
		evt_rsrc->evq.nb_queues - 1];
	const uint8_t event_d_id = evt_rsrc->event_d_id;
	const uint16_t deq_len = evt_rsrc->deq_depth;
	struct rte_event events[MAX_PKT_BURST];
	int i, nb_enq = 0, nb_deq = 0;
	struct lcore_conf *lconf;
	unsigned int lcore_id;

	if (event_p_id < 0)
		return;

	lcore_id = rte_lcore_id();

	lconf = &lcore_conf[lcore_id];

	RTE_LOG(INFO, L3FWD, "entering %s on lcore %u\n", __func__, lcore_id);

	while (!force_quit) {
		/* Read events from RX queues */
		nb_deq = rte_event_dequeue_burst(event_d_id, event_p_id,
				events, deq_len, 0);
		if (nb_deq == 0) {
			rte_pause();
			continue;
		}

#if defined RTE_ARCH_X86 || defined __ARM_NEON
		l3fwd_em_process_events(nb_deq, (struct rte_event **)&events,
					lconf);
#else
		l3fwd_em_no_opt_process_events(nb_deq,
					       (struct rte_event **)&events,
					       lconf);
#endif
		for (i = 0; i < nb_deq; i++) {
			if (flags & L3FWD_EVENT_TX_ENQ) {
				events[i].queue_id = tx_q_id;
				events[i].op = RTE_EVENT_OP_FORWARD;
			}

			if (flags & L3FWD_EVENT_TX_DIRECT)
				rte_event_eth_tx_adapter_txq_set(events[i].mbuf,
								 0);
		}

		if (flags & L3FWD_EVENT_TX_ENQ) {
			nb_enq = rte_event_enqueue_burst(event_d_id, event_p_id,
					events, nb_deq);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_enqueue_burst(event_d_id,
						event_p_id, events + nb_enq,
						nb_deq - nb_enq);
		}

		if (flags & L3FWD_EVENT_TX_DIRECT) {
			nb_enq = rte_event_eth_tx_adapter_enqueue(event_d_id,
					event_p_id, events, nb_deq, 0);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_eth_tx_adapter_enqueue(
						event_d_id, event_p_id,
						events + nb_enq,
						nb_deq - nb_enq, 0);
		}
	}

	l3fwd_event_worker_cleanup(event_d_id, event_p_id, events, nb_enq,
				   nb_deq, 0);
}

static __rte_always_inline void
em_event_loop(struct l3fwd_event_resources *evt_rsrc,
		 const uint8_t flags)
{
	if (flags & L3FWD_EVENT_SINGLE)
		em_event_loop_single(evt_rsrc, flags);
	if (flags & L3FWD_EVENT_BURST)
		em_event_loop_burst(evt_rsrc, flags);
}

int __rte_noinline
em_event_main_loop_tx_d(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
					l3fwd_get_eventdev_rsrc();

	em_event_loop(evt_rsrc, L3FWD_EVENT_TX_DIRECT | L3FWD_EVENT_SINGLE);
	return 0;
}

int __rte_noinline
em_event_main_loop_tx_d_burst(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
					l3fwd_get_eventdev_rsrc();

	em_event_loop(evt_rsrc, L3FWD_EVENT_TX_DIRECT | L3FWD_EVENT_BURST);
	return 0;
}

int __rte_noinline
em_event_main_loop_tx_q(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
					l3fwd_get_eventdev_rsrc();

	em_event_loop(evt_rsrc, L3FWD_EVENT_TX_ENQ | L3FWD_EVENT_SINGLE);
	return 0;
}

int __rte_noinline
em_event_main_loop_tx_q_burst(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
					l3fwd_get_eventdev_rsrc();

	em_event_loop(evt_rsrc, L3FWD_EVENT_TX_ENQ | L3FWD_EVENT_BURST);
	return 0;
}

/* Same eventdev loop for single and burst of vector */
static __rte_always_inline void
em_event_loop_vector(struct l3fwd_event_resources *evt_rsrc,
		     const uint8_t flags)
{
	const int event_p_id = l3fwd_get_free_event_port(evt_rsrc);
	const uint8_t tx_q_id =
		evt_rsrc->evq.event_q_id[evt_rsrc->evq.nb_queues - 1];
	const uint8_t event_d_id = evt_rsrc->event_d_id;
	const uint16_t deq_len = evt_rsrc->deq_depth;
	struct rte_event events[MAX_PKT_BURST];
	int i, nb_enq = 0, nb_deq = 0;
	struct lcore_conf *lconf;
	unsigned int lcore_id;
	uint16_t *dst_ports;

	if (event_p_id < 0)
		return;

	dst_ports = rte_zmalloc("", sizeof(uint16_t) * evt_rsrc->vector_size,
				RTE_CACHE_LINE_SIZE);
	if (dst_ports == NULL)
		return;
	lcore_id = rte_lcore_id();
	lconf = &lcore_conf[lcore_id];

	RTE_LOG(INFO, L3FWD, "entering %s on lcore %u\n", __func__, lcore_id);

	while (!force_quit) {
		/* Read events from RX queues */
		nb_deq = rte_event_dequeue_burst(event_d_id, event_p_id, events,
						 deq_len, 0);
		if (nb_deq == 0) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_deq; i++) {
			if (flags & L3FWD_EVENT_TX_ENQ) {
				events[i].queue_id = tx_q_id;
				events[i].op = RTE_EVENT_OP_FORWARD;
			}

#if defined RTE_ARCH_X86 || defined __ARM_NEON
			l3fwd_em_process_event_vector(events[i].vec, lconf,
						      dst_ports);
#else
			l3fwd_em_no_opt_process_event_vector(events[i].vec,
							     lconf, dst_ports);
#endif
		}

		if (flags & L3FWD_EVENT_TX_ENQ) {
			nb_enq = rte_event_enqueue_burst(event_d_id, event_p_id,
							 events, nb_deq);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_enqueue_burst(
					event_d_id, event_p_id, events + nb_enq,
					nb_deq - nb_enq);
		}

		if (flags & L3FWD_EVENT_TX_DIRECT) {
			nb_enq = rte_event_eth_tx_adapter_enqueue(
				event_d_id, event_p_id, events, nb_deq, 0);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_eth_tx_adapter_enqueue(
					event_d_id, event_p_id, events + nb_enq,
					nb_deq - nb_enq, 0);
		}
	}

	l3fwd_event_worker_cleanup(event_d_id, event_p_id, events, nb_enq,
				   nb_deq, 1);
	rte_free(dst_ports);
}

int __rte_noinline
em_event_main_loop_tx_d_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	em_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_DIRECT);
	return 0;
}

int __rte_noinline
em_event_main_loop_tx_d_burst_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	em_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_DIRECT);
	return 0;
}

int __rte_noinline
em_event_main_loop_tx_q_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	em_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_ENQ);
	return 0;
}

int __rte_noinline
em_event_main_loop_tx_q_burst_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	em_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_ENQ);
	return 0;
}
#endif

/* Initialize exact match (hash) parameters. 8< */
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

	/*
	 * Use data from ipv4/ipv6 l3fwd config file
	 * directly to initialize the hash table.
	 */
	if (ipv6 == 0) {
		/* populate the ipv4 hash */
		populate_ipv4_flow_into_table(
			ipv4_l3fwd_em_lookup_struct[socketid]);
	} else {
		/* populate the ipv6 hash */
		populate_ipv6_flow_into_table(
			ipv6_l3fwd_em_lookup_struct[socketid]);
	}
}
/* >8 End of initialization of hash parameters. */

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
