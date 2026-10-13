/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include <rte_common.h>
#include <rte_flow.h>
#include <flow_graph.h>
#include <rte_ether.h>
#include <rte_hash.h>

#include "ixgbe_ethdev.h"
#include "ixgbe_flow.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"
#include "../common/flow_engine.h"

struct ixgbe_fdir_flow {
	struct rte_flow flow;
	struct ixgbe_fdir_rule rule;
};

/* both fdir engines get their own priv, all pointing at the same shared state */
struct ixgbe_fdir_priv {
	struct ixgbe_fdir_state *state;
	/* hardware capabilities, resolved once at engine_init */
	bool supports_sctp_ports;
	bool supports_mac_vlan_mode;
};

struct ixgbe_fdir_ctx {
	struct ci_flow_engine_ctx base;
	struct ixgbe_fdir_rule rule;
	bool supports_sctp_ports;
	bool supports_mac_vlan_mode;
	const struct rte_flow_action *fwd_action;
	const struct rte_flow_action *aux_action;
};

#define IXGBE_FDIR_VLAN_TCI_MASK	rte_cpu_to_be_16(0xEFFF)
#define NVGRE_FLAGS 0x2000
#define NVGRE_PROTOCOL 0x6558

/**
 * FDIR normal graph implementation
 * Pattern: START -> [ETH] -> (IPv4|IPv6) -> [TCP|UDP|SCTP] -> [RAW] -> END
 * Pattern: START -> ETH -> VLAN -> END
 */

enum ixgbe_fdir_normal_node_id {
	IXGBE_FDIR_NORMAL_NODE_START = FLOW_GRAPH_NODE_FIRST,
	IXGBE_FDIR_NORMAL_NODE_ETH,
	IXGBE_FDIR_NORMAL_NODE_VLAN,
	IXGBE_FDIR_NORMAL_NODE_IPV4,
	IXGBE_FDIR_NORMAL_NODE_IPV6,
	IXGBE_FDIR_NORMAL_NODE_TCP,
	IXGBE_FDIR_NORMAL_NODE_UDP,
	IXGBE_FDIR_NORMAL_NODE_SCTP,
	IXGBE_FDIR_NORMAL_NODE_RAW,
	IXGBE_FDIR_NORMAL_NODE_END,
	IXGBE_FDIR_NORMAL_NODE_MAX,
};

static int
ixgbe_validate_fdir_normal_eth(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct ixgbe_fdir_ctx *fdir_ctx = (const struct ixgbe_fdir_ctx *)ctx;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);

	if (eth_mask == NULL)
		return 0;

	/* we cannot have ETH item in signature mode */
	if (fdir_ctx->rule.mode == RTE_FDIR_MODE_SIGNATURE) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"ETH item not supported in signature mode");
	}
	/* ETH item selects mac-vlan mode, only supported on x550/E610 */
	if (!fdir_ctx->supports_mac_vlan_mode) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"ETH item not supported on this hardware");
	}
	/* ethertype isn't supported by FDIR */
	if (!CI_FIELD_IS_ZERO(&eth_mask->hdr.ether_type)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Ethertype filtering not supported");
	}
	/* source address mask must be all zeroes */
	if (!CI_FIELD_IS_ZERO(&eth_mask->hdr.src_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Source MAC filtering not supported");
	}
	/* destination address mask must be all ones */
	if (!CI_FIELD_IS_MASKED(&eth_mask->hdr.dst_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Destination MAC filtering must be exact match");
	}
	return 0;
}

static int
ixgbe_process_fdir_normal_eth(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);

	if (eth_mask == NULL)
		return 0;

	/* copy dst MAC */
	rule->b_spec = TRUE;
	memcpy(rule->ixgbe_fdir.formatted.inner_mac, eth_spec->hdr.dst_addr.addr_bytes,
			RTE_ETHER_ADDR_LEN);

	/* set tunnel type */
	rule->mode = RTE_FDIR_MODE_PERFECT_MAC_VLAN;
	/* when no VLAN specified, set full mask */
	rule->b_mask = TRUE;
	rule->mask.vlan_tci_mask = IXGBE_FDIR_VLAN_TCI_MASK;

	return 0;
}

static int
ixgbe_process_fdir_normal_vlan(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->mask.vlan_tci_mask = vlan_mask->hdr.vlan_tci;
	rule->mask.vlan_tci_mask &= IXGBE_FDIR_VLAN_TCI_MASK;
	rule->ixgbe_fdir.formatted.vlan_id = vlan_spec->hdr.vlan_tci;

	return 0;
}

static int
ixgbe_validate_fdir_normal_ipv4(const void *ctx,
			    const struct rte_flow_item *item,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_mask = ci_flow_item_mask(item);
	const struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	if (rule->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv4 not supported with ETH/VLAN items");
	}

	if (ipv4_mask->hdr.version_ihl ||
	    ipv4_mask->hdr.type_of_service ||
	    ipv4_mask->hdr.total_length ||
	    ipv4_mask->hdr.packet_id ||
	    ipv4_mask->hdr.fragment_offset ||
	    ipv4_mask->hdr.time_to_live ||
	    ipv4_mask->hdr.next_proto_id ||
	    ipv4_mask->hdr.hdr_checksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst addresses supported");
	}

	return 0;
}

static int
ixgbe_process_fdir_normal_ipv4(void *ctx,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->ixgbe_fdir.formatted.flow_type = IXGBE_ATR_FLOW_TYPE_IPV4;

	rule->b_spec = TRUE;
	rule->ixgbe_fdir.formatted.dst_ip[0] = ipv4_spec->hdr.dst_addr;
	rule->ixgbe_fdir.formatted.src_ip[0] = ipv4_spec->hdr.src_addr;

	rule->b_mask = TRUE;
	rule->mask.dst_ipv4_mask = ipv4_mask->hdr.dst_addr;
	rule->mask.src_ipv4_mask = ipv4_mask->hdr.src_addr;

	return 0;
}

static int
ixgbe_validate_fdir_normal_ipv6(const void *ctx,
			    const struct rte_flow_item *item,
			    struct rte_flow_error *error)
{
	const struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_ipv6 *ipv6_mask = ci_flow_item_mask(item);
	const struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;
	uint8_t i;

