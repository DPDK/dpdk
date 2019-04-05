/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <assert.h>
#include <errno.h>
#include <libmnl/libmnl.h>
#include <linux/gen_stats.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/pkt_cls.h>
#include <linux/pkt_sched.h>
#include <linux/rtnetlink.h>
#include <linux/tc_act/tc_gact.h>
#include <linux/tc_act/tc_mirred.h>
#include <netinet/in.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include "mlx5.h"
#include "mlx5_flow.h"
#include "mlx5_autoconf.h"

#ifdef HAVE_TC_ACT_VLAN

#include <linux/tc_act/tc_vlan.h>

#else /* HAVE_TC_ACT_VLAN */

#define TCA_VLAN_ACT_POP 1
#define TCA_VLAN_ACT_PUSH 2
#define TCA_VLAN_ACT_MODIFY 3
#define TCA_VLAN_PARMS 2
#define TCA_VLAN_PUSH_VLAN_ID 3
#define TCA_VLAN_PUSH_VLAN_PROTOCOL 4
#define TCA_VLAN_PAD 5
#define TCA_VLAN_PUSH_VLAN_PRIORITY 6

struct tc_vlan {
	tc_gen;
	int v_action;
};

#endif /* HAVE_TC_ACT_VLAN */

#ifdef HAVE_TC_ACT_PEDIT

#include <linux/tc_act/tc_pedit.h>

#else /* HAVE_TC_ACT_VLAN */

enum {
	TCA_PEDIT_UNSPEC,
	TCA_PEDIT_TM,
	TCA_PEDIT_PARMS,
	TCA_PEDIT_PAD,
	TCA_PEDIT_PARMS_EX,
	TCA_PEDIT_KEYS_EX,
	TCA_PEDIT_KEY_EX,
	__TCA_PEDIT_MAX
};

enum {
	TCA_PEDIT_KEY_EX_HTYPE = 1,
	TCA_PEDIT_KEY_EX_CMD = 2,
	__TCA_PEDIT_KEY_EX_MAX
};

enum pedit_header_type {
	TCA_PEDIT_KEY_EX_HDR_TYPE_NETWORK = 0,
	TCA_PEDIT_KEY_EX_HDR_TYPE_ETH = 1,
	TCA_PEDIT_KEY_EX_HDR_TYPE_IP4 = 2,
	TCA_PEDIT_KEY_EX_HDR_TYPE_IP6 = 3,
	TCA_PEDIT_KEY_EX_HDR_TYPE_TCP = 4,
	TCA_PEDIT_KEY_EX_HDR_TYPE_UDP = 5,
	__PEDIT_HDR_TYPE_MAX,
};

enum pedit_cmd {
	TCA_PEDIT_KEY_EX_CMD_SET = 0,
	TCA_PEDIT_KEY_EX_CMD_ADD = 1,
	__PEDIT_CMD_MAX,
};

struct tc_pedit_key {
	__u32 mask; /* AND */
	__u32 val; /*XOR */
	__u32 off; /*offset */
	__u32 at;
	__u32 offmask;
	__u32 shift;
};

__extension__
struct tc_pedit_sel {
	tc_gen;
	unsigned char nkeys;
	unsigned char flags;
	struct tc_pedit_key keys[0];
};

#endif /* HAVE_TC_ACT_VLAN */

#ifdef HAVE_TC_ACT_TUNNEL_KEY

#include <linux/tc_act/tc_tunnel_key.h>

#ifndef HAVE_TCA_TUNNEL_KEY_ENC_DST_PORT
#define TCA_TUNNEL_KEY_ENC_DST_PORT 9
#endif

#ifndef HAVE_TCA_TUNNEL_KEY_NO_CSUM
#define TCA_TUNNEL_KEY_NO_CSUM 10
#endif

#ifndef HAVE_TCA_TUNNEL_KEY_ENC_TOS
#define TCA_TUNNEL_KEY_ENC_TOS 12
#endif

#ifndef	HAVE_TCA_TUNNEL_KEY_ENC_TTL
#define TCA_TUNNEL_KEY_ENC_TTL 13
#endif

#else /* HAVE_TC_ACT_TUNNEL_KEY */

#define TCA_ACT_TUNNEL_KEY 17
#define TCA_TUNNEL_KEY_ACT_SET 1
#define TCA_TUNNEL_KEY_ACT_RELEASE 2
#define TCA_TUNNEL_KEY_PARMS 2
#define TCA_TUNNEL_KEY_ENC_IPV4_SRC 3
#define TCA_TUNNEL_KEY_ENC_IPV4_DST 4
#define TCA_TUNNEL_KEY_ENC_IPV6_SRC 5
#define TCA_TUNNEL_KEY_ENC_IPV6_DST 6
#define TCA_TUNNEL_KEY_ENC_KEY_ID 7
#define TCA_TUNNEL_KEY_ENC_DST_PORT 9
#define TCA_TUNNEL_KEY_NO_CSUM 10
#define TCA_TUNNEL_KEY_ENC_TOS 12
#define TCA_TUNNEL_KEY_ENC_TTL 13

struct tc_tunnel_key {
	tc_gen;
	int t_action;
};

#endif /* HAVE_TC_ACT_TUNNEL_KEY */

/* Normally found in linux/netlink.h. */
#ifndef NETLINK_CAP_ACK
#define NETLINK_CAP_ACK 10
#endif

/* Normally found in linux/pkt_sched.h. */
#ifndef TC_H_MIN_INGRESS
#define TC_H_MIN_INGRESS 0xfff2u
#endif

/* Normally found in linux/pkt_cls.h. */
#ifndef TCA_CLS_FLAGS_SKIP_SW
#define TCA_CLS_FLAGS_SKIP_SW (1 << 1)
#endif
#ifndef TCA_CLS_FLAGS_IN_HW
#define TCA_CLS_FLAGS_IN_HW (1 << 2)
#endif
#ifndef HAVE_TCA_CHAIN
#define TCA_CHAIN 11
#endif
#ifndef HAVE_TCA_FLOWER_ACT
#define TCA_FLOWER_ACT 3
#endif
#ifndef HAVE_TCA_FLOWER_FLAGS
#define TCA_FLOWER_FLAGS 22
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_TYPE
#define TCA_FLOWER_KEY_ETH_TYPE 8
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_DST
#define TCA_FLOWER_KEY_ETH_DST 4
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_DST_MASK
#define TCA_FLOWER_KEY_ETH_DST_MASK 5
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_SRC
#define TCA_FLOWER_KEY_ETH_SRC 6
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_SRC_MASK
#define TCA_FLOWER_KEY_ETH_SRC_MASK 7
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IP_PROTO
#define TCA_FLOWER_KEY_IP_PROTO 9
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_SRC
#define TCA_FLOWER_KEY_IPV4_SRC 10
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_SRC_MASK
#define TCA_FLOWER_KEY_IPV4_SRC_MASK 11
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_DST
#define TCA_FLOWER_KEY_IPV4_DST 12
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_DST_MASK
#define TCA_FLOWER_KEY_IPV4_DST_MASK 13
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_SRC
#define TCA_FLOWER_KEY_IPV6_SRC 14
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_SRC_MASK
#define TCA_FLOWER_KEY_IPV6_SRC_MASK 15
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_DST
#define TCA_FLOWER_KEY_IPV6_DST 16
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_DST_MASK
#define TCA_FLOWER_KEY_IPV6_DST_MASK 17
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_SRC
#define TCA_FLOWER_KEY_TCP_SRC 18
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_SRC_MASK
#define TCA_FLOWER_KEY_TCP_SRC_MASK 35
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_DST
#define TCA_FLOWER_KEY_TCP_DST 19
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_DST_MASK
#define TCA_FLOWER_KEY_TCP_DST_MASK 36
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_SRC
#define TCA_FLOWER_KEY_UDP_SRC 20
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_SRC_MASK
#define TCA_FLOWER_KEY_UDP_SRC_MASK 37
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_DST
#define TCA_FLOWER_KEY_UDP_DST 21
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_DST_MASK
#define TCA_FLOWER_KEY_UDP_DST_MASK 38
#endif
#ifndef HAVE_TCA_FLOWER_KEY_VLAN_ID
#define TCA_FLOWER_KEY_VLAN_ID 23
#endif
#ifndef HAVE_TCA_FLOWER_KEY_VLAN_PRIO
#define TCA_FLOWER_KEY_VLAN_PRIO 24
#endif
#ifndef HAVE_TCA_FLOWER_KEY_VLAN_ETH_TYPE
#define TCA_FLOWER_KEY_VLAN_ETH_TYPE 25
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_KEY_ID
#define TCA_FLOWER_KEY_ENC_KEY_ID 26
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV4_SRC
#define TCA_FLOWER_KEY_ENC_IPV4_SRC 27
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK
#define TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK 28
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV4_DST
#define TCA_FLOWER_KEY_ENC_IPV4_DST 29
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV4_DST_MASK
#define TCA_FLOWER_KEY_ENC_IPV4_DST_MASK 30
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV6_SRC
#define TCA_FLOWER_KEY_ENC_IPV6_SRC 31
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK
#define TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK 32
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV6_DST
#define TCA_FLOWER_KEY_ENC_IPV6_DST 33
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_IPV6_DST_MASK
#define TCA_FLOWER_KEY_ENC_IPV6_DST_MASK 34
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_UDP_SRC_PORT
#define TCA_FLOWER_KEY_ENC_UDP_SRC_PORT 43
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK
#define TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK 44
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_UDP_DST_PORT
#define TCA_FLOWER_KEY_ENC_UDP_DST_PORT 45
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK
#define TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK 46
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_FLAGS
#define TCA_FLOWER_KEY_TCP_FLAGS 71
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_FLAGS_MASK
#define TCA_FLOWER_KEY_TCP_FLAGS_MASK 72
#endif
#ifndef	HAVE_TCA_FLOWER_KEY_IP_TOS
#define	TCA_FLOWER_KEY_IP_TOS 73
#endif
#ifndef	HAVE_TCA_FLOWER_KEY_IP_TOS_MASK
#define TCA_FLOWER_KEY_IP_TOS_MASK 74
#endif
#ifndef	HAVE_TCA_FLOWER_KEY_IP_TTL
#define TCA_FLOWER_KEY_IP_TTL 75
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IP_TTL_MASK
#define TCA_FLOWER_KEY_IP_TTL_MASK 76
#endif
#ifndef	HAVE_TCA_FLOWER_KEY_ENC_IP_TOS
#define TCA_FLOWER_KEY_ENC_IP_TOS 80
#endif
#ifndef	HAVE_TCA_FLOWER_KEY_ENC_IP_TOS_MASK
#define TCA_FLOWER_KEY_ENC_IP_TOS_MASK 81
#endif
#ifndef	HAVE_TCA_FLOWER_KEY_ENC_IP_TTL
#define	TCA_FLOWER_KEY_ENC_IP_TTL 82
#endif
#ifndef	HAVE_TCA_FLOWER_KEY_ENC_IP_TTL_MASK
#define TCA_FLOWER_KEY_ENC_IP_TTL_MASK 83
#endif

#ifndef HAVE_TC_ACT_GOTO_CHAIN
#define TC_ACT_GOTO_CHAIN 0x20000000
#endif

#ifndef IPV6_ADDR_LEN
#define IPV6_ADDR_LEN 16
#endif

#ifndef IPV4_ADDR_LEN
#define IPV4_ADDR_LEN 4
#endif

#ifndef TP_PORT_LEN
#define TP_PORT_LEN 2 /* Transport Port (UDP/TCP) Length */
#endif

#ifndef TTL_LEN
#define TTL_LEN 1
#endif

#ifndef TCA_ACT_MAX_PRIO
#define TCA_ACT_MAX_PRIO 32
#endif

/** Parameters of VXLAN devices created by driver. */
#define MLX5_VXLAN_DEFAULT_VNI	1
#define MLX5_VXLAN_DEVICE_PFX "vmlx_"
/**
 * Timeout in milliseconds to wait VXLAN UDP offloaded port
 * registration  completed within the mlx5 driver.
 */
#define MLX5_VXLAN_WAIT_PORT_REG_MS 250

/** Tunnel action type, used for @p type in header structure. */
enum flow_tcf_tunact_type {
	FLOW_TCF_TUNACT_VXLAN_DECAP,
	FLOW_TCF_TUNACT_VXLAN_ENCAP,
};

/** Flags used for @p mask in tunnel action encap descriptors. */
#define FLOW_TCF_ENCAP_ETH_SRC (1u << 0)
#define FLOW_TCF_ENCAP_ETH_DST (1u << 1)
#define FLOW_TCF_ENCAP_IPV4_SRC (1u << 2)
#define FLOW_TCF_ENCAP_IPV4_DST (1u << 3)
#define FLOW_TCF_ENCAP_IPV6_SRC (1u << 4)
#define FLOW_TCF_ENCAP_IPV6_DST (1u << 5)
#define FLOW_TCF_ENCAP_UDP_SRC (1u << 6)
#define FLOW_TCF_ENCAP_UDP_DST (1u << 7)
#define FLOW_TCF_ENCAP_VXLAN_VNI (1u << 8)
#define FLOW_TCF_ENCAP_IP_TTL (1u << 9)
#define FLOW_TCF_ENCAP_IP_TOS (1u << 10)

/**
 * Structure for holding netlink context.
 * Note the size of the message buffer which is MNL_SOCKET_BUFFER_SIZE.
 * Using this (8KB) buffer size ensures that netlink messages will never be
 * truncated.
 */
struct mlx5_flow_tcf_context {
	struct mnl_socket *nl; /* NETLINK_ROUTE libmnl socket. */
	uint32_t seq; /* Message sequence number. */
	uint32_t buf_size; /* Message buffer size. */
	uint8_t *buf; /* Message buffer. */
};

/**
 * Neigh rule structure. The neigh rule is applied via Netlink to
 * outer tunnel iface in order to provide destination MAC address
 * for the VXLAN encapsultion. The neigh rule is implicitly related
 * to the Flow itself and can be shared by multiple Flows.
 */
struct tcf_neigh_rule {
	LIST_ENTRY(tcf_neigh_rule) next;
	uint32_t refcnt;
	struct ether_addr eth;
	uint16_t mask;
	union {
		struct {
			rte_be32_t dst;
		} ipv4;
		struct {
			uint8_t dst[IPV6_ADDR_LEN];
		} ipv6;
	};
};

/**
 * Local rule structure. The local rule is applied via Netlink to
 * outer tunnel iface in order to provide local and peer IP addresses
 * of the VXLAN tunnel for encapsulation. The local rule is implicitly
 * related to the Flow itself and can be shared by multiple Flows.
 */
struct tcf_local_rule {
	LIST_ENTRY(tcf_local_rule) next;
	uint32_t refcnt;
	uint16_t mask;
	union {
		struct {
			rte_be32_t dst;
			rte_be32_t src;
		} ipv4;
		struct {
			uint8_t dst[IPV6_ADDR_LEN];
			uint8_t src[IPV6_ADDR_LEN];
		} ipv6;
	};
};

/** Outer interface VXLAN encapsulation rules container. */
struct tcf_irule {
	LIST_ENTRY(tcf_irule) next;
	LIST_HEAD(, tcf_neigh_rule) neigh;
	LIST_HEAD(, tcf_local_rule) local;
	uint32_t refcnt;
	unsigned int ifouter; /**< Own interface index. */
};

/** VXLAN virtual netdev. */
struct tcf_vtep {
	LIST_ENTRY(tcf_vtep) next;
	uint32_t refcnt;
	unsigned int ifindex; /**< Own interface index. */
	uint16_t port;
	uint32_t created:1; /**< Actually created by PMD. */
	uint32_t waitreg:1; /**< Wait for VXLAN UDP port registration. */
};

/** Tunnel descriptor header, common for all tunnel types. */
struct flow_tcf_tunnel_hdr {
	uint32_t type; /**< Tunnel action type. */
	struct tcf_vtep *vtep; /**< Virtual tunnel endpoint device. */
	unsigned int ifindex_org; /**< Original dst/src interface */
	unsigned int *ifindex_ptr; /**< Interface ptr in message. */
};

struct flow_tcf_vxlan_decap {
	struct flow_tcf_tunnel_hdr hdr;
	uint16_t udp_port;
};

struct flow_tcf_vxlan_encap {
	struct flow_tcf_tunnel_hdr hdr;
	struct tcf_irule *iface;
	uint32_t mask;
	uint8_t ip_tos;
	uint8_t ip_ttl_hop;
	struct {
		struct ether_addr dst;
		struct ether_addr src;
	} eth;
	union {
		struct {
			rte_be32_t dst;
			rte_be32_t src;
		} ipv4;
		struct {
			uint8_t dst[IPV6_ADDR_LEN];
			uint8_t src[IPV6_ADDR_LEN];
		} ipv6;
	};
	struct {
		rte_be16_t src;
		rte_be16_t dst;
	} udp;
	struct {
		uint8_t vni[3];
	} vxlan;
};

/** Structure used when extracting the values of a flow counters
 * from a netlink message.
 */
struct flow_tcf_stats_basic {
	bool valid;
	struct gnet_stats_basic counters;
};

/** Empty masks for known item types. */
static const union {
	struct rte_flow_item_port_id port_id;
	struct rte_flow_item_eth eth;
	struct rte_flow_item_vlan vlan;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_udp udp;
	struct rte_flow_item_vxlan vxlan;
} flow_tcf_mask_empty = {
	{0},
};

/** Supported masks for known item types. */
static const struct {
	struct rte_flow_item_port_id port_id;
	struct rte_flow_item_eth eth;
	struct rte_flow_item_vlan vlan;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_udp udp;
	struct rte_flow_item_vxlan vxlan;
} flow_tcf_mask_supported = {
	.port_id = {
		.id = 0xffffffff,
	},
	.eth = {
		.type = RTE_BE16(0xffff),
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	},
	.vlan = {
		/* PCP and VID only, no DEI. */
		.tci = RTE_BE16(0xefff),
		.inner_type = RTE_BE16(0xffff),
	},
	.ipv4.hdr = {
		.next_proto_id = 0xff,
		.time_to_live = 0xff,
		.type_of_service = 0xff,
		.src_addr = RTE_BE32(0xffffffff),
		.dst_addr = RTE_BE32(0xffffffff),
	},
	.ipv6.hdr = {
		.proto = 0xff,
		.vtc_flow = RTE_BE32(0xfful << IPV6_HDR_FL_SHIFT),
		.hop_limits = 0xff,
		.src_addr =
			"\xff\xff\xff\xff\xff\xff\xff\xff"
			"\xff\xff\xff\xff\xff\xff\xff\xff",
		.dst_addr =
			"\xff\xff\xff\xff\xff\xff\xff\xff"
			"\xff\xff\xff\xff\xff\xff\xff\xff",
	},
	.tcp.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
		.tcp_flags = 0xff,
	},
	.udp.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
	.vxlan = {
	       .vni = "\xff\xff\xff",
	},
};

#define SZ_NLATTR_HDR MNL_ALIGN(sizeof(struct nlattr))
#define SZ_NLATTR_NEST SZ_NLATTR_HDR
#define SZ_NLATTR_DATA_OF(len) MNL_ALIGN(SZ_NLATTR_HDR + (len))
#define SZ_NLATTR_TYPE_OF(typ) SZ_NLATTR_DATA_OF(sizeof(typ))
#define SZ_NLATTR_STRZ_OF(str) SZ_NLATTR_DATA_OF(strlen(str) + 1)

#define PTOI_TABLE_SZ_MAX(dev) (mlx5_dev_to_port_id((dev)->device, NULL, 0) + 2)

/** DPDK port to network interface index (ifindex) conversion. */
struct flow_tcf_ptoi {
	uint16_t port_id; /**< DPDK port ID. */
	unsigned int ifindex; /**< Network interface index. */
};

/* Due to a limitation on driver/FW. */
#define MLX5_TCF_GROUP_ID_MAX 3

/*
 * Due to a limitation on driver/FW, priority ranges from 1 to 16 in kernel.
 * Priority in rte_flow attribute starts from 0 and is added by 1 in
 * translation. This is subject to be changed to determine the max priority
 * based on trial-and-error like Verbs driver once the restriction is lifted or
 * the range is extended.
 */
#define MLX5_TCF_GROUP_PRIORITY_MAX 15

#define MLX5_TCF_FATE_ACTIONS \
	(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_PORT_ID | \
	 MLX5_FLOW_ACTION_JUMP)

#define MLX5_TCF_VLAN_ACTIONS \
	(MLX5_FLOW_ACTION_OF_POP_VLAN | MLX5_FLOW_ACTION_OF_PUSH_VLAN | \
	 MLX5_FLOW_ACTION_OF_SET_VLAN_VID | MLX5_FLOW_ACTION_OF_SET_VLAN_PCP)

#define MLX5_TCF_VXLAN_ACTIONS \
	(MLX5_FLOW_ACTION_VXLAN_ENCAP | MLX5_FLOW_ACTION_VXLAN_DECAP)

#define MLX5_TCF_PEDIT_ACTIONS \
	(MLX5_FLOW_ACTION_SET_IPV4_SRC | MLX5_FLOW_ACTION_SET_IPV4_DST | \
	 MLX5_FLOW_ACTION_SET_IPV6_SRC | MLX5_FLOW_ACTION_SET_IPV6_DST | \
	 MLX5_FLOW_ACTION_SET_TP_SRC | MLX5_FLOW_ACTION_SET_TP_DST | \
	 MLX5_FLOW_ACTION_SET_TTL | MLX5_FLOW_ACTION_DEC_TTL | \
	 MLX5_FLOW_ACTION_SET_MAC_SRC | MLX5_FLOW_ACTION_SET_MAC_DST)

#define MLX5_TCF_CONFIG_ACTIONS \
	(MLX5_FLOW_ACTION_PORT_ID | MLX5_FLOW_ACTION_JUMP | \
	 MLX5_FLOW_ACTION_OF_PUSH_VLAN | MLX5_FLOW_ACTION_OF_SET_VLAN_VID | \
	 MLX5_FLOW_ACTION_OF_SET_VLAN_PCP | \
	 (MLX5_TCF_PEDIT_ACTIONS & ~MLX5_FLOW_ACTION_DEC_TTL))

#define MAX_PEDIT_KEYS 128
#define SZ_PEDIT_KEY_VAL 4

#define NUM_OF_PEDIT_KEYS(sz) \
	(((sz) / SZ_PEDIT_KEY_VAL) + (((sz) % SZ_PEDIT_KEY_VAL) ? 1 : 0))

struct pedit_key_ex {
	enum pedit_header_type htype;
	enum pedit_cmd cmd;
};

struct pedit_parser {
	struct tc_pedit_sel sel;
	struct tc_pedit_key keys[MAX_PEDIT_KEYS];
	struct pedit_key_ex keys_ex[MAX_PEDIT_KEYS];
};

/**
 * Create space for using the implicitly created TC flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   A pointer to the counter data structure, NULL otherwise and
 *   rte_errno is set.
 */
static struct mlx5_flow_counter *
flow_tcf_counter_new(void)
{
	struct mlx5_flow_counter *cnt;

	/*
	 * eswitch counter cannot be shared and its id is unknown.
	 * currently returning all with id 0.
	 * in the future maybe better to switch to unique numbers.
	 */
	struct mlx5_flow_counter tmpl = {
		.ref_cnt = 1,
	};
	cnt = rte_calloc(__func__, 1, sizeof(*cnt), 0);
	if (!cnt) {
		rte_errno = ENOMEM;
		return NULL;
	}
	*cnt = tmpl;
	/* Implicit counter, do not add to list. */
	return cnt;
}

/**
 * Set pedit key of MAC address
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 */
static void
flow_tcf_pedit_key_set_mac(const struct rte_flow_action *actions,
			   struct pedit_parser *p_parser)
{
	int idx = p_parser->sel.nkeys;
	uint32_t off = actions->type == RTE_FLOW_ACTION_TYPE_SET_MAC_SRC ?
					offsetof(struct ether_hdr, s_addr) :
					offsetof(struct ether_hdr, d_addr);
	const struct rte_flow_action_set_mac *conf =
		(const struct rte_flow_action_set_mac *)actions->conf;

	p_parser->keys[idx].off = off;
	p_parser->keys[idx].mask = ~UINT32_MAX;
	p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_ETH;
	p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
	memcpy(&p_parser->keys[idx].val,
		conf->mac_addr, SZ_PEDIT_KEY_VAL);
	idx++;
	p_parser->keys[idx].off = off + SZ_PEDIT_KEY_VAL;
	p_parser->keys[idx].mask = 0xFFFF0000;
	p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_ETH;
	p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
	memcpy(&p_parser->keys[idx].val,
		conf->mac_addr + SZ_PEDIT_KEY_VAL,
		ETHER_ADDR_LEN - SZ_PEDIT_KEY_VAL);
	p_parser->sel.nkeys = (++idx);
}

