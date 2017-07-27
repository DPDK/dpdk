/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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

#include <assert.h>

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>

/* Generated configuration header. */
#include "mlx4_autoconf.h"

/* PMD headers. */
#include "mlx4.h"
#include "mlx4_flow.h"

/** Static initializer for items. */
#define ITEMS(...) \
	(const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	}

/** Structure to generate a simple graph of layers supported by the NIC. */
struct mlx4_flow_items {
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
	 * Check support for a given item.
	 *
	 * @param item[in]
	 *   Item specification.
	 * @param mask[in]
	 *   Bit-masks covering supported fields to compare with spec,
	 *   last and mask in
	 *   \item.
	 * @param size
	 *   Bit-Mask size in bytes.
	 *
	 * @return
	 *   0 on success, negative value otherwise.
	 */
	int (*validate)(const struct rte_flow_item *item,
			const uint8_t *mask, unsigned int size);
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

struct rte_flow_drop {
	struct ibv_qp *qp; /**< Verbs queue pair. */
	struct ibv_cq *cq; /**< Verbs completion queue. */
};

/** Valid action for this PMD. */
static const enum rte_flow_action_type valid_actions[] = {
	RTE_FLOW_ACTION_TYPE_DROP,
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_RSS,
	RTE_FLOW_ACTION_TYPE_END,
};

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
mlx4_flow_create_eth(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct mlx4_flow *flow = (struct mlx4_flow *)data;
	struct ibv_flow_spec_eth *eth;
	const unsigned int eth_size = sizeof(struct ibv_flow_spec_eth);
	unsigned int i;

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 2;
	eth = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*eth = (struct ibv_flow_spec_eth) {
		.type = IBV_FLOW_SPEC_ETH,
		.size = eth_size,
	};
	if (!spec) {
		flow->ibv_attr->type = IBV_FLOW_ATTR_ALL_DEFAULT;
		return 0;
	}
	if (!mask)
		mask = default_mask;
	memcpy(eth->val.dst_mac, spec->dst.addr_bytes, ETHER_ADDR_LEN);
	memcpy(eth->val.src_mac, spec->src.addr_bytes, ETHER_ADDR_LEN);
	memcpy(eth->mask.dst_mac, mask->dst.addr_bytes, ETHER_ADDR_LEN);
	memcpy(eth->mask.src_mac, mask->src.addr_bytes, ETHER_ADDR_LEN);
	/* Remove unwanted bits from values. */
	for (i = 0; i < ETHER_ADDR_LEN; ++i) {
		eth->val.dst_mac[i] &= eth->mask.dst_mac[i];
		eth->val.src_mac[i] &= eth->mask.src_mac[i];
	}
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
mlx4_flow_create_vlan(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	struct mlx4_flow *flow = (struct mlx4_flow *)data;
	struct ibv_flow_spec_eth *eth;
	const unsigned int eth_size = sizeof(struct ibv_flow_spec_eth);

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
mlx4_flow_create_ipv4(const struct rte_flow_item *item,
		      const void *default_mask,
		      void *data)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct mlx4_flow *flow = (struct mlx4_flow *)data;
	struct ibv_flow_spec_ipv4 *ipv4;
	unsigned int ipv4_size = sizeof(struct ibv_flow_spec_ipv4);

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 1;
	ipv4 = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*ipv4 = (struct ibv_flow_spec_ipv4) {
		.type = IBV_FLOW_SPEC_IPV4,
		.size = ipv4_size,
	};
	if (!spec)
		return 0;
	ipv4->val = (struct ibv_flow_ipv4_filter) {
		.src_ip = spec->hdr.src_addr,
		.dst_ip = spec->hdr.dst_addr,
	};
	if (!mask)
		mask = default_mask;
	ipv4->mask = (struct ibv_flow_ipv4_filter) {
		.src_ip = mask->hdr.src_addr,
		.dst_ip = mask->hdr.dst_addr,
	};
	/* Remove unwanted bits from values. */
	ipv4->val.src_ip &= ipv4->mask.src_ip;
	ipv4->val.dst_ip &= ipv4->mask.dst_ip;
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
mlx4_flow_create_udp(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct mlx4_flow *flow = (struct mlx4_flow *)data;
	struct ibv_flow_spec_tcp_udp *udp;
	unsigned int udp_size = sizeof(struct ibv_flow_spec_tcp_udp);

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 0;
	udp = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*udp = (struct ibv_flow_spec_tcp_udp) {
		.type = IBV_FLOW_SPEC_UDP,
		.size = udp_size,
	};
	if (!spec)
		return 0;
	udp->val.dst_port = spec->hdr.dst_port;
	udp->val.src_port = spec->hdr.src_port;
	if (!mask)
		mask = default_mask;
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
mlx4_flow_create_tcp(const struct rte_flow_item *item,
		     const void *default_mask,
		     void *data)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct mlx4_flow *flow = (struct mlx4_flow *)data;
	struct ibv_flow_spec_tcp_udp *tcp;
	unsigned int tcp_size = sizeof(struct ibv_flow_spec_tcp_udp);

	++flow->ibv_attr->num_of_specs;
	flow->ibv_attr->priority = 0;
	tcp = (void *)((uintptr_t)flow->ibv_attr + flow->offset);
	*tcp = (struct ibv_flow_spec_tcp_udp) {
		.type = IBV_FLOW_SPEC_TCP,
		.size = tcp_size,
	};
	if (!spec)
		return 0;
	tcp->val.dst_port = spec->hdr.dst_port;
	tcp->val.src_port = spec->hdr.src_port;
	if (!mask)
		mask = default_mask;
	tcp->mask.dst_port = mask->hdr.dst_port;
	tcp->mask.src_port = mask->hdr.src_port;
	/* Remove unwanted bits from values. */
	tcp->val.src_port &= tcp->mask.src_port;
	tcp->val.dst_port &= tcp->mask.dst_port;
	return 0;
}

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
 *   0 on success, negative value otherwise.
 */
static int
mlx4_flow_item_validate(const struct rte_flow_item *item,
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

static int
mlx4_flow_validate_eth(const struct rte_flow_item *item,
		       const uint8_t *mask, unsigned int size)
{
	if (item->mask) {
		const struct rte_flow_item_eth *mask = item->mask;

		if (mask->dst.addr_bytes[0] != 0xff ||
				mask->dst.addr_bytes[1] != 0xff ||
				mask->dst.addr_bytes[2] != 0xff ||
				mask->dst.addr_bytes[3] != 0xff ||
				mask->dst.addr_bytes[4] != 0xff ||
				mask->dst.addr_bytes[5] != 0xff)
			return -1;
	}
	return mlx4_flow_item_validate(item, mask, size);
}

static int
mlx4_flow_validate_vlan(const struct rte_flow_item *item,
			const uint8_t *mask, unsigned int size)
{
	if (item->mask) {
		const struct rte_flow_item_vlan *mask = item->mask;

		if (mask->tci != 0 &&
		    ntohs(mask->tci) != 0x0fff)
			return -1;
	}
	return mlx4_flow_item_validate(item, mask, size);
}

static int
mlx4_flow_validate_ipv4(const struct rte_flow_item *item,
			const uint8_t *mask, unsigned int size)
{
	if (item->mask) {
		const struct rte_flow_item_ipv4 *mask = item->mask;

		if (mask->hdr.src_addr != 0 &&
		    mask->hdr.src_addr != 0xffffffff)
			return -1;
		if (mask->hdr.dst_addr != 0 &&
		    mask->hdr.dst_addr != 0xffffffff)
			return -1;
	}
	return mlx4_flow_item_validate(item, mask, size);
}

static int
mlx4_flow_validate_udp(const struct rte_flow_item *item,
		       const uint8_t *mask, unsigned int size)
{
	if (item->mask) {
		const struct rte_flow_item_udp *mask = item->mask;

		if (mask->hdr.src_port != 0 &&
		    mask->hdr.src_port != 0xffff)
			return -1;
		if (mask->hdr.dst_port != 0 &&
		    mask->hdr.dst_port != 0xffff)
			return -1;
	}
	return mlx4_flow_item_validate(item, mask, size);
}

static int
mlx4_flow_validate_tcp(const struct rte_flow_item *item,
		       const uint8_t *mask, unsigned int size)
{
	if (item->mask) {
		const struct rte_flow_item_tcp *mask = item->mask;

		if (mask->hdr.src_port != 0 &&
		    mask->hdr.src_port != 0xffff)
			return -1;
		if (mask->hdr.dst_port != 0 &&
		    mask->hdr.dst_port != 0xffff)
			return -1;
	}
	return mlx4_flow_item_validate(item, mask, size);
}

/** Graph of supported items and associated actions. */
static const struct mlx4_flow_items mlx4_flow_items[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH),
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_IPV4),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_eth){
			.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		},
		.default_mask = &rte_flow_item_eth_mask,
		.mask_sz = sizeof(struct rte_flow_item_eth),
		.validate = mlx4_flow_validate_eth,
		.convert = mlx4_flow_create_eth,
		.dst_sz = sizeof(struct ibv_flow_spec_eth),
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_IPV4),
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_vlan){
		/* rte_flow_item_vlan_mask is invalid for mlx4. */
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			.tci = 0x0fff,
#else
			.tci = 0xff0f,
#endif
		},
		.mask_sz = sizeof(struct rte_flow_item_vlan),
		.validate = mlx4_flow_validate_vlan,
		.convert = mlx4_flow_create_vlan,
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
			},
		},
		.default_mask = &rte_flow_item_ipv4_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv4),
		.validate = mlx4_flow_validate_ipv4,
		.convert = mlx4_flow_create_ipv4,
		.dst_sz = sizeof(struct ibv_flow_spec_ipv4),
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.actions = valid_actions,
		.mask = &(const struct rte_flow_item_udp){
			.hdr = {
				.src_port = -1,
				.dst_port = -1,
			},
		},
		.default_mask = &rte_flow_item_udp_mask,
		.mask_sz = sizeof(struct rte_flow_item_udp),
		.validate = mlx4_flow_validate_udp,
		.convert = mlx4_flow_create_udp,
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
		.validate = mlx4_flow_validate_tcp,
		.convert = mlx4_flow_create_tcp,
		.dst_sz = sizeof(struct ibv_flow_spec_tcp_udp),
	},
};

