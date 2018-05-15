/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <sys/queue.h>
#include <stdint.h>
#include <string.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_common.h>
#include <rte_ether.h>
#include <rte_eth_ctrl.h>
#include <rte_ethdev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_ip.h>

#include "mlx5.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"
#include "mlx5_glue.h"

/* Flow priority for control plane flows. */
#define MLX5_CTRL_FLOW_PRIORITY 1

/* Internet Protocol versions. */
#define MLX5_IPV4 4
#define MLX5_IPV6 6
#define MLX5_GRE 47

#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
struct ibv_flow_spec_counter_action {
	int dummy;
};
#endif

/* Dev ops structure defined in mlx5.c */
extern const struct eth_dev_ops mlx5_dev_ops;
extern const struct eth_dev_ops mlx5_dev_ops_isolate;

/** Structure give to the conversion functions. */
struct mlx5_flow_data {
	struct rte_eth_dev *dev; /** Ethernet device. */
	struct mlx5_flow_parse *parser; /** Parser context. */
	struct rte_flow_error *error; /** Error context. */
};

static int
mlx5_flow_create_eth(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data);

static int
mlx5_flow_create_vlan(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data);

static int
mlx5_flow_create_ipv4(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data);

static int
mlx5_flow_create_ipv6(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data);

static int
mlx5_flow_create_udp(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data);

static int
mlx5_flow_create_tcp(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data);

static int
mlx5_flow_create_vxlan(const struct rte_flow_item *item,
		       const void *default_mask,
		       struct mlx5_flow_data *data);

static int
mlx5_flow_create_vxlan_gpe(const struct rte_flow_item *item,
			   const void *default_mask,
			   struct mlx5_flow_data *data);

static int
mlx5_flow_create_gre(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data);

static int
mlx5_flow_create_mpls(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data);

struct mlx5_flow_parse;

static void
mlx5_flow_create_copy(struct mlx5_flow_parse *parser, void *src,
		      unsigned int size);

static int
mlx5_flow_create_flag_mark(struct mlx5_flow_parse *parser, uint32_t mark_id);

static int
mlx5_flow_create_count(struct rte_eth_dev *dev, struct mlx5_flow_parse *parser);

/* Hash RX queue types. */
enum hash_rxq_type {
	HASH_RXQ_TCPV4,
	HASH_RXQ_UDPV4,
	HASH_RXQ_IPV4,
	HASH_RXQ_TCPV6,
	HASH_RXQ_UDPV6,
	HASH_RXQ_IPV6,
	HASH_RXQ_ETH,
	HASH_RXQ_TUNNEL,
};

/* Initialization data for hash RX queue. */
struct hash_rxq_init {
	uint64_t hash_fields; /* Fields that participate in the hash. */
	uint64_t dpdk_rss_hf; /* Matching DPDK RSS hash fields. */
	unsigned int flow_priority; /* Flow priority to use. */
	unsigned int ip_version; /* Internet protocol. */
};

/* Initialization data for hash RX queues. */
const struct hash_rxq_init hash_rxq_init[] = {
	[HASH_RXQ_TCPV4] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV4 |
				IBV_RX_HASH_DST_IPV4 |
				IBV_RX_HASH_SRC_PORT_TCP |
				IBV_RX_HASH_DST_PORT_TCP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV4_TCP,
		.flow_priority = 0,
		.ip_version = MLX5_IPV4,
	},
	[HASH_RXQ_UDPV4] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV4 |
				IBV_RX_HASH_DST_IPV4 |
				IBV_RX_HASH_SRC_PORT_UDP |
				IBV_RX_HASH_DST_PORT_UDP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV4_UDP,
		.flow_priority = 0,
		.ip_version = MLX5_IPV4,
	},
	[HASH_RXQ_IPV4] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV4 |
				IBV_RX_HASH_DST_IPV4),
		.dpdk_rss_hf = (ETH_RSS_IPV4 |
				ETH_RSS_FRAG_IPV4),
		.flow_priority = 1,
		.ip_version = MLX5_IPV4,
	},
	[HASH_RXQ_TCPV6] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV6 |
				IBV_RX_HASH_DST_IPV6 |
				IBV_RX_HASH_SRC_PORT_TCP |
				IBV_RX_HASH_DST_PORT_TCP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV6_TCP,
		.flow_priority = 0,
		.ip_version = MLX5_IPV6,
	},
	[HASH_RXQ_UDPV6] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV6 |
				IBV_RX_HASH_DST_IPV6 |
				IBV_RX_HASH_SRC_PORT_UDP |
				IBV_RX_HASH_DST_PORT_UDP),
		.dpdk_rss_hf = ETH_RSS_NONFRAG_IPV6_UDP,
		.flow_priority = 0,
		.ip_version = MLX5_IPV6,
	},
	[HASH_RXQ_IPV6] = {
		.hash_fields = (IBV_RX_HASH_SRC_IPV6 |
				IBV_RX_HASH_DST_IPV6),
		.dpdk_rss_hf = (ETH_RSS_IPV6 |
				ETH_RSS_FRAG_IPV6),
		.flow_priority = 1,
		.ip_version = MLX5_IPV6,
	},
	[HASH_RXQ_ETH] = {
		.hash_fields = 0,
		.dpdk_rss_hf = 0,
		.flow_priority = 2,
	},
};

/* Number of entries in hash_rxq_init[]. */
const unsigned int hash_rxq_init_n = RTE_DIM(hash_rxq_init);

/** Structure for holding counter stats. */
struct mlx5_flow_counter_stats {
	uint64_t hits; /**< Number of packets matched by the rule. */
	uint64_t bytes; /**< Number of bytes matched by the rule. */
};

/** Structure for Drop queue. */
struct mlx5_hrxq_drop {
	struct ibv_rwq_ind_table *ind_table; /**< Indirection table. */
	struct ibv_qp *qp; /**< Verbs queue pair. */
	struct ibv_wq *wq; /**< Verbs work queue. */
	struct ibv_cq *cq; /**< Verbs completion queue. */
};

/* Flows structures. */
struct mlx5_flow {
	uint64_t hash_fields; /**< Fields that participate in the hash. */
	struct ibv_flow_attr *ibv_attr; /**< Pointer to Verbs attributes. */
	struct ibv_flow *ibv_flow; /**< Verbs flow. */
	struct mlx5_hrxq *hrxq; /**< Hash Rx queues. */
};

/* Drop flows structures. */
struct mlx5_flow_drop {
	struct ibv_flow_attr *ibv_attr; /**< Pointer to Verbs attributes. */
	struct ibv_flow *ibv_flow; /**< Verbs flow. */
};

struct rte_flow {
	TAILQ_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	uint32_t mark:1; /**< Set if the flow is marked. */
	uint32_t drop:1; /**< Drop queue. */
	struct rte_flow_action_rss rss_conf; /**< RSS configuration */
	uint16_t (*queues)[]; /**< Queues indexes to use. */
	uint8_t rss_key[40]; /**< copy of the RSS key. */
	uint32_t tunnel; /**< Tunnel type of RTE_PTYPE_TUNNEL_XXX. */
	struct ibv_counter_set *cs; /**< Holds the counters for the rule. */
	struct mlx5_flow_counter_stats counter_stats;/**<The counter stats. */
	struct mlx5_flow frxq[RTE_DIM(hash_rxq_init)];
	/**< Flow with Rx queue. */
};

/** Static initializer for items. */
#define ITEMS(...) \
	(const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	}

#define IS_TUNNEL(type) ( \
	(type) == RTE_FLOW_ITEM_TYPE_VXLAN || \
	(type) == RTE_FLOW_ITEM_TYPE_VXLAN_GPE || \
	(type) == RTE_FLOW_ITEM_TYPE_GRE || \
	(type) == RTE_FLOW_ITEM_TYPE_MPLS)

const uint32_t flow_ptype[] = {
	[RTE_FLOW_ITEM_TYPE_VXLAN] = RTE_PTYPE_TUNNEL_VXLAN,
	[RTE_FLOW_ITEM_TYPE_VXLAN_GPE] = RTE_PTYPE_TUNNEL_VXLAN_GPE,
	[RTE_FLOW_ITEM_TYPE_GRE] = RTE_PTYPE_TUNNEL_GRE,
	[RTE_FLOW_ITEM_TYPE_MPLS] = RTE_PTYPE_TUNNEL_MPLS_IN_GRE,
};

#define PTYPE_IDX(t) ((RTE_PTYPE_TUNNEL_MASK & (t)) >> 12)

const uint32_t ptype_ext[] = {
	[PTYPE_IDX(RTE_PTYPE_TUNNEL_VXLAN)] = RTE_PTYPE_TUNNEL_VXLAN |
					      RTE_PTYPE_L4_UDP,
	[PTYPE_IDX(RTE_PTYPE_TUNNEL_VXLAN_GPE)]	= RTE_PTYPE_TUNNEL_VXLAN_GPE |
						  RTE_PTYPE_L4_UDP,
	[PTYPE_IDX(RTE_PTYPE_TUNNEL_GRE)] = RTE_PTYPE_TUNNEL_GRE,
	[PTYPE_IDX(RTE_PTYPE_TUNNEL_MPLS_IN_GRE)] =
		RTE_PTYPE_TUNNEL_MPLS_IN_GRE,
	[PTYPE_IDX(RTE_PTYPE_TUNNEL_MPLS_IN_UDP)] =
		RTE_PTYPE_TUNNEL_MPLS_IN_GRE | RTE_PTYPE_L4_UDP,
};

/** Structure to generate a simple graph of layers supported by the NIC. */
struct mlx5_flow_items {
	/** List of possible actions for these items. */
	const enum rte_flow_action_type *const actions;
	/** Bit-masks corresponding to the possibilities for the item. */
	const void *mask;
	/**
	 * Default bit-masks to use when item->mask is not provided. When
	 * \default_mask is also NULL, the full supported bit-mask (\mask) is
	 * used instead.
	 */
	const void *default_mask;
	/** Bit-masks size in bytes. */
	const unsigned int mask_sz;
	/**
	 * Conversion function from rte_flow to NIC specific flow.
	 *
	 * @param item
	 *   rte_flow item to convert.
	 * @param default_mask
	 *   Default bit-masks to use when item->mask is not provided.
	 * @param data
	 *   Internal structure to store the conversion.
	 *
	 * @return
	 *   0 on success, a negative errno value otherwise and rte_errno is
	 *   set.
	 */
	int (*convert)(const struct rte_flow_item *item,
		       const void *default_mask,
		       struct mlx5_flow_data *data);
	/** Size in bytes of the destination structure. */
	const unsigned int dst_sz;
	/** List of possible following items.  */
	const enum rte_flow_item_type *const items;
};

/** Valid action for this PMD. */
static const enum rte_flow_action_type valid_actions[] = {
	RTE_FLOW_ACTION_TYPE_DROP,
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_MARK,
	RTE_FLOW_ACTION_TYPE_FLAG,
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	RTE_FLOW_ACTION_TYPE_COUNT,
#endif
	RTE_FLOW_ACTION_TYPE_END,
};

/** Graph of supported items and associated actions. */
static const struct mlx5_flow_items mlx5_flow_items[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VXLAN,
			       RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
			       RTE_FLOW_ITEM_TYPE_GRE),
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_eth){
			.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.type = -1,
		},
		.default_mask = &rte_flow_item_eth_mask,
		.mask_sz = sizeof(struct rte_flow_item_eth),
		.convert = mlx5_flow_create_eth,
		.dst_sz = sizeof(struct ibv_flow_spec_eth),
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_vlan){
			.tci = -1,
			.inner_type = -1,
		},
		.default_mask = &rte_flow_item_vlan_mask,
		.mask_sz = sizeof(struct rte_flow_item_vlan),
		.convert = mlx5_flow_create_vlan,
		.dst_sz = 0,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_TCP,
			       RTE_FLOW_ITEM_TYPE_GRE),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_ipv4){
			.hdr = {
				.src_addr = -1,
				.dst_addr = -1,
				.type_of_service = -1,
				.next_proto_id = -1,
			},
		},
		.default_mask = &rte_flow_item_ipv4_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv4),
		.convert = mlx5_flow_create_ipv4,
		.dst_sz = sizeof(struct ibv_flow_spec_ipv4_ext),
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_TCP,
			       RTE_FLOW_ITEM_TYPE_GRE),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_ipv6){
			.hdr = {
				.src_addr = {
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
				},
				.dst_addr = {
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
				},
				.vtc_flow = -1,
				.proto = -1,
				.hop_limits = -1,
			},
		},
		.default_mask = &rte_flow_item_ipv6_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv6),
		.convert = mlx5_flow_create_ipv6,
		.dst_sz = sizeof(struct ibv_flow_spec_ipv6),
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_VXLAN,
			       RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
			       RTE_FLOW_ITEM_TYPE_MPLS),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_udp){
			.hdr = {
				.src_port = -1,
				.dst_port = -1,
			},
		},
		.default_mask = &rte_flow_item_udp_mask,
		.mask_sz = sizeof(struct rte_flow_item_udp),
		.convert = mlx5_flow_create_udp,
		.dst_sz = sizeof(struct ibv_flow_spec_tcp_udp),
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_tcp){
			.hdr = {
				.src_port = -1,
				.dst_port = -1,
			},
		},
		.default_mask = &rte_flow_item_tcp_mask,
		.mask_sz = sizeof(struct rte_flow_item_tcp),
		.convert = mlx5_flow_create_tcp,
		.dst_sz = sizeof(struct ibv_flow_spec_tcp_udp),
	},
	[RTE_FLOW_ITEM_TYPE_GRE] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_MPLS),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_gre){
			.protocol = -1,
		},
		.default_mask = &rte_flow_item_gre_mask,
		.mask_sz = sizeof(struct rte_flow_item_gre),
		.convert = mlx5_flow_create_gre,
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
		.dst_sz = sizeof(struct ibv_flow_spec_gre),