	if (rule->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv6 not supported with ETH/VLAN items");
	}

	if (rule->mode != RTE_FDIR_MODE_SIGNATURE) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv6 only supported in signature mode");
	}

	if (ipv6_mask->hdr.vtc_flow ||
	    ipv6_mask->hdr.payload_len ||
	    ipv6_mask->hdr.proto ||
	    ipv6_mask->hdr.hop_limits) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst addresses supported");
	}

	for (i = 0; i < sizeof(ipv6_mask->hdr.src_addr); i++) {
		if (!CI_FIELD_IS_ZERO_OR_MASKED(&ipv6_mask->hdr.src_addr.a[i]) ||
		    !CI_FIELD_IS_ZERO_OR_MASKED(&ipv6_mask->hdr.dst_addr.a[i])) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Partial address byte masks not supported");
		}
	}

	return 0;
}

static int
ixgbe_process_fdir_normal_ipv6(void *ctx,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_ipv6 *ipv6_spec = item->spec;
	const struct rte_flow_item_ipv6 *ipv6_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;
	uint8_t j;

	rule->ixgbe_fdir.formatted.flow_type = IXGBE_ATR_FLOW_TYPE_IPV6;

	rule->b_spec = TRUE;
	memcpy(rule->ixgbe_fdir.formatted.src_ip, &ipv6_spec->hdr.src_addr,
			sizeof(struct rte_ipv6_addr));
	memcpy(rule->ixgbe_fdir.formatted.dst_ip, &ipv6_spec->hdr.dst_addr,
			sizeof(struct rte_ipv6_addr));

	rule->b_mask = TRUE;
	for (j = 0; j < sizeof(struct rte_ipv6_addr); j++) {
		if (ipv6_mask->hdr.src_addr.a[j] == 0)
			rule->mask.src_ipv6_mask &= ~RTE_BIT32(j);
		if (ipv6_mask->hdr.dst_addr.a[j] == 0)
			rule->mask.dst_ipv6_mask &= ~RTE_BIT32(j);
	}

	return 0;
}

static int
ixgbe_validate_fdir_normal_tcp(const void *ctx,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_mask = ci_flow_item_mask(item);
	const struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	if (rule->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"TCP not supported with ETH/VLAN items");
	}

	if (tcp_mask == NULL)
		return 0;

	if (tcp_mask->hdr.sent_seq ||
	    tcp_mask->hdr.recv_ack ||
	    tcp_mask->hdr.data_off ||
	    tcp_mask->hdr.tcp_flags ||
	    tcp_mask->hdr.rx_win ||
	    tcp_mask->hdr.cksum ||
	    tcp_mask->hdr.tcp_urp) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst ports supported");
	}

	return 0;
}

static int
ixgbe_process_fdir_normal_tcp(void *ctx,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_tcp *tcp_spec = item->spec;
	const struct rte_flow_item_tcp *tcp_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->ixgbe_fdir.formatted.flow_type |= IXGBE_ATR_L4TYPE_TCP;

	if (tcp_mask == NULL)
		return 0;

	rule->b_mask = TRUE;
	rule->mask.src_port_mask = tcp_mask->hdr.src_port;
	rule->mask.dst_port_mask = tcp_mask->hdr.dst_port;

	rule->b_spec = TRUE;
	rule->ixgbe_fdir.formatted.src_port = tcp_spec->hdr.src_port;
	rule->ixgbe_fdir.formatted.dst_port = tcp_spec->hdr.dst_port;

	return 0;
}

static int
ixgbe_validate_fdir_normal_udp(const void *ctx,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *udp_mask = ci_flow_item_mask(item);
	const struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	if (rule->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"UDP not supported with ETH/VLAN items");
	}

	if (udp_mask == NULL)
		return 0;

	if (udp_mask->hdr.dgram_len ||
	    udp_mask->hdr.dgram_cksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only src/dst ports supported");
	}

	return 0;
}

static int
ixgbe_process_fdir_normal_udp(void *ctx,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->ixgbe_fdir.formatted.flow_type |= IXGBE_ATR_L4TYPE_UDP;

	if (udp_mask == NULL)
		return 0;

	rule->b_mask = TRUE;
	rule->mask.src_port_mask = udp_mask->hdr.src_port;
	rule->mask.dst_port_mask = udp_mask->hdr.dst_port;

	rule->b_spec = TRUE;
	rule->ixgbe_fdir.formatted.src_port = udp_spec->hdr.src_port;
	rule->ixgbe_fdir.formatted.dst_port = udp_spec->hdr.dst_port;

	return 0;
}

static int
ixgbe_validate_fdir_normal_sctp(const void *ctx,
			    const struct rte_flow_item *item,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_sctp *sctp_mask = ci_flow_item_mask(item);
	const struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	if (rule->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"SCTP not supported with ETH/VLAN items");
	}

	if (sctp_mask == NULL)
		return 0;

	/* Tag and checksum not supported */
	if (sctp_mask->hdr.tag ||
	    sctp_mask->hdr.cksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"SCTP tag/cksum not supported");
	}

	/* SCTP port filtering is only supported on some hardware. */
	if (!fdir_ctx->supports_sctp_ports &&
	    (sctp_mask->hdr.src_port || sctp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"SCTP port filtering not supported by hardware");
	}

	return 0;
}

static int
ixgbe_process_fdir_normal_sctp(void *ctx,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_sctp *sctp_spec = item->spec;
	const struct rte_flow_item_sctp *sctp_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->ixgbe_fdir.formatted.flow_type |= IXGBE_ATR_L4TYPE_SCTP;

	if (sctp_mask == NULL)
		return 0;

	rule->b_mask = TRUE;
	rule->mask.src_port_mask = sctp_mask->hdr.src_port;
	rule->mask.dst_port_mask = sctp_mask->hdr.dst_port;

	rule->b_spec = TRUE;
	rule->ixgbe_fdir.formatted.src_port = sctp_spec->hdr.src_port;
	rule->ixgbe_fdir.formatted.dst_port = sctp_spec->hdr.dst_port;

	return 0;
}

