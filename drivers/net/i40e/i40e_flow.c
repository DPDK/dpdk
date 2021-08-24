/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>
#include <rte_bitmap.h>

#include "i40e_logs.h"
#include "base/i40e_type.h"
#include "base/i40e_prototype.h"
#include "i40e_ethdev.h"
#include "i40e_hash.h"

#define I40E_IPV6_TC_MASK	(0xFF << I40E_FDIR_IPv6_TC_OFFSET)
#define I40E_IPV6_FRAG_HEADER	44
#define I40E_TENANT_ARRAY_NUM	3
#define I40E_VLAN_TCI_MASK	0xFFFF
#define I40E_VLAN_PRI_MASK	0xE000
#define I40E_VLAN_CFI_MASK	0x1000
#define I40E_VLAN_VID_MASK	0x0FFF

static int i40e_flow_validate(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error);
static struct rte_flow *i40e_flow_create(struct rte_eth_dev *dev,
					 const struct rte_flow_attr *attr,
					 const struct rte_flow_item pattern[],
					 const struct rte_flow_action actions[],
					 struct rte_flow_error *error);
static int i40e_flow_destroy(struct rte_eth_dev *dev,
			     struct rte_flow *flow,
			     struct rte_flow_error *error);
static int i40e_flow_flush(struct rte_eth_dev *dev,
			   struct rte_flow_error *error);
static int i40e_flow_query(struct rte_eth_dev *dev,
			   struct rte_flow *flow,
			   const struct rte_flow_action *actions,
			   void *data, struct rte_flow_error *error);
static int
i40e_flow_parse_ethertype_pattern(struct rte_eth_dev *dev,
				  const struct rte_flow_item *pattern,
				  struct rte_flow_error *error,
				  struct rte_eth_ethertype_filter *filter);
static int i40e_flow_parse_ethertype_action(struct rte_eth_dev *dev,
				    const struct rte_flow_action *actions,
				    struct rte_flow_error *error,
				    struct rte_eth_ethertype_filter *filter);
static int i40e_flow_parse_fdir_pattern(struct rte_eth_dev *dev,
					const struct rte_flow_attr *attr,
					const struct rte_flow_item *pattern,
					struct rte_flow_error *error,
					struct i40e_fdir_filter_conf *filter);
static int i40e_flow_parse_fdir_action(struct rte_eth_dev *dev,
				       const struct rte_flow_action *actions,
				       struct rte_flow_error *error,
				       struct i40e_fdir_filter_conf *filter);
static int i40e_flow_parse_tunnel_action(struct rte_eth_dev *dev,
				 const struct rte_flow_action *actions,
				 struct rte_flow_error *error,
				 struct i40e_tunnel_filter_conf *filter);
static int i40e_flow_parse_attr(const struct rte_flow_attr *attr,
				struct rte_flow_error *error);
static int i40e_flow_parse_ethertype_filter(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item pattern[],
				    const struct rte_flow_action actions[],
				    struct rte_flow_error *error,
				    union i40e_filter_t *filter);
static int i40e_flow_parse_fdir_filter(struct rte_eth_dev *dev,
				       const struct rte_flow_attr *attr,
				       const struct rte_flow_item pattern[],
				       const struct rte_flow_action actions[],
				       struct rte_flow_error *error,
				       union i40e_filter_t *filter);
static int i40e_flow_parse_vxlan_filter(struct rte_eth_dev *dev,
					const struct rte_flow_attr *attr,
					const struct rte_flow_item pattern[],
					const struct rte_flow_action actions[],
					struct rte_flow_error *error,
					union i40e_filter_t *filter);
static int i40e_flow_parse_nvgre_filter(struct rte_eth_dev *dev,
					const struct rte_flow_attr *attr,
					const struct rte_flow_item pattern[],
					const struct rte_flow_action actions[],
					struct rte_flow_error *error,
					union i40e_filter_t *filter);
static int i40e_flow_parse_mpls_filter(struct rte_eth_dev *dev,
				       const struct rte_flow_attr *attr,
				       const struct rte_flow_item pattern[],
				       const struct rte_flow_action actions[],
				       struct rte_flow_error *error,
				       union i40e_filter_t *filter);
static int i40e_flow_parse_gtp_filter(struct rte_eth_dev *dev,
				      const struct rte_flow_attr *attr,
				      const struct rte_flow_item pattern[],
				      const struct rte_flow_action actions[],
				      struct rte_flow_error *error,
				      union i40e_filter_t *filter);
static int i40e_flow_destroy_ethertype_filter(struct i40e_pf *pf,
				      struct i40e_ethertype_filter *filter);
static int i40e_flow_destroy_tunnel_filter(struct i40e_pf *pf,
					   struct i40e_tunnel_filter *filter);
static int i40e_flow_flush_fdir_filter(struct i40e_pf *pf);
static int i40e_flow_flush_ethertype_filter(struct i40e_pf *pf);
static int i40e_flow_flush_tunnel_filter(struct i40e_pf *pf);
static int
i40e_flow_parse_qinq_filter(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error,
			      union i40e_filter_t *filter);
static int
i40e_flow_parse_qinq_pattern(struct rte_eth_dev *dev,
			      const struct rte_flow_item *pattern,
			      struct rte_flow_error *error,
			      struct i40e_tunnel_filter_conf *filter);

static int i40e_flow_parse_l4_cloud_filter(struct rte_eth_dev *dev,
					   const struct rte_flow_attr *attr,
					   const struct rte_flow_item pattern[],
					   const struct rte_flow_action actions[],
					   struct rte_flow_error *error,
					   union i40e_filter_t *filter);
const struct rte_flow_ops i40e_flow_ops = {
	.validate = i40e_flow_validate,
	.create = i40e_flow_create,
	.destroy = i40e_flow_destroy,
	.flush = i40e_flow_flush,
	.query = i40e_flow_query,
};

static union i40e_filter_t cons_filter;
static enum rte_filter_type cons_filter_type = RTE_ETH_FILTER_NONE;
/* internal pattern w/o VOID items */
struct rte_flow_item g_items[32];

