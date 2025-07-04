/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2019 Vladimir Medvedkin <medvedkinv@gmail.com>
 */

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ip.h>
#include <rte_random.h>
#include <rte_malloc.h>
#include <rte_byteorder.h>

#include "test.h"

#include <rte_thash.h>

#define HASH_MSK(reta_sz)	((1 << reta_sz) - 1)
#define TUPLE_SZ	(RTE_THASH_V4_L4_LEN * 4)

struct test_thash_v4 {
	uint32_t	dst_ip;
	uint32_t	src_ip;
	uint16_t	dst_port;
	uint16_t	src_port;
	uint32_t	hash_l3;
	uint32_t	hash_l3l4;
};

struct test_thash_v6 {
	struct rte_ipv6_addr dst_ip;
	struct rte_ipv6_addr src_ip;
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
{RTE_IPV6(0x3ffe, 0x2501, 0x0200, 0x0003, 0, 0, 0, 0x0001),
/*3ffe:2501:200:1fff::7*/
RTE_IPV6(0x3ffe, 0x2501, 0x0200, 0x1fff, 0, 0, 0, 0x0007),
1766, 2794, 0x2cc18cd5, 0x40207d3d},
/*ff02::1*/
{RTE_IPV6(0xff02, 0, 0, 0, 0, 0, 0, 0x0001),
/*3ffe:501:8::260:97ff:fe40:efab*/
RTE_IPV6(0x3ffe, 0x0501, 0x0008, 0, 0x0260, 0x97ff, 0xfe40, 0xefab),
4739, 14230, 0x0f0c461c, 0xdde51bbf},
/*fe80::200:f8ff:fe21:67cf*/
{RTE_IPV6(0xfe80, 0, 0, 0, 0x0200, 0xf8ff, 0xfe21, 0x67cf),
/*3ffe:1900:4545:3:200:f8ff:fe21:67cf*/
RTE_IPV6(0x3ffe, 0x1900, 0x4545, 0x0003, 0x0200, 0xf8ff, 0xfe21, 0x67cf),
38024, 44251, 0x4b61e985, 0x02d1feef},
};

uint8_t default_rss_key[] = {
0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};

static const uint8_t big_rss_key[] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};

static int
test_toeplitz_hash_calc(void)
{
	uint32_t i;
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
			return -TEST_FAILED;
		/*Calculate hash with converted key*/
		rss_l3 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V4_L3_LEN, rss_key_be);
		rss_l3l4 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V4_L4_LEN, rss_key_be);
		if ((rss_l3 != v4_tbl[i].hash_l3) ||
				(rss_l3l4 != v4_tbl[i].hash_l3l4))
			return -TEST_FAILED;
	}
	for (i = 0; i < RTE_DIM(v6_tbl); i++) {
		/*Fill ipv6 hdr*/
		ipv6_hdr.src_addr = v6_tbl[i].src_ip;
		ipv6_hdr.dst_addr = v6_tbl[i].dst_ip;
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
			return -TEST_FAILED;
		/*Calculate hash with converted key*/
		rss_l3 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V6_L3_LEN, rss_key_be);
		rss_l3l4 = rte_softrss_be((uint32_t *)&tuple,
				RTE_THASH_V6_L4_LEN, rss_key_be);
		if ((rss_l3 != v6_tbl[i].hash_l3) ||
				(rss_l3l4 != v6_tbl[i].hash_l3l4))
			return -TEST_FAILED;
	}
	return TEST_SUCCESS;
}

static int
test_toeplitz_hash_gfni(void)
{
	uint32_t i;
	union rte_thash_tuple tuple;
	uint32_t rss_l3, rss_l3l4;
	uint64_t rss_key_matrixes[RTE_DIM(default_rss_key)];

	if (!rte_thash_gfni_supported())
		return TEST_SKIPPED;

	/* Convert RSS key into matrixes */
	rte_thash_complete_matrix(rss_key_matrixes, default_rss_key,
		RTE_DIM(default_rss_key));

	for (i = 0; i < RTE_DIM(v4_tbl); i++) {
		tuple.v4.src_addr = rte_cpu_to_be_32(v4_tbl[i].src_ip);
		tuple.v4.dst_addr = rte_cpu_to_be_32(v4_tbl[i].dst_ip);
		tuple.v4.sport = rte_cpu_to_be_16(v4_tbl[i].dst_port);
		tuple.v4.dport = rte_cpu_to_be_16(v4_tbl[i].src_port);

		rss_l3 = rte_thash_gfni(rss_key_matrixes, (uint8_t *)&tuple,
				RTE_THASH_V4_L3_LEN * 4);
		rss_l3l4 = rte_thash_gfni(rss_key_matrixes, (uint8_t *)&tuple,
				RTE_THASH_V4_L4_LEN * 4);
		if ((rss_l3 != v4_tbl[i].hash_l3) ||
				(rss_l3l4 != v4_tbl[i].hash_l3l4))
			return -TEST_FAILED;
	}

	for (i = 0; i < RTE_DIM(v6_tbl); i++) {
		tuple.v6.src_addr = v6_tbl[i].src_ip;
		tuple.v6.dst_addr = v6_tbl[i].dst_ip;
		tuple.v6.sport = rte_cpu_to_be_16(v6_tbl[i].dst_port);
		tuple.v6.dport = rte_cpu_to_be_16(v6_tbl[i].src_port);
		rss_l3 = rte_thash_gfni(rss_key_matrixes, (uint8_t *)&tuple,
				RTE_THASH_V6_L3_LEN * 4);
		rss_l3l4 = rte_thash_gfni(rss_key_matrixes, (uint8_t *)&tuple,
				RTE_THASH_V6_L4_LEN * 4);
		if ((rss_l3 != v6_tbl[i].hash_l3) ||
				(rss_l3l4 != v6_tbl[i].hash_l3l4))
			return -TEST_FAILED;
	}

	return TEST_SUCCESS;
}

