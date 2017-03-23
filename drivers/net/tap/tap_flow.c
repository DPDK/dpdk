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

#include <sys/queue.h>

#include <rte_byteorder.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <rte_eth_tap.h>
#include <tap_flow.h>
#include <tap_autoconf.h>
#include <tap_tcmsgs.h>

#ifndef HAVE_TC_FLOWER
/*
 * For kernels < 4.2, this enum is not defined. Runtime checks will be made to
 * avoid sending TC messages the kernel cannot understand.
 */
enum {
	TCA_FLOWER_UNSPEC,
	TCA_FLOWER_CLASSID,
	TCA_FLOWER_INDEV,
	TCA_FLOWER_ACT,
	TCA_FLOWER_KEY_ETH_DST,         /* ETH_ALEN */
	TCA_FLOWER_KEY_ETH_DST_MASK,    /* ETH_ALEN */
	TCA_FLOWER_KEY_ETH_SRC,         /* ETH_ALEN */
	TCA_FLOWER_KEY_ETH_SRC_MASK,    /* ETH_ALEN */
	TCA_FLOWER_KEY_ETH_TYPE,        /* be16 */
	TCA_FLOWER_KEY_IP_PROTO,        /* u8 */
	TCA_FLOWER_KEY_IPV4_SRC,        /* be32 */
	TCA_FLOWER_KEY_IPV4_SRC_MASK,   /* be32 */
	TCA_FLOWER_KEY_IPV4_DST,        /* be32 */
	TCA_FLOWER_KEY_IPV4_DST_MASK,   /* be32 */
	TCA_FLOWER_KEY_IPV6_SRC,        /* struct in6_addr */
	TCA_FLOWER_KEY_IPV6_SRC_MASK,   /* struct in6_addr */
	TCA_FLOWER_KEY_IPV6_DST,        /* struct in6_addr */
	TCA_FLOWER_KEY_IPV6_DST_MASK,   /* struct in6_addr */
	TCA_FLOWER_KEY_TCP_SRC,         /* be16 */
	TCA_FLOWER_KEY_TCP_DST,         /* be16 */
	TCA_FLOWER_KEY_UDP_SRC,         /* be16 */
	TCA_FLOWER_KEY_UDP_DST,         /* be16 */
};
#endif
#ifndef HAVE_TC_VLAN_ID
enum {
	/* TCA_FLOWER_FLAGS, */
	TCA_FLOWER_KEY_VLAN_ID = TCA_FLOWER_KEY_UDP_DST + 2, /* be16 */
	TCA_FLOWER_KEY_VLAN_PRIO,       /* u8   */
	TCA_FLOWER_KEY_VLAN_ETH_TYPE,   /* be16 */
};
#endif

struct rte_flow {
	LIST_ENTRY(rte_flow) next; /* Pointer to the next rte_flow structure */
	struct nlmsg msg;
};

struct convert_data {
	uint16_t eth_type;
	uint16_t ip_proto;
	uint8_t vlan;
	struct rte_flow *flow;
};

static int tap_flow_create_eth(const struct rte_flow_item *item, void *data);
static int tap_flow_create_vlan(const struct rte_flow_item *item, void *data);
static int tap_flow_create_ipv4(const struct rte_flow_item *item, void *data);
static int tap_flow_create_ipv6(const struct rte_flow_item *item, void *data);
static int tap_flow_create_udp(const struct rte_flow_item *item, void *data);
static int tap_flow_create_tcp(const struct rte_flow_item *item, void *data);
static int
tap_flow_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error);

static struct rte_flow *
tap_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);

static int
tap_flow_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error);

static const struct rte_flow_ops tap_flow_ops = {
	.validate = tap_flow_validate,
	.create = tap_flow_create,
	.destroy = tap_flow_destroy,
	.flush = tap_flow_flush,
};

/* Static initializer for items. */
#define ITEMS(...) \
	(const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	}

/* Structure to generate a simple graph of layers supported by the NIC. */
struct tap_flow_items {
	/* Bit-mask corresponding to what is supported for this item. */
	const void *mask;
	const unsigned int mask_sz; /* Bit-mask size in bytes. */
	/*
	 * Bit-mask corresponding to the default mask, if none is provided
	 * along with the item.
	 */
	const void *default_mask;
	/**
	 * Conversion function from rte_flow to netlink attributes.
	 *
	 * @param item
	 *   rte_flow item to convert.
	 * @param data
	 *   Internal structure to store the conversion.
	 *
	 * @return
	 *   0 on success, negative value otherwise.
	 */
	int (*convert)(const struct rte_flow_item *item, void *data);
	/** List of possible following items.  */
	const enum rte_flow_item_type *const items;
};

