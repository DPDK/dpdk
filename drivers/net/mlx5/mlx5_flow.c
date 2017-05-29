/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *   Copyright 2016 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>
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

#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_prm.h"

/* Number of Work Queue necessary for the DROP queue. */
#define MLX5_DROP_WQ_N 4

static int
mlx5_flow_create_eth(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data);

static int
mlx5_flow_create_vlan(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data);

static int
mlx5_flow_create_ipv4(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data);

static int
mlx5_flow_create_ipv6(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data);

static int
mlx5_flow_create_udp(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data);

static int
mlx5_flow_create_tcp(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data);

static int
mlx5_flow_create_vxlan(const struct rte_flow_item *item,
		       const void *default_mask,
		       void *data);

struct rte_flow {
	TAILQ_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	struct ibv_exp_flow_attr *ibv_attr; /**< Pointer to Verbs attributes. */
	struct ibv_exp_rwq_ind_table *ind_table; /**< Indirection table. */
	struct ibv_qp *qp; /**< Verbs queue pair. */
	struct ibv_exp_flow *ibv_flow; /**< Verbs flow. */
	struct ibv_exp_wq *wq; /**< Verbs work queue. */
	struct ibv_cq *cq; /**< Verbs completion queue. */
	uint16_t rxqs_n; /**< Number of queues in this flow, 0 if drop queue. */
	uint32_t mark:1; /**< Set if the flow is marked. */
	uint32_t drop:1; /**< Drop queue. */
	uint64_t hash_fields; /**< Fields that participate in the hash. */
	struct rxq *rxqs[]; /**< Pointer to the queues array. */
};

/** Static initializer for items. */
#define ITEMS(...) \
	(const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	}

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
	 *   0 on success, negative value otherwise.
	 */
	int (*convert)(const struct rte_flow_item *item,
		       const void *default_mask,
		       void *data);
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
	RTE_FLOW_ACTION_TYPE_END,
};

/** Graph of supported items and associated actions. */
static const struct mlx5_flow_items mlx5_flow_items[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VXLAN),
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
		.dst_sz = sizeof(struct ibv_exp_flow_spec_eth),
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_vlan){
			.tci = -1,
		},
		.default_mask = &rte_flow_item_vlan_mask,
		.mask_sz = sizeof(struct rte_flow_item_vlan),
		.convert = mlx5_flow_create_vlan,
		.dst_sz = 0,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_TCP),
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
		.dst_sz = sizeof(struct ibv_exp_flow_spec_ipv4_ext),
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_TCP),
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
		.dst_sz = sizeof(struct ibv_exp_flow_spec_ipv6_ext),
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_VXLAN),
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
		.dst_sz = sizeof(struct ibv_exp_flow_spec_tcp_udp),
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
		.dst_sz = sizeof(struct ibv_exp_flow_spec_tcp_udp),
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_vxlan){
			.vni = "\xff\xff\xff",
		},
		.default_mask = &rte_flow_item_vxlan_mask,
		.mask_sz = sizeof(struct rte_flow_item_vxlan),
		.convert = mlx5_flow_create_vxlan,
		.dst_sz = sizeof(struct ibv_exp_flow_spec_tunnel),
	},
};

/** Structure to pass to the conversion function. */
struct mlx5_flow {
	struct ibv_exp_flow_attr *ibv_attr; /**< Verbs attribute. */
	unsigned int offset; /**< Offset in bytes in the ibv_attr buffer. */
	uint32_t inner; /**< Set once VXLAN is encountered. */
	uint64_t hash_fields; /**< Fields that participate in the hash. */
};

/** Structure for Drop queue. */
struct rte_flow_drop {
	struct ibv_exp_rwq_ind_table *ind_table; /**< Indirection table. */
	struct ibv_qp *qp; /**< Verbs queue pair. */
	struct ibv_exp_wq *wqs[MLX5_DROP_WQ_N]; /**< Verbs work queue. */
	struct ibv_cq *cq; /**< Verbs completion queue. */
};

struct mlx5_flow_action {
	uint32_t queue:1; /**< Target is a receive queue. */
	uint32_t drop:1; /**< Target is a drop queue. */
	uint32_t mark:1; /**< Mark is present in the flow. */
	uint32_t mark_id; /**< Mark identifier. */
	uint16_t queues[RTE_MAX_QUEUES_PER_PORT]; /**< Queues indexes to use. */
	uint16_t queues_n; /**< Number of entries in queue[]. */
};

/**
 * Check support for a given item.
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
 *   0 on success.
 */
