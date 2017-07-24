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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_branch_prediction.h>
#include "rte_ethdev.h"
#include "rte_flow_driver.h"
#include "rte_flow.h"

/**
 * Flow elements description tables.
 */
struct rte_flow_desc_data {
	const char *name;
	size_t size;
};

/** Generate flow_item[] entry. */
#define MK_FLOW_ITEM(t, s) \
	[RTE_FLOW_ITEM_TYPE_ ## t] = { \
		.name = # t, \
		.size = s, \
	}

/** Information about known flow pattern items. */
static const struct rte_flow_desc_data rte_flow_desc_item[] = {
	MK_FLOW_ITEM(END, 0),
	MK_FLOW_ITEM(VOID, 0),
	MK_FLOW_ITEM(INVERT, 0),
	MK_FLOW_ITEM(ANY, sizeof(struct rte_flow_item_any)),
	MK_FLOW_ITEM(PF, 0),
	MK_FLOW_ITEM(VF, sizeof(struct rte_flow_item_vf)),
	MK_FLOW_ITEM(PORT, sizeof(struct rte_flow_item_port)),
	MK_FLOW_ITEM(RAW, sizeof(struct rte_flow_item_raw)), /* +pattern[] */
	MK_FLOW_ITEM(ETH, sizeof(struct rte_flow_item_eth)),
	MK_FLOW_ITEM(VLAN, sizeof(struct rte_flow_item_vlan)),
	MK_FLOW_ITEM(IPV4, sizeof(struct rte_flow_item_ipv4)),
	MK_FLOW_ITEM(IPV6, sizeof(struct rte_flow_item_ipv6)),
	MK_FLOW_ITEM(ICMP, sizeof(struct rte_flow_item_icmp)),
	MK_FLOW_ITEM(UDP, sizeof(struct rte_flow_item_udp)),
	MK_FLOW_ITEM(TCP, sizeof(struct rte_flow_item_tcp)),
	MK_FLOW_ITEM(SCTP, sizeof(struct rte_flow_item_sctp)),
	MK_FLOW_ITEM(VXLAN, sizeof(struct rte_flow_item_vxlan)),
	MK_FLOW_ITEM(MPLS, sizeof(struct rte_flow_item_mpls)),
	MK_FLOW_ITEM(GRE, sizeof(struct rte_flow_item_gre)),
	MK_FLOW_ITEM(E_TAG, sizeof(struct rte_flow_item_e_tag)),
	MK_FLOW_ITEM(NVGRE, sizeof(struct rte_flow_item_nvgre)),
};

/** Generate flow_action[] entry. */
#define MK_FLOW_ACTION(t, s) \
	[RTE_FLOW_ACTION_TYPE_ ## t] = { \
		.name = # t, \
		.size = s, \
	}

/** Information about known flow actions. */
static const struct rte_flow_desc_data rte_flow_desc_action[] = {
	MK_FLOW_ACTION(END, 0),
	MK_FLOW_ACTION(VOID, 0),
	MK_FLOW_ACTION(PASSTHRU, 0),
	MK_FLOW_ACTION(MARK, sizeof(struct rte_flow_action_mark)),
	MK_FLOW_ACTION(FLAG, 0),
	MK_FLOW_ACTION(QUEUE, sizeof(struct rte_flow_action_queue)),
	MK_FLOW_ACTION(DROP, 0),
	MK_FLOW_ACTION(COUNT, 0),
	MK_FLOW_ACTION(DUP, sizeof(struct rte_flow_action_dup)),
	MK_FLOW_ACTION(RSS, sizeof(struct rte_flow_action_rss)), /* +queue[] */
	MK_FLOW_ACTION(PF, 0),
	MK_FLOW_ACTION(VF, sizeof(struct rte_flow_action_vf)),
};

/* Get generic flow operations structure from a port. */
const struct rte_flow_ops *
rte_flow_ops_get(uint8_t port_id, struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops;
	int code;

	if (unlikely(!rte_eth_dev_is_valid_port(port_id)))
		code = ENODEV;
	else if (unlikely(!dev->dev_ops->filter_ctrl ||
			  dev->dev_ops->filter_ctrl(dev,
						    RTE_ETH_FILTER_GENERIC,
						    RTE_ETH_FILTER_GET,
						    &ops) ||
			  !ops))
		code = ENOSYS;
	else
		return ops;
	rte_flow_error_set(error, code, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, rte_strerror(code));
	return NULL;
}

/* Check whether a flow rule can be created on a given port. */
int
rte_flow_validate(uint8_t port_id,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->validate))
		return ops->validate(dev, attr, pattern, actions, error);
	return -rte_flow_error_set(error, ENOSYS,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(ENOSYS));
}

/* Create a flow rule on a given port. */
struct rte_flow *
rte_flow_create(uint8_t port_id,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return NULL;
	if (likely(!!ops->create))
		return ops->create(dev, attr, pattern, actions, error);
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, rte_strerror(ENOSYS));
	return NULL;
}

/* Destroy a flow rule on a given port. */
int
rte_flow_destroy(uint8_t port_id,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->destroy))
		return ops->destroy(dev, flow, error);
	return -rte_flow_error_set(error, ENOSYS,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(ENOSYS));
}

/* Destroy all flow rules associated with a port. */
int
rte_flow_flush(uint8_t port_id,
	       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->flush))
		return ops->flush(dev, error);
	return -rte_flow_error_set(error, ENOSYS,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(ENOSYS));
}