static int
ixgbe_validate_fdir_normal_raw(const void *ctx __rte_unused,
			   const struct rte_flow_item *item,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_raw *raw_spec;
	const struct rte_flow_item_raw *raw_mask;

	raw_spec = item->spec;
	raw_mask = item->mask;

	if (raw_mask == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW item requires an explicit mask");
	}

	if (raw_spec->pattern == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid RAW spec");
	}

	if (raw_mask->pattern == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid RAW mask");
	}

	if (raw_mask->length != raw_spec->length &&
			raw_mask->length != 0xffff) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid RAW mask");
	}

	if (raw_mask->relative != 0x1 ||
	    raw_mask->search != 0x1 ||
	    raw_mask->reserved != 0x0 ||
	    (uint32_t)raw_mask->offset != 0xffffffff ||
	    raw_mask->limit != 0xffff) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid RAW mask");
	}

	if (raw_spec->relative != 0 ||
	    raw_spec->search != 0 ||
	    raw_spec->reserved != 0 ||
	    raw_spec->offset > IXGBE_MAX_FLX_SOURCE_OFF ||
	    raw_spec->offset % 2 ||
	    raw_spec->limit != 0 ||
	    raw_spec->length != 2 ||
	    (raw_spec->pattern[0] == 0xff &&
	     raw_spec->pattern[1] == 0xff)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid RAW spec");
	}

	if (raw_mask->pattern[0] != 0xff ||
	    raw_mask->pattern[1] != 0xff) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW pattern must be fully masked");
	}

	return 0;
}

static int
ixgbe_process_fdir_normal_raw(void *ctx,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_raw *raw_spec = item->spec;
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->b_spec = TRUE;
	rule->ixgbe_fdir.formatted.flex_bytes =
		(((uint16_t)raw_spec->pattern[1]) << 8) | raw_spec->pattern[0];
	rule->flex_bytes_offset = raw_spec->offset;

	rule->b_mask = TRUE;
	rule->mask.flex_bytes_mask = 0xffff;

	return 0;
}

static int
ixgbe_process_fdir_normal_end(void *ctx, const struct rte_flow_item *item __rte_unused,
			  struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;

	/* check if we need L4 protocol */
	if (fdir_ctx->rule.ixgbe_fdir.formatted.flow_type & IXGBE_ATR_L4TYPE_MASK)
		fdir_ctx->rule.mask.l4_proto_match = 1;

	return 0;
}

static const struct flow_graph ixgbe_fdir_normal_graph = {
	.ignore_nodes = (enum rte_flow_item_type[]) {
		RTE_FLOW_ITEM_TYPE_FUZZY,
		RTE_FLOW_ITEM_TYPE_END,
	},
	.nodes = (struct flow_graph_node[]) {
		[IXGBE_FDIR_NORMAL_NODE_START] = {
			.name = "START",
		},
		[IXGBE_FDIR_NORMAL_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.validate = ixgbe_validate_fdir_normal_eth,
			.process = ixgbe_process_fdir_normal_eth,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_NORMAL_NODE_VLAN] = {
			.name = "VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.process = ixgbe_process_fdir_normal_vlan,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_NORMAL_NODE_IPV4] = {
			.name = "IPV4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.validate = ixgbe_validate_fdir_normal_ipv4,
			.process = ixgbe_process_fdir_normal_ipv4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_NORMAL_NODE_IPV6] = {
			.name = "IPV6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.validate = ixgbe_validate_fdir_normal_ipv6,
			.process = ixgbe_process_fdir_normal_ipv6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_NORMAL_NODE_TCP] = {
			.name = "TCP",
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.validate = ixgbe_validate_fdir_normal_tcp,
			.process = ixgbe_process_fdir_normal_tcp,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_NORMAL_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.validate = ixgbe_validate_fdir_normal_udp,
			.process = ixgbe_process_fdir_normal_udp,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_NORMAL_NODE_SCTP] = {
			.name = "SCTP",
			.type = RTE_FLOW_ITEM_TYPE_SCTP,
			.validate = ixgbe_validate_fdir_normal_sctp,
			.process = ixgbe_process_fdir_normal_sctp,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_NORMAL_NODE_RAW] = {
			.name = "RAW",
			.type = RTE_FLOW_ITEM_TYPE_RAW,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_fdir_normal_raw,
			.process = ixgbe_process_fdir_normal_raw,
		},
		[IXGBE_FDIR_NORMAL_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
			.process = ixgbe_process_fdir_normal_end,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[IXGBE_FDIR_NORMAL_NODE_START] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_ETH,
				IXGBE_FDIR_NORMAL_NODE_IPV4,
				IXGBE_FDIR_NORMAL_NODE_IPV6,
				IXGBE_FDIR_NORMAL_NODE_TCP,
				IXGBE_FDIR_NORMAL_NODE_UDP,
				IXGBE_FDIR_NORMAL_NODE_SCTP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_ETH] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_VLAN,
				IXGBE_FDIR_NORMAL_NODE_IPV4,
				IXGBE_FDIR_NORMAL_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_VLAN] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_IPV4] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_TCP,
				IXGBE_FDIR_NORMAL_NODE_UDP,
				IXGBE_FDIR_NORMAL_NODE_SCTP,
				IXGBE_FDIR_NORMAL_NODE_RAW,
				IXGBE_FDIR_NORMAL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_IPV6] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_TCP,
				IXGBE_FDIR_NORMAL_NODE_UDP,
				IXGBE_FDIR_NORMAL_NODE_SCTP,
				IXGBE_FDIR_NORMAL_NODE_RAW,
				IXGBE_FDIR_NORMAL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_TCP] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_RAW,
				IXGBE_FDIR_NORMAL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_UDP] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_RAW,
				IXGBE_FDIR_NORMAL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_SCTP] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_RAW,
				IXGBE_FDIR_NORMAL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_NORMAL_NODE_RAW] = {
			.next = (size_t[]) {
				IXGBE_FDIR_NORMAL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

/**
 * FDIR tunnel graph implementation (VxLAN and NVGRE)
 * Pattern: START -> [OUTER_ETH] -> (OUTER_IPv4|OUTER_IPv6) -> [UDP] -> (VXLAN|NVGRE) -> INNER_ETH -> [VLAN] -> END
 * VxLAN:  START -> [OUTER_ETH] -> (OUTER_IPv4|OUTER_IPv6) -> UDP -> VXLAN -> INNER_ETH -> [VLAN] -> END
 * NVGRE:  START -> [OUTER_ETH] -> (OUTER_IPv4|OUTER_IPv6) -> NVGRE -> INNER_ETH -> [VLAN] -> END
 */

enum ixgbe_fdir_tunnel_node_id {
	IXGBE_FDIR_TUNNEL_NODE_START = FLOW_GRAPH_NODE_FIRST,
	IXGBE_FDIR_TUNNEL_NODE_OUTER_ETH,
	IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV4,
	IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV6,
	IXGBE_FDIR_TUNNEL_NODE_UDP,
	IXGBE_FDIR_TUNNEL_NODE_VXLAN,
	IXGBE_FDIR_TUNNEL_NODE_NVGRE,
	IXGBE_FDIR_TUNNEL_NODE_INNER_ETH,
	IXGBE_FDIR_TUNNEL_NODE_INNER_IPV4,
	IXGBE_FDIR_TUNNEL_NODE_VLAN,
	IXGBE_FDIR_TUNNEL_NODE_END,
	IXGBE_FDIR_TUNNEL_NODE_MAX,
};

static int
ixgbe_validate_fdir_tunnel_vxlan(const void *ctx __rte_unused,
				 const struct rte_flow_item *item,
				 struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan *vxlan_mask = ci_flow_item_mask(item);

	if (vxlan_mask->hdr.flags) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"VxLAN flags must be masked");
	}

	if (!CI_FIELD_IS_ZERO_OR_MASKED(&vxlan_mask->hdr.vni)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Partial VNI mask not supported");
	}

	return 0;
}