#define DATA_SZ		4
#define ITER		1000

enum {
	SCALAR_DATA_BUF_1_HASH_IDX = 0,
	SCALAR_DATA_BUF_2_HASH_IDX,
	GFNI_DATA_BUF_1_HASH_IDX,
	GFNI_DATA_BUF_2_HASH_IDX,
	GFNI_BULK_DATA_BUF_1_HASH_IDX,
	GFNI_BULK_DATA_BUF_2_HASH_IDX,
	HASH_IDXES
};

static int
test_toeplitz_hash_rand_data(void)
{
	uint32_t data[2][DATA_SZ];
	uint32_t scalar_data[2][DATA_SZ];
	uint32_t hash[HASH_IDXES] = { 0 };
	uint64_t rss_key_matrixes[RTE_DIM(default_rss_key)];
	int i, j;
	uint8_t *bulk_data[2];

	if (!rte_thash_gfni_supported())
		return TEST_SKIPPED;

	rte_thash_complete_matrix(rss_key_matrixes, default_rss_key,
		RTE_DIM(default_rss_key));

	for (i = 0; i < 2; i++)
		bulk_data[i] = (uint8_t *)data[i];

	for (i = 0; i < ITER; i++) {
		for (j = 0; j < DATA_SZ; j++) {
			data[0][j] = rte_rand();
			data[1][j] = rte_rand();
			scalar_data[0][j] = rte_cpu_to_be_32(data[0][j]);
			scalar_data[1][j] = rte_cpu_to_be_32(data[1][j]);
		}

		hash[SCALAR_DATA_BUF_1_HASH_IDX] = rte_softrss(scalar_data[0],
			DATA_SZ, default_rss_key);
		hash[SCALAR_DATA_BUF_2_HASH_IDX] = rte_softrss(scalar_data[1],
			DATA_SZ, default_rss_key);
		hash[GFNI_DATA_BUF_1_HASH_IDX] = rte_thash_gfni(
			rss_key_matrixes, (uint8_t *)data[0],
			DATA_SZ * sizeof(uint32_t));
		hash[GFNI_DATA_BUF_2_HASH_IDX] = rte_thash_gfni(
			rss_key_matrixes, (uint8_t *)data[1],
			DATA_SZ * sizeof(uint32_t));
		rte_thash_gfni_bulk(rss_key_matrixes,
			DATA_SZ * sizeof(uint32_t), bulk_data,
			&hash[GFNI_BULK_DATA_BUF_1_HASH_IDX], 2);

		if ((hash[SCALAR_DATA_BUF_1_HASH_IDX] !=
				hash[GFNI_DATA_BUF_1_HASH_IDX]) ||
				(hash[SCALAR_DATA_BUF_1_HASH_IDX] !=
				hash[GFNI_BULK_DATA_BUF_1_HASH_IDX]) ||
				(hash[SCALAR_DATA_BUF_2_HASH_IDX] !=
				hash[GFNI_DATA_BUF_2_HASH_IDX]) ||
				(hash[SCALAR_DATA_BUF_2_HASH_IDX] !=
				hash[GFNI_BULK_DATA_BUF_2_HASH_IDX]))

			return -TEST_FAILED;
	}

	return TEST_SUCCESS;
}

enum {
	RSS_V4_IDX,
	RSS_V6_IDX
};