/**
 * Set pedit key of decrease/set ttl
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 * @param[in] item_flags
 *   flags of all items presented
 */
static void
flow_tcf_pedit_key_set_dec_ttl(const struct rte_flow_action *actions,
				struct pedit_parser *p_parser,
				uint64_t item_flags)
{
	int idx = p_parser->sel.nkeys;

	p_parser->keys[idx].mask = 0xFFFFFF00;
	if (item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV4) {
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_IP4;
		p_parser->keys[idx].off =
			offsetof(struct ipv4_hdr, time_to_live);
	}
	if (item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV6) {
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_IP6;
		p_parser->keys[idx].off =
			offsetof(struct ipv6_hdr, hop_limits);
	}
	if (actions->type == RTE_FLOW_ACTION_TYPE_DEC_TTL) {
		p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_ADD;
		p_parser->keys[idx].val = 0x000000FF;
	} else {
		p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
		p_parser->keys[idx].val =
			(__u32)((const struct rte_flow_action_set_ttl *)
			 actions->conf)->ttl_value;
	}
	p_parser->sel.nkeys = (++idx);
}

/**
 * Set pedit key of transport (TCP/UDP) port value
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 * @param[in] item_flags
 *   flags of all items presented
 */
static void
flow_tcf_pedit_key_set_tp_port(const struct rte_flow_action *actions,
				struct pedit_parser *p_parser,
				uint64_t item_flags)
{
	int idx = p_parser->sel.nkeys;

	if (item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP)
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_UDP;
	if (item_flags & MLX5_FLOW_LAYER_OUTER_L4_TCP)
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_TCP;
	p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
	/* offset of src/dst port is same for TCP and UDP */
	p_parser->keys[idx].off =
		actions->type == RTE_FLOW_ACTION_TYPE_SET_TP_SRC ?
		offsetof(struct tcp_hdr, src_port) :
		offsetof(struct tcp_hdr, dst_port);
	p_parser->keys[idx].mask = 0xFFFF0000;
	p_parser->keys[idx].val =
		(__u32)((const struct rte_flow_action_set_tp *)
				actions->conf)->port;
	p_parser->sel.nkeys = (++idx);
}

/**
 * Set pedit key of ipv6 address
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 */
static void
flow_tcf_pedit_key_set_ipv6_addr(const struct rte_flow_action *actions,
				 struct pedit_parser *p_parser)
{
	int idx = p_parser->sel.nkeys;
	int keys = NUM_OF_PEDIT_KEYS(IPV6_ADDR_LEN);
	int off_base =
		actions->type == RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC ?
		offsetof(struct ipv6_hdr, src_addr) :
		offsetof(struct ipv6_hdr, dst_addr);
	const struct rte_flow_action_set_ipv6 *conf =
		(const struct rte_flow_action_set_ipv6 *)actions->conf;

	for (int i = 0; i < keys; i++, idx++) {
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_IP6;
		p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
		p_parser->keys[idx].off = off_base + i * SZ_PEDIT_KEY_VAL;
		p_parser->keys[idx].mask = ~UINT32_MAX;
		memcpy(&p_parser->keys[idx].val,
			conf->ipv6_addr + i *  SZ_PEDIT_KEY_VAL,
			SZ_PEDIT_KEY_VAL);
	}
	p_parser->sel.nkeys += keys;
}

/**
 * Set pedit key of ipv4 address
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 */
static void
flow_tcf_pedit_key_set_ipv4_addr(const struct rte_flow_action *actions,
				 struct pedit_parser *p_parser)
{
	int idx = p_parser->sel.nkeys;

	p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_IP4;
	p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
	p_parser->keys[idx].off =
		actions->type == RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC ?
		offsetof(struct ipv4_hdr, src_addr) :
		offsetof(struct ipv4_hdr, dst_addr);
	p_parser->keys[idx].mask = ~UINT32_MAX;
	p_parser->keys[idx].val =
		((const struct rte_flow_action_set_ipv4 *)
		 actions->conf)->ipv4_addr;
	p_parser->sel.nkeys = (++idx);
}

/**
 * Create the pedit's na attribute in netlink message
 * on pre-allocate message buffer
 *
 * @param[in,out] nl
 *   pointer to pre-allocated netlink message buffer
 * @param[in,out] actions
 *   pointer to pointer of actions specification.
 * @param[in,out] action_flags
 *   pointer to actions flags
 * @param[in] item_flags
 *   flags of all item presented
 */
static void
flow_tcf_create_pedit_mnl_msg(struct nlmsghdr *nl,
			      const struct rte_flow_action **actions,
			      uint64_t item_flags)
{
	struct pedit_parser p_parser;
	struct nlattr *na_act_options;
	struct nlattr *na_pedit_keys;

	memset(&p_parser, 0, sizeof(p_parser));
	mnl_attr_put_strz(nl, TCA_ACT_KIND, "pedit");
	na_act_options = mnl_attr_nest_start(nl, TCA_ACT_OPTIONS);
	/* all modify header actions should be in one tc-pedit action */
	for (; (*actions)->type != RTE_FLOW_ACTION_TYPE_END; (*actions)++) {
		switch ((*actions)->type) {
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			flow_tcf_pedit_key_set_ipv4_addr(*actions, &p_parser);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			flow_tcf_pedit_key_set_ipv6_addr(*actions, &p_parser);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			flow_tcf_pedit_key_set_tp_port(*actions,
							&p_parser, item_flags);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
			flow_tcf_pedit_key_set_dec_ttl(*actions,
							&p_parser, item_flags);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			flow_tcf_pedit_key_set_mac(*actions, &p_parser);
			break;
		default:
			goto pedit_mnl_msg_done;
		}
	}
pedit_mnl_msg_done:
	p_parser.sel.action = TC_ACT_PIPE;
	mnl_attr_put(nl, TCA_PEDIT_PARMS_EX,
		     sizeof(p_parser.sel) +
		     p_parser.sel.nkeys * sizeof(struct tc_pedit_key),
		     &p_parser);
	na_pedit_keys =
		mnl_attr_nest_start(nl, TCA_PEDIT_KEYS_EX | NLA_F_NESTED);
	for (int i = 0; i < p_parser.sel.nkeys; i++) {
		struct nlattr *na_pedit_key =
			mnl_attr_nest_start(nl,
					    TCA_PEDIT_KEY_EX | NLA_F_NESTED);
		mnl_attr_put_u16(nl, TCA_PEDIT_KEY_EX_HTYPE,
				 p_parser.keys_ex[i].htype);
		mnl_attr_put_u16(nl, TCA_PEDIT_KEY_EX_CMD,
				 p_parser.keys_ex[i].cmd);
		mnl_attr_nest_end(nl, na_pedit_key);
	}
	mnl_attr_nest_end(nl, na_pedit_keys);
	mnl_attr_nest_end(nl, na_act_options);
	(*actions)--;
}

/**
 * Calculate max memory size of one TC-pedit actions.
 * One TC-pedit action can contain set of keys each defining
 * a rewrite element (rte_flow action)
 *
 * @param[in,out] actions
 *   actions specification.
 * @param[in,out] action_flags
 *   actions flags
 * @param[in,out] size
 *   accumulated size
 * @return
 *   Max memory size of one TC-pedit action
 */
static int
flow_tcf_get_pedit_actions_size(const struct rte_flow_action **actions,
				uint64_t *action_flags)
{
	int pedit_size = 0;
	int keys = 0;
	uint64_t flags = 0;

	pedit_size += SZ_NLATTR_NEST + /* na_act_index. */
		      SZ_NLATTR_STRZ_OF("pedit") +
		      SZ_NLATTR_NEST; /* TCA_ACT_OPTIONS. */
	for (; (*actions)->type != RTE_FLOW_ACTION_TYPE_END; (*actions)++) {
		switch ((*actions)->type) {
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
			keys += NUM_OF_PEDIT_KEYS(IPV4_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV4_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			keys += NUM_OF_PEDIT_KEYS(IPV4_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV4_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
			keys += NUM_OF_PEDIT_KEYS(IPV6_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV6_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			keys += NUM_OF_PEDIT_KEYS(IPV6_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV6_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
			/* TCP is as same as UDP */
			keys += NUM_OF_PEDIT_KEYS(TP_PORT_LEN);
			flags |= MLX5_FLOW_ACTION_SET_TP_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			/* TCP is as same as UDP */
			keys += NUM_OF_PEDIT_KEYS(TP_PORT_LEN);
			flags |= MLX5_FLOW_ACTION_SET_TP_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
			keys += NUM_OF_PEDIT_KEYS(TTL_LEN);
			flags |= MLX5_FLOW_ACTION_SET_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
			keys += NUM_OF_PEDIT_KEYS(TTL_LEN);
			flags |= MLX5_FLOW_ACTION_DEC_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
			keys += NUM_OF_PEDIT_KEYS(ETHER_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_MAC_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			keys += NUM_OF_PEDIT_KEYS(ETHER_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_MAC_DST;
			break;
		default:
			goto get_pedit_action_size_done;
		}
	}
get_pedit_action_size_done:
	/* TCA_PEDIT_PARAMS_EX */
	pedit_size +=
		SZ_NLATTR_DATA_OF(sizeof(struct tc_pedit_sel) +
				  keys * sizeof(struct tc_pedit_key));
	pedit_size += SZ_NLATTR_NEST; /* TCA_PEDIT_KEYS */
	pedit_size += keys *
		      /* TCA_PEDIT_KEY_EX + HTYPE + CMD */
		      (SZ_NLATTR_NEST + SZ_NLATTR_DATA_OF(2) +
		       SZ_NLATTR_DATA_OF(2));
	(*action_flags) |= flags;
	(*actions)--;
	return pedit_size;
}

/**
 * Retrieve mask for pattern item.
 *
 * This function does basic sanity checks on a pattern item in order to
 * return the most appropriate mask for it.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] mask_default
 *   Default mask for pattern item as specified by the flow API.
 * @param[in] mask_supported
 *   Mask fields supported by the implementation.
 * @param[in] mask_empty
 *   Empty mask to return when there is no specification.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   Either @p item->mask or one of the mask parameters on success, NULL
 *   otherwise and rte_errno is set.
 */
static const void *
flow_tcf_item_mask(const struct rte_flow_item *item, const void *mask_default,
		   const void *mask_supported, const void *mask_empty,
		   size_t mask_size, struct rte_flow_error *error)
{
	const uint8_t *mask;
	size_t i;

	/* item->last and item->mask cannot exist without item->spec. */
	if (!item->spec && (item->mask || item->last)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "\"mask\" or \"last\" field provided without"
				   " a corresponding \"spec\"");
		return NULL;
	}
	/* No spec, no mask, no problem. */
	if (!item->spec)
		return mask_empty;
	mask = item->mask ? item->mask : mask_default;
	assert(mask);
	/*
	 * Single-pass check to make sure that:
	 * - Mask is supported, no bits are set outside mask_supported.
	 * - Both item->spec and item->last are included in mask.
	 */
	for (i = 0; i != mask_size; ++i) {
		if (!mask[i])
			continue;
		if ((mask[i] | ((const uint8_t *)mask_supported)[i]) !=
		    ((const uint8_t *)mask_supported)[i]) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					   "unsupported field found"
					   " in \"mask\"");
			return NULL;
		}
		if (item->last &&
		    (((const uint8_t *)item->spec)[i] & mask[i]) !=
		    (((const uint8_t *)item->last)[i] & mask[i])) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM_LAST,
					   item->last,
					   "range between \"spec\" and \"last\""
					   " not comprised in \"mask\"");
			return NULL;
		}
	}
	return mask;
}

/**
 * Build a conversion table between port ID and ifindex.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] ptoi
 *   Pointer to ptoi table.
 * @param[in] len
 *   Size of ptoi table provided.
 *
 * @return
 *   Size of ptoi table filled.
 */
static unsigned int
flow_tcf_build_ptoi_table(struct rte_eth_dev *dev, struct flow_tcf_ptoi *ptoi,
			  unsigned int len)
{
	unsigned int n = mlx5_dev_to_port_id(dev->device, NULL, 0);
	uint16_t port_id[n + 1];
	unsigned int i;
	unsigned int own = 0;

	/* At least one port is needed when no switch domain is present. */
	if (!n) {
		n = 1;
		port_id[0] = dev->data->port_id;
	} else {
		n = RTE_MIN(mlx5_dev_to_port_id(dev->device, port_id, n), n);
	}
	if (n > len)
		return 0;
	for (i = 0; i != n; ++i) {
		struct rte_eth_dev_info dev_info;

		rte_eth_dev_info_get(port_id[i], &dev_info);
		if (port_id[i] == dev->data->port_id)
			own = i;
		ptoi[i].port_id = port_id[i];
		ptoi[i].ifindex = dev_info.if_index;
	}
	/* Ensure first entry of ptoi[] is the current device. */
	if (own) {
		ptoi[n] = ptoi[0];
		ptoi[0] = ptoi[own];
		ptoi[own] = ptoi[n];
	}
	/* An entry with zero ifindex terminates ptoi[]. */
	ptoi[n].port_id = 0;
	ptoi[n].ifindex = 0;
	return n;
}

/**
 * Verify the @p attr will be correctly understood by the E-switch.
 *
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_validate_attributes(const struct rte_flow_attr *attr,
			     struct rte_flow_error *error)
{
	/*
	 * Supported attributes: groups, some priorities and ingress only.
	 * group is supported only if kernel supports chain. Don't care about
	 * transfer as it is the caller's problem.
	 */
	if (attr->group > MLX5_TCF_GROUP_ID_MAX)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP, attr,
					  "group ID larger than "
					  RTE_STR(MLX5_TCF_GROUP_ID_MAX)
					  " isn't supported");
	else if (attr->priority > MLX5_TCF_GROUP_PRIORITY_MAX)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  attr,
					  "priority more than "
					  RTE_STR(MLX5_TCF_GROUP_PRIORITY_MAX)
					  " is not supported");
	if (!attr->ingress)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  attr, "only ingress is supported");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  attr, "egress is not supported");
	return 0;
}

/**
 * Validate VXLAN_ENCAP action RTE_FLOW_ITEM_TYPE_ETH item for E-Switch.
 * The routine checks the L2 fields to be used in encapsulation header.
 *
 * @param[in] item
 *   Pointer to the item structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 **/
static int
flow_tcf_validate_vxlan_encap_eth(const struct rte_flow_item *item,
				  struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;

	if (!spec) {
		/*
		 * Specification for L2 addresses can be empty
		 * because these ones are optional and not
		 * required directly by tc rule. Kernel tries
		 * to resolve these ones on its own
		 */
		return 0;
	}
	if (!mask) {
		/* If mask is not specified use the default one. */
		mask = &rte_flow_item_eth_mask;
	}
	if (memcmp(&mask->dst,
		   &flow_tcf_mask_empty.eth.dst,
		   sizeof(flow_tcf_mask_empty.eth.dst))) {
		if (memcmp(&mask->dst,
			   &rte_flow_item_eth_mask.dst,
			   sizeof(rte_flow_item_eth_mask.dst)))
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
				 "no support for partial mask on"
				 " \"eth.dst\" field");
	}
	if (memcmp(&mask->src,
		   &flow_tcf_mask_empty.eth.src,
		   sizeof(flow_tcf_mask_empty.eth.src))) {
		if (memcmp(&mask->src,
			   &rte_flow_item_eth_mask.src,
			   sizeof(rte_flow_item_eth_mask.src)))
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
				 "no support for partial mask on"
				 " \"eth.src\" field");
	}
	if (mask->type != RTE_BE16(0x0000)) {
		if (mask->type != RTE_BE16(0xffff))
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
				 "no support for partial mask on"
				 " \"eth.type\" field");
		DRV_LOG(WARNING,
			"outer ethernet type field"
			" cannot be forced for vxlan"
			" encapsulation, parameter ignored");
	}
	return 0;
}

/**
 * Validate VXLAN_ENCAP action RTE_FLOW_ITEM_TYPE_IPV4 item for E-Switch.
 * The routine checks the IPv4 fields to be used in encapsulation header.
 *
 * @param[in] item
 *   Pointer to the item structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 **/
