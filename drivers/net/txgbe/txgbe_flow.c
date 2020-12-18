/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <sys/queue.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>

#include "txgbe_ethdev.h"

#define TXGBE_MIN_N_TUPLE_PRIO 1
#define TXGBE_MAX_N_TUPLE_PRIO 7

/**
 * Endless loop will never happen with below assumption
 * 1. there is at least one no-void item(END)
 * 2. cur is before END.
 */
static inline
const struct rte_flow_item *next_no_void_pattern(
		const struct rte_flow_item pattern[],
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

static inline
const struct rte_flow_action *next_no_void_action(
		const struct rte_flow_action actions[],
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

/**
 * Please aware there's an assumption for all the parsers.
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
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	const struct rte_flow_item_vlan *vlan_spec;
	const struct rte_flow_item_vlan *vlan_mask;
	struct rte_flow_item_eth eth_null;
	struct rte_flow_item_vlan vlan_null;

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

	memset(&eth_null, 0, sizeof(struct rte_flow_item_eth));
	memset(&vlan_null, 0, sizeof(struct rte_flow_item_vlan));

	/* the first not void item can be MAC or IPv4 */
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
		eth_spec = item->spec;
		eth_mask = item->mask;
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error,
			  EINVAL,
			  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			  item, "Not supported last point for range");
			return -rte_errno;
		}
		/* if the first item is MAC, the content should be NULL */
		if ((item->spec || item->mask) &&
			(memcmp(eth_spec, &eth_null,
				sizeof(struct rte_flow_item_eth)) ||
			 memcmp(eth_mask, &eth_null,
				sizeof(struct rte_flow_item_eth)))) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}
		/* check if the next not void item is IPv4 or Vlan */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
			item->type != RTE_FLOW_ITEM_TYPE_VLAN) {
			rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}
	}

	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_spec = item->spec;
		vlan_mask = item->mask;
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}
		/* the content should be NULL */
		if ((item->spec || item->mask) &&
			(memcmp(vlan_spec, &vlan_null,
				sizeof(struct rte_flow_item_vlan)) ||
			 memcmp(vlan_mask, &vlan_null,
				sizeof(struct rte_flow_item_vlan)))) {
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

	if (item->mask) {
		/* get the IPv4 info */
		if (!item->spec || !item->mask) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid ntuple mask");
			return -rte_errno;
		}
		/*Not supported last point for range*/
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
		if ((ipv4_mask->hdr.src_addr != 0 &&
			ipv4_mask->hdr.src_addr != UINT32_MAX) ||
			(ipv4_mask->hdr.dst_addr != 0 &&
			ipv4_mask->hdr.dst_addr != UINT32_MAX) ||
			(ipv4_mask->hdr.next_proto_id != UINT8_MAX &&
			ipv4_mask->hdr.next_proto_id != 0)) {
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
	}

	/* check if the next not void item is TCP or UDP */
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
	    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
	    item->type != RTE_FLOW_ITEM_TYPE_SCTP &&
	    item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

	if (item->type != RTE_FLOW_ITEM_TYPE_END &&
		(!item->spec && !item->mask)) {
		goto action;
	}

	/* get the TCP/UDP/SCTP info */
	if (item->type != RTE_FLOW_ITEM_TYPE_END &&
		(!item->spec || !item->mask)) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Invalid ntuple mask");
		return -rte_errno;
	}

	/*Not supported last point for range*/
	if (item->last) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
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
		if ((tcp_mask->hdr.src_port != 0 &&
			tcp_mask->hdr.src_port != UINT16_MAX) ||
			(tcp_mask->hdr.dst_port != 0 &&
			tcp_mask->hdr.dst_port != UINT16_MAX)) {
			rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
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
			memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}

		tcp_spec = item->spec;
		filter->dst_port  = tcp_spec->hdr.dst_port;
		filter->src_port  = tcp_spec->hdr.src_port;
		filter->tcp_flags = tcp_spec->hdr.tcp_flags;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
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
		if ((udp_mask->hdr.src_port != 0 &&
			udp_mask->hdr.src_port != UINT16_MAX) ||
			(udp_mask->hdr.dst_port != 0 &&
			udp_mask->hdr.dst_port != UINT16_MAX)) {
			rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -rte_errno;
		}

		filter->dst_port_mask = udp_mask->hdr.dst_port;
		filter->src_port_mask = udp_mask->hdr.src_port;

		udp_spec = item->spec;
		filter->dst_port = udp_spec->hdr.dst_port;
		filter->src_port = udp_spec->hdr.src_port;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_SCTP) {
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

		sctp_spec = item->spec;
		filter->dst_port = sctp_spec->hdr.dst_port;
		filter->src_port = sctp_spec->hdr.src_port;
	} else {
		goto action;
	}

	/* check if the next not void item is END */
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -rte_errno;
	}