/* Pattern matched ethertype filter */
static enum rte_flow_item_type pattern_ethertype[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Pattern matched flow director filter */
static enum rte_flow_item_type pattern_fdir_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_gtpc[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPC,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_gtpu[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_gtpu_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_gtpu_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_gtpc[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPC,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_gtpu[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_gtpu_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_gtpu_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp_raw_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp_raw_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp_raw_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ethertype_vlan_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_udp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_tcp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv4_sctp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_udp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_tcp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp_raw_1_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp_raw_2_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_vlan_ipv6_sctp_raw_3_vf[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_RAW,
	RTE_FLOW_ITEM_TYPE_VF,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Pattern matched tunnel filter */
static enum rte_flow_item_type pattern_vxlan_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_vxlan_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_vxlan_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_vxlan_4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_nvgre_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_nvgre_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_nvgre_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_nvgre_4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_mpls_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_MPLS,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_mpls_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_MPLS,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_mpls_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_GRE,
	RTE_FLOW_ITEM_TYPE_MPLS,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_mpls_4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_GRE,
	RTE_FLOW_ITEM_TYPE_MPLS,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_qinq_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_l2tpv3oip[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_L2TPV3OIP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_l2tpv3oip[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_L2TPV3OIP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_esp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_END,
};

static struct i40e_valid_pattern i40e_supported_patterns[] = {
	/* Ethertype */
	{ pattern_ethertype, i40e_flow_parse_ethertype_filter },
	/* FDIR - support default flow type without flexible payload*/
	{ pattern_ethertype, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_gtpc, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_gtpu, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_gtpu_ipv4, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_gtpu_ipv6, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_esp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_esp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_gtpc, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_gtpu, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_gtpu_ipv4, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_gtpu_ipv6, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_esp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_esp, i40e_flow_parse_fdir_filter },
	/* FDIR - support default flow type with flexible payload */
	{ pattern_fdir_ethertype_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_raw_3, i40e_flow_parse_fdir_filter },
	/* FDIR - support single vlan input set */
	{ pattern_fdir_ethertype_vlan, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_vlan_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_vlan_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_vlan_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp_raw_3, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp_raw_1, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp_raw_2, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp_raw_3, i40e_flow_parse_fdir_filter },
	/* FDIR - support VF item */
	{ pattern_fdir_ipv4_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_vlan_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_vlan_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_vlan_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ethertype_vlan_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_udp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_tcp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv4_sctp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_udp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_tcp_raw_3_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp_raw_1_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp_raw_2_vf, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_vlan_ipv6_sctp_raw_3_vf, i40e_flow_parse_fdir_filter },
	/* VXLAN */
	{ pattern_vxlan_1, i40e_flow_parse_vxlan_filter },
	{ pattern_vxlan_2, i40e_flow_parse_vxlan_filter },
	{ pattern_vxlan_3, i40e_flow_parse_vxlan_filter },
	{ pattern_vxlan_4, i40e_flow_parse_vxlan_filter },
	/* NVGRE */
	{ pattern_nvgre_1, i40e_flow_parse_nvgre_filter },
	{ pattern_nvgre_2, i40e_flow_parse_nvgre_filter },
	{ pattern_nvgre_3, i40e_flow_parse_nvgre_filter },
	{ pattern_nvgre_4, i40e_flow_parse_nvgre_filter },
	/* MPLSoUDP & MPLSoGRE */
	{ pattern_mpls_1, i40e_flow_parse_mpls_filter },
	{ pattern_mpls_2, i40e_flow_parse_mpls_filter },
	{ pattern_mpls_3, i40e_flow_parse_mpls_filter },
	{ pattern_mpls_4, i40e_flow_parse_mpls_filter },
	/* GTP-C & GTP-U */
	{ pattern_fdir_ipv4_gtpc, i40e_flow_parse_gtp_filter },
	{ pattern_fdir_ipv4_gtpu, i40e_flow_parse_gtp_filter },
	{ pattern_fdir_ipv6_gtpc, i40e_flow_parse_gtp_filter },
	{ pattern_fdir_ipv6_gtpu, i40e_flow_parse_gtp_filter },
	/* QINQ */
	{ pattern_qinq_1, i40e_flow_parse_qinq_filter },
	/* L2TPv3 over IP */
	{ pattern_fdir_ipv4_l2tpv3oip, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_l2tpv3oip, i40e_flow_parse_fdir_filter },
	/* L4 over port */
	{ pattern_fdir_ipv4_udp, i40e_flow_parse_l4_cloud_filter },
	{ pattern_fdir_ipv4_tcp, i40e_flow_parse_l4_cloud_filter },
	{ pattern_fdir_ipv4_sctp, i40e_flow_parse_l4_cloud_filter },
	{ pattern_fdir_ipv6_udp, i40e_flow_parse_l4_cloud_filter },
	{ pattern_fdir_ipv6_tcp, i40e_flow_parse_l4_cloud_filter },
	{ pattern_fdir_ipv6_sctp, i40e_flow_parse_l4_cloud_filter },
};

#define NEXT_ITEM_OF_ACTION(act, actions, index)                        \
	do {                                                            \
		act = actions + index;                                  \
		while (act->type == RTE_FLOW_ACTION_TYPE_VOID) {        \
			index++;                                        \
			act = actions + index;                          \
		}                                                       \
	} while (0)

/* Find the first VOID or non-VOID item pointer */
static const struct rte_flow_item *
i40e_find_first_item(const struct rte_flow_item *item, bool is_void)
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
i40e_pattern_skip_void_item(struct rte_flow_item *items,
			    const struct rte_flow_item *pattern)
{
	uint32_t cpy_count = 0;
	const struct rte_flow_item *pb = pattern, *pe = pattern;

	for (;;) {
		/* Find a non-void item first */
		pb = i40e_find_first_item(pb, false);
		if (pb->type == RTE_FLOW_ITEM_TYPE_END) {
			pe = pb;
			break;
		}

		/* Find a void item */
		pe = i40e_find_first_item(pb + 1, true);

		cpy_count = pe - pb;
		rte_memcpy(items, pb, sizeof(struct rte_flow_item) * cpy_count);

		items += cpy_count;

		if (pe->type == RTE_FLOW_ITEM_TYPE_END) {
			pb = pe;
			break;
		}

		pb = pe + 1;
	}
	/* Copy the END item. */
	rte_memcpy(items, pe, sizeof(struct rte_flow_item));
}

/* Check if the pattern matches a supported item type array */
static bool
i40e_match_pattern(enum rte_flow_item_type *item_array,
		   struct rte_flow_item *pattern)
{
	struct rte_flow_item *item = pattern;

	while ((*item_array == item->type) &&
	       (*item_array != RTE_FLOW_ITEM_TYPE_END)) {
		item_array++;
		item++;
	}

	return (*item_array == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

/* Find if there's parse filter function matched */
static parse_filter_t
i40e_find_parse_filter_func(struct rte_flow_item *pattern, uint32_t *idx)
{
	parse_filter_t parse_filter = NULL;
	uint8_t i = *idx;

	for (; i < RTE_DIM(i40e_supported_patterns); i++) {
		if (i40e_match_pattern(i40e_supported_patterns[i].items,
					pattern)) {
			parse_filter = i40e_supported_patterns[i].parse_filter;
			break;
		}
	}

	*idx = ++i;

	return parse_filter;
}

/* Parse attributes */
static int
i40e_flow_parse_attr(const struct rte_flow_attr *attr,
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

static uint16_t
i40e_get_outer_vlan(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int qinq = dev->data->dev_conf.rxmode.offloads &
		DEV_RX_OFFLOAD_VLAN_EXTEND;
	uint64_t reg_r = 0;
	uint16_t reg_id;
	uint16_t tpid;

	if (qinq)
		reg_id = 2;
	else
		reg_id = 3;

	i40e_aq_debug_read_register(hw, I40E_GL_SWT_L2TAGCTRL(reg_id),
				    &reg_r, NULL);

	tpid = (reg_r >> I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_SHIFT) & 0xFFFF;

	return tpid;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: MAC_ETHTYPE and ETHTYPE.
 * 3. SRC mac_addr mask should be 00:00:00:00:00:00.
 * 4. DST mac_addr mask should be 00:00:00:00:00:00 or
 *    FF:FF:FF:FF:FF:FF
 * 5. Ether_type mask should be 0xFFFF.
 */
static int
i40e_flow_parse_ethertype_pattern(struct rte_eth_dev *dev,
				  const struct rte_flow_item *pattern,
				  struct rte_flow_error *error,
				  struct rte_eth_ethertype_filter *filter)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	enum rte_flow_item_type item_type;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;
			/* Get the MAC info. */
			if (!eth_spec || !eth_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL ETH spec/mask");
				return -rte_errno;
			}

			/* Mask bits of source MAC address must be full of 0.
			 * Mask bits of destination MAC address must be full
			 * of 1 or full of 0.
			 */
			if (!rte_is_zero_ether_addr(&eth_mask->src) ||
			    (!rte_is_zero_ether_addr(&eth_mask->dst) &&
			     !rte_is_broadcast_ether_addr(&eth_mask->dst))) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid MAC_addr mask");
				return -rte_errno;
			}

			if ((eth_mask->type & UINT16_MAX) != UINT16_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ethertype mask");
				return -rte_errno;
			}

			/* If mask bits of destination MAC address
			 * are full of 1, set RTE_ETHTYPE_FLAGS_MAC.
			 */
			if (rte_is_broadcast_ether_addr(&eth_mask->dst)) {
				filter->mac_addr = eth_spec->dst;
				filter->flags |= RTE_ETHTYPE_FLAGS_MAC;
			} else {
				filter->flags &= ~RTE_ETHTYPE_FLAGS_MAC;
			}
			filter->ether_type = rte_be_to_cpu_16(eth_spec->type);

			if (filter->ether_type == RTE_ETHER_TYPE_IPV4 ||
			    filter->ether_type == RTE_ETHER_TYPE_IPV6 ||
			    filter->ether_type == RTE_ETHER_TYPE_LLDP ||
			    filter->ether_type == i40e_get_outer_vlan(dev)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Unsupported ether_type in"
						   " control packet filter.");
				return -rte_errno;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

/* Ethertype action only supports QUEUE or DROP. */
static int
i40e_flow_parse_ethertype_action(struct rte_eth_dev *dev,
				 const struct rte_flow_action *actions,
				 struct rte_flow_error *error,
				 struct rte_eth_ethertype_filter *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_action *act;
	const struct rte_flow_action_queue *act_q;
	uint32_t index = 0;

	/* Check if the first non-void action is QUEUE or DROP. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE &&
	    act->type != RTE_FLOW_ACTION_TYPE_DROP) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	if (act->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		act_q = act->conf;
		filter->queue = act_q->index;
		if (filter->queue >= pf->dev_data->nb_rx_queues) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act, "Invalid queue ID for"
					   " ethertype_filter.");
			return -rte_errno;
		}
	} else {
		filter->flags |= RTE_ETHTYPE_FLAGS_DROP;
	}

	/* Check if the next non-void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

static int
i40e_flow_parse_ethertype_filter(struct rte_eth_dev *dev,
				 const struct rte_flow_attr *attr,
				 const struct rte_flow_item pattern[],
				 const struct rte_flow_action actions[],
				 struct rte_flow_error *error,
				 union i40e_filter_t *filter)
{
	struct rte_eth_ethertype_filter *ethertype_filter =
		&filter->ethertype_filter;
	int ret;

	ret = i40e_flow_parse_ethertype_pattern(dev, pattern, error,
						ethertype_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_ethertype_action(dev, actions, error,
					       ethertype_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_ETHERTYPE;

	return ret;
}

static int
i40e_flow_check_raw_item(const struct rte_flow_item *item,
			 const struct rte_flow_item_raw *raw_spec,
			 struct rte_flow_error *error)
{
	if (!raw_spec->relative) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "Relative should be 1.");
		return -rte_errno;
	}

	if (raw_spec->offset % sizeof(uint16_t)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "Offset should be even.");
		return -rte_errno;
	}

	if (raw_spec->search || raw_spec->limit) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "search or limit is not supported.");
		return -rte_errno;
	}

	if (raw_spec->offset < 0) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "Offset should be non-negative.");
		return -rte_errno;
	}
	return 0;
}


static uint8_t
i40e_flow_fdir_get_pctype_value(struct i40e_pf *pf,
				enum rte_flow_item_type item_type,
				struct i40e_fdir_filter_conf *filter)
{
	struct i40e_customized_pctype *cus_pctype = NULL;

	switch (item_type) {
	case RTE_FLOW_ITEM_TYPE_GTPC:
		cus_pctype = i40e_find_customized_pctype(pf,
							 I40E_CUSTOMIZED_GTPC);
		break;
	case RTE_FLOW_ITEM_TYPE_GTPU:
		if (!filter->input.flow_ext.inner_ip)
			cus_pctype = i40e_find_customized_pctype(pf,
							 I40E_CUSTOMIZED_GTPU);
		else if (filter->input.flow_ext.iip_type ==
			 I40E_FDIR_IPTYPE_IPV4)
			cus_pctype = i40e_find_customized_pctype(pf,
						 I40E_CUSTOMIZED_GTPU_IPV4);
		else if (filter->input.flow_ext.iip_type ==
			 I40E_FDIR_IPTYPE_IPV6)
			cus_pctype = i40e_find_customized_pctype(pf,
						 I40E_CUSTOMIZED_GTPU_IPV6);
		break;
	case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
		if (filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4)
			cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_IPV4_L2TPV3);
		else if (filter->input.flow_ext.oip_type ==
			 I40E_FDIR_IPTYPE_IPV6)
			cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_IPV6_L2TPV3);
		break;
	case RTE_FLOW_ITEM_TYPE_ESP:
		if (!filter->input.flow_ext.is_udp) {
			if (filter->input.flow_ext.oip_type ==
				I40E_FDIR_IPTYPE_IPV4)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV4);
			else if (filter->input.flow_ext.oip_type ==
				I40E_FDIR_IPTYPE_IPV6)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV6);
		} else {
			if (filter->input.flow_ext.oip_type ==
				I40E_FDIR_IPTYPE_IPV4)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV4_UDP);
			else if (filter->input.flow_ext.oip_type ==
					I40E_FDIR_IPTYPE_IPV6)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV6_UDP);
			filter->input.flow_ext.is_udp = false;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported item type");
		break;
	}

	if (cus_pctype && cus_pctype->valid)
		return cus_pctype->pctype;

	return I40E_FILTER_PCTYPE_INVALID;
}

static void
i40e_flow_set_filter_spi(struct i40e_fdir_filter_conf *filter,
	const struct rte_flow_item_esp *esp_spec)
{
	if (filter->input.flow_ext.oip_type ==
		I40E_FDIR_IPTYPE_IPV4) {
		if (filter->input.flow_ext.is_udp)
			filter->input.flow.esp_ipv4_udp_flow.spi =
				esp_spec->hdr.spi;
		else
			filter->input.flow.esp_ipv4_flow.spi =
				esp_spec->hdr.spi;
	}
	if (filter->input.flow_ext.oip_type ==
		I40E_FDIR_IPTYPE_IPV6) {
		if (filter->input.flow_ext.is_udp)
			filter->input.flow.esp_ipv6_udp_flow.spi =
				esp_spec->hdr.spi;
		else
			filter->input.flow.esp_ipv6_flow.spi =
				esp_spec->hdr.spi;
	}
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported patterns: refer to array i40e_supported_patterns.
 * 3. Default supported flow type and input set: refer to array
 *    valid_fdir_inset_table in i40e_ethdev.c.
 * 4. Mask of fields which need to be matched should be
 *    filled with 1.
 * 5. Mask of fields which needn't to be matched should be
 *    filled with 0.
 * 6. GTP profile supports GTPv1 only.
 * 7. GTP-C response message ('source_port' = 2123) is not supported.
 */
static int
i40e_flow_parse_fdir_pattern(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_item *pattern,
			     struct rte_flow_error *error,
			     struct i40e_fdir_filter_conf *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_vlan *vlan_spec, *vlan_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_last, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_gtp *gtp_spec, *gtp_mask;
	const struct rte_flow_item_esp *esp_spec, *esp_mask;
	const struct rte_flow_item_raw *raw_spec, *raw_mask;
	const struct rte_flow_item_vf *vf_spec;
	const struct rte_flow_item_l2tpv3oip *l2tpv3oip_spec, *l2tpv3oip_mask;

	uint8_t pctype = 0;
	uint64_t input_set = I40E_INSET_NONE;
	enum rte_flow_item_type item_type;
	enum rte_flow_item_type next_type;
	enum rte_flow_item_type l3 = RTE_FLOW_ITEM_TYPE_END;
	enum rte_flow_item_type cus_proto = RTE_FLOW_ITEM_TYPE_END;
	uint32_t i, j;
	uint8_t  ipv6_addr_mask[16] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	enum i40e_flxpld_layer_idx layer_idx = I40E_FLXPLD_L2_IDX;
	uint8_t raw_id = 0;
	int32_t off_arr[I40E_MAX_FLXPLD_FIED];
	uint16_t len_arr[I40E_MAX_FLXPLD_FIED];
	struct i40e_fdir_flex_pit flex_pit;
	uint8_t next_dst_off = 0;
	uint16_t flex_size;
	uint16_t ether_type;
	uint32_t vtc_flow_cpu;
	bool outer_ip = true;
	uint8_t field_idx;
	int ret;

	memset(off_arr, 0, sizeof(off_arr));
	memset(len_arr, 0, sizeof(len_arr));
	filter->input.flow_ext.customized_pctype = false;
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last && item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;
			next_type = (item + 1)->type;

			if (next_type == RTE_FLOW_ITEM_TYPE_END &&
						(!eth_spec || !eth_mask)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL eth spec/mask.");
				return -rte_errno;
			}

			if (eth_spec && eth_mask) {
				if (rte_is_broadcast_ether_addr(&eth_mask->dst) &&
					rte_is_zero_ether_addr(&eth_mask->src)) {
					filter->input.flow.l2_flow.dst =
						eth_spec->dst;
					input_set |= I40E_INSET_DMAC;
				} else if (rte_is_zero_ether_addr(&eth_mask->dst) &&
					rte_is_broadcast_ether_addr(&eth_mask->src)) {
					filter->input.flow.l2_flow.src =
						eth_spec->src;
					input_set |= I40E_INSET_SMAC;
				} else if (rte_is_broadcast_ether_addr(&eth_mask->dst) &&
					rte_is_broadcast_ether_addr(&eth_mask->src)) {
					filter->input.flow.l2_flow.dst =
						eth_spec->dst;
					filter->input.flow.l2_flow.src =
						eth_spec->src;
					input_set |= (I40E_INSET_DMAC | I40E_INSET_SMAC);
				} else if (!rte_is_zero_ether_addr(&eth_mask->src) ||
					   !rte_is_zero_ether_addr(&eth_mask->dst)) {
					rte_flow_error_set(error, EINVAL,
						      RTE_FLOW_ERROR_TYPE_ITEM,
						      item,
						      "Invalid MAC_addr mask.");
					return -rte_errno;
				}
			}
			if (eth_spec && eth_mask &&
			next_type == RTE_FLOW_ITEM_TYPE_END) {
				if (eth_mask->type != RTE_BE16(0xffff)) {
					rte_flow_error_set(error, EINVAL,
						      RTE_FLOW_ERROR_TYPE_ITEM,
						      item,
						      "Invalid type mask.");
					return -rte_errno;
				}

				ether_type = rte_be_to_cpu_16(eth_spec->type);

				if (next_type == RTE_FLOW_ITEM_TYPE_VLAN ||
				    ether_type == RTE_ETHER_TYPE_IPV4 ||
				    ether_type == RTE_ETHER_TYPE_IPV6 ||
				    ether_type == i40e_get_outer_vlan(dev)) {
					rte_flow_error_set(error, EINVAL,
						     RTE_FLOW_ERROR_TYPE_ITEM,
						     item,
						     "Unsupported ether_type.");
					return -rte_errno;
				}
				input_set |= I40E_INSET_LAST_ETHER_TYPE;
				filter->input.flow.l2_flow.ether_type =
					eth_spec->type;
			}

			pctype = I40E_FILTER_PCTYPE_L2_PAYLOAD;
			layer_idx = I40E_FLXPLD_L2_IDX;

			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;

			RTE_ASSERT(!(input_set & I40E_INSET_LAST_ETHER_TYPE));
			if (vlan_spec && vlan_mask) {
				if (vlan_mask->tci !=
				    rte_cpu_to_be_16(I40E_VLAN_TCI_MASK) &&
				    vlan_mask->tci !=
				    rte_cpu_to_be_16(I40E_VLAN_PRI_MASK) &&
				    vlan_mask->tci !=
				    rte_cpu_to_be_16(I40E_VLAN_CFI_MASK) &&
				    vlan_mask->tci !=
				    rte_cpu_to_be_16(I40E_VLAN_VID_MASK)) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Unsupported TCI mask.");
				}
				input_set |= I40E_INSET_VLAN_INNER;
				filter->input.flow_ext.vlan_tci =
					vlan_spec->tci;
			}
			if (vlan_spec && vlan_mask && vlan_mask->inner_type) {
				if (vlan_mask->inner_type != RTE_BE16(0xffff)) {
					rte_flow_error_set(error, EINVAL,
						      RTE_FLOW_ERROR_TYPE_ITEM,
						      item,
						      "Invalid inner_type"
						      " mask.");
					return -rte_errno;
				}

				ether_type =
					rte_be_to_cpu_16(vlan_spec->inner_type);

				if (ether_type == RTE_ETHER_TYPE_IPV4 ||
				    ether_type == RTE_ETHER_TYPE_IPV6 ||
				    ether_type == i40e_get_outer_vlan(dev)) {
					rte_flow_error_set(error, EINVAL,
						     RTE_FLOW_ERROR_TYPE_ITEM,
						     item,
						     "Unsupported inner_type.");
					return -rte_errno;
				}
				input_set |= I40E_INSET_LAST_ETHER_TYPE;
				filter->input.flow.l2_flow.ether_type =
					vlan_spec->inner_type;
			}

			pctype = I40E_FILTER_PCTYPE_L2_PAYLOAD;
			layer_idx = I40E_FLXPLD_L2_IDX;

			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			l3 = RTE_FLOW_ITEM_TYPE_IPV4;
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;
			ipv4_last = item->last;
			pctype = I40E_FILTER_PCTYPE_NONF_IPV4_OTHER;
			layer_idx = I40E_FLXPLD_L3_IDX;

			if (ipv4_last) {
				if (!ipv4_spec || !ipv4_mask || !outer_ip) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Not support range");
					return -rte_errno;
				}
				/* Only fragment_offset supports range */
				if (ipv4_last->hdr.version_ihl ||
				    ipv4_last->hdr.type_of_service ||
				    ipv4_last->hdr.total_length ||
				    ipv4_last->hdr.packet_id ||
				    ipv4_last->hdr.time_to_live ||
				    ipv4_last->hdr.next_proto_id ||
				    ipv4_last->hdr.hdr_checksum ||
				    ipv4_last->hdr.src_addr ||
				    ipv4_last->hdr.dst_addr) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Not support range");
					return -rte_errno;
				}
			}
			if (ipv4_spec && ipv4_mask && outer_ip) {
				/* Check IPv4 mask and update input set */
				if (ipv4_mask->hdr.version_ihl ||
				    ipv4_mask->hdr.total_length ||
				    ipv4_mask->hdr.packet_id ||
				    ipv4_mask->hdr.hdr_checksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 mask.");
					return -rte_errno;
				}

				if (ipv4_mask->hdr.src_addr == UINT32_MAX)
					input_set |= I40E_INSET_IPV4_SRC;
				if (ipv4_mask->hdr.dst_addr == UINT32_MAX)
					input_set |= I40E_INSET_IPV4_DST;
				if (ipv4_mask->hdr.type_of_service == UINT8_MAX)
					input_set |= I40E_INSET_IPV4_TOS;
				if (ipv4_mask->hdr.time_to_live == UINT8_MAX)
					input_set |= I40E_INSET_IPV4_TTL;
				if (ipv4_mask->hdr.next_proto_id == UINT8_MAX)
					input_set |= I40E_INSET_IPV4_PROTO;

				/* Check if it is fragment. */
				uint16_t frag_mask =
					ipv4_mask->hdr.fragment_offset;
				uint16_t frag_spec =
					ipv4_spec->hdr.fragment_offset;
				uint16_t frag_last = 0;
				if (ipv4_last)
					frag_last =
					ipv4_last->hdr.fragment_offset;
				if (frag_mask) {
					frag_mask = rte_be_to_cpu_16(frag_mask);
					frag_spec = rte_be_to_cpu_16(frag_spec);
					frag_last = rte_be_to_cpu_16(frag_last);
					/* frag_off mask has to be 0x3fff */
					if (frag_mask !=
					    (RTE_IPV4_HDR_OFFSET_MASK |
					    RTE_IPV4_HDR_MF_FLAG)) {
						rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 fragment_offset mask");
						return -rte_errno;
					}
					/*
					 * non-frag rule:
					 * mask=0x3fff,spec=0
					 * frag rule:
					 * mask=0x3fff,spec=0x8,last=0x2000
					 */
					if (frag_spec ==
					    (1 << RTE_IPV4_HDR_FO_SHIFT) &&
					    frag_last == RTE_IPV4_HDR_MF_FLAG) {
						pctype =
						  I40E_FILTER_PCTYPE_FRAG_IPV4;
					} else if (frag_spec || frag_last) {
						rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 fragment_offset rule");
						return -rte_errno;
					}
				} else if (frag_spec || frag_last) {
					rte_flow_error_set(error,
						EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid fragment_offset");
					return -rte_errno;
				}

				if (input_set & (I40E_INSET_DMAC | I40E_INSET_SMAC)) {
					if (input_set & (I40E_INSET_IPV4_SRC |
						I40E_INSET_IPV4_DST | I40E_INSET_IPV4_TOS |
						I40E_INSET_IPV4_TTL | I40E_INSET_IPV4_PROTO)) {
						rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_ITEM,
							item,
							"L2 and L3 input set are exclusive.");
						return -rte_errno;
					}
				} else {
					/* Get the filter info */
					filter->input.flow.ip4_flow.proto =
						ipv4_spec->hdr.next_proto_id;
					filter->input.flow.ip4_flow.tos =
						ipv4_spec->hdr.type_of_service;
					filter->input.flow.ip4_flow.ttl =
						ipv4_spec->hdr.time_to_live;
					filter->input.flow.ip4_flow.src_ip =
						ipv4_spec->hdr.src_addr;
					filter->input.flow.ip4_flow.dst_ip =
						ipv4_spec->hdr.dst_addr;

					filter->input.flow_ext.inner_ip = false;
					filter->input.flow_ext.oip_type =
						I40E_FDIR_IPTYPE_IPV4;
				}
			} else if (!ipv4_spec && !ipv4_mask && !outer_ip) {
				filter->input.flow_ext.inner_ip = true;
				filter->input.flow_ext.iip_type =
					I40E_FDIR_IPTYPE_IPV4;
			} else if (!ipv4_spec && !ipv4_mask && outer_ip) {
				filter->input.flow_ext.inner_ip = false;
				filter->input.flow_ext.oip_type =
					I40E_FDIR_IPTYPE_IPV4;
			} else if ((ipv4_spec || ipv4_mask) && !outer_ip) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid inner IPv4 mask.");
				return -rte_errno;
			}

			if (outer_ip)
				outer_ip = false;

			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			l3 = RTE_FLOW_ITEM_TYPE_IPV6;
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;
			pctype = I40E_FILTER_PCTYPE_NONF_IPV6_OTHER;
			layer_idx = I40E_FLXPLD_L3_IDX;

			if (ipv6_spec && ipv6_mask && outer_ip) {
				/* Check IPv6 mask and update input set */
				if (ipv6_mask->hdr.payload_len) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 mask");
					return -rte_errno;
				}

				if (!memcmp(ipv6_mask->hdr.src_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.src_addr)))
					input_set |= I40E_INSET_IPV6_SRC;
				if (!memcmp(ipv6_mask->hdr.dst_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.dst_addr)))
					input_set |= I40E_INSET_IPV6_DST;

				if ((ipv6_mask->hdr.vtc_flow &
				     rte_cpu_to_be_32(I40E_IPV6_TC_MASK))
				    == rte_cpu_to_be_32(I40E_IPV6_TC_MASK))
					input_set |= I40E_INSET_IPV6_TC;
				if (ipv6_mask->hdr.proto == UINT8_MAX)
					input_set |= I40E_INSET_IPV6_NEXT_HDR;
				if (ipv6_mask->hdr.hop_limits == UINT8_MAX)
					input_set |= I40E_INSET_IPV6_HOP_LIMIT;

				/* Get filter info */
				vtc_flow_cpu =
				      rte_be_to_cpu_32(ipv6_spec->hdr.vtc_flow);
				filter->input.flow.ipv6_flow.tc =
					(uint8_t)(vtc_flow_cpu >>
						  I40E_FDIR_IPv6_TC_OFFSET);
				filter->input.flow.ipv6_flow.proto =
					ipv6_spec->hdr.proto;
				filter->input.flow.ipv6_flow.hop_limits =
					ipv6_spec->hdr.hop_limits;

				filter->input.flow_ext.inner_ip = false;
				filter->input.flow_ext.oip_type =
					I40E_FDIR_IPTYPE_IPV6;

				rte_memcpy(filter->input.flow.ipv6_flow.src_ip,
					   ipv6_spec->hdr.src_addr, 16);
				rte_memcpy(filter->input.flow.ipv6_flow.dst_ip,
					   ipv6_spec->hdr.dst_addr, 16);

				/* Check if it is fragment. */
				if (ipv6_spec->hdr.proto ==
				    I40E_IPV6_FRAG_HEADER)
					pctype = I40E_FILTER_PCTYPE_FRAG_IPV6;
			} else if (!ipv6_spec && !ipv6_mask && !outer_ip) {
				filter->input.flow_ext.inner_ip = true;
				filter->input.flow_ext.iip_type =
					I40E_FDIR_IPTYPE_IPV6;
			} else if (!ipv6_spec && !ipv6_mask && outer_ip) {
				filter->input.flow_ext.inner_ip = false;
				filter->input.flow_ext.oip_type =
					I40E_FDIR_IPTYPE_IPV6;
			} else if ((ipv6_spec || ipv6_mask) && !outer_ip) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid inner IPv6 mask");
				return -rte_errno;
			}

			if (outer_ip)
				outer_ip = false;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				pctype =
					I40E_FILTER_PCTYPE_NONF_IPV4_TCP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				pctype =
					I40E_FILTER_PCTYPE_NONF_IPV6_TCP;
			if (tcp_spec && tcp_mask) {
				/* Check TCP mask and update input set */
				if (tcp_mask->hdr.sent_seq ||
				    tcp_mask->hdr.recv_ack ||
				    tcp_mask->hdr.data_off ||
				    tcp_mask->hdr.tcp_flags ||
				    tcp_mask->hdr.rx_win ||
				    tcp_mask->hdr.cksum ||
				    tcp_mask->hdr.tcp_urp) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid TCP mask");
					return -rte_errno;
				}

				if (tcp_mask->hdr.src_port == UINT16_MAX)
					input_set |= I40E_INSET_SRC_PORT;
				if (tcp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= I40E_INSET_DST_PORT;

				if (input_set & (I40E_INSET_DMAC | I40E_INSET_SMAC)) {
					if (input_set &
						(I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT)) {
						rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_ITEM,
							item,
							"L2 and L4 input set are exclusive.");
						return -rte_errno;
					}
				} else {
					/* Get filter info */
					if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
						filter->input.flow.tcp4_flow.src_port =
							tcp_spec->hdr.src_port;
						filter->input.flow.tcp4_flow.dst_port =
							tcp_spec->hdr.dst_port;
					} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
						filter->input.flow.tcp6_flow.src_port =
							tcp_spec->hdr.src_port;
						filter->input.flow.tcp6_flow.dst_port =
							tcp_spec->hdr.dst_port;
					}
				}
			}

			layer_idx = I40E_FLXPLD_L4_IDX;

			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				pctype =
					I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				pctype =
					I40E_FILTER_PCTYPE_NONF_IPV6_UDP;

			if (udp_spec && udp_mask) {
				/* Check UDP mask and update input set*/
				if (udp_mask->hdr.dgram_len ||
				    udp_mask->hdr.dgram_cksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
					return -rte_errno;
				}

				if (udp_mask->hdr.src_port == UINT16_MAX)
					input_set |= I40E_INSET_SRC_PORT;
				if (udp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= I40E_INSET_DST_PORT;

				if (input_set & (I40E_INSET_DMAC | I40E_INSET_SMAC)) {
					if (input_set &
						(I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT)) {
						rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_ITEM,
							item,
							"L2 and L4 input set are exclusive.");
						return -rte_errno;
					}
				} else {
					/* Get filter info */
					if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
						filter->input.flow.udp4_flow.src_port =
							udp_spec->hdr.src_port;
						filter->input.flow.udp4_flow.dst_port =
							udp_spec->hdr.dst_port;
					} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
						filter->input.flow.udp6_flow.src_port =
							udp_spec->hdr.src_port;
						filter->input.flow.udp6_flow.dst_port =
							udp_spec->hdr.dst_port;
					}
				}
			}
			filter->input.flow_ext.is_udp = true;
			layer_idx = I40E_FLXPLD_L4_IDX;

			break;
		case RTE_FLOW_ITEM_TYPE_GTPC:
		case RTE_FLOW_ITEM_TYPE_GTPU:
			if (!pf->gtp_support) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Unsupported protocol");
				return -rte_errno;
			}

			gtp_spec = item->spec;
			gtp_mask = item->mask;

			if (gtp_spec && gtp_mask) {
				if (gtp_mask->v_pt_rsv_flags ||
				    gtp_mask->msg_type ||
				    gtp_mask->msg_len ||
				    gtp_mask->teid != UINT32_MAX) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid GTP mask");
					return -rte_errno;
				}

				filter->input.flow.gtp_flow.teid =
					gtp_spec->teid;
				filter->input.flow_ext.customized_pctype = true;
				cus_proto = item_type;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			if (!pf->esp_support) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Unsupported ESP protocol");
				return -rte_errno;
			}

			esp_spec = item->spec;
			esp_mask = item->mask;

			if (!esp_spec || !esp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ESP item");
				return -rte_errno;
			}

			if (esp_spec && esp_mask) {
				if (esp_mask->hdr.spi != UINT32_MAX) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ESP mask");
					return -rte_errno;
				}
				i40e_flow_set_filter_spi(filter, esp_spec);
				filter->input.flow_ext.customized_pctype = true;
				cus_proto = item_type;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				pctype =
					I40E_FILTER_PCTYPE_NONF_IPV4_SCTP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				pctype =
					I40E_FILTER_PCTYPE_NONF_IPV6_SCTP;

			if (sctp_spec && sctp_mask) {
				/* Check SCTP mask and update input set */
				if (sctp_mask->hdr.cksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
					return -rte_errno;
				}

				if (sctp_mask->hdr.src_port == UINT16_MAX)
					input_set |= I40E_INSET_SRC_PORT;
				if (sctp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= I40E_INSET_DST_PORT;
				if (sctp_mask->hdr.tag == UINT32_MAX)
					input_set |= I40E_INSET_SCTP_VT;

				/* Get filter info */
				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
					filter->input.flow.sctp4_flow.src_port =
						sctp_spec->hdr.src_port;
					filter->input.flow.sctp4_flow.dst_port =
						sctp_spec->hdr.dst_port;
					filter->input.flow.sctp4_flow.verify_tag
						= sctp_spec->hdr.tag;
				} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
					filter->input.flow.sctp6_flow.src_port =
						sctp_spec->hdr.src_port;
					filter->input.flow.sctp6_flow.dst_port =
						sctp_spec->hdr.dst_port;
					filter->input.flow.sctp6_flow.verify_tag
						= sctp_spec->hdr.tag;
				}
			}

			layer_idx = I40E_FLXPLD_L4_IDX;

			break;
		case RTE_FLOW_ITEM_TYPE_RAW:
			raw_spec = item->spec;
			raw_mask = item->mask;

			if (!raw_spec || !raw_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL RAW spec/mask");
				return -rte_errno;
			}

			if (pf->support_multi_driver) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Unsupported flexible payload.");
				return -rte_errno;
			}

			ret = i40e_flow_check_raw_item(item, raw_spec, error);
			if (ret < 0)
				return ret;

			off_arr[raw_id] = raw_spec->offset;
			len_arr[raw_id] = raw_spec->length;

			flex_size = 0;
			memset(&flex_pit, 0, sizeof(struct i40e_fdir_flex_pit));
			field_idx = layer_idx * I40E_MAX_FLXPLD_FIED + raw_id;
			flex_pit.size =
				raw_spec->length / sizeof(uint16_t);
			flex_pit.dst_offset =
				next_dst_off / sizeof(uint16_t);

			for (i = 0; i <= raw_id; i++) {
				if (i == raw_id)
					flex_pit.src_offset +=
						raw_spec->offset /
						sizeof(uint16_t);
				else
					flex_pit.src_offset +=
						(off_arr[i] + len_arr[i]) /
						sizeof(uint16_t);
				flex_size += len_arr[i];
			}
			if (((flex_pit.src_offset + flex_pit.size) >=
			     I40E_MAX_FLX_SOURCE_OFF / sizeof(uint16_t)) ||
				flex_size > I40E_FDIR_MAX_FLEXLEN) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Exceeds maxmial payload limit.");
				return -rte_errno;
			}

			for (i = 0; i < raw_spec->length; i++) {
				j = i + next_dst_off;
				filter->input.flow_ext.flexbytes[j] =
					raw_spec->pattern[i];
				filter->input.flow_ext.flex_mask[j] =
					raw_mask->pattern[i];
			}

			next_dst_off += raw_spec->length;
			raw_id++;

			memcpy(&filter->input.flow_ext.flex_pit[field_idx],
			       &flex_pit, sizeof(struct i40e_fdir_flex_pit));
			filter->input.flow_ext.layer_idx = layer_idx;
			filter->input.flow_ext.raw_id = raw_id;
			filter->input.flow_ext.is_flex_flow = true;
			break;
		case RTE_FLOW_ITEM_TYPE_VF:
			vf_spec = item->spec;
			if (!attr->transfer) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Matching VF traffic"
						   " without affecting it"
						   " (transfer attribute)"
						   " is unsupported");
				return -rte_errno;
			}
			filter->input.flow_ext.is_vf = 1;
			filter->input.flow_ext.dst_id = vf_spec->id;
			if (filter->input.flow_ext.is_vf &&
			    filter->input.flow_ext.dst_id >= pf->vf_num) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid VF ID for FDIR.");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
			l2tpv3oip_spec = item->spec;
			l2tpv3oip_mask = item->mask;

			if (!l2tpv3oip_spec || !l2tpv3oip_mask)
				break;

			if (l2tpv3oip_mask->session_id != UINT32_MAX) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Invalid L2TPv3 mask");
				return -rte_errno;
			}

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
				filter->input.flow.ip4_l2tpv3oip_flow.session_id =
					l2tpv3oip_spec->session_id;
				filter->input.flow_ext.oip_type =
					I40E_FDIR_IPTYPE_IPV4;
			} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
				filter->input.flow.ip6_l2tpv3oip_flow.session_id =
					l2tpv3oip_spec->session_id;
				filter->input.flow_ext.oip_type =
					I40E_FDIR_IPTYPE_IPV6;
			}

			filter->input.flow_ext.customized_pctype = true;
			cus_proto = item_type;
			break;
		default:
			break;
		}
	}

	/* Get customized pctype value */
	if (filter->input.flow_ext.customized_pctype) {
		pctype = i40e_flow_fdir_get_pctype_value(pf, cus_proto, filter);
		if (pctype == I40E_FILTER_PCTYPE_INVALID) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Unsupported pctype");
			return -rte_errno;
		}
	}

	/* If customized pctype is not used, set fdir configuration.*/
	if (!filter->input.flow_ext.customized_pctype) {
		/* Check if the input set is valid */
		if (i40e_validate_input_set(pctype, RTE_ETH_FILTER_FDIR,
						input_set) != 0) {
			PMD_DRV_LOG(ERR, "Invalid input set");
			return -EINVAL;
		}

		filter->input.flow_ext.input_set = input_set;
	}

	filter->input.pctype = pctype;

	return 0;
}

