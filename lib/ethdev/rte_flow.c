/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <stdalign.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_branch_prediction.h>
#include <rte_string_fns.h>
#include <rte_mbuf_dyn.h>
#include "rte_flow_driver.h"
#include "rte_flow.h"

#include "ethdev_trace.h"

#define FLOW_LOG RTE_ETHDEV_LOG_LINE

/* Mbuf dynamic field name for metadata. */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_dynf_metadata_offs, 19.11)
int32_t rte_flow_dynf_metadata_offs = -1;

/* Mbuf dynamic field flag bit number for metadata. */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_dynf_metadata_mask, 19.11)
uint64_t rte_flow_dynf_metadata_mask;

/**
 * Flow elements description tables.
 */
struct rte_flow_desc_data {
	const char *name;
	size_t size;
	size_t (*desc_fn)(void *dst, const void *src);
};

/**
 *
 * @param buf
 * Destination memory.
 * @param data
 * Source memory
 * @param size
 * Requested copy size
 * @param desc
 * rte_flow_desc_item - for flow item conversion.
 * rte_flow_desc_action - for flow action conversion.
 * @param type
 * Offset into the desc param or negative value for private flow elements.
 */
static inline size_t
rte_flow_conv_copy(void *buf, const void *data, const size_t size,
		   const struct rte_flow_desc_data *desc, int type)
{
	/**
	 * Allow PMD private flow item
	 */
	bool rte_type = type >= 0;

	size_t sz = rte_type ? desc[type].size : sizeof(void *);
	if (data == NULL)
		return 0;
	if (buf != NULL)
		rte_memcpy(buf, data, (size > sz ? sz : size));
	if (rte_type && desc[type].desc_fn)
		sz += desc[type].desc_fn(size > 0 ? buf : NULL, data);
	return sz;
}

static size_t
rte_flow_item_flex_conv(void *buf, const void *data)
{
	struct rte_flow_item_flex *dst = buf;
	const struct rte_flow_item_flex *src = data;
	if (buf) {
		dst->pattern = rte_memcpy
			((void *)((uintptr_t)(dst + 1)), src->pattern,
			 src->length);
	}
	return src->length;
}

/** Generate flow_item[] entry. */
#define MK_FLOW_ITEM(t, s) \
	[RTE_FLOW_ITEM_TYPE_ ## t] = { \
		.name = # t, \
		.size = s,               \
		.desc_fn = NULL,\
	}

#define MK_FLOW_ITEM_FN(t, s, fn) \
	[RTE_FLOW_ITEM_TYPE_ ## t] = {\
		.name = # t,                 \
		.size = s,                   \
		.desc_fn = fn,               \
	}

/** Information about known flow pattern items. */
static const struct rte_flow_desc_data rte_flow_desc_item[] = {
	MK_FLOW_ITEM(END, 0),
	MK_FLOW_ITEM(VOID, 0),
	MK_FLOW_ITEM(INVERT, 0),
	MK_FLOW_ITEM(ANY, sizeof(struct rte_flow_item_any)),
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
	MK_FLOW_ITEM(E_TAG, sizeof(struct rte_flow_item_e_tag)),
	MK_FLOW_ITEM(NVGRE, sizeof(struct rte_flow_item_nvgre)),
	MK_FLOW_ITEM(MPLS, sizeof(struct rte_flow_item_mpls)),
	MK_FLOW_ITEM(GRE, sizeof(struct rte_flow_item_gre)),
	MK_FLOW_ITEM(FUZZY, sizeof(struct rte_flow_item_fuzzy)),
	MK_FLOW_ITEM(GTP, sizeof(struct rte_flow_item_gtp)),
	MK_FLOW_ITEM(GTPC, sizeof(struct rte_flow_item_gtp)),
	MK_FLOW_ITEM(GTPU, sizeof(struct rte_flow_item_gtp)),
	MK_FLOW_ITEM(ESP, sizeof(struct rte_flow_item_esp)),
	MK_FLOW_ITEM(GENEVE, sizeof(struct rte_flow_item_geneve)),
	MK_FLOW_ITEM(VXLAN_GPE, sizeof(struct rte_flow_item_vxlan_gpe)),
	MK_FLOW_ITEM(ARP_ETH_IPV4, sizeof(struct rte_flow_item_arp_eth_ipv4)),
	MK_FLOW_ITEM(IPV6_EXT, sizeof(struct rte_flow_item_ipv6_ext)),
	MK_FLOW_ITEM(IPV6_FRAG_EXT, sizeof(struct rte_flow_item_ipv6_frag_ext)),
	MK_FLOW_ITEM(ICMP6, sizeof(struct rte_flow_item_icmp6)),
	MK_FLOW_ITEM(ICMP6_ECHO_REQUEST, sizeof(struct rte_flow_item_icmp6_echo)),
	MK_FLOW_ITEM(ICMP6_ECHO_REPLY, sizeof(struct rte_flow_item_icmp6_echo)),
	MK_FLOW_ITEM(ICMP6_ND_NS, sizeof(struct rte_flow_item_icmp6_nd_ns)),
	MK_FLOW_ITEM(ICMP6_ND_NA, sizeof(struct rte_flow_item_icmp6_nd_na)),
	MK_FLOW_ITEM(ICMP6_ND_OPT, sizeof(struct rte_flow_item_icmp6_nd_opt)),
	MK_FLOW_ITEM(ICMP6_ND_OPT_SLA_ETH,
		     sizeof(struct rte_flow_item_icmp6_nd_opt_sla_eth)),
	MK_FLOW_ITEM(ICMP6_ND_OPT_TLA_ETH,
		     sizeof(struct rte_flow_item_icmp6_nd_opt_tla_eth)),
	MK_FLOW_ITEM(MARK, sizeof(struct rte_flow_item_mark)),
	MK_FLOW_ITEM(META, sizeof(struct rte_flow_item_meta)),
	MK_FLOW_ITEM(RANDOM, sizeof(struct rte_flow_item_random)),
	MK_FLOW_ITEM(TAG, sizeof(struct rte_flow_item_tag)),
	MK_FLOW_ITEM(GRE_KEY, sizeof(rte_be32_t)),
	MK_FLOW_ITEM(GRE_OPTION, sizeof(struct rte_flow_item_gre_opt)),
	MK_FLOW_ITEM(GTP_PSC, sizeof(struct rte_flow_item_gtp_psc)),
	MK_FLOW_ITEM(PPPOES, sizeof(struct rte_flow_item_pppoe)),
	MK_FLOW_ITEM(PPPOED, sizeof(struct rte_flow_item_pppoe)),
	MK_FLOW_ITEM(PPPOE_PROTO_ID,
			sizeof(struct rte_flow_item_pppoe_proto_id)),
	MK_FLOW_ITEM(NSH, sizeof(struct rte_flow_item_nsh)),
	MK_FLOW_ITEM(IGMP, sizeof(struct rte_flow_item_igmp)),
	MK_FLOW_ITEM(AH, sizeof(struct rte_flow_item_ah)),
	MK_FLOW_ITEM(HIGIG2, sizeof(struct rte_flow_item_higig2_hdr)),
	MK_FLOW_ITEM(L2TPV3OIP, sizeof(struct rte_flow_item_l2tpv3oip)),
	MK_FLOW_ITEM(PFCP, sizeof(struct rte_flow_item_pfcp)),
	MK_FLOW_ITEM(ECPRI, sizeof(struct rte_flow_item_ecpri)),
	MK_FLOW_ITEM(GENEVE_OPT, sizeof(struct rte_flow_item_geneve_opt)),
	MK_FLOW_ITEM(INTEGRITY, sizeof(struct rte_flow_item_integrity)),
	MK_FLOW_ITEM(CONNTRACK, sizeof(uint32_t)),
	MK_FLOW_ITEM(PORT_REPRESENTOR, sizeof(struct rte_flow_item_ethdev)),
	MK_FLOW_ITEM(REPRESENTED_PORT, sizeof(struct rte_flow_item_ethdev)),
	MK_FLOW_ITEM_FN(FLEX, sizeof(struct rte_flow_item_flex),
			rte_flow_item_flex_conv),
	MK_FLOW_ITEM(L2TPV2, sizeof(struct rte_flow_item_l2tpv2)),
	MK_FLOW_ITEM(PPP, sizeof(struct rte_flow_item_ppp)),
	MK_FLOW_ITEM(METER_COLOR, sizeof(struct rte_flow_item_meter_color)),
	MK_FLOW_ITEM(IPV6_ROUTING_EXT, sizeof(struct rte_flow_item_ipv6_routing_ext)),
	MK_FLOW_ITEM(QUOTA, sizeof(struct rte_flow_item_quota)),
	MK_FLOW_ITEM(AGGR_AFFINITY, sizeof(struct rte_flow_item_aggr_affinity)),
	MK_FLOW_ITEM(TX_QUEUE, sizeof(struct rte_flow_item_tx_queue)),
	MK_FLOW_ITEM(IB_BTH, sizeof(struct rte_flow_item_ib_bth)),
	MK_FLOW_ITEM(PTYPE, sizeof(struct rte_flow_item_ptype)),
	MK_FLOW_ITEM(COMPARE, sizeof(struct rte_flow_item_compare)),
};

/** Generate flow_action[] entry. */
#define MK_FLOW_ACTION(t, s) \
	[RTE_FLOW_ACTION_TYPE_ ## t] = { \
		.name = # t, \
		.size = s, \
		.desc_fn = NULL,\
	}

#define MK_FLOW_ACTION_FN(t, fn) \
	[RTE_FLOW_ACTION_TYPE_ ## t] = { \
		.name = # t, \
		.size = 0, \
		.desc_fn = fn,\
	}