/* Graph of supported items and associated actions. */
static const struct tap_flow_items tap_flow_items[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_ETH),
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.items = ITEMS(
			RTE_FLOW_ITEM_TYPE_VLAN,
			RTE_FLOW_ITEM_TYPE_IPV4,
			RTE_FLOW_ITEM_TYPE_IPV6),
		.mask = &(const struct rte_flow_item_eth){
			.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.type = -1,
		},
		.mask_sz = sizeof(struct rte_flow_item_eth),
		.default_mask = &rte_flow_item_eth_mask,
		.convert = tap_flow_create_eth,
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6),
		.mask = &(const struct rte_flow_item_vlan){
			.tpid = -1,
			/* DEI matching is not supported */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
			.tci = 0xffef,
#else
			.tci = 0xefff,
#endif
		},
		.mask_sz = sizeof(struct rte_flow_item_vlan),
		.default_mask = &rte_flow_item_vlan_mask,
		.convert = tap_flow_create_vlan,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_TCP),
		.mask = &(const struct rte_flow_item_ipv4){
			.hdr = {
				.src_addr = -1,
				.dst_addr = -1,
				.next_proto_id = -1,
			},
		},
		.mask_sz = sizeof(struct rte_flow_item_ipv4),
		.default_mask = &rte_flow_item_ipv4_mask,
		.convert = tap_flow_create_ipv4,
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.items = ITEMS(RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_TCP),
		.mask = &(const struct rte_flow_item_ipv6){
			.hdr = {
				.src_addr = {
					"\xff\xff\xff\xff\xff\xff\xff\xff"
					"\xff\xff\xff\xff\xff\xff\xff\xff",
				},
				.dst_addr = {
					"\xff\xff\xff\xff\xff\xff\xff\xff"
					"\xff\xff\xff\xff\xff\xff\xff\xff",
				},
				.proto = -1,
			},
		},
		.mask_sz = sizeof(struct rte_flow_item_ipv6),
		.default_mask = &rte_flow_item_ipv6_mask,
		.convert = tap_flow_create_ipv6,
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.mask = &(const struct rte_flow_item_udp){
			.hdr = {
				.src_port = -1,
				.dst_port = -1,
			},
		},
		.mask_sz = sizeof(struct rte_flow_item_udp),
		.default_mask = &rte_flow_item_udp_mask,
		.convert = tap_flow_create_udp,
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.mask = &(const struct rte_flow_item_tcp){
			.hdr = {
				.src_port = -1,
				.dst_port = -1,
			},
		},
		.mask_sz = sizeof(struct rte_flow_item_tcp),
		.default_mask = &rte_flow_item_tcp_mask,
		.convert = tap_flow_create_tcp,
	},
};

/**
 * Make as much checks as possible on an Ethernet item, and if a flow is
 * provided, fill it appropriately with Ethernet info.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] data
 *   Additional data structure to tell next layers we've been here.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
tap_flow_create_eth(const struct rte_flow_item *item, void *data)
{
	struct convert_data *info = (struct convert_data *)data;
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct rte_flow *flow = info->flow;
	struct nlmsg *msg;

	/* use default mask if none provided */
	if (!mask)
		mask = tap_flow_items[RTE_FLOW_ITEM_TYPE_ETH].default_mask;
	/* TC does not support eth_type masking. Only accept if exact match. */
	if (mask->type && mask->type != 0xffff)
		return -1;
	if (!spec)
		return 0;
	/* store eth_type for consistency if ipv4/6 pattern item comes next */
	if (spec->type & mask->type)
		info->eth_type = spec->type;
	if (!flow)
		return 0;
	msg = &flow->msg;
	if (spec->type & mask->type)
		msg->t.tcm_info = TC_H_MAKE(msg->t.tcm_info,
					    (spec->type & mask->type));
	if (!is_zero_ether_addr(&spec->dst)) {
		nlattr_add(&msg->nh, TCA_FLOWER_KEY_ETH_DST, ETHER_ADDR_LEN,
			   &spec->dst.addr_bytes);
		nlattr_add(&msg->nh,
			   TCA_FLOWER_KEY_ETH_DST_MASK, ETHER_ADDR_LEN,
			   &mask->dst.addr_bytes);
	}
	if (!is_zero_ether_addr(&mask->src)) {
		nlattr_add(&msg->nh, TCA_FLOWER_KEY_ETH_SRC, ETHER_ADDR_LEN,
			   &spec->src.addr_bytes);
		nlattr_add(&msg->nh,
			   TCA_FLOWER_KEY_ETH_SRC_MASK, ETHER_ADDR_LEN,
			   &mask->src.addr_bytes);
	}
	return 0;
}

