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

#include <rte_arp.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_icmp.h>
#include <rte_ip.h>
#include <rte_sctp.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_byteorder.h>
#include <rte_esp.h>
#include <rte_higig.h>
#include <rte_ecpri.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * Flow rules can apply to inbound and/or outbound traffic (ingress/egress).
 *
 * Several pattern items and actions are valid and can be used in both
 * directions. Those valid for only one direction are described as such.
 *
 * At least one direction must be specified.
 *
 * Specifying both directions at once for a given rule is not recommended
 * but may be valid in a few cases (e.g. shared counter).
 */
struct rte_flow_attr {
	uint32_t group; /**< Priority group. */
	uint32_t priority; /**< Rule priority level within group. */
	uint32_t ingress:1; /**< Rule applies to ingress traffic. */
	uint32_t egress:1; /**< Rule applies to egress traffic. */
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
	 * It complements the behavior of some pattern items such as
	 * RTE_FLOW_ITEM_TYPE_PHY_PORT and is meaningless without them.
	 *
	 * When transferring flow rules, ingress and egress attributes keep
	 * their original meaning, as if processing traffic emitted or
	 * received by the application.
	 */
	uint32_t transfer:1;
	uint32_t reserved:29; /**< Reserved, must be zero. */
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
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress)
	 * the physical function of the current device.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_PF,

	/**
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress) a
	 * given virtual function of the current device.
	 *
	 * See struct rte_flow_item_vf.
	 */
	RTE_FLOW_ITEM_TYPE_VF,

	/**
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress) a
	 * physical port of the underlying device.
	 *
	 * See struct rte_flow_item_phy_port.
	 */
	RTE_FLOW_ITEM_TYPE_PHY_PORT,

	/**
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
};

/**
 *
 * RTE_FLOW_ITEM_TYPE_HIGIG2
 * Matches higig2 header
 */
RTE_STD_C11
struct rte_flow_item_higig2_hdr {
	struct rte_higig2_hdr hdr;
};

