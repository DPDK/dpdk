/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <sys/queue.h>
#include <rte_bus_pci.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>

#include "txgbe_ethdev.h"

#define TXGBE_MIN_N_TUPLE_PRIO 1
#define TXGBE_MAX_N_TUPLE_PRIO 7
#define TXGBE_MAX_FLX_SOURCE_OFF 62

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
 * Parse the rule to see if it is a L2 tunnel rule.
 * And get the L2 tunnel filter info BTW.
 * Only support E-tag now.
 * pattern:
 * The first not void item can be E_TAG.
 * The next not void item must be END.
 * action:
 * The first not void action should be VF or PF.
 * The next not void action should be END.
 * pattern example:
 * ITEM		Spec			Mask
 * E_TAG	grp		0x1	0x3
		e_cid_base	0x309	0xFFF
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 */
static int
cons_parse_l2_tn_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct txgbe_l2_tunnel_conf *filter,
			struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_item_e_tag *e_tag_spec;
	const struct rte_flow_item_e_tag *e_tag_mask;
	const struct rte_flow_action *act;
	const struct rte_flow_action_vf *act_vf;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

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

	/* The first not void item should be e-tag. */
	item = next_no_void_pattern(pattern, NULL);
	if (item->type != RTE_FLOW_ITEM_TYPE_E_TAG) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by L2 tunnel filter");
		return -rte_errno;
	}

	if (!item->spec || !item->mask) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by L2 tunnel filter");
		return -rte_errno;
	}

	/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	e_tag_spec = item->spec;
	e_tag_mask = item->mask;

	/* Only care about GRP and E cid base. */
	if (e_tag_mask->epcp_edei_in_ecid_b ||
	    e_tag_mask->in_ecid_e ||
	    e_tag_mask->ecid_e ||
	    e_tag_mask->rsvd_grp_ecid_b != rte_cpu_to_be_16(0x3FFF)) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by L2 tunnel filter");
		return -rte_errno;
	}

	filter->l2_tunnel_type = RTE_L2_TUNNEL_TYPE_E_TAG;
	/**
	 * grp and e_cid_base are bit fields and only use 14 bits.
	 * e-tag id is taken as little endian by HW.
	 */
	filter->tunnel_id = rte_be_to_cpu_16(e_tag_spec->rsvd_grp_ecid_b);

	/* check if the next not void item is END */
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by L2 tunnel filter");
		return -rte_errno;
	}

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
			attr, "Only support ingress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->egress) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
			attr, "Not support egress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->transfer) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
			attr, "No support for transfer.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->priority) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			attr, "Not support priority.");
		return -rte_errno;
	}

	/* check if the first not void action is VF or PF. */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_VF &&
			act->type != RTE_FLOW_ACTION_TYPE_PF) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	if (act->type == RTE_FLOW_ACTION_TYPE_VF) {
		act_vf = (const struct rte_flow_action_vf *)act->conf;
		filter->pool = act_vf->id;
	} else {
		filter->pool = pci_dev->max_vfs;
	}

	/* check if the next not void item is END */
	act = next_no_void_action(actions, act);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

static int
txgbe_parse_l2_tn_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct txgbe_l2_tunnel_conf *l2_tn_filter,
			struct rte_flow_error *error)
{
	int ret = 0;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	uint16_t vf_num;

	ret = cons_parse_l2_tn_filter(dev, attr, pattern,
				actions, l2_tn_filter, error);

	vf_num = pci_dev->max_vfs;

	if (l2_tn_filter->pool > vf_num)
		return -rte_errno;

	return ret;
}

