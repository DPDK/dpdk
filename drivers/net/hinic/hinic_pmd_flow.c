/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "base/hinic_compat.h"
#include "base/hinic_pmd_hwdev.h"
#include "base/hinic_pmd_hwif.h"
#include "base/hinic_pmd_wq.h"
#include "base/hinic_pmd_cmdq.h"
#include "base/hinic_pmd_niccfg.h"
#include "hinic_pmd_ethdev.h"

#define HINIC_MAX_RX_QUEUE_NUM		64

#ifndef UINT8_MAX
#define UINT8_MAX          (u8)(~((u8)0))	/* 0xFF               */
#define UINT16_MAX         (u16)(~((u16)0))	/* 0xFFFF             */
#define UINT32_MAX         (u32)(~((u32)0))	/* 0xFFFFFFFF         */
#define UINT64_MAX         (u64)(~((u64)0))	/* 0xFFFFFFFFFFFFFFFF */
#define ASCII_MAX          (0x7F)
#endif

/* IPSURX MACRO */
#define PA_ETH_TYPE_ROCE		0
#define PA_ETH_TYPE_IPV4		1
#define PA_ETH_TYPE_IPV6		2
#define PA_ETH_TYPE_OTHER		3

#define PA_IP_PROTOCOL_TYPE_TCP		1
#define PA_IP_PROTOCOL_TYPE_UDP		2
#define PA_IP_PROTOCOL_TYPE_ICMP	3
#define PA_IP_PROTOCOL_TYPE_IPV4_IGMP	4
#define PA_IP_PROTOCOL_TYPE_SCTP	5
#define PA_IP_PROTOCOL_TYPE_VRRP	112

#define IP_HEADER_PROTOCOL_TYPE_TCP     6
#define IP_HEADER_PROTOCOL_TYPE_UDP     17
#define IP_HEADER_PROTOCOL_TYPE_ICMP    1
#define IP_HEADER_PROTOCOL_TYPE_ICMPV6  58

#define FDIR_TCAM_NORMAL_PACKET         0
#define FDIR_TCAM_TUNNEL_PACKET         1

#define HINIC_MIN_N_TUPLE_PRIO		1
#define HINIC_MAX_N_TUPLE_PRIO		7

/* TCAM type mask in hardware */
#define TCAM_PKT_BGP_SPORT	1
#define TCAM_PKT_VRRP		2
#define TCAM_PKT_BGP_DPORT	3
#define TCAM_PKT_LACP		4

#define TCAM_DIP_IPV4_TYPE	0
#define TCAM_DIP_IPV6_TYPE	1

#define BGP_DPORT_ID		179
#define IPPROTO_VRRP		112

/* Packet type defined in hardware to perform filter */
#define PKT_IGMP_IPV4_TYPE     64
#define PKT_ICMP_IPV4_TYPE     65
#define PKT_ICMP_IPV6_TYPE     66
#define PKT_ICMP_IPV6RS_TYPE   67
#define PKT_ICMP_IPV6RA_TYPE   68
#define PKT_ICMP_IPV6NS_TYPE   69
#define PKT_ICMP_IPV6NA_TYPE   70
#define PKT_ICMP_IPV6RE_TYPE   71
#define PKT_DHCP_IPV4_TYPE     72
#define PKT_DHCP_IPV6_TYPE     73
#define PKT_LACP_TYPE          74
#define PKT_ARP_REQ_TYPE       79
#define PKT_ARP_REP_TYPE       80
#define PKT_ARP_TYPE           81
#define PKT_BGPD_DPORT_TYPE    83
#define PKT_BGPD_SPORT_TYPE    84
#define PKT_VRRP_TYPE          85

#define HINIC_DEV_PRIVATE_TO_FILTER_INFO(nic_dev) \
	(&((struct hinic_nic_dev *)nic_dev)->filter)

#define HINIC_DEV_PRIVATE_TO_TCAM_INFO(nic_dev) \
	(&((struct hinic_nic_dev *)nic_dev)->tcam)


enum hinic_atr_flow_type {
	HINIC_ATR_FLOW_TYPE_IPV4_DIP    = 0x1,
	HINIC_ATR_FLOW_TYPE_IPV4_SIP    = 0x2,
	HINIC_ATR_FLOW_TYPE_DPORT       = 0x3,
	HINIC_ATR_FLOW_TYPE_SPORT       = 0x4,
};

/* Structure to store fdir's info. */
struct hinic_fdir_info {
	uint8_t fdir_flag;
	uint8_t qid;
	uint32_t fdir_key;
};

/**
 * Endless loop will never happen with below assumption
 * 1. there is at least one no-void item(END)
 * 2. cur is before END.
 */
static inline const struct rte_flow_item *
next_no_void_pattern(const struct rte_flow_item pattern[],
		const struct rte_flow_item *cur)
{
	const struct rte_flow_item *next =
		cur ? cur + 1 : &pattern[0];
	while (1) {
		if (next->type != RTE_FLOW_ITEM_TYPE_VOID)
			return next;
		next++;
	}
}

static inline const struct rte_flow_action *
next_no_void_action(const struct rte_flow_action actions[],
		const struct rte_flow_action *cur)
{
	const struct rte_flow_action *next =
		cur ? cur + 1 : &actions[0];
	while (1) {
		if (next->type != RTE_FLOW_ACTION_TYPE_VOID)
			return next;
		next++;
	}
}

static int hinic_check_ethertype_attr_ele(const struct rte_flow_attr *attr,
					struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
			attr, "Only support ingress.");
		return -rte_errno;
	}

	if (attr->egress) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				attr, "Not support egress.");
		return -rte_errno;
	}

	if (attr->priority) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				attr, "Not support priority.");
		return -rte_errno;
	}

	if (attr->group) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				attr, "Not support group.");
		return -rte_errno;
	}

	return 0;
}

static int hinic_check_filter_arg(const struct rte_flow_attr *attr,
				const struct rte_flow_item *pattern,
				const struct rte_flow_action *actions,
				struct rte_flow_error *error)
{
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
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -rte_errno;
	}

	return 0;
}

static int hinic_check_ethertype_first_item(const struct rte_flow_item *item,
					struct rte_flow_error *error)
{
	/* The first non-void item should be MAC */
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ethertype filter");
		return -rte_errno;
	}

	/* Not supported last point for range */
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	/* Get the MAC info. */
	if (!item->spec || !item->mask) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ethertype filter");
		return -rte_errno;
	}
	return 0;
}

static int
hinic_parse_ethertype_aciton(const struct rte_flow_action *actions,
			const struct rte_flow_action *act,
			const struct rte_flow_action_queue *act_q,
			struct rte_eth_ethertype_filter *filter,
			struct rte_flow_error *error)
{
	/* Parse action */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE &&
		act->type != RTE_FLOW_ACTION_TYPE_DROP) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	if (act->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		act_q = (const struct rte_flow_action_queue *)act->conf;
		filter->queue = act_q->index;
	} else {
		filter->flags |= RTE_ETHTYPE_FLAGS_DROP;
	}

	/* Check if the next non-void item is END */
	act = next_no_void_action(actions, act);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

/**
 * Parse the rule to see if it is a ethertype rule.
 * And get the ethertype filter info BTW.
 * pattern:
 * The first not void item can be ETH.
 * The next not void item must be END.
 * action:
 * The first not void action should be QUEUE.
 * The next not void action should be END.
 * pattern example:
 * ITEM		Spec			Mask
 * ETH		type	0x0807		0xFFFF
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 */
static int cons_parse_ethertype_filter(const struct rte_flow_attr *attr,
			const struct rte_flow_item *pattern,
			const struct rte_flow_action *actions,
			struct rte_eth_ethertype_filter *filter,
			struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_action *act = NULL;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	const struct rte_flow_action_queue *act_q = NULL;

	if (hinic_check_filter_arg(attr, pattern, actions, error))
		return -rte_errno;

	item = next_no_void_pattern(pattern, NULL);
	if (hinic_check_ethertype_first_item(item, error))
		return -rte_errno;

	eth_spec = (const struct rte_flow_item_eth *)item->spec;
	eth_mask = (const struct rte_flow_item_eth *)item->mask;

	/*
	 * Mask bits of source MAC address must be full of 0.
	 * Mask bits of destination MAC address must be full
	 * of 1 or full of 0.
	 */
	if (!rte_is_zero_ether_addr(&eth_mask->src) ||
	    (!rte_is_zero_ether_addr(&eth_mask->dst) &&
	     !rte_is_broadcast_ether_addr(&eth_mask->dst))) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid ether address mask");
		return -rte_errno;
	}

	if ((eth_mask->type & UINT16_MAX) != UINT16_MAX) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid ethertype mask");
		return -rte_errno;
	}

	/*
	 * If mask bits of destination MAC address
	 * are full of 1, set RTE_ETHTYPE_FLAGS_MAC.
	 */
	if (rte_is_broadcast_ether_addr(&eth_mask->dst)) {
		filter->mac_addr = eth_spec->dst;
		filter->flags |= RTE_ETHTYPE_FLAGS_MAC;
	} else {
		filter->flags &= ~RTE_ETHTYPE_FLAGS_MAC;
	}
	filter->ether_type = rte_be_to_cpu_16(eth_spec->type);

	/* Check if the next non-void item is END. */
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ethertype filter.");
		return -rte_errno;
	}

	if (hinic_parse_ethertype_aciton(actions, act, act_q, filter, error))
		return -rte_errno;

	if (hinic_check_ethertype_attr_ele(attr, error))
		return -rte_errno;

	return 0;
}

static int hinic_parse_ethertype_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_eth_ethertype_filter *filter,
			struct rte_flow_error *error)
{
	if (cons_parse_ethertype_filter(attr, pattern, actions, filter, error))
		return -rte_errno;

	/* NIC doesn't support MAC address. */
	if (filter->flags & RTE_ETHTYPE_FLAGS_MAC) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "Not supported by ethertype filter");
		return -rte_errno;
	}

	if (filter->queue >= dev->data->nb_rx_queues) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "Queue index much too big");
		return -rte_errno;
	}

	if (filter->ether_type == RTE_ETHER_TYPE_IPV4 ||
		filter->ether_type == RTE_ETHER_TYPE_IPV6) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "IPv4/IPv6 not supported by ethertype filter");
		return -rte_errno;
	}

	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "Drop option is unsupported");
		return -rte_errno;
	}

	/* Hinic only support LACP/ARP for ether type */
	if (filter->ether_type != RTE_ETHER_TYPE_SLOW &&
		filter->ether_type != RTE_ETHER_TYPE_ARP) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"only lacp/arp type supported by ethertype filter");
		return -rte_errno;
	}

	return 0;
}

static int hinic_check_ntuple_attr_ele(const struct rte_flow_attr *attr,
				struct rte_eth_ntuple_filter *filter,
				struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -rte_errno;
	}

	if (attr->egress) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -rte_errno;
	}

	if (attr->priority > 0xFFFF) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Error priority.");
		return -rte_errno;
	}

	if (attr->priority < HINIC_MIN_N_TUPLE_PRIO ||
		    attr->priority > HINIC_MAX_N_TUPLE_PRIO)
		filter->priority = 1;
	else
		filter->priority = (uint16_t)attr->priority;

	return 0;
}

static int
hinic_check_ntuple_act_ele(__rte_unused const struct rte_flow_item *item,
			const struct rte_flow_action actions[],
			struct rte_eth_ntuple_filter *filter,
			struct rte_flow_error *error)
{
	const struct rte_flow_action *act;
	/*
	 * n-tuple only supports forwarding,
	 * check if the first not void action is QUEUE.
	 */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Flow action type is not QUEUE.");
		return -rte_errno;
	}
	filter->queue =
		((const struct rte_flow_action_queue *)act->conf)->index;

	/* Check if the next not void item is END */
	act = next_no_void_action(actions, act);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Next not void item is not END.");
		return -rte_errno;
	}

	return 0;
}

static int hinic_ntuple_item_check_ether(const struct rte_flow_item **ipv4_item,
					const struct rte_flow_item pattern[],
					struct rte_flow_error *error)
{
	const struct rte_flow_item *item;

	/* The first not void item can be MAC or IPv4 */
	item = next_no_void_pattern(pattern, NULL);

	if (item->type != RTE_FLOW_ITEM_TYPE_ETH &&
		item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

	/* Skip Ethernet */
	if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
		/* Not supported last point for range */
		if (item->last) {
			rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}
		/* if the first item is MAC, the content should be NULL */
		if (item->spec || item->mask) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}
		/* check if the next not void item is IPv4 */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
			rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}
	}

	*ipv4_item = item;
	return 0;
}

