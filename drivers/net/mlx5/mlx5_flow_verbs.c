/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <netinet/in.h>
#include <sys/queue.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_ip.h>

#include <mlx5_glue.h>
#include <mlx5_prm.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_flow.h"
#include "mlx5_rx.h"
#include "mlx5_flow_os.h"

#define VERBS_SPEC_INNER(item_flags) \
	(!!((item_flags) & MLX5_FLOW_LAYER_TUNNEL) ? IBV_FLOW_SPEC_INNER : 0)

/* Verbs specification header. */
struct ibv_spec_header {
	enum ibv_flow_spec_type type;
	uint16_t size;
};

/**
 * Discover the maximum number of priority available.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] vprio
 *   Expected result variants.
 * @param[in] vprio_n
 *   Number of entries in @p vprio array.
 * @return
 *   Number of supported flow priority on success, a negative errno
 *   value otherwise and rte_errno is set.
 */
static int
flow_verbs_discover_priorities(struct rte_eth_dev *dev,
			       const uint16_t *vprio, int vprio_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct {
		struct ibv_flow_attr attr;
		struct ibv_flow_spec_eth eth;
		struct ibv_flow_spec_action_drop drop;
	} flow_attr = {
		.attr = {
			.num_of_specs = 2,
			.port = (uint8_t)priv->dev_port,
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
	struct mlx5_hrxq *drop = priv->drop_queue.hrxq;
	int i;
	int priority = 0;

#if defined(HAVE_MLX5DV_DR_DEVX_PORT) || defined(HAVE_MLX5DV_DR_DEVX_PORT_V35)
	/* If DevX supported, driver must support 16 verbs flow priorities. */
	priority = 16;
	goto out;
#endif
	if (!drop->qp) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	for (i = 0; i != vprio_n; i++) {
		flow_attr.attr.priority = vprio[i] - 1;
		flow = mlx5_glue->create_flow(drop->qp, &flow_attr.attr);
		if (!flow)
			break;
		claim_zero(mlx5_glue->destroy_flow(flow));
		priority = vprio[i];
	}
#if defined(HAVE_MLX5DV_DR_DEVX_PORT) || defined(HAVE_MLX5DV_DR_DEVX_PORT_V35)
out:
#endif
	DRV_LOG(INFO, "port %u supported flow priorities:"
		" 0-%d for ingress or egress root table,"
		" 0-%d for non-root table or transfer root table.",
		dev->data->port_id, priority - 2,
		MLX5_NON_ROOT_FLOW_MAX_PRIO - 1);
	return priority;
}

/**
 * Get Verbs flow counter by index.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] idx
 *   mlx5 flow counter index in the container.
 * @param[out] ppool
 *   mlx5 flow counter pool in the container,
 *
 * @return
 *   A pointer to the counter, NULL otherwise.
 */
static struct mlx5_flow_counter *
flow_verbs_counter_get_by_idx(struct rte_eth_dev *dev,
			      uint32_t idx,
			      struct mlx5_flow_counter_pool **ppool)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_mng *cmng = &priv->sh->sws_cmng;
	struct mlx5_flow_counter_pool *pool;

	idx = (idx - 1) & (MLX5_CNT_SHARED_OFFSET - 1);
	pool = cmng->pools[idx / MLX5_COUNTERS_PER_POOL];
	MLX5_ASSERT(pool);
	if (ppool)
		*ppool = pool;
	return MLX5_POOL_GET_CNT(pool, idx % MLX5_COUNTERS_PER_POOL);
}

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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_context *ctx = priv->sh->cdev->ctx;
	struct ibv_counter_set_init_attr init = {
			 .counter_set_id = counter->shared_info.id};

	counter->dcs_when_free = mlx5_glue->create_counter_set(ctx, &init);
	if (!counter->dcs_when_free) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	return 0;
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_context *ctx = priv->sh->cdev->ctx;
	struct ibv_counters_init_attr init = {0};
	struct ibv_counter_attach_attr attach;
	int ret;

	memset(&attach, 0, sizeof(attach));
	counter->dcs_when_free = mlx5_glue->create_counters(ctx, &init);
	if (!counter->dcs_when_free) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	attach.counter_desc = IBV_COUNTER_PACKETS;
	attach.index = 0;
	ret = mlx5_glue->attach_counters(counter->dcs_when_free, &attach, NULL);
	if (!ret) {
		attach.counter_desc = IBV_COUNTER_BYTES;
		attach.index = 1;
		ret = mlx5_glue->attach_counters
					(counter->dcs_when_free, &attach, NULL);
	}
	if (ret) {
		claim_zero(mlx5_glue->destroy_counters(counter->dcs_when_free));
		counter->dcs_when_free = NULL;
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
 * @param[in] id
 *   Counter identifier.
 *
 * @return
 *   Index to the counter, 0 otherwise and rte_errno is set.
 */
static uint32_t
flow_verbs_counter_new(struct rte_eth_dev *dev, uint32_t id __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_mng *cmng = &priv->sh->sws_cmng;
	struct mlx5_flow_counter_pool *pool = NULL;
	struct mlx5_flow_counter *cnt = NULL;
	uint32_t n_valid = cmng->n_valid;
	uint32_t pool_idx, cnt_idx;
	uint32_t i;
	int ret;

	for (pool_idx = 0; pool_idx < n_valid; ++pool_idx) {
		pool = cmng->pools[pool_idx];
		if (!pool)
			continue;
		cnt = TAILQ_FIRST(&pool->counters[0]);
		if (cnt)
			break;
	}
	if (!cnt) {
		uint32_t size;

		if (n_valid == MLX5_COUNTER_POOLS_MAX_NUM) {
			DRV_LOG(ERR, "All counter is in used, try again later.");
			rte_errno = EAGAIN;
			return 0;
		}
		/* Allocate memory for new pool */
		size = sizeof(*pool) + sizeof(*cnt) * MLX5_COUNTERS_PER_POOL;
		pool = mlx5_malloc(MLX5_MEM_ZERO, size, 0, SOCKET_ID_ANY);
		if (!pool)
			return 0;
		for (i = 0; i < MLX5_COUNTERS_PER_POOL; ++i) {
			cnt = MLX5_POOL_GET_CNT(pool, i);
			TAILQ_INSERT_HEAD(&pool->counters[0], cnt, next);
		}
		cnt = MLX5_POOL_GET_CNT(pool, 0);
		cmng->pools[n_valid] = pool;
		pool_idx = n_valid;
		cmng->n_valid++;
	}
	TAILQ_REMOVE(&pool->counters[0], cnt, next);
	i = MLX5_CNT_ARRAY_IDX(pool, cnt);
	cnt_idx = MLX5_MAKE_CNT_IDX(pool_idx, i);
	/* Create counter with Verbs. */
	ret = flow_verbs_counter_create(dev, cnt);
	if (!ret) {
		cnt->dcs_when_active = cnt->dcs_when_free;
		cnt->hits = 0;
		cnt->bytes = 0;
		return cnt_idx;
	}
	TAILQ_INSERT_HEAD(&pool->counters[0], cnt, next);
	/* Some error occurred in Verbs library. */
	rte_errno = -ret;
	return 0;
}

/**
 * Release a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] counter
 *   Index to the counter handler.
 */
static void
flow_verbs_counter_release(struct rte_eth_dev *dev, uint32_t counter)
{
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_flow_counter *cnt;

	cnt = flow_verbs_counter_get_by_idx(dev, counter, &pool);
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
	claim_zero(mlx5_glue->destroy_counter_set
			((struct ibv_counter_set *)cnt->dcs_when_active));
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	claim_zero(mlx5_glue->destroy_counters
				((struct ibv_counters *)cnt->dcs_when_active));
#endif
	TAILQ_INSERT_HEAD(&pool->counters[0], cnt, next);
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
	if (flow->counter) {
		struct mlx5_flow_counter_pool *pool;
		struct mlx5_flow_counter *cnt = flow_verbs_counter_get_by_idx
						(dev, flow->counter, &pool);
		struct rte_flow_query_count *qc = data;
		uint64_t counters[2] = {0, 0};
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
		struct ibv_query_counter_set_attr query_cs_attr = {
			.dcs_when_free = (struct ibv_counter_set *)
						cnt->dcs_when_active,
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
			((struct ibv_counters *)cnt->dcs_when_active, counters,
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
		qc->hits = counters[0] - cnt->hits;
		qc->bytes = counters[1] - cnt->bytes;
		if (qc->reset) {
			cnt->hits = counters[0];
			cnt->bytes = counters[1];
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
 * Add a verbs item specification into @p verbs.
 *
 * @param[out] verbs
 *   Pointer to verbs structure.
 * @param[in] src
 *   Create specification.
 * @param[in] size
 *   Size in bytes of the specification to copy.
 */
static void
flow_verbs_spec_add(struct mlx5_flow_verbs_workspace *verbs,
		    void *src, unsigned int size)
{
	void *dst;

	if (!verbs)
		return;
	MLX5_ASSERT(verbs->specs);
	dst = (void *)(verbs->specs + verbs->size);
	memcpy(dst, src, size);
	++verbs->attr.num_of_specs;
	verbs->size += size;
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_eth(struct mlx5_flow *dev_flow,
			      const struct rte_flow_item *item,
			      uint64_t item_flags)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	const unsigned int size = sizeof(struct ibv_flow_spec_eth);
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH | VERBS_SPEC_INNER(item_flags),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_eth_mask;
	if (spec) {
		unsigned int i;

		memcpy(&eth.val.dst_mac, spec->hdr.dst_addr.addr_bytes,
			RTE_ETHER_ADDR_LEN);
		memcpy(&eth.val.src_mac, spec->hdr.src_addr.addr_bytes,
			RTE_ETHER_ADDR_LEN);
		eth.val.ether_type = spec->hdr.ether_type;
		memcpy(&eth.mask.dst_mac, mask->hdr.dst_addr.addr_bytes,
			RTE_ETHER_ADDR_LEN);
		memcpy(&eth.mask.src_mac, mask->hdr.src_addr.addr_bytes,
			RTE_ETHER_ADDR_LEN);
		eth.mask.ether_type = mask->hdr.ether_type;
		/* Remove unwanted bits from values. */
		for (i = 0; i < RTE_ETHER_ADDR_LEN; ++i) {
			eth.val.dst_mac[i] &= eth.mask.dst_mac[i];
			eth.val.src_mac[i] &= eth.mask.src_mac[i];
		}
		eth.val.ether_type &= eth.mask.ether_type;
	}
	flow_verbs_spec_add(&dev_flow->verbs, &eth, size);
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
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_vlan(struct mlx5_flow *dev_flow,
			       const struct rte_flow_item *item,
			       uint64_t item_flags)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_eth);
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH | VERBS_SPEC_INNER(item_flags),
		.size = size,
	};
	const uint32_t l2m = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
				      MLX5_FLOW_LAYER_OUTER_L2;

	if (!mask)
		mask = &rte_flow_item_vlan_mask;
	if (spec) {
		eth.val.vlan_tag = spec->hdr.vlan_tci;
		eth.mask.vlan_tag = mask->hdr.vlan_tci;
		eth.val.vlan_tag &= eth.mask.vlan_tag;
		eth.val.ether_type = spec->hdr.eth_proto;
		eth.mask.ether_type = mask->hdr.eth_proto;
		eth.val.ether_type &= eth.mask.ether_type;
	}
	if (!(item_flags & l2m))
		flow_verbs_spec_add(&dev_flow->verbs, &eth, size);
	else
		flow_verbs_item_vlan_update(&dev_flow->verbs.attr, &eth);
	if (!tunnel)
		dev_flow->handle->vf_vlan.tag =
			rte_be_to_cpu_16(spec->hdr.vlan_tci) & 0x0fff;
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_ipv4(struct mlx5_flow *dev_flow,
			       const struct rte_flow_item *item,
			       uint64_t item_flags)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_ipv4_ext);
	struct ibv_flow_spec_ipv4_ext ipv4 = {
		.type = IBV_FLOW_SPEC_IPV4_EXT | VERBS_SPEC_INNER(item_flags),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_ipv4_mask;
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
	flow_verbs_spec_add(&dev_flow->verbs, &ipv4, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_ipv6(struct mlx5_flow *dev_flow,
			       const struct rte_flow_item *item,
			       uint64_t item_flags)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_ipv6);
	struct ibv_flow_spec_ipv6 ipv6 = {
		.type = IBV_FLOW_SPEC_IPV6 | VERBS_SPEC_INNER(item_flags),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_ipv6_mask;
	if (spec) {
		unsigned int i;
		uint32_t vtc_flow_val;
		uint32_t vtc_flow_mask;

		memcpy(&ipv6.val.src_ip, &spec->hdr.src_addr,
		       RTE_DIM(ipv6.val.src_ip));
		memcpy(&ipv6.val.dst_ip, &spec->hdr.dst_addr,
		       RTE_DIM(ipv6.val.dst_ip));
		memcpy(&ipv6.mask.src_ip, &mask->hdr.src_addr,
		       RTE_DIM(ipv6.mask.src_ip));
		memcpy(&ipv6.mask.dst_ip, &mask->hdr.dst_addr,
		       RTE_DIM(ipv6.mask.dst_ip));
		vtc_flow_val = rte_be_to_cpu_32(spec->hdr.vtc_flow);
		vtc_flow_mask = rte_be_to_cpu_32(mask->hdr.vtc_flow);
		ipv6.val.flow_label =
			rte_cpu_to_be_32((vtc_flow_val & RTE_IPV6_HDR_FL_MASK) >>
					 RTE_IPV6_HDR_FL_SHIFT);
		ipv6.val.traffic_class = (vtc_flow_val & RTE_IPV6_HDR_TC_MASK) >>
					 RTE_IPV6_HDR_TC_SHIFT;
		ipv6.val.next_hdr = spec->hdr.proto;
		ipv6.mask.flow_label =
			rte_cpu_to_be_32((vtc_flow_mask & RTE_IPV6_HDR_FL_MASK) >>
					 RTE_IPV6_HDR_FL_SHIFT);
		ipv6.mask.traffic_class = (vtc_flow_mask & RTE_IPV6_HDR_TC_MASK) >>
					  RTE_IPV6_HDR_TC_SHIFT;
		ipv6.mask.next_hdr = mask->hdr.proto;
		/* Remove unwanted bits from values. */
		for (i = 0; i < RTE_DIM(ipv6.val.src_ip); ++i) {
			ipv6.val.src_ip[i] &= ipv6.mask.src_ip[i];
			ipv6.val.dst_ip[i] &= ipv6.mask.dst_ip[i];
		}
		ipv6.val.flow_label &= ipv6.mask.flow_label;
		ipv6.val.traffic_class &= ipv6.mask.traffic_class;
		ipv6.val.next_hdr &= ipv6.mask.next_hdr;
	}
	flow_verbs_spec_add(&dev_flow->verbs, &ipv6, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_tcp(struct mlx5_flow *dev_flow,
			      const struct rte_flow_item *item,
			      uint64_t item_flags __rte_unused)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp tcp = {
		.type = IBV_FLOW_SPEC_TCP | VERBS_SPEC_INNER(item_flags),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_tcp_mask;
	if (spec) {
		tcp.val.dst_port = spec->hdr.dst_port;
		tcp.val.src_port = spec->hdr.src_port;
		tcp.mask.dst_port = mask->hdr.dst_port;
		tcp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		tcp.val.src_port &= tcp.mask.src_port;
		tcp.val.dst_port &= tcp.mask.dst_port;
	}
	flow_verbs_spec_add(&dev_flow->verbs, &tcp, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
#ifdef HAVE_IBV_FLOW_SPEC_ESP
static void
flow_verbs_translate_item_esp(struct mlx5_flow *dev_flow,
			      const struct rte_flow_item *item,
			      uint64_t item_flags __rte_unused)
{
	const struct rte_flow_item_esp *spec = item->spec;
	const struct rte_flow_item_esp *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_esp);
	struct ibv_flow_spec_esp esp = {
		.type = IBV_FLOW_SPEC_ESP | VERBS_SPEC_INNER(item_flags),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_esp_mask;
	if (spec) {
		esp.val.spi = spec->hdr.spi & mask->hdr.spi;
		esp.mask.spi = mask->hdr.spi;
	}
	flow_verbs_spec_add(&dev_flow->verbs, &esp, size);
}
#endif

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_udp(struct mlx5_flow *dev_flow,
			      const struct rte_flow_item *item,
			      uint64_t item_flags __rte_unused)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp udp = {
		.type = IBV_FLOW_SPEC_UDP | VERBS_SPEC_INNER(item_flags),
		.size = size,
	};

	if (!mask)
		mask = &rte_flow_item_udp_mask;
	if (spec) {
		udp.val.dst_port = spec->hdr.dst_port;
		udp.val.src_port = spec->hdr.src_port;
		udp.mask.dst_port = mask->hdr.dst_port;
		udp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		udp.val.src_port &= udp.mask.src_port;
		udp.val.dst_port &= udp.mask.dst_port;
	}
	item++;
	while (item->type == RTE_FLOW_ITEM_TYPE_VOID)
		item++;
	if (!(udp.val.dst_port & udp.mask.dst_port)) {
		switch ((item)->type) {
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			udp.val.dst_port = htons(MLX5_UDP_PORT_VXLAN);
			udp.mask.dst_port = 0xffff;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			udp.val.dst_port = htons(MLX5_UDP_PORT_VXLAN_GPE);
			udp.mask.dst_port = 0xffff;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			udp.val.dst_port = htons(MLX5_UDP_PORT_MPLS);
			udp.mask.dst_port = 0xffff;
			break;
		default:
			break;
		}
	}

	flow_verbs_spec_add(&dev_flow->verbs, &udp, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_vxlan(struct mlx5_flow *dev_flow,
				const struct rte_flow_item *item,
				uint64_t item_flags __rte_unused)
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
		memcpy(&id.vni[1], spec->hdr.vni, 3);
		vxlan.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->hdr.vni, 3);
		vxlan.mask.tunnel_id = id.vlan_id;
		/* Remove unwanted bits from values. */
		vxlan.val.tunnel_id &= vxlan.mask.tunnel_id;
	}
	flow_verbs_spec_add(&dev_flow->verbs, &vxlan, size);
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested item
 * into the flow.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_vxlan_gpe(struct mlx5_flow *dev_flow,
				    const struct rte_flow_item *item,
				    uint64_t item_flags __rte_unused)
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
		memcpy(&id.vni[1], spec->hdr.vni, 3);
		vxlan_gpe.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->hdr.vni, 3);
		vxlan_gpe.mask.tunnel_id = id.vlan_id;
		/* Remove unwanted bits from values. */
		vxlan_gpe.val.tunnel_id &= vxlan_gpe.mask.tunnel_id;
	}
	flow_verbs_spec_add(&dev_flow->verbs, &vxlan_gpe, size);
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
 * Reserve space for GRE spec in spec buffer.
 *
 * @param[in,out] dev_flow
 *   Pointer to dev_flow structure.
 *
 * @return
 *   Pointer to reserved space in spec buffer.
 */
static uint8_t *
flow_verbs_reserve_gre(struct mlx5_flow *dev_flow)
{
	uint8_t *buffer;
	struct mlx5_flow_verbs_workspace *verbs = &dev_flow->verbs;
#ifndef HAVE_IBV_DEVICE_MPLS_SUPPORT
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel tunnel = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
#else
	unsigned int size = sizeof(struct ibv_flow_spec_gre);
	struct ibv_flow_spec_gre tunnel = {
		.type = IBV_FLOW_SPEC_GRE,
		.size = size,
	};
#endif

	buffer = verbs->specs + verbs->size;
	flow_verbs_spec_add(verbs, &tunnel, size);
	return buffer;
}

/**
 * Convert the @p item into a Verbs specification. This function assumes that
 * the input is valid and that Verbs specification will be placed in
 * the pre-reserved space.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in, out] gre_spec
 *   Pointer to space reserved for GRE spec.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_gre(struct mlx5_flow *dev_flow,
			      uint8_t *gre_spec,
			      const struct rte_flow_item *item __rte_unused,
			      uint64_t item_flags)
{
	struct mlx5_flow_verbs_workspace *verbs = &dev_flow->verbs;
#ifndef HAVE_IBV_DEVICE_MPLS_SUPPORT
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel tunnel = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
#else
	static const struct rte_flow_item_gre empty_gre = {0,};
	const struct rte_flow_item_gre *spec = item->spec;
	const struct rte_flow_item_gre *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_gre);
	struct ibv_flow_spec_gre tunnel = {
		.type = IBV_FLOW_SPEC_GRE,
		.size = size,
	};

	if (!spec) {
		spec = &empty_gre;
		mask = &empty_gre;
	} else {
		if (!mask)
			mask = &rte_flow_item_gre_mask;
	}
	tunnel.val.c_ks_res0_ver = spec->c_rsvd0_ver;
	tunnel.val.protocol = spec->protocol;
	tunnel.mask.c_ks_res0_ver = mask->c_rsvd0_ver;
	tunnel.mask.protocol = mask->protocol;
	/* Remove unwanted bits from values. */
	tunnel.val.c_ks_res0_ver &= tunnel.mask.c_ks_res0_ver;
	tunnel.val.key &= tunnel.mask.key;
	if (tunnel.mask.protocol) {
		tunnel.val.protocol &= tunnel.mask.protocol;
	} else {
		tunnel.val.protocol = mlx5_translate_tunnel_etypes(item_flags);
		if (tunnel.val.protocol) {
			tunnel.mask.protocol = 0xFFFF;
			tunnel.val.protocol =
				rte_cpu_to_be_16(tunnel.val.protocol);
		}
	}
#endif
	if (item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV4)
		flow_verbs_item_gre_ip_protocol_update(&verbs->attr,
						       IBV_FLOW_SPEC_IPV4_EXT,
						       IPPROTO_GRE);
	else
		flow_verbs_item_gre_ip_protocol_update(&verbs->attr,
						       IBV_FLOW_SPEC_IPV6,
						       IPPROTO_GRE);
	MLX5_ASSERT(gre_spec);
	memcpy(gre_spec, &tunnel, size);
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow. This function also return the action that was added.
 *
 * @param[in, out] dev_flow
 *   Pointer to dev_flow structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Parsed item flags.
 */
static void
flow_verbs_translate_item_mpls(struct mlx5_flow *dev_flow __rte_unused,
			       const struct rte_flow_item *item __rte_unused,
			       uint64_t item_flags __rte_unused)
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
	flow_verbs_spec_add(&dev_flow->verbs, &mpls, size);
#endif
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow.
 *
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 * @param[in] action
 *   Action configuration.
 */
static void
flow_verbs_translate_action_drop
	(struct mlx5_flow *dev_flow,
	 const struct rte_flow_action *action __rte_unused)
{
	unsigned int size = sizeof(struct ibv_flow_spec_action_drop);
	struct ibv_flow_spec_action_drop drop = {
			.type = IBV_FLOW_SPEC_ACTION_DROP,
			.size = size,
	};

	flow_verbs_spec_add(&dev_flow->verbs, &drop, size);
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow.
 *
 * @param[in] rss_desc
 *   Pointer to mlx5_flow_rss_desc.
 * @param[in] action
 *   Action configuration.
 */
static void
flow_verbs_translate_action_queue(struct mlx5_flow_rss_desc *rss_desc,
				  const struct rte_flow_action *action)
{
	const struct rte_flow_action_queue *queue = action->conf;

	rss_desc->queue[0] = queue->index;
	rss_desc->queue_num = 1;
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow.
 *
 * @param[in] rss_desc
 *   Pointer to mlx5_flow_rss_desc.
 * @param[in] action
 *   Action configuration.
 */
static void
flow_verbs_translate_action_rss(struct mlx5_flow_rss_desc *rss_desc,
				const struct rte_flow_action *action)
{
	const struct rte_flow_action_rss *rss = action->conf;
	const uint8_t *rss_key;

	memcpy(rss_desc->queue, rss->queue, rss->queue_num * sizeof(uint16_t));
	rss_desc->queue_num = rss->queue_num;
	/* NULL RSS key indicates default RSS key. */
	rss_key = !rss->key ? rss_hash_default_key : rss->key;
	memcpy(rss_desc->key, rss_key, MLX5_RSS_HASH_KEY_LEN);
	/*
	 * rss->level and rss.types should be set in advance when expanding
	 * items for RSS.
	 */
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow.
 *
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 * @param[in] action
 *   Action configuration.
 */
static void
flow_verbs_translate_action_flag
	(struct mlx5_flow *dev_flow,
	 const struct rte_flow_action *action __rte_unused)
{
	unsigned int size = sizeof(struct ibv_flow_spec_action_tag);
	struct ibv_flow_spec_action_tag tag = {
		.type = IBV_FLOW_SPEC_ACTION_TAG,
		.size = size,
		.tag_id = mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT),
	};

	flow_verbs_spec_add(&dev_flow->verbs, &tag, size);
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow.
 *
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 * @param[in] action
 *   Action configuration.
 */
static void
flow_verbs_translate_action_mark(struct mlx5_flow *dev_flow,
				 const struct rte_flow_action *action)
{
	const struct rte_flow_action_mark *mark = action->conf;
	unsigned int size = sizeof(struct ibv_flow_spec_action_tag);
	struct ibv_flow_spec_action_tag tag = {
		.type = IBV_FLOW_SPEC_ACTION_TAG,
		.size = size,
		.tag_id = mlx5_flow_mark_set(mark->id),
	};

	flow_verbs_spec_add(&dev_flow->verbs, &tag, size);
}

/**
 * Convert the @p action into a Verbs specification. This function assumes that
 * the input is valid and that there is space to insert the requested action
 * into the flow.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] action
 *   Action configuration.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 On success else a negative errno value is returned and rte_errno is set.
 */
static int
flow_verbs_translate_action_count(struct mlx5_flow *dev_flow,
				  const struct rte_flow_action *action,
				  struct rte_eth_dev *dev,
				  struct rte_flow_error *error)
{
	const struct rte_flow_action_count *count = action->conf;
	struct rte_flow *flow = dev_flow->flow;
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) || \
	defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_flow_counter *cnt = NULL;
	unsigned int size = sizeof(struct ibv_flow_spec_counter_action);
	struct ibv_flow_spec_counter_action counter = {
		.type = IBV_FLOW_SPEC_ACTION_COUNT,
		.size = size,
	};
#endif

	if (!flow->counter) {
		flow->counter = flow_verbs_counter_new(dev, count->id);
		if (!flow->counter)
			return rte_flow_error_set(error, rte_errno,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "cannot get counter"
						  " context.");
	}
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
	cnt = flow_verbs_counter_get_by_idx(dev, flow->counter, &pool);
	counter.counter_set_handle =
		((struct ibv_counter_set *)cnt->dcs_when_active)->handle;
	flow_verbs_spec_add(&dev_flow->verbs, &counter, size);
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	cnt = flow_verbs_counter_get_by_idx(dev, flow->counter, &pool);
	counter.counters = (struct ibv_counters *)cnt->dcs_when_active;
	flow_verbs_spec_add(&dev_flow->verbs, &counter, size);
#endif
	return 0;
}

/**
 * Validates @p attributes of the flow rule.
 *
 * This function is used if and only if legacy Verbs flow engine is used.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attributes
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_verbs_validate_attributes(struct rte_eth_dev *dev,
			       const struct rte_flow_attr *attributes,
			       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t priority_max = priv->sh->flow_max_priority - 1;

	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL, "groups is not supported");
	if (attributes->priority != MLX5_FLOW_LOWEST_PRIO_INDICATOR &&
	    attributes->priority >= priority_max)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  NULL, "priority out of range");
	if (attributes->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "egress is not supported");
	if (attributes->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  NULL, "transfer is not supported");
	if (!attributes->ingress)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  NULL,
					  "ingress attribute is mandatory");
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
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[in] hairpin
 *   Number of hairpin TX actions, 0 means classic flow.
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
		    bool external __rte_unused,
		    int hairpin __rte_unused,
		    struct rte_flow_error *error)
{
	int ret;
	uint64_t action_flags = 0;
	uint64_t item_flags = 0;
	uint64_t last_item = 0;
	uint8_t next_protocol = 0xff;
	uint16_t ether_type = 0;
	bool is_empty_vlan = false;
	uint16_t udp_dport = 0;
	/* Verbs interface does not support groups higher than 0. */
	bool is_root = true;

	if (items == NULL)
		return -1;
	ret = flow_verbs_validate_attributes(dev, attr, error);
	if (ret < 0)
		return ret;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		int ret = 0;

		switch (items->type) {
#ifdef HAVE_IBV_FLOW_SPEC_ESP
		case RTE_FLOW_ITEM_TYPE_ESP:
			ret = mlx5_flow_os_validate_item_esp(dev, items, item_flags,
							     next_protocol, false, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_ESP;
			break;
#endif
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(dev, items, item_flags,
							  false, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					     MLX5_FLOW_LAYER_OUTER_L2;
			if (items->mask != NULL && items->spec != NULL) {
				ether_type =
					((const struct rte_flow_item_eth *)
					 items->spec)->hdr.ether_type;
				ether_type &=
					((const struct rte_flow_item_eth *)
					 items->mask)->hdr.ether_type;
				if (ether_type == RTE_BE16(RTE_ETHER_TYPE_VLAN))
					is_empty_vlan = true;
				ether_type = rte_be_to_cpu_16(ether_type);
			} else {
				ether_type = 0;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5_flow_validate_item_vlan(items, item_flags,
							   dev, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? (MLX5_FLOW_LAYER_INNER_L2 |
					      MLX5_FLOW_LAYER_INNER_VLAN) :
					     (MLX5_FLOW_LAYER_OUTER_L2 |
					      MLX5_FLOW_LAYER_OUTER_VLAN);
			if (items->mask != NULL && items->spec != NULL) {
				ether_type =
					((const struct rte_flow_item_vlan *)
					 items->spec)->hdr.eth_proto;
				ether_type &=
					((const struct rte_flow_item_vlan *)
					 items->mask)->hdr.eth_proto;
				ether_type = rte_be_to_cpu_16(ether_type);
			} else {
				ether_type = 0;
			}
			is_empty_vlan = false;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_validate_item_ipv4
						(dev, items, item_flags,
						 last_item, ether_type, NULL,
						 MLX5_ITEM_RANGE_NOT_ACCEPTED,
						 error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv4 *)
			     items->mask)->hdr.next_proto_id) {
				next_protocol =
					((const struct rte_flow_item_ipv4 *)
					 (items->spec))->hdr.next_proto_id;
				next_protocol &=
					((const struct rte_flow_item_ipv4 *)
					 (items->mask))->hdr.next_proto_id;
			} else {
				/* Reset for inner layer. */
				next_protocol = 0xff;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_validate_item_ipv6(dev, items,
							   item_flags,
							   last_item,
							   ether_type, NULL,
							   error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv6 *)
			     items->mask)->hdr.proto) {
				next_protocol =
					((const struct rte_flow_item_ipv6 *)
					 items->spec)->hdr.proto;
				next_protocol &=
					((const struct rte_flow_item_ipv6 *)
					 items->mask)->hdr.proto;
			} else {
				/* Reset for inner layer. */
				next_protocol = 0xff;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(dev, items,
							  item_flags,
							  next_protocol,
							  error);
			const struct rte_flow_item_udp *spec = items->spec;
			const struct rte_flow_item_udp *mask = items->mask;
			if (!mask)
				mask = &rte_flow_item_udp_mask;
			if (spec != NULL)
				udp_dport = rte_be_to_cpu_16
						(spec->hdr.dst_port &
						 mask->hdr.dst_port);

			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					     MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_validate_item_tcp
						(dev, items, item_flags,
						 next_protocol,
						 &rte_flow_item_tcp_mask,
						 error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					     MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5_flow_validate_item_vxlan(dev, udp_dport,
							    items, item_flags,
							    is_root, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			ret = mlx5_flow_validate_item_vxlan_gpe(items,
								item_flags,
								dev, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			ret = mlx5_flow_validate_item_gre(dev, items, item_flags,
							  next_protocol, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			ret = mlx5_flow_validate_item_mpls(dev, items,
							   item_flags,
							   last_item, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_MPLS;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
		case RTE_FLOW_ITEM_TYPE_ICMP6:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "ICMP/ICMP6 "
						  "item not supported");
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
		item_flags |= last_item;
	}
	if (is_empty_vlan)
		return rte_flow_error_set(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
		    "VLAN matching without vid specification is not supported");
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
			ret = mlx5_flow_validate_action_mark(dev, actions,
							     action_flags,
							     attr,
							     error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			ret = mlx5_flow_validate_action_drop(dev,
							     is_root,
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
							    attr, item_flags,
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
	/*
	 * Validate the drop action mutual exclusion with other actions.
	 * Drop action is mutually-exclusive with any other action, except for
	 * Count action.
	 */
	if ((action_flags & MLX5_FLOW_ACTION_DROP) &&
	    (action_flags & ~(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_COUNT)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Drop action is mutually-exclusive "
					  "with any other action, except for "
					  "Count action");
	if (!(action_flags & MLX5_FLOW_FATE_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "no fate action is found");
	return 0;
}

/**
 * Calculate the required bytes that are needed for the action part of the verbs
 * flow.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 *
 * @return
 *   The size of the memory needed for all actions.
 */
static int
flow_verbs_get_actions_size(const struct rte_flow_action actions[])
{
	int size = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			size += sizeof(struct ibv_flow_spec_action_tag);
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			size += sizeof(struct ibv_flow_spec_action_tag);
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			size += sizeof(struct ibv_flow_spec_action_drop);
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) || \
	defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
			size += sizeof(struct ibv_flow_spec_counter_action);
#endif
			break;
		default:
			break;
		}
	}
	return size;
}

/**
 * Calculate the required bytes that are needed for the item part of the verbs
 * flow.
 *
 * @param[in] items
 *   Pointer to the list of items.
 *
 * @return
 *   The size of the memory needed for all items.
 */
static int
flow_verbs_get_items_size(const struct rte_flow_item items[])
{
	int size = 0;

	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			size += sizeof(struct ibv_flow_spec_eth);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			size += sizeof(struct ibv_flow_spec_eth);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			size += sizeof(struct ibv_flow_spec_ipv4_ext);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			size += sizeof(struct ibv_flow_spec_ipv6);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			size += sizeof(struct ibv_flow_spec_tcp_udp);
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			size += sizeof(struct ibv_flow_spec_tcp_udp);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			size += sizeof(struct ibv_flow_spec_tunnel);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			size += sizeof(struct ibv_flow_spec_tunnel);
			break;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
		case RTE_FLOW_ITEM_TYPE_GRE:
			size += sizeof(struct ibv_flow_spec_gre);
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			size += sizeof(struct ibv_flow_spec_mpls);
			break;
#else
		case RTE_FLOW_ITEM_TYPE_GRE:
			size += sizeof(struct ibv_flow_spec_tunnel);
			break;
#endif
		default:
			break;
		}
	}
	return size;
}

/**
 * Internal preparation function. Allocate mlx5_flow with the required size.
 * The required size is calculate based on the actions and items. This function
 * also returns the detected actions and items for later use.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
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
 *   Pointer to mlx5_flow object on success, otherwise NULL and rte_errno
 *   is set.
 */
static struct mlx5_flow *
flow_verbs_prepare(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr __rte_unused,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	size_t size = 0;
	uint32_t handle_idx = 0;
	struct mlx5_flow *dev_flow;
	struct mlx5_flow_handle *dev_handle;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();

	MLX5_ASSERT(wks);
	size += flow_verbs_get_actions_size(actions);
	size += flow_verbs_get_items_size(items);
	if (size > MLX5_VERBS_MAX_SPEC_ACT_SIZE) {
		rte_flow_error_set(error, E2BIG,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Verbs spec/action size too large");
		return NULL;
	}
	/* In case of corrupting the memory. */
	if (wks->flow_idx >= MLX5_NUM_MAX_DEV_FLOWS) {
		rte_flow_error_set(error, ENOSPC,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not free temporary device flow");
		return NULL;
	}
	dev_handle = mlx5_ipool_zmalloc(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
				   &handle_idx);
	if (!dev_handle) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not enough memory to create flow handle");
		return NULL;
	}
	MLX5_ASSERT(wks->flow_idx + 1 < RTE_DIM(wks->flows));
	dev_flow = &wks->flows[wks->flow_idx++];
	dev_flow->handle = dev_handle;
	dev_flow->handle_idx = handle_idx;
	/* Memcpy is used, only size needs to be cleared to 0. */
	dev_flow->verbs.size = 0;
	dev_flow->verbs.attr.num_of_specs = 0;
	dev_flow->ingress = attr->ingress;
	dev_flow->hash_fields = 0;
	/* Need to set transfer attribute: not supported in Verbs mode. */
	return dev_flow;
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
 *   0 on success, else a negative errno value otherwise and rte_errno is set.
 */
static int
flow_verbs_translate(struct rte_eth_dev *dev,
		     struct mlx5_flow *dev_flow,
		     const struct rte_flow_attr *attr,
		     const struct rte_flow_item items[],
		     const struct rte_flow_action actions[],
		     struct rte_flow_error *error)
{
	uint64_t item_flags = 0;
	uint64_t action_flags = 0;
	uint64_t priority = attr->priority;
	uint32_t subpriority = 0;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct mlx5_flow_rss_desc *rss_desc;
	const struct rte_flow_item *tunnel_item = NULL;
	uint8_t *gre_spec = NULL;

	MLX5_ASSERT(wks);
	rss_desc = &wks->rss_desc;
	if (priority == MLX5_FLOW_LOWEST_PRIO_INDICATOR)
		priority = priv->sh->flow_max_priority - 1;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		int ret;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			flow_verbs_translate_action_flag(dev_flow, actions);
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			wks->mark = 1;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			flow_verbs_translate_action_mark(dev_flow, actions);
			action_flags |= MLX5_FLOW_ACTION_MARK;
			wks->mark = 1;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			flow_verbs_translate_action_drop(dev_flow, actions);
			action_flags |= MLX5_FLOW_ACTION_DROP;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			flow_verbs_translate_action_queue(rss_desc, actions);
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			flow_verbs_translate_action_rss(rss_desc, actions);
			action_flags |= MLX5_FLOW_ACTION_RSS;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_verbs_translate_action_count(dev_flow,
								actions,
								dev, error);
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
	dev_flow->act_flags = action_flags;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			flow_verbs_translate_item_eth(dev_flow, items,
						      item_flags);
			subpriority = MLX5_PRIORITY_MAP_L2;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					       MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			flow_verbs_translate_item_vlan(dev_flow, items,
						       item_flags);
			subpriority = MLX5_PRIORITY_MAP_L2;
			item_flags |= tunnel ? (MLX5_FLOW_LAYER_INNER_L2 |
						MLX5_FLOW_LAYER_INNER_VLAN) :
					       (MLX5_FLOW_LAYER_OUTER_L2 |
						MLX5_FLOW_LAYER_OUTER_VLAN);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			flow_verbs_translate_item_ipv4(dev_flow, items,
						       item_flags);
			subpriority = MLX5_PRIORITY_MAP_L3;
			dev_flow->hash_fields |=
				mlx5_flow_hashfields_adjust
					(rss_desc, tunnel,
					 MLX5_IPV4_LAYER_TYPES,
					 MLX5_IPV4_IBV_RX_HASH);
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					       MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			flow_verbs_translate_item_ipv6(dev_flow, items,
						       item_flags);
			subpriority = MLX5_PRIORITY_MAP_L3;
			dev_flow->hash_fields |=
				mlx5_flow_hashfields_adjust
					(rss_desc, tunnel,
					 MLX5_IPV6_LAYER_TYPES,
					 MLX5_IPV6_IBV_RX_HASH);
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					       MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			flow_verbs_translate_item_tcp(dev_flow, items,
						      item_flags);
			subpriority = MLX5_PRIORITY_MAP_L4;
			if (dev_flow->hash_fields != 0)
				dev_flow->hash_fields |=
					mlx5_flow_hashfields_adjust
					(rss_desc, tunnel, RTE_ETH_RSS_TCP,
					 (IBV_RX_HASH_SRC_PORT_TCP |
					  IBV_RX_HASH_DST_PORT_TCP));
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					       MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			flow_verbs_translate_item_udp(dev_flow, items,
						      item_flags);
			subpriority = MLX5_PRIORITY_MAP_L4;
			if (dev_flow->hash_fields != 0)
				dev_flow->hash_fields |=
					mlx5_flow_hashfields_adjust
					(rss_desc, tunnel, RTE_ETH_RSS_UDP,
					 (IBV_RX_HASH_SRC_PORT_UDP |
					  IBV_RX_HASH_DST_PORT_UDP));
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					       MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
#ifdef HAVE_IBV_FLOW_SPEC_ESP
		case RTE_FLOW_ITEM_TYPE_ESP:
			flow_verbs_translate_item_esp(dev_flow, items,
						      item_flags);
			dev_flow->hash_fields |=
				mlx5_flow_hashfields_adjust
				(rss_desc, tunnel,
				RTE_ETH_RSS_ESP,
				IBV_RX_HASH_IPSEC_SPI);
			item_flags |= MLX5_FLOW_ITEM_ESP;
			break;
#endif
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			flow_verbs_translate_item_vxlan(dev_flow, items,
							item_flags);
			subpriority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			item_flags |= MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			flow_verbs_translate_item_vxlan_gpe(dev_flow, items,
							    item_flags);
			subpriority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			item_flags |= MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			gre_spec = flow_verbs_reserve_gre(dev_flow);
			subpriority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			item_flags |= MLX5_FLOW_LAYER_GRE;
			tunnel_item = items;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			flow_verbs_translate_item_mpls(dev_flow, items,
						       item_flags);
			subpriority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			item_flags |= MLX5_FLOW_LAYER_MPLS;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
	}
	if (item_flags & MLX5_FLOW_LAYER_GRE)
		flow_verbs_translate_item_gre(dev_flow, gre_spec,
					      tunnel_item, item_flags);
	dev_flow->handle->layers = item_flags;
	/* Other members of attr will be ignored. */
	dev_flow->verbs.attr.priority =
		mlx5_flow_adjust_priority(dev, priority, subpriority);
	dev_flow->verbs.attr.port = (uint8_t)priv->dev_port;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_handle *handle;
	uint32_t handle_idx;

	if (!flow)
		return;
	SILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW], flow->dev_handles,
		       handle_idx, handle, next) {
		if (handle->drv_flow) {
			claim_zero(mlx5_glue->destroy_flow(handle->drv_flow));
			handle->drv_flow = NULL;
		}
		/* hrxq is union, don't touch it only the flag is set. */
		if (handle->rix_hrxq &&
		    handle->fate_action == MLX5_FLOW_FATE_QUEUE) {
			mlx5_hrxq_release(dev, handle->rix_hrxq);
			handle->rix_hrxq = 0;
		}
		if (handle->vf_vlan.tag && handle->vf_vlan.created)
			mlx5_vlan_vmwa_release(dev, &handle->vf_vlan);
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_handle *handle;

	if (!flow)
		return;
	flow_verbs_remove(dev, flow);
	while (flow->dev_handles) {
		uint32_t tmp_idx = flow->dev_handles;

		handle = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
				   tmp_idx);
		if (!handle)
			return;
		flow->dev_handles = handle->next.next;
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
			   tmp_idx);
	}
	if (flow->counter) {
		flow_verbs_counter_release(dev, flow->counter);
		flow->counter = 0;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_handle *handle;
	struct mlx5_flow *dev_flow;
	struct mlx5_hrxq *hrxq;
	uint32_t dev_handles;
	int err;
	int idx;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();

	MLX5_ASSERT(wks);
	for (idx = wks->flow_idx - 1; idx >= 0; idx--) {
		dev_flow = &wks->flows[idx];
		handle = dev_flow->handle;
		if (handle->fate_action == MLX5_FLOW_FATE_DROP) {
			MLX5_ASSERT(priv->drop_queue.hrxq);
			hrxq = priv->drop_queue.hrxq;
		} else {
			struct mlx5_flow_rss_desc *rss_desc = &wks->rss_desc;

			MLX5_ASSERT(rss_desc->queue_num);
			rss_desc->key_len = MLX5_RSS_HASH_KEY_LEN;
			rss_desc->hash_fields = dev_flow->hash_fields;
			rss_desc->tunnel = !!(handle->layers &
					      MLX5_FLOW_LAYER_TUNNEL);
			rss_desc->shared_rss = 0;
			hrxq = mlx5_hrxq_get(dev, rss_desc);
			if (!hrxq) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get hash queue");
				goto error;
			}
			handle->rix_hrxq = hrxq->idx;
		}
		MLX5_ASSERT(hrxq);
		handle->drv_flow = mlx5_glue->create_flow
					(hrxq->qp, &dev_flow->verbs.attr);
		if (!handle->drv_flow) {
			rte_flow_error_set(error, errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "hardware refuses to create flow");
			goto error;
		}
		if (priv->vmwa_context &&
		    handle->vf_vlan.tag && !handle->vf_vlan.created) {
			/*
			 * The rule contains the VLAN pattern.
			 * For VF we are going to create VLAN
			 * interface to make hypervisor set correct
			 * e-Switch vport context.
			 */
			mlx5_vlan_vmwa_acquire(dev, &handle->vf_vlan);
		}
	}
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	SILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW], flow->dev_handles,
		       dev_handles, handle, next) {
		/* hrxq is union, don't touch it only the flag is set. */
		if (handle->rix_hrxq &&
		    handle->fate_action == MLX5_FLOW_FATE_QUEUE) {
			mlx5_hrxq_release(dev, handle->rix_hrxq);
			handle->rix_hrxq = 0;
		}
		if (handle->vf_vlan.tag && handle->vf_vlan.created)
			mlx5_vlan_vmwa_release(dev, &handle->vf_vlan);
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

static int
flow_verbs_sync_domain(struct rte_eth_dev *dev, uint32_t domains,
		       uint32_t flags)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(domains);
	RTE_SET_USED(flags);

	return 0;
}

const struct mlx5_flow_driver_ops mlx5_flow_verbs_drv_ops = {
	.list_create = flow_legacy_list_create,
	.list_destroy = flow_legacy_list_destroy,
	.validate = flow_verbs_validate,
	.prepare = flow_verbs_prepare,
	.translate = flow_verbs_translate,
	.apply = flow_verbs_apply,
	.remove = flow_verbs_remove,
	.destroy = flow_verbs_destroy,
	.query = flow_verbs_query,
	.sync_domain = flow_verbs_sync_domain,
	.discover_priorities = flow_verbs_discover_priorities,
	.get_aged_flows = flow_null_get_aged_flows,
	.counter_alloc = flow_null_counter_allocate,
	.counter_free = flow_null_counter_free,
	.counter_query = flow_null_counter_query,
};