static int
flow_tcf_validate_vxlan_encap_ipv4(const struct rte_flow_item *item,
				   struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;

	if (!spec) {
		/*
		 * Specification for IP addresses cannot be empty
		 * because it is required by tunnel_key parameter.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "NULL outer ipv4 address"
					  " specification for vxlan"
					  " encapsulation");
	}
	if (!mask)
		mask = &rte_flow_item_ipv4_mask;
	if (mask->hdr.dst_addr != RTE_BE32(0x00000000)) {
		if (mask->hdr.dst_addr != RTE_BE32(0xffffffff))
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
				 "no support for partial mask on"
				 " \"ipv4.hdr.dst_addr\" field"
				 " for vxlan encapsulation");
		/* More IPv4 address validations can be put here. */
	} else {
		/*
		 * Kernel uses the destination IP address to determine
		 * the routing path and obtain the MAC destination
		 * address, so IP destination address must be
		 * specified in the tc rule.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "outer ipv4 destination address"
					  " must be specified for"
					  " vxlan encapsulation");
	}
	if (mask->hdr.src_addr != RTE_BE32(0x00000000)) {
		if (mask->hdr.src_addr != RTE_BE32(0xffffffff))
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
				 "no support for partial mask on"
				 " \"ipv4.hdr.src_addr\" field"
				 " for vxlan encapsulation");
		/* More IPv4 address validations can be put here. */
	} else {
		/*
		 * Kernel uses the source IP address to select the
		 * interface for egress encapsulated traffic, so
		 * it must be specified in the tc rule.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "outer ipv4 source address"
					  " must be specified for"
					  " vxlan encapsulation");
	}
	if (mask->hdr.type_of_service &&
	    mask->hdr.type_of_service != 0xff)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "no support for partial mask on"
					  " \"ipv4.hdr.type_of_service\" field"
					  " for vxlan encapsulation");
	if (mask->hdr.time_to_live &&
	    mask->hdr.time_to_live != 0xff)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "no support for partial mask on"
					  " \"ipv4.hdr.time_to_live\" field"
					  " for vxlan encapsulation");
	return 0;
}

/**
 * Validate VXLAN_ENCAP action RTE_FLOW_ITEM_TYPE_IPV6 item for E-Switch.
 * The routine checks the IPv6 fields to be used in encapsulation header.
 *
 * @param[in] item
 *   Pointer to the item structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 **/
static int
flow_tcf_validate_vxlan_encap_ipv6(const struct rte_flow_item *item,
				   struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	uint8_t msk6;

	if (!spec) {
		/*
		 * Specification for IP addresses cannot be empty
		 * because it is required by tunnel_key parameter.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "NULL outer ipv6 address"
					  " specification for"
					  " vxlan encapsulation");
	}
	if (!mask)
		mask = &rte_flow_item_ipv6_mask;
	if (memcmp(&mask->hdr.dst_addr,
		   &flow_tcf_mask_empty.ipv6.hdr.dst_addr,
		   IPV6_ADDR_LEN)) {
		if (memcmp(&mask->hdr.dst_addr,
			   &rte_flow_item_ipv6_mask.hdr.dst_addr,
			   IPV6_ADDR_LEN))
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					 "no support for partial mask on"
					 " \"ipv6.hdr.dst_addr\" field"
					 " for vxlan encapsulation");
		/* More IPv6 address validations can be put here. */
	} else {
		/*
		 * Kernel uses the destination IP address to determine
		 * the routing path and obtain the MAC destination
		 * address (heigh or gate), so IP destination address
		 * must be specified within the tc rule.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "outer ipv6 destination address"
					  " must be specified for"
					  " vxlan encapsulation");
	}
	if (memcmp(&mask->hdr.src_addr,
		   &flow_tcf_mask_empty.ipv6.hdr.src_addr,
		   IPV6_ADDR_LEN)) {
		if (memcmp(&mask->hdr.src_addr,
			   &rte_flow_item_ipv6_mask.hdr.src_addr,
			   IPV6_ADDR_LEN))
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					 "no support for partial mask on"
					 " \"ipv6.hdr.src_addr\" field"
					 " for vxlan encapsulation");
		/* More L3 address validation can be put here. */
	} else {
		/*
		 * Kernel uses the source IP address to select the
		 * interface for egress encapsulated traffic, so
		 * it must be specified in the tc rule.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "outer L3 source address"
					  " must be specified for"
					  " vxlan encapsulation");
	}
	msk6 = (rte_be_to_cpu_32(mask->hdr.vtc_flow) >>
		IPV6_HDR_TC_SHIFT) & 0xff;
	if (msk6 && msk6 != 0xff)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "no support for partial mask on"
					  " \"ipv6.hdr.vtc_flow.tos\" field"
					  " for vxlan encapsulation");
	if (mask->hdr.hop_limits && mask->hdr.hop_limits != 0xff)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "no support for partial mask on"
					  " \"ipv6.hdr.hop_limits\" field"
					  " for vxlan encapsulation");
	return 0;
}

/**
 * Validate VXLAN_ENCAP action RTE_FLOW_ITEM_TYPE_UDP item for E-Switch.
 * The routine checks the UDP fields to be used in encapsulation header.
 *
 * @param[in] item
 *   Pointer to the item structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 **/
static int
flow_tcf_validate_vxlan_encap_udp(const struct rte_flow_item *item,
				  struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;

	if (!spec) {
		/*
		 * Specification for UDP ports cannot be empty
		 * because it is required by tunnel_key parameter.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "NULL UDP port specification "
					  " for vxlan encapsulation");
	}
	if (!mask)
		mask = &rte_flow_item_udp_mask;
	if (mask->hdr.dst_port != RTE_BE16(0x0000)) {
		if (mask->hdr.dst_port != RTE_BE16(0xffff))
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					 "no support for partial mask on"
					 " \"udp.hdr.dst_port\" field"
					 " for vxlan encapsulation");
		if (!spec->hdr.dst_port)
			return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, item,
					 "outer UDP remote port cannot be"
					 " 0 for vxlan encapsulation");
	} else {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "outer UDP remote port"
					  " must be specified for"
					  " vxlan encapsulation");
	}
	if (mask->hdr.src_port != RTE_BE16(0x0000)) {
		if (mask->hdr.src_port != RTE_BE16(0xffff))
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					 "no support for partial mask on"
					 " \"udp.hdr.src_port\" field"
					 " for vxlan encapsulation");
		DRV_LOG(WARNING,
			"outer UDP source port cannot be"
			" forced for vxlan encapsulation,"
			" parameter ignored");
	}
	return 0;
}

/**
 * Validate VXLAN_ENCAP action RTE_FLOW_ITEM_TYPE_VXLAN item for E-Switch.
 * The routine checks the VNIP fields to be used in encapsulation header.
 *
 * @param[in] item
 *   Pointer to the item structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 **/
static int
flow_tcf_validate_vxlan_encap_vni(const struct rte_flow_item *item,
				  struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;

	if (!spec) {
		/* Outer VNI is required by tunnel_key parameter. */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "NULL VNI specification"
					  " for vxlan encapsulation");
	}
	if (!mask)
		mask = &rte_flow_item_vxlan_mask;
	if (!mask->vni[0] && !mask->vni[1] && !mask->vni[2])
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "outer VNI must be specified "
					  "for vxlan encapsulation");
	if (mask->vni[0] != 0xff ||
	    mask->vni[1] != 0xff ||
	    mask->vni[2] != 0xff)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "no support for partial mask on"
					  " \"vxlan.vni\" field");

	if (!spec->vni[0] && !spec->vni[1] && !spec->vni[2])
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "vxlan vni cannot be 0");
	return 0;
}

/**
 * Validate VXLAN_ENCAP action item list for E-Switch.
 * The routine checks items to be used in encapsulation header.
 *
 * @param[in] action
 *   Pointer to the VXLAN_ENCAP action structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 **/
static int
flow_tcf_validate_vxlan_encap(const struct rte_flow_action *action,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item *items;
	int ret;
	uint32_t item_flags = 0;

	if (!action->conf)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Missing vxlan tunnel"
					  " action configuration");
	items = ((const struct rte_flow_action_vxlan_encap *)
					action->conf)->definition;
	if (!items)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Missing vxlan tunnel"
					  " encapsulation parameters");
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(items, item_flags,
							  error);
			if (ret < 0)
				return ret;
			ret = flow_tcf_validate_vxlan_encap_eth(items, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L2;
			break;
		break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_validate_item_ipv4
					(items, item_flags,
					 &flow_tcf_mask_supported.ipv4, error);
			if (ret < 0)
				return ret;
			ret = flow_tcf_validate_vxlan_encap_ipv4(items, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_validate_item_ipv6
					(items, item_flags,
					 &flow_tcf_mask_supported.ipv6, error);
			if (ret < 0)
				return ret;
			ret = flow_tcf_validate_vxlan_encap_ipv6(items, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(items, item_flags,
							   0xFF, error);
			if (ret < 0)
				return ret;
			ret = flow_tcf_validate_vxlan_encap_udp(items, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5_flow_validate_item_vxlan(items,
							    item_flags, error);
			if (ret < 0)
				return ret;
			ret = flow_tcf_validate_vxlan_encap_vni(items, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_VXLAN;
			break;
		default:
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM, items,
					 "vxlan encap item not supported");
		}
	}
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "no outer IP layer found"
					  " for vxlan encapsulation");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "no outer UDP layer found"
					  " for vxlan encapsulation");
	if (!(item_flags & MLX5_FLOW_LAYER_VXLAN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "no VXLAN VNI found"
					  " for vxlan encapsulation");
	return 0;
}

/**
 * Validate outer RTE_FLOW_ITEM_TYPE_UDP item if tunnel item
 * RTE_FLOW_ITEM_TYPE_VXLAN is present in item list.
 *
 * @param[in] udp
 *   Outer UDP layer item (if any, NULL otherwise).
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 **/
static int
flow_tcf_validate_vxlan_decap_udp(const struct rte_flow_item *udp,
				  struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec = udp->spec;
	const struct rte_flow_item_udp *mask = udp->mask;

	if (!spec)
		/*
		 * Specification for UDP ports cannot be empty
		 * because it is required as decap parameter.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, udp,
					  "NULL UDP port specification"
					  " for VXLAN decapsulation");
	if (!mask)
		mask = &rte_flow_item_udp_mask;
	if (mask->hdr.dst_port != RTE_BE16(0x0000)) {
		if (mask->hdr.dst_port != RTE_BE16(0xffff))
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					 "no support for partial mask on"
					 " \"udp.hdr.dst_port\" field");
		if (!spec->hdr.dst_port)
			return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, udp,
					 "zero decap local UDP port");
	} else {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, udp,
					  "outer UDP destination port must be "
					  "specified for vxlan decapsulation");
	}
	if (mask->hdr.src_port != RTE_BE16(0x0000)) {
		if (mask->hdr.src_port != RTE_BE16(0xffff))
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					 "no support for partial mask on"
					 " \"udp.hdr.src_port\" field");
		DRV_LOG(WARNING,
			"outer UDP local port cannot be "
			"forced for VXLAN encapsulation, "
			"parameter ignored");
	}
	return 0;
}

/**
 * Validate flow for E-Switch.
 *
 * @param[in] priv
 *   Pointer to the priv structure.
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
flow_tcf_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	union {
		const struct rte_flow_item_port_id *port_id;
		const struct rte_flow_item_eth *eth;
		const struct rte_flow_item_vlan *vlan;
		const struct rte_flow_item_ipv4 *ipv4;
		const struct rte_flow_item_ipv6 *ipv6;
		const struct rte_flow_item_tcp *tcp;
		const struct rte_flow_item_udp *udp;
		const struct rte_flow_item_vxlan *vxlan;
	} spec, mask;
	union {
		const struct rte_flow_action_port_id *port_id;
		const struct rte_flow_action_jump *jump;
		const struct rte_flow_action_of_push_vlan *of_push_vlan;
		const struct rte_flow_action_of_set_vlan_vid *
			of_set_vlan_vid;
		const struct rte_flow_action_of_set_vlan_pcp *
			of_set_vlan_pcp;
		const struct rte_flow_action_vxlan_encap *vxlan_encap;
		const struct rte_flow_action_set_ipv4 *set_ipv4;
		const struct rte_flow_action_set_ipv6 *set_ipv6;
	} conf;
	const struct rte_flow_item *outer_udp = NULL;
	rte_be16_t inner_etype = RTE_BE16(ETH_P_ALL);
	rte_be16_t outer_etype = RTE_BE16(ETH_P_ALL);
	rte_be16_t vlan_etype = RTE_BE16(ETH_P_ALL);
	uint64_t item_flags = 0;
	uint64_t action_flags = 0;
	uint8_t next_protocol = 0xff;
	unsigned int tcm_ifindex = 0;
	uint8_t pedit_validated = 0;
	struct flow_tcf_ptoi ptoi[PTOI_TABLE_SZ_MAX(dev)];
	struct rte_eth_dev *port_id_dev = NULL;
	bool in_port_id_set;
	int ret;

	claim_nonzero(flow_tcf_build_ptoi_table(dev, ptoi,
						PTOI_TABLE_SZ_MAX(dev)));
	ret = flow_tcf_validate_attributes(attr, error);
	if (ret < 0)
		return ret;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		unsigned int i;
		uint64_t current_action_flag = 0;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			current_action_flag = MLX5_FLOW_ACTION_PORT_ID;
			if (!actions->conf)
				break;
			conf.port_id = actions->conf;
			if (conf.port_id->original)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == conf.port_id->id)
						break;
			if (!ptoi[i].ifindex)
				return rte_flow_error_set
					(error, ENODEV,
					 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					 conf.port_id,
					 "missing data to convert port ID to"
					 " ifindex");
			port_id_dev = &rte_eth_devices[conf.port_id->id];
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			current_action_flag = MLX5_FLOW_ACTION_JUMP;
			if (!actions->conf)
				break;
			conf.jump = actions->conf;
			if (attr->group >= conf.jump->group)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION,
					 actions,
					 "can jump only to a group forward");
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			current_action_flag = MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			current_action_flag = MLX5_FLOW_ACTION_OF_POP_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN: {
			rte_be16_t ethertype;

			current_action_flag = MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			if (!actions->conf)
				break;
			conf.of_push_vlan = actions->conf;
			ethertype = conf.of_push_vlan->ethertype;
			if (ethertype != RTE_BE16(ETH_P_8021Q) &&
			    ethertype != RTE_BE16(ETH_P_8021AD))
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ACTION, actions,
					 "vlan push TPID must be "
					 "802.1Q or 802.1AD");
			break;
		}
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			if (!(action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION, actions,
					 "vlan modify is not supported,"
					 " set action must follow push action");
			current_action_flag = MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			if (!(action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION, actions,
					 "vlan modify is not supported,"
					 " set action must follow push action");
			current_action_flag = MLX5_FLOW_ACTION_OF_SET_VLAN_PCP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
			current_action_flag = MLX5_FLOW_ACTION_VXLAN_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			ret = flow_tcf_validate_vxlan_encap(actions, error);
			if (ret < 0)
				return ret;
			current_action_flag = MLX5_FLOW_ACTION_VXLAN_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV4_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV4_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV6_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV6_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
			current_action_flag = MLX5_FLOW_ACTION_SET_TP_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			current_action_flag = MLX5_FLOW_ACTION_SET_TP_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
			current_action_flag = MLX5_FLOW_ACTION_SET_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
			current_action_flag = MLX5_FLOW_ACTION_DEC_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
			current_action_flag = MLX5_FLOW_ACTION_SET_MAC_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			current_action_flag = MLX5_FLOW_ACTION_SET_MAC_DST;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
		if (current_action_flag & MLX5_TCF_CONFIG_ACTIONS) {
			if (!actions->conf)
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					 actions,
					 "action configuration not set");
		}
		if ((current_action_flag & MLX5_TCF_PEDIT_ACTIONS) &&
		    pedit_validated)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "set actions should be "
						  "listed successively");
		if ((current_action_flag & ~MLX5_TCF_PEDIT_ACTIONS) &&
		    (action_flags & MLX5_TCF_PEDIT_ACTIONS))
			pedit_validated = 1;
		if ((current_action_flag & MLX5_TCF_FATE_ACTIONS) &&
		    (action_flags & MLX5_TCF_FATE_ACTIONS))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "can't have multiple fate"
						  " actions");
		if ((current_action_flag & MLX5_TCF_VXLAN_ACTIONS) &&
		    (action_flags & MLX5_TCF_VXLAN_ACTIONS))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "can't have multiple vxlan"
						  " actions");
		if ((current_action_flag & MLX5_TCF_VXLAN_ACTIONS) &&
		    (action_flags & MLX5_TCF_VLAN_ACTIONS))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "can't have vxlan and vlan"
						  " actions in the same rule");
		action_flags |= current_action_flag;
	}
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		unsigned int i;

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM, items,
					 "inner tunnel port id"
					 " item is not supported");
			mask.port_id = flow_tcf_item_mask
				(items, &rte_flow_item_port_id_mask,
				 &flow_tcf_mask_supported.port_id,
				 &flow_tcf_mask_empty.port_id,
				 sizeof(flow_tcf_mask_supported.port_id),
				 error);
			if (!mask.port_id)
				return -rte_errno;
			if (mask.port_id == &flow_tcf_mask_empty.port_id) {
				in_port_id_set = 1;
				break;
			}
			spec.port_id = items->spec;
			if (mask.port_id->id && mask.port_id->id != 0xffffffff)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.port_id,
					 "no support for partial mask on"
					 " \"id\" field");
			if (!mask.port_id->id)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == spec.port_id->id)
						break;
			if (!ptoi[i].ifindex)
				return rte_flow_error_set
					(error, ENODEV,
					 RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					 spec.port_id,
					 "missing data to convert port ID to"
					 " ifindex");
			if (in_port_id_set && ptoi[i].ifindex != tcm_ifindex)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					 spec.port_id,
					 "cannot match traffic for"
					 " several port IDs through"
					 " a single flow rule");
			tcm_ifindex = ptoi[i].ifindex;
			in_port_id_set = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(items, item_flags,
							  error);
			if (ret < 0)
				return ret;
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L2 :
				      MLX5_FLOW_LAYER_OUTER_L2;
			/* TODO:
			 * Redundant check due to different supported mask.
			 * Same for the rest of items.
			 */
			mask.eth = flow_tcf_item_mask
				(items, &rte_flow_item_eth_mask,
				 &flow_tcf_mask_supported.eth,
				 &flow_tcf_mask_empty.eth,
				 sizeof(flow_tcf_mask_supported.eth),
				 error);
			if (!mask.eth)
				return -rte_errno;
			if (mask.eth->type && mask.eth->type !=
			    RTE_BE16(0xffff))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.eth,
					 "no support for partial mask on"
					 " \"type\" field");
			assert(items->spec);
			spec.eth = items->spec;
			if (mask.eth->type &&
			    (item_flags & MLX5_FLOW_LAYER_TUNNEL) &&
			    inner_etype != RTE_BE16(ETH_P_ALL) &&
			    inner_etype != spec.eth->type)
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 items,
					 "inner eth_type conflict");
			if (mask.eth->type &&
			    !(item_flags & MLX5_FLOW_LAYER_TUNNEL) &&
			    outer_etype != RTE_BE16(ETH_P_ALL) &&
			    outer_etype != spec.eth->type)
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 items,
					 "outer eth_type conflict");
			if (mask.eth->type) {
				if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
					inner_etype = spec.eth->type;
				else
					outer_etype = spec.eth->type;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM, items,
					 "inner tunnel VLAN"
					 " is not supported");
			ret = mlx5_flow_validate_item_vlan(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_VLAN;
			mask.vlan = flow_tcf_item_mask
				(items, &rte_flow_item_vlan_mask,
				 &flow_tcf_mask_supported.vlan,
				 &flow_tcf_mask_empty.vlan,
				 sizeof(flow_tcf_mask_supported.vlan),
				 error);
			if (!mask.vlan)
				return -rte_errno;
			if ((mask.vlan->tci & RTE_BE16(0xe000) &&
			     (mask.vlan->tci & RTE_BE16(0xe000)) !=
			      RTE_BE16(0xe000)) ||
			    (mask.vlan->tci & RTE_BE16(0x0fff) &&
			     (mask.vlan->tci & RTE_BE16(0x0fff)) !=
			      RTE_BE16(0x0fff)) ||
			    (mask.vlan->inner_type &&
			     mask.vlan->inner_type != RTE_BE16(0xffff)))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.vlan,
					 "no support for partial masks on"
					 " \"tci\" (PCP and VID parts) and"
					 " \"inner_type\" fields");
			if (outer_etype != RTE_BE16(ETH_P_ALL) &&
			    outer_etype != RTE_BE16(ETH_P_8021Q))
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 items,
					 "outer eth_type conflict,"
					 " must be 802.1Q");
			outer_etype = RTE_BE16(ETH_P_8021Q);
			assert(items->spec);
			spec.vlan = items->spec;
			if (mask.vlan->inner_type &&
			    vlan_etype != RTE_BE16(ETH_P_ALL) &&
			    vlan_etype != spec.vlan->inner_type)
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 items,
					 "vlan eth_type conflict");
			if (mask.vlan->inner_type)
				vlan_etype = spec.vlan->inner_type;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_validate_item_ipv4
					(items, item_flags,
					 &flow_tcf_mask_supported.ipv4, error);
			if (ret < 0)
				return ret;
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L3_IPV4 :
				      MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			mask.ipv4 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv4_mask,
				 &flow_tcf_mask_supported.ipv4,
				 &flow_tcf_mask_empty.ipv4,
				 sizeof(flow_tcf_mask_supported.ipv4),
				 error);
			if (!mask.ipv4)
				return -rte_errno;
			if (mask.ipv4->hdr.next_proto_id &&
			    mask.ipv4->hdr.next_proto_id != 0xff)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.ipv4,
					 "no support for partial mask on"
					 " \"hdr.next_proto_id\" field");
			else if (mask.ipv4->hdr.next_proto_id)
				next_protocol =
					((const struct rte_flow_item_ipv4 *)
					 (items->spec))->hdr.next_proto_id;
			if (item_flags & MLX5_FLOW_LAYER_TUNNEL) {
				if (inner_etype != RTE_BE16(ETH_P_ALL) &&
				    inner_etype != RTE_BE16(ETH_P_IP))
					return rte_flow_error_set
						(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 items,
						 "inner eth_type conflict,"
						 " IPv4 is required");
				inner_etype = RTE_BE16(ETH_P_IP);
			} else if (item_flags & MLX5_FLOW_LAYER_OUTER_VLAN) {
				if (vlan_etype != RTE_BE16(ETH_P_ALL) &&
				    vlan_etype != RTE_BE16(ETH_P_IP))
					return rte_flow_error_set
						(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 items,
						 "vlan eth_type conflict,"
						 " IPv4 is required");
				vlan_etype = RTE_BE16(ETH_P_IP);
			} else {
				if (outer_etype != RTE_BE16(ETH_P_ALL) &&
				    outer_etype != RTE_BE16(ETH_P_IP))
					return rte_flow_error_set
						(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 items,
						 "eth_type conflict,"
						 " IPv4 is required");
				outer_etype = RTE_BE16(ETH_P_IP);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_validate_item_ipv6
					(items, item_flags,
					 &flow_tcf_mask_supported.ipv6, error);
			if (ret < 0)
				return ret;
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L3_IPV6 :
				      MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			mask.ipv6 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv6_mask,
				 &flow_tcf_mask_supported.ipv6,
				 &flow_tcf_mask_empty.ipv6,
				 sizeof(flow_tcf_mask_supported.ipv6),
				 error);
			if (!mask.ipv6)
				return -rte_errno;
			if (mask.ipv6->hdr.proto &&
			    mask.ipv6->hdr.proto != 0xff)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.ipv6,
					 "no support for partial mask on"
					 " \"hdr.proto\" field");
			else if (mask.ipv6->hdr.proto)
				next_protocol =
					((const struct rte_flow_item_ipv6 *)
					 (items->spec))->hdr.proto;
			if (item_flags & MLX5_FLOW_LAYER_TUNNEL) {
				if (inner_etype != RTE_BE16(ETH_P_ALL) &&
				    inner_etype != RTE_BE16(ETH_P_IPV6))
					return rte_flow_error_set
						(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 items,
						 "inner eth_type conflict,"
						 " IPv6 is required");
				inner_etype = RTE_BE16(ETH_P_IPV6);
			} else if (item_flags & MLX5_FLOW_LAYER_OUTER_VLAN) {
				if (vlan_etype != RTE_BE16(ETH_P_ALL) &&
				    vlan_etype != RTE_BE16(ETH_P_IPV6))
					return rte_flow_error_set
						(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 items,
						 "vlan eth_type conflict,"
						 " IPv6 is required");
				vlan_etype = RTE_BE16(ETH_P_IPV6);
			} else {
				if (outer_etype != RTE_BE16(ETH_P_ALL) &&
				    outer_etype != RTE_BE16(ETH_P_IPV6))
					return rte_flow_error_set
						(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 items,
						 "eth_type conflict,"
						 " IPv6 is required");
				outer_etype = RTE_BE16(ETH_P_IPV6);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(items, item_flags,
							  next_protocol, error);
			if (ret < 0)
				return ret;
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L4_UDP :
				      MLX5_FLOW_LAYER_OUTER_L4_UDP;
			mask.udp = flow_tcf_item_mask
				(items, &rte_flow_item_udp_mask,
				 &flow_tcf_mask_supported.udp,
				 &flow_tcf_mask_empty.udp,
				 sizeof(flow_tcf_mask_supported.udp),
				 error);
			if (!mask.udp)
				return -rte_errno;
			/*
			 * Save the presumed outer UDP item for extra check
			 * if the tunnel item will be found later in the list.
			 */
			if (!(item_flags & MLX5_FLOW_LAYER_TUNNEL))
				outer_udp = items;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_validate_item_tcp
					     (items, item_flags,
					      next_protocol,
					      &flow_tcf_mask_supported.tcp,
					      error);
			if (ret < 0)
				return ret;
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L4_TCP :
				      MLX5_FLOW_LAYER_OUTER_L4_TCP;
			mask.tcp = flow_tcf_item_mask
				(items, &rte_flow_item_tcp_mask,
				 &flow_tcf_mask_supported.tcp,
				 &flow_tcf_mask_empty.tcp,
				 sizeof(flow_tcf_mask_supported.tcp),
				 error);
			if (!mask.tcp)
				return -rte_errno;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			if (item_flags & MLX5_FLOW_LAYER_OUTER_VLAN)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM, items,
					 "vxlan tunnel over vlan"
					 " is not supported");
			ret = mlx5_flow_validate_item_vxlan(items,
							    item_flags, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_VXLAN;
			mask.vxlan = flow_tcf_item_mask
				(items, &rte_flow_item_vxlan_mask,
				 &flow_tcf_mask_supported.vxlan,
				 &flow_tcf_mask_empty.vxlan,
				 sizeof(flow_tcf_mask_supported.vxlan), error);
			if (!mask.vxlan)
				return -rte_errno;
			if (mask.vxlan->vni[0] != 0xff ||
			    mask.vxlan->vni[1] != 0xff ||
			    mask.vxlan->vni[2] != 0xff)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.vxlan,
					 "no support for partial or "
					 "empty mask on \"vxlan.vni\" field");
			/*
			 * The VNI item assumes the VXLAN tunnel, it requires
			 * at least the outer destination UDP port must be
			 * specified without wildcards to allow kernel select
			 * the virtual VXLAN device by port. Also outer IPv4
			 * or IPv6 item must be specified (wilcards or even
			 * zero mask are allowed) to let driver know the tunnel
			 * IP version and process UDP traffic correctly.
			 */
			if (!(item_flags &
			     (MLX5_FLOW_LAYER_OUTER_L3_IPV4 |
			      MLX5_FLOW_LAYER_OUTER_L3_IPV6)))
				return rte_flow_error_set
						 (error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "no outer IP pattern found"
						  " for vxlan tunnel");
			if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
				return rte_flow_error_set
						 (error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "no outer UDP pattern found"
						  " for vxlan tunnel");
			/*
			 * All items preceding the tunnel item become outer
			 * ones and we should do extra validation for them
			 * due to tc limitations for tunnel outer parameters.
			 * Currently only outer UDP item requres extra check,
			 * use the saved pointer instead of item list rescan.
			 */
			assert(outer_udp);
			ret = flow_tcf_validate_vxlan_decap_udp
						(outer_udp, error);
			if (ret < 0)
				return ret;
			/* Reset L4 protocol for inner parameters. */
			next_protocol = 0xff;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  items, "item not supported");
		}
	}
	if ((action_flags & MLX5_TCF_PEDIT_ACTIONS) &&
	    (action_flags & MLX5_FLOW_ACTION_DROP))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  actions,
					  "set action is not compatible with "
					  "drop action");
	if ((action_flags & MLX5_TCF_PEDIT_ACTIONS) &&
	    !(action_flags & MLX5_FLOW_ACTION_PORT_ID))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  actions,
					  "set action must be followed by "
					  "port_id action");
	if (action_flags &
	   (MLX5_FLOW_ACTION_SET_IPV4_SRC | MLX5_FLOW_ACTION_SET_IPV4_DST)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV4))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no ipv4 item found in"
						  " pattern");
	}
	if (action_flags &
	   (MLX5_FLOW_ACTION_SET_IPV6_SRC | MLX5_FLOW_ACTION_SET_IPV6_DST)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV6))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no ipv6 item found in"
						  " pattern");
	}
	if (action_flags &
	   (MLX5_FLOW_ACTION_SET_TP_SRC | MLX5_FLOW_ACTION_SET_TP_DST)) {
		if (!(item_flags &
		     (MLX5_FLOW_LAYER_OUTER_L4_UDP |
		      MLX5_FLOW_LAYER_OUTER_L4_TCP)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no TCP/UDP item found in"
						  " pattern");
	}
	/*
	 * FW syndrome (0xA9C090):
	 *     set_flow_table_entry: push vlan action fte in fdb can ONLY be
	 *     forward to the uplink.
	 */
	if ((action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN) &&
	    (action_flags & MLX5_FLOW_ACTION_PORT_ID) &&
	    ((struct mlx5_priv *)port_id_dev->data->dev_private)->representor)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "vlan push can only be applied"
					  " when forwarding to uplink port");
	/*
	 * FW syndrome (0x294609):
	 *     set_flow_table_entry: modify/pop/push actions in fdb flow table
	 *     are supported only while forwarding to vport.
	 */
	if ((action_flags & MLX5_TCF_VLAN_ACTIONS) &&
	    !(action_flags & MLX5_FLOW_ACTION_PORT_ID))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "vlan actions are supported"
					  " only with port_id action");
	if ((action_flags & MLX5_TCF_VXLAN_ACTIONS) &&
	    !(action_flags & MLX5_FLOW_ACTION_PORT_ID))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "vxlan actions are supported"
					  " only with port_id action");
	if (!(action_flags & MLX5_TCF_FATE_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "no fate action is found");
	if (action_flags &
	   (MLX5_FLOW_ACTION_SET_TTL | MLX5_FLOW_ACTION_DEC_TTL)) {
		if (!(item_flags &
		     (MLX5_FLOW_LAYER_OUTER_L3_IPV4 |
		      MLX5_FLOW_LAYER_OUTER_L3_IPV6)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no IP found in pattern");
	}
	if (action_flags &
	    (MLX5_FLOW_ACTION_SET_MAC_SRC | MLX5_FLOW_ACTION_SET_MAC_DST)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L2))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no ethernet found in"
						  " pattern");
	}
	if ((action_flags & MLX5_FLOW_ACTION_VXLAN_DECAP) &&
	    !(item_flags & MLX5_FLOW_LAYER_VXLAN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "no VNI pattern found"
					  " for vxlan decap action");
	if ((action_flags & MLX5_FLOW_ACTION_VXLAN_ENCAP) &&
	    (item_flags & MLX5_FLOW_LAYER_TUNNEL))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "vxlan encap not supported"
					  " for tunneled traffic");
	return 0;
}

/**
 * Calculate maximum size of memory for flow items of Linux TC flower.
 *
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[out] action_flags
 *   Pointer to the detected actions.
 *
 * @return
 *   Maximum size of memory for items.
 */