static int
mlx5_flow_item_validate(const struct rte_flow_item *item,
			const uint8_t *mask, unsigned int size)
{
	int ret = 0;

	if (!item->spec && (item->mask || item->last))
		return -1;
	if (item->spec && !item->mask) {
		unsigned int i;
		const uint8_t *spec = item->spec;

		for (i = 0; i < size; ++i)
			if ((spec[i] | mask[i]) != mask[i])
				return -1;
	}
	if (item->last && !item->mask) {
		unsigned int i;
		const uint8_t *spec = item->last;

		for (i = 0; i < size; ++i)
			if ((spec[i] | mask[i]) != mask[i])
				return -1;
	}
	if (item->mask) {
		unsigned int i;
		const uint8_t *spec = item->mask;

		for (i = 0; i < size; ++i)
			if ((spec[i] | mask[i]) != mask[i])
				return -1;
	}
	if (item->spec && item->last) {
		uint8_t spec[size];
		uint8_t last[size];
		const uint8_t *apply = mask;
		unsigned int i;

		if (item->mask)
			apply = item->mask;
		for (i = 0; i < size; ++i) {
			spec[i] = ((const uint8_t *)item->spec)[i] & apply[i];
			last[i] = ((const uint8_t *)item->last)[i] & apply[i];
		}
		ret = memcmp(spec, last, size);
	}
	return ret;
}

/**
 * Validate a flow supported by the NIC.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @param[in, out] flow
 *   Flow structure to update.
 * @param[in, out] action
 *   Action structure to update.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
priv_flow_validate(struct priv *priv,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error,
		   struct mlx5_flow *flow,
		   struct mlx5_flow_action *action)
{
	const struct mlx5_flow_items *cur_item = mlx5_flow_items;

	(void)priv;
	if (attr->group) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				   NULL,
				   "groups are not supported");
		return -rte_errno;
	}
	if (attr->priority) {
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
	if (!attr->ingress) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   NULL,
				   "only ingress is supported");
		return -rte_errno;
	}
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; ++items) {
		const struct mlx5_flow_items *token = NULL;
		unsigned int i;
		int err;

		if (items->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;
		for (i = 0;
		     cur_item->items &&
		     cur_item->items[i] != RTE_FLOW_ITEM_TYPE_END;
		     ++i) {
			if (cur_item->items[i] == items->type) {
				token = &mlx5_flow_items[items->type];
				break;
			}
		}
		if (!token)
			goto exit_item_not_supported;
		cur_item = token;
		err = mlx5_flow_item_validate(items,
					      (const uint8_t *)cur_item->mask,
					      cur_item->mask_sz);
		if (err)
			goto exit_item_not_supported;
		if (flow->ibv_attr && cur_item->convert) {
			err = cur_item->convert(items,
						(cur_item->default_mask ?
						 cur_item->default_mask :
						 cur_item->mask),
						flow);
			if (err)
				goto exit_item_not_supported;
		} else if (items->type == RTE_FLOW_ITEM_TYPE_VXLAN) {
			if (flow->inner) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   items,
						   "cannot recognize multiple"
						   " VXLAN encapsulations");
				return -rte_errno;
			}
			flow->inner = 1;
		}
		flow->offset += cur_item->dst_sz;
	}
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID) {
			continue;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_DROP) {
			action->drop = 1;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *queue =
				(const struct rte_flow_action_queue *)
				actions->conf;
			uint16_t n;
			uint16_t found = 0;

			if (!queue || (queue->index > (priv->rxqs_n - 1)))
				goto exit_action_not_supported;
			for (n = 0; n < action->queues_n; ++n) {
				if (action->queues[n] == queue->index) {
					found = 1;
					break;
				}
			}
			if (action->queues_n > 1 && !found) {
				rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions,
					   "queue action not in RSS queues");
				return -rte_errno;
			}
			if (!found) {
				action->queue = 1;
				action->queues_n = 1;
				action->queues[0] = queue->index;
			}
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_RSS) {
			const struct rte_flow_action_rss *rss =
				(const struct rte_flow_action_rss *)
				actions->conf;
			uint16_t n;

			if (!rss || !rss->num) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "no valid queues");
				return -rte_errno;
			}
			if (action->queues_n == 1) {
				uint16_t found = 0;

				assert(action->queues_n);
				for (n = 0; n < rss->num; ++n) {
					if (action->queues[0] ==
					    rss->queue[n]) {
						found = 1;
						break;
					}
				}
				if (!found) {
					rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "queue action not in RSS"
						   " queues");
					return -rte_errno;
				}
			}
			for (n = 0; n < rss->num; ++n) {
				if (rss->queue[n] >= priv->rxqs_n) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "queue id > number of"
						   " queues");
					return -rte_errno;
				}
			}
			action->queue = 1;
			for (n = 0; n < rss->num; ++n)
				action->queues[n] = rss->queue[n];
			action->queues_n = rss->num;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_MARK) {
			const struct rte_flow_action_mark *mark =
				(const struct rte_flow_action_mark *)
				actions->conf;

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
			action->mark = 1;
			action->mark_id = mark->id;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_FLAG) {
			action->mark = 1;
		} else {
			goto exit_action_not_supported;
		}
	}
	if (action->mark && !flow->ibv_attr && !action->drop)
		flow->offset += sizeof(struct ibv_exp_flow_spec_action_tag);
	if (!action->queue && !action->drop) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "no valid action");
		return -rte_errno;
	}
	return 0;
exit_item_not_supported:
	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
			   items, "item not supported");
	return -rte_errno;
exit_action_not_supported:
	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
			   actions, "action not supported");
	return -rte_errno;
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
	struct priv *priv = dev->data->dev_private;
	int ret;
	struct mlx5_flow flow = { .offset = sizeof(struct ibv_exp_flow_attr) };
	struct mlx5_flow_action action = {
		.queue = 0,
		.drop = 0,
		.mark = 0,
		.mark_id = MLX5_FLOW_MARK_DEFAULT,
		.queues_n = 0,
	};

	priv_lock(priv);
	ret = priv_flow_validate(priv, attr, items, actions, error, &flow,
				 &action);
	priv_unlock(priv);
	return ret;
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
 */
