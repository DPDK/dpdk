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
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>
#include <rte_flow.h>
#include "base/ice_type.h"
#include "base/ice_switch.h"
#include "ice_logs.h"
#include "ice_ethdev.h"
#include "ice_generic_flow.h"


#define MAX_QGRP_NUM_TYPE 7

#define ICE_SW_INSET_ETHER ( \
	ICE_INSET_DMAC | ICE_INSET_SMAC | ICE_INSET_ETHERTYPE)
#define ICE_SW_INSET_MAC_IPV4 ( \
	ICE_INSET_DMAC | ICE_INSET_IPV4_DST | ICE_INSET_IPV4_SRC | \
	ICE_INSET_IPV4_PROTO | ICE_INSET_IPV4_TTL | ICE_INSET_IPV4_TOS)
#define ICE_SW_INSET_MAC_IPV4_TCP ( \
	ICE_INSET_DMAC | ICE_INSET_IPV4_DST | ICE_INSET_IPV4_SRC | \
	ICE_INSET_IPV4_TTL | ICE_INSET_IPV4_TOS | \
	ICE_INSET_TCP_DST_PORT | ICE_INSET_TCP_SRC_PORT)
#define ICE_SW_INSET_MAC_IPV4_UDP ( \
	ICE_INSET_DMAC | ICE_INSET_IPV4_DST | ICE_INSET_IPV4_SRC | \
	ICE_INSET_IPV4_TTL | ICE_INSET_IPV4_TOS | \
	ICE_INSET_UDP_DST_PORT | ICE_INSET_UDP_SRC_PORT)
#define ICE_SW_INSET_MAC_IPV6 ( \
	ICE_INSET_DMAC | ICE_INSET_IPV6_DST | ICE_INSET_IPV6_SRC | \
	ICE_INSET_IPV6_TC | ICE_INSET_IPV6_HOP_LIMIT | \
	ICE_INSET_IPV6_NEXT_HDR)
#define ICE_SW_INSET_MAC_IPV6_TCP ( \
	ICE_INSET_DMAC | ICE_INSET_IPV6_DST | ICE_INSET_IPV6_SRC | \
	ICE_INSET_IPV6_HOP_LIMIT | ICE_INSET_IPV6_TC | \
	ICE_INSET_TCP_DST_PORT | ICE_INSET_TCP_SRC_PORT)
#define ICE_SW_INSET_MAC_IPV6_UDP ( \
	ICE_INSET_DMAC | ICE_INSET_IPV6_DST | ICE_INSET_IPV6_SRC | \
	ICE_INSET_IPV6_HOP_LIMIT | ICE_INSET_IPV6_TC | \
	ICE_INSET_UDP_DST_PORT | ICE_INSET_UDP_SRC_PORT)
#define ICE_SW_INSET_DIST_NVGRE_IPV4 ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_DMAC | ICE_INSET_TUN_NVGRE_TNI | ICE_INSET_IPV4_DST)
#define ICE_SW_INSET_DIST_VXLAN_IPV4 ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_DMAC | ICE_INSET_TUN_VXLAN_VNI | ICE_INSET_IPV4_DST)
#define ICE_SW_INSET_DIST_NVGRE_IPV4_TCP ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_TCP_SRC_PORT | ICE_INSET_TUN_TCP_DST_PORT | \
	ICE_INSET_TUN_DMAC | ICE_INSET_TUN_NVGRE_TNI | ICE_INSET_IPV4_DST)
#define ICE_SW_INSET_DIST_NVGRE_IPV4_UDP ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_UDP_SRC_PORT | ICE_INSET_TUN_UDP_DST_PORT | \
	ICE_INSET_TUN_DMAC | ICE_INSET_TUN_NVGRE_TNI | ICE_INSET_IPV4_DST)
#define ICE_SW_INSET_DIST_VXLAN_IPV4_TCP ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_TCP_SRC_PORT | ICE_INSET_TUN_TCP_DST_PORT | \
	ICE_INSET_TUN_DMAC | ICE_INSET_TUN_VXLAN_VNI | ICE_INSET_IPV4_DST)
#define ICE_SW_INSET_DIST_VXLAN_IPV4_UDP ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_UDP_SRC_PORT | ICE_INSET_TUN_UDP_DST_PORT | \
	ICE_INSET_TUN_DMAC | ICE_INSET_TUN_VXLAN_VNI | ICE_INSET_IPV4_DST)
#define ICE_SW_INSET_PERM_TUNNEL_IPV4 ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_IPV4_PROTO | ICE_INSET_TUN_IPV4_TOS)
#define ICE_SW_INSET_PERM_TUNNEL_IPV4_TCP ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_TCP_SRC_PORT | ICE_INSET_TUN_TCP_DST_PORT | \
	ICE_INSET_TUN_IPV4_TOS)
