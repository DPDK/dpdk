/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#ifndef RTE_FLOW_H_
#define RTE_FLOW_H_

/**
 * @file
 * RTE generic flow API
 *
 * This interface provides the ability to program packet matching and
 * associated actions in hardware through flow rules.
 */

#include <stddef.h>
#include <stdint.h>

#include <rte_compat.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_arp.h>
#include <rte_icmp.h>
#include <rte_ip.h>
#include <rte_sctp.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_vxlan.h>
#include <rte_esp.h>
#include <rte_higig.h>
#include <rte_ecpri.h>
#include <rte_bitops.h>
#include <rte_mbuf_dyn.h>
#include <rte_meter.h>
#include <rte_gtp.h>
#include <rte_l2tpv2.h>
#include <rte_ppp.h>
#include <rte_gre.h>
#include <rte_macsec.h>
#include <rte_ib.h>

#include "rte_ethdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_FLOW_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, rte_eth_dev_logtype, "" __VA_ARGS__)

/**
 * Flow rule attributes.
 *
 * Priorities are set on a per rule based within groups.
 *
 * Lower values denote higher priority, the highest priority for a flow rule
 * is 0, so that a flow that matches for than one rule, the rule with the
 * lowest priority value will always be matched.
 *
 * Although optional, applications are encouraged to group similar rules as
 * much as possible to fully take advantage of hardware capabilities
 * (e.g. optimized matching) and work around limitations (e.g. a single
 * pattern type possibly allowed in a given group). Applications should be
 * aware that groups are not linked by default, and that they must be
 * explicitly linked by the application using the JUMP action.
 *
 * Priority levels are arbitrary and up to the application, they
 * do not need to be contiguous nor start from 0, however the maximum number
 * varies between devices and may be affected by existing flow rules.
 *
 * If a packet is matched by several rules of a given group for a given
 * priority level, the outcome is undefined. It can take any path, may be
 * duplicated or even cause unrecoverable errors.
 *
 * Note that support for more than a single group and priority level is not
 * guaranteed.
 *
 * At vNIC / ethdev level, flow rules can apply to inbound and / or outbound
 * traffic (ingress / egress), with respect to the vNIC / ethdev in question.
 * At embedded switch level, flow rules apply to all traffic seen by it
 * unless fitting meta items are used to set concrete traffic source(s).
 *
 * Several pattern items and actions are valid and can be used in both
 * directions. Those valid for only one direction are described as such.
 *
 * At least one direction must be specified.
 *
 * Specifying both directions at once for a given rule is not recommended
 * but may be valid in a few cases.
 */
struct rte_flow_attr {
	/**
	 * A group is a superset of multiple rules.
	 * The default group is 0 and is processed for all packets.
	 * Rules in other groups are processed only if the group is chained
	 * by a jump action from a previously matched rule.
	 * It means the group hierarchy is made by the flow rules,
	 * and the group 0 is the hierarchy root.
	 * Note there is no automatic dead loop protection.
	 * @see rte_flow_action_jump
	 */
	uint32_t group;
	uint32_t priority; /**< Rule priority level within group. */
	/**
	 * The rule in question applies to ingress traffic (non-"transfer").
	 */
	uint32_t ingress:1;
	/**
	 * The rule in question applies to egress traffic (non-"transfer").
	 */
	uint32_t egress:1;
	/**
	 * Instead of simply matching the properties of traffic as it would
	 * appear on a given DPDK port ID, enabling this attribute transfers
	 * a flow rule to the lowest possible level of any device endpoints
	 * found in the pattern.
	 *
	 * When supported, this effectively enables an application to
	 * re-route traffic not necessarily intended for it (e.g. coming
	 * from or addressed to different physical ports, VFs or
	 * applications) at the device level.
	 *
	 * The application should match traffic originating from precise
	 * locations. See items PORT_REPRESENTOR and REPRESENTED_PORT.
	 *
	 * Managing "transfer" flows requires that the user communicate them
	 * through a suitable port. @see rte_flow_pick_transfer_proxy().
	 */
	uint32_t transfer:1;
	uint32_t reserved:29; /**< Reserved, must be zero. */
};

struct rte_flow_group_attr {
	uint32_t ingress:1;
	uint32_t egress:1;
	uint32_t transfer:1;
};

/**
 * Matching pattern item types.
 *
 * Pattern items fall in two categories:
 *
 * - Matching protocol headers and packet data, usually associated with a
 *   specification structure. These must be stacked in the same order as the
 *   protocol layers to match inside packets, starting from the lowest.
 *
 * - Matching meta-data or affecting pattern processing, often without a
 *   specification structure. Since they do not match packet contents, their
 *   position in the list is usually not relevant.
 *
 * See the description of individual types for more information. Those
 * marked with [META] fall into the second category.
 */
enum rte_flow_item_type {
	/**
	 * [META]
	 *
	 * End marker for item lists. Prevents further processing of items,
	 * thereby ending the pattern.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_END,

	/**
	 * [META]
	 *
	 * Used as a placeholder for convenience. It is ignored and simply
	 * discarded by PMDs.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_VOID,

	/**
	 * [META]
	 *
	 * Inverted matching, i.e. process packets that do not match the
	 * pattern.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_INVERT,

	/**
	 * Matches any protocol in place of the current layer, a single ANY
	 * may also stand for several protocol layers.
	 *
	 * See struct rte_flow_item_any.
	 */
	RTE_FLOW_ITEM_TYPE_ANY,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT
	 *
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress) a
	 * given DPDK port ID.
	 *
	 * See struct rte_flow_item_port_id.
	 */
	RTE_FLOW_ITEM_TYPE_PORT_ID,

	/**
	 * Matches a byte string of a given length at a given offset.
	 *
	 * See struct rte_flow_item_raw.
	 */
	RTE_FLOW_ITEM_TYPE_RAW,

	/**
	 * Matches an Ethernet header.
	 *
	 * See struct rte_flow_item_eth.
	 */
	RTE_FLOW_ITEM_TYPE_ETH,

	/**
	 * Matches an 802.1Q/ad VLAN tag.
	 *
	 * See struct rte_flow_item_vlan.
	 */
	RTE_FLOW_ITEM_TYPE_VLAN,

	/**
	 * Matches an IPv4 header.
	 *
	 * See struct rte_flow_item_ipv4.
	 */
	RTE_FLOW_ITEM_TYPE_IPV4,

	/**
	 * Matches an IPv6 header.
	 *
	 * See struct rte_flow_item_ipv6.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6,

	/**
	 * Matches an ICMP header.
	 *
	 * See struct rte_flow_item_icmp.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP,

	/**
	 * Matches a UDP header.
	 *
	 * See struct rte_flow_item_udp.
	 */
	RTE_FLOW_ITEM_TYPE_UDP,

	/**
	 * Matches a TCP header.
	 *
	 * See struct rte_flow_item_tcp.
	 */
	RTE_FLOW_ITEM_TYPE_TCP,

	/**
	 * Matches a SCTP header.
	 *
	 * See struct rte_flow_item_sctp.
	 */
	RTE_FLOW_ITEM_TYPE_SCTP,

	/**
	 * Matches a VXLAN header.
	 *
	 * See struct rte_flow_item_vxlan.
	 */
	RTE_FLOW_ITEM_TYPE_VXLAN,

	/**
	 * Matches a E_TAG header.
	 *
	 * See struct rte_flow_item_e_tag.
	 */
	RTE_FLOW_ITEM_TYPE_E_TAG,

	/**
	 * Matches a NVGRE header.
	 *
	 * See struct rte_flow_item_nvgre.
	 */
	RTE_FLOW_ITEM_TYPE_NVGRE,

	/**
	 * Matches a MPLS header.
	 *
	 * See struct rte_flow_item_mpls.
	 */
	RTE_FLOW_ITEM_TYPE_MPLS,

	/**
	 * Matches a GRE header.
	 *
	 * See struct rte_flow_item_gre.
	 */
	RTE_FLOW_ITEM_TYPE_GRE,

	/**
	 * [META]
	 *
	 * Fuzzy pattern match, expect faster than default.
	 *
	 * This is for device that support fuzzy matching option.
	 * Usually a fuzzy matching is fast but the cost is accuracy.
	 *
	 * See struct rte_flow_item_fuzzy.
	 */
	RTE_FLOW_ITEM_TYPE_FUZZY,

	/**
	 * Matches a GTP header.
	 *
	 * Configure flow for GTP packets.
	 *
	 * See struct rte_flow_item_gtp.
	 */
	RTE_FLOW_ITEM_TYPE_GTP,

	/**
	 * Matches a GTP header.
	 *
	 * Configure flow for GTP-C packets.
	 *
	 * See struct rte_flow_item_gtp.
	 */
	RTE_FLOW_ITEM_TYPE_GTPC,

	/**
	 * Matches a GTP header.
	 *
	 * Configure flow for GTP-U packets.
	 *
	 * See struct rte_flow_item_gtp.
	 */
	RTE_FLOW_ITEM_TYPE_GTPU,

	/**
	 * Matches a ESP header.
	 *
	 * See struct rte_flow_item_esp.
	 */
	RTE_FLOW_ITEM_TYPE_ESP,

	/**
	 * Matches a GENEVE header.
	 *
	 * See struct rte_flow_item_geneve.
	 */
	RTE_FLOW_ITEM_TYPE_GENEVE,

	/**
	 * Matches a VXLAN-GPE header.
	 *
	 * See struct rte_flow_item_vxlan_gpe.
	 */
	RTE_FLOW_ITEM_TYPE_VXLAN_GPE,

	/**
	 * Matches an ARP header for Ethernet/IPv4.
	 *
	 * See struct rte_flow_item_arp_eth_ipv4.
	 */
	RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4,

	/**
	 * Matches the presence of any IPv6 extension header.
	 *
	 * See struct rte_flow_item_ipv6_ext.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6_EXT,

	/**
	 * Matches any ICMPv6 header.
	 *
	 * See struct rte_flow_item_icmp6.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6,

	/**
	 * Matches an ICMPv6 neighbor discovery solicitation.
	 *
	 * See struct rte_flow_item_icmp6_nd_ns.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS,

	/**
	 * Matches an ICMPv6 neighbor discovery advertisement.
	 *
	 * See struct rte_flow_item_icmp6_nd_na.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA,

	/**
	 * Matches the presence of any ICMPv6 neighbor discovery option.
	 *
	 * See struct rte_flow_item_icmp6_nd_opt.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT,

	/**
	 * Matches an ICMPv6 neighbor discovery source Ethernet link-layer
	 * address option.
	 *
	 * See struct rte_flow_item_icmp6_nd_opt_sla_eth.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_SLA_ETH,

	/**
	 * Matches an ICMPv6 neighbor discovery target Ethernet link-layer
	 * address option.
	 *
	 * See struct rte_flow_item_icmp6_nd_opt_tla_eth.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_TLA_ETH,

	/**
	 * Matches specified mark field.
	 *
	 * See struct rte_flow_item_mark.
	 */
	RTE_FLOW_ITEM_TYPE_MARK,

	/**
	 * [META]
	 *
	 * Matches a metadata value.
	 *
	 * See struct rte_flow_item_meta.
	 */
	RTE_FLOW_ITEM_TYPE_META,

	/**
	 * Matches a GRE optional key field.
	 *
	 * The value should a big-endian 32bit integer.
	 *
	 * When this item present the K bit is implicitly matched as "1"
	 * in the default mask.
	 *
	 * @p spec/mask type:
	 * @code rte_be32_t * @endcode
	 */
	RTE_FLOW_ITEM_TYPE_GRE_KEY,

	/**
	 * Matches a GTP extension header: PDU session container.
	 *
	 * Configure flow for GTP packets with extension header type 0x85.
	 *
	 * See struct rte_flow_item_gtp_psc.
	 */
	RTE_FLOW_ITEM_TYPE_GTP_PSC,

	/**
	 * Matches a PPPoE header.
	 *
	 * Configure flow for PPPoE session packets.
	 *
	 * See struct rte_flow_item_pppoe.
	 */
	RTE_FLOW_ITEM_TYPE_PPPOES,

	/**
	 * Matches a PPPoE header.
	 *
	 * Configure flow for PPPoE discovery packets.
	 *
	 * See struct rte_flow_item_pppoe.
	 */
	RTE_FLOW_ITEM_TYPE_PPPOED,

	/**
	 * Matches a PPPoE optional proto_id field.
	 *
	 * It only applies to PPPoE session packets.
	 *
	 * See struct rte_flow_item_pppoe_proto_id.
	 */
	RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID,

	/**
	 * Matches Network service header (NSH).
	 * See struct rte_flow_item_nsh.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_NSH,

	/**
	 * Matches Internet Group Management Protocol (IGMP).
	 * See struct rte_flow_item_igmp.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_IGMP,

	/**
	 * Matches IP Authentication Header (AH).
	 * See struct rte_flow_item_ah.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_AH,

	/**
	 * Matches a HIGIG header.
	 * see struct rte_flow_item_higig2_hdr.
	 */
	RTE_FLOW_ITEM_TYPE_HIGIG2,

	/**
	 * [META]
	 *
	 * Matches a tag value.
	 *
	 * See struct rte_flow_item_tag.
	 */
	RTE_FLOW_ITEM_TYPE_TAG,

	/**
	 * Matches a L2TPv3 over IP header.
	 *
	 * Configure flow for L2TPv3 over IP packets.
	 *
	 * See struct rte_flow_item_l2tpv3oip.
	 */
	RTE_FLOW_ITEM_TYPE_L2TPV3OIP,

	/**
	 * Matches PFCP Header.
	 * See struct rte_flow_item_pfcp.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_PFCP,

	/**
	 * Matches eCPRI Header.
	 *
	 * Configure flow for eCPRI over ETH or UDP packets.
	 *
	 * See struct rte_flow_item_ecpri.
	 */
	RTE_FLOW_ITEM_TYPE_ECPRI,

	/**
	 * Matches the presence of IPv6 fragment extension header.
	 *
	 * See struct rte_flow_item_ipv6_frag_ext.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT,

	/**
	 * Matches Geneve Variable Length Option
	 *
	 * See struct rte_flow_item_geneve_opt
	 */
	RTE_FLOW_ITEM_TYPE_GENEVE_OPT,

	/**
	 * [META]
	 *
	 * Matches on packet integrity.
	 * For some devices application needs to enable integration checks in HW
	 * before using this item.
	 *
	 * @see struct rte_flow_item_integrity.
	 */
	RTE_FLOW_ITEM_TYPE_INTEGRITY,

	/**
	 * [META]
	 *
	 * Matches conntrack state.
	 *
	 * @see struct rte_flow_item_conntrack.
	 */
	RTE_FLOW_ITEM_TYPE_CONNTRACK,

	/**
	 * [META]
	 *
	 * Matches traffic entering the embedded switch from the given ethdev.
	 *
	 * @see struct rte_flow_item_ethdev
	 */
	RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR,

	/**
	 * [META]
	 *
	 * Matches traffic entering the embedded switch from
	 * the entity represented by the given ethdev.
	 *
	 * @see struct rte_flow_item_ethdev
	 */
	RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,

	/**
	 * Matches a configured set of fields at runtime calculated offsets
	 * over the generic network header with variable length and
	 * flexible pattern
	 *
	 * @see struct rte_flow_item_flex.
	 */
	RTE_FLOW_ITEM_TYPE_FLEX,

	/**
	 * Matches L2TPv2 Header.
	 *
	 * See struct rte_flow_item_l2tpv2.
	 */
	RTE_FLOW_ITEM_TYPE_L2TPV2,

	/**
	 * Matches PPP Header.
	 *
	 * See struct rte_flow_item_ppp.
	 */
	RTE_FLOW_ITEM_TYPE_PPP,

	/**
	 * Matches GRE optional fields.
	 *
	 * See struct rte_flow_item_gre_opt.
	 */
	RTE_FLOW_ITEM_TYPE_GRE_OPTION,

	/**
	 * Matches MACsec Ethernet Header.
	 *
	 * See struct rte_flow_item_macsec.
	 */
	RTE_FLOW_ITEM_TYPE_MACSEC,

	/**
	 * Matches Meter Color Marker.
	 *
	 * See struct rte_flow_item_meter_color.
	 */
	RTE_FLOW_ITEM_TYPE_METER_COLOR,

	/**
	 * Matches the presence of IPv6 routing extension header.
	 *
	 * @see struct rte_flow_item_ipv6_routing_ext.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT,

	/**
	 * Matches an ICMPv6 echo request.
	 *
	 * @see struct rte_flow_item_icmp6_echo.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST,

	/**
	 * Matches an ICMPv6 echo reply.
	 *
	 * @see struct rte_flow_item_icmp6_echo.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY,

	/**
	 * Match Quota state
	 *
	 * @see struct rte_flow_item_quota
	 */
	 RTE_FLOW_ITEM_TYPE_QUOTA,

	/**
	 * Matches on the aggregated port of the received packet.
	 * Used in case multiple ports are aggregated to the a DPDK port.
	 * First port is number 1.
	 *
	 * @see struct rte_flow_item_aggr_affinity.
	 */
	RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY,

	/**
	 * Match Tx queue number.
	 * This is valid only for egress rules.
	 *
	 * @see struct rte_flow_item_tx_queue
	 */
	 RTE_FLOW_ITEM_TYPE_TX_QUEUE,

	/**
	 * Matches an InfiniBand base transport header in RoCE packet.
	 *
	 * @see struct rte_flow_item_ib_bth.
	 */
	RTE_FLOW_ITEM_TYPE_IB_BTH,

	/**
	 * Matches the packet type as defined in rte_mbuf_ptype.
	 *
	 * See struct rte_flow_item_ptype.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_PTYPE,
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * QUOTA state.
 *
 * @see struct rte_flow_item_quota
 */
enum rte_flow_quota_state {
	RTE_FLOW_QUOTA_STATE_PASS, /**< PASS quota state */
	RTE_FLOW_QUOTA_STATE_BLOCK /**< BLOCK quota state */
};

/**
 * RTE_FLOW_ITEM_TYPE_QUOTA
 *
 * Matches QUOTA state
 */
struct rte_flow_item_quota {
	enum rte_flow_quota_state state;
};

/**
 * Default mask for RTE_FLOW_ITEM_TYPE_QUOTA
 */
#ifndef __cplusplus
static const struct rte_flow_item_quota rte_flow_item_quota_mask = {
	.state = (enum rte_flow_quota_state)0xff
};
#endif

/**
 *
 * RTE_FLOW_ITEM_TYPE_HIGIG2
 * Matches higig2 header
 */
