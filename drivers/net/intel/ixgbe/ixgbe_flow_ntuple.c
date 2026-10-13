/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include <rte_flow.h>
#include <flow_graph.h>
#include <rte_ether.h>

#include "ixgbe_ethdev.h"
#include "ixgbe_flow.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"
#include "../common/flow_engine.h"

#define IXGBE_MIN_N_TUPLE_PRIO 1
#define IXGBE_MAX_N_TUPLE_PRIO 7

struct ixgbe_ntuple_flow {
	struct rte_flow flow;
	struct ixgbe_5tuple_filter_info key; /* HW-shaped match key */
	uint16_t queue;
	uint16_t index; /* HW filter slot */
};

/* per-device table of registered filters, indexed by assigned HW slot */
struct ixgbe_ntuple_priv {
	struct ixgbe_ntuple_flow *slots[IXGBE_MAX_FTQF_FILTERS];
};

struct ixgbe_ntuple_ctx {
	struct ci_flow_engine_ctx base;
	struct rte_eth_ntuple_filter ntuple;
};

/**
 * Ntuple filter graph implementation
 * Pattern: START -> [ETH] -> [VLAN] -> IPV4 -> [TCP|UDP|SCTP] -> END
 */

enum ixgbe_ntuple_node_id {
	IXGBE_NTUPLE_NODE_START = FLOW_GRAPH_NODE_FIRST,
	IXGBE_NTUPLE_NODE_ETH,
	IXGBE_NTUPLE_NODE_VLAN,
	IXGBE_NTUPLE_NODE_IPV4,
	IXGBE_NTUPLE_NODE_TCP,
	IXGBE_NTUPLE_NODE_UDP,
	IXGBE_NTUPLE_NODE_SCTP,
	IXGBE_NTUPLE_NODE_END,
	IXGBE_NTUPLE_NODE_MAX,
};

/* spec and effective mask, if present, must be all-zero */
static int
ixgbe_validate_ntuple_empty_item(const struct rte_flow_item *item, size_t size,
		struct rte_flow_error *error)
{
	const void *mask = ci_flow_item_mask(item);

	if ((item->spec != NULL && !ci_is_all_byte(item->spec, size, 0)) ||
	    (mask != NULL && !ci_is_all_byte(mask, size, 0))) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Not supported by ntuple filter");
	}

	return 0;
}

static int
ixgbe_validate_ntuple_eth(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	return ixgbe_validate_ntuple_empty_item(item,
			sizeof(struct rte_flow_item_eth), error);
}

static int
ixgbe_validate_ntuple_vlan(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	return ixgbe_validate_ntuple_empty_item(item,
			sizeof(struct rte_flow_item_vlan), error);
}

static int
ixgbe_validate_ntuple_ipv4(const void *ctx __rte_unused,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_mask;

	ipv4_mask = ci_flow_item_mask(item);

	if (ipv4_mask == NULL)
		return 0;

	/* Only src/dst addresses and protocol supported */
	if (ipv4_mask->hdr.version_ihl ||
	    ipv4_mask->hdr.type_of_service ||
	    ipv4_mask->hdr.total_length ||
	    ipv4_mask->hdr.packet_id ||
	    ipv4_mask->hdr.fragment_offset ||
	    ipv4_mask->hdr.time_to_live ||
	    ipv4_mask->hdr.hdr_checksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst IP and protocol supported");
	}

	/* Masks must be 0 or all-ones */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.src_addr) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.dst_addr) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.next_proto_id)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Partial masks not supported");
	}

	return 0;
}

static int
ixgbe_process_ntuple_ipv4(void *ctx,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_ntuple_ctx *ntuple_ctx = ctx;
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = ci_flow_item_mask(item);

	if (ipv4_mask == NULL)
		return 0;

	if (ipv4_mask->hdr.dst_addr != 0) {
		ntuple_ctx->ntuple.dst_ip = ipv4_spec->hdr.dst_addr;
		ntuple_ctx->ntuple.dst_ip_mask = ipv4_mask->hdr.dst_addr;
	}
	if (ipv4_mask->hdr.src_addr != 0) {
		ntuple_ctx->ntuple.src_ip = ipv4_spec->hdr.src_addr;
		ntuple_ctx->ntuple.src_ip_mask = ipv4_mask->hdr.src_addr;
	}
	if (ipv4_mask->hdr.next_proto_id != 0) {
		ntuple_ctx->ntuple.proto = ipv4_spec->hdr.next_proto_id;
		ntuple_ctx->ntuple.proto_mask = ipv4_mask->hdr.next_proto_id;
	}

	return 0;
}