static int
test_toeplitz_hash_gfni_bulk(void)
{
	uint32_t i;
	union rte_thash_tuple tuple[2];
	uint8_t *tuples[2];
	uint32_t rss[2] = { 0 };
	uint64_t rss_key_matrixes[RTE_DIM(default_rss_key)];

	if (!rte_thash_gfni_supported())
		return TEST_SKIPPED;

	/* Convert RSS key into matrixes */
	rte_thash_complete_matrix(rss_key_matrixes, default_rss_key,
		RTE_DIM(default_rss_key));

	for (i = 0; i < RTE_DIM(tuples); i++) {
		/* allocate memory enough for a biggest tuple */
		tuples[i] = rte_zmalloc(NULL, RTE_THASH_V6_L4_LEN * 4, 0);
		if (tuples[i] == NULL)
			return -TEST_FAILED;
	}

	for (i = 0; i < RTE_MIN(RTE_DIM(v4_tbl), RTE_DIM(v6_tbl)); i++) {
		/*Load IPv4 headers and copy it into the corresponding tuple*/
		tuple[0].v4.src_addr = rte_cpu_to_be_32(v4_tbl[i].src_ip);
		tuple[0].v4.dst_addr = rte_cpu_to_be_32(v4_tbl[i].dst_ip);
		tuple[0].v4.sport = rte_cpu_to_be_16(v4_tbl[i].dst_port);
		tuple[0].v4.dport = rte_cpu_to_be_16(v4_tbl[i].src_port);
		rte_memcpy(tuples[0], &tuple[0], RTE_THASH_V4_L4_LEN * 4);

		/*Load IPv6 headers and copy it into the corresponding tuple*/
		tuple[1].v6.src_addr = v6_tbl[i].src_ip;
		tuple[1].v6.dst_addr = v6_tbl[i].dst_ip;
		tuple[1].v6.sport = rte_cpu_to_be_16(v6_tbl[i].dst_port);
		tuple[1].v6.dport = rte_cpu_to_be_16(v6_tbl[i].src_port);
		rte_memcpy(tuples[1], &tuple[1], RTE_THASH_V6_L4_LEN * 4);

		rte_thash_gfni_bulk(rss_key_matrixes, RTE_THASH_V6_L4_LEN * 4,
			tuples, rss, 2);

		if ((rss[RSS_V4_IDX] != v4_tbl[i].hash_l3l4) ||
				(rss[RSS_V6_IDX] != v6_tbl[i].hash_l3l4))
			return -TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_big_tuple_gfni(void)
{
	uint32_t arr[16];
	uint32_t arr_softrss[16];
	uint32_t hash_1, hash_2;
	uint64_t rss_key_matrixes[RTE_DIM(big_rss_key)];
	unsigned int i, size = RTE_DIM(arr) * sizeof(uint32_t);

	if (!rte_thash_gfni_supported())
		return TEST_SKIPPED;

	/* Convert RSS key into matrixes */
	rte_thash_complete_matrix(rss_key_matrixes, big_rss_key,
		RTE_DIM(big_rss_key));

	for (i = 0; i < RTE_DIM(arr); i++) {
		arr[i] = rte_rand();
		arr_softrss[i] = rte_be_to_cpu_32(arr[i]);
	}

	hash_1 = rte_softrss(arr_softrss, RTE_DIM(arr), big_rss_key);
	hash_2 = rte_thash_gfni(rss_key_matrixes, (uint8_t *)arr, size);

	if (hash_1 != hash_2)
		return -TEST_FAILED;

	return TEST_SUCCESS;
}

static int
test_create_invalid(void)
{
	struct rte_thash_ctx *ctx;
	int key_len = 40;
	int reta_sz = 7;

	ctx = rte_thash_init_ctx(NULL, key_len, reta_sz, NULL, 0);
	RTE_TEST_ASSERT(ctx == NULL,
		"Call succeeded with invalid parameters\n");

	ctx = rte_thash_init_ctx("test", 0, reta_sz, NULL, 0);
	RTE_TEST_ASSERT(ctx == NULL,
		"Call succeeded with invalid parameters\n");

	ctx = rte_thash_init_ctx(NULL, key_len, 1, NULL, 0);
	RTE_TEST_ASSERT(ctx == NULL,
		"Call succeeded with invalid parameters\n");

	ctx = rte_thash_init_ctx(NULL, key_len, 17, NULL, 0);
	RTE_TEST_ASSERT(ctx == NULL,
		"Call succeeded with invalid parameters\n");

	return TEST_SUCCESS;
}

static int
test_multiple_create(void)
{
	struct rte_thash_ctx *ctx;
	int key_len = 40;
	int reta_sz = 7;
	int i;

	for (i = 0; i < 100; i++) {
		ctx = rte_thash_init_ctx("test", key_len, reta_sz, NULL, 0);
		RTE_TEST_ASSERT(ctx != NULL, "Can not create CTX\n");

		rte_thash_free_ctx(ctx);
	}

	return TEST_SUCCESS;
}

static int
test_free_null(void)
{
	struct rte_thash_ctx *ctx;

	ctx = rte_thash_init_ctx("test", 40, 7, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "Can not create CTX\n");

	rte_thash_free_ctx(ctx);
	rte_thash_free_ctx(NULL);

	return TEST_SUCCESS;
}

static int
test_add_invalid_helper(void)
{
	struct rte_thash_ctx *ctx;
	const int key_len = 40;
	int reta_sz = 7;
	int ret;

	ctx = rte_thash_init_ctx("test", key_len, reta_sz, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "can not create thash ctx\n");

	ret = rte_thash_add_helper(NULL, "test", reta_sz, 0);
	RTE_TEST_ASSERT(ret == -EINVAL,
		"Call succeeded with invalid parameters\n");

	ret = rte_thash_add_helper(ctx, NULL, reta_sz, 0);
	RTE_TEST_ASSERT(ret == -EINVAL,
		"Call succeeded with invalid parameters\n");

	ret = rte_thash_add_helper(ctx, "test", reta_sz - 1, 0);
	RTE_TEST_ASSERT(ret == -EINVAL,
		"Call succeeded with invalid parameters\n");

	ret = rte_thash_add_helper(ctx, "test", reta_sz, key_len * 8);
	RTE_TEST_ASSERT(ret == -EINVAL,
		"Call succeeded with invalid parameters\n");

	ret = rte_thash_add_helper(ctx, "first_range", reta_sz, 0);
	RTE_TEST_ASSERT(ret == 0, "Can not create helper\n");

	ret = rte_thash_add_helper(ctx, "first_range", reta_sz, 0);
	RTE_TEST_ASSERT(ret == -EEXIST,
		"Call succeeded with duplicated name\n");

	/*
	 * Create second helper with offset 3 * reta_sz.
	 * Note first_range helper created range in key:
	 * [0, 32 + length{= reta_sz} - 1), i.e [0, 37).
	 * second range is [44, 81)
	 */
	ret = rte_thash_add_helper(ctx, "second_range", reta_sz,
		32 +  2 * reta_sz);
	RTE_TEST_ASSERT(ret == 0, "Can not create helper\n");

	/*
	 * Try to create overlapping with first_ and second_ ranges,
	 * i.e. [6, 49)
	 */
	ret = rte_thash_add_helper(ctx, "third_range", 2 * reta_sz, reta_sz);
	RTE_TEST_ASSERT(ret == -EEXIST,
		"Call succeeded with overlapping ranges\n");

	rte_thash_free_ctx(ctx);

	return TEST_SUCCESS;
}

static int
test_find_existing(void)
{
	struct rte_thash_ctx *ctx, *ret_ctx;

	ctx = rte_thash_init_ctx("test", 40, 7, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "can not create thash ctx\n");

	ret_ctx = rte_thash_find_existing("test");
	RTE_TEST_ASSERT(ret_ctx != NULL, "can not find existing ctx\n");

	rte_thash_free_ctx(ctx);

	return TEST_SUCCESS;
}

static int
test_get_helper(void)
{
	struct rte_thash_ctx *ctx;
	struct rte_thash_subtuple_helper *h;
	int ret;

	ctx = rte_thash_init_ctx("test", 40, 7, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "Can not create thash ctx\n");

	h = rte_thash_get_helper(NULL, "first_range");
	RTE_TEST_ASSERT(h == NULL, "Call succeeded with invalid parameters\n");

	h = rte_thash_get_helper(ctx, NULL);
	RTE_TEST_ASSERT(h == NULL, "Call succeeded with invalid parameters\n");

	ret = rte_thash_add_helper(ctx, "first_range", 8, 0);
	RTE_TEST_ASSERT(ret == 0, "Can not create helper\n");

	h = rte_thash_get_helper(ctx, "first_range");
	RTE_TEST_ASSERT(h != NULL, "Can not find helper\n");

	rte_thash_free_ctx(ctx);

	return TEST_SUCCESS;
}

static int
test_period_overflow(void)
{
	struct rte_thash_ctx *ctx;
	int reta_sz = 7; /* reflects polynomial degree */
	int ret;

	/* first create without RTE_THASH_IGNORE_PERIOD_OVERFLOW flag */
	ctx = rte_thash_init_ctx("test", 40, reta_sz, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "Can not create thash ctx\n");

	/* requested range > (2^reta_sz) - 1 */
	ret = rte_thash_add_helper(ctx, "test", (1 << reta_sz), 0);
	RTE_TEST_ASSERT(ret == -ENOSPC,
		"Call succeeded with invalid parameters\n");

	/* requested range == len + 32 - 1, smaller than (2^reta_sz) - 1 */
	ret = rte_thash_add_helper(ctx, "test", (1 << reta_sz) - 32, 0);
	RTE_TEST_ASSERT(ret == 0, "Can not create helper\n");

	rte_thash_free_ctx(ctx);

	/* create with RTE_THASH_IGNORE_PERIOD_OVERFLOW flag */
	ctx = rte_thash_init_ctx("test", 40, reta_sz, NULL,
		RTE_THASH_IGNORE_PERIOD_OVERFLOW);
	RTE_TEST_ASSERT(ctx != NULL, "Can not create thash ctx\n");

	/* requested range > (2^reta_sz - 1) */
	ret = rte_thash_add_helper(ctx, "test", (1 << reta_sz) + 10, 0);
	RTE_TEST_ASSERT(ret == 0, "Can not create helper\n");

	rte_thash_free_ctx(ctx);

	return TEST_SUCCESS;
}

static int
test_predictable_rss_min_seq(void)
{
	struct rte_thash_ctx *ctx;
	struct rte_thash_subtuple_helper *h;
	int reta_sz = 6;
	uint8_t initial_key[40];
	const uint8_t *new_key;
	int ret;
	union rte_thash_tuple tuple;
	uint32_t orig_hash, adj_hash, adj;
	unsigned int desired_value = 27 & HASH_MSK(reta_sz);
	uint16_t port_value = 22;

	memset(initial_key, 0, RTE_DIM(initial_key));

	ctx = rte_thash_init_ctx("test", RTE_DIM(initial_key), reta_sz, initial_key,
		RTE_THASH_MINIMAL_SEQ);
	RTE_TEST_ASSERT(ctx != NULL, "can not create thash ctx\n");

	ret = rte_thash_add_helper(ctx, "snat", sizeof(uint16_t) * 8,
		offsetof(union rte_thash_tuple, v4.sport) * 8);
	RTE_TEST_ASSERT(ret == 0, "can not add helper, ret %d\n", ret);

	h = rte_thash_get_helper(ctx, "snat");
	RTE_TEST_ASSERT(h != NULL, "can not find helper\n");

	new_key = rte_thash_get_key(ctx);
	tuple.v4.src_addr = RTE_IPV4(0, 0, 0, 0);
	tuple.v4.dst_addr = RTE_IPV4(0, 0, 0, 0);
	tuple.v4.sport = 0;
	tuple.v4.sport = rte_cpu_to_be_16(port_value);
	tuple.v4.dport = 0;
	tuple.v4.sctp_tag = rte_be_to_cpu_32(tuple.v4.sctp_tag);

	orig_hash = rte_softrss((uint32_t *)&tuple,
		RTE_THASH_V4_L4_LEN, new_key);
	adj = rte_thash_get_complement(h, orig_hash, desired_value);

	tuple.v4.sctp_tag = rte_cpu_to_be_32(tuple.v4.sctp_tag);
	tuple.v4.sport ^= rte_cpu_to_be_16(adj);
	tuple.v4.sctp_tag = rte_be_to_cpu_32(tuple.v4.sctp_tag);

	adj_hash = rte_softrss((uint32_t *)&tuple,
		RTE_THASH_V4_L4_LEN, new_key);
	RTE_TEST_ASSERT((adj_hash & HASH_MSK(reta_sz)) ==
		desired_value, "bad desired value\n");

	rte_thash_free_ctx(ctx);

	return TEST_SUCCESS;
}

/*
 * This test creates 7 subranges in the following order:
 * range_one	= [56, 95),	len = 8, offset = 56
 * range_two	= [64, 103),	len = 8, offset = 64
 * range_three	= [120, 159),	len = 8, offset = 120
 * range_four	= [48, 87),	len = 8, offset = 48
 * range_five	= [57, 95),	len = 7, offset = 57
 * range_six	= [40, 111),	len = 40, offset = 40
 * range_seven	= [0, 39),	len = 8, offset = 0
 */
struct range {
	const char *name;
	int len;
	int offset;
	int byte_idx;
};

struct range rng_arr[] = {
	{"one",   8,  56,  7},
	{"two",   8,  64,  8},
	{"three", 8,  120, 15},
	{"four",  8,  48,  6},
	{"six",   40, 40,  9},
	{"five",  7,  57,  7},
	{"seven", 8,  0,   0}
};

static int
test_predictable_rss_multirange(void)
{
	struct rte_thash_ctx *ctx;
	struct rte_thash_subtuple_helper *h[RTE_DIM(rng_arr)];
	const uint8_t *new_key;
	const int key_len = 40;
	int reta_sz = 7;
	unsigned int i, j, k;
	int ret;
	uint32_t desired_value = rte_rand() & HASH_MSK(reta_sz);
	uint8_t tuples[RTE_DIM(rng_arr)][16] = { {0} };
	uint32_t *ptr;
	uint32_t hashes[RTE_DIM(rng_arr)];
	uint32_t adj_hashes[RTE_DIM(rng_arr)];
	uint32_t adj;

	ctx = rte_thash_init_ctx("test", key_len, reta_sz, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "can not create thash ctx\n");

	for (i = 0; i < RTE_DIM(rng_arr); i++) {
		ret = rte_thash_add_helper(ctx, rng_arr[i].name,
			rng_arr[i].len, rng_arr[i].offset);
		RTE_TEST_ASSERT(ret == 0, "can not add helper\n");

		h[i] = rte_thash_get_helper(ctx, rng_arr[i].name);
		RTE_TEST_ASSERT(h[i] != NULL, "can not find helper\n");
	}
	new_key = rte_thash_get_key(ctx);

	/*
	 * calculate hashes, complements, then adjust keys with
	 * complements and recalculate hashes
	 */
	for (i = 0; i < RTE_DIM(rng_arr); i++) {
		for (k = 0; k < 100; k++) {
			/* init with random keys */
			ptr = (uint32_t *)&tuples[i][0];
			for (j = 0; j < 4; j++)
				ptr[j] = rte_rand();
			/* convert keys from BE to CPU byte order */
			for (j = 0; j < 4; j++)
				ptr[j] = rte_be_to_cpu_32(ptr[j]);

			hashes[i] = rte_softrss(ptr, 4, new_key);
			adj = rte_thash_get_complement(h[i], hashes[i],
				desired_value);
			/* convert back to BE to adjust the value */
			for (j = 0; j < 4; j++)
				ptr[j] = rte_cpu_to_be_32(ptr[j]);

			tuples[i][rng_arr[i].byte_idx] ^= adj;

			for (j = 0; j < 4; j++)
				ptr[j] = rte_be_to_cpu_32(ptr[j]);

			adj_hashes[i] = rte_softrss(ptr, 4, new_key);
			RTE_TEST_ASSERT((adj_hashes[i] & HASH_MSK(reta_sz)) ==
				desired_value,
				"bad desired value for %d tuple\n", i);
		}
	}

	rte_thash_free_ctx(ctx);

	return TEST_SUCCESS;
}

static int
cmp_tuple_eq(void *userdata, uint8_t *tuple)
{
	return memcmp(userdata, tuple, TUPLE_SZ);
}

static int
test_adjust_tuple(void)
{
	struct rte_thash_ctx *ctx;
	struct rte_thash_subtuple_helper *h;
	const int key_len = 40;
	const uint8_t *new_key;
	uint8_t tuple[TUPLE_SZ];
	uint32_t tmp_tuple[TUPLE_SZ / sizeof(uint32_t)];
	uint32_t tuple_copy[TUPLE_SZ / sizeof(uint32_t)];
	uint32_t hash;
	int reta_sz = CHAR_BIT;
	int ret;
	unsigned int i, desired_value = rte_rand() & HASH_MSK(reta_sz);

	memset(tuple, 0xab, TUPLE_SZ);

	ctx = rte_thash_init_ctx("test", key_len, reta_sz, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "can not create thash ctx\n");

	/*
	 * set offset to be in the middle of a byte
	 * set size of the subtuple to be 2 * rets_sz
	 * to have the room for random bits
	 */
	ret = rte_thash_add_helper(ctx, "test", reta_sz * 2,
		(5 * CHAR_BIT) + 4);
	RTE_TEST_ASSERT(ret == 0, "can not add helper, ret %d\n", ret);

	new_key = rte_thash_get_key(ctx);

	h = rte_thash_get_helper(ctx, "test");
	RTE_TEST_ASSERT(h != NULL, "can not find helper\n");

	ret = rte_thash_adjust_tuple(ctx, h, tuple, TUPLE_SZ, desired_value,
		1, NULL, NULL);
	RTE_TEST_ASSERT(ret == 0, "can not adjust tuple, ret %d\n", ret);

	for (i = 0; i < (TUPLE_SZ / 4); i++)
		tmp_tuple[i] =
			rte_be_to_cpu_32(*(uint32_t *)&tuple[i * 4]);

	hash = rte_softrss(tmp_tuple, TUPLE_SZ / 4, new_key);
	RTE_TEST_ASSERT((hash & HASH_MSK(reta_sz)) ==
		desired_value, "bad desired value\n");


	/* Pass previously calculated tuple to callback function */
	memcpy(tuple_copy, tuple, TUPLE_SZ);

	memset(tuple, 0xab, TUPLE_SZ);
	ret = rte_thash_adjust_tuple(ctx, h, tuple, TUPLE_SZ, desired_value,
		1, cmp_tuple_eq, tuple_copy);
	RTE_TEST_ASSERT(ret == -EEXIST,
		"adjust tuple didn't indicate collision\n");

	/*
	 * Make the function to generate random bits into subtuple
	 * after first adjustment attempt.
	 */
	memset(tuple, 0xab, TUPLE_SZ);
	ret = rte_thash_adjust_tuple(ctx, h, tuple, TUPLE_SZ, desired_value,
		2, cmp_tuple_eq, tuple_copy);
	RTE_TEST_ASSERT(ret == 0, "can not adjust tuple, ret %d\n", ret);

	for (i = 0; i < (TUPLE_SZ / 4); i++)
		tmp_tuple[i] =
			rte_be_to_cpu_32(*(uint32_t *)&tuple[i * 4]);

	hash = rte_softrss(tmp_tuple, TUPLE_SZ / 4, new_key);
	RTE_TEST_ASSERT((hash & HASH_MSK(reta_sz)) ==
		desired_value, "bad desired value\n");

	rte_thash_free_ctx(ctx);

	return TEST_SUCCESS;
}

static uint32_t
calc_tuple_hash(const uint8_t tuple[TUPLE_SZ], const uint8_t *key)
{
	uint32_t i, hash;
	uint32_t tmp[TUPLE_SZ / sizeof(uint32_t)];

	for (i = 0; i < RTE_DIM(tmp); i++)
		tmp[i] = rte_be_to_cpu_32(
			*(const uint32_t *)&tuple[i * sizeof(uint32_t)]);

	hash = rte_softrss(tmp, RTE_DIM(tmp), key);
	return hash;
}

static int
check_adj_tuple(const uint8_t tuple[TUPLE_SZ], const uint8_t *key,
		uint32_t dhv, uint32_t ohv, uint32_t adjust, uint32_t reta_sz,
		const char *prefix)
{
	uint32_t hash, hashlsb;

	hash = calc_tuple_hash(tuple, key);
	hashlsb = hash & HASH_MSK(reta_sz);

	printf("%s(%s) for tuple:\n", __func__, prefix);
	rte_memdump(stdout, NULL, tuple, TUPLE_SZ);
	printf("\treta_sz: %u,\n"
		"\torig hash: %#x,\n"
		"\tdesired: %#x,\n"
		"\tadjust: %#x,\n"
		"\tactual: %#x,\n",
	       reta_sz, ohv, dhv, adjust, hashlsb);

	if (dhv == hashlsb) {
		printf("\t***Succeeded\n");
		return 0;
	}

	printf("\t***Failed\n");
	return -1;
}

static int
test_adjust_tuple_mb(uint32_t reta_sz, uint32_t bofs)
{
	struct rte_thash_ctx *ctx;
	struct rte_thash_subtuple_helper *h;
	const int key_len = 40;
	const uint8_t *new_key;
	uint8_t orig_tuple[TUPLE_SZ];
	uint8_t tuple_1[TUPLE_SZ];
	uint8_t tuple_2[TUPLE_SZ];
	uint32_t orig_hash;
	int rc, ret;
	uint32_t adj_bits;
	unsigned int random = rte_rand();
	unsigned int desired_value = random & HASH_MSK(reta_sz);

	const uint32_t h_offset = offsetof(union rte_thash_tuple, v4.dport) * CHAR_BIT;
	const uint32_t h_size = sizeof(uint16_t) * CHAR_BIT - bofs;

	printf("===%s(reta_sz=%u,bofs=%u)===\n", __func__, reta_sz, bofs);

	memset(orig_tuple, 0xab, sizeof(orig_tuple));

	ctx = rte_thash_init_ctx("test", key_len, reta_sz, NULL, 0);
	RTE_TEST_ASSERT(ctx != NULL, "can not create thash ctx\n");

	ret = rte_thash_add_helper(ctx, "test", h_size, h_offset);
	RTE_TEST_ASSERT(ret == 0, "can not add helper, ret %d\n", ret);

	new_key = rte_thash_get_key(ctx);

	h = rte_thash_get_helper(ctx, "test");

	orig_hash = calc_tuple_hash(orig_tuple, new_key);

	adj_bits = rte_thash_get_complement(h, orig_hash, desired_value);

	/* use method #1, update tuple manually */
	memcpy(tuple_1, orig_tuple, sizeof(tuple_1));
	{
		uint16_t nv, ov, *p;

		p = (uint16_t *)(tuple_1 + h_offset / CHAR_BIT);
		ov = p[0];
		nv = ov ^ rte_cpu_to_be_16(adj_bits << bofs);
		printf("%s#%d: ov=%#hx, nv=%#hx, adj=%#x;\n",
			__func__, __LINE__, ov, nv, adj_bits);
		p[0] = nv;
	}

	rc = check_adj_tuple(tuple_1, new_key, desired_value, orig_hash,
		adj_bits, reta_sz, "method #1");
	if (h_offset % CHAR_BIT == 0)
		ret |= rc;

	/* use method #2, use library function to adjust tuple */
	memcpy(tuple_2, orig_tuple, sizeof(tuple_2));

	rte_thash_adjust_tuple(ctx, h, tuple_2, sizeof(tuple_2),
		desired_value, 1, NULL, NULL);
	ret |= check_adj_tuple(tuple_2, new_key, desired_value, orig_hash,
		adj_bits, reta_sz, "method #2");

	rte_thash_free_ctx(ctx);

	ret |= memcmp(tuple_1, tuple_2, sizeof(tuple_1));

	printf("%s EXIT=======\n", __func__);
	return ret;
}

static int
test_adjust_tuple_mult_reta(void)
{
	uint32_t i, j, np, nt;

	nt = 0, np = 0;
	for (i = 0; i < CHAR_BIT; i++) {
		for (j = 6; j <= RTE_THASH_RETA_SZ_MAX - i; j++) {
			np += (test_adjust_tuple_mb(j, i) == 0);
			nt++;
		}
	}

	printf("%s: tests executed: %u, test passed: %u\n", __func__, nt, np);
	RTE_TEST_ASSERT(nt == np, "%u subtests failed", nt - np);
	return TEST_SUCCESS;
}

#define RETA_SZ_LOG		11
#define RSS_KEY_SZ		40
#define RETA_SZ			(1 << RETA_SZ_LOG)
#define NB_HASH_ITER	RETA_SZ
#define NB_TEST_ITER	10

static inline void
run_hash_calc_loop(uint8_t *key, union rte_thash_tuple *tuple,
					unsigned int *rss_reta_hits)
{
	uint32_t rss_hash;
	int i;

	for (i = 0; i < NB_HASH_ITER; i++) {
		/* variable part starts from the most significant bit */
		tuple->v4.dport = (i << (sizeof(tuple->v4.dport) * CHAR_BIT -
			RETA_SZ_LOG));
		/*
		 * swap sport and dport on LE arch since rte_softrss()
		 * works with host byte order uint32_t values
		 */
		tuple->v4.dport = rte_cpu_to_be_16(tuple->v4.dport);
		tuple->v4.sctp_tag = rte_be_to_cpu_32(tuple->v4.sctp_tag);
		rss_hash = rte_softrss((uint32_t *)tuple,
				RTE_THASH_V4_L4_LEN, key);
		/* unroll swap, required only for sport */
		tuple->v4.sctp_tag = rte_cpu_to_be_32(tuple->v4.sctp_tag);
		rss_reta_hits[rss_hash & (RETA_SZ - 1)]++;
	}
}

static int
hash_calc_iteration(unsigned int *min_before, unsigned int *max_before,
		unsigned int *min_after, unsigned int *max_after,
		unsigned int *min_default, unsigned int *max_default)
{
	uint8_t key[RSS_KEY_SZ] = {0};
	union rte_thash_tuple tuple;
	unsigned int rss_reta_hits_before_adjust[RETA_SZ] = {0};
	unsigned int rss_reta_hits_after_adjust[RETA_SZ] = {0};
	unsigned int rss_reta_hits_default_key[RETA_SZ] = {0};
	int i;

	for (i = 0; i < RSS_KEY_SZ; i++)
		key[i] = rte_rand();

	tuple.v4.src_addr = rte_rand();
	tuple.v4.dst_addr = rte_rand();
	tuple.v4.sport = rte_rand();

	run_hash_calc_loop(key, &tuple, rss_reta_hits_before_adjust);

	int ret = rte_thash_gen_key(key, RSS_KEY_SZ, RETA_SZ_LOG,
		offsetof(union rte_thash_tuple, v4.dport)*CHAR_BIT,
		RETA_SZ_LOG);

	if (ret) {
		printf("Can't generate key\n");
		return -1;
	}

	run_hash_calc_loop(key, &tuple, rss_reta_hits_after_adjust);

	run_hash_calc_loop(default_rss_key, &tuple, rss_reta_hits_default_key);

	for (i = 0; i < RETA_SZ; i++) {
		*min_before = RTE_MIN(*min_before, rss_reta_hits_before_adjust[i]);
		*max_before = RTE_MAX(*max_before, rss_reta_hits_before_adjust[i]);
		*min_after = RTE_MIN(*min_after, rss_reta_hits_after_adjust[i]);
		*max_after = RTE_MAX(*max_after, rss_reta_hits_after_adjust[i]);
		*min_default = RTE_MIN(*min_default, rss_reta_hits_default_key[i]);
		*max_default = RTE_MAX(*max_default, rss_reta_hits_default_key[i]);
	}

	return 0;
}

static int
test_keygen(void)
{
	int i, ret;
	unsigned int min_before = UINT32_MAX;
	unsigned int min_after = UINT32_MAX;
	unsigned int min_default = UINT32_MAX;
	unsigned int max_before = 0;
	unsigned int max_after = 0;
	unsigned int max_default = 0;

	for (i = 0; i < NB_TEST_ITER; i++) {
		/* calculates the worst distribution for each key */
		ret = hash_calc_iteration(&min_before, &max_before, &min_after,
			&max_after, &min_default, &max_default);
		if (ret)
			return ret;
	}

	printf("RSS before key adjustment: min=%d, max=%d\n",
		min_before, max_before);
	printf("RSS after key adjustment: min=%d, max=%d\n",
		min_after, max_after);
	printf("RSS default key: min=%d, max=%d\n",
		min_default, max_default);

	return TEST_SUCCESS;
}

static struct unit_test_suite thash_tests = {
	.suite_name = "thash autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
	TEST_CASE(test_toeplitz_hash_calc),
	TEST_CASE(test_toeplitz_hash_gfni),
	TEST_CASE(test_toeplitz_hash_rand_data),
	TEST_CASE(test_toeplitz_hash_gfni_bulk),
	TEST_CASE(test_big_tuple_gfni),
	TEST_CASE(test_create_invalid),
	TEST_CASE(test_multiple_create),
	TEST_CASE(test_free_null),
	TEST_CASE(test_add_invalid_helper),
	TEST_CASE(test_find_existing),
	TEST_CASE(test_get_helper),
	TEST_CASE(test_period_overflow),
	TEST_CASE(test_predictable_rss_min_seq),
	TEST_CASE(test_predictable_rss_multirange),
	TEST_CASE(test_adjust_tuple),
	TEST_CASE(test_adjust_tuple_mult_reta),
	TEST_CASE(test_keygen),
	TEST_CASES_END()
	}
};

static int
test_thash(void)
{
	return unit_test_suite_runner(&thash_tests);
}

REGISTER_FAST_TEST(thash_autotest, true, true, test_thash);