/** Information about known flow actions. */
static const struct rte_flow_desc_data rte_flow_desc_action[] = {
	MK_FLOW_ACTION(END, 0),
	MK_FLOW_ACTION(VOID, 0),
	MK_FLOW_ACTION(PASSTHRU, 0),
	MK_FLOW_ACTION(JUMP, sizeof(struct rte_flow_action_jump)),
	MK_FLOW_ACTION(MARK, sizeof(struct rte_flow_action_mark)),
	MK_FLOW_ACTION(FLAG, 0),
	MK_FLOW_ACTION(QUEUE, sizeof(struct rte_flow_action_queue)),
	MK_FLOW_ACTION(DROP, 0),
	MK_FLOW_ACTION(COUNT, sizeof(struct rte_flow_action_count)),
	MK_FLOW_ACTION(RSS, sizeof(struct rte_flow_action_rss)),
	MK_FLOW_ACTION(PF, 0),
	MK_FLOW_ACTION(VF, sizeof(struct rte_flow_action_vf)),
	MK_FLOW_ACTION(PORT_ID, sizeof(struct rte_flow_action_port_id)),
	MK_FLOW_ACTION(METER, sizeof(struct rte_flow_action_meter)),
	MK_FLOW_ACTION(SECURITY, sizeof(struct rte_flow_action_security)),
	MK_FLOW_ACTION(OF_DEC_NW_TTL, 0),
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
	MK_FLOW_ACTION(VXLAN_ENCAP, sizeof(struct rte_flow_action_vxlan_encap)),
	MK_FLOW_ACTION(VXLAN_DECAP, 0),
	MK_FLOW_ACTION(NVGRE_ENCAP, sizeof(struct rte_flow_action_nvgre_encap)),
	MK_FLOW_ACTION(NVGRE_DECAP, 0),
	MK_FLOW_ACTION(RAW_ENCAP, sizeof(struct rte_flow_action_raw_encap)),
	MK_FLOW_ACTION(RAW_DECAP, sizeof(struct rte_flow_action_raw_decap)),
	MK_FLOW_ACTION(SET_IPV4_SRC,
		       sizeof(struct rte_flow_action_set_ipv4)),
	MK_FLOW_ACTION(SET_IPV4_DST,
		       sizeof(struct rte_flow_action_set_ipv4)),
	MK_FLOW_ACTION(SET_IPV6_SRC,
		       sizeof(struct rte_flow_action_set_ipv6)),
	MK_FLOW_ACTION(SET_IPV6_DST,
		       sizeof(struct rte_flow_action_set_ipv6)),
	MK_FLOW_ACTION(SET_TP_SRC,
		       sizeof(struct rte_flow_action_set_tp)),
	MK_FLOW_ACTION(SET_TP_DST,
		       sizeof(struct rte_flow_action_set_tp)),
	MK_FLOW_ACTION(MAC_SWAP, 0),
	MK_FLOW_ACTION(DEC_TTL, 0),
	MK_FLOW_ACTION(SET_TTL, sizeof(struct rte_flow_action_set_ttl)),
	MK_FLOW_ACTION(SET_MAC_SRC, sizeof(struct rte_flow_action_set_mac)),
	MK_FLOW_ACTION(SET_MAC_DST, sizeof(struct rte_flow_action_set_mac)),
	MK_FLOW_ACTION(INC_TCP_SEQ, sizeof(rte_be32_t)),
	MK_FLOW_ACTION(DEC_TCP_SEQ, sizeof(rte_be32_t)),
	MK_FLOW_ACTION(INC_TCP_ACK, sizeof(rte_be32_t)),
	MK_FLOW_ACTION(DEC_TCP_ACK, sizeof(rte_be32_t)),
	MK_FLOW_ACTION(SET_TAG, sizeof(struct rte_flow_action_set_tag)),
	MK_FLOW_ACTION(SET_META, sizeof(struct rte_flow_action_set_meta)),
	MK_FLOW_ACTION(SET_IPV4_DSCP, sizeof(struct rte_flow_action_set_dscp)),
	MK_FLOW_ACTION(SET_IPV6_DSCP, sizeof(struct rte_flow_action_set_dscp)),
	MK_FLOW_ACTION(AGE, sizeof(struct rte_flow_action_age)),
	MK_FLOW_ACTION(SAMPLE, sizeof(struct rte_flow_action_sample)),
	MK_FLOW_ACTION(MODIFY_FIELD,
		       sizeof(struct rte_flow_action_modify_field)),
	/**
	 * Indirect action represented as handle of type
	 * (struct rte_flow_action_handle *) stored in conf field (see
	 * struct rte_flow_action); no need for additional structure to * store
	 * indirect action handle.
	 */
	MK_FLOW_ACTION(INDIRECT, 0),
	MK_FLOW_ACTION(CONNTRACK, sizeof(struct rte_flow_action_conntrack)),
	MK_FLOW_ACTION(PORT_REPRESENTOR, sizeof(struct rte_flow_action_ethdev)),
	MK_FLOW_ACTION(REPRESENTED_PORT, sizeof(struct rte_flow_action_ethdev)),
	MK_FLOW_ACTION(METER_MARK, sizeof(struct rte_flow_action_meter_mark)),
	MK_FLOW_ACTION(SEND_TO_KERNEL, 0),
	MK_FLOW_ACTION(QUOTA, sizeof(struct rte_flow_action_quota)),
	MK_FLOW_ACTION(IPV6_EXT_PUSH, sizeof(struct rte_flow_action_ipv6_ext_push)),
	MK_FLOW_ACTION(IPV6_EXT_REMOVE, sizeof(struct rte_flow_action_ipv6_ext_remove)),
	MK_FLOW_ACTION(INDIRECT_LIST,
		       sizeof(struct rte_flow_action_indirect_list)),
	MK_FLOW_ACTION(PROG,
		       sizeof(struct rte_flow_action_prog)),
	MK_FLOW_ACTION(NAT64, sizeof(struct rte_flow_action_nat64)),
	MK_FLOW_ACTION(JUMP_TO_TABLE_INDEX, sizeof(struct rte_flow_action_jump_to_table_index)),
};

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_dynf_metadata_register, 19.11)
int
rte_flow_dynf_metadata_register(void)
{
	int offset;
	int flag;

	static const struct rte_mbuf_dynfield desc_offs = {
		.name = RTE_MBUF_DYNFIELD_METADATA_NAME,
		.size = sizeof(uint32_t),
		.align = alignof(uint32_t),
	};
	static const struct rte_mbuf_dynflag desc_flag = {
		.name = RTE_MBUF_DYNFLAG_METADATA_NAME,
	};

	offset = rte_mbuf_dynfield_register(&desc_offs);
	if (offset < 0)
		goto error;
	flag = rte_mbuf_dynflag_register(&desc_flag);
	if (flag < 0)
		goto error;
	rte_flow_dynf_metadata_offs = offset;
	rte_flow_dynf_metadata_mask = RTE_BIT64(flag);

	rte_flow_trace_dynf_metadata_register(offset, RTE_BIT64(flag));

	return 0;

error:
	rte_flow_dynf_metadata_offs = -1;
	rte_flow_dynf_metadata_mask = UINT64_C(0);
	return -rte_errno;
}

static inline void
fts_enter(struct rte_eth_dev *dev)
{
	if (!(dev->data->dev_flags & RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE))
		pthread_mutex_lock(&dev->data->flow_ops_mutex);
}

static inline void
fts_exit(struct rte_eth_dev *dev)
{
	if (!(dev->data->dev_flags & RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE))
		pthread_mutex_unlock(&dev->data->flow_ops_mutex);
}

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
	else if (unlikely(dev->dev_ops->flow_ops_get == NULL))
		/* flow API not supported with this driver dev_ops */
		code = ENOSYS;
	else
		code = dev->dev_ops->flow_ops_get(dev, &ops);
	if (code == 0 && ops == NULL)
		/* flow API not supported with this device */
		code = ENOSYS;

	if (code != 0) {
		rte_flow_error_set(error, code, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(code));
		return NULL;
	}
	return ops;
}

/* Check whether a flow rule can be created on a given port. */
RTE_EXPORT_SYMBOL(rte_flow_validate)
int
rte_flow_validate(uint16_t port_id,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

	if (likely(!!attr) && attr->transfer &&
	    (attr->ingress || attr->egress)) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR,
					  attr, "cannot use attr ingress/egress with attr transfer");
	}

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->validate)) {
		fts_enter(dev);
		ret = ops->validate(dev, attr, pattern, actions, error);
		fts_exit(dev);
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_validate(port_id, attr, pattern, actions, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Create a flow rule on a given port. */
RTE_EXPORT_SYMBOL(rte_flow_create)
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
		fts_enter(dev);
		flow = ops->create(dev, attr, pattern, actions, error);
		fts_exit(dev);
		if (flow == NULL)
			flow_err(port_id, -rte_errno, error);

		rte_flow_trace_create(port_id, attr, pattern, actions, flow);

		return flow;
	}
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, rte_strerror(ENOSYS));
	return NULL;
}