/* an L4 item always implies its protocol, even when empty */
static int
ixgbe_ntuple_set_l4_proto(struct ixgbe_ntuple_ctx *ntuple_ctx,
		const struct rte_flow_item *item, uint8_t proto,
		struct rte_flow_error *error)
{
	struct rte_eth_ntuple_filter *ntuple = &ntuple_ctx->ntuple;

	if (ntuple->proto_mask != 0 && ntuple->proto != proto) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"L4 item conflicts with IPv4 protocol");
	}

	ntuple->proto = proto;
	ntuple->proto_mask = UINT8_MAX;

	return 0;
}

static int
ixgbe_validate_ntuple_tcp(const void *ctx __rte_unused,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_mask;

	tcp_mask = ci_flow_item_mask(item);

	if (tcp_mask == NULL)
		return 0;

	/* Only src/dst ports and tcp_flags supported */
	if (tcp_mask->hdr.sent_seq ||
	    tcp_mask->hdr.recv_ack ||
	    tcp_mask->hdr.data_off ||
	    tcp_mask->hdr.rx_win ||
	    tcp_mask->hdr.cksum ||
	    tcp_mask->hdr.tcp_urp) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst ports and flags supported");
	}

	/* Port masks must be 0 or all-ones */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&tcp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&tcp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Partial port masks not supported");
	}

	/* TCP flags not supported by hardware */
	if (!CI_FIELD_IS_ZERO(&tcp_mask->hdr.tcp_flags)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"TCP flags filtering not supported");
	}

	return 0;
}

static int
ixgbe_process_ntuple_tcp(void *ctx,
			 const struct rte_flow_item *item,
			 struct rte_flow_error *error)
{
	struct ixgbe_ntuple_ctx *ntuple_ctx = ctx;
	const struct rte_flow_item_tcp *tcp_spec = item->spec;
	const struct rte_flow_item_tcp *tcp_mask = ci_flow_item_mask(item);
	int ret;

	ret = ixgbe_ntuple_set_l4_proto(ntuple_ctx, item, IPPROTO_TCP, error);
	if (ret != 0 || tcp_mask == NULL)
		return ret;

	if (tcp_mask->hdr.dst_port != 0) {
		ntuple_ctx->ntuple.dst_port = tcp_spec->hdr.dst_port;
		ntuple_ctx->ntuple.dst_port_mask = tcp_mask->hdr.dst_port;
	}
	if (tcp_mask->hdr.src_port != 0) {
		ntuple_ctx->ntuple.src_port = tcp_spec->hdr.src_port;
		ntuple_ctx->ntuple.src_port_mask = tcp_mask->hdr.src_port;
	}

	return 0;
}

static int
ixgbe_validate_ntuple_udp(const void *ctx __rte_unused,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *udp_mask;

	udp_mask = ci_flow_item_mask(item);

	if (udp_mask == NULL)
		return 0;

	/* Only src/dst ports supported */
	if (udp_mask->hdr.dgram_len ||
	    udp_mask->hdr.dgram_cksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst ports supported");
	}

	/* Port masks must be 0 or all-ones */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&udp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&udp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Partial port masks not supported");
	}

	return 0;
}

static int
ixgbe_process_ntuple_udp(void *ctx,
			 const struct rte_flow_item *item,
			 struct rte_flow_error *error)
{
	struct ixgbe_ntuple_ctx *ntuple_ctx = ctx;
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = ci_flow_item_mask(item);
	int ret;

	ret = ixgbe_ntuple_set_l4_proto(ntuple_ctx, item, IPPROTO_UDP, error);
	if (ret != 0 || udp_mask == NULL)
		return ret;

	if (udp_mask->hdr.dst_port != 0) {
		ntuple_ctx->ntuple.dst_port = udp_spec->hdr.dst_port;
		ntuple_ctx->ntuple.dst_port_mask = udp_mask->hdr.dst_port;
	}
	if (udp_mask->hdr.src_port != 0) {
		ntuple_ctx->ntuple.src_port = udp_spec->hdr.src_port;
		ntuple_ctx->ntuple.src_port_mask = udp_mask->hdr.src_port;
	}