/**
 * Make as much checks as possible on a VLAN item, and if a flow is provided,
 * fill it appropriately with VLAN info.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] data
 *   Additional data structure to tell next layers we've been here.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
tap_flow_create_vlan(const struct rte_flow_item *item, void *data)
{
	struct convert_data *info = (struct convert_data *)data;
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	struct rte_flow *flow = info->flow;
	struct nlmsg *msg;

	/* use default mask if none provided */
	if (!mask)
		mask = tap_flow_items[RTE_FLOW_ITEM_TYPE_VLAN].default_mask;
	/* TC does not support tpid masking. Only accept if exact match. */
	if (mask->tpid && mask->tpid != 0xffff)
		return -1;
	/* Double-tagging not supported. */
	if (spec && mask->tpid && spec->tpid != htons(ETH_P_8021Q))
		return -1;
	info->vlan = 1;
	if (!flow)
		return 0;
	msg = &flow->msg;
	msg->t.tcm_info = TC_H_MAKE(msg->t.tcm_info, htons(ETH_P_8021Q));
#define VLAN_PRIO(tci) ((tci) >> 13)
#define VLAN_ID(tci) ((tci) & 0xfff)
	if (!spec)
		return 0;
	if (spec->tci) {
		uint16_t tci = ntohs(spec->tci) & mask->tci;
		uint16_t prio = VLAN_PRIO(tci);
		uint8_t vid = VLAN_ID(tci);

		if (prio)
			nlattr_add8(&msg->nh, TCA_FLOWER_KEY_VLAN_PRIO, prio);
		if (vid)
			nlattr_add16(&msg->nh, TCA_FLOWER_KEY_VLAN_ID, vid);
	}
	return 0;
}

/**
 * Make as much checks as possible on an IPv4 item, and if a flow is provided,
 * fill it appropriately with IPv4 info.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] data
 *   Additional data structure to tell next layers we've been here.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
tap_flow_create_ipv4(const struct rte_flow_item *item, void *data)
{
	struct convert_data *info = (struct convert_data *)data;
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct rte_flow *flow = info->flow;
	struct nlmsg *msg;

	/* use default mask if none provided */
	if (!mask)
		mask = tap_flow_items[RTE_FLOW_ITEM_TYPE_IPV4].default_mask;
	/* check that previous eth type is compatible with ipv4 */
	if (info->eth_type && info->eth_type != htons(ETH_P_IP))
		return -1;
	/* store ip_proto for consistency if udp/tcp pattern item comes next */
	if (spec)
		info->ip_proto = spec->hdr.next_proto_id;
	if (!flow)
		return 0;
	msg = &flow->msg;
	if (!info->eth_type)
		info->eth_type = htons(ETH_P_IP);
	if (!info->vlan)
		msg->t.tcm_info = TC_H_MAKE(msg->t.tcm_info, htons(ETH_P_IP));
	if (!spec)
		return 0;
	if (spec->hdr.dst_addr) {
		nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_DST,
			     spec->hdr.dst_addr);
		nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_DST_MASK,
			     mask->hdr.dst_addr);
	}
	if (spec->hdr.src_addr) {
		nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_SRC,
			     spec->hdr.src_addr);
		nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_SRC_MASK,
			     mask->hdr.src_addr);
	}
	if (spec->hdr.next_proto_id)
		nlattr_add8(&msg->nh, TCA_FLOWER_KEY_IP_PROTO,
			    spec->hdr.next_proto_id);
	return 0;
}