/* Destroy a flow rule on a given port. */
RTE_EXPORT_SYMBOL(rte_flow_destroy)
int
rte_flow_destroy(uint16_t port_id,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->destroy)) {
		fts_enter(dev);
		ret = ops->destroy(dev, flow, error);
		fts_exit(dev);
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_destroy(port_id, flow, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_actions_update, 23.07)
int
rte_flow_actions_update(uint16_t port_id,
			struct rte_flow *flow,
			const struct rte_flow_action actions[],
			struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->actions_update)) {
		fts_enter(dev);
		ret = ops->actions_update(dev, flow, actions, error);
		fts_exit(dev);

		rte_flow_trace_actions_update(port_id, flow, actions, ret);

		return flow_err(port_id, ret, error);
	}
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Destroy all flow rules associated with a port. */
RTE_EXPORT_SYMBOL(rte_flow_flush)
int
rte_flow_flush(uint16_t port_id,
	       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->flush)) {
		fts_enter(dev);
		ret = ops->flush(dev, error);
		fts_exit(dev);
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_flush(port_id, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Query an existing flow rule. */
RTE_EXPORT_SYMBOL(rte_flow_query)
int
rte_flow_query(uint16_t port_id,
	       struct rte_flow *flow,
	       const struct rte_flow_action *action,
	       void *data,
	       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (!ops)
		return -rte_errno;
	if (likely(!!ops->query)) {
		fts_enter(dev);
		ret = ops->query(dev, flow, action, data, error);
		fts_exit(dev);
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_query(port_id, flow, action, data, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Restrict ingress traffic to the defined flow rules. */
RTE_EXPORT_SYMBOL(rte_flow_isolate)
int
rte_flow_isolate(uint16_t port_id,
		 int set,
		 struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (!ops)
		return -rte_errno;
	if (likely(!!ops->isolate)) {
		fts_enter(dev);
		ret = ops->isolate(dev, set, error);
		fts_exit(dev);
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_isolate(port_id, set, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

/* Initialize flow error structure. */
RTE_EXPORT_SYMBOL(rte_flow_error_set)
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
enum rte_flow_conv_item_spec_type {
	RTE_FLOW_CONV_ITEM_SPEC,
	RTE_FLOW_CONV_ITEM_LAST,
	RTE_FLOW_CONV_ITEM_MASK,
};

/**
 * Copy pattern item specification.
 *
 * @param[out] buf
 *   Output buffer. Can be NULL if @p size is zero.
 * @param size
 *   Size of @p buf in bytes.
 * @param[in] item
 *   Pattern item to copy specification from.
 * @param type
 *   Specification selector for either @p spec, @p last or @p mask.
 *
 * @return
 *   Number of bytes needed to store pattern item specification regardless
 *   of @p size. @p buf contents are truncated to @p size if not large
 *   enough.
 */
static size_t
rte_flow_conv_item_spec(void *buf, const size_t size,
			const struct rte_flow_item *item,
			enum rte_flow_conv_item_spec_type type)
{
	size_t off;
	const void *data =
		type == RTE_FLOW_CONV_ITEM_SPEC ? item->spec :
		type == RTE_FLOW_CONV_ITEM_LAST ? item->last :
		type == RTE_FLOW_CONV_ITEM_MASK ? item->mask :
		NULL;

	switch (item->type) {
		union {
			const struct rte_flow_item_raw *raw;
			const struct rte_flow_item_geneve_opt *geneve_opt;
		} spec;
		union {
			const struct rte_flow_item_raw *raw;
		} last;
		union {
			const struct rte_flow_item_raw *raw;
		} mask;
		union {
			const struct rte_flow_item_raw *raw;
			const struct rte_flow_item_geneve_opt *geneve_opt;
		} src;
		union {
			struct rte_flow_item_raw *raw;
			struct rte_flow_item_geneve_opt *geneve_opt;
		} dst;
		void *deep_src;
		size_t tmp;

	case RTE_FLOW_ITEM_TYPE_RAW:
		spec.raw = item->spec;
		last.raw = item->last ? item->last : item->spec;
		mask.raw = item->mask ? item->mask : &rte_flow_item_raw_mask;
		src.raw = data;
		dst.raw = buf;
		rte_memcpy(dst.raw,
			   (&(struct rte_flow_item_raw){
				.relative = src.raw->relative,
				.search = src.raw->search,
				.reserved = src.raw->reserved,
				.offset = src.raw->offset,
				.limit = src.raw->limit,
				.length = src.raw->length,
			   }),
			   size > sizeof(*dst.raw) ? sizeof(*dst.raw) : size);
		off = sizeof(*dst.raw);
		if (type == RTE_FLOW_CONV_ITEM_SPEC ||
		    (type == RTE_FLOW_CONV_ITEM_MASK &&
		     ((spec.raw->length & mask.raw->length) >=
		      (last.raw->length & mask.raw->length))))
			tmp = spec.raw->length & mask.raw->length;
		else
			tmp = last.raw->length & mask.raw->length;
		if (tmp) {
			off = RTE_ALIGN_CEIL(off, sizeof(*dst.raw->pattern));
			if (size >= off + tmp) {
				deep_src = (void *)((uintptr_t)dst.raw + off);
				dst.raw->pattern = rte_memcpy(deep_src,
							      src.raw->pattern,
							      tmp);
			}
			off += tmp;
		}
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
		off = rte_flow_conv_copy(buf, data, size,
					 rte_flow_desc_item, item->type);
		spec.geneve_opt = item->spec;
		src.geneve_opt = data;
		dst.geneve_opt = buf;
		tmp = spec.geneve_opt->option_len << 2;
		if (size > 0 && src.geneve_opt->data) {
			deep_src = (void *)((uintptr_t)(dst.geneve_opt + 1));
			dst.geneve_opt->data = rte_memcpy(deep_src,
							  src.geneve_opt->data,
							  tmp);
		}
		off += tmp;
		break;
	default:
		off = rte_flow_conv_copy(buf, data, size,
					 rte_flow_desc_item, item->type);
		break;
	}
	return off;
}

/**
 * Copy action configuration.
 *
 * @param[out] buf
 *   Output buffer. Can be NULL if @p size is zero.
 * @param size
 *   Size of @p buf in bytes.
 * @param[in] action
 *   Action to copy configuration from.
 *
 * @return
 *   Number of bytes needed to store pattern item specification regardless
 *   of @p size. @p buf contents are truncated to @p size if not large
 *   enough.
 */
static size_t
rte_flow_conv_action_conf(void *buf, const size_t size,
			  const struct rte_flow_action *action)
{
	size_t off;

	switch (action->type) {
		union {
			const struct rte_flow_action_rss *rss;
			const struct rte_flow_action_vxlan_encap *vxlan_encap;
			const struct rte_flow_action_nvgre_encap *nvgre_encap;
		} src;
		union {
			struct rte_flow_action_rss *rss;
			struct rte_flow_action_vxlan_encap *vxlan_encap;
			struct rte_flow_action_nvgre_encap *nvgre_encap;
		} dst;
		size_t tmp;
		int ret;

	case RTE_FLOW_ACTION_TYPE_RSS:
		src.rss = action->conf;
		dst.rss = buf;
		rte_memcpy(dst.rss,
			   (&(struct rte_flow_action_rss){
				.func = src.rss->func,
				.level = src.rss->level,
				.types = src.rss->types,
				.key_len = src.rss->key_len,
				.queue_num = src.rss->queue_num,
			   }),
			   size > sizeof(*dst.rss) ? sizeof(*dst.rss) : size);
		off = sizeof(*dst.rss);
		if (src.rss->key_len && src.rss->key) {
			off = RTE_ALIGN_CEIL(off, sizeof(*dst.rss->key));
			tmp = sizeof(*src.rss->key) * src.rss->key_len;
			if (size >= (uint64_t)off + (uint64_t)tmp)
				dst.rss->key = rte_memcpy
					((void *)((uintptr_t)dst.rss + off),
					 src.rss->key, tmp);
			off += tmp;
		}
		if (src.rss->queue_num) {
			off = RTE_ALIGN_CEIL(off, sizeof(*dst.rss->queue));
			tmp = sizeof(*src.rss->queue) * src.rss->queue_num;
			if (size >= (uint64_t)off + (uint64_t)tmp)
				dst.rss->queue = rte_memcpy
					((void *)((uintptr_t)dst.rss + off),
					 src.rss->queue, tmp);
			off += tmp;
		}
		break;
	case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
	case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		src.vxlan_encap = action->conf;
		dst.vxlan_encap = buf;
		RTE_BUILD_BUG_ON(sizeof(*src.vxlan_encap) !=
				 sizeof(*src.nvgre_encap) ||
				 offsetof(struct rte_flow_action_vxlan_encap,
					  definition) !=
				 offsetof(struct rte_flow_action_nvgre_encap,
					  definition));
		off = sizeof(*dst.vxlan_encap);
		if (src.vxlan_encap->definition) {
			off = RTE_ALIGN_CEIL
				(off, sizeof(*dst.vxlan_encap->definition));
			ret = rte_flow_conv
				(RTE_FLOW_CONV_OP_PATTERN,
				 (void *)((uintptr_t)dst.vxlan_encap + off),
				 size > off ? size - off : 0,
				 src.vxlan_encap->definition, NULL);
			if (ret < 0)
				return 0;
			if (size >= off + ret)
				dst.vxlan_encap->definition =
					(void *)((uintptr_t)dst.vxlan_encap +
						 off);
			off += ret;
		}
		break;
	default:
		off = rte_flow_conv_copy(buf, action->conf, size,
					 rte_flow_desc_action, action->type);
		break;
	}
	return off;
}

/**
 * Copy a list of pattern items.
 *
 * @param[out] dst
 *   Destination buffer. Can be NULL if @p size is zero.
 * @param size
 *   Size of @p dst in bytes.
 * @param[in] src
 *   Source pattern items.
 * @param num
 *   Maximum number of pattern items to process from @p src or 0 to process
 *   the entire list. In both cases, processing stops after
 *   RTE_FLOW_ITEM_TYPE_END is encountered.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A positive value representing the number of bytes needed to store
 *   pattern items regardless of @p size on success (@p buf contents are
 *   truncated to @p size if not large enough), a negative errno value
 *   otherwise and rte_errno is set.
 */
static int
rte_flow_conv_pattern(struct rte_flow_item *dst,
		      const size_t size,
		      const struct rte_flow_item *src,
		      unsigned int num,
		      struct rte_flow_error *error)
{
	uintptr_t data = (uintptr_t)dst;
	size_t off;
	size_t ret;
	unsigned int i;

	for (i = 0, off = 0; !num || i != num; ++i, ++src, ++dst) {
		/**
		 * allow PMD private flow item
		 */
		if (((int)src->type >= 0) &&
			((size_t)src->type >= RTE_DIM(rte_flow_desc_item) ||
		    !rte_flow_desc_item[src->type].name))
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, src,
				 "cannot convert unknown item type");
		if (size >= off + sizeof(*dst))
			*dst = (struct rte_flow_item){
				.type = src->type,
			};
		off += sizeof(*dst);
		if (!src->type)
			num = i + 1;
	}
	num = i;
	src -= num;
	dst -= num;
	do {
		if (src->spec) {
			off = RTE_ALIGN_CEIL(off, sizeof(double));
			ret = rte_flow_conv_item_spec
				((void *)(data + off),
				 size > off ? size - off : 0, src,
				 RTE_FLOW_CONV_ITEM_SPEC);
			if (size && size >= off + ret)
				dst->spec = (void *)(data + off);
			off += ret;

		}
		if (src->last) {
			off = RTE_ALIGN_CEIL(off, sizeof(double));
			ret = rte_flow_conv_item_spec
				((void *)(data + off),
				 size > off ? size - off : 0, src,
				 RTE_FLOW_CONV_ITEM_LAST);
			if (size && size >= off + ret)
				dst->last = (void *)(data + off);
			off += ret;
		}
		if (src->mask) {
			off = RTE_ALIGN_CEIL(off, sizeof(double));
			ret = rte_flow_conv_item_spec
				((void *)(data + off),
				 size > off ? size - off : 0, src,
				 RTE_FLOW_CONV_ITEM_MASK);
			if (size && size >= off + ret)
				dst->mask = (void *)(data + off);
			off += ret;
		}
		++src;
		++dst;
	} while (--num);
	return off;
}

/**
 * Copy a list of actions.
 *
 * @param[out] dst
 *   Destination buffer. Can be NULL if @p size is zero.
 * @param size
 *   Size of @p dst in bytes.
 * @param[in] src
 *   Source actions.
 * @param num
 *   Maximum number of actions to process from @p src or 0 to process the
 *   entire list. In both cases, processing stops after
 *   RTE_FLOW_ACTION_TYPE_END is encountered.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A positive value representing the number of bytes needed to store
 *   actions regardless of @p size on success (@p buf contents are truncated
 *   to @p size if not large enough), a negative errno value otherwise and
 *   rte_errno is set.
 */
static int
rte_flow_conv_actions(struct rte_flow_action *dst,
		      const size_t size,
		      const struct rte_flow_action *src,
		      unsigned int num,
		      struct rte_flow_error *error)
{
	uintptr_t data = (uintptr_t)dst;
	size_t off;
	size_t ret;
	unsigned int i;

	for (i = 0, off = 0; !num || i != num; ++i, ++src, ++dst) {
		/**
		 * allow PMD private flow action
		 */
		if (((int)src->type >= 0) &&
		    ((size_t)src->type >= RTE_DIM(rte_flow_desc_action) ||
		    !rte_flow_desc_action[src->type].name))
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				 src, "cannot convert unknown action type");
		if (size >= off + sizeof(*dst))
			*dst = (struct rte_flow_action){
				.type = src->type,
			};
		off += sizeof(*dst);
		if (!src->type)
			num = i + 1;
	}
	num = i;
	src -= num;
	dst -= num;
	do {
		if (src->type == RTE_FLOW_ACTION_TYPE_INDIRECT) {
			/*
			 * Indirect action conf fills the indirect action
			 * handler. Copy the action handle directly instead
			 * of duplicating the pointer memory.
			 */
			if (size)
				dst->conf = src->conf;
		} else if (src->conf) {
			off = RTE_ALIGN_CEIL(off, sizeof(double));
			ret = rte_flow_conv_action_conf
				((void *)(data + off),
				 size > off ? size - off : 0, src);
			if (size && size >= off + ret)
				dst->conf = (void *)(data + off);
			off += ret;
		}
		++src;
		++dst;
	} while (--num);
	return off;
}

/**
 * Copy flow rule components.
 *
 * This comprises the flow rule descriptor itself, attributes, pattern and
 * actions list. NULL components in @p src are skipped.
 *
 * @param[out] dst
 *   Destination buffer. Can be NULL if @p size is zero.
 * @param size
 *   Size of @p dst in bytes.
 * @param[in] src
 *   Source flow rule descriptor.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A positive value representing the number of bytes needed to store all
 *   components including the descriptor regardless of @p size on success
 *   (@p buf contents are truncated to @p size if not large enough), a
 *   negative errno value otherwise and rte_errno is set.
 */
static int
rte_flow_conv_rule(struct rte_flow_conv_rule *dst,
		   const size_t size,
		   const struct rte_flow_conv_rule *src,
		   struct rte_flow_error *error)
{
	size_t off;
	int ret;

	rte_memcpy(dst,
		   (&(struct rte_flow_conv_rule){
			.attr = NULL,
			.pattern = NULL,
			.actions = NULL,
		   }),
		   size > sizeof(*dst) ? sizeof(*dst) : size);
	off = sizeof(*dst);
	if (src->attr_ro) {
		off = RTE_ALIGN_CEIL(off, sizeof(double));
		if (size && size >= off + sizeof(*dst->attr))
			dst->attr = rte_memcpy
				((void *)((uintptr_t)dst + off),
				 src->attr_ro, sizeof(*dst->attr));
		off += sizeof(*dst->attr);
	}
	if (src->pattern_ro) {
		off = RTE_ALIGN_CEIL(off, sizeof(double));
		ret = rte_flow_conv_pattern((void *)((uintptr_t)dst + off),
					    size > off ? size - off : 0,
					    src->pattern_ro, 0, error);
		if (ret < 0)
			return ret;
		if (size && size >= off + (size_t)ret)
			dst->pattern = (void *)((uintptr_t)dst + off);
		off += ret;
	}
	if (src->actions_ro) {
		off = RTE_ALIGN_CEIL(off, sizeof(double));
		ret = rte_flow_conv_actions((void *)((uintptr_t)dst + off),
					    size > off ? size - off : 0,
					    src->actions_ro, 0, error);
		if (ret < 0)
			return ret;
		if (size >= off + (size_t)ret)
			dst->actions = (void *)((uintptr_t)dst + off);
		off += ret;
	}
	return off;
}

/**
 * Retrieve the name of a pattern item/action type.
 *
 * @param is_action
 *   Nonzero when @p src represents an action type instead of a pattern item
 *   type.
 * @param is_ptr
 *   Nonzero to write string address instead of contents into @p dst.
 * @param[out] dst
 *   Destination buffer. Can be NULL if @p size is zero.
 * @param size
 *   Size of @p dst in bytes.
 * @param[in] src
 *   Depending on @p is_action, source pattern item or action type cast as a
 *   pointer.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A positive value representing the number of bytes needed to store the
 *   name or its address regardless of @p size on success (@p buf contents
 *   are truncated to @p size if not large enough), a negative errno value
 *   otherwise and rte_errno is set.
 */
static int
rte_flow_conv_name(int is_action,
		   int is_ptr,
		   char *dst,
		   const size_t size,
		   const void *src,
		   struct rte_flow_error *error)
{
	struct desc_info {
		const struct rte_flow_desc_data *data;
		size_t num;
	};
	static const struct desc_info info_rep[2] = {
		{ rte_flow_desc_item, RTE_DIM(rte_flow_desc_item), },
		{ rte_flow_desc_action, RTE_DIM(rte_flow_desc_action), },
	};
	const struct desc_info *const info = &info_rep[!!is_action];
	unsigned int type = (uintptr_t)src;

	if (type >= info->num)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "unknown object type to retrieve the name of");
	if (!is_ptr)
		return strlcpy(dst, info->data[type].name, size);
	if (size >= sizeof(const char **))
		*((const char **)dst) = info->data[type].name;
	return sizeof(const char **);
}

/** Helper function to convert flow API objects. */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_conv, 18.11)
int
rte_flow_conv(enum rte_flow_conv_op op,
	      void *dst,
	      size_t size,
	      const void *src,
	      struct rte_flow_error *error)
{
	int ret;

	switch (op) {
		const struct rte_flow_attr *attr;
		const struct rte_flow_item *item;

	case RTE_FLOW_CONV_OP_NONE:
		ret = 0;
		break;
	case RTE_FLOW_CONV_OP_ATTR:
		attr = src;
		if (size > sizeof(*attr))
			size = sizeof(*attr);
		rte_memcpy(dst, attr, size);
		ret = sizeof(*attr);
		break;
	case RTE_FLOW_CONV_OP_ITEM:
		ret = rte_flow_conv_pattern(dst, size, src, 1, error);
		break;
	case RTE_FLOW_CONV_OP_ITEM_MASK:
		item = src;
		if (item->mask == NULL) {
			ret = rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
						 item, "Mask not provided");
			break;
		}
		ret = rte_flow_conv_item_spec(dst, size, src, RTE_FLOW_CONV_ITEM_MASK);
		break;
	case RTE_FLOW_CONV_OP_ACTION:
		ret = rte_flow_conv_actions(dst, size, src, 1, error);
		break;
	case RTE_FLOW_CONV_OP_PATTERN:
		ret = rte_flow_conv_pattern(dst, size, src, 0, error);
		break;
	case RTE_FLOW_CONV_OP_ACTIONS:
		ret = rte_flow_conv_actions(dst, size, src, 0, error);
		break;
	case RTE_FLOW_CONV_OP_RULE:
		ret = rte_flow_conv_rule(dst, size, src, error);
		break;
	case RTE_FLOW_CONV_OP_ITEM_NAME:
		ret = rte_flow_conv_name(0, 0, dst, size, src, error);
		break;
	case RTE_FLOW_CONV_OP_ACTION_NAME:
		ret = rte_flow_conv_name(1, 0, dst, size, src, error);
		break;
	case RTE_FLOW_CONV_OP_ITEM_NAME_PTR:
		ret = rte_flow_conv_name(0, 1, dst, size, src, error);
		break;
	case RTE_FLOW_CONV_OP_ACTION_NAME_PTR:
		ret = rte_flow_conv_name(1, 1, dst, size, src, error);
		break;
	default:
		ret = rte_flow_error_set
		(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		 "unknown object conversion operation");
	}

	rte_flow_trace_conv(op, dst, size, src, ret);

	return ret;
}

/** Store a full rte_flow description. */
RTE_EXPORT_SYMBOL(rte_flow_copy)
size_t
rte_flow_copy(struct rte_flow_desc *desc, size_t len,
	      const struct rte_flow_attr *attr,
	      const struct rte_flow_item *items,
	      const struct rte_flow_action *actions)
{
	/*
	 * Overlap struct rte_flow_conv with struct rte_flow_desc in order
	 * to convert the former to the latter without wasting space.
	 */
	struct rte_flow_conv_rule *dst =
		len ?
		(void *)((uintptr_t)desc +
			 (offsetof(struct rte_flow_desc, actions) -
			  offsetof(struct rte_flow_conv_rule, actions))) :
		NULL;
	size_t dst_size =
		len > sizeof(*desc) - sizeof(*dst) ?
		len - (sizeof(*desc) - sizeof(*dst)) :
		0;
	struct rte_flow_conv_rule src = {
		.attr_ro = NULL,
		.pattern_ro = items,
		.actions_ro = actions,
	};
	int ret;

	RTE_BUILD_BUG_ON(sizeof(struct rte_flow_desc) <
			 sizeof(struct rte_flow_conv_rule));
	if (dst_size &&
	    (&dst->pattern != &desc->items ||
	     &dst->actions != &desc->actions ||
	     (uintptr_t)(dst + 1) != (uintptr_t)(desc + 1))) {
		rte_errno = EINVAL;
		return 0;
	}
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_RULE, dst, dst_size, &src, NULL);
	if (ret < 0)
		return 0;
	ret += sizeof(*desc) - sizeof(*dst);
	rte_memcpy(desc,
		   (&(struct rte_flow_desc){
			.size = ret,
			.attr = *attr,
			.items = dst_size ? dst->pattern : NULL,
			.actions = dst_size ? dst->actions : NULL,
		   }),
		   len > sizeof(*desc) ? sizeof(*desc) : len);

	rte_flow_trace_copy(desc, len, attr, items, actions, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_dev_dump, 20.02)
int
rte_flow_dev_dump(uint16_t port_id, struct rte_flow *flow,
			FILE *file, struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->dev_dump)) {
		fts_enter(dev);
		ret = ops->dev_dump(dev, flow, file, error);
		fts_exit(dev);
		return flow_err(port_id, ret, error);
	}
	return rte_flow_error_set(error, ENOSYS,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOSYS));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_get_aged_flows, 20.05)