#else
		.dst_sz = sizeof(struct ibv_flow_spec_tunnel),
#endif
	},
	[RTE_FLOW_ITEM_TYPE_MPLS] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_mpls){
			.label_tc_s = "\xff\xff\xf0",
		},
		.default_mask = &rte_flow_item_mpls_mask,
		.mask_sz = sizeof(struct rte_flow_item_mpls),
		.convert = mlx5_flow_create_mpls,
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
		.dst_sz = sizeof(struct ibv_flow_spec_mpls),
#endif
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_IPV4, /* For L3 VXLAN. */
			       RTE_FLOW_ITEM_TYPE_IPV6), /* For L3 VXLAN. */
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_vxlan){
			.vni = "\xff\xff\xff",
		},
		.default_mask = &rte_flow_item_vxlan_mask,
		.mask_sz = sizeof(struct rte_flow_item_vxlan),
		.convert = mlx5_flow_create_vxlan,
		.dst_sz = sizeof(struct ibv_flow_spec_tunnel),
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN_GPE] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_vxlan_gpe){
			.vni = "\xff\xff\xff",
		},
		.default_mask = &rte_flow_item_vxlan_gpe_mask,
		.mask_sz = sizeof(struct rte_flow_item_vxlan_gpe),
		.convert = mlx5_flow_create_vxlan_gpe,
		.dst_sz = sizeof(struct ibv_flow_spec_tunnel),
	},
};

/** Structure to pass to the conversion function. */
struct mlx5_flow_parse {
	uint32_t inner; /**< Verbs value, set once tunnel is encountered. */
	uint32_t create:1;
	/**< Whether resources should remain after a validate. */
	uint32_t drop:1; /**< Target is a drop queue. */
	uint32_t mark:1; /**< Mark is present in the flow. */
	uint32_t count:1; /**< Count is present in the flow. */
	uint32_t mark_id; /**< Mark identifier. */
	struct rte_flow_action_rss rss_conf; /**< RSS configuration */
	uint16_t queues[RTE_MAX_QUEUES_PER_PORT]; /**< Queues indexes to use. */
	uint8_t rss_key[40]; /**< copy of the RSS key. */
	enum hash_rxq_type layer; /**< Last pattern layer detected. */
	enum hash_rxq_type out_layer; /**< Last outer pattern layer detected. */
	uint32_t tunnel; /**< Tunnel type of RTE_PTYPE_TUNNEL_XXX. */
	struct ibv_counter_set *cs; /**< Holds the counter set for the rule */
	struct {
		struct ibv_flow_attr *ibv_attr;
		/**< Pointer to Verbs attributes. */
		unsigned int offset;
		/**< Current position or total size of the attribute. */
		uint64_t hash_fields; /**< Verbs hash fields. */
	} queue[RTE_DIM(hash_rxq_init)];
};

static const struct rte_flow_ops mlx5_flow_ops = {
	.validate = mlx5_flow_validate,
	.create = mlx5_flow_create,
	.destroy = mlx5_flow_destroy,
	.flush = mlx5_flow_flush,
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	.query = mlx5_flow_query,
#else
	.query = NULL,
#endif
	.isolate = mlx5_flow_isolate,
};

/* Convert FDIR request to Generic flow. */
struct mlx5_fdir {
	struct rte_flow_attr attr;
	struct rte_flow_action actions[2];
	struct rte_flow_item items[4];
	struct rte_flow_item_eth l2;
	struct rte_flow_item_eth l2_mask;
	union {
		struct rte_flow_item_ipv4 ipv4;
		struct rte_flow_item_ipv6 ipv6;
	} l3;
	union {
		struct rte_flow_item_ipv4 ipv4;
		struct rte_flow_item_ipv6 ipv6;
	} l3_mask;
	union {
		struct rte_flow_item_udp udp;
		struct rte_flow_item_tcp tcp;
	} l4;
	union {
		struct rte_flow_item_udp udp;
		struct rte_flow_item_tcp tcp;
	} l4_mask;
	struct rte_flow_action_queue queue;
};

/* Verbs specification header. */
struct ibv_spec_header {
	enum ibv_flow_spec_type type;
	uint16_t size;
};

/**
 * Check item is fully supported by the NIC matching capability.
 *
 * @param item[in]
 *   Item specification.
 * @param mask[in]
 *   Bit-masks covering supported fields to compare with spec, last and mask in
 *   \item.
 * @param size
 *   Bit-Mask size in bytes.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_item_validate(const struct rte_flow_item *item,
			const uint8_t *mask, unsigned int size)
{
	unsigned int i;
	const uint8_t *spec = item->spec;
	const uint8_t *last = item->last;
	const uint8_t *m = item->mask ? item->mask : mask;

	if (!spec && (item->mask || last))
		goto error;
	if (!spec)
		return 0;
	/*
	 * Single-pass check to make sure that:
	 * - item->mask is supported, no bits are set outside mask.
	 * - Both masked item->spec and item->last are equal (no range
	 *   supported).
	 */
	for (i = 0; i < size; i++) {
		if (!m[i])
			continue;
		if ((m[i] | mask[i]) != mask[i])
			goto error;
		if (last && ((spec[i] & m[i]) != (last[i] & m[i])))
			goto error;
	}
	return 0;
error:
	rte_errno = ENOTSUP;
	return -rte_errno;
}

/**
 * Extract attribute to the parser.
 *
 * @param[in] attr
 *   Flow rule attributes.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_convert_attributes(const struct rte_flow_attr *attr,
			     struct rte_flow_error *error)
{
	if (attr->group) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				   NULL,
				   "groups are not supported");
		return -rte_errno;
	}
	if (attr->priority && attr->priority != MLX5_CTRL_FLOW_PRIORITY) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   NULL,
				   "priorities are not supported");
		return -rte_errno;
	}
	if (attr->egress) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   NULL,
				   "egress is not supported");
		return -rte_errno;
	}
	if (attr->transfer) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
				   NULL,
				   "transfer is not supported");
		return -rte_errno;
	}
	if (!attr->ingress) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   NULL,
				   "only ingress is supported");
		return -rte_errno;
	}
	return 0;
}

/**
 * Extract actions request to the parser.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @param[in, out] parser
 *   Internal parser structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_convert_actions(struct rte_eth_dev *dev,
			  const struct rte_flow_action actions[],
			  struct rte_flow_error *error,
			  struct mlx5_flow_parse *parser)
{
	enum { FATE = 1, MARK = 2, COUNT = 4, };
	uint32_t overlap = 0;
	struct priv *priv = dev->data->dev_private;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID) {
			continue;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_DROP) {
			if (overlap & FATE)
				goto exit_action_overlap;
			overlap |= FATE;
			parser->drop = 1;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *queue =
				(const struct rte_flow_action_queue *)
				actions->conf;

			if (overlap & FATE)
				goto exit_action_overlap;
			overlap |= FATE;
			if (!queue || (queue->index > (priv->rxqs_n - 1)))
				goto exit_action_not_supported;
			parser->queues[0] = queue->index;
			parser->rss_conf = (struct rte_flow_action_rss){
				.queue_num = 1,
				.queue = parser->queues,
			};
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_RSS) {
			const struct rte_flow_action_rss *rss =
				(const struct rte_flow_action_rss *)
				actions->conf;
			const uint8_t *rss_key;
			uint32_t rss_key_len;
			uint16_t n;

			if (overlap & FATE)
				goto exit_action_overlap;
			overlap |= FATE;
			if (rss->func &&
			    rss->func != RTE_ETH_HASH_FUNCTION_TOEPLITZ) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "the only supported RSS hash"
						   " function is Toeplitz");
				return -rte_errno;
			}
#ifndef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
			if (parser->rss_conf.level > 1) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "a nonzero RSS encapsulation"
						   " level is not supported");
				return -rte_errno;
			}
#endif
			if (parser->rss_conf.level > 2) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "RSS encapsulation level"
						   " > 1 is not supported");
				return -rte_errno;
			}
			if (rss->types & MLX5_RSS_HF_MASK) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "unsupported RSS type"
						   " requested");
				return -rte_errno;
			}
			if (rss->key_len) {
				rss_key_len = rss->key_len;
				rss_key = rss->key;
			} else {
				rss_key_len = rss_hash_default_key_len;
				rss_key = rss_hash_default_key;
			}
			if (rss_key_len != RTE_DIM(parser->rss_key)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "RSS hash key must be"
						   " exactly 40 bytes long");
				return -rte_errno;
			}
			if (!rss->queue_num) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "no valid queues");
				return -rte_errno;
			}
			if (rss->queue_num > RTE_DIM(parser->queues)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "too many queues for RSS"
						   " context");
				return -rte_errno;
			}
			for (n = 0; n < rss->queue_num; ++n) {
				if (rss->queue[n] >= priv->rxqs_n) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "queue id > number of"
						   " queues");
					return -rte_errno;
				}
			}
			parser->rss_conf = (struct rte_flow_action_rss){
				.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
				.level = rss->level ? rss->level : 1,
				.types = rss->types,
				.key_len = rss_key_len,
				.queue_num = rss->queue_num,
				.key = memcpy(parser->rss_key, rss_key,
					      sizeof(*rss_key) * rss_key_len),
				.queue = memcpy(parser->queues, rss->queue,
						sizeof(*rss->queue) *
						rss->queue_num),
			};
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_MARK) {
			const struct rte_flow_action_mark *mark =
				(const struct rte_flow_action_mark *)
				actions->conf;

			if (overlap & MARK)
				goto exit_action_overlap;
			overlap |= MARK;
			if (!mark) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "mark must be defined");
				return -rte_errno;
			} else if (mark->id >= MLX5_FLOW_MARK_MAX) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "mark must be between 0"
						   " and 16777199");
				return -rte_errno;
			}
			parser->mark = 1;
			parser->mark_id = mark->id;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_FLAG) {
			if (overlap & MARK)
				goto exit_action_overlap;
			overlap |= MARK;
			parser->mark = 1;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_COUNT &&
			   priv->config.flow_counter_en) {
			if (overlap & COUNT)
				goto exit_action_overlap;
			overlap |= COUNT;
			parser->count = 1;
		} else {
			goto exit_action_not_supported;
		}
	}
	/* When fate is unknown, drop traffic. */
	if (!(overlap & FATE))
		parser->drop = 1;
	if (parser->drop && parser->mark)
		parser->mark = 0;
	if (!parser->rss_conf.queue_num && !parser->drop) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "no valid action");
		return -rte_errno;
	}
	return 0;
exit_action_not_supported:
	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
			   actions, "action not supported");
	return -rte_errno;
exit_action_overlap:
	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
			   actions, "overlapping actions are not supported");
	return -rte_errno;
}

/**
 * Validate items.
 *
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @param[in, out] parser
 *   Internal parser structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_convert_items_validate(struct rte_eth_dev *dev,
				 const struct rte_flow_item items[],
				 struct rte_flow_error *error,
				 struct mlx5_flow_parse *parser)
{
	struct priv *priv = dev->data->dev_private;
	const struct mlx5_flow_items *cur_item = mlx5_flow_items;
	unsigned int i;
	unsigned int last_voids = 0;
	int ret = 0;

	/* Initialise the offsets to start after verbs attribute. */
	for (i = 0; i != hash_rxq_init_n; ++i)
		parser->queue[i].offset = sizeof(struct ibv_flow_attr);
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; ++items) {
		const struct mlx5_flow_items *token = NULL;
		unsigned int n;

		if (items->type == RTE_FLOW_ITEM_TYPE_VOID) {
			last_voids++;
			continue;
		}
		for (i = 0;
		     cur_item->items &&
		     cur_item->items[i] != RTE_FLOW_ITEM_TYPE_END;
		     ++i) {
			if (cur_item->items[i] == items->type) {
				token = &mlx5_flow_items[items->type];
				break;
			}
		}
		if (!token) {
			ret = -ENOTSUP;
			goto exit_item_not_supported;
		}
		cur_item = token;
		ret = mlx5_flow_item_validate(items,
					      (const uint8_t *)cur_item->mask,
					      cur_item->mask_sz);
		if (ret)
			goto exit_item_not_supported;
		if (IS_TUNNEL(items->type)) {
			if (parser->tunnel &&
			    !((items - last_voids - 1)->type ==
			      RTE_FLOW_ITEM_TYPE_GRE && items->type ==
			      RTE_FLOW_ITEM_TYPE_MPLS)) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   items,
						   "Cannot recognize multiple"
						   " tunnel encapsulations.");
				return -rte_errno;
			}
			if (items->type == RTE_FLOW_ITEM_TYPE_MPLS &&
			    !priv->config.mpls_en) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   items,
						   "MPLS not supported or"
						   " disabled in firmware"
						   " configuration.");
				return -rte_errno;
			}
			if (!priv->config.tunnel_en &&
			    parser->rss_conf.level > 1) {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM,
					items,
					"RSS on tunnel is not supported");
				return -rte_errno;
			}
			parser->inner = IBV_FLOW_SPEC_INNER;
			parser->tunnel = flow_ptype[items->type];
		}
		if (parser->drop) {
			parser->queue[HASH_RXQ_ETH].offset += cur_item->dst_sz;
		} else {
			for (n = 0; n != hash_rxq_init_n; ++n)
				parser->queue[n].offset += cur_item->dst_sz;
		}
		last_voids = 0;
	}
	if (parser->drop) {
		parser->queue[HASH_RXQ_ETH].offset +=
			sizeof(struct ibv_flow_spec_action_drop);
	}
	if (parser->mark) {
		for (i = 0; i != hash_rxq_init_n; ++i)
			parser->queue[i].offset +=
				sizeof(struct ibv_flow_spec_action_tag);
	}
	if (parser->count) {
		unsigned int size = sizeof(struct ibv_flow_spec_counter_action);

		for (i = 0; i != hash_rxq_init_n; ++i)
			parser->queue[i].offset += size;
	}
	return 0;