/* Parse to get the action info of a FDIR filter.
 * FDIR action supports QUEUE or (QUEUE + MARK).
 */
static int
i40e_flow_parse_fdir_action(struct rte_eth_dev *dev,
			    const struct rte_flow_action *actions,
			    struct rte_flow_error *error,
			    struct i40e_fdir_filter_conf *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_action *act;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_mark *mark_spec = NULL;
	uint32_t index = 0;

	/* Check if the first non-void action is QUEUE or DROP or PASSTHRU. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		act_q = act->conf;
		filter->action.rx_queue = act_q->index;
		if ((!filter->input.flow_ext.is_vf &&
		     filter->action.rx_queue >= pf->dev_data->nb_rx_queues) ||
		    (filter->input.flow_ext.is_vf &&
		     filter->action.rx_queue >= pf->vf_nb_qps)) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Invalid queue ID for FDIR.");
			return -rte_errno;
		}
		filter->action.behavior = I40E_FDIR_ACCEPT;
		break;
	case RTE_FLOW_ACTION_TYPE_DROP:
		filter->action.behavior = I40E_FDIR_REJECT;
		break;
	case RTE_FLOW_ACTION_TYPE_PASSTHRU:
		filter->action.behavior = I40E_FDIR_PASSTHRU;
		break;
	case RTE_FLOW_ACTION_TYPE_MARK:
		filter->action.behavior = I40E_FDIR_PASSTHRU;
		mark_spec = act->conf;
		filter->action.report_status = I40E_FDIR_REPORT_ID;
		filter->soft_id = mark_spec->id;
	break;
	default:
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Invalid action.");
		return -rte_errno;
	}

	/* Check if the next non-void item is MARK or FLAG or END. */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_MARK:
		if (mark_spec) {
			/* Double MARK actions requested */
			rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, act,
			   "Invalid action.");
			return -rte_errno;
		}
		mark_spec = act->conf;
		filter->action.report_status = I40E_FDIR_REPORT_ID;
		filter->soft_id = mark_spec->id;
		break;
	case RTE_FLOW_ACTION_TYPE_FLAG:
		if (mark_spec) {
			/* MARK + FLAG not supported */
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Invalid action.");
			return -rte_errno;
		}
		filter->action.report_status = I40E_FDIR_NO_REPORT_STATUS;
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		if (filter->action.behavior != I40E_FDIR_PASSTHRU) {
			/* RSS filter won't be next if FDIR did not pass thru */
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Invalid action.");
			return -rte_errno;
		}
		break;
	case RTE_FLOW_ACTION_TYPE_END:
		return 0;
	default:
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid action.");
		return -rte_errno;
	}

	/* Check if the next non-void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid action.");
		return -rte_errno;
	}

	return 0;
}

static int
i40e_flow_parse_fdir_filter(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[],
			    struct rte_flow_error *error,
			    union i40e_filter_t *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_fdir_filter_conf *fdir_filter =
		&filter->fdir_filter;
	int ret;

	ret = i40e_flow_parse_fdir_pattern(dev, attr, pattern, error,
					   fdir_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_fdir_action(dev, actions, error, fdir_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_FDIR;

	if (pf->fdir.fdir_vsi == NULL) {
		/* Enable fdir when fdir flow is added at first time. */
		ret = i40e_fdir_setup(pf);
		if (ret != I40E_SUCCESS) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL, "Failed to setup fdir.");
			return -rte_errno;
		}
		ret = i40e_fdir_configure(dev);
		if (ret < 0) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL, "Failed to configure fdir.");
			goto err;
		}
	}

	/* If create the first fdir rule, enable fdir check for rx queues */
	if (TAILQ_EMPTY(&pf->fdir.fdir_list))
		i40e_fdir_rx_proc_enable(dev, 1);

	return 0;