/**
 * Make as much checks as possible on an IPv6 item, and if a flow is provided,
 * fill it appropriately with IPv6 info.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] data
 *   Additional data structure to tell next layers we've been here.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
tap_flow_create_ipv6(const struct rte_flow_item *item, void *data)
{
	struct convert_data *info = (struct convert_data *)data;
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	struct rte_flow *flow = info->flow;
	uint8_t empty_addr[16] = { 0 };
	struct nlmsg *msg;

	/* use default mask if none provided */
	if (!mask)
		mask = tap_flow_items[RTE_FLOW_ITEM_TYPE_IPV6].default_mask;
	/* check that previous eth type is compatible with ipv6 */
	if (info->eth_type && info->eth_type != htons(ETH_P_IPV6))
		return -1;
	/* store ip_proto for consistency if udp/tcp pattern item comes next */
	if (spec)
		info->ip_proto = spec->hdr.proto;
	if (!flow)
		return 0;
	msg = &flow->msg;
	if (!info->eth_type)
		info->eth_type = htons(ETH_P_IPV6);
	if (!info->vlan)
		msg->t.tcm_info = TC_H_MAKE(msg->t.tcm_info, htons(ETH_P_IPV6));
	if (!spec)
		return 0;
	if (memcmp(spec->hdr.dst_addr, empty_addr, 16)) {
		nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_DST,
			   sizeof(spec->hdr.dst_addr), &spec->hdr.dst_addr);
		nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_DST_MASK,
			   sizeof(mask->hdr.dst_addr), &mask->hdr.dst_addr);
	}
	if (memcmp(spec->hdr.src_addr, empty_addr, 16)) {
		nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_SRC,
			   sizeof(spec->hdr.src_addr), &spec->hdr.src_addr);
		nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_SRC_MASK,
			   sizeof(mask->hdr.src_addr), &mask->hdr.src_addr);
	}
	if (spec->hdr.proto)
		nlattr_add8(&msg->nh, TCA_FLOWER_KEY_IP_PROTO, spec->hdr.proto);
	return 0;
}

/**
 * Make as much checks as possible on a UDP item, and if a flow is provided,
 * fill it appropriately with UDP info.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] data
 *   Additional data structure to tell next layers we've been here.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
tap_flow_create_udp(const struct rte_flow_item *item, void *data)
{
	struct convert_data *info = (struct convert_data *)data;
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct rte_flow *flow = info->flow;
	struct nlmsg *msg;

	/* use default mask if none provided */
	if (!mask)
		mask = tap_flow_items[RTE_FLOW_ITEM_TYPE_UDP].default_mask;
	/* check that previous ip_proto is compatible with udp */
	if (info->ip_proto && info->ip_proto != IPPROTO_UDP)
		return -1;
	if (!flow)
		return 0;
	msg = &flow->msg;
	nlattr_add8(&msg->nh, TCA_FLOWER_KEY_IP_PROTO, IPPROTO_UDP);
	if (!spec)
		return 0;
	if (spec->hdr.dst_port &&
	    (spec->hdr.dst_port & mask->hdr.dst_port) == spec->hdr.dst_port)
		nlattr_add16(&msg->nh, TCA_FLOWER_KEY_UDP_DST,
			     spec->hdr.dst_port);
	if (spec->hdr.src_port &&
	    (spec->hdr.src_port & mask->hdr.src_port) == spec->hdr.src_port)
		nlattr_add16(&msg->nh, TCA_FLOWER_KEY_UDP_SRC,
			     spec->hdr.src_port);
	return 0;
}

/**
 * Make as much checks as possible on a TCP item, and if a flow is provided,
 * fill it appropriately with TCP info.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] data
 *   Additional data structure to tell next layers we've been here.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
tap_flow_create_tcp(const struct rte_flow_item *item, void *data)
{
	struct convert_data *info = (struct convert_data *)data;
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct rte_flow *flow = info->flow;
	struct nlmsg *msg;

	/* use default mask if none provided */
	if (!mask)
		mask = tap_flow_items[RTE_FLOW_ITEM_TYPE_TCP].default_mask;
	/* check that previous ip_proto is compatible with tcp */
	if (info->ip_proto && info->ip_proto != IPPROTO_TCP)
		return -1;
	if (!flow)
		return 0;
	msg = &flow->msg;
	nlattr_add8(&msg->nh, TCA_FLOWER_KEY_IP_PROTO, IPPROTO_TCP);
	if (!spec)
		return 0;
	if (spec->hdr.dst_port &&
	    (spec->hdr.dst_port & mask->hdr.dst_port) == spec->hdr.dst_port)
		nlattr_add16(&msg->nh, TCA_FLOWER_KEY_TCP_DST,
			     spec->hdr.dst_port);
	if (spec->hdr.src_port &&
	    (spec->hdr.src_port & mask->hdr.src_port) == spec->hdr.src_port)
		nlattr_add16(&msg->nh, TCA_FLOWER_KEY_TCP_SRC,
			     spec->hdr.src_port);
	return 0;
}