exit_item_not_supported:
	return rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_ITEM,
				  items, "item not supported");
}

/**
 * Allocate memory space to store verbs flow attributes.
 *
 * @param[in] size
 *   Amount of byte to allocate.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A verbs flow attribute on success, NULL otherwise and rte_errno is set.
 */
static struct ibv_flow_attr *
mlx5_flow_convert_allocate(unsigned int size, struct rte_flow_error *error)
{
	struct ibv_flow_attr *ibv_attr;

	ibv_attr = rte_calloc(__func__, 1, size, 0);
	if (!ibv_attr) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate verbs spec attributes");
		return NULL;
	}
	return ibv_attr;
}

/**
 * Make inner packet matching with an higher priority from the non Inner
 * matching.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in, out] parser
 *   Internal parser structure.
 * @param attr
 *   User flow attribute.
 */
static void
mlx5_flow_update_priority(struct rte_eth_dev *dev,
			  struct mlx5_flow_parse *parser,
			  const struct rte_flow_attr *attr)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	uint16_t priority;

	/*			8 priorities	>= 16 priorities
	 * Control flow:	4-7		8-15
	 * User normal flow:	1-3		4-7
	 * User tunnel flow:	0-2		0-3
	 */
	priority = attr->priority * MLX5_VERBS_FLOW_PRIO_8;
	if (priv->config.max_verbs_prio == MLX5_VERBS_FLOW_PRIO_8)
		priority /= 2;
	/*
	 * Lower non-tunnel flow Verbs priority 1 if only support 8 Verbs
	 * priorities, lower 4 otherwise.
	 */
	if (!parser->inner) {
		if (priv->config.max_verbs_prio == MLX5_VERBS_FLOW_PRIO_8)
			priority += 1;
		else
			priority += MLX5_VERBS_FLOW_PRIO_8 / 2;
	}
	if (parser->drop) {
		parser->queue[HASH_RXQ_ETH].ibv_attr->priority = priority +
				hash_rxq_init[HASH_RXQ_ETH].flow_priority;
		return;
	}
	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (!parser->queue[i].ibv_attr)
			continue;
		parser->queue[i].ibv_attr->priority = priority +
				hash_rxq_init[i].flow_priority;
	}
}

/**
 * Finalise verbs flow attributes.
 *
 * @param[in, out] parser
 *   Internal parser structure.
 */
static void
mlx5_flow_convert_finalise(struct mlx5_flow_parse *parser)
{
	unsigned int i;
	uint32_t inner = parser->inner;

	/* Don't create extra flows for outer RSS. */
	if (parser->tunnel && parser->rss_conf.level < 2)
		return;
	/*
	 * Fill missing layers in verbs specifications, or compute the correct
	 * offset to allocate the memory space for the attributes and
	 * specifications.
	 */
	for (i = 0; i != hash_rxq_init_n - 1; ++i) {
		union {
			struct ibv_flow_spec_ipv4_ext ipv4;
			struct ibv_flow_spec_ipv6 ipv6;
			struct ibv_flow_spec_tcp_udp udp_tcp;
			struct ibv_flow_spec_eth eth;
		} specs;
		void *dst;
		uint16_t size;

		if (i == parser->layer)
			continue;
		if (parser->layer == HASH_RXQ_ETH ||
		    parser->layer == HASH_RXQ_TUNNEL) {
			if (hash_rxq_init[i].ip_version == MLX5_IPV4) {
				size = sizeof(struct ibv_flow_spec_ipv4_ext);
				specs.ipv4 = (struct ibv_flow_spec_ipv4_ext){
					.type = inner | IBV_FLOW_SPEC_IPV4_EXT,
					.size = size,
				};
			} else {
				size = sizeof(struct ibv_flow_spec_ipv6);
				specs.ipv6 = (struct ibv_flow_spec_ipv6){
					.type = inner | IBV_FLOW_SPEC_IPV6,
					.size = size,
				};
			}
			if (parser->queue[i].ibv_attr) {
				dst = (void *)((uintptr_t)
					       parser->queue[i].ibv_attr +
					       parser->queue[i].offset);
				memcpy(dst, &specs, size);
				++parser->queue[i].ibv_attr->num_of_specs;
			}
			parser->queue[i].offset += size;
		}
		if ((i == HASH_RXQ_UDPV4) || (i == HASH_RXQ_TCPV4) ||
		    (i == HASH_RXQ_UDPV6) || (i == HASH_RXQ_TCPV6)) {
			size = sizeof(struct ibv_flow_spec_tcp_udp);
			specs.udp_tcp = (struct ibv_flow_spec_tcp_udp) {
				.type = inner | ((i == HASH_RXQ_UDPV4 ||
					  i == HASH_RXQ_UDPV6) ?
					 IBV_FLOW_SPEC_UDP :
					 IBV_FLOW_SPEC_TCP),
				.size = size,
			};
			if (parser->queue[i].ibv_attr) {
				dst = (void *)((uintptr_t)
					       parser->queue[i].ibv_attr +
					       parser->queue[i].offset);
				memcpy(dst, &specs, size);
				++parser->queue[i].ibv_attr->num_of_specs;
			}
			parser->queue[i].offset += size;
		}
	}
}

/**
 * Update flows according to pattern and RSS hash fields.
 *
 * @param[in, out] parser
 *   Internal parser structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_convert_rss(struct mlx5_flow_parse *parser)
{
	unsigned int i;
	enum hash_rxq_type start;
	enum hash_rxq_type layer;
	int outer = parser->tunnel && parser->rss_conf.level < 2;
	uint64_t rss = parser->rss_conf.types;

	layer = outer ? parser->out_layer : parser->layer;
	if (layer == HASH_RXQ_TUNNEL)
		layer = HASH_RXQ_ETH;
	if (outer) {
		/* Only one hash type for outer RSS. */
		if (rss && layer == HASH_RXQ_ETH) {
			start = HASH_RXQ_TCPV4;
		} else if (rss && layer != HASH_RXQ_ETH &&
			   !(rss & hash_rxq_init[layer].dpdk_rss_hf)) {
			/* If RSS not match L4 pattern, try L3 RSS. */
			if (layer < HASH_RXQ_IPV4)
				layer = HASH_RXQ_IPV4;
			else if (layer > HASH_RXQ_IPV4 && layer < HASH_RXQ_IPV6)
				layer = HASH_RXQ_IPV6;
			start = layer;
		} else {
			start = layer;
		}
		/* Scan first valid hash type. */
		for (i = start; rss && i <= layer; ++i) {
			if (!parser->queue[i].ibv_attr)
				continue;
			if (hash_rxq_init[i].dpdk_rss_hf & rss)
				break;
		}
		if (rss && i <= layer)
			parser->queue[layer].hash_fields =
					hash_rxq_init[i].hash_fields;
		/* Trim unused hash types. */
		for (i = 0; i != hash_rxq_init_n; ++i) {
			if (parser->queue[i].ibv_attr && i != layer) {
				rte_free(parser->queue[i].ibv_attr);
				parser->queue[i].ibv_attr = NULL;
			}
		}
	} else {
		/* Expand for inner or normal RSS. */
		if (rss && (layer == HASH_RXQ_ETH || layer == HASH_RXQ_IPV4))
			start = HASH_RXQ_TCPV4;
		else if (rss && layer == HASH_RXQ_IPV6)
			start = HASH_RXQ_TCPV6;
		else
			start = layer;
		/* For L4 pattern, try L3 RSS if no L4 RSS. */
		/* Trim unused hash types. */
		for (i = 0; i != hash_rxq_init_n; ++i) {
			if (!parser->queue[i].ibv_attr)
				continue;
			if (i < start || i > layer) {
				rte_free(parser->queue[i].ibv_attr);
				parser->queue[i].ibv_attr = NULL;
				continue;
			}
			if (!rss)
				continue;
			if (hash_rxq_init[i].dpdk_rss_hf & rss) {
				parser->queue[i].hash_fields =
						hash_rxq_init[i].hash_fields;
			} else if (i != layer) {
				/* Remove unused RSS expansion. */
				rte_free(parser->queue[i].ibv_attr);
				parser->queue[i].ibv_attr = NULL;
			} else if (layer < HASH_RXQ_IPV4 &&
				   (hash_rxq_init[HASH_RXQ_IPV4].dpdk_rss_hf &
				    rss)) {
				/* Allow IPv4 RSS on L4 pattern. */
				parser->queue[i].hash_fields =
					hash_rxq_init[HASH_RXQ_IPV4]
						.hash_fields;
			} else if (i > HASH_RXQ_IPV4 && i < HASH_RXQ_IPV6 &&
				   (hash_rxq_init[HASH_RXQ_IPV6].dpdk_rss_hf &
				    rss)) {
				/* Allow IPv4 RSS on L4 pattern. */
				parser->queue[i].hash_fields =
					hash_rxq_init[HASH_RXQ_IPV6]
						.hash_fields;
			}
		}
	}
	return 0;
}

/**
 * Validate and convert a flow supported by the NIC.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @param[in, out] parser
 *   Internal parser structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_convert(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error,
		  struct mlx5_flow_parse *parser)
{
	const struct mlx5_flow_items *cur_item = mlx5_flow_items;
	unsigned int i;
	int ret;

	/* First step. Validate the attributes, items and actions. */
	*parser = (struct mlx5_flow_parse){
		.create = parser->create,
		.layer = HASH_RXQ_ETH,
		.mark_id = MLX5_FLOW_MARK_DEFAULT,
	};
	ret = mlx5_flow_convert_attributes(attr, error);
	if (ret)
		return ret;
	ret = mlx5_flow_convert_actions(dev, actions, error, parser);
	if (ret)
		return ret;
	ret = mlx5_flow_convert_items_validate(dev, items, error, parser);
	if (ret)
		return ret;
	mlx5_flow_convert_finalise(parser);
	/*
	 * Second step.
	 * Allocate the memory space to store verbs specifications.
	 */
	if (parser->drop) {
		unsigned int offset = parser->queue[HASH_RXQ_ETH].offset;

		parser->queue[HASH_RXQ_ETH].ibv_attr =
			mlx5_flow_convert_allocate(offset, error);
		if (!parser->queue[HASH_RXQ_ETH].ibv_attr)
			goto exit_enomem;
		parser->queue[HASH_RXQ_ETH].offset =
			sizeof(struct ibv_flow_attr);
	} else {
		for (i = 0; i != hash_rxq_init_n; ++i) {
			unsigned int offset;

			offset = parser->queue[i].offset;
			parser->queue[i].ibv_attr =
				mlx5_flow_convert_allocate(offset, error);
			if (!parser->queue[i].ibv_attr)
				goto exit_enomem;
			parser->queue[i].offset = sizeof(struct ibv_flow_attr);
		}
	}
	/* Third step. Conversion parse, fill the specifications. */
	parser->inner = 0;
	parser->tunnel = 0;
	parser->layer = HASH_RXQ_ETH;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; ++items) {
		struct mlx5_flow_data data = {
			.dev = dev,
			.parser = parser,
			.error = error,
		};

		if (items->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;
		cur_item = &mlx5_flow_items[items->type];
		ret = cur_item->convert(items,
					(cur_item->default_mask ?
					 cur_item->default_mask :
					 cur_item->mask),
					 &data);
		if (ret)
			goto exit_free;
	}
	if (!parser->drop) {
		/* RSS check, remove unused hash types. */
		ret = mlx5_flow_convert_rss(parser);
		if (ret)
			goto exit_free;
		/* Complete missing specification. */
		mlx5_flow_convert_finalise(parser);
	}
	mlx5_flow_update_priority(dev, parser, attr);
	if (parser->mark)
		mlx5_flow_create_flag_mark(parser, parser->mark_id);
	if (parser->count && parser->create) {
		mlx5_flow_create_count(dev, parser);
		if (!parser->cs)
			goto exit_count_error;
	}
exit_free:
	/* Only verification is expected, all resources should be released. */
	if (!parser->create) {
		for (i = 0; i != hash_rxq_init_n; ++i) {
			if (parser->queue[i].ibv_attr) {
				rte_free(parser->queue[i].ibv_attr);
				parser->queue[i].ibv_attr = NULL;
			}
		}
	}
	return ret;
exit_enomem:
	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (parser->queue[i].ibv_attr) {
			rte_free(parser->queue[i].ibv_attr);
			parser->queue[i].ibv_attr = NULL;
		}
	}
	rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, "cannot allocate verbs spec attributes");
	return -rte_errno;