/**
 * Validate a flow supported by the NIC.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @param[in, out] flow
 *   Flow structure to update.
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
		   struct mlx4_flow *flow)
{
	const struct mlx4_flow_items *cur_item = mlx4_flow_items;
	struct mlx4_flow_action action = {
		.queue = 0,
		.drop = 0,
	};

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
	/* Go over items list. */
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; ++items) {
		const struct mlx4_flow_items *token = NULL;
		unsigned int i;
		int err;

		if (items->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;
		/*
		 * The nic can support patterns with NULL eth spec only
		 * if eth is a single item in a rule.
		 */
		if (!items->spec &&
			items->type == RTE_FLOW_ITEM_TYPE_ETH) {
			const struct rte_flow_item *next = items + 1;

			if (next->type != RTE_FLOW_ITEM_TYPE_END) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   items,
						   "the rule requires"
						   " an Ethernet spec");
				return -rte_errno;
			}
		}
		for (i = 0;
		     cur_item->items &&
		     cur_item->items[i] != RTE_FLOW_ITEM_TYPE_END;
		     ++i) {
			if (cur_item->items[i] == items->type) {
				token = &mlx4_flow_items[items->type];
				break;
			}
		}
		if (!token)
			goto exit_item_not_supported;
		cur_item = token;
		err = cur_item->validate(items,
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
		}
		flow->offset += cur_item->dst_sz;
	}
	/* Go over actions list */
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID) {
			continue;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_DROP) {
			action.drop = 1;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *queue =
				(const struct rte_flow_action_queue *)
				actions->conf;

			if (!queue || (queue->index > (priv->rxqs_n - 1)))
				goto exit_action_not_supported;
			action.queue = 1;
			action.queues_n = 1;
			action.queues[0] = queue->index;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_RSS) {
			int i;
			int ierr;
			const struct rte_flow_action_rss *rss =
				(const struct rte_flow_action_rss *)
				actions->conf;

			if (!priv->hw_rss) {
				rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions,
					   "RSS cannot be used with "
					   "the current configuration");
				return -rte_errno;
			}
			if (!priv->isolated) {
				rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions,
					   "RSS cannot be used without "
					   "isolated mode");
				return -rte_errno;
			}
			if (!rte_is_power_of_2(rss->num)) {
				rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions,
					   "the number of queues "
					   "should be power of two");
				return -rte_errno;
			}
			if (priv->max_rss_tbl_sz < rss->num) {
				rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions,
					   "the number of queues "
					   "is too large");
				return -rte_errno;
			}
			/* checking indexes array */
			ierr = 0;
			for (i = 0; i < rss->num; ++i) {
				int j;
				if (rss->queue[i] >= priv->rxqs_n)
					ierr = 1;
				/*
				 * Prevent the user from specifying
				 * the same queue twice in the RSS array.
				 */
				for (j = i + 1; j < rss->num && !ierr; ++j)
					if (rss->queue[j] == rss->queue[i])
						ierr = 1;
				if (ierr) {
					rte_flow_error_set(
						error,
						ENOTSUP,
						RTE_FLOW_ERROR_TYPE_HANDLE,
						NULL,
						"RSS action only supports "
						"unique queue indices "
						"in a list");
					return -rte_errno;
				}
			}
			action.queue = 1;
			action.queues_n = rss->num;
			for (i = 0; i < rss->num; ++i)
				action.queues[i] = rss->queue[i];
		} else {
			goto exit_action_not_supported;
		}
	}
	if (!action.queue && !action.drop) {
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
mlx4_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	int ret;
	struct mlx4_flow flow = { .offset = sizeof(struct ibv_flow_attr) };

	priv_lock(priv);
	ret = priv_flow_validate(priv, attr, items, actions, error, &flow);
	priv_unlock(priv);
	return ret;
}