/* Query an existing flow rule. */
int
rte_flow_query(uint8_t port_id,
	       struct rte_flow *flow,
	       enum rte_flow_action_type action,
	       void *data,
	       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (!ops)
		return -rte_errno;
	if (likely(!!ops->query))
		return ops->query(dev, flow, action, data, error);
	return -rte_flow_error_set(error, ENOSYS,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(ENOSYS));
}

/* Restrict ingress traffic to the defined flow rules. */
int
rte_flow_isolate(uint8_t port_id,
		 int set,
		 struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (!ops)
		return -rte_errno;
	if (likely(!!ops->isolate))
		return ops->isolate(dev, set, error);
	return -rte_flow_error_set(error, ENOSYS,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(ENOSYS));
}

/** Compute storage space needed by item specification. */
static void
flow_item_spec_size(const struct rte_flow_item *item,
		    size_t *size, size_t *pad)
{
	if (!item->spec) {
		*size = 0;
		goto empty;
	}
	switch (item->type) {
		union {
			const struct rte_flow_item_raw *raw;
		} spec;

	/* Not a fall-through */
	case RTE_FLOW_ITEM_TYPE_RAW:
		spec.raw = item->spec;
		*size = offsetof(struct rte_flow_item_raw, pattern) +
			spec.raw->length * sizeof(*spec.raw->pattern);
		break;
	default:
		*size = rte_flow_desc_item[item->type].size;
		break;
	}
empty:
	*pad = RTE_ALIGN_CEIL(*size, sizeof(double)) - *size;
}

/** Compute storage space needed by action configuration. */
static void
flow_action_conf_size(const struct rte_flow_action *action,
		      size_t *size, size_t *pad)
{
	if (!action->conf) {
		*size = 0;
		goto empty;
	}
	switch (action->type) {
		union {
			const struct rte_flow_action_rss *rss;
		} conf;

	/* Not a fall-through. */
	case RTE_FLOW_ACTION_TYPE_RSS:
		conf.rss = action->conf;
		*size = offsetof(struct rte_flow_action_rss, queue) +
			conf.rss->num * sizeof(*conf.rss->queue);
		break;
	default:
		*size = rte_flow_desc_action[action->type].size;
		break;
	}
empty:
	*pad = RTE_ALIGN_CEIL(*size, sizeof(double)) - *size;
}

/** Store a full rte_flow description. */
size_t
rte_flow_copy(struct rte_flow_desc *desc, size_t len,
	      const struct rte_flow_attr *attr,
	      const struct rte_flow_item *items,
	      const struct rte_flow_action *actions)
{
	struct rte_flow_desc *fd = NULL;
	size_t tmp;
	size_t pad;
	size_t off1 = 0;
	size_t off2 = 0;
	size_t size = 0;

store:
	if (items) {
		const struct rte_flow_item *item;

		item = items;
		if (fd)
			fd->items = (void *)&fd->data[off1];
		do {
			struct rte_flow_item *dst = NULL;

			if ((size_t)item->type >=
				RTE_DIM(rte_flow_desc_item) ||
			    !rte_flow_desc_item[item->type].name) {
				rte_errno = ENOTSUP;
				return 0;
			}
			if (fd)
				dst = memcpy(fd->data + off1, item,
					     sizeof(*item));
			off1 += sizeof(*item);
			flow_item_spec_size(item, &tmp, &pad);
			if (item->spec) {
				if (fd)
					dst->spec = memcpy(fd->data + off2,
							   item->spec, tmp);
				off2 += tmp + pad;
			}
			if (item->last) {
				if (fd)
					dst->last = memcpy(fd->data + off2,
							   item->last, tmp);
				off2 += tmp + pad;
			}
			if (item->mask) {
				if (fd)
					dst->mask = memcpy(fd->data + off2,
							   item->mask, tmp);
				off2 += tmp + pad;
			}
			off2 = RTE_ALIGN_CEIL(off2, sizeof(double));
		} while ((item++)->type != RTE_FLOW_ITEM_TYPE_END);
		off1 = RTE_ALIGN_CEIL(off1, sizeof(double));
	}
	if (actions) {
		const struct rte_flow_action *action;

		action = actions;
		if (fd)
			fd->actions = (void *)&fd->data[off1];
		do {
			struct rte_flow_action *dst = NULL;

			if ((size_t)action->type >=
				RTE_DIM(rte_flow_desc_action) ||
			    !rte_flow_desc_action[action->type].name) {
				rte_errno = ENOTSUP;
				return 0;
			}
			if (fd)
				dst = memcpy(fd->data + off1, action,
					     sizeof(*action));
			off1 += sizeof(*action);
			flow_action_conf_size(action, &tmp, &pad);
			if (action->conf) {
				if (fd)
					dst->conf = memcpy(fd->data + off2,
							   action->conf, tmp);
				off2 += tmp + pad;
			}
			off2 = RTE_ALIGN_CEIL(off2, sizeof(double));
		} while ((action++)->type != RTE_FLOW_ACTION_TYPE_END);
	}
	if (fd != NULL)
		return size;
	off1 = RTE_ALIGN_CEIL(off1, sizeof(double));
	tmp = RTE_ALIGN_CEIL(offsetof(struct rte_flow_desc, data),
			     sizeof(double));
	size = tmp + off1 + off2;
	if (size > len)
		return size;
	fd = desc;
	if (fd != NULL) {
		*fd = (const struct rte_flow_desc) {
			.size = size,
			.attr = *attr,
		};
		tmp -= offsetof(struct rte_flow_desc, data);
		off2 = tmp + off1;
		off1 = tmp;
		goto store;
	}
	return 0;
}