err:
	i40e_fdir_teardown(pf);
	return -rte_errno;
}

/* Parse to get the action info of a tunnel filter
 * Tunnel action only supports PF, VF and QUEUE.
 */
static int
i40e_flow_parse_tunnel_action(struct rte_eth_dev *dev,
			      const struct rte_flow_action *actions,
			      struct rte_flow_error *error,
			      struct i40e_tunnel_filter_conf *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_action *act;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_vf *act_vf;
	uint32_t index = 0;

	/* Check if the first non-void action is PF or VF. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_PF &&
	    act->type != RTE_FLOW_ACTION_TYPE_VF) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	if (act->type == RTE_FLOW_ACTION_TYPE_VF) {
		act_vf = act->conf;
		filter->vf_id = act_vf->id;
		filter->is_to_vf = 1;
		if (filter->vf_id >= pf->vf_num) {
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid VF ID for tunnel filter");
			return -rte_errno;
		}
	}

	/* Check if the next non-void item is QUEUE */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		act_q = act->conf;
		filter->queue_id = act_q->index;
		if ((!filter->is_to_vf) &&
		    (filter->queue_id >= pf->dev_data->nb_rx_queues)) {
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid queue ID for tunnel filter");
			return -rte_errno;
		} else if (filter->is_to_vf &&
			   (filter->queue_id >= pf->vf_nb_qps)) {
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid queue ID for tunnel filter");
			return -rte_errno;
		}
	}

	/* Check if the next non-void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: Source port only and Destination port only.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 */