static int
hinic_ntuple_item_check_ipv4(const struct rte_flow_item **in_out_item,
			const struct rte_flow_item pattern[],
			struct rte_eth_ntuple_filter *filter,
			struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv4 *ipv4_mask;
	const struct rte_flow_item *item = *in_out_item;

	/* Get the IPv4 info */
	if (!item->spec || !item->mask) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Invalid ntuple mask");
		return -rte_errno;
	}
	/* Not supported last point for range */
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	ipv4_mask = (const struct rte_flow_item_ipv4 *)item->mask;
	/*
	 * Only support src & dst addresses, protocol,
	 * others should be masked.
	 */
	if (ipv4_mask->hdr.version_ihl ||
		ipv4_mask->hdr.type_of_service ||
		ipv4_mask->hdr.total_length ||
		ipv4_mask->hdr.packet_id ||
		ipv4_mask->hdr.fragment_offset ||
		ipv4_mask->hdr.time_to_live ||
		ipv4_mask->hdr.hdr_checksum ||
		!ipv4_mask->hdr.next_proto_id) {
		rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

	filter->dst_ip_mask = ipv4_mask->hdr.dst_addr;
	filter->src_ip_mask = ipv4_mask->hdr.src_addr;
	filter->proto_mask = ipv4_mask->hdr.next_proto_id;

	ipv4_spec = (const struct rte_flow_item_ipv4 *)item->spec;
	filter->dst_ip = ipv4_spec->hdr.dst_addr;
	filter->src_ip = ipv4_spec->hdr.src_addr;
	filter->proto  = ipv4_spec->hdr.next_proto_id;

	/* Get next no void item */
	*in_out_item = next_no_void_pattern(pattern, item);
	return 0;
}

static int hinic_ntuple_item_check_l4(const struct rte_flow_item **in_out_item,
				const struct rte_flow_item pattern[],
				struct rte_eth_ntuple_filter *filter,
				struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;
	const struct rte_flow_item_icmp *icmp_mask;
	const struct rte_flow_item *item = *in_out_item;
	u32 ntuple_filter_size = sizeof(struct rte_eth_ntuple_filter);

	if (item->type == RTE_FLOW_ITEM_TYPE_END)
		return 0;

	/* Get TCP or UDP info */
	if (item->type != RTE_FLOW_ITEM_TYPE_END &&
		(!item->spec || !item->mask)) {
		memset(filter, 0, ntuple_filter_size);
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Invalid ntuple mask");
		return -rte_errno;
	}

	/* Not supported last point for range */
	if (item->last) {
		memset(filter, 0, ntuple_filter_size);
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
		tcp_mask = (const struct rte_flow_item_tcp *)item->mask;

		/*
		 * Only support src & dst ports, tcp flags,
		 * others should be masked.
		 */
		if (tcp_mask->hdr.sent_seq ||
			tcp_mask->hdr.recv_ack ||
			tcp_mask->hdr.data_off ||
			tcp_mask->hdr.rx_win ||
			tcp_mask->hdr.cksum ||
			tcp_mask->hdr.tcp_urp) {
			memset(filter, 0, ntuple_filter_size);
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}

		filter->dst_port_mask  = tcp_mask->hdr.dst_port;
		filter->src_port_mask  = tcp_mask->hdr.src_port;
		if (tcp_mask->hdr.tcp_flags == 0xFF) {
			filter->flags |= RTE_NTUPLE_FLAGS_TCP_FLAG;
		} else if (!tcp_mask->hdr.tcp_flags) {
			filter->flags &= ~RTE_NTUPLE_FLAGS_TCP_FLAG;
		} else {
			memset(filter, 0, ntuple_filter_size);
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}

		tcp_spec = (const struct rte_flow_item_tcp *)item->spec;
		filter->dst_port  = tcp_spec->hdr.dst_port;
		filter->src_port  = tcp_spec->hdr.src_port;
		filter->tcp_flags = tcp_spec->hdr.tcp_flags;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_ICMP) {
		icmp_mask = (const struct rte_flow_item_icmp *)item->mask;

		/* ICMP all should be masked. */
		if (icmp_mask->hdr.icmp_cksum ||
			icmp_mask->hdr.icmp_ident ||
			icmp_mask->hdr.icmp_seq_nb ||
			icmp_mask->hdr.icmp_type ||
			icmp_mask->hdr.icmp_code) {
			memset(filter, 0, ntuple_filter_size);
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}
	}

	/* Get next no void item */
	*in_out_item = next_no_void_pattern(pattern, item);
	return 0;
}

static int hinic_ntuple_item_check_end(const struct rte_flow_item *item,
					struct rte_eth_ntuple_filter *filter,
					struct rte_flow_error *error)
{
	/* Check if the next not void item is END */
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

	return 0;
}

static int hinic_check_ntuple_item_ele(const struct rte_flow_item *item,
					const struct rte_flow_item pattern[],
					struct rte_eth_ntuple_filter *filter,
					struct rte_flow_error *error)
{
	if (hinic_ntuple_item_check_ether(&item, pattern, error) ||
		hinic_ntuple_item_check_ipv4(&item, pattern, filter, error) ||
		hinic_ntuple_item_check_l4(&item, pattern, filter, error) ||
		hinic_ntuple_item_check_end(item, filter, error))
		return -rte_errno;

	return 0;
}

/**
 * Parse the rule to see if it is a n-tuple rule.
 * And get the n-tuple filter info BTW.
 * pattern:
 * The first not void item can be ETH or IPV4.
 * The second not void item must be IPV4 if the first one is ETH.
 * The third not void item must be UDP or TCP.
 * The next not void item must be END.
 * action:
 * The first not void action should be QUEUE.
 * The next not void action should be END.
 * pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4		src_addr 192.168.1.20	0xFFFFFFFF
 *		dst_addr 192.167.3.50	0xFFFFFFFF
 *		next_proto_id	17	0xFF
 * UDP/TCP/	src_port	80	0xFFFF
 * SCTP		dst_port	80	0xFFFF
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 * Please be aware there's an assumption for all the parsers.
 * rte_flow_item is using big endian, rte_flow_attr and
 * rte_flow_action are using CPU order.
 * Because the pattern is used to describe the packets,
 * normally the packets should use network order.
 */
static int cons_parse_ntuple_filter(const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_eth_ntuple_filter *filter,
			struct rte_flow_error *error)
{
	const struct rte_flow_item *item = NULL;

	if (hinic_check_filter_arg(attr, pattern, actions, error))
		return -rte_errno;

	if (hinic_check_ntuple_item_ele(item, pattern, filter, error))
		return -rte_errno;

	if (hinic_check_ntuple_act_ele(item, actions, filter, error))
		return -rte_errno;

	if (hinic_check_ntuple_attr_ele(attr, filter, error))
		return -rte_errno;

	return 0;
}

static int hinic_parse_ntuple_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_eth_ntuple_filter *filter,
			struct rte_flow_error *error)
{
	int ret;

	ret = cons_parse_ntuple_filter(attr, pattern, actions, filter, error);
	if (ret)
		return ret;

	/* Hinic doesn't support tcp flags */
	if (filter->flags & RTE_NTUPLE_FLAGS_TCP_FLAG) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Not supported by ntuple filter");
		return -rte_errno;
	}

	/* Hinic doesn't support many priorities */
	if (filter->priority < HINIC_MIN_N_TUPLE_PRIO ||
	    filter->priority > HINIC_MAX_N_TUPLE_PRIO) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "Priority not supported by ntuple filter");
		return -rte_errno;
	}

	if (filter->queue >= dev->data->nb_rx_queues)
		return -rte_errno;

	/* Fixed value for hinic */
	filter->flags = RTE_5TUPLE_FLAGS;
	return 0;
}

static int hinic_normal_item_check_ether(const struct rte_flow_item **ip_item,
					const struct rte_flow_item pattern[],
					struct rte_flow_error *error)
{
	const struct rte_flow_item *item;

	/* The first not void item can be MAC or IPv4  or TCP or UDP */
	item = next_no_void_pattern(pattern, NULL);

	if (item->type != RTE_FLOW_ITEM_TYPE_ETH &&
		item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
		item->type != RTE_FLOW_ITEM_TYPE_TCP &&
		item->type != RTE_FLOW_ITEM_TYPE_UDP) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Not supported by fdir filter,support mac,ipv4,tcp,udp");
		return -rte_errno;
	}

	/* Not supported last point for range */
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, item,
			"Not supported last point for range");
		return -rte_errno;
	}

	/* Skip Ethernet */
	if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
		/* All should be masked. */
		if (item->spec || item->mask) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter,support mac");
			return -rte_errno;
		}
		/* Check if the next not void item is IPv4 */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
			item->type != RTE_FLOW_ITEM_TYPE_IPV6) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by fdir filter,support mac,ipv4");
			return -rte_errno;
		}
	}

	*ip_item = item;
	return 0;
}

static int hinic_normal_item_check_ip(const struct rte_flow_item **in_out_item,
				const struct rte_flow_item pattern[],
				struct hinic_fdir_rule *rule,
				struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv4 *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec;
	const struct rte_flow_item_ipv6 *ipv6_mask;
	const struct rte_flow_item *item = *in_out_item;
	int i;

	/* Get the IPv4 info */
	if (item->type == RTE_FLOW_ITEM_TYPE_IPV4) {
		/* Not supported last point for range */
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		if (!item->mask) {
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid fdir filter mask");
			return -rte_errno;
		}

		ipv4_mask = (const struct rte_flow_item_ipv4 *)item->mask;
		/*
		 * Only support src & dst addresses,
		 * others should be masked.
		 */
		if (ipv4_mask->hdr.version_ihl ||
			ipv4_mask->hdr.type_of_service ||
			ipv4_mask->hdr.total_length ||
			ipv4_mask->hdr.packet_id ||
			ipv4_mask->hdr.fragment_offset ||
			ipv4_mask->hdr.time_to_live ||
			ipv4_mask->hdr.next_proto_id ||
			ipv4_mask->hdr.hdr_checksum) {
			rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by fdir filter, support src,dst ip");
			return -rte_errno;
		}

		rule->mask.dst_ipv4_mask = ipv4_mask->hdr.dst_addr;
		rule->mask.src_ipv4_mask = ipv4_mask->hdr.src_addr;
		rule->mode = HINIC_FDIR_MODE_NORMAL;

		if (item->spec) {
			ipv4_spec =
				(const struct rte_flow_item_ipv4 *)item->spec;
			rule->hinic_fdir.dst_ip = ipv4_spec->hdr.dst_addr;
			rule->hinic_fdir.src_ip = ipv4_spec->hdr.src_addr;
		}

		/*
		 * Check if the next not void item is
		 * TCP or UDP or END.
		 */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
		    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
		    item->type != RTE_FLOW_ITEM_TYPE_ICMP &&
		    item->type != RTE_FLOW_ITEM_TYPE_ANY &&
		    item->type != RTE_FLOW_ITEM_TYPE_END) {
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by fdir filter, support tcp, udp, end");
			return -rte_errno;
		}
	} else if (item->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		/* Not supported last point for range */
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		if (!item->mask) {
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid fdir filter mask");
			return -rte_errno;
		}

		ipv6_mask = (const struct rte_flow_item_ipv6 *)item->mask;

		/* Only support dst addresses,  others should be masked */
		if (ipv6_mask->hdr.vtc_flow ||
		    ipv6_mask->hdr.payload_len ||
		    ipv6_mask->hdr.proto ||
		    ipv6_mask->hdr.hop_limits) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by fdir filter, support dst ipv6");
			return -rte_errno;
		}

		/* check ipv6 src addr mask, ipv6 src addr is 16 bytes */
		for (i = 0; i < 16; i++) {
			if (ipv6_mask->hdr.src_addr[i] == UINT8_MAX) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Not supported by fdir filter, do not support src ipv6");
				return -rte_errno;
			}
		}

		if (!item->spec) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by fdir filter, ipv6 spec is NULL");
			return -rte_errno;
		}

		for (i = 0; i < 16; i++) {
			if (ipv6_mask->hdr.dst_addr[i] == UINT8_MAX)
				rule->mask.dst_ipv6_mask |= 1 << i;
		}

		ipv6_spec = (const struct rte_flow_item_ipv6 *)item->spec;
		rte_memcpy(rule->hinic_fdir.dst_ipv6,
			   ipv6_spec->hdr.dst_addr, 16);

		/*
		 * Check if the next not void item is TCP or UDP or ICMP.
		 */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
		    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
		    item->type != RTE_FLOW_ITEM_TYPE_ICMP &&
		    item->type != RTE_FLOW_ITEM_TYPE_ICMP6){
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by fdir filter, support tcp, udp, icmp");
			return -rte_errno;
		}
	}

	*in_out_item = item;
	return 0;
}

static int hinic_normal_item_check_l4(const struct rte_flow_item **in_out_item,
			__rte_unused const struct rte_flow_item pattern[],
			__rte_unused struct hinic_fdir_rule *rule,
			struct rte_flow_error *error)
{
	const struct rte_flow_item *item = *in_out_item;

	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by normal fdir filter, not support l4");
		return -rte_errno;
	}

	return 0;
}


static int hinic_normal_item_check_end(const struct rte_flow_item *item,
					struct hinic_fdir_rule *rule,
					struct rte_flow_error *error)
{
	/* Check if the next not void item is END */
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter, support end");
		return -rte_errno;
	}

	return 0;
}

static int hinic_check_normal_item_ele(const struct rte_flow_item *item,
					const struct rte_flow_item pattern[],
					struct hinic_fdir_rule *rule,
					struct rte_flow_error *error)
{
	if (hinic_normal_item_check_ether(&item, pattern, error) ||
	    hinic_normal_item_check_ip(&item, pattern, rule, error) ||
	    hinic_normal_item_check_l4(&item, pattern, rule, error) ||
	    hinic_normal_item_check_end(item, rule, error))
		return -rte_errno;

	return 0;
}