	return 0;
}

static int
ixgbe_validate_ntuple_sctp(const void *ctx __rte_unused,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_sctp *sctp_mask;

	sctp_mask = ci_flow_item_mask(item);

	if (sctp_mask == NULL)
		return 0;

	/* Only src/dst ports supported */
	if (sctp_mask->hdr.tag ||
	    sctp_mask->hdr.cksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst ports supported");
	}

	/* Port masks must be 0 or all-ones */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&sctp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&sctp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Partial port masks not supported");
	}

	return 0;
}

static int
ixgbe_process_ntuple_sctp(void *ctx,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error)
{
	struct ixgbe_ntuple_ctx *ntuple_ctx = ctx;
	const struct rte_flow_item_sctp *sctp_spec = item->spec;
	const struct rte_flow_item_sctp *sctp_mask = ci_flow_item_mask(item);
	int ret;

	ret = ixgbe_ntuple_set_l4_proto(ntuple_ctx, item, IPPROTO_SCTP, error);
	if (ret != 0 || sctp_mask == NULL)
		return ret;

	if (sctp_mask->hdr.dst_port != 0) {
		ntuple_ctx->ntuple.dst_port = sctp_spec->hdr.dst_port;
		ntuple_ctx->ntuple.dst_port_mask = sctp_mask->hdr.dst_port;
	}
	if (sctp_mask->hdr.src_port != 0) {
		ntuple_ctx->ntuple.src_port = sctp_spec->hdr.src_port;
		ntuple_ctx->ntuple.src_port_mask = sctp_mask->hdr.src_port;
	}

	return 0;
}

