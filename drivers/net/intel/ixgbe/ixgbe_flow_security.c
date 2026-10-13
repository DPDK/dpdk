/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include <rte_common.h>
#include <rte_flow.h>
#include <flow_graph.h>
#include <rte_ether.h>
#include <rte_security_driver.h>

#include "ixgbe_ethdev.h"
#include "ixgbe_flow.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"
#include "../common/flow_engine.h"

struct ixgbe_security_ip_spec {
	bool is_ipv6;
	union {
		struct rte_flow_item_ipv4 ipv4;
		struct rte_flow_item_ipv6 ipv6;
	} spec;
};

struct ixgbe_security_filter {
	struct rte_security_session *session;
	struct ixgbe_crypto_rx_sa sa;
};

struct ixgbe_security_flow {
	struct rte_flow flow;
	struct ixgbe_security_filter security;
};

struct ixgbe_security_ctx {
	struct ci_flow_engine_ctx base;
	struct ixgbe_security_ip_spec spec;
	struct rte_security_session *session;
};

struct ixgbe_security_ip_slot {
	ci_usage_count_t use_count;
	struct ipaddr ip;
};

struct ixgbe_security_priv {
	struct ixgbe_security_ip_slot ip_slots[IPSEC_MAX_RX_IP_COUNT];
	struct ixgbe_security_flow *sa_slots[IPSEC_MAX_SA_COUNT];
};

/**
 * Ingress security filter graph implementation
 * Pattern: START -> [ETH] -> IPV4 | IPV6 -> [ESP | UDP -> ESP] -> END
 */

enum ixgbe_security_node_id {
	IXGBE_SECURITY_NODE_START = FLOW_GRAPH_NODE_FIRST,
	IXGBE_SECURITY_NODE_ETH,
	IXGBE_SECURITY_NODE_IPV4,
	IXGBE_SECURITY_NODE_IPV6,
	IXGBE_SECURITY_NODE_UDP,
	IXGBE_SECURITY_NODE_ESP,
	IXGBE_SECURITY_NODE_END,
	IXGBE_SECURITY_NODE_MAX,
};

