/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Stephen Hemminger
 */

#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include <rte_test.h>
#include "test.h"

#define N 1000000

static const struct rte_ether_addr zero_ea;
static const struct rte_ether_addr bcast_ea = {
	.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
};

static struct rte_mempool *net_ether_test_pool;

static int
test_ether_addr(void)
{
	struct rte_ether_addr rand_ea = { };
	unsigned int i;

	RTE_TEST_ASSERT(rte_is_zero_ether_addr(&zero_ea), "Zero address is not zero");
	RTE_TEST_ASSERT(!rte_is_zero_ether_addr(&bcast_ea), "Broadcast is zero");

	for (i = 0; i < N; i++) {
		rte_eth_random_addr(rand_ea.addr_bytes);
		RTE_TEST_ASSERT(!rte_is_zero_ether_addr(&rand_ea),
				"Random address is zero");
		RTE_TEST_ASSERT(rte_is_unicast_ether_addr(&rand_ea),
				"Random address is not unicast");
		RTE_TEST_ASSERT(rte_is_local_admin_ether_addr(&rand_ea),
				"Random address is not local admin");
	}

	return 0;
}

static int
test_format_addr(void)
{
	struct rte_ether_addr rand_ea = { };
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	unsigned int i;

	for (i = 0; i < N; i++) {
		struct rte_ether_addr result = { };
		int ret;

		rte_eth_random_addr(rand_ea.addr_bytes);

		rte_ether_format_addr(buf, sizeof(buf), &rand_ea);

		ret = rte_ether_unformat_addr(buf, &result);
		if (ret != 0) {
			fprintf(stderr, "rte_ether_unformat_addr(%s) failed\n", buf);
			return -1;
		}
		RTE_TEST_ASSERT(rte_is_same_ether_addr(&rand_ea, &result),
			"rte_ether_format/unformat mismatch");
	}
	return 0;

}

static int
test_unformat_addr(void)
{
	const struct rte_ether_addr expected = {
		.addr_bytes = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc },
	};
	const struct rte_ether_addr nozero_ea = {
		.addr_bytes = { 1, 2, 3, 4, 5, 6 },
	};
	struct rte_ether_addr result;
	int ret;

	/* Test IETF format */
	memset(&result, 0, sizeof(result));
	ret = rte_ether_unformat_addr("12:34:56:78:9a:bc", &result);
	RTE_TEST_ASSERT(ret == 0, "IETF unformat failed");
	RTE_TEST_ASSERT(rte_is_same_ether_addr(&expected, &result),
		"IETF unformat mismatch");

	/* Test IEEE format */
	memset(&result, 0, sizeof(result));
	ret = rte_ether_unformat_addr("12-34-56-78-9A-BC", &result);
	RTE_TEST_ASSERT(ret == 0, "IEEE unformat failed");
	RTE_TEST_ASSERT(rte_is_same_ether_addr(&expected, &result),
			"IEEE unformat mismatch");

	/* Test Cisco format */
	memset(&result, 0, sizeof(result));
	ret = rte_ether_unformat_addr("1234.5678.9ABC", &result);
	RTE_TEST_ASSERT(ret == 0, "Cisco unformat failed");
	RTE_TEST_ASSERT(rte_is_same_ether_addr(&expected, &result),
			"Cisco unformat mismatch");

	/* Test no leading zeros - IETF */
	memset(&result, 0, sizeof(result));
	ret = rte_ether_unformat_addr("1:2:3:4:5:6", &result);
	RTE_TEST_ASSERT(ret == 0, "IETF leading zero failed");
	RTE_TEST_ASSERT(rte_is_same_ether_addr(&nozero_ea, &result),
			"IETF leading zero mismatch");

	/* Test no-leading zero - IEEE format */
	memset(&result, 0, sizeof(result));
	ret = rte_ether_unformat_addr("1-2-3-4-5-6", &result);
	RTE_TEST_ASSERT(ret == 0, "IEEE leading zero failed");
	RTE_TEST_ASSERT(rte_is_same_ether_addr(&nozero_ea, &result),
			"IEEE leading zero mismatch");


	return 0;
}

static int
test_invalid_addr(void)
{
	static const char * const invalid[] = {
		"123",
		"123:456",
		"12:34:56:78:9a:gh",
		"12:34:56:78:9a",
		"100:34:56:78:9a:bc",
		"34-56-78-9a-bc",
		"12:34:56-78:9a:bc",
		"12:34:56.78:9a:bc",
		"123:456:789:abc",
		"NOT.AN.ADDRESS",
		"102.304.506",
		"",
	};
	struct rte_ether_addr result;
	unsigned int i;

	for (i = 0; i < RTE_DIM(invalid); ++i) {
		if (!rte_ether_unformat_addr(invalid[i], &result)) {
			fprintf(stderr, "rte_ether_unformat_addr(%s) succeeded!\n",
				invalid[i]);
			return -1;
		}
	}
	return 0;
}

/*
 * Build a minimal Ethernet frame in an mbuf: Ethernet header with the given
 * ether_type followed by payload_len bytes of padding.
 */