/**
 * Destroy a drop queue.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
mlx4_flow_destroy_drop_queue(struct priv *priv)
{
	if (priv->flow_drop_queue) {
		struct rte_flow_drop *fdq = priv->flow_drop_queue;

		priv->flow_drop_queue = NULL;
		claim_zero(ibv_destroy_qp(fdq->qp));
		claim_zero(ibv_destroy_cq(fdq->cq));
		rte_free(fdq);
	}
}

/**
 * Create a single drop queue for all drop flows.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative value otherwise.
 */
static int
mlx4_flow_create_drop_queue(struct priv *priv)
{
	struct ibv_qp *qp;
	struct ibv_cq *cq;
	struct rte_flow_drop *fdq;

	fdq = rte_calloc(__func__, 1, sizeof(*fdq), 0);
	if (!fdq) {
		ERROR("Cannot allocate memory for drop struct");
		goto err;
	}
	cq = ibv_exp_create_cq(priv->ctx, 1, NULL, NULL, 0,
			      &(struct ibv_exp_cq_init_attr){
					.comp_mask = 0,
			      });
	if (!cq) {
		ERROR("Cannot create drop CQ");
		goto err_create_cq;
	}
	qp = ibv_exp_create_qp(priv->ctx,
			      &(struct ibv_exp_qp_init_attr){
					.send_cq = cq,
					.recv_cq = cq,
					.cap = {
						.max_recv_wr = 1,
						.max_recv_sge = 1,
					},
					.qp_type = IBV_QPT_RAW_PACKET,
					.comp_mask =
						IBV_EXP_QP_INIT_ATTR_PD |
						IBV_EXP_QP_INIT_ATTR_PORT,
					.pd = priv->pd,
					.port_num = priv->port,
			      });
	if (!qp) {
		ERROR("Cannot create drop QP");
		goto err_create_qp;
	}
	*fdq = (struct rte_flow_drop){
		.qp = qp,
		.cq = cq,
	};
	priv->flow_drop_queue = fdq;
	return 0;
err_create_qp:
	claim_zero(ibv_destroy_cq(cq));
err_create_cq:
	rte_free(fdq);
err:
	return -1;
}