exit_count_error:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, "cannot create counter");
	return -rte_errno;
}

/**
 * Copy the specification created into the flow.
 *
 * @param parser
 *   Internal parser structure.
 * @param src
 *   Create specification.
 * @param size
 *   Size in bytes of the specification to copy.
 */
static void
mlx5_flow_create_copy(struct mlx5_flow_parse *parser, void *src,
		      unsigned int size)
{
	unsigned int i;
	void *dst;

	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (!parser->queue[i].ibv_attr)
			continue;
		dst = (void *)((uintptr_t)parser->queue[i].ibv_attr +
				parser->queue[i].offset);
		memcpy(dst, src, size);
		++parser->queue[i].ibv_attr->num_of_specs;
		parser->queue[i].offset += size;
	}
}

/**
 * Convert Ethernet item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_eth(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	const unsigned int eth_size = sizeof(struct ibv_flow_spec_eth);
	struct ibv_flow_spec_eth eth = {
		.type = parser->inner | IBV_FLOW_SPEC_ETH,
		.size = eth_size,
	};

	parser->layer = HASH_RXQ_ETH;
	if (spec) {
		unsigned int i;

		if (!mask)
			mask = default_mask;
		memcpy(&eth.val.dst_mac, spec->dst.addr_bytes, ETHER_ADDR_LEN);
		memcpy(&eth.val.src_mac, spec->src.addr_bytes, ETHER_ADDR_LEN);
		eth.val.ether_type = spec->type;
		memcpy(&eth.mask.dst_mac, mask->dst.addr_bytes, ETHER_ADDR_LEN);
		memcpy(&eth.mask.src_mac, mask->src.addr_bytes, ETHER_ADDR_LEN);
		eth.mask.ether_type = mask->type;
		/* Remove unwanted bits from values. */
		for (i = 0; i < ETHER_ADDR_LEN; ++i) {
			eth.val.dst_mac[i] &= eth.mask.dst_mac[i];
			eth.val.src_mac[i] &= eth.mask.src_mac[i];
		}
		eth.val.ether_type &= eth.mask.ether_type;
	}
	mlx5_flow_create_copy(parser, &eth, eth_size);
	return 0;
}

/**
 * Convert VLAN item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_vlan(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	struct ibv_flow_spec_eth *eth;
	const unsigned int eth_size = sizeof(struct ibv_flow_spec_eth);
	const char *msg = "VLAN cannot be empty";

	if (spec) {
		unsigned int i;
		if (!mask)
			mask = default_mask;

		for (i = 0; i != hash_rxq_init_n; ++i) {
			if (!parser->queue[i].ibv_attr)
				continue;

			eth = (void *)((uintptr_t)parser->queue[i].ibv_attr +
				       parser->queue[i].offset - eth_size);
			eth->val.vlan_tag = spec->tci;
			eth->mask.vlan_tag = mask->tci;
			eth->val.vlan_tag &= eth->mask.vlan_tag;
			/*
			 * From verbs perspective an empty VLAN is equivalent
			 * to a packet without VLAN layer.
			 */
			if (!eth->mask.vlan_tag)
				goto error;
			/* Outer TPID cannot be matched. */
			if (eth->mask.ether_type) {
				msg = "VLAN TPID matching is not supported";
				goto error;
			}
			eth->val.ether_type = spec->inner_type;
			eth->mask.ether_type = mask->inner_type;
			eth->val.ether_type &= eth->mask.ether_type;
		}
		return 0;
	}
error:
	return rte_flow_error_set(data->error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, msg);
}

/**
 * Convert IPv4 item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_ipv4(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data)
{
	struct priv *priv = data->dev->data->dev_private;
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	unsigned int ipv4_size = sizeof(struct ibv_flow_spec_ipv4_ext);
	struct ibv_flow_spec_ipv4_ext ipv4 = {
		.type = parser->inner | IBV_FLOW_SPEC_IPV4_EXT,
		.size = ipv4_size,
	};

	if (parser->layer == HASH_RXQ_TUNNEL &&
	    parser->tunnel == ptype_ext[PTYPE_IDX(RTE_PTYPE_TUNNEL_VXLAN)] &&
	    !priv->config.l3_vxlan_en)
		return rte_flow_error_set(data->error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 VXLAN not enabled by device"
					  " parameter and/or not configured"
					  " in firmware");
	parser->layer = HASH_RXQ_IPV4;
	if (spec) {
		if (!mask)
			mask = default_mask;
		ipv4.val = (struct ibv_flow_ipv4_ext_filter){
			.src_ip = spec->hdr.src_addr,
			.dst_ip = spec->hdr.dst_addr,
			.proto = spec->hdr.next_proto_id,
			.tos = spec->hdr.type_of_service,
		};
		ipv4.mask = (struct ibv_flow_ipv4_ext_filter){
			.src_ip = mask->hdr.src_addr,
			.dst_ip = mask->hdr.dst_addr,
			.proto = mask->hdr.next_proto_id,
			.tos = mask->hdr.type_of_service,
		};
		/* Remove unwanted bits from values. */
		ipv4.val.src_ip &= ipv4.mask.src_ip;
		ipv4.val.dst_ip &= ipv4.mask.dst_ip;
		ipv4.val.proto &= ipv4.mask.proto;
		ipv4.val.tos &= ipv4.mask.tos;
	}
	mlx5_flow_create_copy(parser, &ipv4, ipv4_size);
	return 0;
}

/**
 * Convert IPv6 item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_ipv6(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data)
{
	struct priv *priv = data->dev->data->dev_private;
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	unsigned int ipv6_size = sizeof(struct ibv_flow_spec_ipv6);
	struct ibv_flow_spec_ipv6 ipv6 = {
		.type = parser->inner | IBV_FLOW_SPEC_IPV6,
		.size = ipv6_size,
	};

	if (parser->layer == HASH_RXQ_TUNNEL &&
	    parser->tunnel == ptype_ext[PTYPE_IDX(RTE_PTYPE_TUNNEL_VXLAN)] &&
	    !priv->config.l3_vxlan_en)
		return rte_flow_error_set(data->error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 VXLAN not enabled by device"
					  " parameter and/or not configured"
					  " in firmware");
	parser->layer = HASH_RXQ_IPV6;
	if (spec) {
		unsigned int i;
		uint32_t vtc_flow_val;
		uint32_t vtc_flow_mask;

		if (!mask)
			mask = default_mask;
		memcpy(&ipv6.val.src_ip, spec->hdr.src_addr,
		       RTE_DIM(ipv6.val.src_ip));
		memcpy(&ipv6.val.dst_ip, spec->hdr.dst_addr,
		       RTE_DIM(ipv6.val.dst_ip));
		memcpy(&ipv6.mask.src_ip, mask->hdr.src_addr,
		       RTE_DIM(ipv6.mask.src_ip));
		memcpy(&ipv6.mask.dst_ip, mask->hdr.dst_addr,
		       RTE_DIM(ipv6.mask.dst_ip));
		vtc_flow_val = rte_be_to_cpu_32(spec->hdr.vtc_flow);
		vtc_flow_mask = rte_be_to_cpu_32(mask->hdr.vtc_flow);
		ipv6.val.flow_label =
			rte_cpu_to_be_32((vtc_flow_val & IPV6_HDR_FL_MASK) >>
					 IPV6_HDR_FL_SHIFT);
		ipv6.val.traffic_class = (vtc_flow_val & IPV6_HDR_TC_MASK) >>
					 IPV6_HDR_TC_SHIFT;
		ipv6.val.next_hdr = spec->hdr.proto;
		ipv6.val.hop_limit = spec->hdr.hop_limits;
		ipv6.mask.flow_label =
			rte_cpu_to_be_32((vtc_flow_mask & IPV6_HDR_FL_MASK) >>
					 IPV6_HDR_FL_SHIFT);
		ipv6.mask.traffic_class = (vtc_flow_mask & IPV6_HDR_TC_MASK) >>
					  IPV6_HDR_TC_SHIFT;
		ipv6.mask.next_hdr = mask->hdr.proto;
		ipv6.mask.hop_limit = mask->hdr.hop_limits;
		/* Remove unwanted bits from values. */
		for (i = 0; i < RTE_DIM(ipv6.val.src_ip); ++i) {
			ipv6.val.src_ip[i] &= ipv6.mask.src_ip[i];
			ipv6.val.dst_ip[i] &= ipv6.mask.dst_ip[i];
		}
		ipv6.val.flow_label &= ipv6.mask.flow_label;
		ipv6.val.traffic_class &= ipv6.mask.traffic_class;
		ipv6.val.next_hdr &= ipv6.mask.next_hdr;
		ipv6.val.hop_limit &= ipv6.mask.hop_limit;
	}
	mlx5_flow_create_copy(parser, &ipv6, ipv6_size);
	return 0;
}

/**
 * Convert UDP item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_udp(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	unsigned int udp_size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp udp = {
		.type = parser->inner | IBV_FLOW_SPEC_UDP,
		.size = udp_size,
	};

	if (parser->layer == HASH_RXQ_IPV4)
		parser->layer = HASH_RXQ_UDPV4;
	else
		parser->layer = HASH_RXQ_UDPV6;
	if (spec) {
		if (!mask)
			mask = default_mask;
		udp.val.dst_port = spec->hdr.dst_port;
		udp.val.src_port = spec->hdr.src_port;
		udp.mask.dst_port = mask->hdr.dst_port;
		udp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		udp.val.src_port &= udp.mask.src_port;
		udp.val.dst_port &= udp.mask.dst_port;
	}
	mlx5_flow_create_copy(parser, &udp, udp_size);
	return 0;
}

/**
 * Convert TCP item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_tcp(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	unsigned int tcp_size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp tcp = {
		.type = parser->inner | IBV_FLOW_SPEC_TCP,
		.size = tcp_size,
	};

	if (parser->layer == HASH_RXQ_IPV4)
		parser->layer = HASH_RXQ_TCPV4;
	else
		parser->layer = HASH_RXQ_TCPV6;
	if (spec) {
		if (!mask)
			mask = default_mask;
		tcp.val.dst_port = spec->hdr.dst_port;
		tcp.val.src_port = spec->hdr.src_port;
		tcp.mask.dst_port = mask->hdr.dst_port;
		tcp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		tcp.val.src_port &= tcp.mask.src_port;
		tcp.val.dst_port &= tcp.mask.dst_port;
	}
	mlx5_flow_create_copy(parser, &tcp, tcp_size);
	return 0;
}

/**
 * Convert VXLAN item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_vxlan(const struct rte_flow_item *item,
		       const void *default_mask,
		       struct mlx5_flow_data *data)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel vxlan = {
		.type = parser->inner | IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id;

	id.vni[0] = 0;
	parser->inner = IBV_FLOW_SPEC_INNER;
	parser->tunnel = ptype_ext[PTYPE_IDX(RTE_PTYPE_TUNNEL_VXLAN)];
	parser->out_layer = parser->layer;
	parser->layer = HASH_RXQ_TUNNEL;
	if (spec) {
		if (!mask)
			mask = default_mask;
		memcpy(&id.vni[1], spec->vni, 3);
		vxlan.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vxlan.mask.tunnel_id = id.vlan_id;
		/* Remove unwanted bits from values. */
		vxlan.val.tunnel_id &= vxlan.mask.tunnel_id;
	}
	/*
	 * Tunnel id 0 is equivalent as not adding a VXLAN layer, if only this
	 * layer is defined in the Verbs specification it is interpreted as
	 * wildcard and all packets will match this rule, if it follows a full
	 * stack layer (ex: eth / ipv4 / udp), all packets matching the layers
	 * before will also match this rule.
	 * To avoid such situation, VNI 0 is currently refused.
	 */
	/* Only allow tunnel w/o tunnel id pattern after proper outer spec. */
	if (parser->out_layer == HASH_RXQ_ETH && !vxlan.val.tunnel_id)
		return rte_flow_error_set(data->error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "VxLAN vni cannot be 0");
	mlx5_flow_create_copy(parser, &vxlan, size);
	return 0;
}