static int
flow_tcf_get_items_size(const struct rte_flow_attr *attr,
			const struct rte_flow_item items[],
			uint64_t *action_flags)
{
	int size = 0;

	size += SZ_NLATTR_STRZ_OF("flower") +
		SZ_NLATTR_TYPE_OF(uint16_t) + /* Outer ether type. */
		SZ_NLATTR_NEST + /* TCA_OPTIONS. */
		SZ_NLATTR_TYPE_OF(uint32_t); /* TCA_CLS_FLAGS_SKIP_SW. */
	if (attr->group > 0)
		size += SZ_NLATTR_TYPE_OF(uint32_t); /* TCA_CHAIN. */
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			size += SZ_NLATTR_DATA_OF(ETHER_ADDR_LEN) * 4;
				/* dst/src MAC addr and mask. */
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			size +=	SZ_NLATTR_TYPE_OF(uint16_t) +
				/* VLAN Ether type. */
				SZ_NLATTR_TYPE_OF(uint8_t) + /* VLAN prio. */
				SZ_NLATTR_TYPE_OF(uint16_t); /* VLAN ID. */
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4: {
			const struct rte_flow_item_ipv4 *ipv4 = items->mask;

			size +=	SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_TYPE_OF(uint32_t) * 4;
				/* dst/src IP addr and mask. */
			if (ipv4 && ipv4->hdr.time_to_live)
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			if (ipv4 && ipv4->hdr.type_of_service)
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_IPV6: {
			const struct rte_flow_item_ipv6 *ipv6 = items->mask;

			size +=	SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_DATA_OF(IPV6_ADDR_LEN) * 4;
				/* dst/src IP addr and mask. */
			if (ipv6 && ipv6->hdr.hop_limits)
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			if (ipv6 && (rte_be_to_cpu_32(ipv6->hdr.vtc_flow) &
				     (0xfful << IPV6_HDR_TC_SHIFT)))
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_UDP:
			size += SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_TYPE_OF(uint16_t) * 4;
				/* dst/src port and mask. */
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			size += SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_TYPE_OF(uint16_t) * 4;
				/* dst/src port and mask. */
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			size += SZ_NLATTR_TYPE_OF(uint32_t);
			/*
			 * There might be no VXLAN decap action in the action
			 * list, nonetheless the VXLAN tunnel flow requires
			 * the decap structure to be correctly applied to
			 * VXLAN device, set the flag to create the structure.
			 * Translation routine will not put the decap action
			 * in tne Netlink message if there is no actual action
			 * in the list.
			 */
			*action_flags |= MLX5_FLOW_ACTION_VXLAN_DECAP;
			break;
		default:
			DRV_LOG(WARNING,
				"unsupported item %p type %d,"
				" items must be validated before flow creation",
				(const void *)items, items->type);
			break;
		}
	}
	return size;
}

/**
 * Calculate size of memory to store the VXLAN encapsultion
 * related items in the Netlink message buffer. Items list
 * is specified by RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP action.
 * The item list should be validated.
 *
 * @param[in] action
 *   RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP action object.
 *   List of pattern items to scan data from.
 *
 * @return
 *   The size the part of Netlink message buffer to store the
 *   VXLAN encapsulation item attributes.
 */
static int
flow_tcf_vxlan_encap_size(const struct rte_flow_action *action)
{
	const struct rte_flow_item *items;
	int size = 0;

	assert(action->type == RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP);
	assert(action->conf);

	items = ((const struct rte_flow_action_vxlan_encap *)
					action->conf)->definition;
	assert(items);
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			/* This item does not require message buffer. */
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4: {
			const struct rte_flow_item_ipv4 *ipv4 = items->mask;

			size += SZ_NLATTR_DATA_OF(IPV4_ADDR_LEN) * 2;
			if (ipv4 && ipv4->hdr.time_to_live)
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			if (ipv4 && ipv4->hdr.type_of_service)
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_IPV6: {
			const struct rte_flow_item_ipv6 *ipv6 = items->mask;

			size += SZ_NLATTR_DATA_OF(IPV6_ADDR_LEN) * 2;
			if (ipv6 && ipv6->hdr.hop_limits)
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			if (ipv6 && (rte_be_to_cpu_32(ipv6->hdr.vtc_flow) &
				     (0xfful << IPV6_HDR_TC_SHIFT)))
				size += SZ_NLATTR_TYPE_OF(uint8_t) * 2;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_UDP: {
			const struct rte_flow_item_udp *udp = items->mask;

			size += SZ_NLATTR_TYPE_OF(uint16_t);
			if (!udp || udp->hdr.src_port != RTE_BE16(0x0000))
				size += SZ_NLATTR_TYPE_OF(uint16_t);
			break;
		}
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			size +=	SZ_NLATTR_TYPE_OF(uint32_t);
			break;
		default:
			assert(false);
			DRV_LOG(WARNING,
				"unsupported item %p type %d,"
				" items must be validated"
				" before flow creation",
				(const void *)items, items->type);
			return 0;
		}
	}
	return size;
}

/**
 * Calculate maximum size of memory for flow actions of Linux TC flower and
 * extract specified actions.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] action_flags
 *   Pointer to the detected actions.
 *
 * @return
 *   Maximum size of memory for actions.
 */
static int
flow_tcf_get_actions_and_size(const struct rte_flow_action actions[],
			      uint64_t *action_flags)
{
	int size = 0;
	uint64_t flags = *action_flags;

	size += SZ_NLATTR_NEST; /* TCA_FLOWER_ACT. */
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("mirred") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(struct tc_mirred);
			flags |= MLX5_FLOW_ACTION_PORT_ID;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("gact") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(struct tc_gact);
			flags |= MLX5_FLOW_ACTION_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("gact") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(struct tc_gact);
			flags |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			flags |= MLX5_FLOW_ACTION_OF_POP_VLAN;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			flags |= MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_PCP;
			goto action_of_vlan;
action_of_vlan:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("vlan") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(struct tc_vlan) +
				SZ_NLATTR_TYPE_OF(uint16_t) +
				/* VLAN protocol. */
				SZ_NLATTR_TYPE_OF(uint16_t) + /* VLAN ID. */
				SZ_NLATTR_TYPE_OF(uint8_t); /* VLAN prio. */
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("tunnel_key") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(uint8_t);
			size += SZ_NLATTR_TYPE_OF(struct tc_tunnel_key);
			size +=	flow_tcf_vxlan_encap_size(actions) +
				RTE_ALIGN_CEIL /* preceding encap params. */
				(sizeof(struct flow_tcf_vxlan_encap),
				MNL_ALIGNTO);
			flags |= MLX5_FLOW_ACTION_VXLAN_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("tunnel_key") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(uint8_t);
			size +=	SZ_NLATTR_TYPE_OF(struct tc_tunnel_key);
			size +=	RTE_ALIGN_CEIL /* preceding decap params. */
				(sizeof(struct flow_tcf_vxlan_decap),
				MNL_ALIGNTO);
			flags |= MLX5_FLOW_ACTION_VXLAN_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			size += flow_tcf_get_pedit_actions_size(&actions,
								&flags);
			break;
		default:
			DRV_LOG(WARNING,
				"unsupported action %p type %d,"
				" items must be validated before flow creation",
				(const void *)actions, actions->type);
			break;
		}
	}
	*action_flags = flags;
	return size;
}

/**
 * Prepare a flow object for Linux TC flower. It calculates the maximum size of
 * memory required, allocates the memory, initializes Netlink message headers
 * and set unique TC message handle.
 *
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
 *   Pointer to mlx5_flow object on success,
 *   otherwise NULL and rte_errno is set.
 */
static struct mlx5_flow *
flow_tcf_prepare(const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	size_t size = RTE_ALIGN_CEIL
			(sizeof(struct mlx5_flow),
			 alignof(struct flow_tcf_tunnel_hdr)) +
		      MNL_ALIGN(sizeof(struct nlmsghdr)) +
		      MNL_ALIGN(sizeof(struct tcmsg));
	struct mlx5_flow *dev_flow;
	uint64_t action_flags = 0;
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;
	uint8_t *sp, *tun = NULL;

	size += flow_tcf_get_items_size(attr, items, &action_flags);
	size += flow_tcf_get_actions_and_size(actions, &action_flags);
	dev_flow = rte_zmalloc(__func__, size, MNL_ALIGNTO);
	if (!dev_flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not enough memory to create E-Switch flow");
		return NULL;
	}
	sp = (uint8_t *)(dev_flow + 1);
	if (action_flags & MLX5_FLOW_ACTION_VXLAN_ENCAP) {
		sp = RTE_PTR_ALIGN
			(sp, alignof(struct flow_tcf_tunnel_hdr));
		tun = sp;
		sp += RTE_ALIGN_CEIL
			(sizeof(struct flow_tcf_vxlan_encap),
			MNL_ALIGNTO);
#ifndef NDEBUG
		size -= RTE_ALIGN_CEIL
			(sizeof(struct flow_tcf_vxlan_encap),
			MNL_ALIGNTO);
#endif
	} else if (action_flags & MLX5_FLOW_ACTION_VXLAN_DECAP) {
		sp = RTE_PTR_ALIGN
			(sp, alignof(struct flow_tcf_tunnel_hdr));
		tun = sp;
		sp += RTE_ALIGN_CEIL
			(sizeof(struct flow_tcf_vxlan_decap),
			MNL_ALIGNTO);
#ifndef NDEBUG
		size -= RTE_ALIGN_CEIL
			(sizeof(struct flow_tcf_vxlan_decap),
			MNL_ALIGNTO);
#endif
	} else {
		sp = RTE_PTR_ALIGN(sp, MNL_ALIGNTO);
	}
	nlh = mnl_nlmsg_put_header(sp);
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	*dev_flow = (struct mlx5_flow){
		.tcf = (struct mlx5_flow_tcf){
#ifndef NDEBUG
			.nlsize = size - RTE_ALIGN_CEIL
				(sizeof(struct mlx5_flow),
				 alignof(struct flow_tcf_tunnel_hdr)),
#endif
			.tunnel = (struct flow_tcf_tunnel_hdr *)tun,
			.nlh = nlh,
			.tcm = tcm,
		},
	};
	if (action_flags & MLX5_FLOW_ACTION_VXLAN_DECAP)
		dev_flow->tcf.tunnel->type = FLOW_TCF_TUNACT_VXLAN_DECAP;
	else if (action_flags & MLX5_FLOW_ACTION_VXLAN_ENCAP)
		dev_flow->tcf.tunnel->type = FLOW_TCF_TUNACT_VXLAN_ENCAP;
	return dev_flow;
}

/**
 * Make adjustments for supporting count actions.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] dev_flow
 *   Pointer to mlx5_flow.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 On success else a negative errno value is returned and rte_errno is set.
 */
static int
flow_tcf_translate_action_count(struct rte_eth_dev *dev __rte_unused,
				  struct mlx5_flow *dev_flow,
				  struct rte_flow_error *error)
{
	struct rte_flow *flow = dev_flow->flow;

	if (!flow->counter) {
		flow->counter = flow_tcf_counter_new();
		if (!flow->counter)
			return rte_flow_error_set(error, rte_errno,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "cannot get counter"
						  " context.");
	}
	return 0;
}

/**
 * Convert VXLAN VNI to 32-bit integer.
 *
 * @param[in] vni
 *   VXLAN VNI in 24-bit wire format.
 *
 * @return
 *   VXLAN VNI as a 32-bit integer value in network endianness.
 */
static inline rte_be32_t
vxlan_vni_as_be32(const uint8_t vni[3])
{
	union {
		uint8_t vni[4];
		rte_be32_t dword;
	} ret = {
		.vni = { 0, vni[0], vni[1], vni[2] },
	};
	return ret.dword;
}

/**
 * Helper function to process RTE_FLOW_ITEM_TYPE_ETH entry in configuration
 * of action RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP. Fills the MAC address fields
 * in the encapsulation parameters structure. The item must be prevalidated,
 * no any validation checks performed by function.
 *
 * @param[in] spec
 *   RTE_FLOW_ITEM_TYPE_ETH entry specification.
 * @param[in] mask
 *   RTE_FLOW_ITEM_TYPE_ETH entry mask.
 * @param[out] encap
 *   Structure to fill the gathered MAC address data.
 */
static void
flow_tcf_parse_vxlan_encap_eth(const struct rte_flow_item_eth *spec,
			       const struct rte_flow_item_eth *mask,
			       struct flow_tcf_vxlan_encap *encap)
{
	/* Item must be validated before. No redundant checks. */
	assert(spec);
	if (!mask || !memcmp(&mask->dst,
			     &rte_flow_item_eth_mask.dst,
			     sizeof(rte_flow_item_eth_mask.dst))) {
		/*
		 * Ethernet addresses are not supported by
		 * tc as tunnel_key parameters. Destination
		 * address is needed to form encap packet
		 * header and retrieved by kernel from
		 * implicit sources (ARP table, etc),
		 * address masks are not supported at all.
		 */
		encap->eth.dst = spec->dst;
		encap->mask |= FLOW_TCF_ENCAP_ETH_DST;
	}
	if (!mask || !memcmp(&mask->src,
			     &rte_flow_item_eth_mask.src,
			     sizeof(rte_flow_item_eth_mask.src))) {
		/*
		 * Ethernet addresses are not supported by
		 * tc as tunnel_key parameters. Source ethernet
		 * address is ignored anyway.
		 */
		encap->eth.src = spec->src;
		encap->mask |= FLOW_TCF_ENCAP_ETH_SRC;
	}
}

/**
 * Helper function to process RTE_FLOW_ITEM_TYPE_IPV4 entry in configuration
 * of action RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP. Fills the IPV4 address fields
 * in the encapsulation parameters structure. The item must be prevalidated,
 * no any validation checks performed by function.
 *
 * @param[in] spec
 *   RTE_FLOW_ITEM_TYPE_IPV4 entry specification.
 * @param[in] mask
 *  RTE_FLOW_ITEM_TYPE_IPV4 entry mask.
 * @param[out] encap
 *   Structure to fill the gathered IPV4 address data.
 */
static void
flow_tcf_parse_vxlan_encap_ipv4(const struct rte_flow_item_ipv4 *spec,
				const struct rte_flow_item_ipv4 *mask,
				struct flow_tcf_vxlan_encap *encap)
{
	/* Item must be validated before. No redundant checks. */
	assert(spec);
	encap->ipv4.dst = spec->hdr.dst_addr;
	encap->ipv4.src = spec->hdr.src_addr;
	encap->mask |= FLOW_TCF_ENCAP_IPV4_SRC |
		       FLOW_TCF_ENCAP_IPV4_DST;
	if (mask && mask->hdr.type_of_service) {
		encap->mask |= FLOW_TCF_ENCAP_IP_TOS;
		encap->ip_tos = spec->hdr.type_of_service;
	}
	if (mask && mask->hdr.time_to_live) {
		encap->mask |= FLOW_TCF_ENCAP_IP_TTL;
		encap->ip_ttl_hop = spec->hdr.time_to_live;
	}
}

/**
 * Helper function to process RTE_FLOW_ITEM_TYPE_IPV6 entry in configuration
 * of action RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP. Fills the IPV6 address fields
 * in the encapsulation parameters structure. The item must be prevalidated,
 * no any validation checks performed by function.
 *
 * @param[in] spec
 *   RTE_FLOW_ITEM_TYPE_IPV6 entry specification.
 * @param[in] mask
 *  RTE_FLOW_ITEM_TYPE_IPV6 entry mask.
 * @param[out] encap
 *   Structure to fill the gathered IPV6 address data.
 */
static void
flow_tcf_parse_vxlan_encap_ipv6(const struct rte_flow_item_ipv6 *spec,
				const struct rte_flow_item_ipv6 *mask,
				struct flow_tcf_vxlan_encap *encap)
{
	/* Item must be validated before. No redundant checks. */
	assert(spec);
	memcpy(encap->ipv6.dst, spec->hdr.dst_addr, IPV6_ADDR_LEN);
	memcpy(encap->ipv6.src, spec->hdr.src_addr, IPV6_ADDR_LEN);
	encap->mask |= FLOW_TCF_ENCAP_IPV6_SRC |
		       FLOW_TCF_ENCAP_IPV6_DST;
	if (mask) {
		if ((rte_be_to_cpu_32(mask->hdr.vtc_flow) >>
		    IPV6_HDR_TC_SHIFT) & 0xff) {
			encap->mask |= FLOW_TCF_ENCAP_IP_TOS;
			encap->ip_tos = (rte_be_to_cpu_32
						(spec->hdr.vtc_flow) >>
						 IPV6_HDR_TC_SHIFT) & 0xff;
		}
		if (mask->hdr.hop_limits) {
			encap->mask |= FLOW_TCF_ENCAP_IP_TTL;
			encap->ip_ttl_hop = spec->hdr.hop_limits;
		}
	}
}

/**
 * Helper function to process RTE_FLOW_ITEM_TYPE_UDP entry in configuration
 * of action RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP. Fills the UDP port fields
 * in the encapsulation parameters structure. The item must be prevalidated,
 * no any validation checks performed by function.
 *
 * @param[in] spec
 *   RTE_FLOW_ITEM_TYPE_UDP entry specification.
 * @param[in] mask
 *   RTE_FLOW_ITEM_TYPE_UDP entry mask.
 * @param[out] encap
 *   Structure to fill the gathered UDP port data.
 */
static void
flow_tcf_parse_vxlan_encap_udp(const struct rte_flow_item_udp *spec,
			       const struct rte_flow_item_udp *mask,
			       struct flow_tcf_vxlan_encap *encap)
{
	assert(spec);
	encap->udp.dst = spec->hdr.dst_port;
	encap->mask |= FLOW_TCF_ENCAP_UDP_DST;
	if (!mask || mask->hdr.src_port != RTE_BE16(0x0000)) {
		encap->udp.src = spec->hdr.src_port;
		encap->mask |= FLOW_TCF_ENCAP_IPV4_SRC;
	}
}

/**
 * Helper function to process RTE_FLOW_ITEM_TYPE_VXLAN entry in configuration
 * of action RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP. Fills the VNI fields
 * in the encapsulation parameters structure. The item must be prevalidated,
 * no any validation checks performed by function.
 *
 * @param[in] spec
 *   RTE_FLOW_ITEM_TYPE_VXLAN entry specification.
 * @param[out] encap
 *   Structure to fill the gathered VNI address data.
 */
static void
flow_tcf_parse_vxlan_encap_vni(const struct rte_flow_item_vxlan *spec,
			       struct flow_tcf_vxlan_encap *encap)
{
	/* Item must be validated before. Do not redundant checks. */
	assert(spec);
	memcpy(encap->vxlan.vni, spec->vni, sizeof(encap->vxlan.vni));
	encap->mask |= FLOW_TCF_ENCAP_VXLAN_VNI;
}

/**
 * Populate consolidated encapsulation object from list of pattern items.
 *
 * Helper function to process configuration of action such as
 * RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP. The item list should be
 * validated, there is no way to return an meaningful error.
 *
 * @param[in] action
 *   RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP action object.
 *   List of pattern items to gather data from.
 * @param[out] src
 *   Structure to fill gathered data.
 */
static void
flow_tcf_vxlan_encap_parse(const struct rte_flow_action *action,
			   struct flow_tcf_vxlan_encap *encap)
{
	union {
		const struct rte_flow_item_eth *eth;
		const struct rte_flow_item_ipv4 *ipv4;
		const struct rte_flow_item_ipv6 *ipv6;
		const struct rte_flow_item_udp *udp;
		const struct rte_flow_item_vxlan *vxlan;
	} spec, mask;
	const struct rte_flow_item *items;

	assert(action->type == RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP);
	assert(action->conf);

	items = ((const struct rte_flow_action_vxlan_encap *)
					action->conf)->definition;
	assert(items);
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			mask.eth = items->mask;
			spec.eth = items->spec;
			flow_tcf_parse_vxlan_encap_eth(spec.eth, mask.eth,
						       encap);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			spec.ipv4 = items->spec;
			mask.ipv4 = items->mask;
			flow_tcf_parse_vxlan_encap_ipv4(spec.ipv4, mask.ipv4,
							encap);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			spec.ipv6 = items->spec;
			mask.ipv6 = items->mask;
			flow_tcf_parse_vxlan_encap_ipv6(spec.ipv6, mask.ipv6,
							encap);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			mask.udp = items->mask;
			spec.udp = items->spec;
			flow_tcf_parse_vxlan_encap_udp(spec.udp, mask.udp,
						       encap);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			spec.vxlan = items->spec;
			flow_tcf_parse_vxlan_encap_vni(spec.vxlan, encap);
			break;
		default:
			assert(false);
			DRV_LOG(WARNING,
				"unsupported item %p type %d,"
				" items must be validated"
				" before flow creation",
				(const void *)items, items->type);
			encap->mask = 0;
			return;
		}
	}
}