/** Default mask for RTE_FLOW_ITEM_TYPE_HIGIG2. */
#ifndef __cplusplus
static const struct rte_flow_item_higig2_hdr rte_flow_item_higig2_hdr_mask = {
	.hdr = {
		.ppt1 = {
			.classification = 0xffff,
			.vid = 0xfff,
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
 * RTE_FLOW_ITEM_TYPE_VF
 *
 * Matches traffic originating from (ingress) or going to (egress) a given
 * virtual function of the current device.
 *
 * If supported, should work even if the virtual function is not managed by
 * the application and thus not associated with a DPDK port ID.
 *
 * Note this pattern item does not match VF representors traffic which, as
 * separate entities, should be addressed through their own DPDK port IDs.
 *
 * - Can be specified multiple times to match traffic addressed to several
 *   VF IDs.
 * - Can be combined with a PF item to match both PF and VF traffic.
 *
 * A zeroed mask can be used to match any VF ID.
 */
struct rte_flow_item_vf {
	uint32_t id; /**< VF ID. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_VF. */
#ifndef __cplusplus
static const struct rte_flow_item_vf rte_flow_item_vf_mask = {
	.id = 0x00000000,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_PHY_PORT
 *
 * Matches traffic originating from (ingress) or going to (egress) a
 * physical port of the underlying device.
 *
 * The first PHY_PORT item overrides the physical port normally associated
 * with the specified DPDK input port (port_id). This item can be provided
 * several times to match additional physical ports.
 *
 * Note that physical ports are not necessarily tied to DPDK input ports
 * (port_id) when those are not under DPDK control. Possible values are
 * specific to each device, they are not necessarily indexed from zero and
 * may not be contiguous.
 *
 * As a device property, the list of allowed values as well as the value
 * associated with a port_id should be retrieved by other means.
 *
 * A zeroed mask can be used to match any port index.
 */
struct rte_flow_item_phy_port {
	uint32_t index; /**< Physical port index. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_PHY_PORT. */
#ifndef __cplusplus
static const struct rte_flow_item_phy_port rte_flow_item_phy_port_mask = {
	.index = 0x00000000,
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_PORT_ID
 *
 * Matches traffic originating from (ingress) or going to (egress) a given
 * DPDK port ID.
 *
 * Normally only supported if the port ID in question is known by the
 * underlying PMD and related to the device the flow rule is created
 * against.
 *
 * This must not be confused with @p PHY_PORT which refers to the physical
 * port of a device, whereas @p PORT_ID refers to a struct rte_eth_dev
 * object on the application side (also known as "port representor"
 * depending on the kind of underlying device).
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
 * The @p type field either stands for "EtherType" or "TPID" when followed
 * by so-called layer 2.5 pattern items such as RTE_FLOW_ITEM_TYPE_VLAN. In
 * the latter case, @p type refers to that of the outer header, with the
 * inner EtherType/TPID provided by the subsequent pattern item. This is the
 * same order as on the wire.
 * If the @p type field contains a TPID value, then only tagged packets with the
 * specified TPID will match the pattern.
 * The field @p has_vlan can be used to match any type of tagged packets,
 * instead of using the @p type field.
 * If the @p type and @p has_vlan fields are not specified, then both tagged
 * and untagged packets will match the pattern.
 */
struct rte_flow_item_eth {
	struct rte_ether_addr dst; /**< Destination MAC. */
	struct rte_ether_addr src; /**< Source MAC. */
	rte_be16_t type; /**< EtherType or TPID. */
	uint32_t has_vlan:1; /**< Packet header contains at least one VLAN. */
	uint32_t reserved:31; /**< Reserved, must be zero. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_ETH. */
#ifndef __cplusplus
static const struct rte_flow_item_eth rte_flow_item_eth_mask = {
	.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.type = RTE_BE16(0x0000),
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
 * instead of using the @p inner_type field.
 * If the @p inner_type and @p has_more_vlan fields are not specified,
 * then any tagged packets will match the pattern.
 */
struct rte_flow_item_vlan {
	rte_be16_t tci; /**< Tag control information. */
	rte_be16_t inner_type; /**< Inner EtherType or TPID. */
	uint32_t has_more_vlan:1;
	/**< Packet header contains at least one more VLAN, after this VLAN. */
	uint32_t reserved:31; /**< Reserved, must be zero. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_VLAN. */
#ifndef __cplusplus
static const struct rte_flow_item_vlan rte_flow_item_vlan_mask = {
	.tci = RTE_BE16(0x0fff),
	.inner_type = RTE_BE16(0x0000),
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
	uint32_t has_hop_ext:1;
	/**< Header contains Hop-by-Hop Options extension header. */
	uint32_t has_route_ext:1;
	/**< Header contains Routing extension header. */
	uint32_t has_frag_ext:1;
	/**< Header contains Fragment extension header. */
	uint32_t has_auth_ext:1;
	/**< Header contains Authentication extension header. */
	uint32_t has_esp_ext:1;
	/**< Header contains Encapsulation Security Payload extension header. */
	uint32_t has_dest_ext:1;
	/**< Header contains Destination Options extension header. */
	uint32_t has_mobil_ext:1;
	/**< Header contains Mobility extension header. */
	uint32_t has_hip_ext:1;
	/**< Header contains Host Identity Protocol extension header. */
	uint32_t has_shim6_ext:1;
	/**< Header contains Shim6 Protocol extension header. */
	uint32_t reserved:23;
	/**< Reserved for future extension headers, must be zero. */
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
	uint8_t flags; /**< Normally 0x08 (I flag). */
	uint8_t rsvd0[3]; /**< Reserved, normally 0x000000. */
	uint8_t vni[3]; /**< VXLAN identifier. */
	uint8_t rsvd1; /**< Reserved, normally 0x00. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_VXLAN. */
#ifndef __cplusplus
static const struct rte_flow_item_vxlan rte_flow_item_vxlan_mask = {
	.vni = "\xff\xff\xff",
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

/** Default mask for RTE_FLOW_ITEM_TYPE_GTP. */
#ifndef __cplusplus
static const struct rte_flow_item_gtp rte_flow_item_gtp_mask = {
	.teid = RTE_BE32(0xffffffff),
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
	uint8_t flags; /**< Normally 0x0c (I and P flags). */
	uint8_t rsvd0[2]; /**< Reserved, normally 0x0000. */
	uint8_t protocol; /**< Protocol type. */
	uint8_t vni[3]; /**< VXLAN identifier. */
	uint8_t rsvd1; /**< Reserved, normally 0x00. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_VXLAN_GPE. */
#ifndef __cplusplus
static const struct rte_flow_item_vxlan_gpe rte_flow_item_vxlan_gpe_mask = {
	.vni = "\xff\xff\xff",
};
#endif

/**
 * RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4
 *
 * Matches an ARP header for Ethernet/IPv4.
 */
struct rte_flow_item_arp_eth_ipv4 {
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

/** Default mask for RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4. */
#ifndef __cplusplus
static const struct rte_flow_item_arp_eth_ipv4
rte_flow_item_arp_eth_ipv4_mask = {
	.sha.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.spa = RTE_BE32(0xffffffff),
	.tha.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.tpa = RTE_BE32(0xffffffff),
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
 * either by mbuf dynamic metadata field with PKT_TX_DYNF_METADATA flag or
 * RTE_FLOW_ACTION_TYPE_SET_META. On ingress, RTE_FLOW_ACTION_TYPE_SET_META
 * sets metadata for a packet and the metadata will be reported via mbuf
 * metadata dynamic field with PKT_RX_DYNF_METADATA flag. The dynamic mbuf
 * field must be registered in advance by rte_flow_dynf_metadata_register().
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
	uint8_t pdu_type; /**< PDU type. */
	uint8_t qfi; /**< PPP, RQI, QoS flow identifier. */
};

/** Default mask for RTE_FLOW_ITEM_TYPE_GTP_PSC. */
#ifndef __cplusplus
static const struct rte_flow_item_gtp_psc
rte_flow_item_gtp_psc_mask = {
	.qfi = 0xff,
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
 *
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
 *
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
 *
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
	 * Attaches an integer value to packets and sets PKT_RX_FDIR and
	 * PKT_RX_FDIR_ID mbuf flags.
	 *
	 * See struct rte_flow_action_mark.
	 */
	RTE_FLOW_ACTION_TYPE_MARK,

	/**
	 * Flags packets. Similar to MARK without a specific value; only
	 * sets the PKT_RX_FDIR mbuf flag.
	 *
	 * No associated configuration structure.
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
	 * rte_flow_shared_action_query() if the action provided via handle,
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
	 * Directs matching traffic to the physical function (PF) of the
	 * current device.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_PF,

	/**
	 * Directs matching traffic to a given virtual function of the
	 * current device.
	 *
	 * See struct rte_flow_action_vf.
	 */
	RTE_FLOW_ACTION_TYPE_VF,

	/**
	 * Directs packets to a given physical port index of the underlying
	 * device.
	 *
	 * See struct rte_flow_action_phy_port.
	 */
	RTE_FLOW_ACTION_TYPE_PHY_PORT,

	/**
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
	 * Implements OFPAT_SET_MPLS_TTL ("MPLS TTL") as defined by the
	 * OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_set_mpls_ttl.
	 */
	RTE_FLOW_ACTION_TYPE_OF_SET_MPLS_TTL,

	/**
	 * Implements OFPAT_DEC_MPLS_TTL ("decrement MPLS TTL") as defined
	 * by the OpenFlow Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_DEC_MPLS_TTL,

	/**
	 * Implements OFPAT_SET_NW_TTL ("IP TTL") as defined by the OpenFlow
	 * Switch Specification.
	 *
	 * See struct rte_flow_action_of_set_nw_ttl.
	 */
	RTE_FLOW_ACTION_TYPE_OF_SET_NW_TTL,

	/**
	 * Implements OFPAT_DEC_NW_TTL ("decrement IP TTL") as defined by
	 * the OpenFlow Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_DEC_NW_TTL,

	/**
	 * Implements OFPAT_COPY_TTL_OUT ("copy TTL "outwards" -- from
	 * next-to-outermost to outermost") as defined by the OpenFlow
	 * Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_COPY_TTL_OUT,

	/**
	 * Implements OFPAT_COPY_TTL_IN ("copy TTL "inwards" -- from
	 * outermost to next-to-outermost") as defined by the OpenFlow
	 * Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_COPY_TTL_IN,

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
	 * Implements OFPAT_SET_VLAN_VID ("set the 802.1q VLAN id") as
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
	 * Modify IPv4 source address in the outermost IPv4 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv4.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC,

	/**
	 * Modify IPv4 destination address in the outermost IPv4 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv4.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_DST,

	/**
	 * Modify IPv6 source address in the outermost IPv6 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv6.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC,

	/**
	 * Modify IPv6 destination address in the outermost IPv6 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv6.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_DST,

	/**
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
	 * Decrease TTL value directly
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_DEC_TTL,

	/**
	 * Set TTL value
	 *
	 * See struct rte_flow_action_set_ttl
	 */
	RTE_FLOW_ACTION_TYPE_SET_TTL,

	/**
	 * Set source MAC address from matched flow.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_mac.
	 */
	RTE_FLOW_ACTION_TYPE_SET_MAC_SRC,

	/**
	 * Set destination MAC address from matched flow.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_mac.
	 */
	RTE_FLOW_ACTION_TYPE_SET_MAC_DST,

	/**
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
	 * Set Tag.
	 *
	 * Tag is for internal flow usage only and
	 * is not delivered to the application.
	 *
	 * See struct rte_flow_action_set_tag.
	 */
	RTE_FLOW_ACTION_TYPE_SET_TAG,

	/**
	 * Set metadata on ingress or egress path.
	 *
	 * See struct rte_flow_action_set_meta.
	 */
	RTE_FLOW_ACTION_TYPE_SET_META,

	/**
	 * Modify IPv4 DSCP in the outermost IP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_dscp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP,

	/**
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
	 * See function rte_flow_get_aged_flows
	 * see enum RTE_ETH_EVENT_FLOW_AGED
	 * See struct rte_flow_query_age
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
	 * Describe action shared across multiple flow rules.
	 *
	 * Allow multiple rules reference the same action by handle (see
	 * struct rte_flow_shared_action).
	 */
	RTE_FLOW_ACTION_TYPE_SHARED,
};

/**
 * RTE_FLOW_ACTION_TYPE_MARK
 *
 * Attaches an integer value to packets and sets PKT_RX_FDIR and
 * PKT_RX_FDIR_ID mbuf flags.
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
 * The flow context and the flow handle will be reported by the
 * rte_flow_get_aged_flows API.
 */
struct rte_flow_action_age {
	uint32_t timeout:24; /**< Time in seconds. */
	uint32_t reserved:8; /**< Reserved, must be zero. */
	void *context;
		/**< The user flow context, NULL means the rte_flow pointer. */
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
	uint32_t sec_since_last_hit_valid:1;
	/**< sec_since_last_hit value is valid. */
	uint32_t sec_since_last_hit:24; /**< Seconds since last traffic hit. */
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
 * action must specify a unique id.
 *
 * Counters can be retrieved and reset through ``rte_flow_query()``, see
 * ``struct rte_flow_query_count``.
 *
 * @deprecated Shared attribute is deprecated, use generic
 * RTE_FLOW_ACTION_TYPE_SHARED action.
 *
 * The shared flag indicates whether the counter is unique to the flow rule the
 * action is specified with, or whether it is a shared counter.
 *
 * For a count action with the shared flag set, then then a global device
 * namespace is assumed for the counter id, so that any matched flow rules using
 * a count action with the same counter id on the same port will contribute to
 * that counter.
 *
 * For ports within the same switch domain then the counter id namespace extends
 * to all ports within that switch domain.
 */
struct rte_flow_action_count {
	/** @deprecated Share counter ID with other flow rules. */
	uint32_t shared:1;
	uint32_t reserved:31; /**< Reserved, must be zero. */
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
 * Hash function types.
 */
enum rte_eth_hash_function {
	RTE_ETH_HASH_FUNCTION_DEFAULT = 0,
	RTE_ETH_HASH_FUNCTION_TOEPLITZ, /**< Toeplitz */
	RTE_ETH_HASH_FUNCTION_SIMPLE_XOR, /**< Simple XOR */
	/**
	 * Symmetric Toeplitz: src, dst will be replaced by
	 * xor(src, dst). For the case with src/dst only,
	 * src or dst address will xor with zero pair.
	 */
	RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ,
	RTE_ETH_HASH_FUNCTION_MAX,
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
	uint64_t types; /**< Specific RSS hash types (see ETH_RSS_*). */
	uint32_t key_len; /**< Hash key length in bytes. */
	uint32_t queue_num; /**< Number of entries in @p queue. */
	const uint8_t *key; /**< Hash key. */
	const uint16_t *queue; /**< Queue indices to use. */
};

/**
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
 * RTE_FLOW_ACTION_TYPE_PHY_PORT
 *
 * Directs packets to a given physical port index of the underlying
 * device.
 *
 * @see RTE_FLOW_ITEM_TYPE_PHY_PORT
 */
struct rte_flow_action_phy_port {
	uint32_t original:1; /**< Use original port index if possible. */
	uint32_t reserved:31; /**< Reserved, must be zero. */
	uint32_t index; /**< Physical port index. */
};

/**
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
 * RTE_FLOW_ACTION_TYPE_OF_SET_MPLS_TTL
 *
 * Implements OFPAT_SET_MPLS_TTL ("MPLS TTL") as defined by the OpenFlow
 * Switch Specification.
 */
struct rte_flow_action_of_set_mpls_ttl {
	uint8_t mpls_ttl; /**< MPLS TTL. */
};

/**
 * RTE_FLOW_ACTION_TYPE_OF_SET_NW_TTL
 *
 * Implements OFPAT_SET_NW_TTL ("IP TTL") as defined by the OpenFlow Switch
 * Specification.
 */
struct rte_flow_action_of_set_nw_ttl {
	uint8_t nw_ttl; /**< IP TTL. */
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
 * Implements OFPAT_SET_VLAN_VID ("set the 802.1q VLAN id") as defined by
 * the OpenFlow Switch Specification.
 */
struct rte_flow_action_of_set_vlan_vid {
	rte_be16_t vlan_vid; /**< VLAN id. */
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
 *
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
 *
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
 * PKT_TX_DYNF_DATA flag on egress will be overridden by this action. On
 * ingress, the metadata will be carried by mbuf metadata dynamic field
 * with PKT_RX_DYNF_METADATA flag if set.  The dynamic mbuf field must be
 * registered in advance by rte_flow_dynf_metadata_register().
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
 * RTE_FLOW_ACTION_TYPE_SHARED
 *
 * Opaque type returned after successfully creating a shared action.
 *
 * This handle can be used to manage and query the related action:
 * - share it across multiple flow rules
 * - update action configuration
 * - query action data
 * - destroy action
 */
struct rte_flow_shared_action;

/* Mbuf dynamic field offset for metadata. */
extern int32_t rte_flow_dynf_metadata_offs;

/* Mbuf dynamic field flag mask for metadata. */
extern uint64_t rte_flow_dynf_metadata_mask;

/* Mbuf dynamic field pointer for metadata. */
#define RTE_FLOW_DYNF_METADATA(m) \
	RTE_MBUF_DYNFIELD((m), rte_flow_dynf_metadata_offs, uint32_t *)

/* Mbuf dynamic flags for metadata. */
#define PKT_RX_DYNF_METADATA (rte_flow_dynf_metadata_mask)
#define PKT_TX_DYNF_METADATA (rte_flow_dynf_metadata_mask)

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
	const struct rte_flow_action *actions;
		/**< sub-action list specific for the sampling hit cases. */
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
RTE_STD_C11
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
	 **/
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
	 **/
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
rte_flow_dev_dump(uint16_t port_id, FILE *file, struct rte_flow_error *error);

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
 * Specify shared action configuration
 */
struct rte_flow_shared_action_conf {
	/**
	 * Flow direction for shared action configuration.
	 *
	 * Shared action should be valid at least for one flow direction,
	 * otherwise it is invalid for both ingress and egress rules.
	 */
	uint32_t ingress:1;
	/**< Action valid for rules applied to ingress traffic. */
	uint32_t egress:1;
	/**< Action valid for rules applied to egress traffic. */

	/**
	 * When set to 1, indicates that the action is valid for
	 * transfer traffic; otherwise, for non-transfer traffic.
	 *
	 * See struct rte_flow_attr.
	 */
	uint32_t transfer:1;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create shared action for reuse in multiple flow rules.
 * The created shared action has single state and configuration
 * across all flow rules using it.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] conf
 *   Shared action configuration.
 * @param[in] action
 *   Action configuration for shared action creation.
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
struct rte_flow_shared_action *
rte_flow_shared_action_create(uint16_t port_id,
			      const struct rte_flow_shared_action_conf *conf,
			      const struct rte_flow_action *action,
			      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Destroy the shared action by handle.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] action
 *   Handle for the shared action to be destroyed.
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
rte_flow_shared_action_destroy(uint16_t port_id,
			       struct rte_flow_shared_action *action,
			       struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Update in-place the shared action configuration pointed by *action* handle
 * with the configuration provided as *update* argument.
 * The update of the shared action configuration effects all flow rules reusing
 * the action via handle.
 *
 * @param[in] port_id
 *    The port identifier of the Ethernet device.
 * @param[in] action
 *   Handle for the shared action to be updated.
 * @param[in] update
 *   Action specification used to modify the action pointed by handle.
 *   *update* should be of same type with the action pointed by the *action*
 *   handle argument, otherwise considered as invalid.
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
 *   - (-ENOENT) if action pointed by *ctx* was not found.
 *   rte_errno is also set.
 */
__rte_experimental
int
rte_flow_shared_action_update(uint16_t port_id,
			      struct rte_flow_shared_action *action,
			      const struct rte_flow_action *update,
			      struct rte_flow_error *error);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Query the shared action by handle.
 *
 * Retrieve action-specific data such as counters.
 * Data is gathered by special action which may be present/referenced in
 * more than one flow rule definition.
 *
 * \see RTE_FLOW_ACTION_TYPE_COUNT
 *
 * @param port_id
 *   Port identifier of Ethernet device.
 * @param[in] action
 *   Handle for the shared action to query.
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
rte_flow_shared_action_query(uint16_t port_id,
			     const struct rte_flow_shared_action *action,
			     void *data,
			     struct rte_flow_error *error);

/* Tunnel has a type and the key information. */
struct rte_flow_tunnel {
	/**
	 * Tunnel type, for example RTE_FLOW_ITEM_TYPE_VXLAN,
	 * RTE_FLOW_ITEM_TYPE_NVGRE etc.
	 */
	enum rte_flow_item_type	type;
	uint64_t tun_id; /**< Tunnel identification. */

	RTE_STD_C11
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
#define RTE_FLOW_RESTORE_INFO_TUNNEL  (1ULL << 0)

/**
 * Indicate that the packet has a non decapsulated tunnel header.
 */
#define RTE_FLOW_RESTORE_INFO_ENCAPSULATED  (1ULL << 1)

/**
 * Indicate that the packet has a group_id.
 */
#define RTE_FLOW_RESTORE_INFO_GROUP_ID  (1ULL << 2)

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
 * Populate the current packet processing state, if exists, for the given mbuf.
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
#ifdef __cplusplus
}
#endif

#endif /* RTE_FLOW_H_ */