/**
 * Check support for a given item.
 *
 * @param[in] item
 *   Item specification.
 * @param size
 *   Bit-Mask size in bytes.
 * @param[in] supported_mask
 *   Bit-mask covering supported fields to compare with spec, last and mask in
 *   \item.
 * @param[in] default_mask
 *   Bit-mask default mask if none is provided in \item.
 *
 * @return
 *   0 on success.
 */
static int
tap_flow_item_validate(const struct rte_flow_item *item,
		       unsigned int size,
		       const uint8_t *supported_mask,
		       const uint8_t *default_mask)
{
	int ret = 0;

	/* An empty layer is allowed, as long as all fields are NULL */
	if (!item->spec && (item->mask || item->last))
		return -1;
	/* Is the item spec compatible with what the NIC supports? */
	if (item->spec && !item->mask) {
		unsigned int i;
		const uint8_t *spec = item->spec;

		for (i = 0; i < size; ++i)
			if ((spec[i] | supported_mask[i]) != supported_mask[i])
				return -1;
		/* Is the default mask compatible with what the NIC supports? */
		for (i = 0; i < size; i++)
			if ((default_mask[i] | supported_mask[i]) !=
			    supported_mask[i])
				return -1;
	}
	/* Is the item last compatible with what the NIC supports? */
	if (item->last && !item->mask) {
		unsigned int i;
		const uint8_t *spec = item->last;

		for (i = 0; i < size; ++i)
			if ((spec[i] | supported_mask[i]) != supported_mask[i])
				return -1;
	}
	/* Is the item mask compatible with what the NIC supports? */
	if (item->mask) {
		unsigned int i;
		const uint8_t *spec = item->mask;

		for (i = 0; i < size; ++i)
			if ((spec[i] | supported_mask[i]) != supported_mask[i])
				return -1;
	}
	/**
	 * Once masked, Are item spec and item last equal?
	 * TC does not support range so anything else is invalid.
	 */
	if (item->spec && item->last) {
		uint8_t spec[size];
		uint8_t last[size];
		const uint8_t *apply = default_mask;
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
 * Transform a DROP/PASSTHRU action item in the provided flow for TC.
 *
 * @param[in, out] flow
 *   Flow to be filled.
 * @param[in] action
 *   Appropriate action to be set in the TCA_GACT_PARMS structure.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
add_action_gact(struct rte_flow *flow, int action)
{
	struct nlmsg *msg = &flow->msg;
	size_t act_index = 1;
	struct tc_gact p = {
		.action = action
	};

	if (nlattr_nested_start(msg, TCA_FLOWER_ACT) < 0)
		return -1;
	if (nlattr_nested_start(msg, act_index++) < 0)
		return -1;
	nlattr_add(&msg->nh, TCA_ACT_KIND, sizeof("gact"), "gact");
	if (nlattr_nested_start(msg, TCA_ACT_OPTIONS) < 0)
		return -1;
	nlattr_add(&msg->nh, TCA_GACT_PARMS, sizeof(p), &p);
	nlattr_nested_finish(msg); /* nested TCA_ACT_OPTIONS */
	nlattr_nested_finish(msg); /* nested act_index */
	nlattr_nested_finish(msg); /* nested TCA_FLOWER_ACT */
	return 0;
}

/**
 * Transform a QUEUE action item in the provided flow for TC.
 *
 * @param[in, out] flow
 *   Flow to be filled.
 * @param[in] queue
 *   Queue id to use.
 *
 * @return
 *   0 if checks are alright, -1 otherwise.
 */
static int
add_action_skbedit(struct rte_flow *flow, uint16_t queue)
{
	struct nlmsg *msg = &flow->msg;
	size_t act_index = 1;
	struct tc_skbedit p = {
		.action = TC_ACT_PIPE
	};

	if (nlattr_nested_start(msg, TCA_FLOWER_ACT) < 0)
		return -1;
	if (nlattr_nested_start(msg, act_index++) < 0)
		return -1;
	nlattr_add(&msg->nh, TCA_ACT_KIND, sizeof("skbedit"), "skbedit");
	if (nlattr_nested_start(msg, TCA_ACT_OPTIONS) < 0)
		return -1;
	nlattr_add(&msg->nh, TCA_SKBEDIT_PARMS, sizeof(p), &p);
	nlattr_add16(&msg->nh, TCA_SKBEDIT_QUEUE_MAPPING, queue);
	nlattr_nested_finish(msg); /* nested TCA_ACT_OPTIONS */
	nlattr_nested_finish(msg); /* nested act_index */
	nlattr_nested_finish(msg); /* nested TCA_FLOWER_ACT */
	return 0;
}

/**
 * Validate a flow supported by TC.
 * If flow param is not NULL, then also fill the netlink message inside.
 *
 * @param pmd
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
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
priv_flow_process(struct pmd_internals *pmd,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error,
		  struct rte_flow *flow)
{
	const struct tap_flow_items *cur_item = tap_flow_items;
	struct convert_data data = {
		.eth_type = 0,
		.ip_proto = 0,
		.flow = flow,
	};
	int action = 0; /* Only one action authorized for now */

	if (attr->group > MAX_GROUP) {
		rte_flow_error_set(
			error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
			NULL, "group value too big: cannot exceed 15");
		return -rte_errno;
	}
	if (attr->priority > MAX_PRIORITY) {
		rte_flow_error_set(
			error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			NULL, "priority value too big");
		return -rte_errno;
	} else if (flow) {
		uint16_t group = attr->group << GROUP_SHIFT;
		uint16_t prio = group | (attr->priority + PRIORITY_OFFSET);
		flow->msg.t.tcm_info = TC_H_MAKE(prio << 16,
						 flow->msg.t.tcm_info);
	}
	if (!attr->ingress) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "direction should be ingress");
		return -rte_errno;
	}
	/* rte_flow ingress is actually egress as seen in the kernel */
	if (attr->ingress && flow)
		flow->msg.t.tcm_parent = TC_H_MAKE(MULTIQ_MAJOR_HANDLE, 0);
	if (flow) {
		/* use flower filter type */
		nlattr_add(&flow->msg.nh, TCA_KIND, sizeof("flower"), "flower");
		if (nlattr_nested_start(&flow->msg, TCA_OPTIONS) < 0)
			goto exit_item_not_supported;
	}
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; ++items) {
		const struct tap_flow_items *token = NULL;
		unsigned int i;
		int err = 0;

		if (items->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;
		for (i = 0;
		     cur_item->items &&
		     cur_item->items[i] != RTE_FLOW_ITEM_TYPE_END;
		     ++i) {
			if (cur_item->items[i] == items->type) {
				token = &tap_flow_items[items->type];
				break;
			}
		}
		if (!token)
			goto exit_item_not_supported;
		cur_item = token;
		err = tap_flow_item_validate(
			items, cur_item->mask_sz,
			(const uint8_t *)cur_item->mask,
			(const uint8_t *)cur_item->default_mask);
		if (err)
			goto exit_item_not_supported;
		if (flow && cur_item->convert) {
			if (!pmd->flower_vlan_support &&
			    cur_item->convert == tap_flow_create_vlan)
				goto exit_item_not_supported;
			err = cur_item->convert(items, &data);
			if (err)
				goto exit_item_not_supported;
		}
	}
	if (flow) {
		if (pmd->flower_vlan_support && data.vlan) {
			nlattr_add16(&flow->msg.nh, TCA_FLOWER_KEY_ETH_TYPE,
				     htons(ETH_P_8021Q));
			nlattr_add16(&flow->msg.nh,
				     TCA_FLOWER_KEY_VLAN_ETH_TYPE,
				     data.eth_type ?
				     data.eth_type : htons(ETH_P_ALL));
		} else if (data.eth_type) {
			nlattr_add16(&flow->msg.nh, TCA_FLOWER_KEY_ETH_TYPE,
				     data.eth_type);
		}
	}
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		int err = 0;

		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID) {
			continue;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_DROP) {
			if (action)
				goto exit_action_not_supported;
			action = 1;
			if (flow)
				err = add_action_gact(flow, TC_ACT_SHOT);
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_PASSTHRU) {
			if (action)
				goto exit_action_not_supported;
			action = 1;
			if (flow)
				err = add_action_gact(flow, TC_ACT_UNSPEC);
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *queue =
				(const struct rte_flow_action_queue *)
				actions->conf;
			if (action)
				goto exit_action_not_supported;
			action = 1;
			if (!queue || (queue->index >= pmd->nb_queues))
				goto exit_action_not_supported;
			if (flow)
				err = add_action_skbedit(flow, queue->index);
		} else {
			goto exit_action_not_supported;
		}
		if (err)
			goto exit_action_not_supported;
	}
	if (flow)
		nlattr_nested_finish(&flow->msg); /* nested TCA_OPTIONS */
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
 * Validate a flow.
 *
 * @see rte_flow_validate()
 * @see rte_flow_ops
 */
