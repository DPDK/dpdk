/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Stephen Hemminger
 */

#include <rte_ether.h>

#include <rte_test.h>
#include "test.h"

#define N 1000000

static const struct rte_ether_addr zero_ea;
static const struct rte_ether_addr bcast_ea = {
	.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
};

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

static int
test_net_ether(void)
{
	if (test_ether_addr())
		return -1;

	if (test_format_addr())
		return -1;

	if (test_unformat_addr())
		return -1;

	if (test_invalid_addr())
		return -1;

	return 0;
}

REGISTER_FAST_TEST(net_ether_autotest, true, true, test_net_ether);