/* Parse to get the attr and action info of flow director rule. */
static int
txgbe_parse_fdir_act_attr(const struct rte_flow_attr *attr,
			  const struct rte_flow_action actions[],
			  struct txgbe_fdir_rule *rule,
			  struct rte_flow_error *error)
{
	const struct rte_flow_action *act;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_mark *mark;

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
			attr, "Only support ingress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->egress) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
			attr, "Not support egress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->transfer) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
			attr, "No support for transfer.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->priority) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			attr, "Not support priority.");
		return -rte_errno;
	}

	/* check if the first not void action is QUEUE or DROP. */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE &&
	    act->type != RTE_FLOW_ACTION_TYPE_DROP) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	if (act->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		act_q = (const struct rte_flow_action_queue *)act->conf;
		rule->queue = act_q->index;
	} else { /* drop */
		/* signature mode does not support drop action. */
		if (rule->mode == RTE_FDIR_MODE_SIGNATURE) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				act, "Not supported action.");
			return -rte_errno;
		}
		rule->fdirflags = TXGBE_FDIRPICMD_DROP;
	}

	/* check if the next not void item is MARK */
	act = next_no_void_action(actions, act);
	if (act->type != RTE_FLOW_ACTION_TYPE_MARK &&
		act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	rule->soft_id = 0;

	if (act->type == RTE_FLOW_ACTION_TYPE_MARK) {
		mark = (const struct rte_flow_action_mark *)act->conf;
		rule->soft_id = mark->id;
		act = next_no_void_action(actions, act);
	}

	/* check if the next not void item is END */
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

/* search next no void pattern and skip fuzzy */
static inline
const struct rte_flow_item *next_no_fuzzy_pattern(
		const struct rte_flow_item pattern[],
		const struct rte_flow_item *cur)
{
	const struct rte_flow_item *next =
		next_no_void_pattern(pattern, cur);
	while (1) {
		if (next->type != RTE_FLOW_ITEM_TYPE_FUZZY)
			return next;
		next = next_no_void_pattern(pattern, next);
	}
}

static inline uint8_t signature_match(const struct rte_flow_item pattern[])
{
	const struct rte_flow_item_fuzzy *spec, *last, *mask;
	const struct rte_flow_item *item;
	uint32_t sh, lh, mh;
	int i = 0;

	while (1) {
		item = pattern + i;
		if (item->type == RTE_FLOW_ITEM_TYPE_END)
			break;

		if (item->type == RTE_FLOW_ITEM_TYPE_FUZZY) {
			spec = item->spec;
			last = item->last;
			mask = item->mask;

			if (!spec || !mask)
				return 0;

			sh = spec->thresh;

			if (!last)
				lh = sh;
			else
				lh = last->thresh;

			mh = mask->thresh;
			sh = sh & mh;
			lh = lh & mh;

			if (!sh || sh > lh)
				return 0;

			return 1;
		}

		i++;
	}

	return 0;
}

/**
 * Parse the rule to see if it is a IP or MAC VLAN flow director rule.
 * And get the flow director filter info BTW.
 * UDP/TCP/SCTP PATTERN:
 * The first not void item can be ETH or IPV4 or IPV6
 * The second not void item must be IPV4 or IPV6 if the first one is ETH.
 * The next not void item could be UDP or TCP or SCTP (optional)
 * The next not void item could be RAW (for flexbyte, optional)
 * The next not void item must be END.
 * A Fuzzy Match pattern can appear at any place before END.
 * Fuzzy Match is optional for IPV4 but is required for IPV6
 * MAC VLAN PATTERN:
 * The first not void item must be ETH.
 * The second not void item must be MAC VLAN.
 * The next not void item must be END.
 * ACTION:
 * The first not void action should be QUEUE or DROP.
 * The second not void optional action should be MARK,
 * mark_id is a uint32_t number.
 * The next not void action should be END.
 * UDP/TCP/SCTP pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4		src_addr 192.168.1.20	0xFFFFFFFF
 *		dst_addr 192.167.3.50	0xFFFFFFFF
 * UDP/TCP/SCTP	src_port	80	0xFFFF
 *		dst_port	80	0xFFFF
 * FLEX	relative	0	0x1
 *		search		0	0x1
 *		reserved	0	0
 *		offset		12	0xFFFFFFFF
 *		limit		0	0xFFFF
 *		length		2	0xFFFF
 *		pattern[0]	0x86	0xFF
 *		pattern[1]	0xDD	0xFF
 * END
 * MAC VLAN pattern example:
 * ITEM		Spec			Mask
 * ETH		dst_addr
		{0xAC, 0x7B, 0xA1,	{0xFF, 0xFF, 0xFF,
		0x2C, 0x6D, 0x36}	0xFF, 0xFF, 0xFF}
 * MAC VLAN	tci	0x2016		0xEFFF
 * END
 * Other members in mask and spec should set to 0x00.
 * Item->last should be NULL.
 */
static int
txgbe_parse_fdir_filter_normal(struct rte_eth_dev *dev __rte_unused,
			       const struct rte_flow_attr *attr,
			       const struct rte_flow_item pattern[],
			       const struct rte_flow_action actions[],
			       struct txgbe_fdir_rule *rule,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_item_eth *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv4 *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec;
	const struct rte_flow_item_ipv6 *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;
	const struct rte_flow_item_udp *udp_spec;
	const struct rte_flow_item_udp *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec;
	const struct rte_flow_item_sctp *sctp_mask;
	const struct rte_flow_item_raw *raw_mask;
	const struct rte_flow_item_raw *raw_spec;
	u32 ptype = 0;
	uint8_t j;

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

	/**
	 * Some fields may not be provided. Set spec to 0 and mask to default
	 * value. So, we need not do anything for the not provided fields later.
	 */
	memset(rule, 0, sizeof(struct txgbe_fdir_rule));
	memset(&rule->mask, 0xFF, sizeof(struct txgbe_hw_fdir_mask));
	rule->mask.vlan_tci_mask = 0;
	rule->mask.flex_bytes_mask = 0;

	/**
	 * The first not void item should be
	 * MAC or IPv4 or TCP or UDP or SCTP.
	 */
	item = next_no_fuzzy_pattern(pattern, NULL);
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV6 &&
	    item->type != RTE_FLOW_ITEM_TYPE_TCP &&
	    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
	    item->type != RTE_FLOW_ITEM_TYPE_SCTP) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter");
		return -rte_errno;
	}

	if (signature_match(pattern))
		rule->mode = RTE_FDIR_MODE_SIGNATURE;
	else
		rule->mode = RTE_FDIR_MODE_PERFECT;

	/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	/* Get the MAC info. */
	if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
		/**
		 * Only support vlan and dst MAC address,
		 * others should be masked.
		 */
		if (item->spec && !item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}

		if (item->mask) {
			rule->b_mask = TRUE;
			eth_mask = item->mask;

			/* Ether type should be masked. */
			if (eth_mask->type ||
			    rule->mode == RTE_FDIR_MODE_SIGNATURE) {
				memset(rule, 0, sizeof(struct txgbe_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter");
				return -rte_errno;
			}

			/* If ethernet has meaning, it means MAC VLAN mode. */
			rule->mode = RTE_FDIR_MODE_PERFECT_MAC_VLAN;

			/**
			 * src MAC address must be masked,
			 * and don't support dst MAC address mask.
			 */
			for (j = 0; j < RTE_ETHER_ADDR_LEN; j++) {
				if (eth_mask->src.addr_bytes[j] ||
					eth_mask->dst.addr_bytes[j] != 0xFF) {
					memset(rule, 0,
					sizeof(struct txgbe_fdir_rule));
					rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter");
					return -rte_errno;
				}
			}

			/* When no VLAN, considered as full mask. */
			rule->mask.vlan_tci_mask = rte_cpu_to_be_16(0xEFFF);
		}
		/*** If both spec and mask are item,
		 * it means don't care about ETH.
		 * Do nothing.
		 */

		/**
		 * Check if the next not void item is vlan or ipv4.
		 * IPv6 is not supported.
		 */
		item = next_no_fuzzy_pattern(pattern, item);
		if (rule->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
			if (item->type != RTE_FLOW_ITEM_TYPE_VLAN) {
				memset(rule, 0, sizeof(struct txgbe_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter");
				return -rte_errno;
			}
		} else {
			if (item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
					item->type != RTE_FLOW_ITEM_TYPE_VLAN) {
				memset(rule, 0, sizeof(struct txgbe_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter");
				return -rte_errno;
			}
		}
	}

	/* Get the IPV4 info. */
	if (item->type == RTE_FLOW_ITEM_TYPE_IPV4) {
		/**
		 * Set the flow type even if there's no content
		 * as we must have a flow type.
		 */
		rule->input.flow_type = TXGBE_ATR_FLOW_TYPE_IPV4;
		ptype = txgbe_ptype_table[TXGBE_PT_IPV4];
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}
		/**
		 * Only care about src & dst addresses,
		 * others should be masked.
		 */
		if (!item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->b_mask = TRUE;
		ipv4_mask = item->mask;
		if (ipv4_mask->hdr.version_ihl ||
		    ipv4_mask->hdr.type_of_service ||
		    ipv4_mask->hdr.total_length ||
		    ipv4_mask->hdr.packet_id ||
		    ipv4_mask->hdr.fragment_offset ||
		    ipv4_mask->hdr.time_to_live ||
		    ipv4_mask->hdr.next_proto_id ||
		    ipv4_mask->hdr.hdr_checksum) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->mask.dst_ipv4_mask = ipv4_mask->hdr.dst_addr;
		rule->mask.src_ipv4_mask = ipv4_mask->hdr.src_addr;

		if (item->spec) {
			rule->b_spec = TRUE;
			ipv4_spec = item->spec;
			rule->input.dst_ip[0] =
				ipv4_spec->hdr.dst_addr;
			rule->input.src_ip[0] =
				ipv4_spec->hdr.src_addr;
		}

		/**
		 * Check if the next not void item is
		 * TCP or UDP or SCTP or END.
		 */
		item = next_no_fuzzy_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
		    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
		    item->type != RTE_FLOW_ITEM_TYPE_SCTP &&
		    item->type != RTE_FLOW_ITEM_TYPE_END &&
		    item->type != RTE_FLOW_ITEM_TYPE_RAW) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* Get the IPV6 info. */
	if (item->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		/**
		 * Set the flow type even if there's no content
		 * as we must have a flow type.
		 */
		rule->input.flow_type = TXGBE_ATR_FLOW_TYPE_IPV6;
		ptype = txgbe_ptype_table[TXGBE_PT_IPV6];

		/**
		 * 1. must signature match
		 * 2. not support last
		 * 3. mask must not null
		 */
		if (rule->mode != RTE_FDIR_MODE_SIGNATURE ||
		    item->last ||
		    !item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		rule->b_mask = TRUE;
		ipv6_mask = item->mask;
		if (ipv6_mask->hdr.vtc_flow ||
		    ipv6_mask->hdr.payload_len ||
		    ipv6_mask->hdr.proto ||
		    ipv6_mask->hdr.hop_limits) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}

		/* check src addr mask */
		for (j = 0; j < 16; j++) {
			if (ipv6_mask->hdr.src_addr[j] == UINT8_MAX) {
				rule->mask.src_ipv6_mask |= 1 << j;
			} else if (ipv6_mask->hdr.src_addr[j] != 0) {
				memset(rule, 0, sizeof(struct txgbe_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter");
				return -rte_errno;
			}
		}

		/* check dst addr mask */
		for (j = 0; j < 16; j++) {
			if (ipv6_mask->hdr.dst_addr[j] == UINT8_MAX) {
				rule->mask.dst_ipv6_mask |= 1 << j;
			} else if (ipv6_mask->hdr.dst_addr[j] != 0) {
				memset(rule, 0, sizeof(struct txgbe_fdir_rule));
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Not supported by fdir filter");
				return -rte_errno;
			}
		}

		if (item->spec) {
			rule->b_spec = TRUE;
			ipv6_spec = item->spec;
			rte_memcpy(rule->input.src_ip,
				   ipv6_spec->hdr.src_addr, 16);
			rte_memcpy(rule->input.dst_ip,
				   ipv6_spec->hdr.dst_addr, 16);
		}

		/**
		 * Check if the next not void item is
		 * TCP or UDP or SCTP or END.
		 */
		item = next_no_fuzzy_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
		    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
		    item->type != RTE_FLOW_ITEM_TYPE_SCTP &&
		    item->type != RTE_FLOW_ITEM_TYPE_END &&
		    item->type != RTE_FLOW_ITEM_TYPE_RAW) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* Get the TCP info. */
	if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
		/**
		 * Set the flow type even if there's no content
		 * as we must have a flow type.
		 */
		rule->input.flow_type |= TXGBE_ATR_L4TYPE_TCP;
		ptype = txgbe_ptype_table[TXGBE_PT_IPV4_TCP];
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}
		/**
		 * Only care about src & dst ports,
		 * others should be masked.
		 */
		if (!item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->b_mask = TRUE;
		tcp_mask = item->mask;
		if (tcp_mask->hdr.sent_seq ||
		    tcp_mask->hdr.recv_ack ||
		    tcp_mask->hdr.data_off ||
		    tcp_mask->hdr.tcp_flags ||
		    tcp_mask->hdr.rx_win ||
		    tcp_mask->hdr.cksum ||
		    tcp_mask->hdr.tcp_urp) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->mask.src_port_mask = tcp_mask->hdr.src_port;
		rule->mask.dst_port_mask = tcp_mask->hdr.dst_port;

		if (item->spec) {
			rule->b_spec = TRUE;
			tcp_spec = item->spec;
			rule->input.src_port =
				tcp_spec->hdr.src_port;
			rule->input.dst_port =
				tcp_spec->hdr.dst_port;
		}

		item = next_no_fuzzy_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_RAW &&
		    item->type != RTE_FLOW_ITEM_TYPE_END) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* Get the UDP info */
	if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
		/**
		 * Set the flow type even if there's no content
		 * as we must have a flow type.
		 */
		rule->input.flow_type |= TXGBE_ATR_L4TYPE_UDP;
		ptype = txgbe_ptype_table[TXGBE_PT_IPV4_UDP];
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}
		/**
		 * Only care about src & dst ports,
		 * others should be masked.
		 */
		if (!item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->b_mask = TRUE;
		udp_mask = item->mask;
		if (udp_mask->hdr.dgram_len ||
		    udp_mask->hdr.dgram_cksum) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->mask.src_port_mask = udp_mask->hdr.src_port;
		rule->mask.dst_port_mask = udp_mask->hdr.dst_port;

		if (item->spec) {
			rule->b_spec = TRUE;
			udp_spec = item->spec;
			rule->input.src_port =
				udp_spec->hdr.src_port;
			rule->input.dst_port =
				udp_spec->hdr.dst_port;
		}

		item = next_no_fuzzy_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_RAW &&
		    item->type != RTE_FLOW_ITEM_TYPE_END) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* Get the SCTP info */
	if (item->type == RTE_FLOW_ITEM_TYPE_SCTP) {
		/**
		 * Set the flow type even if there's no content
		 * as we must have a flow type.
		 */
		rule->input.flow_type |= TXGBE_ATR_L4TYPE_SCTP;
		ptype = txgbe_ptype_table[TXGBE_PT_IPV4_SCTP];
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		/**
		 * Only care about src & dst ports,
		 * others should be masked.
		 */
		if (!item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->b_mask = TRUE;
		sctp_mask = item->mask;
		if (sctp_mask->hdr.tag ||
			sctp_mask->hdr.cksum) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		rule->mask.src_port_mask = sctp_mask->hdr.src_port;
		rule->mask.dst_port_mask = sctp_mask->hdr.dst_port;

		if (item->spec) {
			rule->b_spec = TRUE;
			sctp_spec = item->spec;
			rule->input.src_port =
				sctp_spec->hdr.src_port;
			rule->input.dst_port =
				sctp_spec->hdr.dst_port;
		}
		/* others even sctp port is not supported */
		sctp_mask = item->mask;
		if (sctp_mask &&
			(sctp_mask->hdr.src_port ||
			 sctp_mask->hdr.dst_port ||
			 sctp_mask->hdr.tag ||
			 sctp_mask->hdr.cksum)) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}

		item = next_no_fuzzy_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_RAW &&
			item->type != RTE_FLOW_ITEM_TYPE_END) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* Get the flex byte info */
	if (item->type == RTE_FLOW_ITEM_TYPE_RAW) {
		/* Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}
		/* mask should not be null */
		if (!item->mask || !item->spec) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}

		raw_mask = item->mask;

		/* check mask */
		if (raw_mask->relative != 0x1 ||
		    raw_mask->search != 0x1 ||
		    raw_mask->reserved != 0x0 ||
		    (uint32_t)raw_mask->offset != 0xffffffff ||
		    raw_mask->limit != 0xffff ||
		    raw_mask->length != 0xffff) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}

		raw_spec = item->spec;

		/* check spec */
		if (raw_spec->relative != 0 ||
		    raw_spec->search != 0 ||
		    raw_spec->reserved != 0 ||
		    raw_spec->offset > TXGBE_MAX_FLX_SOURCE_OFF ||
		    raw_spec->offset % 2 ||
		    raw_spec->limit != 0 ||
		    raw_spec->length != 2 ||
		    /* pattern can't be 0xffff */
		    (raw_spec->pattern[0] == 0xff &&
		     raw_spec->pattern[1] == 0xff)) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}

		/* check pattern mask */
		if (raw_mask->pattern[0] != 0xff ||
		    raw_mask->pattern[1] != 0xff) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}

		rule->mask.flex_bytes_mask = 0xffff;
		rule->input.flex_bytes =
			(((uint16_t)raw_spec->pattern[1]) << 8) |
			raw_spec->pattern[0];
		rule->flex_bytes_offset = raw_spec->offset;
	}

	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		/* check if the next not void item is END */
		item = next_no_fuzzy_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_END) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	rule->input.pkt_type = cpu_to_be16(txgbe_encode_ptype(ptype));

	return txgbe_parse_fdir_act_attr(attr, actions, rule, error);
}