#define ICE_SW_INSET_PERM_TUNNEL_IPV4_UDP ( \
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST | \
	ICE_INSET_TUN_UDP_SRC_PORT | ICE_INSET_TUN_UDP_DST_PORT | \
	ICE_INSET_TUN_IPV4_TOS)
#define ICE_SW_INSET_MAC_PPPOE  ( \
	ICE_INSET_VLAN_OUTER | ICE_INSET_VLAN_INNER | \
	ICE_INSET_DMAC | ICE_INSET_ETHERTYPE)

struct sw_meta {
	struct ice_adv_lkup_elem *list;
	uint16_t lkups_num;
	struct ice_adv_rule_info rule_info;
};

static struct ice_flow_parser ice_switch_dist_parser_os;
static struct ice_flow_parser ice_switch_dist_parser_comms;
static struct ice_flow_parser ice_switch_perm_parser;

static struct
ice_pattern_match_item ice_switch_pattern_dist_comms[] = {
	{pattern_ethertype,
			ICE_SW_INSET_ETHER, ICE_INSET_NONE},
	{pattern_eth_ipv4,
			ICE_SW_INSET_MAC_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,
			ICE_SW_INSET_MAC_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,
			ICE_SW_INSET_MAC_IPV4_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv6,
			ICE_SW_INSET_MAC_IPV6, ICE_INSET_NONE},
	{pattern_eth_ipv6_udp,
			ICE_SW_INSET_MAC_IPV6_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv6_tcp,
			ICE_SW_INSET_MAC_IPV6_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4,
			ICE_SW_INSET_DIST_VXLAN_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp,
			ICE_SW_INSET_DIST_VXLAN_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp,
			ICE_SW_INSET_DIST_VXLAN_IPV4_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4,
			ICE_SW_INSET_DIST_NVGRE_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4_udp,
			ICE_SW_INSET_DIST_NVGRE_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4_tcp,
			ICE_SW_INSET_DIST_NVGRE_IPV4_TCP, ICE_INSET_NONE},
	{pattern_eth_pppoed,
			ICE_SW_INSET_MAC_PPPOE, ICE_INSET_NONE},
	{pattern_eth_vlan_pppoed,
			ICE_SW_INSET_MAC_PPPOE, ICE_INSET_NONE},
	{pattern_eth_pppoes,
			ICE_SW_INSET_MAC_PPPOE, ICE_INSET_NONE},
	{pattern_eth_vlan_pppoes,
			ICE_SW_INSET_MAC_PPPOE, ICE_INSET_NONE},
};

static struct
ice_pattern_match_item ice_switch_pattern_dist_os[] = {
	{pattern_ethertype,
			ICE_SW_INSET_ETHER, ICE_INSET_NONE},
	{pattern_eth_arp,
			ICE_INSET_NONE, ICE_INSET_NONE},
	{pattern_eth_ipv4,
			ICE_SW_INSET_MAC_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,
			ICE_SW_INSET_MAC_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,
			ICE_SW_INSET_MAC_IPV4_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv6,
			ICE_SW_INSET_MAC_IPV6, ICE_INSET_NONE},
	{pattern_eth_ipv6_udp,
			ICE_SW_INSET_MAC_IPV6_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv6_tcp,
			ICE_SW_INSET_MAC_IPV6_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4,
			ICE_SW_INSET_DIST_VXLAN_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp,
			ICE_SW_INSET_DIST_VXLAN_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp,
			ICE_SW_INSET_DIST_VXLAN_IPV4_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4,
			ICE_SW_INSET_DIST_NVGRE_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4_udp,
			ICE_SW_INSET_DIST_NVGRE_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4_tcp,
			ICE_SW_INSET_DIST_NVGRE_IPV4_TCP, ICE_INSET_NONE},
};

static struct
ice_pattern_match_item ice_switch_pattern_perm[] = {
	{pattern_eth_ipv4,
			ICE_SW_INSET_MAC_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,
			ICE_SW_INSET_MAC_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,
			ICE_SW_INSET_MAC_IPV4_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv6,
			ICE_SW_INSET_MAC_IPV6, ICE_INSET_NONE},
	{pattern_eth_ipv6_udp,
			ICE_SW_INSET_MAC_IPV6_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv6_tcp,
			ICE_SW_INSET_MAC_IPV6_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4,
			ICE_SW_INSET_PERM_TUNNEL_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp,
			ICE_SW_INSET_PERM_TUNNEL_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp,
			ICE_SW_INSET_PERM_TUNNEL_IPV4_TCP, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4,
			ICE_SW_INSET_PERM_TUNNEL_IPV4, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4_udp,
			ICE_SW_INSET_PERM_TUNNEL_IPV4_UDP, ICE_INSET_NONE},
	{pattern_eth_ipv4_nvgre_eth_ipv4_tcp,
			ICE_SW_INSET_PERM_TUNNEL_IPV4_TCP, ICE_INSET_NONE},
};

