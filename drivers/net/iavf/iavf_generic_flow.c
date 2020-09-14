/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "iavf.h"
#include "iavf_generic_flow.h"

static struct iavf_engine_list engine_list =
		TAILQ_HEAD_INITIALIZER(engine_list);

static int iavf_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);
static struct rte_flow *iavf_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);
static int iavf_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error);
static int iavf_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error);

const struct rte_flow_ops iavf_flow_ops = {
	.validate = iavf_flow_validate,
	.create = iavf_flow_create,
	.destroy = iavf_flow_destroy,
	.flush = iavf_flow_flush,
	.query = iavf_flow_query,
};

/* empty */
enum rte_flow_item_type iavf_pattern_empty[] = {
	RTE_FLOW_ITEM_TYPE_END,
};

/* L2 */
enum rte_flow_item_type iavf_pattern_ethertype[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_ethertype_vlan[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_ethertype_qinq[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

/* ARP */
enum rte_flow_item_type iavf_pattern_eth_arp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

/* non-tunnel IPv4 */
enum rte_flow_item_type iavf_pattern_eth_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* non-tunnel IPv6 */
enum rte_flow_item_type iavf_pattern_eth_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 GTPC */
enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpc[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPC,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV4 GTPU (EH) */
enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 GTPC */
enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpc[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPC,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV6 GTPU (EH) */
enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV4 GTPU IPv4 */
enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV4 GTPU IPv6 */
enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV6 GTPU IPv4 */
enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV6 GTPU IPv6 */
enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV4 GTPU EH IPv4 */
enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV4 GTPU EH IPv6 */
enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV6 GTPU EH IPv4 */
enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPV6 GTPU EH IPv6 */
enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* ESP */
enum rte_flow_item_type iavf_pattern_eth_ipv4_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv4_udp_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_udp_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* AH */
enum rte_flow_item_type iavf_pattern_eth_ipv4_ah[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_AH,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_ah[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_AH,
	RTE_FLOW_ITEM_TYPE_END,
};

/* L2TPV3 */
enum rte_flow_item_type iavf_pattern_eth_ipv4_l2tpv3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_L2TPV3OIP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_l2tpv3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_L2TPV3OIP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* PFCP */
enum rte_flow_item_type iavf_pattern_eth_ipv4_pfcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_PFCP,
	RTE_FLOW_ITEM_TYPE_END,
};

enum rte_flow_item_type iavf_pattern_eth_ipv6_pfcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_PFCP,
	RTE_FLOW_ITEM_TYPE_END,
};

typedef struct iavf_flow_engine * (*parse_engine_t)(struct iavf_adapter *ad,
		struct rte_flow *flow,
		struct iavf_parser_list *parser_list,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);

void
iavf_register_flow_engine(struct iavf_flow_engine *engine)
{
	TAILQ_INSERT_TAIL(&engine_list, engine, node);
}

int
iavf_flow_init(struct iavf_adapter *ad)
{
	int ret;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	void *temp;
	struct iavf_flow_engine *engine;

	TAILQ_INIT(&vf->flow_list);
	TAILQ_INIT(&vf->rss_parser_list);
	TAILQ_INIT(&vf->dist_parser_list);
	rte_spinlock_init(&vf->flow_ops_lock);

	TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
		if (engine->init == NULL) {
			PMD_INIT_LOG(ERR, "Invalid engine type (%d)",
				     engine->type);
			return -ENOTSUP;
		}

		ret = engine->init(ad);
		if (ret && ret != -ENOTSUP) {
			PMD_INIT_LOG(ERR, "Failed to initialize engine %d",
				     engine->type);
			return ret;
		}
	}
	return 0;
}

void
iavf_flow_uninit(struct iavf_adapter *ad)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_flow_engine *engine;
	struct rte_flow *p_flow;
	struct iavf_flow_parser_node *p_parser;
	void *temp;

	TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
		if (engine->uninit)
			engine->uninit(ad);
	}

	/* Remove all flows */
	while ((p_flow = TAILQ_FIRST(&vf->flow_list))) {
		TAILQ_REMOVE(&vf->flow_list, p_flow, node);
		if (p_flow->engine->free)
			p_flow->engine->free(p_flow);
		rte_free(p_flow);
	}

	/* Cleanup parser list */
	while ((p_parser = TAILQ_FIRST(&vf->rss_parser_list))) {
		TAILQ_REMOVE(&vf->rss_parser_list, p_parser, node);
		rte_free(p_parser);
	}

	while ((p_parser = TAILQ_FIRST(&vf->dist_parser_list))) {
		TAILQ_REMOVE(&vf->dist_parser_list, p_parser, node);
		rte_free(p_parser);
	}
}

int
iavf_register_parser(struct iavf_flow_parser *parser,
		     struct iavf_adapter *ad)
{
	struct iavf_parser_list *list = NULL;
	struct iavf_flow_parser_node *parser_node;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);

	parser_node = rte_zmalloc("iavf_parser", sizeof(*parser_node), 0);
	if (parser_node == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory.");
		return -ENOMEM;
	}
	parser_node->parser = parser;

	if (parser->engine->type == IAVF_FLOW_ENGINE_HASH) {
		list = &vf->rss_parser_list;
		TAILQ_INSERT_TAIL(list, parser_node, node);
	} else if (parser->engine->type == IAVF_FLOW_ENGINE_FDIR) {
		list = &vf->dist_parser_list;
		TAILQ_INSERT_HEAD(list, parser_node, node);
	} else {
		return -EINVAL;
	}

	return 0;
}

