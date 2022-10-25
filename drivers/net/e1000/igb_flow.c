/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <dev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>

#include "e1000_logs.h"
#include "base/e1000_api.h"
#include "e1000_ethdev.h"

#define NEXT_ITEM_OF_PATTERN(item, pattern, index)		\
	do {							\
		item = (pattern) + (index);			\
		while (item->type == RTE_FLOW_ITEM_TYPE_VOID) {	\
		(index)++;					\
		item = (pattern) + (index);			\
		}						\
	} while (0)

#define NEXT_ITEM_OF_ACTION(act, actions, index)		\
	do {							\
		act = (actions) + (index);			\
		while (act->type == RTE_FLOW_ACTION_TYPE_VOID) {\
		(index)++;					\
		act = (actions) + (index);			\
		}						\
	} while (0)

#define	IGB_FLEX_RAW_NUM	12

struct igb_flow_mem_list igb_flow_list;
struct igb_ntuple_filter_list igb_filter_ntuple_list;
struct igb_ethertype_filter_list igb_filter_ethertype_list;
struct igb_syn_filter_list igb_filter_syn_list;
struct igb_flex_filter_list igb_filter_flex_list;
struct igb_rss_filter_list igb_filter_rss_list;

/**
 * Please be aware there's an assumption for all the parsers.
 * rte_flow_item is using big endian, rte_flow_attr and
 * rte_flow_action are using CPU order.
 * Because the pattern is used to describe the packets,
 * normally the packets should use network order.
 */

/**
 * Parse the rule to see if it is a n-tuple rule.
 * And get the n-tuple filter info BTW.
 * pattern:
 * The first not void item can be ETH or IPV4.
 * The second not void item must be IPV4 if the first one is ETH.
 * The third not void item must be UDP or TCP or SCTP
 * The next not void item must be END.
 * action:
 * The first not void action should be QUEUE.
 * The next not void action should be END.
 * pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4		src_addr 192.168.1.20	0xFFFFFFFF
 *			dst_addr 192.167.3.50	0xFFFFFFFF
 *			next_proto_id	17	0xFF
 * UDP/TCP/	src_port	80	0xFFFF
 * SCTP		dst_port	80	0xFFFF
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 */
static int
cons_parse_ntuple_filter(const struct rte_flow_attr *attr,
			 const struct rte_flow_item pattern[],
			 const struct rte_flow_action actions[],
			 struct rte_eth_ntuple_filter *filter,
			 struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_action *act;
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv4 *ipv4_mask;
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;
	const struct rte_flow_item_udp *udp_spec;
	const struct rte_flow_item_udp *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec;
	const struct rte_flow_item_sctp *sctp_mask;
	uint32_t index;

	if (!pattern) {
		rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
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

	/* parse pattern */
	index = 0;

	/* the first not void item can be MAC or IPv4 */
	NEXT_ITEM_OF_PATTERN(item, pattern, index);

	if (item->type != RTE_FLOW_ITEM_TYPE_ETH &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}
	/* Skip Ethernet */
	if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
		/*Not supported last point for range*/
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
		index++;
		NEXT_ITEM_OF_PATTERN(item, pattern, index);
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
			rte_flow_error_set(error,
			  EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			  item, "Not supported by ntuple filter");
			return -rte_errno;
		}
	}

	/* get the IPv4 info */
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

	ipv4_mask = item->mask;
	/**
	 * Only support src & dst addresses, protocol,
	 * others should be masked.
	 */

	if (ipv4_mask->hdr.version_ihl ||
		ipv4_mask->hdr.type_of_service ||
		ipv4_mask->hdr.total_length ||
		ipv4_mask->hdr.packet_id ||
		ipv4_mask->hdr.fragment_offset ||
		ipv4_mask->hdr.time_to_live ||
		ipv4_mask->hdr.hdr_checksum) {
		rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

	filter->dst_ip_mask = ipv4_mask->hdr.dst_addr;
	filter->src_ip_mask = ipv4_mask->hdr.src_addr;
	filter->proto_mask  = ipv4_mask->hdr.next_proto_id;

	ipv4_spec = item->spec;
	filter->dst_ip = ipv4_spec->hdr.dst_addr;
	filter->src_ip = ipv4_spec->hdr.src_addr;
	filter->proto  = ipv4_spec->hdr.next_proto_id;

	/* check if the next not void item is TCP or UDP or SCTP */
	index++;
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
	    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
	    item->type != RTE_FLOW_ITEM_TYPE_SCTP) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

	/* Not supported last point for range */
	if (item->last) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	/* get the TCP/UDP/SCTP info */
	if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
		if (item->spec && item->mask) {
			tcp_mask = item->mask;

			/**
			 * Only support src & dst ports, tcp flags,
			 * others should be masked.
			 */
			if (tcp_mask->hdr.sent_seq ||
				tcp_mask->hdr.recv_ack ||
				tcp_mask->hdr.data_off ||
				tcp_mask->hdr.rx_win ||
				tcp_mask->hdr.cksum ||
				tcp_mask->hdr.tcp_urp) {
				memset(filter, 0,
					sizeof(struct rte_eth_ntuple_filter));
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
				memset(filter, 0,
					sizeof(struct rte_eth_ntuple_filter));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by ntuple filter");
				return -rte_errno;
			}

			tcp_spec = item->spec;
			filter->dst_port  = tcp_spec->hdr.dst_port;
			filter->src_port  = tcp_spec->hdr.src_port;
			filter->tcp_flags = tcp_spec->hdr.tcp_flags;
		}
	} else if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
		if (item->spec && item->mask) {
			udp_mask = item->mask;

			/**
			 * Only support src & dst ports,
			 * others should be masked.
			 */
			if (udp_mask->hdr.dgram_len ||
			    udp_mask->hdr.dgram_cksum) {
				memset(filter, 0,
					sizeof(struct rte_eth_ntuple_filter));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by ntuple filter");
				return -rte_errno;
			}

			filter->dst_port_mask = udp_mask->hdr.dst_port;
			filter->src_port_mask = udp_mask->hdr.src_port;

			udp_spec = item->spec;
			filter->dst_port = udp_spec->hdr.dst_port;
			filter->src_port = udp_spec->hdr.src_port;
		}
	} else {
		if (item->spec && item->mask) {
			sctp_mask = item->mask;

			/**
			 * Only support src & dst ports,
			 * others should be masked.
			 */
			if (sctp_mask->hdr.tag ||
			    sctp_mask->hdr.cksum) {
				memset(filter, 0,
					sizeof(struct rte_eth_ntuple_filter));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by ntuple filter");
				return -rte_errno;
			}

			filter->dst_port_mask = sctp_mask->hdr.dst_port;
			filter->src_port_mask = sctp_mask->hdr.src_port;

			sctp_spec = (const struct rte_flow_item_sctp *)
					item->spec;
			filter->dst_port = sctp_spec->hdr.dst_port;
			filter->src_port = sctp_spec->hdr.src_port;
		}
	}
	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

	/* parse action */
	index = 0;

	/**
	 * n-tuple only supports forwarding,
	 * check if the first not void action is QUEUE.
	 */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}
	filter->queue =
		((const struct rte_flow_action_queue *)act->conf)->index;

	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->egress) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->transfer) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
				   attr, "No support for transfer.");
		return -rte_errno;
	}

	if (attr->priority > 0xFFFF) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Error priority.");
		return -rte_errno;
	}
	filter->priority = (uint16_t)attr->priority;

	return 0;
}