int
rte_flow_get_aged_flows(uint16_t port_id, void **contexts,
		    uint32_t nb_contexts, struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->get_aged_flows)) {
		fts_enter(dev);
		ret = ops->get_aged_flows(dev, contexts, nb_contexts, error);
		fts_exit(dev);
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_get_aged_flows(port_id, contexts, nb_contexts, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_get_q_aged_flows, 22.11)
int
rte_flow_get_q_aged_flows(uint16_t port_id, uint32_t queue_id, void **contexts,
			  uint32_t nb_contexts, struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->get_q_aged_flows)) {
		fts_enter(dev);
		ret = ops->get_q_aged_flows(dev, queue_id, contexts,
					    nb_contexts, error);
		fts_exit(dev);
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_get_q_aged_flows(port_id, queue_id, contexts,
						nb_contexts, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_handle_create, 21.05)
struct rte_flow_action_handle *
rte_flow_action_handle_create(uint16_t port_id,
			      const struct rte_flow_indir_action_conf *conf,
			      const struct rte_flow_action *action,
			      struct rte_flow_error *error)
{
	struct rte_flow_action_handle *handle;
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return NULL;
	if (unlikely(!ops->action_handle_create)) {
		rte_flow_error_set(error, ENOSYS,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENOSYS));
		return NULL;
	}
	handle = ops->action_handle_create(&rte_eth_devices[port_id],
					   conf, action, error);
	if (handle == NULL)
		flow_err(port_id, -rte_errno, error);

	rte_flow_trace_action_handle_create(port_id, conf, action, handle);

	return handle;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_handle_destroy, 21.05)
