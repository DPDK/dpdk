/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_FLOW_H_
#define _HINIC3_FLOW_H_

#include <rte_flow.h>

/* Indicate the type of the IPv4 ICPM matching pattern. */
static enum rte_flow_item_type pattern_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 any protocol matching pattern. */
static enum rte_flow_item_type pattern_ipv4_any[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ANY,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the Ether matching pattern. */
static enum rte_flow_item_type pattern_ethertype[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the TCP matching pattern. */
static enum rte_flow_item_type pattern_ethertype_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the UDP matching pattern. */
static enum rte_flow_item_type pattern_ethertype_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan any protocol matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_any[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ANY, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan TCP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_TCP, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan UDP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan IPv4 matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan IPv4 TCP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan IPv4 UDP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan IPv6 matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan IPv6 TCP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 vxlan IPv6 UDP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_vxlan_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 matching pattern. */
static enum rte_flow_item_type pattern_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 UDP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv4 TCP matching pattern. */
static enum rte_flow_item_type pattern_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv6 matching pattern. */
static enum rte_flow_item_type pattern_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv6 UDP matching pattern. */
static enum rte_flow_item_type pattern_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv6 TCP matching pattern. */
static enum rte_flow_item_type pattern_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv6 TCP matching pattern. */
static enum rte_flow_item_type pattern_ipv6_vxlan[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv6 VXLAN any protocol matching pattern. */
static enum rte_flow_item_type pattern_ipv6_vxlan_any[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ANY, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv6 VXLAN TCP matching pattern. */
static enum rte_flow_item_type pattern_ipv6_vxlan_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_TCP, RTE_FLOW_ITEM_TYPE_END,
};

/* Indicate the type of the IPv6 VXLAN UDP matching pattern. */
static enum rte_flow_item_type pattern_ipv6_vxlan_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH, RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_END,
};

#endif /**< _HINIC3_FLOW_H_ */