static int
i40e_flow_parse_l4_pattern(const struct rte_flow_item *pattern,
			   struct rte_flow_error *error,
			   struct i40e_tunnel_filter_conf *filter)
{
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type item_type;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ETH item");
				return -rte_errno;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV4;
			/* IPv4 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 item");
				return -rte_errno;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV6;
			/* IPv6 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 item");
				return -rte_errno;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			if (!udp_spec || !udp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid udp item");
				return -rte_errno;
			}

			if (udp_spec->hdr.src_port != 0 &&
			    udp_spec->hdr.dst_port != 0) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid udp spec");
				return -rte_errno;
			}

			if (udp_spec->hdr.src_port != 0) {
				filter->l4_port_type =
					I40E_L4_PORT_TYPE_SRC;
				filter->tenant_id =
				rte_be_to_cpu_32(udp_spec->hdr.src_port);
			}

			if (udp_spec->hdr.dst_port != 0) {
				filter->l4_port_type =
					I40E_L4_PORT_TYPE_DST;
				filter->tenant_id =
				rte_be_to_cpu_32(udp_spec->hdr.dst_port);
			}

			filter->tunnel_type = I40E_CLOUD_TYPE_UDP;

			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (!tcp_spec || !tcp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid tcp item");
				return -rte_errno;
			}

			if (tcp_spec->hdr.src_port != 0 &&
			    tcp_spec->hdr.dst_port != 0) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid tcp spec");
				return -rte_errno;
			}

			if (tcp_spec->hdr.src_port != 0) {
				filter->l4_port_type =
					I40E_L4_PORT_TYPE_SRC;
				filter->tenant_id =
				rte_be_to_cpu_32(tcp_spec->hdr.src_port);
			}

			if (tcp_spec->hdr.dst_port != 0) {
				filter->l4_port_type =
					I40E_L4_PORT_TYPE_DST;
				filter->tenant_id =
				rte_be_to_cpu_32(tcp_spec->hdr.dst_port);
			}

			filter->tunnel_type = I40E_CLOUD_TYPE_TCP;

			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (!sctp_spec || !sctp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid sctp item");
				return -rte_errno;
			}

			if (sctp_spec->hdr.src_port != 0 &&
			    sctp_spec->hdr.dst_port != 0) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid sctp spec");
				return -rte_errno;
			}

			if (sctp_spec->hdr.src_port != 0) {
				filter->l4_port_type =
					I40E_L4_PORT_TYPE_SRC;
				filter->tenant_id =
					rte_be_to_cpu_32(sctp_spec->hdr.src_port);
			}

			if (sctp_spec->hdr.dst_port != 0) {
				filter->l4_port_type =
					I40E_L4_PORT_TYPE_DST;
				filter->tenant_id =
					rte_be_to_cpu_32(sctp_spec->hdr.dst_port);
			}

			filter->tunnel_type = I40E_CLOUD_TYPE_SCTP;

			break;
		default:
			break;
		}
	}

	return 0;
}

static int
i40e_flow_parse_l4_cloud_filter(struct rte_eth_dev *dev,
				const struct rte_flow_attr *attr,
				const struct rte_flow_item pattern[],
				const struct rte_flow_action actions[],
				struct rte_flow_error *error,
				union i40e_filter_t *filter)
{
	struct i40e_tunnel_filter_conf *tunnel_filter =
		&filter->consistent_tunnel_filter;
	int ret;

	ret = i40e_flow_parse_l4_pattern(pattern, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_tunnel_action(dev, actions, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_TUNNEL;

	return ret;
}

static uint16_t i40e_supported_tunnel_filter_types[] = {
	ETH_TUNNEL_FILTER_IMAC | ETH_TUNNEL_FILTER_TENID |
	ETH_TUNNEL_FILTER_IVLAN,
	ETH_TUNNEL_FILTER_IMAC | ETH_TUNNEL_FILTER_IVLAN,
	ETH_TUNNEL_FILTER_IMAC | ETH_TUNNEL_FILTER_TENID,
	ETH_TUNNEL_FILTER_OMAC | ETH_TUNNEL_FILTER_TENID |
	ETH_TUNNEL_FILTER_IMAC,
	ETH_TUNNEL_FILTER_IMAC,
};

static int
i40e_check_tunnel_filter_type(uint8_t filter_type)
{
	uint8_t i;

	for (i = 0; i < RTE_DIM(i40e_supported_tunnel_filter_types); i++) {
		if (filter_type == i40e_supported_tunnel_filter_types[i])
			return 0;
	}

	return -1;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: IMAC_IVLAN_TENID, IMAC_IVLAN,
 *    IMAC_TENID, OMAC_TENID_IMAC and IMAC.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 */
static int
i40e_flow_parse_vxlan_pattern(__rte_unused struct rte_eth_dev *dev,
			      const struct rte_flow_item *pattern,
			      struct rte_flow_error *error,
			      struct i40e_tunnel_filter_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	const struct rte_flow_item_vxlan *vxlan_spec;
	const struct rte_flow_item_vxlan *vxlan_mask;
	const struct rte_flow_item_vlan *vlan_spec;
	const struct rte_flow_item_vlan *vlan_mask;
	uint8_t filter_type = 0;
	bool is_vni_masked = 0;
	uint8_t vni_mask[] = {0xFF, 0xFF, 0xFF};
	enum rte_flow_item_type item_type;
	bool vxlan_flag = 0;
	uint32_t tenant_id_be = 0;
	int ret;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			/* Check if ETH item is used for place holder.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!eth_spec && eth_mask) ||
			    (eth_spec && !eth_mask)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ether spec/mask");
				return -rte_errno;
			}

			if (eth_spec && eth_mask) {
				/* DST address of inner MAC shouldn't be masked.
				 * SRC address of Inner MAC should be masked.
				 */
				if (!rte_is_broadcast_ether_addr(&eth_mask->dst) ||
				    !rte_is_zero_ether_addr(&eth_mask->src) ||
				    eth_mask->type) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ether spec/mask");
					return -rte_errno;
				}