static int
tap_flow_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	return priv_flow_process(pmd, attr, items, actions, error, NULL);
}

/**
 * Set a unique handle in a flow.
 *
 * The kernel supports TC rules with equal priority, as long as they use the
 * same matching fields (e.g.: dst mac and ipv4) with different values (and
 * full mask to ensure no collision is possible).
 * In those rules, the handle (uint32_t) is the part that would identify
 * specifically each rule.
 *
 * On 32-bit architectures, the handle can simply be the flow's pointer address.
 * On 64-bit architectures, we rely on jhash(flow) to find a (sufficiently)
 * unique handle.
 *
 * @param[in, out] flow
 *   The flow that needs its handle set.
 */
static void
tap_flow_set_handle(struct rte_flow *flow)
{
	uint32_t handle = 0;

	if (sizeof(flow) > 4)
		handle = rte_jhash(&flow, sizeof(flow), 1);
	else
		handle = (uintptr_t)flow;
	/* must be at least 1 to avoid letting the kernel choose one for us */
	if (!handle)
		handle = 1;
	flow->msg.t.tcm_handle = handle;
}

/**
 * Create a flow.
 *
 * @see rte_flow_create()
 * @see rte_flow_ops
 */
static struct rte_flow *
tap_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct rte_flow *flow = NULL;
	struct nlmsg *msg = NULL;
	int err;

	if (!pmd->if_index) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL,
				   "can't create rule, ifindex not found");
		goto fail;
	}
	flow = rte_malloc(__func__, sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate memory for rte_flow");
		goto fail;
	}
	msg = &flow->msg;
	tc_init_msg(msg, pmd->if_index, RTM_NEWTFILTER,
		    NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL | NLM_F_CREATE);
	msg->t.tcm_info = TC_H_MAKE(0, htons(ETH_P_ALL));
	tap_flow_set_handle(flow);
	if (priv_flow_process(pmd, attr, items, actions, error, flow))
		goto fail;
	err = nl_send(pmd->nlsk_fd, &msg->nh);
	if (err < 0) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "couldn't send request to kernel");
		goto fail;
	}
	err = nl_recv_ack(pmd->nlsk_fd);
	if (err < 0) {
		rte_flow_error_set(error, EEXIST, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "overlapping rules");
		goto fail;
	}
	LIST_INSERT_HEAD(&pmd->flows, flow, next);
	return flow;
