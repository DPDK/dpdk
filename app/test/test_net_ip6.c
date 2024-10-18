/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Robin Jarry
 */

#include <rte_ip6.h>

#include "test.h"

static const struct rte_ipv6_addr mask_full = RTE_IPV6_MASK_FULL;
static const struct rte_ipv6_addr zero_addr = RTE_IPV6_ADDR_UNSPEC;

static int
test_ipv6_addr_mask(void)
{
	const struct rte_ipv6_addr masked_3 = RTE_IPV6(0xe000, 0, 0, 0, 0, 0, 0, 0);
	const struct rte_ipv6_addr masked_42 = RTE_IPV6(0xffff, 0xffff, 0xffc0, 0, 0, 0, 0, 0);
	const struct rte_ipv6_addr masked_85 =
		RTE_IPV6(0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf800, 0, 0);
	const struct rte_ipv6_addr masked_127 =
		RTE_IPV6(0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xfffe);
	struct rte_ipv6_addr ip;

	ip = mask_full;
	rte_ipv6_addr_mask(&ip, 0);
	TEST_ASSERT(rte_ipv6_addr_eq(&ip, &zero_addr), "");
	TEST_ASSERT_EQUAL(rte_ipv6_mask_depth(&zero_addr), 0, "");

	ip = mask_full;
	rte_ipv6_addr_mask(&ip, 3);
	TEST_ASSERT(rte_ipv6_addr_eq(&ip, &masked_3), "");
	TEST_ASSERT_EQUAL(rte_ipv6_mask_depth(&masked_3), 3, "");

	ip = mask_full;
	rte_ipv6_addr_mask(&ip, 42);
	TEST_ASSERT(rte_ipv6_addr_eq(&ip, &masked_42), "");
	TEST_ASSERT_EQUAL(rte_ipv6_mask_depth(&masked_42), 42, "");

	ip = mask_full;
	rte_ipv6_addr_mask(&ip, 85);
	TEST_ASSERT(rte_ipv6_addr_eq(&ip, &masked_85), "");
	TEST_ASSERT_EQUAL(rte_ipv6_mask_depth(&masked_85), 85, "");

	ip = mask_full;
	rte_ipv6_addr_mask(&ip, 127);
	TEST_ASSERT(rte_ipv6_addr_eq(&ip, &masked_127), "");
	TEST_ASSERT_EQUAL(rte_ipv6_mask_depth(&masked_127), 127, "");

	ip = mask_full;
	rte_ipv6_addr_mask(&ip, 128);
	TEST_ASSERT(rte_ipv6_addr_eq(&ip, &mask_full), "");
	TEST_ASSERT_EQUAL(rte_ipv6_mask_depth(&mask_full), 128, "");

	const struct rte_ipv6_addr mask_holed =
		RTE_IPV6(0xffff, 0xffff, 0xffff, 0xefff, 0xffff, 0xffff, 0xffff, 0xffff);
	TEST_ASSERT_EQUAL(rte_ipv6_mask_depth(&mask_holed), 51, "");

	return TEST_SUCCESS;
}

static int
test_ipv6_addr_eq_prefix(void)
{
	const struct rte_ipv6_addr ip1 =
		RTE_IPV6(0x2a01, 0xcb00, 0x0254, 0x3300, 0x1b9f, 0x8071, 0x67cd, 0xbf20);
	const struct rte_ipv6_addr ip2 =
		RTE_IPV6(0x2a01, 0xcb00, 0x0254, 0x3300, 0x6239, 0xe1f4, 0x7a0b, 0x2371);
	const struct rte_ipv6_addr ip3 =
		RTE_IPV6(0xfd10, 0x0039, 0x0208, 0x0001, 0x0000, 0x0000, 0x0000, 0x1008);

	TEST_ASSERT(rte_ipv6_addr_eq_prefix(&ip1, &ip2, 1), "");
	TEST_ASSERT(rte_ipv6_addr_eq_prefix(&ip1, &ip2, 37), "");
	TEST_ASSERT(rte_ipv6_addr_eq_prefix(&ip1, &ip2, 64), "");
	TEST_ASSERT(!rte_ipv6_addr_eq_prefix(&ip1, &ip2, 112), "");
	TEST_ASSERT(rte_ipv6_addr_eq_prefix(&ip1, &ip3, 0), "");
	TEST_ASSERT(!rte_ipv6_addr_eq_prefix(&ip1, &ip3, 13), "");

	return TEST_SUCCESS;
}

static int
test_ipv6_addr_kind(void)
{
	TEST_ASSERT(rte_ipv6_addr_is_unspec(&zero_addr), "");

	const struct rte_ipv6_addr ucast =
		RTE_IPV6(0x2a01, 0xcb00, 0x0254, 0x3300, 0x6239, 0xe1f4, 0x7a0b, 0x2371);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&ucast), "");

	const struct rte_ipv6_addr mcast = RTE_IPV6(0xff01, 0, 0, 0, 0, 0, 0, 1);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&mcast), "");

	const struct rte_ipv6_addr lo = RTE_IPV6_ADDR_LOOPBACK;
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&lo), "");

	const struct rte_ipv6_addr local =
		RTE_IPV6(0xfe80, 0, 0, 0, 0x5a84, 0xc52c, 0x6aef, 0x4639);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&local), "");

	return TEST_SUCCESS;
}

static int
test_net_ipv6(void)
{
	TEST_ASSERT_SUCCESS(test_ipv6_addr_mask(), "");
	TEST_ASSERT_SUCCESS(test_ipv6_addr_eq_prefix(), "");
	TEST_ASSERT_SUCCESS(test_ipv6_addr_kind(), "");
	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(net_ipv6_autotest, true, true, test_net_ipv6);