/**
 * Get RSS parent rxq structure for given queues.
 *
 * Creates a new or returns an existed one.
 *
 * @param priv
 *   Pointer to private structure.
 * @param queues
 *   queues indices array, NULL in default RSS case.
 * @param children_n
 *   the size of queues array.
 *
 * @return
 *   Pointer to a parent rxq structure, NULL on failure.
 */
static struct rxq *
priv_parent_get(struct priv *priv,
		uint16_t queues[],
		uint16_t children_n,
		struct rte_flow_error *error)
{
	unsigned int i;
	struct rxq *parent;

	for (parent = LIST_FIRST(&priv->parents);
	     parent;
	     parent = LIST_NEXT(parent, next)) {
		unsigned int same = 0;
		unsigned int overlap = 0;

		/*
		 * Find out whether an appropriate parent queue already exists
		 * and can be reused, otherwise make sure there are no overlaps.
		 */
		for (i = 0; i < children_n; ++i) {
			unsigned int j;

			for (j = 0; j < parent->rss.queues_n; ++j) {
				if (parent->rss.queues[j] != queues[i])
					continue;
				++overlap;
				if (i == j)
					++same;
			}
		}
		if (same == children_n &&
			children_n == parent->rss.queues_n)
			return parent;
		else if (overlap)
			goto error;
	}
	/* Exclude the cases when some QPs were created without RSS */
	for (i = 0; i < children_n; ++i) {
		struct rxq *rxq = (*priv->rxqs)[queues[i]];
		if (rxq->qp)
			goto error;
	}
	parent = priv_parent_create(priv, queues, children_n);
	if (!parent) {
		rte_flow_error_set(error,
				   ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "flow rule creation failure");
		return NULL;
	}
	return parent;

error:
	rte_flow_error_set(error,
			   EEXIST,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL,
			   "sharing a queue between several"
			   " RSS groups is not supported");
	return NULL;
}