static int
ixgbe_validate_security_ipv4(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_mask = ci_flow_item_mask(item);

	/* only src/dst addresses are supported */
	if (ipv4_mask->hdr.version_ihl ||
	    ipv4_mask->hdr.type_of_service ||
	    ipv4_mask->hdr.total_length ||
	    ipv4_mask->hdr.packet_id ||
	    ipv4_mask->hdr.fragment_offset ||
	    ipv4_mask->hdr.next_proto_id ||
	    ipv4_mask->hdr.time_to_live ||
	    ipv4_mask->hdr.hdr_checksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv4 mask");
	}

	/* both src/dst addresses must be fully masked */
	if (!CI_FIELD_IS_MASKED(&ipv4_mask->hdr.src_addr) ||
	    !CI_FIELD_IS_MASKED(&ipv4_mask->hdr.dst_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv4 mask");
	}

	return 0;
}

static int
ixgbe_process_security_ipv4(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_security_ctx *sec_ctx = (struct ixgbe_security_ctx *)ctx;
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;

	/* copy entire spec */
	sec_ctx->spec.spec.ipv4 = *ipv4_spec;
	sec_ctx->spec.is_ipv6 = false;

	return 0;
}

static int
ixgbe_validate_security_ipv6(const void *ctx __rte_unused,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *ipv6_mask = ci_flow_item_mask(item);

	/* only src/dst addresses are supported */
	if (ipv6_mask->hdr.vtc_flow ||
	    ipv6_mask->hdr.payload_len ||
	    ipv6_mask->hdr.proto ||
	    ipv6_mask->hdr.hop_limits) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv6 mask");
	}
	/* both src/dst addresses must be fully masked */
	if (!CI_FIELD_IS_MASKED(&ipv6_mask->hdr.src_addr) ||
	    !CI_FIELD_IS_MASKED(&ipv6_mask->hdr.dst_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv6 mask");
	}

	return 0;
}

static int
ixgbe_process_security_ipv6(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_security_ctx *sec_ctx = (struct ixgbe_security_ctx *)ctx;
	const struct rte_flow_item_ipv6 *ipv6_spec = item->spec;

	/* copy entire spec */
	sec_ctx->spec.spec.ipv6 = *ipv6_spec;
	sec_ctx->spec.is_ipv6 = true;

	return 0;
}

static const struct flow_graph ixgbe_security_graph = {
	.nodes = (struct flow_graph_node[]) {
		[IXGBE_SECURITY_NODE_START] = {
			.name = "START",
		},
		[IXGBE_SECURITY_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_SECURITY_NODE_IPV4] = {
			.name = "IPV4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_security_ipv4,
			.process = ixgbe_process_security_ipv4,
		},
		[IXGBE_SECURITY_NODE_IPV6] = {
			.name = "IPV6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_security_ipv6,
			.process = ixgbe_process_security_ipv6,
		},
		[IXGBE_SECURITY_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			/* for backwards compatibility, ignore all contents */
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_SECURITY_NODE_ESP] = {
			.name = "ESP",
			.type = RTE_FLOW_ITEM_TYPE_ESP,
			/* for backwards compatibility, ignore all contents */
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_SECURITY_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[IXGBE_SECURITY_NODE_START] = {
			.next = (size_t[]) {
				IXGBE_SECURITY_NODE_ETH,
				IXGBE_SECURITY_NODE_IPV4,
				IXGBE_SECURITY_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SECURITY_NODE_ETH] = {
			.next = (size_t[]) {
				IXGBE_SECURITY_NODE_IPV4,
				IXGBE_SECURITY_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SECURITY_NODE_IPV4] = {
			.next = (size_t[]) {
				IXGBE_SECURITY_NODE_UDP,
				IXGBE_SECURITY_NODE_ESP,
				IXGBE_SECURITY_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SECURITY_NODE_IPV6] = {
			.next = (size_t[]) {
				IXGBE_SECURITY_NODE_UDP,
				IXGBE_SECURITY_NODE_ESP,
				IXGBE_SECURITY_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SECURITY_NODE_UDP] = {
			.next = (size_t[]) {
				IXGBE_SECURITY_NODE_ESP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SECURITY_NODE_ESP] = {
			.next = (size_t[]) {
				IXGBE_SECURITY_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
ixgbe_flow_security_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct ixgbe_security_ctx *sec_ctx = (struct ixgbe_security_ctx *)ctx;
	struct ci_flow_actions parsed_actions;
	struct ci_flow_actions_check_param ap_param = {
		.allowed_types = (const enum rte_flow_action_type[]){
			/* only security is allowed here */
			RTE_FLOW_ACTION_TYPE_SECURITY,
			RTE_FLOW_ACTION_TYPE_END
		},
		.max_actions = 1,
	};
	const struct rte_flow_action_security *security;
	struct rte_security_session *session;
	const struct ixgbe_crypto_session *ic_session;
	int ret;

	/* validate attributes */
	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	/* parse requested actions */
	ret = ci_flow_check_actions(actions, &ap_param, &parsed_actions, error);
	if (ret)
		return ret;

	security = (const struct rte_flow_action_security *)parsed_actions.actions[0]->conf;

	if (security->security_session == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, parsed_actions.actions[0],
				"NULL security session");
	}

	/* cast away constness since we need to store the session pointer in the context */
	session = RTE_CAST_PTR(struct rte_security_session *, security->security_session);

	/* verify that the session is of a correct type */
	ic_session = SECURITY_GET_SESS_PRIV(session);
	if (ic_session->dev_data != ctx->dev_data) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, parsed_actions.actions[0],
				"Security session was created for a different device");
	}
	if (ic_session->op != IXGBE_OP_AUTHENTICATED_DECRYPTION) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, parsed_actions.actions[0],
				"Only authenticated decryption is supported");
	}
	sec_ctx->session = session;

	return 0;
}

static int
ixgbe_flow_security_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct ixgbe_security_ctx *security_ctx = (const struct ixgbe_security_ctx *)ctx;
	struct ixgbe_security_flow *security_flow = (struct ixgbe_security_flow *)flow;
	struct ixgbe_security_filter *filter = &security_flow->security;
	const struct ixgbe_crypto_session *ic_session;

	filter->session = security_ctx->session;
	ic_session = SECURITY_GET_SESS_PRIV(filter->session);
	filter->sa.key = ic_session->key;
	filter->sa.salt = ic_session->salt;
	filter->sa.spi = ic_session->spi;
	filter->sa.mode = IPSRXMOD_VALID | IPSRXMOD_PROTO | IPSRXMOD_DECRYPT;
	/* slot indices stay out of range until the flow is registered */
	filter->sa.sa_idx = IPSEC_MAX_SA_COUNT;
	filter->sa.ip_idx = IPSEC_MAX_RX_IP_COUNT;
	if (security_ctx->spec.is_ipv6) {
		filter->sa.dst_ip.type = IPv6;
		filter->sa.mode |= IPSRXMOD_IPV6;
		memcpy(filter->sa.dst_ip.ipv6,
				&security_ctx->spec.spec.ipv6.hdr.dst_addr,
				sizeof(filter->sa.dst_ip.ipv6));
	} else {
		filter->sa.dst_ip.type = IPv4;
		filter->sa.dst_ip.ipv4 = security_ctx->spec.spec.ipv4.hdr.dst_addr;
	}

	return 0;
}

static bool
ixgbe_security_ip_equal(const struct ipaddr *lhs, const struct ipaddr *rhs)
{
	if (lhs->type != rhs->type)
		return false;
	if (lhs->type == IPv4)
		return lhs->ipv4 == rhs->ipv4;

	return memcmp(lhs->ipv6, rhs->ipv6, sizeof(lhs->ipv6)) == 0;
}

/* two SA filters are the same rule if they match the same SPI and destination IP */
static bool
ixgbe_security_sa_key_equal(const struct ixgbe_crypto_rx_sa *lhs,
		const struct ixgbe_crypto_rx_sa *rhs)
{
	return lhs->spi == rhs->spi &&
			ixgbe_security_ip_equal(&lhs->dst_ip, &rhs->dst_ip);
}

static int
ixgbe_security_sa_slot_find(const struct ixgbe_security_priv *priv,
		const struct ixgbe_security_flow *security_flow)
{
	uint32_t free_idx = IPSEC_MAX_SA_COUNT;
	uint32_t idx;

	for (idx = 0; idx < IPSEC_MAX_SA_COUNT; idx++) {
		const struct ixgbe_security_flow *registered = priv->sa_slots[idx];

		if (registered == NULL) {
			if (free_idx == IPSEC_MAX_SA_COUNT)
				free_idx = idx;
			continue;
		}
		if (ixgbe_security_sa_key_equal(&registered->security.sa,
				&security_flow->security.sa))
			return -EEXIST;
	}
	if (free_idx == IPSEC_MAX_SA_COUNT)
		return -ENOSPC;

	return (int)free_idx;
}

static int
ixgbe_security_ip_slot_find(const struct ixgbe_security_priv *priv,
		const struct ipaddr *ip)
{
	uint32_t free_idx = IPSEC_MAX_RX_IP_COUNT;
	uint32_t idx;

	for (idx = 0; idx < IPSEC_MAX_RX_IP_COUNT; idx++) {
		if (priv->ip_slots[idx].use_count == 0) {
			if (free_idx == IPSEC_MAX_RX_IP_COUNT)
				free_idx = idx;
			continue;
		}
		if (ixgbe_security_ip_equal(&priv->ip_slots[idx].ip, ip))
			return (int)idx;
	}
	if (free_idx == IPSEC_MAX_RX_IP_COUNT)
		return -ENOSPC;

	return (int)free_idx;
}

static void
ixgbe_security_ip_slot_get(struct ixgbe_security_priv *priv, uint32_t idx,
		const struct ipaddr *ip)
{
	struct ixgbe_security_ip_slot *slot = &priv->ip_slots[idx];

	if (ci_usage_count_claim(&slot->use_count))
		slot->ip = *ip;
}

static void
ixgbe_security_ip_slot_put(struct ixgbe_security_priv *priv, uint32_t idx)
{
	struct ixgbe_security_ip_slot *slot = &priv->ip_slots[idx];

	if (ci_usage_count_release(&slot->use_count))
		*slot = (struct ixgbe_security_ip_slot){0};
}

static bool
ixgbe_security_flow_is_registered(const struct ixgbe_security_flow *security_flow)
{
	const struct ixgbe_security_priv *priv = security_flow->flow.flow.engine_priv;
	const struct ixgbe_crypto_rx_sa *sa = &security_flow->security.sa;

	return sa->ip_idx < IPSEC_MAX_RX_IP_COUNT &&
			sa->sa_idx < IPSEC_MAX_SA_COUNT &&
			priv->sa_slots[sa->sa_idx] == security_flow &&
			priv->ip_slots[sa->ip_idx].use_count != 0 &&
			ixgbe_security_ip_equal(&priv->ip_slots[sa->ip_idx].ip,
					&sa->dst_ip);
}

static int
ixgbe_flow_security_flow_register(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_security_flow *security_flow = (struct ixgbe_security_flow *)flow;
	struct ixgbe_security_priv *priv = flow->engine_priv;
	struct ixgbe_security_filter *filter = &security_flow->security;
	int sa_idx, ip_idx, ret;

	sa_idx = ixgbe_security_sa_slot_find(priv, security_flow);
	if (sa_idx == -EEXIST) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ingress security filter already exists");
	}
	if (sa_idx == -ENOSPC) {
		return rte_flow_error_set(error, ENOSPC,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ingress security SA table is full");
	}
	if (sa_idx < 0) {
		return rte_flow_error_set(error, -sa_idx,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to reserve ingress security SA slot");
	}

	ip_idx = ixgbe_security_ip_slot_find(priv, &filter->sa.dst_ip);
	if (ip_idx == -ENOSPC) {
		return rte_flow_error_set(error, ENOSPC,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ingress security IP table is full");
	}
	if (ip_idx < 0) {
		return rte_flow_error_set(error, -ip_idx,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to reserve ingress security IP slot");
	}

	ret = ixgbe_crypto_session_acquire(filter->session);
	if (ret != 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to acquire ingress security session");
	}

	/* every resource is acquired by this point, so nothing below may fail */
	filter->sa.sa_idx = sa_idx;
	filter->sa.ip_idx = ip_idx;
	priv->sa_slots[sa_idx] = security_flow;
	ixgbe_security_ip_slot_get(priv, ip_idx, &filter->sa.dst_ip);

	return 0;
}

static int
ixgbe_flow_security_flow_unregister(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_security_flow *security_flow = (struct ixgbe_security_flow *)flow;
	struct ixgbe_security_priv *priv = flow->engine_priv;
	struct ixgbe_security_filter *filter = &security_flow->security;
	int ret;

	if (!ixgbe_security_flow_is_registered(security_flow)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Ingress security flow registration is invalid");
	}
	ret = ixgbe_crypto_session_release(filter->session);
	if (ret != 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to release ingress security session");
	}

	priv->sa_slots[filter->sa.sa_idx] = NULL;
	ixgbe_security_ip_slot_put(priv, filter->sa.ip_idx);
	return 0;
}

static int
ixgbe_flow_security_flow_install(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_security_flow *security_flow = (struct ixgbe_security_flow *)flow;

	if (!ixgbe_security_flow_is_registered(security_flow)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Ingress security flow registration is invalid");
	}

	ixgbe_crypto_install_rx_sa(flow->dev_data, &security_flow->security.sa);
	return 0;
}

static int
ixgbe_flow_security_flow_uninstall(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_security_flow *security_flow = (struct ixgbe_security_flow *)flow;
	struct ixgbe_security_priv *priv = flow->engine_priv;
	const struct ixgbe_crypto_rx_sa *sa = &security_flow->security.sa;

	if (!ixgbe_security_flow_is_registered(security_flow)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Ingress security flow registration is invalid");
	}

	ixgbe_crypto_uninstall_rx_sa(flow->dev_data, sa,
			ci_usage_count_is_last(&priv->ip_slots[sa->ip_idx].use_count));
	return 0;
}

static int
ixgbe_flow_security_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv __rte_unused)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);

	if (hw->mac.type == ixgbe_mac_82599EB ||
			hw->mac.type == ixgbe_mac_X540 ||
			hw->mac.type == ixgbe_mac_X550 ||
			hw->mac.type == ixgbe_mac_X550EM_x ||
			hw->mac.type == ixgbe_mac_X550EM_a ||
			hw->mac.type == ixgbe_mac_E610)
		return 0;

	return -ENOTSUP;
}

static const struct ci_flow_engine_ops ixgbe_security_ops = {
	.engine_init = ixgbe_flow_security_engine_init,
	.ctx_init = ixgbe_flow_security_ctx_init,
	.ctx_to_flow = ixgbe_flow_security_ctx_to_flow,
	.flow_register = ixgbe_flow_security_flow_register,
	.flow_unregister = ixgbe_flow_security_flow_unregister,
	.flow_install = ixgbe_flow_security_flow_install,
	.flow_uninstall = ixgbe_flow_security_flow_uninstall,
};

const struct ci_flow_engine ixgbe_security_flow_engine = {
	.name = "ixgbe_security",
	.ctx_size = sizeof(struct ixgbe_security_ctx),
	.flow_size = sizeof(struct ixgbe_security_flow),
	.priv_size = sizeof(struct ixgbe_security_priv),
	.ops = &ixgbe_security_ops,
	.graph = &ixgbe_security_graph,
};