void
iavf_unregister_parser(struct iavf_flow_parser *parser,
		       struct iavf_adapter *ad)
{
	struct iavf_parser_list *list = NULL;
	struct iavf_flow_parser_node *p_parser;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	void *temp;

	if (parser->engine->type == IAVF_FLOW_ENGINE_HASH)
		list = &vf->rss_parser_list;
	else if (parser->engine->type == IAVF_FLOW_ENGINE_FDIR)
		list = &vf->dist_parser_list;

	if (list == NULL)
		return;

	TAILQ_FOREACH_SAFE(p_parser, list, node, temp) {
		if (p_parser->parser->engine->type == parser->engine->type) {
			TAILQ_REMOVE(list, p_parser, node);
			rte_free(p_parser);
		}
	}
}

static int
iavf_flow_valid_attr(const struct rte_flow_attr *attr,
		     struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				attr, "Only support ingress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->egress) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				attr, "Not support egress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->priority) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				attr, "Not support priority.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->group) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				attr, "Not support group.");
		return -rte_errno;
	}

	return 0;
}

/* Find the first VOID or non-VOID item pointer */
static const struct rte_flow_item *
iavf_find_first_item(const struct rte_flow_item *item, bool is_void)
{
	bool is_find;

	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		if (is_void)
			is_find = item->type == RTE_FLOW_ITEM_TYPE_VOID;
		else
			is_find = item->type != RTE_FLOW_ITEM_TYPE_VOID;
		if (is_find)
			break;
		item++;
	}
	return item;
}

/* Skip all VOID items of the pattern */
static void
iavf_pattern_skip_void_item(struct rte_flow_item *items,
			const struct rte_flow_item *pattern)
{
	uint32_t cpy_count = 0;
	const struct rte_flow_item *pb = pattern, *pe = pattern;

	for (;;) {
		/* Find a non-void item first */
		pb = iavf_find_first_item(pb, false);
		if (pb->type == RTE_FLOW_ITEM_TYPE_END) {
			pe = pb;
			break;
		}

		/* Find a void item */
		pe = iavf_find_first_item(pb + 1, true);

		cpy_count = pe - pb;
		rte_memcpy(items, pb, sizeof(struct rte_flow_item) * cpy_count);

		items += cpy_count;

		if (pe->type == RTE_FLOW_ITEM_TYPE_END)
			break;

		pb = pe + 1;
	}
	/* Copy the END item. */
	rte_memcpy(items, pe, sizeof(struct rte_flow_item));
}

/* Check if the pattern matches a supported item type array */
static bool
iavf_match_pattern(enum rte_flow_item_type *item_array,
		   const struct rte_flow_item *pattern)
{
	const struct rte_flow_item *item = pattern;

	while ((*item_array == item->type) &&
	       (*item_array != RTE_FLOW_ITEM_TYPE_END)) {
		item_array++;
		item++;
	}

	return (*item_array == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

struct iavf_pattern_match_item *
iavf_search_pattern_match_item(const struct rte_flow_item pattern[],
		struct iavf_pattern_match_item *array,
		uint32_t array_len,
		struct rte_flow_error *error)
{
	uint16_t i = 0;
	struct iavf_pattern_match_item *pattern_match_item;
	/* need free by each filter */
	struct rte_flow_item *items; /* used for pattern without VOID items */
	uint32_t item_num = 0; /* non-void item number */

	/* Get the non-void item number of pattern */
	while ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_END) {
		if ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_VOID)
			item_num++;
		i++;
	}
	item_num++;

	items = rte_zmalloc("iavf_pattern",
			    item_num * sizeof(struct rte_flow_item), 0);
	if (!items) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "No memory for PMD internal items.");
		return NULL;
	}
	pattern_match_item = rte_zmalloc("iavf_pattern_match_item",
				sizeof(struct iavf_pattern_match_item), 0);
	if (!pattern_match_item) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to allocate memory.");
		return NULL;
	}

	iavf_pattern_skip_void_item(items, pattern);

	for (i = 0; i < array_len; i++)
		if (iavf_match_pattern(array[i].pattern_list,
				       items)) {
			pattern_match_item->input_set_mask =
				array[i].input_set_mask;
			pattern_match_item->pattern_list =
				array[i].pattern_list;
			pattern_match_item->meta = array[i].meta;
			rte_free(items);
			return pattern_match_item;
		}
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			   pattern, "Unsupported pattern");

	rte_free(items);
	rte_free(pattern_match_item);
	return NULL;
}

