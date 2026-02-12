/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Robin Jarry
 */

#include <rte_ip6.h>

#include "test.h"

static const struct rte_ipv6_addr mask_full = RTE_IPV6_MASK_FULL;
static const struct rte_ipv6_addr zero_addr = RTE_IPV6_ADDR_UNSPEC;

static int
test_ipv6_check_version(void)
{
	struct rte_ipv6_hdr h;

	h.vtc_flow = 0;
	TEST_ASSERT_EQUAL(rte_ipv6_check_version(&h), -EINVAL, "");
	h.vtc_flow = RTE_BE32(0x7f00ba44);
	TEST_ASSERT_EQUAL(rte_ipv6_check_version(&h), -EINVAL, "");
	h.vtc_flow = RTE_BE32(0x6badcaca);
	TEST_ASSERT_EQUAL(rte_ipv6_check_version(&h), 0, "");

	return 0;
}

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
	TEST_ASSERT(!rte_ipv6_addr_is_linklocal(&zero_addr), "");
	TEST_ASSERT(!rte_ipv6_addr_is_loopback(&zero_addr), "");
	TEST_ASSERT(!rte_ipv6_addr_is_mcast(&zero_addr), "");
	TEST_ASSERT(rte_ipv6_addr_is_v4compat(&zero_addr), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4mapped(&zero_addr), "");

	const struct rte_ipv6_addr ucast =
		RTE_IPV6(0x2a01, 0xcb00, 0x0254, 0x3300, 0x6239, 0xe1f4, 0x7a0b, 0x2371);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&ucast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_linklocal(&ucast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_loopback(&ucast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_mcast(&ucast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4compat(&ucast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4mapped(&ucast), "");

	const struct rte_ipv6_addr mcast = RTE_IPV6(0xff01, 0, 0, 0, 0, 0, 0, 1);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&mcast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_linklocal(&mcast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_loopback(&mcast), "");
	TEST_ASSERT(rte_ipv6_addr_is_mcast(&mcast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4compat(&mcast), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4mapped(&mcast), "");

	const struct rte_ipv6_addr lo = RTE_IPV6_ADDR_LOOPBACK;
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&lo), "");
	TEST_ASSERT(!rte_ipv6_addr_is_linklocal(&lo), "");
	TEST_ASSERT(rte_ipv6_addr_is_loopback(&lo), "");
	TEST_ASSERT(!rte_ipv6_addr_is_mcast(&lo), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4compat(&lo), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4mapped(&lo), "");

	const struct rte_ipv6_addr local =
		RTE_IPV6(0xfe80, 0, 0, 0, 0x5a84, 0xc52c, 0x6aef, 0x4639);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&local), "");
	TEST_ASSERT(rte_ipv6_addr_is_linklocal(&local), "");
	TEST_ASSERT(!rte_ipv6_addr_is_loopback(&local), "");
	TEST_ASSERT(!rte_ipv6_addr_is_mcast(&local), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4compat(&local), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4mapped(&local), "");

	const struct rte_ipv6_addr v4compat = RTE_IPV6(0, 0, 0, 0, 0, 0, 0xc0a8, 0x0001);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&v4compat), "");
	TEST_ASSERT(!rte_ipv6_addr_is_linklocal(&v4compat), "");
	TEST_ASSERT(!rte_ipv6_addr_is_loopback(&v4compat), "");
	TEST_ASSERT(!rte_ipv6_addr_is_mcast(&v4compat), "");
	TEST_ASSERT(rte_ipv6_addr_is_v4compat(&v4compat), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4mapped(&v4compat), "");

	const struct rte_ipv6_addr v4mapped = RTE_IPV6(0, 0, 0, 0, 0, 0xffff, 0xc0a8, 0x0001);
	TEST_ASSERT(!rte_ipv6_addr_is_unspec(&v4mapped), "");
	TEST_ASSERT(!rte_ipv6_addr_is_linklocal(&v4mapped), "");
	TEST_ASSERT(!rte_ipv6_addr_is_loopback(&v4mapped), "");
	TEST_ASSERT(!rte_ipv6_addr_is_mcast(&v4mapped), "");
	TEST_ASSERT(!rte_ipv6_addr_is_v4compat(&v4mapped), "");
	TEST_ASSERT(rte_ipv6_addr_is_v4mapped(&v4mapped), "");

	return TEST_SUCCESS;
}

static int
test_ipv6_llocal_from_ethernet(void)
{
	const struct rte_ether_addr local_mac = {{0x04, 0x7b, 0xcb, 0x5c, 0x08, 0x44}};
	const struct rte_ipv6_addr local_ip =
		RTE_IPV6(0xfe80, 0, 0, 0, 0x067b, 0xcbff, 0xfe5c, 0x0844);
	struct rte_ipv6_addr ip;

	rte_ipv6_llocal_from_ethernet(&ip, &local_mac);
	TEST_ASSERT(rte_ipv6_addr_eq(&ip, &local_ip), "");

	return TEST_SUCCESS;
}

static int
test_ipv6_solnode_from_addr(void)
{
	struct rte_ipv6_addr sol;

	const struct rte_ipv6_addr llocal =
		RTE_IPV6(0xfe80, 0, 0, 0, 0x047b, 0xcbff, 0xfe5c, 0x0844);
	const struct rte_ipv6_addr llocal_sol =
		RTE_IPV6(0xff02, 0, 0, 0, 0, 0x0001, 0xff5c, 0x0844);
	rte_ipv6_solnode_from_addr(&sol, &llocal);
	TEST_ASSERT(rte_ipv6_addr_eq(&sol, &llocal_sol), "");

	const struct rte_ipv6_addr ucast =
		RTE_IPV6(0x2a01, 0xcb00, 0x0254, 0x3300, 0x1b9f, 0x8071, 0x67cd, 0xbf20);
	const struct rte_ipv6_addr ucast_sol =
		RTE_IPV6(0xff02, 0, 0, 0, 0, 0x0001, 0xffcd, 0xbf20);
	rte_ipv6_solnode_from_addr(&sol, &ucast);
	TEST_ASSERT(rte_ipv6_addr_eq(&sol, &ucast_sol), "");

	return TEST_SUCCESS;
}

static int
test_ether_mcast_from_ipv6(void)
{
	const struct rte_ether_addr mcast_mac = {{0x33, 0x33, 0xd3, 0x00, 0x02, 0x01}};
	const struct rte_ipv6_addr mcast_ip =
		RTE_IPV6(0xff02, 0, 0, 0x0201, 0, 0, 0xd300, 0x0201);
	struct rte_ether_addr mac;

	rte_ether_mcast_from_ipv6(&mac, &mcast_ip);
	TEST_ASSERT(rte_is_same_ether_addr(&mac, &mcast_mac), "");

	return TEST_SUCCESS;
}

static int
test_net_ipv6(void)
{
	TEST_ASSERT_SUCCESS(test_ipv6_check_version(), "");
	TEST_ASSERT_SUCCESS(test_ipv6_addr_mask(), "");
	TEST_ASSERT_SUCCESS(test_ipv6_addr_eq_prefix(), "");
	TEST_ASSERT_SUCCESS(test_ipv6_addr_kind(), "");
	TEST_ASSERT_SUCCESS(test_ipv6_llocal_from_ethernet(), "");
	TEST_ASSERT_SUCCESS(test_ipv6_solnode_from_addr(), "");
	TEST_ASSERT_SUCCESS(test_ether_mcast_from_ipv6(), "");
	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(net_ipv6_autotest, NOHUGE_OK, ASAN_OK, test_net_ipv6);
