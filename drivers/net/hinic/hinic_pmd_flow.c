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

#ifndef UINT8_MAX
#define UINT8_MAX          (u8)(~((u8)0))	/* 0xFF               */
#define UINT16_MAX         (u16)(~((u16)0))	/* 0xFFFF             */
#define UINT32_MAX         (u32)(~((u32)0))	/* 0xFFFFFFFF         */
#define UINT64_MAX         (u64)(~((u64)0))	/* 0xFFFFFFFFFFFFFFFF */
#define ASCII_MAX          (0x7F)
#endif

#define HINIC_MIN_N_TUPLE_PRIO		1
#define HINIC_MAX_N_TUPLE_PRIO		7

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
static int
cons_parse_ethertype_filter(const struct rte_flow_attr *attr,
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

static int
hinic_parse_ethertype_filter(struct rte_eth_dev *dev,
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
 * Please aware there's an asumption for all the parsers.
 * rte_flow_item is using big endian, rte_flow_attr and
 * rte_flow_action are using CPU order.
 * Because the pattern is used to describe the packets,
 * normally the packets should use network order.
 */
static int
cons_parse_ntuple_filter(const struct rte_flow_attr *attr,
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

static int
hinic_parse_ntuple_filter(struct rte_eth_dev *dev,
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
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
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
	const struct rte_flow_item *item = *in_out_item;

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
		    item->type != RTE_FLOW_ITEM_TYPE_END) {
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by fdir filter, support tcp, udp, end");
			return -rte_errno;
		}
	}

	*in_out_item = item;
	return 0;
}

static int hinic_normal_item_check_l4(const struct rte_flow_item **in_out_item,
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
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		/* Get TCP/UDP info */
		if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
			/*
			 * Only care about src & dst ports,
			 * others should be masked.
			 */
			if (!item->mask) {
				memset(rule, 0, sizeof(struct hinic_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Not supported by fdir filter,support src,dst ports");
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
				memset(rule, 0, sizeof(struct hinic_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter,support tcp");
				return -rte_errno;
			}

			rule->mask.src_port_mask = tcp_mask->hdr.src_port;
			rule->mask.dst_port_mask = tcp_mask->hdr.dst_port;

			if (item->spec) {
				tcp_spec =
					(const struct rte_flow_item_tcp *)
					item->spec;
				rule->hinic_fdir.src_port =
					tcp_spec->hdr.src_port;
				rule->hinic_fdir.dst_port =
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
					item, "Not supported by fdir filter,support src,dst ports");
				return -rte_errno;
			}

			udp_mask = (const struct rte_flow_item_udp *)item->mask;
			if (udp_mask->hdr.dgram_len ||
			    udp_mask->hdr.dgram_cksum) {
				memset(rule, 0, sizeof(struct hinic_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter,support udp");
				return -rte_errno;
			}
			rule->mask.src_port_mask = udp_mask->hdr.src_port;
			rule->mask.dst_port_mask = udp_mask->hdr.dst_port;

			if (item->spec) {
				udp_spec =
					(const struct rte_flow_item_udp *)
					item->spec;
				rule->hinic_fdir.src_port =
					udp_spec->hdr.src_port;
				rule->hinic_fdir.dst_port =
					udp_spec->hdr.dst_port;
			}
		} else {
			memset(rule, 0, sizeof(struct hinic_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter,support tcp/udp");
			return -rte_errno;
		}

		/* Get next no void item */
		*in_out_item = next_no_void_pattern(pattern, item);
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
			item, "Not supported by fdir filter,support end");
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

static int
hinic_parse_fdir_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct hinic_fdir_rule *rule,
			struct rte_flow_error *error)
{
	int ret;

	ret = hinic_parse_fdir_filter_normal(attr, pattern,
						actions, rule, error);
	if (ret)
		return ret;

	if (rule->queue >= dev->data->nb_rx_queues)
		return -ENOTSUP;

	return ret;
}

/**
 * Check if the flow rule is supported by nic.
 * It only checkes the format. Don't guarantee the rule can be programmed into
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

const struct rte_flow_ops hinic_flow_ops = {
	.validate = hinic_flow_validate,
};
