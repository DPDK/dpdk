/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>
#include <sys/resource.h>

#include <rte_byteorder.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <rte_eth_tap.h>
#include <tap_flow.h>
#include <tap_autoconf.h>
#include <tap_tcmsgs.h>
#include <tap_rss.h>

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
/*
 * For kernels < 4.2 BPF related enums may not be defined.
 * Runtime checks will be carried out to gracefully report on TC messages that
 * are rejected by the kernel. Rejection reasons may be due to:
 * 1. enum is not defined
 * 2. enum is defined but kernel is not configured to support BPF system calls,
 *    BPF classifications or BPF actions.
 */
#ifndef HAVE_TC_BPF
enum {
	TCA_BPF_UNSPEC,
	TCA_BPF_ACT,
	TCA_BPF_POLICE,
	TCA_BPF_CLASSID,
	TCA_BPF_OPS_LEN,
	TCA_BPF_OPS,
};
#endif
#ifndef HAVE_TC_BPF_FD
enum {
	TCA_BPF_FD = TCA_BPF_OPS + 1,
	TCA_BPF_NAME,
};
#endif
#ifndef HAVE_TC_ACT_BPF
#define tc_gen \
	__u32                 index; \
	__u32                 capab; \
	int                   action; \
	int                   refcnt; \
	int                   bindcnt

struct tc_act_bpf {
	tc_gen;
};

enum {
	TCA_ACT_BPF_UNSPEC,
	TCA_ACT_BPF_TM,
	TCA_ACT_BPF_PARMS,
	TCA_ACT_BPF_OPS_LEN,
	TCA_ACT_BPF_OPS,
};

#endif
#ifndef HAVE_TC_ACT_BPF_FD
enum {
	TCA_ACT_BPF_FD = TCA_ACT_BPF_OPS + 1,
	TCA_ACT_BPF_NAME,
};
#endif

/* RSS key management */
enum bpf_rss_key_e {
	KEY_CMD_GET = 1,
	KEY_CMD_RELEASE,
	KEY_CMD_INIT,
	KEY_CMD_DEINIT,
};

enum key_status_e {
	KEY_STAT_UNSPEC,
	KEY_STAT_USED,
	KEY_STAT_AVAILABLE,
};

#define ISOLATE_HANDLE 1
#define REMOTE_PROMISCUOUS_HANDLE 2

struct rte_flow {
	LIST_ENTRY(rte_flow) next; /* Pointer to the next rte_flow structure */
	struct rte_flow *remote_flow; /* associated remote flow */
	int bpf_fd[SEC_MAX]; /* list of bfs fds per ELF section */
	uint32_t key_idx; /* RSS rule key index into BPF map */
	struct nlmsg msg;
};

struct convert_data {
	uint16_t eth_type;
	uint16_t ip_proto;
	uint8_t vlan;
	struct rte_flow *flow;
};

struct remote_rule {
	struct rte_flow_attr attr;
	struct rte_flow_item items[2];
	struct rte_flow_action actions[2];
	int mirred;
};

struct action_data {
	char id[16];

	union {
		struct tc_gact gact;
		struct tc_mirred mirred;
		struct skbedit {
			struct tc_skbedit skbedit;
			uint16_t queue;
		} skbedit;
		struct bpf {
			struct tc_act_bpf bpf;
			int bpf_fd;
			const char *annotation;
		} bpf;
	};
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

static void
tap_flow_free(struct pmd_internals *pmd,
	struct rte_flow *flow);

static int
tap_flow_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error);

static int
tap_flow_isolate(struct rte_eth_dev *dev,
		 int set,
		 struct rte_flow_error *error);

static int bpf_rss_key(enum bpf_rss_key_e cmd, __u32 *key_idx);
static int rss_enable(struct pmd_internals *pmd,
			const struct rte_flow_attr *attr,
			struct rte_flow_error *error);
static int rss_add_actions(struct rte_flow *flow, struct pmd_internals *pmd,
			const struct rte_flow_action_rss *rss,
			struct rte_flow_error *error);