int
rte_flow_action_handle_destroy(uint16_t port_id,
			       struct rte_flow_action_handle *handle,
			       struct rte_flow_error *error)
{
	int ret;
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (unlikely(!ops->action_handle_destroy))
		return rte_flow_error_set(error, ENOSYS,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, rte_strerror(ENOSYS));
	ret = ops->action_handle_destroy(&rte_eth_devices[port_id],
					 handle, error);
	ret = flow_err(port_id, ret, error);

	rte_flow_trace_action_handle_destroy(port_id, handle, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_handle_update, 21.05)
int
rte_flow_action_handle_update(uint16_t port_id,
			      struct rte_flow_action_handle *handle,
			      const void *update,
			      struct rte_flow_error *error)
{
	int ret;
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (unlikely(!ops->action_handle_update))
		return rte_flow_error_set(error, ENOSYS,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, rte_strerror(ENOSYS));
	ret = ops->action_handle_update(&rte_eth_devices[port_id], handle,
					update, error);
	ret = flow_err(port_id, ret, error);

	rte_flow_trace_action_handle_update(port_id, handle, update, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_handle_query, 21.05)
int
rte_flow_action_handle_query(uint16_t port_id,
			     const struct rte_flow_action_handle *handle,
			     void *data,
			     struct rte_flow_error *error)
{
	int ret;
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (unlikely(!ops->action_handle_query))
		return rte_flow_error_set(error, ENOSYS,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, rte_strerror(ENOSYS));
	ret = ops->action_handle_query(&rte_eth_devices[port_id], handle,
				       data, error);
	ret = flow_err(port_id, ret, error);

	rte_flow_trace_action_handle_query(port_id, handle, data, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_tunnel_decap_set, 20.11)
int
rte_flow_tunnel_decap_set(uint16_t port_id,
			  struct rte_flow_tunnel *tunnel,
			  struct rte_flow_action **actions,
			  uint32_t *num_of_actions,
			  struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->tunnel_decap_set)) {
		ret = flow_err(port_id,
			       ops->tunnel_decap_set(dev, tunnel, actions,
						     num_of_actions, error),
			       error);

		rte_flow_trace_tunnel_decap_set(port_id, tunnel, actions,
						num_of_actions, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_tunnel_match, 20.11)
int
rte_flow_tunnel_match(uint16_t port_id,
		      struct rte_flow_tunnel *tunnel,
		      struct rte_flow_item **items,
		      uint32_t *num_of_items,
		      struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->tunnel_match)) {
		ret = flow_err(port_id,
			       ops->tunnel_match(dev, tunnel, items,
						 num_of_items, error),
			       error);

		rte_flow_trace_tunnel_match(port_id, tunnel, items, num_of_items,
					    ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_get_restore_info, 20.11)
int
rte_flow_get_restore_info(uint16_t port_id,
			  struct rte_mbuf *m,
			  struct rte_flow_restore_info *restore_info,
			  struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->get_restore_info)) {
		ret = flow_err(port_id,
			       ops->get_restore_info(dev, m, restore_info,
						     error),
			       error);

		rte_flow_trace_get_restore_info(port_id, m, restore_info, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

static struct {
	const struct rte_mbuf_dynflag desc;
	uint64_t value;
} flow_restore_info_dynflag = {
	.desc = { .name = "RTE_MBUF_F_RX_RESTORE_INFO", },
};

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_restore_info_dynflag, 23.07)
uint64_t
rte_flow_restore_info_dynflag(void)
{
	return flow_restore_info_dynflag.value;
}

int
rte_flow_restore_info_dynflag_register(void)
{
	if (flow_restore_info_dynflag.value == 0) {
		int offset = rte_mbuf_dynflag_register(&flow_restore_info_dynflag.desc);

		if (offset < 0)
			return -1;
		flow_restore_info_dynflag.value = RTE_BIT64(offset);
	}

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_tunnel_action_decap_release, 20.11)
int
rte_flow_tunnel_action_decap_release(uint16_t port_id,
				     struct rte_flow_action *actions,
				     uint32_t num_of_actions,
				     struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->tunnel_action_decap_release)) {
		ret = flow_err(port_id,
			       ops->tunnel_action_decap_release(dev, actions,
								num_of_actions,
								error),
			       error);

		rte_flow_trace_tunnel_action_decap_release(port_id, actions,
							   num_of_actions, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_tunnel_item_release, 20.11)
int
rte_flow_tunnel_item_release(uint16_t port_id,
			     struct rte_flow_item *items,
			     uint32_t num_of_items,
			     struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->tunnel_item_release)) {
		ret = flow_err(port_id,
			       ops->tunnel_item_release(dev, items,
							num_of_items, error),
			       error);

		rte_flow_trace_tunnel_item_release(port_id, items, num_of_items, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_SYMBOL(rte_flow_pick_transfer_proxy)
int
rte_flow_pick_transfer_proxy(uint16_t port_id, uint16_t *proxy_port_id,
			     struct rte_flow_error *error)
{
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	struct rte_eth_dev *dev;
	int ret;

	if (unlikely(ops == NULL))
		return -rte_errno;

	if (ops->pick_transfer_proxy == NULL) {
		*proxy_port_id = port_id;
		return 0;
	}

	dev = &rte_eth_devices[port_id];

	ret = flow_err(port_id,
		       ops->pick_transfer_proxy(dev, proxy_port_id, error),
		       error);

	rte_flow_trace_pick_transfer_proxy(port_id, proxy_port_id, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_flex_item_create, 21.11)
struct rte_flow_item_flex_handle *
rte_flow_flex_item_create(uint16_t port_id,
			  const struct rte_flow_item_flex_conf *conf,
			  struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	struct rte_flow_item_flex_handle *handle;

	if (unlikely(!ops))
		return NULL;
	if (unlikely(!ops->flex_item_create)) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(ENOTSUP));
		return NULL;
	}
	handle = ops->flex_item_create(dev, conf, error);
	if (handle == NULL)
		flow_err(port_id, -rte_errno, error);

	rte_flow_trace_flex_item_create(port_id, conf, handle);

	return handle;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_flex_item_release, 21.11)
int
rte_flow_flex_item_release(uint16_t port_id,
			   const struct rte_flow_item_flex_handle *handle,
			   struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops || !ops->flex_item_release))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, rte_strerror(ENOTSUP));
	ret = ops->flex_item_release(dev, handle, error);
	ret = flow_err(port_id, ret, error);