static struct rte_mbuf *
alloc_frame(struct rte_mempool *mp, uint16_t ether_type, uint16_t payload_len)
{
	struct rte_ether_hdr *eh;
	struct rte_mbuf *m;

	m = rte_pktmbuf_alloc(mp);
	if (m == NULL)
		return NULL;

	eh = (struct rte_ether_hdr *)rte_pktmbuf_append(m,
			sizeof(*eh) + payload_len);
	if (eh == NULL) {
		rte_pktmbuf_free(m);
		return NULL;
	}

	memset(eh->dst_addr.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);
	memset(eh->src_addr.addr_bytes, 0x00, RTE_ETHER_ADDR_LEN);
	eh->ether_type = rte_cpu_to_be_16(ether_type);

	return m;
}

static int
test_vlan_insert_8021q(void)
{
	struct rte_ether_hdr *eh;
	struct rte_vlan_hdr *vh;
	struct rte_mbuf *m;
	int ret;

	m = alloc_frame(net_ether_test_pool, RTE_ETHER_TYPE_IPV4, 46);
	TEST_ASSERT_NOT_NULL(m, "Failed to allocate mbuf");

	m->vlan_tci = 100;
	m->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;

	ret = rte_vlan_insert(&m);
	TEST_ASSERT_SUCCESS(ret, "rte_vlan_insert failed");

	eh = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	TEST_ASSERT_EQUAL(rte_be_to_cpu_16(eh->ether_type), RTE_ETHER_TYPE_VLAN,
		"Expected 802.1Q TPID 0x%04x, got 0x%04x",
		RTE_ETHER_TYPE_VLAN, rte_be_to_cpu_16(eh->ether_type));

	vh = (struct rte_vlan_hdr *)(eh + 1);
	TEST_ASSERT_EQUAL(rte_be_to_cpu_16(vh->vlan_tci), 100,
		"Expected VID 100, got %u", rte_be_to_cpu_16(vh->vlan_tci));

	rte_pktmbuf_free(m);
	return TEST_SUCCESS;
}

static int
test_vlan_insert_tpid(void)
{
	struct rte_ether_hdr *eh;
	struct rte_vlan_hdr *vh;
	struct rte_mbuf *m;
	int ret;

	m = alloc_frame(net_ether_test_pool, RTE_ETHER_TYPE_VLAN, sizeof(*vh) + 46);
	TEST_ASSERT_NOT_NULL(m, "Failed to allocate mbuf");

	vh = (struct rte_vlan_hdr *)(rte_pktmbuf_mtod(m, struct rte_ether_hdr *) + 1);
	vh->vlan_tci = rte_cpu_to_be_16(200);
	vh->eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

	m->vlan_tci = 50;
	m->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;

	ret = rte_vlan_insert_tpid(&m, RTE_ETHER_TYPE_QINQ);
	TEST_ASSERT_SUCCESS(ret, "rte_vlan_insert_tpid failed");

	eh = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	TEST_ASSERT_EQUAL(rte_be_to_cpu_16(eh->ether_type), RTE_ETHER_TYPE_QINQ,
		"Outer TPID: expected 0x%04x (802.1ad) got 0x%04x",
		RTE_ETHER_TYPE_QINQ, rte_be_to_cpu_16(eh->ether_type));

	vh = (struct rte_vlan_hdr *)(eh + 1);
	TEST_ASSERT_EQUAL(rte_be_to_cpu_16(vh->vlan_tci), 50,
		"Outer VID: expected 50, got %u",
		rte_be_to_cpu_16(vh->vlan_tci));
	TEST_ASSERT_EQUAL(rte_be_to_cpu_16(vh->eth_proto), RTE_ETHER_TYPE_VLAN,
		"Outer eth_proto: expected 0x%04x (802.1Q), got 0x%04x",
		RTE_ETHER_TYPE_VLAN, rte_be_to_cpu_16(vh->eth_proto));

	vh = vh + 1;
	TEST_ASSERT_EQUAL(rte_be_to_cpu_16(vh->vlan_tci), 200,
		"Inner VID: expected 200, got %u",
		rte_be_to_cpu_16(vh->vlan_tci));
	TEST_ASSERT_EQUAL(rte_be_to_cpu_16(vh->eth_proto), RTE_ETHER_TYPE_IPV4,
		"Inner eth_proto: expected 0x%04x (IPv4), got 0x%04x",
		RTE_ETHER_TYPE_IPV4, rte_be_to_cpu_16(vh->eth_proto));

	rte_pktmbuf_free(m);
	return TEST_SUCCESS;
}

static int
net_ether_testsuite_setup(void)
{
	net_ether_test_pool = rte_pktmbuf_pool_create("net_ether_test_pool", 64, 0, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, SOCKET_ID_ANY);
	TEST_ASSERT_NOT_NULL(net_ether_test_pool, "Failed to create mempool");

	return TEST_SUCCESS;
}

static void
net_ether_testsuite_teardown(void)
{
	rte_mempool_free(net_ether_test_pool);
	net_ether_test_pool = NULL;
}

static struct unit_test_suite net_ether_testsuite = {
	.suite_name = "net_ether autotest",
	.setup = net_ether_testsuite_setup,
	.teardown = net_ether_testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE(test_ether_addr),
		TEST_CASE(test_format_addr),
		TEST_CASE(test_unformat_addr),
		TEST_CASE(test_invalid_addr),
		TEST_CASE(test_vlan_insert_8021q),
		TEST_CASE(test_vlan_insert_tpid),
		TEST_CASES_END()
	}
};

static int
test_net_ether(void)
{
	return unit_test_suite_runner(&net_ether_testsuite);
}

REGISTER_FAST_TEST(net_ether_autotest, NOHUGE_OK, ASAN_OK, test_net_ether);
