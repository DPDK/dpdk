/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <rte_net.h>
#include <rte_mbuf.h>
#include <rte_ip.h>

#include "test.h"

#define MEMPOOL_CACHE_SIZE      0
#define MBUF_DATA_SIZE          256
#define NB_MBUF                 128

/*
 * Test L3/L4 checksum API.
 */

#define GOTO_FAIL(str, ...) do {					\
		printf("cksum test FAILED (l.%d): <" str ">\n",		\
		       __LINE__,  ##__VA_ARGS__);			\
		goto fail;						\
	} while (0)

/* generated in scapy with Ether()/IP()/TCP())) */
static const char test_cksum_ipv4_tcp[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x28, 0x00, 0x01, 0x00, 0x00, 0x40, 0x06,
	0x7c, 0xcd, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x00,
	0x00, 0x01, 0x00, 0x14, 0x00, 0x50, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x02,
	0x20, 0x00, 0x91, 0x7c, 0x00, 0x00,

};

/* generated in scapy with Ether()/IPv6()/TCP()) */
static const char test_cksum_ipv6_tcp[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x60, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x06, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x14,
	0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x50, 0x02, 0x20, 0x00, 0x8f, 0x7d,
	0x00, 0x00,
};

/* generated in scapy with Ether()/IP()/UDP()/Raw('x')) */
static const char test_cksum_ipv4_udp[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x1d, 0x00, 0x01, 0x00, 0x00, 0x40, 0x11,
	0x7c, 0xcd, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x00,
	0x00, 0x01, 0x00, 0x35, 0x00, 0x35, 0x00, 0x09,
	0x89, 0x6f, 0x78,
};

/* generated in scapy with Ether()/IPv6()/UDP()/Raw('x')) */
static const char test_cksum_ipv6_udp[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x86, 0xdd, 0x60, 0x00,
	0x00, 0x00, 0x00, 0x09, 0x11, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x35,
	0x00, 0x35, 0x00, 0x09, 0x87, 0x70, 0x78,
};

/* generated in scapy with Ether()/IP(options='\x00')/UDP()/Raw('x')) */
static const char test_cksum_ipv4_opts_udp[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x46, 0x00,
	0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x40, 0x11,
	0x7b, 0xc9, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x35,
	0x00, 0x35, 0x00, 0x09, 0x89, 0x6f, 0x78,
};