				if (!vxlan_flag) {
					rte_memcpy(&filter->outer_mac,
						   &eth_spec->dst,
						   RTE_ETHER_ADDR_LEN);
					filter_type |= ETH_TUNNEL_FILTER_OMAC;
				} else {
					rte_memcpy(&filter->inner_mac,
						   &eth_spec->dst,
						   RTE_ETHER_ADDR_LEN);
					filter_type |= ETH_TUNNEL_FILTER_IMAC;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;
			if (!(vlan_spec && vlan_mask) ||
			    vlan_mask->inner_type) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid vlan item");
				return -rte_errno;
			}

			if (vlan_spec && vlan_mask) {
				if (vlan_mask->tci ==
				    rte_cpu_to_be_16(I40E_VLAN_TCI_MASK))
					filter->inner_vlan =
					      rte_be_to_cpu_16(vlan_spec->tci) &
					      I40E_VLAN_TCI_MASK;
				filter_type |= ETH_TUNNEL_FILTER_IVLAN;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV4;
			/* IPv4 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV6;
			/* IPv6 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			/* UDP is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid UDP item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan_spec = item->spec;
			vxlan_mask = item->mask;
			/* Check if VXLAN item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!vxlan_spec && vxlan_mask) ||
			    (vxlan_spec && !vxlan_mask)) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid VXLAN item");
				return -rte_errno;
			}

			/* Check if VNI is masked. */
			if (vxlan_spec && vxlan_mask) {
				is_vni_masked =
					!!memcmp(vxlan_mask->vni, vni_mask,
						 RTE_DIM(vni_mask));
				if (is_vni_masked) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid VNI mask");
					return -rte_errno;
				}

				rte_memcpy(((uint8_t *)&tenant_id_be + 1),
					   vxlan_spec->vni, 3);
				filter->tenant_id =
					rte_be_to_cpu_32(tenant_id_be);
				filter_type |= ETH_TUNNEL_FILTER_TENID;
			}

			vxlan_flag = 1;
			break;
		default:
			break;
		}
	}

	ret = i40e_check_tunnel_filter_type(filter_type);
	if (ret < 0) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL,
				   "Invalid filter type");
		return -rte_errno;
	}
	filter->filter_type = filter_type;

	filter->tunnel_type = I40E_TUNNEL_TYPE_VXLAN;

	return 0;
}

static int
i40e_flow_parse_vxlan_filter(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct rte_flow_error *error,
			     union i40e_filter_t *filter)
{
	struct i40e_tunnel_filter_conf *tunnel_filter =
		&filter->consistent_tunnel_filter;
	int ret;

	ret = i40e_flow_parse_vxlan_pattern(dev, pattern,
					    error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_tunnel_action(dev, actions, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_TUNNEL;

	return ret;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: IMAC_IVLAN_TENID, IMAC_IVLAN,
 *    IMAC_TENID, OMAC_TENID_IMAC and IMAC.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 */
static int
i40e_flow_parse_nvgre_pattern(__rte_unused struct rte_eth_dev *dev,
			      const struct rte_flow_item *pattern,
			      struct rte_flow_error *error,
			      struct i40e_tunnel_filter_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	const struct rte_flow_item_nvgre *nvgre_spec;
	const struct rte_flow_item_nvgre *nvgre_mask;
	const struct rte_flow_item_vlan *vlan_spec;
	const struct rte_flow_item_vlan *vlan_mask;
	enum rte_flow_item_type item_type;
	uint8_t filter_type = 0;
	bool is_tni_masked = 0;
	uint8_t tni_mask[] = {0xFF, 0xFF, 0xFF};
	bool nvgre_flag = 0;
	uint32_t tenant_id_be = 0;
	int ret;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			/* Check if ETH item is used for place holder.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!eth_spec && eth_mask) ||
			    (eth_spec && !eth_mask)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ether spec/mask");
				return -rte_errno;
			}

			if (eth_spec && eth_mask) {
				/* DST address of inner MAC shouldn't be masked.
				 * SRC address of Inner MAC should be masked.
				 */
				if (!rte_is_broadcast_ether_addr(&eth_mask->dst) ||
				    !rte_is_zero_ether_addr(&eth_mask->src) ||
				    eth_mask->type) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ether spec/mask");
					return -rte_errno;
				}

				if (!nvgre_flag) {
					rte_memcpy(&filter->outer_mac,
						   &eth_spec->dst,
						   RTE_ETHER_ADDR_LEN);
					filter_type |= ETH_TUNNEL_FILTER_OMAC;
				} else {
					rte_memcpy(&filter->inner_mac,
						   &eth_spec->dst,
						   RTE_ETHER_ADDR_LEN);
					filter_type |= ETH_TUNNEL_FILTER_IMAC;
				}
			}

			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;
			if (!(vlan_spec && vlan_mask) ||
			    vlan_mask->inner_type) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid vlan item");
				return -rte_errno;
			}