/**
 * Translate flow for Linux TC flower and construct Netlink message.
 *
 * @param[in] priv
 *   Pointer to the priv structure.
 * @param[in, out] flow
 *   Pointer to the sub flow.
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
flow_tcf_translate(struct rte_eth_dev *dev, struct mlx5_flow *dev_flow,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	union {
		const struct rte_flow_item_port_id *port_id;
		const struct rte_flow_item_eth *eth;
		const struct rte_flow_item_vlan *vlan;
		const struct rte_flow_item_ipv4 *ipv4;
		const struct rte_flow_item_ipv6 *ipv6;
		const struct rte_flow_item_tcp *tcp;
		const struct rte_flow_item_udp *udp;
		const struct rte_flow_item_vxlan *vxlan;
	} spec, mask;
	union {
		const struct rte_flow_action_port_id *port_id;
		const struct rte_flow_action_jump *jump;
		const struct rte_flow_action_of_push_vlan *of_push_vlan;
		const struct rte_flow_action_of_set_vlan_vid *
			of_set_vlan_vid;
		const struct rte_flow_action_of_set_vlan_pcp *
			of_set_vlan_pcp;
	} conf;
	union {
		struct flow_tcf_tunnel_hdr *hdr;
		struct flow_tcf_vxlan_decap *vxlan;
	} decap = {
		.hdr = NULL,
	};
	union {
		struct flow_tcf_tunnel_hdr *hdr;
		struct flow_tcf_vxlan_encap *vxlan;
	} encap = {
		.hdr = NULL,
	};
	struct flow_tcf_ptoi ptoi[PTOI_TABLE_SZ_MAX(dev)];
	struct nlmsghdr *nlh = dev_flow->tcf.nlh;
	struct tcmsg *tcm = dev_flow->tcf.tcm;
	uint32_t na_act_index_cur;
	rte_be16_t inner_etype = RTE_BE16(ETH_P_ALL);
	rte_be16_t outer_etype = RTE_BE16(ETH_P_ALL);
	rte_be16_t vlan_etype = RTE_BE16(ETH_P_ALL);
	bool ip_proto_set = 0;
	bool tunnel_outer = 0;
	struct nlattr *na_flower;
	struct nlattr *na_flower_act;
	struct nlattr *na_vlan_id = NULL;
	struct nlattr *na_vlan_priority = NULL;
	uint64_t item_flags = 0;
	int ret;

	claim_nonzero(flow_tcf_build_ptoi_table(dev, ptoi,
						PTOI_TABLE_SZ_MAX(dev)));
	if (dev_flow->tcf.tunnel) {
		switch (dev_flow->tcf.tunnel->type) {
		case FLOW_TCF_TUNACT_VXLAN_DECAP:
			decap.vxlan = dev_flow->tcf.vxlan_decap;
			tunnel_outer = 1;
			break;
		case FLOW_TCF_TUNACT_VXLAN_ENCAP:
			encap.vxlan = dev_flow->tcf.vxlan_encap;
			break;
		/* New tunnel actions can be added here. */
		default:
			assert(false);
			break;
		}
	}
	nlh = dev_flow->tcf.nlh;
	tcm = dev_flow->tcf.tcm;
	/* Prepare API must have been called beforehand. */
	assert(nlh != NULL && tcm != NULL);
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ptoi[0].ifindex;
	tcm->tcm_parent = TC_H_MAKE(TC_H_INGRESS, TC_H_MIN_INGRESS);
	/*
	 * Priority cannot be zero to prevent the kernel from picking one
	 * automatically.
	 */
	tcm->tcm_info = TC_H_MAKE((attr->priority + 1) << 16, outer_etype);
	if (attr->group > 0)
		mnl_attr_put_u32(nlh, TCA_CHAIN, attr->group);
	mnl_attr_put_strz(nlh, TCA_KIND, "flower");
	na_flower = mnl_attr_nest_start(nlh, TCA_OPTIONS);
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		unsigned int i;

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			mask.port_id = flow_tcf_item_mask
				(items, &rte_flow_item_port_id_mask,
				 &flow_tcf_mask_supported.port_id,
				 &flow_tcf_mask_empty.port_id,
				 sizeof(flow_tcf_mask_supported.port_id),
				 error);
			assert(mask.port_id);
			if (mask.port_id == &flow_tcf_mask_empty.port_id)
				break;
			spec.port_id = items->spec;
			if (!mask.port_id->id)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == spec.port_id->id)
						break;
			assert(ptoi[i].ifindex);
			tcm->tcm_ifindex = ptoi[i].ifindex;
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L2 :
				      MLX5_FLOW_LAYER_OUTER_L2;
			mask.eth = flow_tcf_item_mask
				(items, &rte_flow_item_eth_mask,
				 &flow_tcf_mask_supported.eth,
				 &flow_tcf_mask_empty.eth,
				 sizeof(flow_tcf_mask_supported.eth),
				 error);
			assert(mask.eth);
			if (mask.eth == &flow_tcf_mask_empty.eth)
				break;
			spec.eth = items->spec;
			if (mask.eth->type) {
				if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
					inner_etype = spec.eth->type;
				else
					outer_etype = spec.eth->type;
			}
			if (tunnel_outer) {
				DRV_LOG(WARNING,
					"outer L2 addresses cannot be"
					" forced is outer ones for tunnel,"
					" parameter is ignored");
				break;
			}
			if (!is_zero_ether_addr(&mask.eth->dst)) {
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_DST,
					     ETHER_ADDR_LEN,
					     spec.eth->dst.addr_bytes);
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_DST_MASK,
					     ETHER_ADDR_LEN,
					     mask.eth->dst.addr_bytes);
			}
			if (!is_zero_ether_addr(&mask.eth->src)) {
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_SRC,
					     ETHER_ADDR_LEN,
					     spec.eth->src.addr_bytes);
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_SRC_MASK,
					     ETHER_ADDR_LEN,
					     mask.eth->src.addr_bytes);
			}
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			assert(!encap.hdr);
			assert(!decap.hdr);
			assert(!tunnel_outer);
			item_flags |= MLX5_FLOW_LAYER_OUTER_VLAN;
			mask.vlan = flow_tcf_item_mask
				(items, &rte_flow_item_vlan_mask,
				 &flow_tcf_mask_supported.vlan,
				 &flow_tcf_mask_empty.vlan,
				 sizeof(flow_tcf_mask_supported.vlan),
				 error);
			assert(mask.vlan);
			if (mask.vlan == &flow_tcf_mask_empty.vlan)
				break;
			spec.vlan = items->spec;
			assert(outer_etype == RTE_BE16(ETH_P_ALL) ||
			       outer_etype == RTE_BE16(ETH_P_8021Q));
			outer_etype = RTE_BE16(ETH_P_8021Q);
			if (mask.vlan->inner_type)
				vlan_etype = spec.vlan->inner_type;
			if (mask.vlan->tci & RTE_BE16(0xe000))
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_VLAN_PRIO,
						(rte_be_to_cpu_16
						 (spec.vlan->tci) >> 13) & 0x7);
			if (mask.vlan->tci & RTE_BE16(0x0fff))
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_VLAN_ID,
						 rte_be_to_cpu_16
						 (spec.vlan->tci &
						  RTE_BE16(0x0fff)));
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L3_IPV4 :
				      MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			mask.ipv4 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv4_mask,
				 &flow_tcf_mask_supported.ipv4,
				 &flow_tcf_mask_empty.ipv4,
				 sizeof(flow_tcf_mask_supported.ipv4),
				 error);
			assert(mask.ipv4);
			if (item_flags & MLX5_FLOW_LAYER_TUNNEL) {
				assert(inner_etype == RTE_BE16(ETH_P_ALL) ||
				       inner_etype == RTE_BE16(ETH_P_IP));
				inner_etype = RTE_BE16(ETH_P_IP);
			} else if (outer_etype == RTE_BE16(ETH_P_8021Q)) {
				assert(vlan_etype == RTE_BE16(ETH_P_ALL) ||
				       vlan_etype == RTE_BE16(ETH_P_IP));
				vlan_etype = RTE_BE16(ETH_P_IP);
			} else {
				assert(outer_etype == RTE_BE16(ETH_P_ALL) ||
				       outer_etype == RTE_BE16(ETH_P_IP));
				outer_etype = RTE_BE16(ETH_P_IP);
			}
			spec.ipv4 = items->spec;
			if (!tunnel_outer && mask.ipv4->hdr.next_proto_id) {
				/*
				 * No way to set IP protocol for outer tunnel
				 * layers. Usually it is fixed, for example,
				 * to UDP for VXLAN/GPE.
				 */
				assert(spec.ipv4); /* Mask is not empty. */
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_PROTO,
						spec.ipv4->hdr.next_proto_id);
				ip_proto_set = 1;
			}
			if (mask.ipv4 == &flow_tcf_mask_empty.ipv4 ||
			     (!mask.ipv4->hdr.src_addr &&
			      !mask.ipv4->hdr.dst_addr)) {
				if (!tunnel_outer)
					break;
				/*
				 * For tunnel outer we must set outer IP key
				 * anyway, even if the specification/mask is
				 * empty. There is no another way to tell
				 * kernel about he outer layer protocol.
				 */
				mnl_attr_put_u32
					(nlh, TCA_FLOWER_KEY_ENC_IPV4_SRC,
					 mask.ipv4->hdr.src_addr);
				mnl_attr_put_u32
					(nlh, TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK,
					 mask.ipv4->hdr.src_addr);
				assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
				break;
			}
			if (mask.ipv4->hdr.src_addr) {
				mnl_attr_put_u32
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IPV4_SRC :
					 TCA_FLOWER_KEY_IPV4_SRC,
					 spec.ipv4->hdr.src_addr);
				mnl_attr_put_u32
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK :
					 TCA_FLOWER_KEY_IPV4_SRC_MASK,
					 mask.ipv4->hdr.src_addr);
			}
			if (mask.ipv4->hdr.dst_addr) {
				mnl_attr_put_u32
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IPV4_DST :
					 TCA_FLOWER_KEY_IPV4_DST,
					 spec.ipv4->hdr.dst_addr);
				mnl_attr_put_u32
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IPV4_DST_MASK :
					 TCA_FLOWER_KEY_IPV4_DST_MASK,
					 mask.ipv4->hdr.dst_addr);
			}
			if (mask.ipv4->hdr.time_to_live) {
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TTL :
					 TCA_FLOWER_KEY_IP_TTL,
					 spec.ipv4->hdr.time_to_live);
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TTL_MASK :
					 TCA_FLOWER_KEY_IP_TTL_MASK,
					 mask.ipv4->hdr.time_to_live);
			}
			if (mask.ipv4->hdr.type_of_service) {
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TOS :
					 TCA_FLOWER_KEY_IP_TOS,
					 spec.ipv4->hdr.type_of_service);
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TOS_MASK :
					 TCA_FLOWER_KEY_IP_TOS_MASK,
					 mask.ipv4->hdr.type_of_service);
			}
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6: {
			bool ipv6_src, ipv6_dst;
			uint8_t msk6, tos6;

			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L3_IPV6 :
				      MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			mask.ipv6 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv6_mask,
				 &flow_tcf_mask_supported.ipv6,
				 &flow_tcf_mask_empty.ipv6,
				 sizeof(flow_tcf_mask_supported.ipv6),
				 error);
			assert(mask.ipv6);
			if (item_flags & MLX5_FLOW_LAYER_TUNNEL) {
				assert(inner_etype == RTE_BE16(ETH_P_ALL) ||
				       inner_etype == RTE_BE16(ETH_P_IPV6));
				inner_etype = RTE_BE16(ETH_P_IPV6);
			} else if (outer_etype == RTE_BE16(ETH_P_8021Q)) {
				assert(vlan_etype == RTE_BE16(ETH_P_ALL) ||
				       vlan_etype == RTE_BE16(ETH_P_IPV6));
				vlan_etype = RTE_BE16(ETH_P_IPV6);
			} else {
				assert(outer_etype == RTE_BE16(ETH_P_ALL) ||
				       outer_etype == RTE_BE16(ETH_P_IPV6));
				outer_etype = RTE_BE16(ETH_P_IPV6);
			}
			spec.ipv6 = items->spec;
			if (!tunnel_outer && mask.ipv6->hdr.proto) {
				/*
				 * No way to set IP protocol for outer tunnel
				 * layers. Usually it is fixed, for example,
				 * to UDP for VXLAN/GPE.
				 */
				assert(spec.ipv6); /* Mask is not empty. */
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_PROTO,
						spec.ipv6->hdr.proto);
				ip_proto_set = 1;
			}
			ipv6_dst = !IN6_IS_ADDR_UNSPECIFIED
						(mask.ipv6->hdr.dst_addr);
			ipv6_src = !IN6_IS_ADDR_UNSPECIFIED
						(mask.ipv6->hdr.src_addr);
			if (mask.ipv6 == &flow_tcf_mask_empty.ipv6 ||
			     (!ipv6_dst && !ipv6_src)) {
				if (!tunnel_outer)
					break;
				/*
				 * For tunnel outer we must set outer IP key
				 * anyway, even if the specification/mask is
				 * empty. There is no another way to tell
				 * kernel about he outer layer protocol.
				 */
				mnl_attr_put(nlh,
					     TCA_FLOWER_KEY_ENC_IPV6_SRC,
					     IPV6_ADDR_LEN,
					     mask.ipv6->hdr.src_addr);
				mnl_attr_put(nlh,
					     TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK,
					     IPV6_ADDR_LEN,
					     mask.ipv6->hdr.src_addr);
				assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
				break;
			}
			if (ipv6_src) {
				mnl_attr_put(nlh, tunnel_outer ?
					     TCA_FLOWER_KEY_ENC_IPV6_SRC :
					     TCA_FLOWER_KEY_IPV6_SRC,
					     IPV6_ADDR_LEN,
					     spec.ipv6->hdr.src_addr);
				mnl_attr_put(nlh, tunnel_outer ?
					     TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK :
					     TCA_FLOWER_KEY_IPV6_SRC_MASK,
					     IPV6_ADDR_LEN,
					     mask.ipv6->hdr.src_addr);
			}
			if (ipv6_dst) {
				mnl_attr_put(nlh, tunnel_outer ?
					     TCA_FLOWER_KEY_ENC_IPV6_DST :
					     TCA_FLOWER_KEY_IPV6_DST,
					     IPV6_ADDR_LEN,
					     spec.ipv6->hdr.dst_addr);
				mnl_attr_put(nlh, tunnel_outer ?
					     TCA_FLOWER_KEY_ENC_IPV6_DST_MASK :
					     TCA_FLOWER_KEY_IPV6_DST_MASK,
					     IPV6_ADDR_LEN,
					     mask.ipv6->hdr.dst_addr);
			}
			if (mask.ipv6->hdr.hop_limits) {
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TTL :
					 TCA_FLOWER_KEY_IP_TTL,
					 spec.ipv6->hdr.hop_limits);
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TTL_MASK :
					 TCA_FLOWER_KEY_IP_TTL_MASK,
					 mask.ipv6->hdr.hop_limits);
			}
			msk6 = (rte_be_to_cpu_32(mask.ipv6->hdr.vtc_flow) >>
				IPV6_HDR_TC_SHIFT) & 0xff;
			if (msk6) {
				tos6 = (rte_be_to_cpu_32
					(spec.ipv6->hdr.vtc_flow) >>
						IPV6_HDR_TC_SHIFT) & 0xff;
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TOS :
					 TCA_FLOWER_KEY_IP_TOS, tos6);
				mnl_attr_put_u8
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_IP_TOS_MASK :
					 TCA_FLOWER_KEY_IP_TOS_MASK, msk6);
			}
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		}
		case RTE_FLOW_ITEM_TYPE_UDP:
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L4_UDP :
				      MLX5_FLOW_LAYER_OUTER_L4_UDP;
			mask.udp = flow_tcf_item_mask
				(items, &rte_flow_item_udp_mask,
				 &flow_tcf_mask_supported.udp,
				 &flow_tcf_mask_empty.udp,
				 sizeof(flow_tcf_mask_supported.udp),
				 error);
			assert(mask.udp);
			spec.udp = items->spec;
			if (!tunnel_outer) {
				if (!ip_proto_set)
					mnl_attr_put_u8
						(nlh, TCA_FLOWER_KEY_IP_PROTO,
						IPPROTO_UDP);
				if (mask.udp == &flow_tcf_mask_empty.udp)
					break;
			} else {
				assert(mask.udp != &flow_tcf_mask_empty.udp);
				decap.vxlan->udp_port =
					rte_be_to_cpu_16
						(spec.udp->hdr.dst_port);
			}
			if (mask.udp->hdr.src_port) {
				mnl_attr_put_u16
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_UDP_SRC_PORT :
					 TCA_FLOWER_KEY_UDP_SRC,
					 spec.udp->hdr.src_port);
				mnl_attr_put_u16
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK :
					 TCA_FLOWER_KEY_UDP_SRC_MASK,
					 mask.udp->hdr.src_port);
			}
			if (mask.udp->hdr.dst_port) {
				mnl_attr_put_u16
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_UDP_DST_PORT :
					 TCA_FLOWER_KEY_UDP_DST,
					 spec.udp->hdr.dst_port);
				mnl_attr_put_u16
					(nlh, tunnel_outer ?
					 TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK :
					 TCA_FLOWER_KEY_UDP_DST_MASK,
					 mask.udp->hdr.dst_port);
			}
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			item_flags |= (item_flags & MLX5_FLOW_LAYER_TUNNEL) ?
				      MLX5_FLOW_LAYER_INNER_L4_TCP :
				      MLX5_FLOW_LAYER_OUTER_L4_TCP;
			mask.tcp = flow_tcf_item_mask
				(items, &rte_flow_item_tcp_mask,
				 &flow_tcf_mask_supported.tcp,
				 &flow_tcf_mask_empty.tcp,
				 sizeof(flow_tcf_mask_supported.tcp),
				 error);
			assert(mask.tcp);
			if (!ip_proto_set)
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_PROTO,
						IPPROTO_TCP);
			if (mask.tcp == &flow_tcf_mask_empty.tcp)
				break;
			spec.tcp = items->spec;
			if (mask.tcp->hdr.src_port) {
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_TCP_SRC,
						 spec.tcp->hdr.src_port);
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_TCP_SRC_MASK,
						 mask.tcp->hdr.src_port);
			}
			if (mask.tcp->hdr.dst_port) {
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_TCP_DST,
						 spec.tcp->hdr.dst_port);
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_TCP_DST_MASK,
						 mask.tcp->hdr.dst_port);
			}
			if (mask.tcp->hdr.tcp_flags) {
				mnl_attr_put_u16
					(nlh,
					 TCA_FLOWER_KEY_TCP_FLAGS,
					 rte_cpu_to_be_16
						(spec.tcp->hdr.tcp_flags));
				mnl_attr_put_u16
					(nlh,
					 TCA_FLOWER_KEY_TCP_FLAGS_MASK,
					 rte_cpu_to_be_16
						(mask.tcp->hdr.tcp_flags));
			}
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			assert(decap.vxlan);
			tunnel_outer = 0;
			item_flags |= MLX5_FLOW_LAYER_VXLAN;
			spec.vxlan = items->spec;
			mnl_attr_put_u32(nlh,
					 TCA_FLOWER_KEY_ENC_KEY_ID,
					 vxlan_vni_as_be32(spec.vxlan->vni));
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
	}
	/*
	 * Set the ether_type flower key and tc rule protocol:
	 * - if there is nor VLAN neither VXLAN the key is taken from
	 *   eth item directly or deduced from L3 items.
	 * - if there is vlan item then key is fixed to 802.1q.
	 * - if there is vxlan item then key is set to inner tunnel type.
	 * - simultaneous vlan and vxlan items are prohibited.
	 */
	if (outer_etype != RTE_BE16(ETH_P_ALL)) {
		tcm->tcm_info = TC_H_MAKE((attr->priority + 1) << 16,
					   outer_etype);
		if (item_flags & MLX5_FLOW_LAYER_TUNNEL) {
			if (inner_etype != RTE_BE16(ETH_P_ALL))
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_ETH_TYPE,
						 inner_etype);
		} else {
			mnl_attr_put_u16(nlh,
					 TCA_FLOWER_KEY_ETH_TYPE,
					 outer_etype);
			if (outer_etype == RTE_BE16(ETH_P_8021Q) &&
			    vlan_etype != RTE_BE16(ETH_P_ALL))
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_VLAN_ETH_TYPE,
						 vlan_etype);
		}
		assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
	}
	na_flower_act = mnl_attr_nest_start(nlh, TCA_FLOWER_ACT);
	na_act_index_cur = 1;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		struct nlattr *na_act_index;
		struct nlattr *na_act;
		unsigned int vlan_act;
		unsigned int i;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			conf.port_id = actions->conf;
			if (conf.port_id->original)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == conf.port_id->id)
						break;
			assert(ptoi[i].ifindex);
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "mirred");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			if (encap.hdr) {
				assert(dev_flow->tcf.tunnel);
				dev_flow->tcf.tunnel->ifindex_ptr =
					&((struct tc_mirred *)
					mnl_attr_get_payload
					(mnl_nlmsg_get_payload_tail
						(nlh)))->ifindex;
			} else if (decap.hdr) {
				assert(dev_flow->tcf.tunnel);
				dev_flow->tcf.tunnel->ifindex_ptr =
					(unsigned int *)&tcm->tcm_ifindex;
			}
			mnl_attr_put(nlh, TCA_MIRRED_PARMS,
				     sizeof(struct tc_mirred),
				     &(struct tc_mirred){
					.action = TC_ACT_STOLEN,
					.eaction = TCA_EGRESS_REDIR,
					.ifindex = ptoi[i].ifindex,
				     });
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			conf.jump = actions->conf;
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "gact");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_GACT_PARMS,
				     sizeof(struct tc_gact),
				     &(struct tc_gact){
					.action = TC_ACT_GOTO_CHAIN |
						  conf.jump->group,
				     });
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "gact");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_GACT_PARMS,
				     sizeof(struct tc_gact),
				     &(struct tc_gact){
					.action = TC_ACT_SHOT,
				     });
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			/*
			 * Driver adds the count action implicitly for
			 * each rule it creates.
			 */
			ret = flow_tcf_translate_action_count(dev,
							      dev_flow, error);
			if (ret < 0)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			conf.of_push_vlan = NULL;
			vlan_act = TCA_VLAN_ACT_POP;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			conf.of_push_vlan = actions->conf;
			vlan_act = TCA_VLAN_ACT_PUSH;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			conf.of_set_vlan_vid = actions->conf;
			if (na_vlan_id)
				goto override_na_vlan_id;
			vlan_act = TCA_VLAN_ACT_MODIFY;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			conf.of_set_vlan_pcp = actions->conf;
			if (na_vlan_priority)
				goto override_na_vlan_priority;
			vlan_act = TCA_VLAN_ACT_MODIFY;
			goto action_of_vlan;
action_of_vlan:
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "vlan");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_VLAN_PARMS,
				     sizeof(struct tc_vlan),
				     &(struct tc_vlan){
					.action = TC_ACT_PIPE,
					.v_action = vlan_act,
				     });
			if (vlan_act == TCA_VLAN_ACT_POP) {
				mnl_attr_nest_end(nlh, na_act);
				mnl_attr_nest_end(nlh, na_act_index);
				break;
			}
			if (vlan_act == TCA_VLAN_ACT_PUSH)
				mnl_attr_put_u16(nlh,
						 TCA_VLAN_PUSH_VLAN_PROTOCOL,
						 conf.of_push_vlan->ethertype);
			na_vlan_id = mnl_nlmsg_get_payload_tail(nlh);
			mnl_attr_put_u16(nlh, TCA_VLAN_PAD, 0);
			na_vlan_priority = mnl_nlmsg_get_payload_tail(nlh);
			mnl_attr_put_u8(nlh, TCA_VLAN_PAD, 0);
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			if (actions->type ==
			    RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID) {
override_na_vlan_id:
				na_vlan_id->nla_type = TCA_VLAN_PUSH_VLAN_ID;
				*(uint16_t *)mnl_attr_get_payload(na_vlan_id) =
					rte_be_to_cpu_16
					(conf.of_set_vlan_vid->vlan_vid);
			} else if (actions->type ==
				   RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP) {
override_na_vlan_priority:
				na_vlan_priority->nla_type =
					TCA_VLAN_PUSH_VLAN_PRIORITY;
				*(uint8_t *)mnl_attr_get_payload
					(na_vlan_priority) =
					conf.of_set_vlan_pcp->vlan_pcp;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
			assert(decap.vxlan);
			assert(dev_flow->tcf.tunnel);
			dev_flow->tcf.tunnel->ifindex_ptr =
				(unsigned int *)&tcm->tcm_ifindex;
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "tunnel_key");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_TUNNEL_KEY_PARMS,
				sizeof(struct tc_tunnel_key),
				&(struct tc_tunnel_key){
					.action = TC_ACT_PIPE,
					.t_action = TCA_TUNNEL_KEY_ACT_RELEASE,
					});
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			assert(encap.vxlan);
			flow_tcf_vxlan_encap_parse(actions, encap.vxlan);
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "tunnel_key");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_TUNNEL_KEY_PARMS,
				sizeof(struct tc_tunnel_key),
				&(struct tc_tunnel_key){
					.action = TC_ACT_PIPE,
					.t_action = TCA_TUNNEL_KEY_ACT_SET,
					});
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_UDP_DST)
				mnl_attr_put_u16(nlh,
					 TCA_TUNNEL_KEY_ENC_DST_PORT,
					 encap.vxlan->udp.dst);
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_IPV4_SRC)
				mnl_attr_put_u32(nlh,
					 TCA_TUNNEL_KEY_ENC_IPV4_SRC,
					 encap.vxlan->ipv4.src);
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_IPV4_DST)
				mnl_attr_put_u32(nlh,
					 TCA_TUNNEL_KEY_ENC_IPV4_DST,
					 encap.vxlan->ipv4.dst);
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_IPV6_SRC)
				mnl_attr_put(nlh,
					 TCA_TUNNEL_KEY_ENC_IPV6_SRC,
					 sizeof(encap.vxlan->ipv6.src),
					 &encap.vxlan->ipv6.src);
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_IPV6_DST)
				mnl_attr_put(nlh,
					 TCA_TUNNEL_KEY_ENC_IPV6_DST,
					 sizeof(encap.vxlan->ipv6.dst),
					 &encap.vxlan->ipv6.dst);
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_IP_TTL)
				mnl_attr_put_u8(nlh,
					 TCA_TUNNEL_KEY_ENC_TTL,
					 encap.vxlan->ip_ttl_hop);
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_IP_TOS)
				mnl_attr_put_u8(nlh,
					 TCA_TUNNEL_KEY_ENC_TOS,
					 encap.vxlan->ip_tos);
			if (encap.vxlan->mask & FLOW_TCF_ENCAP_VXLAN_VNI)
				mnl_attr_put_u32(nlh,
					 TCA_TUNNEL_KEY_ENC_KEY_ID,
					 vxlan_vni_as_be32
						(encap.vxlan->vxlan.vni));
			mnl_attr_put_u8(nlh, TCA_TUNNEL_KEY_NO_CSUM, 0);
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			flow_tcf_create_pedit_mnl_msg(nlh,
						      &actions, item_flags);
			mnl_attr_nest_end(nlh, na_act_index);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	assert(na_flower);
	assert(na_flower_act);
	mnl_attr_nest_end(nlh, na_flower_act);
	dev_flow->tcf.ptc_flags = mnl_attr_get_payload
					(mnl_nlmsg_get_payload_tail(nlh));
	mnl_attr_put_u32(nlh, TCA_FLOWER_FLAGS,	decap.vxlan ?
						0 : TCA_CLS_FLAGS_SKIP_SW);
	mnl_attr_nest_end(nlh, na_flower);
	if (dev_flow->tcf.tunnel && dev_flow->tcf.tunnel->ifindex_ptr)
		dev_flow->tcf.tunnel->ifindex_org =
			*dev_flow->tcf.tunnel->ifindex_ptr;
	assert(dev_flow->tcf.nlsize >= nlh->nlmsg_len);
	return 0;
}

/**
 * Send Netlink message with acknowledgment.
 *
 * @param tcf
 *   Flow context to use.
 * @param nlh
 *   Message to send. This function always raises the NLM_F_ACK flag before
 *   sending.
 * @param[in] cb
 *   Callback handler for received message.
 * @param[in] arg
 *   Context pointer for callback handler.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_nl_ack(struct mlx5_flow_tcf_context *tcf,
		struct nlmsghdr *nlh,
		mnl_cb_t cb, void *arg)
{
	unsigned int portid = mnl_socket_get_portid(tcf->nl);
	uint32_t seq = tcf->seq++;
	int ret, err = 0;

	assert(tcf->nl);
	assert(tcf->buf);
	if (!seq) {
		/* seq 0 is reserved for kernel event-driven notifications. */
		seq = tcf->seq++;
	}
	nlh->nlmsg_seq = seq;
	nlh->nlmsg_flags |= NLM_F_ACK;
	ret = mnl_socket_sendto(tcf->nl, nlh, nlh->nlmsg_len);
	if (ret <= 0) {
		/* Message send error occurred. */
		rte_errno = errno;
		return -rte_errno;
	}
	nlh = (struct nlmsghdr *)(tcf->buf);
	/*
	 * The following loop postpones non-fatal errors until multipart
	 * messages are complete.
	 */
	while (true) {
		ret = mnl_socket_recvfrom(tcf->nl, tcf->buf, tcf->buf_size);
		if (ret < 0) {
			err = errno;
			/*
			 * In case of overflow Will receive till
			 * end of multipart message. We may lost part
			 * of reply messages but mark and return an error.
			 */
			if (err != ENOSPC ||
			    !(nlh->nlmsg_flags & NLM_F_MULTI) ||
			    nlh->nlmsg_type == NLMSG_DONE)
				break;
		} else {
			ret = mnl_cb_run(nlh, ret, seq, portid, cb, arg);
			if (!ret) {
				/*
				 * libmnl returns 0 if DONE or
				 * success ACK message found.
				 */
				break;
			}
			if (ret < 0) {
				/*
				 * ACK message with error found
				 * or some error occurred.
				 */
				err = errno;
				break;
			}
			/* We should continue receiving. */
		}
	}
	if (!err)
		return 0;
	rte_errno = err;
	return -err;
}