static int
ixgbe_process_fdir_tunnel_vxlan(void *ctx,
				const struct rte_flow_item *item,
				struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_vxlan *vxlan_spec = item->spec;
	const struct rte_flow_item_vxlan *vxlan_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->ixgbe_fdir.formatted.tunnel_type = IXGBE_FDIR_VXLAN_TUNNEL_TYPE;

	rule->b_spec = TRUE;
	memcpy(((uint8_t *)&rule->ixgbe_fdir.formatted.tni_vni), vxlan_spec->hdr.vni,
			RTE_DIM(vxlan_spec->hdr.vni));

	rule->b_mask = TRUE;
	rule->mask.tunnel_type_mask = 1;
	memcpy(&rule->mask.tunnel_id_mask, vxlan_mask->hdr.vni, RTE_DIM(vxlan_mask->hdr.vni));

	return 0;
}

static int
ixgbe_validate_fdir_tunnel_nvgre(const void *ctx __rte_unused,
				 const struct rte_flow_item *item,
				 struct rte_flow_error *error)
{
	const struct rte_flow_item_nvgre *nvgre_spec = item->spec;
	const struct rte_flow_item_nvgre *nvgre_mask = ci_flow_item_mask(item);

	if (nvgre_mask->flow_id) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"NVGRE flow ID must not be masked");
	}

	if (!CI_FIELD_IS_ZERO_OR_MASKED(&nvgre_mask->protocol)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"NVGRE protocol must be fully masked or unmasked");
	}

	if (!CI_FIELD_IS_ZERO_OR_MASKED(&nvgre_mask->c_k_s_rsvd0_ver)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"NVGRE flags must be fully masked or unmasked");
	}

	if (!CI_FIELD_IS_ZERO_OR_MASKED(&nvgre_mask->tni)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Partial TNI mask not supported");
	}

	if (nvgre_mask->c_k_s_rsvd0_ver &&
	    nvgre_spec->c_k_s_rsvd0_ver != rte_cpu_to_be_16(NVGRE_FLAGS)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"NVGRE flags must be 0x2000");
	}
	if (nvgre_mask->protocol &&
	    nvgre_spec->protocol != rte_cpu_to_be_16(NVGRE_PROTOCOL)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"NVGRE protocol must be 0x6558");
	}

	return 0;
}

static int
ixgbe_process_fdir_tunnel_nvgre(void *ctx,
				const struct rte_flow_item *item,
				struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_nvgre *nvgre_spec = item->spec;
	const struct rte_flow_item_nvgre *nvgre_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->ixgbe_fdir.formatted.tunnel_type = IXGBE_FDIR_NVGRE_TUNNEL_TYPE;

	rule->b_spec = TRUE;
	memcpy(&rule->ixgbe_fdir.formatted.tni_vni,
			nvgre_spec->tni, RTE_DIM(nvgre_spec->tni));

	rule->b_mask = TRUE;
	rule->mask.tunnel_type_mask = 1;
	memcpy(&rule->mask.tunnel_id_mask, nvgre_mask->tni, RTE_DIM(nvgre_mask->tni));
	rule->mask.tunnel_id_mask <<= 8;
	return 0;
}

static int
ixgbe_validate_fdir_tunnel_inner_eth(const void *ctx __rte_unused,
				     const struct rte_flow_item *item,
				     struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);
	uint8_t i;

	if (eth_mask->hdr.ether_type != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Ether type mask not supported");
	}

	/* src addr must not be masked */
	if (!CI_FIELD_IS_ZERO(&eth_mask->hdr.src_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Masking not supported for src MAC address");
	}

	/* Each destination MAC byte must be fully masked or fully unmasked. */
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		if (!CI_FIELD_IS_ZERO_OR_MASKED(&eth_mask->hdr.dst_addr.addr_bytes[i])) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Partial dst MAC byte masks not supported");
		}
	}

	return 0;
}