static int
hinic_tcam_normal_item_check_l4(const struct rte_flow_item **in_out_item,
				const struct rte_flow_item pattern[],
				struct hinic_fdir_rule *rule,
				struct rte_flow_error *error)
{
	const struct rte_flow_item *item = *in_out_item;
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;
	const struct rte_flow_item_udp *udp_spec;
	const struct rte_flow_item_udp *udp_mask;

	if (item->type == RTE_FLOW_ITEM_TYPE_ICMP) {
		rule->mode = HINIC_FDIR_MODE_TCAM;
		rule->mask.proto_mask = UINT16_MAX;
		rule->hinic_fdir.proto = IP_HEADER_PROTOCOL_TYPE_ICMP;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_ICMP6) {
		rule->mode = HINIC_FDIR_MODE_TCAM;
		rule->mask.proto_mask = UINT16_MAX;
		rule->hinic_fdir.proto = IP_HEADER_PROTOCOL_TYPE_ICMPV6;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_ANY) {
		rule->mode = HINIC_FDIR_MODE_TCAM;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
		if (!item->mask) {
			(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter, support src, dst ports");
			return -rte_errno;
		}

		tcp_mask = (const struct rte_flow_item_tcp *)item->mask;

		/*
		 * Only support src & dst ports, tcp flags,
		 * others should be masked.
		 */
		if (tcp_mask->hdr.sent_seq ||
			tcp_mask->hdr.recv_ack ||
			tcp_mask->hdr.data_off ||
			tcp_mask->hdr.rx_win ||
			tcp_mask->hdr.cksum ||
			tcp_mask->hdr.tcp_urp) {
			(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir normal tcam filter");
			return -rte_errno;
		}

		rule->mode = HINIC_FDIR_MODE_TCAM;
		rule->mask.proto_mask = UINT16_MAX;
		rule->mask.dst_port_mask = tcp_mask->hdr.dst_port;
		rule->mask.src_port_mask = tcp_mask->hdr.src_port;

		rule->hinic_fdir.proto = IP_HEADER_PROTOCOL_TYPE_TCP;
		if (item->spec) {
			tcp_spec = (const struct rte_flow_item_tcp *)item->spec;
			rule->hinic_fdir.dst_port = tcp_spec->hdr.dst_port;
			rule->hinic_fdir.src_port = tcp_spec->hdr.src_port;
		}
	} else if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
		/*
		 * Only care about src & dst ports,
		 * others should be masked.
		 */
		if (!item->mask) {
			(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter, support src, dst ports");
			return -rte_errno;
		}

		udp_mask = (const struct rte_flow_item_udp *)item->mask;
		if (udp_mask->hdr.dgram_len ||
			udp_mask->hdr.dgram_cksum) {
			(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter, support udp");
			return -rte_errno;
		}

		rule->mode = HINIC_FDIR_MODE_TCAM;
		rule->mask.proto_mask = UINT16_MAX;
		rule->mask.src_port_mask = udp_mask->hdr.src_port;
		rule->mask.dst_port_mask = udp_mask->hdr.dst_port;

		rule->hinic_fdir.proto = IP_HEADER_PROTOCOL_TYPE_UDP;
		if (item->spec) {
			udp_spec = (const struct rte_flow_item_udp *)item->spec;
			rule->hinic_fdir.src_port = udp_spec->hdr.src_port;
			rule->hinic_fdir.dst_port = udp_spec->hdr.dst_port;
		}
	} else {
		(void)memset(rule,  0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter tcam normal, l4 only support icmp, tcp");
		return -rte_errno;
	}

	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter tcam normal, support end");
		return -rte_errno;
	}

	/* get next no void item */
	*in_out_item = item;

	return 0;
}

static int hinic_check_tcam_normal_item_ele(const struct rte_flow_item *item,
					const struct rte_flow_item pattern[],
					struct hinic_fdir_rule *rule,
					struct rte_flow_error *error)
{
	if (hinic_normal_item_check_ether(&item, pattern, error) ||
		hinic_normal_item_check_ip(&item, pattern, rule, error) ||
		hinic_tcam_normal_item_check_l4(&item, pattern, rule, error) ||
		hinic_normal_item_check_end(item, rule, error))
		return -rte_errno;

	return 0;
}

static int hinic_tunnel_item_check_l4(const struct rte_flow_item **in_out_item,
					const struct rte_flow_item pattern[],
					struct hinic_fdir_rule *rule,
					struct rte_flow_error *error)
{
	const struct rte_flow_item *item = *in_out_item;

	if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_VXLAN) {
			(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter, support vxlan");
			return -rte_errno;
		}

		*in_out_item = item;
	} else {
		(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter tcam tunnel, outer l4 only support udp");
		return -rte_errno;
	}

	return 0;
}

static int
hinic_tunnel_item_check_vxlan(const struct rte_flow_item **in_out_item,
				const struct rte_flow_item pattern[],
				struct hinic_fdir_rule *rule,
				struct rte_flow_error *error)
{
	const struct rte_flow_item *item = *in_out_item;


	if (item->type == RTE_FLOW_ITEM_TYPE_VXLAN) {
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
		    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
		    item->type != RTE_FLOW_ITEM_TYPE_ANY) {
			(void)memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter, support tcp/udp");
			return -rte_errno;
		}

		*in_out_item = item;
	}

	return 0;
}

static int
hinic_tunnel_inner_item_check_l4(const struct rte_flow_item **in_out_item,
				const struct rte_flow_item pattern[],
				struct hinic_fdir_rule *rule,
				struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;
	const struct rte_flow_item_udp *udp_spec;
	const struct rte_flow_item_udp *udp_mask;
	const struct rte_flow_item *item = *in_out_item;

	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		/* Not supported last point for range */
		if (item->last) {
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		/* get the TCP/UDP info */
		if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
			/*
			 * Only care about src & dst ports,
			 * others should be masked.
			 */
			if (!item->mask) {
				memset(rule, 0, sizeof(struct hinic_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter, support src, dst ports");
				return -rte_errno;
			}

			tcp_mask = (const struct rte_flow_item_tcp *)item->mask;
			if (tcp_mask->hdr.sent_seq ||
				tcp_mask->hdr.recv_ack ||
				tcp_mask->hdr.data_off ||
				tcp_mask->hdr.tcp_flags ||
				tcp_mask->hdr.rx_win ||
				tcp_mask->hdr.cksum ||
				tcp_mask->hdr.tcp_urp) {
				(void)memset(rule, 0,
					sizeof(struct hinic_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter, support tcp");
				return -rte_errno;
			}

			rule->mode = HINIC_FDIR_MODE_TCAM;
			rule->mask.tunnel_flag = UINT16_MAX;
			rule->mask.tunnel_inner_src_port_mask =
							tcp_mask->hdr.src_port;
			rule->mask.tunnel_inner_dst_port_mask =
							tcp_mask->hdr.dst_port;
			rule->mask.proto_mask = UINT16_MAX;

			rule->hinic_fdir.proto = IP_HEADER_PROTOCOL_TYPE_TCP;
			if (item->spec) {
				tcp_spec =
				(const struct rte_flow_item_tcp *)item->spec;
				rule->hinic_fdir.tunnel_inner_src_port =
							tcp_spec->hdr.src_port;
				rule->hinic_fdir.tunnel_inner_dst_port =
							tcp_spec->hdr.dst_port;
			}
		} else if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
			/*
			 * Only care about src & dst ports,
			 * others should be masked.
			 */
			if (!item->mask) {
				memset(rule, 0, sizeof(struct hinic_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter, support src, dst ports");
				return -rte_errno;
			}

			udp_mask = (const struct rte_flow_item_udp *)item->mask;
			if (udp_mask->hdr.dgram_len ||
			    udp_mask->hdr.dgram_cksum) {
				memset(rule, 0, sizeof(struct hinic_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter, support udp");
				return -rte_errno;
			}

			rule->mode = HINIC_FDIR_MODE_TCAM;
			rule->mask.tunnel_flag = UINT16_MAX;
			rule->mask.tunnel_inner_src_port_mask =
							udp_mask->hdr.src_port;
			rule->mask.tunnel_inner_dst_port_mask =
							udp_mask->hdr.dst_port;
			rule->mask.proto_mask = UINT16_MAX;

			rule->hinic_fdir.proto = IP_HEADER_PROTOCOL_TYPE_UDP;
			if (item->spec) {
				udp_spec =
				(const struct rte_flow_item_udp *)item->spec;
				rule->hinic_fdir.tunnel_inner_src_port =
							udp_spec->hdr.src_port;
				rule->hinic_fdir.tunnel_inner_dst_port =
							udp_spec->hdr.dst_port;
			}
		} else if (item->type == RTE_FLOW_ITEM_TYPE_ANY) {
			rule->mode = HINIC_FDIR_MODE_TCAM;
			rule->mask.tunnel_flag = UINT16_MAX;
		} else {
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter, support tcp/udp");
			return -rte_errno;
		}

		/* get next no void item */
		*in_out_item = next_no_void_pattern(pattern, item);
	}

	return 0;
}

static int hinic_check_tcam_tunnel_item_ele(const struct rte_flow_item *item,
					const struct rte_flow_item pattern[],
					struct hinic_fdir_rule *rule,
					struct rte_flow_error *error)
{
	if (hinic_normal_item_check_ether(&item, pattern, error) ||
		hinic_normal_item_check_ip(&item, pattern, rule, error) ||
		hinic_tunnel_item_check_l4(&item, pattern, rule, error) ||
		hinic_tunnel_item_check_vxlan(&item, pattern, rule, error) ||
		hinic_tunnel_inner_item_check_l4(&item, pattern, rule, error) ||
		hinic_normal_item_check_end(item, rule, error))
		return -rte_errno;

	return 0;
}

static int hinic_check_normal_attr_ele(const struct rte_flow_attr *attr,
					struct hinic_fdir_rule *rule,
					struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->egress) {
		memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->priority) {
		memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			attr, "Not support priority.");
		return -rte_errno;
	}

	return 0;
}

static int hinic_check_normal_act_ele(const struct rte_flow_item *item,
				const struct rte_flow_action actions[],
				struct hinic_fdir_rule *rule,
				struct rte_flow_error *error)
{
	const struct rte_flow_action *act;

	/* Check if the first not void action is QUEUE */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
			item, "Not supported action.");
		return -rte_errno;
	}

	rule->queue = ((const struct rte_flow_action_queue *)act->conf)->index;

	/* Check if the next not void item is END */
	act = next_no_void_action(actions, act);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(rule, 0, sizeof(struct hinic_fdir_rule));
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

/**
 * Parse the rule to see if it is a IP or MAC VLAN flow director rule.
 * And get the flow director filter info BTW.
 * UDP/TCP/SCTP PATTERN:
 * The first not void item can be ETH or IPV4 or IPV6
 * The second not void item must be IPV4 or IPV6 if the first one is ETH.
 * The next not void item could be UDP or TCP(optional)
 * The next not void item must be END.
 * ACTION:
 * The first not void action should be QUEUE.
 * The second not void optional action should be MARK,
 * mark_id is a uint32_t number.
 * The next not void action should be END.
 * UDP/TCP pattern example:
 * ITEM          Spec	                                    Mask
 * ETH            NULL                                    NULL
 * IPV4           src_addr  1.2.3.6                 0xFFFFFFFF
 *                   dst_addr  1.2.3.5                 0xFFFFFFFF
 * UDP/TCP    src_port  80                         0xFFFF
 *                   dst_port  80                         0xFFFF
 * END
 * Other members in mask and spec should set to 0x00.
 * Item->last should be NULL.
 */
static int
hinic_parse_fdir_filter_normal(const struct rte_flow_attr *attr,
			       const struct rte_flow_item pattern[],
			       const struct rte_flow_action actions[],
			       struct hinic_fdir_rule *rule,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item *item = NULL;

	if (hinic_check_filter_arg(attr, pattern, actions, error))
		return -rte_errno;

	if (hinic_check_normal_item_ele(item, pattern, rule, error))
		return -rte_errno;

	if (hinic_check_normal_attr_ele(attr, rule, error))
		return -rte_errno;

	if (hinic_check_normal_act_ele(item, actions, rule, error))
		return -rte_errno;

	return 0;
}

/**
 * Parse the rule to see if it is a IP or MAC VLAN flow director rule.
 * And get the flow director filter info BTW.
 * UDP/TCP/SCTP PATTERN:
 * The first not void item can be ETH or IPV4 or IPV6
 * The second not void item must be IPV4 or IPV6 if the first one is ETH.
 * The next not void item can be ANY/TCP/UDP
 * ACTION:
 * The first not void action should be QUEUE.
 * The second not void optional action should be MARK,
 * mark_id is a uint32_t number.
 * The next not void action should be END.
 * UDP/TCP pattern example:
 * ITEM                 Spec	                       Mask
 * ETH            NULL                                 NULL
 * IPV4           src_addr  1.2.3.6                 0xFFFFFFFF
 *                dst_addr  1.2.3.5                 0xFFFFFFFF
 * UDP/TCP        src_port  80                      0xFFFF
 *                dst_port  80                      0xFFFF
 * END
 * Other members in mask and spec should set to 0x00.
 * Item->last should be NULL.
 */
static int
hinic_parse_fdir_filter_tcam_normal(const struct rte_flow_attr *attr,
			       const struct rte_flow_item pattern[],
			       const struct rte_flow_action actions[],
			       struct hinic_fdir_rule *rule,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item *item = NULL;

	if (hinic_check_filter_arg(attr, pattern, actions, error))
		return -rte_errno;

	if (hinic_check_tcam_normal_item_ele(item, pattern, rule, error))
		return -rte_errno;

	if (hinic_check_normal_attr_ele(attr, rule, error))
		return -rte_errno;

	if (hinic_check_normal_act_ele(item, actions, rule, error))
		return -rte_errno;

	return 0;
}

/**
 * Parse the rule to see if it is a IP or MAC VLAN flow director rule.
 * And get the flow director filter info BTW.
 * UDP/TCP/SCTP PATTERN:
 * The first not void item can be ETH or IPV4 or IPV6
 * The second not void item must be IPV4 or IPV6 if the first one is ETH.
 * The next not void item must be UDP
 * The next not void item must be VXLAN(optional)
 * The first not void item can be ETH or IPV4 or IPV6
 * The next not void item could be ANY or UDP or TCP(optional)
 * The next not void item must be END.
 * ACTION:
 * The first not void action should be QUEUE.
 * The second not void optional action should be MARK,
 * mark_id is a uint32_t number.
 * The next not void action should be END.
 * UDP/TCP pattern example:
 * ITEM             Spec	                    Mask
 * ETH            NULL                              NULL
 * IPV4        src_addr  1.2.3.6                 0xFFFFFFFF
 *             dst_addr  1.2.3.5                 0xFFFFFFFF
 * UDP            NULL                              NULL
 * VXLAN          NULL                              NULL
 * UDP/TCP     src_port  80                      0xFFFF
 *             dst_port  80                      0xFFFF
 * END
 * Other members in mask and spec should set to 0x00.
 * Item->last should be NULL.
 */
static int
hinic_parse_fdir_filter_tacm_tunnel(const struct rte_flow_attr *attr,
			       const struct rte_flow_item pattern[],
			       const struct rte_flow_action actions[],
			       struct hinic_fdir_rule *rule,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item *item = NULL;

	if (hinic_check_filter_arg(attr, pattern, actions, error))
		return -rte_errno;

	if (hinic_check_tcam_tunnel_item_ele(item, pattern, rule, error))
		return -rte_errno;

	if (hinic_check_normal_attr_ele(attr, rule, error))
		return -rte_errno;

	if (hinic_check_normal_act_ele(item, actions, rule, error))
		return -rte_errno;

	return 0;
}

static int hinic_parse_fdir_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct hinic_fdir_rule *rule,
			struct rte_flow_error *error)
{
	int ret;

	ret = hinic_parse_fdir_filter_normal(attr, pattern, actions,
						rule, error);
	if (!ret)
		goto step_next;

	ret = hinic_parse_fdir_filter_tcam_normal(attr, pattern, actions,
						rule, error);
	if (!ret)
		goto step_next;

	ret = hinic_parse_fdir_filter_tacm_tunnel(attr, pattern, actions,
						rule, error);
	if (ret)
		return ret;

step_next:
	if (rule->queue >= dev->data->nb_rx_queues)
		return -ENOTSUP;

	return ret;
}

/**
 * Check if the flow rule is supported by nic.
 * It only checks the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 */
static int hinic_flow_validate(struct rte_eth_dev *dev,
				const struct rte_flow_attr *attr,
				const struct rte_flow_item pattern[],
				const struct rte_flow_action actions[],
				struct rte_flow_error *error)
{
	struct rte_eth_ethertype_filter ethertype_filter;
	struct rte_eth_ntuple_filter ntuple_filter;
	struct hinic_fdir_rule fdir_rule;
	int ret;

	memset(&ntuple_filter, 0, sizeof(struct rte_eth_ntuple_filter));
	ret = hinic_parse_ntuple_filter(dev, attr, pattern,
				actions, &ntuple_filter, error);
	if (!ret)
		return 0;

	memset(&ethertype_filter, 0, sizeof(struct rte_eth_ethertype_filter));
	ret = hinic_parse_ethertype_filter(dev, attr, pattern,
				actions, &ethertype_filter, error);

	if (!ret)
		return 0;

	memset(&fdir_rule, 0, sizeof(struct hinic_fdir_rule));
	ret = hinic_parse_fdir_filter(dev, attr, pattern,
				actions, &fdir_rule, error);

	return ret;
}

static inline int ntuple_ip_filter(struct rte_eth_ntuple_filter *filter,
		 struct hinic_5tuple_filter_info *hinic_filter_info)
{
	switch (filter->dst_ip_mask) {
	case UINT32_MAX:
		hinic_filter_info->dst_ip_mask = 0;
		hinic_filter_info->dst_ip = filter->dst_ip;
		break;
	case 0:
		hinic_filter_info->dst_ip_mask = 1;
		hinic_filter_info->dst_ip = 0;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid dst_ip mask.");
		return -EINVAL;
	}

	switch (filter->src_ip_mask) {
	case UINT32_MAX:
		hinic_filter_info->src_ip_mask = 0;
		hinic_filter_info->src_ip = filter->src_ip;
		break;
	case 0:
		hinic_filter_info->src_ip_mask = 1;
		hinic_filter_info->src_ip = 0;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid src_ip mask.");
		return -EINVAL;
	}
	return 0;
}

static inline int ntuple_port_filter(struct rte_eth_ntuple_filter *filter,
		   struct hinic_5tuple_filter_info *hinic_filter_info)
{
	switch (filter->dst_port_mask) {
	case UINT16_MAX:
		hinic_filter_info->dst_port_mask = 0;
		hinic_filter_info->dst_port = filter->dst_port;
		break;
	case 0:
		hinic_filter_info->dst_port_mask = 1;
		hinic_filter_info->dst_port = 0;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid dst_port mask.");
		return -EINVAL;
	}

	switch (filter->src_port_mask) {
	case UINT16_MAX:
		hinic_filter_info->src_port_mask = 0;
		hinic_filter_info->src_port = filter->src_port;
		break;
	case 0:
		hinic_filter_info->src_port_mask = 1;
		hinic_filter_info->src_port = 0;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid src_port mask.");
		return -EINVAL;
	}

	return 0;
}

static inline int ntuple_proto_filter(struct rte_eth_ntuple_filter *filter,
		    struct hinic_5tuple_filter_info *hinic_filter_info)
{
	switch (filter->proto_mask) {
	case UINT8_MAX:
		hinic_filter_info->proto_mask = 0;
		hinic_filter_info->proto = filter->proto;
		break;
	case 0:
		hinic_filter_info->proto_mask = 1;
		hinic_filter_info->proto = 0;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid protocol mask.");
		return -EINVAL;
	}

	return 0;
}

static inline int ntuple_filter_to_5tuple(struct rte_eth_ntuple_filter *filter,
			struct hinic_5tuple_filter_info *filter_info)
{
	if (filter->queue >= HINIC_MAX_RX_QUEUE_NUM ||
		filter->priority > HINIC_MAX_N_TUPLE_PRIO ||
		filter->priority < HINIC_MIN_N_TUPLE_PRIO)
		return -EINVAL;

	if (ntuple_ip_filter(filter, filter_info) ||
		ntuple_port_filter(filter, filter_info) ||
		ntuple_proto_filter(filter, filter_info))
		return -EINVAL;

	filter_info->priority = (uint8_t)filter->priority;
	return 0;
}

static inline struct hinic_5tuple_filter *
hinic_5tuple_filter_lookup(struct hinic_5tuple_filter_list *filter_list,
			   struct hinic_5tuple_filter_info *key)
{
	struct hinic_5tuple_filter *it;

	TAILQ_FOREACH(it, filter_list, entries) {
		if (memcmp(key, &it->filter_info,
			sizeof(struct hinic_5tuple_filter_info)) == 0) {
			return it;
		}
	}

	return NULL;
}

static int hinic_set_lacp_tcam(struct hinic_nic_dev *nic_dev)
{
	struct tag_pa_rule lacp_rule;
	struct tag_pa_action lacp_action;

	memset(&lacp_rule, 0, sizeof(lacp_rule));
	memset(&lacp_action, 0, sizeof(lacp_action));
	/* LACP TCAM rule */
	lacp_rule.eth_type = PA_ETH_TYPE_OTHER;
	lacp_rule.l2_header.eth_type.val16 = 0x8809;
	lacp_rule.l2_header.eth_type.mask16 = 0xffff;

	/* LACP TCAM action */
	lacp_action.err_type = 0x3f; /* err from ipsu, not convert */
	lacp_action.fwd_action = 0x7; /* 0x3:drop; 0x7: not convert */
	lacp_action.pkt_type = PKT_LACP_TYPE;
	lacp_action.pri = 0x0;
	lacp_action.push_len = 0xf; /* push_len:0xf, not convert */

	return hinic_set_fdir_tcam(nic_dev->hwdev, TCAM_PKT_LACP,
					&lacp_rule, &lacp_action);
}

static int hinic_set_bgp_dport_tcam(struct hinic_nic_dev *nic_dev)
{
	struct tag_pa_rule bgp_rule;
	struct tag_pa_action bgp_action;

	memset(&bgp_rule, 0, sizeof(bgp_rule));
	memset(&bgp_action, 0, sizeof(bgp_action));
	/* BGP TCAM rule */
	bgp_rule.eth_type = PA_ETH_TYPE_IPV4; /* Eth type is IPV4 */
	bgp_rule.ip_header.protocol.val8 = IP_HEADER_PROTOCOL_TYPE_TCP;
	bgp_rule.ip_header.protocol.mask8 = UINT8_MAX;
	bgp_rule.ip_protocol_type = PA_IP_PROTOCOL_TYPE_TCP;
	bgp_rule.eth_ip_tcp.dport.val16 = BGP_DPORT_ID; /* Dport is 179 */
	bgp_rule.eth_ip_tcp.dport.mask16 = UINT16_MAX;

	/* BGP TCAM action */
	bgp_action.err_type = 0x3f; /* err from ipsu, not convert */
	bgp_action.fwd_action = 0x7; /* 0x3:drop; 0x7: not convert */
	bgp_action.pkt_type = PKT_BGPD_DPORT_TYPE; /* bgp_dport: 83 */
	bgp_action.pri = 0xf; /* pri of BGP is 0xf, result from ipsu parse
			       * results, not need to convert
			       */
	bgp_action.push_len = 0xf; /* push_len:0xf, not convert */

	return hinic_set_fdir_tcam(nic_dev->hwdev,
			TCAM_PKT_BGP_DPORT, &bgp_rule, &bgp_action);
}

static int hinic_set_bgp_sport_tcam(struct hinic_nic_dev *nic_dev)
{
	struct tag_pa_rule bgp_rule;
	struct tag_pa_action bgp_action;

	memset(&bgp_rule, 0, sizeof(bgp_rule));
	memset(&bgp_action, 0, sizeof(bgp_action));
	/* BGP TCAM rule */
	bgp_rule.eth_type = PA_ETH_TYPE_IPV4;
	bgp_rule.ip_header.protocol.val8 = IP_HEADER_PROTOCOL_TYPE_TCP;
	bgp_rule.ip_header.protocol.mask8 = UINT8_MAX;
	bgp_rule.ip_protocol_type = PA_IP_PROTOCOL_TYPE_TCP;
	bgp_rule.eth_ip_tcp.sport.val16 = BGP_DPORT_ID;
	bgp_rule.eth_ip_tcp.sport.mask16 = UINT16_MAX;

	/* BGP TCAM action */
	bgp_action.err_type = 0x3f; /* err from ipsu, not convert */
	bgp_action.fwd_action = 0x7; /* 0x3:drop; 0x7: not convert */
	bgp_action.pkt_type = PKT_BGPD_SPORT_TYPE; /* bgp:sport: 84 */
	bgp_action.pri = 0xf; /* pri of BGP is 0xf, result from ipsu parse
			       * results, not need to convert
			       */
	bgp_action.push_len = 0xf; /* push_len:0xf, not convert */

	return hinic_set_fdir_tcam(nic_dev->hwdev, TCAM_PKT_BGP_SPORT,
					&bgp_rule, &bgp_action);
}

static int hinic_set_vrrp_tcam(struct hinic_nic_dev *nic_dev)
{
	struct tag_pa_rule vrrp_rule;
	struct tag_pa_action vrrp_action;

	memset(&vrrp_rule, 0, sizeof(vrrp_rule));
	memset(&vrrp_action, 0, sizeof(vrrp_action));
	/* VRRP TCAM rule */
	vrrp_rule.eth_type = PA_ETH_TYPE_IPV4;
	vrrp_rule.ip_protocol_type = PA_IP_PROTOCOL_TYPE_TCP;
	vrrp_rule.ip_header.protocol.mask8 = 0xff;
	vrrp_rule.ip_header.protocol.val8 = PA_IP_PROTOCOL_TYPE_VRRP;

	/* VRRP TCAM action */
	vrrp_action.err_type = 0x3f;
	vrrp_action.fwd_action = 0x7;
	vrrp_action.pkt_type = PKT_VRRP_TYPE; /* VRRP: 85 */
	vrrp_action.pri = 0xf;
	vrrp_action.push_len = 0xf;

	return hinic_set_fdir_tcam(nic_dev->hwdev, TCAM_PKT_VRRP,
					&vrrp_rule, &vrrp_action);
}

/**
 *  Clear all fdir configuration.
 *
 * @param nic_dev
 *   The hardware interface of a Ethernet device.
 *
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
void hinic_free_fdir_filter(struct hinic_nic_dev *nic_dev)
{
	(void)hinic_set_fdir_filter(nic_dev->hwdev, 0, 0, 0, false);

	(void)hinic_set_fdir_tcam_rule_filter(nic_dev->hwdev, false);

	(void)hinic_clear_fdir_tcam(nic_dev->hwdev, TCAM_PKT_BGP_DPORT);

	(void)hinic_clear_fdir_tcam(nic_dev->hwdev, TCAM_PKT_BGP_SPORT);

	(void)hinic_clear_fdir_tcam(nic_dev->hwdev, TCAM_PKT_VRRP);

	(void)hinic_clear_fdir_tcam(nic_dev->hwdev, TCAM_PKT_LACP);

	(void)hinic_flush_tcam_rule(nic_dev->hwdev);
}

static int hinic_filter_info_init(struct hinic_5tuple_filter *filter,
		       struct hinic_filter_info *filter_info)
{
	switch (filter->filter_info.proto) {
	case IPPROTO_TCP:
		/* Filter type is bgp type if dst_port or src_port is 179 */
		if (filter->filter_info.dst_port == RTE_BE16(BGP_DPORT_ID) &&
			!(filter->filter_info.dst_port_mask)) {
			filter_info->pkt_type = PKT_BGPD_DPORT_TYPE;
		} else if (filter->filter_info.src_port ==
			RTE_BE16(BGP_DPORT_ID) &&
			!(filter->filter_info.src_port_mask)) {
			filter_info->pkt_type = PKT_BGPD_SPORT_TYPE;
		} else {
			PMD_DRV_LOG(INFO, "TCP PROTOCOL:5tuple filters"
			" just support BGP now, proto:0x%x, "
			"dst_port:0x%x, dst_port_mask:0x%x."
			"src_port:0x%x, src_port_mask:0x%x.",
			filter->filter_info.proto,
			filter->filter_info.dst_port,
			filter->filter_info.dst_port_mask,
			filter->filter_info.src_port,
			filter->filter_info.src_port_mask);
			return -EINVAL;
		}
		break;

	case IPPROTO_VRRP:
		filter_info->pkt_type = PKT_VRRP_TYPE;
		break;

	case IPPROTO_ICMP:
		filter_info->pkt_type = PKT_ICMP_IPV4_TYPE;
		break;

	case IPPROTO_ICMPV6:
		filter_info->pkt_type = PKT_ICMP_IPV6_TYPE;
		break;

	default:
		PMD_DRV_LOG(ERR, "5tuple filters just support BGP/VRRP/ICMP now, "
		"proto: 0x%x, dst_port: 0x%x, dst_port_mask: 0x%x."
		"src_port: 0x%x, src_port_mask: 0x%x.",
		filter->filter_info.proto, filter->filter_info.dst_port,
		filter->filter_info.dst_port_mask,
		filter->filter_info.src_port,
		filter->filter_info.src_port_mask);
		return -EINVAL;
	}

	return 0;
}

static int hinic_lookup_new_filter(struct hinic_5tuple_filter *filter,
			struct hinic_filter_info *filter_info, int *index)
{
	int type_id;

	type_id = HINIC_PKT_TYPE_FIND_ID(filter_info->pkt_type);

	if (type_id > HINIC_MAX_Q_FILTERS - 1) {
		PMD_DRV_LOG(ERR, "Pkt filters only support 64 filter type.");
		return -EINVAL;
	}

	if (!(filter_info->type_mask & (1 << type_id))) {
		filter_info->type_mask |= 1 << type_id;
		filter->index = type_id;
		filter_info->pkt_filters[type_id].enable = true;
		filter_info->pkt_filters[type_id].pkt_proto =
						filter->filter_info.proto;
		TAILQ_INSERT_TAIL(&filter_info->fivetuple_list,
				  filter, entries);
	} else {
		PMD_DRV_LOG(ERR, "Filter type: %d exists.", type_id);
		return -EIO;
	}

	*index = type_id;
	return 0;
}

/*
 * Add a 5tuple filter
 *
 * @param dev:
 *  Pointer to struct rte_eth_dev.
 * @param filter:
 *  Pointer to the filter that will be added.
 * @return
 *    - On success, zero.
 *    - On failure, a negative value.
 */
static int hinic_add_5tuple_filter(struct rte_eth_dev *dev,
				struct hinic_5tuple_filter *filter)
{
	struct hinic_filter_info *filter_info =
		HINIC_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	int i, ret_fw;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (hinic_filter_info_init(filter, filter_info) ||
		hinic_lookup_new_filter(filter, filter_info, &i))
		return -EFAULT;

	ret_fw = hinic_set_fdir_filter(nic_dev->hwdev, filter_info->pkt_type,
					filter_info->qid,
					filter_info->pkt_filters[i].enable,
					true);
	if (ret_fw) {
		PMD_DRV_LOG(ERR, "Set fdir filter failed, type: 0x%x, qid: 0x%x, enable: 0x%x",
			filter_info->pkt_type, filter->queue,
			filter_info->pkt_filters[i].enable);
		return -EFAULT;
	}

	PMD_DRV_LOG(INFO, "Add 5tuple succeed, type: 0x%x, qid: 0x%x, enable: 0x%x",
			filter_info->pkt_type, filter_info->qid,
			filter_info->pkt_filters[filter->index].enable);

	switch (filter->filter_info.proto) {
	case IPPROTO_TCP:
		if (filter->filter_info.dst_port == RTE_BE16(BGP_DPORT_ID)) {
			ret_fw = hinic_set_bgp_dport_tcam(nic_dev);
			if (ret_fw) {
				PMD_DRV_LOG(ERR, "Set dport bgp failed, "
					"type: 0x%x, qid: 0x%x, enable: 0x%x",
					filter_info->pkt_type, filter->queue,
					filter_info->pkt_filters[i].enable);
				return -EFAULT;
			}

			PMD_DRV_LOG(INFO, "Set dport bgp succeed, qid: 0x%x, enable: 0x%x",
				filter->queue,
				filter_info->pkt_filters[i].enable);
		} else if (filter->filter_info.src_port ==
			RTE_BE16(BGP_DPORT_ID)) {
			ret_fw = hinic_set_bgp_sport_tcam(nic_dev);
			if (ret_fw) {
				PMD_DRV_LOG(ERR, "Set sport bgp failed, "
					"type: 0x%x, qid: 0x%x, enable: 0x%x",
					filter_info->pkt_type, filter->queue,
					filter_info->pkt_filters[i].enable);
				return -EFAULT;
			}

			PMD_DRV_LOG(INFO, "Set sport bgp succeed, qid: 0x%x, enable: 0x%x",
					filter->queue,
					filter_info->pkt_filters[i].enable);
		}

		break;

	case IPPROTO_VRRP:
		ret_fw = hinic_set_vrrp_tcam(nic_dev);
		if (ret_fw) {
			PMD_DRV_LOG(ERR, "Set VRRP failed, "
				"type: 0x%x, qid: 0x%x, enable: 0x%x",
				filter_info->pkt_type, filter->queue,
				filter_info->pkt_filters[i].enable);
			return -EFAULT;
		}
		PMD_DRV_LOG(INFO, "Set VRRP succeed, qid: 0x%x, enable: 0x%x",
				filter->queue,
				filter_info->pkt_filters[i].enable);
		break;

	default:
		break;
	}

	return 0;
}

/*
 * Remove a 5tuple filter
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param filter
 *  The pointer of the filter will be removed.
 */
static void hinic_remove_5tuple_filter(struct rte_eth_dev *dev,
			   struct hinic_5tuple_filter *filter)
{
	struct hinic_filter_info *filter_info =
		HINIC_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	switch (filter->filter_info.proto) {
	case IPPROTO_VRRP:
		(void)hinic_clear_fdir_tcam(nic_dev->hwdev, TCAM_PKT_VRRP);
		break;

	case IPPROTO_TCP:
		if (filter->filter_info.dst_port == RTE_BE16(BGP_DPORT_ID))
			(void)hinic_clear_fdir_tcam(nic_dev->hwdev,
							TCAM_PKT_BGP_DPORT);
		else if (filter->filter_info.src_port == RTE_BE16(BGP_DPORT_ID))
			(void)hinic_clear_fdir_tcam(nic_dev->hwdev,
							TCAM_PKT_BGP_SPORT);
		break;

	default:
		break;
	}

	hinic_filter_info_init(filter, filter_info);

	filter_info->pkt_filters[filter->index].enable = false;
	filter_info->pkt_filters[filter->index].pkt_proto = 0;

	PMD_DRV_LOG(INFO, "Del 5tuple succeed, type: 0x%x, qid: 0x%x, enable: 0x%x",
		filter_info->pkt_type,
		filter_info->pkt_filters[filter->index].qid,
		filter_info->pkt_filters[filter->index].enable);
	(void)hinic_set_fdir_filter(nic_dev->hwdev, filter_info->pkt_type,
				filter_info->pkt_filters[filter->index].qid,
				filter_info->pkt_filters[filter->index].enable,
				true);

	filter_info->pkt_type = 0;
	filter_info->qid = 0;
	filter_info->pkt_filters[filter->index].qid = 0;
	filter_info->type_mask &= ~(1 <<  (filter->index));
	TAILQ_REMOVE(&filter_info->fivetuple_list, filter, entries);

	rte_free(filter);
}

/*
 * Add or delete a ntuple filter
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param ntuple_filter
 *  Pointer to struct rte_eth_ntuple_filter
 * @param add
 *  If true, add filter; if false, remove filter
 * @return
 *    - On success, zero.
 *    - On failure, a negative value.
 */
static int hinic_add_del_ntuple_filter(struct rte_eth_dev *dev,
				struct rte_eth_ntuple_filter *ntuple_filter,
				bool add)
{
	struct hinic_filter_info *filter_info =
		HINIC_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	struct hinic_5tuple_filter_info filter_5tuple;
	struct hinic_5tuple_filter *filter;
	int ret;

	if (ntuple_filter->flags != RTE_5TUPLE_FLAGS) {
		PMD_DRV_LOG(ERR, "Only 5tuple is supported.");
		return -EINVAL;
	}

	memset(&filter_5tuple, 0, sizeof(struct hinic_5tuple_filter_info));
	ret = ntuple_filter_to_5tuple(ntuple_filter, &filter_5tuple);
	if (ret < 0)
		return ret;

	filter = hinic_5tuple_filter_lookup(&filter_info->fivetuple_list,
					 &filter_5tuple);
	if (filter != NULL && add) {
		PMD_DRV_LOG(ERR, "Filter exists.");
		return -EEXIST;
	}
	if (filter == NULL && !add) {
		PMD_DRV_LOG(ERR, "Filter doesn't exist.");
		return -ENOENT;
	}

	if (add) {
		filter = rte_zmalloc("hinic_5tuple_filter",
				sizeof(struct hinic_5tuple_filter), 0);
		if (filter == NULL)
			return -ENOMEM;
		rte_memcpy(&filter->filter_info, &filter_5tuple,
				sizeof(struct hinic_5tuple_filter_info));
		filter->queue = ntuple_filter->queue;

		filter_info->qid = ntuple_filter->queue;

		ret = hinic_add_5tuple_filter(dev, filter);
		if (ret)
			rte_free(filter);

		return ret;
	}

	hinic_remove_5tuple_filter(dev, filter);

	return 0;
}

static inline int
hinic_check_ethertype_filter(struct rte_eth_ethertype_filter *filter)
{
	if (filter->queue >= HINIC_MAX_RX_QUEUE_NUM)
		return -EINVAL;

	if (filter->ether_type == RTE_ETHER_TYPE_IPV4 ||
		filter->ether_type == RTE_ETHER_TYPE_IPV6) {
		PMD_DRV_LOG(ERR, "Unsupported ether_type(0x%04x) in"
			" ethertype filter", filter->ether_type);
		return -EINVAL;
	}

	if (filter->flags & RTE_ETHTYPE_FLAGS_MAC) {
		PMD_DRV_LOG(ERR, "Mac compare is not supported");
		return -EINVAL;
	}
	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP) {
		PMD_DRV_LOG(ERR, "Drop option is not supported");
		return -EINVAL;
	}

	return 0;
}

static inline int
hinic_ethertype_filter_lookup(struct hinic_filter_info *filter_info,
			      struct hinic_pkt_filter *ethertype_filter)
{
	switch (ethertype_filter->pkt_proto) {
	case RTE_ETHER_TYPE_SLOW:
		filter_info->pkt_type = PKT_LACP_TYPE;
		break;

	case RTE_ETHER_TYPE_ARP:
		filter_info->pkt_type = PKT_ARP_TYPE;
		break;

	default:
		PMD_DRV_LOG(ERR, "Just support LACP/ARP for ethertype filters");
		return -EIO;
	}