static const struct flow_graph ixgbe_ntuple_graph = {
	.nodes = (struct flow_graph_node[]) {
		[IXGBE_NTUPLE_NODE_START] = {
			.name = "START",
		},
		[IXGBE_NTUPLE_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_ntuple_eth,
		},
		[IXGBE_NTUPLE_NODE_VLAN] = {
			.name = "VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_ntuple_vlan,
		},
		[IXGBE_NTUPLE_NODE_IPV4] = {
			.name = "IPV4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_ntuple_ipv4,
			.process = ixgbe_process_ntuple_ipv4,
		},
		[IXGBE_NTUPLE_NODE_TCP] = {
			.name = "TCP",
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_ntuple_tcp,
			.process = ixgbe_process_ntuple_tcp,
		},
		[IXGBE_NTUPLE_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_ntuple_udp,
			.process = ixgbe_process_ntuple_udp,
		},
		[IXGBE_NTUPLE_NODE_SCTP] = {
			.name = "SCTP",
			.type = RTE_FLOW_ITEM_TYPE_SCTP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_ntuple_sctp,
			.process = ixgbe_process_ntuple_sctp,
		},
		[IXGBE_NTUPLE_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[IXGBE_NTUPLE_NODE_START] = {
			.next = (size_t[]) {
				IXGBE_NTUPLE_NODE_ETH,
				IXGBE_NTUPLE_NODE_IPV4,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_NTUPLE_NODE_ETH] = {
			.next = (size_t[]) {
				IXGBE_NTUPLE_NODE_VLAN,
				IXGBE_NTUPLE_NODE_IPV4,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_NTUPLE_NODE_VLAN] = {
			.next = (size_t[]) {
				IXGBE_NTUPLE_NODE_IPV4,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_NTUPLE_NODE_IPV4] = {
			.next = (size_t[]) {
				IXGBE_NTUPLE_NODE_TCP,
				IXGBE_NTUPLE_NODE_UDP,
				IXGBE_NTUPLE_NODE_SCTP,
				IXGBE_NTUPLE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_NTUPLE_NODE_TCP] = {
			.next = (size_t[]) {
				IXGBE_NTUPLE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_NTUPLE_NODE_UDP] = {
			.next = (size_t[]) {
				IXGBE_NTUPLE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_NTUPLE_NODE_SCTP] = {
			.next = (size_t[]) {
				IXGBE_NTUPLE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
ixgbe_flow_ntuple_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct ixgbe_ntuple_ctx *ntuple_ctx = (struct ixgbe_ntuple_ctx *)ctx;
	struct ci_flow_attr_check_param attr_param = {
		.allow_priority = true,
	};
	struct ci_flow_actions parsed_actions;
	struct ci_flow_actions_check_param ap_param = {
		.allowed_types = (const enum rte_flow_action_type[]){
			/* only queue is allowed here */
			RTE_FLOW_ACTION_TYPE_QUEUE,
			RTE_FLOW_ACTION_TYPE_END
		},
		.driver_ctx = ctx->dev_data,
		.check = ixgbe_flow_actions_check,
		.max_actions = 1,
	};
	const struct rte_flow_action_queue *q_act;
	uint16_t priority;
	int ret;

	/* validate attributes */
	ret = ci_flow_check_attr(attr, &attr_param, error);
	if (ret)
		return ret;

	/* Priority must be 16-bit */
	if (attr->priority > UINT16_MAX) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, attr,
				"Priority must be 16-bit");
	}

	/* parse requested actions */
	ret = ci_flow_check_actions(actions, &ap_param, &parsed_actions, error);
	if (ret)
		return ret;

	q_act = (const struct rte_flow_action_queue *)parsed_actions.actions[0]->conf;

	ntuple_ctx->ntuple.queue = q_act->index;

	/*
	 * rte_flow priority: 0 through UINT32_MAX, 0 is highest
	 *
	 * ntuple priority: 001b through 111b, 111b is highest
	 *
	 * which means we need to transform priority from rte_flow to ntuple:
	 *
	 * 1) clamp max value
	 * 2) reverse
	 * 3) add min value
	 */
	priority = RTE_MIN(IXGBE_MAX_N_TUPLE_PRIO - 1, (uint16_t)attr->priority);
	priority = IXGBE_MAX_N_TUPLE_PRIO - 1 - priority;
	priority += IXGBE_MIN_N_TUPLE_PRIO;
	ntuple_ctx->ntuple.priority = priority;

	/* fixed value for ixgbe */
	ntuple_ctx->ntuple.flags = RTE_5TUPLE_FLAGS;

	return 0;
}

static enum ixgbe_5tuple_protocol
convert_protocol_type(uint8_t protocol_value)
{
	if (protocol_value == IPPROTO_TCP)
		return IXGBE_FILTER_PROTOCOL_TCP;
	else if (protocol_value == IPPROTO_UDP)
		return IXGBE_FILTER_PROTOCOL_UDP;
	else if (protocol_value == IPPROTO_SCTP)
		return IXGBE_FILTER_PROTOCOL_SCTP;
	else
		return IXGBE_FILTER_PROTOCOL_NONE;
}

static int
ixgbe_flow_ntuple_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct ixgbe_ntuple_ctx *ntuple_ctx = (const struct ixgbe_ntuple_ctx *)ctx;
	struct ixgbe_ntuple_flow *ntuple_flow = (struct ixgbe_ntuple_flow *)flow;
	const struct rte_eth_ntuple_filter *ntuple = &ntuple_ctx->ntuple;
	struct ixgbe_5tuple_filter_info *key = &ntuple_flow->key;

	/* mask shape (0 or all-ones) is already guaranteed by the graph */
	memset(key, 0, sizeof(*key));

	key->dst_ip_mask = ntuple->dst_ip_mask == 0;
	if (key->dst_ip_mask == 0)
		key->dst_ip = ntuple->dst_ip;
	key->src_ip_mask = ntuple->src_ip_mask == 0;
	if (key->src_ip_mask == 0)
		key->src_ip = ntuple->src_ip;
	key->dst_port_mask = ntuple->dst_port_mask == 0;
	if (key->dst_port_mask == 0)
		key->dst_port = ntuple->dst_port;
	key->src_port_mask = ntuple->src_port_mask == 0;
	if (key->src_port_mask == 0)
		key->src_port = ntuple->src_port;
	key->proto_mask = ntuple->proto_mask == 0;
	if (key->proto_mask == 0)
		key->proto = convert_protocol_type(ntuple->proto);
	key->priority = (uint8_t)ntuple->priority;

	ntuple_flow->queue = ntuple->queue;

	return 0;
}

/* two ntuple filters are the same rule if they match the same 5-tuple key */
static bool
ixgbe_ntuple_key_equal(const struct ixgbe_5tuple_filter_info *lhs,
		const struct ixgbe_5tuple_filter_info *rhs)
{
	return memcmp(lhs, rhs, sizeof(*lhs)) == 0;
}

static int
ixgbe_ntuple_slot_find(const struct ixgbe_ntuple_priv *priv,
		const struct ixgbe_ntuple_flow *ntuple_flow)
{
	uint16_t free_idx = IXGBE_MAX_FTQF_FILTERS;
	uint16_t idx;

	for (idx = 0; idx < IXGBE_MAX_FTQF_FILTERS; idx++) {
		const struct ixgbe_ntuple_flow *registered = priv->slots[idx];

		if (registered == NULL) {
			if (free_idx == IXGBE_MAX_FTQF_FILTERS)
				free_idx = idx;
			continue;
		}
		if (ixgbe_ntuple_key_equal(&registered->key, &ntuple_flow->key))
			return -EEXIST;
	}
	if (free_idx == IXGBE_MAX_FTQF_FILTERS)
		return -ENOSPC;

	return (int)free_idx;
}

static int
ixgbe_flow_ntuple_flow_register(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct ixgbe_ntuple_flow *ntuple_flow = (struct ixgbe_ntuple_flow *)flow;
	struct ixgbe_ntuple_priv *priv = flow->engine_priv;
	int idx;

	idx = ixgbe_ntuple_slot_find(priv, ntuple_flow);
	if (idx == -EEXIST) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ntuple filter already exists");
	}
	if (idx == -ENOSPC) {
		return rte_flow_error_set(error, ENOSPC,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ntuple filter table is full");
	}
	if (idx < 0) {
		return rte_flow_error_set(error, -idx,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to register ntuple filter");
	}

	priv->slots[idx] = ntuple_flow;
	ntuple_flow->index = idx;

	return 0;
}

static int
ixgbe_flow_ntuple_flow_unregister(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct ixgbe_ntuple_flow *ntuple_flow = (struct ixgbe_ntuple_flow *)flow;
	struct ixgbe_ntuple_priv *priv = flow->engine_priv;

	if (ntuple_flow->index >= IXGBE_MAX_FTQF_FILTERS ||
			priv->slots[ntuple_flow->index] != ntuple_flow) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ntuple filter slot does not match on unregister");
	}

	priv->slots[ntuple_flow->index] = NULL;

	return 0;
}

static int
ixgbe_flow_ntuple_flow_install(struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_ntuple_flow *ntuple_flow = (struct ixgbe_ntuple_flow *)flow;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(flow->dev_data->dev_private);

	ixgbe_ntuple_filter_program(hw, ntuple_flow->index, &ntuple_flow->key,
			ntuple_flow->queue);

	return 0;
}

static int
ixgbe_flow_ntuple_flow_uninstall(struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_ntuple_flow *ntuple_flow = (struct ixgbe_ntuple_flow *)flow;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(flow->dev_data->dev_private);

	ixgbe_ntuple_filter_clear(hw, ntuple_flow->index);

	return 0;
}

static int
ixgbe_flow_ntuple_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv __rte_unused)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);

	/* only 82599 and X540 have L3/L4 5-tuple (ntuple) filters */
	if (hw->mac.type == ixgbe_mac_82599EB ||
			hw->mac.type == ixgbe_mac_X540)
		return 0;

	return -ENOTSUP;
}

static const struct ci_flow_engine_ops ixgbe_ntuple_ops = {
	.engine_init = ixgbe_flow_ntuple_engine_init,
	.ctx_init = ixgbe_flow_ntuple_ctx_init,
	.ctx_to_flow = ixgbe_flow_ntuple_ctx_to_flow,
	.flow_register = ixgbe_flow_ntuple_flow_register,
	.flow_unregister = ixgbe_flow_ntuple_flow_unregister,
	.flow_install = ixgbe_flow_ntuple_flow_install,
	.flow_uninstall = ixgbe_flow_ntuple_flow_uninstall,
};

const struct ci_flow_engine ixgbe_ntuple_flow_engine = {
	.name = "ixgbe_ntuple",
	.ctx_size = sizeof(struct ixgbe_ntuple_ctx),
	.flow_size = sizeof(struct ixgbe_ntuple_flow),
	.priv_size = sizeof(struct ixgbe_ntuple_priv),
	.ops = &ixgbe_ntuple_ops,
	.graph = &ixgbe_ntuple_graph,
};