static int
ixgbe_process_fdir_tunnel_inner_eth(void *ctx,
				    const struct rte_flow_item *item,
				    struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;
	uint8_t j;

	memcpy(&rule->ixgbe_fdir.formatted.inner_mac,
			eth_spec->hdr.dst_addr.addr_bytes,
			RTE_ETHER_ADDR_LEN);

	rule->b_mask = TRUE;
	rule->mask.mac_addr_byte_mask = 0;
	for (j = 0; j < RTE_ETHER_ADDR_LEN; j++) {
		if (eth_mask->hdr.dst_addr.addr_bytes[j] == 0xFF) {
			rule->mask.mac_addr_byte_mask |= RTE_BIT32(j);
		}
	}

	/* When no vlan, considered as full mask. */
	rule->mask.vlan_tci_mask = IXGBE_FDIR_VLAN_TCI_MASK;

	return 0;
}

static int
ixgbe_process_fdir_tunnel_vlan(void *ctx,
			       const struct rte_flow_item *item,
			       struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_fdir_ctx *fdir_ctx = ctx;
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	rule->mask.vlan_tci_mask = vlan_mask->hdr.vlan_tci;
	rule->mask.vlan_tci_mask &= IXGBE_FDIR_VLAN_TCI_MASK;
	rule->ixgbe_fdir.formatted.vlan_id = vlan_spec->hdr.vlan_tci;

	return 0;
}

static const struct flow_graph ixgbe_fdir_tunnel_graph = {
	.nodes = (struct flow_graph_node[]) {
		[IXGBE_FDIR_TUNNEL_NODE_START] = {
			.name = "START",
		},
		[IXGBE_FDIR_TUNNEL_NODE_OUTER_ETH] = {
			.name = "OUTER_ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV4] = {
			.name = "OUTER_IPV4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV6] = {
			.name = "OUTER_IPV6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_FDIR_TUNNEL_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_FDIR_TUNNEL_NODE_VXLAN] = {
			.name = "VXLAN",
			.type = RTE_FLOW_ITEM_TYPE_VXLAN,
			.validate = ixgbe_validate_fdir_tunnel_vxlan,
			.process = ixgbe_process_fdir_tunnel_vxlan,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_TUNNEL_NODE_NVGRE] = {
			.name = "NVGRE",
			.type = RTE_FLOW_ITEM_TYPE_NVGRE,
			.validate = ixgbe_validate_fdir_tunnel_nvgre,
			.process = ixgbe_process_fdir_tunnel_nvgre,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_TUNNEL_NODE_INNER_ETH] = {
			.name = "INNER_ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.validate = ixgbe_validate_fdir_tunnel_inner_eth,
			.process = ixgbe_process_fdir_tunnel_inner_eth,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_TUNNEL_NODE_INNER_IPV4] = {
			.name = "INNER_IPV4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_FDIR_TUNNEL_NODE_VLAN] = {
			.name = "VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.process = ixgbe_process_fdir_tunnel_vlan,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
		},
		[IXGBE_FDIR_TUNNEL_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[IXGBE_FDIR_TUNNEL_NODE_START] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_OUTER_ETH,
				IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV4,
				IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV6,
				IXGBE_FDIR_TUNNEL_NODE_UDP,
				IXGBE_FDIR_TUNNEL_NODE_VXLAN,
				IXGBE_FDIR_TUNNEL_NODE_NVGRE,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_OUTER_ETH] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV4,
				IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV4] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_UDP,
				IXGBE_FDIR_TUNNEL_NODE_NVGRE,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_OUTER_IPV6] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_UDP,
				IXGBE_FDIR_TUNNEL_NODE_NVGRE,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_UDP] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_VXLAN,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_VXLAN] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_INNER_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_NVGRE] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_INNER_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_INNER_ETH] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_VLAN,
				IXGBE_FDIR_TUNNEL_NODE_INNER_IPV4,
				IXGBE_FDIR_TUNNEL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_INNER_IPV4] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_FDIR_TUNNEL_NODE_VLAN] = {
			.next = (size_t[]) {
				IXGBE_FDIR_TUNNEL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static inline uint8_t
signature_match(const struct rte_flow_item *item)
{
	const struct rte_flow_item_fuzzy *spec, *last, *mask;
	uint32_t sh, lh, mh;

	spec = item->spec;
	last = item->last;
	mask = item->mask;

	if (spec == NULL || mask == NULL)
		return 0;

	sh = spec->thresh;

	if (last == NULL)
		lh = sh;
	else
		lh = last->thresh;

	mh = mask->thresh;
	sh = sh & mh;
	lh = lh & mh;

	/*
	 * A fuzzy item selects signature mode only when the masked threshold range
	 * is non-empty. Otherwise this stays a perfect-match rule.
	 */
	if (!sh || sh > lh)
		return 0;

	return 1;
}

/* pre-parse pattern to determine if this is a signature or perfect match rule */
static int
ixgbe_fdir_pattern_parse(struct ci_flow_engine_ctx *ctx,
		const struct rte_flow_item pattern[],
		struct rte_flow_error *error)
{
	struct ixgbe_fdir_ctx *fdir_ctx = (struct ixgbe_fdir_ctx *)ctx;
	const struct rte_flow_item *item;
	bool found = false;

	fdir_ctx->rule.mode = RTE_FDIR_MODE_PERFECT;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type != RTE_FLOW_ITEM_TYPE_FUZZY)
			continue;

		if (found) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Multiple FUZZY items not supported");
		}
		found = true;

		if (signature_match(item))
			fdir_ctx->rule.mode = RTE_FDIR_MODE_SIGNATURE;

		break;
	}

	return 0;
}

static int
ixgbe_fdir_actions_check(const struct ci_flow_actions *parsed_actions,
	const struct ci_flow_actions_check_param *param __rte_unused,
	struct rte_flow_error *error)
{
	const enum rte_flow_action_type fwd_actions[] = {
		RTE_FLOW_ACTION_TYPE_QUEUE,
		RTE_FLOW_ACTION_TYPE_DROP,
		RTE_FLOW_ACTION_TYPE_END
	};
	const struct rte_flow_action *action;

	/* do the generic checks first */
	int ret = ixgbe_flow_actions_check(parsed_actions, param, error);
	if (ret)
		return ret;