/* a specific function for igb because the flags is specific */
static int
igb_parse_ntuple_filter(struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_item pattern[],
			  const struct rte_flow_action actions[],
			  struct rte_eth_ntuple_filter *filter,
			  struct rte_flow_error *error)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	MAC_TYPE_FILTER_SUP(hw->mac.type);

	ret = cons_parse_ntuple_filter(attr, pattern, actions, filter, error);

	if (ret)
		return ret;

	/* Igb doesn't support many priorities. */
	if (filter->priority > E1000_2TUPLE_MAX_PRI) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "Priority not supported by ntuple filter");
		return -rte_errno;
	}

	if (hw->mac.type == e1000_82576) {
		if (filter->queue >= IGB_MAX_RX_QUEUE_NUM_82576) {
			memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "queue number not "
				"supported by ntuple filter");
			return -rte_errno;
		}
		filter->flags |= RTE_5TUPLE_FLAGS;
	} else {
		if (filter->src_ip_mask || filter->dst_ip_mask ||
			filter->src_port_mask) {
			memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "only two tuple are "
				"supported by this filter");
			return -rte_errno;
		}
		if (filter->queue >= IGB_MAX_RX_QUEUE_NUM) {
			memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "queue number not "
				"supported by ntuple filter");
			return -rte_errno;
		}
		filter->flags |= RTE_2TUPLE_FLAGS;
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
	const struct rte_flow_action *act;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	const struct rte_flow_action_queue *act_q;
	uint32_t index;

	if (!pattern) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_NUM,
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

	/* Parse pattern */
	index = 0;

	/* The first non-void item should be MAC. */
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ethertype filter");
		return -rte_errno;
	}

	/*Not supported last point for range*/
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

	eth_spec = item->spec;
	eth_mask = item->mask;

	/* Mask bits of source MAC address must be full of 0.
	 * Mask bits of destination MAC address must be full
	 * of 1 or full of 0.
	 */
	if (!rte_is_zero_ether_addr(&eth_mask->hdr.src_addr) ||
	    (!rte_is_zero_ether_addr(&eth_mask->hdr.dst_addr) &&
	     !rte_is_broadcast_ether_addr(&eth_mask->hdr.dst_addr))) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid ether address mask");
		return -rte_errno;
	}

	if ((eth_mask->hdr.ether_type & UINT16_MAX) != UINT16_MAX) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid ethertype mask");
		return -rte_errno;
	}

	/* If mask bits of destination MAC address
	 * are full of 1, set RTE_ETHTYPE_FLAGS_MAC.
	 */
	if (rte_is_broadcast_ether_addr(&eth_mask->hdr.dst_addr)) {
		filter->mac_addr = eth_spec->hdr.dst_addr;
		filter->flags |= RTE_ETHTYPE_FLAGS_MAC;
	} else {
		filter->flags &= ~RTE_ETHTYPE_FLAGS_MAC;
	}
	filter->ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);

	/* Check if the next non-void item is END. */
	index++;
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ethertype filter.");
		return -rte_errno;
	}

	/* Parse action */

	index = 0;
	/* Check if the first non-void action is QUEUE or DROP. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
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
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	/* Parse attr */
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
	if (attr->transfer) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
				attr, "No support for transfer.");
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

