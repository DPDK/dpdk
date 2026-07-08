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

/*
 * generated in scapy with
 * Ether()/IP()/TCP(options=[NOP,NOP,Timestamps])/os.urandom(113))
 */
static const char test_cksum_ipv4_tcp_multi_segs[] = {
	0x00, 0x16, 0x3e, 0x0b, 0x6b, 0xd2, 0xee, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x08, 0x00, 0x45, 0x00,
	0x00, 0xa5, 0x46, 0x10, 0x40, 0x00, 0x40, 0x06,
	0x80, 0xb5, 0xc0, 0xa8, 0xf9, 0x1d, 0xc0, 0xa8,
	0xf9, 0x1e, 0xdc, 0xa2, 0x14, 0x51, 0xbb, 0x8f,
	0xa0, 0x00, 0xe4, 0x7c, 0xe4, 0xb8, 0x80, 0x10,
	0x02, 0x00, 0x4b, 0xc1, 0x00, 0x00, 0x01, 0x01,
	0x08, 0x0a, 0x90, 0x60, 0xf4, 0xff, 0x03, 0xc5,
	0xb4, 0x19, 0x77, 0x34, 0xd4, 0xdc, 0x84, 0x86,
	0xff, 0x44, 0x09, 0x63, 0x36, 0x2e, 0x26, 0x9b,
	0x90, 0x70, 0xf2, 0xed, 0xc8, 0x5b, 0x87, 0xaa,
	0xb4, 0x67, 0x6b, 0x32, 0x3d, 0xc4, 0xbf, 0x15,
	0xa9, 0x16, 0x6c, 0x2a, 0x9d, 0xb2, 0xb7, 0x6b,
	0x58, 0x44, 0x58, 0x12, 0x4b, 0x8f, 0xe5, 0x12,
	0x11, 0x90, 0x94, 0x68, 0x37, 0xad, 0x0a, 0x9b,
	0xd6, 0x79, 0xf2, 0xb7, 0x31, 0xcf, 0x44, 0x22,
	0xc8, 0x99, 0x3f, 0xe5, 0xe7, 0xac, 0xc7, 0x0b,
	0x86, 0xdf, 0xda, 0xed, 0x0a, 0x0f, 0x86, 0xd7,
	0x48, 0xe2, 0xf1, 0xc2, 0x43, 0xed, 0x47, 0x3a,
	0xea, 0x25, 0x2d, 0xd6, 0x60, 0x38, 0x30, 0x07,
	0x28, 0xdd, 0x1f, 0x0c, 0xdd, 0x7b, 0x7c, 0xd9,
	0x35, 0x9d, 0x14, 0xaa, 0xc6, 0x35, 0xd1, 0x03,
	0x38, 0xb1, 0xf5,
};

static const uint8_t test_cksum_ipv4_tcp_multi_segs_len[] = {
	66,  /* the first seg contains all headers, including L2 to L4 */
	61,  /* the second seg length is odd, test byte order independent */
	52,  /* three segs are sufficient to test the most complex scenarios */
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

/* test l4 checksum api for a packet with mbuf chain */
static int
test_l4_cksum_mbuf_chain(struct rte_mempool *pktmbuf_pool, const char *pktdata, size_t len,
			     const uint8_t *segs, size_t segs_len)
{
	struct rte_mbuf *m[NB_MBUF] = {0};
	struct rte_mbuf *m_hdr = NULL;
	struct rte_net_hdr_lens hdr_lens;
	size_t i, off = 0;
	uint32_t packet_type, l3;
	void *l3_hdr;
	char *data;

	for (i = 0; i < segs_len; i++) {
		m[i] = rte_pktmbuf_alloc(pktmbuf_pool);
		if (m[i] == NULL)
			GOTO_FAIL("Cannot allocate mbuf");

		data = rte_pktmbuf_append(m[i], segs[i]);
		if (data == NULL) {
			rte_pktmbuf_free(m[i]);
			m[i] = NULL;
			GOTO_FAIL("Cannot append data");
		}

		memcpy(data, pktdata + off, segs[i]);
		off += segs[i];

		if (m_hdr) {
			if (rte_pktmbuf_chain(m_hdr, m[i])) {
				rte_pktmbuf_free(m[i]);
				m[i] = NULL;
				GOTO_FAIL("Cannot chain mbuf");
			}
		} else {
			m_hdr = m[i];
		}
	}

	if (off != len)
		GOTO_FAIL("Invalid segs");

	packet_type = rte_net_get_ptype(m_hdr, &hdr_lens, RTE_PTYPE_ALL_MASK);
	l3 = packet_type & RTE_PTYPE_L3_MASK;

	l3_hdr = rte_pktmbuf_mtod_offset(m_hdr, void *, hdr_lens.l2_len);
	off = hdr_lens.l2_len + hdr_lens.l3_len;

	if (l3 == RTE_PTYPE_L3_IPV4 || l3 == RTE_PTYPE_L3_IPV4_EXT) {
		if (rte_ipv4_udptcp_cksum_mbuf_verify(m_hdr, l3_hdr, off) != 0)
			GOTO_FAIL("Invalid L4 checksum verification for mbuf chain");
	} else if (l3 == RTE_PTYPE_L3_IPV6 || l3 == RTE_PTYPE_L3_IPV6_EXT) {
		if (rte_ipv6_udptcp_cksum_mbuf_verify(m_hdr, l3_hdr, off) != 0)
			GOTO_FAIL("Invalid L4 checksum verification for mbuf chain");
	}

	rte_pktmbuf_free(m_hdr);

	return 0;

fail:
	if (m_hdr)
		rte_pktmbuf_free(m_hdr);

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

	if (test_l4_cksum_mbuf_chain(pktmbuf_pool, test_cksum_ipv4_tcp_multi_segs,
			  sizeof(test_cksum_ipv4_tcp_multi_segs),
			  test_cksum_ipv4_tcp_multi_segs_len,
			  sizeof(test_cksum_ipv4_tcp_multi_segs_len)) < 0)
		GOTO_FAIL("checksum error on mbuf chain check");

	rte_mempool_free(pktmbuf_pool);

	return 0;

fail:
	rte_mempool_free(pktmbuf_pool);

	return -1;
}
#undef GOTO_FAIL

REGISTER_FAST_TEST(cksum_autotest, NOHUGE_OK, ASAN_OK, test_cksum);