action:

	/**
	 * n-tuple only supports forwarding,
	 * check if the first not void action is QUEUE.
	 */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			item, "Not supported action.");
		return -rte_errno;
	}
	filter->queue =
		((const struct rte_flow_action_queue *)act->conf)->index;

	/* check if the next not void item is END */
	act = next_no_void_action(actions, act);
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
	if (attr->priority < TXGBE_MIN_N_TUPLE_PRIO ||
		attr->priority > TXGBE_MAX_N_TUPLE_PRIO)
		filter->priority = 1;

	return 0;
}

/* a specific function for txgbe because the flags is specific */
static int
txgbe_parse_ntuple_filter(struct rte_eth_dev *dev,
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

	/* txgbe doesn't support tcp flags */
	if (filter->flags & RTE_NTUPLE_FLAGS_TCP_FLAG) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Not supported by ntuple filter");
		return -rte_errno;
	}

	/* txgbe doesn't support many priorities */
	if (filter->priority < TXGBE_MIN_N_TUPLE_PRIO ||
	    filter->priority > TXGBE_MAX_N_TUPLE_PRIO) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "Priority not supported by ntuple filter");
		return -rte_errno;
	}

	if (filter->queue >= dev->data->nb_rx_queues) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Not supported by ntuple filter");
		return -rte_errno;
	}

	/* fixed value for txgbe */
	filter->flags = RTE_5TUPLE_FLAGS;
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

	item = next_no_void_pattern(pattern, NULL);
	/* The first non-void item should be MAC. */
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
	if (!rte_is_zero_ether_addr(&eth_mask->src) ||
	    (!rte_is_zero_ether_addr(&eth_mask->dst) &&
	     !rte_is_broadcast_ether_addr(&eth_mask->dst))) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid ether address mask");
		return -rte_errno;
	}

	if ((eth_mask->type & UINT16_MAX) != UINT16_MAX) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Invalid ethertype mask");
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

	/* Check if the next non-void item is END. */
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ethertype filter.");
		return -rte_errno;
	}

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
txgbe_parse_ethertype_filter(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct rte_eth_ethertype_filter *filter,
			     struct rte_flow_error *error)
{
	int ret;

	ret = cons_parse_ethertype_filter(attr, pattern,
					actions, filter, error);

	if (ret)
		return ret;

	if (filter->queue >= dev->data->nb_rx_queues) {
		memset(filter, 0, sizeof(struct rte_eth_ethertype_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			NULL, "queue index much too big");
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


	/* the first not void item should be MAC or IPv4 or IPv6 or TCP */
	item = next_no_void_pattern(pattern, NULL);
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
		item = next_no_void_pattern(pattern, item);
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
		item = next_no_void_pattern(pattern, item);
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
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by syn filter");
		return -rte_errno;
	}

	/* check if the first not void action is QUEUE. */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	act_q = (const struct rte_flow_action_queue *)act->conf;
	filter->queue = act_q->index;
	if (filter->queue >= TXGBE_MAX_RX_QUEUE_NUM) {
		memset(filter, 0, sizeof(struct rte_eth_syn_filter));
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
		return -rte_errno;
	}

	/* check if the next not void item is END */
	act = next_no_void_action(actions, act);
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
txgbe_parse_syn_filter(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct rte_eth_syn_filter *filter,
			     struct rte_flow_error *error)
{
	int ret;

	ret = cons_parse_syn_filter(attr, pattern,
					actions, filter, error);

	if (filter->queue >= dev->data->nb_rx_queues)
		return -rte_errno;

	if (ret)
		return ret;

	return 0;
}

/**
 * Create or destroy a flow rule.
 * Theorically one rule can match more than one filters.
 * We will let it use the filter which it hitt first.
 * So, the sequence matters.
 */
static struct rte_flow *
txgbe_flow_create(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;
	return flow;
}

/**
 * Check if the flow rule is supported by txgbe.
 * It only checks the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 */
static int
txgbe_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct rte_eth_ntuple_filter ntuple_filter;
	struct rte_eth_ethertype_filter ethertype_filter;
	struct rte_eth_syn_filter syn_filter;
	int ret = 0;

	memset(&ntuple_filter, 0, sizeof(struct rte_eth_ntuple_filter));
	ret = txgbe_parse_ntuple_filter(dev, attr, pattern,
				actions, &ntuple_filter, error);
	if (!ret)
		return 0;

	memset(&ethertype_filter, 0, sizeof(struct rte_eth_ethertype_filter));
	ret = txgbe_parse_ethertype_filter(dev, attr, pattern,
				actions, &ethertype_filter, error);
	if (!ret)
		return 0;

	memset(&syn_filter, 0, sizeof(struct rte_eth_syn_filter));
	ret = txgbe_parse_syn_filter(dev, attr, pattern,
				actions, &syn_filter, error);
	if (!ret)
		return 0;

	return ret;
}

/* Destroy a flow rule on txgbe. */
static int
txgbe_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	int ret = 0;

	return ret;
}

/*  Destroy all flow rules associated with a port on txgbe. */
static int
txgbe_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	int ret = 0;

	return ret;
}

const struct rte_flow_ops txgbe_flow_ops = {
	.validate = txgbe_flow_validate,
	.create = txgbe_flow_create,
	.destroy = txgbe_flow_destroy,
	.flush = txgbe_flow_flush,
};