			if (vlan_spec && vlan_mask) {
				if (vlan_mask->tci ==
				    rte_cpu_to_be_16(I40E_VLAN_TCI_MASK))
					filter->inner_vlan =
					      rte_be_to_cpu_16(vlan_spec->tci) &
					      I40E_VLAN_TCI_MASK;
				filter_type |= ETH_TUNNEL_FILTER_IVLAN;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV4;
			/* IPv4 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV6;
			/* IPv6 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			nvgre_spec = item->spec;
			nvgre_mask = item->mask;
			/* Check if NVGRE item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!nvgre_spec && nvgre_mask) ||
			    (nvgre_spec && !nvgre_mask)) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid NVGRE item");
				return -rte_errno;
			}

			if (nvgre_spec && nvgre_mask) {
				is_tni_masked =
					!!memcmp(nvgre_mask->tni, tni_mask,
						 RTE_DIM(tni_mask));
				if (is_tni_masked) {
					rte_flow_error_set(error, EINVAL,
						       RTE_FLOW_ERROR_TYPE_ITEM,
						       item,
						       "Invalid TNI mask");
					return -rte_errno;
				}
				if (nvgre_mask->protocol &&
					nvgre_mask->protocol != 0xFFFF) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid NVGRE item");
					return -rte_errno;
				}
				if (nvgre_mask->c_k_s_rsvd0_ver &&
					nvgre_mask->c_k_s_rsvd0_ver !=
					rte_cpu_to_be_16(0xFFFF)) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid NVGRE item");
					return -rte_errno;
				}
				if (nvgre_spec->c_k_s_rsvd0_ver !=
					rte_cpu_to_be_16(0x2000) &&
					nvgre_mask->c_k_s_rsvd0_ver) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid NVGRE item");
					return -rte_errno;
				}
				if (nvgre_mask->protocol &&
					nvgre_spec->protocol !=
					rte_cpu_to_be_16(0x6558)) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid NVGRE item");
					return -rte_errno;
				}
				rte_memcpy(((uint8_t *)&tenant_id_be + 1),
					   nvgre_spec->tni, 3);
				filter->tenant_id =
					rte_be_to_cpu_32(tenant_id_be);
				filter_type |= ETH_TUNNEL_FILTER_TENID;
			}

			nvgre_flag = 1;
			break;
		default:
			break;
		}
	}

	ret = i40e_check_tunnel_filter_type(filter_type);
	if (ret < 0) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL,
				   "Invalid filter type");
		return -rte_errno;
	}
	filter->filter_type = filter_type;

	filter->tunnel_type = I40E_TUNNEL_TYPE_NVGRE;

	return 0;
}

static int
i40e_flow_parse_nvgre_filter(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct rte_flow_error *error,
			     union i40e_filter_t *filter)
{
	struct i40e_tunnel_filter_conf *tunnel_filter =
		&filter->consistent_tunnel_filter;
	int ret;

	ret = i40e_flow_parse_nvgre_pattern(dev, pattern,
					    error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_tunnel_action(dev, actions, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_TUNNEL;

	return ret;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: MPLS label.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 */
static int
i40e_flow_parse_mpls_pattern(__rte_unused struct rte_eth_dev *dev,
			     const struct rte_flow_item *pattern,
			     struct rte_flow_error *error,
			     struct i40e_tunnel_filter_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_mpls *mpls_spec;
	const struct rte_flow_item_mpls *mpls_mask;
	enum rte_flow_item_type item_type;
	bool is_mplsoudp = 0; /* 1 - MPLSoUDP, 0 - MPLSoGRE */
	const uint8_t label_mask[3] = {0xFF, 0xFF, 0xF0};
	uint32_t label_be = 0;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ETH item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV4;
			/* IPv4 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV6;
			/* IPv6 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			/* UDP is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP item");
				return -rte_errno;
			}
			is_mplsoudp = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			/* GRE is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid GRE item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			mpls_spec = item->spec;
			mpls_mask = item->mask;

			if (!mpls_spec || !mpls_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid MPLS item");
				return -rte_errno;
			}

			if (memcmp(mpls_mask->label_tc_s, label_mask, 3)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid MPLS label mask");
				return -rte_errno;
			}
			rte_memcpy(((uint8_t *)&label_be + 1),
				   mpls_spec->label_tc_s, 3);
			filter->tenant_id = rte_be_to_cpu_32(label_be) >> 4;
			break;
		default:
			break;
		}
	}

	if (is_mplsoudp)
		filter->tunnel_type = I40E_TUNNEL_TYPE_MPLSoUDP;
	else
		filter->tunnel_type = I40E_TUNNEL_TYPE_MPLSoGRE;

	return 0;
}

static int
i40e_flow_parse_mpls_filter(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[],
			    struct rte_flow_error *error,
			    union i40e_filter_t *filter)
{
	struct i40e_tunnel_filter_conf *tunnel_filter =
		&filter->consistent_tunnel_filter;
	int ret;

	ret = i40e_flow_parse_mpls_pattern(dev, pattern,
					   error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_tunnel_action(dev, actions, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_TUNNEL;

	return ret;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: GTP TEID.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 * 5. GTP profile supports GTPv1 only.
 * 6. GTP-C response message ('source_port' = 2123) is not supported.
 */
static int
i40e_flow_parse_gtp_pattern(struct rte_eth_dev *dev,
			    const struct rte_flow_item *pattern,
			    struct rte_flow_error *error,
			    struct i40e_tunnel_filter_conf *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_gtp *gtp_spec;
	const struct rte_flow_item_gtp *gtp_mask;
	enum rte_flow_item_type item_type;

	if (!pf->gtp_support) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "GTP is not supported by default.");
		return -rte_errno;
	}

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ETH item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			filter->ip_type = I40E_TUNNEL_IPTYPE_IPV4;
			/* IPv4 is used to describe protocol,
			 * spec and mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GTPC:
		case RTE_FLOW_ITEM_TYPE_GTPU:
			gtp_spec = item->spec;
			gtp_mask = item->mask;

			if (!gtp_spec || !gtp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid GTP item");
				return -rte_errno;
			}

			if (gtp_mask->v_pt_rsv_flags ||
			    gtp_mask->msg_type ||
			    gtp_mask->msg_len ||
			    gtp_mask->teid != UINT32_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid GTP mask");
				return -rte_errno;
			}

			if (item_type == RTE_FLOW_ITEM_TYPE_GTPC)
				filter->tunnel_type = I40E_TUNNEL_TYPE_GTPC;
			else if (item_type == RTE_FLOW_ITEM_TYPE_GTPU)
				filter->tunnel_type = I40E_TUNNEL_TYPE_GTPU;

			filter->tenant_id = rte_be_to_cpu_32(gtp_spec->teid);

			break;
		default:
			break;
		}
	}

	return 0;
}

static int
i40e_flow_parse_gtp_filter(struct rte_eth_dev *dev,
			   const struct rte_flow_attr *attr,
			   const struct rte_flow_item pattern[],
			   const struct rte_flow_action actions[],
			   struct rte_flow_error *error,
			   union i40e_filter_t *filter)
{
	struct i40e_tunnel_filter_conf *tunnel_filter =
		&filter->consistent_tunnel_filter;
	int ret;

	ret = i40e_flow_parse_gtp_pattern(dev, pattern,
					  error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_tunnel_action(dev, actions, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_TUNNEL;

	return ret;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: QINQ.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 */
static int
i40e_flow_parse_qinq_pattern(__rte_unused struct rte_eth_dev *dev,
			      const struct rte_flow_item *pattern,
			      struct rte_flow_error *error,
			      struct i40e_tunnel_filter_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_vlan *vlan_spec = NULL;
	const struct rte_flow_item_vlan *vlan_mask = NULL;
	const struct rte_flow_item_vlan *i_vlan_spec = NULL;
	const struct rte_flow_item_vlan *i_vlan_mask = NULL;
	const struct rte_flow_item_vlan *o_vlan_spec = NULL;
	const struct rte_flow_item_vlan *o_vlan_mask = NULL;

	enum rte_flow_item_type item_type;
	bool vlan_flag = 0;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ETH item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;

			if (!(vlan_spec && vlan_mask) ||
			    vlan_mask->inner_type) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid vlan item");
				return -rte_errno;
			}

			if (!vlan_flag) {
				o_vlan_spec = vlan_spec;
				o_vlan_mask = vlan_mask;
				vlan_flag = 1;
			} else {
				i_vlan_spec = vlan_spec;
				i_vlan_mask = vlan_mask;
				vlan_flag = 0;
			}
			break;

		default:
			break;
		}
	}

	/* Get filter specification */
	if (o_vlan_mask != NULL &&  i_vlan_mask != NULL) {
		filter->outer_vlan = rte_be_to_cpu_16(o_vlan_spec->tci);
		filter->inner_vlan = rte_be_to_cpu_16(i_vlan_spec->tci);
	} else {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL,
					   "Invalid filter type");
			return -rte_errno;
	}

	filter->tunnel_type = I40E_TUNNEL_TYPE_QINQ;
	return 0;
}

static int
i40e_flow_parse_qinq_filter(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error,
			      union i40e_filter_t *filter)
{
	struct i40e_tunnel_filter_conf *tunnel_filter =
		&filter->consistent_tunnel_filter;
	int ret;

	ret = i40e_flow_parse_qinq_pattern(dev, pattern,
					     error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_tunnel_action(dev, actions, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_TUNNEL;

	return ret;
}

static int
i40e_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct rte_flow_item *items; /* internal pattern w/o VOID items */
	parse_filter_t parse_filter;
	uint32_t item_num = 0; /* non-void item number of pattern*/
	uint32_t i = 0;
	bool flag = false;
	int ret = I40E_NOT_SUPPORTED;

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
	memset(&cons_filter, 0, sizeof(cons_filter));

	/* Get the non-void item of action */
	while ((actions + i)->type == RTE_FLOW_ACTION_TYPE_VOID)
		i++;

	if ((actions + i)->type == RTE_FLOW_ACTION_TYPE_RSS) {
		ret = i40e_flow_parse_attr(attr, error);
		if (ret)
			return ret;

		cons_filter_type = RTE_ETH_FILTER_HASH;
		return i40e_hash_parse(dev, pattern, actions + i,
				       &cons_filter.rss_conf, error);
	}

	i = 0;
	/* Get the non-void item number of pattern */
	while ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_END) {
		if ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_VOID)
			item_num++;
		i++;
	}
	item_num++;

	if (item_num <= ARRAY_SIZE(g_items)) {
		items = g_items;
	} else {
		items = rte_zmalloc("i40e_pattern",
				    item_num * sizeof(struct rte_flow_item), 0);
		if (!items) {
			rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_ITEM_NUM,
					NULL,
					"No memory for PMD internal items.");
			return -ENOMEM;
		}
	}

	i40e_pattern_skip_void_item(items, pattern);

	i = 0;
	do {
		parse_filter = i40e_find_parse_filter_func(items, &i);
		if (!parse_filter && !flag) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   pattern, "Unsupported pattern");

			if (items != g_items)
				rte_free(items);
			return -rte_errno;
		}

		if (parse_filter)
			ret = parse_filter(dev, attr, items, actions,
					   error, &cons_filter);

		flag = true;
	} while ((ret < 0) && (i < RTE_DIM(i40e_supported_patterns)));

	if (items != g_items)
		rte_free(items);

	return ret;
}

static struct rte_flow *
i40e_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_flow *flow = NULL;
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	int ret;

	ret = i40e_flow_validate(dev, attr, pattern, actions, error);
	if (ret < 0)
		return NULL;

	if (cons_filter_type == RTE_ETH_FILTER_FDIR) {
		flow = i40e_fdir_entry_pool_get(fdir_info);
		if (flow == NULL) {
			rte_flow_error_set(error, ENOBUFS,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Fdir space full");

			return flow;
		}
	} else {
		flow = rte_zmalloc("i40e_flow", sizeof(struct rte_flow), 0);
		if (!flow) {
			rte_flow_error_set(error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to allocate memory");
			return flow;
		}
	}

	switch (cons_filter_type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = i40e_ethertype_filter_set(pf,
					&cons_filter.ethertype_filter, 1);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->ethertype.ethertype_list,
					i40e_ethertype_filter_list);
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = i40e_flow_add_del_fdir_filter(dev,
			       &cons_filter.fdir_filter, 1);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->fdir.fdir_list,
					i40e_fdir_filter_list);
		break;
	case RTE_ETH_FILTER_TUNNEL:
		ret = i40e_dev_consistent_tunnel_filter_set(pf,
			    &cons_filter.consistent_tunnel_filter, 1);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->tunnel.tunnel_list,
					i40e_tunnel_filter_list);
		break;
	case RTE_ETH_FILTER_HASH:
		ret = i40e_hash_filter_create(pf, &cons_filter.rss_conf);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->rss_config_list,
					i40e_rss_conf_list);
		break;
	default:
		goto free_flow;
	}

	flow->filter_type = cons_filter_type;
	TAILQ_INSERT_TAIL(&pf->flow_list, flow, node);
	return flow;