/**
 * Complete flow rule creation.
 *
 * @param priv
 *   Pointer to private structure.
 * @param ibv_attr
 *   Verbs flow attributes.
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
			      struct ibv_flow_attr *ibv_attr,
			      struct mlx4_flow_action *action,
			      struct rte_flow_error *error)
{
	struct ibv_qp *qp;
	struct rte_flow *rte_flow;
	struct rxq *rxq_parent = NULL;

	assert(priv->pd);
	assert(priv->ctx);
	rte_flow = rte_calloc(__func__, 1, sizeof(*rte_flow), 0);
	if (!rte_flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate flow memory");
		return NULL;
	}
	if (action->drop) {
		qp = priv->flow_drop_queue ? priv->flow_drop_queue->qp : NULL;
	} else {
		int ret;
		unsigned int i;
		struct rxq *rxq = NULL;

		if (action->queues_n > 1) {
			rxq_parent = priv_parent_get(priv, action->queues,
						     action->queues_n, error);
			if (!rxq_parent)
				goto error;
		}
		for (i = 0; i < action->queues_n; ++i) {
			rxq = (*priv->rxqs)[action->queues[i]];
			/*
			 * In case of isolated mode we postpone
			 * ibv receive queue creation till the first
			 * rte_flow rule will be applied on that queue.
			 */
			if (!rxq->qp) {
				assert(priv->isolated);
				ret = rxq_create_qp(rxq, rxq->elts_n,
						    0, 0, rxq_parent);
				if (ret) {
					rte_flow_error_set(
						error,
						ENOMEM,
						RTE_FLOW_ERROR_TYPE_HANDLE,
						NULL,
						"flow rule creation failure");
					goto error;
				}
			}
		}
		qp = action->queues_n > 1 ? rxq_parent->qp : rxq->qp;
		rte_flow->qp = qp;
	}
	rte_flow->ibv_attr = ibv_attr;
	if (!priv->started)
		return rte_flow;
	rte_flow->ibv_flow = ibv_create_flow(qp, rte_flow->ibv_attr);
	if (!rte_flow->ibv_flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "flow rule creation failure");
		goto error;
	}
	return rte_flow;

