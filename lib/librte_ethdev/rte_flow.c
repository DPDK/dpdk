/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
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
	MK_FLOW_ITEM(PHY_PORT, sizeof(struct rte_flow_item_phy_port)),
	MK_FLOW_ITEM(PORT_ID, sizeof(struct rte_flow_item_port_id)),
	MK_FLOW_ITEM(RAW, sizeof(struct rte_flow_item_raw)),
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
	MK_FLOW_ITEM(GENEVE, sizeof(struct rte_flow_item_geneve)),
	MK_FLOW_ITEM(VXLAN_GPE, sizeof(struct rte_flow_item_vxlan_gpe)),
	MK_FLOW_ITEM(ARP_ETH_IPV4, sizeof(struct rte_flow_item_arp_eth_ipv4)),
	MK_FLOW_ITEM(IPV6_EXT, sizeof(struct rte_flow_item_ipv6_ext)),
	MK_FLOW_ITEM(ICMP6, sizeof(struct rte_flow_item_icmp6)),
	MK_FLOW_ITEM(ICMP6_ND_NS, sizeof(struct rte_flow_item_icmp6_nd_ns)),
	MK_FLOW_ITEM(ICMP6_ND_NA, sizeof(struct rte_flow_item_icmp6_nd_na)),
	MK_FLOW_ITEM(ICMP6_ND_OPT, sizeof(struct rte_flow_item_icmp6_nd_opt)),
	MK_FLOW_ITEM(ICMP6_ND_OPT_SLA_ETH,
		     sizeof(struct rte_flow_item_icmp6_nd_opt_sla_eth)),
	MK_FLOW_ITEM(ICMP6_ND_OPT_TLA_ETH,
		     sizeof(struct rte_flow_item_icmp6_nd_opt_tla_eth)),
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
	MK_FLOW_ACTION(RSS, sizeof(struct rte_flow_action_rss)),
	MK_FLOW_ACTION(PF, 0),
	MK_FLOW_ACTION(VF, sizeof(struct rte_flow_action_vf)),
	MK_FLOW_ACTION(PHY_PORT, sizeof(struct rte_flow_action_phy_port)),
	MK_FLOW_ACTION(PORT_ID, sizeof(struct rte_flow_action_port_id)),
	MK_FLOW_ACTION(OF_SET_MPLS_TTL,
		       sizeof(struct rte_flow_action_of_set_mpls_ttl)),
	MK_FLOW_ACTION(OF_DEC_MPLS_TTL, 0),
	MK_FLOW_ACTION(OF_SET_NW_TTL,
		       sizeof(struct rte_flow_action_of_set_nw_ttl)),
	MK_FLOW_ACTION(OF_DEC_NW_TTL, 0),
	MK_FLOW_ACTION(OF_COPY_TTL_OUT, 0),
	MK_FLOW_ACTION(OF_COPY_TTL_IN, 0),
	MK_FLOW_ACTION(OF_POP_VLAN, 0),
	MK_FLOW_ACTION(OF_PUSH_VLAN,
		       sizeof(struct rte_flow_action_of_push_vlan)),
	MK_FLOW_ACTION(OF_SET_VLAN_VID,
		       sizeof(struct rte_flow_action_of_set_vlan_vid)),
	MK_FLOW_ACTION(OF_SET_VLAN_PCP,
		       sizeof(struct rte_flow_action_of_set_vlan_pcp)),
	MK_FLOW_ACTION(OF_POP_MPLS,
		       sizeof(struct rte_flow_action_of_pop_mpls)),
	MK_FLOW_ACTION(OF_PUSH_MPLS,
		       sizeof(struct rte_flow_action_of_push_mpls)),
};

static int
flow_err(uint16_t port_id, int ret, struct rte_flow_error *error)
{
	if (ret == 0)
		return 0;
	if (rte_eth_dev_is_removed(port_id))
		return rte_flow_error_set(error, EIO,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, rte_strerror(EIO));
	return ret;
}

/* Get generic flow operations structure from a port. */
const struct rte_flow_ops *
rte_flow_ops_get(uint16_t port_id, struct rte_flow_error *error)
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
rte_flow_validate(uint16_t port_id,
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
		return flow_err(port_id, ops->validate(dev, attr, pattern,
						       actions, error), error);
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Create a flow rule on a given port. */
struct rte_flow *
rte_flow_create(uint16_t port_id,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow *flow;
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return NULL;
	if (likely(!!ops->create)) {
		flow = ops->create(dev, attr, pattern, actions, error);
		if (flow == NULL)
			flow_err(port_id, -rte_errno, error);
		return flow;
	}
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, rte_strerror(ENOSYS));
	return NULL;
}