static int
igb_parse_ethertype_filter(struct rte_eth_dev *dev,
				 const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct rte_eth_ethertype_filter *filter,
			     struct rte_flow_error *error)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	MAC_TYPE_FILTER_SUP(hw->mac.type);

	ret = cons_parse_ethertype_filter(attr, pattern,
					actions, filter, error);

	if (ret)
		return ret;

	if (hw->mac.type == e1000_82576) {
		if (filter->queue >= IGB_MAX_RX_QUEUE_NUM_82576) {
			memset(filter, 0, sizeof(
					struct rte_eth_ethertype_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "queue number not supported "
					"by ethertype filter");
			return -rte_errno;
		}
	} else {
		if (filter->queue >= IGB_MAX_RX_QUEUE_NUM) {
			memset(filter, 0, sizeof(
					struct rte_eth_ethertype_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "queue number not supported "
					"by ethertype filter");
			return -rte_errno;
		}
	}

	if (filter->ether_type == RTE_ETHER_TYPE_IPV4 ||
		filter->ether_type == RTE_ETHER_TYPE_IPV6) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "IPv4/IPv6 not supported by ethertype filter");
		return -rte_errno;
	}

	if (filter->flags & RTE_ETHTYPE_FLAGS_MAC) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "mac compare is unsupported");
		return -rte_errno;
	}

	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "drop option is unsupported");
		return -rte_errno;
	}

	return 0;
}

/**
 * Parse the rule to see if it is a TCP SYN rule.
 * And get the TCP SYN filter info BTW.
 * pattern:
 * The first not void item must be ETH.
 * The second not void item must be IPV4 or IPV6.
 * The third not void item must be TCP.
 * The next not void item must be END.
 * action:
 * The first not void action should be QUEUE.
 * The next not void action should be END.
 * pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4/IPV6	NULL			NULL
 * TCP		tcp_flags	0x02	0xFF
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 */
static int
cons_parse_syn_filter(const struct rte_flow_attr *attr,
				const struct rte_flow_item pattern[],
				const struct rte_flow_action actions[],
				struct rte_eth_syn_filter *filter,
				struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_action *act;
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;
	const struct rte_flow_action_queue *act_q;
	uint32_t index;

	if (!pattern) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_NUM,
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

	/* parse pattern */
	index = 0;

	/* the first not void item should be MAC or IPv4 or IPv6 or TCP */
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV6 &&
	    item->type != RTE_FLOW_ITEM_TYPE_TCP) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by syn filter");
		return -rte_errno;
	}
		/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	/* Skip Ethernet */
	if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
		/* if the item is MAC, the content should be NULL */
		if (item->spec || item->mask) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid SYN address mask");
			return -rte_errno;
		}

		/* check if the next not void item is IPv4 or IPv6 */
		index++;
		NEXT_ITEM_OF_PATTERN(item, pattern, index);
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
		    item->type != RTE_FLOW_ITEM_TYPE_IPV6) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by syn filter");
			return -rte_errno;
		}
	}

	/* Skip IP */
	if (item->type == RTE_FLOW_ITEM_TYPE_IPV4 ||
	    item->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		/* if the item is IP, the content should be NULL */
		if (item->spec || item->mask) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid SYN mask");
			return -rte_errno;
		}

		/* check if the next not void item is TCP */
		index++;
		NEXT_ITEM_OF_PATTERN(item, pattern, index);
		if (item->type != RTE_FLOW_ITEM_TYPE_TCP) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by syn filter");
			return -rte_errno;
		}
	}

	/* Get the TCP info. Only support SYN. */
	if (!item->spec || !item->mask) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid SYN mask");
		return -rte_errno;
	}
	/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	tcp_spec = item->spec;
	tcp_mask = item->mask;
	if (!(tcp_spec->hdr.tcp_flags & RTE_TCP_SYN_FLAG) ||
	    tcp_mask->hdr.src_port ||
	    tcp_mask->hdr.dst_port ||
	    tcp_mask->hdr.sent_seq ||
	    tcp_mask->hdr.recv_ack ||
	    tcp_mask->hdr.data_off ||
	    tcp_mask->hdr.tcp_flags != RTE_TCP_SYN_FLAG ||
	    tcp_mask->hdr.rx_win ||
	    tcp_mask->hdr.cksum ||
	    tcp_mask->hdr.tcp_urp) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by syn filter");
		return -rte_errno;
	}

	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by syn filter");
		return -rte_errno;
	}

	/* parse action */
	index = 0;

	/* check if the first not void action is QUEUE. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	act_q = (const struct rte_flow_action_queue *)act->conf;
	filter->queue = act_q->index;

	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
			attr, "Only support ingress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->egress) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
			attr, "Not support egress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->transfer) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
			attr, "No support for transfer.");
		return -rte_errno;
	}

	/* Support 2 priorities, the lowest or highest. */
	if (!attr->priority) {
		filter->hig_pri = 0;
	} else if (attr->priority == (uint32_t)~0U) {
		filter->hig_pri = 1;
	} else {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			attr, "Not support priority.");
		return -rte_errno;
	}

	return 0;
}