static const struct rte_flow_ops tap_flow_ops = {
	.validate = tap_flow_validate,
	.create = tap_flow_create,
	.destroy = tap_flow_destroy,
	.flush = tap_flow_flush,
	.isolate = tap_flow_isolate,
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
			/* DEI matching is not supported */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
			.tci = 0xffef,
#else
			.tci = 0xefff,
#endif
			.inner_type = -1,
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

/*
 *                TC rules, by growing priority
 *
 *        Remote netdevice                  Tap netdevice
 * +-------------+-------------+  +-------------+-------------+
 * |   Ingress   |   Egress    |  |   Ingress   |   Egress    |
 * |-------------|-------------|  |-------------|-------------|
 * |             |  \       /  |  |             |  REMOTE TX  | prio 1
 * |             |   \     /   |  |             |   \     /   | prio 2
 * |  EXPLICIT   |    \   /    |  |  EXPLICIT   |    \   /    |   .
 * |             |     \ /     |  |             |     \ /     |   .
 * |    RULES    |      X      |  |    RULES    |      X      |   .
 * |      .      |     / \     |  |      .      |     / \     |   .
 * |      .      |    /   \    |  |      .      |    /   \    |   .
 * |      .      |   /     \   |  |      .      |   /     \   |   .
 * |      .      |  /       \  |  |      .      |  /       \  |   .
 *
 *      ....           ....           ....           ....
 *
 * |      .      |  \       /  |  |      .      |  \       /  |   .
 * |      .      |   \     /   |  |      .      |   \     /   |   .
 * |             |    \   /    |  |             |    \   /    |
 * |  LOCAL_MAC  |     \ /     |  |    \   /    |     \ /     | last prio - 5
 * |   PROMISC   |      X      |  |     \ /     |      X      | last prio - 4
 * |   ALLMULTI  |     / \     |  |      X      |     / \     | last prio - 3
 * |  BROADCAST  |    /   \    |  |     / \     |    /   \    | last prio - 2
 * | BROADCASTV6 |   /     \   |  |    /   \    |   /     \   | last prio - 1
 * |     xx      |  /       \  |  |   ISOLATE   |  /       \  | last prio
 * +-------------+-------------+  +-------------+-------------+
 *
 * The implicit flow rules are stored in a list in with mandatorily the last two
 * being the ISOLATE and REMOTE_TX rules. e.g.:
 *
 * LOCAL_MAC -> BROADCAST -> BROADCASTV6 -> REMOTE_TX -> ISOLATE -> NULL
 *
 * That enables tap_flow_isolate() to remove implicit rules by popping the list
 * head and remove it as long as it applies on the remote netdevice. The
 * implicit rule for TX redirection is not removed, as isolate concerns only
 * incoming traffic.
 */

static struct remote_rule implicit_rte_flows[TAP_REMOTE_MAX_IDX] = {
	[TAP_REMOTE_LOCAL_MAC] = {
		.attr = {
			.group = MAX_GROUP,
			.priority = PRIORITY_MASK - TAP_REMOTE_LOCAL_MAC,
			.ingress = 1,
		},
		.items[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask =  &(const struct rte_flow_item_eth){
				.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			},
		},
		.items[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
		.mirred = TCA_EGRESS_REDIR,
	},
	[TAP_REMOTE_BROADCAST] = {
		.attr = {
			.group = MAX_GROUP,
			.priority = PRIORITY_MASK - TAP_REMOTE_BROADCAST,
			.ingress = 1,
		},
		.items[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask =  &(const struct rte_flow_item_eth){
				.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			},
			.spec = &(const struct rte_flow_item_eth){
				.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			},
		},
		.items[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
		.mirred = TCA_EGRESS_MIRROR,
	},
	[TAP_REMOTE_BROADCASTV6] = {
		.attr = {
			.group = MAX_GROUP,
			.priority = PRIORITY_MASK - TAP_REMOTE_BROADCASTV6,
			.ingress = 1,
		},
		.items[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask =  &(const struct rte_flow_item_eth){
				.dst.addr_bytes = "\x33\x33\x00\x00\x00\x00",
			},
			.spec = &(const struct rte_flow_item_eth){
				.dst.addr_bytes = "\x33\x33\x00\x00\x00\x00",
			},
		},
		.items[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
		.mirred = TCA_EGRESS_MIRROR,
	},
	[TAP_REMOTE_PROMISC] = {
		.attr = {
			.group = MAX_GROUP,
			.priority = PRIORITY_MASK - TAP_REMOTE_PROMISC,
			.ingress = 1,
		},
		.items[0] = {
			.type = RTE_FLOW_ITEM_TYPE_VOID,
		},
		.items[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
		.mirred = TCA_EGRESS_MIRROR,
	},
	[TAP_REMOTE_ALLMULTI] = {
		.attr = {
			.group = MAX_GROUP,
			.priority = PRIORITY_MASK - TAP_REMOTE_ALLMULTI,
			.ingress = 1,
		},
		.items[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask =  &(const struct rte_flow_item_eth){
				.dst.addr_bytes = "\x01\x00\x00\x00\x00\x00",
			},
			.spec = &(const struct rte_flow_item_eth){
				.dst.addr_bytes = "\x01\x00\x00\x00\x00\x00",
			},
		},
		.items[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
		.mirred = TCA_EGRESS_MIRROR,
	},
	[TAP_REMOTE_TX] = {
		.attr = {
			.group = 0,
			.priority = TAP_REMOTE_TX,
			.egress = 1,
		},
		.items[0] = {
			.type = RTE_FLOW_ITEM_TYPE_VOID,
		},
		.items[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
		.mirred = TCA_EGRESS_MIRROR,
	},
	[TAP_ISOLATE] = {
		.attr = {
			.group = MAX_GROUP,
			.priority = PRIORITY_MASK - TAP_ISOLATE,
			.ingress = 1,
		},
		.items[0] = {
			.type = RTE_FLOW_ITEM_TYPE_VOID,
		},
		.items[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
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
	if (!rte_is_zero_ether_addr(&mask->dst)) {
		tap_nlattr_add(&msg->nh, TCA_FLOWER_KEY_ETH_DST,
			RTE_ETHER_ADDR_LEN,
			   &spec->dst.addr_bytes);
		tap_nlattr_add(&msg->nh,
			   TCA_FLOWER_KEY_ETH_DST_MASK, RTE_ETHER_ADDR_LEN,
			   &mask->dst.addr_bytes);
	}
	if (!rte_is_zero_ether_addr(&mask->src)) {
		tap_nlattr_add(&msg->nh, TCA_FLOWER_KEY_ETH_SRC,
			RTE_ETHER_ADDR_LEN,
			&spec->src.addr_bytes);
		tap_nlattr_add(&msg->nh,
			   TCA_FLOWER_KEY_ETH_SRC_MASK, RTE_ETHER_ADDR_LEN,
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
	/* Outer TPID cannot be matched. */
	if (info->eth_type)
		return -1;
	/* Double-tagging not supported. */
	if (info->vlan)
		return -1;
	info->vlan = 1;
	if (mask->inner_type) {
		/* TC does not support partial eth_type masking */
		if (mask->inner_type != RTE_BE16(0xffff))
			return -1;
		info->eth_type = spec->inner_type;
	}
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
			tap_nlattr_add8(&msg->nh,
					TCA_FLOWER_KEY_VLAN_PRIO, prio);
		if (vid)
			tap_nlattr_add16(&msg->nh,
					 TCA_FLOWER_KEY_VLAN_ID, vid);
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
	if (!spec)
		return 0;
	if (mask->hdr.dst_addr) {
		tap_nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_DST,
			     spec->hdr.dst_addr);
		tap_nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_DST_MASK,
			     mask->hdr.dst_addr);
	}
	if (mask->hdr.src_addr) {
		tap_nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_SRC,
			     spec->hdr.src_addr);
		tap_nlattr_add32(&msg->nh, TCA_FLOWER_KEY_IPV4_SRC_MASK,
			     mask->hdr.src_addr);
	}
	if (spec->hdr.next_proto_id)
		tap_nlattr_add8(&msg->nh, TCA_FLOWER_KEY_IP_PROTO,
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
	if (!spec)
		return 0;
	if (memcmp(mask->hdr.dst_addr, empty_addr, 16)) {
		tap_nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_DST,
			   sizeof(spec->hdr.dst_addr), &spec->hdr.dst_addr);
		tap_nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_DST_MASK,
			   sizeof(mask->hdr.dst_addr), &mask->hdr.dst_addr);
	}
	if (memcmp(mask->hdr.src_addr, empty_addr, 16)) {
		tap_nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_SRC,
			   sizeof(spec->hdr.src_addr), &spec->hdr.src_addr);
		tap_nlattr_add(&msg->nh, TCA_FLOWER_KEY_IPV6_SRC_MASK,
			   sizeof(mask->hdr.src_addr), &mask->hdr.src_addr);
	}
	if (spec->hdr.proto)
		tap_nlattr_add8(&msg->nh,
				TCA_FLOWER_KEY_IP_PROTO, spec->hdr.proto);
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
	/* TC does not support UDP port masking. Only accept if exact match. */
	if ((mask->hdr.src_port && mask->hdr.src_port != 0xffff) ||
	    (mask->hdr.dst_port && mask->hdr.dst_port != 0xffff))
		return -1;
	if (!flow)
		return 0;
	msg = &flow->msg;
	tap_nlattr_add8(&msg->nh, TCA_FLOWER_KEY_IP_PROTO, IPPROTO_UDP);
	if (!spec)
		return 0;
	if (mask->hdr.dst_port)
		tap_nlattr_add16(&msg->nh, TCA_FLOWER_KEY_UDP_DST,
			     spec->hdr.dst_port);
	if (mask->hdr.src_port)
		tap_nlattr_add16(&msg->nh, TCA_FLOWER_KEY_UDP_SRC,
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
	/* TC does not support TCP port masking. Only accept if exact match. */
	if ((mask->hdr.src_port && mask->hdr.src_port != 0xffff) ||
	    (mask->hdr.dst_port && mask->hdr.dst_port != 0xffff))
		return -1;
	if (!flow)
		return 0;
	msg = &flow->msg;
	tap_nlattr_add8(&msg->nh, TCA_FLOWER_KEY_IP_PROTO, IPPROTO_TCP);
	if (!spec)
		return 0;
	if (mask->hdr.dst_port)
		tap_nlattr_add16(&msg->nh, TCA_FLOWER_KEY_TCP_DST,
			     spec->hdr.dst_port);
	if (mask->hdr.src_port)
		tap_nlattr_add16(&msg->nh, TCA_FLOWER_KEY_TCP_SRC,
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
 * Configure the kernel with a TC action and its configured parameters
 * Handled actions: "gact", "mirred", "skbedit", "bpf"
 *
 * @param[in] flow
 *   Pointer to rte flow containing the netlink message
 *
 * @param[in, out] act_index
 *   Pointer to action sequence number in the TC command
 *
 * @param[in] adata
 *  Pointer to struct holding the action parameters
 *
 * @return
 *   -1 on failure, 0 on success
 */
static int
add_action(struct rte_flow *flow, size_t *act_index, struct action_data *adata)
{
	struct nlmsg *msg = &flow->msg;

	if (tap_nlattr_nested_start(msg, (*act_index)++) < 0)
		return -1;

	tap_nlattr_add(&msg->nh, TCA_ACT_KIND,
				strlen(adata->id) + 1, adata->id);
	if (tap_nlattr_nested_start(msg, TCA_ACT_OPTIONS) < 0)
		return -1;
	if (strcmp("gact", adata->id) == 0) {
		tap_nlattr_add(&msg->nh, TCA_GACT_PARMS, sizeof(adata->gact),
			   &adata->gact);
	} else if (strcmp("mirred", adata->id) == 0) {
		if (adata->mirred.eaction == TCA_EGRESS_MIRROR)
			adata->mirred.action = TC_ACT_PIPE;
		else /* REDIRECT */
			adata->mirred.action = TC_ACT_STOLEN;
		tap_nlattr_add(&msg->nh, TCA_MIRRED_PARMS,
			   sizeof(adata->mirred),
			   &adata->mirred);
	} else if (strcmp("skbedit", adata->id) == 0) {
		tap_nlattr_add(&msg->nh, TCA_SKBEDIT_PARMS,
			   sizeof(adata->skbedit.skbedit),
			   &adata->skbedit.skbedit);
		tap_nlattr_add16(&msg->nh, TCA_SKBEDIT_QUEUE_MAPPING,
			     adata->skbedit.queue);
	} else if (strcmp("bpf", adata->id) == 0) {
		tap_nlattr_add32(&msg->nh, TCA_ACT_BPF_FD, adata->bpf.bpf_fd);
		tap_nlattr_add(&msg->nh, TCA_ACT_BPF_NAME,
			   strlen(adata->bpf.annotation) + 1,
			   adata->bpf.annotation);
		tap_nlattr_add(&msg->nh, TCA_ACT_BPF_PARMS,
			   sizeof(adata->bpf.bpf),
			   &adata->bpf.bpf);
	} else {
		return -1;
	}
	tap_nlattr_nested_finish(msg); /* nested TCA_ACT_OPTIONS */
	tap_nlattr_nested_finish(msg); /* nested act_index */
	return 0;
}

/**
 * Helper function to send a series of TC actions to the kernel
 *
 * @param[in] flow
 *   Pointer to rte flow containing the netlink message
 *
 * @param[in] nb_actions
 *   Number of actions in an array of action structs
 *
 * @param[in] data
 *   Pointer to an array of action structs
 *
 * @param[in] classifier_actions
 *   The classifier on behave of which the actions are configured
 *
 * @return
 *   -1 on failure, 0 on success
 */
static int
add_actions(struct rte_flow *flow, int nb_actions, struct action_data *data,
	    int classifier_action)
{
	struct nlmsg *msg = &flow->msg;
	size_t act_index = 1;
	int i;

	if (tap_nlattr_nested_start(msg, classifier_action) < 0)
		return -1;
	for (i = 0; i < nb_actions; i++)
		if (add_action(flow, &act_index, data + i) < 0)
			return -1;
	tap_nlattr_nested_finish(msg); /* nested TCA_FLOWER_ACT */
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
 * @param[in] mirred
 *   If set to TCA_EGRESS_REDIR, provided actions will be replaced with a
 *   redirection to the tap netdevice, and the TC rule will be configured
 *   on the remote netdevice in pmd.
 *   If set to TCA_EGRESS_MIRROR, provided actions will be replaced with a
 *   mirroring to the tap netdevice, and the TC rule will be configured
 *   on the remote netdevice in pmd. Matching packets will thus be duplicated.
 *   If set to 0, the standard behavior is to be used: set correct actions for
 *   the TC rule, and apply it on the tap netdevice.
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
		  struct rte_flow *flow,
		  int mirred)
{
	const struct tap_flow_items *cur_item = tap_flow_items;
	struct convert_data data = {
		.eth_type = 0,
		.ip_proto = 0,
		.flow = flow,
	};
	int action = 0; /* Only one action authorized for now */

	if (attr->transfer) {
		rte_flow_error_set(
			error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
			NULL, "transfer is not supported");
		return -rte_errno;
	}
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
		uint16_t prio = group | (attr->priority +
				RSS_PRIORITY_OFFSET + PRIORITY_OFFSET);
		flow->msg.t.tcm_info = TC_H_MAKE(prio << 16,
						 flow->msg.t.tcm_info);
	}
	if (flow) {
		if (mirred) {
			/*
			 * If attr->ingress, the rule applies on remote ingress
			 * to match incoming packets
			 * If attr->egress, the rule applies on tap ingress (as
			 * seen from the kernel) to deal with packets going out
			 * from the DPDK app.
			 */
			flow->msg.t.tcm_parent = TC_H_MAKE(TC_H_INGRESS, 0);
		} else {
			/* Standard rule on tap egress (kernel standpoint). */
			flow->msg.t.tcm_parent =
				TC_H_MAKE(MULTIQ_MAJOR_HANDLE, 0);
		}
		/* use flower filter type */
		tap_nlattr_add(&flow->msg.nh, TCA_KIND, sizeof("flower"), "flower");
		if (tap_nlattr_nested_start(&flow->msg, TCA_OPTIONS) < 0)
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
			err = cur_item->convert(items, &data);
			if (err)
				goto exit_item_not_supported;
		}
	}
	if (flow) {
		if (data.vlan) {
			tap_nlattr_add16(&flow->msg.nh, TCA_FLOWER_KEY_ETH_TYPE,
				     htons(ETH_P_8021Q));
			tap_nlattr_add16(&flow->msg.nh,
				     TCA_FLOWER_KEY_VLAN_ETH_TYPE,
				     data.eth_type ?
				     data.eth_type : htons(ETH_P_ALL));
		} else if (data.eth_type) {
			tap_nlattr_add16(&flow->msg.nh, TCA_FLOWER_KEY_ETH_TYPE,
				     data.eth_type);
		}
	}
	if (mirred && flow) {
		struct action_data adata = {
			.id = "mirred",
			.mirred = {
				.eaction = mirred,
			},
		};

		/*
		 * If attr->egress && mirred, then this is a special
		 * case where the rule must be applied on the tap, to
		 * redirect packets coming from the DPDK App, out
		 * through the remote netdevice.
		 */
		adata.mirred.ifindex = attr->ingress ? pmd->if_index :
			pmd->remote_if_index;
		if (mirred == TCA_EGRESS_MIRROR)
			adata.mirred.action = TC_ACT_PIPE;
		else
			adata.mirred.action = TC_ACT_STOLEN;
		if (add_actions(flow, 1, &adata, TCA_FLOWER_ACT) < 0)
			goto exit_action_not_supported;
		else
			goto end;
	}
actions:
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		int err = 0;

		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID) {
			continue;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_DROP) {
			if (action)
				goto exit_action_not_supported;
			action = 1;
			if (flow) {
				struct action_data adata = {
					.id = "gact",
					.gact = {
						.action = TC_ACT_SHOT,
					},
				};

				err = add_actions(flow, 1, &adata,
						  TCA_FLOWER_ACT);
			}
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_PASSTHRU) {
			if (action)
				goto exit_action_not_supported;
			action = 1;
			if (flow) {
				struct action_data adata = {
					.id = "gact",
					.gact = {
						/* continue */
						.action = TC_ACT_UNSPEC,
					},
				};

				err = add_actions(flow, 1, &adata,
						  TCA_FLOWER_ACT);
			}
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *queue =
				(const struct rte_flow_action_queue *)
				actions->conf;

			if (action)
				goto exit_action_not_supported;
			action = 1;
			if (!queue ||
			    (queue->index > pmd->dev->data->nb_rx_queues - 1))
				goto exit_action_not_supported;
			if (flow) {
				struct action_data adata = {
					.id = "skbedit",
					.skbedit = {
						.skbedit = {
							.action = TC_ACT_PIPE,
						},
						.queue = queue->index,
					},
				};

				err = add_actions(flow, 1, &adata,
					TCA_FLOWER_ACT);
			}
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_RSS) {
			const struct rte_flow_action_rss *rss =
				(const struct rte_flow_action_rss *)
				actions->conf;

			if (action++)
				goto exit_action_not_supported;

			if (!pmd->rss_enabled) {
				err = rss_enable(pmd, attr, error);
				if (err)
					goto exit_action_not_supported;
			}
			if (flow)
				err = rss_add_actions(flow, pmd, rss, error);
		} else {
			goto exit_action_not_supported;
		}
		if (err)
			goto exit_action_not_supported;
	}
	/* When fate is unknown, drop traffic. */
	if (!action) {
		static const struct rte_flow_action drop[] = {
			{ .type = RTE_FLOW_ACTION_TYPE_DROP, },
			{ .type = RTE_FLOW_ACTION_TYPE_END, },
		};

		actions = drop;
		goto actions;
	}
end:
	if (flow)
		tap_nlattr_nested_finish(&flow->msg); /* nested TCA_OPTIONS */
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

	return priv_flow_process(pmd, attr, items, actions, error, NULL, 0);
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
	union {
		struct rte_flow *flow;
		const void *key;
	} tmp;
	uint32_t handle = 0;

	tmp.flow = flow;

	if (sizeof(flow) > 4)
		handle = rte_jhash(tmp.key, sizeof(flow), 1);
	else
		handle = (uintptr_t)flow;
	/* must be at least 1 to avoid letting the kernel choose one for us */
	if (!handle)
		handle = 1;
	flow->msg.t.tcm_handle = handle;
}

/**
 * Free the flow opened file descriptors and allocated memory
 *
 * @param[in] flow
 *   Pointer to the flow to free
 *
 */
static void
tap_flow_free(struct pmd_internals *pmd, struct rte_flow *flow)
{
	int i;

	if (!flow)
		return;

	if (pmd->rss_enabled) {
		/* Close flow BPF file descriptors */
		for (i = 0; i < SEC_MAX; i++)
			if (flow->bpf_fd[i] != 0) {
				close(flow->bpf_fd[i]);
				flow->bpf_fd[i] = 0;
			}

		/* Release the map key for this RSS rule */
		bpf_rss_key(KEY_CMD_RELEASE, &flow->key_idx);
		flow->key_idx = 0;
	}

	/* Free flow allocated memory */
	rte_free(flow);
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
	struct rte_flow *remote_flow = NULL;
	struct rte_flow *flow = NULL;
	struct nlmsg *msg = NULL;
	int err;

	if (!pmd->if_index) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL,
				   "can't create rule, ifindex not found");
		goto fail;
	}
	/*
	 * No rules configured through standard rte_flow should be set on the
	 * priorities used by implicit rules.
	 */
	if ((attr->group == MAX_GROUP) &&
	    attr->priority > (MAX_PRIORITY - TAP_REMOTE_MAX_IDX)) {
		rte_flow_error_set(
			error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			NULL, "priority value too big");
		goto fail;
	}
	flow = rte_zmalloc(__func__, sizeof(struct rte_flow), 0);
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
	if (priv_flow_process(pmd, attr, items, actions, error, flow, 0))
		goto fail;
	err = tap_nl_send(pmd->nlsk_fd, &msg->nh);
	if (err < 0) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "couldn't send request to kernel");
		goto fail;
	}
	err = tap_nl_recv_ack(pmd->nlsk_fd);
	if (err < 0) {
		TAP_LOG(ERR,
			"Kernel refused TC filter rule creation (%d): %s",
			errno, strerror(errno));
		rte_flow_error_set(error, EEXIST, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL,
				   "overlapping rules or Kernel too old for flower support");
		goto fail;
	}
	LIST_INSERT_HEAD(&pmd->flows, flow, next);
	/**
	 * If a remote device is configured, a TC rule with identical items for
	 * matching must be set on that device, with a single action: redirect
	 * to the local pmd->if_index.
	 */
	if (pmd->remote_if_index) {
		remote_flow = rte_zmalloc(__func__, sizeof(struct rte_flow), 0);
		if (!remote_flow) {
			rte_flow_error_set(
				error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"cannot allocate memory for rte_flow");
			goto fail;
		}
		msg = &remote_flow->msg;
		/* set the rule if_index for the remote netdevice */
		tc_init_msg(
			msg, pmd->remote_if_index, RTM_NEWTFILTER,
			NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL | NLM_F_CREATE);
		msg->t.tcm_info = TC_H_MAKE(0, htons(ETH_P_ALL));
		tap_flow_set_handle(remote_flow);
		if (priv_flow_process(pmd, attr, items, NULL,
				      error, remote_flow, TCA_EGRESS_REDIR)) {
			rte_flow_error_set(
				error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "rte flow rule validation failed");
			goto fail;
		}
		err = tap_nl_send(pmd->nlsk_fd, &msg->nh);
		if (err < 0) {
			rte_flow_error_set(
				error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Failure sending nl request");
			goto fail;
		}
		err = tap_nl_recv_ack(pmd->nlsk_fd);
		if (err < 0) {
			TAP_LOG(ERR,
				"Kernel refused TC filter rule creation (%d): %s",
				errno, strerror(errno));
			rte_flow_error_set(
				error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"overlapping rules or Kernel too old for flower support");
			goto fail;
		}
		flow->remote_flow = remote_flow;
	}
	return flow;
fail:
	if (remote_flow)
		rte_free(remote_flow);
	if (flow)
		tap_flow_free(pmd, flow);
	return NULL;
}

/**
 * Destroy a flow using pointer to pmd_internal.
 *
 * @param[in, out] pmd
 *   Pointer to private structure.
 * @param[in] flow
 *   Pointer to the flow to destroy.
 * @param[in, out] error
 *   Pointer to the flow error handler
 *
 * @return 0 if the flow could be destroyed, -1 otherwise.
 */
static int
tap_flow_destroy_pmd(struct pmd_internals *pmd,
		     struct rte_flow *flow,
		     struct rte_flow_error *error)
{
	struct rte_flow *remote_flow = flow->remote_flow;
	int ret = 0;

	LIST_REMOVE(flow, next);
	flow->msg.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	flow->msg.nh.nlmsg_type = RTM_DELTFILTER;

	ret = tap_nl_send(pmd->nlsk_fd, &flow->msg.nh);
	if (ret < 0) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "couldn't send request to kernel");
		goto end;
	}
	ret = tap_nl_recv_ack(pmd->nlsk_fd);
	/* If errno is ENOENT, the rule is already no longer in the kernel. */
	if (ret < 0 && errno == ENOENT)
		ret = 0;
	if (ret < 0) {
		TAP_LOG(ERR,
			"Kernel refused TC filter rule deletion (%d): %s",
			errno, strerror(errno));
		rte_flow_error_set(
			error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"couldn't receive kernel ack to our request");
		goto end;
	}

	if (remote_flow) {
		remote_flow->msg.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		remote_flow->msg.nh.nlmsg_type = RTM_DELTFILTER;

		ret = tap_nl_send(pmd->nlsk_fd, &remote_flow->msg.nh);
		if (ret < 0) {
			rte_flow_error_set(
				error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Failure sending nl request");
			goto end;
		}
		ret = tap_nl_recv_ack(pmd->nlsk_fd);
		if (ret < 0 && errno == ENOENT)
			ret = 0;
		if (ret < 0) {
			TAP_LOG(ERR,
				"Kernel refused TC filter rule deletion (%d): %s",
				errno, strerror(errno));
			rte_flow_error_set(
				error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Failure trying to receive nl ack");
			goto end;
		}
	}
end:
	if (remote_flow)
		rte_free(remote_flow);
	tap_flow_free(pmd, flow);
	return ret;
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

	return tap_flow_destroy_pmd(pmd, flow, error);
}

/**
 * Enable/disable flow isolation.
 *
 * @see rte_flow_isolate()
 * @see rte_flow_ops
 */
static int
tap_flow_isolate(struct rte_eth_dev *dev,
		 int set,
		 struct rte_flow_error *error __rte_unused)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *process_private = dev->process_private;

	/* normalize 'set' variable to contain 0 or 1 values */
	if (set)
		set = 1;
	/* if already in the right isolation mode - nothing to do */
	if ((set ^ pmd->flow_isolate) == 0)
		return 0;
	/* mark the isolation mode for tap_flow_implicit_create() */
	pmd->flow_isolate = set;
	/*
	 * If netdevice is there, setup appropriate flow rules immediately.
	 * Otherwise it will be set when bringing up the netdevice (tun_alloc).
	 */
	if (!process_private->rxq_fds[0])
		return 0;
	if (set) {
		struct rte_flow *remote_flow;

		while (1) {
			remote_flow = LIST_FIRST(&pmd->implicit_flows);
			if (!remote_flow)
				break;
			/*
			 * Remove all implicit rules on the remote.
			 * Keep the local rule to redirect packets on TX.
			 * Keep also the last implicit local rule: ISOLATE.
			 */
			if (remote_flow->msg.t.tcm_ifindex == pmd->if_index)
				break;
			if (tap_flow_destroy_pmd(pmd, remote_flow, NULL) < 0)
				goto error;
		}
		/* Switch the TC rule according to pmd->flow_isolate */
		if (tap_flow_implicit_create(pmd, TAP_ISOLATE) == -1)
			goto error;
	} else {
		/* Switch the TC rule according to pmd->flow_isolate */
		if (tap_flow_implicit_create(pmd, TAP_ISOLATE) == -1)
			goto error;
		if (!pmd->remote_if_index)
			return 0;
		if (tap_flow_implicit_create(pmd, TAP_REMOTE_TX) < 0)
			goto error;
		if (tap_flow_implicit_create(pmd, TAP_REMOTE_LOCAL_MAC) < 0)
			goto error;
		if (tap_flow_implicit_create(pmd, TAP_REMOTE_BROADCAST) < 0)
			goto error;
		if (tap_flow_implicit_create(pmd, TAP_REMOTE_BROADCASTV6) < 0)
			goto error;
		if (dev->data->promiscuous &&
		    tap_flow_implicit_create(pmd, TAP_REMOTE_PROMISC) < 0)
			goto error;
		if (dev->data->all_multicast &&
		    tap_flow_implicit_create(pmd, TAP_REMOTE_ALLMULTI) < 0)
			goto error;
	}
	return 0;
error:
	pmd->flow_isolate = 0;
	return rte_flow_error_set(
		error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		"TC rule creation failed");
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
 * Add an implicit flow rule on the remote device to make sure traffic gets to
 * the tap netdevice from there.
 *
 * @param pmd
 *   Pointer to private structure.
 * @param[in] idx
 *   The idx in the implicit_rte_flows array specifying which rule to apply.
 *
 * @return -1 if the rule couldn't be applied, 0 otherwise.
 */
int tap_flow_implicit_create(struct pmd_internals *pmd,
			     enum implicit_rule_index idx)
{
	uint16_t flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL | NLM_F_CREATE;
	struct rte_flow_action *actions = implicit_rte_flows[idx].actions;
	struct rte_flow_action isolate_actions[2] = {
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct rte_flow_item *items = implicit_rte_flows[idx].items;
	struct rte_flow_attr *attr = &implicit_rte_flows[idx].attr;
	struct rte_flow_item_eth eth_local = { .type = 0 };
	uint16_t if_index = pmd->remote_if_index;
	struct rte_flow *remote_flow = NULL;
	struct nlmsg *msg = NULL;
	int err = 0;
	struct rte_flow_item items_local[2] = {
		[0] = {
			.type = items[0].type,
			.spec = &eth_local,
			.mask = items[0].mask,
		},
		[1] = {
			.type = items[1].type,
		}
	};

	remote_flow = rte_zmalloc(__func__, sizeof(struct rte_flow), 0);
	if (!remote_flow) {
		TAP_LOG(ERR, "Cannot allocate memory for rte_flow");
		goto fail;
	}
	msg = &remote_flow->msg;
	if (idx == TAP_REMOTE_TX) {
		if_index = pmd->if_index;
	} else if (idx == TAP_ISOLATE) {
		if_index = pmd->if_index;
		/* Don't be exclusive for this rule, it can be changed later. */
		flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE;
		isolate_actions[0].type = pmd->flow_isolate ?
			RTE_FLOW_ACTION_TYPE_DROP :
			RTE_FLOW_ACTION_TYPE_PASSTHRU;
		actions = isolate_actions;
	} else if (idx == TAP_REMOTE_LOCAL_MAC) {
		/*
		 * eth addr couldn't be set in implicit_rte_flows[] as it is not
		 * known at compile time.
		 */
		memcpy(&eth_local.dst, &pmd->eth_addr, sizeof(pmd->eth_addr));
		items = items_local;
	}
	tc_init_msg(msg, if_index, RTM_NEWTFILTER, flags);
	msg->t.tcm_info = TC_H_MAKE(0, htons(ETH_P_ALL));
	/*
	 * The ISOLATE rule is always present and must have a static handle, as
	 * the action is changed whether the feature is enabled (DROP) or
	 * disabled (PASSTHRU).
	 * There is just one REMOTE_PROMISCUOUS rule in all cases. It should
	 * have a static handle such that adding it twice will fail with EEXIST
	 * with any kernel version. Remark: old kernels may falsely accept the
	 * same REMOTE_PROMISCUOUS rules if they had different handles.
	 */
	if (idx == TAP_ISOLATE)
		remote_flow->msg.t.tcm_handle = ISOLATE_HANDLE;
	else if (idx == TAP_REMOTE_PROMISC)
		remote_flow->msg.t.tcm_handle = REMOTE_PROMISCUOUS_HANDLE;
	else
		tap_flow_set_handle(remote_flow);
	if (priv_flow_process(pmd, attr, items, actions, NULL,
			      remote_flow, implicit_rte_flows[idx].mirred)) {
		TAP_LOG(ERR, "rte flow rule validation failed");
		goto fail;
	}
	err = tap_nl_send(pmd->nlsk_fd, &msg->nh);
	if (err < 0) {
		TAP_LOG(ERR, "Failure sending nl request");
		goto fail;
	}
	err = tap_nl_recv_ack(pmd->nlsk_fd);
	if (err < 0) {
		/* Silently ignore re-entering existing rule */
		if (errno == EEXIST)
			goto success;
		TAP_LOG(ERR,
			"Kernel refused TC filter rule creation (%d): %s",
			errno, strerror(errno));
		goto fail;
	}
	LIST_INSERT_HEAD(&pmd->implicit_flows, remote_flow, next);
success:
	return 0;
fail:
	if (remote_flow)
		rte_free(remote_flow);
	return -1;
}

/**
 * Remove specific implicit flow rule on the remote device.
 *
 * @param[in, out] pmd
 *   Pointer to private structure.
 * @param[in] idx
 *   The idx in the implicit_rte_flows array specifying which rule to remove.
 *
 * @return -1 if one of the implicit rules couldn't be created, 0 otherwise.
 */
int tap_flow_implicit_destroy(struct pmd_internals *pmd,
			      enum implicit_rule_index idx)
{
	struct rte_flow *remote_flow;
	int cur_prio = -1;
	int idx_prio = implicit_rte_flows[idx].attr.priority + PRIORITY_OFFSET;

	for (remote_flow = LIST_FIRST(&pmd->implicit_flows);
	     remote_flow;
	     remote_flow = LIST_NEXT(remote_flow, next)) {
		cur_prio = (remote_flow->msg.t.tcm_info >> 16) & PRIORITY_MASK;
		if (cur_prio != idx_prio)
			continue;
		return tap_flow_destroy_pmd(pmd, remote_flow, NULL);
	}
	return 0;
}

/**
 * Destroy all implicit flows.
 *
 * @see rte_flow_flush()
 */
int
tap_flow_implicit_flush(struct pmd_internals *pmd, struct rte_flow_error *error)
{
	struct rte_flow *remote_flow;

	while (!LIST_EMPTY(&pmd->implicit_flows)) {
		remote_flow = LIST_FIRST(&pmd->implicit_flows);
		if (tap_flow_destroy_pmd(pmd, remote_flow, error) < 0)
			return -1;
	}
	return 0;
}

#define MAX_RSS_KEYS 256
#define KEY_IDX_OFFSET (3 * MAX_RSS_KEYS)
#define SEC_NAME_CLS_Q "cls_q"

static const char *sec_name[SEC_MAX] = {
	[SEC_L3_L4] = "l3_l4",
};

/**
 * Enable RSS on tap: create TC rules for queuing.
 *
 * @param[in, out] pmd
 *   Pointer to private structure.
 *
 * @param[in] attr
 *   Pointer to rte_flow to get flow group
 *
 * @param[out] error
 *   Pointer to error reporting if not NULL.
 *
 * @return 0 on success, negative value on failure.
 */
static int rss_enable(struct pmd_internals *pmd,
			const struct rte_flow_attr *attr,
			struct rte_flow_error *error)
{
	struct rte_flow *rss_flow = NULL;
	struct nlmsg *msg = NULL;
	/* 4096 is the maximum number of instructions for a BPF program */
	char annotation[64];
	int i;
	int err = 0;

	/* unlimit locked memory */
	struct rlimit memlock_limit = {
		.rlim_cur = RLIM_INFINITY,
		.rlim_max = RLIM_INFINITY,
	};
	setrlimit(RLIMIT_MEMLOCK, &memlock_limit);

	 /* Get a new map key for a new RSS rule */
	err = bpf_rss_key(KEY_CMD_INIT, NULL);
	if (err < 0) {
		rte_flow_error_set(
			error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to initialize BPF RSS keys");

		return -1;
	}

	/*
	 *  Create BPF RSS MAP
	 */
	pmd->map_fd = tap_flow_bpf_rss_map_create(sizeof(__u32), /* key size */
				sizeof(struct rss_key),
				MAX_RSS_KEYS);
	if (pmd->map_fd < 0) {
		TAP_LOG(ERR,
			"Failed to create BPF map (%d): %s",
				errno, strerror(errno));
		rte_flow_error_set(
			error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Kernel too old or not configured "
			"to support BPF maps");

		return -ENOTSUP;
	}

	/*
	 * Add a rule per queue to match reclassified packets and direct them to
	 * the correct queue.
	 */
	for (i = 0; i < pmd->dev->data->nb_rx_queues; i++) {
		pmd->bpf_fd[i] = tap_flow_bpf_cls_q(i);
		if (pmd->bpf_fd[i] < 0) {
			TAP_LOG(ERR,
				"Failed to load BPF section %s for queue %d",
				SEC_NAME_CLS_Q, i);
			rte_flow_error_set(
				error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"Kernel too old or not configured "
				"to support BPF programs loading");

			return -ENOTSUP;
		}

		rss_flow = rte_zmalloc(__func__, sizeof(struct rte_flow), 0);
		if (!rss_flow) {
			TAP_LOG(ERR,
				"Cannot allocate memory for rte_flow");
			return -1;
		}
		msg = &rss_flow->msg;
		tc_init_msg(msg, pmd->if_index, RTM_NEWTFILTER, NLM_F_REQUEST |
			    NLM_F_ACK | NLM_F_EXCL | NLM_F_CREATE);
		msg->t.tcm_info = TC_H_MAKE(0, htons(ETH_P_ALL));
		tap_flow_set_handle(rss_flow);
		uint16_t group = attr->group << GROUP_SHIFT;
		uint16_t prio = group | (i + PRIORITY_OFFSET);
		msg->t.tcm_info = TC_H_MAKE(prio << 16, msg->t.tcm_info);
		msg->t.tcm_parent = TC_H_MAKE(MULTIQ_MAJOR_HANDLE, 0);

		tap_nlattr_add(&msg->nh, TCA_KIND, sizeof("bpf"), "bpf");
		if (tap_nlattr_nested_start(msg, TCA_OPTIONS) < 0)
			return -1;
		tap_nlattr_add32(&msg->nh, TCA_BPF_FD, pmd->bpf_fd[i]);
		snprintf(annotation, sizeof(annotation), "[%s%d]",
			SEC_NAME_CLS_Q, i);
		tap_nlattr_add(&msg->nh, TCA_BPF_NAME, strlen(annotation) + 1,
			   annotation);
		/* Actions */
		{
			struct action_data adata = {
				.id = "skbedit",
				.skbedit = {
					.skbedit = {
						.action = TC_ACT_PIPE,
					},
					.queue = i,
				},
			};
			if (add_actions(rss_flow, 1, &adata, TCA_BPF_ACT) < 0)
				return -1;
		}
		tap_nlattr_nested_finish(msg); /* nested TCA_OPTIONS */

		/* Netlink message is now ready to be sent */
		if (tap_nl_send(pmd->nlsk_fd, &msg->nh) < 0)
			return -1;
		err = tap_nl_recv_ack(pmd->nlsk_fd);
		if (err < 0) {
			TAP_LOG(ERR,
				"Kernel refused TC filter rule creation (%d): %s",
				errno, strerror(errno));
			return err;
		}
		LIST_INSERT_HEAD(&pmd->rss_flows, rss_flow, next);
	}

	pmd->rss_enabled = 1;
	return err;
}

/**
 * Manage bpf RSS keys repository with operations: init, get, release
 *
 * @param[in] cmd
 *   Command on RSS keys: init, get, release
 *
 * @param[in, out] key_idx
 *   Pointer to RSS Key index (out for get command, in for release command)
 *
 * @return -1 if couldn't get, release or init the RSS keys, 0 otherwise.
 */
static int bpf_rss_key(enum bpf_rss_key_e cmd, __u32 *key_idx)
{
	__u32 i;
	int err = 0;
	static __u32 num_used_keys;
	static __u32 rss_keys[MAX_RSS_KEYS] = {KEY_STAT_UNSPEC};
	static __u32 rss_keys_initialized;
	__u32 key;

	switch (cmd) {
	case KEY_CMD_GET:
		if (!rss_keys_initialized) {
			err = -1;
			break;
		}

		if (num_used_keys == RTE_DIM(rss_keys)) {
			err = -1;
			break;
		}

		*key_idx = num_used_keys % RTE_DIM(rss_keys);
		while (rss_keys[*key_idx] == KEY_STAT_USED)
			*key_idx = (*key_idx + 1) % RTE_DIM(rss_keys);

		rss_keys[*key_idx] = KEY_STAT_USED;

		/*
		 * Add an offset to key_idx in order to handle a case of
		 * RSS and non RSS flows mixture.
		 * If a non RSS flow is destroyed it has an eBPF map
		 * index 0 (initialized on flow creation) and might
		 * unintentionally remove RSS entry 0 from eBPF map.
		 * To avoid this issue, add an offset to the real index
		 * during a KEY_CMD_GET operation and subtract this offset
		 * during a KEY_CMD_RELEASE operation in order to restore
		 * the real index.
		 */
		*key_idx += KEY_IDX_OFFSET;
		num_used_keys++;
	break;

	case KEY_CMD_RELEASE:
		if (!rss_keys_initialized)
			break;

		/*
		 * Subtract offset to restore real key index
		 * If a non RSS flow is falsely trying to release map
		 * entry 0 - the offset subtraction will calculate the real
		 * map index as an out-of-range value and the release operation
		 * will be silently ignored.
		 */
		key = *key_idx - KEY_IDX_OFFSET;
		if (key >= RTE_DIM(rss_keys))
			break;

		if (rss_keys[key] == KEY_STAT_USED) {
			rss_keys[key] = KEY_STAT_AVAILABLE;
			num_used_keys--;
		}
	break;

	case KEY_CMD_INIT:
		for (i = 0; i < RTE_DIM(rss_keys); i++)
			rss_keys[i] = KEY_STAT_AVAILABLE;

		rss_keys_initialized = 1;
		num_used_keys = 0;
	break;

	case KEY_CMD_DEINIT:
		for (i = 0; i < RTE_DIM(rss_keys); i++)
			rss_keys[i] = KEY_STAT_UNSPEC;

		rss_keys_initialized = 0;
		num_used_keys = 0;
	break;

	default:
		break;
	}

	return err;
}

/**
 * Add RSS hash calculations and queue selection
 *
 * @param[in, out] pmd
 *   Pointer to internal structure. Used to set/get RSS map fd
 *
 * @param[in] rss
 *   Pointer to RSS flow actions
 *
 * @param[out] error
 *   Pointer to error reporting if not NULL.
 *
 * @return 0 on success, negative value on failure
 */
static int rss_add_actions(struct rte_flow *flow, struct pmd_internals *pmd,
			   const struct rte_flow_action_rss *rss,
			   struct rte_flow_error *error)
{
	/* 4096 is the maximum number of instructions for a BPF program */
	unsigned int i;
	int err;
	struct rss_key rss_entry = { .hash_fields = 0,
				     .key_size = 0 };

	/* Check supported RSS features */
	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "non-default RSS hash functions are not supported");
	if (rss->level)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "a nonzero RSS encapsulation level is not supported");

	/* Get a new map key for a new RSS rule */
	err = bpf_rss_key(KEY_CMD_GET, &flow->key_idx);
	if (err < 0) {
		rte_flow_error_set(
			error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to get BPF RSS key");

		return -1;
	}

	/* Update RSS map entry with queues */
	rss_entry.nb_queues = rss->queue_num;
	for (i = 0; i < rss->queue_num; i++)
		rss_entry.queues[i] = rss->queue[i];
	rss_entry.hash_fields =
		(1 << HASH_FIELD_IPV4_L3_L4) | (1 << HASH_FIELD_IPV6_L3_L4);

	/* Add this RSS entry to map */
	err = tap_flow_bpf_update_rss_elem(pmd->map_fd,
				&flow->key_idx, &rss_entry);

	if (err) {
		TAP_LOG(ERR,
			"Failed to update BPF map entry #%u (%d): %s",
			flow->key_idx, errno, strerror(errno));
		rte_flow_error_set(
			error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Kernel too old or not configured "
			"to support BPF maps updates");

		return -ENOTSUP;
	}


	/*
	 * Load bpf rules to calculate hash for this key_idx
	 */

	flow->bpf_fd[SEC_L3_L4] =
		tap_flow_bpf_calc_l3_l4_hash(flow->key_idx, pmd->map_fd);
	if (flow->bpf_fd[SEC_L3_L4] < 0) {
		TAP_LOG(ERR,
			"Failed to load BPF section %s (%d): %s",
				sec_name[SEC_L3_L4], errno, strerror(errno));
		rte_flow_error_set(
			error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Kernel too old or not configured "
			"to support BPF program loading");

		return -ENOTSUP;
	}

	/* Actions */
	{
		struct action_data adata[] = {
			{
				.id = "bpf",
				.bpf = {
					.bpf_fd = flow->bpf_fd[SEC_L3_L4],
					.annotation = sec_name[SEC_L3_L4],
					.bpf = {
						.action = TC_ACT_PIPE,
					},
				},
			},
		};

		if (add_actions(flow, RTE_DIM(adata), adata,
			TCA_FLOWER_ACT) < 0)
			return -1;
	}

	return 0;
}

/**
 * Get rte_flow operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param ops
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
tap_dev_flow_ops_get(struct rte_eth_dev *dev __rte_unused,
		     const struct rte_flow_ops **ops)
{
	*ops = &tap_flow_ops;
	return 0;
}