/**
 * Parse the rule to see if it is a VxLAN or NVGRE flow director rule.
 * And get the flow director filter info BTW.
 * VxLAN PATTERN:
 * The first not void item must be ETH.
 * The second not void item must be IPV4/ IPV6.
 * The third not void item must be NVGRE.
 * The next not void item must be END.
 * NVGRE PATTERN:
 * The first not void item must be ETH.
 * The second not void item must be IPV4/ IPV6.
 * The third not void item must be NVGRE.
 * The next not void item must be END.
 * ACTION:
 * The first not void action should be QUEUE or DROP.
 * The second not void optional action should be MARK,
 * mark_id is a uint32_t number.
 * The next not void action should be END.
 * VxLAN pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4/IPV6	NULL			NULL
 * UDP		NULL			NULL
 * VxLAN	vni{0x00, 0x32, 0x54}	{0xFF, 0xFF, 0xFF}
 * MAC VLAN	tci	0x2016		0xEFFF
 * END
 * NEGRV pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4/IPV6	NULL			NULL
 * NVGRE	protocol	0x6558	0xFFFF
 *		tni{0x00, 0x32, 0x54}	{0xFF, 0xFF, 0xFF}
 * MAC VLAN	tci	0x2016		0xEFFF
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 */
static int
txgbe_parse_fdir_filter_tunnel(const struct rte_flow_attr *attr,
			       const struct rte_flow_item pattern[],
			       const struct rte_flow_action actions[],
			       struct txgbe_fdir_rule *rule,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_item_eth *eth_mask;
	uint32_t j;

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

	/**
	 * Some fields may not be provided. Set spec to 0 and mask to default
	 * value. So, we need not do anything for the not provided fields later.
	 */
	memset(rule, 0, sizeof(struct txgbe_fdir_rule));
	memset(&rule->mask, 0xFF, sizeof(struct txgbe_hw_fdir_mask));
	rule->mask.vlan_tci_mask = 0;

	/**
	 * The first not void item should be
	 * MAC or IPv4 or IPv6 or UDP or VxLAN.
	 */
	item = next_no_void_pattern(pattern, NULL);
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV6 &&
	    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
	    item->type != RTE_FLOW_ITEM_TYPE_VXLAN &&
	    item->type != RTE_FLOW_ITEM_TYPE_NVGRE) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter");
		return -rte_errno;
	}

	rule->mode = RTE_FDIR_MODE_PERFECT_TUNNEL;

	/* Skip MAC. */
	if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
		/* Only used to describe the protocol stack. */
		if (item->spec || item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		/* Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		/* Check if the next not void item is IPv4 or IPv6. */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4 &&
		    item->type != RTE_FLOW_ITEM_TYPE_IPV6) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* Skip IP. */
	if (item->type == RTE_FLOW_ITEM_TYPE_IPV4 ||
	    item->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		/* Only used to describe the protocol stack. */
		if (item->spec || item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		/* Check if the next not void item is UDP or NVGRE. */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_UDP &&
		    item->type != RTE_FLOW_ITEM_TYPE_NVGRE) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* Skip UDP. */
	if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
		/* Only used to describe the protocol stack. */
		if (item->spec || item->mask) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				item, "Not supported last point for range");
			return -rte_errno;
		}

		/* Check if the next not void item is VxLAN. */
		item = next_no_void_pattern(pattern, item);
		if (item->type != RTE_FLOW_ITEM_TYPE_VXLAN) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* check if the next not void item is MAC */
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter");
		return -rte_errno;
	}

	/**
	 * Only support vlan and dst MAC address,
	 * others should be masked.
	 */

	if (!item->mask) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter");
		return -rte_errno;
	}
	/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}
	rule->b_mask = TRUE;
	eth_mask = item->mask;

	/* Ether type should be masked. */
	if (eth_mask->type) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter");
		return -rte_errno;
	}

	/* src MAC address should be masked. */
	for (j = 0; j < RTE_ETHER_ADDR_LEN; j++) {
		if (eth_mask->src.addr_bytes[j]) {
			memset(rule, 0,
			       sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}
	rule->mask.mac_addr_byte_mask = 0;
	for (j = 0; j < ETH_ADDR_LEN; j++) {
		/* It's a per byte mask. */
		if (eth_mask->dst.addr_bytes[j] == 0xFF) {
			rule->mask.mac_addr_byte_mask |= 0x1 << j;
		} else if (eth_mask->dst.addr_bytes[j]) {
			memset(rule, 0, sizeof(struct txgbe_fdir_rule));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by fdir filter");
			return -rte_errno;
		}
	}

	/* When no vlan, considered as full mask. */
	rule->mask.vlan_tci_mask = rte_cpu_to_be_16(0xEFFF);

	/**
	 * Check if the next not void item is vlan or ipv4.
	 * IPv6 is not supported.
	 */
	item = next_no_void_pattern(pattern, item);
	if (item->type != RTE_FLOW_ITEM_TYPE_VLAN &&
		item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
		memset(rule, 0, sizeof(struct txgbe_fdir_rule));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by fdir filter");
		return -rte_errno;
	}
	/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -rte_errno;
	}

	/**
	 * If the tags is 0, it means don't care about the VLAN.
	 * Do nothing.
	 */

	return txgbe_parse_fdir_act_attr(attr, actions, rule, error);
}