	return HINIC_PKT_TYPE_FIND_ID(filter_info->pkt_type);
}

static inline int
hinic_ethertype_filter_insert(struct hinic_filter_info *filter_info,
			      struct hinic_pkt_filter *ethertype_filter)
{
	int id;

	/* Find LACP or VRRP type id */
	id = hinic_ethertype_filter_lookup(filter_info, ethertype_filter);
	if (id < 0)
		return -EINVAL;

	if (!(filter_info->type_mask & (1 << id))) {
		filter_info->type_mask |= 1 << id;
		filter_info->pkt_filters[id].pkt_proto =
			ethertype_filter->pkt_proto;
		filter_info->pkt_filters[id].enable = ethertype_filter->enable;
		filter_info->qid = ethertype_filter->qid;
		return id;
	}

	PMD_DRV_LOG(ERR, "Filter type: %d exists", id);
	return -EINVAL;
}

static inline void
hinic_ethertype_filter_remove(struct hinic_filter_info *filter_info,
			      uint8_t idx)
{
	if (idx >= HINIC_MAX_Q_FILTERS)
		return;

	filter_info->pkt_type = 0;
	filter_info->type_mask &= ~(1 << idx);
	filter_info->pkt_filters[idx].pkt_proto = (uint16_t)0;
	filter_info->pkt_filters[idx].enable = FALSE;
	filter_info->pkt_filters[idx].qid = 0;
}

static inline int
hinic_add_del_ethertype_filter(struct rte_eth_dev *dev,
			       struct rte_eth_ethertype_filter *filter,
			       bool add)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic_filter_info *filter_info =
		HINIC_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	struct hinic_pkt_filter ethertype_filter;
	int i;
	int ret_fw;