static int
ice_switch_create(struct ice_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error)
{
	int ret = 0;
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_rule_query_data rule_added = {0};
	struct ice_rule_query_data *filter_ptr;
	struct ice_adv_lkup_elem *list =
		((struct sw_meta *)meta)->list;
	uint16_t lkups_cnt =
		((struct sw_meta *)meta)->lkups_num;
	struct ice_adv_rule_info *rule_info =
		&((struct sw_meta *)meta)->rule_info;

	if (lkups_cnt > ICE_MAX_CHAIN_WORDS) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"item number too large for rule");
		goto error;
	}
	if (!list) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"lookup list should not be NULL");
		goto error;
	}
	ret = ice_add_adv_rule(hw, list, lkups_cnt, rule_info, &rule_added);
	if (!ret) {
		filter_ptr = rte_zmalloc("ice_switch_filter",
			sizeof(struct ice_rule_query_data), 0);
		if (!filter_ptr) {
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "No memory for ice_switch_filter");
			goto error;
		}
		flow->rule = filter_ptr;
		rte_memcpy(filter_ptr,
			&rule_added,
			sizeof(struct ice_rule_query_data));
	} else {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"switch filter create flow fail");
		goto error;
	}

	rte_free(list);
	rte_free(meta);
	return 0;

error:
	rte_free(list);
	rte_free(meta);

	return -rte_errno;
}

static int
ice_switch_destroy(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct ice_hw *hw = &ad->hw;
	int ret;
	struct ice_rule_query_data *filter_ptr;

	filter_ptr = (struct ice_rule_query_data *)
		flow->rule;

	if (!filter_ptr) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"no such flow"
			" create by switch filter");
		return -rte_errno;
	}

	ret = ice_rem_adv_rule_by_id(hw, filter_ptr);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"fail to destroy switch filter rule");
		return -rte_errno;
	}

	rte_free(filter_ptr);
	return ret;
}

static void
ice_switch_filter_rule_free(struct rte_flow *flow)
{
	rte_free(flow->rule);
}

static uint64_t
ice_switch_inset_get(const struct rte_flow_item pattern[],
		struct rte_flow_error *error,
		struct ice_adv_lkup_elem *list,
		uint16_t *lkups_num,
		enum ice_sw_tunnel_type tun_type)
{
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type item_type;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_nvgre *nvgre_spec, *nvgre_mask;
	const struct rte_flow_item_vxlan *vxlan_spec, *vxlan_mask;
	const struct rte_flow_item_vlan *vlan_spec, *vlan_mask;
	const struct rte_flow_item_pppoe *pppoe_spec, *pppoe_mask;
	uint8_t  ipv6_addr_mask[16] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	uint64_t input_set = ICE_INSET_NONE;
	uint16_t j, t = 0;
	uint16_t tunnel_valid = 0;