	rte_flow_trace_flex_item_release(port_id, handle, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_info_get, 22.03)
int
rte_flow_info_get(uint16_t port_id,
		  struct rte_flow_port_info *port_info,
		  struct rte_flow_queue_info *queue_info,
		  struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (dev->data->dev_configured == 0) {
		FLOW_LOG(INFO,
			"Device with port_id=%"PRIu16" is not configured.",
			port_id);
		return -EINVAL;
	}
	if (port_info == NULL) {
		FLOW_LOG(ERR, "Port %"PRIu16" info is NULL.", port_id);
		return -EINVAL;
	}
	if (likely(!!ops->info_get)) {
		ret = flow_err(port_id,
			       ops->info_get(dev, port_info, queue_info, error),
			       error);

		rte_flow_trace_info_get(port_id, port_info, queue_info, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_configure, 22.03)
int
rte_flow_configure(uint16_t port_id,
		   const struct rte_flow_port_attr *port_attr,
		   uint16_t nb_queue,
		   const struct rte_flow_queue_attr *queue_attr[],
		   struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (dev->data->dev_configured == 0) {
		FLOW_LOG(INFO,
			"Device with port_id=%"PRIu16" is not configured.",
			port_id);
		goto error;
	}
	if (dev->data->dev_started != 0) {
		FLOW_LOG(INFO,
			"Device with port_id=%"PRIu16" already started.",
			port_id);
		goto error;
	}
	if (port_attr == NULL) {
		FLOW_LOG(ERR, "Port %"PRIu16" info is NULL.", port_id);
		goto error;
	}
	if (queue_attr == NULL) {
		FLOW_LOG(ERR, "Port %"PRIu16" queue info is NULL.", port_id);
		goto error;
	}
	if ((port_attr->flags & RTE_FLOW_PORT_FLAG_SHARE_INDIRECT) &&
	     !rte_eth_dev_is_valid_port(port_attr->host_port_id)) {
		return rte_flow_error_set(error, ENODEV,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, rte_strerror(ENODEV));
	}
	if (likely(!!ops->configure)) {
		ret = ops->configure(dev, port_attr, nb_queue, queue_attr, error);
		if (ret == 0)
			dev->data->flow_configured = 1;
		ret = flow_err(port_id, ret, error);

		rte_flow_trace_configure(port_id, port_attr, nb_queue, queue_attr, ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
error:
	return rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(EINVAL));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_pattern_template_create, 22.03)
struct rte_flow_pattern_template *
rte_flow_pattern_template_create(uint16_t port_id,
		const struct rte_flow_pattern_template_attr *template_attr,
		const struct rte_flow_item pattern[],
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	struct rte_flow_pattern_template *template;

	if (unlikely(!ops))
		return NULL;
	if (dev->data->flow_configured == 0) {
		FLOW_LOG(INFO,
			"Flow engine on port_id=%"PRIu16" is not configured.",
			port_id);
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_STATE,
				NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (template_attr == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" template attr is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (pattern == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" pattern is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (likely(!!ops->pattern_template_create)) {
		template = ops->pattern_template_create(dev, template_attr,
							pattern, error);
		if (template == NULL)
			flow_err(port_id, -rte_errno, error);

		rte_flow_trace_pattern_template_create(port_id, template_attr,
						       pattern, template);

		return template;
	}
	rte_flow_error_set(error, ENOTSUP,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, rte_strerror(ENOTSUP));
	return NULL;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_pattern_template_destroy, 22.03)
int
rte_flow_pattern_template_destroy(uint16_t port_id,
		struct rte_flow_pattern_template *pattern_template,
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (unlikely(pattern_template == NULL))
		return 0;
	if (likely(!!ops->pattern_template_destroy)) {
		ret = flow_err(port_id,
			       ops->pattern_template_destroy(dev,
							     pattern_template,
							     error),
			       error);

		rte_flow_trace_pattern_template_destroy(port_id, pattern_template,
							ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_actions_template_create, 22.03)
struct rte_flow_actions_template *
rte_flow_actions_template_create(uint16_t port_id,
			const struct rte_flow_actions_template_attr *template_attr,
			const struct rte_flow_action actions[],
			const struct rte_flow_action masks[],
			struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	struct rte_flow_actions_template *template;

	if (unlikely(!ops))
		return NULL;
	if (dev->data->flow_configured == 0) {
		FLOW_LOG(INFO,
			"Flow engine on port_id=%"PRIu16" is not configured.",
			port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_STATE,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (template_attr == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" template attr is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (actions == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" actions is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (masks == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" masks is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));

	}
	if (likely(!!ops->actions_template_create)) {
		template = ops->actions_template_create(dev, template_attr,
							actions, masks, error);
		if (template == NULL)
			flow_err(port_id, -rte_errno, error);

		rte_flow_trace_actions_template_create(port_id, template_attr, actions,
						       masks, template);

		return template;
	}
	rte_flow_error_set(error, ENOTSUP,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, rte_strerror(ENOTSUP));
	return NULL;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_actions_template_destroy, 22.03)
int
rte_flow_actions_template_destroy(uint16_t port_id,
			struct rte_flow_actions_template *actions_template,
			struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (unlikely(actions_template == NULL))
		return 0;
	if (likely(!!ops->actions_template_destroy)) {
		ret = flow_err(port_id,
			       ops->actions_template_destroy(dev,
							     actions_template,
							     error),
			       error);

		rte_flow_trace_actions_template_destroy(port_id, actions_template,
							ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_template_table_create, 22.03)
struct rte_flow_template_table *
rte_flow_template_table_create(uint16_t port_id,
			const struct rte_flow_template_table_attr *table_attr,
			struct rte_flow_pattern_template *pattern_templates[],
			uint8_t nb_pattern_templates,
			struct rte_flow_actions_template *actions_templates[],
			uint8_t nb_actions_templates,
			struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	struct rte_flow_template_table *table;

	if (unlikely(!ops))
		return NULL;
	if (dev->data->flow_configured == 0) {
		FLOW_LOG(INFO,
			"Flow engine on port_id=%"PRIu16" is not configured.",
			port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_STATE,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (table_attr == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" table attr is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (pattern_templates == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" pattern templates is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (actions_templates == NULL) {
		FLOW_LOG(ERR,
			     "Port %"PRIu16" actions templates is NULL.",
			     port_id);
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, rte_strerror(EINVAL));
		return NULL;
	}
	if (likely(!!ops->template_table_create)) {
		table = ops->template_table_create(dev, table_attr,
					pattern_templates, nb_pattern_templates,
					actions_templates, nb_actions_templates,
					error);
		if (table == NULL)
			flow_err(port_id, -rte_errno, error);

		rte_flow_trace_template_table_create(port_id, table_attr,
						     pattern_templates,
						     nb_pattern_templates,
						     actions_templates,
						     nb_actions_templates, table);

		return table;
	}
	rte_flow_error_set(error, ENOTSUP,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, rte_strerror(ENOTSUP));
	return NULL;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_template_table_destroy, 22.03)
int
rte_flow_template_table_destroy(uint16_t port_id,
				struct rte_flow_template_table *template_table,
				struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);
	int ret;

	if (unlikely(!ops))
		return -rte_errno;
	if (unlikely(template_table == NULL))
		return 0;
	if (likely(!!ops->template_table_destroy)) {
		ret = flow_err(port_id,
			       ops->template_table_destroy(dev,
							   template_table,
							   error),
			       error);

		rte_flow_trace_template_table_destroy(port_id, template_table,
						      ret);

		return ret;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_group_set_miss_actions, 23.11)
int
rte_flow_group_set_miss_actions(uint16_t port_id,
				uint32_t group_id,
				const struct rte_flow_group_attr *attr,
				const struct rte_flow_action actions[],
				struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_flow_ops *ops = rte_flow_ops_get(port_id, error);

	if (unlikely(!ops))
		return -rte_errno;
	if (likely(!!ops->group_set_miss_actions)) {
		return flow_err(port_id,
				ops->group_set_miss_actions(dev, group_id, attr, actions, error),
				error);
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, rte_strerror(ENOTSUP));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_create, 22.03)
struct rte_flow *
rte_flow_async_create(uint16_t port_id,
		      uint32_t queue_id,
		      const struct rte_flow_op_attr *op_attr,
		      struct rte_flow_template_table *template_table,
		      const struct rte_flow_item pattern[],
		      uint8_t pattern_template_index,
		      const struct rte_flow_action actions[],
		      uint8_t actions_template_index,
		      void *user_data,
		      struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow *flow;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENODEV));
		return NULL;
	}
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_create == NULL) {
		rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENOSYS));
		return NULL;
	}
#endif

	flow = dev->flow_fp_ops->async_create(dev, queue_id,
					      op_attr, template_table,
					      pattern, pattern_template_index,
					      actions, actions_template_index,
					      user_data, error);

	rte_flow_trace_async_create(port_id, queue_id, op_attr, template_table,
				    pattern, pattern_template_index, actions,
				    actions_template_index, user_data, flow);

	return flow;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_create_by_index, 23.03)
struct rte_flow *
rte_flow_async_create_by_index(uint16_t port_id,
			       uint32_t queue_id,
			       const struct rte_flow_op_attr *op_attr,
			       struct rte_flow_template_table *template_table,
			       uint32_t rule_index,
			       const struct rte_flow_action actions[],
			       uint8_t actions_template_index,
			       void *user_data,
			       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow *flow;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENODEV));
		return NULL;
	}
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_create_by_index == NULL) {
		rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENOSYS));
		return NULL;
	}
#endif

	flow = dev->flow_fp_ops->async_create_by_index(dev, queue_id,
						       op_attr, template_table, rule_index,
						       actions, actions_template_index,
						       user_data, error);

	rte_flow_trace_async_create_by_index(port_id, queue_id, op_attr, template_table, rule_index,
					     actions, actions_template_index, user_data, flow);

	return flow;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_create_by_index_with_pattern, 24.11)
struct rte_flow *
rte_flow_async_create_by_index_with_pattern(uint16_t port_id,
					    uint32_t queue_id,
					    const struct rte_flow_op_attr *op_attr,
					    struct rte_flow_template_table *template_table,
					    uint32_t rule_index,
					    const struct rte_flow_item pattern[],
					    uint8_t pattern_template_index,
					    const struct rte_flow_action actions[],
					    uint8_t actions_template_index,
					    void *user_data,
					    struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow *flow;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENODEV));
		return NULL;
	}
	if (dev->flow_fp_ops == NULL ||
	    dev->flow_fp_ops->async_create_by_index_with_pattern == NULL) {
		rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENOSYS));
		return NULL;
	}