/**
 * Convert VXLAN-GPE item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_vxlan_gpe(const struct rte_flow_item *item,
			   const void *default_mask,
			   struct mlx5_flow_data *data)
{
	struct priv *priv = data->dev->data->dev_private;
	const struct rte_flow_item_vxlan_gpe *spec = item->spec;
	const struct rte_flow_item_vxlan_gpe *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel vxlan = {
		.type = parser->inner | IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id;

	if (!priv->config.l3_vxlan_en)
		return rte_flow_error_set(data->error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 VXLAN not enabled by device"
					  " parameter and/or not configured"
					  " in firmware");
	id.vni[0] = 0;
	parser->inner = IBV_FLOW_SPEC_INNER;
	parser->tunnel = ptype_ext[PTYPE_IDX(RTE_PTYPE_TUNNEL_VXLAN_GPE)];
	parser->out_layer = parser->layer;
	parser->layer = HASH_RXQ_TUNNEL;
	if (spec) {
		if (!mask)
			mask = default_mask;
		memcpy(&id.vni[1], spec->vni, 3);
		vxlan.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vxlan.mask.tunnel_id = id.vlan_id;
		if (spec->protocol)
			return rte_flow_error_set(data->error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "VxLAN-GPE protocol not"
						  " supported");
		/* Remove unwanted bits from values. */
		vxlan.val.tunnel_id &= vxlan.mask.tunnel_id;
	}
	/*
	 * Tunnel id 0 is equivalent as not adding a VXLAN layer, if only this
	 * layer is defined in the Verbs specification it is interpreted as
	 * wildcard and all packets will match this rule, if it follows a full
	 * stack layer (ex: eth / ipv4 / udp), all packets matching the layers
	 * before will also match this rule.
	 * To avoid such situation, VNI 0 is currently refused.
	 */
	/* Only allow tunnel w/o tunnel id pattern after proper outer spec. */
	if (parser->out_layer == HASH_RXQ_ETH && !vxlan.val.tunnel_id)
		return rte_flow_error_set(data->error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "VxLAN-GPE vni cannot be 0");
	mlx5_flow_create_copy(parser, &vxlan, size);
	return 0;
}

/**
 * Convert GRE item to Verbs specification.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_gre(const struct rte_flow_item *item,
		     const void *default_mask,
		     struct mlx5_flow_data *data)
{
	struct mlx5_flow_parse *parser = data->parser;
#ifndef HAVE_IBV_DEVICE_MPLS_SUPPORT
	(void)default_mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel tunnel = {
		.type = parser->inner | IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
#else
	const struct rte_flow_item_gre *spec = item->spec;
	const struct rte_flow_item_gre *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_gre);
	struct ibv_flow_spec_gre tunnel = {
		.type = parser->inner | IBV_FLOW_SPEC_GRE,
		.size = size,
	};
#endif
	struct ibv_flow_spec_ipv4_ext *ipv4;
	struct ibv_flow_spec_ipv6 *ipv6;
	unsigned int i;

	parser->inner = IBV_FLOW_SPEC_INNER;
	parser->tunnel = ptype_ext[PTYPE_IDX(RTE_PTYPE_TUNNEL_GRE)];
	parser->out_layer = parser->layer;
	parser->layer = HASH_RXQ_TUNNEL;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	if (spec) {
		if (!mask)
			mask = default_mask;
		tunnel.val.c_ks_res0_ver = spec->c_rsvd0_ver;
		tunnel.val.protocol = spec->protocol;
		tunnel.mask.c_ks_res0_ver = mask->c_rsvd0_ver;
		tunnel.mask.protocol = mask->protocol;
		/* Remove unwanted bits from values. */
		tunnel.val.c_ks_res0_ver &= tunnel.mask.c_ks_res0_ver;
		tunnel.val.protocol &= tunnel.mask.protocol;
		tunnel.val.key &= tunnel.mask.key;
	}
#endif
	/* Update encapsulation IP layer protocol. */
	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (!parser->queue[i].ibv_attr)
			continue;
		if (parser->out_layer == HASH_RXQ_IPV4) {
			ipv4 = (void *)((uintptr_t)parser->queue[i].ibv_attr +
				parser->queue[i].offset -
				sizeof(struct ibv_flow_spec_ipv4_ext));
			if (ipv4->mask.proto && ipv4->val.proto != MLX5_GRE)
				break;
			ipv4->val.proto = MLX5_GRE;
			ipv4->mask.proto = 0xff;
		} else if (parser->out_layer == HASH_RXQ_IPV6) {
			ipv6 = (void *)((uintptr_t)parser->queue[i].ibv_attr +
				parser->queue[i].offset -
				sizeof(struct ibv_flow_spec_ipv6));
			if (ipv6->mask.next_hdr &&
			    ipv6->val.next_hdr != MLX5_GRE)
				break;
			ipv6->val.next_hdr = MLX5_GRE;
			ipv6->mask.next_hdr = 0xff;
		}
	}
	if (i != hash_rxq_init_n)
		return rte_flow_error_set(data->error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "IP protocol of GRE must be 47");
	mlx5_flow_create_copy(parser, &tunnel, size);
	return 0;
}

/**
 * Convert MPLS item to Verbs specification.
 * MPLS tunnel types currently supported are MPLS-in-GRE and MPLS-in-UDP.
 *
 * @param item[in]
 *   Item specification.
 * @param default_mask[in]
 *   Default bit-masks to use when item->mask is not provided.
 * @param data[in, out]
 *   User structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_mpls(const struct rte_flow_item *item,
		      const void *default_mask,
		      struct mlx5_flow_data *data)
{
#ifndef HAVE_IBV_DEVICE_MPLS_SUPPORT
	(void)default_mask;
	return rte_flow_error_set(data->error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_ITEM,
				  item,
				  "MPLS is not supported by driver");
#else
	const struct rte_flow_item_mpls *spec = item->spec;
	const struct rte_flow_item_mpls *mask = item->mask;
	struct mlx5_flow_parse *parser = data->parser;
	unsigned int size = sizeof(struct ibv_flow_spec_mpls);
	struct ibv_flow_spec_mpls mpls = {
		.type = IBV_FLOW_SPEC_MPLS,
		.size = size,
	};

	parser->inner = IBV_FLOW_SPEC_INNER;
	if (parser->layer == HASH_RXQ_UDPV4 ||
	    parser->layer == HASH_RXQ_UDPV6) {
		parser->tunnel =
			ptype_ext[PTYPE_IDX(RTE_PTYPE_TUNNEL_MPLS_IN_UDP)];
		parser->out_layer = parser->layer;
	} else {
		parser->tunnel =
			ptype_ext[PTYPE_IDX(RTE_PTYPE_TUNNEL_MPLS_IN_GRE)];
		/* parser->out_layer stays as in GRE out_layer. */
	}
	parser->layer = HASH_RXQ_TUNNEL;
	if (spec) {
		if (!mask)
			mask = default_mask;
		/*
		 * The verbs label field includes the entire MPLS header:
		 * bits 0:19 - label value field.
		 * bits 20:22 - traffic class field.
		 * bits 23 - bottom of stack bit.
		 * bits 24:31 - ttl field.
		 */
		mpls.val.label = *(const uint32_t *)spec;
		mpls.mask.label = *(const uint32_t *)mask;
		/* Remove unwanted bits from values. */
		mpls.val.label &= mpls.mask.label;
	}
	mlx5_flow_create_copy(parser, &mpls, size);
	return 0;
#endif
}

/**
 * Convert mark/flag action to Verbs specification.
 *
 * @param parser
 *   Internal parser structure.
 * @param mark_id
 *   Mark identifier.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_flag_mark(struct mlx5_flow_parse *parser, uint32_t mark_id)
{
	unsigned int size = sizeof(struct ibv_flow_spec_action_tag);
	struct ibv_flow_spec_action_tag tag = {
		.type = IBV_FLOW_SPEC_ACTION_TAG,
		.size = size,
		.tag_id = mlx5_flow_mark_set(mark_id),
	};

	assert(parser->mark);
	mlx5_flow_create_copy(parser, &tag, size);
	return 0;
}

/**
 * Convert count action to Verbs specification.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param parser
 *   Pointer to MLX5 flow parser structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_count(struct rte_eth_dev *dev __rte_unused,
		       struct mlx5_flow_parse *parser __rte_unused)
{
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	struct priv *priv = dev->data->dev_private;
	unsigned int size = sizeof(struct ibv_flow_spec_counter_action);
	struct ibv_counter_set_init_attr init_attr = {0};
	struct ibv_flow_spec_counter_action counter = {
		.type = IBV_FLOW_SPEC_ACTION_COUNT,
		.size = size,
		.counter_set_handle = 0,
	};

	init_attr.counter_set_id = 0;
	parser->cs = mlx5_glue->create_counter_set(priv->ctx, &init_attr);
	if (!parser->cs) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	counter.counter_set_handle = parser->cs->handle;
	mlx5_flow_create_copy(parser, &counter, size);
#endif
	return 0;
}

/**
 * Complete flow rule creation with a drop queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param parser
 *   Internal parser structure.
 * @param flow
 *   Pointer to the rte_flow.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_action_queue_drop(struct rte_eth_dev *dev,
				   struct mlx5_flow_parse *parser,
				   struct rte_flow *flow,
				   struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	struct ibv_flow_spec_action_drop *drop;
	unsigned int size = sizeof(struct ibv_flow_spec_action_drop);

	assert(priv->pd);
	assert(priv->ctx);
	flow->drop = 1;
	drop = (void *)((uintptr_t)parser->queue[HASH_RXQ_ETH].ibv_attr +
			parser->queue[HASH_RXQ_ETH].offset);
	*drop = (struct ibv_flow_spec_action_drop){
			.type = IBV_FLOW_SPEC_ACTION_DROP,
			.size = size,
	};
	++parser->queue[HASH_RXQ_ETH].ibv_attr->num_of_specs;
	parser->queue[HASH_RXQ_ETH].offset += size;
	flow->frxq[HASH_RXQ_ETH].ibv_attr =
		parser->queue[HASH_RXQ_ETH].ibv_attr;
	if (parser->count)
		flow->cs = parser->cs;
	if (!dev->data->dev_started)
		return 0;
	parser->queue[HASH_RXQ_ETH].ibv_attr = NULL;
	flow->frxq[HASH_RXQ_ETH].ibv_flow =
		mlx5_glue->create_flow(priv->flow_drop_queue->qp,
				       flow->frxq[HASH_RXQ_ETH].ibv_attr);
	if (!flow->frxq[HASH_RXQ_ETH].ibv_flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "flow rule creation failure");
		goto error;
	}
	return 0;
error:
	assert(flow);
	if (flow->frxq[HASH_RXQ_ETH].ibv_flow) {
		claim_zero(mlx5_glue->destroy_flow
			   (flow->frxq[HASH_RXQ_ETH].ibv_flow));
		flow->frxq[HASH_RXQ_ETH].ibv_flow = NULL;
	}
	if (flow->frxq[HASH_RXQ_ETH].ibv_attr) {
		rte_free(flow->frxq[HASH_RXQ_ETH].ibv_attr);
		flow->frxq[HASH_RXQ_ETH].ibv_attr = NULL;
	}
	if (flow->cs) {
		claim_zero(mlx5_glue->destroy_counter_set(flow->cs));
		flow->cs = NULL;
		parser->cs = NULL;
	}
	return -rte_errno;
}

/**
 * Create hash Rx queues when RSS is enabled.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param parser
 *   Internal parser structure.
 * @param flow
 *   Pointer to the rte_flow.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_action_queue_rss(struct rte_eth_dev *dev,
				  struct mlx5_flow_parse *parser,
				  struct rte_flow *flow,
				  struct rte_flow_error *error)
{
	unsigned int i;

	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (!parser->queue[i].ibv_attr)
			continue;
		flow->frxq[i].ibv_attr = parser->queue[i].ibv_attr;
		parser->queue[i].ibv_attr = NULL;
		flow->frxq[i].hash_fields = parser->queue[i].hash_fields;
		if (!dev->data->dev_started)
			continue;
		flow->frxq[i].hrxq =
			mlx5_hrxq_get(dev,
				      parser->rss_conf.key,
				      parser->rss_conf.key_len,
				      flow->frxq[i].hash_fields,
				      parser->rss_conf.queue,
				      parser->rss_conf.queue_num,
				      parser->tunnel,
				      parser->rss_conf.level);
		if (flow->frxq[i].hrxq)
			continue;
		flow->frxq[i].hrxq =
			mlx5_hrxq_new(dev,
				      parser->rss_conf.key,
				      parser->rss_conf.key_len,
				      flow->frxq[i].hash_fields,
				      parser->rss_conf.queue,
				      parser->rss_conf.queue_num,
				      parser->tunnel,
				      parser->rss_conf.level);
		if (!flow->frxq[i].hrxq) {
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_HANDLE,
						  NULL,
						  "cannot create hash rxq");
		}
	}
	return 0;
}

/**
 * RXQ update after flow rule creation.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param flow
 *   Pointer to the flow rule.
 */