#define MNL_BUF_EXTRA_SPACE 16
#define MNL_REQUEST_SIZE_MIN 256
#define MNL_REQUEST_SIZE_MAX 2048
#define MNL_REQUEST_SIZE RTE_MIN(RTE_MAX(sysconf(_SC_PAGESIZE), \
				 MNL_REQUEST_SIZE_MIN), MNL_REQUEST_SIZE_MAX)

/* Data structures used by flow_tcf_xxx_cb() routines. */
struct tcf_nlcb_buf {
	LIST_ENTRY(tcf_nlcb_buf) next;
	uint32_t size;
	alignas(struct nlmsghdr)
	uint8_t msg[]; /**< Netlink message data. */
};

struct tcf_nlcb_context {
	unsigned int ifindex; /**< Base interface index. */
	uint32_t bufsize;
	LIST_HEAD(, tcf_nlcb_buf) nlbuf;
};

/**
 * Allocate space for netlink command in buffer list
 *
 * @param[in, out] ctx
 *   Pointer to callback context with command buffers list.
 * @param[in] size
 *   Required size of data buffer to be allocated.
 *
 * @return
 *   Pointer to allocated memory, aligned as message header.
 *   NULL if some error occurred.
 */
static struct nlmsghdr *
flow_tcf_alloc_nlcmd(struct tcf_nlcb_context *ctx, uint32_t size)
{
	struct tcf_nlcb_buf *buf;
	struct nlmsghdr *nlh;

	size = NLMSG_ALIGN(size);
	buf = LIST_FIRST(&ctx->nlbuf);
	if (buf && (buf->size + size) <= ctx->bufsize) {
		nlh = (struct nlmsghdr *)&buf->msg[buf->size];
		buf->size += size;
		return nlh;
	}
	if (size > ctx->bufsize) {
		DRV_LOG(WARNING, "netlink: too long command buffer requested");
		return NULL;
	}
	buf = rte_malloc(__func__,
			ctx->bufsize + sizeof(struct tcf_nlcb_buf),
			alignof(struct tcf_nlcb_buf));
	if (!buf) {
		DRV_LOG(WARNING, "netlink: no memory for command buffer");
		return NULL;
	}
	LIST_INSERT_HEAD(&ctx->nlbuf, buf, next);
	buf->size = size;
	nlh = (struct nlmsghdr *)&buf->msg[0];
	return nlh;
}

/**
 * Send the buffers with prepared netlink commands. Scans the list and
 * sends all found buffers. Buffers are sent and freed anyway in order
 * to prevent memory leakage if some every message in received packet.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in, out] ctx
 *   Pointer to callback context with command buffers list.
 *
 * @return
 *   Zero value on success, negative errno value otherwise
 *   and rte_errno is set.
 */
static int
flow_tcf_send_nlcmd(struct mlx5_flow_tcf_context *tcf,
		    struct tcf_nlcb_context *ctx)
{
	struct tcf_nlcb_buf *bc = LIST_FIRST(&ctx->nlbuf);
	int ret = 0;

	while (bc) {
		struct tcf_nlcb_buf *bn = LIST_NEXT(bc, next);
		struct nlmsghdr *nlh;
		uint32_t msg = 0;
		int rc;

		while (msg < bc->size) {
			/*
			 * Send Netlink commands from buffer in one by one
			 * fashion. If we send multiple rule deletion commands
			 * in one Netlink message and some error occurs it may
			 * cause multiple ACK error messages and break sequence
			 * numbers of Netlink communication, because we expect
			 * the only one ACK reply.
			 */
			assert((bc->size - msg) >= sizeof(struct nlmsghdr));
			nlh = (struct nlmsghdr *)&bc->msg[msg];
			assert((bc->size - msg) >= nlh->nlmsg_len);
			msg += nlh->nlmsg_len;
			rc = flow_tcf_nl_ack(tcf, nlh, NULL, NULL);
			if (rc) {
				DRV_LOG(WARNING,
					"netlink: cleanup error %d", rc);
				if (!ret)
					ret = rc;
			}
		}
		rte_free(bc);
		bc = bn;
	}
	LIST_INIT(&ctx->nlbuf);
	return ret;
}

/**
 * Collect local IP address rules with scope link attribute  on specified
 * network device. This is callback routine called by libmnl mnl_cb_run()
 * in loop for every message in received packet.
 *
 * @param[in] nlh
 *   Pointer to reply header.
 * @param[in, out] arg
 *   Opaque data pointer for this callback.
 *
 * @return
 *   A positive, nonzero value on success, negative errno value otherwise
 *   and rte_errno is set.
 */
static int
flow_tcf_collect_local_cb(const struct nlmsghdr *nlh, void *arg)
{
	struct tcf_nlcb_context *ctx = arg;
	struct nlmsghdr *cmd;
	struct ifaddrmsg *ifa;
	struct nlattr *na;
	struct nlattr *na_local = NULL;
	struct nlattr *na_peer = NULL;
	unsigned char family;
	uint32_t size;

	if (nlh->nlmsg_type != RTM_NEWADDR) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ifa = mnl_nlmsg_get_payload(nlh);
	family = ifa->ifa_family;
	if (ifa->ifa_index != ctx->ifindex ||
	    ifa->ifa_scope != RT_SCOPE_LINK ||
	    !(ifa->ifa_flags & IFA_F_PERMANENT) ||
	    (family != AF_INET && family != AF_INET6))
		return 1;
	mnl_attr_for_each(na, nlh, sizeof(*ifa)) {
		switch (mnl_attr_get_type(na)) {
		case IFA_LOCAL:
			na_local = na;
			break;
		case IFA_ADDRESS:
			na_peer = na;
			break;
		}
		if (na_local && na_peer)
			break;
	}
	if (!na_local || !na_peer)
		return 1;
	/* Local rule found with scope link, permanent and assigned peer. */
	size = MNL_ALIGN(sizeof(struct nlmsghdr)) +
	       MNL_ALIGN(sizeof(struct ifaddrmsg)) +
	       (family == AF_INET6 ? 2 * SZ_NLATTR_DATA_OF(IPV6_ADDR_LEN)
				   : 2 * SZ_NLATTR_TYPE_OF(uint32_t));
	cmd = flow_tcf_alloc_nlcmd(ctx, size);
	if (!cmd) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	cmd = mnl_nlmsg_put_header(cmd);
	cmd->nlmsg_type = RTM_DELADDR;
	cmd->nlmsg_flags = NLM_F_REQUEST;
	ifa = mnl_nlmsg_put_extra_header(cmd, sizeof(*ifa));
	ifa->ifa_flags = IFA_F_PERMANENT;
	ifa->ifa_scope = RT_SCOPE_LINK;
	ifa->ifa_index = ctx->ifindex;
	if (family == AF_INET) {
		ifa->ifa_family = AF_INET;
		ifa->ifa_prefixlen = 32;
		mnl_attr_put_u32(cmd, IFA_LOCAL, mnl_attr_get_u32(na_local));
		mnl_attr_put_u32(cmd, IFA_ADDRESS, mnl_attr_get_u32(na_peer));
	} else {
		ifa->ifa_family = AF_INET6;
		ifa->ifa_prefixlen = 128;
		mnl_attr_put(cmd, IFA_LOCAL, IPV6_ADDR_LEN,
			mnl_attr_get_payload(na_local));
		mnl_attr_put(cmd, IFA_ADDRESS, IPV6_ADDR_LEN,
			mnl_attr_get_payload(na_peer));
	}
	assert(size == cmd->nlmsg_len);
	return 1;
}

/**
 * Cleanup the local IP addresses on outer interface.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] ifindex
 *   Network interface index to perform cleanup.
 */
static void
flow_tcf_encap_local_cleanup(struct mlx5_flow_tcf_context *tcf,
			    unsigned int ifindex)
{
	struct nlmsghdr *nlh;
	struct ifaddrmsg *ifa;
	struct tcf_nlcb_context ctx = {
		.ifindex = ifindex,
		.bufsize = MNL_REQUEST_SIZE,
		.nlbuf = LIST_HEAD_INITIALIZER(),
	};
	int ret;

	assert(ifindex);
	/*
	 * Seek and destroy leftovers of local IP addresses with
	 * matching properties "scope link".
	 */
	nlh = mnl_nlmsg_put_header(tcf->buf);
	nlh->nlmsg_type = RTM_GETADDR;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	ifa = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifa));
	ifa->ifa_family = AF_UNSPEC;
	ifa->ifa_index = ifindex;
	ifa->ifa_scope = RT_SCOPE_LINK;
	ret = flow_tcf_nl_ack(tcf, nlh, flow_tcf_collect_local_cb, &ctx);
	if (ret)
		DRV_LOG(WARNING, "netlink: query device list error %d", ret);
	ret = flow_tcf_send_nlcmd(tcf, &ctx);
	if (ret)
		DRV_LOG(WARNING, "netlink: device delete error %d", ret);
}

/**
 * Collect neigh permanent rules on specified network device.
 * This is callback routine called by libmnl mnl_cb_run() in loop for
 * every message in received packet.
 *
 * @param[in] nlh
 *   Pointer to reply header.
 * @param[in, out] arg
 *   Opaque data pointer for this callback.
 *
 * @return
 *   A positive, nonzero value on success, negative errno value otherwise
 *   and rte_errno is set.
 */
static int
flow_tcf_collect_neigh_cb(const struct nlmsghdr *nlh, void *arg)
{
	struct tcf_nlcb_context *ctx = arg;
	struct nlmsghdr *cmd;
	struct ndmsg *ndm;
	struct nlattr *na;
	struct nlattr *na_ip = NULL;
	struct nlattr *na_mac = NULL;
	unsigned char family;
	uint32_t size;

	if (nlh->nlmsg_type != RTM_NEWNEIGH) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ndm = mnl_nlmsg_get_payload(nlh);
	family = ndm->ndm_family;
	if (ndm->ndm_ifindex != (int)ctx->ifindex ||
	   !(ndm->ndm_state & NUD_PERMANENT) ||
	   (family != AF_INET && family != AF_INET6))
		return 1;
	mnl_attr_for_each(na, nlh, sizeof(*ndm)) {
		switch (mnl_attr_get_type(na)) {
		case NDA_DST:
			na_ip = na;
			break;
		case NDA_LLADDR:
			na_mac = na;
			break;
		}
		if (na_mac && na_ip)
			break;
	}
	if (!na_mac || !na_ip)
		return 1;
	/* Neigh rule with permanent attribute found. */
	size = MNL_ALIGN(sizeof(struct nlmsghdr)) +
	       MNL_ALIGN(sizeof(struct ndmsg)) +
	       SZ_NLATTR_DATA_OF(ETHER_ADDR_LEN) +
	       (family == AF_INET6 ? SZ_NLATTR_DATA_OF(IPV6_ADDR_LEN)
				   : SZ_NLATTR_TYPE_OF(uint32_t));
	cmd = flow_tcf_alloc_nlcmd(ctx, size);
	if (!cmd) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	cmd = mnl_nlmsg_put_header(cmd);
	cmd->nlmsg_type = RTM_DELNEIGH;
	cmd->nlmsg_flags = NLM_F_REQUEST;
	ndm = mnl_nlmsg_put_extra_header(cmd, sizeof(*ndm));
	ndm->ndm_ifindex = ctx->ifindex;
	ndm->ndm_state = NUD_PERMANENT;
	ndm->ndm_flags = 0;
	ndm->ndm_type = 0;
	if (family == AF_INET) {
		ndm->ndm_family = AF_INET;
		mnl_attr_put_u32(cmd, NDA_DST, mnl_attr_get_u32(na_ip));
	} else {
		ndm->ndm_family = AF_INET6;
		mnl_attr_put(cmd, NDA_DST, IPV6_ADDR_LEN,
			     mnl_attr_get_payload(na_ip));
	}
	mnl_attr_put(cmd, NDA_LLADDR, ETHER_ADDR_LEN,
		     mnl_attr_get_payload(na_mac));
	assert(size == cmd->nlmsg_len);
	return 1;
}

/**
 * Cleanup the neigh rules on outer interface.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] ifindex
 *   Network interface index to perform cleanup.
 */
static void
flow_tcf_encap_neigh_cleanup(struct mlx5_flow_tcf_context *tcf,
			    unsigned int ifindex)
{
	struct nlmsghdr *nlh;
	struct ndmsg *ndm;
	struct tcf_nlcb_context ctx = {
		.ifindex = ifindex,
		.bufsize = MNL_REQUEST_SIZE,
		.nlbuf = LIST_HEAD_INITIALIZER(),
	};
	int ret;

	assert(ifindex);
	/* Seek and destroy leftovers of neigh rules. */
	nlh = mnl_nlmsg_put_header(tcf->buf);
	nlh->nlmsg_type = RTM_GETNEIGH;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	ndm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ndm));
	ndm->ndm_family = AF_UNSPEC;
	ndm->ndm_ifindex = ifindex;
	ndm->ndm_state = NUD_PERMANENT;
	ret = flow_tcf_nl_ack(tcf, nlh, flow_tcf_collect_neigh_cb, &ctx);
	if (ret)
		DRV_LOG(WARNING, "netlink: query device list error %d", ret);
	ret = flow_tcf_send_nlcmd(tcf, &ctx);
	if (ret)
		DRV_LOG(WARNING, "netlink: device delete error %d", ret);
}

/**
 * Collect indices of VXLAN encap/decap interfaces associated with device.
 * This is callback routine called by libmnl mnl_cb_run() in loop for
 * every message in received packet.
 *
 * @param[in] nlh
 *   Pointer to reply header.
 * @param[in, out] arg
 *   Opaque data pointer for this callback.
 *
 * @return
 *   A positive, nonzero value on success, negative errno value otherwise
 *   and rte_errno is set.
 */
static int
flow_tcf_collect_vxlan_cb(const struct nlmsghdr *nlh, void *arg)
{
	struct tcf_nlcb_context *ctx = arg;
	struct nlmsghdr *cmd;
	struct ifinfomsg *ifm;
	struct nlattr *na;
	struct nlattr *na_info = NULL;
	struct nlattr *na_vxlan = NULL;
	bool found = false;
	unsigned int vxindex;
	uint32_t size;

	if (nlh->nlmsg_type != RTM_NEWLINK) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ifm = mnl_nlmsg_get_payload(nlh);
	if (!ifm->ifi_index) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	mnl_attr_for_each(na, nlh, sizeof(*ifm))
		if (mnl_attr_get_type(na) == IFLA_LINKINFO) {
			na_info = na;
			break;
		}
	if (!na_info)
		return 1;
	mnl_attr_for_each_nested(na, na_info) {
		switch (mnl_attr_get_type(na)) {
		case IFLA_INFO_KIND:
			if (!strncmp("vxlan", mnl_attr_get_str(na),
				     mnl_attr_get_len(na)))
				found = true;
			break;
		case IFLA_INFO_DATA:
			na_vxlan = na;
			break;
		}
		if (found && na_vxlan)
			break;
	}
	if (!found || !na_vxlan)
		return 1;
	found = false;
	mnl_attr_for_each_nested(na, na_vxlan) {
		if (mnl_attr_get_type(na) == IFLA_VXLAN_LINK &&
		    mnl_attr_get_u32(na) == ctx->ifindex) {
			found = true;
			break;
		}
	}
	if (!found)
		return 1;
	/* Attached VXLAN device found, store the command to delete. */
	vxindex = ifm->ifi_index;
	size = MNL_ALIGN(sizeof(struct nlmsghdr)) +
	       MNL_ALIGN(sizeof(struct ifinfomsg));
	cmd = flow_tcf_alloc_nlcmd(ctx, size);
	if (!cmd) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	cmd = mnl_nlmsg_put_header(cmd);
	cmd->nlmsg_type = RTM_DELLINK;
	cmd->nlmsg_flags = NLM_F_REQUEST;
	ifm = mnl_nlmsg_put_extra_header(cmd, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_index = vxindex;
	assert(size == cmd->nlmsg_len);
	return 1;
}

/**
 * Cleanup the outer interface. Removes all found vxlan devices
 * attached to specified index, flushes the neigh and local IP
 * database.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] ifindex
 *   Network inferface index to perform cleanup.
 */
static void
flow_tcf_encap_iface_cleanup(struct mlx5_flow_tcf_context *tcf,
			    unsigned int ifindex)
{
	struct nlmsghdr *nlh;
	struct ifinfomsg *ifm;
	struct tcf_nlcb_context ctx = {
		.ifindex = ifindex,
		.bufsize = MNL_REQUEST_SIZE,
		.nlbuf = LIST_HEAD_INITIALIZER(),
	};
	int ret;

	assert(ifindex);
	/*
	 * Seek and destroy leftover VXLAN encap/decap interfaces with
	 * matching properties.
	 */
	nlh = mnl_nlmsg_put_header(tcf->buf);
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ret = flow_tcf_nl_ack(tcf, nlh, flow_tcf_collect_vxlan_cb, &ctx);
	if (ret)
		DRV_LOG(WARNING, "netlink: query device list error %d", ret);
	ret = flow_tcf_send_nlcmd(tcf, &ctx);
	if (ret)
		DRV_LOG(WARNING, "netlink: device delete error %d", ret);
}

/**
 * Emit Netlink message to add/remove local address to the outer device.
 * The address being added is visible within the link only (scope link).
 *
 * Note that an implicit route is maintained by the kernel due to the
 * presence of a peer address (IFA_ADDRESS).
 *
 * These rules are used for encapsulation only and allow to assign
 * the outer tunnel source IP address.
 *
 * @param[in] tcf
 *   Libmnl socket context object.
 * @param[in] encap
 *   Encapsulation properties (source address and its peer).
 * @param[in] ifindex
 *   Network interface to apply rule.
 * @param[in] enable
 *   Toggle between add and remove.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_rule_local(struct mlx5_flow_tcf_context *tcf,
		    const struct flow_tcf_vxlan_encap *encap,
		    unsigned int ifindex,
		    bool enable,
		    struct rte_flow_error *error)
{
	struct nlmsghdr *nlh;
	struct ifaddrmsg *ifa;
	alignas(struct nlmsghdr)
	uint8_t buf[mnl_nlmsg_size(sizeof(*ifa) + 128)];

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = enable ? RTM_NEWADDR : RTM_DELADDR;
	nlh->nlmsg_flags =
		NLM_F_REQUEST | (enable ? NLM_F_CREATE | NLM_F_REPLACE : 0);
	nlh->nlmsg_seq = 0;
	ifa = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifa));
	ifa->ifa_flags = IFA_F_PERMANENT;
	ifa->ifa_scope = RT_SCOPE_LINK;
	ifa->ifa_index = ifindex;
	if (encap->mask & FLOW_TCF_ENCAP_IPV4_SRC) {
		ifa->ifa_family = AF_INET;
		ifa->ifa_prefixlen = 32;
		mnl_attr_put_u32(nlh, IFA_LOCAL, encap->ipv4.src);
		if (encap->mask & FLOW_TCF_ENCAP_IPV4_DST)
			mnl_attr_put_u32(nlh, IFA_ADDRESS,
					      encap->ipv4.dst);
	} else {
		assert(encap->mask & FLOW_TCF_ENCAP_IPV6_SRC);
		ifa->ifa_family = AF_INET6;
		ifa->ifa_prefixlen = 128;
		mnl_attr_put(nlh, IFA_LOCAL,
				  sizeof(encap->ipv6.src),
				  &encap->ipv6.src);
		if (encap->mask & FLOW_TCF_ENCAP_IPV6_DST)
			mnl_attr_put(nlh, IFA_ADDRESS,
					  sizeof(encap->ipv6.dst),
					  &encap->ipv6.dst);
	}
	if (!flow_tcf_nl_ack(tcf, nlh, NULL, NULL))
		return 0;
	return rte_flow_error_set(error, rte_errno,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "netlink: cannot complete IFA request"
				  " (ip addr add)");
}

/**
 * Emit Netlink message to add/remove neighbor.
 *
 * @param[in] tcf
 *   Libmnl socket context object.
 * @param[in] encap
 *   Encapsulation properties (destination address).
 * @param[in] ifindex
 *   Network interface.
 * @param[in] enable
 *   Toggle between add and remove.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_rule_neigh(struct mlx5_flow_tcf_context *tcf,
		     const struct flow_tcf_vxlan_encap *encap,
		     unsigned int ifindex,
		     bool enable,
		     struct rte_flow_error *error)
{
	struct nlmsghdr *nlh;
	struct ndmsg *ndm;
	alignas(struct nlmsghdr)
	uint8_t buf[mnl_nlmsg_size(sizeof(*ndm) + 128)];

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = enable ? RTM_NEWNEIGH : RTM_DELNEIGH;
	nlh->nlmsg_flags =
		NLM_F_REQUEST | (enable ? NLM_F_CREATE | NLM_F_REPLACE : 0);
	nlh->nlmsg_seq = 0;
	ndm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ndm));
	ndm->ndm_ifindex = ifindex;
	ndm->ndm_state = NUD_PERMANENT;
	ndm->ndm_flags = 0;
	ndm->ndm_type = 0;
	if (encap->mask & FLOW_TCF_ENCAP_IPV4_DST) {
		ndm->ndm_family = AF_INET;
		mnl_attr_put_u32(nlh, NDA_DST, encap->ipv4.dst);
	} else {
		assert(encap->mask & FLOW_TCF_ENCAP_IPV6_DST);
		ndm->ndm_family = AF_INET6;
		mnl_attr_put(nlh, NDA_DST, sizeof(encap->ipv6.dst),
						 &encap->ipv6.dst);
	}
	if (encap->mask & FLOW_TCF_ENCAP_ETH_SRC && enable)
		DRV_LOG(WARNING,
			"outer ethernet source address cannot be "
			"forced for VXLAN encapsulation");
	if (encap->mask & FLOW_TCF_ENCAP_ETH_DST)
		mnl_attr_put(nlh, NDA_LLADDR, sizeof(encap->eth.dst),
						    &encap->eth.dst);
	if (!flow_tcf_nl_ack(tcf, nlh, NULL, NULL))
		return 0;
	return rte_flow_error_set(error, rte_errno,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "netlink: cannot complete ND request"
				  " (ip neigh)");
}

/**
 * Manage the local IP addresses and their peers IP addresses on the
 * outer interface for encapsulation purposes. The kernel searches the
 * appropriate device for tunnel egress traffic using the outer source
 * IP, this IP should be assigned to the outer network device, otherwise
 * kernel rejects the rule.
 *
 * Adds or removes the addresses using the Netlink command like this:
 *   ip addr add <src_ip> peer <dst_ip> scope link dev <ifouter>
 *
 * The addresses are local to the netdev ("scope link"), this reduces
 * the risk of conflicts. Note that an implicit route is maintained by
 * the kernel due to the presence of a peer address (IFA_ADDRESS).
 *
 * @param[in] tcf
 *   Libmnl socket context object.
 * @param[in] iface
 *   Object, contains rule database and ifouter index.
 * @param[in] dev_flow
 *   Flow object, contains the tunnel parameters (for encap only).
 * @param[in] enable
 *   Toggle between add and remove.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_encap_local(struct mlx5_flow_tcf_context *tcf,
		     struct tcf_irule *iface,
		     struct mlx5_flow *dev_flow,
		     bool enable,
		     struct rte_flow_error *error)
{
	const struct flow_tcf_vxlan_encap *encap = dev_flow->tcf.vxlan_encap;
	struct tcf_local_rule *rule = NULL;
	int ret;

	assert(encap);
	assert(encap->hdr.type == FLOW_TCF_TUNACT_VXLAN_ENCAP);
	if (encap->mask & FLOW_TCF_ENCAP_IPV4_SRC) {
		assert(encap->mask & FLOW_TCF_ENCAP_IPV4_DST);
		LIST_FOREACH(rule, &iface->local, next) {
			if (rule->mask & FLOW_TCF_ENCAP_IPV4_SRC &&
			    encap->ipv4.src == rule->ipv4.src &&
			    encap->ipv4.dst == rule->ipv4.dst) {
				break;
			}
		}
	} else {
		assert(encap->mask & FLOW_TCF_ENCAP_IPV6_SRC);
		assert(encap->mask & FLOW_TCF_ENCAP_IPV6_DST);
		LIST_FOREACH(rule, &iface->local, next) {
			if (rule->mask & FLOW_TCF_ENCAP_IPV6_SRC &&
			    !memcmp(&encap->ipv6.src, &rule->ipv6.src,
					    sizeof(encap->ipv6.src)) &&
			    !memcmp(&encap->ipv6.dst, &rule->ipv6.dst,
					    sizeof(encap->ipv6.dst))) {
				break;
			}
		}
	}
	if (rule) {
		if (enable) {
			rule->refcnt++;
			return 0;
		}
		if (!rule->refcnt || !--rule->refcnt) {
			LIST_REMOVE(rule, next);
			return flow_tcf_rule_local(tcf, encap,
					iface->ifouter, false, error);
		}
		return 0;
	}
	if (!enable) {
		DRV_LOG(WARNING, "disabling not existing local rule");
		rte_flow_error_set(error, ENOENT,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "disabling not existing local rule");
		return -ENOENT;
	}
	rule = rte_zmalloc(__func__, sizeof(struct tcf_local_rule),
				alignof(struct tcf_local_rule));
	if (!rule) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "unable to allocate memory for local rule");
		return -rte_errno;
	}
	*rule = (struct tcf_local_rule){.refcnt = 0,
					.mask = 0,
					};
	if (encap->mask & FLOW_TCF_ENCAP_IPV4_SRC) {
		rule->mask = FLOW_TCF_ENCAP_IPV4_SRC
			   | FLOW_TCF_ENCAP_IPV4_DST;
		rule->ipv4.src = encap->ipv4.src;
		rule->ipv4.dst = encap->ipv4.dst;
	} else {
		rule->mask = FLOW_TCF_ENCAP_IPV6_SRC
			   | FLOW_TCF_ENCAP_IPV6_DST;
		memcpy(&rule->ipv6.src, &encap->ipv6.src, IPV6_ADDR_LEN);
		memcpy(&rule->ipv6.dst, &encap->ipv6.dst, IPV6_ADDR_LEN);
	}
	ret = flow_tcf_rule_local(tcf, encap, iface->ifouter, true, error);
	if (ret) {
		rte_free(rule);
		return ret;
	}
	rule->refcnt++;
	LIST_INSERT_HEAD(&iface->local, rule, next);
	return 0;
}

/**
 * Manage the destination MAC/IP addresses neigh database, kernel uses
 * this one to determine the destination MAC address within encapsulation
 * header. Adds or removes the entries using the Netlink command like this:
 *   ip neigh add dev <ifouter> lladdr <dst_mac> to <dst_ip> nud permanent
 *
 * @param[in] tcf
 *   Libmnl socket context object.
 * @param[in] iface
 *   Object, contains rule database and ifouter index.
 * @param[in] dev_flow
 *   Flow object, contains the tunnel parameters (for encap only).
 * @param[in] enable
 *   Toggle between add and remove.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_encap_neigh(struct mlx5_flow_tcf_context *tcf,
		     struct tcf_irule *iface,
		     struct mlx5_flow *dev_flow,
		     bool enable,
		     struct rte_flow_error *error)
{
	const struct flow_tcf_vxlan_encap *encap = dev_flow->tcf.vxlan_encap;
	struct tcf_neigh_rule *rule = NULL;
	int ret;

	assert(encap);
	assert(encap->hdr.type == FLOW_TCF_TUNACT_VXLAN_ENCAP);
	if (encap->mask & FLOW_TCF_ENCAP_IPV4_DST) {
		assert(encap->mask & FLOW_TCF_ENCAP_IPV4_SRC);
		LIST_FOREACH(rule, &iface->neigh, next) {
			if (rule->mask & FLOW_TCF_ENCAP_IPV4_DST &&
			    encap->ipv4.dst == rule->ipv4.dst) {
				break;
			}
		}
	} else {
		assert(encap->mask & FLOW_TCF_ENCAP_IPV6_SRC);
		assert(encap->mask & FLOW_TCF_ENCAP_IPV6_DST);
		LIST_FOREACH(rule, &iface->neigh, next) {
			if (rule->mask & FLOW_TCF_ENCAP_IPV6_DST &&
			    !memcmp(&encap->ipv6.dst, &rule->ipv6.dst,
						sizeof(encap->ipv6.dst))) {
				break;
			}
		}
	}
	if (rule) {
		if (memcmp(&encap->eth.dst, &rule->eth,
			   sizeof(encap->eth.dst))) {
			DRV_LOG(WARNING, "Destination MAC differs"
					 " in neigh rule");
			rte_flow_error_set(error, EEXIST,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "Different MAC address"
					   " neigh rule for the same"
					   " destination IP");
					return -EEXIST;
		}
		if (enable) {
			rule->refcnt++;
			return 0;
		}
		if (!rule->refcnt || !--rule->refcnt) {
			LIST_REMOVE(rule, next);
			return flow_tcf_rule_neigh(tcf, encap,
						   iface->ifouter,
						   false, error);
		}
		return 0;
	}
	if (!enable) {
		DRV_LOG(WARNING, "Disabling not existing neigh rule");
		rte_flow_error_set(error, ENOENT,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "unable to allocate memory for neigh rule");
		return -ENOENT;
	}
	rule = rte_zmalloc(__func__, sizeof(struct tcf_neigh_rule),
				alignof(struct tcf_neigh_rule));
	if (!rule) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "unable to allocate memory for neigh rule");
		return -rte_errno;
	}
	*rule = (struct tcf_neigh_rule){.refcnt = 0,
					.mask = 0,
					};
	if (encap->mask & FLOW_TCF_ENCAP_IPV4_DST) {
		rule->mask = FLOW_TCF_ENCAP_IPV4_DST;
		rule->ipv4.dst = encap->ipv4.dst;
	} else {
		rule->mask = FLOW_TCF_ENCAP_IPV6_DST;
		memcpy(&rule->ipv6.dst, &encap->ipv6.dst, IPV6_ADDR_LEN);
	}
	memcpy(&rule->eth, &encap->eth.dst, sizeof(rule->eth));
	ret = flow_tcf_rule_neigh(tcf, encap, iface->ifouter, true, error);
	if (ret) {
		rte_free(rule);
		return ret;
	}
	rule->refcnt++;
	LIST_INSERT_HEAD(&iface->neigh, rule, next);
	return 0;
}

/* VXLAN encap rule database for outer interfaces. */
static  LIST_HEAD(, tcf_irule) iface_list_vxlan = LIST_HEAD_INITIALIZER();

/* VTEP device list is shared between PMD port instances. */
static LIST_HEAD(, tcf_vtep) vtep_list_vxlan = LIST_HEAD_INITIALIZER();
static pthread_mutex_t vtep_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Acquire the VXLAN encap rules container for specified interface.
 * First looks for the container in the existing ones list, creates
 * and initializes the new container if existing not found.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] ifouter
 *   Network interface index to create VXLAN encap rules on.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   Rule container pointer on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct tcf_irule*