static int
igb_parse_syn_filter(struct rte_eth_dev *dev,
				 const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct rte_eth_syn_filter *filter,
			     struct rte_flow_error *error)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	MAC_TYPE_FILTER_SUP(hw->mac.type);

	ret = cons_parse_syn_filter(attr, pattern,
					actions, filter, error);

	if (hw->mac.type == e1000_82576) {
		if (filter->queue >= IGB_MAX_RX_QUEUE_NUM_82576) {
			memset(filter, 0, sizeof(struct rte_eth_syn_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "queue number not "
					"supported by syn filter");
			return -rte_errno;
		}
	} else {
		if (filter->queue >= IGB_MAX_RX_QUEUE_NUM) {
			memset(filter, 0, sizeof(struct rte_eth_syn_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "queue number not "
					"supported by syn filter");
			return -rte_errno;
		}
	}

	if (ret)
		return ret;

	return 0;
}

/**
 * Parse the rule to see if it is a flex byte rule.
 * And get the flex byte filter info BTW.
 * pattern:
 * The first not void item must be RAW.
 * The second not void item can be RAW or END.
 * The third not void item can be RAW or END.
 * The last not void item must be END.
 * action:
 * The first not void action should be QUEUE.
 * The next not void action should be END.
 * pattern example:
 * ITEM		Spec			Mask
 * RAW		relative	0		0x1
 *			offset	0		0xFFFFFFFF
 *			pattern	{0x08, 0x06}		{0xFF, 0xFF}
 * RAW		relative	1		0x1
 *			offset	100		0xFFFFFFFF
 *			pattern	{0x11, 0x22, 0x33}	{0xFF, 0xFF, 0xFF}
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 */
static int
cons_parse_flex_filter(const struct rte_flow_attr *attr,
				const struct rte_flow_item pattern[],
				const struct rte_flow_action actions[],
				struct igb_flex_filter *filter,
				struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_action *act;
	const struct rte_flow_item_raw *raw_spec;
	const struct rte_flow_item_raw *raw_mask;
	const struct rte_flow_action_queue *act_q;
	uint32_t index, i, offset, total_offset;
	uint32_t max_offset = 0;
	int32_t shift, j, raw_index = 0;
	int32_t relative[IGB_FLEX_RAW_NUM] = {0};
	int32_t	raw_offset[IGB_FLEX_RAW_NUM] = {0};

	if (!pattern) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_NUM,
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

	/* parse pattern */
	index = 0;

item_loop:

	/* the first not void item should be RAW */
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_RAW) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by flex filter");
		return -rte_errno;
	}
		/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	raw_spec = item->spec;
	raw_mask = item->mask;

	if (!raw_mask->length ||
	    !raw_mask->relative) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by flex filter");
		return -rte_errno;
	}

	if (raw_mask->offset)
		offset = raw_spec->offset;
	else
		offset = 0;

	for (j = 0; j < raw_spec->length; j++) {
		if (raw_mask->pattern[j] != 0xFF) {
			memset(filter, 0, sizeof(struct igb_flex_filter));
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by flex filter");
			return -rte_errno;
		}
	}

	total_offset = 0;

	if (raw_spec->relative) {
		for (j = raw_index; j > 0; j--) {
			total_offset += raw_offset[j - 1];
			if (!relative[j - 1])
				break;
		}
		if (total_offset + raw_spec->length + offset > max_offset)
			max_offset = total_offset + raw_spec->length + offset;
	} else {
		if (raw_spec->length + offset > max_offset)
			max_offset = raw_spec->length + offset;
	}

	if ((raw_spec->length + offset + total_offset) >
			IGB_FLEX_FILTER_MAXLEN) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by flex filter");
		return -rte_errno;
	}

	if (raw_spec->relative == 0) {
		for (j = 0; j < raw_spec->length; j++)
			filter->bytes[offset + j] =
			raw_spec->pattern[j];
		j = offset / CHAR_BIT;
		shift = offset % CHAR_BIT;
	} else {
		for (j = 0; j < raw_spec->length; j++)
			filter->bytes[total_offset + offset + j] =
				raw_spec->pattern[j];
		j = (total_offset + offset) / CHAR_BIT;
		shift = (total_offset + offset) % CHAR_BIT;
	}

	i = 0;

	for ( ; shift < CHAR_BIT; shift++) {
		filter->mask[j] |= (0x80 >> shift);
		i++;
		if (i == raw_spec->length)
			break;
		if (shift == (CHAR_BIT - 1)) {
			j++;
			shift = -1;
		}
	}

	relative[raw_index] = raw_spec->relative;
	raw_offset[raw_index] = offset + raw_spec->length;
	raw_index++;

	/* check if the next not void item is RAW */
	index++;
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_RAW &&
		item->type != RTE_FLOW_ITEM_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by flex filter");
		return -rte_errno;
	}

	/* go back to parser */
	if (item->type == RTE_FLOW_ITEM_TYPE_RAW) {
		/* if the item is RAW, the content should be parse */
		goto item_loop;
	}

	filter->len = RTE_ALIGN(max_offset, 8);

	/* parse action */
	index = 0;

	/* check if the first not void action is QUEUE. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	act_q = (const struct rte_flow_action_queue *)act->conf;
	filter->queue = act_q->index;

	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
			attr, "Only support ingress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->egress) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
			attr, "Not support egress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->transfer) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
			attr, "No support for transfer.");
		return -rte_errno;
	}

	if (attr->priority > 0xFFFF) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Error priority.");
		return -rte_errno;
	}

	filter->priority = (uint16_t)attr->priority;

	return 0;
}

static int
igb_parse_flex_filter(struct rte_eth_dev *dev,
				 const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct igb_flex_filter *filter,
			     struct rte_flow_error *error)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	MAC_TYPE_FILTER_SUP_EXT(hw->mac.type);

	ret = cons_parse_flex_filter(attr, pattern,
					actions, filter, error);

	if (filter->queue >= IGB_MAX_RX_QUEUE_NUM) {
		memset(filter, 0, sizeof(struct igb_flex_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "queue number not supported by flex filter");
		return -rte_errno;
	}

	if (filter->len == 0 || filter->len > E1000_MAX_FLEX_FILTER_LEN ||
		filter->len % sizeof(uint64_t) != 0) {
		PMD_DRV_LOG(ERR, "filter's length is out of range");
		return -EINVAL;
	}

	if (filter->priority > E1000_MAX_FLEX_FILTER_PRI) {
		PMD_DRV_LOG(ERR, "filter's priority is out of range");
		return -EINVAL;
	}

	if (ret)
		return ret;

	return 0;
}

static int
igb_parse_rss_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_action actions[],
			struct igb_rte_flow_rss_conf *rss_conf,
			struct rte_flow_error *error)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	const struct rte_flow_action *act;
	const struct rte_flow_action_rss *rss;
	uint16_t n, index;

	/**
	 * rss only supports forwarding,
	 * check if the first not void action is RSS.
	 */
	index = 0;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_RSS) {
		memset(rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	rss = (const struct rte_flow_action_rss *)act->conf;

	if (!rss || !rss->queue_num) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act,
			   "no valid queues");
		return -rte_errno;
	}

	for (n = 0; n < rss->queue_num; n++) {
		if (rss->queue[n] >= dev->data->nb_rx_queues) {
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "queue id > max number of queues");
			return -rte_errno;
		}
	}

	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "non-default RSS hash functions are not supported");
	if (rss->level)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "a nonzero RSS encapsulation level is not supported");
	if (rss->key_len && rss->key_len != RTE_DIM(rss_conf->key))
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "RSS hash key must be exactly 40 bytes");
	if (((hw->mac.type == e1000_82576) &&
	     (rss->queue_num > IGB_MAX_RX_QUEUE_NUM_82576)) ||
	    ((hw->mac.type != e1000_82576) &&
	     (rss->queue_num > IGB_MAX_RX_QUEUE_NUM)))
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "too many queues for RSS context");
	if (igb_rss_conf_init(dev, rss_conf, rss))
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "RSS context initialization failure");

	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->egress) {
		memset(rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->transfer) {
		memset(rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
				   attr, "No support for transfer.");
		return -rte_errno;
	}

	if (attr->priority > 0xFFFF) {
		memset(rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Error priority.");
		return -rte_errno;
	}

	return 0;
}

/**
 * Create a flow rule.
 * Theorically one rule can match more than one filters.
 * We will let it use the filter which it hitt first.
 * So, the sequence matters.
 */
static struct rte_flow *
igb_flow_create(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_ntuple_filter ntuple_filter;
	struct rte_eth_ethertype_filter ethertype_filter;
	struct rte_eth_syn_filter syn_filter;
	struct igb_flex_filter flex_filter;
	struct igb_rte_flow_rss_conf rss_conf;
	struct rte_flow *flow = NULL;
	struct igb_ntuple_filter_ele *ntuple_filter_ptr;
	struct igb_ethertype_filter_ele *ethertype_filter_ptr;
	struct igb_eth_syn_filter_ele *syn_filter_ptr;
	struct igb_flex_filter_ele *flex_filter_ptr;
	struct igb_rss_conf_ele *rss_filter_ptr;
	struct igb_flow_mem *igb_flow_mem_ptr;

	flow = rte_zmalloc("igb_rte_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return (struct rte_flow *)flow;
	}
	igb_flow_mem_ptr = rte_zmalloc("igb_flow_mem",
			sizeof(struct igb_flow_mem), 0);
	if (!igb_flow_mem_ptr) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		rte_free(flow);
		return NULL;
	}
	igb_flow_mem_ptr->flow = flow;
	igb_flow_mem_ptr->dev = dev;
	TAILQ_INSERT_TAIL(&igb_flow_list,
				igb_flow_mem_ptr, entries);

	memset(&ntuple_filter, 0, sizeof(struct rte_eth_ntuple_filter));
	ret = igb_parse_ntuple_filter(dev, attr, pattern,
			actions, &ntuple_filter, error);
	if (!ret) {
		ret = igb_add_del_ntuple_filter(dev, &ntuple_filter, TRUE);
		if (!ret) {
			ntuple_filter_ptr = rte_zmalloc("igb_ntuple_filter",
				sizeof(struct igb_ntuple_filter_ele), 0);
			if (!ntuple_filter_ptr) {
				PMD_DRV_LOG(ERR, "failed to allocate memory");
				goto out;
			}

			rte_memcpy(&ntuple_filter_ptr->filter_info,
				&ntuple_filter,
				sizeof(struct rte_eth_ntuple_filter));
			TAILQ_INSERT_TAIL(&igb_filter_ntuple_list,
				ntuple_filter_ptr, entries);
			flow->rule = ntuple_filter_ptr;
			flow->filter_type = RTE_ETH_FILTER_NTUPLE;
			return flow;
		}
		goto out;
	}

	memset(&ethertype_filter, 0, sizeof(struct rte_eth_ethertype_filter));
	ret = igb_parse_ethertype_filter(dev, attr, pattern,
				actions, &ethertype_filter, error);
	if (!ret) {
		ret = igb_add_del_ethertype_filter(dev,
				&ethertype_filter, TRUE);
		if (!ret) {
			ethertype_filter_ptr = rte_zmalloc(
				"igb_ethertype_filter",
				sizeof(struct igb_ethertype_filter_ele), 0);
			if (!ethertype_filter_ptr) {
				PMD_DRV_LOG(ERR, "failed to allocate memory");
				goto out;
			}

			rte_memcpy(&ethertype_filter_ptr->filter_info,
				&ethertype_filter,
				sizeof(struct rte_eth_ethertype_filter));
			TAILQ_INSERT_TAIL(&igb_filter_ethertype_list,
				ethertype_filter_ptr, entries);
			flow->rule = ethertype_filter_ptr;
			flow->filter_type = RTE_ETH_FILTER_ETHERTYPE;
			return flow;
		}
		goto out;
	}

	memset(&syn_filter, 0, sizeof(struct rte_eth_syn_filter));
	ret = igb_parse_syn_filter(dev, attr, pattern,
				actions, &syn_filter, error);
	if (!ret) {
		ret = eth_igb_syn_filter_set(dev, &syn_filter, TRUE);
		if (!ret) {
			syn_filter_ptr = rte_zmalloc("igb_syn_filter",
				sizeof(struct igb_eth_syn_filter_ele), 0);
			if (!syn_filter_ptr) {
				PMD_DRV_LOG(ERR, "failed to allocate memory");
				goto out;
			}

			rte_memcpy(&syn_filter_ptr->filter_info,
				&syn_filter,
				sizeof(struct rte_eth_syn_filter));
			TAILQ_INSERT_TAIL(&igb_filter_syn_list,
				syn_filter_ptr,
				entries);
			flow->rule = syn_filter_ptr;
			flow->filter_type = RTE_ETH_FILTER_SYN;
			return flow;
		}
		goto out;
	}

	memset(&flex_filter, 0, sizeof(struct igb_flex_filter));
	ret = igb_parse_flex_filter(dev, attr, pattern,
					actions, &flex_filter, error);
	if (!ret) {
		ret = eth_igb_add_del_flex_filter(dev, &flex_filter, TRUE);
		if (!ret) {
			flex_filter_ptr = rte_zmalloc("igb_flex_filter",
				sizeof(struct igb_flex_filter_ele), 0);
			if (!flex_filter_ptr) {
				PMD_DRV_LOG(ERR, "failed to allocate memory");
				goto out;
			}

			rte_memcpy(&flex_filter_ptr->filter_info,
				&flex_filter,
				sizeof(struct igb_flex_filter));
			TAILQ_INSERT_TAIL(&igb_filter_flex_list,
				flex_filter_ptr, entries);
			flow->rule = flex_filter_ptr;
			flow->filter_type = RTE_ETH_FILTER_FLEXIBLE;
			return flow;
		}
	}

	memset(&rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
	ret = igb_parse_rss_filter(dev, attr,
					actions, &rss_conf, error);
	if (!ret) {
		ret = igb_config_rss_filter(dev, &rss_conf, TRUE);
		if (!ret) {
			rss_filter_ptr = rte_zmalloc("igb_rss_filter",
				sizeof(struct igb_rss_conf_ele), 0);
			if (!rss_filter_ptr) {
				PMD_DRV_LOG(ERR, "failed to allocate memory");
				goto out;
			}
			igb_rss_conf_init(dev, &rss_filter_ptr->filter_info,
					  &rss_conf.conf);
			TAILQ_INSERT_TAIL(&igb_filter_rss_list,
				rss_filter_ptr, entries);
			flow->rule = rss_filter_ptr;
			flow->filter_type = RTE_ETH_FILTER_HASH;
			return flow;
		}
	}

out:
	TAILQ_REMOVE(&igb_flow_list,
		igb_flow_mem_ptr, entries);
	rte_flow_error_set(error, -ret,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");
	rte_free(igb_flow_mem_ptr);
	rte_free(flow);
	return NULL;
}

/**
 * Check if the flow rule is supported by igb.
 * It only checks the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 */
static int
igb_flow_validate(__rte_unused struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct rte_eth_ntuple_filter ntuple_filter;
	struct rte_eth_ethertype_filter ethertype_filter;
	struct rte_eth_syn_filter syn_filter;
	struct igb_flex_filter flex_filter;
	struct igb_rte_flow_rss_conf rss_conf;
	int ret;

	memset(&ntuple_filter, 0, sizeof(struct rte_eth_ntuple_filter));
	ret = igb_parse_ntuple_filter(dev, attr, pattern,
				actions, &ntuple_filter, error);
	if (!ret)
		return 0;

	memset(&ethertype_filter, 0, sizeof(struct rte_eth_ethertype_filter));
	ret = igb_parse_ethertype_filter(dev, attr, pattern,
				actions, &ethertype_filter, error);
	if (!ret)
		return 0;

	memset(&syn_filter, 0, sizeof(struct rte_eth_syn_filter));
	ret = igb_parse_syn_filter(dev, attr, pattern,
				actions, &syn_filter, error);
	if (!ret)
		return 0;

	memset(&flex_filter, 0, sizeof(struct igb_flex_filter));
	ret = igb_parse_flex_filter(dev, attr, pattern,
				actions, &flex_filter, error);
	if (!ret)
		return 0;

	memset(&rss_conf, 0, sizeof(struct igb_rte_flow_rss_conf));
	ret = igb_parse_rss_filter(dev, attr,
					actions, &rss_conf, error);

	return ret;
}

/* Destroy a flow rule on igb. */
static int
igb_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	int ret;
	struct rte_flow *pmd_flow = flow;
	enum rte_filter_type filter_type = pmd_flow->filter_type;
	struct igb_ntuple_filter_ele *ntuple_filter_ptr;
	struct igb_ethertype_filter_ele *ethertype_filter_ptr;
	struct igb_eth_syn_filter_ele *syn_filter_ptr;
	struct igb_flex_filter_ele *flex_filter_ptr;
	struct igb_flow_mem *igb_flow_mem_ptr;
	struct igb_rss_conf_ele *rss_filter_ptr;

	switch (filter_type) {
	case RTE_ETH_FILTER_NTUPLE:
		ntuple_filter_ptr = (struct igb_ntuple_filter_ele *)
					pmd_flow->rule;
		ret = igb_add_del_ntuple_filter(dev,
				&ntuple_filter_ptr->filter_info, FALSE);
		if (!ret) {
			TAILQ_REMOVE(&igb_filter_ntuple_list,
			ntuple_filter_ptr, entries);
			rte_free(ntuple_filter_ptr);
		}
		break;
	case RTE_ETH_FILTER_ETHERTYPE:
		ethertype_filter_ptr = (struct igb_ethertype_filter_ele *)
					pmd_flow->rule;
		ret = igb_add_del_ethertype_filter(dev,
				&ethertype_filter_ptr->filter_info, FALSE);
		if (!ret) {
			TAILQ_REMOVE(&igb_filter_ethertype_list,
				ethertype_filter_ptr, entries);
			rte_free(ethertype_filter_ptr);
		}
		break;
	case RTE_ETH_FILTER_SYN:
		syn_filter_ptr = (struct igb_eth_syn_filter_ele *)
				pmd_flow->rule;
		ret = eth_igb_syn_filter_set(dev,
				&syn_filter_ptr->filter_info, FALSE);
		if (!ret) {
			TAILQ_REMOVE(&igb_filter_syn_list,
				syn_filter_ptr, entries);
			rte_free(syn_filter_ptr);
		}
		break;
	case RTE_ETH_FILTER_FLEXIBLE:
		flex_filter_ptr = (struct igb_flex_filter_ele *)
				pmd_flow->rule;
		ret = eth_igb_add_del_flex_filter(dev,
				&flex_filter_ptr->filter_info, FALSE);
		if (!ret) {
			TAILQ_REMOVE(&igb_filter_flex_list,
				flex_filter_ptr, entries);
			rte_free(flex_filter_ptr);
		}
		break;
	case RTE_ETH_FILTER_HASH:
		rss_filter_ptr = (struct igb_rss_conf_ele *)
				pmd_flow->rule;
		ret = igb_config_rss_filter(dev,
					&rss_filter_ptr->filter_info, FALSE);
		if (!ret) {
			TAILQ_REMOVE(&igb_filter_rss_list,
				rss_filter_ptr, entries);
			rte_free(rss_filter_ptr);
		}
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
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

	TAILQ_FOREACH(igb_flow_mem_ptr, &igb_flow_list, entries) {
		if (igb_flow_mem_ptr->flow == pmd_flow) {
			TAILQ_REMOVE(&igb_flow_list,
				igb_flow_mem_ptr, entries);
			rte_free(igb_flow_mem_ptr);
		}
	}
	rte_free(flow);

	return ret;
}

/* remove all the n-tuple filters */
static void
igb_clear_all_ntuple_filter(struct rte_eth_dev *dev)
{
	struct e1000_filter_info *filter_info =
		E1000_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	struct e1000_5tuple_filter *p_5tuple;
	struct e1000_2tuple_filter *p_2tuple;

	while ((p_5tuple = TAILQ_FIRST(&filter_info->fivetuple_list)))
		igb_delete_5tuple_filter_82576(dev, p_5tuple);

	while ((p_2tuple = TAILQ_FIRST(&filter_info->twotuple_list)))
		igb_delete_2tuple_filter(dev, p_2tuple);
}

/* remove all the ether type filters */
static void
igb_clear_all_ethertype_filter(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_filter_info *filter_info =
		E1000_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	int i;

	for (i = 0; i < E1000_MAX_ETQF_FILTERS; i++) {
		if (filter_info->ethertype_mask & (1 << i)) {
			(void)igb_ethertype_filter_remove(filter_info,
							    (uint8_t)i);
			E1000_WRITE_REG(hw, E1000_ETQF(i), 0);
			E1000_WRITE_FLUSH(hw);
		}
	}
}

/* remove the SYN filter */
static void
igb_clear_syn_filter(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_filter_info *filter_info =
		E1000_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);

	if (filter_info->syn_info & E1000_SYN_FILTER_ENABLE) {
		filter_info->syn_info = 0;
		E1000_WRITE_REG(hw, E1000_SYNQF(0), 0);
		E1000_WRITE_FLUSH(hw);
	}
}

/* remove all the flex filters */
static void
igb_clear_all_flex_filter(struct rte_eth_dev *dev)
{
	struct e1000_filter_info *filter_info =
		E1000_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	struct e1000_flex_filter *flex_filter;

	while ((flex_filter = TAILQ_FIRST(&filter_info->flex_list)))
		igb_remove_flex_filter(dev, flex_filter);
}

/* remove the rss filter */
static void
igb_clear_rss_filter(struct rte_eth_dev *dev)
{
	struct e1000_filter_info *filter =
		E1000_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);

	if (filter->rss_info.conf.queue_num)
		igb_config_rss_filter(dev, &filter->rss_info, FALSE);
}