	if (hinic_check_ethertype_filter(filter))
		return -EINVAL;

	if (add) {
		ethertype_filter.pkt_proto = filter->ether_type;
		ethertype_filter.enable = TRUE;
		ethertype_filter.qid = (u8)filter->queue;
		i = hinic_ethertype_filter_insert(filter_info,
						    &ethertype_filter);
		if (i < 0)
			return -ENOSPC;

		ret_fw = hinic_set_fdir_filter(nic_dev->hwdev,
				filter_info->pkt_type, filter_info->qid,
				filter_info->pkt_filters[i].enable, true);
		if (ret_fw) {
			PMD_DRV_LOG(ERR, "add ethertype failed, type: 0x%x, qid: 0x%x, enable: 0x%x",
				filter_info->pkt_type, filter->queue,
				filter_info->pkt_filters[i].enable);

			hinic_ethertype_filter_remove(filter_info, i);
			return -ENOENT;
		}
		PMD_DRV_LOG(INFO, "Add ethertype succeed, type: 0x%x, qid: 0x%x, enable: 0x%x",
				filter_info->pkt_type, filter->queue,
				filter_info->pkt_filters[i].enable);

		switch (ethertype_filter.pkt_proto) {
		case RTE_ETHER_TYPE_SLOW:
			ret_fw = hinic_set_lacp_tcam(nic_dev);
			if (ret_fw) {
				PMD_DRV_LOG(ERR, "Add lacp tcam failed");
				hinic_ethertype_filter_remove(filter_info, i);
				return -ENOENT;
			}

			PMD_DRV_LOG(INFO, "Add lacp tcam succeed");
			break;
		default:
			break;
		}
	} else {
		ethertype_filter.pkt_proto = filter->ether_type;
		i = hinic_ethertype_filter_lookup(filter_info,
						&ethertype_filter);
		if (i < 0)
			return -EINVAL;

		if ((filter_info->type_mask & (1 << i))) {
			filter_info->pkt_filters[i].enable = FALSE;
			(void)hinic_set_fdir_filter(nic_dev->hwdev,
					filter_info->pkt_type,
					filter_info->pkt_filters[i].qid,
					filter_info->pkt_filters[i].enable,
					true);

			PMD_DRV_LOG(INFO, "Del ethertype succeed, type: 0x%x, qid: 0x%x, enable: 0x%x",
					filter_info->pkt_type,
					filter_info->pkt_filters[i].qid,
					filter_info->pkt_filters[i].enable);

			switch (ethertype_filter.pkt_proto) {
			case RTE_ETHER_TYPE_SLOW:
				(void)hinic_clear_fdir_tcam(nic_dev->hwdev,
								TCAM_PKT_LACP);
				PMD_DRV_LOG(INFO, "Del lacp tcam succeed");
				break;
			default:
				break;
			}

			hinic_ethertype_filter_remove(filter_info, i);

		} else {
			PMD_DRV_LOG(ERR, "Ethertype doesn't exist, type: 0x%x, qid: 0x%x, enable: 0x%x",
					filter_info->pkt_type, filter->queue,
					filter_info->pkt_filters[i].enable);
			return -ENOENT;
		}
	}