#endif

	flow = dev->flow_fp_ops->async_create_by_index_with_pattern(dev, queue_id, op_attr,
								    template_table, rule_index,
								    pattern, pattern_template_index,
								    actions, actions_template_index,
								    user_data, error);

	rte_flow_trace_async_create_by_index_with_pattern(port_id, queue_id, op_attr,
							  template_table, rule_index, pattern,
							  pattern_template_index, actions,
							  actions_template_index, user_data, flow);

	return flow;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_destroy, 22.03)
int
rte_flow_async_destroy(uint16_t port_id,
		       uint32_t queue_id,
		       const struct rte_flow_op_attr *op_attr,
		       struct rte_flow *flow,
		       void *user_data,
		       struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_destroy == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->async_destroy(dev, queue_id,
					      op_attr, flow,
					      user_data, error);

	rte_flow_trace_async_destroy(port_id, queue_id, op_attr, flow,
				     user_data, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_actions_update, 23.07)
int
rte_flow_async_actions_update(uint16_t port_id,
			      uint32_t queue_id,
			      const struct rte_flow_op_attr *op_attr,
			      struct rte_flow *flow,
			      const struct rte_flow_action actions[],
			      uint8_t actions_template_index,
			      void *user_data,
			      struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_actions_update == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->async_actions_update(dev, queue_id, op_attr,
						     flow, actions,
						     actions_template_index,
						     user_data, error);

	rte_flow_trace_async_actions_update(port_id, queue_id, op_attr, flow,
					    actions, actions_template_index,
					    user_data, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_push, 22.03)
int
rte_flow_push(uint16_t port_id,
	      uint32_t queue_id,
	      struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->push == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->push(dev, queue_id, error);

	rte_flow_trace_push(port_id, queue_id, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_pull, 22.03)
int
rte_flow_pull(uint16_t port_id,
	      uint32_t queue_id,
	      struct rte_flow_op_result res[],
	      uint16_t n_res,
	      struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->pull == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->pull(dev, queue_id, res, n_res, error);

	rte_flow_trace_pull(port_id, queue_id, res, n_res, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_handle_create, 22.03)
struct rte_flow_action_handle *
rte_flow_async_action_handle_create(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		const struct rte_flow_indir_action_conf *indir_action_conf,
		const struct rte_flow_action *action,
		void *user_data,
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow_action_handle *handle;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENODEV));
		return NULL;
	}
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_action_handle_create == NULL) {
		rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENOSYS));
		return NULL;
	}
#endif

	handle = dev->flow_fp_ops->async_action_handle_create(dev, queue_id, op_attr,
							      indir_action_conf, action,
							      user_data, error);

	rte_flow_trace_async_action_handle_create(port_id, queue_id, op_attr,
						  indir_action_conf, action,
						  user_data, handle);

	return handle;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_handle_destroy, 22.03)
int
rte_flow_async_action_handle_destroy(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		struct rte_flow_action_handle *action_handle,
		void *user_data,
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_action_handle_destroy == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->async_action_handle_destroy(dev, queue_id, op_attr,
							    action_handle, user_data, error);

	rte_flow_trace_async_action_handle_destroy(port_id, queue_id, op_attr,
						   action_handle, user_data, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_handle_update, 22.03)
int
rte_flow_async_action_handle_update(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		struct rte_flow_action_handle *action_handle,
		const void *update,
		void *user_data,
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_action_handle_update == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->async_action_handle_update(dev, queue_id, op_attr,
							   action_handle, update, user_data, error);

	rte_flow_trace_async_action_handle_update(port_id, queue_id, op_attr,
						  action_handle, update,
						  user_data, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_handle_query, 22.11)
int
rte_flow_async_action_handle_query(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		const struct rte_flow_action_handle *action_handle,
		void *data,
		void *user_data,
		struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_action_handle_query == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->async_action_handle_query(dev, queue_id, op_attr,
							  action_handle, data, user_data, error);

	rte_flow_trace_async_action_handle_query(port_id, queue_id, op_attr,
						 action_handle, data, user_data,
						 ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_handle_query_update, 23.03)
int
rte_flow_action_handle_query_update(uint16_t port_id,
				    struct rte_flow_action_handle *handle,
				    const void *update, void *query,
				    enum rte_flow_query_update_mode mode,
				    struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (!handle)
		return -EINVAL;
	if (!update && !query)
		return -EINVAL;
	dev = &rte_eth_devices[port_id];
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->action_handle_query_update)
		return -ENOTSUP;
	ret = ops->action_handle_query_update(dev, handle, update,
					      query, mode, error);
	return flow_err(port_id, ret, error);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_handle_query_update, 23.03)
int
rte_flow_async_action_handle_query_update(uint16_t port_id, uint32_t queue_id,
					  const struct rte_flow_op_attr *attr,
					  struct rte_flow_action_handle *handle,
					  const void *update, void *query,
					  enum rte_flow_query_update_mode mode,
					  void *user_data,
					  struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_action_handle_query_update == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	return dev->flow_fp_ops->async_action_handle_query_update(dev, queue_id, attr,
								  handle, update,
								  query, mode,
								  user_data, error);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_list_handle_create, 23.07)
struct rte_flow_action_list_handle *
rte_flow_action_list_handle_create(uint16_t port_id,
				   const
				   struct rte_flow_indir_action_conf *conf,
				   const struct rte_flow_action *actions,
				   struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;
	struct rte_flow_action_list_handle *handle;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, NULL);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->action_list_handle_create) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "action_list handle not supported");
		return NULL;
	}
	dev = &rte_eth_devices[port_id];
	handle = ops->action_list_handle_create(dev, conf, actions, error);
	ret = flow_err(port_id, -rte_errno, error);
	rte_flow_trace_action_list_handle_create(port_id, conf, actions, ret);
	return handle;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_list_handle_destroy, 23.07)
int
rte_flow_action_list_handle_destroy(uint16_t port_id,
				    struct rte_flow_action_list_handle *handle,
				    struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->action_list_handle_destroy)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "action_list handle not supported");
	dev = &rte_eth_devices[port_id];
	ret = ops->action_list_handle_destroy(dev, handle, error);
	ret = flow_err(port_id, ret, error);
	rte_flow_trace_action_list_handle_destroy(port_id, handle, ret);
	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_list_handle_create, 23.07)
struct rte_flow_action_list_handle *
rte_flow_async_action_list_handle_create(uint16_t port_id, uint32_t queue_id,
					 const struct rte_flow_op_attr *attr,
					 const struct rte_flow_indir_action_conf *conf,
					 const struct rte_flow_action *actions,
					 void *user_data,
					 struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow_action_list_handle *handle;
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENODEV));
		return NULL;
	}
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_action_list_handle_create == NULL) {
		rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   rte_strerror(ENOSYS));
		return NULL;
	}
#endif

	handle = dev->flow_fp_ops->async_action_list_handle_create(dev, queue_id, attr, conf,
								   actions, user_data,
								   error);
	ret = flow_err(port_id, -rte_errno, error);

	rte_flow_trace_async_action_list_handle_create(port_id, queue_id, attr,
						       conf, actions, user_data,
						       ret);
	return handle;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_list_handle_destroy, 23.07)
int
rte_flow_async_action_list_handle_destroy(uint16_t port_id, uint32_t queue_id,
				 const struct rte_flow_op_attr *op_attr,
				 struct rte_flow_action_list_handle *handle,
				 void *user_data, struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL || dev->flow_fp_ops->async_action_list_handle_destroy == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->async_action_list_handle_destroy(dev, queue_id, op_attr,
								 handle, user_data, error);

	rte_flow_trace_async_action_list_handle_destroy(port_id, queue_id,
							op_attr, handle,
							user_data, ret);
	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_action_list_handle_query_update, 23.07)
int
rte_flow_action_list_handle_query_update(uint16_t port_id,
			 const struct rte_flow_action_list_handle *handle,
			 const void **update, void **query,
			 enum rte_flow_query_update_mode mode,
			 struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->action_list_handle_query_update)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "action_list query_update not supported");
	dev = &rte_eth_devices[port_id];
	ret = ops->action_list_handle_query_update(dev, handle, update, query,
						   mode, error);
	ret = flow_err(port_id, ret, error);
	rte_flow_trace_action_list_handle_query_update(port_id, handle, update,
						       query, mode, ret);
	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_action_list_handle_query_update, 23.07)
int
rte_flow_async_action_list_handle_query_update(uint16_t port_id, uint32_t queue_id,
			 const struct rte_flow_op_attr *attr,
			 const struct rte_flow_action_list_handle *handle,
			 const void **update, void **query,
			 enum rte_flow_query_update_mode mode,
			 void *user_data, struct rte_flow_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

#ifdef RTE_FLOW_DEBUG
	if (!rte_eth_dev_is_valid_port(port_id))
		return rte_flow_error_set(error, ENODEV, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENODEV));
	if (dev->flow_fp_ops == NULL ||
	    dev->flow_fp_ops->async_action_list_handle_query_update == NULL)
		return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  rte_strerror(ENOSYS));