void
igb_filterlist_flush(struct rte_eth_dev *dev)
{
	struct igb_ntuple_filter_ele *ntuple_filter_ptr;
	struct igb_ethertype_filter_ele *ethertype_filter_ptr;
	struct igb_eth_syn_filter_ele *syn_filter_ptr;
	struct igb_flex_filter_ele *flex_filter_ptr;
	struct igb_rss_conf_ele  *rss_filter_ptr;
	struct igb_flow_mem *igb_flow_mem_ptr;
	enum rte_filter_type filter_type;
	struct rte_flow *pmd_flow;

	TAILQ_FOREACH(igb_flow_mem_ptr, &igb_flow_list, entries) {
		if (igb_flow_mem_ptr->dev == dev) {
			pmd_flow = igb_flow_mem_ptr->flow;
			filter_type = pmd_flow->filter_type;

			switch (filter_type) {
			case RTE_ETH_FILTER_NTUPLE:
				ntuple_filter_ptr =
				(struct igb_ntuple_filter_ele *)
					pmd_flow->rule;
				TAILQ_REMOVE(&igb_filter_ntuple_list,
						ntuple_filter_ptr, entries);
				rte_free(ntuple_filter_ptr);
				break;
			case RTE_ETH_FILTER_ETHERTYPE:
				ethertype_filter_ptr =
				(struct igb_ethertype_filter_ele *)
					pmd_flow->rule;
				TAILQ_REMOVE(&igb_filter_ethertype_list,
						ethertype_filter_ptr, entries);
				rte_free(ethertype_filter_ptr);
				break;
			case RTE_ETH_FILTER_SYN:
				syn_filter_ptr =
					(struct igb_eth_syn_filter_ele *)
						pmd_flow->rule;
				TAILQ_REMOVE(&igb_filter_syn_list,
						syn_filter_ptr, entries);
				rte_free(syn_filter_ptr);
				break;
			case RTE_ETH_FILTER_FLEXIBLE:
				flex_filter_ptr =
					(struct igb_flex_filter_ele *)
						pmd_flow->rule;
				TAILQ_REMOVE(&igb_filter_flex_list,
						flex_filter_ptr, entries);
				rte_free(flex_filter_ptr);
				break;
			case RTE_ETH_FILTER_HASH:
				rss_filter_ptr =
					(struct igb_rss_conf_ele *)
						pmd_flow->rule;
				TAILQ_REMOVE(&igb_filter_rss_list,
						rss_filter_ptr, entries);
				rte_free(rss_filter_ptr);
				break;
			default:
				PMD_DRV_LOG(WARNING, "Filter type"
					"(%d) not supported", filter_type);
				break;
			}
			TAILQ_REMOVE(&igb_flow_list,
				 igb_flow_mem_ptr,
				 entries);
			rte_free(igb_flow_mem_ptr->flow);
			rte_free(igb_flow_mem_ptr);
		}
	}
}

/*  Destroy all flow rules associated with a port on igb. */
static int
igb_flow_flush(struct rte_eth_dev *dev,
		__rte_unused struct rte_flow_error *error)
{
	igb_clear_all_ntuple_filter(dev);
	igb_clear_all_ethertype_filter(dev);
	igb_clear_syn_filter(dev);
	igb_clear_all_flex_filter(dev);
	igb_clear_rss_filter(dev);
	igb_filterlist_flush(dev);

	return 0;
}

const struct rte_flow_ops igb_flow_ops = {
	.validate = igb_flow_validate,
	.create = igb_flow_create,
	.destroy = igb_flow_destroy,
	.flush = igb_flow_flush,
};