flow_tcf_encap_irule_acquire(struct mlx5_flow_tcf_context *tcf,
			     unsigned int ifouter,
			     struct rte_flow_error *error)
{
	struct tcf_irule *iface;

	/* Look whether the container for encap rules is created. */
	assert(ifouter);
	LIST_FOREACH(iface, &iface_list_vxlan, next) {
		if (iface->ifouter == ifouter)
			break;
	}
	if (iface) {
		/* Container already exists, just increment the reference. */
		iface->refcnt++;
		return iface;
	}
	/* Not found, we should create the new container. */
	iface = rte_zmalloc(__func__, sizeof(*iface),
			    alignof(struct tcf_irule));
	if (!iface) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "unable to allocate memory for container");
		return NULL;
	}
	*iface = (struct tcf_irule){
			.local = LIST_HEAD_INITIALIZER(),
			.neigh = LIST_HEAD_INITIALIZER(),
			.ifouter = ifouter,
			.refcnt = 1,
	};
	/* Interface cleanup for new container created. */
	flow_tcf_encap_iface_cleanup(tcf, ifouter);
	flow_tcf_encap_local_cleanup(tcf, ifouter);
	flow_tcf_encap_neigh_cleanup(tcf, ifouter);
	LIST_INSERT_HEAD(&iface_list_vxlan, iface, next);
	return iface;
}

/**
 * Releases VXLAN encap rules container by pointer. Decrements the
 * reference counter and deletes the container if counter is zero.
 *
 * @param[in] irule
 *   VXLAN rule container pointer to release.
 */
static void
flow_tcf_encap_irule_release(struct tcf_irule *iface)
{
	assert(iface->refcnt);
	if (--iface->refcnt == 0) {
		/* Reference counter is zero, delete the container. */
		assert(LIST_EMPTY(&iface->local));
		assert(LIST_EMPTY(&iface->neigh));
		LIST_REMOVE(iface, next);
		rte_free(iface);
	}
}

/**
 * Deletes VTEP network device.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] vtep
 *   Object representing the network device to delete. Memory
 *   allocated for this object is freed by routine.
 */
static void
flow_tcf_vtep_delete(struct mlx5_flow_tcf_context *tcf,
		     struct tcf_vtep *vtep)
{
	struct nlmsghdr *nlh;
	struct ifinfomsg *ifm;
	alignas(struct nlmsghdr)
	uint8_t buf[mnl_nlmsg_size(MNL_ALIGN(sizeof(*ifm))) +
		    MNL_BUF_EXTRA_SPACE];
	int ret;

	assert(!vtep->refcnt);
	/* Delete only ifaces those we actually created. */
	if (vtep->created && vtep->ifindex) {
		DRV_LOG(INFO, "VTEP delete (%d)", vtep->ifindex);
		nlh = mnl_nlmsg_put_header(buf);
		nlh->nlmsg_type = RTM_DELLINK;
		nlh->nlmsg_flags = NLM_F_REQUEST;
		ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
		ifm->ifi_family = AF_UNSPEC;
		ifm->ifi_index = vtep->ifindex;
		assert(sizeof(buf) >= nlh->nlmsg_len);
		ret = flow_tcf_nl_ack(tcf, nlh, NULL, NULL);
		if (ret)
			DRV_LOG(WARNING, "netlink: error deleting vxlan"
					 " encap/decap ifindex %u",
					 ifm->ifi_index);
	}
	rte_free(vtep);
}

/**
 * Creates VTEP network device.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] port
 *   UDP port of created VTEP device.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 * Pointer to created device structure on success,
 * NULL otherwise and rte_errno is set.
 */
static struct tcf_vtep*
flow_tcf_vtep_create(struct mlx5_flow_tcf_context *tcf,
		     uint16_t port, struct rte_flow_error *error)
{
	struct tcf_vtep *vtep;
	struct nlmsghdr *nlh;
	struct ifinfomsg *ifm;
	char name[sizeof(MLX5_VXLAN_DEVICE_PFX) + 24];
	alignas(struct nlmsghdr)
	uint8_t buf[mnl_nlmsg_size(sizeof(*ifm)) +
		    SZ_NLATTR_DATA_OF(sizeof(name)) +
		    SZ_NLATTR_NEST * 2 +
		    SZ_NLATTR_STRZ_OF("vxlan") +
		    SZ_NLATTR_DATA_OF(sizeof(uint32_t)) +
		    SZ_NLATTR_DATA_OF(sizeof(uint16_t)) +
		    SZ_NLATTR_DATA_OF(sizeof(uint8_t)) * 3 +
		    MNL_BUF_EXTRA_SPACE];
	struct nlattr *na_info;
	struct nlattr *na_vxlan;
	rte_be16_t vxlan_port = rte_cpu_to_be_16(port);
	int ret;

	vtep = rte_zmalloc(__func__, sizeof(*vtep), alignof(struct tcf_vtep));
	if (!vtep) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "unable to allocate memory for VTEP");
		return NULL;
	}
	*vtep = (struct tcf_vtep){
			.port = port,
	};
	memset(buf, 0, sizeof(buf));
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE  | NLM_F_EXCL;
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_type = 0;
	ifm->ifi_index = 0;
	ifm->ifi_flags = IFF_UP;
	ifm->ifi_change = 0xffffffff;
	snprintf(name, sizeof(name), "%s%u", MLX5_VXLAN_DEVICE_PFX, port);
	mnl_attr_put_strz(nlh, IFLA_IFNAME, name);
	na_info = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	assert(na_info);
	mnl_attr_put_strz(nlh, IFLA_INFO_KIND, "vxlan");
	na_vxlan = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
	assert(na_vxlan);
#ifdef HAVE_IFLA_VXLAN_COLLECT_METADATA
	/*
	 * RH 7.2 does not support metadata for tunnel device.
	 * It does not matter because we are going to use the
	 * hardware offload by mlx5 driver.
	 */
	mnl_attr_put_u8(nlh, IFLA_VXLAN_COLLECT_METADATA, 1);
#endif
	mnl_attr_put_u8(nlh, IFLA_VXLAN_UDP_ZERO_CSUM6_RX, 1);
	mnl_attr_put_u8(nlh, IFLA_VXLAN_LEARNING, 0);
	mnl_attr_put_u16(nlh, IFLA_VXLAN_PORT, vxlan_port);
#ifndef HAVE_IFLA_VXLAN_COLLECT_METADATA
	/*
	 *  We must specify VNI explicitly if metadata not supported.
	 *  Note, VNI is transferred with native endianness format.
	 */
	mnl_attr_put_u16(nlh, IFLA_VXLAN_ID, MLX5_VXLAN_DEFAULT_VNI);
#endif
	mnl_attr_nest_end(nlh, na_vxlan);
	mnl_attr_nest_end(nlh, na_info);
	assert(sizeof(buf) >= nlh->nlmsg_len);
	ret = flow_tcf_nl_ack(tcf, nlh, NULL, NULL);
	if (ret) {
		DRV_LOG(WARNING,
			"netlink: VTEP %s create failure (%d)",
			name, rte_errno);
		if (rte_errno != EEXIST)
			/*
			 * Some unhandled error occurred or device is
			 * for encapsulation and cannot be shared.
			 */
			goto error;
	} else {
		/*
		 * Mark device we actually created.
		 * We should explicitly delete
		 * when we do not need it anymore.
		 */
		vtep->created = 1;
		vtep->waitreg = 1;
	}
	/* Try to get ifindex of created of pre-existing device. */
	ret = if_nametoindex(name);
	if (!ret) {
		DRV_LOG(WARNING,
			"VTEP %s failed to get index (%d)", name, errno);
		rte_flow_error_set
			(error, -errno,
			 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "netlink: failed to retrieve VTEP ifindex");
		goto error;
	}
	vtep->ifindex = ret;
	memset(buf, 0, sizeof(buf));
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_type = 0;
	ifm->ifi_index = vtep->ifindex;
	ifm->ifi_flags = IFF_UP;
	ifm->ifi_change = IFF_UP;
	ret = flow_tcf_nl_ack(tcf, nlh, NULL, NULL);
	if (ret) {
		rte_flow_error_set(error, -errno,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "netlink: failed to set VTEP link up");
		DRV_LOG(WARNING, "netlink: VTEP %s set link up failure (%d)",
			name, rte_errno);
		goto clean;
	}
	ret = mlx5_flow_tcf_init(tcf, vtep->ifindex, error);
	if (ret) {
		DRV_LOG(WARNING, "VTEP %s init failure (%d)", name, rte_errno);
		goto clean;
	}
	DRV_LOG(INFO, "VTEP create (%d, %d)", vtep->port, vtep->ifindex);
	vtep->refcnt = 1;
	return vtep;
clean:
	flow_tcf_vtep_delete(tcf, vtep);
	return NULL;
error:
	rte_free(vtep);
	return NULL;
}

/**
 * Acquire target interface index for VXLAN tunneling decapsulation.
 * In order to share the UDP port within the other interfaces the
 * VXLAN device created as not attached to any interface (if created).
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] dev_flow
 *   Flow tcf object with tunnel structure pointer set.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   Interface descriptor pointer on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct tcf_vtep*
flow_tcf_decap_vtep_acquire(struct mlx5_flow_tcf_context *tcf,
			    struct mlx5_flow *dev_flow,
			    struct rte_flow_error *error)
{
	struct tcf_vtep *vtep;
	uint16_t port = dev_flow->tcf.vxlan_decap->udp_port;

	LIST_FOREACH(vtep, &vtep_list_vxlan, next) {
		if (vtep->port == port)
			break;
	}
	if (vtep) {
		/* Device exists, just increment the reference counter. */
		vtep->refcnt++;
		assert(vtep->ifindex);
		return vtep;
	}
	/* No decapsulation device exists, try to create the new one. */
	vtep = flow_tcf_vtep_create(tcf, port, error);
	if (vtep)
		LIST_INSERT_HEAD(&vtep_list_vxlan, vtep, next);
	return vtep;
}

/**
 * Acquire target interface index for VXLAN tunneling encapsulation.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] ifouter
 *   Network interface index to attach VXLAN encap device to.
 * @param[in] dev_flow
 *   Flow tcf object with tunnel structure pointer set.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   Interface descriptor pointer on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct tcf_vtep*
flow_tcf_encap_vtep_acquire(struct mlx5_flow_tcf_context *tcf,
			    unsigned int ifouter,
			    struct mlx5_flow *dev_flow,
			    struct rte_flow_error *error)
{
	static uint16_t port;
	struct tcf_vtep *vtep;
	struct tcf_irule *iface;
	int ret;

	assert(ifouter);
	/* Look whether the VTEP for specified port is created. */
	port = rte_be_to_cpu_16(dev_flow->tcf.vxlan_encap->udp.dst);
	LIST_FOREACH(vtep, &vtep_list_vxlan, next) {
		if (vtep->port == port)
			break;
	}
	if (vtep) {
		/* VTEP already exists, just increment the reference. */
		vtep->refcnt++;
	} else {
		/* Not found, we should create the new VTEP. */
		vtep = flow_tcf_vtep_create(tcf, port, error);
		if (!vtep)
			return NULL;
		LIST_INSERT_HEAD(&vtep_list_vxlan, vtep, next);
	}
	assert(vtep->ifindex);
	iface = flow_tcf_encap_irule_acquire(tcf, ifouter, error);
	if (!iface) {
		if (--vtep->refcnt == 0)
			flow_tcf_vtep_delete(tcf, vtep);
		return NULL;
	}
	dev_flow->tcf.vxlan_encap->iface = iface;
	/* Create local ipaddr with peer to specify the outer IPs. */
	ret = flow_tcf_encap_local(tcf, iface, dev_flow, true, error);
	if (!ret) {
		/* Create neigh rule to specify outer destination MAC. */
		ret = flow_tcf_encap_neigh(tcf, iface, dev_flow, true, error);
		if (ret)
			flow_tcf_encap_local(tcf, iface,
					     dev_flow, false, error);
	}
	if (ret) {
		dev_flow->tcf.vxlan_encap->iface = NULL;
		flow_tcf_encap_irule_release(iface);
		if (--vtep->refcnt == 0)
			flow_tcf_vtep_delete(tcf, vtep);
		return NULL;
	}
	return vtep;
}

/**
 * Acquires target interface index for tunneling of any type.
 * Creates the new VTEP if needed.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] ifouter
 *   Network interface index to create VXLAN encap rules on.
 * @param[in] dev_flow
 *   Flow tcf object with tunnel structure pointer set.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   Interface descriptor pointer on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct tcf_vtep*
flow_tcf_vtep_acquire(struct mlx5_flow_tcf_context *tcf,
		      unsigned int ifouter,
		      struct mlx5_flow *dev_flow,
		      struct rte_flow_error *error)
{
	struct tcf_vtep *vtep = NULL;

	assert(dev_flow->tcf.tunnel);
	pthread_mutex_lock(&vtep_list_mutex);
	switch (dev_flow->tcf.tunnel->type) {
	case FLOW_TCF_TUNACT_VXLAN_ENCAP:
		vtep = flow_tcf_encap_vtep_acquire(tcf, ifouter,
						  dev_flow, error);
		break;
	case FLOW_TCF_TUNACT_VXLAN_DECAP:
		vtep = flow_tcf_decap_vtep_acquire(tcf, dev_flow, error);
		break;
	default:
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "unsupported tunnel type");
		break;
	}
	pthread_mutex_unlock(&vtep_list_mutex);
	return vtep;
}

/**
 * Release tunneling interface by ifindex. Decrements reference
 * counter and actually removes the device if counter is zero.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] vtep
 *   VTEP device descriptor structure.
 * @param[in] dev_flow
 *   Flow tcf object with tunnel structure pointer set.
 */
static void
flow_tcf_vtep_release(struct mlx5_flow_tcf_context *tcf,
		      struct tcf_vtep *vtep,
		      struct mlx5_flow *dev_flow)
{
	assert(dev_flow->tcf.tunnel);
	pthread_mutex_lock(&vtep_list_mutex);
	switch (dev_flow->tcf.tunnel->type) {
	case FLOW_TCF_TUNACT_VXLAN_DECAP:
		break;
	case FLOW_TCF_TUNACT_VXLAN_ENCAP: {
		struct tcf_irule *iface;

		/* Remove the encap ancillary rules first. */
		iface = dev_flow->tcf.vxlan_encap->iface;
		assert(iface);
		flow_tcf_encap_neigh(tcf, iface, dev_flow, false, NULL);
		flow_tcf_encap_local(tcf, iface, dev_flow, false, NULL);
		flow_tcf_encap_irule_release(iface);
		dev_flow->tcf.vxlan_encap->iface = NULL;
		break;
	}
	default:
		assert(false);
		DRV_LOG(WARNING, "Unsupported tunnel type");
		break;
	}
	assert(vtep->refcnt);
	if (--vtep->refcnt == 0) {
		LIST_REMOVE(vtep, next);
		flow_tcf_vtep_delete(tcf, vtep);
	}
	pthread_mutex_unlock(&vtep_list_mutex);
}

struct tcf_nlcb_query {
	uint32_t handle;
	uint32_t tc_flags;
	uint32_t flags_valid:1;
};

/**
 * Collect queried rule attributes. This is callback routine called by
 * libmnl mnl_cb_run() in loop for every message in received packet.
 * Current implementation collects the flower flags only.
 *
 * @param[in] nlh
 *   Pointer to reply header.
 * @param[in, out] arg
 *   Context pointer for this callback.
 *
 * @return
 *   A positive, nonzero value on success (required by libmnl
 *   to continue messages processing).
 */
static int
flow_tcf_collect_query_cb(const struct nlmsghdr *nlh, void *arg)
{
	struct tcf_nlcb_query *query = arg;
	struct tcmsg *tcm = mnl_nlmsg_get_payload(nlh);
	struct nlattr *na, *na_opt;
	bool flower = false;

	if (nlh->nlmsg_type != RTM_NEWTFILTER ||
	    tcm->tcm_handle != query->handle)
		return 1;
	mnl_attr_for_each(na, nlh, sizeof(*tcm)) {
		switch (mnl_attr_get_type(na)) {
		case TCA_KIND:
			if (strcmp(mnl_attr_get_payload(na), "flower")) {
				/* Not flower filter, drop entire message. */
				return 1;
			}
			flower = true;
			break;
		case TCA_OPTIONS:
			if (!flower) {
				/* Not flower options, drop entire message. */
				return 1;
			}
			/* Check nested flower options. */
			mnl_attr_for_each_nested(na_opt, na) {
				switch (mnl_attr_get_type(na_opt)) {
				case TCA_FLOWER_FLAGS:
					query->flags_valid = 1;
					query->tc_flags =
						mnl_attr_get_u32(na_opt);
					break;
				}
			}
			break;
		}
	}
	return 1;
}

/**
 * Query a TC flower rule flags via netlink.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] dev_flow
 *   Pointer to the flow.
 * @param[out] pflags
 *   pointer to the data retrieved by the query.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
flow_tcf_query_flags(struct mlx5_flow_tcf_context *tcf,
		     struct mlx5_flow *dev_flow,
		     uint32_t *pflags)
{
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;
	struct tcf_nlcb_query query = {
		.handle = dev_flow->tcf.tcm->tcm_handle,
	};

	nlh = mnl_nlmsg_put_header(tcf->buf);
	nlh->nlmsg_type = RTM_GETTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	memcpy(tcm, dev_flow->tcf.tcm, sizeof(*tcm));
	/*
	 * Ignore Netlink error for filter query operations.
	 * The reply length is sent by kernel as errno.
	 * Just check we got the flags option.
	 */
	flow_tcf_nl_ack(tcf, nlh, flow_tcf_collect_query_cb, &query);
	if (!query.flags_valid) {
		*pflags = 0;
		return -ENOENT;
	}
	*pflags = query.tc_flags;
	return 0;
}