static int
txgbe_parse_fdir_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct txgbe_fdir_rule *rule,
			struct rte_flow_error *error)
{
	int ret;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	enum rte_fdir_mode fdir_mode = dev->data->dev_conf.fdir_conf.mode;

	ret = txgbe_parse_fdir_filter_normal(dev, attr, pattern,
					actions, rule, error);
	if (!ret)
		goto step_next;

	ret = txgbe_parse_fdir_filter_tunnel(attr, pattern,
					actions, rule, error);
	if (ret)
		return ret;

step_next:

	if (hw->mac.type == txgbe_mac_raptor &&
		rule->fdirflags == TXGBE_FDIRPICMD_DROP &&
		(rule->input.src_port != 0 || rule->input.dst_port != 0))
		return -ENOTSUP;

	if (fdir_mode == RTE_FDIR_MODE_NONE ||
	    fdir_mode != rule->mode)
		return -ENOTSUP;

	if (rule->queue >= dev->data->nb_rx_queues)
		return -ENOTSUP;

	return ret;
}

static int
txgbe_parse_rss_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_action actions[],
			struct txgbe_rte_flow_rss_conf *rss_conf,
			struct rte_flow_error *error)
{
	const struct rte_flow_action *act;
	const struct rte_flow_action_rss *rss;
	uint16_t n;