static void
mlx5_flow_create_update_rxqs(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int j;

	if (!dev->data->dev_started)
		return;
	for (i = 0; i != flow->rss_conf.queue_num; ++i) {
		struct mlx5_rxq_data *rxq_data = (*priv->rxqs)
						 [(*flow->queues)[i]];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
		uint8_t tunnel = PTYPE_IDX(flow->tunnel);

		rxq_data->mark |= flow->mark;
		if (!tunnel)
			continue;
		rxq_ctrl->tunnel_types[tunnel] += 1;
		/* Clear tunnel type if more than one tunnel types set. */
		for (j = 0; j != RTE_DIM(rxq_ctrl->tunnel_types); ++j) {
			if (j == tunnel)
				continue;
			if (rxq_ctrl->tunnel_types[j] > 0) {
				rxq_data->tunnel = 0;
				break;
			}
		}
		if (j == RTE_DIM(rxq_ctrl->tunnel_types))
			rxq_data->tunnel = flow->tunnel;
	}
}

/**
 * Dump flow hash RX queue detail.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param flow
 *   Pointer to the rte_flow.
 * @param hrxq_idx
 *   Hash RX queue index.
 */
static void
mlx5_flow_dump(struct rte_eth_dev *dev __rte_unused,
	       struct rte_flow *flow __rte_unused,
	       unsigned int hrxq_idx __rte_unused)
{
#ifndef NDEBUG
	uintptr_t spec_ptr;
	uint16_t j;
	char buf[256];
	uint8_t off;
	uint64_t extra_hash_fields = 0;

#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (flow->tunnel && flow->rss_conf.level > 1)
		extra_hash_fields = (uint32_t)IBV_RX_HASH_INNER;
#endif
	spec_ptr = (uintptr_t)(flow->frxq[hrxq_idx].ibv_attr + 1);
	for (j = 0, off = 0; j < flow->frxq[hrxq_idx].ibv_attr->num_of_specs;
	     j++) {
		struct ibv_flow_spec *spec = (void *)spec_ptr;
		off += sprintf(buf + off, " %x(%hu)", spec->hdr.type,
			       spec->hdr.size);
		spec_ptr += spec->hdr.size;
	}
	DRV_LOG(DEBUG,
		"port %u Verbs flow %p type %u: hrxq:%p qp:%p ind:%p,"
		" hash:%" PRIx64 "/%u specs:%hhu(%hu), priority:%hu, type:%d,"
		" flags:%x, comp_mask:%x specs:%s",
		dev->data->port_id, (void *)flow, hrxq_idx,
		(void *)flow->frxq[hrxq_idx].hrxq,
		(void *)flow->frxq[hrxq_idx].hrxq->qp,
		(void *)flow->frxq[hrxq_idx].hrxq->ind_table,
		(flow->frxq[hrxq_idx].hash_fields | extra_hash_fields),
		flow->rss_conf.queue_num,
		flow->frxq[hrxq_idx].ibv_attr->num_of_specs,
		flow->frxq[hrxq_idx].ibv_attr->size,
		flow->frxq[hrxq_idx].ibv_attr->priority,
		flow->frxq[hrxq_idx].ibv_attr->type,
		flow->frxq[hrxq_idx].ibv_attr->flags,
		flow->frxq[hrxq_idx].ibv_attr->comp_mask,
		buf);
#endif
}

/**
 * Complete flow rule creation.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param parser
 *   Internal parser structure.
 * @param flow
 *   Pointer to the rte_flow.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_create_action_queue(struct rte_eth_dev *dev,
			      struct mlx5_flow_parse *parser,
			      struct rte_flow *flow,
			      struct rte_flow_error *error)
{
	struct priv *priv __rte_unused = dev->data->dev_private;
	int ret;
	unsigned int i;
	unsigned int flows_n = 0;

	assert(priv->pd);
	assert(priv->ctx);
	assert(!parser->drop);
	ret = mlx5_flow_create_action_queue_rss(dev, parser, flow, error);
	if (ret)
		goto error;
	if (parser->count)
		flow->cs = parser->cs;
	if (!dev->data->dev_started)
		return 0;
	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (!flow->frxq[i].hrxq)
			continue;
		flow->frxq[i].ibv_flow =
			mlx5_glue->create_flow(flow->frxq[i].hrxq->qp,
					       flow->frxq[i].ibv_attr);
		mlx5_flow_dump(dev, flow, i);
		if (!flow->frxq[i].ibv_flow) {
			rte_flow_error_set(error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL, "flow rule creation failure");
			goto error;
		}
		++flows_n;
	}
	if (!flows_n) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "internal error in flow creation");
		goto error;
	}
	mlx5_flow_create_update_rxqs(dev, flow);
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	assert(flow);
	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (flow->frxq[i].ibv_flow) {
			struct ibv_flow *ibv_flow = flow->frxq[i].ibv_flow;

			claim_zero(mlx5_glue->destroy_flow(ibv_flow));
		}
		if (flow->frxq[i].hrxq)
			mlx5_hrxq_release(dev, flow->frxq[i].hrxq);
		if (flow->frxq[i].ibv_attr)
			rte_free(flow->frxq[i].ibv_attr);
	}
	if (flow->cs) {
		claim_zero(mlx5_glue->destroy_counter_set(flow->cs));
		flow->cs = NULL;
		parser->cs = NULL;
	}
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Convert a flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A flow on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow *
mlx5_flow_list_create(struct rte_eth_dev *dev,
		      struct mlx5_flows *list,
		      const struct rte_flow_attr *attr,
		      const struct rte_flow_item items[],
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error)
{
	struct mlx5_flow_parse parser = { .create = 1, };
	struct rte_flow *flow = NULL;
	unsigned int i;
	int ret;

	ret = mlx5_flow_convert(dev, attr, items, actions, error, &parser);
	if (ret)
		goto exit;
	flow = rte_calloc(__func__, 1,
			  sizeof(*flow) +
			  parser.rss_conf.queue_num * sizeof(uint16_t),
			  0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow memory");
		return NULL;
	}
	/* Copy configuration. */
	flow->queues = (uint16_t (*)[])(flow + 1);
	flow->tunnel = parser.tunnel;
	flow->rss_conf = (struct rte_flow_action_rss){
		.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
		.level = parser.rss_conf.level,
		.types = parser.rss_conf.types,
		.key_len = parser.rss_conf.key_len,
		.queue_num = parser.rss_conf.queue_num,
		.key = memcpy(flow->rss_key, parser.rss_conf.key,
			      sizeof(*parser.rss_conf.key) *
			      parser.rss_conf.key_len),
		.queue = memcpy(flow->queues, parser.rss_conf.queue,
				sizeof(*parser.rss_conf.queue) *
				parser.rss_conf.queue_num),
	};
	flow->mark = parser.mark;
	/* finalise the flow. */
	if (parser.drop)
		ret = mlx5_flow_create_action_queue_drop(dev, &parser, flow,
							 error);
	else
		ret = mlx5_flow_create_action_queue(dev, &parser, flow, error);
	if (ret)
		goto exit;
	TAILQ_INSERT_TAIL(list, flow, next);
	DRV_LOG(DEBUG, "port %u flow created %p", dev->data->port_id,
		(void *)flow);
	return flow;
exit:
	DRV_LOG(ERR, "port %u flow creation error: %s", dev->data->port_id,
		error->message);
	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (parser.queue[i].ibv_attr)
			rte_free(parser.queue[i].ibv_attr);
	}
	rte_free(flow);
	return NULL;
}

/**
 * Validate a flow supported by the NIC.
 *
 * @see rte_flow_validate()
 * @see rte_flow_ops
 */
int
mlx5_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct mlx5_flow_parse parser = { .create = 0, };

	return mlx5_flow_convert(dev, attr, items, actions, error, &parser);
}

/**
 * Create a flow.
 *
 * @see rte_flow_create()
 * @see rte_flow_ops
 */
struct rte_flow *
mlx5_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;

	return mlx5_flow_list_create(dev, &priv->flows, attr, items, actions,
				     error);
}

/**
 * Destroy a flow in a list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 * @param[in] flow
 *   Flow to destroy.
 */
static void
mlx5_flow_list_destroy(struct rte_eth_dev *dev, struct mlx5_flows *list,
		       struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;

	if (flow->drop || !dev->data->dev_started)
		goto free;
	for (i = 0; flow->tunnel && i != flow->rss_conf.queue_num; ++i) {
		/* Update queue tunnel type. */
		struct mlx5_rxq_data *rxq_data = (*priv->rxqs)
						 [(*flow->queues)[i]];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
		uint8_t tunnel = PTYPE_IDX(flow->tunnel);

		assert(rxq_ctrl->tunnel_types[tunnel] > 0);
		rxq_ctrl->tunnel_types[tunnel] -= 1;
		if (!rxq_ctrl->tunnel_types[tunnel]) {
			/* Update tunnel type. */
			uint8_t j;
			uint8_t types = 0;
			uint8_t last;

			for (j = 0; j < RTE_DIM(rxq_ctrl->tunnel_types); j++)
				if (rxq_ctrl->tunnel_types[j]) {
					types += 1;
					last = j;
				}
			/* Keep same if more than one tunnel types left. */
			if (types == 1)
				rxq_data->tunnel = ptype_ext[last];
			else if (types == 0)
				/* No tunnel type left. */
				rxq_data->tunnel = 0;
		}
	}
	for (i = 0; flow->mark && i != flow->rss_conf.queue_num; ++i) {
		struct rte_flow *tmp;
		int mark = 0;

		/*
		 * To remove the mark from the queue, the queue must not be
		 * present in any other marked flow (RSS or not).
		 */
		TAILQ_FOREACH(tmp, list, next) {
			unsigned int j;
			uint16_t *tqs = NULL;
			uint16_t tq_n = 0;

			if (!tmp->mark)
				continue;
			for (j = 0; j != hash_rxq_init_n; ++j) {
				if (!tmp->frxq[j].hrxq)
					continue;
				tqs = tmp->frxq[j].hrxq->ind_table->queues;
				tq_n = tmp->frxq[j].hrxq->ind_table->queues_n;
			}
			if (!tq_n)
				continue;
			for (j = 0; (j != tq_n) && !mark; j++)
				if (tqs[j] == (*flow->queues)[i])
					mark = 1;
		}
		(*priv->rxqs)[(*flow->queues)[i]]->mark = mark;
	}
free:
	if (flow->drop) {
		if (flow->frxq[HASH_RXQ_ETH].ibv_flow)
			claim_zero(mlx5_glue->destroy_flow
				   (flow->frxq[HASH_RXQ_ETH].ibv_flow));
		rte_free(flow->frxq[HASH_RXQ_ETH].ibv_attr);
	} else {
		for (i = 0; i != hash_rxq_init_n; ++i) {
			struct mlx5_flow *frxq = &flow->frxq[i];

			if (frxq->ibv_flow)
				claim_zero(mlx5_glue->destroy_flow
					   (frxq->ibv_flow));
			if (frxq->hrxq)
				mlx5_hrxq_release(dev, frxq->hrxq);
			if (frxq->ibv_attr)
				rte_free(frxq->ibv_attr);
		}
	}
	if (flow->cs) {
		claim_zero(mlx5_glue->destroy_counter_set(flow->cs));
		flow->cs = NULL;
	}
	TAILQ_REMOVE(list, flow, next);
	DRV_LOG(DEBUG, "port %u flow destroyed %p", dev->data->port_id,
		(void *)flow);
	rte_free(flow);
}

/**
 * Destroy all flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 */
void
mlx5_flow_list_flush(struct rte_eth_dev *dev, struct mlx5_flows *list)
{
	while (!TAILQ_EMPTY(list)) {
		struct rte_flow *flow;

		flow = TAILQ_FIRST(list);
		mlx5_flow_list_destroy(dev, list, flow);
	}
}