error:
	if (rxq_parent)
		rxq_parent_cleanup(rxq_parent);
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
 * @param[in] items
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
	struct mlx4_flow_action action;
	struct mlx4_flow flow = { .offset = sizeof(struct ibv_flow_attr), };
	int err;

	err = priv_flow_validate(priv, attr, items, actions, error, &flow);
	if (err)
		return NULL;
	flow.ibv_attr = rte_malloc(__func__, flow.offset, 0);
	if (!flow.ibv_attr) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate ibv_attr memory");
		return NULL;
	}
	flow.offset = sizeof(struct ibv_flow_attr);
	*flow.ibv_attr = (struct ibv_flow_attr){
		.comp_mask = 0,
		.type = IBV_FLOW_ATTR_NORMAL,
		.size = sizeof(struct ibv_flow_attr),
		.priority = attr->priority,
		.num_of_specs = 0,
		.port = priv->port,
		.flags = 0,
	};
	claim_zero(priv_flow_validate(priv, attr, items, actions,
				      error, &flow));
	action = (struct mlx4_flow_action){
		.queue = 0,
		.drop = 0,
	};
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID) {
			continue;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			action.queue = 1;
			action.queues_n = 1;
			action.queues[0] =
				((const struct rte_flow_action_queue *)
				 actions->conf)->index;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_DROP) {
			action.drop = 1;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_RSS) {
			unsigned int i;
			const struct rte_flow_action_rss *rss =
				(const struct rte_flow_action_rss *)
				 actions->conf;

			action.queue = 1;
			action.queues_n = rss->num;
			for (i = 0; i < rss->num; ++i)
				action.queues[i] = rss->queue[i];
		} else {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions, "unsupported action");
			goto exit;
		}
	}
	rte_flow = priv_flow_create_action_queue(priv, flow.ibv_attr,
						 &action, error);
	if (rte_flow)
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
mlx4_flow_create(struct rte_eth_dev *dev,
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
		LIST_INSERT_HEAD(&priv->flows, flow, next);
		DEBUG("Flow created %p", (void *)flow);
	}
	priv_unlock(priv);
	return flow;
}

/**
 * @see rte_flow_isolate()
 *
 * Must be done before calling dev_configure().
 *
 * @param dev
 *   Pointer to the ethernet device structure.
 * @param enable
 *   Nonzero to enter isolated mode, attempt to leave it otherwise.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative value on error.
 */
int
mlx4_flow_isolate(struct rte_eth_dev *dev,
		  int enable,
		  struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;

	priv_lock(priv);
	if (priv->rxqs) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "isolated mode must be set"
				   " before configuring the device");
		priv_unlock(priv);
		return -rte_errno;
	}
	priv->isolated = !!enable;
	priv_unlock(priv);
	return 0;
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
priv_flow_destroy(struct priv *priv, struct rte_flow *flow)
{
	(void)priv;
	LIST_REMOVE(flow, next);
	if (flow->ibv_flow)
		claim_zero(ibv_destroy_flow(flow->ibv_flow));
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
mlx4_flow_destroy(struct rte_eth_dev *dev,
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
	while (!LIST_EMPTY(&priv->flows)) {
		struct rte_flow *flow;

		flow = LIST_FIRST(&priv->flows);
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
mlx4_flow_flush(struct rte_eth_dev *dev,
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
 * Remove all flows.
 *
 * Called by dev_stop() to remove all flows.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
mlx4_priv_flow_stop(struct priv *priv)
{
	struct rte_flow *flow;

	for (flow = LIST_FIRST(&priv->flows);
	     flow;
	     flow = LIST_NEXT(flow, next)) {
		claim_zero(ibv_destroy_flow(flow->ibv_flow));
		flow->ibv_flow = NULL;
		DEBUG("Flow %p removed", (void *)flow);
	}
	mlx4_flow_destroy_drop_queue(priv);
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
mlx4_priv_flow_start(struct priv *priv)
{
	int ret;
	struct ibv_qp *qp;
	struct rte_flow *flow;

	ret = mlx4_flow_create_drop_queue(priv);
	if (ret)
		return -1;
	for (flow = LIST_FIRST(&priv->flows);
	     flow;
	     flow = LIST_NEXT(flow, next)) {
		qp = flow->qp ? flow->qp : priv->flow_drop_queue->qp;
		flow->ibv_flow = ibv_create_flow(qp, flow->ibv_attr);
		if (!flow->ibv_flow) {
			DEBUG("Flow %p cannot be applied", (void *)flow);
			rte_errno = EINVAL;
			return rte_errno;
		}
		DEBUG("Flow %p applied", (void *)flow);
	}
	return 0;
}