	/* first action must be a forwarding action */
	action = parsed_actions->actions[0];
	if (!ci_flow_action_type_in_list(action->type, fwd_actions)) {
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					  action, "First action must be QUEUE or DROP");
	}
	/* second action, if specified, must not be a forwarding action */
	action = parsed_actions->actions[1];
	if (action != NULL && ci_flow_action_type_in_list(action->type, fwd_actions)) {
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					  action, "Conflicting actions");
	}
	return 0;
}

static void
ixgbe_flow_normalize_rule(struct ixgbe_fdir_rule *rule)
{
	union ixgbe_atr_input *input = &rule->ixgbe_fdir;
	const struct ixgbe_hw_fdir_mask *mask = &rule->mask;
	bool need_ipv6, need_ipv4;
	uint8_t j;

	input->formatted.vlan_id &= mask->vlan_tci_mask;
	if (rule->mode == RTE_FDIR_MODE_PERFECT_TUNNEL) {
		for (j = 0; j < RTE_DIM(input->formatted.inner_mac); j++) {
			if ((mask->mac_addr_byte_mask & RTE_BIT32(j)) == 0)
				input->formatted.inner_mac[j] = 0;
		}
	}

	need_ipv4 = (rule->mode == RTE_FDIR_MODE_PERFECT ||
			rule->mode == RTE_FDIR_MODE_SIGNATURE) &&
		(input->formatted.flow_type & ~IXGBE_ATR_L4TYPE_MASK) ==
		IXGBE_ATR_FLOW_TYPE_IPV4;
	need_ipv6 = rule->mode == RTE_FDIR_MODE_SIGNATURE &&
		(input->formatted.flow_type & ~IXGBE_ATR_L4TYPE_MASK) ==
		IXGBE_ATR_FLOW_TYPE_IPV6;

	if (need_ipv4) {
		input->formatted.src_ip[0] &= mask->src_ipv4_mask;
		input->formatted.dst_ip[0] &= mask->dst_ipv4_mask;
	} else if (need_ipv6) {
		uint8_t *src_ip = (uint8_t *)input->formatted.src_ip;
		uint8_t *dst_ip = (uint8_t *)input->formatted.dst_ip;

		for (j = 0; j < sizeof(struct rte_ipv6_addr); j++) {
			if ((mask->src_ipv6_mask & RTE_BIT32(j)) == 0)
				src_ip[j] = 0;
			if ((mask->dst_ipv6_mask & RTE_BIT32(j)) == 0)
				dst_ip[j] = 0;
		}
	}
	input->formatted.src_port &= mask->src_port_mask;
	input->formatted.dst_port &= mask->dst_port_mask;

	if (rule->mode == RTE_FDIR_MODE_PERFECT_TUNNEL && mask->tunnel_id_mask == 0)
		input->formatted.tni_vni = 0;
	input->formatted.flex_bytes &= mask->flex_bytes_mask;
}

static int
ixgbe_flow_fdir_ctx_finalize(struct ci_flow_engine_ctx *ctx, struct rte_flow_error *error)
{
	struct ixgbe_fdir_ctx *fdir_ctx = (struct ixgbe_fdir_ctx *)ctx;
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;

	/* DROP action should not be used with signature matches */
	if ((fdir_ctx->rule.mode == RTE_FDIR_MODE_SIGNATURE) &&
	    (fdir_ctx->fwd_action->type == RTE_FLOW_ACTION_TYPE_DROP)) {
		return rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"DROP action not allowed with signature mode");
	}

	/* rules without spec aren't allowed */
	if (!fdir_ctx->rule.b_spec) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Rule spec cannot be empty");
	}
	/*
	 * We support masking, but we unconditionally copy spec values from flow
	 * item into the rule. This makes it so that a rule that uses the same
	 * mask but different spec value may still be effectively the same rule,
	 * because spec value *after masking* is the same as in other rule.
	 *
	 * Therefore we need to make sure that spec bits do not contain anything
	 * that wasn't in the mask, so that later when we deduplicate rules, we
	 * look at their effective spec values only.
	 */
	ixgbe_flow_normalize_rule(rule);

	return 0;
}

static int
ixgbe_flow_fdir_ctx_init_common(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct ixgbe_fdir_ctx *fdir_ctx = (struct ixgbe_fdir_ctx *)ctx;
	struct ci_flow_actions parsed_actions;
	struct ci_flow_actions_check_param ap_param = {
		.allowed_types = (const enum rte_flow_action_type[]){
			/* queue/mark/drop allowed here */
			RTE_FLOW_ACTION_TYPE_QUEUE,
			RTE_FLOW_ACTION_TYPE_DROP,
			RTE_FLOW_ACTION_TYPE_MARK,
			RTE_FLOW_ACTION_TYPE_END
		},
		.driver_ctx = ctx->dev_data,
		.check = ixgbe_fdir_actions_check
	};
	struct ixgbe_fdir_rule *rule = &fdir_ctx->rule;
	int ret;

	ixgbe_fdir_mask_set_default(&rule->mask);

	/* validate attributes */
	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	/* parse requested actions */
	ret = ci_flow_check_actions(actions, &ap_param, &parsed_actions, error);
	if (ret)
		return ret;

	fdir_ctx->fwd_action = parsed_actions.actions[0];
	/* can be NULL */
	fdir_ctx->aux_action = parsed_actions.actions[1];

	/* set up forward/drop action */
	if (fdir_ctx->fwd_action->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		const struct rte_flow_action_queue *q_act = fdir_ctx->fwd_action->conf;
		rule->queue = q_act->index;
	} else {
		rule->fdirflags = IXGBE_FDIRCMD_DROP;
	}

	/* set up mark action */
	if (fdir_ctx->aux_action != NULL && fdir_ctx->aux_action->type == RTE_FLOW_ACTION_TYPE_MARK) {
		const struct rte_flow_action_mark *m_act = fdir_ctx->aux_action->conf;
		rule->soft_id = m_act->id;
	}

	return ret;
}