#endif

	ret = dev->flow_fp_ops->async_action_list_handle_query_update(dev, queue_id, attr,
								      handle, update, query,
								      mode, user_data,
								      error);

	rte_flow_trace_async_action_list_handle_query_update(port_id, queue_id,
							     attr, handle,
							     update, query,
							     mode, user_data,
							     ret);
	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_calc_table_hash, 23.11)
int
rte_flow_calc_table_hash(uint16_t port_id, const struct rte_flow_template_table *table,
			 const struct rte_flow_item pattern[], uint8_t pattern_template_index,
			 uint32_t *hash, struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->flow_calc_table_hash)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "action_list async query_update not supported");
	dev = &rte_eth_devices[port_id];
	ret = ops->flow_calc_table_hash(dev, table, pattern, pattern_template_index,
					hash, error);
	return flow_err(port_id, ret, error);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_calc_encap_hash, 24.03)
int
rte_flow_calc_encap_hash(uint16_t port_id, const struct rte_flow_item pattern[],
			 enum rte_flow_encap_hash_field dest_field, uint8_t hash_len,
			 uint8_t *hash, struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->flow_calc_encap_hash)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "calc encap hash is not supported");
	if (dest_field > RTE_FLOW_ENCAP_HASH_FIELD_NVGRE_FLOW_ID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "hash dest field is not defined");
	if ((dest_field == RTE_FLOW_ENCAP_HASH_FIELD_SRC_PORT && hash_len != 2) ||
	    (dest_field == RTE_FLOW_ENCAP_HASH_FIELD_NVGRE_FLOW_ID && hash_len != 1))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "hash len doesn't match the requested field len");
	dev = &rte_eth_devices[port_id];
	ret = ops->flow_calc_encap_hash(dev, pattern, dest_field, hash, error);
	return flow_err(port_id, ret, error);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_template_table_resizable, 24.03)
bool
rte_flow_template_table_resizable(__rte_unused uint16_t port_id,
				  const struct rte_flow_template_table_attr *tbl_attr)
{
	return (tbl_attr->specialize &
		RTE_FLOW_TABLE_SPECIALIZE_RESIZABLE) != 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_template_table_resize, 24.03)
int
rte_flow_template_table_resize(uint16_t port_id,
			       struct rte_flow_template_table *table,
			       uint32_t nb_rules,
			       struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->flow_template_table_resize)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "flow_template_table_resize not supported");
	dev = &rte_eth_devices[port_id];
	ret = ops->flow_template_table_resize(dev, table, nb_rules, error);
	ret = flow_err(port_id, ret, error);
	rte_flow_trace_template_table_resize(port_id, table, nb_rules, ret);
	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_async_update_resized, 24.03)
int
rte_flow_async_update_resized(uint16_t port_id, uint32_t queue,
			      const struct rte_flow_op_attr *attr,
			      struct rte_flow *rule, void *user_data,
			      struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->flow_update_resized)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "async_flow_async_transfer not supported");
	dev = &rte_eth_devices[port_id];
	ret = ops->flow_update_resized(dev, queue, attr, rule, user_data, error);
	ret = flow_err(port_id, ret, error);
	rte_flow_trace_async_update_resized(port_id, queue, attr,
					    rule, user_data, ret);
	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_template_table_resize_complete, 24.03)
int
rte_flow_template_table_resize_complete(uint16_t port_id,
					struct rte_flow_template_table *table,
					struct rte_flow_error *error)
{
	int ret;
	struct rte_eth_dev *dev;
	const struct rte_flow_ops *ops;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ops = rte_flow_ops_get(port_id, error);
	if (!ops || !ops->flow_template_table_resize_complete)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "flow_template_table_transfer_complete not supported");
	dev = &rte_eth_devices[port_id];
	ret = ops->flow_template_table_resize_complete(dev, table, error);
	ret = flow_err(port_id, ret, error);
	rte_flow_trace_table_resize_complete(port_id, table, ret);
	return ret;
}

static struct rte_flow *
rte_flow_dummy_async_create(struct rte_eth_dev *dev __rte_unused,
			    uint32_t queue __rte_unused,
			    const struct rte_flow_op_attr *attr __rte_unused,
			    struct rte_flow_template_table *table __rte_unused,
			    const struct rte_flow_item items[] __rte_unused,
			    uint8_t pattern_template_index __rte_unused,
			    const struct rte_flow_action actions[] __rte_unused,
			    uint8_t action_template_index __rte_unused,
			    void *user_data __rte_unused,
			    struct rte_flow_error *error)
{
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   rte_strerror(ENOSYS));
	return NULL;
}

static struct rte_flow *
rte_flow_dummy_async_create_by_index(struct rte_eth_dev *dev __rte_unused,
				     uint32_t queue __rte_unused,
				     const struct rte_flow_op_attr *attr __rte_unused,
				     struct rte_flow_template_table *table __rte_unused,
				     uint32_t rule_index __rte_unused,
				     const struct rte_flow_action actions[] __rte_unused,
				     uint8_t action_template_index __rte_unused,
				     void *user_data __rte_unused,
				     struct rte_flow_error *error)
{
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   rte_strerror(ENOSYS));
	return NULL;
}

static struct rte_flow *
rte_flow_dummy_async_create_by_index_with_pattern(struct rte_eth_dev *dev __rte_unused,
						uint32_t queue __rte_unused,
						const struct rte_flow_op_attr *attr __rte_unused,
						struct rte_flow_template_table *table __rte_unused,
						uint32_t rule_index __rte_unused,
						const struct rte_flow_item items[] __rte_unused,
						uint8_t pattern_template_index __rte_unused,
						const struct rte_flow_action actions[] __rte_unused,
						uint8_t action_template_index __rte_unused,
						void *user_data __rte_unused,
						struct rte_flow_error *error)
{
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   rte_strerror(ENOSYS));
	return NULL;
}

static int
rte_flow_dummy_async_actions_update(struct rte_eth_dev *dev __rte_unused,
				    uint32_t queue_id __rte_unused,
				    const struct rte_flow_op_attr *op_attr __rte_unused,
				    struct rte_flow *flow __rte_unused,
				    const struct rte_flow_action actions[] __rte_unused,
				    uint8_t actions_template_index __rte_unused,
				    void *user_data __rte_unused,
				    struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static int
rte_flow_dummy_async_destroy(struct rte_eth_dev *dev __rte_unused,
			     uint32_t queue_id __rte_unused,
			     const struct rte_flow_op_attr *op_attr __rte_unused,
			     struct rte_flow *flow __rte_unused,
			     void *user_data __rte_unused,
			     struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static int
rte_flow_dummy_push(struct rte_eth_dev *dev __rte_unused,
		    uint32_t queue_id __rte_unused,
		    struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static int
rte_flow_dummy_pull(struct rte_eth_dev *dev __rte_unused,
		    uint32_t queue_id __rte_unused,
		    struct rte_flow_op_result res[] __rte_unused,
		    uint16_t n_res __rte_unused,
		    struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static struct rte_flow_action_handle *
rte_flow_dummy_async_action_handle_create(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *op_attr __rte_unused,
	const struct rte_flow_indir_action_conf *indir_action_conf __rte_unused,
	const struct rte_flow_action *action __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   rte_strerror(ENOSYS));
	return NULL;
}

static int
rte_flow_dummy_async_action_handle_destroy(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *op_attr __rte_unused,
	struct rte_flow_action_handle *action_handle __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static int
rte_flow_dummy_async_action_handle_update(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *op_attr __rte_unused,
	struct rte_flow_action_handle *action_handle __rte_unused,
	const void *update __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static int
rte_flow_dummy_async_action_handle_query(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *op_attr __rte_unused,
	const struct rte_flow_action_handle *action_handle __rte_unused,
	void *data __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static int
rte_flow_dummy_async_action_handle_query_update(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *attr __rte_unused,
	struct rte_flow_action_handle *handle __rte_unused,
	const void *update __rte_unused,
	void *query __rte_unused,
	enum rte_flow_query_update_mode mode __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static struct rte_flow_action_list_handle *
rte_flow_dummy_async_action_list_handle_create(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *attr __rte_unused,
	const struct rte_flow_indir_action_conf *conf __rte_unused,
	const struct rte_flow_action *actions __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   rte_strerror(ENOSYS));
	return NULL;
}

static int
rte_flow_dummy_async_action_list_handle_destroy(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *op_attr __rte_unused,
	struct rte_flow_action_list_handle *handle __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

static int
rte_flow_dummy_async_action_list_handle_query_update(
	struct rte_eth_dev *dev __rte_unused,
	uint32_t queue_id __rte_unused,
	const struct rte_flow_op_attr *attr __rte_unused,
	const struct rte_flow_action_list_handle *handle __rte_unused,
	const void **update __rte_unused,
	void **query __rte_unused,
	enum rte_flow_query_update_mode mode __rte_unused,
	void *user_data __rte_unused,
	struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOSYS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  rte_strerror(ENOSYS));
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_flow_fp_default_ops)
struct rte_flow_fp_ops rte_flow_fp_default_ops = {
	.async_create = rte_flow_dummy_async_create,
	.async_create_by_index = rte_flow_dummy_async_create_by_index,
	.async_actions_update = rte_flow_dummy_async_actions_update,
	.async_create_by_index_with_pattern = rte_flow_dummy_async_create_by_index_with_pattern,
	.async_destroy = rte_flow_dummy_async_destroy,
	.push = rte_flow_dummy_push,
	.pull = rte_flow_dummy_pull,
	.async_action_handle_create = rte_flow_dummy_async_action_handle_create,
	.async_action_handle_destroy = rte_flow_dummy_async_action_handle_destroy,
	.async_action_handle_update = rte_flow_dummy_async_action_handle_update,
	.async_action_handle_query = rte_flow_dummy_async_action_handle_query,
	.async_action_handle_query_update = rte_flow_dummy_async_action_handle_query_update,
	.async_action_list_handle_create = rte_flow_dummy_async_action_list_handle_create,
	.async_action_list_handle_destroy = rte_flow_dummy_async_action_list_handle_destroy,
	.async_action_list_handle_query_update =
		rte_flow_dummy_async_action_list_handle_query_update,
};
