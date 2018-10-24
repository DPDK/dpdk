/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <netinet/in.h>
#include <sys/queue.h>
#include <stdalign.h>
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
#include "mlx5_flow.h"

/**
 * Create Verbs flow counter with Verbs library.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] counter
 *   mlx5 flow counter object, contains the counter id,
 *   handle of created Verbs flow counter is returned
 *   in cs field (if counters are supported).
 *
 * @return
 *   0 On success else a negative errno value is returned
 *   and rte_errno is set.
 */
static int
flow_verbs_counter_create(struct rte_eth_dev *dev,
			  struct mlx5_flow_counter *counter)
{
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
	struct priv *priv = dev->data->dev_private;
	struct ibv_counter_set_init_attr init = {
			 .counter_set_id = counter->id};

	counter->cs = mlx5_glue->create_counter_set(priv->ctx, &init);
	if (!counter->cs) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	return 0;
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	struct priv *priv = dev->data->dev_private;
	struct ibv_counters_init_attr init = {0};
	struct ibv_counter_attach_attr attach = {0};
	int ret;

	counter->cs = mlx5_glue->create_counters(priv->ctx, &init);
	if (!counter->cs) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	attach.counter_desc = IBV_COUNTER_PACKETS;
	attach.index = 0;
	ret = mlx5_glue->attach_counters(counter->cs, &attach, NULL);
	if (!ret) {
		attach.counter_desc = IBV_COUNTER_BYTES;
		attach.index = 1;
		ret = mlx5_glue->attach_counters
					(counter->cs, &attach, NULL);
	}
	if (ret) {
		claim_zero(mlx5_glue->destroy_counters(counter->cs));
		counter->cs = NULL;
		rte_errno = ret;
		return -ret;
	}
	return 0;
#else
	(void)dev;
	(void)counter;
	rte_errno = ENOTSUP;
	return -ENOTSUP;
#endif
}

/**
 * Get a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] shared
 *   Indicate if this counter is shared with other flows.
 * @param[in] id
 *   Counter identifier.
 *
 * @return
 *   A pointer to the counter, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter *
flow_verbs_counter_new(struct rte_eth_dev *dev, uint32_t shared, uint32_t id)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter *cnt;
	int ret;

	LIST_FOREACH(cnt, &priv->flow_counters, next) {
		if (!cnt->shared || cnt->shared != shared)
			continue;
		if (cnt->id != id)
			continue;
		cnt->ref_cnt++;
		return cnt;
	}
	cnt = rte_calloc(__func__, 1, sizeof(*cnt), 0);
	if (!cnt) {
		rte_errno = ENOMEM;
		return NULL;
	}
	cnt->id = id;
	cnt->shared = shared;
	cnt->ref_cnt = 1;
	cnt->hits = 0;
	cnt->bytes = 0;
	/* Create counter with Verbs. */
	ret = flow_verbs_counter_create(dev, cnt);
	if (!ret) {
		LIST_INSERT_HEAD(&priv->flow_counters, cnt, next);
		return cnt;
	}
	/* Some error occurred in Verbs library. */
	rte_free(cnt);
	rte_errno = -ret;
	return NULL;
}

/**
 * Release a flow counter.
 *
 * @param[in] counter
 *   Pointer to the counter handler.
 */
static void
flow_verbs_counter_release(struct mlx5_flow_counter *counter)
{
	if (--counter->ref_cnt == 0) {
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
		claim_zero(mlx5_glue->destroy_counter_set(counter->cs));
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
		claim_zero(mlx5_glue->destroy_counters(counter->cs));
#endif
		LIST_REMOVE(counter, next);
		rte_free(counter);
	}
}

/**
 * Query a flow counter via Verbs library call.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
static int
flow_verbs_counter_query(struct rte_eth_dev *dev __rte_unused,
			 struct rte_flow *flow, void *data,
			 struct rte_flow_error *error)
{
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) || \
	defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	if (flow->actions & MLX5_FLOW_ACTION_COUNT) {
		struct rte_flow_query_count *qc = data;
		uint64_t counters[2] = {0, 0};
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
		struct ibv_query_counter_set_attr query_cs_attr = {
			.cs = flow->counter->cs,
			.query_flags = IBV_COUNTER_SET_FORCE_UPDATE,
		};
		struct ibv_counter_set_data query_out = {
			.out = counters,
			.outlen = 2 * sizeof(uint64_t),
		};
		int err = mlx5_glue->query_counter_set(&query_cs_attr,
						       &query_out);
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
		int err = mlx5_glue->query_counters
			       (flow->counter->cs, counters,
				RTE_DIM(counters),
				IBV_READ_COUNTERS_ATTR_PREFER_CACHED);
#endif
		if (err)
			return rte_flow_error_set
				(error, err,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL,
				 "cannot read counter");
		qc->hits_set = 1;
		qc->bytes_set = 1;
		qc->hits = counters[0] - flow->counter->hits;
		qc->bytes = counters[1] - flow->counter->bytes;
		if (qc->reset) {
			flow->counter->hits = counters[0];
			flow->counter->bytes = counters[1];
		}
		return 0;
	}
	return rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "flow does not have counter");
#else
	(void)flow;
	(void)data;
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "counters are not available");
#endif
}

/**
 * Add a verbs item specification into @p flow.
 *
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] src
 *   Create specification.
 * @param[in] size
 *   Size in bytes of the specification to copy.
 */