static int
ixgbe_flow_fdir_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	const struct ixgbe_fdir_priv *fdir_priv = ctx->engine_priv;
	struct ixgbe_fdir_ctx *fdir_ctx = (struct ixgbe_fdir_ctx *)ctx;
	int ret;

	/* call into common part first */
	ret = ixgbe_flow_fdir_ctx_init_common(actions, attr, ctx, error);
	if (ret)
		return ret;

	/* hardware capabilities are resolved once at engine_init */
	fdir_ctx->supports_sctp_ports = fdir_priv->supports_sctp_ports;
	fdir_ctx->supports_mac_vlan_mode = fdir_priv->supports_mac_vlan_mode;

	return 0;
}

static int
ixgbe_flow_fdir_tunnel_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct ixgbe_fdir_ctx *fdir_ctx = (struct ixgbe_fdir_ctx *)ctx;
	int ret;

	/* call into common part first */
	ret = ixgbe_flow_fdir_ctx_init_common(actions, attr, ctx, error);
	if (ret)
		return ret;

	fdir_ctx->rule.mode = RTE_FDIR_MODE_PERFECT_TUNNEL;

	return 0;
}

static int
ixgbe_flow_fdir_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct ixgbe_fdir_ctx *fdir_ctx = (const struct ixgbe_fdir_ctx *)ctx;
	struct ixgbe_fdir_flow *fdir_flow = (struct ixgbe_fdir_flow *)flow;

	fdir_flow->rule = fdir_ctx->rule;

	return 0;
}

static bool
ixgbe_fdir_table_contains(const struct rte_hash *hash_handle,
		const struct ixgbe_fdir_rule *rule)
{
	return rte_hash_lookup(hash_handle, &rule->ixgbe_fdir) >= 0;
}

static int
ixgbe_fdir_table_add(struct rte_hash *hash_handle,
		struct ixgbe_fdir_flow *fdir_flow)
{
	return rte_hash_add_key(hash_handle, &fdir_flow->rule.ixgbe_fdir);
}

static int
ixgbe_fdir_table_del(struct rte_hash *hash_handle,
		struct ixgbe_fdir_flow *fdir_flow)
{
	return rte_hash_del_key(hash_handle, &fdir_flow->rule.ixgbe_fdir);
}

static bool
ixgbe_fdir_rule_is_compatible(const struct ixgbe_fdir_state *state,
		const struct ixgbe_fdir_rule *rule)
{
	/*
	 * A rule is compatible if:
	 *
	 * - No other rules are present
	 * - The mode matches existing rules
	 * - The flex bytes mask matches existing rules
	 * - The flex bytes offset matches existing rules
	 */
	if (state->config.rule_count == 0)
		return true;

	if (state->config.mode != rule->mode)
		return false;

	if (memcmp(&state->mask_conf.mask, &rule->mask,
			sizeof(state->mask_conf.mask)) != 0)
		return false;

	return state->mask_conf.flex_bytes_offset == rule->flex_bytes_offset;
}

static void
ixgbe_fdir_config_claim(struct ixgbe_fdir_state *state,
		const struct ixgbe_fdir_rule *rule)
{
	if (ci_usage_count_claim(&state->config.rule_count)) {
		state->config.mode = rule->mode;
		state->mask_conf.mask = rule->mask;
		/* flex bytes offset may not be meaningful */
		if (rule->mask.flex_bytes_mask != 0)
			state->mask_conf.flex_bytes_offset = rule->flex_bytes_offset;
	}
}

static void
ixgbe_fdir_config_release(struct ixgbe_fdir_state *state)
{
	if (ci_usage_count_release(&state->config.rule_count)) {
		state->stats = (struct ixgbe_fdir_stats){0};
		state->config.mode = RTE_FDIR_MODE_NONE;
		state->config.hw_configured = false;
		/* reset the mask */
		ixgbe_fdir_mask_set_default(&state->mask_conf.mask);
		state->mask_conf.flex_bytes_offset = 0;
		state->mask_conf.mask_configured = false;
	}
}

static int
ixgbe_flow_fdir_flow_register(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct ixgbe_fdir_flow *fdir_flow = (struct ixgbe_fdir_flow *)flow;
	struct ixgbe_fdir_priv *priv = flow->engine_priv;
	struct ixgbe_fdir_state *state = priv->state;
	struct ixgbe_fdir_rule *rule = &fdir_flow->rule;
	int ret;

	if (ixgbe_fdir_table_contains(state->hash_handle, rule)) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Conflict with existing flow director filter");
	}
	if (!ixgbe_fdir_rule_is_compatible(state, rule)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Flow rule is incompatible with existing rules");
	}

	ret = ixgbe_fdir_table_add(state->hash_handle, fdir_flow);
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to add flow director filter to table");
	}

	ixgbe_fdir_config_claim(state, rule);

	return 0;
}

static int
ixgbe_flow_fdir_flow_unregister(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct ixgbe_fdir_flow *fdir_flow = (struct ixgbe_fdir_flow *)flow;
	struct ixgbe_fdir_priv *priv = flow->engine_priv;
	struct ixgbe_fdir_state *state = priv->state;
	int ret;

	ret = ixgbe_fdir_table_del(state->hash_handle, fdir_flow);
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to remove flow director filter from table");
	}

	ixgbe_fdir_config_release(state);

	return 0;
}