/* test l3/l4 checksum api */
static int
test_l4_cksum(struct rte_mempool *pktmbuf_pool, const char *pktdata, size_t len)
{
	struct rte_net_hdr_lens hdr_lens;
	struct rte_mbuf *m = NULL;
	uint32_t packet_type;
	uint16_t prev_cksum;
	void *l3_hdr;
	void *l4_hdr;
	uint32_t l3;
	uint32_t l4;
	char *data;

	m = rte_pktmbuf_alloc(pktmbuf_pool);
	if (m == NULL)
		GOTO_FAIL("Cannot allocate mbuf");

	data = rte_pktmbuf_append(m, len);
	if (data == NULL)
		GOTO_FAIL("Cannot append data");

	memcpy(data, pktdata, len);

	packet_type = rte_net_get_ptype(m, &hdr_lens, RTE_PTYPE_ALL_MASK);
	l3 = packet_type & RTE_PTYPE_L3_MASK;
	l4 = packet_type & RTE_PTYPE_L4_MASK;

	l3_hdr = rte_pktmbuf_mtod_offset(m, void *, hdr_lens.l2_len);
	l4_hdr = rte_pktmbuf_mtod_offset(m, void *,
					 hdr_lens.l2_len + hdr_lens.l3_len);

	if (l3 == RTE_PTYPE_L3_IPV4 || l3 == RTE_PTYPE_L3_IPV4_EXT) {
		struct rte_ipv4_hdr *ip = l3_hdr;

		/* verify IPv4 checksum */
		if (rte_ipv4_cksum(l3_hdr) != 0)
			GOTO_FAIL("invalid IPv4 checksum verification");

		/* verify bad IPv4 checksum */
		ip->hdr_checksum++;
		if (rte_ipv4_cksum(l3_hdr) == 0)
			GOTO_FAIL("invalid IPv4 bad checksum verification");
		ip->hdr_checksum--;

		/* recalculate IPv4 checksum */
		prev_cksum = ip->hdr_checksum;
		ip->hdr_checksum = 0;
		ip->hdr_checksum = rte_ipv4_cksum(ip);
		if (ip->hdr_checksum != prev_cksum)
			GOTO_FAIL("invalid IPv4 checksum calculation");

		/* verify L4 checksum */
		if (rte_ipv4_udptcp_cksum_verify(l3_hdr, l4_hdr) != 0)
			GOTO_FAIL("invalid L4 checksum verification");

		if (l4 == RTE_PTYPE_L4_TCP) {
			struct rte_tcp_hdr *tcp = l4_hdr;

			/* verify bad TCP checksum */
			tcp->cksum++;
			if (rte_ipv4_udptcp_cksum_verify(l3_hdr, l4_hdr) == 0)
				GOTO_FAIL("invalid bad TCP checksum verification");
			tcp->cksum--;

			/* recalculate TCP checksum */
			prev_cksum = tcp->cksum;
			tcp->cksum = 0;
			tcp->cksum = rte_ipv4_udptcp_cksum(l3_hdr, l4_hdr);
			if (tcp->cksum != prev_cksum)
				GOTO_FAIL("invalid TCP checksum calculation");

		} else if (l4 == RTE_PTYPE_L4_UDP) {
			struct rte_udp_hdr *udp = l4_hdr;

			/* verify bad UDP checksum */
			udp->dgram_cksum++;
			if (rte_ipv4_udptcp_cksum_verify(l3_hdr, l4_hdr) == 0)
				GOTO_FAIL("invalid bad UDP checksum verification");
			udp->dgram_cksum--;

			/* recalculate UDP checksum */
			prev_cksum = udp->dgram_cksum;
			udp->dgram_cksum = 0;
			udp->dgram_cksum = rte_ipv4_udptcp_cksum(l3_hdr,
								 l4_hdr);
			if (udp->dgram_cksum != prev_cksum)
				GOTO_FAIL("invalid TCP checksum calculation");
		}
	} else if (l3 == RTE_PTYPE_L3_IPV6 || l3 == RTE_PTYPE_L3_IPV6_EXT) {
		if (rte_ipv6_udptcp_cksum_verify(l3_hdr, l4_hdr) != 0)
			GOTO_FAIL("invalid L4 checksum verification");

		if (l4 == RTE_PTYPE_L4_TCP) {
			struct rte_tcp_hdr *tcp = l4_hdr;

			/* verify bad TCP checksum */
			tcp->cksum++;
			if (rte_ipv6_udptcp_cksum_verify(l3_hdr, l4_hdr) == 0)
				GOTO_FAIL("invalid bad TCP checksum verification");
			tcp->cksum--;

			/* recalculate TCP checksum */
			prev_cksum = tcp->cksum;
			tcp->cksum = 0;
			tcp->cksum = rte_ipv6_udptcp_cksum(l3_hdr, l4_hdr);
			if (tcp->cksum != prev_cksum)
				GOTO_FAIL("invalid TCP checksum calculation");

		} else if (l4 == RTE_PTYPE_L4_UDP) {
			struct rte_udp_hdr *udp = l4_hdr;

			/* verify bad UDP checksum */
			udp->dgram_cksum++;
			if (rte_ipv6_udptcp_cksum_verify(l3_hdr, l4_hdr) == 0)
				GOTO_FAIL("invalid bad UDP checksum verification");
			udp->dgram_cksum--;

			/* recalculate UDP checksum */
			prev_cksum = udp->dgram_cksum;
			udp->dgram_cksum = 0;
			udp->dgram_cksum = rte_ipv6_udptcp_cksum(l3_hdr,
								 l4_hdr);
			if (udp->dgram_cksum != prev_cksum)
				GOTO_FAIL("invalid TCP checksum calculation");
		}
	}

	rte_pktmbuf_free(m);

	return 0;

fail:
	rte_pktmbuf_free(m);

	return -1;
}

static int
test_cksum(void)
{
	struct rte_mempool *pktmbuf_pool = NULL;

	/* create pktmbuf pool if it does not exist */
	pktmbuf_pool = rte_pktmbuf_pool_create("test_cksum_mbuf_pool",
			NB_MBUF, MEMPOOL_CACHE_SIZE, 0, MBUF_DATA_SIZE,
			SOCKET_ID_ANY);

	if (pktmbuf_pool == NULL)
		GOTO_FAIL("cannot allocate mbuf pool");

	if (test_l4_cksum(pktmbuf_pool, test_cksum_ipv4_tcp,
			  sizeof(test_cksum_ipv4_tcp)) < 0)
		GOTO_FAIL("checksum error on ipv4_tcp");

	if (test_l4_cksum(pktmbuf_pool, test_cksum_ipv6_tcp,
			  sizeof(test_cksum_ipv6_tcp)) < 0)
		GOTO_FAIL("checksum error on ipv6_tcp");

	if (test_l4_cksum(pktmbuf_pool, test_cksum_ipv4_udp,
			  sizeof(test_cksum_ipv4_udp)) < 0)
		GOTO_FAIL("checksum error on ipv4_udp");

	if (test_l4_cksum(pktmbuf_pool, test_cksum_ipv6_udp,
			  sizeof(test_cksum_ipv6_udp)) < 0)
		GOTO_FAIL("checksum error on ipv6_udp");

	if (test_l4_cksum(pktmbuf_pool, test_cksum_ipv4_opts_udp,
			  sizeof(test_cksum_ipv4_opts_udp)) < 0)
		GOTO_FAIL("checksum error on ipv4_opts_udp");

	rte_mempool_free(pktmbuf_pool);

	return 0;

fail:
	rte_mempool_free(pktmbuf_pool);

	return -1;
}
#undef GOTO_FAIL

REGISTER_TEST_COMMAND(cksum_autotest, test_cksum);