static int
mlx5_flow_create_eth(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct mlx5_flow *flow = (struct mlx5_flow *)data;
	struct ibv_exp_flow_spec_eth *eth;
	const unsigned int eth_size = sizeof(struct ibv_exp_flow_spec_eth);
	unsigned int i;

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 2;
	flow->hash_fields = 0;
	eth = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*eth = (struct ibv_exp_flow_spec_eth) {
		.type = flow->inner | IBV_EXP_FLOW_SPEC_ETH,
		.size = eth_size,
	};
	if (!spec)
		return 0;
	if (!mask)
		mask = default_mask;
	memcpy(eth->val.dst_mac, spec->dst.addr_bytes, ETHER_ADDR_LEN);
	memcpy(eth->val.src_mac, spec->src.addr_bytes, ETHER_ADDR_LEN);
	eth->val.ether_type = spec->type;
	memcpy(eth->mask.dst_mac, mask->dst.addr_bytes, ETHER_ADDR_LEN);
	memcpy(eth->mask.src_mac, mask->src.addr_bytes, ETHER_ADDR_LEN);
	eth->mask.ether_type = mask->type;
	/* Remove unwanted bits from values. */
	for (i = 0; i < ETHER_ADDR_LEN; ++i) {
		eth->val.dst_mac[i] &= eth->mask.dst_mac[i];
		eth->val.src_mac[i] &= eth->mask.src_mac[i];
	}
	eth->val.ether_type &= eth->mask.ether_type;
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
 */
static int
mlx5_flow_create_vlan(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	struct mlx5_flow *flow = (struct mlx5_flow *)data;
	struct ibv_exp_flow_spec_eth *eth;
	const unsigned int eth_size = sizeof(struct ibv_exp_flow_spec_eth);

	eth = (void *)((uintptr_t)flow->ibv_attr + flow->offset - eth_size);
	if (!spec)
		return 0;
	if (!mask)
		mask = default_mask;
	eth->val.vlan_tag = spec->tci;
	eth->mask.vlan_tag = mask->tci;
	eth->val.vlan_tag &= eth->mask.vlan_tag;
	return 0;
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
 */
static int
mlx5_flow_create_ipv4(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct mlx5_flow *flow = (struct mlx5_flow *)data;
	struct ibv_exp_flow_spec_ipv4_ext *ipv4;
	unsigned int ipv4_size = sizeof(struct ibv_exp_flow_spec_ipv4_ext);

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 1;
	flow->hash_fields = (IBV_EXP_RX_HASH_SRC_IPV4 |
			     IBV_EXP_RX_HASH_DST_IPV4);
	ipv4 = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*ipv4 = (struct ibv_exp_flow_spec_ipv4_ext) {
		.type = flow->inner | IBV_EXP_FLOW_SPEC_IPV4_EXT,
		.size = ipv4_size,
	};
	if (!spec)
		return 0;
	if (!mask)
		mask = default_mask;
	ipv4->val = (struct ibv_exp_flow_ipv4_ext_filter){
		.src_ip = spec->hdr.src_addr,
		.dst_ip = spec->hdr.dst_addr,
		.proto = spec->hdr.next_proto_id,
		.tos = spec->hdr.type_of_service,
	};
	ipv4->mask = (struct ibv_exp_flow_ipv4_ext_filter){
		.src_ip = mask->hdr.src_addr,
		.dst_ip = mask->hdr.dst_addr,
		.proto = mask->hdr.next_proto_id,
		.tos = mask->hdr.type_of_service,
	};
	/* Remove unwanted bits from values. */
	ipv4->val.src_ip &= ipv4->mask.src_ip;
	ipv4->val.dst_ip &= ipv4->mask.dst_ip;
	ipv4->val.proto &= ipv4->mask.proto;
	ipv4->val.tos &= ipv4->mask.tos;
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
 */
static int
mlx5_flow_create_ipv6(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	struct mlx5_flow *flow = (struct mlx5_flow *)data;
	struct ibv_exp_flow_spec_ipv6_ext *ipv6;
	unsigned int ipv6_size = sizeof(struct ibv_exp_flow_spec_ipv6_ext);
	unsigned int i;

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 1;
	flow->hash_fields = (IBV_EXP_RX_HASH_SRC_IPV6 |
			     IBV_EXP_RX_HASH_DST_IPV6);
	ipv6 = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*ipv6 = (struct ibv_exp_flow_spec_ipv6_ext) {
		.type = flow->inner | IBV_EXP_FLOW_SPEC_IPV6_EXT,
		.size = ipv6_size,
	};
	if (!spec)
		return 0;
	if (!mask)
		mask = default_mask;
	memcpy(ipv6->val.src_ip, spec->hdr.src_addr,
	       RTE_DIM(ipv6->val.src_ip));
	memcpy(ipv6->val.dst_ip, spec->hdr.dst_addr,
	       RTE_DIM(ipv6->val.dst_ip));
	memcpy(ipv6->mask.src_ip, mask->hdr.src_addr,
	       RTE_DIM(ipv6->mask.src_ip));
	memcpy(ipv6->mask.dst_ip, mask->hdr.dst_addr,
	       RTE_DIM(ipv6->mask.dst_ip));
	ipv6->mask.flow_label = mask->hdr.vtc_flow;
	ipv6->mask.next_hdr = mask->hdr.proto;
	ipv6->mask.hop_limit = mask->hdr.hop_limits;
	/* Remove unwanted bits from values. */
	for (i = 0; i < RTE_DIM(ipv6->val.src_ip); ++i) {
		ipv6->val.src_ip[i] &= ipv6->mask.src_ip[i];
		ipv6->val.dst_ip[i] &= ipv6->mask.dst_ip[i];
	}
	ipv6->val.flow_label &= ipv6->mask.flow_label;
	ipv6->val.next_hdr &= ipv6->mask.next_hdr;
	ipv6->val.hop_limit &= ipv6->mask.hop_limit;
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
 */
static int
mlx5_flow_create_udp(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct mlx5_flow *flow = (struct mlx5_flow *)data;
	struct ibv_exp_flow_spec_tcp_udp *udp;
	unsigned int udp_size = sizeof(struct ibv_exp_flow_spec_tcp_udp);

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 0;
	flow->hash_fields |= (IBV_EXP_RX_HASH_SRC_PORT_UDP |
			      IBV_EXP_RX_HASH_DST_PORT_UDP);
	udp = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*udp = (struct ibv_exp_flow_spec_tcp_udp) {
		.type = flow->inner | IBV_EXP_FLOW_SPEC_UDP,
		.size = udp_size,
	};
	if (!spec)
		return 0;
	if (!mask)
		mask = default_mask;
	udp->val.dst_port = spec->hdr.dst_port;
	udp->val.src_port = spec->hdr.src_port;
	udp->mask.dst_port = mask->hdr.dst_port;
	udp->mask.src_port = mask->hdr.src_port;
	/* Remove unwanted bits from values. */
	udp->val.src_port &= udp->mask.src_port;
	udp->val.dst_port &= udp->mask.dst_port;
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
 */
static int
mlx5_flow_create_tcp(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct mlx5_flow *flow = (struct mlx5_flow *)data;
	struct ibv_exp_flow_spec_tcp_udp *tcp;
	unsigned int tcp_size = sizeof(struct ibv_exp_flow_spec_tcp_udp);

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 0;
	flow->hash_fields |= (IBV_EXP_RX_HASH_SRC_PORT_TCP |
			      IBV_EXP_RX_HASH_DST_PORT_TCP);
	tcp = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*tcp = (struct ibv_exp_flow_spec_tcp_udp) {
		.type = flow->inner | IBV_EXP_FLOW_SPEC_TCP,
		.size = tcp_size,
	};
	if (!spec)
		return 0;
	if (!mask)
		mask = default_mask;
	tcp->val.dst_port = spec->hdr.dst_port;
	tcp->val.src_port = spec->hdr.src_port;
	tcp->mask.dst_port = mask->hdr.dst_port;
	tcp->mask.src_port = mask->hdr.src_port;
	/* Remove unwanted bits from values. */
	tcp->val.src_port &= tcp->mask.src_port;
	tcp->val.dst_port &= tcp->mask.dst_port;
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
 */
static int
mlx5_flow_create_vxlan(const struct rte_flow_item *item,
		       const void *default_mask,
		       void *data)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	struct mlx5_flow *flow = (struct mlx5_flow *)data;
	struct ibv_exp_flow_spec_tunnel *vxlan;
	unsigned int size = sizeof(struct ibv_exp_flow_spec_tunnel);
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id;

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 0;
	id.vni[0] = 0;
	vxlan = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*vxlan = (struct ibv_exp_flow_spec_tunnel) {
		.type = flow->inner | IBV_EXP_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
	flow->inner = IBV_EXP_FLOW_SPEC_INNER;
	if (!spec)
		return 0;
	if (!mask)
		mask = default_mask;
	memcpy(&id.vni[1], spec->vni, 3);
	vxlan->val.tunnel_id = id.vlan_id;
	memcpy(&id.vni[1], mask->vni, 3);
	vxlan->mask.tunnel_id = id.vlan_id;
	/* Remove unwanted bits from values. */
	vxlan->val.tunnel_id &= vxlan->mask.tunnel_id;
	return 0;
}

/**
 * Convert mark/flag action to Verbs specification.
 *
 * @param flow
 *   Pointer to MLX5 flow structure.
 * @param mark_id
 *   Mark identifier.
 */
static int
mlx5_flow_create_flag_mark(struct mlx5_flow *flow, uint32_t mark_id)
{
	struct ibv_exp_flow_spec_action_tag *tag;
	unsigned int size = sizeof(struct ibv_exp_flow_spec_action_tag);

	tag = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*tag = (struct ibv_exp_flow_spec_action_tag){
		.type = IBV_EXP_FLOW_SPEC_ACTION_TAG,
		.size = size,
		.tag_id = mlx5_flow_mark_set(mark_id),
	};
	++flow->ibv_attr->num_of_specs;
	return 0;
}

/**
 * Complete flow rule creation with a drop queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param flow
 *   MLX5 flow attributes (filled by mlx5_flow_validate()).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A flow if the rule could be created.
 */
static struct rte_flow *
priv_flow_create_action_queue_drop(struct priv *priv,
				   struct mlx5_flow *flow,
				   struct rte_flow_error *error)
{
	struct rte_flow *rte_flow;

	assert(priv->pd);
	assert(priv->ctx);
	rte_flow = rte_calloc(__func__, 1, sizeof(*rte_flow), 0);
	if (!rte_flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate flow memory");
		return NULL;
	}
	rte_flow->drop = 1;
	rte_flow->ibv_attr = flow->ibv_attr;
	rte_flow->qp = priv->flow_drop_queue->qp;
	if (!priv->started)
		return rte_flow;
	rte_flow->ibv_flow = ibv_exp_create_flow(rte_flow->qp,
						 rte_flow->ibv_attr);
	if (!rte_flow->ibv_flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "flow rule creation failure");
		goto error;
	}
	return rte_flow;
error:
	assert(rte_flow);
	rte_free(rte_flow);
	return NULL;
}

/**
 * Complete flow rule creation.
 *
 * @param priv
 *   Pointer to private structure.
 * @param flow
 *   MLX5 flow attributes (filled by mlx5_flow_validate()).
 * @param action
 *   Target action structure.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A flow if the rule could be created.
 */
static struct rte_flow *
priv_flow_create_action_queue(struct priv *priv,
			      struct mlx5_flow *flow,
			      struct mlx5_flow_action *action,
			      struct rte_flow_error *error)
{
	struct rte_flow *rte_flow;
	unsigned int i;
	unsigned int j;
	const unsigned int wqs_n = 1 << log2above(action->queues_n);
	struct ibv_exp_wq *wqs[wqs_n];

	assert(priv->pd);
	assert(priv->ctx);
	assert(!action->drop);
	rte_flow = rte_calloc(__func__, 1, sizeof(*rte_flow) +
			      sizeof(*rte_flow->rxqs) * action->queues_n, 0);
	if (!rte_flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate flow memory");
		return NULL;
	}
	for (i = 0; i < action->queues_n; ++i) {
		struct rxq_ctrl *rxq;

		rxq = container_of((*priv->rxqs)[action->queues[i]],
				   struct rxq_ctrl, rxq);
		wqs[i] = rxq->wq;
		rte_flow->rxqs[i] = &rxq->rxq;
		++rte_flow->rxqs_n;
		rxq->rxq.mark |= action->mark;
	}
	/* finalise indirection table. */
	for (j = 0; i < wqs_n; ++i, ++j) {
		wqs[i] = wqs[j];
		if (j == action->queues_n)
			j = 0;
	}
	rte_flow->mark = action->mark;
	rte_flow->ibv_attr = flow->ibv_attr;
	rte_flow->hash_fields = flow->hash_fields;
	rte_flow->ind_table = ibv_exp_create_rwq_ind_table(
		priv->ctx,
		&(struct ibv_exp_rwq_ind_table_init_attr){
			.pd = priv->pd,
			.log_ind_tbl_size = log2above(action->queues_n),
			.ind_tbl = wqs,
			.comp_mask = 0,
		});
	if (!rte_flow->ind_table) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate indirection table");
		goto error;
	}
	rte_flow->qp = ibv_exp_create_qp(
		priv->ctx,
		&(struct ibv_exp_qp_init_attr){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask =
				IBV_EXP_QP_INIT_ATTR_PD |
				IBV_EXP_QP_INIT_ATTR_PORT |
				IBV_EXP_QP_INIT_ATTR_RX_HASH,
			.pd = priv->pd,
			.rx_hash_conf = &(struct ibv_exp_rx_hash_conf){
				.rx_hash_function =
					IBV_EXP_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = rss_hash_default_key_len,
				.rx_hash_key = rss_hash_default_key,
				.rx_hash_fields_mask = rte_flow->hash_fields,
				.rwq_ind_tbl = rte_flow->ind_table,
			},
			.port_num = priv->port,
		});
	if (!rte_flow->qp) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate QP");
		goto error;
	}
	if (!priv->started)
		return rte_flow;
	rte_flow->ibv_flow = ibv_exp_create_flow(rte_flow->qp,
						 rte_flow->ibv_attr);
	if (!rte_flow->ibv_flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "flow rule creation failure");
		goto error;
	}
	return rte_flow;
error:
	assert(rte_flow);
	if (rte_flow->qp)
		ibv_destroy_qp(rte_flow->qp);
	if (rte_flow->ind_table)
		ibv_exp_destroy_rwq_ind_table(rte_flow->ind_table);
	rte_free(rte_flow);
	return NULL;
}

/**
 * Convert a flow.
 *
 * @param priv
 *   Pointer to private structure.
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
 *   A flow on success, NULL otherwise.
 */
static struct rte_flow *
priv_flow_create(struct priv *priv,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct rte_flow *rte_flow;
	struct mlx5_flow flow = { .offset = sizeof(struct ibv_exp_flow_attr), };
	struct mlx5_flow_action action = {
		.queue = 0,
		.drop = 0,
		.mark = 0,
		.mark_id = MLX5_FLOW_MARK_DEFAULT,
		.queues_n = 0,
	};
	int err;

	err = priv_flow_validate(priv, attr, items, actions, error, &flow,
				 &action);
	if (err)
		goto exit;
	flow.ibv_attr = rte_malloc(__func__, flow.offset, 0);
	flow.offset = sizeof(struct ibv_exp_flow_attr);
	if (!flow.ibv_attr) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate ibv_attr memory");
		goto exit;
	}
	*flow.ibv_attr = (struct ibv_exp_flow_attr){
		.type = IBV_EXP_FLOW_ATTR_NORMAL,
		.size = sizeof(struct ibv_exp_flow_attr),
		.priority = attr->priority,
		.num_of_specs = 0,
		.port = 0,
		.flags = 0,
		.reserved = 0,
	};
	flow.inner = 0;
	flow.hash_fields = 0;
	claim_zero(priv_flow_validate(priv, attr, items, actions,
				      error, &flow, &action));
	if (action.mark && !action.drop) {
		mlx5_flow_create_flag_mark(&flow, action.mark_id);
		flow.offset += sizeof(struct ibv_exp_flow_spec_action_tag);
	}
	if (action.drop)
		rte_flow =
			priv_flow_create_action_queue_drop(priv, &flow, error);
	else
		rte_flow = priv_flow_create_action_queue(priv, &flow, &action,
							 error);
	if (!rte_flow)
		goto exit;
	return rte_flow;
exit:
	rte_free(flow.ibv_attr);
	return NULL;
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
	struct rte_flow *flow;

	priv_lock(priv);
	flow = priv_flow_create(priv, attr, items, actions, error);
	if (flow) {
		TAILQ_INSERT_TAIL(&priv->flows, flow, next);
		DEBUG("Flow created %p", (void *)flow);
	}
	priv_unlock(priv);
	return flow;
}

/**
 * Destroy a flow.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] flow
 *   Flow to destroy.
 */
static void
priv_flow_destroy(struct priv *priv,
		  struct rte_flow *flow)
{
	TAILQ_REMOVE(&priv->flows, flow, next);
	if (flow->ibv_flow)
		claim_zero(ibv_exp_destroy_flow(flow->ibv_flow));
	if (flow->drop)
		goto free;
	if (flow->qp)
		claim_zero(ibv_destroy_qp(flow->qp));
	if (flow->ind_table)
		claim_zero(ibv_exp_destroy_rwq_ind_table(flow->ind_table));
	if (flow->drop && flow->wq)
		claim_zero(ibv_exp_destroy_wq(flow->wq));
	if (flow->drop && flow->cq)
		claim_zero(ibv_destroy_cq(flow->cq));
	if (flow->mark) {
		struct rte_flow *tmp;
		struct rxq *rxq;
		uint32_t mark_n = 0;
		uint32_t queue_n;

		/*
		 * To remove the mark from the queue, the queue must not be
		 * present in any other marked flow (RSS or not).
		 */
		for (queue_n = 0; queue_n < flow->rxqs_n; ++queue_n) {
			rxq = flow->rxqs[queue_n];
			for (tmp = TAILQ_FIRST(&priv->flows);
			     tmp;
			     tmp = TAILQ_NEXT(tmp, next)) {
				uint32_t tqueue_n;

				if (tmp->drop)
					continue;
				for (tqueue_n = 0;
				     tqueue_n < tmp->rxqs_n;
				     ++tqueue_n) {
					struct rxq *trxq;

					trxq = tmp->rxqs[tqueue_n];
					if (rxq == trxq)
						++mark_n;
				}
			}
			rxq->mark = !!mark_n;
		}
	}
free:
	rte_free(flow->ibv_attr);
	DEBUG("Flow destroyed %p", (void *)flow);
	rte_free(flow);
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
		  struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;

	(void)error;
	priv_lock(priv);
	priv_flow_destroy(priv, flow);
	priv_unlock(priv);
	return 0;
}

/**
 * Destroy all flows.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
priv_flow_flush(struct priv *priv)
{
	while (!TAILQ_EMPTY(&priv->flows)) {
		struct rte_flow *flow;

		flow = TAILQ_FIRST(&priv->flows);
		priv_flow_destroy(priv, flow);
	}
}

/**
 * Destroy all flows.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
int
mlx5_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;

	(void)error;
	priv_lock(priv);
	priv_flow_flush(priv);
	priv_unlock(priv);
	return 0;
}

/**
 * Create drop queue.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success.
 */
static int
priv_flow_create_drop_queue(struct priv *priv)
{
	struct rte_flow_drop *fdq = NULL;
	unsigned int i;

	assert(priv->pd);
	assert(priv->ctx);
	fdq = rte_calloc(__func__, 1, sizeof(*fdq), 0);
	if (!fdq) {
		WARN("cannot allocate memory for drop queue");
		goto error;
	}
	fdq->cq = ibv_exp_create_cq(priv->ctx, 1, NULL, NULL, 0,
			&(struct ibv_exp_cq_init_attr){
			.comp_mask = 0,
			});
	if (!fdq->cq) {
		WARN("cannot allocate CQ for drop queue");
		goto error;
	}
	for (i = 0; i != MLX5_DROP_WQ_N; ++i) {
		fdq->wqs[i] = ibv_exp_create_wq(priv->ctx,
				&(struct ibv_exp_wq_init_attr){
				.wq_type = IBV_EXP_WQT_RQ,
				.max_recv_wr = 1,
				.max_recv_sge = 1,
				.pd = priv->pd,
				.cq = fdq->cq,
				});
		if (!fdq->wqs[i]) {
			WARN("cannot allocate WQ for drop queue");
			goto error;
		}
	}
	fdq->ind_table = ibv_exp_create_rwq_ind_table(priv->ctx,
			&(struct ibv_exp_rwq_ind_table_init_attr){
			.pd = priv->pd,
			.log_ind_tbl_size = 0,
			.ind_tbl = fdq->wqs,
			.comp_mask = 0,
			});
	if (!fdq->ind_table) {
		WARN("cannot allocate indirection table for drop queue");
		goto error;
	}
	fdq->qp = ibv_exp_create_qp(priv->ctx,
		&(struct ibv_exp_qp_init_attr){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask =
				IBV_EXP_QP_INIT_ATTR_PD |
				IBV_EXP_QP_INIT_ATTR_PORT |
				IBV_EXP_QP_INIT_ATTR_RX_HASH,
			.pd = priv->pd,
			.rx_hash_conf = &(struct ibv_exp_rx_hash_conf){
				.rx_hash_function =
					IBV_EXP_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = rss_hash_default_key_len,
				.rx_hash_key = rss_hash_default_key,
				.rx_hash_fields_mask = 0,
				.rwq_ind_tbl = fdq->ind_table,
				},
			.port_num = priv->port,
			});
	if (!fdq->qp) {
		WARN("cannot allocate QP for drop queue");
		goto error;
	}
	priv->flow_drop_queue = fdq;
	return 0;
error:
	if (fdq->qp)
		claim_zero(ibv_destroy_qp(fdq->qp));
	if (fdq->ind_table)
		claim_zero(ibv_exp_destroy_rwq_ind_table(fdq->ind_table));
	for (i = 0; i != MLX5_DROP_WQ_N; ++i) {
		if (fdq->wqs[i])
			claim_zero(ibv_exp_destroy_wq(fdq->wqs[i]));
	}
	if (fdq->cq)
		claim_zero(ibv_destroy_cq(fdq->cq));
	if (fdq)
		rte_free(fdq);
	priv->flow_drop_queue = NULL;
	return -1;
}

/**
 * Delete drop queue.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
priv_flow_delete_drop_queue(struct priv *priv)
{
	struct rte_flow_drop *fdq = priv->flow_drop_queue;
	unsigned int i;

	if (!fdq)
		return;
	if (fdq->qp)
		claim_zero(ibv_destroy_qp(fdq->qp));
	if (fdq->ind_table)
		claim_zero(ibv_exp_destroy_rwq_ind_table(fdq->ind_table));
	for (i = 0; i != MLX5_DROP_WQ_N; ++i) {
		if (fdq->wqs[i])
			claim_zero(ibv_exp_destroy_wq(fdq->wqs[i]));
	}
	if (fdq->cq)
		claim_zero(ibv_destroy_cq(fdq->cq));
	rte_free(fdq);
	priv->flow_drop_queue = NULL;
}

/**
 * Remove all flows.
 *
 * Called by dev_stop() to remove all flows.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_flow_stop(struct priv *priv)
{
	struct rte_flow *flow;

	TAILQ_FOREACH_REVERSE(flow, &priv->flows, mlx5_flows, next) {
		claim_zero(ibv_exp_destroy_flow(flow->ibv_flow));
		flow->ibv_flow = NULL;
		if (flow->mark) {
			unsigned int n;

			for (n = 0; n < flow->rxqs_n; ++n)
				flow->rxqs[n]->mark = 0;
		}
		DEBUG("Flow %p removed", (void *)flow);
	}
	priv_flow_delete_drop_queue(priv);
}

/**
 * Add all flows.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, a errno value otherwise and rte_errno is set.
 */
int
priv_flow_start(struct priv *priv)
{
	int ret;
	struct rte_flow *flow;

	ret = priv_flow_create_drop_queue(priv);
	if (ret)
		return -1;
	TAILQ_FOREACH(flow, &priv->flows, next) {
		struct ibv_qp *qp;

		if (flow->drop)
			qp = priv->flow_drop_queue->qp;
		else
			qp = flow->qp;
		flow->ibv_flow = ibv_exp_create_flow(qp, flow->ibv_attr);
		if (!flow->ibv_flow) {
			DEBUG("Flow %p cannot be applied", (void *)flow);
			rte_errno = EINVAL;
			return rte_errno;
		}
		DEBUG("Flow %p applied", (void *)flow);
		if (flow->mark) {
			unsigned int n;

			for (n = 0; n < flow->rxqs_n; ++n)
				flow->rxqs[n]->mark = 1;
		}
	}
	return 0;
}

/**
 * Verify if the Rx queue is used in a flow.
 *
 * @param priv
 *   Pointer to private structure.
 * @param rxq
 *   Pointer to the queue to search.
 *
 * @return
 *   Nonzero if the queue is used by a flow.
 */
int
priv_flow_rxq_in_use(struct priv *priv, struct rxq *rxq)
{
	struct rte_flow *flow;

	for (flow = TAILQ_FIRST(&priv->flows);
	     flow;
	     flow = TAILQ_NEXT(flow, next)) {
		unsigned int n;

		if (flow->drop)
			continue;
		for (n = 0; n < flow->rxqs_n; ++n) {
			if (flow->rxqs[n] == rxq)
				return 1;
		}
	}
	return 0;
}