/**
 * Create drop queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_create_drop_queue(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_hrxq_drop *fdq = NULL;

	assert(priv->pd);
	assert(priv->ctx);
	fdq = rte_calloc(__func__, 1, sizeof(*fdq), 0);
	if (!fdq) {
		DRV_LOG(WARNING,
			"port %u cannot allocate memory for drop queue",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	fdq->cq = mlx5_glue->create_cq(priv->ctx, 1, NULL, NULL, 0);
	if (!fdq->cq) {
		DRV_LOG(WARNING, "port %u cannot allocate CQ for drop queue",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	fdq->wq = mlx5_glue->create_wq
		(priv->ctx,
		 &(struct ibv_wq_init_attr){
			.wq_type = IBV_WQT_RQ,
			.max_wr = 1,
			.max_sge = 1,
			.pd = priv->pd,
			.cq = fdq->cq,
		 });
	if (!fdq->wq) {
		DRV_LOG(WARNING, "port %u cannot allocate WQ for drop queue",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	fdq->ind_table = mlx5_glue->create_rwq_ind_table
		(priv->ctx,
		 &(struct ibv_rwq_ind_table_init_attr){
			.log_ind_tbl_size = 0,
			.ind_tbl = &fdq->wq,
			.comp_mask = 0,
		 });
	if (!fdq->ind_table) {
		DRV_LOG(WARNING,
			"port %u cannot allocate indirection table for drop"
			" queue",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	fdq->qp = mlx5_glue->create_qp_ex
		(priv->ctx,
		 &(struct ibv_qp_init_attr_ex){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask =
				IBV_QP_INIT_ATTR_PD |
				IBV_QP_INIT_ATTR_IND_TABLE |
				IBV_QP_INIT_ATTR_RX_HASH,
			.rx_hash_conf = (struct ibv_rx_hash_conf){
				.rx_hash_function =
					IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = rss_hash_default_key_len,
				.rx_hash_key = rss_hash_default_key,
				.rx_hash_fields_mask = 0,
				},
			.rwq_ind_tbl = fdq->ind_table,
			.pd = priv->pd
		 });
	if (!fdq->qp) {
		DRV_LOG(WARNING, "port %u cannot allocate QP for drop queue",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	priv->flow_drop_queue = fdq;
	return 0;
error:
	if (fdq->qp)
		claim_zero(mlx5_glue->destroy_qp(fdq->qp));
	if (fdq->ind_table)
		claim_zero(mlx5_glue->destroy_rwq_ind_table(fdq->ind_table));
	if (fdq->wq)
		claim_zero(mlx5_glue->destroy_wq(fdq->wq));
	if (fdq->cq)
		claim_zero(mlx5_glue->destroy_cq(fdq->cq));
	if (fdq)
		rte_free(fdq);
	priv->flow_drop_queue = NULL;
	return -rte_errno;
}

/**
 * Delete drop queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_flow_delete_drop_queue(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_hrxq_drop *fdq = priv->flow_drop_queue;

	if (!fdq)
		return;
	if (fdq->qp)
		claim_zero(mlx5_glue->destroy_qp(fdq->qp));
	if (fdq->ind_table)
		claim_zero(mlx5_glue->destroy_rwq_ind_table(fdq->ind_table));
	if (fdq->wq)
		claim_zero(mlx5_glue->destroy_wq(fdq->wq));
	if (fdq->cq)
		claim_zero(mlx5_glue->destroy_cq(fdq->cq));
	rte_free(fdq);
	priv->flow_drop_queue = NULL;
}

/**
 * Remove all flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 */
void
mlx5_flow_stop(struct rte_eth_dev *dev, struct mlx5_flows *list)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow *flow;
	unsigned int i;

	TAILQ_FOREACH_REVERSE(flow, list, mlx5_flows, next) {
		struct mlx5_ind_table_ibv *ind_tbl = NULL;

		if (flow->drop) {
			if (!flow->frxq[HASH_RXQ_ETH].ibv_flow)
				continue;
			claim_zero(mlx5_glue->destroy_flow
				   (flow->frxq[HASH_RXQ_ETH].ibv_flow));
			flow->frxq[HASH_RXQ_ETH].ibv_flow = NULL;
			DRV_LOG(DEBUG, "port %u flow %p removed",
				dev->data->port_id, (void *)flow);
			/* Next flow. */
			continue;
		}
		/* Verify the flow has not already been cleaned. */
		for (i = 0; i != hash_rxq_init_n; ++i) {
			if (!flow->frxq[i].ibv_flow)
				continue;
			/*
			 * Indirection table may be necessary to remove the
			 * flags in the Rx queues.
			 * This helps to speed-up the process by avoiding
			 * another loop.
			 */
			ind_tbl = flow->frxq[i].hrxq->ind_table;
			break;
		}
		if (i == hash_rxq_init_n)
			return;
		if (flow->mark) {
			assert(ind_tbl);
			for (i = 0; i != ind_tbl->queues_n; ++i)
				(*priv->rxqs)[ind_tbl->queues[i]]->mark = 0;
		}
		for (i = 0; i != hash_rxq_init_n; ++i) {
			if (!flow->frxq[i].ibv_flow)
				continue;
			claim_zero(mlx5_glue->destroy_flow
				   (flow->frxq[i].ibv_flow));
			flow->frxq[i].ibv_flow = NULL;
			mlx5_hrxq_release(dev, flow->frxq[i].hrxq);
			flow->frxq[i].hrxq = NULL;
		}
		DRV_LOG(DEBUG, "port %u flow %p removed", dev->data->port_id,
			(void *)flow);
	}
	/* Cleanup Rx queue tunnel info. */
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *q = (*priv->rxqs)[i];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(q, struct mlx5_rxq_ctrl, rxq);

		if (!q)
			continue;
		memset((void *)rxq_ctrl->tunnel_types, 0,
		       sizeof(rxq_ctrl->tunnel_types));
		q->tunnel = 0;
	}
}

/**
 * Add all flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_start(struct rte_eth_dev *dev, struct mlx5_flows *list)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow *flow;

	TAILQ_FOREACH(flow, list, next) {
		unsigned int i;

		if (flow->drop) {
			flow->frxq[HASH_RXQ_ETH].ibv_flow =
				mlx5_glue->create_flow
				(priv->flow_drop_queue->qp,
				 flow->frxq[HASH_RXQ_ETH].ibv_attr);
			if (!flow->frxq[HASH_RXQ_ETH].ibv_flow) {
				DRV_LOG(DEBUG,
					"port %u flow %p cannot be applied",
					dev->data->port_id, (void *)flow);
				rte_errno = EINVAL;
				return -rte_errno;
			}
			DRV_LOG(DEBUG, "port %u flow %p applied",
				dev->data->port_id, (void *)flow);
			/* Next flow. */
			continue;
		}
		for (i = 0; i != hash_rxq_init_n; ++i) {
			if (!flow->frxq[i].ibv_attr)
				continue;
			flow->frxq[i].hrxq =
				mlx5_hrxq_get(dev, flow->rss_conf.key,
					      flow->rss_conf.key_len,
					      flow->frxq[i].hash_fields,
					      flow->rss_conf.queue,
					      flow->rss_conf.queue_num,
					      flow->tunnel,
					      flow->rss_conf.level);
			if (flow->frxq[i].hrxq)
				goto flow_create;
			flow->frxq[i].hrxq =
				mlx5_hrxq_new(dev, flow->rss_conf.key,
					      flow->rss_conf.key_len,
					      flow->frxq[i].hash_fields,
					      flow->rss_conf.queue,
					      flow->rss_conf.queue_num,
					      flow->tunnel,
					      flow->rss_conf.level);
			if (!flow->frxq[i].hrxq) {
				DRV_LOG(DEBUG,
					"port %u flow %p cannot create hash"
					" rxq",
					dev->data->port_id, (void *)flow);
				rte_errno = EINVAL;
				return -rte_errno;
			}
flow_create:
			mlx5_flow_dump(dev, flow, i);
			flow->frxq[i].ibv_flow =
				mlx5_glue->create_flow(flow->frxq[i].hrxq->qp,
						       flow->frxq[i].ibv_attr);
			if (!flow->frxq[i].ibv_flow) {
				DRV_LOG(DEBUG,
					"port %u flow %p type %u cannot be"
					" applied",
					dev->data->port_id, (void *)flow, i);
				rte_errno = EINVAL;
				return -rte_errno;
			}
		}
		mlx5_flow_create_update_rxqs(dev, flow);
	}
	return 0;
}

/**
 * Verify the flow list is empty
 *
 * @param dev
 *  Pointer to Ethernet device.
 *
 * @return the number of flows not released.
 */
int
mlx5_flow_verify(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow *flow;
	int ret = 0;

	TAILQ_FOREACH(flow, &priv->flows, next) {
		DRV_LOG(DEBUG, "port %u flow %p still referenced",
			dev->data->port_id, (void *)flow);
		++ret;
	}
	return ret;
}

/**
 * Enable a control flow configured from the control plane.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param eth_spec
 *   An Ethernet flow spec to apply.
 * @param eth_mask
 *   An Ethernet flow mask to apply.
 * @param vlan_spec
 *   A VLAN flow spec to apply.
 * @param vlan_mask
 *   A VLAN flow mask to apply.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_ctrl_flow_vlan(struct rte_eth_dev *dev,
		    struct rte_flow_item_eth *eth_spec,
		    struct rte_flow_item_eth *eth_mask,
		    struct rte_flow_item_vlan *vlan_spec,
		    struct rte_flow_item_vlan *vlan_mask)
{
	struct priv *priv = dev->data->dev_private;
	const struct rte_flow_attr attr = {
		.ingress = 1,
		.priority = MLX5_CTRL_FLOW_PRIORITY,
	};
	struct rte_flow_item items[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = eth_spec,
			.last = NULL,
			.mask = eth_mask,
		},
		{
			.type = (vlan_spec) ? RTE_FLOW_ITEM_TYPE_VLAN :
				RTE_FLOW_ITEM_TYPE_END,
			.spec = vlan_spec,
			.last = NULL,
			.mask = vlan_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	uint16_t queue[priv->reta_idx_n];
	struct rte_flow_action_rss action_rss = {
		.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
		.level = 0,
		.types = priv->rss_conf.rss_hf,
		.key_len = priv->rss_conf.rss_key_len,
		.queue_num = priv->reta_idx_n,
		.key = priv->rss_conf.rss_key,
		.queue = queue,
	};
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_RSS,
			.conf = &action_rss,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct rte_flow *flow;
	struct rte_flow_error error;
	unsigned int i;

	if (!priv->reta_idx_n) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	for (i = 0; i != priv->reta_idx_n; ++i)
		queue[i] = (*priv->reta_idx)[i];
	flow = mlx5_flow_list_create(dev, &priv->ctrl_flows, &attr, items,
				     actions, &error);
	if (!flow)
		return -rte_errno;
	return 0;
}

/**
 * Enable a flow control configured from the control plane.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param eth_spec
 *   An Ethernet flow spec to apply.
 * @param eth_mask
 *   An Ethernet flow mask to apply.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_ctrl_flow(struct rte_eth_dev *dev,
	       struct rte_flow_item_eth *eth_spec,
	       struct rte_flow_item_eth *eth_mask)
{
	return mlx5_ctrl_flow_vlan(dev, eth_spec, eth_mask, NULL, NULL);
}

/**
 * Destroy a flow.
 *
 * @see rte_flow_destroy()
 * @see rte_flow_ops
 */
int
mlx5_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error __rte_unused)
{
	struct priv *priv = dev->data->dev_private;

	mlx5_flow_list_destroy(dev, &priv->flows, flow);
	return 0;
}

/**
 * Destroy all flows.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
int
mlx5_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error __rte_unused)
{
	struct priv *priv = dev->data->dev_private;

	mlx5_flow_list_flush(dev, &priv->flows);
	return 0;
}

#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
/**
 * Query flow counter.
 *
 * @param cs
 *   the counter set.
 * @param counter_value
 *   returned data from the counter.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_query_count(struct ibv_counter_set *cs,
		      struct mlx5_flow_counter_stats *counter_stats,
		      struct rte_flow_query_count *query_count,
		      struct rte_flow_error *error)
{
	uint64_t counters[2];
	struct ibv_query_counter_set_attr query_cs_attr = {
		.cs = cs,
		.query_flags = IBV_COUNTER_SET_FORCE_UPDATE,
	};
	struct ibv_counter_set_data query_out = {
		.out = counters,
		.outlen = 2 * sizeof(uint64_t),
	};
	int err = mlx5_glue->query_counter_set(&query_cs_attr, &query_out);

	if (err)
		return rte_flow_error_set(error, err,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot read counter");
	query_count->hits_set = 1;
	query_count->bytes_set = 1;
	query_count->hits = counters[0] - counter_stats->hits;
	query_count->bytes = counters[1] - counter_stats->bytes;
	if (query_count->reset) {
		counter_stats->hits = counters[0];
		counter_stats->bytes = counters[1];
	}
	return 0;
}

/**
 * Query a flows.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
int
mlx5_flow_query(struct rte_eth_dev *dev __rte_unused,
		struct rte_flow *flow,
		const struct rte_flow_action *action __rte_unused,
		void *data,
		struct rte_flow_error *error)
{
	if (flow->cs) {
		int ret;

		ret = mlx5_flow_query_count(flow->cs,
					    &flow->counter_stats,
					    (struct rte_flow_query_count *)data,
					    error);
		if (ret)
			return ret;
	} else {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "no counter found for flow");
	}
	return 0;
}
#endif

/**
 * Isolated mode.
 *
 * @see rte_flow_isolate()
 * @see rte_flow_ops
 */
int
mlx5_flow_isolate(struct rte_eth_dev *dev,
		  int enable,
		  struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;

	if (dev->data->dev_started) {
		rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "port must be stopped first");
		return -rte_errno;
	}
	priv->isolated = !!enable;
	if (enable)
		dev->dev_ops = &mlx5_dev_ops_isolate;
	else
		dev->dev_ops = &mlx5_dev_ops;
	return 0;
}