	return 0;
}

static int hinic_fdir_info_init(struct hinic_fdir_rule *rule,
				struct hinic_fdir_info *fdir_info)
{
	switch (rule->mask.src_ipv4_mask) {
	case UINT32_MAX:
		fdir_info->fdir_flag = HINIC_ATR_FLOW_TYPE_IPV4_SIP;
		fdir_info->qid = rule->queue;
		fdir_info->fdir_key = rule->hinic_fdir.src_ip;
		return 0;

	case 0:
		break;

	default:
		PMD_DRV_LOG(ERR, "Invalid src_ip mask.");
		return -EINVAL;
	}

	switch (rule->mask.dst_ipv4_mask) {
	case UINT32_MAX:
		fdir_info->fdir_flag = HINIC_ATR_FLOW_TYPE_IPV4_DIP;
		fdir_info->qid = rule->queue;
		fdir_info->fdir_key = rule->hinic_fdir.dst_ip;
		return 0;

	case 0:
		break;

	default:
		PMD_DRV_LOG(ERR, "Invalid dst_ip mask.");
		return -EINVAL;
	}

	if (fdir_info->fdir_flag == 0) {
		PMD_DRV_LOG(ERR, "All support mask is NULL.");
		return -EINVAL;
	}

	return 0;
}

static inline int hinic_add_del_fdir_filter(struct rte_eth_dev *dev,
					struct hinic_fdir_rule *rule, bool add)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic_fdir_info fdir_info;
	int ret;

	memset(&fdir_info, 0, sizeof(struct hinic_fdir_info));

	ret = hinic_fdir_info_init(rule, &fdir_info);
	if (ret) {
		PMD_DRV_LOG(ERR, "Init hinic fdir info failed!");
		return ret;
	}

	if (add) {
		ret = hinic_set_normal_filter(nic_dev->hwdev, fdir_info.qid,
						true, fdir_info.fdir_key,
						true, fdir_info.fdir_flag);
		if (ret) {
			PMD_DRV_LOG(ERR, "Add fdir filter failed, flag: 0x%x, qid: 0x%x, key: 0x%x",
					fdir_info.fdir_flag, fdir_info.qid,
					fdir_info.fdir_key);
			return -ENOENT;
		}
		PMD_DRV_LOG(INFO, "Add fdir filter succeed, flag: 0x%x, qid: 0x%x, key: 0x%x",
				fdir_info.fdir_flag, fdir_info.qid,
				fdir_info.fdir_key);
	} else {
		ret = hinic_set_normal_filter(nic_dev->hwdev, fdir_info.qid,
						false, fdir_info.fdir_key, true,
						fdir_info.fdir_flag);
		if (ret) {
			PMD_DRV_LOG(ERR, "Del fdir filter failed, flag: 0x%x, qid: 0x%x, key: 0x%x",
				fdir_info.fdir_flag, fdir_info.qid,
				fdir_info.fdir_key);
			return -ENOENT;
		}
		PMD_DRV_LOG(INFO, "Del fdir filter succeed, flag: 0x%x, qid: 0x%x, key: 0x%x",
				fdir_info.fdir_flag, fdir_info.qid,
				fdir_info.fdir_key);
	}

	return 0;
}

static void tcam_translate_key_y(u8 *key_y, u8 *src_input, u8 *mask, u8 len)
{
	u8 idx;

	for (idx = 0; idx < len; idx++)
		key_y[idx] = src_input[idx] & mask[idx];
}

static void tcam_translate_key_x(u8 *key_x, u8 *key_y, u8 *mask, u8 len)
{
	u8 idx;

	for (idx = 0; idx < len; idx++)
		key_x[idx] = key_y[idx] ^ mask[idx];
}

static void tcam_key_calculate(struct tag_tcam_key *tcam_key,
				struct tag_tcam_cfg_rule *fdir_tcam_rule)
{
	tcam_translate_key_y(fdir_tcam_rule->key.y,
		(u8 *)(&tcam_key->key_info),
		(u8 *)(&tcam_key->key_mask),
		TCAM_FLOW_KEY_SIZE);
	tcam_translate_key_x(fdir_tcam_rule->key.x,
		fdir_tcam_rule->key.y,
		(u8 *)(&tcam_key->key_mask),
		TCAM_FLOW_KEY_SIZE);
}

static int hinic_fdir_tcam_ipv4_init(struct rte_eth_dev *dev,
				     struct hinic_fdir_rule *rule,
				     struct tag_tcam_key *tcam_key)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	switch (rule->mask.dst_ipv4_mask) {
	case UINT32_MAX:
		tcam_key->key_info.ext_dip_h =
			(rule->hinic_fdir.dst_ip >> 16) & 0xffffU;
		tcam_key->key_info.ext_dip_l =
			rule->hinic_fdir.dst_ip & 0xffffU;
		tcam_key->key_mask.ext_dip_h =
			(rule->mask.dst_ipv4_mask >> 16) & 0xffffU;
		tcam_key->key_mask.ext_dip_l =
			rule->mask.dst_ipv4_mask & 0xffffU;
		break;

	case 0:
		break;

	default:
		PMD_DRV_LOG(ERR, "invalid src_ip mask.");
		return -EINVAL;
	}

	if (rule->mask.dst_port_mask > 0) {
		tcam_key->key_info.dst_port = rule->hinic_fdir.dst_port;
		tcam_key->key_mask.dst_port = rule->mask.dst_port_mask;
	}

	if (rule->mask.src_port_mask > 0) {
		tcam_key->key_info.src_port = rule->hinic_fdir.src_port;
		tcam_key->key_mask.src_port = rule->mask.src_port_mask;
	}

	switch (rule->mask.tunnel_flag) {
	case UINT16_MAX:
		tcam_key->key_info.tunnel_flag = FDIR_TCAM_TUNNEL_PACKET;
		tcam_key->key_mask.tunnel_flag = UINT8_MAX;
		break;

	case 0:
		tcam_key->key_info.tunnel_flag = FDIR_TCAM_NORMAL_PACKET;
		tcam_key->key_mask.tunnel_flag = 0;
		break;

	default:
		PMD_DRV_LOG(ERR, "invalid tunnel flag mask.");
		return -EINVAL;
	}

	if (rule->mask.tunnel_inner_dst_port_mask > 0) {
		tcam_key->key_info.dst_port =
					rule->hinic_fdir.tunnel_inner_dst_port;
		tcam_key->key_mask.dst_port =
					rule->mask.tunnel_inner_dst_port_mask;
	}

	if (rule->mask.tunnel_inner_src_port_mask > 0) {
		tcam_key->key_info.src_port =
					rule->hinic_fdir.tunnel_inner_src_port;
		tcam_key->key_mask.src_port =
					rule->mask.tunnel_inner_src_port_mask;
	}

	switch (rule->mask.proto_mask) {
	case UINT16_MAX:
		tcam_key->key_info.protocol = rule->hinic_fdir.proto;
		tcam_key->key_mask.protocol = UINT8_MAX;
		break;

	case 0:
		break;

	default:
		PMD_DRV_LOG(ERR, "invalid tunnel flag mask.");
		return -EINVAL;
	}

	tcam_key->key_mask.function_id = UINT16_MAX;
	tcam_key->key_info.function_id =
		hinic_global_func_id(nic_dev->hwdev) & 0x7fff;

	return 0;
}