fail:
	if (flow)
		rte_free(flow);
	return NULL;
}

/**
 * Destroy a flow.
 *
 * @see rte_flow_destroy()
 * @see rte_flow_ops
 */
static int
tap_flow_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int ret = 0;

	LIST_REMOVE(flow, next);
	flow->msg.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	flow->msg.nh.nlmsg_type = RTM_DELTFILTER;

	ret = nl_send(pmd->nlsk_fd, &flow->msg.nh);
	if (ret < 0) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "couldn't send request to kernel");
		goto end;
	}
	ret = nl_recv_ack(pmd->nlsk_fd);
	if (ret < 0)
		rte_flow_error_set(
			error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"couldn't receive kernel ack to our request");
end:
	rte_free(flow);
	return ret;
}

/**
 * Destroy all flows.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
int
tap_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct rte_flow *flow;

	while (!LIST_EMPTY(&pmd->flows)) {
		flow = LIST_FIRST(&pmd->flows);
		if (tap_flow_destroy(dev, flow, error) < 0)
			return -1;
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
 *   0 on success, negative errno value on failure.
 */
int
tap_dev_filter_ctrl(struct rte_eth_dev *dev,
		    enum rte_filter_type filter_type,
		    enum rte_filter_op filter_op,
		    void *arg)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	if (!pmd->flower_support)
		return -ENOTSUP;
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &tap_flow_ops;
		return 0;
	default:
		RTE_LOG(ERR, PMD, "%p: filter type (%d) not supported",
			(void *)dev, filter_type);
	}
	return -EINVAL;
}