	for (item = pattern; item->type !=
			RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support range");
			return 0;
		}
		item_type = item->type;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;
			if (eth_spec && eth_mask) {
				if (tunnel_valid &&
				    rte_is_broadcast_ether_addr(&eth_mask->src))
					input_set |= ICE_INSET_TUN_SMAC;
				else if (
				rte_is_broadcast_ether_addr(&eth_mask->src))
					input_set |= ICE_INSET_SMAC;
				if (tunnel_valid &&
				    rte_is_broadcast_ether_addr(&eth_mask->dst))
					input_set |= ICE_INSET_TUN_DMAC;
				else if (
				rte_is_broadcast_ether_addr(&eth_mask->dst))
					input_set |= ICE_INSET_DMAC;
				if (eth_mask->type == RTE_BE16(0xffff))
					input_set |= ICE_INSET_ETHERTYPE;
				list[t].type = (tunnel_valid  == 0) ?
					ICE_MAC_OFOS : ICE_MAC_IL;
				struct ice_ether_hdr *h;
				struct ice_ether_hdr *m;
				uint16_t i = 0;
				h = &list[t].h_u.eth_hdr;
				m = &list[t].m_u.eth_hdr;
				for (j = 0; j < RTE_ETHER_ADDR_LEN; j++) {
					if (eth_mask->src.addr_bytes[j] ==
								UINT8_MAX) {
						h->src_addr[j] =
						eth_spec->src.addr_bytes[j];
						m->src_addr[j] =
						eth_mask->src.addr_bytes[j];
						i = 1;
					}
					if (eth_mask->dst.addr_bytes[j] ==
								UINT8_MAX) {
						h->dst_addr[j] =
						eth_spec->dst.addr_bytes[j];
						m->dst_addr[j] =
						eth_mask->dst.addr_bytes[j];
						i = 1;
					}
				}
				if (i)
					t++;
				if (eth_mask->type == UINT16_MAX) {
					list[t].type = ICE_ETYPE_OL;
					list[t].h_u.ethertype.ethtype_id =
						eth_spec->type;
					list[t].m_u.ethertype.ethtype_id =
						UINT16_MAX;
					t++;
				}
			} else if (!eth_spec && !eth_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_MAC_OFOS : ICE_MAC_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;
			if (ipv4_spec && ipv4_mask) {
				/* Check IPv4 mask and update input set */
				if (ipv4_mask->hdr.version_ihl ||
					ipv4_mask->hdr.total_length ||
					ipv4_mask->hdr.packet_id ||
					ipv4_mask->hdr.hdr_checksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 mask.");
					return 0;
				}

				if (tunnel_valid) {
					if (ipv4_mask->hdr.type_of_service ==
							UINT8_MAX)
						input_set |=
							ICE_INSET_TUN_IPV4_TOS;
					if (ipv4_mask->hdr.src_addr ==
							UINT32_MAX)
						input_set |=
							ICE_INSET_TUN_IPV4_SRC;
					if (ipv4_mask->hdr.dst_addr ==
							UINT32_MAX)
						input_set |=
							ICE_INSET_TUN_IPV4_DST;
					if (ipv4_mask->hdr.time_to_live ==
							UINT8_MAX)
						input_set |=
							ICE_INSET_TUN_IPV4_TTL;
					if (ipv4_mask->hdr.next_proto_id ==
							UINT8_MAX)
						input_set |=
						ICE_INSET_TUN_IPV4_PROTO;
				} else {
					if (ipv4_mask->hdr.src_addr ==
							UINT32_MAX)
						input_set |= ICE_INSET_IPV4_SRC;
					if (ipv4_mask->hdr.dst_addr ==
							UINT32_MAX)
						input_set |= ICE_INSET_IPV4_DST;
					if (ipv4_mask->hdr.time_to_live ==
							UINT8_MAX)
						input_set |= ICE_INSET_IPV4_TTL;
					if (ipv4_mask->hdr.next_proto_id ==
							UINT8_MAX)
						input_set |=
						ICE_INSET_IPV4_PROTO;
					if (ipv4_mask->hdr.type_of_service ==
							UINT8_MAX)
						input_set |=
							ICE_INSET_IPV4_TOS;
				}
				list[t].type = (tunnel_valid  == 0) ?
					ICE_IPV4_OFOS : ICE_IPV4_IL;
				if (ipv4_mask->hdr.src_addr == UINT32_MAX) {
					list[t].h_u.ipv4_hdr.src_addr =
						ipv4_spec->hdr.src_addr;
					list[t].m_u.ipv4_hdr.src_addr =
						UINT32_MAX;
				}
				if (ipv4_mask->hdr.dst_addr == UINT32_MAX) {
					list[t].h_u.ipv4_hdr.dst_addr =
						ipv4_spec->hdr.dst_addr;
					list[t].m_u.ipv4_hdr.dst_addr =
						UINT32_MAX;
				}
				if (ipv4_mask->hdr.time_to_live == UINT8_MAX) {
					list[t].h_u.ipv4_hdr.time_to_live =
						ipv4_spec->hdr.time_to_live;
					list[t].m_u.ipv4_hdr.time_to_live =
						UINT8_MAX;
				}
				if (ipv4_mask->hdr.next_proto_id == UINT8_MAX) {
					list[t].h_u.ipv4_hdr.protocol =
						ipv4_spec->hdr.next_proto_id;
					list[t].m_u.ipv4_hdr.protocol =
						UINT8_MAX;
				}
				if (ipv4_mask->hdr.type_of_service ==
						UINT8_MAX) {
					list[t].h_u.ipv4_hdr.tos =
						ipv4_spec->hdr.type_of_service;
					list[t].m_u.ipv4_hdr.tos = UINT8_MAX;
				}
				t++;
			} else if (!ipv4_spec && !ipv4_mask) {
				list[t].type = (tunnel_valid  == 0) ?
					ICE_IPV4_OFOS : ICE_IPV4_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;
			if (ipv6_spec && ipv6_mask) {
				if (ipv6_mask->hdr.payload_len) {
					rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid IPv6 mask");
					return 0;
				}

				if (tunnel_valid) {
					if (!memcmp(ipv6_mask->hdr.src_addr,
						ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.src_addr)))
						input_set |=
							ICE_INSET_TUN_IPV6_SRC;
					if (!memcmp(ipv6_mask->hdr.dst_addr,
						ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.dst_addr)))
						input_set |=
							ICE_INSET_TUN_IPV6_DST;
					if (ipv6_mask->hdr.proto == UINT8_MAX)
						input_set |=
						ICE_INSET_TUN_IPV6_NEXT_HDR;
					if (ipv6_mask->hdr.hop_limits ==
							UINT8_MAX)
						input_set |=
						ICE_INSET_TUN_IPV6_HOP_LIMIT;
					if ((ipv6_mask->hdr.vtc_flow &
						rte_cpu_to_be_32
						(RTE_IPV6_HDR_TC_MASK))
							== rte_cpu_to_be_32
							(RTE_IPV6_HDR_TC_MASK))
						input_set |=
							ICE_INSET_TUN_IPV6_TC;
				} else {
					if (!memcmp(ipv6_mask->hdr.src_addr,
						ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.src_addr)))
						input_set |= ICE_INSET_IPV6_SRC;
					if (!memcmp(ipv6_mask->hdr.dst_addr,
						ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.dst_addr)))
						input_set |= ICE_INSET_IPV6_DST;
					if (ipv6_mask->hdr.proto == UINT8_MAX)
						input_set |=
						ICE_INSET_IPV6_NEXT_HDR;
					if (ipv6_mask->hdr.hop_limits ==
							UINT8_MAX)
						input_set |=
						ICE_INSET_IPV6_HOP_LIMIT;
					if ((ipv6_mask->hdr.vtc_flow &
						rte_cpu_to_be_32
						(RTE_IPV6_HDR_TC_MASK))
							== rte_cpu_to_be_32
							(RTE_IPV6_HDR_TC_MASK))
						input_set |= ICE_INSET_IPV6_TC;
				}
				list[t].type = (tunnel_valid  == 0) ?
					ICE_IPV6_OFOS : ICE_IPV6_IL;
				struct ice_ipv6_hdr *f;
				struct ice_ipv6_hdr *s;
				f = &list[t].h_u.ipv6_hdr;
				s = &list[t].m_u.ipv6_hdr;
				for (j = 0; j < ICE_IPV6_ADDR_LENGTH; j++) {
					if (ipv6_mask->hdr.src_addr[j] ==
						UINT8_MAX) {
						f->src_addr[j] =
						ipv6_spec->hdr.src_addr[j];
						s->src_addr[j] =
						ipv6_mask->hdr.src_addr[j];
					}
					if (ipv6_mask->hdr.dst_addr[j] ==
								UINT8_MAX) {
						f->dst_addr[j] =
						ipv6_spec->hdr.dst_addr[j];
						s->dst_addr[j] =
						ipv6_mask->hdr.dst_addr[j];
					}
				}
				if (ipv6_mask->hdr.proto == UINT8_MAX) {
					f->next_hdr =
						ipv6_spec->hdr.proto;
					s->next_hdr = UINT8_MAX;
				}
				if (ipv6_mask->hdr.hop_limits == UINT8_MAX) {
					f->hop_limit =
						ipv6_spec->hdr.hop_limits;
					s->hop_limit = UINT8_MAX;
				}
				if ((ipv6_mask->hdr.vtc_flow &
						rte_cpu_to_be_32
						(RTE_IPV6_HDR_TC_MASK))
						== rte_cpu_to_be_32
						(RTE_IPV6_HDR_TC_MASK)) {
					struct ice_le_ver_tc_flow vtf;
					vtf.u.fld.version = 0;
					vtf.u.fld.flow_label = 0;
					vtf.u.fld.tc = (rte_be_to_cpu_32
						(ipv6_spec->hdr.vtc_flow) &
							RTE_IPV6_HDR_TC_MASK) >>
							RTE_IPV6_HDR_TC_SHIFT;
					f->be_ver_tc_flow = CPU_TO_BE32(vtf.u.val);
					vtf.u.fld.tc = UINT8_MAX;
					s->be_ver_tc_flow = CPU_TO_BE32(vtf.u.val);
				}
				t++;
			} else if (!ipv6_spec && !ipv6_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_IPV4_OFOS : ICE_IPV4_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;
			if (udp_spec && udp_mask) {
				/* Check UDP mask and update input set*/
				if (udp_mask->hdr.dgram_len ||
				    udp_mask->hdr.dgram_cksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
					return 0;
				}

				if (tunnel_valid) {
					if (udp_mask->hdr.src_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TUN_UDP_SRC_PORT;
					if (udp_mask->hdr.dst_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TUN_UDP_DST_PORT;
				} else {
					if (udp_mask->hdr.src_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_UDP_SRC_PORT;
					if (udp_mask->hdr.dst_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_UDP_DST_PORT;
				}
				if (tun_type == ICE_SW_TUN_VXLAN &&
						tunnel_valid == 0)
					list[t].type = ICE_UDP_OF;
				else
					list[t].type = ICE_UDP_ILOS;
				if (udp_mask->hdr.src_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.src_port =
						udp_spec->hdr.src_port;
					list[t].m_u.l4_hdr.src_port =
						udp_mask->hdr.src_port;
				}
				if (udp_mask->hdr.dst_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.dst_port =
						udp_spec->hdr.dst_port;
					list[t].m_u.l4_hdr.dst_port =
						udp_mask->hdr.dst_port;
				}
						t++;
			} else if (!udp_spec && !udp_mask) {
				list[t].type = ICE_UDP_ILOS;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;
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
					return 0;
				}

				if (tunnel_valid) {
					if (tcp_mask->hdr.src_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TUN_TCP_SRC_PORT;
					if (tcp_mask->hdr.dst_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TUN_TCP_DST_PORT;
				} else {
					if (tcp_mask->hdr.src_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TCP_SRC_PORT;
					if (tcp_mask->hdr.dst_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TCP_DST_PORT;
				}
				list[t].type = ICE_TCP_IL;
				if (tcp_mask->hdr.src_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.src_port =
						tcp_spec->hdr.src_port;
					list[t].m_u.l4_hdr.src_port =
						tcp_mask->hdr.src_port;
				}
				if (tcp_mask->hdr.dst_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.dst_port =
						tcp_spec->hdr.dst_port;
					list[t].m_u.l4_hdr.dst_port =
						tcp_mask->hdr.dst_port;
				}
				t++;
			} else if (!tcp_spec && !tcp_mask) {
				list[t].type = ICE_TCP_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;
			if (sctp_spec && sctp_mask) {
				/* Check SCTP mask and update input set */
				if (sctp_mask->hdr.cksum) {
					rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid SCTP mask");
					return 0;
				}

				if (tunnel_valid) {
					if (sctp_mask->hdr.src_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TUN_SCTP_SRC_PORT;
					if (sctp_mask->hdr.dst_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_TUN_SCTP_DST_PORT;
				} else {
					if (sctp_mask->hdr.src_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_SCTP_SRC_PORT;
					if (sctp_mask->hdr.dst_port ==
							UINT16_MAX)
						input_set |=
						ICE_INSET_SCTP_DST_PORT;
				}
				list[t].type = ICE_SCTP_IL;
				if (sctp_mask->hdr.src_port == UINT16_MAX) {
					list[t].h_u.sctp_hdr.src_port =
						sctp_spec->hdr.src_port;
					list[t].m_u.sctp_hdr.src_port =
						sctp_mask->hdr.src_port;
				}
				if (sctp_mask->hdr.dst_port == UINT16_MAX) {
					list[t].h_u.sctp_hdr.dst_port =
						sctp_spec->hdr.dst_port;
					list[t].m_u.sctp_hdr.dst_port =
						sctp_mask->hdr.dst_port;
				}
				t++;
			} else if (!sctp_spec && !sctp_mask) {
				list[t].type = ICE_SCTP_IL;
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
				return 0;
			}

			tunnel_valid = 1;
			if (vxlan_spec && vxlan_mask) {
				list[t].type = ICE_VXLAN;
				if (vxlan_mask->vni[0] == UINT8_MAX &&
					vxlan_mask->vni[1] == UINT8_MAX &&
					vxlan_mask->vni[2] == UINT8_MAX) {
					list[t].h_u.tnl_hdr.vni =
						(vxlan_spec->vni[2] << 16) |
						(vxlan_spec->vni[1] << 8) |
						vxlan_spec->vni[0];
					list[t].m_u.tnl_hdr.vni =
						UINT32_MAX;
					input_set |=
						ICE_INSET_TUN_VXLAN_VNI;
				}
				t++;
			} else if (!vxlan_spec && !vxlan_mask) {
				list[t].type = ICE_VXLAN;
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
				return 0;
			}
			tunnel_valid = 1;
			if (nvgre_spec && nvgre_mask) {
				list[t].type = ICE_NVGRE;
				if (nvgre_mask->tni[0] == UINT8_MAX &&
					nvgre_mask->tni[1] == UINT8_MAX &&
					nvgre_mask->tni[2] == UINT8_MAX) {
					list[t].h_u.nvgre_hdr.tni_flow =
						(nvgre_spec->tni[2] << 16) |
						(nvgre_spec->tni[1] << 8) |
						nvgre_spec->tni[0];
					list[t].m_u.nvgre_hdr.tni_flow =
						UINT32_MAX;
					input_set |=
						ICE_INSET_TUN_NVGRE_TNI;
				}
				t++;
			} else if (!nvgre_spec && !nvgre_mask) {
				list[t].type = ICE_NVGRE;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;
			/* Check if VLAN item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!vlan_spec && vlan_mask) ||
			    (vlan_spec && !vlan_mask)) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid VLAN item");
				return 0;
			}
			if (vlan_spec && vlan_mask) {
				list[t].type = ICE_VLAN_OFOS;
				if (vlan_mask->tci == UINT16_MAX) {
					list[t].h_u.vlan_hdr.vlan =
						vlan_spec->tci;
					list[t].m_u.vlan_hdr.vlan =
						UINT16_MAX;
					input_set |= ICE_INSET_VLAN_OUTER;
				}
				if (vlan_mask->inner_type == UINT16_MAX) {
					list[t].h_u.vlan_hdr.type =
						vlan_spec->inner_type;
					list[t].m_u.vlan_hdr.type =
						UINT16_MAX;
					input_set |= ICE_INSET_VLAN_OUTER;
				}
				t++;
			} else if (!vlan_spec && !vlan_mask) {
				list[t].type = ICE_VLAN_OFOS;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_PPPOED:
		case RTE_FLOW_ITEM_TYPE_PPPOES:
			pppoe_spec = item->spec;
			pppoe_mask = item->mask;
			/* Check if PPPoE item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 */
			if (pppoe_spec || pppoe_mask) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid pppoe item");
				return 0;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_VOID:
			break;

		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, pattern,
				   "Invalid pattern item.");
			goto out;
		}
	}

	*lkups_num = t;

	return input_set;
out:
	return 0;
}


static int
ice_switch_parse_action(struct ice_pf *pf,
		const struct rte_flow_action *actions,
		struct rte_flow_error *error,
		struct ice_adv_rule_info *rule_info)
{
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_rss *act_qgrop;
	uint16_t base_queue, i;
	const struct rte_flow_action *action;
	enum rte_flow_action_type action_type;
	uint16_t valid_qgrop_number[MAX_QGRP_NUM_TYPE] = {
		 2, 4, 8, 16, 32, 64, 128};

	base_queue = pf->base_queue + vsi->base_queue;
	for (action = actions; action->type !=
			RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_RSS:
			act_qgrop = action->conf;
			rule_info->sw_act.fltr_act =
				ICE_FWD_TO_QGRP;
			rule_info->sw_act.fwd_id.q_id =
				base_queue + act_qgrop->queue[0];
			for (i = 0; i < MAX_QGRP_NUM_TYPE; i++) {
				if (act_qgrop->queue_num ==
					valid_qgrop_number[i])
					break;
			}
			if (i == MAX_QGRP_NUM_TYPE)
				goto error;
			if ((act_qgrop->queue[0] +
				act_qgrop->queue_num) >
				dev->data->nb_rx_queues)
				goto error;
			for (i = 0; i < act_qgrop->queue_num - 1; i++)
				if (act_qgrop->queue[i + 1] !=
					act_qgrop->queue[i] + 1)
					goto error;
			rule_info->sw_act.qgrp_size =
				act_qgrop->queue_num;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			act_q = action->conf;
			if (act_q->index >= dev->data->nb_rx_queues)
				goto error;
			rule_info->sw_act.fltr_act =
				ICE_FWD_TO_Q;
			rule_info->sw_act.fwd_id.q_id =
				base_queue + act_q->index;
			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			rule_info->sw_act.fltr_act =
				ICE_DROP_PACKET;
			break;

		case RTE_FLOW_ACTION_TYPE_VOID:
			break;

		default:
			goto error;
		}
	}

	rule_info->sw_act.vsi_handle = vsi->idx;
	rule_info->rx = 1;
	rule_info->sw_act.src = vsi->idx;
	rule_info->priority = 5;

	return 0;

error:
	rte_flow_error_set(error,
		EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
		actions,
		"Invalid action type or queue number");
	return -rte_errno;
}

static int
ice_switch_parse_pattern_action(struct ice_adapter *ad,
		struct ice_pattern_match_item *array,
		uint32_t array_len,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	uint64_t inputset = 0;
	int ret = 0;
	struct sw_meta *sw_meta_ptr = NULL;
	struct ice_adv_rule_info rule_info;
	struct ice_adv_lkup_elem *list = NULL;
	uint16_t lkups_num = 0;
	const struct rte_flow_item *item = pattern;
	uint16_t item_num = 0;
	enum ice_sw_tunnel_type tun_type = ICE_NON_TUN;
	struct ice_pattern_match_item *pattern_match_item = NULL;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		item_num++;
		if (item->type == RTE_FLOW_ITEM_TYPE_VXLAN)
			tun_type = ICE_SW_TUN_VXLAN;
		if (item->type == RTE_FLOW_ITEM_TYPE_NVGRE)
			tun_type = ICE_SW_TUN_NVGRE;
		if (item->type == RTE_FLOW_ITEM_TYPE_PPPOED ||
				item->type == RTE_FLOW_ITEM_TYPE_PPPOES)
			tun_type = ICE_SW_TUN_PPPOE;
		if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
			const struct rte_flow_item_eth *eth_mask;
			if (item->mask)
				eth_mask = item->mask;
			else
				continue;
			if (eth_mask->type == UINT16_MAX)
				tun_type = ICE_SW_TUN_AND_NON_TUN;
		}
		/* reserve one more memory slot for ETH which may
		 * consume 2 lookup items.
		 */
		if (item->type == RTE_FLOW_ITEM_TYPE_ETH)
			item_num++;
	}

	list = rte_zmalloc(NULL, item_num * sizeof(*list), 0);
	if (!list) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "No memory for PMD internal items");
		return -rte_errno;
	}

	rule_info.tun_type = tun_type;

	sw_meta_ptr =
		rte_zmalloc(NULL, sizeof(*sw_meta_ptr), 0);
	if (!sw_meta_ptr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "No memory for sw_pattern_meta_ptr");
		goto error;
	}

	pattern_match_item =
		ice_search_pattern_match_item(pattern, array, array_len, error);
	if (!pattern_match_item) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Invalid input pattern");
		goto error;
	}

	inputset = ice_switch_inset_get
		(pattern, error, list, &lkups_num, tun_type);
	if (!inputset || (inputset & ~pattern_match_item->input_set_mask)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   pattern,
				   "Invalid input set");
		goto error;
	}

	ret = ice_switch_parse_action(pf, actions, error, &rule_info);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Invalid input action");
		goto error;
	}

	if (meta) {
		*meta = sw_meta_ptr;
		((struct sw_meta *)*meta)->list = list;
		((struct sw_meta *)*meta)->lkups_num = lkups_num;
		((struct sw_meta *)*meta)->rule_info = rule_info;
	} else {
		rte_free(list);
		rte_free(sw_meta_ptr);
	}

	rte_free(pattern_match_item);

	return 0;

error:
	rte_free(list);
	rte_free(sw_meta_ptr);
	rte_free(pattern_match_item);

	return -rte_errno;
}