/* Destroy a flow rule on a given port. */
int
rte_flow_destroy(uint16_t port_id,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->destroy))
		return flow_err(port_id, ops->destroy(dev, flow, error),
				error);
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Destroy all flow rules associated with a port. */
int
rte_flow_flush(uint16_t port_id,
	       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->flush))
		return flow_err(port_id, ops->flush(dev, error), error);
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Query an existing flow rule. */
int
rte_flow_query(uint16_t port_id,
	       struct rte_flow *flow,
	       const struct rte_flow_action *action,
	       void *data,
	       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (!ops)
		return -rte_errno;
	if (likely(!!ops->query))
		return flow_err(port_id, ops->query(dev, flow, action, data,
						    error), error);
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Restrict ingress traffic to the defined flow rules. */
int
rte_flow_isolate(uint16_t port_id,
		 int set,
		 struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (!ops)
		return -rte_errno;
	if (likely(!!ops->isolate))
		return flow_err(port_id, ops->isolate(dev, set, error), error);
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Initialize flow error structure. */
int
rte_flow_error_set(struct rte_flow_error *error,
		   int code,
		   enum rte_flow_error_type type,
		   const void *cause,
		   const char *message)
{
	if (error) {
		*error = (struct rte_flow_error){
			.type = type,
			.cause = cause,
			.message = message,
		};
	}
	rte_errno = code;
	return -code;
}

/** Pattern item specification types. */
enum item_spec_type {
	ITEM_SPEC,
	ITEM_LAST,
	ITEM_MASK,
};

/** Compute storage space needed by item specification and copy it. */
static size_t
flow_item_spec_copy(void *buf, const struct rte_flow_item *item,
		    enum item_spec_type type)
{
	size_t size = 0;
	const void *data =
		type == ITEM_SPEC ? item->spec :
		type == ITEM_LAST ? item->last :
		type == ITEM_MASK ? item->mask :
		NULL;

	if (!item->spec || !data)
		goto empty;
	switch (item->type) {
		union {
			const struct rte_flow_item_raw *raw;
		} spec;
		union {
			const struct rte_flow_item_raw *raw;
		} last;
		union {
			const struct rte_flow_item_raw *raw;
		} mask;
		union {
			const struct rte_flow_item_raw *raw;
		} src;
		union {
			struct rte_flow_item_raw *raw;
		} dst;
		size_t off;

	case RTE_FLOW_ITEM_TYPE_RAW:
		spec.raw = item->spec;
		last.raw = item->last ? item->last : item->spec;
		mask.raw = item->mask ? item->mask : &rte_flow_item_raw_mask;
		src.raw = data;
		dst.raw = buf;
		off = RTE_ALIGN_CEIL(sizeof(struct rte_flow_item_raw),
				     sizeof(*src.raw->pattern));
		if (type == ITEM_SPEC ||
		    (type == ITEM_MASK &&
		     ((spec.raw->length & mask.raw->length) >=
		      (last.raw->length & mask.raw->length))))
			size = spec.raw->length & mask.raw->length;
		else
			size = last.raw->length & mask.raw->length;
		size = off + size * sizeof(*src.raw->pattern);
		if (dst.raw) {
			memcpy(dst.raw, src.raw, sizeof(*src.raw));
			dst.raw->pattern = memcpy((uint8_t *)dst.raw + off,
						  src.raw->pattern,
						  size - off);
		}
		break;
	default:
		size = rte_flow_desc_item[item->type].size;
		if (buf)
			memcpy(buf, data, size);
		break;
	}
empty:
	return RTE_ALIGN_CEIL(size, sizeof(double));
}

/** Compute storage space needed by action configuration and copy it. */
static size_t
flow_action_conf_copy(void *buf, const struct rte_flow_action *action)
{
	size_t size = 0;

	if (!action->conf)
		goto empty;
	switch (action->type) {
		union {
			const struct rte_flow_action_rss *rss;
		} src;
		union {
			struct rte_flow_action_rss *rss;
		} dst;
		size_t off;

	case RTE_FLOW_ACTION_TYPE_RSS:
		src.rss = action->conf;
		dst.rss = buf;
		off = 0;
		if (dst.rss)
			*dst.rss = (struct rte_flow_action_rss){
				.func = src.rss->func,
				.level = src.rss->level,
				.types = src.rss->types,
				.key_len = src.rss->key_len,
				.queue_num = src.rss->queue_num,
			};
		off += sizeof(*src.rss);
		if (src.rss->key_len) {
			off = RTE_ALIGN_CEIL(off, sizeof(double));
			size = sizeof(*src.rss->key) * src.rss->key_len;
			if (dst.rss)
				dst.rss->key = memcpy
					((void *)((uintptr_t)dst.rss + off),
					 src.rss->key, size);
			off += size;
		}
		if (src.rss->queue_num) {
			off = RTE_ALIGN_CEIL(off, sizeof(double));
			size = sizeof(*src.rss->queue) * src.rss->queue_num;
			if (dst.rss)
				dst.rss->queue = memcpy
					((void *)((uintptr_t)dst.rss + off),
					 src.rss->queue, size);
			off += size;
		}
		size = off;
		break;
	default:
		size = rte_flow_desc_action[action->type].size;
		if (buf)
			memcpy(buf, action->conf, size);
		break;
	}
empty:
	return RTE_ALIGN_CEIL(size, sizeof(double));
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
			if (item->spec) {
				if (fd)
					dst->spec = fd->data + off2;
				off2 += flow_item_spec_copy
					(fd ? fd->data + off2 : NULL, item,
					 ITEM_SPEC);
			}
			if (item->last) {
				if (fd)
					dst->last = fd->data + off2;
				off2 += flow_item_spec_copy
					(fd ? fd->data + off2 : NULL, item,
					 ITEM_LAST);
			}
			if (item->mask) {
				if (fd)
					dst->mask = fd->data + off2;
				off2 += flow_item_spec_copy
					(fd ? fd->data + off2 : NULL, item,
					 ITEM_MASK);
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
			if (action->conf) {
				if (fd)
					dst->conf = fd->data + off2;
				off2 += flow_action_conf_copy
					(fd ? fd->data + off2 : NULL, action);
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