static struct iavf_flow_engine *
iavf_parse_engine_create(struct iavf_adapter *ad,
		struct rte_flow *flow,
		struct iavf_parser_list *parser_list,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct iavf_flow_engine *engine = NULL;
	struct iavf_flow_parser_node *parser_node;
	void *temp;
	void *meta = NULL;

	TAILQ_FOREACH_SAFE(parser_node, parser_list, node, temp) {
		if (parser_node->parser->parse_pattern_action(ad,
				parser_node->parser->array,
				parser_node->parser->array_len,
				pattern, actions, &meta, error) < 0)
			continue;

		engine = parser_node->parser->engine;

		RTE_ASSERT(engine->create != NULL);
		if (!(engine->create(ad, flow, meta, error)))
			return engine;
	}
	return NULL;
}

static struct iavf_flow_engine *
iavf_parse_engine_validate(struct iavf_adapter *ad,
		struct rte_flow *flow,
		struct iavf_parser_list *parser_list,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct iavf_flow_engine *engine = NULL;
	struct iavf_flow_parser_node *parser_node;
	void *temp;
	void *meta = NULL;

	TAILQ_FOREACH_SAFE(parser_node, parser_list, node, temp) {
		if (parser_node->parser->parse_pattern_action(ad,
				parser_node->parser->array,
				parser_node->parser->array_len,
				pattern, actions, &meta,  error) < 0)
			continue;

		engine = parser_node->parser->engine;
		if (engine->validation == NULL) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Validation not support");
			continue;
		}

		if (engine->validation(ad, flow, meta, error)) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Validation failed");
			break;
		}
	}
	return engine;
}


static int
iavf_flow_process_filter(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct iavf_flow_engine **engine,
		parse_engine_t iavf_parse_engine,
		struct rte_flow_error *error)
{
	int ret = IAVF_ERR_CONFIG;
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "NULL pattern.");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -rte_errno;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -rte_errno;
	}

	ret = iavf_flow_valid_attr(attr, error);
	if (ret)
		return ret;

	*engine = iavf_parse_engine(ad, flow, &vf->rss_parser_list, pattern,
				    actions, error);
	if (*engine)
		return 0;

	*engine = iavf_parse_engine(ad, flow, &vf->dist_parser_list, pattern,
				    actions, error);

	if (!*engine) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to create parser engine.");
		return -rte_errno;
	}

	return 0;
}

static int
iavf_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct iavf_flow_engine *engine;

	return iavf_flow_process_filter(dev, NULL, attr, pattern, actions,
			&engine, iavf_parse_engine_validate, error);
}

static struct rte_flow *
iavf_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_flow_engine *engine = NULL;
	struct rte_flow *flow = NULL;
	int ret;

	flow = rte_zmalloc("iavf_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return flow;
	}

	ret = iavf_flow_process_filter(dev, flow, attr, pattern, actions,
			&engine, iavf_parse_engine_create, error);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to create flow");
		rte_free(flow);
		flow = NULL;
		goto free_flow;
	}

	flow->engine = engine;
	TAILQ_INSERT_TAIL(&vf->flow_list, flow, node);
	PMD_DRV_LOG(INFO, "Succeeded to create (%d) flow", engine->type);

free_flow:
	rte_spinlock_unlock(&vf->flow_ops_lock);
	return flow;
}

static bool
iavf_flow_is_valid(struct rte_flow *flow)
{
	struct iavf_flow_engine *engine;
	void *temp;

	if (flow && flow->engine) {
		TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
			if (engine == flow->engine)
				return true;
		}
	}

	return false;
}

static int
iavf_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	int ret = 0;

	if (!iavf_flow_is_valid(flow) || !flow->engine->destroy) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Invalid flow destroy");
		return -rte_errno;
	}

	rte_spinlock_lock(&vf->flow_ops_lock);

	ret = flow->engine->destroy(ad, flow, error);

	if (!ret) {
		TAILQ_REMOVE(&vf->flow_list, flow, node);
		rte_free(flow);
	} else {
		PMD_DRV_LOG(ERR, "Failed to destroy flow");
	}

	rte_spinlock_unlock(&vf->flow_ops_lock);

	return ret;
}

int
iavf_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct rte_flow *p_flow;
	void *temp;
	int ret = 0;

	TAILQ_FOREACH_SAFE(p_flow, &vf->flow_list, node, temp) {
		ret = iavf_flow_destroy(dev, p_flow, error);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to flush flows");
			return -EINVAL;
		}
	}

	return ret;
}

static int
iavf_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error)
{
	int ret = -EINVAL;
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_flow_query_count *count = data;

	if (!iavf_flow_is_valid(flow) || !flow->engine->query_count) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Invalid flow query");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow->engine->query_count(ad, flow, count, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions,
					"action not supported");
		}
	}
	return ret;
}