struct rte_flow_item_higig2_hdr {
	struct rte_higig2_hdr hdr;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_HIGIG2. */
#ifndef __cplusplus
static const struct rte_flow_item_higig2_hdr rte_flow_item_higig2_hdr_mask = {
	.hdr = {
		.ppt1 = {
			.classification = RTE_BE16(UINT16_MAX),
			.vid = RTE_BE16(0xfff),
		},
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ANY
 *
 * Matches any protocol in place of the current layer, a single ANY may also
 * stand for several protocol layers.
 *
 * This is usually specified as the first pattern item when looking for a
 * protocol anywhere in a packet.
 *
 * A zeroed mask stands for any number of layers.
 */
struct rte_flow_item_any {
	uint32_t num; /**< Number of layers covered. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ANY. */
#ifndef __cplusplus
static const struct rte_flow_item_any rte_flow_item_any_mask = {
	.num = 0x00000000,
};
#endif

/**
 * @deprecated
 * @see RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR
 * @see RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT
 *
 * RTE_FLOW_ITEM_TYPE_PORT_ID
 *
 * Matches traffic originating from (ingress) or going to (egress) a given
 * DPDK port ID.
 *
 * Normally only supported if the port ID in question is known by the
 * underlying PMD and related to the device the flow rule is created
 * against.
 */
struct rte_flow_item_port_id {
	uint32_t id; /**< DPDK port ID. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_PORT_ID. */
#ifndef __cplusplus
static const struct rte_flow_item_port_id rte_flow_item_port_id_mask = {
	.id = 0xffffffff,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_RAW
 *
 * Matches a byte string of a given length at a given offset.
 *
 * Offset is either absolute (using the start of the packet) or relative to
 * the end of the previous matched item in the stack, in which case negative
 * values are allowed.
 *
 * If search is enabled, offset is used as the starting point. The search
 * area can be delimited by setting limit to a nonzero value, which is the
 * maximum number of bytes after offset where the pattern may start.
 *
 * Matching a zero-length pattern is allowed, doing so resets the relative
 * offset for subsequent items.
 *
 * This type does not support ranges (struct rte_flow_item.last).
 */
struct rte_flow_item_raw {
	uint32_t relative:1; /**< Look for pattern after the previous item. */
	uint32_t search:1; /**< Search pattern from offset (see also limit). */
	uint32_t reserved:30; /**< Reserved, must be set to zero. */
	int32_t offset; /**< Absolute or relative offset for pattern. */
	uint16_t limit; /**< Search area limit for start of pattern. */
	uint16_t length; /**< Pattern length. */
	const uint8_t *pattern; /**< Byte string to look for. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_RAW. */
#ifndef __cplusplus
static const struct rte_flow_item_raw rte_flow_item_raw_mask = {
	.relative = 1,
	.search = 1,
	.reserved = 0x3fffffff,
	.offset = 0xffffffff,
	.limit = 0xffff,
	.length = 0xffff,
	.pattern = NULL,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ETH
 *
 * Matches an Ethernet header.
 *
 * Inside @p hdr field, the sub-field @p ether_type stands either for EtherType
 * or TPID, depending on whether the item is followed by a VLAN item or not. If
 * two VLAN items follow, the sub-field refers to the outer one, which, in turn,
 * contains the inner TPID in the similar header field. The innermost VLAN item
 * contains a layer-3 EtherType. All of that follows the order seen on the wire.
 *
 * If the field in question contains a TPID value, only tagged packets with the
 * specified TPID will match the pattern. Alternatively, it's possible to match
 * any type of tagged packets by means of the field @p has_vlan rather than use
 * the EtherType/TPID field. Also, it's possible to leave the two fields unused.
 * If this is the case, both tagged and untagged packets will match the pattern.
 */
struct rte_flow_item_eth {
	union {
		struct {
			/*
			 * These fields are retained for compatibility.
			 * Please switch to the new header field below.
			 */
			struct rte_ether_addr dst; /**< Destination MAC. */
			struct rte_ether_addr src; /**< Source MAC. */
			rte_be16_t type; /**< EtherType or TPID. */
		};
		struct rte_ether_hdr hdr;
	};
	uint32_t has_vlan:1; /**< Packet header contains at least one VLAN. */
	uint32_t reserved:31; /**< Reserved, must be zero. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ETH. */
#ifndef __cplusplus
static const struct rte_flow_item_eth rte_flow_item_eth_mask = {
	.hdr.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.hdr.src_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.hdr.ether_type = RTE_BE16(0x0000),
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_VLAN
 *
 * Matches an 802.1Q/ad VLAN tag.
 *
 * The corresponding standard outer EtherType (TPID) values are
 * RTE_ETHER_TYPE_VLAN or RTE_ETHER_TYPE_QINQ. It can be overridden by
 * the preceding pattern item.
 * If a @p VLAN item is present in the pattern, then only tagged packets will
 * match the pattern.
 * The field @p has_more_vlan can be used to match any type of tagged packets,
 * instead of using the @p eth_proto field of @p hdr.
 * If the @p eth_proto of @p hdr and @p has_more_vlan fields are not specified,
 * then any tagged packets will match the pattern.
 */
struct rte_flow_item_vlan {
	union {
		struct {
			/*
			 * These fields are retained for compatibility.
			 * Please switch to the new header field below.
			 */
			rte_be16_t tci; /**< Tag control information. */
			rte_be16_t inner_type; /**< Inner EtherType or TPID. */
		};
		struct rte_vlan_hdr hdr;
	};
	/** Packet header contains at least one more VLAN, after this VLAN. */
	uint32_t has_more_vlan:1;
	uint32_t reserved:31; /**< Reserved, must be zero. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_VLAN. */
#ifndef __cplusplus
static const struct rte_flow_item_vlan rte_flow_item_vlan_mask = {
	.hdr.vlan_tci = RTE_BE16(0x0fff),
	.hdr.eth_proto = RTE_BE16(0x0000),
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_IPV4
 *
 * Matches an IPv4 header.
 *
 * Note: IPv4 options are handled by dedicated pattern items.
 */
struct rte_flow_item_ipv4 {
	struct rte_ipv4_hdr hdr; /**< IPv4 header definition. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_IPV4. */
#ifndef __cplusplus
static const struct rte_flow_item_ipv4 rte_flow_item_ipv4_mask = {
	.hdr = {
		.src_addr = RTE_BE32(0xffffffff),
		.dst_addr = RTE_BE32(0xffffffff),
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_IPV6.
 *
 * Matches an IPv6 header.
 *
 * Dedicated flags indicate if header contains specific extension headers.
 */
struct rte_flow_item_ipv6 {
	struct rte_ipv6_hdr hdr; /**< IPv6 header definition. */
	/** Header contains Hop-by-Hop Options extension header. */
	uint32_t has_hop_ext:1;
	/** Header contains Routing extension header. */
	uint32_t has_route_ext:1;
	/** Header contains Fragment extension header. */
	uint32_t has_frag_ext:1;
	/** Header contains Authentication extension header. */
	uint32_t has_auth_ext:1;
	/** Header contains Encapsulation Security Payload extension header. */
	uint32_t has_esp_ext:1;
	/** Header contains Destination Options extension header. */
	uint32_t has_dest_ext:1;
	/** Header contains Mobility extension header. */
	uint32_t has_mobil_ext:1;
	/** Header contains Host Identity Protocol extension header. */
	uint32_t has_hip_ext:1;
	/** Header contains Shim6 Protocol extension header. */
	uint32_t has_shim6_ext:1;
	/** Reserved for future extension headers, must be zero. */
	uint32_t reserved:23;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_IPV6. */
#ifndef __cplusplus
static const struct rte_flow_item_ipv6 rte_flow_item_ipv6_mask = {
	.hdr = {
		.src_addr =
			"\xff\xff\xff\xff\xff\xff\xff\xff"
			"\xff\xff\xff\xff\xff\xff\xff\xff",
		.dst_addr =
			"\xff\xff\xff\xff\xff\xff\xff\xff"
			"\xff\xff\xff\xff\xff\xff\xff\xff",
	},
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT.
 *
 * Matches an IPv6 routing extension header.
 */
struct rte_flow_item_ipv6_routing_ext {
	struct rte_ipv6_routing_ext hdr;
};

/**
 * RTE_FLOW_ITEM_TYPE_ICMP.
 *
 * Matches an ICMP header.
 */
struct rte_flow_item_icmp {
	struct rte_icmp_hdr hdr; /**< ICMP header definition. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ICMP. */
#ifndef __cplusplus
static const struct rte_flow_item_icmp rte_flow_item_icmp_mask = {
	.hdr = {
		.icmp_type = 0xff,
		.icmp_code = 0xff,
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_UDP.
 *
 * Matches a UDP header.
 */
struct rte_flow_item_udp {
	struct rte_udp_hdr hdr; /**< UDP header definition. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_UDP. */
#ifndef __cplusplus
static const struct rte_flow_item_udp rte_flow_item_udp_mask = {
	.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_TCP.
 *
 * Matches a TCP header.
 */
struct rte_flow_item_tcp {
	struct rte_tcp_hdr hdr; /**< TCP header definition. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_TCP. */
#ifndef __cplusplus
static const struct rte_flow_item_tcp rte_flow_item_tcp_mask = {
	.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_SCTP.
 *
 * Matches a SCTP header.
 */
struct rte_flow_item_sctp {
	struct rte_sctp_hdr hdr; /**< SCTP header definition. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_SCTP. */
#ifndef __cplusplus
static const struct rte_flow_item_sctp rte_flow_item_sctp_mask = {
	.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_VXLAN.
 *
 * Matches a VXLAN header (RFC 7348).
 */
struct rte_flow_item_vxlan {
	union {
		struct {
			/*
			 * These fields are retained for compatibility.
			 * Please switch to the new header field below.
			 */
			uint8_t flags; /**< Normally 0x08 (I flag). */
			uint8_t rsvd0[3]; /**< Reserved, normally 0x000000. */
			uint8_t vni[3]; /**< VXLAN identifier. */
			uint8_t rsvd1; /**< Reserved, normally 0x00. */
		};
		struct rte_vxlan_hdr hdr;
	};
};

/** Default mask for RTE_FLOW_ITEM_TYPE_VXLAN. */
#ifndef __cplusplus
static const struct rte_flow_item_vxlan rte_flow_item_vxlan_mask = {
	.hdr.vni = "\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_E_TAG.
 *
 * Matches a E-tag header.
 *
 * The corresponding standard outer EtherType (TPID) value is
 * RTE_ETHER_TYPE_ETAG. It can be overridden by the preceding pattern item.
 */
struct rte_flow_item_e_tag {
	/**
	 * E-Tag control information (E-TCI).
	 * E-PCP (3b), E-DEI (1b), ingress E-CID base (12b).
	 */
	rte_be16_t epcp_edei_in_ecid_b;
	/** Reserved (2b), GRP (2b), E-CID base (12b). */
	rte_be16_t rsvd_grp_ecid_b;
	uint8_t in_ecid_e; /**< Ingress E-CID ext. */
	uint8_t ecid_e; /**< E-CID ext. */
	rte_be16_t inner_type; /**< Inner EtherType or TPID. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_E_TAG. */
#ifndef __cplusplus
static const struct rte_flow_item_e_tag rte_flow_item_e_tag_mask = {
	.rsvd_grp_ecid_b = RTE_BE16(0x3fff),
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_NVGRE.
 *
 * Matches a NVGRE header.
 */
struct rte_flow_item_nvgre {
	/**
	 * Checksum (1b), undefined (1b), key bit (1b), sequence number (1b),
	 * reserved 0 (9b), version (3b).
	 *
	 * c_k_s_rsvd0_ver must have value 0x2000 according to RFC 7637.
	 */
	rte_be16_t c_k_s_rsvd0_ver;
	rte_be16_t protocol; /**< Protocol type (0x6558). */
	uint8_t tni[3]; /**< Virtual subnet ID. */
	uint8_t flow_id; /**< Flow ID. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_NVGRE. */
#ifndef __cplusplus
static const struct rte_flow_item_nvgre rte_flow_item_nvgre_mask = {
	.tni = "\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_MPLS.
 *
 * Matches a MPLS header.
 */
struct rte_flow_item_mpls {
	/**
	 * Label (20b), TC (3b), Bottom of Stack (1b).
	 */
	uint8_t label_tc_s[3];
	uint8_t ttl; /** Time-to-Live. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_MPLS. */
#ifndef __cplusplus
static const struct rte_flow_item_mpls rte_flow_item_mpls_mask = {
	.label_tc_s = "\xff\xff\xf0",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_GRE.
 *
 * Matches a GRE header.
 */
struct rte_flow_item_gre {
	/**
	 * Checksum (1b), reserved 0 (12b), version (3b).
	 * Refer to RFC 2784.
	 */
	rte_be16_t c_rsvd0_ver;
	rte_be16_t protocol; /**< Protocol type. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_GRE. */
#ifndef __cplusplus
static const struct rte_flow_item_gre rte_flow_item_gre_mask = {
	.protocol = RTE_BE16(0xffff),
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_GRE_OPTION.
 *
 * Matches GRE optional fields in header.
 */
struct rte_flow_item_gre_opt {
	struct rte_gre_hdr_opt_checksum_rsvd checksum_rsvd;
	struct rte_gre_hdr_opt_key key;
	struct rte_gre_hdr_opt_sequence sequence;
};

/**
 * RTE_FLOW_ITEM_TYPE_MACSEC.
 *
 * Matches MACsec header.
 */
struct rte_flow_item_macsec {
	struct rte_macsec_hdr macsec_hdr;
};

/**
 * RTE_FLOW_ITEM_TYPE_FUZZY
 *
 * Fuzzy pattern match, expect faster than default.
 *
 * This is for device that support fuzzy match option.
 * Usually a fuzzy match is fast but the cost is accuracy.
 * i.e. Signature Match only match pattern's hash value, but it is
 * possible two different patterns have the same hash value.
 *
 * Matching accuracy level can be configure by threshold.
 * Driver can divide the range of threshold and map to different
 * accuracy levels that device support.
 *
 * Threshold 0 means perfect match (no fuzziness), while threshold
 * 0xffffffff means fuzziest match.
 */
struct rte_flow_item_fuzzy {
	uint32_t thresh; /**< Accuracy threshold. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_FUZZY. */
#ifndef __cplusplus
static const struct rte_flow_item_fuzzy rte_flow_item_fuzzy_mask = {
	.thresh = 0xffffffff,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_GTP.
 *
 * Matches a GTPv1 header.
 */
struct rte_flow_item_gtp {
	union {
		struct {
			/*
			 * These are old fields kept for compatibility.
			 * Please prefer hdr field below.
			 */
			/**
			 * Version (3b), protocol type (1b), reserved (1b),
			 * Extension header flag (1b),
			 * Sequence number flag (1b),
			 * N-PDU number flag (1b).
			 */
			uint8_t v_pt_rsv_flags;
			uint8_t msg_type; /**< Message type. */
			rte_be16_t msg_len; /**< Message length. */
			rte_be32_t teid; /**< Tunnel endpoint identifier. */
		};
		struct rte_gtp_hdr hdr; /**< GTP header definition. */
	};
};

/** Default mask for RTE_FLOW_ITEM_TYPE_GTP. */
#ifndef __cplusplus
static const struct rte_flow_item_gtp rte_flow_item_gtp_mask = {
	.hdr.teid = RTE_BE32(UINT32_MAX),
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ESP
 *
 * Matches an ESP header.
 */
struct rte_flow_item_esp {
	struct rte_esp_hdr hdr; /**< ESP header definition. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ESP. */
#ifndef __cplusplus
static const struct rte_flow_item_esp rte_flow_item_esp_mask = {
	.hdr = {
		.spi = RTE_BE32(0xffffffff),
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_GENEVE.
 *
 * Matches a GENEVE header.
 */
struct rte_flow_item_geneve {
	/**
	 * Version (2b), length of the options fields (6b), OAM packet (1b),
	 * critical options present (1b), reserved 0 (6b).
	 */
	rte_be16_t ver_opt_len_o_c_rsvd0;
	rte_be16_t protocol; /**< Protocol type. */
	uint8_t vni[3]; /**< Virtual Network Identifier. */
	uint8_t rsvd1; /**< Reserved, normally 0x00. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_GENEVE. */
#ifndef __cplusplus
static const struct rte_flow_item_geneve rte_flow_item_geneve_mask = {
	.vni = "\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_VXLAN_GPE (draft-ietf-nvo3-vxlan-gpe-05).
 *
 * Matches a VXLAN-GPE header.
 */
struct rte_flow_item_vxlan_gpe {
	union {
		struct {
			/*
			 * These are old fields kept for compatibility.
			 * Please prefer hdr field below.
			 */
			uint8_t flags; /**< Normally 0x0c (I and P flags). */
			uint8_t rsvd0[2]; /**< Reserved, normally 0x0000. */
			uint8_t protocol; /**< Protocol type. */
			uint8_t vni[3]; /**< VXLAN identifier. */
			uint8_t rsvd1; /**< Reserved, normally 0x00. */
		};
		struct rte_vxlan_gpe_hdr hdr;
	};
};

/** Default mask for RTE_FLOW_ITEM_TYPE_VXLAN_GPE. */
#ifndef __cplusplus
static const struct rte_flow_item_vxlan_gpe rte_flow_item_vxlan_gpe_mask = {
	.hdr.vni = "\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4
 *
 * Matches an ARP header for Ethernet/IPv4.
 */
struct rte_flow_item_arp_eth_ipv4 {
	union {
		struct {
			/*
			 * These are old fields kept for compatibility.
			 * Please prefer hdr field below.
			 */
			rte_be16_t hrd; /**< Hardware type, normally 1. */
			rte_be16_t pro; /**< Protocol type, normally 0x0800. */
			uint8_t hln; /**< Hardware address length, normally 6. */
			uint8_t pln; /**< Protocol address length, normally 4. */
			rte_be16_t op; /**< Opcode (1 for request, 2 for reply). */
			struct rte_ether_addr sha; /**< Sender hardware address. */
			rte_be32_t spa; /**< Sender IPv4 address. */
			struct rte_ether_addr tha; /**< Target hardware address. */
			rte_be32_t tpa; /**< Target IPv4 address. */
		};
		struct rte_arp_hdr hdr; /**< ARP header definition. */
	};
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4. */
#ifndef __cplusplus
static const struct rte_flow_item_arp_eth_ipv4
rte_flow_item_arp_eth_ipv4_mask = {
	.hdr.arp_data.arp_sha.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.hdr.arp_data.arp_sip = RTE_BE32(UINT32_MAX),
	.hdr.arp_data.arp_tha.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.hdr.arp_data.arp_tip = RTE_BE32(UINT32_MAX),
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_IPV6_EXT
 *
 * Matches the presence of any IPv6 extension header.
 *
 * Normally preceded by any of:
 *
 * - RTE_FLOW_ITEM_TYPE_IPV6
 * - RTE_FLOW_ITEM_TYPE_IPV6_EXT
 */
struct rte_flow_item_ipv6_ext {
	uint8_t next_hdr; /**< Next header. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_IPV6_EXT. */
#ifndef __cplusplus
static const
struct rte_flow_item_ipv6_ext rte_flow_item_ipv6_ext_mask = {
	.next_hdr = 0xff,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT
 *
 * Matches the presence of IPv6 fragment extension header.
 *
 * Preceded by any of:
 *
 * - RTE_FLOW_ITEM_TYPE_IPV6
 * - RTE_FLOW_ITEM_TYPE_IPV6_EXT
 */
struct rte_flow_item_ipv6_frag_ext {
	struct rte_ipv6_fragment_ext hdr;
};

/**
 * RTE_FLOW_ITEM_TYPE_ICMP6
 *
 * Matches any ICMPv6 header.
 */
struct rte_flow_item_icmp6 {
	uint8_t type; /**< ICMPv6 type. */
	uint8_t code; /**< ICMPv6 code. */
	uint16_t checksum; /**< ICMPv6 checksum. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ICMP6. */
#ifndef __cplusplus
static const struct rte_flow_item_icmp6 rte_flow_item_icmp6_mask = {
	.type = 0xff,
	.code = 0xff,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST
 * RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY
 *
 * Matches an ICMPv6 echo request or reply.
 */
struct rte_flow_item_icmp6_echo {
	struct rte_icmp_echo_hdr hdr;
};

/**
 * RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS
 *
 * Matches an ICMPv6 neighbor discovery solicitation.
 */
struct rte_flow_item_icmp6_nd_ns {
	uint8_t type; /**< ICMPv6 type, normally 135. */
	uint8_t code; /**< ICMPv6 code, normally 0. */
	rte_be16_t checksum; /**< ICMPv6 checksum. */
	rte_be32_t reserved; /**< Reserved, normally 0. */
	uint8_t target_addr[16]; /**< Target address. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS. */
#ifndef __cplusplus
static const
struct rte_flow_item_icmp6_nd_ns rte_flow_item_icmp6_nd_ns_mask = {
	.target_addr =
		"\xff\xff\xff\xff\xff\xff\xff\xff"
		"\xff\xff\xff\xff\xff\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA
 *
 * Matches an ICMPv6 neighbor discovery advertisement.
 */
struct rte_flow_item_icmp6_nd_na {
	uint8_t type; /**< ICMPv6 type, normally 136. */
	uint8_t code; /**< ICMPv6 code, normally 0. */
	rte_be16_t checksum; /**< ICMPv6 checksum. */
	/**
	 * Route flag (1b), solicited flag (1b), override flag (1b),
	 * reserved (29b).
	 */
	rte_be32_t rso_reserved;
	uint8_t target_addr[16]; /**< Target address. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA. */
#ifndef __cplusplus
static const
struct rte_flow_item_icmp6_nd_na rte_flow_item_icmp6_nd_na_mask = {
	.target_addr =
		"\xff\xff\xff\xff\xff\xff\xff\xff"
		"\xff\xff\xff\xff\xff\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT
 *
 * Matches the presence of any ICMPv6 neighbor discovery option.
 *
 * Normally preceded by any of:
 *
 * - RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA
 * - RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS
 * - RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT
 */
struct rte_flow_item_icmp6_nd_opt {
	uint8_t type; /**< ND option type. */
	uint8_t length; /**< ND option length. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT. */
#ifndef __cplusplus
static const struct rte_flow_item_icmp6_nd_opt
rte_flow_item_icmp6_nd_opt_mask = {
	.type = 0xff,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_SLA_ETH
 *
 * Matches an ICMPv6 neighbor discovery source Ethernet link-layer address
 * option.
 *
 * Normally preceded by any of:
 *
 * - RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA
 * - RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT
 */
struct rte_flow_item_icmp6_nd_opt_sla_eth {
	uint8_t type; /**< ND option type, normally 1. */
	uint8_t length; /**< ND option length, normally 1. */
	struct rte_ether_addr sla; /**< Source Ethernet LLA. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_SLA_ETH. */
#ifndef __cplusplus
static const struct rte_flow_item_icmp6_nd_opt_sla_eth
rte_flow_item_icmp6_nd_opt_sla_eth_mask = {
	.sla.addr_bytes = "\xff\xff\xff\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_TLA_ETH
 *
 * Matches an ICMPv6 neighbor discovery target Ethernet link-layer address
 * option.
 *
 * Normally preceded by any of:
 *
 * - RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS
 * - RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT
 */
struct rte_flow_item_icmp6_nd_opt_tla_eth {
	uint8_t type; /**< ND option type, normally 2. */
	uint8_t length; /**< ND option length, normally 1. */
	struct rte_ether_addr tla; /**< Target Ethernet LLA. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_TLA_ETH. */
#ifndef __cplusplus
static const struct rte_flow_item_icmp6_nd_opt_tla_eth
rte_flow_item_icmp6_nd_opt_tla_eth_mask = {
	.tla.addr_bytes = "\xff\xff\xff\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_META
 *
 * Matches a specified metadata value. On egress, metadata can be set
 * either by mbuf dynamic metadata field with RTE_MBUF_DYNFLAG_TX_METADATA flag
 * or RTE_FLOW_ACTION_TYPE_SET_META. On ingress, RTE_FLOW_ACTION_TYPE_SET_META
 * sets metadata for a packet and the metadata will be reported via mbuf
 * metadata dynamic field with RTE_MBUF_DYNFLAG_RX_METADATA flag. The dynamic
 * mbuf field must be registered in advance by
 * rte_flow_dynf_metadata_register().
 */
struct rte_flow_item_meta {
	uint32_t data;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_META. */
#ifndef __cplusplus
static const struct rte_flow_item_meta rte_flow_item_meta_mask = {
	.data = UINT32_MAX,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_GTP_PSC.
 *
 * Matches a GTP PDU extension header with type 0x85.
 */
struct rte_flow_item_gtp_psc {
	struct rte_gtp_psc_generic_hdr hdr; /**< gtp psc generic hdr. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_GTP_PSC. */
#ifndef __cplusplus
static const struct rte_flow_item_gtp_psc
rte_flow_item_gtp_psc_mask = {
	.hdr.qfi = 0x3f,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_PPPOE.
 *
 * Matches a PPPoE header.
 */
struct rte_flow_item_pppoe {
	/**
	 * Version (4b), type (4b).
	 */
	uint8_t version_type;
	uint8_t code; /**< Message type. */
	rte_be16_t session_id; /**< Session identifier. */
	rte_be16_t length; /**< Payload length. */
};

/**
 * RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID.
 *
 * Matches a PPPoE optional proto_id field.
 *
 * It only applies to PPPoE session packets.
 *
 * Normally preceded by any of:
 *
 * - RTE_FLOW_ITEM_TYPE_PPPOE
 * - RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID
 */
struct rte_flow_item_pppoe_proto_id {
	rte_be16_t proto_id; /**< PPP protocol identifier. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID. */
#ifndef __cplusplus
static const struct rte_flow_item_pppoe_proto_id
rte_flow_item_pppoe_proto_id_mask = {
	.proto_id = RTE_BE16(0xffff),
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_TAG
 *
 * Matches a specified tag value at the specified index.
 */
struct rte_flow_item_tag {
	uint32_t data;
	uint8_t index;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_TAG. */
#ifndef __cplusplus
static const struct rte_flow_item_tag rte_flow_item_tag_mask = {
	.data = 0xffffffff,
	.index = 0xff,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_L2TPV3OIP.
 *
 * Matches a L2TPv3 over IP header.
 */
struct rte_flow_item_l2tpv3oip {
	rte_be32_t session_id; /**< Session ID. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_L2TPV3OIP. */
#ifndef __cplusplus
static const struct rte_flow_item_l2tpv3oip rte_flow_item_l2tpv3oip_mask = {
	.session_id = RTE_BE32(UINT32_MAX),
};
#endif


/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_MARK
 *
 * Matches an arbitrary integer value which was set using the ``MARK`` action
 * in a previously matched rule.
 *
 * This item can only be specified once as a match criteria as the ``MARK``
 * action can only be specified once in a flow action.
 *
 * This value is arbitrary and application-defined. Maximum allowed value
 * depends on the underlying implementation.
 *
 * Depending on the underlying implementation the MARK item may be supported on
 * the physical device, with virtual groups in the PMD or not at all.
 */
struct rte_flow_item_mark {
	uint32_t id; /**< Integer value to match against. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_MARK. */
#ifndef __cplusplus
static const struct rte_flow_item_mark rte_flow_item_mark_mask = {
	.id = 0xffffffff,
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_NSH
 *
 * Match network service header (NSH), RFC 8300
 */
struct rte_flow_item_nsh {
	uint32_t version:2;
	uint32_t oam_pkt:1;
	uint32_t reserved:1;
	uint32_t ttl:6;
	uint32_t length:6;
	uint32_t reserved1:4;
	uint32_t mdtype:4;
	uint32_t next_proto:8;
	uint32_t spi:24;
	uint32_t sindex:8;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_NSH. */
#ifndef __cplusplus
static const struct rte_flow_item_nsh rte_flow_item_nsh_mask = {
	.mdtype = 0xf,
	.next_proto = 0xff,
	.spi = 0xffffff,
	.sindex = 0xff,
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_IGMP
 *
 * Match Internet Group Management Protocol (IGMP), RFC 2236
 */
struct rte_flow_item_igmp {
	uint32_t type:8;
	uint32_t max_resp_time:8;
	uint32_t checksum:16;
	uint32_t group_addr;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_IGMP. */
#ifndef __cplusplus
static const struct rte_flow_item_igmp rte_flow_item_igmp_mask = {
	.group_addr = 0xffffffff,
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_AH
 *
 * Match IP Authentication Header (AH), RFC 4302
 */
struct rte_flow_item_ah {
	uint32_t next_hdr:8;
	uint32_t payload_len:8;
	uint32_t reserved:16;
	uint32_t spi;
	uint32_t seq_num;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_AH. */
#ifndef __cplusplus
static const struct rte_flow_item_ah rte_flow_item_ah_mask = {
	.spi = 0xffffffff,
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_PFCP
 *
 * Match PFCP Header
 */
struct rte_flow_item_pfcp {
	uint8_t s_field;
	uint8_t msg_type;
	rte_be16_t msg_len;
	rte_be64_t seid;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_PFCP. */
#ifndef __cplusplus
static const struct rte_flow_item_pfcp rte_flow_item_pfcp_mask = {
	.s_field = 0x01,
	.seid = RTE_BE64(UINT64_C(0xffffffffffffffff)),
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_ECPRI
 *
 * Match eCPRI Header
 */
struct rte_flow_item_ecpri {
	struct rte_ecpri_combined_msg_hdr hdr;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ECPRI. */
#ifndef __cplusplus
static const struct rte_flow_item_ecpri rte_flow_item_ecpri_mask = {
	.hdr = {
		.common = {
			.u32 = 0x0,
		},
	},
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_GENEVE_OPT
 *
 * Matches a GENEVE Variable Length Option
 */
struct rte_flow_item_geneve_opt {
	rte_be16_t option_class;
	uint8_t option_type;
	uint8_t option_len;
	uint32_t *data;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_GENEVE_OPT. */
#ifndef __cplusplus
static const struct rte_flow_item_geneve_opt
rte_flow_item_geneve_opt_mask = {
	.option_type = 0xff,
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_INTEGRITY
 *
 * Match on packet integrity check result.
 */
struct rte_flow_item_integrity {
	/** Tunnel encapsulation level the item should apply to.
	 * @see rte_flow_action_rss
	 */
	uint32_t level;
	union {
		__extension__
		struct {
			/** The packet is valid after passing all HW checks. */
			uint64_t packet_ok:1;
			/** L2 layer is valid after passing all HW checks. */
			uint64_t l2_ok:1;
			/** L3 layer is valid after passing all HW checks. */
			uint64_t l3_ok:1;
			/** L4 layer is valid after passing all HW checks. */
			uint64_t l4_ok:1;
			/** L2 layer CRC is valid. */
			uint64_t l2_crc_ok:1;
			/** IPv4 layer checksum is valid. */
			uint64_t ipv4_csum_ok:1;
			/** L4 layer checksum is valid. */
			uint64_t l4_csum_ok:1;
			/** L3 length is smaller than frame length. */
			uint64_t l3_len_ok:1;
			uint64_t reserved:56;
		};
		uint64_t value;
	};
};

#ifndef __cplusplus
static const struct rte_flow_item_integrity
rte_flow_item_integrity_mask = {
	.level = 0,
	.value = 0,
};
#endif

/**
 * The packet is valid after conntrack checking.
 */
#define RTE_FLOW_CONNTRACK_PKT_STATE_VALID RTE_BIT32(0)
/**
 * The state of the connection is changed.
 */
#define RTE_FLOW_CONNTRACK_PKT_STATE_CHANGED RTE_BIT32(1)
/**
 * Error is detected on this packet for this connection and
 * an invalid state is set.
 */
#define RTE_FLOW_CONNTRACK_PKT_STATE_INVALID RTE_BIT32(2)
/**
 * The HW connection tracking module is disabled.
 * It can be due to application command or an invalid state.
 */
#define RTE_FLOW_CONNTRACK_PKT_STATE_DISABLED RTE_BIT32(3)
/**
 * The packet contains some bad field(s) and cannot continue
 * with the conntrack module checking.
 */
#define RTE_FLOW_CONNTRACK_PKT_STATE_BAD RTE_BIT32(4)

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_CONNTRACK
 *
 * Matches the state of a packet after it passed the connection tracking
 * examination. The state is a bitmap of one RTE_FLOW_CONNTRACK_PKT_STATE*
 * or a reasonable combination of these bits.
 */
struct rte_flow_item_conntrack {
	uint32_t flags;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_CONNTRACK. */
#ifndef __cplusplus
static const struct rte_flow_item_conntrack rte_flow_item_conntrack_mask = {
	.flags = 0xffffffff,
};
#endif

/**
 * Provides an ethdev port ID for use with the following items:
 * RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR,
 * RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT.
 */
struct rte_flow_item_ethdev {
	uint16_t port_id; /**< ethdev port ID */
};

/** Default mask for items based on struct rte_flow_item_ethdev */
#ifndef __cplusplus
static const struct rte_flow_item_ethdev rte_flow_item_ethdev_mask = {
	.port_id = 0xffff,
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_L2TPV2
 *
 * Matches L2TPv2 Header
 */
struct rte_flow_item_l2tpv2 {
	struct rte_l2tpv2_combined_msg_hdr hdr;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_L2TPV2. */
#ifndef __cplusplus
static const struct rte_flow_item_l2tpv2 rte_flow_item_l2tpv2_mask = {
	/*
	 * flags and version bit mask
	 * 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
	 * T L x x S x O P x x x x V V V V
	 */
	.hdr = {
		.common = {
			.flags_version = RTE_BE16(0xcb0f),
		},
	},
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_PPP
 *
 * Matches PPP Header
 */
struct rte_flow_item_ppp {
	struct rte_ppp_hdr hdr;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_PPP. */
#ifndef __cplusplus
static const struct rte_flow_item_ppp rte_flow_item_ppp_mask = {
	.hdr = {
		.addr = 0xff,
		.ctrl = 0xff,
		.proto_id = RTE_BE16(0xffff),
	}
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_IB_BTH.
 *
 * Matches an InfiniBand base transport header in RoCE packet.
 */
struct rte_flow_item_ib_bth {
	struct rte_ib_bth hdr; /**< InfiniBand base transport header definition. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_IB_BTH. */
#ifndef __cplusplus
static const struct rte_flow_item_ib_bth rte_flow_item_ib_bth_mask = {
	.hdr = {
		.opcode = 0xff,
		.dst_qp = "\xff\xff\xff",
	},
};
#endif

/**
 * Matching pattern item definition.
 *
 * A pattern is formed by stacking items starting from the lowest protocol
 * layer to match. This stacking restriction does not apply to meta items
 * which can be placed anywhere in the stack without affecting the meaning
 * of the resulting pattern.
 *
 * Patterns are terminated by END items.
 *
 * The spec field should be a valid pointer to a structure of the related
 * item type. It may remain unspecified (NULL) in many cases to request
 * broad (nonspecific) matching. In such cases, last and mask must also be
 * set to NULL.
 *
 * Optionally, last can point to a structure of the same type to define an
 * inclusive range. This is mostly supported by integer and address fields,
 * may cause errors otherwise. Fields that do not support ranges must be set
 * to 0 or to the same value as the corresponding fields in spec.
 *
 * Only the fields defined to nonzero values in the default masks (see
 * rte_flow_item_{name}_mask constants) are considered relevant by
 * default. This can be overridden by providing a mask structure of the
 * same type with applicable bits set to one. It can also be used to
 * partially filter out specific fields (e.g. as an alternate mean to match
 * ranges of IP addresses).
 *
 * Mask is a simple bit-mask applied before interpreting the contents of
 * spec and last, which may yield unexpected results if not used
 * carefully. For example, if for an IPv4 address field, spec provides
 * 10.1.2.3, last provides 10.3.4.5 and mask provides 255.255.0.0, the
 * effective range becomes 10.1.0.0 to 10.3.255.255.
 */
struct rte_flow_item {
	enum rte_flow_item_type type; /**< Item type. */
	const void *spec; /**< Pointer to item specification structure. */
	const void *last; /**< Defines an inclusive range (spec to last). */
	const void *mask; /**< Bit-mask applied to spec and last. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_FLEX
 *
 * Matches a specified set of fields within the network protocol
 * header. Each field is presented as set of bits with specified width, and
 * bit offset from the header beginning.
 *
 * The pattern is concatenation of bit fields configured at item creation
 * by rte_flow_flex_item_create(). At configuration the fields are presented
 * by sample_data array.
 *
 * This type does not support ranges (struct rte_flow_item.last).
 */
struct rte_flow_item_flex {
	struct rte_flow_item_flex_handle *handle; /**< Opaque item handle. */
	uint32_t length; /**< Pattern length in bytes. */
	const uint8_t *pattern; /**< Combined bitfields pattern to match. */
};
/**
 * Field bit offset calculation mode.
 */
enum rte_flow_item_flex_field_mode {
	/**
	 * Dummy field, used for byte boundary alignment in pattern.
	 * Pattern mask and data are ignored in the match. All configuration
	 * parameters besides field size are ignored.
	 */
	FIELD_MODE_DUMMY = 0,
	/**
	 * Fixed offset field. The bit offset from header beginning
	 * is permanent and defined by field_base parameter.
	 */
	FIELD_MODE_FIXED,
	/**
	 * The field bit offset is extracted from other header field (indirect
	 * offset field). The resulting field offset to match is calculated as:
	 *
	 *    field_base + (*offset_base & offset_mask) << offset_shift
	 */
	FIELD_MODE_OFFSET,
	/**
	 * The field bit offset is extracted from other header field (indirect
	 * offset field), the latter is considered as bitmask containing some
	 * number of one bits, the resulting field offset to match is
	 * calculated as:
	 *
	 *    field_base + bitcount(*offset_base & offset_mask) << offset_shift
	 */
	FIELD_MODE_BITMASK,
};

/**
 * Flex item field tunnel mode
 */
enum rte_flow_item_flex_tunnel_mode {
	/**
	 * The protocol header can be present in the packet only once.
	 * No multiple flex item flow inclusions (for inner/outer) are allowed.
	 * No any relations with tunnel protocols are imposed. The drivers
	 * can optimize hardware resource usage to handle match on single flex
	 * item of specific type.
	 */
	FLEX_TUNNEL_MODE_SINGLE = 0,
	/**
	 * Flex item presents outer header only.
	 */
	FLEX_TUNNEL_MODE_OUTER,
	/**
	 * Flex item presents inner header only.
	 */
	FLEX_TUNNEL_MODE_INNER,
	/**
	 * Flex item presents either inner or outer header. The driver
	 * handles as many multiple inners as hardware supports.
	 */
	FLEX_TUNNEL_MODE_MULTI,
	/**
	 * Flex item presents tunnel protocol header.
	 */
	FLEX_TUNNEL_MODE_TUNNEL,
};

/**
 *
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 */
__extension__
struct rte_flow_item_flex_field {
	/** Defines how match field offset is calculated over the packet. */
	enum rte_flow_item_flex_field_mode field_mode;
	uint32_t field_size; /**< Field size in bits. */
	int32_t field_base; /**< Field offset in bits. */
	uint32_t offset_base; /**< Indirect offset field offset in bits. */
	uint32_t offset_mask; /**< Indirect offset field bit mask. */
	int32_t offset_shift; /**< Indirect offset multiply factor. */
	uint32_t field_id:16; /**< Device hint, for multiple items in flow. */
	uint32_t reserved:16; /**< Reserved field. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 */
struct rte_flow_item_flex_link {
	/**
	 * Preceding/following header. The item type must be always provided.
	 * For preceding one item must specify the header value/mask to match
	 * for the link be taken and start the flex item header parsing.
	 */
	struct rte_flow_item item;
	/**
	 * Next field value to match to continue with one of the configured
	 * next protocols.
	 */
	uint32_t next;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 */
struct rte_flow_item_flex_conf {
	/**
	 * Specifies the flex item and tunnel relations and tells the PMD
	 * whether flex item can be used for inner, outer or both headers,
	 * or whether flex item presents the tunnel protocol itself.
	 */
	enum rte_flow_item_flex_tunnel_mode tunnel;
	/**
	 * The next header offset, it presents the network header size covered
	 * by the flex item and can be obtained with all supported offset
	 * calculating methods (fixed, dedicated field, bitmask, etc).
	 */
	struct rte_flow_item_flex_field next_header;
	/**
	 * Specifies the next protocol field to match with link next protocol
	 * values and continue packet parsing with matching link.
	 */
	struct rte_flow_item_flex_field next_protocol;
	/**
	 * The fields will be sampled and presented for explicit match
	 * with pattern in the rte_flow_flex_item. There can be multiple
	 * fields descriptors, the number should be specified by nb_samples.
	 */
	struct rte_flow_item_flex_field *sample_data;
	/** Number of field descriptors in the sample_data array. */
	uint32_t nb_samples;
	/**
	 * Input link defines the flex item relation with preceding
	 * header. It specified the preceding item type and provides pattern
	 * to match. The flex item will continue parsing and will provide the
	 * data to flow match in case if there is the match with one of input
	 * links.
	 */
	struct rte_flow_item_flex_link *input_link;
	/** Number of link descriptors in the input link array. */
	uint32_t nb_inputs;
	/**
	 * Output link defines the next protocol field value to match and
	 * the following protocol header to continue packet parsing. Also
	 * defines the tunnel-related behaviour.
	 */
	struct rte_flow_item_flex_link *output_link;
	/** Number of link descriptors in the output link array. */
	uint32_t nb_outputs;
};

/**
 * RTE_FLOW_ITEM_TYPE_METER_COLOR.
 *
 * Matches Color Marker set by a Meter.
 */
struct rte_flow_item_meter_color {
	enum rte_color color; /**< Meter color marker. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_METER_COLOR. */
#ifndef __cplusplus
static const struct rte_flow_item_meter_color rte_flow_item_meter_color_mask = {
	.color = RTE_COLORS,
};
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY
 *
 * For multiple ports aggregated to a single DPDK port,
 * match the aggregated port receiving the packets.
 */
struct rte_flow_item_aggr_affinity {
	/**
	 * An aggregated port receiving the packets.
	 * Numbering starts from 1.
	 * Number of aggregated ports is reported by rte_eth_dev_count_aggr_ports().
	 */
	uint8_t affinity;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY. */
#ifndef __cplusplus
static const struct rte_flow_item_aggr_affinity
rte_flow_item_aggr_affinity_mask = {
	.affinity = 0xff,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_TX_QUEUE
 *
 * Tx queue number.
 *
 * @see struct rte_flow_item_tx_queue
 */
struct rte_flow_item_tx_queue {
	/** Tx queue number of packet being transmitted. */
	uint16_t tx_queue;
};

/** Default mask for RTE_FLOW_ITEM_TX_QUEUE. */
#ifndef __cplusplus
static const struct rte_flow_item_tx_queue rte_flow_item_tx_queue_mask = {
	.tx_queue = 0xffff,
};
#endif

/**
 *
 * RTE_FLOW_ITEM_TYPE_PTYPE
 *
 * Matches the packet type as defined in rte_mbuf_ptype.
 */
struct rte_flow_item_ptype {
	uint32_t packet_type; /**< L2/L3/L4 and tunnel information. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_PTYPE. */
#ifndef __cplusplus
static const struct rte_flow_item_ptype rte_flow_item_ptype_mask = {
	.packet_type = 0xffffffff,
};
#endif

/**
 * Action types.
 *
 * Each possible action is represented by a type.
 * An action can have an associated configuration object.
 * Several actions combined in a list can be assigned
 * to a flow rule and are performed in order.
 *
 * They fall in three categories:
 *
 * - Actions that modify the fate of matching traffic, for instance by
 *   dropping or assigning it a specific destination.
 *
 * - Actions that modify matching traffic contents or its properties. This
 *   includes adding/removing encapsulation, encryption, compression and
 *   marks.
 *
 * - Actions related to the flow rule itself, such as updating counters or
 *   making it non-terminating.
 *
 * Flow rules being terminating by default, not specifying any action of the
 * fate kind results in undefined behavior. This applies to both ingress and
 * egress.
 *
 * PASSTHRU, when supported, makes a flow rule non-terminating.
 */
enum rte_flow_action_type {
	/**
	 * End marker for action lists. Prevents further processing of
	 * actions, thereby ending the list.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_END,

	/**
	 * Used as a placeholder for convenience. It is ignored and simply
	 * discarded by PMDs.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_VOID,

	/**
	 * Leaves traffic up for additional processing by subsequent flow
	 * rules; makes a flow rule non-terminating.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_PASSTHRU,

	/**
	 * RTE_FLOW_ACTION_TYPE_JUMP
	 *
	 * Redirects packets to a group on the current device.
	 *
	 * See struct rte_flow_action_jump.
	 */
	RTE_FLOW_ACTION_TYPE_JUMP,

	/**
	 * Attaches an integer value to packets and sets RTE_MBUF_F_RX_FDIR and
	 * RTE_MBUF_F_RX_FDIR_ID mbuf flags.
	 *
	 * See struct rte_flow_action_mark.
	 *
	 * One should negotiate mark delivery from the NIC to the PMD.
	 * @see rte_eth_rx_metadata_negotiate()
	 * @see RTE_ETH_RX_METADATA_USER_MARK
	 */
	RTE_FLOW_ACTION_TYPE_MARK,

	/**
	 * Flags packets. Similar to MARK without a specific value; only
	 * sets the RTE_MBUF_F_RX_FDIR mbuf flag.
	 *
	 * No associated configuration structure.
	 *
	 * One should negotiate flag delivery from the NIC to the PMD.
	 * @see rte_eth_rx_metadata_negotiate()
	 * @see RTE_ETH_RX_METADATA_USER_FLAG
	 */
	RTE_FLOW_ACTION_TYPE_FLAG,

	/**
	 * Assigns packets to a given queue index.
	 *
	 * See struct rte_flow_action_queue.
	 */
	RTE_FLOW_ACTION_TYPE_QUEUE,

	/**
	 * Drops packets.
	 *
	 * PASSTHRU overrides this action if both are specified.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_DROP,

	/**
	 * Enables counters for this flow rule.
	 *
	 * These counters can be retrieved and reset through rte_flow_query() or
	 * rte_flow_action_handle_query() if the action provided via handle,
	 * see struct rte_flow_query_count.
	 *
	 * See struct rte_flow_action_count.
	 */
	RTE_FLOW_ACTION_TYPE_COUNT,

	/**
	 * Similar to QUEUE, except RSS is additionally performed on packets
	 * to spread them among several queues according to the provided
	 * parameters.
	 *
	 * See struct rte_flow_action_rss.
	 */
	RTE_FLOW_ACTION_TYPE_RSS,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
	 *
	 * Directs matching traffic to the physical function (PF) of the
	 * current device.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_PF,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
	 *
	 * Directs matching traffic to a given virtual function of the
	 * current device.
	 *
	 * See struct rte_flow_action_vf.
	 */
	RTE_FLOW_ACTION_TYPE_VF,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
	 *
	 * Directs matching traffic to a given DPDK port ID.
	 *
	 * See struct rte_flow_action_port_id.
	 */
	RTE_FLOW_ACTION_TYPE_PORT_ID,

	/**
	 * Traffic metering and policing (MTR).
	 *
	 * See struct rte_flow_action_meter.
	 * See file rte_mtr.h for MTR object configuration.
	 */
	RTE_FLOW_ACTION_TYPE_METER,

	/**
	 * Redirects packets to security engine of current device for security
	 * processing as specified by security session.
	 *
	 * See struct rte_flow_action_security.
	 */
	RTE_FLOW_ACTION_TYPE_SECURITY,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Implements OFPAT_DEC_NW_TTL ("decrement IP TTL") as defined by
	 * the OpenFlow Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_DEC_NW_TTL,

	/**
	 * Implements OFPAT_POP_VLAN ("pop the outer VLAN tag") as defined
	 * by the OpenFlow Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_POP_VLAN,

	/**
	 * Implements OFPAT_PUSH_VLAN ("push a new VLAN tag") as defined by
	 * the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_push_vlan.
	 */
	RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN,

	/**
	 * Implements OFPAT_SET_VLAN_VID ("set the 802.1q VLAN ID") as
	 * defined by the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_set_vlan_vid.
	 */
	RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID,

	/**
	 * Implements OFPAT_SET_LAN_PCP ("set the 802.1q priority") as
	 * defined by the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_set_vlan_pcp.
	 */
	RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP,

	/**
	 * Implements OFPAT_POP_MPLS ("pop the outer MPLS tag") as defined
	 * by the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_pop_mpls.
	 */
	RTE_FLOW_ACTION_TYPE_OF_POP_MPLS,

	/**
	 * Implements OFPAT_PUSH_MPLS ("push a new MPLS tag") as defined by
	 * the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_push_mpls.
	 */
	RTE_FLOW_ACTION_TYPE_OF_PUSH_MPLS,

	/**
	 * Encapsulate flow in VXLAN tunnel as defined in
	 * rte_flow_action_vxlan_encap action structure.
	 *
	 * See struct rte_flow_action_vxlan_encap.
	 */
	RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP,

	/**
	 * Decapsulate outer most VXLAN tunnel from matched flow.
	 *
	 * If flow pattern does not define a valid VXLAN tunnel (as specified by
	 * RFC7348) then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION
	 * error.
	 */
	RTE_FLOW_ACTION_TYPE_VXLAN_DECAP,

	/**
	 * Encapsulate flow in NVGRE tunnel defined in the
	 * rte_flow_action_nvgre_encap action structure.
	 *
	 * See struct rte_flow_action_nvgre_encap.
	 */
	RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP,

	/**
	 * Decapsulate outer most NVGRE tunnel from matched flow.
	 *
	 * If flow pattern does not define a valid NVGRE tunnel (as specified by
	 * RFC7637) then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION
	 * error.
	 */
	RTE_FLOW_ACTION_TYPE_NVGRE_DECAP,

	/**
	 * Add outer header whose template is provided in its data buffer
	 *
	 * See struct rte_flow_action_raw_encap.
	 */
	RTE_FLOW_ACTION_TYPE_RAW_ENCAP,

	/**
	 * Remove outer header whose template is provided in its data buffer.
	 *
	 * See struct rte_flow_action_raw_decap
	 */
	RTE_FLOW_ACTION_TYPE_RAW_DECAP,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv4 source address in the outermost IPv4 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv4.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv4 destination address in the outermost IPv4 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv4.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_DST,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv6 source address in the outermost IPv6 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv6.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv6 destination address in the outermost IPv6 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv6.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_DST,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify source port number in the outermost TCP/UDP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_TCP
	 * or RTE_FLOW_ITEM_TYPE_UDP, then the PMD should return a
	 * RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_tp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_TP_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify destination port number in the outermost TCP/UDP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_TCP
	 * or RTE_FLOW_ITEM_TYPE_UDP, then the PMD should return a
	 * RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_tp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_TP_DST,

	/**
	 * Swap the source and destination MAC addresses in the outermost
	 * Ethernet header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_MAC_SWAP,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Decrease TTL value directly
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_DEC_TTL,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set TTL value
	 *
	 * See struct rte_flow_action_set_ttl
	 */
	RTE_FLOW_ACTION_TYPE_SET_TTL,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set source MAC address from matched flow.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_mac.
	 */
	RTE_FLOW_ACTION_TYPE_SET_MAC_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set destination MAC address from matched flow.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_mac.
	 */
	RTE_FLOW_ACTION_TYPE_SET_MAC_DST,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Increase sequence number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to increase
	 * TCP sequence number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode
	 *
	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Decrease sequence number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to decrease
	 * TCP sequence number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode
	 *
	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Increase acknowledgment number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to increase
	 * TCP acknowledgment number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode

	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_INC_TCP_ACK,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Decrease acknowledgment number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to decrease
	 * TCP acknowledgment number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode
	 *
	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set Tag.
	 *
	 * Tag is for internal flow usage only and
	 * is not delivered to the application.
	 *
	 * See struct rte_flow_action_set_tag.
	 */
	RTE_FLOW_ACTION_TYPE_SET_TAG,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set metadata on ingress or egress path.
	 *
	 * See struct rte_flow_action_set_meta.
	 */
	RTE_FLOW_ACTION_TYPE_SET_META,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv4 DSCP in the outermost IP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_dscp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv6 DSCP in the outermost IP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_dscp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP,

	/**
	 * Report as aged flow if timeout passed without any matching on the
	 * flow.
	 *
	 * See struct rte_flow_action_age.
	 * See function rte_flow_get_q_aged_flows
	 * See function rte_flow_get_aged_flows
	 * see enum RTE_ETH_EVENT_FLOW_AGED
	 * See struct rte_flow_query_age
	 * See struct rte_flow_update_age
	 */
	RTE_FLOW_ACTION_TYPE_AGE,

	/**
	 * The matching packets will be duplicated with specified ratio and
	 * applied with own set of actions with a fate action.
	 *
	 * See struct rte_flow_action_sample.
	 */
	RTE_FLOW_ACTION_TYPE_SAMPLE,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_INDIRECT
	 *
	 * Describe action shared across multiple flow rules.
	 *
	 * Allow multiple rules reference the same action by handle (see
	 * struct rte_flow_shared_action).
	 */
	RTE_FLOW_ACTION_TYPE_SHARED,

	/**
	 * Modify a packet header field, tag, mark or metadata.
	 *
	 * Allow the modification of an arbitrary header field via
	 * set, add and sub operations or copying its content into
	 * tag, meta or mark for future processing.
	 *
	 * See struct rte_flow_action_modify_field.
	 */
	RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,

	/**
	 * An action handle is referenced in a rule through an indirect action.
	 *
	 * The same action handle may be used in multiple rules for the same
	 * or different ethdev ports.
	 */
	RTE_FLOW_ACTION_TYPE_INDIRECT,

	/**
	 * [META]
	 *
	 * Enable tracking a TCP connection state.
	 *
	 * @see struct rte_flow_action_conntrack.
	 */
	RTE_FLOW_ACTION_TYPE_CONNTRACK,

	/**
	 * Color the packet to reflect the meter color result.
	 * Set the meter color in the mbuf to the selected color.
	 *
	 * See struct rte_flow_action_meter_color.
	 */
	RTE_FLOW_ACTION_TYPE_METER_COLOR,

	/**
	 * At embedded switch level, sends matching traffic to the given ethdev.
	 *
	 * @see struct rte_flow_action_ethdev
	 */
	RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR,

	/**
	 * At embedded switch level, send matching traffic to
	 * the entity represented by the given ethdev.
	 *
	 * @see struct rte_flow_action_ethdev
	 */
	RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,

	/**
	 * Traffic metering and marking (MTR).
	 *
	 * @see struct rte_flow_action_meter_mark
	 * See file rte_mtr.h for MTR profile object configuration.
	 */
	RTE_FLOW_ACTION_TYPE_METER_MARK,

	/**
	 * Send packets to the kernel, without going to userspace at all.
	 * The packets will be received by the kernel driver sharing
	 * the same device as the DPDK port on which this action is configured.
	 * This action mostly suits bifurcated driver model.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL,

	/**
	 * Apply the quota verdict (PASS or BLOCK) to a flow.
	 *
	 * @see struct rte_flow_action_quota
	 * @see struct rte_flow_query_quota
	 * @see struct rte_flow_update_quota
	 */
	 RTE_FLOW_ACTION_TYPE_QUOTA,

	/**
	 * Skip congestion management configuration.
	 *
	 * Using rte_eth_cman_config_set(), the application
	 * can configure ethdev Rx queue's congestion mechanism.
	 * This flow action allows to skip the congestion configuration
	 * applied to the given ethdev Rx queue.
	 */
	RTE_FLOW_ACTION_TYPE_SKIP_CMAN,

	/**
	 * RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH
	 *
	 * Push IPv6 extension into IPv6 packet.
	 *
	 * @see struct rte_flow_action_ipv6_ext_push.
	 */
	RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH,

	/**
	 * RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE
	 *
	 * Remove IPv6 extension from IPv6 packet whose type
	 * is provided in its configuration buffer.
	 *
	 * @see struct rte_flow_action_ipv6_ext_remove.
	 */
	RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE,

	/**
	 * Action handle to reference flow actions list.
	 *
	 * @see struct rte_flow_action_indirect_list
	 */
	RTE_FLOW_ACTION_TYPE_INDIRECT_LIST,

	/**
	 * Program action. These actions are defined by the program currently
	 * loaded on the device. For example, these actions are applicable to
	 * devices that can be programmed through the P4 language.
	 *
	 * @see struct rte_flow_action_prog.
	 */
	RTE_FLOW_ACTION_TYPE_PROG,
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * QUOTA operational mode.
 *
 * @see struct rte_flow_action_quota
 */
enum rte_flow_quota_mode {
	RTE_FLOW_QUOTA_MODE_PACKET = 1, /**< Count packets. */
	RTE_FLOW_QUOTA_MODE_L2 = 2, /**< Count packet bytes starting from L2. */
	RTE_FLOW_QUOTA_MODE_L3 = 3, /**< Count packet bytes starting from L3. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create QUOTA action.
 *
 * @see RTE_FLOW_ACTION_TYPE_QUOTA
 */
struct rte_flow_action_quota {
	enum rte_flow_quota_mode mode; /**< Quota operational mode. */
	int64_t quota;                 /**< Quota value. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Query indirect QUOTA action.
 *
 * @see RTE_FLOW_ACTION_TYPE_QUOTA
 */
struct rte_flow_query_quota {
	int64_t quota; /**< Quota value. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Indirect QUOTA update operations.
 *
 * @see struct rte_flow_update_quota
 */
enum rte_flow_update_quota_op {
	RTE_FLOW_UPDATE_QUOTA_SET, /**< Set new quota value. */
	RTE_FLOW_UPDATE_QUOTA_ADD, /**< Increase quota value. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @see RTE_FLOW_ACTION_TYPE_QUOTA
 *
 * Update indirect QUOTA action.
 */
struct rte_flow_update_quota {
	enum rte_flow_update_quota_op op; /**< Update operation. */
	int64_t quota;                    /**< Quota value. */
};

/**
 * RTE_FLOW_ACTION_TYPE_MARK
 *
 * Attaches an integer value to packets and sets RTE_MBUF_F_RX_FDIR and
 * RTE_MBUF_F_RX_FDIR_ID mbuf flags.
 *
 * This value is arbitrary and application-defined. Maximum allowed value
 * depends on the underlying implementation. It is returned in the
 * hash.fdir.hi mbuf field.
 */
struct rte_flow_action_mark {
	uint32_t id; /**< Integer value to return with packets. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_JUMP
 *
 * Redirects packets to a group on the current device.
 *
 * In a hierarchy of groups, which can be used to represent physical or logical
 * flow tables on the device, this action allows the action to be a redirect to
 * a group on that device.
 */
struct rte_flow_action_jump {
	uint32_t group;
};

/**
 * RTE_FLOW_ACTION_TYPE_QUEUE
 *
 * Assign packets to a given queue index.
 */
struct rte_flow_action_queue {
	uint16_t index; /**< Queue index to use. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_AGE
 *
 * Report flow as aged-out if timeout passed without any matching
 * on the flow. RTE_ETH_EVENT_FLOW_AGED event is triggered when a
 * port detects new aged-out flows.
 *
 * The flow context and the flow handle will be reported by the either
 * rte_flow_get_aged_flows or rte_flow_get_q_aged_flows APIs.
 */
struct rte_flow_action_age {
	uint32_t timeout:24; /**< Time in seconds. */
	uint32_t reserved:8; /**< Reserved, must be zero. */
	/** The user flow context, NULL means the rte_flow pointer. */
	void *context;
};

/**
 * RTE_FLOW_ACTION_TYPE_AGE (query)
 *
 * Query structure to retrieve the aging status information of a
 * shared AGE action, or a flow rule using the AGE action.
 */
struct rte_flow_query_age {
	uint32_t reserved:6; /**< Reserved, must be zero. */
	uint32_t aged:1; /**< 1 if aging timeout expired, 0 otherwise. */
	/** sec_since_last_hit value is valid. */
	uint32_t sec_since_last_hit_valid:1;
	uint32_t sec_since_last_hit:24; /**< Seconds since last traffic hit. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_AGE
 *
 * Update indirect AGE action attributes:
 *  - Timeout can be updated including stop/start action:
 *     +-------------+-------------+------------------------------+
 *     | Old Timeout | New Timeout | Updating                     |
 *     +=============+=============+==============================+
 *     | 0           | positive    | Start aging with new value   |
 *     +-------------+-------------+------------------------------+
 *     | positive    | 0           | Stop aging			  |
 *     +-------------+-------------+------------------------------+
 *     | positive    | positive    | Change timeout to new value  |
 *     +-------------+-------------+------------------------------+
 *  - sec_since_last_hit can be reset.
 */
struct rte_flow_update_age {
	uint32_t reserved:6; /**< Reserved, must be zero. */
	uint32_t timeout_valid:1; /**< The timeout is valid for update. */
	uint32_t timeout:24; /**< Time in seconds. */
	/** Means that aging should assume packet passed the aging. */
	uint32_t touch:1;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_COUNT
 *
 * Adds a counter action to a matched flow.
 *
 * If more than one count action is specified in a single flow rule, then each
 * action must specify a unique ID.
 *
 * Counters can be retrieved and reset through ``rte_flow_query()``, see
 * ``struct rte_flow_query_count``.
 *
 * For ports within the same switch domain then the counter ID namespace extends
 * to all ports within that switch domain.
 */
struct rte_flow_action_count {
	uint32_t id; /**< Counter ID. */
};

/**
 * RTE_FLOW_ACTION_TYPE_COUNT (query)
 *
 * Query structure to retrieve and reset flow rule counters.
 */
struct rte_flow_query_count {
	uint32_t reset:1; /**< Reset counters after query [in]. */
	uint32_t hits_set:1; /**< hits field is set [out]. */
	uint32_t bytes_set:1; /**< bytes field is set [out]. */
	uint32_t reserved:29; /**< Reserved, must be zero [in, out]. */
	uint64_t hits; /**< Number of hits for this rule [out]. */
	uint64_t bytes; /**< Number of bytes through this rule [out]. */
};

/**
 * RTE_FLOW_ACTION_TYPE_RSS
 *
 * Similar to QUEUE, except RSS is additionally performed on packets to
 * spread them among several queues according to the provided parameters.
 *
 * Unlike global RSS settings used by other DPDK APIs, unsetting the
 * @p types field does not disable RSS in a flow rule. Doing so instead
 * requests safe unspecified "best-effort" settings from the underlying PMD,
 * which depending on the flow rule, may result in anything ranging from
 * empty (single queue) to all-inclusive RSS.
 *
 * Note: RSS hash result is stored in the hash.rss mbuf field which overlaps
 * hash.fdir.lo. Since the MARK action sets the hash.fdir.hi field only,
 * both can be requested simultaneously.
 */
struct rte_flow_action_rss {
	enum rte_eth_hash_function func; /**< RSS hash function to apply. */
	/**
	 * Packet encapsulation level RSS hash @p types apply to.
	 *
	 * - @p 0 requests the default behavior. Depending on the packet
	 *   type, it can mean outermost, innermost, anything in between or
	 *   even no RSS.
	 *
	 *   It basically stands for the innermost encapsulation level RSS
	 *   can be performed on according to PMD and device capabilities.
	 *
	 * - @p 1 requests RSS to be performed on the outermost packet
	 *   encapsulation level.
	 *
	 * - @p 2 and subsequent values request RSS to be performed on the
	 *   specified inner packet encapsulation level, from outermost to
	 *   innermost (lower to higher values).
	 *
	 * Values other than @p 0 are not necessarily supported.
	 *
	 * Requesting a specific RSS level on unrecognized traffic results
	 * in undefined behavior. For predictable results, it is recommended
	 * to make the flow rule pattern match packet headers up to the
	 * requested encapsulation level so that only matching traffic goes
	 * through.
	 */
	uint32_t level;
	uint64_t types; /**< Specific RSS hash types (see RTE_ETH_RSS_*). */
	uint32_t key_len; /**< Hash key length in bytes. */
	uint32_t queue_num; /**< Number of entries in @p queue. */
	const uint8_t *key; /**< Hash key. */
	const uint16_t *queue; /**< Queue indices to use. */
};

/**
 * @deprecated
 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
 *
 * RTE_FLOW_ACTION_TYPE_VF
 *
 * Directs matching traffic to a given virtual function of the current
 * device.
 *
 * Packets matched by a VF pattern item can be redirected to their original
 * VF ID instead of the specified one. This parameter may not be available
 * and is not guaranteed to work properly if the VF part is matched by a
 * prior flow rule or if packets are not addressed to a VF in the first
 * place.
 */
struct rte_flow_action_vf {
	uint32_t original:1; /**< Use original VF ID if possible. */
	uint32_t reserved:31; /**< Reserved, must be zero. */
	uint32_t id; /**< VF ID. */
};

/**
 * @deprecated
 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
 *
 * RTE_FLOW_ACTION_TYPE_PORT_ID
 *
 * Directs matching traffic to a given DPDK port ID.
 *
 * @see RTE_FLOW_ITEM_TYPE_PORT_ID
 */
struct rte_flow_action_port_id {
	uint32_t original:1; /**< Use original DPDK port ID if possible. */
	uint32_t reserved:31; /**< Reserved, must be zero. */
	uint32_t id; /**< DPDK port ID. */
};

/**
 * RTE_FLOW_ACTION_TYPE_METER
 *
 * Traffic metering and policing (MTR).
 *
 * Packets matched by items of this type can be either dropped or passed to the
 * next item with their color set by the MTR object.
 */
struct rte_flow_action_meter {
	uint32_t mtr_id; /**< MTR object ID created with rte_mtr_create(). */
};

/**
 * RTE_FLOW_ACTION_TYPE_SECURITY
 *
 * Perform the security action on flows matched by the pattern items
 * according to the configuration of the security session.
 *
 * This action modifies the payload of matched flows. For INLINE_CRYPTO, the
 * security protocol headers and IV are fully provided by the application as
 * specified in the flow pattern. The payload of matching packets is
 * encrypted on egress, and decrypted and authenticated on ingress.
 * For INLINE_PROTOCOL, the security protocol is fully offloaded to HW,
 * providing full encapsulation and decapsulation of packets in security
 * protocols. The flow pattern specifies both the outer security header fields
 * and the inner packet fields. The security session specified in the action
 * must match the pattern parameters.
 *
 * The security session specified in the action must be created on the same
 * port as the flow action that is being specified.
 *
 * The ingress/egress flow attribute should match that specified in the
 * security session if the security session supports the definition of the
 * direction.
 *
 * Multiple flows can be configured to use the same security session.
 *
 * The NULL value is allowed for security session. If security session is NULL,
 * then SPI field in ESP flow item and IP addresses in flow items 'IPv4' and
 * 'IPv6' will be allowed to be a range. The rule thus created can enable
 * security processing on multiple flows.
 */
struct rte_flow_action_security {
	void *security_session; /**< Pointer to security session structure. */
};

/**
 * RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN
 *
 * Implements OFPAT_PUSH_VLAN ("push a new VLAN tag") as defined by the
 * OpenFlow Switch Specification.
 */
struct rte_flow_action_of_push_vlan {
	rte_be16_t ethertype; /**< EtherType. */
};

/**
 * RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID
 *
 * Implements OFPAT_SET_VLAN_VID ("set the 802.1q VLAN ID") as defined by
 * the OpenFlow Switch Specification.
 */
struct rte_flow_action_of_set_vlan_vid {
	rte_be16_t vlan_vid; /**< VLAN ID. */
};

/**
 * RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP
 *
 * Implements OFPAT_SET_LAN_PCP ("set the 802.1q priority") as defined by
 * the OpenFlow Switch Specification.
 */
struct rte_flow_action_of_set_vlan_pcp {
	uint8_t vlan_pcp; /**< VLAN priority. */
};

/**
 * RTE_FLOW_ACTION_TYPE_OF_POP_MPLS
 *
 * Implements OFPAT_POP_MPLS ("pop the outer MPLS tag") as defined by the
 * OpenFlow Switch Specification.
 */
struct rte_flow_action_of_pop_mpls {
	rte_be16_t ethertype; /**< EtherType. */
};

/**
 * RTE_FLOW_ACTION_TYPE_OF_PUSH_MPLS
 *
 * Implements OFPAT_PUSH_MPLS ("push a new MPLS tag") as defined by the
 * OpenFlow Switch Specification.
 */
struct rte_flow_action_of_push_mpls {
	rte_be16_t ethertype; /**< EtherType. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP
 *
 * VXLAN tunnel end-point encapsulation data definition
 *
 * The tunnel definition is provided through the flow item pattern, the
 * provided pattern must conform to RFC7348 for the tunnel specified. The flow
 * definition must be provided in order from the RTE_FLOW_ITEM_TYPE_ETH
 * definition up the end item which is specified by RTE_FLOW_ITEM_TYPE_END.
 *
 * The mask field allows user to specify which fields in the flow item
 * definitions can be ignored and which have valid data and can be used
 * verbatim.
 *
 * Note: the last field is not used in the definition of a tunnel and can be
 * ignored.
 *
 * Valid flow definition for RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP include:
 *
 * - ETH / IPV4 / UDP / VXLAN / END
 * - ETH / IPV6 / UDP / VXLAN / END
 * - ETH / VLAN / IPV4 / UDP / VXLAN / END
 */
struct rte_flow_action_vxlan_encap {
	/**
	 * Encapsulating vxlan tunnel definition
	 * (terminated by the END pattern item).
	 */
	struct rte_flow_item *definition;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP
 *
 * NVGRE tunnel end-point encapsulation data definition
 *
 * The tunnel definition is provided through the flow item pattern  the
 * provided pattern must conform with RFC7637. The flow definition must be
 * provided in order from the RTE_FLOW_ITEM_TYPE_ETH definition up the end item
 * which is specified by RTE_FLOW_ITEM_TYPE_END.
 *
 * The mask field allows user to specify which fields in the flow item
 * definitions can be ignored and which have valid data and can be used
 * verbatim.
 *
 * Note: the last field is not used in the definition of a tunnel and can be
 * ignored.
 *
 * Valid flow definition for RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP include:
 *
 * - ETH / IPV4 / NVGRE / END
 * - ETH / VLAN / IPV6 / NVGRE / END
 */
struct rte_flow_action_nvgre_encap {
	/**
	 * Encapsulating vxlan tunnel definition
	 * (terminated by the END pattern item).
	 */
	struct rte_flow_item *definition;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_RAW_ENCAP
 *
 * Raw tunnel end-point encapsulation data definition.
 *
 * The data holds the headers definitions to be applied on the packet.
 * The data must start with ETH header up to the tunnel item header itself.
 * When used right after RAW_DECAP (for decapsulating L3 tunnel type for
 * example MPLSoGRE) the data will just hold layer 2 header.
 *
 * The preserve parameter holds which bits in the packet the PMD is not allowed
 * to change, this parameter can also be NULL and then the PMD is allowed
 * to update any field.
 *
 * size holds the number of bytes in @p data and @p preserve.
 */
struct rte_flow_action_raw_encap {
	uint8_t *data; /**< Encapsulation data. */
	uint8_t *preserve; /**< Bit-mask of @p data to preserve on output. */
	size_t size; /**< Size of @p data and @p preserve. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_RAW_DECAP
 *
 * Raw tunnel end-point decapsulation data definition.
 *
 * The data holds the headers definitions to be removed from the packet.
 * The data must start with ETH header up to the tunnel item header itself.
 * When used right before RAW_DECAP (for encapsulating L3 tunnel type for
 * example MPLSoGRE) the data will just hold layer 2 header.
 *
 * size holds the number of bytes in @p data.
 */
struct rte_flow_action_raw_decap {
	uint8_t *data; /**< Encapsulation data. */
	size_t size; /**< Size of @p data and @p preserve. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC
 * RTE_FLOW_ACTION_TYPE_SET_IPV4_DST
 *
 * Allows modification of IPv4 source (RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC)
 * and destination address (RTE_FLOW_ACTION_TYPE_SET_IPV4_DST) in the
 * specified outermost IPv4 header.
 */
struct rte_flow_action_set_ipv4 {
	rte_be32_t ipv4_addr;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC
 * RTE_FLOW_ACTION_TYPE_SET_IPV6_DST
 *
 * Allows modification of IPv6 source (RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC)
 * and destination address (RTE_FLOW_ACTION_TYPE_SET_IPV6_DST) in the
 * specified outermost IPv6 header.
 */
struct rte_flow_action_set_ipv6 {
	uint8_t ipv6_addr[16];
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH
 *
 * Valid flow definition for RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH include:
 *
 * - IPV6_EXT TYPE / IPV6_EXT_HEADER_IN_TYPE / END
 *
 * The data must be added as the last IPv6 extension.
 */
struct rte_flow_action_ipv6_ext_push {
	uint8_t *data; /**< IPv6 extension header data. */
	size_t size; /**< Size (in bytes) of @p data. */
	uint8_t type; /**< Type of IPv6 extension. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE
 *
 * Valid flow definition for RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE include:
 *
 * - IPV6_EXT TYPE / END
 */
struct rte_flow_action_ipv6_ext_remove {
	uint8_t type; /**< Type of IPv6 extension. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_SET_TP_SRC
 * RTE_FLOW_ACTION_TYPE_SET_TP_DST
 *
 * Allows modification of source (RTE_FLOW_ACTION_TYPE_SET_TP_SRC)
 * and destination (RTE_FLOW_ACTION_TYPE_SET_TP_DST) port numbers
 * in the specified outermost TCP/UDP header.
 */
struct rte_flow_action_set_tp {
	rte_be16_t port;
};

/**
 * RTE_FLOW_ACTION_TYPE_SET_TTL
 *
 * Set the TTL value directly for IPv4 or IPv6
 */
struct rte_flow_action_set_ttl {
	uint8_t ttl_value;
};

/**
 * RTE_FLOW_ACTION_TYPE_SET_MAC
 *
 * Set MAC address from the matched flow
 */
struct rte_flow_action_set_mac {
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_SET_TAG
 *
 * Set a tag which is a transient data used during flow matching. This is not
 * delivered to application. Multiple tags are supported by specifying index.
 */
struct rte_flow_action_set_tag {
	uint32_t data;
	uint32_t mask;
	uint8_t index;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_SET_META
 *
 * Set metadata. Metadata set by mbuf metadata dynamic field with
 * RTE_MBUF_DYNFLAG_TX_METADATA flag on egress will be overridden by this
 * action. On ingress, the metadata will be carried by mbuf metadata dynamic
 * field with RTE_MBUF_DYNFLAG_RX_METADATA flag if set.  The dynamic mbuf field
 * must be registered in advance by rte_flow_dynf_metadata_register().
 *
 * Altering partial bits is supported with mask. For bits which have never
 * been set, unpredictable value will be seen depending on driver
 * implementation. For loopback/hairpin packet, metadata set on Rx/Tx may
 * or may not be propagated to the other path depending on HW capability.
 *
 * RTE_FLOW_ITEM_TYPE_META matches metadata.
 */
struct rte_flow_action_set_meta {
	uint32_t data;
	uint32_t mask;
};

/**
 * RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP
 * RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP
 *
 * Set the DSCP value for IPv4/IPv6 header.
 * DSCP in low 6 bits, rest ignored.
 */
struct rte_flow_action_set_dscp {
	uint8_t dscp;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_INDIRECT
 *
 * Opaque type returned after successfully creating an indirect action object.
 * The definition of the object handle is different per driver or
 * per direct action type.
 *
 * This handle can be used to manage and query the related direct action:
 * - referenced in single flow rule or across multiple flow rules
 *   over multiple ports
 * - update action object configuration
 * - query action object data
 * - destroy action object
 */
struct rte_flow_action_handle;

/**
 * The state of a TCP connection.
 */
enum rte_flow_conntrack_state {
	/** SYN-ACK packet was seen. */
	RTE_FLOW_CONNTRACK_STATE_SYN_RECV,
	/** 3-way handshake was done. */
	RTE_FLOW_CONNTRACK_STATE_ESTABLISHED,
	/** First FIN packet was received to close the connection. */
	RTE_FLOW_CONNTRACK_STATE_FIN_WAIT,
	/** First FIN was ACKed. */
	RTE_FLOW_CONNTRACK_STATE_CLOSE_WAIT,
	/** Second FIN was received, waiting for the last ACK. */
	RTE_FLOW_CONNTRACK_STATE_LAST_ACK,
	/** Second FIN was ACKed, connection was closed. */
	RTE_FLOW_CONNTRACK_STATE_TIME_WAIT,
};

/**
 * The last passed TCP packet flags of a connection.
 */
enum rte_flow_conntrack_tcp_last_index {
	RTE_FLOW_CONNTRACK_FLAG_NONE = 0, /**< No Flag. */
	RTE_FLOW_CONNTRACK_FLAG_SYN = RTE_BIT32(0), /**< With SYN flag. */
	RTE_FLOW_CONNTRACK_FLAG_SYNACK = RTE_BIT32(1), /**< With SYNACK flag. */
	RTE_FLOW_CONNTRACK_FLAG_FIN = RTE_BIT32(2), /**< With FIN flag. */
	RTE_FLOW_CONNTRACK_FLAG_ACK = RTE_BIT32(3), /**< With ACK flag. */
	RTE_FLOW_CONNTRACK_FLAG_RST = RTE_BIT32(4), /**< With RST flag. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * Configuration parameters for each direction of a TCP connection.
 * All fields should be in host byte order.
 * If needed, driver should convert all fields to network byte order
 * if HW needs them in that way.
 */
struct rte_flow_tcp_dir_param {
	/** TCP window scaling factor, 0xF to disable. */
	uint32_t scale:4;
	/** The FIN was sent by this direction. */
	uint32_t close_initiated:1;
	/** An ACK packet has been received by this side. */
	uint32_t last_ack_seen:1;
	/**
	 * If set, it indicates that there is unacknowledged data for the
	 * packets sent from this direction.
	 */
	uint32_t data_unacked:1;
	/**
	 * Maximal value of sequence + payload length in sent
	 * packets (next ACK from the opposite direction).
	 */
	uint32_t sent_end;
	/**
	 * Maximal value of (ACK + window size) in received packet + length
	 * over sent packet (maximal sequence could be sent).
	 */
	uint32_t reply_end;
	/** Maximal value of actual window size in sent packets. */
	uint32_t max_win;
	/** Maximal value of ACK in sent packets. */
	uint32_t max_ack;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_CONNTRACK
 *
 * Configuration and initial state for the connection tracking module.
 * This structure could be used for both setting and query.
 * All fields should be in host byte order.
 */
struct rte_flow_action_conntrack {
	/** The peer port number, can be the same port. */
	uint16_t peer_port;
	/**
	 * Direction of this connection when creating a flow rule, the
	 * value only affects the creation of subsequent flow rules.
	 */
	uint32_t is_original_dir:1;
	/**
	 * Enable / disable the conntrack HW module. When disabled, the
	 * result will always be RTE_FLOW_CONNTRACK_FLAG_DISABLED.
	 * In this state the HW will act as passthrough.
	 * It only affects this conntrack object in the HW without any effect
	 * to the other objects.
	 */
	uint32_t enable:1;
	/** At least one ack was seen after the connection was established. */
	uint32_t live_connection:1;
	/** Enable selective ACK on this connection. */
	uint32_t selective_ack:1;
	/** A challenge ack has passed. */
	uint32_t challenge_ack_passed:1;
	/**
	 * 1: The last packet is seen from the original direction.
	 * 0: The last packet is seen from the reply direction.
	 */
	uint32_t last_direction:1;
	/** No TCP check will be done except the state change. */
	uint32_t liberal_mode:1;
	/** The current state of this connection. */
	enum rte_flow_conntrack_state state;
	/** Scaling factor for maximal allowed ACK window. */
	uint8_t max_ack_window;
	/** Maximal allowed number of retransmission times. */
	uint8_t retransmission_limit;
	/** TCP parameters of the original direction. */
	struct rte_flow_tcp_dir_param original_dir;
	/** TCP parameters of the reply direction. */
	struct rte_flow_tcp_dir_param reply_dir;
	/** The window value of the last packet passed this conntrack. */
	uint16_t last_window;
	enum rte_flow_conntrack_tcp_last_index last_index;
	/** The sequence of the last packet passed this conntrack. */
	uint32_t last_seq;
	/** The acknowledgment of the last packet passed this conntrack. */
	uint32_t last_ack;
	/**
	 * The total value ACK + payload length of the last packet
	 * passed this conntrack.
	 */
	uint32_t last_end;
};

/**
 * RTE_FLOW_ACTION_TYPE_CONNTRACK
 *
 * Wrapper structure for the context update interface.
 * Ports cannot support updating, and the only valid solution is to
 * destroy the old context and create a new one instead.
 */
struct rte_flow_modify_conntrack {
	/** New connection tracking parameters to be updated. */
	struct rte_flow_action_conntrack new_ct;
	/** The direction field will be updated. */
	uint32_t direction:1;
	/** All the other fields except direction will be updated. */
	uint32_t state:1;
	/** Reserved bits for the future usage. */
	uint32_t reserved:30;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_METER_COLOR
 *
 * The meter color should be set in the packet meta-data
 * (i.e. struct rte_mbuf::sched::color).
 */
struct rte_flow_action_meter_color {
	enum rte_color color; /**< Packet color. */
};

/**
 * Provides an ethdev port ID for use with the following actions:
 * RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR,
 * RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT.
 */
struct rte_flow_action_ethdev {
	uint16_t port_id; /**< ethdev port ID */
};

/**
 * Field IDs for MODIFY_FIELD action.
 */
enum rte_flow_field_id {
	RTE_FLOW_FIELD_START = 0,	/**< Start of a packet. */
	RTE_FLOW_FIELD_MAC_DST,		/**< Destination MAC Address. */
	RTE_FLOW_FIELD_MAC_SRC,		/**< Source MAC Address. */
	RTE_FLOW_FIELD_VLAN_TYPE,	/**< VLAN Tag Identifier. */
	RTE_FLOW_FIELD_VLAN_ID,		/**< VLAN Identifier. */
	RTE_FLOW_FIELD_MAC_TYPE,	/**< EtherType. */
	RTE_FLOW_FIELD_IPV4_DSCP,	/**< IPv4 DSCP. */
	RTE_FLOW_FIELD_IPV4_TTL,	/**< IPv4 Time To Live. */
	RTE_FLOW_FIELD_IPV4_SRC,	/**< IPv4 Source Address. */
	RTE_FLOW_FIELD_IPV4_DST,	/**< IPv4 Destination Address. */
	RTE_FLOW_FIELD_IPV6_DSCP,	/**< IPv6 DSCP. */
	RTE_FLOW_FIELD_IPV6_HOPLIMIT,	/**< IPv6 Hop Limit. */
	RTE_FLOW_FIELD_IPV6_SRC,	/**< IPv6 Source Address. */
	RTE_FLOW_FIELD_IPV6_DST,	/**< IPv6 Destination Address. */
	RTE_FLOW_FIELD_TCP_PORT_SRC,	/**< TCP Source Port Number. */
	RTE_FLOW_FIELD_TCP_PORT_DST,	/**< TCP Destination Port Number. */
	RTE_FLOW_FIELD_TCP_SEQ_NUM,	/**< TCP Sequence Number. */
	RTE_FLOW_FIELD_TCP_ACK_NUM,	/**< TCP Acknowledgment Number. */
	RTE_FLOW_FIELD_TCP_FLAGS,	/**< TCP Flags. */
	RTE_FLOW_FIELD_UDP_PORT_SRC,	/**< UDP Source Port Number. */
	RTE_FLOW_FIELD_UDP_PORT_DST,	/**< UDP Destination Port Number. */
	RTE_FLOW_FIELD_VXLAN_VNI,	/**< VXLAN Network Identifier. */
	RTE_FLOW_FIELD_GENEVE_VNI,	/**< GENEVE Network Identifier. */
	RTE_FLOW_FIELD_GTP_TEID,	/**< GTP Tunnel Endpoint Identifier. */
	RTE_FLOW_FIELD_TAG,		/**< Tag value. */
	RTE_FLOW_FIELD_MARK,		/**< Mark value. */
	RTE_FLOW_FIELD_META,		/**< Metadata value. */
	RTE_FLOW_FIELD_POINTER,		/**< Memory pointer. */
	RTE_FLOW_FIELD_VALUE,		/**< Immediate value. */
	RTE_FLOW_FIELD_IPV4_ECN,	/**< IPv4 ECN. */
	RTE_FLOW_FIELD_IPV6_ECN,	/**< IPv6 ECN. */
	RTE_FLOW_FIELD_GTP_PSC_QFI,	/**< GTP QFI. */
	RTE_FLOW_FIELD_METER_COLOR,	/**< Meter color marker. */
	RTE_FLOW_FIELD_IPV6_PROTO,	/**< IPv6 next header. */
	RTE_FLOW_FIELD_FLEX_ITEM,	/**< Flex item. */
	RTE_FLOW_FIELD_HASH_RESULT,	/**< Hash result. */
	RTE_FLOW_FIELD_GENEVE_OPT_TYPE,	/**< GENEVE option type. */
	RTE_FLOW_FIELD_GENEVE_OPT_CLASS,/**< GENEVE option class. */
	RTE_FLOW_FIELD_GENEVE_OPT_DATA,	/**< GENEVE option data. */
	RTE_FLOW_FIELD_MPLS,		/**< MPLS header. */
	RTE_FLOW_FIELD_TCP_DATA_OFFSET,	/**< TCP data offset. */
	RTE_FLOW_FIELD_IPV4_IHL,	/**< IPv4 IHL. */
	RTE_FLOW_FIELD_IPV4_TOTAL_LEN,	/**< IPv4 total length. */
	RTE_FLOW_FIELD_IPV6_PAYLOAD_LEN	/**< IPv6 payload length. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * Field description for MODIFY_FIELD action.
 */
struct rte_flow_action_modify_data {
	enum rte_flow_field_id field; /**< Field or memory type ID. */
	union {
		struct {
			/** Encapsulation level and tag index or flex item handle. */
			union {
				struct {
					/**
					 * Packet encapsulation level containing
					 * the field to modify.
					 *
					 * - @p 0 requests the default behavior.
					 *   Depending on the packet type, it
					 *   can mean outermost, innermost or
					 *   anything in between.
					 *
					 *   It basically stands for the
					 *   innermost encapsulation level.
					 *   Modification can be performed
					 *   according to PMD and device
					 *   capabilities.
					 *
					 * - @p 1 requests modification to be
					 *   performed on the outermost packet
					 *   encapsulation level.
					 *
					 * - @p 2 and subsequent values request
					 *   modification to be performed on
					 *   the specified inner packet
					 *   encapsulation level, from
					 *   outermost to innermost (lower to
					 *   higher values).
					 *
					 * Values other than @p 0 are not
					 * necessarily supported.
					 *
					 * @note that for MPLS field,
					 * encapsulation level also include
					 * tunnel since MPLS may appear in
					 * outer, inner or tunnel.
					 */
					uint8_t level;
					union {
						/**
						 * Tag index array inside
						 * encapsulation level.
						 * Used for VLAN, MPLS or TAG types.
						 */
						uint8_t tag_index;
						/**
						 * Geneve option identifier.
						 * Relevant only for
						 * RTE_FLOW_FIELD_GENEVE_OPT_XXXX
						 * modification type.
						 */
						struct {
							/**
							 * Geneve option type.
							 */
							uint8_t type;
							/**
							 * Geneve option class.
							 */
							rte_be16_t class_id;
						};
					};
				};
				struct rte_flow_item_flex_handle *flex_handle;
			};
			/** Number of bits to skip from a field. */
			uint32_t offset;
		};
		/**
		 * Immediate value for RTE_FLOW_FIELD_VALUE, presented in the
		 * same byte order and length as in relevant rte_flow_item_xxx.
		 * The immediate source bitfield offset is inherited from
		 * the destination's one.
		 */
		uint8_t value[16];
		/**
		 * Memory address for RTE_FLOW_FIELD_POINTER, memory layout
		 * should be the same as for relevant field in the
		 * rte_flow_item_xxx structure.
		 */
		void *pvalue;
	};
};

/**
 * Operation types for MODIFY_FIELD action.
 */
enum rte_flow_modify_op {
	RTE_FLOW_MODIFY_SET = 0, /**< Set a new value. */
	RTE_FLOW_MODIFY_ADD,     /**< Add a value to a field.  */
	RTE_FLOW_MODIFY_SUB,     /**< Subtract a value from a field. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
 *
 * Modify a destination header field according to the specified
 * operation. Another field of the packet can be used as a source as well
 * as tag, mark, metadata, immediate value or a pointer to it.
 */
struct rte_flow_action_modify_field {
	enum rte_flow_modify_op operation; /**< Operation to perform. */
	struct rte_flow_action_modify_data dst; /**< Destination field. */
	struct rte_flow_action_modify_data src; /**< Source field. */
	uint32_t width; /**< Number of bits to use from a source field. */
};

/**
 * RTE_FLOW_ACTION_TYPE_METER_MARK
 *
 * Traffic metering and marking (MTR).
 *
 * Meters a packet stream and marks its packets either
 * green, yellow, or red according to the specified profile.
 * The policy is optional and may be specified for defining
 * subsequent actions based on a color assigned by MTR.
 * Alternatively, the METER_COLOR item may be used for this.
 */
struct rte_flow_action_meter_mark {

	/**< Profile config retrieved with rte_mtr_profile_get(). */
	struct rte_flow_meter_profile *profile;
	/**< Policy config retrieved with rte_mtr_policy_get(). */
	struct rte_flow_meter_policy *policy;
	/** Metering mode: 0 - Color-Blind, 1 - Color-Aware. */
	int color_mode;
	/** Metering state: 0 - Disabled, 1 - Enabled. */
	int state;
};

/**
 * RTE_FLOW_ACTION_TYPE_METER_MARK
 *
 * Wrapper structure for the context update interface.
 */
struct rte_flow_update_meter_mark {
	/** New meter_mark parameters to be updated. */
	struct rte_flow_action_meter_mark meter_mark;
	/** The profile will be updated. */
	uint32_t profile_valid:1;
	/** The policy will be updated. */
	uint32_t policy_valid:1;
	/** The color mode will be updated. */
	uint32_t color_mode_valid:1;
	/** The meter state will be updated. */
	uint32_t state_valid:1;
	/** Reserved bits for the future usage. */
	uint32_t reserved:28;
};

/**
 * @see RTE_FLOW_ACTION_TYPE_METER_MARK
 * @see RTE_FLOW_ACTION_TYPE_INDIRECT_LIST
 *
 * Update flow mutable context.
 */
struct rte_flow_indirect_update_flow_meter_mark {
	/** Updated init color applied to packet */
	enum rte_color init_color;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * Program action argument configuration parameters.
 *
 * For each action argument, its *size* must be non-zero and its *value* must
 * point to a valid array of *size* bytes specified in network byte order.
 *
 * @see struct rte_flow_action_prog
 */
struct rte_flow_action_prog_argument {
	/** Argument name. */
	const char *name;
	/** Argument size in bytes. */
	uint32_t size;
	/** Argument value. */
	const uint8_t *value;
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice.
 *
 * RTE_FLOW_ACTION_TYPE_PROG
 *
 * Program action configuration parameters.
 *
 * Each action can have zero or more arguments. When *args_num* is non-zero, the
 * *args* parameter must point to a valid array of *args_num* elements.
 *
 * @see RTE_FLOW_ACTION_TYPE_PROG
 */
struct rte_flow_action_prog {
	/** Action name. */
	const char *name;
	/** Number of action arguments. */
	uint32_t args_num;
	/** Action arguments array. */
	const struct rte_flow_action_prog_argument *args;
};

/* Mbuf dynamic field offset for metadata. */
extern int32_t rte_flow_dynf_metadata_offs;

/* Mbuf dynamic field flag mask for metadata. */
extern uint64_t rte_flow_dynf_metadata_mask;

/* Mbuf dynamic field pointer for metadata. */
#define RTE_FLOW_DYNF_METADATA(m) \
	RTE_MBUF_DYNFIELD((m), rte_flow_dynf_metadata_offs, uint32_t *)

/* Mbuf dynamic flags for metadata. */
#define RTE_MBUF_DYNFLAG_RX_METADATA (rte_flow_dynf_metadata_mask)
#define RTE_MBUF_DYNFLAG_TX_METADATA (rte_flow_dynf_metadata_mask)

__rte_experimental
static inline uint32_t
rte_flow_dynf_metadata_get(struct rte_mbuf *m)
{
	return *RTE_FLOW_DYNF_METADATA(m);
}

__rte_experimental
static inline void
rte_flow_dynf_metadata_set(struct rte_mbuf *m, uint32_t v)
{
	*RTE_FLOW_DYNF_METADATA(m) = v;
}

/**
 * Definition of a single action.
 *
 * A list of actions is terminated by a END action.
 *
 * For simple actions without a configuration object, conf remains NULL.
 */
struct rte_flow_action {
	enum rte_flow_action_type type; /**< Action type. */
	const void *conf; /**< Pointer to action configuration object. */
};

/**
 * Opaque type returned after successfully creating a flow.
 *
 * This handle can be used to manage and query the related flow (e.g. to
 * destroy it or retrieve counters).
 */
struct rte_flow;

/**
 * Opaque type for Meter profile object returned by MTR API.
 *
 * This handle can be used to create Meter actions instead of profile ID.
 */
struct rte_flow_meter_profile;

/**
 * Opaque type for Meter policy object returned by MTR API.
 *
 * This handle can be used to create Meter actions instead of policy ID.
 */
struct rte_flow_meter_policy;

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_SAMPLE
 *
 * Adds a sample action to a matched flow.
 *
 * The matching packets will be duplicated with specified ratio and applied
 * with own set of actions with a fate action, the sampled packet could be
 * redirected to queue or port. All the packets continue processing on the
 * default flow path.
 *
 * When the sample ratio is set to 1 then the packets will be 100% mirrored.
 * Additional action list be supported to add for sampled or mirrored packets.
 */
struct rte_flow_action_sample {
	uint32_t ratio; /**< packets sampled equals to '1/ratio'. */
	/** sub-action list specific for the sampling hit cases. */
	const struct rte_flow_action *actions;
};

/**
 * Verbose error types.
 *
 * Most of them provide the type of the object referenced by struct
 * rte_flow_error.cause.
 */
enum rte_flow_error_type {
	RTE_FLOW_ERROR_TYPE_NONE, /**< No error. */
	RTE_FLOW_ERROR_TYPE_UNSPECIFIED, /**< Cause unspecified. */
	RTE_FLOW_ERROR_TYPE_HANDLE, /**< Flow rule (handle). */
	RTE_FLOW_ERROR_TYPE_ATTR_GROUP, /**< Group field. */
	RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, /**< Priority field. */
	RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, /**< Ingress field. */
	RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, /**< Egress field. */
	RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER, /**< Transfer field. */
	RTE_FLOW_ERROR_TYPE_ATTR, /**< Attributes structure. */
	RTE_FLOW_ERROR_TYPE_ITEM_NUM, /**< Pattern length. */
	RTE_FLOW_ERROR_TYPE_ITEM_SPEC, /**< Item specification. */
	RTE_FLOW_ERROR_TYPE_ITEM_LAST, /**< Item specification range. */
	RTE_FLOW_ERROR_TYPE_ITEM_MASK, /**< Item specification mask. */
	RTE_FLOW_ERROR_TYPE_ITEM, /**< Specific pattern item. */
	RTE_FLOW_ERROR_TYPE_ACTION_NUM, /**< Number of actions. */
	RTE_FLOW_ERROR_TYPE_ACTION_CONF, /**< Action configuration. */
	RTE_FLOW_ERROR_TYPE_ACTION, /**< Specific action. */
	RTE_FLOW_ERROR_TYPE_STATE, /**< Current device state. */
};

/**
 * Verbose error structure definition.
 *
 * This object is normally allocated by applications and set by PMDs, the
 * message points to a constant string which does not need to be freed by
 * the application, however its pointer can be considered valid only as long
 * as its associated DPDK port remains configured. Closing the underlying
 * device or unloading the PMD invalidates it.
 *
 * Both cause and message may be NULL regardless of the error type.
 */
struct rte_flow_error {
	enum rte_flow_error_type type; /**< Cause field and error types. */
	const void *cause; /**< Object responsible for the error. */
	const char *message; /**< Human-readable error message. */
};

/**
 * Complete flow rule description.
 *
 * This object type is used when converting a flow rule description.
 *
 * @see RTE_FLOW_CONV_OP_RULE
 * @see rte_flow_conv()
 */
struct rte_flow_conv_rule {
	union {
		const struct rte_flow_attr *attr_ro; /**< RO attributes. */
		struct rte_flow_attr *attr; /**< Attributes. */
	};
	union {
		const struct rte_flow_item *pattern_ro; /**< RO pattern. */
		struct rte_flow_item *pattern; /**< Pattern items. */
	};
	union {
		const struct rte_flow_action *actions_ro; /**< RO actions. */
		struct rte_flow_action *actions; /**< List of actions. */
	};
};

/**
 * Conversion operations for flow API objects.
 *
 * @see rte_flow_conv()
 */
enum rte_flow_conv_op {
	/**
	 * No operation to perform.
	 *
	 * rte_flow_conv() simply returns 0.
	 */
	RTE_FLOW_CONV_OP_NONE,

	/**
	 * Convert attributes structure.
	 *
	 * This is a basic copy of an attributes structure.
	 *
	 * - @p src type:
	 *   @code const struct rte_flow_attr * @endcode
	 * - @p dst type:
	 *   @code struct rte_flow_attr * @endcode
	 */
	RTE_FLOW_CONV_OP_ATTR,

	/**
	 * Convert a single item.
	 *
	 * Duplicates @p spec, @p last and @p mask but not outside objects.
	 *
	 * - @p src type:
	 *   @code const struct rte_flow_item * @endcode
	 * - @p dst type:
	 *   @code struct rte_flow_item * @endcode
	 */
	RTE_FLOW_CONV_OP_ITEM,

	/**
	 * Convert a single action.
	 *
	 * Duplicates @p conf but not outside objects.
	 *
	 * - @p src type:
	 *   @code const struct rte_flow_action * @endcode
	 * - @p dst type:
	 *   @code struct rte_flow_action * @endcode
	 */
	RTE_FLOW_CONV_OP_ACTION,

	/**
	 * Convert an entire pattern.
	 *
	 * Duplicates all pattern items at once with the same constraints as
	 * RTE_FLOW_CONV_OP_ITEM.
	 *
	 * - @p src type:
	 *   @code const struct rte_flow_item * @endcode
	 * - @p dst type:
	 *   @code struct rte_flow_item * @endcode
	 */
	RTE_FLOW_CONV_OP_PATTERN,

	/**
	 * Convert a list of actions.
	 *
	 * Duplicates the entire list of actions at once with the same
	 * constraints as RTE_FLOW_CONV_OP_ACTION.
	 *
	 * - @p src type:
	 *   @code const struct rte_flow_action * @endcode
	 * - @p dst type:
	 *   @code struct rte_flow_action * @endcode
	 */
	RTE_FLOW_CONV_OP_ACTIONS,

	/**
	 * Convert a complete flow rule description.
	 *
	 * Comprises attributes, pattern and actions together at once with
	 * the usual constraints.
	 *
	 * - @p src type:
	 *   @code const struct rte_flow_conv_rule * @endcode
	 * - @p dst type:
	 *   @code struct rte_flow_conv_rule * @endcode
	 */
	RTE_FLOW_CONV_OP_RULE,

	/**
	 * Convert item type to its name string.
	 *
	 * Writes a NUL-terminated string to @p dst. Like snprintf(), the
	 * returned value excludes the terminator which is always written
	 * nonetheless.
	 *
	 * - @p src type:
	 *   @code (const void *)enum rte_flow_item_type @endcode
	 * - @p dst type:
	 *   @code char * @endcode
	 */
	RTE_FLOW_CONV_OP_ITEM_NAME,

	/**
	 * Convert action type to its name string.
	 *
	 * Writes a NUL-terminated string to @p dst. Like snprintf(), the
	 * returned value excludes the terminator which is always written
	 * nonetheless.
	 *
	 * - @p src type:
	 *   @code (const void *)enum rte_flow_action_type @endcode
	 * - @p dst type:
	 *   @code char * @endcode
	 */
	RTE_FLOW_CONV_OP_ACTION_NAME,

	/**
	 * Convert item type to pointer to item name.
	 *
	 * Retrieves item name pointer from its type. The string itself is
	 * not copied; instead, a unique pointer to an internal static
	 * constant storage is written to @p dst.
	 *
	 * - @p src type:
	 *   @code (const void *)enum rte_flow_item_type @endcode
	 * - @p dst type:
	 *   @code const char ** @endcode
	 */
	RTE_FLOW_CONV_OP_ITEM_NAME_PTR,

	/**
	 * Convert action type to pointer to action name.
	 *
	 * Retrieves action name pointer from its type. The string itself is
	 * not copied; instead, a unique pointer to an internal static
	 * constant storage is written to @p dst.
	 *
	 * - @p src type:
	 *   @code (const void *)enum rte_flow_action_type @endcode
	 * - @p dst type:
	 *   @code const char ** @endcode
	 */
	RTE_FLOW_CONV_OP_ACTION_NAME_PTR,
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Dump hardware internal representation information of
 * rte flow to file.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] flow
 *   The pointer of flow rule to dump. Dump all rules if NULL.
 * @param[in] file
 *   A pointer to a file for output.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   0 on success, a negative value otherwise.
 */
__rte_experimental
int
rte_flow_dev_dump(uint16_t port_id, struct rte_flow *flow,
		FILE *file, struct rte_flow_error *error);

/**
 * Check if mbuf dynamic field for metadata is registered.
 *
 * @return
 *   True if registered, false otherwise.
 */
__rte_experimental
static inline int
rte_flow_dynf_metadata_avail(void)
{
	return !!rte_flow_dynf_metadata_mask;
}

/**
 * Register mbuf dynamic field and flag for metadata.
 *
 * This function must be called prior to use SET_META action in order to
 * register the dynamic mbuf field. Otherwise, the data cannot be delivered to
 * application.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_dynf_metadata_register(void);

/**
 * Check whether a flow rule can be created on a given port.
 *
 * The flow rule is validated for correctness and whether it could be accepted
 * by the device given sufficient resources. The rule is checked against the
 * current device mode and queue configuration. The flow rule may also
 * optionally be validated against existing flow rules and device resources.
 * This function has no effect on the target device.
 *
 * The returned value is guaranteed to remain valid only as long as no
 * successful calls to rte_flow_create() or rte_flow_destroy() are made in
 * the meantime and no device parameter affecting flow rules in any way are
 * modified, due to possible collisions or resource limitations (although in
 * such cases EINVAL should not be returned).
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 if flow rule is valid and can be created. A negative errno value
 *   otherwise (rte_errno is also set), the following errors are defined:
 *
 *   -ENOSYS: underlying device does not support this functionality.
 *
 *   -EIO: underlying device is removed.
 *
 *   -EINVAL: unknown or invalid rule specification.
 *
 *   -ENOTSUP: valid but unsupported rule specification (e.g. partial
 *   bit-masks are unsupported).
 *
 *   -EEXIST: collision with an existing rule. Only returned if device
 *   supports flow rule collision checking and there was a flow rule
 *   collision. Not receiving this return code is no guarantee that creating
 *   the rule will not fail due to a collision.
 *
 *   -ENOMEM: not enough memory to execute the function, or if the device
 *   supports resource validation, resource limitation on the device.
 *
 *   -EBUSY: action cannot be performed due to busy device resources, may
 *   succeed if the affected queues or even the entire port are in a stopped
 *   state (see rte_eth_dev_rx_queue_stop() and rte_eth_dev_stop()).
 */
int
rte_flow_validate(uint16_t port_id,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error);

/**
 * Create a flow rule on a given port.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   A valid handle in case of success, NULL otherwise and rte_errno is set
 *   to the positive version of one of the error codes defined for
 *   rte_flow_validate().
 */
struct rte_flow *
rte_flow_create(uint16_t port_id,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);

/**
 * Destroy a flow rule on a given port.
 *
 * Failure to destroy a flow rule handle may occur when other flow rules
 * depend on it, and destroying it would result in an inconsistent state.
 *
 * This function is only guaranteed to succeed if handles are destroyed in
 * reverse order of their creation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param flow
 *   Flow rule handle to destroy.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
rte_flow_destroy(uint16_t port_id,
		 struct rte_flow *flow,
		 struct rte_flow_error *error);

/**
 * Update a flow rule with new actions on a given port.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param flow
 *   Flow rule handle to update.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_actions_update(uint16_t port_id,
			struct rte_flow *flow,
			const struct rte_flow_action actions[],
			struct rte_flow_error *error);

/**
 * Destroy all flow rules associated with a port.
 *
 * In the unlikely event of failure, handles are still considered destroyed
 * and no longer valid but the port must be assumed to be in an inconsistent
 * state.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
rte_flow_flush(uint16_t port_id,
	       struct rte_flow_error *error);

/**
 * Query an existing flow rule.
 *
 * This function allows retrieving flow-specific data such as counters.
 * Data is gathered by special actions which must be present in the flow
 * rule definition.
 *
 * \see RTE_FLOW_ACTION_TYPE_COUNT
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param flow
 *   Flow rule handle to query.
 * @param action
 *   Action definition as defined in original flow rule.
 * @param[in, out] data
 *   Pointer to storage for the associated query data type.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
rte_flow_query(uint16_t port_id,
	       struct rte_flow *flow,
	       const struct rte_flow_action *action,
	       void *data,
	       struct rte_flow_error *error);

/**
 * Restrict ingress traffic to the defined flow rules.
 *
 * Isolated mode guarantees that all ingress traffic comes from defined flow
 * rules only (current and future).
 * When enabled with a bifurcated driver,
 * non-matched packets are routed to the kernel driver interface.
 * When disabled (the default),
 * there may be some default rules routing traffic to the DPDK port.
 *
 * Besides making ingress more deterministic, it allows PMDs to safely reuse
 * resources otherwise assigned to handle the remaining traffic, such as
 * global RSS configuration settings, VLAN filters, MAC address entries,
 * legacy filter API rules and so on in order to expand the set of possible
 * flow rule types.
 *
 * Calling this function as soon as possible after device initialization,
 * ideally before the first call to rte_eth_dev_configure(), is recommended
 * to avoid possible failures due to conflicting settings.
 *
 * Once effective, leaving isolated mode may not be possible depending on
 * PMD implementation.
 *
 * Additionally, the following functionality has no effect on the underlying
 * port and may return errors such as ENOTSUP ("not supported"):
 *
 * - Toggling promiscuous mode.
 * - Toggling allmulticast mode.
 * - Configuring MAC addresses.
 * - Configuring multicast addresses.
 * - Configuring VLAN filters.
 * - Configuring Rx filters through the legacy API (e.g. FDIR).
 * - Configuring global RSS settings.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param set
 *   Nonzero to enter isolated mode, attempt to leave it otherwise.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
rte_flow_isolate(uint16_t port_id, int set, struct rte_flow_error *error);

/**
 * Initialize flow error structure.
 *
 * @param[out] error
 *   Pointer to flow error structure (may be NULL).
 * @param code
 *   Related error code (rte_errno).
 * @param type
 *   Cause field and error types.
 * @param cause
 *   Object responsible for the error.
 * @param message
 *   Human-readable error message.
 *
 * @return
 *   Negative error code (errno value) and rte_errno is set.
 */
int
rte_flow_error_set(struct rte_flow_error *error,
		   int code,
		   enum rte_flow_error_type type,
		   const void *cause,
		   const char *message);

/**
 * @deprecated
 * @see rte_flow_copy()
 */
struct rte_flow_desc {
	size_t size; /**< Allocated space including data[]. */
	struct rte_flow_attr attr; /**< Attributes. */
	struct rte_flow_item *items; /**< Items. */
	struct rte_flow_action *actions; /**< Actions. */
	uint8_t data[]; /**< Storage for items/actions. */
};

/**
 * @deprecated
 * Copy an rte_flow rule description.
 *
 * This interface is kept for compatibility with older applications but is
 * implemented as a wrapper to rte_flow_conv(). It is deprecated due to its
 * lack of flexibility and reliance on a type unusable with C++ programs
 * (struct rte_flow_desc).
 *
 * @param[in] fd
 *   Flow rule description.
 * @param[in] len
 *   Total size of allocated data for the flow description.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 *
 * @return
 *   If len is greater or equal to the size of the flow, the total size of the
 *   flow description and its data.
 *   If len is lower than the size of the flow, the number of bytes that would
 *   have been written to desc had it been sufficient. Nothing is written.
 */
__rte_deprecated
size_t
rte_flow_copy(struct rte_flow_desc *fd, size_t len,
	      const struct rte_flow_attr *attr,
	      const struct rte_flow_item *items,
	      const struct rte_flow_action *actions);

/**
 * Flow object conversion helper.
 *
 * This function performs conversion of various flow API objects to a
 * pre-allocated destination buffer. See enum rte_flow_conv_op for possible
 * operations and details about each of them.
 *
 * Since destination buffer must be large enough, it works in a manner
 * reminiscent of snprintf():
 *
 * - If @p size is 0, @p dst may be a NULL pointer, otherwise @p dst must be
 *   non-NULL.
 * - If positive, the returned value represents the number of bytes needed
 *   to store the conversion of @p src to @p dst according to @p op
 *   regardless of the @p size parameter.
 * - Since no more than @p size bytes can be written to @p dst, output is
 *   truncated and may be inconsistent when the returned value is larger
 *   than that.
 * - In case of conversion error, a negative error code is returned and
 *   @p dst contents are unspecified.
 *
 * @param op
 *   Operation to perform, related to the object type of @p dst.
 * @param[out] dst
 *   Destination buffer address. Must be suitably aligned by the caller.
 * @param size
 *   Destination buffer size in bytes.
 * @param[in] src
 *   Source object to copy. Depending on @p op, its type may differ from
 *   that of @p dst.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   The number of bytes required to convert @p src to @p dst on success, a
 *   negative errno value otherwise and rte_errno is set.
 *
 * @see rte_flow_conv_op
 */
__rte_experimental
int
rte_flow_conv(enum rte_flow_conv_op op,
	      void *dst,
	      size_t size,
	      const void *src,
	      struct rte_flow_error *error);

/**
 * Get aged-out flows of a given port.
 *
 * RTE_ETH_EVENT_FLOW_AGED event will be triggered when at least one new aged
 * out flow was detected after the last call to rte_flow_get_aged_flows.
 * This function can be called to get the aged flows asynchronously from the
 * event callback or synchronously regardless the event.
 * This is not safe to call rte_flow_get_aged_flows function with other flow
 * functions from multiple threads simultaneously.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in, out] contexts
 *   The address of an array of pointers to the aged-out flows contexts.
 * @param[in] nb_contexts
 *   The length of context array pointers.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   if nb_contexts is 0, return the amount of all aged contexts.
 *   if nb_contexts is not 0 , return the amount of aged flows reported
 *   in the context array, otherwise negative errno value.
 *
 * @see rte_flow_action_age
 * @see RTE_ETH_EVENT_FLOW_AGED
 */
__rte_experimental
int
rte_flow_get_aged_flows(uint16_t port_id, void **contexts,
			uint32_t nb_contexts, struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get aged-out flows of a given port on the given flow queue.
 *
 * If application configure port attribute with RTE_FLOW_PORT_FLAG_STRICT_QUEUE,
 * there is no RTE_ETH_EVENT_FLOW_AGED event and this function must be called to
 * get the aged flows synchronously.
 *
 * If application configure port attribute without
 * RTE_FLOW_PORT_FLAG_STRICT_QUEUE, RTE_ETH_EVENT_FLOW_AGED event will be
 * triggered at least one new aged out flow was detected on any flow queue after
 * the last call to rte_flow_get_q_aged_flows.
 * In addition, the @p queue_id will be ignored.
 * This function can be called to get the aged flows asynchronously from the
 * event callback or synchronously regardless the event.
 *
 * @param[in] port_id
 *   Port identifier of Ethernet device.
 * @param[in] queue_id
 *   Flow queue to query. Ignored when RTE_FLOW_PORT_FLAG_STRICT_QUEUE not set.
 * @param[in, out] contexts
 *   The address of an array of pointers to the aged-out flows contexts.
 * @param[in] nb_contexts
 *   The length of context array pointers.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   if nb_contexts is 0, return the amount of all aged contexts.
 *   if nb_contexts is not 0 , return the amount of aged flows reported
 *   in the context array, otherwise negative errno value.
 *
 * @see rte_flow_action_age
 * @see RTE_ETH_EVENT_FLOW_AGED
 * @see rte_flow_port_flag
 */
__rte_experimental
int
rte_flow_get_q_aged_flows(uint16_t port_id, uint32_t queue_id, void **contexts,
			  uint32_t nb_contexts, struct rte_flow_error *error);

/**
 * Specify indirect action object configuration
 */
struct rte_flow_indir_action_conf {
	/**
	 * Flow direction for the indirect action configuration.
	 *
	 * Action should be valid at least for one flow direction,
	 * otherwise it is invalid for both ingress and egress rules.
	 */
	/** Action valid for rules applied to ingress traffic. */
	uint32_t ingress:1;
	/** Action valid for rules applied to egress traffic. */
	uint32_t egress:1;
	/**
	 * When set to 1, indicates that the action is valid for
	 * transfer traffic; otherwise, for non-transfer traffic.
	 */
	uint32_t transfer:1;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create an indirect action object that can be used in flow rules
 * via its handle.
 * The created object handle has single state and configuration
 * across all the flow rules using it.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] conf
 *   Action configuration for the indirect action object creation.
 * @param[in] action
 *   Specific configuration of the indirect action object.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   A valid handle in case of success, NULL otherwise and rte_errno is set
 *   to one of the error codes defined:
 *   - (ENODEV) if *port_id* invalid.
 *   - (ENOSYS) if underlying device does not support this functionality.
 *   - (EIO) if underlying device is removed.
 *   - (EINVAL) if *action* invalid.
 *   - (ENOTSUP) if *action* valid but unsupported.
 */
__rte_experimental
struct rte_flow_action_handle *
rte_flow_action_handle_create(uint16_t port_id,
			      const struct rte_flow_indir_action_conf *conf,
			      const struct rte_flow_action *action,
			      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Destroy indirect action by handle.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] handle
 *   Handle for the indirect action object to be destroyed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   - (0) if success.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if underlying device does not support this functionality.
 *   - (-EIO) if underlying device is removed.
 *   - (-ENOENT) if action pointed by *action* handle was not found.
 *   - (-EBUSY) if action pointed by *action* handle still used by some rules
 *   rte_errno is also set.
 */
__rte_experimental
int
rte_flow_action_handle_destroy(uint16_t port_id,
			       struct rte_flow_action_handle *handle,
			       struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Update in-place the action configuration and / or state pointed
 * by action *handle* with the configuration provided as *update* argument.
 * The update of the action configuration effects all flow rules reusing
 * the action via *handle*.
 * The update general pointer provides the ability of partial updating.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] handle
 *   Handle for the indirect action object to be updated.
 * @param[in] update
 *   Update profile specification used to modify the action pointed by handle.
 *   *update* could be with the same type of the immediate action corresponding
 *   to the *handle* argument when creating, or a wrapper structure includes
 *   action configuration to be updated and bit fields to indicate the member
 *   of fields inside the action to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   - (0) if success.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if underlying device does not support this functionality.
 *   - (-EIO) if underlying device is removed.
 *   - (-EINVAL) if *update* invalid.
 *   - (-ENOTSUP) if *update* valid but unsupported.
 *   - (-ENOENT) if indirect action object pointed by *handle* was not found.
 *   rte_errno is also set.
 */
__rte_experimental
int
rte_flow_action_handle_update(uint16_t port_id,
			      struct rte_flow_action_handle *handle,
			      const void *update,
			      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Query the direct action by corresponding indirect action object handle.
 *
 * Retrieve action-specific data such as counters.
 * Data is gathered by special action which may be present/referenced in
 * more than one flow rule definition.
 *
 * @see RTE_FLOW_ACTION_TYPE_COUNT
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] handle
 *   Handle for the action object to query.
 * @param[in, out] data
 *   Pointer to storage for the associated query data type.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_action_handle_query(uint16_t port_id,
			     const struct rte_flow_action_handle *handle,
			     void *data, struct rte_flow_error *error);

/* Tunnel has a type and the key information. */
struct rte_flow_tunnel {
	/**
	 * Tunnel type, for example RTE_FLOW_ITEM_TYPE_VXLAN,
	 * RTE_FLOW_ITEM_TYPE_NVGRE etc.
	 */
	enum rte_flow_item_type	type;
	uint64_t tun_id; /**< Tunnel identification. */

	union {
		struct {
			rte_be32_t src_addr; /**< IPv4 source address. */
			rte_be32_t dst_addr; /**< IPv4 destination address. */
		} ipv4;
		struct {
			uint8_t src_addr[16]; /**< IPv6 source address. */
			uint8_t dst_addr[16]; /**< IPv6 destination address. */
		} ipv6;
	};
	rte_be16_t tp_src; /**< Tunnel port source. */
	rte_be16_t tp_dst; /**< Tunnel port destination. */
	uint16_t   tun_flags; /**< Tunnel flags. */

	bool       is_ipv6; /**< True for valid IPv6 fields. Otherwise IPv4. */

	/**
	 * the following members are required to restore packet
	 * after miss
	 */
	uint8_t    tos; /**< TOS for IPv4, TC for IPv6. */
	uint8_t    ttl; /**< TTL for IPv4, HL for IPv6. */
	uint32_t label; /**< Flow Label for IPv6. */
};

/**
 * Indicate that the packet has a tunnel.
 */
#define RTE_FLOW_RESTORE_INFO_TUNNEL RTE_BIT64(0)

/**
 * Indicate that the packet has a non decapsulated tunnel header.
 */
#define RTE_FLOW_RESTORE_INFO_ENCAPSULATED RTE_BIT64(1)

/**
 * Indicate that the packet has a group_id.
 */
#define RTE_FLOW_RESTORE_INFO_GROUP_ID RTE_BIT64(2)

/**
 * Restore information structure to communicate the current packet processing
 * state when some of the processing pipeline is done in hardware and should
 * continue in software.
 */
struct rte_flow_restore_info {
	/**
	 * Bitwise flags (RTE_FLOW_RESTORE_INFO_*) to indicate validation of
	 * other fields in struct rte_flow_restore_info.
	 */
	uint64_t flags;
	uint32_t group_id; /**< Group ID where packed missed */
	struct rte_flow_tunnel tunnel; /**< Tunnel information. */
};

/**
 * Allocate an array of actions to be used in rte_flow_create, to implement
 * tunnel-decap-set for the given tunnel.
 * Sample usage:
 *   actions vxlan_decap / tunnel-decap-set(tunnel properties) /
 *            jump group 0 / end
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] tunnel
 *   Tunnel properties.
 * @param[out] actions
 *   Array of actions to be allocated by the PMD. This array should be
 *   concatenated with the actions array provided to rte_flow_create.
 * @param[out] num_of_actions
 *   Number of actions allocated.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_tunnel_decap_set(uint16_t port_id,
			  struct rte_flow_tunnel *tunnel,
			  struct rte_flow_action **actions,
			  uint32_t *num_of_actions,
			  struct rte_flow_error *error);

/**
 * Allocate an array of items to be used in rte_flow_create, to implement
 * tunnel-match for the given tunnel.
 * Sample usage:
 *   pattern tunnel-match(tunnel properties) / outer-header-matches /
 *           inner-header-matches / end
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] tunnel
 *   Tunnel properties.
 * @param[out] items
 *   Array of items to be allocated by the PMD. This array should be
 *   concatenated with the items array provided to rte_flow_create.
 * @param[out] num_of_items
 *   Number of items allocated.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_tunnel_match(uint16_t port_id,
		      struct rte_flow_tunnel *tunnel,
		      struct rte_flow_item **items,
		      uint32_t *num_of_items,
		      struct rte_flow_error *error);

/**
 * On reception of a mbuf from HW, a call to rte_flow_get_restore_info() may be
 * required to retrieve some metadata.
 * This function returns the associated mbuf ol_flags.
 *
 * Note: the dynamic flag is registered during a call to
 * rte_eth_rx_metadata_negotiate() with RTE_ETH_RX_METADATA_TUNNEL_ID.
 *
 * @return
 *   The offload flag indicating rte_flow_get_restore_info() must be called.
 */
__rte_experimental
uint64_t
rte_flow_restore_info_dynflag(void);

/**
 * If a mbuf contains the rte_flow_restore_info_dynflag() flag in ol_flags,
 * populate the current packet processing state.
 *
 * One should negotiate tunnel metadata delivery from the NIC to the HW.
 * @see rte_eth_rx_metadata_negotiate()
 * @see RTE_ETH_RX_METADATA_TUNNEL_ID
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] m
 *   Mbuf struct.
 * @param[out] info
 *   Restore information. Upon success contains the HW state.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_get_restore_info(uint16_t port_id,
			  struct rte_mbuf *m,
			  struct rte_flow_restore_info *info,
			  struct rte_flow_error *error);

/**
 * Release the action array as allocated by rte_flow_tunnel_decap_set.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] actions
 *   Array of actions to be released.
 * @param[in] num_of_actions
 *   Number of elements in actions array.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_tunnel_action_decap_release(uint16_t port_id,
				     struct rte_flow_action *actions,
				     uint32_t num_of_actions,
				     struct rte_flow_error *error);

/**
 * Release the item array as allocated by rte_flow_tunnel_match.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] items
 *   Array of items to be released.
 * @param[in] num_of_items
 *   Number of elements in item array.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_tunnel_item_release(uint16_t port_id,
			     struct rte_flow_item *items,
			     uint32_t num_of_items,
			     struct rte_flow_error *error);

/**
 * Get a proxy port to manage "transfer" flows.
 *
 * Managing "transfer" flows requires that the user communicate them
 * via a port which has the privilege to control the embedded switch.
 * For some vendors, all ports in a given switching domain have
 * this privilege. For other vendors, it's only one port.
 *
 * This API indicates such a privileged port (a "proxy")
 * for a given port in the same switching domain.
 *
 * @note
 *   If the PMD serving @p port_id doesn't have the corresponding method
 *   implemented, the API will return @p port_id via @p proxy_port_id.
 *
 * @param port_id
 *   Indicates the port to get a "proxy" for
 * @param[out] proxy_port_id
 *   Indicates the "proxy" port
 * @param[out] error
 *   If not NULL, allows the PMD to provide verbose report in case of error
 *
 * @return
 *   0 on success, a negative error code otherwise
 */
int
rte_flow_pick_transfer_proxy(uint16_t port_id, uint16_t *proxy_port_id,
			     struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create the flex item with specified configuration over
 * the Ethernet device.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] conf
 *   Item configuration.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   Non-NULL opaque pointer on success, NULL otherwise and rte_errno is set.
 */
__rte_experimental
struct rte_flow_item_flex_handle *
rte_flow_flex_item_create(uint16_t port_id,
			  const struct rte_flow_item_flex_conf *conf,
			  struct rte_flow_error *error);

/**
 * Release the flex item on the specified Ethernet device.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] handle
 *   Handle of the item existing on the specified device.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_flex_item_release(uint16_t port_id,
			   const struct rte_flow_item_flex_handle *handle,
			   struct rte_flow_error *error);

/**
 * Indicate all operations for a given flow rule will _strictly_
 * happen on the same queue (create/destroy/query/update).
 */
#define RTE_FLOW_PORT_FLAG_STRICT_QUEUE RTE_BIT32(0)

/**
 * Indicate all steering objects should be created on contexts
 * of the host port, providing indirect object sharing between
 * ports.
 */
#define RTE_FLOW_PORT_FLAG_SHARE_INDIRECT RTE_BIT32(1)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Information about flow engine resources.
 * The zero value means a resource is not supported.
 */
struct rte_flow_port_info {
	/**
	 * Maximum number of queues for asynchronous operations.
	 */
	uint32_t max_nb_queues;
	/**
	 * Maximum number of counters.
	 * @see RTE_FLOW_ACTION_TYPE_COUNT
	 */
	uint32_t max_nb_counters;
	/**
	 * Maximum number of aging objects.
	 * @see RTE_FLOW_ACTION_TYPE_AGE
	 */
	uint32_t max_nb_aging_objects;
	/**
	 * Maximum number traffic meters.
	 * @see RTE_FLOW_ACTION_TYPE_METER
	 */
	uint32_t max_nb_meters;
	/**
	 * Maximum number connection trackings.
	 * @see RTE_FLOW_ACTION_TYPE_CONNTRACK
	 */
	uint32_t max_nb_conn_tracks;
	/**
	 * Maximum number of quota actions.
	 * @see RTE_FLOW_ACTION_TYPE_QUOTA
	 */
	uint32_t max_nb_quotas;
	/**
	 * Port supported flags (RTE_FLOW_PORT_FLAG_*).
	 */
	uint32_t supported_flags;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Information about flow engine asynchronous queues.
 * The value only valid if @p port_attr.max_nb_queues is not zero.
 */
struct rte_flow_queue_info {
	/**
	 * Maximum number of operations a queue can hold.
	 */
	uint32_t max_size;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get information about flow engine resources.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[out] port_info
 *   A pointer to a structure of type *rte_flow_port_info*
 *   to be filled with the resources information of the port.
 * @param[out] queue_info
 *   A pointer to a structure of type *rte_flow_queue_info*
 *   to be filled with the asynchronous queues information.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_info_get(uint16_t port_id,
		  struct rte_flow_port_info *port_info,
		  struct rte_flow_queue_info *queue_info,
		  struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Flow engine resources settings.
 * The zero value means on demand resource allocations only.
 */
struct rte_flow_port_attr {
	/**
	 * Number of counters to configure.
	 * @see RTE_FLOW_ACTION_TYPE_COUNT
	 */
	uint32_t nb_counters;
	/**
	 * Number of aging objects to configure.
	 * @see RTE_FLOW_ACTION_TYPE_AGE
	 */
	uint32_t nb_aging_objects;
	/**
	 * Number of traffic meters to configure.
	 * @see RTE_FLOW_ACTION_TYPE_METER
	 */
	uint32_t nb_meters;
	/**
	 * Number of connection trackings to configure.
	 * @see RTE_FLOW_ACTION_TYPE_CONNTRACK
	 */
	uint32_t nb_conn_tracks;
	/**
	 * Port to base shared objects on.
	 */
	uint16_t host_port_id;
	/**
	 * Maximum number of quota actions.
	 * @see RTE_FLOW_ACTION_TYPE_QUOTA
	 */
	uint32_t nb_quotas;
	/**
	 * Port flags (RTE_FLOW_PORT_FLAG_*).
	 */
	uint32_t flags;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Flow engine asynchronous queues settings.
 * The value means default value picked by PMD.
 */
struct rte_flow_queue_attr {
	/**
	 * Number of flow rule operations a queue can hold.
	 */
	uint32_t size;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Configure the port's flow API engine.
 *
 * This API can only be invoked before the application
 * starts using the rest of the flow library functions.
 *
 * The API can be invoked multiple times to change the settings.
 * The port, however, may reject changes and keep the old config.
 *
 * Parameters in configuration attributes must not exceed
 * numbers of resources returned by the rte_flow_info_get API.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] port_attr
 *   Port configuration attributes.
 * @param[in] nb_queue
 *   Number of flow queues to be configured.
 * @param[in] queue_attr
 *   Array that holds attributes for each flow queue.
 *   Number of elements is set in @p port_attr.nb_queues.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_configure(uint16_t port_id,
		   const struct rte_flow_port_attr *port_attr,
		   uint16_t nb_queue,
		   const struct rte_flow_queue_attr *queue_attr[],
		   struct rte_flow_error *error);

/**
 * Opaque type returned after successful creation of pattern template.
 * This handle can be used to manage the created pattern template.
 */
struct rte_flow_pattern_template;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Flow pattern template attributes.
 */
__extension__
struct rte_flow_pattern_template_attr {
	/**
	 * Relaxed matching policy.
	 * - If 1, matching is performed only on items with the mask member set
	 * and matching on protocol layers specified without any masks is skipped.
	 * - If 0, matching on protocol layers specified without any masks is done
	 * as well. This is the standard behaviour of Flow API now.
	 */
	uint32_t relaxed_matching:1;
	/**
	 * Flow direction for the pattern template.
	 * At least one direction must be specified.
	 */
	/** Pattern valid for rules applied to ingress traffic. */
	uint32_t ingress:1;
	/** Pattern valid for rules applied to egress traffic. */
	uint32_t egress:1;
	/** Pattern valid for rules applied to transfer traffic. */
	uint32_t transfer:1;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create flow pattern template.
 *
 * The pattern template defines common matching fields without values.
 * For example, matching on 5 tuple TCP flow, the template will be
 * eth(null) + IPv4(source + dest) + TCP(s_port + d_port),
 * while values for each rule will be set during the flow rule creation.
 * The number and order of items in the template must be the same
 * at the rule creation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] template_attr
 *   Pattern template attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 *   The spec member of an item is not used unless the end member is used.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   Handle on success, NULL otherwise and rte_errno is set.
 */
__rte_experimental
struct rte_flow_pattern_template *
rte_flow_pattern_template_create(uint16_t port_id,
		const struct rte_flow_pattern_template_attr *template_attr,
		const struct rte_flow_item pattern[],
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Destroy flow pattern template.
 *
 * This function may be called only when
 * there are no more tables referencing this template.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] pattern_template
 *   Handle of the template to be destroyed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_pattern_template_destroy(uint16_t port_id,
		struct rte_flow_pattern_template *pattern_template,
		struct rte_flow_error *error);

/**
 * Opaque type returned after successful creation of actions template.
 * This handle can be used to manage the created actions template.
 */
struct rte_flow_actions_template;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Flow actions template attributes.
 */
__extension__
struct rte_flow_actions_template_attr {
	/**
	 * Flow direction for the actions template.
	 * At least one direction must be specified.
	 */
	/** Action valid for rules applied to ingress traffic. */
	uint32_t ingress:1;
	/** Action valid for rules applied to egress traffic. */
	uint32_t egress:1;
	/** Action valid for rules applied to transfer traffic. */
	uint32_t transfer:1;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create flow actions template.
 *
 * The actions template holds a list of action types without values.
 * For example, the template to change TCP ports is TCP(s_port + d_port),
 * while values for each rule will be set during the flow rule creation.
 * The number and order of actions in the template must be the same
 * at the rule creation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] template_attr
 *   Template attributes.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 *   The spec member is only used if @p masks spec is non-zero.
 * @param[in] masks
 *   List of actions that marks which of the action's member is constant.
 *   A mask has the same format as the corresponding action.
 *   If the action field in @p masks is not 0,
 *   the corresponding value in an action from @p actions will be the part
 *   of the template and used in all flow rules.
 *   The order of actions in @p masks is the same as in @p actions.
 *   In case of indirect actions present in @p actions,
 *   the actual action type should be present in @p mask.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   Handle on success, NULL otherwise and rte_errno is set.
 */
__rte_experimental
struct rte_flow_actions_template *
rte_flow_actions_template_create(uint16_t port_id,
		const struct rte_flow_actions_template_attr *template_attr,
		const struct rte_flow_action actions[],
		const struct rte_flow_action masks[],
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Destroy flow actions template.
 *
 * This function may be called only when
 * there are no more tables referencing this template.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] actions_template
 *   Handle to the template to be destroyed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_actions_template_destroy(uint16_t port_id,
		struct rte_flow_actions_template *actions_template,
		struct rte_flow_error *error);

/**
 * Opaque type returned after successful creation of a template table.
 * This handle can be used to manage the created template table.
 */
struct rte_flow_template_table;

/**@{@name Flags for template table attribute.
 * Each bit is an optional hint for table specialization,
 * offering a potential optimization at driver layer.
 * The driver can ignore the hints silently.
 * The hints do not replace any matching criteria.
 */
/**
 * Specialize table for transfer flows which come only from wire.
 * It allows PMD not to allocate resources for non-wire originated traffic.
 * This bit is not a matching criteria, just an optimization hint.
 * Flow rules which match non-wire originated traffic will be missed
 * if the hint is supported.
 */
#define RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG RTE_BIT32(0)
/**
 * Specialize table for transfer flows which come only from vport (e.g. VF, SF).
 * It allows PMD not to allocate resources for non-vport originated traffic.
 * This bit is not a matching criteria, just an optimization hint.
 * Flow rules which match non-vport originated traffic will be missed
 * if the hint is supported.
 */
#define RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG RTE_BIT32(1)
/**@}*/

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Template table flow rules insertion type.
 */
enum rte_flow_table_insertion_type {
	/**
	 * Pattern-based insertion.
	 */
	RTE_FLOW_TABLE_INSERTION_TYPE_PATTERN,
	/**
	 * Index-based insertion.
	 */
	RTE_FLOW_TABLE_INSERTION_TYPE_INDEX,
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Template table hash index calculation function.
 */
enum rte_flow_table_hash_func {
	/**
	 * Default hash calculation.
	 */
	RTE_FLOW_TABLE_HASH_FUNC_DEFAULT,
	/**
	 * Linear hash calculation.
	 */
	RTE_FLOW_TABLE_HASH_FUNC_LINEAR,
	/**
	 * 32-bit checksum hash calculation.
	 */
	RTE_FLOW_TABLE_HASH_FUNC_CRC32,
	/**
	 * 16-bit checksum hash calculation.
	 */
	RTE_FLOW_TABLE_HASH_FUNC_CRC16,
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Table attributes.
 */
struct rte_flow_template_table_attr {
	/**
	 * Flow attributes to be used in each rule generated from this table.
	 */
	struct rte_flow_attr flow_attr;
	/**
	 * Maximum number of flow rules that this table holds.
	 */
	uint32_t nb_flows;
	/**
	 * Optional hint flags for driver optimization.
	 * The effect may vary in the different drivers.
	 * The functionality must not rely on the hints.
	 * Value is composed with RTE_FLOW_TABLE_SPECIALIZE_* based on application
	 * design choices.
	 * Misused hints may mislead the driver, it may result in an undefined behavior.
	 */
	uint32_t specialize;
	/**
	 * Insertion type for flow rules.
	 */
	enum rte_flow_table_insertion_type insertion_type;
	/**
	 * Hash calculation function for the packet matching.
	 */
	enum rte_flow_table_hash_func hash_func;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create flow template table.
 *
 * A template table consists of multiple pattern templates and actions
 * templates associated with a single set of rule attributes (group ID,
 * priority and traffic direction).
 *
 * Each rule is free to use any combination of pattern and actions templates
 * and specify particular values for items and actions it would like to change.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] table_attr
 *   Template table attributes.
 * @param[in] pattern_templates
 *   Array of pattern templates to be used in this table.
 * @param[in] nb_pattern_templates
 *   The number of pattern templates in the pattern_templates array.
 * @param[in] actions_templates
 *   Array of actions templates to be used in this table.
 * @param[in] nb_actions_templates
 *   The number of actions templates in the actions_templates array.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   Handle on success, NULL otherwise and rte_errno is set.
 */
__rte_experimental
struct rte_flow_template_table *
rte_flow_template_table_create(uint16_t port_id,
		const struct rte_flow_template_table_attr *table_attr,
		struct rte_flow_pattern_template *pattern_templates[],
		uint8_t nb_pattern_templates,
		struct rte_flow_actions_template *actions_templates[],
		uint8_t nb_actions_templates,
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Destroy flow template table.
 *
 * This function may be called only when
 * there are no more flow rules referencing this table.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] template_table
 *   Handle to the table to be destroyed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_template_table_destroy(uint16_t port_id,
		struct rte_flow_template_table *template_table,
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set group miss actions.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param group_id
 *   Identifier of a group to set miss actions for.
 * @param attr
 *   Group attributes.
 * @param actions
 *   List of group miss actions.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_group_set_miss_actions(uint16_t port_id,
				uint32_t group_id,
				const struct rte_flow_group_attr *attr,
				const struct rte_flow_action actions[],
				struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Asynchronous operation attributes.
 */
__extension__
struct rte_flow_op_attr {
	/**
	 * When set, the requested action will not be sent to the HW immediately.
	 * The application must call the rte_flow_queue_push to actually send it.
	 */
	uint32_t postpone:1;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue rule creation operation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue used to insert the rule.
 * @param[in] op_attr
 *   Rule creation operation attributes.
 * @param[in] template_table
 *   Template table to select templates from.
 * @param[in] pattern
 *   List of pattern items to be used.
 *   The list order should match the order in the pattern template.
 *   The spec is the only relevant member of the item that is being used.
 * @param[in] pattern_template_index
 *   Pattern template index in the table.
 * @param[in] actions
 *   List of actions to be used.
 *   The list order should match the order in the actions template.
 * @param[in] actions_template_index
 *   Actions template index in the table.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   Handle on success, NULL otherwise and rte_errno is set.
 *   The rule handle doesn't mean that the rule has been populated.
 *   Only completion result indicates that if there was success or failure.
 */
__rte_experimental
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
		      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue rule creation operation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue used to insert the rule.
 * @param[in] op_attr
 *   Rule creation operation attributes.
 * @param[in] template_table
 *   Template table to select templates from.
 * @param[in] rule_index
 *   Rule index in the table.
 * @param[in] actions
 *   List of actions to be used.
 *   The list order should match the order in the actions template.
 * @param[in] actions_template_index
 *   Actions template index in the table.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   Handle on success, NULL otherwise and rte_errno is set.
 *   The rule handle doesn't mean that the rule has been populated.
 *   Only completion result indicates that if there was success or failure.
 */
__rte_experimental
struct rte_flow *
rte_flow_async_create_by_index(uint16_t port_id,
			       uint32_t queue_id,
			       const struct rte_flow_op_attr *op_attr,
			       struct rte_flow_template_table *template_table,
			       uint32_t rule_index,
			       const struct rte_flow_action actions[],
			       uint8_t actions_template_index,
			       void *user_data,
			       struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue rule destruction operation.
 *
 * This function enqueues a destruction operation on the queue.
 * Application should assume that after calling this function
 * the rule handle is not valid anymore.
 * Completion indicates the full removal of the rule from the HW.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue which is used to destroy the rule.
 *   This must match the queue on which the rule was created.
 * @param[in] op_attr
 *   Rule destruction operation attributes.
 * @param[in] flow
 *   Flow handle to be destroyed.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_async_destroy(uint16_t port_id,
		       uint32_t queue_id,
		       const struct rte_flow_op_attr *op_attr,
		       struct rte_flow *flow,
		       void *user_data,
		       struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue rule update operation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue used to insert the rule.
 * @param[in] op_attr
 *   Rule creation operation attributes.
 * @param[in] flow
 *   Flow rule to be updated.
 * @param[in] actions
 *   List of actions to be used.
 *   The list order should match the order in the actions template.
 * @param[in] actions_template_index
 *   Actions template index in the table.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_async_actions_update(uint16_t port_id,
			      uint32_t queue_id,
			      const struct rte_flow_op_attr *op_attr,
			      struct rte_flow *flow,
			      const struct rte_flow_action actions[],
			      uint8_t actions_template_index,
			      void *user_data,
			      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Push all internally stored rules to the HW.
 * Postponed rules are rules that were inserted with the postpone flag set.
 * Can be used to notify the HW about batch of rules prepared by the SW to
 * reduce the number of communications between the HW and SW.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue to be pushed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_push(uint16_t port_id,
	      uint32_t queue_id,
	      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Asynchronous operation status.
 */
enum rte_flow_op_status {
	/**
	 * The operation was completed successfully.
	 */
	RTE_FLOW_OP_SUCCESS,
	/**
	 * The operation was not completed successfully.
	 */
	RTE_FLOW_OP_ERROR,
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Asynchronous operation result.
 */
__extension__
struct rte_flow_op_result {
	/**
	 * Returns the status of the operation that this completion signals.
	 */
	enum rte_flow_op_status status;
	/**
	 * The user data that will be returned on the completion events.
	 */
	void *user_data;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Pull a rte flow operation.
 * The application must invoke this function in order to complete
 * the flow rule offloading and to retrieve the flow rule operation status.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue which is used to pull the operation.
 * @param[out] res
 *   Array of results that will be set.
 * @param[in] n_res
 *   Maximum number of results that can be returned.
 *   This value is equal to the size of the res array.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   Number of results that were pulled,
 *   a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_pull(uint16_t port_id,
	      uint32_t queue_id,
	      struct rte_flow_op_result res[],
	      uint16_t n_res,
	      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue indirect action creation operation.
 * @see rte_flow_action_handle_create
 *
 * @param[in] port_id
 *   Port identifier of Ethernet device.
 * @param[in] queue_id
 *   Flow queue which is used to create the rule.
 * @param[in] op_attr
 *   Indirect action creation operation attributes.
 * @param[in] indir_action_conf
 *   Action configuration for the indirect action object creation.
 * @param[in] action
 *   Specific configuration of the indirect action object.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   A valid handle in case of success, NULL otherwise and rte_errno is set.
 */
__rte_experimental
struct rte_flow_action_handle *
rte_flow_async_action_handle_create(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		const struct rte_flow_indir_action_conf *indir_action_conf,
		const struct rte_flow_action *action,
		void *user_data,
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue indirect action destruction operation.
 * The destroy queue must be the same
 * as the queue on which the action was created.
 *
 * @param[in] port_id
 *   Port identifier of Ethernet device.
 * @param[in] queue_id
 *   Flow queue which is used to destroy the rule.
 * @param[in] op_attr
 *   Indirect action destruction operation attributes.
 * @param[in] action_handle
 *   Handle for the indirect action object to be destroyed.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_async_action_handle_destroy(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		struct rte_flow_action_handle *action_handle,
		void *user_data,
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue indirect action update operation.
 * @see rte_flow_action_handle_create
 *
 * @param[in] port_id
 *   Port identifier of Ethernet device.
 * @param[in] queue_id
 *   Flow queue which is used to update the rule.
 * @param[in] op_attr
 *   Indirect action update operation attributes.
 * @param[in] action_handle
 *   Handle for the indirect action object to be updated.
 * @param[in] update
 *   Update profile specification used to modify the action pointed by handle.
 *   *update* could be with the same type of the immediate action corresponding
 *   to the *handle* argument when creating, or a wrapper structure includes
 *   action configuration to be updated and bit fields to indicate the member
 *   of fields inside the action to update.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_async_action_handle_update(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		struct rte_flow_action_handle *action_handle,
		const void *update,
		void *user_data,
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue indirect action query operation.
 *
 * Retrieve action-specific data such as counters.
 * Data is gathered by special action which may be present/referenced in
 * more than one flow rule definition.
 * Data will be available only when completion event returns.
 *
 * @see rte_flow_async_action_handle_query
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] queue_id
 *   Flow queue which is used to query the action.
 * @param[in] op_attr
 *   Indirect action update operation attributes.
 * @param[in] action_handle
 *   Handle for the action object to query.
 * @param[in, out] data
 *   Pointer to storage for the associated query data type.
 *   The out data will be available only when completion event returns
 *   from rte_flow_pull.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_experimental
int
rte_flow_async_action_handle_query(uint16_t port_id,
		uint32_t queue_id,
		const struct rte_flow_op_attr *op_attr,
		const struct rte_flow_action_handle *action_handle,
		void *data,
		void *user_data,
		struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Query and update operational mode.
 *
 * @see rte_flow_action_handle_query_update()
 * @see rte_flow_async_action_handle_query_update()
 */
enum rte_flow_query_update_mode {
	RTE_FLOW_QU_QUERY_FIRST = 1,  /**< Query before update. */
	RTE_FLOW_QU_UPDATE_FIRST,     /**< Query after  update. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Query and/or update indirect flow action.
 * If both query and update not NULL, the function atomically
 * queries and updates indirect action. Query and update are carried in order
 * specified in the mode parameter.
 * If ether query or update is NULL, the function executes
 * complementing operation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param handle
 *   Handle for the indirect action object to be updated.
 * @param update
 *   If not NULL, update profile specification used to modify the action
 *   pointed by handle.
 * @param query
 *   If not NULL pointer to storage for the associated query data type.
 * @param mode
 *   Operational mode.
 * @param error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 * 0 on success, a negative errno value otherwise and rte_errno is set.
 * - (-ENODEV) if *port_id* invalid.
 * - (-ENOTSUP) if underlying device does not support this functionality.
 * - (-EINVAL) if *handle* or *mode* invalid or
 *             both *query* and *update* are NULL.
 */
__rte_experimental
int
rte_flow_action_handle_query_update(uint16_t port_id,
				    struct rte_flow_action_handle *handle,
				    const void *update, void *query,
				    enum rte_flow_query_update_mode mode,
				    struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue async indirect flow action query and/or update
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue which is used to update the rule.
 * @param attr
 *   Indirect action update operation attributes.
 * @param handle
 *   Handle for the indirect action object to be updated.
 * @param update
 *   If not NULL, update profile specification used to modify the action
 *   pointed by handle.
 * @param query
 *   If not NULL, pointer to storage for the associated query data type.
 *   Query result returned on async completion event.
 * @param mode
 *   Operational mode.
 * @param user_data
 *   The user data that will be returned on async completion event.
 * @param error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 * - (-ENODEV) if *port_id* invalid.
 * - (-ENOTSUP) if underlying device does not support this functionality.
 * - (-EINVAL) if *handle* or *mode* invalid or
 *             both *update* and *query* are NULL.
 */
__rte_experimental
int
rte_flow_async_action_handle_query_update(uint16_t port_id, uint32_t queue_id,
					  const struct rte_flow_op_attr *attr,
					  struct rte_flow_action_handle *handle,
					  const void *update, void *query,
					  enum rte_flow_query_update_mode mode,
					  void *user_data,
					  struct rte_flow_error *error);

struct rte_flow_action_list_handle;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Configure INDIRECT_LIST flow action.
 *
 * @see RTE_FLOW_ACTION_TYPE_INDIRECT_LIST
 */
struct rte_flow_action_indirect_list {
	/** Indirect action list handle */
	struct rte_flow_action_list_handle *handle;
	/**
	 * Flow mutable configuration array.
	 * NULL if the handle has no flow mutable configuration update.
	 * Otherwise, if the handle was created with list A1 / A2 .. An / END
	 * size of conf is n.
	 * conf[i] points to flow mutable update of Ai in the handle
	 * actions list or NULL if Ai has no update.
	 */
	const void **conf;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create an indirect flow action object from flow actions list.
 * The object is identified by a unique handle.
 * The handle has single state and configuration
 * across all the flow rules using it.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] conf
 *   Action configuration for the indirect action list creation.
 * @param[in] actions
 *   Specific configuration of the indirect action lists.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   A valid handle in case of success, NULL otherwise and rte_errno is set
 *   to one of the error codes defined:
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if underlying device does not support this functionality.
 *   - (-EIO) if underlying device is removed.
 *   - (-EINVAL) if *actions* list invalid.
 *   - (-ENOTSUP) if *action* list element valid but unsupported.
 */
__rte_experimental
struct rte_flow_action_list_handle *
rte_flow_action_list_handle_create(uint16_t port_id,
				   const
				   struct rte_flow_indir_action_conf *conf,
				   const struct rte_flow_action *actions,
				   struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Async function call to create an indirect flow action object
 * from flow actions list.
 * The object is identified by a unique handle.
 * The handle has single state and configuration
 * across all the flow rules using it.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] queue_id
 *   Flow queue which is used to update the rule.
 * @param[in] attr
 *   Indirect action update operation attributes.
 * @param[in] conf
 *   Action configuration for the indirect action list creation.
 * @param[in] actions
 *   Specific configuration of the indirect action list.
 * @param[in] user_data
 *   The user data that will be returned on async completion event.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   A valid handle in case of success, NULL otherwise and rte_errno is set
 *   to one of the error codes defined:
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if underlying device does not support this functionality.
 *   - (-EIO) if underlying device is removed.
 *   - (-EINVAL) if *actions* list invalid.
 *   - (-ENOTSUP) if *action* list element valid but unsupported.
 */
__rte_experimental
struct rte_flow_action_list_handle *
rte_flow_async_action_list_handle_create(uint16_t port_id, uint32_t queue_id,
					 const struct rte_flow_op_attr *attr,
					 const struct rte_flow_indir_action_conf *conf,
					 const struct rte_flow_action *actions,
					 void *user_data,
					 struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Destroy indirect actions list by handle.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] handle
 *   Handle for the indirect actions list to be destroyed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   - (0) if success.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if underlying device does not support this functionality.
 *   - (-EIO) if underlying device is removed.
 *   - (-ENOENT) if actions list pointed by *action* handle was not found.
 *   - (-EBUSY) if actions list pointed by *action* handle still used
 */
__rte_experimental
int
rte_flow_action_list_handle_destroy(uint16_t port_id,
				    struct rte_flow_action_list_handle *handle,
				    struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue indirect action list destruction operation.
 * The destroy queue must be the same
 * as the queue on which the action was created.
 *
 * @param[in] port_id
 *   Port identifier of Ethernet device.
 * @param[in] queue_id
 *   Flow queue which is used to destroy the rule.
 * @param[in] op_attr
 *   Indirect action destruction operation attributes.
 * @param[in] handle
 *   Handle for the indirect action object to be destroyed.
 * @param[in] user_data
 *   The user data that will be returned on the completion events.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   - (0) if success.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOSYS) if underlying device does not support this functionality.
 *   - (-EIO) if underlying device is removed.
 *   - (-ENOENT) if actions list pointed by *action* handle was not found.
 *   - (-EBUSY) if actions list pointed by *action* handle still used
 */
__rte_experimental
int
rte_flow_async_action_list_handle_destroy
		(uint16_t port_id, uint32_t queue_id,
		 const struct rte_flow_op_attr *op_attr,
		 struct rte_flow_action_list_handle *handle,
		 void *user_data, struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Query and/or update indirect flow actions list.
 * If both query and update not NULL, the function atomically
 * queries and updates indirect action. Query and update are carried in order
 * specified in the mode parameter.
 * If ether query or update is NULL, the function executes
 * complementing operation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param handle
 *   Handle for the indirect actions list object to be updated.
 * @param update
 *   If the action list handle was created from n actions A1 / A2 ... An / END
 *   non-NULL update parameter is an array [U1, U2, ... Un] where Ui points to
 *   Ai update context or NULL if Ai should not be updated.
 * @param query
 *   If the action list handle was created from n actions A1 / A2 ... An / END
 *   non-NULL query parameter is an array [Q1, Q2, ... Qn] where Qi points to
 *   Ai query context or NULL if Ai should not be queried.
 * @param mode
 *   Operational mode.
 * @param error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   - (0) if success.
 * - (-ENODEV) if *port_id* invalid.
 * - (-ENOTSUP) if underlying device does not support this functionality.
 * - (-EINVAL) if *handle* or *mode* invalid or
 *             both *query* and *update* are NULL.
 */
__rte_experimental
int
rte_flow_action_list_handle_query_update(uint16_t port_id,
					 const struct rte_flow_action_list_handle *handle,
					 const void **update, void **query,
					 enum rte_flow_query_update_mode mode,
					 struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue async indirect flow actions list query and/or update
 * If both query and update not NULL, the function atomically
 * queries and updates indirect action. Query and update are carried in order
 * specified in the mode parameter.
 * If ether query or update is NULL, the function executes
 * complementing operation.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param queue_id
 *   Flow queue which is used to update the rule.
 * @param attr
 *   Indirect action update operation attributes.
 * @param handle
 *   Handle for the indirect actions list object to be updated.
 * @param update
 *   If the action list handle was created from n actions A1 / A2 ... An / END
 *   non-NULL update parameter is an array [U1, U2, ... Un] where Ui points to
 *   Ai update context or NULL if Ai should not be updated.
 * @param query
 *   If the action list handle was created from n actions A1 / A2 ... An / END
 *   non-NULL query parameter is an array [Q1, Q2, ... Qn] where Qi points to
 *   Ai query context or NULL if Ai should not be queried.
 *   Query result returned on async completion event.
 * @param mode
 *   Operational mode.
 * @param user_data
 *   The user data that will be returned on async completion event.
 * @param error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   - (0) if success.
 * - (-ENODEV) if *port_id* invalid.
 * - (-ENOTSUP) if underlying device does not support this functionality.
 * - (-EINVAL) if *handle* or *mode* invalid or
 *             both *update* and *query* are NULL.
 */
__rte_experimental
int
rte_flow_async_action_list_handle_query_update(uint16_t port_id, uint32_t queue_id,
					  const struct rte_flow_op_attr *attr,
					  const struct rte_flow_action_list_handle *handle,
					  const void **update, void **query,
					  enum rte_flow_query_update_mode mode,
					  void *user_data,
					  struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Calculate the hash for a given pattern in a given table as
 * calculated by the HW.
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param table
 *   The table the SW wishes to simulate.
 * @param pattern
 *   The values to be used in the hash calculation.
 * @param pattern_template_index
 *   The pattern index in the table to be used for the calculation.
 * @param hash
 *   Used to return the calculated hash.
 * @param error
 *   Perform verbose error reporting if not NULL.
 *   PMDs initialize this structure in case of error only.
 *
 * @return
 *   - (0) if success.
 *   - (-ENODEV) if *port_id* invalid.
 *   - (-ENOTSUP) if underlying device does not support this functionality.
 */
__rte_experimental
int
rte_flow_calc_table_hash(uint16_t port_id, const struct rte_flow_template_table *table,
			 const struct rte_flow_item pattern[], uint8_t pattern_template_index,
			 uint32_t *hash, struct rte_flow_error *error);

#ifdef __cplusplus
}
#endif

#endif /* RTE_FLOW_H_ */