	/**
	 * rss only supports forwarding,
	 * check if the first not void action is RSS.
	 */
	act = next_no_void_action(actions, NULL);
	if (act->type != RTE_FLOW_ACTION_TYPE_RSS) {
		memset(rss_conf, 0, sizeof(struct txgbe_rte_flow_rss_conf));
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
	if (rss->queue_num > RTE_DIM(rss_conf->queue))
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "too many queues for RSS context");
	if (txgbe_rss_conf_init(rss_conf, rss))
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, act,
			 "RSS context initialization failure");

	/* check if the next not void item is END */
	act = next_no_void_action(actions, act);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(rss_conf, 0, sizeof(struct rte_eth_rss_conf));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			act, "Not supported action.");
		return -rte_errno;
	}

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(rss_conf, 0, sizeof(struct txgbe_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->egress) {
		memset(rss_conf, 0, sizeof(struct txgbe_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -rte_errno;
	}

	/* not supported */
	if (attr->transfer) {
		memset(rss_conf, 0, sizeof(struct txgbe_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
				   attr, "No support for transfer.");
		return -rte_errno;
	}

	if (attr->priority > 0xFFFF) {
		memset(rss_conf, 0, sizeof(struct txgbe_rte_flow_rss_conf));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Error priority.");
		return -rte_errno;
	}

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
	struct txgbe_l2_tunnel_conf l2_tn_filter;
	struct txgbe_fdir_rule fdir_rule;
	struct txgbe_rte_flow_rss_conf rss_conf;
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

	memset(&fdir_rule, 0, sizeof(struct txgbe_fdir_rule));
	ret = txgbe_parse_fdir_filter(dev, attr, pattern,
				actions, &fdir_rule, error);
	if (!ret)
		return 0;

	memset(&l2_tn_filter, 0, sizeof(struct txgbe_l2_tunnel_conf));
	ret = txgbe_parse_l2_tn_filter(dev, attr, pattern,
				actions, &l2_tn_filter, error);
	if (!ret)
		return 0;

	memset(&rss_conf, 0, sizeof(struct txgbe_rte_flow_rss_conf));
	ret = txgbe_parse_rss_filter(dev, attr,
					actions, &rss_conf, error);

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

