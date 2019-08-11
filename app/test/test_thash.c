/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2019 Vladimir Medvedkin <medvedkinv@gmail.com>
 */

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ip.h>

#include "test.h"

#include <rte_thash.h>

struct test_thash_v4 {
	uint32_t	dst_ip;
	uint32_t	src_ip;
	uint16_t	dst_port;
	uint16_t	src_port;
	uint32_t	hash_l3;
	uint32_t	hash_l3l4;
};

struct test_thash_v6 {
	uint8_t		dst_ip[16];
	uint8_t		src_ip[16];
	uint16_t	dst_port;
	uint16_t	src_port;
	uint32_t	hash_l3;
	uint32_t	hash_l3l4;
};

/*From 82599 Datasheet 7.1.2.8.3 RSS Verification Suite*/
struct test_thash_v4 v4_tbl[] = {
{RTE_IPV4(161, 142, 100, 80), RTE_IPV4(66, 9, 149, 187),
	1766, 2794, 0x323e8fc2, 0x51ccc178},
{RTE_IPV4(65, 69, 140, 83), RTE_IPV4(199, 92, 111, 2),
	4739, 14230, 0xd718262a, 0xc626b0ea},
{RTE_IPV4(12, 22, 207, 184), RTE_IPV4(24, 19, 198, 95),
	38024, 12898, 0xd2d0a5de, 0x5c2b394a},
{RTE_IPV4(209, 142, 163, 6), RTE_IPV4(38, 27, 205, 30),
	2217, 48228, 0x82989176, 0xafc7327f},
{RTE_IPV4(202, 188, 127, 2), RTE_IPV4(153, 39, 163, 191),
	1303, 44251, 0x5d1809c5, 0x10e828a2},
};

struct test_thash_v6 v6_tbl[] = {
/*3ffe:2501:200:3::1*/
{{0x3f, 0xfe, 0x25, 0x01, 0x02, 0x00, 0x00, 0x03,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,},
/*3ffe:2501:200:1fff::7*/
{0x3f, 0xfe, 0x25, 0x01, 0x02, 0x00, 0x1f, 0xff,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,},
1766, 2794, 0x2cc18cd5, 0x40207d3d},
/*ff02::1*/
{{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,},
/*3ffe:501:8::260:97ff:fe40:efab*/
{0x3f, 0xfe, 0x05, 0x01, 0x00, 0x08, 0x00, 0x00,
0x02, 0x60, 0x97, 0xff, 0xfe, 0x40, 0xef, 0xab,},
4739, 14230, 0x0f0c461c, 0xdde51bbf},
/*fe80::200:f8ff:fe21:67cf*/
{{0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x02, 0x00, 0xf8, 0xff, 0xfe, 0x21, 0x67, 0xcf,},
/*3ffe:1900:4545:3:200:f8ff:fe21:67cf*/
{0x3f, 0xfe, 0x19, 0x00, 0x45, 0x45, 0x00, 0x03,
0x02, 0x00, 0xf8, 0xff, 0xfe, 0x21, 0x67, 0xcf,},
38024, 44251, 0x4b61e985, 0x02d1feef},
};

uint8_t default_rss_key[] = {
0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};

static int
test_thash(void)
{
	uint32_t i, j;
	union rte_thash_tuple tuple;
	uint32_t rss_l3, rss_l3l4;
	uint8_t rss_key_be[RTE_DIM(default_rss_key)];
	struct rte_ipv6_hdr ipv6_hdr;

	/* Convert RSS key*/
	rte_convert_rss_key((uint32_t *)&default_rss_key,
		(uint32_t *)rss_key_be, RTE_DIM(default_rss_key));


	for (i = 0; i < RTE_DIM(v4_tbl); i++) {
		tuple.v4.src_addr = v4_tbl[i].src_ip;
		tuple.v4.dst_addr = v4_tbl[i].dst_ip;
		tuple.v4.sport = v4_tbl[i].src_port;
		tuple.v4.dport = v4_tbl[i].dst_port;
		/*Calculate hash with original key*/
		rss_l3 = rte_softrss((uint32_t *)&tuple,
				RTE_THASH_V4_L3_LEN, default_rss_key);
		rss_l3l4 = rte_softrss((uint32_t *)&tuple,
				RTE_THASH_V4_L4_LEN, default_rss_key);
		if ((rss_l3 != v4_tbl[i].hash_l3) ||
				(rss_l3l4 != v4_tbl[i].hash_l3l4))
			return -1;
		/*Calculate hash with converted key*/
		rss_l3 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V4_L3_LEN, rss_key_be);
		rss_l3l4 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V4_L4_LEN, rss_key_be);
		if ((rss_l3 != v4_tbl[i].hash_l3) ||
				(rss_l3l4 != v4_tbl[i].hash_l3l4))
			return -1;
	}
	for (i = 0; i < RTE_DIM(v6_tbl); i++) {
		/*Fill ipv6 hdr*/
		for (j = 0; j < RTE_DIM(ipv6_hdr.src_addr); j++)
			ipv6_hdr.src_addr[j] = v6_tbl[i].src_ip[j];
		for (j = 0; j < RTE_DIM(ipv6_hdr.dst_addr); j++)
			ipv6_hdr.dst_addr[j] = v6_tbl[i].dst_ip[j];
		/*Load and convert ipv6 address into tuple*/
		rte_thash_load_v6_addrs(&ipv6_hdr, &tuple);
		tuple.v6.sport = v6_tbl[i].src_port;
		tuple.v6.dport = v6_tbl[i].dst_port;
		/*Calculate hash with original key*/
		rss_l3 = rte_softrss((uint32_t *)&tuple,
				RTE_THASH_V6_L3_LEN, default_rss_key);
		rss_l3l4 = rte_softrss((uint32_t *)&tuple,
				RTE_THASH_V6_L4_LEN, default_rss_key);
		if ((rss_l3 != v6_tbl[i].hash_l3) ||
				(rss_l3l4 != v6_tbl[i].hash_l3l4))
			return -1;
		/*Calculate hash with converted key*/
		rss_l3 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V6_L3_LEN, rss_key_be);
		rss_l3l4 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V6_L4_LEN, rss_key_be);
		if ((rss_l3 != v6_tbl[i].hash_l3) ||
				(rss_l3l4 != v6_tbl[i].hash_l3l4))
			return -1;
	}
	return 0;
}

REGISTER_TEST_COMMAND(thash_autotest, test_thash);