static int
ixgbe_flow_fdir_flow_install(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(flow->dev_data->dev_private);
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	struct ixgbe_fdir_flow *fdir_flow = (struct ixgbe_fdir_flow *)flow;
	struct ixgbe_fdir_priv *priv = flow->engine_priv;
	struct ixgbe_fdir_config_state *config = &priv->state->config;
	struct ixgbe_fdir_mask_state *mask_conf = &priv->state->mask_conf;
	int ret;

	if (!config->hw_configured) {
		ret = ixgbe_fdir_configure(flow->dev_data,
				&adapter->fdir_hw_config, config->mode);
		if (ret != 0) {
			return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to configure flow director");
		}
		config->hw_configured = true;
	}

	if (!mask_conf->mask_configured) {
		ret = ixgbe_fdir_mask_program(adapter, &mask_conf->mask,
				mask_conf->flex_bytes_offset, config->mode);
		if (ret != 0) {
			ret = rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to program flow director mask");
			goto rollback_mask;
		}
		mask_conf->mask_configured = true;
	}

	ret = ixgbe_fdir_filter_program(adapter, &fdir_flow->rule);
	if (ret != 0) {
		ret = rte_flow_error_set(error, -ret,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Failed to program flow director filter");
		goto rollback_mask;
	}

	return 0;

rollback_mask:
	if (ci_usage_count_is_last(&config->rule_count)) {
		ixgbe_fdir_disable(hw);
		config->hw_configured = false;
		mask_conf->mask_configured = false;
	}

	return ret;
}

static int
ixgbe_flow_fdir_flow_uninstall(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(flow->dev_data->dev_private);
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	struct ixgbe_fdir_flow *fdir_flow = (struct ixgbe_fdir_flow *)flow;
	struct ixgbe_fdir_priv *priv = flow->engine_priv;
	struct ixgbe_fdir_config_state *config = &priv->state->config;
	int ret;

	ret = ixgbe_fdir_filter_clear(adapter, &fdir_flow->rule);
	if (ret != 0) {
		return rte_flow_error_set(error, -ret,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to remove flow director filter");
	}

	/* is this the last rule? */
	if (!ci_usage_count_is_last(&config->rule_count))
		return 0;

	ixgbe_fdir_disable(hw);
	config->hw_configured = false;
	priv->state->mask_conf.mask_configured = false;

	return 0;
}

static void
ixgbe_flow_fdir_engine_uninit(const struct ci_flow_engine *engine __rte_unused,
		void *priv)
{
	struct ixgbe_fdir_priv *fdir_priv = priv;

	ixgbe_fdir_state_detach(fdir_priv->state);
}

static int
ixgbe_flow_fdir_state_attach(struct rte_eth_dev_data *dev_data, void *priv)
{
	struct ixgbe_fdir_priv *fdir_priv = priv;

	fdir_priv->state = ixgbe_fdir_state_attach(dev_data);

	return fdir_priv->state == NULL ? -ENOMEM : 0;
}

static int
ixgbe_flow_fdir_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);
	struct ixgbe_fdir_priv *fdir_priv = priv;

	if (hw->mac.type != ixgbe_mac_82599EB &&
			hw->mac.type != ixgbe_mac_X540 &&
			hw->mac.type != ixgbe_mac_X550 &&
			hw->mac.type != ixgbe_mac_X550EM_x &&
			hw->mac.type != ixgbe_mac_X550EM_a &&
			hw->mac.type != ixgbe_mac_E610)
		return -ENOTSUP;

	/* some hardware does not support SCTP matching, or mac-vlan mode */
	if (hw->mac.type == ixgbe_mac_X550 ||
			hw->mac.type == ixgbe_mac_X550EM_x ||
			hw->mac.type == ixgbe_mac_X550EM_a ||
			hw->mac.type == ixgbe_mac_E610) {
		fdir_priv->supports_sctp_ports = true;
		fdir_priv->supports_mac_vlan_mode = true;
	}

	return ixgbe_flow_fdir_state_attach(dev_data, priv);
}

static int
ixgbe_flow_fdir_tunnel_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);
	struct ixgbe_fdir_priv *fdir_priv = priv;

	if (hw->mac.type != ixgbe_mac_X550 &&
			hw->mac.type != ixgbe_mac_X550EM_x &&
			hw->mac.type != ixgbe_mac_X550EM_a &&
			hw->mac.type != ixgbe_mac_E610)
		return -ENOTSUP;

	/* tunnel engine only ever attaches on hardware that supports both */
	fdir_priv->supports_sctp_ports = true;
	fdir_priv->supports_mac_vlan_mode = true;

	return ixgbe_flow_fdir_state_attach(dev_data, priv);
}

static const struct ci_flow_engine_ops ixgbe_fdir_ops = {
	.engine_init = ixgbe_flow_fdir_engine_init,
	.engine_uninit = ixgbe_flow_fdir_engine_uninit,
	.ctx_init = ixgbe_flow_fdir_ctx_init,
	.pattern_parse = ixgbe_fdir_pattern_parse,
	.ctx_finalize = ixgbe_flow_fdir_ctx_finalize,
	.ctx_to_flow = ixgbe_flow_fdir_ctx_to_flow,
	.flow_register = ixgbe_flow_fdir_flow_register,
	.flow_unregister = ixgbe_flow_fdir_flow_unregister,
	.flow_install = ixgbe_flow_fdir_flow_install,
	.flow_uninstall = ixgbe_flow_fdir_flow_uninstall,
};

static const struct ci_flow_engine_ops ixgbe_fdir_tunnel_ops = {
	.engine_init = ixgbe_flow_fdir_tunnel_engine_init,
	.engine_uninit = ixgbe_flow_fdir_engine_uninit,
	.ctx_init = ixgbe_flow_fdir_tunnel_ctx_init,
	.ctx_finalize = ixgbe_flow_fdir_ctx_finalize,
	.ctx_to_flow = ixgbe_flow_fdir_ctx_to_flow,
	.flow_register = ixgbe_flow_fdir_flow_register,
	.flow_unregister = ixgbe_flow_fdir_flow_unregister,
	.flow_install = ixgbe_flow_fdir_flow_install,
	.flow_uninstall = ixgbe_flow_fdir_flow_uninstall,
};

const struct ci_flow_engine ixgbe_fdir_flow_engine = {
	.name = "ixgbe_fdir",
	.ctx_size = sizeof(struct ixgbe_fdir_ctx),
	.flow_size = sizeof(struct ixgbe_fdir_flow),
	.priv_size = sizeof(struct ixgbe_fdir_priv),
	.ops = &ixgbe_fdir_ops,
	.graph = &ixgbe_fdir_normal_graph,
};

const struct ci_flow_engine ixgbe_fdir_tunnel_flow_engine = {
	.name = "ixgbe_fdir_tunnel",
	.ctx_size = sizeof(struct ixgbe_fdir_ctx),
	.flow_size = sizeof(struct ixgbe_fdir_flow),
	.priv_size = sizeof(struct ixgbe_fdir_priv),
	.ops = &ixgbe_fdir_tunnel_ops,
	.graph = &ixgbe_fdir_tunnel_graph,
};