static int
ice_switch_query(struct ice_adapter *ad __rte_unused,
		struct rte_flow *flow __rte_unused,
		struct rte_flow_query_count *count __rte_unused,
		struct rte_flow_error *error)
{
	rte_flow_error_set(error, EINVAL,
		RTE_FLOW_ERROR_TYPE_HANDLE,
		NULL,
		"count action not supported by switch filter");

	return -rte_errno;
}

static int
ice_switch_init(struct ice_adapter *ad)
{
	int ret = 0;
	struct ice_flow_parser *dist_parser;
	struct ice_flow_parser *perm_parser = &ice_switch_perm_parser;

	if (ad->active_pkg_type == ICE_PKG_TYPE_COMMS)
		dist_parser = &ice_switch_dist_parser_comms;
	else if (ad->active_pkg_type == ICE_PKG_TYPE_OS_DEFAULT)
		dist_parser = &ice_switch_dist_parser_os;
	else
		return -EINVAL;

	if (ad->devargs.pipe_mode_support)
		ret = ice_register_parser(perm_parser, ad);
	else
		ret = ice_register_parser(dist_parser, ad);
	return ret;
}

static void
ice_switch_uninit(struct ice_adapter *ad)
{
	struct ice_flow_parser *dist_parser;
	struct ice_flow_parser *perm_parser = &ice_switch_perm_parser;

	if (ad->active_pkg_type == ICE_PKG_TYPE_COMMS)
		dist_parser = &ice_switch_dist_parser_comms;
	else
		dist_parser = &ice_switch_dist_parser_os;

	if (ad->devargs.pipe_mode_support)
		ice_unregister_parser(perm_parser, ad);
	else
		ice_unregister_parser(dist_parser, ad);
}