static int hinic_fdir_tcam_ipv6_init(struct rte_eth_dev *dev,
				     struct hinic_fdir_rule *rule,
				     struct tag_tcam_key *tcam_key)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	switch (rule->mask.dst_ipv6_mask) {
	case UINT16_MAX:
		tcam_key->key_info_ipv6.ipv6_key0 =
			((rule->hinic_fdir.dst_ipv6[0] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[1];
		tcam_key->key_info_ipv6.ipv6_key1 =
			((rule->hinic_fdir.dst_ipv6[2] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[3];
		tcam_key->key_info_ipv6.ipv6_key2 =
			((rule->hinic_fdir.dst_ipv6[4] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[5];
		tcam_key->key_info_ipv6.ipv6_key3 =
			((rule->hinic_fdir.dst_ipv6[6] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[7];
		tcam_key->key_info_ipv6.ipv6_key4 =
			((rule->hinic_fdir.dst_ipv6[8] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[9];
		tcam_key->key_info_ipv6.ipv6_key5 =
			((rule->hinic_fdir.dst_ipv6[10] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[11];
		tcam_key->key_info_ipv6.ipv6_key6 =
			((rule->hinic_fdir.dst_ipv6[12] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[13];
		tcam_key->key_info_ipv6.ipv6_key7 =
			((rule->hinic_fdir.dst_ipv6[14] << 8) & 0xff00) |
			rule->hinic_fdir.dst_ipv6[15];
		tcam_key->key_mask_ipv6.ipv6_key0 = UINT16_MAX;
		tcam_key->key_mask_ipv6.ipv6_key1 = UINT16_MAX;
		tcam_key->key_mask_ipv6.ipv6_key2 = UINT16_MAX;
		tcam_key->key_mask_ipv6.ipv6_key3 = UINT16_MAX;
		tcam_key->key_mask_ipv6.ipv6_key4 = UINT16_MAX;
		tcam_key->key_mask_ipv6.ipv6_key5 = UINT16_MAX;
		tcam_key->key_mask_ipv6.ipv6_key6 = UINT16_MAX;
		tcam_key->key_mask_ipv6.ipv6_key7 = UINT16_MAX;
		break;

	case 0:
		break;

	default:
		PMD_DRV_LOG(ERR, "invalid dst_ipv6 mask");
		return -EINVAL;
	}

	if (rule->mask.dst_port_mask > 0) {
		tcam_key->key_info_ipv6.dst_port = rule->hinic_fdir.dst_port;
		tcam_key->key_mask_ipv6.dst_port = rule->mask.dst_port_mask;
	}

	switch (rule->mask.proto_mask) {
	case UINT16_MAX:
		tcam_key->key_info_ipv6.protocol =
			(rule->hinic_fdir.proto) & 0x7F;
		tcam_key->key_mask_ipv6.protocol = 0x7F;
		break;

	case 0:
		break;

	default:
		PMD_DRV_LOG(ERR, "invalid tunnel flag mask");
		return -EINVAL;
	}

	tcam_key->key_info_ipv6.ipv6_flag = 1;
	tcam_key->key_mask_ipv6.ipv6_flag = 1;

	tcam_key->key_mask_ipv6.function_id = UINT8_MAX;
	tcam_key->key_info_ipv6.function_id =
			(u8)hinic_global_func_id(nic_dev->hwdev);

	return 0;
}

static int hinic_fdir_tcam_info_init(struct rte_eth_dev *dev,
				     struct hinic_fdir_rule *rule,
				     struct tag_tcam_key *tcam_key,
				     struct tag_tcam_cfg_rule *fdir_tcam_rule)
{
	int ret = -1;

	if (rule->mask.dst_ipv4_mask == UINT32_MAX)
		ret = hinic_fdir_tcam_ipv4_init(dev, rule, tcam_key);
	else if (rule->mask.dst_ipv6_mask == UINT16_MAX)
		ret = hinic_fdir_tcam_ipv6_init(dev, rule, tcam_key);

	if (ret < 0)
		return ret;

	fdir_tcam_rule->data.qid = rule->queue;

	tcam_key_calculate(tcam_key, fdir_tcam_rule);

	return 0;
}

static inline struct hinic_tcam_filter *
hinic_tcam_filter_lookup(struct hinic_tcam_filter_list *filter_list,
			struct tag_tcam_key *key)
{
	struct hinic_tcam_filter *it;

	TAILQ_FOREACH(it, filter_list, entries) {
		if (memcmp(key, &it->tcam_key,
			sizeof(struct tag_tcam_key)) == 0) {
			return it;
		}
	}

	return NULL;
}

static int hinic_lookup_new_tcam_filter(struct rte_eth_dev *dev,
					struct hinic_tcam_info *tcam_info,
					struct hinic_tcam_filter *tcam_filter,
					u16 *tcam_index)
{
	int index;
	int max_index;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (hinic_func_type(nic_dev->hwdev) == TYPE_VF)
		max_index = HINIC_VF_MAX_TCAM_FILTERS;
	else
		max_index = HINIC_PF_MAX_TCAM_FILTERS;

	for (index = 0; index < max_index; index++) {
		if (tcam_info->tcam_index_array[index] == 0)
			break;
	}

	if (index == max_index) {
		PMD_DRV_LOG(ERR, "function 0x%x tcam filters only support %d filter rules",
			hinic_global_func_id(nic_dev->hwdev), max_index);
		return -EINVAL;
	}

	tcam_filter->index = index;
	*tcam_index = index;

	return 0;
}

static int hinic_add_tcam_filter(struct rte_eth_dev *dev,
				struct hinic_tcam_filter *tcam_filter,
				struct tag_tcam_cfg_rule *fdir_tcam_rule)
{
	struct hinic_tcam_info *tcam_info =
		HINIC_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u16 index = 0;
	u16 tcam_block_index = 0;
	int rc;

	if (hinic_lookup_new_tcam_filter(dev, tcam_info, tcam_filter, &index))
		return -EINVAL;

	if (tcam_info->tcam_rule_nums == 0) {
		if (hinic_func_type(nic_dev->hwdev) == TYPE_VF) {
			rc = hinic_alloc_tcam_block(nic_dev->hwdev,
				HINIC_TCAM_BLOCK_TYPE_VF, &tcam_block_index);
			if (rc != 0) {
				PMD_DRV_LOG(ERR, "VF fdir filter tcam alloc block failed!");
				return -EFAULT;
			}
		} else {
			rc = hinic_alloc_tcam_block(nic_dev->hwdev,
				HINIC_TCAM_BLOCK_TYPE_PF, &tcam_block_index);
			if (rc != 0) {
				PMD_DRV_LOG(ERR, "PF fdir filter tcam alloc block failed!");
				return -EFAULT;
			}
		}

		tcam_info->tcam_block_index = tcam_block_index;
	} else {
		tcam_block_index = tcam_info->tcam_block_index;
	}

	if (hinic_func_type(nic_dev->hwdev) == TYPE_VF) {
		fdir_tcam_rule->index =
			HINIC_PKT_VF_TCAM_INDEX_START(tcam_block_index) + index;
	} else {
		fdir_tcam_rule->index =
			tcam_block_index * HINIC_PF_MAX_TCAM_FILTERS + index;
	}

	rc = hinic_add_tcam_rule(nic_dev->hwdev, fdir_tcam_rule);
	if (rc != 0) {
		PMD_DRV_LOG(ERR, "Fdir_tcam_rule add failed!");
		return -EFAULT;
	}

	PMD_DRV_LOG(INFO, "Add fdir_tcam_rule function_id: 0x%x,"
		"tcam_block_id: %d, index: %d, queue: %d, tcam_rule_nums: %d succeed",
		hinic_global_func_id(nic_dev->hwdev), tcam_block_index,
		fdir_tcam_rule->index, fdir_tcam_rule->data.qid,
		tcam_info->tcam_rule_nums + 1);

	if (tcam_info->tcam_rule_nums == 0) {
		rc = hinic_set_fdir_filter(nic_dev->hwdev, 0, 0, 0, true);
		if (rc < 0) {
			(void)hinic_del_tcam_rule(nic_dev->hwdev,
						fdir_tcam_rule->index);
			return rc;
		}

		rc = hinic_set_fdir_tcam_rule_filter(nic_dev->hwdev, true);
		if (rc && rc != HINIC_MGMT_CMD_UNSUPPORTED) {
			/*
			 * hinic supports two methods: linear table and tcam
			 * table, if tcam filter enables failed but linear table
			 * is ok, which also needs to enable filter, so for this
			 * scene, driver should not close fdir switch.
			 */
			(void)hinic_del_tcam_rule(nic_dev->hwdev,
						fdir_tcam_rule->index);
			return rc;
		}
	}

	TAILQ_INSERT_TAIL(&tcam_info->tcam_list, tcam_filter, entries);

	tcam_info->tcam_index_array[index] = 1;
	tcam_info->tcam_rule_nums++;

	return 0;
}

static int hinic_del_tcam_filter(struct rte_eth_dev *dev,
				struct hinic_tcam_filter *tcam_filter)
{
	struct hinic_tcam_info *tcam_info =
		HINIC_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u32 index = 0;
	u16 tcam_block_index = tcam_info->tcam_block_index;
	int rc;
	u8 block_type = 0;

	if (hinic_func_type(nic_dev->hwdev) == TYPE_VF) {
		index = HINIC_PKT_VF_TCAM_INDEX_START(tcam_block_index) +
			tcam_filter->index;
		block_type = HINIC_TCAM_BLOCK_TYPE_VF;
	} else {
		index = tcam_block_index * HINIC_PF_MAX_TCAM_FILTERS +
			tcam_filter->index;
		block_type = HINIC_TCAM_BLOCK_TYPE_PF;
	}

	rc = hinic_del_tcam_rule(nic_dev->hwdev, index);
	if (rc != 0) {
		PMD_DRV_LOG(ERR, "fdir_tcam_rule del failed!");
		return -EFAULT;
	}

	PMD_DRV_LOG(INFO, "Del fdir_tcam_rule function_id: 0x%x, "
		"tcam_block_id: %d, index: %d, tcam_rule_nums: %d succeed",
		hinic_global_func_id(nic_dev->hwdev), tcam_block_index, index,
		tcam_info->tcam_rule_nums - 1);

	TAILQ_REMOVE(&tcam_info->tcam_list, tcam_filter, entries);

	tcam_info->tcam_index_array[tcam_filter->index] = 0;

	rte_free(tcam_filter);

	tcam_info->tcam_rule_nums--;

	if (tcam_info->tcam_rule_nums == 0) {
		(void)hinic_free_tcam_block(nic_dev->hwdev, block_type,
					&tcam_block_index);
	}

	return 0;
}

static int hinic_add_del_tcam_fdir_filter(struct rte_eth_dev *dev,
					struct hinic_fdir_rule *rule, bool add)
{
	struct hinic_tcam_info *tcam_info =
		HINIC_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic_tcam_filter *tcam_filter;
	struct tag_tcam_cfg_rule fdir_tcam_rule;
	struct tag_tcam_key tcam_key;
	int ret;

	memset(&fdir_tcam_rule, 0, sizeof(struct tag_tcam_cfg_rule));
	memset((void *)&tcam_key, 0, sizeof(struct tag_tcam_key));

	ret = hinic_fdir_tcam_info_init(dev, rule, &tcam_key, &fdir_tcam_rule);
	if (ret) {
		PMD_DRV_LOG(ERR, "Init hinic fdir info failed!");
		return ret;
	}

	tcam_filter = hinic_tcam_filter_lookup(&tcam_info->tcam_list,
						&tcam_key);
	if (tcam_filter != NULL && add) {
		PMD_DRV_LOG(ERR, "Filter exists.");
		return -EEXIST;
	}
	if (tcam_filter == NULL && !add) {
		PMD_DRV_LOG(ERR, "Filter doesn't exist.");
		return -ENOENT;
	}

	if (add) {
		tcam_filter = rte_zmalloc("hinic_5tuple_filter",
				sizeof(struct hinic_tcam_filter), 0);
		if (tcam_filter == NULL)
			return -ENOMEM;
		(void)rte_memcpy(&tcam_filter->tcam_key,
				 &tcam_key, sizeof(struct tag_tcam_key));
		tcam_filter->queue = fdir_tcam_rule.data.qid;

		ret = hinic_add_tcam_filter(dev, tcam_filter, &fdir_tcam_rule);
		if (ret < 0) {
			rte_free(tcam_filter);
			return ret;
		}

		rule->tcam_index = fdir_tcam_rule.index;

	} else {
		PMD_DRV_LOG(INFO, "begin to hinic_del_tcam_filter");
		ret = hinic_del_tcam_filter(dev, tcam_filter);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * Create or destroy a flow rule.
 * Theorically one rule can match more than one filters.
 * We will let it use the filter which it hitt first.
 * So, the sequence matters.
 */
static struct rte_flow *hinic_flow_create(struct rte_eth_dev *dev,
					const struct rte_flow_attr *attr,
					const struct rte_flow_item pattern[],
					const struct rte_flow_action actions[],
					struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_ntuple_filter ntuple_filter;
	struct rte_eth_ethertype_filter ethertype_filter;
	struct hinic_fdir_rule fdir_rule;
	struct rte_flow *flow = NULL;
	struct hinic_ethertype_filter_ele *ethertype_filter_ptr;
	struct hinic_ntuple_filter_ele *ntuple_filter_ptr;
	struct hinic_fdir_rule_ele *fdir_rule_ptr;
	struct hinic_flow_mem *hinic_flow_mem_ptr;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	flow = rte_zmalloc("hinic_rte_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		PMD_DRV_LOG(ERR, "Failed to allocate flow memory");
		return NULL;
	}

	hinic_flow_mem_ptr = rte_zmalloc("hinic_flow_mem",
			sizeof(struct hinic_flow_mem), 0);
	if (!hinic_flow_mem_ptr) {
		PMD_DRV_LOG(ERR, "Failed to allocate hinic_flow_mem_ptr");
		rte_free(flow);
		return NULL;
	}

	hinic_flow_mem_ptr->flow = flow;
	TAILQ_INSERT_TAIL(&nic_dev->hinic_flow_list, hinic_flow_mem_ptr,
				entries);

	/* Add ntuple filter */
	memset(&ntuple_filter, 0, sizeof(struct rte_eth_ntuple_filter));
	ret = hinic_parse_ntuple_filter(dev, attr, pattern,
			actions, &ntuple_filter, error);
	if (!ret) {
		ret = hinic_add_del_ntuple_filter(dev, &ntuple_filter, TRUE);
		if (!ret) {
			ntuple_filter_ptr = rte_zmalloc("hinic_ntuple_filter",
				sizeof(struct hinic_ntuple_filter_ele), 0);
			if (ntuple_filter_ptr == NULL) {
				PMD_DRV_LOG(ERR, "Failed to allocate ntuple_filter_ptr");
				(void)hinic_add_del_ntuple_filter(dev,
							&ntuple_filter, FALSE);
				goto out;
			}
			rte_memcpy(&ntuple_filter_ptr->filter_info,
				   &ntuple_filter,
				   sizeof(struct rte_eth_ntuple_filter));
			TAILQ_INSERT_TAIL(&nic_dev->filter_ntuple_list,
			ntuple_filter_ptr, entries);
			flow->rule = ntuple_filter_ptr;
			flow->filter_type = RTE_ETH_FILTER_NTUPLE;

			PMD_DRV_LOG(INFO, "Create flow ntuple succeed, func_id: 0x%x",
			hinic_global_func_id(nic_dev->hwdev));
			return flow;
		}
		goto out;
	}

	/* Add ethertype filter */
	memset(&ethertype_filter, 0, sizeof(struct rte_eth_ethertype_filter));
	ret = hinic_parse_ethertype_filter(dev, attr, pattern, actions,
					&ethertype_filter, error);
	if (!ret) {
		ret = hinic_add_del_ethertype_filter(dev, &ethertype_filter,
						     TRUE);
		if (!ret) {
			ethertype_filter_ptr =
				rte_zmalloc("hinic_ethertype_filter",
				sizeof(struct hinic_ethertype_filter_ele), 0);
			if (ethertype_filter_ptr == NULL) {
				PMD_DRV_LOG(ERR, "Failed to allocate ethertype_filter_ptr");
				(void)hinic_add_del_ethertype_filter(dev,
						&ethertype_filter, FALSE);
				goto out;
			}
			rte_memcpy(&ethertype_filter_ptr->filter_info,
				&ethertype_filter,
				sizeof(struct rte_eth_ethertype_filter));
			TAILQ_INSERT_TAIL(&nic_dev->filter_ethertype_list,
				ethertype_filter_ptr, entries);
			flow->rule = ethertype_filter_ptr;
			flow->filter_type = RTE_ETH_FILTER_ETHERTYPE;

			PMD_DRV_LOG(INFO, "Create flow ethertype succeed, func_id: 0x%x",
					hinic_global_func_id(nic_dev->hwdev));
			return flow;
		}
		goto out;
	}

	/* Add fdir filter */
	memset(&fdir_rule, 0, sizeof(struct hinic_fdir_rule));
	ret = hinic_parse_fdir_filter(dev, attr, pattern,
				      actions, &fdir_rule, error);
	if (!ret) {
		if (fdir_rule.mode == HINIC_FDIR_MODE_NORMAL) {
			ret = hinic_add_del_fdir_filter(dev, &fdir_rule, TRUE);
		} else if (fdir_rule.mode == HINIC_FDIR_MODE_TCAM) {
			ret = hinic_add_del_tcam_fdir_filter(dev, &fdir_rule,
							     TRUE);
		}  else {
			PMD_DRV_LOG(INFO, "flow fdir rule create failed, rule mode wrong");
			goto out;
		}
		if (!ret) {
			fdir_rule_ptr = rte_zmalloc("hinic_fdir_rule",
				sizeof(struct hinic_fdir_rule_ele), 0);
			if (fdir_rule_ptr == NULL) {
				PMD_DRV_LOG(ERR, "Failed to allocate fdir_rule_ptr");
				if (fdir_rule.mode == HINIC_FDIR_MODE_NORMAL)
					hinic_add_del_fdir_filter(dev,
						&fdir_rule, FALSE);
				else if (fdir_rule.mode == HINIC_FDIR_MODE_TCAM)
					hinic_add_del_tcam_fdir_filter(dev,
						&fdir_rule, FALSE);

				goto out;
			}
			rte_memcpy(&fdir_rule_ptr->filter_info, &fdir_rule,
				sizeof(struct hinic_fdir_rule));
			TAILQ_INSERT_TAIL(&nic_dev->filter_fdir_rule_list,
				fdir_rule_ptr, entries);
			flow->rule = fdir_rule_ptr;
			flow->filter_type = RTE_ETH_FILTER_FDIR;

			PMD_DRV_LOG(INFO, "Create flow fdir rule succeed, func_id : 0x%x",
					hinic_global_func_id(nic_dev->hwdev));
			return flow;
		}
		goto out;
	}

out:
	TAILQ_REMOVE(&nic_dev->hinic_flow_list, hinic_flow_mem_ptr, entries);
	rte_flow_error_set(error, -ret,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");
	rte_free(hinic_flow_mem_ptr);
	rte_free(flow);
	return NULL;
}

/* Destroy a flow rule on hinic. */
static int hinic_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
				struct rte_flow_error *error)
{
	int ret;
	struct rte_flow *pmd_flow = flow;
	enum rte_filter_type filter_type = pmd_flow->filter_type;
	struct rte_eth_ntuple_filter ntuple_filter;
	struct rte_eth_ethertype_filter ethertype_filter;
	struct hinic_fdir_rule fdir_rule;
	struct hinic_ntuple_filter_ele *ntuple_filter_ptr;
	struct hinic_ethertype_filter_ele *ethertype_filter_ptr;
	struct hinic_fdir_rule_ele *fdir_rule_ptr;
	struct hinic_flow_mem *hinic_flow_mem_ptr;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	switch (filter_type) {
	case RTE_ETH_FILTER_NTUPLE:
		ntuple_filter_ptr = (struct hinic_ntuple_filter_ele *)
					pmd_flow->rule;
		rte_memcpy(&ntuple_filter, &ntuple_filter_ptr->filter_info,
			sizeof(struct rte_eth_ntuple_filter));
		ret = hinic_add_del_ntuple_filter(dev, &ntuple_filter, FALSE);
		if (!ret) {
			TAILQ_REMOVE(&nic_dev->filter_ntuple_list,
				ntuple_filter_ptr, entries);
			rte_free(ntuple_filter_ptr);
		}
		break;
	case RTE_ETH_FILTER_ETHERTYPE:
		ethertype_filter_ptr = (struct hinic_ethertype_filter_ele *)
					pmd_flow->rule;
		rte_memcpy(&ethertype_filter,
			&ethertype_filter_ptr->filter_info,
			sizeof(struct rte_eth_ethertype_filter));
		ret = hinic_add_del_ethertype_filter(dev,
				&ethertype_filter, FALSE);
		if (!ret) {
			TAILQ_REMOVE(&nic_dev->filter_ethertype_list,
				ethertype_filter_ptr, entries);
			rte_free(ethertype_filter_ptr);
		}
		break;
	case RTE_ETH_FILTER_FDIR:
		fdir_rule_ptr = (struct hinic_fdir_rule_ele *)pmd_flow->rule;
		rte_memcpy(&fdir_rule,
			&fdir_rule_ptr->filter_info,
			sizeof(struct hinic_fdir_rule));
		if (fdir_rule.mode == HINIC_FDIR_MODE_NORMAL) {
			ret = hinic_add_del_fdir_filter(dev, &fdir_rule, FALSE);
		} else if (fdir_rule.mode == HINIC_FDIR_MODE_TCAM) {
			ret = hinic_add_del_tcam_fdir_filter(dev, &fdir_rule,
								FALSE);
		} else {
			PMD_DRV_LOG(ERR, "FDIR Filter type is wrong!");
			ret = -EINVAL;
		}
		if (!ret) {
			TAILQ_REMOVE(&nic_dev->filter_fdir_rule_list,
				fdir_rule_ptr, entries);
			rte_free(fdir_rule_ptr);
		}
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) is not supported",
			filter_type);
		ret = -EINVAL;
		break;
	}

	if (ret) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Failed to destroy flow");
		return ret;
	}

	TAILQ_FOREACH(hinic_flow_mem_ptr, &nic_dev->hinic_flow_list, entries) {
		if (hinic_flow_mem_ptr->flow == pmd_flow) {
			TAILQ_REMOVE(&nic_dev->hinic_flow_list,
				hinic_flow_mem_ptr, entries);
			rte_free(hinic_flow_mem_ptr);
			break;
		}
	}
	rte_free(flow);

	PMD_DRV_LOG(INFO, "Destroy flow succeed, func_id: 0x%x",
			hinic_global_func_id(nic_dev->hwdev));

	return ret;
}

/* Remove all the n-tuple filters */
static void hinic_clear_all_ntuple_filter(struct rte_eth_dev *dev)
{
	struct hinic_filter_info *filter_info =
		HINIC_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	struct hinic_5tuple_filter *p_5tuple;

	while ((p_5tuple = TAILQ_FIRST(&filter_info->fivetuple_list)))
		hinic_remove_5tuple_filter(dev, p_5tuple);
}

/* Remove all the ether type filters */
static void hinic_clear_all_ethertype_filter(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic_filter_info *filter_info =
		HINIC_DEV_PRIVATE_TO_FILTER_INFO(nic_dev);
	int ret = 0;

	if (filter_info->type_mask &
		(1 << HINIC_PKT_TYPE_FIND_ID(PKT_LACP_TYPE))) {
		hinic_ethertype_filter_remove(filter_info,
			HINIC_PKT_TYPE_FIND_ID(PKT_LACP_TYPE));
		ret = hinic_set_fdir_filter(nic_dev->hwdev, PKT_LACP_TYPE,
					filter_info->qid, false, true);

		(void)hinic_clear_fdir_tcam(nic_dev->hwdev, TCAM_PKT_LACP);
	}

	if (filter_info->type_mask &
		(1 << HINIC_PKT_TYPE_FIND_ID(PKT_ARP_TYPE))) {
		hinic_ethertype_filter_remove(filter_info,
			HINIC_PKT_TYPE_FIND_ID(PKT_ARP_TYPE));
		ret = hinic_set_fdir_filter(nic_dev->hwdev, PKT_ARP_TYPE,
			filter_info->qid, false, true);
	}

	if (ret)
		PMD_DRV_LOG(ERR, "Clear ethertype failed, filter type: 0x%x",
				filter_info->pkt_type);
}

/* Remove all the ether type filters */
static void hinic_clear_all_fdir_filter(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic_tcam_info *tcam_info =
		HINIC_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic_tcam_filter *tcam_filter_ptr;

	while ((tcam_filter_ptr = TAILQ_FIRST(&tcam_info->tcam_list)))
		(void)hinic_del_tcam_filter(dev, tcam_filter_ptr);

	(void)hinic_set_fdir_filter(nic_dev->hwdev, 0, 0, 0, false);

	(void)hinic_set_fdir_tcam_rule_filter(nic_dev->hwdev, false);

	(void)hinic_flush_tcam_rule(nic_dev->hwdev);
}

static void hinic_filterlist_flush(struct rte_eth_dev *dev)
{
	struct hinic_ntuple_filter_ele *ntuple_filter_ptr;
	struct hinic_ethertype_filter_ele *ethertype_filter_ptr;
	struct hinic_fdir_rule_ele *fdir_rule_ptr;
	struct hinic_flow_mem *hinic_flow_mem_ptr;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	while ((ntuple_filter_ptr =
			TAILQ_FIRST(&nic_dev->filter_ntuple_list))) {
		TAILQ_REMOVE(&nic_dev->filter_ntuple_list, ntuple_filter_ptr,
				 entries);
		rte_free(ntuple_filter_ptr);
	}

	while ((ethertype_filter_ptr =
			TAILQ_FIRST(&nic_dev->filter_ethertype_list))) {
		TAILQ_REMOVE(&nic_dev->filter_ethertype_list,
				ethertype_filter_ptr,
				entries);
		rte_free(ethertype_filter_ptr);
	}

	while ((fdir_rule_ptr =
			TAILQ_FIRST(&nic_dev->filter_fdir_rule_list))) {
		TAILQ_REMOVE(&nic_dev->filter_fdir_rule_list, fdir_rule_ptr,
				 entries);
		rte_free(fdir_rule_ptr);
	}

	while ((hinic_flow_mem_ptr =
			TAILQ_FIRST(&nic_dev->hinic_flow_list))) {
		TAILQ_REMOVE(&nic_dev->hinic_flow_list, hinic_flow_mem_ptr,
				 entries);
		rte_free(hinic_flow_mem_ptr->flow);
		rte_free(hinic_flow_mem_ptr);
	}
}

/* Destroy all flow rules associated with a port on hinic. */
static int hinic_flow_flush(struct rte_eth_dev *dev,
				__rte_unused struct rte_flow_error *error)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	hinic_clear_all_ntuple_filter(dev);
	hinic_clear_all_ethertype_filter(dev);
	hinic_clear_all_fdir_filter(dev);
	hinic_filterlist_flush(dev);

	PMD_DRV_LOG(INFO, "Flush flow succeed, func_id: 0x%x",
			hinic_global_func_id(nic_dev->hwdev));
	return 0;
}

void hinic_destroy_fdir_filter(struct rte_eth_dev *dev)
{
	hinic_clear_all_ntuple_filter(dev);
	hinic_clear_all_ethertype_filter(dev);
	hinic_clear_all_fdir_filter(dev);
	hinic_filterlist_flush(dev);
}

const struct rte_flow_ops hinic_flow_ops = {
	.validate = hinic_flow_validate,
	.create = hinic_flow_create,
	.destroy = hinic_flow_destroy,
	.flush = hinic_flow_flush,
};