static void
flow_verbs_spec_add(struct mlx5_flow *flow, void *src, unsigned int size)
{
	struct mlx5_flow_verbs *verbs = &flow->verbs;

	if (verbs->specs) {
		void *dst;

		dst = (void *)(verbs->specs + verbs->size);
		memcpy(dst, src, size);
		++verbs->attr->num_of_specs;
	}
	verbs->size += size;
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit field with all detected items.
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 */
static void
flow_verbs_translate_item_eth(const struct rte_flow_item *item,
			      uint64_t *item_flags,
			      struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	const int tunnel = !!(*item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const unsigned int size = sizeof(struct ibv_flow_spec_eth);
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_eth_mask;
	if (spec) {
		unsigned int i;

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
		dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L2;
	}
	flow_verbs_spec_add(dev_flow, &eth, size);
	*item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
				MLX5_FLOW_LAYER_OUTER_L2;
}

/**
 * Update the VLAN tag in the Verbs Ethernet specification.
 * This function assumes that the input is valid and there is space to add
 * the requested item.
 *
 * @param[in, out] attr
 *   Pointer to Verbs attributes structure.
 * @param[in] eth
 *   Verbs structure containing the VLAN information to copy.
 */
static void
flow_verbs_item_vlan_update(struct ibv_flow_attr *attr,
			    struct ibv_flow_spec_eth *eth)
{
	unsigned int i;
	const enum ibv_flow_spec_type search = eth->type;
	struct ibv_spec_header *hdr = (struct ibv_spec_header *)
		((uint8_t *)attr + sizeof(struct ibv_flow_attr));

	for (i = 0; i != attr->num_of_specs; ++i) {
		if (hdr->type == search) {
			struct ibv_flow_spec_eth *e =
				(struct ibv_flow_spec_eth *)hdr;

			e->val.vlan_tag = eth->val.vlan_tag;
			e->mask.vlan_tag = eth->mask.vlan_tag;
			e->val.ether_type = eth->val.ether_type;
			e->mask.ether_type = eth->mask.ether_type;
			break;
		}
		hdr = (struct ibv_spec_header *)((uint8_t *)hdr + hdr->size);
	}
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that holds all detected items.
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 */
static void
flow_verbs_translate_item_vlan(const struct rte_flow_item *item,
			       uint64_t *item_flags,
			       struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_eth);
	const int tunnel = !!(*item_flags & MLX5_FLOW_LAYER_TUNNEL);
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};
	const uint32_t l2m = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
				      MLX5_FLOW_LAYER_OUTER_L2;

	if (!mask)
		mask = &rte_flow_item_vlan_mask;
	if (spec) {
		eth.val.vlan_tag = spec->tci;
		eth.mask.vlan_tag = mask->tci;
		eth.val.vlan_tag &= eth.mask.vlan_tag;
		eth.val.ether_type = spec->inner_type;
		eth.mask.ether_type = mask->inner_type;
		eth.val.ether_type &= eth.mask.ether_type;
	}
	if (!(*item_flags & l2m)) {
		dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L2;
		flow_verbs_spec_add(dev_flow, &eth, size);
	} else {
		flow_verbs_item_vlan_update(dev_flow->verbs.attr, &eth);
		size = 0; /* Only an update is done in eth specification. */
	}
	*item_flags |= tunnel ?
		       (MLX5_FLOW_LAYER_INNER_L2 | MLX5_FLOW_LAYER_INNER_VLAN) :
		       (MLX5_FLOW_LAYER_OUTER_L2 | MLX5_FLOW_LAYER_OUTER_VLAN);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_ipv4(const struct rte_flow_item *item,
			       uint64_t *item_flags,
			       struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	const int tunnel = !!(*item_flags & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_ipv4_ext);
	struct ibv_flow_spec_ipv4_ext ipv4 = {
		.type = IBV_FLOW_SPEC_IPV4_EXT |
			(tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_ipv4_mask;
	*item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
				MLX5_FLOW_LAYER_OUTER_L3_IPV4;
	if (spec) {
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
	dev_flow->verbs.hash_fields |=
		mlx5_flow_hashfields_adjust(dev_flow, tunnel,
					    MLX5_IPV4_LAYER_TYPES,
					    MLX5_IPV4_IBV_RX_HASH);
	dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L3;
	flow_verbs_spec_add(dev_flow, &ipv4, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_ipv6(const struct rte_flow_item *item,
			       uint64_t *item_flags,
			       struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	const int tunnel = !!(dev_flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_ipv6);
	struct ibv_flow_spec_ipv6 ipv6 = {
		.type = IBV_FLOW_SPEC_IPV6 | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_ipv6_mask;
	 *item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
				 MLX5_FLOW_LAYER_OUTER_L3_IPV6;
	if (spec) {
		unsigned int i;
		uint32_t vtc_flow_val;
		uint32_t vtc_flow_mask;

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
	dev_flow->verbs.hash_fields |=
		mlx5_flow_hashfields_adjust(dev_flow, tunnel,
					    MLX5_IPV6_LAYER_TYPES,
					    MLX5_IPV6_IBV_RX_HASH);
	dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L3;
	flow_verbs_spec_add(dev_flow, &ipv6, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_udp(const struct rte_flow_item *item,
			      uint64_t *item_flags,
			      struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	const int tunnel = !!(*item_flags & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp udp = {
		.type = IBV_FLOW_SPEC_UDP | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_udp_mask;
	*item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
				MLX5_FLOW_LAYER_OUTER_L4_UDP;
	if (spec) {
		udp.val.dst_port = spec->hdr.dst_port;
		udp.val.src_port = spec->hdr.src_port;
		udp.mask.dst_port = mask->hdr.dst_port;
		udp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		udp.val.src_port &= udp.mask.src_port;
		udp.val.dst_port &= udp.mask.dst_port;
	}
	dev_flow->verbs.hash_fields |=
		mlx5_flow_hashfields_adjust(dev_flow, tunnel, ETH_RSS_UDP,
					    (IBV_RX_HASH_SRC_PORT_UDP |
					     IBV_RX_HASH_DST_PORT_UDP));
	dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L4;
	flow_verbs_spec_add(dev_flow, &udp, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_tcp(const struct rte_flow_item *item,
			      uint64_t *item_flags,
			      struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	const int tunnel = !!(dev_flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp tcp = {
		.type = IBV_FLOW_SPEC_TCP | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_tcp_mask;
	*item_flags |=  tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
				 MLX5_FLOW_LAYER_OUTER_L4_TCP;
	if (spec) {
		tcp.val.dst_port = spec->hdr.dst_port;
		tcp.val.src_port = spec->hdr.src_port;
		tcp.mask.dst_port = mask->hdr.dst_port;
		tcp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		tcp.val.src_port &= tcp.mask.src_port;
		tcp.val.dst_port &= tcp.mask.dst_port;
	}
	dev_flow->verbs.hash_fields |=
		mlx5_flow_hashfields_adjust(dev_flow, tunnel, ETH_RSS_TCP,
					    (IBV_RX_HASH_SRC_PORT_TCP |
					     IBV_RX_HASH_DST_PORT_TCP));
	dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L4;
	flow_verbs_spec_add(dev_flow, &tcp, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_vxlan(const struct rte_flow_item *item,
				uint64_t *item_flags,
				struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel vxlan = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id = { .vlan_id = 0, };

	if (!mask)
		mask = &rte_flow_item_vxlan_mask;
	if (spec) {
		memcpy(&id.vni[1], spec->vni, 3);
		vxlan.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vxlan.mask.tunnel_id = id.vlan_id;
		/* Remove unwanted bits from values. */
		vxlan.val.tunnel_id &= vxlan.mask.tunnel_id;
	}
	flow_verbs_spec_add(dev_flow, &vxlan, size);
	dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L2;
	*item_flags |= MLX5_FLOW_LAYER_VXLAN;
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_vxlan_gpe(const struct rte_flow_item *item,
				    uint64_t *item_flags,
				    struct mlx5_flow *dev_flow)
{
	const struct rte_flow_item_vxlan_gpe *spec = item->spec;
	const struct rte_flow_item_vxlan_gpe *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel vxlan_gpe = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id = { .vlan_id = 0, };

	if (!mask)
		mask = &rte_flow_item_vxlan_gpe_mask;
	if (spec) {
		memcpy(&id.vni[1], spec->vni, 3);
		vxlan_gpe.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vxlan_gpe.mask.tunnel_id = id.vlan_id;
		/* Remove unwanted bits from values. */
		vxlan_gpe.val.tunnel_id &= vxlan_gpe.mask.tunnel_id;
	}
	flow_verbs_spec_add(dev_flow, &vxlan_gpe, size);
	dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L2;
	*item_flags |= MLX5_FLOW_LAYER_VXLAN_GPE;
}

/**
 * Update the protocol in Verbs IPv4/IPv6 spec.
 *
 * @param[in, out] attr
 *   Pointer to Verbs attributes structure.
 * @param[in] search
 *   Specification type to search in order to update the IP protocol.
 * @param[in] protocol
 *   Protocol value to set if none is present in the specification.
 */
static void
flow_verbs_item_gre_ip_protocol_update(struct ibv_flow_attr *attr,
				       enum ibv_flow_spec_type search,
				       uint8_t protocol)
{
	unsigned int i;
	struct ibv_spec_header *hdr = (struct ibv_spec_header *)
		((uint8_t *)attr + sizeof(struct ibv_flow_attr));

	if (!attr)
		return;
	for (i = 0; i != attr->num_of_specs; ++i) {
		if (hdr->type == search) {
			union {
				struct ibv_flow_spec_ipv4_ext *ipv4;
				struct ibv_flow_spec_ipv6 *ipv6;
			} ip;

			switch (search) {
			case IBV_FLOW_SPEC_IPV4_EXT:
				ip.ipv4 = (struct ibv_flow_spec_ipv4_ext *)hdr;
				if (!ip.ipv4->val.proto) {
					ip.ipv4->val.proto = protocol;
					ip.ipv4->mask.proto = 0xff;
				}
				break;
			case IBV_FLOW_SPEC_IPV6:
				ip.ipv6 = (struct ibv_flow_spec_ipv6 *)hdr;
				if (!ip.ipv6->val.next_hdr) {
					ip.ipv6->val.next_hdr = protocol;
					ip.ipv6->mask.next_hdr = 0xff;
				}
				break;
			default:
				break;
			}
			break;
		}
		hdr = (struct ibv_spec_header *)((uint8_t *)hdr + hdr->size);
	}
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_gre(const struct rte_flow_item *item __rte_unused,
			      uint64_t *item_flags,
			      struct mlx5_flow *dev_flow)
{
	struct mlx5_flow_verbs *verbs = &dev_flow->verbs;
#ifndef HAVE_IBV_DEVICE_MPLS_SUPPORT
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel tunnel = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
#else
	const struct rte_flow_item_gre *spec = item->spec;
	const struct rte_flow_item_gre *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_gre);
	struct ibv_flow_spec_gre tunnel = {
		.type = IBV_FLOW_SPEC_GRE,
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_gre_mask;
	if (spec) {
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
	if (*item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV4)
		flow_verbs_item_gre_ip_protocol_update(verbs->attr,
						       IBV_FLOW_SPEC_IPV4_EXT,
						       IPPROTO_GRE);
	else
		flow_verbs_item_gre_ip_protocol_update(verbs->attr,
						       IBV_FLOW_SPEC_IPV6,
						       IPPROTO_GRE);
	flow_verbs_spec_add(dev_flow, &tunnel, size);
	verbs->attr->priority = MLX5_PRIORITY_MAP_L2;
	*item_flags |= MLX5_FLOW_LAYER_GRE;
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] item_flags
 *   Bit mask that marks all detected items.
 * @param[in, out] dev_flow
 *   Pointer to sepacific flow structure.
 */
static void
flow_verbs_translate_item_mpls(const struct rte_flow_item *item __rte_unused,
			       uint64_t *action_flags __rte_unused,
			       struct mlx5_flow *dev_flow __rte_unused)
{
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	const struct rte_flow_item_mpls *spec = item->spec;
	const struct rte_flow_item_mpls *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_mpls);
	struct ibv_flow_spec_mpls mpls = {
		.type = IBV_FLOW_SPEC_MPLS,
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_mpls_mask;
	if (spec) {
		memcpy(&mpls.val.label, spec, sizeof(mpls.val.label));
		memcpy(&mpls.mask.label, mask, sizeof(mpls.mask.label));
		/* Remove unwanted bits from values.  */
		mpls.val.label &= mpls.mask.label;
	}
	flow_verbs_spec_add(dev_flow, &mpls, size);
	dev_flow->verbs.attr->priority = MLX5_PRIORITY_MAP_L2;
	*action_flags |= MLX5_FLOW_LAYER_MPLS;
#endif
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in, out] action_flags
 *   Pointer to the detected actions.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 */
static void
flow_verbs_translate_action_drop(uint64_t *action_flags,
				 struct mlx5_flow *dev_flow)
{
	unsigned int size = sizeof(struct ibv_flow_spec_action_drop);
	struct ibv_flow_spec_action_drop drop = {
			.type = IBV_FLOW_SPEC_ACTION_DROP,
			.size = size,
	};

	flow_verbs_spec_add(dev_flow, &drop, size);
	*action_flags |= MLX5_FLOW_ACTION_DROP;
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in] action
 *   Action configuration.
 * @param[in, out] action_flags
 *   Pointer to the detected actions.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 */
static void
flow_verbs_translate_action_queue(const struct rte_flow_action *action,
				  uint64_t *action_flags,
				  struct mlx5_flow *dev_flow)
{
	const struct rte_flow_action_queue *queue = action->conf;
	struct rte_flow *flow = dev_flow->flow;

	if (flow->queue)
		(*flow->queue)[0] = queue->index;
	flow->rss.queue_num = 1;
	*action_flags |= MLX5_FLOW_ACTION_QUEUE;
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in] action
 *   Action configuration.
 * @param[in, out] action_flags
 *   Pointer to the detected actions.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 */
static void
flow_verbs_translate_action_rss(const struct rte_flow_action *action,
				uint64_t *action_flags,
				struct mlx5_flow *dev_flow)
{
	const struct rte_flow_action_rss *rss = action->conf;
	struct rte_flow *flow = dev_flow->flow;

	if (flow->queue)
		memcpy((*flow->queue), rss->queue,
		       rss->queue_num * sizeof(uint16_t));
	flow->rss.queue_num = rss->queue_num;
	memcpy(flow->key, rss->key, MLX5_RSS_HASH_KEY_LEN);
	flow->rss.types = rss->types;
	flow->rss.level = rss->level;
	*action_flags |= MLX5_FLOW_ACTION_RSS;
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in] action
 *   Action configuration.
 * @param[in, out] action_flags
 *   Pointer to the detected actions.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 */
static void
flow_verbs_translate_action_flag
			(const struct rte_flow_action *action __rte_unused,
			 uint64_t *action_flags,
			 struct mlx5_flow *dev_flow)
{
	unsigned int size = sizeof(struct ibv_flow_spec_action_tag);
	struct ibv_flow_spec_action_tag tag = {
		.type = IBV_FLOW_SPEC_ACTION_TAG,
		.size = size,
		.tag_id = mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT),
	};
	*action_flags |= MLX5_FLOW_ACTION_MARK;
	flow_verbs_spec_add(dev_flow, &tag, size);
}

/**
 * Update verbs specification to modify the flag to mark.
 *
 * @param[in, out] verbs
 *   Pointer to the mlx5_flow_verbs structure.
 * @param[in] mark_id
 *   Mark identifier to replace the flag.
 */
static void
flow_verbs_mark_update(struct mlx5_flow_verbs *verbs, uint32_t mark_id)
{
	struct ibv_spec_header *hdr;
	int i;

	if (!verbs)
		return;
	/* Update Verbs specification. */
	hdr = (struct ibv_spec_header *)verbs->specs;
	if (!hdr)
		return;
	for (i = 0; i != verbs->attr->num_of_specs; ++i) {
		if (hdr->type == IBV_FLOW_SPEC_ACTION_TAG) {
			struct ibv_flow_spec_action_tag *t =
				(struct ibv_flow_spec_action_tag *)hdr;

			t->tag_id = mlx5_flow_mark_set(mark_id);
		}
		hdr = (struct ibv_spec_header *)((uintptr_t)hdr + hdr->size);
	}
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in] action
 *   Action configuration.
 * @param[in, out] action_flags
 *   Pointer to the detected actions.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 */
static void
flow_verbs_translate_action_mark(const struct rte_flow_action *action,
				 uint64_t *action_flags,
				 struct mlx5_flow *dev_flow)
{
	const struct rte_flow_action_mark *mark = action->conf;
	unsigned int size = sizeof(struct ibv_flow_spec_action_tag);
	struct ibv_flow_spec_action_tag tag = {
		.type = IBV_FLOW_SPEC_ACTION_TAG,
		.size = size,
	};
	struct mlx5_flow_verbs *verbs = &dev_flow->verbs;

	if (*action_flags & MLX5_FLOW_ACTION_FLAG) {
		flow_verbs_mark_update(verbs, mark->id);
		size = 0;
	} else {
		tag.tag_id = mlx5_flow_mark_set(mark->id);
		flow_verbs_spec_add(dev_flow, &tag, size);
	}
	*action_flags |= MLX5_FLOW_ACTION_MARK;
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] action
 *   Action configuration.
 * @param[in, out] action_flags
 *   Pointer to the detected actions.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 On success else a negative errno value is returned and rte_errno is set.
 */
static int
flow_verbs_translate_action_count(struct rte_eth_dev *dev,
				  const struct rte_flow_action *action,
				  uint64_t *action_flags,
				  struct mlx5_flow *dev_flow,
				  struct rte_flow_error *error)
{
	const struct rte_flow_action_count *count = action->conf;
	struct rte_flow *flow = dev_flow->flow;
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) || \
	defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	unsigned int size = sizeof(struct ibv_flow_spec_counter_action);
	struct ibv_flow_spec_counter_action counter = {
		.type = IBV_FLOW_SPEC_ACTION_COUNT,
		.size = size,
	};
#endif

	if (!flow->counter) {
		flow->counter = flow_verbs_counter_new(dev, count->shared,
						       count->id);
		if (!flow->counter)
			return rte_flow_error_set(error, rte_errno,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "cannot get counter"
						  " context.");
	}
	*action_flags |= MLX5_FLOW_ACTION_COUNT;
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
	counter.counter_set_handle = flow->counter->cs->handle;
	flow_verbs_spec_add(dev_flow, &counter, size);
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	counter.counters = flow->counter->cs;
	flow_verbs_spec_add(dev_flow, &counter, size);
#endif
	return 0;
}

/**
 * Internal validation function. For validating both actions and items.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_verbs_validate(struct rte_eth_dev *dev,
		    const struct rte_flow_attr *attr,
		    const struct rte_flow_item items[],
		    const struct rte_flow_action actions[],
		    struct rte_flow_error *error)
{
	int ret;
	uint64_t action_flags = 0;
	uint64_t item_flags = 0;
	int tunnel = 0;
	uint8_t next_protocol = 0xff;

	if (items == NULL)
		return -1;
	ret = mlx5_flow_validate_attributes(dev, attr, error);
	if (ret < 0)
		return ret;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int ret = 0;

		tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(items, item_flags,
							  error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					       MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5_flow_validate_item_vlan(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
					       MLX5_FLOW_LAYER_OUTER_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_validate_item_ipv4(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					       MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv4 *)
			     items->mask)->hdr.next_proto_id)
				next_protocol =
					((const struct rte_flow_item_ipv4 *)
					 (items->spec))->hdr.next_proto_id;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_validate_item_ipv6(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					       MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv6 *)
			     items->mask)->hdr.proto)
				next_protocol =
					((const struct rte_flow_item_ipv6 *)
					 items->spec)->hdr.proto;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(items, item_flags,
							  next_protocol,
							  error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					       MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_validate_item_tcp
						(items, item_flags,
						 next_protocol,
						 &rte_flow_item_tcp_mask,
						 error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					       MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5_flow_validate_item_vxlan(items, item_flags,
							    error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			ret = mlx5_flow_validate_item_vxlan_gpe(items,
								item_flags,
								dev, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			ret = mlx5_flow_validate_item_gre(items, item_flags,
							  next_protocol, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			ret = mlx5_flow_validate_item_mpls(items, item_flags,
							   next_protocol,
							   error);
			if (ret < 0)
				return ret;
			if (next_protocol != 0xff &&
			    next_protocol != IPPROTO_MPLS)
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, items,
					 "protocol filtering not compatible"
					 " with MPLS layer");
			item_flags |= MLX5_FLOW_LAYER_MPLS;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
	}
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			ret = mlx5_flow_validate_action_flag(action_flags,
							     attr,
							     error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			ret = mlx5_flow_validate_action_mark(actions,
							     action_flags,
							     attr,
							     error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			ret = mlx5_flow_validate_action_drop(action_flags,
							     attr,
							     error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			ret = mlx5_flow_validate_action_queue(actions,
							      action_flags, dev,
							      attr,
							      error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = mlx5_flow_validate_action_rss(actions,
							    action_flags, dev,
							    attr,
							    error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_RSS;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = mlx5_flow_validate_action_count(dev, attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_COUNT;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	if (!(action_flags & MLX5_FLOW_FATE_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "no fate action is found");
	return 0;
}

/**
 * Calculate the required bytes that are needed for the action part of the verbs
 * flow, in addtion returns bit-fields with all the detected action, in order to
 * avoid another interation over the actions.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] action_flags
 *   Pointer to the detected actions.
 *
 * @return
 *   The size of the memory needed for all actions.
 */
static int
flow_verbs_get_actions_and_size(const struct rte_flow_action actions[],
				uint64_t *action_flags)
{
	int size = 0;
	uint64_t detected_actions = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			size += sizeof(struct ibv_flow_spec_action_tag);
			detected_actions |= MLX5_FLOW_ACTION_FLAG;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			size += sizeof(struct ibv_flow_spec_action_tag);
			detected_actions |= MLX5_FLOW_ACTION_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			size += sizeof(struct ibv_flow_spec_action_drop);
			detected_actions |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			detected_actions |= MLX5_FLOW_ACTION_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			detected_actions |= MLX5_FLOW_ACTION_RSS;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) || \
	defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
			size += sizeof(struct ibv_flow_spec_counter_action);
#endif
			detected_actions |= MLX5_FLOW_ACTION_COUNT;
			break;
		default:
			break;
		}
	}
	*action_flags = detected_actions;
	return size;
}

/**
 * Calculate the required bytes that are needed for the item part of the verbs
 * flow, in addtion returns bit-fields with all the detected action, in order to
 * avoid another interation over the actions.
 *
 * @param[in] actions
 *   Pointer to the list of items.
 * @param[in, out] item_flags
 *   Pointer to the detected items.
 *
 * @return
 *   The size of the memory needed for all items.
 */
static int
flow_verbs_get_items_and_size(const struct rte_flow_item items[],
			      uint64_t *item_flags)
{
	int size = 0;
	uint64_t detected_items = 0;

	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(detected_items & MLX5_FLOW_LAYER_TUNNEL);

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			size += sizeof(struct ibv_flow_spec_eth);
			detected_items |= tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
						   MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			size += sizeof(struct ibv_flow_spec_eth);
			detected_items |= tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
						   MLX5_FLOW_LAYER_OUTER_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			size += sizeof(struct ibv_flow_spec_ipv4_ext);
			detected_items |= tunnel ?
					  MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					  MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			size += sizeof(struct ibv_flow_spec_ipv6);
			detected_items |= tunnel ?
					  MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					  MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			size += sizeof(struct ibv_flow_spec_tcp_udp);
			detected_items |= tunnel ?
					  MLX5_FLOW_LAYER_INNER_L4_UDP :
					  MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			size += sizeof(struct ibv_flow_spec_tcp_udp);
			detected_items |= tunnel ?
					  MLX5_FLOW_LAYER_INNER_L4_TCP :
					  MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			size += sizeof(struct ibv_flow_spec_tunnel);
			detected_items |= MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			size += sizeof(struct ibv_flow_spec_tunnel);
			detected_items |= MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
		case RTE_FLOW_ITEM_TYPE_GRE:
			size += sizeof(struct ibv_flow_spec_gre);
			detected_items |= MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			size += sizeof(struct ibv_flow_spec_mpls);
			detected_items |= MLX5_FLOW_LAYER_MPLS;
			break;
#else
		case RTE_FLOW_ITEM_TYPE_GRE:
			size += sizeof(struct ibv_flow_spec_tunnel);
			detected_items |= MLX5_FLOW_LAYER_TUNNEL;
			break;
#endif
		default:
			break;
		}
	}
	*item_flags = detected_items;
	return size;
}

/**
 * Internal preparation function. Allocate mlx5_flow with the required size.
 * The required size is calculate based on the actions and items. This function
 * also returns the detected actions and items for later use.
 *
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] item_flags
 *   Pointer to bit mask of all items detected.
 * @param[out] action_flags
 *   Pointer to bit mask of all actions detected.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   Pointer to mlx5_flow object on success, otherwise NULL and rte_errno
 *   is set.
 */
static struct mlx5_flow *
flow_verbs_prepare(const struct rte_flow_attr *attr __rte_unused,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   uint64_t *item_flags,
		   uint64_t *action_flags,
		   struct rte_flow_error *error)
{
	uint32_t size = sizeof(struct mlx5_flow) + sizeof(struct ibv_flow_attr);
	struct mlx5_flow *flow;

	size += flow_verbs_get_actions_and_size(actions, action_flags);
	size += flow_verbs_get_items_and_size(items, item_flags);
	flow = rte_calloc(__func__, 1, size, 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not enough memory to create flow");
		return NULL;
	}
	flow->verbs.attr = (void *)(flow + 1);
	flow->verbs.specs =
		(uint8_t *)(flow + 1) + sizeof(struct ibv_flow_attr);
	return flow;
}

/**
 * Fill the flow with verb spec.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5 flow.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, else a negative errno value otherwise and rte_ernno is set.
 */
static int
flow_verbs_translate(struct rte_eth_dev *dev,
		     struct mlx5_flow *dev_flow,
		     const struct rte_flow_attr *attr,
		     const struct rte_flow_item items[],
		     const struct rte_flow_action actions[],
		     struct rte_flow_error *error)
{
	uint64_t action_flags = 0;
	uint64_t item_flags = 0;
	uint64_t priority = attr->priority;
	struct priv *priv = dev->data->dev_private;

	if (priority == MLX5_FLOW_PRIO_RSVD)
		priority = priv->config.flow_prio - 1;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		int ret;
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			flow_verbs_translate_action_flag(actions,
							 &action_flags,
							 dev_flow);
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			flow_verbs_translate_action_mark(actions,
							 &action_flags,
							 dev_flow);
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			flow_verbs_translate_action_drop(&action_flags,
							 dev_flow);
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			flow_verbs_translate_action_queue(actions,
							  &action_flags,
							  dev_flow);
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			flow_verbs_translate_action_rss(actions,
							&action_flags,
							dev_flow);
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_verbs_translate_action_count(dev,
								actions,
								&action_flags,
								dev_flow,
								error);
			if (ret < 0)
				return ret;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	/* Device flow should have action flags by flow_drv_prepare(). */
	assert(dev_flow->flow->actions == action_flags);
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			flow_verbs_translate_item_eth(items, &item_flags,
						      dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			flow_verbs_translate_item_vlan(items, &item_flags,
						       dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			flow_verbs_translate_item_ipv4(items, &item_flags,
						       dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			flow_verbs_translate_item_ipv6(items, &item_flags,
						       dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			flow_verbs_translate_item_udp(items, &item_flags,
						      dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			flow_verbs_translate_item_tcp(items, &item_flags,
						      dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			flow_verbs_translate_item_vxlan(items, &item_flags,
							dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			flow_verbs_translate_item_vxlan_gpe(items, &item_flags,
							    dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			flow_verbs_translate_item_gre(items, &item_flags,
						      dev_flow);
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			flow_verbs_translate_item_mpls(items, &item_flags,
						       dev_flow);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL,
						  "item not supported");
		}
	}
	dev_flow->verbs.attr->priority =
		mlx5_flow_adjust_priority(dev, priority,
					  dev_flow->verbs.attr->priority);
	return 0;
}

/**
 * Remove the flow from the NIC but keeps it in memory.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static void
flow_verbs_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow_verbs *verbs;
	struct mlx5_flow *dev_flow;

	if (!flow)
		return;
	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		verbs = &dev_flow->verbs;
		if (verbs->flow) {
			claim_zero(mlx5_glue->destroy_flow(verbs->flow));
			verbs->flow = NULL;
		}
		if (verbs->hrxq) {
			if (flow->actions & MLX5_FLOW_ACTION_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, verbs->hrxq);
			verbs->hrxq = NULL;
		}
	}
	if (flow->counter) {
		flow_verbs_counter_release(flow->counter);
		flow->counter = NULL;
	}
}

/**
 * Remove the flow from the NIC and the memory.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static void
flow_verbs_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow *dev_flow;

	if (!flow)
		return;
	flow_verbs_remove(dev, flow);
	while (!LIST_EMPTY(&flow->dev_flows)) {
		dev_flow = LIST_FIRST(&flow->dev_flows);
		LIST_REMOVE(dev_flow, next);
		rte_free(dev_flow);
	}
}

/**
 * Apply the flow to the NIC.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_verbs_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct mlx5_flow_verbs *verbs;
	struct mlx5_flow *dev_flow;
	int err;

	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		verbs = &dev_flow->verbs;
		if (flow->actions & MLX5_FLOW_ACTION_DROP) {
			verbs->hrxq = mlx5_hrxq_drop_new(dev);
			if (!verbs->hrxq) {
				rte_flow_error_set
					(error, errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get drop hash queue");
				goto error;
			}
		} else {
			struct mlx5_hrxq *hrxq;

			hrxq = mlx5_hrxq_get(dev, flow->key,
					     MLX5_RSS_HASH_KEY_LEN,
					     verbs->hash_fields,
					     (*flow->queue),
					     flow->rss.queue_num);
			if (!hrxq)
				hrxq = mlx5_hrxq_new(dev, flow->key,
						     MLX5_RSS_HASH_KEY_LEN,
						     verbs->hash_fields,
						     (*flow->queue),
						     flow->rss.queue_num,
						     !!(dev_flow->layers &
						      MLX5_FLOW_LAYER_TUNNEL));
			if (!hrxq) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get hash queue");
				goto error;
			}
			verbs->hrxq = hrxq;
		}
		verbs->flow = mlx5_glue->create_flow(verbs->hrxq->qp,
						     verbs->attr);
		if (!verbs->flow) {
			rte_flow_error_set(error, errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "hardware refuses to create flow");
			goto error;
		}
	}
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		verbs = &dev_flow->verbs;
		if (verbs->hrxq) {
			if (flow->actions & MLX5_FLOW_ACTION_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, verbs->hrxq);
			verbs->hrxq = NULL;
		}
	}
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Query a flow.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
static int
flow_verbs_query(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 const struct rte_flow_action *actions,
		 void *data,
		 struct rte_flow_error *error)
{
	int ret = -EINVAL;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_verbs_counter_query(dev, flow, data, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	return ret;
}

const struct mlx5_flow_driver_ops mlx5_flow_verbs_drv_ops = {
	.validate = flow_verbs_validate,
	.prepare = flow_verbs_prepare,
	.translate = flow_verbs_translate,
	.apply = flow_verbs_apply,
	.remove = flow_verbs_remove,
	.destroy = flow_verbs_destroy,
	.query = flow_verbs_query,
};