/**
 * Query and check the in_hw set for specified rule.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_flow_tcf_context_create().
 * @param[in] dev_flow
 *   Pointer to the flow to check.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
flow_tcf_check_inhw(struct mlx5_flow_tcf_context *tcf,
		    struct mlx5_flow *dev_flow)
{
	uint32_t flags;
	int ret;

	ret = flow_tcf_query_flags(tcf, dev_flow, &flags);
	if (ret)
		return ret;
	return  (flags & TCA_CLS_FLAGS_IN_HW) ? 0 : -ENOENT;
}

/**
 * Remove flow from E-Switch by sending Netlink message.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 */
static void
flow_tcf_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_tcf_context *ctx = priv->tcf_context;
	struct mlx5_flow *dev_flow;
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;

	if (!flow)
		return;
	dev_flow = LIST_FIRST(&flow->dev_flows);
	if (!dev_flow)
		return;
	/* E-Switch flow can't be expanded. */
	assert(!LIST_NEXT(dev_flow, next));
	if (dev_flow->tcf.applied) {
		nlh = dev_flow->tcf.nlh;
		nlh->nlmsg_type = RTM_DELTFILTER;
		nlh->nlmsg_flags = NLM_F_REQUEST;
		flow_tcf_nl_ack(ctx, nlh, NULL, NULL);
		if (dev_flow->tcf.tunnel) {
			assert(dev_flow->tcf.tunnel->vtep);
			flow_tcf_vtep_release(ctx,
				dev_flow->tcf.tunnel->vtep,
				dev_flow);
			dev_flow->tcf.tunnel->vtep = NULL;
		}
		/* Cleanup the rule handle value. */
		tcm = mnl_nlmsg_get_payload(nlh);
		tcm->tcm_handle = 0;
		dev_flow->tcf.applied = 0;
	}
}

/**
 * Fetch the applied rule handle. This is callback routine called by
 * libmnl mnl_cb_run() in loop for every message in received packet.
 * When the NLM_F_ECHO flag is specified the kernel sends the created
 * rule descriptor back to the application and we can retrieve the
 * actual rule handle from updated descriptor.
 *
 * @param[in] nlh
 *   Pointer to reply header.
 * @param[in, out] arg
 *   Context pointer for this callback.
 *
 * @return
 *   A positive, nonzero value on success (required by libmnl
 *   to continue messages processing).
 */
static int
flow_tcf_collect_apply_cb(const struct nlmsghdr *nlh, void *arg)
{
	struct nlmsghdr *nlhrq = arg;
	struct tcmsg *tcmrq = mnl_nlmsg_get_payload(nlhrq);
	struct tcmsg *tcm = mnl_nlmsg_get_payload(nlh);
	struct nlattr *na;

	if (nlh->nlmsg_type != RTM_NEWTFILTER ||
	    nlh->nlmsg_seq != nlhrq->nlmsg_seq)
		return 1;
	mnl_attr_for_each(na, nlh, sizeof(*tcm)) {
		switch (mnl_attr_get_type(na)) {
		case TCA_KIND:
			if (strcmp(mnl_attr_get_payload(na), "flower")) {
				/* Not flower filter, drop entire message. */
				return 1;
			}
			tcmrq->tcm_handle = tcm->tcm_handle;
			return 1;
		}
	}
	return 1;
}
/**
 * Apply flow to E-Switch by sending Netlink message.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_tcf_context *ctx = priv->tcf_context;
	struct mlx5_flow *dev_flow;
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;
	uint64_t start = 0;
	uint64_t twait = 0;
	int ret;

	dev_flow = LIST_FIRST(&flow->dev_flows);
	/* E-Switch flow can't be expanded. */
	assert(!LIST_NEXT(dev_flow, next));
	if (dev_flow->tcf.applied)
		return 0;
	nlh = dev_flow->tcf.nlh;
	nlh->nlmsg_type = RTM_NEWTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE |
			   NLM_F_EXCL | NLM_F_ECHO;
	tcm = mnl_nlmsg_get_payload(nlh);
	/* Allow kernel to assign handle on its own. */
	tcm->tcm_handle = 0;
	if (dev_flow->tcf.tunnel) {
		/*
		 * Replace the interface index, target for
		 * encapsulation, source for decapsulation.
		 */
		assert(!dev_flow->tcf.tunnel->vtep);
		assert(dev_flow->tcf.tunnel->ifindex_ptr);
		/* Acquire actual VTEP device when rule is being applied. */
		dev_flow->tcf.tunnel->vtep =
			flow_tcf_vtep_acquire(ctx,
					dev_flow->tcf.tunnel->ifindex_org,
					dev_flow, error);
		if (!dev_flow->tcf.tunnel->vtep)
			return -rte_errno;
		DRV_LOG(INFO, "Replace ifindex: %d->%d",
				dev_flow->tcf.tunnel->vtep->ifindex,
				dev_flow->tcf.tunnel->ifindex_org);
		*dev_flow->tcf.tunnel->ifindex_ptr =
			dev_flow->tcf.tunnel->vtep->ifindex;
		if (dev_flow->tcf.tunnel->vtep->waitreg) {
			/* Clear wait flag for VXLAN port registration. */
			dev_flow->tcf.tunnel->vtep->waitreg = 0;
			twait = rte_get_timer_hz();
			assert(twait > MS_PER_S);
			twait = twait * MLX5_VXLAN_WAIT_PORT_REG_MS;
			twait = twait / MS_PER_S;
			start = rte_get_timer_cycles();
		}
	}
	/*
	 * Kernel creates the VXLAN devices and registers UDP ports to
	 * be hardware offloaded within the NIC kernel drivers. The
	 * registration process is being performed into context of
	 * working kernel thread and the race conditions might happen.
	 * The VXLAN device is created and success is returned to
	 * calling application, but the UDP port registration process
	 * is not completed yet. The next applied rule may be rejected
	 * by the driver with ENOSUP code. We are going to wait a bit,
	 * allowing registration process to be completed. The waiting
	 * is performed once after device been created.
	 */
	do {
		struct timespec onems;

		ret = flow_tcf_nl_ack(ctx, nlh,
				      flow_tcf_collect_apply_cb, nlh);
		if (!ret || ret != -ENOTSUP || !twait)
			break;
		/* Wait one millisecond and try again till timeout. */
		onems.tv_sec = 0;
		onems.tv_nsec = NS_PER_S / MS_PER_S;
		nanosleep(&onems, 0);
		if ((rte_get_timer_cycles() - start) > twait) {
			/* Timeout elapsed, try once more and exit. */
			twait = 0;
		}
	} while (true);
	if (!ret) {
		if (!tcm->tcm_handle) {
			flow_tcf_remove(dev, flow);
			return rte_flow_error_set
				(error, ENOENT,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				 "netlink: rule zero handle returned");
		}
		dev_flow->tcf.applied = 1;
		if (*dev_flow->tcf.ptc_flags & TCA_CLS_FLAGS_SKIP_SW)
			return 0;
		/*
		 * Rule was applied without skip_sw flag set.
		 * We should check whether the rule was acctually
		 * accepted by hardware (have look at in_hw flag).
		 */
		if (flow_tcf_check_inhw(ctx, dev_flow)) {
			flow_tcf_remove(dev, flow);
			return rte_flow_error_set
				(error, ENOENT,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				 "netlink: rule has no in_hw flag set");
		}
		return 0;
	}
	if (dev_flow->tcf.tunnel) {
		/* Rollback the VTEP configuration if rule apply failed. */
		assert(dev_flow->tcf.tunnel->vtep);
		flow_tcf_vtep_release(ctx, dev_flow->tcf.tunnel->vtep,
				      dev_flow);
		dev_flow->tcf.tunnel->vtep = NULL;
	}
	return rte_flow_error_set(error, rte_errno,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "netlink: failed to create TC flow rule");
}

/**
 * Remove flow from E-Switch and release resources of the device flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 */
static void
flow_tcf_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow *dev_flow;

	if (!flow)
		return;
	flow_tcf_remove(dev, flow);
	if (flow->counter) {
		if (--flow->counter->ref_cnt == 0) {
			rte_free(flow->counter);
			flow->counter = NULL;
		}
	}
	dev_flow = LIST_FIRST(&flow->dev_flows);
	if (!dev_flow)
		return;
	/* E-Switch flow can't be expanded. */
	assert(!LIST_NEXT(dev_flow, next));
	LIST_REMOVE(dev_flow, next);
	rte_free(dev_flow);
}

/**
 * Helper routine for figuring the space size required for a parse buffer.
 *
 * @param array
 *   array of values to use.
 * @param idx
 *   Current location in array.
 * @param value
 *   Value to compare with.
 *
 * @return
 *   The maximum between the given value and the array value on index.
 */
static uint16_t
flow_tcf_arr_val_max(uint16_t array[], int idx, uint16_t value)
{
	return idx < 0 ? (value) : RTE_MAX((array)[idx], value);
}

/**
 * Parse rtnetlink message attributes filling the attribute table with the info
 * retrieved.
 *
 * @param tb
 *   Attribute table to be filled.
 * @param[out] max
 *   Maxinum entry in the attribute table.
 * @param rte
 *   The attributes section in the message to be parsed.
 * @param len
 *   The length of the attributes section in the message.
 */
static void
flow_tcf_nl_parse_rtattr(struct rtattr *tb[], int max,
			 struct rtattr *rta, int len)
{
	unsigned short type;
	memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
	while (RTA_OK(rta, len)) {
		type = rta->rta_type;
		if (type <= max && !tb[type])
			tb[type] = rta;
		rta = RTA_NEXT(rta, len);
	}
}

/**
 * Extract flow counters from flower action.
 *
 * @param rta
 *   flower action stats properties in the Netlink message received.
 * @param rta_type
 *   The backward sequence of rta_types, as written in the attribute table,
 *   we need to traverse in order to get to the requested object.
 * @param idx
 *   Current location in rta_type table.
 * @param[out] data
 *   data holding the count statistics of the rte_flow retrieved from
 *   the message.
 *
 * @return
 *   0 if data was found and retrieved, -1 otherwise.
 */
static int
flow_tcf_nl_action_stats_parse_and_get(struct rtattr *rta,
				       uint16_t rta_type[], int idx,
				       struct gnet_stats_basic *data)
{
	int tca_stats_max = flow_tcf_arr_val_max(rta_type, idx,
						 TCA_STATS_BASIC);
	struct rtattr *tbs[tca_stats_max + 1];

	if (rta == NULL || idx < 0)
		return -1;
	flow_tcf_nl_parse_rtattr(tbs, tca_stats_max,
				 RTA_DATA(rta), RTA_PAYLOAD(rta));
	switch (rta_type[idx]) {
	case TCA_STATS_BASIC:
		if (tbs[TCA_STATS_BASIC]) {
			memcpy(data, RTA_DATA(tbs[TCA_STATS_BASIC]),
			       RTE_MIN(RTA_PAYLOAD(tbs[TCA_STATS_BASIC]),
			       sizeof(*data)));
			return 0;
		}
		break;
	default:
		break;
	}
	return -1;
}

/**
 * Parse flower single action retrieving the requested action attribute,
 * if found.
 *
 * @param arg
 *   flower action properties in the Netlink message received.
 * @param rta_type
 *   The backward sequence of rta_types, as written in the attribute table,
 *   we need to traverse in order to get to the requested object.
 * @param idx
 *   Current location in rta_type table.
 * @param[out] data
 *   Count statistics retrieved from the message query.
 *
 * @return
 *   0 if data was found and retrieved, -1 otherwise.
 */
static int
flow_tcf_nl_parse_one_action_and_get(struct rtattr *arg,
				     uint16_t rta_type[], int idx, void *data)
{
	int tca_act_max = flow_tcf_arr_val_max(rta_type, idx, TCA_ACT_STATS);
	struct rtattr *tb[tca_act_max + 1];

	if (arg == NULL || idx < 0)
		return -1;
	flow_tcf_nl_parse_rtattr(tb, tca_act_max,
				 RTA_DATA(arg), RTA_PAYLOAD(arg));
	if (tb[TCA_ACT_KIND] == NULL)
		return -1;
	switch (rta_type[idx]) {
	case TCA_ACT_STATS:
		if (tb[TCA_ACT_STATS])
			return flow_tcf_nl_action_stats_parse_and_get
					(tb[TCA_ACT_STATS],
					 rta_type, --idx,
					 (struct gnet_stats_basic *)data);
		break;
	default:
		break;
	}
	return -1;
}

/**
 * Parse flower action section in the message retrieving the requested
 * attribute from the first action that provides it.
 *
 * @param opt
 *   flower section in the Netlink message received.
 * @param rta_type
 *   The backward sequence of rta_types, as written in the attribute table,
 *   we need to traverse in order to get to the requested object.
 * @param idx
 *   Current location in rta_type table.
 * @param[out] data
 *   data retrieved from the message query.
 *
 * @return
 *   0 if data was found and retrieved, -1 otherwise.
 */
static int
flow_tcf_nl_action_parse_and_get(struct rtattr *arg,
				 uint16_t rta_type[], int idx, void *data)
{
	struct rtattr *tb[TCA_ACT_MAX_PRIO + 1];
	int i;

	if (arg == NULL || idx < 0)
		return -1;
	flow_tcf_nl_parse_rtattr(tb, TCA_ACT_MAX_PRIO,
				 RTA_DATA(arg), RTA_PAYLOAD(arg));
	switch (rta_type[idx]) {
	/*
	 * flow counters are stored in the actions defined by the flow
	 * and not in the flow itself, therefore we need to traverse the
	 * flower chain of actions in search for them.
	 *
	 * Note that the index is not decremented here.
	 */
	case TCA_ACT_STATS:
		for (i = 0; i <= TCA_ACT_MAX_PRIO; i++) {
			if (tb[i] &&
			!flow_tcf_nl_parse_one_action_and_get(tb[i],
							      rta_type,
							      idx, data))
				return 0;
		}
		break;
	default:
		break;
	}
	return -1;
}

/**
 * Parse flower classifier options in the message, retrieving the requested
 * attribute if found.
 *
 * @param opt
 *   flower section in the Netlink message received.
 * @param rta_type
 *   The backward sequence of rta_types, as written in the attribute table,
 *   we need to traverse in order to get to the requested object.
 * @param idx
 *   Current location in rta_type table.
 * @param[out] data
 *   data retrieved from the message query.
 *
 * @return
 *   0 if data was found and retrieved, -1 otherwise.
 */
static int
flow_tcf_nl_opts_parse_and_get(struct rtattr *opt,
			       uint16_t rta_type[], int idx, void *data)
{
	int tca_flower_max = flow_tcf_arr_val_max(rta_type, idx,
						  TCA_FLOWER_ACT);
	struct rtattr *tb[tca_flower_max + 1];

	if (!opt || idx < 0)
		return -1;
	flow_tcf_nl_parse_rtattr(tb, tca_flower_max,
				 RTA_DATA(opt), RTA_PAYLOAD(opt));
	switch (rta_type[idx]) {
	case TCA_FLOWER_ACT:
		if (tb[TCA_FLOWER_ACT])
			return flow_tcf_nl_action_parse_and_get
							(tb[TCA_FLOWER_ACT],
							 rta_type, --idx, data);
		break;
	default:
		break;
	}
	return -1;
}

/**
 * Parse Netlink reply on filter query, retrieving the flow counters.
 *
 * @param nlh
 *   Message received from Netlink.
 * @param rta_type
 *   The backward sequence of rta_types, as written in the attribute table,
 *   we need to traverse in order to get to the requested object.
 * @param idx
 *   Current location in rta_type table.
 * @param[out] data
 *   data retrieved from the message query.
 *
 * @return
 *   0 if data was found and retrieved, -1 otherwise.
 */
static int
flow_tcf_nl_filter_parse_and_get(struct nlmsghdr *cnlh,
				 uint16_t rta_type[], int idx, void *data)
{
	struct nlmsghdr *nlh = cnlh;
	struct tcmsg *t = NLMSG_DATA(nlh);
	int len = nlh->nlmsg_len;
	int tca_max = flow_tcf_arr_val_max(rta_type, idx, TCA_OPTIONS);
	struct rtattr *tb[tca_max + 1];

	if (idx < 0)
		return -1;
	if (nlh->nlmsg_type != RTM_NEWTFILTER &&
	    nlh->nlmsg_type != RTM_GETTFILTER &&
	    nlh->nlmsg_type != RTM_DELTFILTER)
		return -1;
	len -= NLMSG_LENGTH(sizeof(*t));
	if (len < 0)
		return -1;
	flow_tcf_nl_parse_rtattr(tb, tca_max, TCA_RTA(t), len);
	/* Not a TC flower flow - bail out */
	if (!tb[TCA_KIND] ||
	    strcmp(RTA_DATA(tb[TCA_KIND]), "flower"))
		return -1;
	switch (rta_type[idx]) {
	case TCA_OPTIONS:
		if (tb[TCA_OPTIONS])
			return flow_tcf_nl_opts_parse_and_get(tb[TCA_OPTIONS],
							      rta_type,
							      --idx, data);
		break;
	default:
		break;
	}
	return -1;
}

/**
 * A callback to parse Netlink reply on TC flower query.
 *
 * @param nlh
 *   Message received from Netlink.
 * @param[out] data
 *   Pointer to data area to be filled by the parsing routine.
 *   assumed to be a pointer to struct flow_tcf_stats_basic.
 *
 * @return
 *   MNL_CB_OK value.
 */
static int
flow_tcf_nl_message_get_stats_basic(const struct nlmsghdr *nlh, void *data)
{
	/*
	 * The backward sequence of rta_types to pass in order to get
	 *  to the counters.
	 */
	uint16_t rta_type[] = { TCA_STATS_BASIC, TCA_ACT_STATS,
				TCA_FLOWER_ACT, TCA_OPTIONS };
	struct flow_tcf_stats_basic *sb_data = data;
	union {
		const struct nlmsghdr *c;
		struct nlmsghdr *nc;
	} tnlh = { .c = nlh };

	if (!flow_tcf_nl_filter_parse_and_get(tnlh.nc, rta_type,
					      RTE_DIM(rta_type) - 1,
					      (void *)&sb_data->counters))
		sb_data->valid = true;
	return MNL_CB_OK;
}

/**
 * Query a TC flower rule for its statistics via netlink.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Pointer to the sub flow.
 * @param[out] data
 *   data retrieved by the query.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_query_count(struct rte_eth_dev *dev,
			  struct rte_flow *flow,
			  void *data,
			  struct rte_flow_error *error)
{
	struct flow_tcf_stats_basic sb_data;
	struct rte_flow_query_count *qc = data;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_tcf_context *ctx = priv->tcf_context;
	struct mnl_socket *nl = ctx->nl;
	struct mlx5_flow *dev_flow;
	struct nlmsghdr *nlh;
	uint32_t seq = priv->tcf_context->seq++;
	ssize_t ret;
	assert(qc);

	memset(&sb_data, 0, sizeof(sb_data));
	dev_flow = LIST_FIRST(&flow->dev_flows);
	/* E-Switch flow can't be expanded. */
	assert(!LIST_NEXT(dev_flow, next));
	if (!dev_flow->flow->counter)
		goto notsup_exit;
	nlh = dev_flow->tcf.nlh;
	nlh->nlmsg_type = RTM_GETTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ECHO;
	nlh->nlmsg_seq = seq;
	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) == -1)
		goto error_exit;
	do {
		ret = mnl_socket_recvfrom(nl, ctx->buf, ctx->buf_size);
		if (ret <= 0)
			break;
		ret = mnl_cb_run(ctx->buf, ret, seq,
				 mnl_socket_get_portid(nl),
				 flow_tcf_nl_message_get_stats_basic,
				 (void *)&sb_data);
	} while (ret > 0);
	/* Return the delta from last reset. */
	if (sb_data.valid) {
		/* Return the delta from last reset. */
		qc->hits_set = 1;
		qc->bytes_set = 1;
		qc->hits = sb_data.counters.packets - flow->counter->hits;
		qc->bytes = sb_data.counters.bytes - flow->counter->bytes;
		if (qc->reset) {
			flow->counter->hits = sb_data.counters.packets;
			flow->counter->bytes = sb_data.counters.bytes;
		}
		return 0;
	}
	return rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "flow does not have counter");
error_exit:
	return rte_flow_error_set
			(error, errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			 NULL, "netlink: failed to read flow rule counters");
notsup_exit:
	return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			 NULL, "counters are not available.");
}

/**
 * Query a flow.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
static int
flow_tcf_query(struct rte_eth_dev *dev,
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
			ret = flow_tcf_query_count(dev, flow, data, error);
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

const struct mlx5_flow_driver_ops mlx5_flow_tcf_drv_ops = {
	.validate = flow_tcf_validate,
	.prepare = flow_tcf_prepare,
	.translate = flow_tcf_translate,
	.apply = flow_tcf_apply,
	.remove = flow_tcf_remove,
	.destroy = flow_tcf_destroy,
	.query = flow_tcf_query,
};

/**
 * Create and configure a libmnl socket for Netlink flow rules.
 *
 * @return
 *   A valid libmnl socket object pointer on success, NULL otherwise and
 *   rte_errno is set.
 */
static struct mnl_socket *
flow_tcf_mnl_socket_create(void)
{
	struct mnl_socket *nl = mnl_socket_open(NETLINK_ROUTE);

	if (nl) {
		mnl_socket_setsockopt(nl, NETLINK_CAP_ACK, &(int){ 1 },
				      sizeof(int));
		if (!mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID))
			return nl;
	}
	rte_errno = errno;
	if (nl)
		mnl_socket_close(nl);
	return NULL;
}

/**
 * Destroy a libmnl socket.
 *
 * @param nl
 *   Libmnl socket of the @p NETLINK_ROUTE kind.
 */
static void
flow_tcf_mnl_socket_destroy(struct mnl_socket *nl)
{
	if (nl)
		mnl_socket_close(nl);
}

/**
 * Initialize ingress qdisc of a given network interface.
 *
 * @param ctx
 *   Pointer to tc-flower context to use.
 * @param ifindex
 *   Index of network interface to initialize.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_tcf_init(struct mlx5_flow_tcf_context *ctx,
		   unsigned int ifindex, struct rte_flow_error *error)
{
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;
	alignas(struct nlmsghdr)
	uint8_t buf[mnl_nlmsg_size(sizeof(*tcm)) +
		    SZ_NLATTR_STRZ_OF("ingress") +
		    MNL_BUF_EXTRA_SPACE];

	/* Destroy existing ingress qdisc and everything attached to it. */
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_DELQDISC;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = TC_H_MAKE(TC_H_INGRESS, 0);
	tcm->tcm_parent = TC_H_INGRESS;
	assert(sizeof(buf) >= nlh->nlmsg_len);
	/* Ignore errors when qdisc is already absent. */
	if (flow_tcf_nl_ack(ctx, nlh, NULL, NULL) &&
	    rte_errno != EINVAL && rte_errno != ENOENT)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "netlink: failed to remove ingress"
					  " qdisc");
	/* Create fresh ingress qdisc. */
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = TC_H_MAKE(TC_H_INGRESS, 0);
	tcm->tcm_parent = TC_H_INGRESS;
	mnl_attr_put_strz_check(nlh, sizeof(buf), TCA_KIND, "ingress");
	assert(sizeof(buf) >= nlh->nlmsg_len);
	if (flow_tcf_nl_ack(ctx, nlh, NULL, NULL))
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "netlink: failed to create ingress"
					  " qdisc");
	return 0;
}

/**
 * Create libmnl context for Netlink flow rules.
 *
 * @return
 *   A valid libmnl socket object pointer on success, NULL otherwise and
 *   rte_errno is set.
 */
struct mlx5_flow_tcf_context *
mlx5_flow_tcf_context_create(void)
{
	struct mlx5_flow_tcf_context *ctx = rte_zmalloc(__func__,
							sizeof(*ctx),
							sizeof(uint32_t));
	if (!ctx)
		goto error;
	ctx->nl = flow_tcf_mnl_socket_create();
	if (!ctx->nl)
		goto error;
	ctx->buf_size = MNL_SOCKET_BUFFER_SIZE;
	ctx->buf = rte_zmalloc(__func__,
			       ctx->buf_size, sizeof(uint32_t));
	if (!ctx->buf)
		goto error;
	ctx->seq = random();
	return ctx;
error:
	mlx5_flow_tcf_context_destroy(ctx);
	return NULL;
}

/**
 * Destroy a libmnl context.
 *
 * @param ctx
 *   Libmnl socket of the @p NETLINK_ROUTE kind.
 */
void
mlx5_flow_tcf_context_destroy(struct mlx5_flow_tcf_context *ctx)
{
	if (!ctx)
		return;
	flow_tcf_mnl_socket_destroy(ctx->nl);
	rte_free(ctx->buf);
	rte_free(ctx);
}