/**
 * Convert a flow director filter to a generic flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Flow director filter to add.
 * @param attributes
 *   Generic flow parameters structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_fdir_filter_convert(struct rte_eth_dev *dev,
			 const struct rte_eth_fdir_filter *fdir_filter,
			 struct mlx5_fdir *attributes)
{
	struct priv *priv = dev->data->dev_private;
	const struct rte_eth_fdir_input *input = &fdir_filter->input;
	const struct rte_eth_fdir_masks *mask =
		&dev->data->dev_conf.fdir_conf.mask;

	/* Validate queue number. */
	if (fdir_filter->action.rx_queue >= priv->rxqs_n) {
		DRV_LOG(ERR, "port %u invalid queue number %d",
			dev->data->port_id, fdir_filter->action.rx_queue);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	attributes->attr.ingress = 1;
	attributes->items[0] = (struct rte_flow_item) {
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &attributes->l2,
		.mask = &attributes->l2_mask,
	};
	switch (fdir_filter->action.behavior) {
	case RTE_ETH_FDIR_ACCEPT:
		attributes->actions[0] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_QUEUE,
			.conf = &attributes->queue,
		};
		break;
	case RTE_ETH_FDIR_REJECT:
		attributes->actions[0] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_DROP,
		};
		break;
	default:
		DRV_LOG(ERR, "port %u invalid behavior %d",
			dev->data->port_id,
			fdir_filter->action.behavior);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	attributes->queue.index = fdir_filter->action.rx_queue;
	/* Handle L3. */
	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		attributes->l3.ipv4.hdr = (struct ipv4_hdr){
			.src_addr = input->flow.ip4_flow.src_ip,
			.dst_addr = input->flow.ip4_flow.dst_ip,
			.time_to_live = input->flow.ip4_flow.ttl,
			.type_of_service = input->flow.ip4_flow.tos,
			.next_proto_id = input->flow.ip4_flow.proto,
		};
		attributes->l3_mask.ipv4.hdr = (struct ipv4_hdr){
			.src_addr = mask->ipv4_mask.src_ip,
			.dst_addr = mask->ipv4_mask.dst_ip,
			.time_to_live = mask->ipv4_mask.ttl,
			.type_of_service = mask->ipv4_mask.tos,
			.next_proto_id = mask->ipv4_mask.proto,
		};
		attributes->items[1] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.spec = &attributes->l3,
			.mask = &attributes->l3_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		attributes->l3.ipv6.hdr = (struct ipv6_hdr){
			.hop_limits = input->flow.ipv6_flow.hop_limits,
			.proto = input->flow.ipv6_flow.proto,
		};

		memcpy(attributes->l3.ipv6.hdr.src_addr,
		       input->flow.ipv6_flow.src_ip,
		       RTE_DIM(attributes->l3.ipv6.hdr.src_addr));
		memcpy(attributes->l3.ipv6.hdr.dst_addr,
		       input->flow.ipv6_flow.dst_ip,
		       RTE_DIM(attributes->l3.ipv6.hdr.src_addr));
		memcpy(attributes->l3_mask.ipv6.hdr.src_addr,
		       mask->ipv6_mask.src_ip,
		       RTE_DIM(attributes->l3_mask.ipv6.hdr.src_addr));
		memcpy(attributes->l3_mask.ipv6.hdr.dst_addr,
		       mask->ipv6_mask.dst_ip,
		       RTE_DIM(attributes->l3_mask.ipv6.hdr.src_addr));
		attributes->items[1] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &attributes->l3,
			.mask = &attributes->l3_mask,
		};
		break;
	default:
		DRV_LOG(ERR, "port %u invalid flow type%d",
			dev->data->port_id, fdir_filter->input.flow_type);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	/* Handle L4. */
	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
		attributes->l4.udp.hdr = (struct udp_hdr){
			.src_port = input->flow.udp4_flow.src_port,
			.dst_port = input->flow.udp4_flow.dst_port,
		};
		attributes->l4_mask.udp.hdr = (struct udp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		attributes->l4.tcp.hdr = (struct tcp_hdr){
			.src_port = input->flow.tcp4_flow.src_port,
			.dst_port = input->flow.tcp4_flow.dst_port,
		};
		attributes->l4_mask.tcp.hdr = (struct tcp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
		attributes->l4.udp.hdr = (struct udp_hdr){
			.src_port = input->flow.udp6_flow.src_port,
			.dst_port = input->flow.udp6_flow.dst_port,
		};
		attributes->l4_mask.udp.hdr = (struct udp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		attributes->l4.tcp.hdr = (struct tcp_hdr){
			.src_port = input->flow.tcp6_flow.src_port,
			.dst_port = input->flow.tcp6_flow.dst_port,
		};
		attributes->l4_mask.tcp.hdr = (struct tcp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		break;
	default:
		DRV_LOG(ERR, "port %u invalid flow type%d",
			dev->data->port_id, fdir_filter->input.flow_type);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return 0;
}

/**
 * Add new flow director filter and store it in list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Flow director filter to add.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_fdir_filter_add(struct rte_eth_dev *dev,
		     const struct rte_eth_fdir_filter *fdir_filter)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_fdir attributes = {
		.attr.group = 0,
		.l2_mask = {
			.dst.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		},
	};
	struct mlx5_flow_parse parser = {
		.layer = HASH_RXQ_ETH,
	};
	struct rte_flow_error error;
	struct rte_flow *flow;
	int ret;

	ret = mlx5_fdir_filter_convert(dev, fdir_filter, &attributes);
	if (ret)
		return ret;
	ret = mlx5_flow_convert(dev, &attributes.attr, attributes.items,
				attributes.actions, &error, &parser);
	if (ret)
		return ret;
	flow = mlx5_flow_list_create(dev, &priv->flows, &attributes.attr,
				     attributes.items, attributes.actions,
				     &error);
	if (flow) {
		DRV_LOG(DEBUG, "port %u FDIR created %p", dev->data->port_id,
			(void *)flow);
		return 0;
	}
	return -rte_errno;
}

/**
 * Delete specific filter.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Filter to be deleted.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_fdir_filter_delete(struct rte_eth_dev *dev,
			const struct rte_eth_fdir_filter *fdir_filter)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_fdir attributes = {
		.attr.group = 0,
	};
	struct mlx5_flow_parse parser = {
		.create = 1,
		.layer = HASH_RXQ_ETH,
	};
	struct rte_flow_error error;
	struct rte_flow *flow;
	unsigned int i;
	int ret;

	ret = mlx5_fdir_filter_convert(dev, fdir_filter, &attributes);
	if (ret)
		return ret;
	ret = mlx5_flow_convert(dev, &attributes.attr, attributes.items,
				attributes.actions, &error, &parser);
	if (ret)
		goto exit;
	/*
	 * Special case for drop action which is only set in the
	 * specifications when the flow is created.  In this situation the
	 * drop specification is missing.
	 */
	if (parser.drop) {
		struct ibv_flow_spec_action_drop *drop;

		drop = (void *)((uintptr_t)parser.queue[HASH_RXQ_ETH].ibv_attr +
				parser.queue[HASH_RXQ_ETH].offset);
		*drop = (struct ibv_flow_spec_action_drop){
			.type = IBV_FLOW_SPEC_ACTION_DROP,
			.size = sizeof(struct ibv_flow_spec_action_drop),
		};
		parser.queue[HASH_RXQ_ETH].ibv_attr->num_of_specs++;
	}
	TAILQ_FOREACH(flow, &priv->flows, next) {
		struct ibv_flow_attr *attr;
		struct ibv_spec_header *attr_h;
		void *spec;
		struct ibv_flow_attr *flow_attr;
		struct ibv_spec_header *flow_h;
		void *flow_spec;
		unsigned int specs_n;
		unsigned int queue_id = parser.drop ? HASH_RXQ_ETH :
						      parser.layer;

		attr = parser.queue[queue_id].ibv_attr;
		flow_attr = flow->frxq[queue_id].ibv_attr;
		/* Compare first the attributes. */
		if (!flow_attr ||
		    memcmp(attr, flow_attr, sizeof(struct ibv_flow_attr)))
			continue;
		if (attr->num_of_specs == 0)
			continue;
		spec = (void *)((uintptr_t)attr +
				sizeof(struct ibv_flow_attr));
		flow_spec = (void *)((uintptr_t)flow_attr +
				     sizeof(struct ibv_flow_attr));
		specs_n = RTE_MIN(attr->num_of_specs, flow_attr->num_of_specs);
		for (i = 0; i != specs_n; ++i) {
			attr_h = spec;
			flow_h = flow_spec;
			if (memcmp(spec, flow_spec,
				   RTE_MIN(attr_h->size, flow_h->size)))
				goto wrong_flow;
			spec = (void *)((uintptr_t)spec + attr_h->size);
			flow_spec = (void *)((uintptr_t)flow_spec +
					     flow_h->size);
		}
		/* At this point, the flow match. */
		break;
wrong_flow:
		/* The flow does not match. */
		continue;
	}
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (flow)
		mlx5_flow_list_destroy(dev, &priv->flows, flow);
exit:
	for (i = 0; i != hash_rxq_init_n; ++i) {
		if (parser.queue[i].ibv_attr)
			rte_free(parser.queue[i].ibv_attr);
	}
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Update queue for specific filter.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Filter to be updated.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_fdir_filter_update(struct rte_eth_dev *dev,
			const struct rte_eth_fdir_filter *fdir_filter)
{
	int ret;

	ret = mlx5_fdir_filter_delete(dev, fdir_filter);
	if (ret)
		return ret;
	return mlx5_fdir_filter_add(dev, fdir_filter);
}

/**
 * Flush all filters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_fdir_filter_flush(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;

	mlx5_flow_list_flush(dev, &priv->flows);
}

/**
 * Get flow director information.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] fdir_info
 *   Resulting flow director information.
 */
static void
mlx5_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir_info *fdir_info)
{
	struct rte_eth_fdir_masks *mask =
		&dev->data->dev_conf.fdir_conf.mask;

	fdir_info->mode = dev->data->dev_conf.fdir_conf.mode;
	fdir_info->guarant_spc = 0;
	rte_memcpy(&fdir_info->mask, mask, sizeof(fdir_info->mask));
	fdir_info->max_flexpayload = 0;
	fdir_info->flow_types_mask[0] = 0;
	fdir_info->flex_payload_unit = 0;
	fdir_info->max_flex_payload_segment_num = 0;
	fdir_info->flex_payload_limit = 0;
	memset(&fdir_info->flex_conf, 0, sizeof(fdir_info->flex_conf));
}

/**
 * Deal with flow director operations.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_fdir_ctrl_func(struct rte_eth_dev *dev, enum rte_filter_op filter_op,
		    void *arg)
{
	enum rte_fdir_mode fdir_mode =
		dev->data->dev_conf.fdir_conf.mode;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;
	if (fdir_mode != RTE_FDIR_MODE_PERFECT &&
	    fdir_mode != RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		DRV_LOG(ERR, "port %u flow director mode %d not supported",
			dev->data->port_id, fdir_mode);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		return mlx5_fdir_filter_add(dev, arg);
	case RTE_ETH_FILTER_UPDATE:
		return mlx5_fdir_filter_update(dev, arg);
	case RTE_ETH_FILTER_DELETE:
		return mlx5_fdir_filter_delete(dev, arg);
	case RTE_ETH_FILTER_FLUSH:
		mlx5_fdir_filter_flush(dev);
		break;
	case RTE_ETH_FILTER_INFO:
		mlx5_fdir_info_get(dev, arg);
		break;
	default:
		DRV_LOG(DEBUG, "port %u unknown operation %u",
			dev->data->port_id, filter_op);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

/**
 * Manage filter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param filter_type
 *   Filter type.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET) {
			rte_errno = EINVAL;
			return -rte_errno;
		}
		*(const void **)arg = &mlx5_flow_ops;
		return 0;
	case RTE_ETH_FILTER_FDIR:
		return mlx5_fdir_ctrl_func(dev, filter_op, arg);
	default:
		DRV_LOG(ERR, "port %u filter type (%d) not supported",
			dev->data->port_id, filter_type);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return 0;
}

/**
 * Detect number of Verbs flow priorities supported.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   number of supported Verbs flow priority.
 */
unsigned int
mlx5_get_max_verbs_prio(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int verb_priorities = MLX5_VERBS_FLOW_PRIO_8;
	struct {
		struct ibv_flow_attr attr;
		struct ibv_flow_spec_eth eth;
		struct ibv_flow_spec_action_drop drop;
	} flow_attr = {
		.attr = {
			.num_of_specs = 2,
		},
		.eth = {
			.type = IBV_FLOW_SPEC_ETH,
			.size = sizeof(struct ibv_flow_spec_eth),
		},
		.drop = {
			.size = sizeof(struct ibv_flow_spec_action_drop),
			.type = IBV_FLOW_SPEC_ACTION_DROP,
		},
	};
	struct ibv_flow *flow;

	do {
		flow_attr.attr.priority = verb_priorities - 1;
		flow = mlx5_glue->create_flow(priv->flow_drop_queue->qp,
					      &flow_attr.attr);
		if (flow) {
			claim_zero(mlx5_glue->destroy_flow(flow));
			/* Try more priorities. */
			verb_priorities *= 2;
		} else {
			/* Failed, restore last right number. */
			verb_priorities /= 2;
			break;
		}
	} while (1);
	DRV_LOG(DEBUG, "port %u Verbs flow priorities: %d,"
		" user flow priorities: %d",
		dev->data->port_id, verb_priorities, MLX5_CTRL_FLOW_PRIORITY);
	return verb_priorities;
}