free_flow:
	rte_flow_error_set(error, -ret,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");

	if (cons_filter_type != RTE_ETH_FILTER_FDIR)
		rte_free(flow);
	else
		i40e_fdir_entry_pool_put(fdir_info, flow);

	return NULL;
}

static int
i40e_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum rte_filter_type filter_type = flow->filter_type;
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	int ret = 0;

	switch (filter_type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = i40e_flow_destroy_ethertype_filter(pf,
			 (struct i40e_ethertype_filter *)flow->rule);
		break;
	case RTE_ETH_FILTER_TUNNEL:
		ret = i40e_flow_destroy_tunnel_filter(pf,
			      (struct i40e_tunnel_filter *)flow->rule);
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = i40e_flow_add_del_fdir_filter(dev,
				&((struct i40e_fdir_filter *)flow->rule)->fdir,
				0);

		/* If the last flow is destroyed, disable fdir. */
		if (!ret && TAILQ_EMPTY(&pf->fdir.fdir_list)) {
			i40e_fdir_rx_proc_enable(dev, 0);
		}
		break;
	case RTE_ETH_FILTER_HASH:
		ret = i40e_hash_filter_destroy(pf, flow->rule);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
			    filter_type);
		ret = -EINVAL;
		break;
	}

	if (!ret) {
		TAILQ_REMOVE(&pf->flow_list, flow, node);
		if (filter_type == RTE_ETH_FILTER_FDIR)
			i40e_fdir_entry_pool_put(fdir_info, flow);
		else
			rte_free(flow);

	} else
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to destroy flow.");

	return ret;
}

static int
i40e_flow_destroy_ethertype_filter(struct i40e_pf *pf,
				   struct i40e_ethertype_filter *filter)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_ethertype_rule *ethertype_rule = &pf->ethertype;
	struct i40e_ethertype_filter *node;
	struct i40e_control_filter_stats stats;
	uint16_t flags = 0;
	int ret = 0;

	if (!(filter->flags & RTE_ETHTYPE_FLAGS_MAC))
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_IGNORE_MAC;
	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP)
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_DROP;
	flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_TO_QUEUE;

	memset(&stats, 0, sizeof(stats));
	ret = i40e_aq_add_rem_control_packet_filter(hw,
				    filter->input.mac_addr.addr_bytes,
				    filter->input.ether_type,
				    flags, pf->main_vsi->seid,
				    filter->queue, 0, &stats, NULL);
	if (ret < 0)
		return ret;

	node = i40e_sw_ethertype_filter_lookup(ethertype_rule, &filter->input);
	if (!node)
		return -EINVAL;

	ret = i40e_sw_ethertype_filter_del(pf, &node->input);

	return ret;
}

static int
i40e_flow_destroy_tunnel_filter(struct i40e_pf *pf,
				struct i40e_tunnel_filter *filter)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi;
	struct i40e_pf_vf *vf;
	struct i40e_aqc_cloud_filters_element_bb cld_filter;
	struct i40e_tunnel_rule *tunnel_rule = &pf->tunnel;
	struct i40e_tunnel_filter *node;
	bool big_buffer = 0;
	int ret = 0;

	memset(&cld_filter, 0, sizeof(cld_filter));
	rte_ether_addr_copy((struct rte_ether_addr *)&filter->input.outer_mac,
			(struct rte_ether_addr *)&cld_filter.element.outer_mac);
	rte_ether_addr_copy((struct rte_ether_addr *)&filter->input.inner_mac,
			(struct rte_ether_addr *)&cld_filter.element.inner_mac);
	cld_filter.element.inner_vlan = filter->input.inner_vlan;
	cld_filter.element.flags = filter->input.flags;
	cld_filter.element.tenant_id = filter->input.tenant_id;
	cld_filter.element.queue_number = filter->queue;
	rte_memcpy(cld_filter.general_fields,
		   filter->input.general_fields,
		   sizeof(cld_filter.general_fields));

	if (!filter->is_to_vf)
		vsi = pf->main_vsi;
	else {
		vf = &pf->vfs[filter->vf_id];
		vsi = vf->vsi;
	}

	if (((filter->input.flags & I40E_AQC_ADD_CLOUD_FILTER_0X11) ==
	    I40E_AQC_ADD_CLOUD_FILTER_0X11) ||
	    ((filter->input.flags & I40E_AQC_ADD_CLOUD_FILTER_0X12) ==
	    I40E_AQC_ADD_CLOUD_FILTER_0X12) ||
	    ((filter->input.flags & I40E_AQC_ADD_CLOUD_FILTER_0X10) ==
	    I40E_AQC_ADD_CLOUD_FILTER_0X10))
		big_buffer = 1;

	if (big_buffer)
		ret = i40e_aq_rem_cloud_filters_bb(hw, vsi->seid,
						&cld_filter, 1);
	else
		ret = i40e_aq_rem_cloud_filters(hw, vsi->seid,
						&cld_filter.element, 1);
	if (ret < 0)
		return -ENOTSUP;

	node = i40e_sw_tunnel_filter_lookup(tunnel_rule, &filter->input);
	if (!node)
		return -EINVAL;

	ret = i40e_sw_tunnel_filter_del(pf, &node->input);

	return ret;
}

static int
i40e_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret;

	ret = i40e_flow_flush_fdir_filter(pf);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to flush FDIR flows.");
		return -rte_errno;
	}

	ret = i40e_flow_flush_ethertype_filter(pf);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to ethertype flush flows.");
		return -rte_errno;
	}

	ret = i40e_flow_flush_tunnel_filter(pf);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to flush tunnel flows.");
		return -rte_errno;
	}

	ret = i40e_hash_filter_flush(pf);
	if (ret)
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to flush RSS flows.");
	return ret;
}

static int
i40e_flow_flush_fdir_filter(struct i40e_pf *pf)
{
	struct rte_eth_dev *dev = &rte_eth_devices[pf->dev_data->port_id];
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_filter *fdir_filter;
	enum i40e_filter_pctype pctype;
	struct rte_flow *flow;
	void *temp;
	int ret;
	uint32_t i = 0;

	ret = i40e_fdir_flush(dev);
	if (!ret) {
		/* Delete FDIR filters in FDIR list. */
		while ((fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list))) {
			ret = i40e_sw_fdir_filter_del(pf,
						      &fdir_filter->fdir.input);
			if (ret < 0)
				return ret;
		}

		/* Delete FDIR flows in flow list. */
		RTE_TAILQ_FOREACH_SAFE(flow, &pf->flow_list, node, temp) {
			if (flow->filter_type == RTE_ETH_FILTER_FDIR) {
				TAILQ_REMOVE(&pf->flow_list, flow, node);
			}
		}

		/* reset bitmap */
		rte_bitmap_reset(fdir_info->fdir_flow_pool.bitmap);
		for (i = 0; i < fdir_info->fdir_space_size; i++) {
			fdir_info->fdir_flow_pool.pool[i].idx = i;
			rte_bitmap_set(fdir_info->fdir_flow_pool.bitmap, i);
		}

		fdir_info->fdir_actual_cnt = 0;
		fdir_info->fdir_guarantee_free_space =
			fdir_info->fdir_guarantee_total_space;
		memset(fdir_info->fdir_filter_array,
			0,
			sizeof(struct i40e_fdir_filter) *
			I40E_MAX_FDIR_FILTER_NUM);

		for (pctype = I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
		     pctype <= I40E_FILTER_PCTYPE_L2_PAYLOAD; pctype++) {
			pf->fdir.flow_count[pctype] = 0;
			pf->fdir.flex_mask_flag[pctype] = 0;
		}

		for (i = 0; i < I40E_MAX_FLXPLD_LAYER; i++)
			pf->fdir.flex_pit_flag[i] = 0;

		/* Disable FDIR processing as all FDIR rules are now flushed */
		i40e_fdir_rx_proc_enable(dev, 0);
	}

	return ret;
}

/* Flush all ethertype filters */
static int
i40e_flow_flush_ethertype_filter(struct i40e_pf *pf)
{
	struct i40e_ethertype_filter_list
		*ethertype_list = &pf->ethertype.ethertype_list;
	struct i40e_ethertype_filter *filter;
	struct rte_flow *flow;
	void *temp;
	int ret = 0;

	while ((filter = TAILQ_FIRST(ethertype_list))) {
		ret = i40e_flow_destroy_ethertype_filter(pf, filter);
		if (ret)
			return ret;
	}

	/* Delete ethertype flows in flow list. */
	RTE_TAILQ_FOREACH_SAFE(flow, &pf->flow_list, node, temp) {
		if (flow->filter_type == RTE_ETH_FILTER_ETHERTYPE) {
			TAILQ_REMOVE(&pf->flow_list, flow, node);
			rte_free(flow);
		}
	}

	return ret;
}

/* Flush all tunnel filters */
static int
i40e_flow_flush_tunnel_filter(struct i40e_pf *pf)
{
	struct i40e_tunnel_filter_list
		*tunnel_list = &pf->tunnel.tunnel_list;
	struct i40e_tunnel_filter *filter;
	struct rte_flow *flow;
	void *temp;
	int ret = 0;

	while ((filter = TAILQ_FIRST(tunnel_list))) {
		ret = i40e_flow_destroy_tunnel_filter(pf, filter);
		if (ret)
			return ret;
	}

	/* Delete tunnel flows in flow list. */
	RTE_TAILQ_FOREACH_SAFE(flow, &pf->flow_list, node, temp) {
		if (flow->filter_type == RTE_ETH_FILTER_TUNNEL) {
			TAILQ_REMOVE(&pf->flow_list, flow, node);
			rte_free(flow);
		}
	}

	return ret;
}

static int
i40e_flow_query(struct rte_eth_dev *dev __rte_unused,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data, struct rte_flow_error *error)
{
	struct i40e_rss_filter *rss_rule = (struct i40e_rss_filter *)flow->rule;
	enum rte_filter_type filter_type = flow->filter_type;
	struct rte_flow_action_rss *rss_conf = data;

	if (!rss_rule) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Invalid rule");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			if (filter_type != RTE_ETH_FILTER_HASH) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "action not supported");
				return -rte_errno;
			}
			rte_memcpy(rss_conf,
				   &rss_rule->rss_filter_info.conf,
				   sizeof(struct rte_flow_action_rss));
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}

	return 0;
}