static struct
ice_flow_engine ice_switch_engine = {
	.init = ice_switch_init,
	.uninit = ice_switch_uninit,
	.create = ice_switch_create,
	.destroy = ice_switch_destroy,
	.query_count = ice_switch_query,
	.free = ice_switch_filter_rule_free,
	.type = ICE_FLOW_ENGINE_SWITCH,
};

static struct
ice_flow_parser ice_switch_dist_parser_os = {
	.engine = &ice_switch_engine,
	.array = ice_switch_pattern_dist_os,
	.array_len = RTE_DIM(ice_switch_pattern_dist_os),
	.parse_pattern_action = ice_switch_parse_pattern_action,
	.stage = ICE_FLOW_STAGE_DISTRIBUTOR,
};

static struct
ice_flow_parser ice_switch_dist_parser_comms = {
	.engine = &ice_switch_engine,
	.array = ice_switch_pattern_dist_comms,
	.array_len = RTE_DIM(ice_switch_pattern_dist_comms),
	.parse_pattern_action = ice_switch_parse_pattern_action,
	.stage = ICE_FLOW_STAGE_DISTRIBUTOR,
};

static struct
ice_flow_parser ice_switch_perm_parser = {
	.engine = &ice_switch_engine,
	.array = ice_switch_pattern_perm,
	.array_len = RTE_DIM(ice_switch_pattern_perm),
	.parse_pattern_action = ice_switch_parse_pattern_action,
	.stage = ICE_FLOW_STAGE_PERMISSION,
};

RTE_INIT(ice_sw_engine_init)
{
	struct ice_flow_engine *engine = &ice_switch_engine;
	ice_register_flow_engine(engine);
}
