/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#include "ice_switch.h"
#include "ice_flex_type.h"
#include "ice_flow.h"

#define ICE_ETH_DA_OFFSET		0
#define ICE_ETH_ETHTYPE_OFFSET		12
#define ICE_ETH_VLAN_TCI_OFFSET		14
#define ICE_MAX_VLAN_ID			0xFFF
#define ICE_IPV4_NVGRE_PROTO_ID		0x002F
#define ICE_PPP_IPV6_PROTO_ID		0x0057
#define ICE_IPV6_ETHER_ID		0x86DD
#define ICE_TCP_PROTO_ID		0x06

/* Dummy ethernet header needed in the ice_aqc_sw_rules_elem
 * struct to configure any switch filter rules.
 * {DA (6 bytes), SA(6 bytes),
 * Ether type (2 bytes for header without VLAN tag) OR
 * VLAN tag (4 bytes for header with VLAN tag) }
 *
 * Word on Hardcoded values
 * byte 0 = 0x2: to identify it as locally administered DA MAC
 * byte 6 = 0x2: to identify it as locally administered SA MAC
 * byte 12 = 0x81 & byte 13 = 0x00:
 *	In case of VLAN filter first two bytes defines ether type (0x8100)
 *	and remaining two bytes are placeholder for programming a given VLAN ID
 *	In case of Ether type filter it is treated as header without VLAN tag
 *	and byte 12 and 13 is used to program a given Ether type instead
 */
static const u8 dummy_eth_header[DUMMY_ETH_HDR_LEN] = { 0x2, 0, 0, 0, 0, 0,
							0x2, 0, 0, 0, 0, 0,
							0x81, 0, 0, 0};

struct ice_dummy_pkt_offsets {
	enum ice_protocol_type type;
	u16 offset; /* ICE_PROTOCOL_LAST indicates end of list */
};

static const struct ice_dummy_pkt_offsets dummy_gre_tcp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_NVGRE,		34 },
	{ ICE_MAC_IL,		42 },
	{ ICE_IPV4_IL,		56 },
	{ ICE_TCP_IL,		76 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_gre_tcp_packet[] = {
	0x00, 0x00, 0x00, 0x00,	/* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x08, 0x00,		/* ICE_ETYPE_OL 12 */

	0x45, 0x00, 0x00, 0x3E,	/* ICE_IPV4_OFOS 14 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x2F, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x80, 0x00, 0x65, 0x58,	/* ICE_NVGRE 34 */
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00,	/* ICE_MAC_IL 42 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x14,	/* ICE_IPV4_IL 56 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x06, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00,	/* ICE_TCP_IL 76 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x02, 0x20, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static const struct ice_dummy_pkt_offsets dummy_gre_udp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_NVGRE,		34 },
	{ ICE_MAC_IL,		42 },
	{ ICE_IPV4_IL,		56 },
	{ ICE_UDP_ILOS,		76 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_gre_udp_packet[] = {
	0x00, 0x00, 0x00, 0x00,	/* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x08, 0x00,		/* ICE_ETYPE_OL 12 */

	0x45, 0x00, 0x00, 0x3E,	/* ICE_IPV4_OFOS 14 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x2F, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x80, 0x00, 0x65, 0x58,	/* ICE_NVGRE 34 */
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00,	/* ICE_MAC_IL 42 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x14,	/* ICE_IPV4_IL 56 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00,	/* ICE_UDP_ILOS 76 */
	0x00, 0x08, 0x00, 0x00,
};

static const struct ice_dummy_pkt_offsets dummy_udp_tun_tcp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_UDP_OF,		34 },
	{ ICE_VXLAN,		42 },
	{ ICE_GENEVE,		42 },
	{ ICE_VXLAN_GPE,	42 },
	{ ICE_MAC_IL,		50 },
	{ ICE_IPV4_IL,		64 },
	{ ICE_TCP_IL,		84 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_udp_tun_tcp_packet[] = {
	0x00, 0x00, 0x00, 0x00,  /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x08, 0x00,		/* ICE_ETYPE_OL 12 */

	0x45, 0x00, 0x00, 0x5a, /* ICE_IPV4_OFOS 14 */
	0x00, 0x01, 0x00, 0x00,
	0x40, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x12, 0xb5, /* ICE_UDP_OF 34 */
	0x00, 0x46, 0x00, 0x00,

	0x00, 0x00, 0x65, 0x58, /* ICE_VXLAN 42 */
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_IL 50 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x28, /* ICE_IPV4_IL 64 */
	0x00, 0x01, 0x00, 0x00,
	0x40, 0x06, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_TCP_IL 84 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x02, 0x20, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static const struct ice_dummy_pkt_offsets dummy_udp_tun_udp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_UDP_OF,		34 },
	{ ICE_VXLAN,		42 },
	{ ICE_GENEVE,		42 },
	{ ICE_VXLAN_GPE,	42 },
	{ ICE_MAC_IL,		50 },
	{ ICE_IPV4_IL,		64 },
	{ ICE_UDP_ILOS,		84 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_udp_tun_udp_packet[] = {
	0x00, 0x00, 0x00, 0x00,  /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x08, 0x00,		/* ICE_ETYPE_OL 12 */

	0x45, 0x00, 0x00, 0x4e, /* ICE_IPV4_OFOS 14 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x12, 0xb5, /* ICE_UDP_OF 34 */
	0x00, 0x3a, 0x00, 0x00,

	0x00, 0x00, 0x65, 0x58, /* ICE_VXLAN 42 */
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_IL 50 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x1c, /* ICE_IPV4_IL 64 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_UDP_ILOS 84 */
	0x00, 0x08, 0x00, 0x00,
};

/* offset info for MAC + IPv4 + UDP dummy packet */
static const struct ice_dummy_pkt_offsets dummy_udp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_UDP_ILOS,		34 },
	{ ICE_PROTOCOL_LAST,	0 },
};

/* Dummy packet for MAC + IPv4 + UDP */
static const u8 dummy_udp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x08, 0x00,		/* ICE_ETYPE_OL 12 */

	0x45, 0x00, 0x00, 0x1c, /* ICE_IPV4_OFOS 14 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_UDP_ILOS 34 */
	0x00, 0x08, 0x00, 0x00,

	0x00, 0x00,	/* 2 bytes for 4 byte alignment */
};

/* offset info for MAC + VLAN + IPv4 + UDP dummy packet */
static const struct ice_dummy_pkt_offsets dummy_vlan_udp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14 },
	{ ICE_IPV4_OFOS,	18 },
	{ ICE_UDP_ILOS,		38 },
	{ ICE_PROTOCOL_LAST,	0 },
};

/* C-tag (801.1Q), IPv4:UDP dummy packet */
static const u8 dummy_vlan_udp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x08, 0x00, /* ICE_VLAN_OFOS 14 */

	0x45, 0x00, 0x00, 0x1c, /* ICE_IPV4_OFOS 18 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_UDP_ILOS 38 */
	0x00, 0x08, 0x00, 0x00,

	0x00, 0x00,	/* 2 bytes for 4 byte alignment */
};

/* offset info for MAC + IPv4 + TCP dummy packet */
static const struct ice_dummy_pkt_offsets dummy_tcp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_TCP_IL,		34 },
	{ ICE_PROTOCOL_LAST,	0 },
};

/* Dummy packet for MAC + IPv4 + TCP */
static const u8 dummy_tcp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x08, 0x00,		/* ICE_ETYPE_OL 12 */

	0x45, 0x00, 0x00, 0x28, /* ICE_IPV4_OFOS 14 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x06, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_TCP_IL 34 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00,	/* 2 bytes for 4 byte alignment */
};

/* offset info for MAC + VLAN (C-tag, 802.1Q) + IPv4 + TCP dummy packet */
static const struct ice_dummy_pkt_offsets dummy_vlan_tcp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14 },
	{ ICE_IPV4_OFOS,	18 },
	{ ICE_TCP_IL,		38 },
	{ ICE_PROTOCOL_LAST,	0 },
};

/* C-tag (801.1Q), IPv4:TCP dummy packet */
static const u8 dummy_vlan_tcp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x08, 0x00, /* ICE_VLAN_OFOS 14 */

	0x45, 0x00, 0x00, 0x28, /* ICE_IPV4_OFOS 18 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x06, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_TCP_IL 38 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00,	/* 2 bytes for 4 byte alignment */
};

static const struct ice_dummy_pkt_offsets dummy_tcp_ipv6_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV6_OFOS,	14 },
	{ ICE_TCP_IL,		54 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_tcp_ipv6_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x86, 0xDD,		/* ICE_ETYPE_OL 12 */

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 40 */
	0x00, 0x14, 0x06, 0x00, /* Next header is TCP */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_TCP_IL 54 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, /* 2 bytes for 4 byte alignment */
};

/* C-tag (802.1Q): IPv6 + TCP */
static const struct ice_dummy_pkt_offsets
dummy_vlan_tcp_ipv6_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14 },
	{ ICE_IPV6_OFOS,	18 },
	{ ICE_TCP_IL,		58 },
	{ ICE_PROTOCOL_LAST,	0 },
};

/* C-tag (802.1Q), IPv6 + TCP dummy packet */
static const u8 dummy_vlan_tcp_ipv6_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x86, 0xDD, /* ICE_VLAN_OFOS 14 */

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 18 */
	0x00, 0x14, 0x06, 0x00, /* Next header is TCP */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_TCP_IL 58 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, /* 2 bytes for 4 byte alignment */
};

/* IPv6 + UDP */
static const struct ice_dummy_pkt_offsets dummy_udp_ipv6_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_IPV6_OFOS,	14 },
	{ ICE_UDP_ILOS,		54 },
	{ ICE_PROTOCOL_LAST,	0 },
};

/* IPv6 + UDP dummy packet */
static const u8 dummy_udp_ipv6_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x86, 0xDD,		/* ICE_ETYPE_OL 12 */

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 40 */
	0x00, 0x10, 0x11, 0x00, /* Next header UDP */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_UDP_ILOS 54 */
	0x00, 0x10, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* needed for ESP packets */
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, /* 2 bytes for 4 byte alignment */
};

/* C-tag (802.1Q): IPv6 + UDP */
static const struct ice_dummy_pkt_offsets
dummy_vlan_udp_ipv6_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14 },
	{ ICE_IPV6_OFOS,	18 },
	{ ICE_UDP_ILOS,		58 },
	{ ICE_PROTOCOL_LAST,	0 },
};

/* C-tag (802.1Q), IPv6 + UDP dummy packet */
static const u8 dummy_vlan_udp_ipv6_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x86, 0xDD, /* ICE_VLAN_OFOS 14 */

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 18 */
	0x00, 0x08, 0x11, 0x00, /* Next header UDP */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_UDP_ILOS 58 */
	0x00, 0x08, 0x00, 0x00,

	0x00, 0x00, /* 2 bytes for 4 byte alignment */
};

static const struct ice_dummy_pkt_offsets dummy_udp_gtp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_UDP_OF,		34 },
	{ ICE_GTP,		42 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_udp_gtp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x30, /* ICE_IPV4_OFOS 14 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x08, 0x68, /* ICE_UDP_OF 34 */
	0x00, 0x1c, 0x00, 0x00,

	0x34, 0xff, 0x00, 0x0c, /* ICE_GTP 42 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x85,

	0x02, 0x00, 0x00, 0x00, /* PDU Session extension header */
	0x00, 0x00, 0x00, 0x00,
};

static const struct ice_dummy_pkt_offsets dummy_pppoe_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14},
	{ ICE_PPPOE,		18 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const struct ice_dummy_pkt_offsets dummy_pppoe_packet_ipv4_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14},
	{ ICE_PPPOE,		18 },
	{ ICE_IPV4_OFOS,	26 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_pppoe_ipv4_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x88, 0x64, /* ICE_VLAN_OFOS 14 */

	0x11, 0x00, 0x00, 0x00, /* ICE_PPPOE 18 */
	0x00, 0x16,

	0x00, 0x21,		/* PPP Link Layer 24 */

	0x45, 0x00, 0x00, 0x14, /* ICE_IPV4_IL 26 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const
struct ice_dummy_pkt_offsets dummy_pppoe_ipv4_tcp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14},
	{ ICE_PPPOE,		18 },
	{ ICE_IPV4_OFOS,	26 },
	{ ICE_TCP_IL,		46 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_pppoe_ipv4_tcp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x88, 0x64, /* ICE_VLAN_OFOS 14 */

	0x11, 0x00, 0x00, 0x00, /* ICE_PPPOE 18 */
	0x00, 0x16,

	0x00, 0x21,		/* PPP Link Layer 24 */

	0x45, 0x00, 0x00, 0x28, /* ICE_IPV4_OFOS 26 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x06, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_TCP_IL 46 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const
struct ice_dummy_pkt_offsets dummy_pppoe_ipv4_udp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14},
	{ ICE_PPPOE,		18 },
	{ ICE_IPV4_OFOS,	26 },
	{ ICE_UDP_ILOS,		46 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_pppoe_ipv4_udp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x88, 0x64, /* ICE_VLAN_OFOS 14 */

	0x11, 0x00, 0x00, 0x00, /* ICE_PPPOE 18 */
	0x00, 0x16,

	0x00, 0x21,		/* PPP Link Layer 24 */

	0x45, 0x00, 0x00, 0x1c, /* ICE_IPV4_OFOS 26 */
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_UDP_ILOS 46 */
	0x00, 0x08, 0x00, 0x00,

	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_pppoe_packet_ipv6_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14},
	{ ICE_PPPOE,		18 },
	{ ICE_IPV6_OFOS,	26 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_pppoe_ipv6_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x88, 0x64, /* ICE_VLAN_OFOS 14 */

	0x11, 0x00, 0x00, 0x00, /* ICE_PPPOE 18 */
	0x00, 0x2a,

	0x00, 0x57,		/* PPP Link Layer 24 */

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 26 */
	0x00, 0x00, 0x3b, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const
struct ice_dummy_pkt_offsets dummy_pppoe_packet_ipv6_tcp_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14},
	{ ICE_PPPOE,		18 },
	{ ICE_IPV6_OFOS,	26 },
	{ ICE_TCP_IL,		66 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_pppoe_ipv6_tcp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x88, 0x64, /* ICE_VLAN_OFOS 14 */

	0x11, 0x00, 0x00, 0x00, /* ICE_PPPOE 18 */
	0x00, 0x2a,

	0x00, 0x57,		/* PPP Link Layer 24 */

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 26 */
	0x00, 0x14, 0x06, 0x00, /* Next header is TCP */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_TCP_IL 66 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x50, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const
struct ice_dummy_pkt_offsets dummy_pppoe_packet_ipv6_udp_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_ETYPE_OL,		12 },
	{ ICE_VLAN_OFOS,	14},
	{ ICE_PPPOE,		18 },
	{ ICE_IPV6_OFOS,	26 },
	{ ICE_UDP_ILOS,		66 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_pppoe_ipv6_udp_packet[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x81, 0x00,		/* ICE_ETYPE_OL 12 */

	0x00, 0x00, 0x88, 0x64, /* ICE_VLAN_OFOS 14 */

	0x11, 0x00, 0x00, 0x00, /* ICE_PPPOE 18 */
	0x00, 0x2a,

	0x00, 0x57,		/* PPP Link Layer 24 */

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 26 */
	0x00, 0x08, 0x11, 0x00, /* Next header UDP*/
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_UDP_ILOS 66 */
	0x00, 0x08, 0x00, 0x00,

	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_ipv4_esp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_ESP,			34 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv4_esp_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x1c, /* ICE_IPV4_IL 14 */
	0x00, 0x00, 0x40, 0x00,
	0x40, 0x32, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_ESP 34 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_ipv6_esp_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV6_OFOS,	14 },
	{ ICE_ESP,			54 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv6_esp_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x86, 0xDD,

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 14 */
	0x00, 0x08, 0x32, 0x00, /* Next header ESP */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_ESP 54 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_ipv4_ah_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_AH,			34 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv4_ah_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x20, /* ICE_IPV4_IL 14 */
	0x00, 0x00, 0x40, 0x00,
	0x40, 0x33, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_AH 34 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_ipv6_ah_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV6_OFOS,	14 },
	{ ICE_AH,			54 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv6_ah_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x86, 0xDD,

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 14 */
	0x00, 0x0c, 0x33, 0x00, /* Next header AH */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_AH 54 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_ipv4_nat_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_UDP_ILOS,		34 },
	{ ICE_NAT_T,		42 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv4_nat_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x24, /* ICE_IPV4_IL 14 */
	0x00, 0x00, 0x40, 0x00,
	0x40, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x11, 0x94, /* ICE_NAT_T 34 */
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_ipv6_nat_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV6_OFOS,	14 },
	{ ICE_UDP_ILOS,		54 },
	{ ICE_NAT_T,		62 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv6_nat_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x86, 0xDD,

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_OFOS 14 */
	0x00, 0x10, 0x11, 0x00, /* Next header NAT_T */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x11, 0x94, /* ICE_NAT_T 54 */
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */

};

static const struct ice_dummy_pkt_offsets dummy_ipv4_l2tpv3_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV4_OFOS,	14 },
	{ ICE_L2TPV3,		34 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv4_l2tpv3_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x20, /* ICE_IPV4_IL 14 */
	0x00, 0x00, 0x40, 0x00,
	0x40, 0x73, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_L2TPV3 34 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

static const struct ice_dummy_pkt_offsets dummy_ipv6_l2tpv3_packet_offsets[] = {
	{ ICE_MAC_OFOS,		0 },
	{ ICE_IPV6_OFOS,	14 },
	{ ICE_L2TPV3,		54 },
	{ ICE_PROTOCOL_LAST,	0 },
};

static const u8 dummy_ipv6_l2tpv3_pkt[] = {
	0x00, 0x00, 0x00, 0x00, /* ICE_MAC_OFOS 0 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x86, 0xDD,

	0x60, 0x00, 0x00, 0x00, /* ICE_IPV6_IL 14 */
	0x00, 0x0c, 0x73, 0x40,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, /* ICE_L2TPV3 54 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,		/* 2 bytes for 4 bytes alignment */
};

/* this is a recipe to profile association bitmap */
static ice_declare_bitmap(recipe_to_profile[ICE_MAX_NUM_RECIPES],
			  ICE_MAX_NUM_PROFILES);

/* this is a profile to recipe association bitmap */
static ice_declare_bitmap(profile_to_recipe[ICE_MAX_NUM_PROFILES],
			  ICE_MAX_NUM_RECIPES);

static void ice_get_recp_to_prof_map(struct ice_hw *hw);

/**
 * ice_collect_result_idx - copy result index values
 * @buf: buffer that contains the result index
 * @recp: the recipe struct to copy data into
 */
static void ice_collect_result_idx(struct ice_aqc_recipe_data_elem *buf,
				   struct ice_sw_recipe *recp)
{
	if (buf->content.result_indx & ICE_AQ_RECIPE_RESULT_EN)
		ice_set_bit(buf->content.result_indx &
			    ~ICE_AQ_RECIPE_RESULT_EN, recp->res_idxs);
}

/**
 * ice_get_tun_type_for_recipe - get tunnel type for the recipe
 * @rid: recipe ID that we are populating
 */
static enum ice_sw_tunnel_type ice_get_tun_type_for_recipe(u8 rid)
{
	u8 vxlan_profile[12] = {10, 11, 12, 16, 17, 18, 22, 23, 24, 25, 26, 27};
	u8 gre_profile[12] = {13, 14, 15, 19, 20, 21, 28, 29, 30, 31, 32, 33};
	u8 pppoe_profile[7] = {34, 35, 36, 37, 38, 39, 40};
	u8 non_tun_profile[6] = {4, 5, 6, 7, 8, 9};
	enum ice_sw_tunnel_type tun_type = ICE_NON_TUN;
	u16 i, j, profile_num = 0;
	bool non_tun_valid = false;
	bool pppoe_valid = false;
	bool vxlan_valid = false;
	bool gre_valid = false;
	bool gtp_valid = false;
	bool flag_valid = false;

	for (j = 0; j < ICE_MAX_NUM_PROFILES; j++) {
		if (!ice_is_bit_set(recipe_to_profile[rid], j))
			continue;
		else
			profile_num++;

		for (i = 0; i < 12; i++) {
			if (gre_profile[i] == j)
				gre_valid = true;
		}

		for (i = 0; i < 12; i++) {
			if (vxlan_profile[i] == j)
				vxlan_valid = true;
		}

		for (i = 0; i < 7; i++) {
			if (pppoe_profile[i] == j)
				pppoe_valid = true;
		}

		for (i = 0; i < 6; i++) {
			if (non_tun_profile[i] == j)
				non_tun_valid = true;
		}

		if (j >= ICE_PROFID_IPV4_GTPU_EH_IPV4_OTHER &&
		    j <= ICE_PROFID_IPV6_GTPU_IPV6_OTHER)
			gtp_valid = true;

		if (j >= ICE_PROFID_IPV4_ESP &&
		    j <= ICE_PROFID_IPV6_PFCP_SESSION)
			flag_valid = true;
	}

	if (!non_tun_valid && vxlan_valid)
		tun_type = ICE_SW_TUN_VXLAN;
	else if (!non_tun_valid && gre_valid)
		tun_type = ICE_SW_TUN_NVGRE;
	else if (!non_tun_valid && pppoe_valid)
		tun_type = ICE_SW_TUN_PPPOE;
	else if (!non_tun_valid && gtp_valid)
		tun_type = ICE_SW_TUN_GTP;
	else if (non_tun_valid &&
		 (vxlan_valid || gre_valid || gtp_valid || pppoe_valid))
		tun_type = ICE_SW_TUN_AND_NON_TUN;
	else if (non_tun_valid && !vxlan_valid && !gre_valid && !gtp_valid &&
		 !pppoe_valid)
		tun_type = ICE_NON_TUN;

	if (profile_num > 1 && tun_type == ICE_SW_TUN_PPPOE) {
		i = ice_is_bit_set(recipe_to_profile[rid],
				   ICE_PROFID_PPPOE_IPV4_OTHER);
		j = ice_is_bit_set(recipe_to_profile[rid],
				   ICE_PROFID_PPPOE_IPV6_OTHER);
		if (i && !j)
			tun_type = ICE_SW_TUN_PPPOE_IPV4;
		else if (!i && j)
			tun_type = ICE_SW_TUN_PPPOE_IPV6;
	}

	if (profile_num == 1 && (flag_valid || non_tun_valid || pppoe_valid)) {
		for (j = 0; j < ICE_MAX_NUM_PROFILES; j++) {
			if (ice_is_bit_set(recipe_to_profile[rid], j)) {
				switch (j) {
				case ICE_PROFID_IPV4_TCP:
					tun_type = ICE_SW_IPV4_TCP;
					break;
				case ICE_PROFID_IPV4_UDP:
					tun_type = ICE_SW_IPV4_UDP;
					break;
				case ICE_PROFID_IPV6_TCP:
					tun_type = ICE_SW_IPV6_TCP;
					break;
				case ICE_PROFID_IPV6_UDP:
					tun_type = ICE_SW_IPV6_UDP;
					break;
				case ICE_PROFID_PPPOE_PAY:
					tun_type = ICE_SW_TUN_PPPOE_PAY;
					break;
				case ICE_PROFID_PPPOE_IPV4_TCP:
					tun_type = ICE_SW_TUN_PPPOE_IPV4_TCP;
					break;
				case ICE_PROFID_PPPOE_IPV4_UDP:
					tun_type = ICE_SW_TUN_PPPOE_IPV4_UDP;
					break;
				case ICE_PROFID_PPPOE_IPV4_OTHER:
					tun_type = ICE_SW_TUN_PPPOE_IPV4;
					break;
				case ICE_PROFID_PPPOE_IPV6_TCP:
					tun_type = ICE_SW_TUN_PPPOE_IPV6_TCP;
					break;
				case ICE_PROFID_PPPOE_IPV6_UDP:
					tun_type = ICE_SW_TUN_PPPOE_IPV6_UDP;
					break;
				case ICE_PROFID_PPPOE_IPV6_OTHER:
					tun_type = ICE_SW_TUN_PPPOE_IPV6;
					break;
				case ICE_PROFID_IPV4_ESP:
					tun_type = ICE_SW_TUN_IPV4_ESP;
					break;
				case ICE_PROFID_IPV6_ESP:
					tun_type = ICE_SW_TUN_IPV6_ESP;
					break;
				case ICE_PROFID_IPV4_AH:
					tun_type = ICE_SW_TUN_IPV4_AH;
					break;
				case ICE_PROFID_IPV6_AH:
					tun_type = ICE_SW_TUN_IPV6_AH;
					break;
				case ICE_PROFID_IPV4_NAT_T:
					tun_type = ICE_SW_TUN_IPV4_NAT_T;
					break;
				case ICE_PROFID_IPV6_NAT_T:
					tun_type = ICE_SW_TUN_IPV6_NAT_T;
					break;
				case ICE_PROFID_IPV4_PFCP_NODE:
					tun_type =
					ICE_SW_TUN_PROFID_IPV4_PFCP_NODE;
					break;
				case ICE_PROFID_IPV6_PFCP_NODE:
					tun_type =
					ICE_SW_TUN_PROFID_IPV6_PFCP_NODE;
					break;
				case ICE_PROFID_IPV4_PFCP_SESSION:
					tun_type =
					ICE_SW_TUN_PROFID_IPV4_PFCP_SESSION;
					break;
				case ICE_PROFID_IPV6_PFCP_SESSION:
					tun_type =
					ICE_SW_TUN_PROFID_IPV6_PFCP_SESSION;
					break;
				case ICE_PROFID_MAC_IPV4_L2TPV3:
					tun_type = ICE_SW_TUN_IPV4_L2TPV3;
					break;
				case ICE_PROFID_MAC_IPV6_L2TPV3:
					tun_type = ICE_SW_TUN_IPV6_L2TPV3;
					break;
				default:
					break;
				}

				return tun_type;
			}
		}
	}

	return tun_type;
}

/**
 * ice_get_recp_frm_fw - update SW bookkeeping from FW recipe entries
 * @hw: pointer to hardware structure
 * @recps: struct that we need to populate
 * @rid: recipe ID that we are populating
 * @refresh_required: true if we should get recipe to profile mapping from FW
 *
 * This function is used to populate all the necessary entries into our
 * bookkeeping so that we have a current list of all the recipes that are
 * programmed in the firmware.
 */
static enum ice_status
ice_get_recp_frm_fw(struct ice_hw *hw, struct ice_sw_recipe *recps, u8 rid,
		    bool *refresh_required)
{
	ice_declare_bitmap(result_bm, ICE_MAX_FV_WORDS);
	struct ice_aqc_recipe_data_elem *tmp;
	u16 num_recps = ICE_MAX_NUM_RECIPES;
	struct ice_prot_lkup_ext *lkup_exts;
	enum ice_status status;
	u8 fv_word_idx = 0;
	u16 sub_recps;

	ice_zero_bitmap(result_bm, ICE_MAX_FV_WORDS);

	/* we need a buffer big enough to accommodate all the recipes */
	tmp = (struct ice_aqc_recipe_data_elem *)ice_calloc(hw,
		ICE_MAX_NUM_RECIPES, sizeof(*tmp));
	if (!tmp)
		return ICE_ERR_NO_MEMORY;

	tmp[0].recipe_indx = rid;
	status = ice_aq_get_recipe(hw, tmp, &num_recps, rid, NULL);
	/* non-zero status meaning recipe doesn't exist */
	if (status)
		goto err_unroll;

	/* Get recipe to profile map so that we can get the fv from lkups that
	 * we read for a recipe from FW. Since we want to minimize the number of
	 * times we make this FW call, just make one call and cache the copy
	 * until a new recipe is added. This operation is only required the
	 * first time to get the changes from FW. Then to search existing
	 * entries we don't need to update the cache again until another recipe
	 * gets added.
	 */
	if (*refresh_required) {
		ice_get_recp_to_prof_map(hw);
		*refresh_required = false;
	}

	/* Start populating all the entries for recps[rid] based on lkups from
	 * firmware. Note that we are only creating the root recipe in our
	 * database.
	 */
	lkup_exts = &recps[rid].lkup_exts;

	for (sub_recps = 0; sub_recps < num_recps; sub_recps++) {
		struct ice_aqc_recipe_data_elem root_bufs = tmp[sub_recps];
		struct ice_recp_grp_entry *rg_entry;
		u8 i, prof, idx, prot = 0;
		bool is_root;
		u16 off = 0;

		rg_entry = (struct ice_recp_grp_entry *)
			ice_malloc(hw, sizeof(*rg_entry));
		if (!rg_entry) {
			status = ICE_ERR_NO_MEMORY;
			goto err_unroll;
		}

		idx = root_bufs.recipe_indx;
		is_root = root_bufs.content.rid & ICE_AQ_RECIPE_ID_IS_ROOT;

		/* Mark all result indices in this chain */
		if (root_bufs.content.result_indx & ICE_AQ_RECIPE_RESULT_EN)
			ice_set_bit(root_bufs.content.result_indx &
				    ~ICE_AQ_RECIPE_RESULT_EN, result_bm);

		/* get the first profile that is associated with rid */
		prof = ice_find_first_bit(recipe_to_profile[idx],
					  ICE_MAX_NUM_PROFILES);
		for (i = 0; i < ICE_NUM_WORDS_RECIPE; i++) {
			u8 lkup_indx = root_bufs.content.lkup_indx[i + 1];

			rg_entry->fv_idx[i] = lkup_indx;
			rg_entry->fv_mask[i] =
				LE16_TO_CPU(root_bufs.content.mask[i + 1]);

			/* If the recipe is a chained recipe then all its
			 * child recipe's result will have a result index.
			 * To fill fv_words we should not use those result
			 * index, we only need the protocol ids and offsets.
			 * We will skip all the fv_idx which stores result
			 * index in them. We also need to skip any fv_idx which
			 * has ICE_AQ_RECIPE_LKUP_IGNORE or 0 since it isn't a
			 * valid offset value.
			 */
			if (ice_is_bit_set(hw->switch_info->prof_res_bm[prof],
					   rg_entry->fv_idx[i]) ||
			    rg_entry->fv_idx[i] & ICE_AQ_RECIPE_LKUP_IGNORE ||
			    rg_entry->fv_idx[i] == 0)
				continue;

			ice_find_prot_off(hw, ICE_BLK_SW, prof,
					  rg_entry->fv_idx[i], &prot, &off);
			lkup_exts->fv_words[fv_word_idx].prot_id = prot;
			lkup_exts->fv_words[fv_word_idx].off = off;
			lkup_exts->field_mask[fv_word_idx] =
				rg_entry->fv_mask[i];
			fv_word_idx++;
		}
		/* populate rg_list with the data from the child entry of this
		 * recipe
		 */
		LIST_ADD(&rg_entry->l_entry, &recps[rid].rg_list);

		/* Propagate some data to the recipe database */
		recps[idx].is_root = !!is_root;
		recps[idx].priority = root_bufs.content.act_ctrl_fwd_priority;
		ice_zero_bitmap(recps[idx].res_idxs, ICE_MAX_FV_WORDS);
		if (root_bufs.content.result_indx & ICE_AQ_RECIPE_RESULT_EN) {
			recps[idx].chain_idx = root_bufs.content.result_indx &
				~ICE_AQ_RECIPE_RESULT_EN;
			ice_set_bit(recps[idx].chain_idx, recps[idx].res_idxs);
		} else {
			recps[idx].chain_idx = ICE_INVAL_CHAIN_IND;
		}

		if (!is_root)
			continue;

		/* Only do the following for root recipes entries */
		ice_memcpy(recps[idx].r_bitmap, root_bufs.recipe_bitmap,
			   sizeof(recps[idx].r_bitmap), ICE_NONDMA_TO_NONDMA);
		recps[idx].root_rid = root_bufs.content.rid &
			~ICE_AQ_RECIPE_ID_IS_ROOT;
		recps[idx].priority = root_bufs.content.act_ctrl_fwd_priority;
	}

	/* Complete initialization of the root recipe entry */
	lkup_exts->n_val_words = fv_word_idx;
	recps[rid].big_recp = (num_recps > 1);
	recps[rid].n_grp_count = (u8)num_recps;
	recps[rid].tun_type = ice_get_tun_type_for_recipe(rid);
	recps[rid].root_buf = (struct ice_aqc_recipe_data_elem *)
		ice_memdup(hw, tmp, recps[rid].n_grp_count *
			   sizeof(*recps[rid].root_buf), ICE_NONDMA_TO_NONDMA);
	if (!recps[rid].root_buf)
		goto err_unroll;

	/* Copy result indexes */
	ice_cp_bitmap(recps[rid].res_idxs, result_bm, ICE_MAX_FV_WORDS);
	recps[rid].recp_created = true;

err_unroll:
	ice_free(hw, tmp);
	return status;
}

/**
 * ice_get_recp_to_prof_map - updates recipe to profile mapping
 * @hw: pointer to hardware structure
 *
 * This function is used to populate recipe_to_profile matrix where index to
 * this array is the recipe ID and the element is the mapping of which profiles
 * is this recipe mapped to.
 */
static void ice_get_recp_to_prof_map(struct ice_hw *hw)
{
	ice_declare_bitmap(r_bitmap, ICE_MAX_NUM_RECIPES);
	u16 i;

	for (i = 0; i < ICE_MAX_NUM_PROFILES; i++) {
		u16 j;

		ice_zero_bitmap(profile_to_recipe[i], ICE_MAX_NUM_RECIPES);
		ice_zero_bitmap(r_bitmap, ICE_MAX_NUM_RECIPES);
		if (ice_aq_get_recipe_to_profile(hw, i, (u8 *)r_bitmap, NULL))
			continue;
		ice_cp_bitmap(profile_to_recipe[i], r_bitmap,
			      ICE_MAX_NUM_RECIPES);
		for (j = 0; j < ICE_MAX_NUM_RECIPES; j++)
			if (ice_is_bit_set(r_bitmap, j))
				ice_set_bit(i, recipe_to_profile[j]);
	}
}

/**
 * ice_init_def_sw_recp - initialize the recipe book keeping tables
 * @hw: pointer to the HW struct
 * @recp_list: pointer to sw recipe list
 *
 * Allocate memory for the entire recipe table and initialize the structures/
 * entries corresponding to basic recipes.
 */
enum ice_status
ice_init_def_sw_recp(struct ice_hw *hw, struct ice_sw_recipe **recp_list)
{
	struct ice_sw_recipe *recps;
	u8 i;

	recps = (struct ice_sw_recipe *)
		ice_calloc(hw, ICE_MAX_NUM_RECIPES, sizeof(*recps));
	if (!recps)
		return ICE_ERR_NO_MEMORY;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		recps[i].root_rid = i;
		INIT_LIST_HEAD(&recps[i].filt_rules);
		INIT_LIST_HEAD(&recps[i].filt_replay_rules);
		INIT_LIST_HEAD(&recps[i].rg_list);
		ice_init_lock(&recps[i].filt_rule_lock);
	}

	*recp_list = recps;

	return ICE_SUCCESS;
}

/**
 * ice_aq_get_sw_cfg - get switch configuration
 * @hw: pointer to the hardware structure
 * @buf: pointer to the result buffer
 * @buf_size: length of the buffer available for response
 * @req_desc: pointer to requested descriptor
 * @num_elems: pointer to number of elements
 * @cd: pointer to command details structure or NULL
 *
 * Get switch configuration (0x0200) to be placed in 'buff'.
 * This admin command returns information such as initial VSI/port number
 * and switch ID it belongs to.
 *
 * NOTE: *req_desc is both an input/output parameter.
 * The caller of this function first calls this function with *request_desc set
 * to 0. If the response from f/w has *req_desc set to 0, all the switch
 * configuration information has been returned; if non-zero (meaning not all
 * the information was returned), the caller should call this function again
 * with *req_desc set to the previous value returned by f/w to get the
 * next block of switch configuration information.
 *
 * *num_elems is output only parameter. This reflects the number of elements
 * in response buffer. The caller of this function to use *num_elems while
 * parsing the response buffer.
 */
static enum ice_status
ice_aq_get_sw_cfg(struct ice_hw *hw, struct ice_aqc_get_sw_cfg_resp *buf,
		  u16 buf_size, u16 *req_desc, u16 *num_elems,
		  struct ice_sq_cd *cd)
{
	struct ice_aqc_get_sw_cfg *cmd;
	enum ice_status status;
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_sw_cfg);
	cmd = &desc.params.get_sw_conf;
	cmd->element = CPU_TO_LE16(*req_desc);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status) {
		*req_desc = LE16_TO_CPU(cmd->element);
		*num_elems = LE16_TO_CPU(cmd->num_elems);
	}

	return status;
}

/**
 * ice_alloc_sw - allocate resources specific to switch
 * @hw: pointer to the HW struct
 * @ena_stats: true to turn on VEB stats
 * @shared_res: true for shared resource, false for dedicated resource
 * @sw_id: switch ID returned
 * @counter_id: VEB counter ID returned
 *
 * allocates switch resources (SWID and VEB counter) (0x0208)
 */
enum ice_status
ice_alloc_sw(struct ice_hw *hw, bool ena_stats, bool shared_res, u16 *sw_id,
	     u16 *counter_id)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	struct ice_aqc_res_elem *sw_ele;
	enum ice_status status;
	u16 buf_len;

	buf_len = sizeof(*sw_buf);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)
		   ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	/* Prepare buffer for switch ID.
	 * The number of resource entries in buffer is passed as 1 since only a
	 * single switch/VEB instance is allocated, and hence a single sw_id
	 * is requested.
	 */
	sw_buf->num_elems = CPU_TO_LE16(1);
	sw_buf->res_type =
		CPU_TO_LE16(ICE_AQC_RES_TYPE_SWID |
			    (shared_res ? ICE_AQC_RES_TYPE_FLAG_SHARED :
			    ICE_AQC_RES_TYPE_FLAG_DEDICATED));

	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);

	if (status)
		goto ice_alloc_sw_exit;

	sw_ele = &sw_buf->elem[0];
	*sw_id = LE16_TO_CPU(sw_ele->e.sw_resp);

	if (ena_stats) {
		/* Prepare buffer for VEB Counter */
		enum ice_adminq_opc opc = ice_aqc_opc_alloc_res;
		struct ice_aqc_alloc_free_res_elem *counter_buf;
		struct ice_aqc_res_elem *counter_ele;

		counter_buf = (struct ice_aqc_alloc_free_res_elem *)
				ice_malloc(hw, buf_len);
		if (!counter_buf) {
			status = ICE_ERR_NO_MEMORY;
			goto ice_alloc_sw_exit;
		}

		/* The number of resource entries in buffer is passed as 1 since
		 * only a single switch/VEB instance is allocated, and hence a
		 * single VEB counter is requested.
		 */
		counter_buf->num_elems = CPU_TO_LE16(1);
		counter_buf->res_type =
			CPU_TO_LE16(ICE_AQC_RES_TYPE_VEB_COUNTER |
				    ICE_AQC_RES_TYPE_FLAG_DEDICATED);
		status = ice_aq_alloc_free_res(hw, 1, counter_buf, buf_len,
					       opc, NULL);

		if (status) {
			ice_free(hw, counter_buf);
			goto ice_alloc_sw_exit;
		}
		counter_ele = &counter_buf->elem[0];
		*counter_id = LE16_TO_CPU(counter_ele->e.sw_resp);
		ice_free(hw, counter_buf);
	}

ice_alloc_sw_exit:
	ice_free(hw, sw_buf);
	return status;
}

/**
 * ice_free_sw - free resources specific to switch
 * @hw: pointer to the HW struct
 * @sw_id: switch ID returned
 * @counter_id: VEB counter ID returned
 *
 * free switch resources (SWID and VEB counter) (0x0209)
 *
 * NOTE: This function frees multiple resources. It continues
 * releasing other resources even after it encounters error.
 * The error code returned is the last error it encountered.
 */
enum ice_status ice_free_sw(struct ice_hw *hw, u16 sw_id, u16 counter_id)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf, *counter_buf;
	enum ice_status status, ret_status;
	u16 buf_len;

	buf_len = sizeof(*sw_buf);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)
		   ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	/* Prepare buffer to free for switch ID res.
	 * The number of resource entries in buffer is passed as 1 since only a
	 * single switch/VEB instance is freed, and hence a single sw_id
	 * is released.
	 */
	sw_buf->num_elems = CPU_TO_LE16(1);
	sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_SWID);
	sw_buf->elem[0].e.sw_resp = CPU_TO_LE16(sw_id);

	ret_status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len,
					   ice_aqc_opc_free_res, NULL);

	if (ret_status)
		ice_debug(hw, ICE_DBG_SW, "CQ CMD Buffer:\n");

	/* Prepare buffer to free for VEB Counter resource */
	counter_buf = (struct ice_aqc_alloc_free_res_elem *)
			ice_malloc(hw, buf_len);
	if (!counter_buf) {
		ice_free(hw, sw_buf);
		return ICE_ERR_NO_MEMORY;
	}

	/* The number of resource entries in buffer is passed as 1 since only a
	 * single switch/VEB instance is freed, and hence a single VEB counter
	 * is released
	 */
	counter_buf->num_elems = CPU_TO_LE16(1);
	counter_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_VEB_COUNTER);
	counter_buf->elem[0].e.sw_resp = CPU_TO_LE16(counter_id);

	status = ice_aq_alloc_free_res(hw, 1, counter_buf, buf_len,
				       ice_aqc_opc_free_res, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_SW,
			  "VEB counter resource could not be freed\n");
		ret_status = status;
	}

	ice_free(hw, counter_buf);
	ice_free(hw, sw_buf);
	return ret_status;
}

/**
 * ice_aq_add_vsi
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Add a VSI context to the hardware (0x0210)
 */
enum ice_status
ice_aq_add_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
	       struct ice_sq_cd *cd)
{
	struct ice_aqc_add_update_free_vsi_resp *res;
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	res = &desc.params.add_update_free_vsi_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_vsi);

	if (!vsi_ctx->alloc_from_pool)
		cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num |
					   ICE_AQ_VSI_IS_VALID);

	cmd->vsi_flags = CPU_TO_LE16(vsi_ctx->flags);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	status = ice_aq_send_cmd(hw, &desc, &vsi_ctx->info,
				 sizeof(vsi_ctx->info), cd);

	if (!status) {
		vsi_ctx->vsi_num = LE16_TO_CPU(res->vsi_num) & ICE_AQ_VSI_NUM_M;
		vsi_ctx->vsis_allocd = LE16_TO_CPU(res->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(res->vsi_free);
	}

	return status;
}

/**
 * ice_aq_free_vsi
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @keep_vsi_alloc: keep VSI allocation as part of this PF's resources
 * @cd: pointer to command details structure or NULL
 *
 * Free VSI context info from hardware (0x0213)
 */
enum ice_status
ice_aq_free_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		bool keep_vsi_alloc, struct ice_sq_cd *cd)
{
	struct ice_aqc_add_update_free_vsi_resp *resp;
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	resp = &desc.params.add_update_free_vsi_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_free_vsi);

	cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num | ICE_AQ_VSI_IS_VALID);
	if (keep_vsi_alloc)
		cmd->cmd_flags = CPU_TO_LE16(ICE_AQ_VSI_KEEP_ALLOC);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (!status) {
		vsi_ctx->vsis_allocd = LE16_TO_CPU(resp->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(resp->vsi_free);
	}

	return status;
}

/**
 * ice_aq_update_vsi
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Update VSI context in the hardware (0x0211)
 */
enum ice_status
ice_aq_update_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		  struct ice_sq_cd *cd)
{
	struct ice_aqc_add_update_free_vsi_resp *resp;
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	resp = &desc.params.add_update_free_vsi_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_update_vsi);

	cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num | ICE_AQ_VSI_IS_VALID);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	status = ice_aq_send_cmd(hw, &desc, &vsi_ctx->info,
				 sizeof(vsi_ctx->info), cd);

	if (!status) {
		vsi_ctx->vsis_allocd = LE16_TO_CPU(resp->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(resp->vsi_free);
	}

	return status;
}

/**
 * ice_is_vsi_valid - check whether the VSI is valid or not
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * check whether the VSI is valid or not
 */
bool ice_is_vsi_valid(struct ice_hw *hw, u16 vsi_handle)
{
	return vsi_handle < ICE_MAX_VSI && hw->vsi_ctx[vsi_handle];
}

/**
 * ice_get_hw_vsi_num - return the HW VSI number
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * return the HW VSI number
 * Caution: call this function only if VSI is valid (ice_is_vsi_valid)
 */
u16 ice_get_hw_vsi_num(struct ice_hw *hw, u16 vsi_handle)
{
	return hw->vsi_ctx[vsi_handle]->vsi_num;
}

/**
 * ice_get_vsi_ctx - return the VSI context entry for a given VSI handle
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * return the VSI context entry for a given VSI handle
 */
struct ice_vsi_ctx *ice_get_vsi_ctx(struct ice_hw *hw, u16 vsi_handle)
{
	return (vsi_handle >= ICE_MAX_VSI) ? NULL : hw->vsi_ctx[vsi_handle];
}

/**
 * ice_save_vsi_ctx - save the VSI context for a given VSI handle
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 * @vsi: VSI context pointer
 *
 * save the VSI context entry for a given VSI handle
 */
static void
ice_save_vsi_ctx(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi)
{
	hw->vsi_ctx[vsi_handle] = vsi;
}

/**
 * ice_clear_vsi_q_ctx - clear VSI queue contexts for all TCs
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 */
static void ice_clear_vsi_q_ctx(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_vsi_ctx *vsi;
	u8 i;

	vsi = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi)
		return;
	ice_for_each_traffic_class(i) {
		if (vsi->lan_q_ctx[i]) {
			ice_free(hw, vsi->lan_q_ctx[i]);
			vsi->lan_q_ctx[i] = NULL;
		}
	}
}

/**
 * ice_clear_vsi_ctx - clear the VSI context entry
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * clear the VSI context entry
 */
static void ice_clear_vsi_ctx(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_vsi_ctx *vsi;

	vsi = ice_get_vsi_ctx(hw, vsi_handle);
	if (vsi) {
		ice_clear_vsi_q_ctx(hw, vsi_handle);
		ice_free(hw, vsi);
		hw->vsi_ctx[vsi_handle] = NULL;
	}
}

/**
 * ice_clear_all_vsi_ctx - clear all the VSI context entries
 * @hw: pointer to the HW struct
 */
void ice_clear_all_vsi_ctx(struct ice_hw *hw)
{
	u16 i;

	for (i = 0; i < ICE_MAX_VSI; i++)
		ice_clear_vsi_ctx(hw, i);
}

/**
 * ice_add_vsi - add VSI context to the hardware and VSI handle list
 * @hw: pointer to the HW struct
 * @vsi_handle: unique VSI handle provided by drivers
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Add a VSI context to the hardware also add it into the VSI handle list.
 * If this function gets called after reset for existing VSIs then update
 * with the new HW VSI number in the corresponding VSI handle list entry.
 */
enum ice_status
ice_add_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	    struct ice_sq_cd *cd)
{
	struct ice_vsi_ctx *tmp_vsi_ctx;
	enum ice_status status;

	if (vsi_handle >= ICE_MAX_VSI)
		return ICE_ERR_PARAM;
	status = ice_aq_add_vsi(hw, vsi_ctx, cd);
	if (status)
		return status;
	tmp_vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!tmp_vsi_ctx) {
		/* Create a new VSI context */
		tmp_vsi_ctx = (struct ice_vsi_ctx *)
			ice_malloc(hw, sizeof(*tmp_vsi_ctx));
		if (!tmp_vsi_ctx) {
			ice_aq_free_vsi(hw, vsi_ctx, false, cd);
			return ICE_ERR_NO_MEMORY;
		}
		*tmp_vsi_ctx = *vsi_ctx;

		ice_save_vsi_ctx(hw, vsi_handle, tmp_vsi_ctx);
	} else {
		/* update with new HW VSI num */
		tmp_vsi_ctx->vsi_num = vsi_ctx->vsi_num;
	}

	return ICE_SUCCESS;
}

/**
 * ice_free_vsi- free VSI context from hardware and VSI handle list
 * @hw: pointer to the HW struct
 * @vsi_handle: unique VSI handle
 * @vsi_ctx: pointer to a VSI context struct
 * @keep_vsi_alloc: keep VSI allocation as part of this PF's resources
 * @cd: pointer to command details structure or NULL
 *
 * Free VSI context info from hardware as well as from VSI handle list
 */
enum ice_status
ice_free_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	     bool keep_vsi_alloc, struct ice_sq_cd *cd)
{
	enum ice_status status;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx->vsi_num = ice_get_hw_vsi_num(hw, vsi_handle);
	status = ice_aq_free_vsi(hw, vsi_ctx, keep_vsi_alloc, cd);
	if (!status)
		ice_clear_vsi_ctx(hw, vsi_handle);
	return status;
}

/**
 * ice_update_vsi
 * @hw: pointer to the HW struct
 * @vsi_handle: unique VSI handle
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Update VSI context in the hardware
 */
enum ice_status
ice_update_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	       struct ice_sq_cd *cd)
{
	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx->vsi_num = ice_get_hw_vsi_num(hw, vsi_handle);
	return ice_aq_update_vsi(hw, vsi_ctx, cd);
}

/**
 * ice_aq_get_vsi_params
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Get VSI context info from hardware (0x0212)
 */
enum ice_status
ice_aq_get_vsi_params(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		      struct ice_sq_cd *cd)
{
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aqc_get_vsi_resp *resp;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	resp = &desc.params.get_vsi_resp;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_vsi_params);

	cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num | ICE_AQ_VSI_IS_VALID);

	status = ice_aq_send_cmd(hw, &desc, &vsi_ctx->info,
				 sizeof(vsi_ctx->info), cd);
	if (!status) {
		vsi_ctx->vsi_num = LE16_TO_CPU(resp->vsi_num) &
					ICE_AQ_VSI_NUM_M;
		vsi_ctx->vsis_allocd = LE16_TO_CPU(resp->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(resp->vsi_free);
	}

	return status;
}

/**
 * ice_aq_add_update_mir_rule - add/update a mirror rule
 * @hw: pointer to the HW struct
 * @rule_type: Rule Type
 * @dest_vsi: VSI number to which packets will be mirrored
 * @count: length of the list
 * @mr_buf: buffer for list of mirrored VSI numbers
 * @cd: pointer to command details structure or NULL
 * @rule_id: Rule ID
 *
 * Add/Update Mirror Rule (0x260).
 */
enum ice_status
ice_aq_add_update_mir_rule(struct ice_hw *hw, u16 rule_type, u16 dest_vsi,
			   u16 count, struct ice_mir_rule_buf *mr_buf,
			   struct ice_sq_cd *cd, u16 *rule_id)
{
	struct ice_aqc_add_update_mir_rule *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;
	__le16 *mr_list = NULL;
	u16 buf_size = 0;

	switch (rule_type) {
	case ICE_AQC_RULE_TYPE_VPORT_INGRESS:
	case ICE_AQC_RULE_TYPE_VPORT_EGRESS:
		/* Make sure count and mr_buf are set for these rule_types */
		if (!(count && mr_buf))
			return ICE_ERR_PARAM;

		buf_size = count * sizeof(__le16);
		mr_list = (_FORCE_ __le16 *)ice_malloc(hw, buf_size);
		if (!mr_list)
			return ICE_ERR_NO_MEMORY;
		break;
	case ICE_AQC_RULE_TYPE_PPORT_INGRESS:
	case ICE_AQC_RULE_TYPE_PPORT_EGRESS:
		/* Make sure count and mr_buf are not set for these
		 * rule_types
		 */
		if (count || mr_buf)
			return ICE_ERR_PARAM;
		break;
	default:
		ice_debug(hw, ICE_DBG_SW,
			  "Error due to unsupported rule_type %u\n", rule_type);
		return ICE_ERR_OUT_OF_RANGE;
	}

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_update_mir_rule);

	/* Pre-process 'mr_buf' items for add/update of virtual port
	 * ingress/egress mirroring (but not physical port ingress/egress
	 * mirroring)
	 */
	if (mr_buf) {
		int i;

		for (i = 0; i < count; i++) {
			u16 id;

			id = mr_buf[i].vsi_idx & ICE_AQC_RULE_MIRRORED_VSI_M;

			/* Validate specified VSI number, make sure it is less
			 * than ICE_MAX_VSI, if not return with error.
			 */
			if (id >= ICE_MAX_VSI) {
				ice_debug(hw, ICE_DBG_SW,
					  "Error VSI index (%u) out-of-range\n",
					  id);
				ice_free(hw, mr_list);
				return ICE_ERR_OUT_OF_RANGE;
			}

			/* add VSI to mirror rule */
			if (mr_buf[i].add)
				mr_list[i] =
					CPU_TO_LE16(id | ICE_AQC_RULE_ACT_M);
			else /* remove VSI from mirror rule */
				mr_list[i] = CPU_TO_LE16(id);
		}
	}

	cmd = &desc.params.add_update_rule;
	if ((*rule_id) != ICE_INVAL_MIRROR_RULE_ID)
		cmd->rule_id = CPU_TO_LE16(((*rule_id) & ICE_AQC_RULE_ID_M) |
					   ICE_AQC_RULE_ID_VALID_M);
	cmd->rule_type = CPU_TO_LE16(rule_type & ICE_AQC_RULE_TYPE_M);
	cmd->num_entries = CPU_TO_LE16(count);
	cmd->dest = CPU_TO_LE16(dest_vsi);

	status = ice_aq_send_cmd(hw, &desc, mr_list, buf_size, cd);
	if (!status)
		*rule_id = LE16_TO_CPU(cmd->rule_id) & ICE_AQC_RULE_ID_M;

	ice_free(hw, mr_list);

	return status;
}

/**
 * ice_aq_delete_mir_rule - delete a mirror rule
 * @hw: pointer to the HW struct
 * @rule_id: Mirror rule ID (to be deleted)
 * @keep_allocd: if set, the VSI stays part of the PF allocated res,
 *		 otherwise it is returned to the shared pool
 * @cd: pointer to command details structure or NULL
 *
 * Delete Mirror Rule (0x261).
 */
enum ice_status
ice_aq_delete_mir_rule(struct ice_hw *hw, u16 rule_id, bool keep_allocd,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_delete_mir_rule *cmd;
	struct ice_aq_desc desc;

	/* rule_id should be in the range 0...63 */
	if (rule_id >= ICE_MAX_NUM_MIRROR_RULES)
		return ICE_ERR_OUT_OF_RANGE;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_del_mir_rule);

	cmd = &desc.params.del_rule;
	rule_id |= ICE_AQC_RULE_ID_VALID_M;
	cmd->rule_id = CPU_TO_LE16(rule_id);

	if (keep_allocd)
		cmd->flags = CPU_TO_LE16(ICE_AQC_FLAG_KEEP_ALLOCD_M);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_alloc_free_vsi_list
 * @hw: pointer to the HW struct
 * @vsi_list_id: VSI list ID returned or used for lookup
 * @lkup_type: switch rule filter lookup type
 * @opc: switch rules population command type - pass in the command opcode
 *
 * allocates or free a VSI list resource
 */
static enum ice_status
ice_aq_alloc_free_vsi_list(struct ice_hw *hw, u16 *vsi_list_id,
			   enum ice_sw_lkup_type lkup_type,
			   enum ice_adminq_opc opc)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	struct ice_aqc_res_elem *vsi_ele;
	enum ice_status status;
	u16 buf_len;

	buf_len = sizeof(*sw_buf);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)
		ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;
	sw_buf->num_elems = CPU_TO_LE16(1);

	if (lkup_type == ICE_SW_LKUP_MAC ||
	    lkup_type == ICE_SW_LKUP_MAC_VLAN ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE_MAC ||
	    lkup_type == ICE_SW_LKUP_PROMISC ||
	    lkup_type == ICE_SW_LKUP_PROMISC_VLAN ||
	    lkup_type == ICE_SW_LKUP_LAST) {
		sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_VSI_LIST_REP);
	} else if (lkup_type == ICE_SW_LKUP_VLAN) {
		sw_buf->res_type =
			CPU_TO_LE16(ICE_AQC_RES_TYPE_VSI_LIST_PRUNE);
	} else {
		status = ICE_ERR_PARAM;
		goto ice_aq_alloc_free_vsi_list_exit;
	}

	if (opc == ice_aqc_opc_free_res)
		sw_buf->elem[0].e.sw_resp = CPU_TO_LE16(*vsi_list_id);

	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len, opc, NULL);
	if (status)
		goto ice_aq_alloc_free_vsi_list_exit;

	if (opc == ice_aqc_opc_alloc_res) {
		vsi_ele = &sw_buf->elem[0];
		*vsi_list_id = LE16_TO_CPU(vsi_ele->e.sw_resp);
	}

ice_aq_alloc_free_vsi_list_exit:
	ice_free(hw, sw_buf);
	return status;
}

/**
 * ice_aq_set_storm_ctrl - Sets storm control configuration
 * @hw: pointer to the HW struct
 * @bcast_thresh: represents the upper threshold for broadcast storm control
 * @mcast_thresh: represents the upper threshold for multicast storm control
 * @ctl_bitmask: storm control control knobs
 *
 * Sets the storm control configuration (0x0280)
 */
enum ice_status
ice_aq_set_storm_ctrl(struct ice_hw *hw, u32 bcast_thresh, u32 mcast_thresh,
		      u32 ctl_bitmask)
{
	struct ice_aqc_storm_cfg *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.storm_conf;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_storm_cfg);

	cmd->bcast_thresh_size = CPU_TO_LE32(bcast_thresh & ICE_AQ_THRESHOLD_M);
	cmd->mcast_thresh_size = CPU_TO_LE32(mcast_thresh & ICE_AQ_THRESHOLD_M);
	cmd->storm_ctrl_ctrl = CPU_TO_LE32(ctl_bitmask);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_aq_get_storm_ctrl - gets storm control configuration
 * @hw: pointer to the HW struct
 * @bcast_thresh: represents the upper threshold for broadcast storm control
 * @mcast_thresh: represents the upper threshold for multicast storm control
 * @ctl_bitmask: storm control control knobs
 *
 * Gets the storm control configuration (0x0281)
 */
enum ice_status
ice_aq_get_storm_ctrl(struct ice_hw *hw, u32 *bcast_thresh, u32 *mcast_thresh,
		      u32 *ctl_bitmask)
{
	enum ice_status status;
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_storm_cfg);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	if (!status) {
		struct ice_aqc_storm_cfg *resp = &desc.params.storm_conf;

		if (bcast_thresh)
			*bcast_thresh = LE32_TO_CPU(resp->bcast_thresh_size) &
				ICE_AQ_THRESHOLD_M;
		if (mcast_thresh)
			*mcast_thresh = LE32_TO_CPU(resp->mcast_thresh_size) &
				ICE_AQ_THRESHOLD_M;
		if (ctl_bitmask)
			*ctl_bitmask = LE32_TO_CPU(resp->storm_ctrl_ctrl);
	}

	return status;
}

/**
 * ice_aq_sw_rules - add/update/remove switch rules
 * @hw: pointer to the HW struct
 * @rule_list: pointer to switch rule population list
 * @rule_list_sz: total size of the rule list in bytes
 * @num_rules: number of switch rules in the rule_list
 * @opc: switch rules population command type - pass in the command opcode
 * @cd: pointer to command details structure or NULL
 *
 * Add(0x02a0)/Update(0x02a1)/Remove(0x02a2) switch rules commands to firmware
 */
static enum ice_status
ice_aq_sw_rules(struct ice_hw *hw, void *rule_list, u16 rule_list_sz,
		u8 num_rules, enum ice_adminq_opc opc, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	enum ice_status status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	if (opc != ice_aqc_opc_add_sw_rules &&
	    opc != ice_aqc_opc_update_sw_rules &&
	    opc != ice_aqc_opc_remove_sw_rules)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, opc);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	desc.params.sw_rules.num_rules_fltr_entry_index =
		CPU_TO_LE16(num_rules);
	status = ice_aq_send_cmd(hw, &desc, rule_list, rule_list_sz, cd);
	if (opc != ice_aqc_opc_add_sw_rules &&
	    hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT)
		status = ICE_ERR_DOES_NOT_EXIST;

	return status;
}

/**
 * ice_aq_add_recipe - add switch recipe
 * @hw: pointer to the HW struct
 * @s_recipe_list: pointer to switch rule population list
 * @num_recipes: number of switch recipes in the list
 * @cd: pointer to command details structure or NULL
 *
 * Add(0x0290)
 */
enum ice_status
ice_aq_add_recipe(struct ice_hw *hw,
		  struct ice_aqc_recipe_data_elem *s_recipe_list,
		  u16 num_recipes, struct ice_sq_cd *cd)
{
	struct ice_aqc_add_get_recipe *cmd;
	struct ice_aq_desc desc;
	u16 buf_size;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);
	cmd = &desc.params.add_get_recipe;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_recipe);

	cmd->num_sub_recipes = CPU_TO_LE16(num_recipes);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	buf_size = num_recipes * sizeof(*s_recipe_list);

	return ice_aq_send_cmd(hw, &desc, s_recipe_list, buf_size, cd);
}

/**
 * ice_aq_get_recipe - get switch recipe
 * @hw: pointer to the HW struct
 * @s_recipe_list: pointer to switch rule population list
 * @num_recipes: pointer to the number of recipes (input and output)
 * @recipe_root: root recipe number of recipe(s) to retrieve
 * @cd: pointer to command details structure or NULL
 *
 * Get(0x0292)
 *
 * On input, *num_recipes should equal the number of entries in s_recipe_list.
 * On output, *num_recipes will equal the number of entries returned in
 * s_recipe_list.
 *
 * The caller must supply enough space in s_recipe_list to hold all possible
 * recipes and *num_recipes must equal ICE_MAX_NUM_RECIPES.
 */
enum ice_status
ice_aq_get_recipe(struct ice_hw *hw,
		  struct ice_aqc_recipe_data_elem *s_recipe_list,
		  u16 *num_recipes, u16 recipe_root, struct ice_sq_cd *cd)
{
	struct ice_aqc_add_get_recipe *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;
	u16 buf_size;

	if (*num_recipes != ICE_MAX_NUM_RECIPES)
		return ICE_ERR_PARAM;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);
	cmd = &desc.params.add_get_recipe;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_recipe);

	cmd->return_index = CPU_TO_LE16(recipe_root);
	cmd->num_sub_recipes = 0;

	buf_size = *num_recipes * sizeof(*s_recipe_list);

	status = ice_aq_send_cmd(hw, &desc, s_recipe_list, buf_size, cd);
	/* cppcheck-suppress constArgument */
	*num_recipes = LE16_TO_CPU(cmd->num_sub_recipes);

	return status;
}

/**
 * ice_aq_map_recipe_to_profile - Map recipe to packet profile
 * @hw: pointer to the HW struct
 * @profile_id: package profile ID to associate the recipe with
 * @r_bitmap: Recipe bitmap filled in and need to be returned as response
 * @cd: pointer to command details structure or NULL
 * Recipe to profile association (0x0291)
 */
enum ice_status
ice_aq_map_recipe_to_profile(struct ice_hw *hw, u32 profile_id, u8 *r_bitmap,
			     struct ice_sq_cd *cd)
{
	struct ice_aqc_recipe_to_profile *cmd;
	struct ice_aq_desc desc;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);
	cmd = &desc.params.recipe_to_profile;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_recipe_to_profile);
	cmd->profile_id = CPU_TO_LE16(profile_id);
	/* Set the recipe ID bit in the bitmask to let the device know which
	 * profile we are associating the recipe to
	 */
	ice_memcpy(cmd->recipe_assoc, r_bitmap, sizeof(cmd->recipe_assoc),
		   ICE_NONDMA_TO_NONDMA);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_get_recipe_to_profile - Map recipe to packet profile
 * @hw: pointer to the HW struct
 * @profile_id: package profile ID to associate the recipe with
 * @r_bitmap: Recipe bitmap filled in and need to be returned as response
 * @cd: pointer to command details structure or NULL
 * Associate profile ID with given recipe (0x0293)
 */
enum ice_status
ice_aq_get_recipe_to_profile(struct ice_hw *hw, u32 profile_id, u8 *r_bitmap,
			     struct ice_sq_cd *cd)
{
	struct ice_aqc_recipe_to_profile *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);
	cmd = &desc.params.recipe_to_profile;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_recipe_to_profile);
	cmd->profile_id = CPU_TO_LE16(profile_id);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (!status)
		ice_memcpy(r_bitmap, cmd->recipe_assoc,
			   sizeof(cmd->recipe_assoc), ICE_NONDMA_TO_NONDMA);

	return status;
}

/**
 * ice_alloc_recipe - add recipe resource
 * @hw: pointer to the hardware structure
 * @rid: recipe ID returned as response to AQ call
 */
enum ice_status ice_alloc_recipe(struct ice_hw *hw, u16 *rid)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	enum ice_status status;
	u16 buf_len;

	buf_len = sizeof(*sw_buf);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	sw_buf->num_elems = CPU_TO_LE16(1);
	sw_buf->res_type = CPU_TO_LE16((ICE_AQC_RES_TYPE_RECIPE <<
					ICE_AQC_RES_TYPE_S) |
					ICE_AQC_RES_TYPE_FLAG_SHARED);
	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);
	if (!status)
		*rid = LE16_TO_CPU(sw_buf->elem[0].e.sw_resp);
	ice_free(hw, sw_buf);

	return status;
}

/* ice_init_port_info - Initialize port_info with switch configuration data
 * @pi: pointer to port_info
 * @vsi_port_num: VSI number or port number
 * @type: Type of switch element (port or VSI)
 * @swid: switch ID of the switch the element is attached to
 * @pf_vf_num: PF or VF number
 * @is_vf: true if the element is a VF, false otherwise
 */
static void
ice_init_port_info(struct ice_port_info *pi, u16 vsi_port_num, u8 type,
		   u16 swid, u16 pf_vf_num, bool is_vf)
{
	switch (type) {
	case ICE_AQC_GET_SW_CONF_RESP_PHYS_PORT:
		pi->lport = (u8)(vsi_port_num & ICE_LPORT_MASK);
		pi->sw_id = swid;
		pi->pf_vf_num = pf_vf_num;
		pi->is_vf = is_vf;
		pi->dflt_tx_vsi_num = ICE_DFLT_VSI_INVAL;
		pi->dflt_rx_vsi_num = ICE_DFLT_VSI_INVAL;
		break;
	default:
		ice_debug(pi->hw, ICE_DBG_SW,
			  "incorrect VSI/port type received\n");
		break;
	}
}

/* ice_get_initial_sw_cfg - Get initial port and default VSI data
 * @hw: pointer to the hardware structure
 */
enum ice_status ice_get_initial_sw_cfg(struct ice_hw *hw)
{
	struct ice_aqc_get_sw_cfg_resp *rbuf;
	enum ice_status status;
	u8 num_total_ports;
	u16 req_desc = 0;
	u16 num_elems;
	u8 j = 0;
	u16 i;

	num_total_ports = 1;

	rbuf = (struct ice_aqc_get_sw_cfg_resp *)
		ice_malloc(hw, ICE_SW_CFG_MAX_BUF_LEN);

	if (!rbuf)
		return ICE_ERR_NO_MEMORY;

	/* Multiple calls to ice_aq_get_sw_cfg may be required
	 * to get all the switch configuration information. The need
	 * for additional calls is indicated by ice_aq_get_sw_cfg
	 * writing a non-zero value in req_desc
	 */
	do {
		status = ice_aq_get_sw_cfg(hw, rbuf, ICE_SW_CFG_MAX_BUF_LEN,
					   &req_desc, &num_elems, NULL);

		if (status)
			break;

		for (i = 0; i < num_elems; i++) {
			struct ice_aqc_get_sw_cfg_resp_elem *ele;
			u16 pf_vf_num, swid, vsi_port_num;
			bool is_vf = false;
			u8 res_type;

			ele = rbuf[i].elements;
			vsi_port_num = LE16_TO_CPU(ele->vsi_port_num) &
				ICE_AQC_GET_SW_CONF_RESP_VSI_PORT_NUM_M;

			pf_vf_num = LE16_TO_CPU(ele->pf_vf_num) &
				ICE_AQC_GET_SW_CONF_RESP_FUNC_NUM_M;

			swid = LE16_TO_CPU(ele->swid);

			if (LE16_TO_CPU(ele->pf_vf_num) &
			    ICE_AQC_GET_SW_CONF_RESP_IS_VF)
				is_vf = true;

			res_type = (u8)(LE16_TO_CPU(ele->vsi_port_num) >>
					ICE_AQC_GET_SW_CONF_RESP_TYPE_S);

			switch (res_type) {
			case ICE_AQC_GET_SW_CONF_RESP_PHYS_PORT:
			case ICE_AQC_GET_SW_CONF_RESP_VIRT_PORT:
				if (j == num_total_ports) {
					ice_debug(hw, ICE_DBG_SW,
						  "more ports than expected\n");
					status = ICE_ERR_CFG;
					goto out;
				}
				ice_init_port_info(hw->port_info,
						   vsi_port_num, res_type, swid,
						   pf_vf_num, is_vf);
				j++;
				break;
			default:
				break;
			}
		}
	} while (req_desc && !status);

out:
	ice_free(hw, (void *)rbuf);
	return status;
}

/**
 * ice_fill_sw_info - Helper function to populate lb_en and lan_en
 * @hw: pointer to the hardware structure
 * @fi: filter info structure to fill/update
 *
 * This helper function populates the lb_en and lan_en elements of the provided
 * ice_fltr_info struct using the switch's type and characteristics of the
 * switch rule being configured.
 */
static void ice_fill_sw_info(struct ice_hw *hw, struct ice_fltr_info *fi)
{
	if ((fi->flag & ICE_FLTR_RX) &&
	    (fi->fltr_act == ICE_FWD_TO_VSI ||
	     fi->fltr_act == ICE_FWD_TO_VSI_LIST) &&
	    fi->lkup_type == ICE_SW_LKUP_LAST)
		fi->lan_en = true;
	fi->lb_en = false;
	fi->lan_en = false;
	if ((fi->flag & ICE_FLTR_TX) &&
	    (fi->fltr_act == ICE_FWD_TO_VSI ||
	     fi->fltr_act == ICE_FWD_TO_VSI_LIST ||
	     fi->fltr_act == ICE_FWD_TO_Q ||
	     fi->fltr_act == ICE_FWD_TO_QGRP)) {
		/* Setting LB for prune actions will result in replicated
		 * packets to the internal switch that will be dropped.
		 */
		if (fi->lkup_type != ICE_SW_LKUP_VLAN)
			fi->lb_en = true;

		/* Set lan_en to TRUE if
		 * 1. The switch is a VEB AND
		 * 2
		 * 2.1 The lookup is a directional lookup like ethertype,
		 * promiscuous, ethertype-MAC, promiscuous-VLAN
		 * and default-port OR
		 * 2.2 The lookup is VLAN, OR
		 * 2.3 The lookup is MAC with mcast or bcast addr for MAC, OR
		 * 2.4 The lookup is MAC_VLAN with mcast or bcast addr for MAC.
		 *
		 * OR
		 *
		 * The switch is a VEPA.
		 *
		 * In all other cases, the LAN enable has to be set to false.
		 */
		if (hw->evb_veb) {
			if (fi->lkup_type == ICE_SW_LKUP_ETHERTYPE ||
			    fi->lkup_type == ICE_SW_LKUP_PROMISC ||
			    fi->lkup_type == ICE_SW_LKUP_ETHERTYPE_MAC ||
			    fi->lkup_type == ICE_SW_LKUP_PROMISC_VLAN ||
			    fi->lkup_type == ICE_SW_LKUP_DFLT ||
			    fi->lkup_type == ICE_SW_LKUP_VLAN ||
			    (fi->lkup_type == ICE_SW_LKUP_MAC &&
			     !IS_UNICAST_ETHER_ADDR(fi->l_data.mac.mac_addr)) ||
			    (fi->lkup_type == ICE_SW_LKUP_MAC_VLAN &&
			     !IS_UNICAST_ETHER_ADDR(fi->l_data.mac.mac_addr)))
				fi->lan_en = true;
		} else {
			fi->lan_en = true;
		}
	}
}

/**
 * ice_fill_sw_rule - Helper function to fill switch rule structure
 * @hw: pointer to the hardware structure
 * @f_info: entry containing packet forwarding information
 * @s_rule: switch rule structure to be filled in based on mac_entry
 * @opc: switch rules population command type - pass in the command opcode
 */
static void
ice_fill_sw_rule(struct ice_hw *hw, struct ice_fltr_info *f_info,
		 struct ice_aqc_sw_rules_elem *s_rule, enum ice_adminq_opc opc)
{
	u16 vlan_id = ICE_MAX_VLAN_ID + 1;
	void *daddr = NULL;
	u16 eth_hdr_sz;
	u8 *eth_hdr;
	u32 act = 0;
	__be16 *off;
	u8 q_rgn;

	if (opc == ice_aqc_opc_remove_sw_rules) {
		s_rule->pdata.lkup_tx_rx.act = 0;
		s_rule->pdata.lkup_tx_rx.index =
			CPU_TO_LE16(f_info->fltr_rule_id);
		s_rule->pdata.lkup_tx_rx.hdr_len = 0;
		return;
	}

	eth_hdr_sz = sizeof(dummy_eth_header);
	eth_hdr = s_rule->pdata.lkup_tx_rx.hdr;

	/* initialize the ether header with a dummy header */
	ice_memcpy(eth_hdr, dummy_eth_header, eth_hdr_sz, ICE_NONDMA_TO_NONDMA);
	ice_fill_sw_info(hw, f_info);

	switch (f_info->fltr_act) {
	case ICE_FWD_TO_VSI:
		act |= (f_info->fwd_id.hw_vsi_id << ICE_SINGLE_ACT_VSI_ID_S) &
			ICE_SINGLE_ACT_VSI_ID_M;
		if (f_info->lkup_type != ICE_SW_LKUP_VLAN)
			act |= ICE_SINGLE_ACT_VSI_FORWARDING |
				ICE_SINGLE_ACT_VALID_BIT;
		break;
	case ICE_FWD_TO_VSI_LIST:
		act |= ICE_SINGLE_ACT_VSI_LIST;
		act |= (f_info->fwd_id.vsi_list_id <<
			ICE_SINGLE_ACT_VSI_LIST_ID_S) &
			ICE_SINGLE_ACT_VSI_LIST_ID_M;
		if (f_info->lkup_type != ICE_SW_LKUP_VLAN)
			act |= ICE_SINGLE_ACT_VSI_FORWARDING |
				ICE_SINGLE_ACT_VALID_BIT;
		break;
	case ICE_FWD_TO_Q:
		act |= ICE_SINGLE_ACT_TO_Q;
		act |= (f_info->fwd_id.q_id << ICE_SINGLE_ACT_Q_INDEX_S) &
			ICE_SINGLE_ACT_Q_INDEX_M;
		break;
	case ICE_DROP_PACKET:
		act |= ICE_SINGLE_ACT_VSI_FORWARDING | ICE_SINGLE_ACT_DROP |
			ICE_SINGLE_ACT_VALID_BIT;
		break;
	case ICE_FWD_TO_QGRP:
		q_rgn = f_info->qgrp_size > 0 ?
			(u8)ice_ilog2(f_info->qgrp_size) : 0;
		act |= ICE_SINGLE_ACT_TO_Q;
		act |= (f_info->fwd_id.q_id << ICE_SINGLE_ACT_Q_INDEX_S) &
			ICE_SINGLE_ACT_Q_INDEX_M;
		act |= (q_rgn << ICE_SINGLE_ACT_Q_REGION_S) &
			ICE_SINGLE_ACT_Q_REGION_M;
		break;
	default:
		return;
	}

	if (f_info->lb_en)
		act |= ICE_SINGLE_ACT_LB_ENABLE;
	if (f_info->lan_en)
		act |= ICE_SINGLE_ACT_LAN_ENABLE;

	switch (f_info->lkup_type) {
	case ICE_SW_LKUP_MAC:
		daddr = f_info->l_data.mac.mac_addr;
		break;
	case ICE_SW_LKUP_VLAN:
		vlan_id = f_info->l_data.vlan.vlan_id;
		if (f_info->fltr_act == ICE_FWD_TO_VSI ||
		    f_info->fltr_act == ICE_FWD_TO_VSI_LIST) {
			act |= ICE_SINGLE_ACT_PRUNE;
			act |= ICE_SINGLE_ACT_EGRESS | ICE_SINGLE_ACT_INGRESS;
		}
		break;
	case ICE_SW_LKUP_ETHERTYPE_MAC:
		daddr = f_info->l_data.ethertype_mac.mac_addr;
		/* fall-through */
	case ICE_SW_LKUP_ETHERTYPE:
		off = (_FORCE_ __be16 *)(eth_hdr + ICE_ETH_ETHTYPE_OFFSET);
		*off = CPU_TO_BE16(f_info->l_data.ethertype_mac.ethertype);
		break;
	case ICE_SW_LKUP_MAC_VLAN:
		daddr = f_info->l_data.mac_vlan.mac_addr;
		vlan_id = f_info->l_data.mac_vlan.vlan_id;
		break;
	case ICE_SW_LKUP_PROMISC_VLAN:
		vlan_id = f_info->l_data.mac_vlan.vlan_id;
		/* fall-through */
	case ICE_SW_LKUP_PROMISC:
		daddr = f_info->l_data.mac_vlan.mac_addr;
		break;
	default:
		break;
	}

	s_rule->type = (f_info->flag & ICE_FLTR_RX) ?
		CPU_TO_LE16(ICE_AQC_SW_RULES_T_LKUP_RX) :
		CPU_TO_LE16(ICE_AQC_SW_RULES_T_LKUP_TX);

	/* Recipe set depending on lookup type */
	s_rule->pdata.lkup_tx_rx.recipe_id = CPU_TO_LE16(f_info->lkup_type);
	s_rule->pdata.lkup_tx_rx.src = CPU_TO_LE16(f_info->src);
	s_rule->pdata.lkup_tx_rx.act = CPU_TO_LE32(act);

	if (daddr)
		ice_memcpy(eth_hdr + ICE_ETH_DA_OFFSET, daddr, ETH_ALEN,
			   ICE_NONDMA_TO_NONDMA);

	if (!(vlan_id > ICE_MAX_VLAN_ID)) {
		off = (_FORCE_ __be16 *)(eth_hdr + ICE_ETH_VLAN_TCI_OFFSET);
		*off = CPU_TO_BE16(vlan_id);
	}

	/* Create the switch rule with the final dummy Ethernet header */
	if (opc != ice_aqc_opc_update_sw_rules)
		s_rule->pdata.lkup_tx_rx.hdr_len = CPU_TO_LE16(eth_hdr_sz);
}

/**
 * ice_add_marker_act
 * @hw: pointer to the hardware structure
 * @m_ent: the management entry for which sw marker needs to be added
 * @sw_marker: sw marker to tag the Rx descriptor with
 * @l_id: large action resource ID
 *
 * Create a large action to hold software marker and update the switch rule
 * entry pointed by m_ent with newly created large action
 */
static enum ice_status
ice_add_marker_act(struct ice_hw *hw, struct ice_fltr_mgmt_list_entry *m_ent,
		   u16 sw_marker, u16 l_id)
{
	struct ice_aqc_sw_rules_elem *lg_act, *rx_tx;
	/* For software marker we need 3 large actions
	 * 1. FWD action: FWD TO VSI or VSI LIST
	 * 2. GENERIC VALUE action to hold the profile ID
	 * 3. GENERIC VALUE action to hold the software marker ID
	 */
	const u16 num_lg_acts = 3;
	enum ice_status status;
	u16 lg_act_size;
	u16 rules_size;
	u32 act;
	u16 id;

	if (m_ent->fltr_info.lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	/* Create two back-to-back switch rules and submit them to the HW using
	 * one memory buffer:
	 *    1. Large Action
	 *    2. Look up Tx Rx
	 */
	lg_act_size = (u16)ICE_SW_RULE_LG_ACT_SIZE(num_lg_acts);
	rules_size = lg_act_size + ICE_SW_RULE_RX_TX_ETH_HDR_SIZE;
	lg_act = (struct ice_aqc_sw_rules_elem *)ice_malloc(hw, rules_size);
	if (!lg_act)
		return ICE_ERR_NO_MEMORY;

	rx_tx = (struct ice_aqc_sw_rules_elem *)((u8 *)lg_act + lg_act_size);

	/* Fill in the first switch rule i.e. large action */
	lg_act->type = CPU_TO_LE16(ICE_AQC_SW_RULES_T_LG_ACT);
	lg_act->pdata.lg_act.index = CPU_TO_LE16(l_id);
	lg_act->pdata.lg_act.size = CPU_TO_LE16(num_lg_acts);

	/* First action VSI forwarding or VSI list forwarding depending on how
	 * many VSIs
	 */
	id = (m_ent->vsi_count > 1) ? m_ent->fltr_info.fwd_id.vsi_list_id :
		m_ent->fltr_info.fwd_id.hw_vsi_id;

	act = ICE_LG_ACT_VSI_FORWARDING | ICE_LG_ACT_VALID_BIT;
	act |= (id << ICE_LG_ACT_VSI_LIST_ID_S) &
		ICE_LG_ACT_VSI_LIST_ID_M;
	if (m_ent->vsi_count > 1)
		act |= ICE_LG_ACT_VSI_LIST;
	lg_act->pdata.lg_act.act[0] = CPU_TO_LE32(act);

	/* Second action descriptor type */
	act = ICE_LG_ACT_GENERIC;

	act |= (1 << ICE_LG_ACT_GENERIC_VALUE_S) & ICE_LG_ACT_GENERIC_VALUE_M;
	lg_act->pdata.lg_act.act[1] = CPU_TO_LE32(act);

	act = (ICE_LG_ACT_GENERIC_OFF_RX_DESC_PROF_IDX <<
	       ICE_LG_ACT_GENERIC_OFFSET_S) & ICE_LG_ACT_GENERIC_OFFSET_M;

	/* Third action Marker value */
	act |= ICE_LG_ACT_GENERIC;
	act |= (sw_marker << ICE_LG_ACT_GENERIC_VALUE_S) &
		ICE_LG_ACT_GENERIC_VALUE_M;

	lg_act->pdata.lg_act.act[2] = CPU_TO_LE32(act);

	/* call the fill switch rule to fill the lookup Tx Rx structure */
	ice_fill_sw_rule(hw, &m_ent->fltr_info, rx_tx,
			 ice_aqc_opc_update_sw_rules);

	/* Update the action to point to the large action ID */
	rx_tx->pdata.lkup_tx_rx.act =
		CPU_TO_LE32(ICE_SINGLE_ACT_PTR |
			    ((l_id << ICE_SINGLE_ACT_PTR_VAL_S) &
			     ICE_SINGLE_ACT_PTR_VAL_M));

	/* Use the filter rule ID of the previously created rule with single
	 * act. Once the update happens, hardware will treat this as large
	 * action
	 */
	rx_tx->pdata.lkup_tx_rx.index =
		CPU_TO_LE16(m_ent->fltr_info.fltr_rule_id);

	status = ice_aq_sw_rules(hw, lg_act, rules_size, 2,
				 ice_aqc_opc_update_sw_rules, NULL);
	if (!status) {
		m_ent->lg_act_idx = l_id;
		m_ent->sw_marker_id = sw_marker;
	}

	ice_free(hw, lg_act);
	return status;
}

/**
 * ice_add_counter_act - add/update filter rule with counter action
 * @hw: pointer to the hardware structure
 * @m_ent: the management entry for which counter needs to be added
 * @counter_id: VLAN counter ID returned as part of allocate resource
 * @l_id: large action resource ID
 */
static enum ice_status
ice_add_counter_act(struct ice_hw *hw, struct ice_fltr_mgmt_list_entry *m_ent,
		    u16 counter_id, u16 l_id)
{
	struct ice_aqc_sw_rules_elem *lg_act;
	struct ice_aqc_sw_rules_elem *rx_tx;
	enum ice_status status;
	/* 2 actions will be added while adding a large action counter */
	const int num_acts = 2;
	u16 lg_act_size;
	u16 rules_size;
	u16 f_rule_id;
	u32 act;
	u16 id;

	if (m_ent->fltr_info.lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	/* Create two back-to-back switch rules and submit them to the HW using
	 * one memory buffer:
	 * 1. Large Action
	 * 2. Look up Tx Rx
	 */
	lg_act_size = (u16)ICE_SW_RULE_LG_ACT_SIZE(num_acts);
	rules_size = lg_act_size + ICE_SW_RULE_RX_TX_ETH_HDR_SIZE;
	lg_act = (struct ice_aqc_sw_rules_elem *)ice_malloc(hw,
								 rules_size);
	if (!lg_act)
		return ICE_ERR_NO_MEMORY;

	rx_tx = (struct ice_aqc_sw_rules_elem *)
		((u8 *)lg_act + lg_act_size);

	/* Fill in the first switch rule i.e. large action */
	lg_act->type = CPU_TO_LE16(ICE_AQC_SW_RULES_T_LG_ACT);
	lg_act->pdata.lg_act.index = CPU_TO_LE16(l_id);
	lg_act->pdata.lg_act.size = CPU_TO_LE16(num_acts);

	/* First action VSI forwarding or VSI list forwarding depending on how
	 * many VSIs
	 */
	id = (m_ent->vsi_count > 1) ?  m_ent->fltr_info.fwd_id.vsi_list_id :
		m_ent->fltr_info.fwd_id.hw_vsi_id;

	act = ICE_LG_ACT_VSI_FORWARDING | ICE_LG_ACT_VALID_BIT;
	act |= (id << ICE_LG_ACT_VSI_LIST_ID_S) &
		ICE_LG_ACT_VSI_LIST_ID_M;
	if (m_ent->vsi_count > 1)
		act |= ICE_LG_ACT_VSI_LIST;
	lg_act->pdata.lg_act.act[0] = CPU_TO_LE32(act);

	/* Second action counter ID */
	act = ICE_LG_ACT_STAT_COUNT;
	act |= (counter_id << ICE_LG_ACT_STAT_COUNT_S) &
		ICE_LG_ACT_STAT_COUNT_M;
	lg_act->pdata.lg_act.act[1] = CPU_TO_LE32(act);

	/* call the fill switch rule to fill the lookup Tx Rx structure */
	ice_fill_sw_rule(hw, &m_ent->fltr_info, rx_tx,
			 ice_aqc_opc_update_sw_rules);

	act = ICE_SINGLE_ACT_PTR;
	act |= (l_id << ICE_SINGLE_ACT_PTR_VAL_S) & ICE_SINGLE_ACT_PTR_VAL_M;
	rx_tx->pdata.lkup_tx_rx.act = CPU_TO_LE32(act);

	/* Use the filter rule ID of the previously created rule with single
	 * act. Once the update happens, hardware will treat this as large
	 * action
	 */
	f_rule_id = m_ent->fltr_info.fltr_rule_id;
	rx_tx->pdata.lkup_tx_rx.index = CPU_TO_LE16(f_rule_id);

	status = ice_aq_sw_rules(hw, lg_act, rules_size, 2,
				 ice_aqc_opc_update_sw_rules, NULL);
	if (!status) {
		m_ent->lg_act_idx = l_id;
		m_ent->counter_index = counter_id;
	}

	ice_free(hw, lg_act);
	return status;
}

/**
 * ice_create_vsi_list_map
 * @hw: pointer to the hardware structure
 * @vsi_handle_arr: array of VSI handles to set in the VSI mapping
 * @num_vsi: number of VSI handles in the array
 * @vsi_list_id: VSI list ID generated as part of allocate resource
 *
 * Helper function to create a new entry of VSI list ID to VSI mapping
 * using the given VSI list ID
 */
static struct ice_vsi_list_map_info *
ice_create_vsi_list_map(struct ice_hw *hw, u16 *vsi_handle_arr, u16 num_vsi,
			u16 vsi_list_id)
{
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_vsi_list_map_info *v_map;
	int i;

	v_map = (struct ice_vsi_list_map_info *)ice_calloc(hw, 1,
		sizeof(*v_map));
	if (!v_map)
		return NULL;

	v_map->vsi_list_id = vsi_list_id;
	v_map->ref_cnt = 1;
	for (i = 0; i < num_vsi; i++)
		ice_set_bit(vsi_handle_arr[i], v_map->vsi_map);

	LIST_ADD(&v_map->list_entry, &sw->vsi_list_map_head);
	return v_map;
}

/**
 * ice_update_vsi_list_rule
 * @hw: pointer to the hardware structure
 * @vsi_handle_arr: array of VSI handles to form a VSI list
 * @num_vsi: number of VSI handles in the array
 * @vsi_list_id: VSI list ID generated as part of allocate resource
 * @remove: Boolean value to indicate if this is a remove action
 * @opc: switch rules population command type - pass in the command opcode
 * @lkup_type: lookup type of the filter
 *
 * Call AQ command to add a new switch rule or update existing switch rule
 * using the given VSI list ID
 */
static enum ice_status
ice_update_vsi_list_rule(struct ice_hw *hw, u16 *vsi_handle_arr, u16 num_vsi,
			 u16 vsi_list_id, bool remove, enum ice_adminq_opc opc,
			 enum ice_sw_lkup_type lkup_type)
{
	struct ice_aqc_sw_rules_elem *s_rule;
	enum ice_status status;
	u16 s_rule_size;
	u16 rule_type;
	int i;

	if (!num_vsi)
		return ICE_ERR_PARAM;

	if (lkup_type == ICE_SW_LKUP_MAC ||
	    lkup_type == ICE_SW_LKUP_MAC_VLAN ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE_MAC ||
	    lkup_type == ICE_SW_LKUP_PROMISC ||
	    lkup_type == ICE_SW_LKUP_PROMISC_VLAN ||
	    lkup_type == ICE_SW_LKUP_LAST)
		rule_type = remove ? ICE_AQC_SW_RULES_T_VSI_LIST_CLEAR :
			ICE_AQC_SW_RULES_T_VSI_LIST_SET;
	else if (lkup_type == ICE_SW_LKUP_VLAN)
		rule_type = remove ? ICE_AQC_SW_RULES_T_PRUNE_LIST_CLEAR :
			ICE_AQC_SW_RULES_T_PRUNE_LIST_SET;
	else
		return ICE_ERR_PARAM;

	s_rule_size = (u16)ICE_SW_RULE_VSI_LIST_SIZE(num_vsi);
	s_rule = (struct ice_aqc_sw_rules_elem *)ice_malloc(hw, s_rule_size);
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;
	for (i = 0; i < num_vsi; i++) {
		if (!ice_is_vsi_valid(hw, vsi_handle_arr[i])) {
			status = ICE_ERR_PARAM;
			goto exit;
		}
		/* AQ call requires hw_vsi_id(s) */
		s_rule->pdata.vsi_list.vsi[i] =
			CPU_TO_LE16(ice_get_hw_vsi_num(hw, vsi_handle_arr[i]));
	}

	s_rule->type = CPU_TO_LE16(rule_type);
	s_rule->pdata.vsi_list.number_vsi = CPU_TO_LE16(num_vsi);
	s_rule->pdata.vsi_list.index = CPU_TO_LE16(vsi_list_id);

	status = ice_aq_sw_rules(hw, s_rule, s_rule_size, 1, opc, NULL);

exit:
	ice_free(hw, s_rule);
	return status;
}

/**
 * ice_create_vsi_list_rule - Creates and populates a VSI list rule
 * @hw: pointer to the HW struct
 * @vsi_handle_arr: array of VSI handles to form a VSI list
 * @num_vsi: number of VSI handles in the array
 * @vsi_list_id: stores the ID of the VSI list to be created
 * @lkup_type: switch rule filter's lookup type
 */
static enum ice_status
ice_create_vsi_list_rule(struct ice_hw *hw, u16 *vsi_handle_arr, u16 num_vsi,
			 u16 *vsi_list_id, enum ice_sw_lkup_type lkup_type)
{
	enum ice_status status;

	status = ice_aq_alloc_free_vsi_list(hw, vsi_list_id, lkup_type,
					    ice_aqc_opc_alloc_res);
	if (status)
		return status;

	/* Update the newly created VSI list to include the specified VSIs */
	return ice_update_vsi_list_rule(hw, vsi_handle_arr, num_vsi,
					*vsi_list_id, false,
					ice_aqc_opc_add_sw_rules, lkup_type);
}

/**
 * ice_create_pkt_fwd_rule
 * @hw: pointer to the hardware structure
 * @recp_list: corresponding filter management list
 * @f_entry: entry containing packet forwarding information
 *
 * Create switch rule with given filter information and add an entry
 * to the corresponding filter management list to track this switch rule
 * and VSI mapping
 */
static enum ice_status
ice_create_pkt_fwd_rule(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
			struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_mgmt_list_entry *fm_entry;
	struct ice_aqc_sw_rules_elem *s_rule;
	enum ice_status status;

	s_rule = (struct ice_aqc_sw_rules_elem *)
		ice_malloc(hw, ICE_SW_RULE_RX_TX_ETH_HDR_SIZE);
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;
	fm_entry = (struct ice_fltr_mgmt_list_entry *)
		   ice_malloc(hw, sizeof(*fm_entry));
	if (!fm_entry) {
		status = ICE_ERR_NO_MEMORY;
		goto ice_create_pkt_fwd_rule_exit;
	}

	fm_entry->fltr_info = f_entry->fltr_info;

	/* Initialize all the fields for the management entry */
	fm_entry->vsi_count = 1;
	fm_entry->lg_act_idx = ICE_INVAL_LG_ACT_INDEX;
	fm_entry->sw_marker_id = ICE_INVAL_SW_MARKER_ID;
	fm_entry->counter_index = ICE_INVAL_COUNTER_ID;

	ice_fill_sw_rule(hw, &fm_entry->fltr_info, s_rule,
			 ice_aqc_opc_add_sw_rules);

	status = ice_aq_sw_rules(hw, s_rule, ICE_SW_RULE_RX_TX_ETH_HDR_SIZE, 1,
				 ice_aqc_opc_add_sw_rules, NULL);
	if (status) {
		ice_free(hw, fm_entry);
		goto ice_create_pkt_fwd_rule_exit;
	}

	f_entry->fltr_info.fltr_rule_id =
		LE16_TO_CPU(s_rule->pdata.lkup_tx_rx.index);
	fm_entry->fltr_info.fltr_rule_id =
		LE16_TO_CPU(s_rule->pdata.lkup_tx_rx.index);

	/* The book keeping entries will get removed when base driver
	 * calls remove filter AQ command
	 */
	LIST_ADD(&fm_entry->list_entry, &recp_list->filt_rules);

ice_create_pkt_fwd_rule_exit:
	ice_free(hw, s_rule);
	return status;
}

/**
 * ice_update_pkt_fwd_rule
 * @hw: pointer to the hardware structure
 * @f_info: filter information for switch rule
 *
 * Call AQ command to update a previously created switch rule with a
 * VSI list ID
 */
static enum ice_status
ice_update_pkt_fwd_rule(struct ice_hw *hw, struct ice_fltr_info *f_info)
{
	struct ice_aqc_sw_rules_elem *s_rule;
	enum ice_status status;

	s_rule = (struct ice_aqc_sw_rules_elem *)
		ice_malloc(hw, ICE_SW_RULE_RX_TX_ETH_HDR_SIZE);
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;

	ice_fill_sw_rule(hw, f_info, s_rule, ice_aqc_opc_update_sw_rules);

	s_rule->pdata.lkup_tx_rx.index = CPU_TO_LE16(f_info->fltr_rule_id);

	/* Update switch rule with new rule set to forward VSI list */
	status = ice_aq_sw_rules(hw, s_rule, ICE_SW_RULE_RX_TX_ETH_HDR_SIZE, 1,
				 ice_aqc_opc_update_sw_rules, NULL);

	ice_free(hw, s_rule);
	return status;
}

/**
 * ice_update_sw_rule_bridge_mode
 * @hw: pointer to the HW struct
 *
 * Updates unicast switch filter rules based on VEB/VEPA mode
 */
enum ice_status ice_update_sw_rule_bridge_mode(struct ice_hw *hw)
{
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_fltr_mgmt_list_entry *fm_entry;
	enum ice_status status = ICE_SUCCESS;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */

	rule_lock = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rule_lock;
	rule_head = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rules;

	ice_acquire_lock(rule_lock);
	LIST_FOR_EACH_ENTRY(fm_entry, rule_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		struct ice_fltr_info *fi = &fm_entry->fltr_info;
		u8 *addr = fi->l_data.mac.mac_addr;

		/* Update unicast Tx rules to reflect the selected
		 * VEB/VEPA mode
		 */
		if ((fi->flag & ICE_FLTR_TX) && IS_UNICAST_ETHER_ADDR(addr) &&
		    (fi->fltr_act == ICE_FWD_TO_VSI ||
		     fi->fltr_act == ICE_FWD_TO_VSI_LIST ||
		     fi->fltr_act == ICE_FWD_TO_Q ||
		     fi->fltr_act == ICE_FWD_TO_QGRP)) {
			status = ice_update_pkt_fwd_rule(hw, fi);
			if (status)
				break;
		}
	}

	ice_release_lock(rule_lock);

	return status;
}

/**
 * ice_add_update_vsi_list
 * @hw: pointer to the hardware structure
 * @m_entry: pointer to current filter management list entry
 * @cur_fltr: filter information from the book keeping entry
 * @new_fltr: filter information with the new VSI to be added
 *
 * Call AQ command to add or update previously created VSI list with new VSI.
 *
 * Helper function to do book keeping associated with adding filter information
 * The algorithm to do the book keeping is described below :
 * When a VSI needs to subscribe to a given filter (MAC/VLAN/Ethtype etc.)
 *	if only one VSI has been added till now
 *		Allocate a new VSI list and add two VSIs
 *		to this list using switch rule command
 *		Update the previously created switch rule with the
 *		newly created VSI list ID
 *	if a VSI list was previously created
 *		Add the new VSI to the previously created VSI list set
 *		using the update switch rule command
 */
static enum ice_status
ice_add_update_vsi_list(struct ice_hw *hw,
			struct ice_fltr_mgmt_list_entry *m_entry,
			struct ice_fltr_info *cur_fltr,
			struct ice_fltr_info *new_fltr)
{
	enum ice_status status = ICE_SUCCESS;
	u16 vsi_list_id = 0;

	if ((cur_fltr->fltr_act == ICE_FWD_TO_Q ||
	     cur_fltr->fltr_act == ICE_FWD_TO_QGRP))
		return ICE_ERR_NOT_IMPL;

	if ((new_fltr->fltr_act == ICE_FWD_TO_Q ||
	     new_fltr->fltr_act == ICE_FWD_TO_QGRP) &&
	    (cur_fltr->fltr_act == ICE_FWD_TO_VSI ||
	     cur_fltr->fltr_act == ICE_FWD_TO_VSI_LIST))
		return ICE_ERR_NOT_IMPL;

	if (m_entry->vsi_count < 2 && !m_entry->vsi_list_info) {
		/* Only one entry existed in the mapping and it was not already
		 * a part of a VSI list. So, create a VSI list with the old and
		 * new VSIs.
		 */
		struct ice_fltr_info tmp_fltr;
		u16 vsi_handle_arr[2];

		/* A rule already exists with the new VSI being added */
		if (cur_fltr->fwd_id.hw_vsi_id == new_fltr->fwd_id.hw_vsi_id)
			return ICE_ERR_ALREADY_EXISTS;

		vsi_handle_arr[0] = cur_fltr->vsi_handle;
		vsi_handle_arr[1] = new_fltr->vsi_handle;
		status = ice_create_vsi_list_rule(hw, &vsi_handle_arr[0], 2,
						  &vsi_list_id,
						  new_fltr->lkup_type);
		if (status)
			return status;

		tmp_fltr = *new_fltr;
		tmp_fltr.fltr_rule_id = cur_fltr->fltr_rule_id;
		tmp_fltr.fltr_act = ICE_FWD_TO_VSI_LIST;
		tmp_fltr.fwd_id.vsi_list_id = vsi_list_id;
		/* Update the previous switch rule of "MAC forward to VSI" to
		 * "MAC fwd to VSI list"
		 */
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr);
		if (status)
			return status;

		cur_fltr->fwd_id.vsi_list_id = vsi_list_id;
		cur_fltr->fltr_act = ICE_FWD_TO_VSI_LIST;
		m_entry->vsi_list_info =
			ice_create_vsi_list_map(hw, &vsi_handle_arr[0], 2,
						vsi_list_id);

		/* If this entry was large action then the large action needs
		 * to be updated to point to FWD to VSI list
		 */
		if (m_entry->sw_marker_id != ICE_INVAL_SW_MARKER_ID)
			status =
			    ice_add_marker_act(hw, m_entry,
					       m_entry->sw_marker_id,
					       m_entry->lg_act_idx);
	} else {
		u16 vsi_handle = new_fltr->vsi_handle;
		enum ice_adminq_opc opcode;

		if (!m_entry->vsi_list_info)
			return ICE_ERR_CFG;

		/* A rule already exists with the new VSI being added */
		if (ice_is_bit_set(m_entry->vsi_list_info->vsi_map, vsi_handle))
			return ICE_SUCCESS;

		/* Update the previously created VSI list set with
		 * the new VSI ID passed in
		 */
		vsi_list_id = cur_fltr->fwd_id.vsi_list_id;
		opcode = ice_aqc_opc_update_sw_rules;

		status = ice_update_vsi_list_rule(hw, &vsi_handle, 1,
						  vsi_list_id, false, opcode,
						  new_fltr->lkup_type);
		/* update VSI list mapping info with new VSI ID */
		if (!status)
			ice_set_bit(vsi_handle,
				    m_entry->vsi_list_info->vsi_map);
	}
	if (!status)
		m_entry->vsi_count++;
	return status;
}

/**
 * ice_find_rule_entry - Search a rule entry
 * @list_head: head of rule list
 * @f_info: rule information
 *
 * Helper function to search for a given rule entry
 * Returns pointer to entry storing the rule if found
 */
static struct ice_fltr_mgmt_list_entry *
ice_find_rule_entry(struct LIST_HEAD_TYPE *list_head,
		    struct ice_fltr_info *f_info)
{
	struct ice_fltr_mgmt_list_entry *list_itr, *ret = NULL;

	LIST_FOR_EACH_ENTRY(list_itr, list_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		if (!memcmp(&f_info->l_data, &list_itr->fltr_info.l_data,
			    sizeof(f_info->l_data)) &&
		    f_info->flag == list_itr->fltr_info.flag) {
			ret = list_itr;
			break;
		}
	}
	return ret;
}

/**
 * ice_find_vsi_list_entry - Search VSI list map with VSI count 1
 * @recp_list: VSI lists needs to be searched
 * @vsi_handle: VSI handle to be found in VSI list
 * @vsi_list_id: VSI list ID found containing vsi_handle
 *
 * Helper function to search a VSI list with single entry containing given VSI
 * handle element. This can be extended further to search VSI list with more
 * than 1 vsi_count. Returns pointer to VSI list entry if found.
 */
static struct ice_vsi_list_map_info *
ice_find_vsi_list_entry(struct ice_sw_recipe *recp_list, u16 vsi_handle,
			u16 *vsi_list_id)
{
	struct ice_vsi_list_map_info *map_info = NULL;
	struct LIST_HEAD_TYPE *list_head;

	list_head = &recp_list->filt_rules;
	if (recp_list->adv_rule) {
		struct ice_adv_fltr_mgmt_list_entry *list_itr;

		LIST_FOR_EACH_ENTRY(list_itr, list_head,
				    ice_adv_fltr_mgmt_list_entry,
				    list_entry) {
			if (list_itr->vsi_list_info) {
				map_info = list_itr->vsi_list_info;
				if (ice_is_bit_set(map_info->vsi_map,
						   vsi_handle)) {
					*vsi_list_id = map_info->vsi_list_id;
					return map_info;
				}
			}
		}
	} else {
		struct ice_fltr_mgmt_list_entry *list_itr;

		LIST_FOR_EACH_ENTRY(list_itr, list_head,
				    ice_fltr_mgmt_list_entry,
				    list_entry) {
			if (list_itr->vsi_count == 1 &&
			    list_itr->vsi_list_info) {
				map_info = list_itr->vsi_list_info;
				if (ice_is_bit_set(map_info->vsi_map,
						   vsi_handle)) {
					*vsi_list_id = map_info->vsi_list_id;
					return map_info;
				}
			}
		}
	}
	return NULL;
}

/**
 * ice_add_rule_internal - add rule for a given lookup type
 * @hw: pointer to the hardware structure
 * @recp_list: recipe list for which rule has to be added
 * @lport: logic port number on which function add rule
 * @f_entry: structure containing MAC forwarding information
 *
 * Adds or updates the rule lists for a given recipe
 */
static enum ice_status
ice_add_rule_internal(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
		      u8 lport, struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_info *new_fltr, *cur_fltr;
	struct ice_fltr_mgmt_list_entry *m_entry;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;

	if (!ice_is_vsi_valid(hw, f_entry->fltr_info.vsi_handle))
		return ICE_ERR_PARAM;

	/* Load the hw_vsi_id only if the fwd action is fwd to VSI */
	if (f_entry->fltr_info.fltr_act == ICE_FWD_TO_VSI)
		f_entry->fltr_info.fwd_id.hw_vsi_id =
			ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);

	rule_lock = &recp_list->filt_rule_lock;

	ice_acquire_lock(rule_lock);
	new_fltr = &f_entry->fltr_info;
	if (new_fltr->flag & ICE_FLTR_RX)
		new_fltr->src = lport;
	else if (new_fltr->flag & ICE_FLTR_TX)
		new_fltr->src =
			ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);

	m_entry = ice_find_rule_entry(&recp_list->filt_rules, new_fltr);
	if (!m_entry) {
		status = ice_create_pkt_fwd_rule(hw, recp_list, f_entry);
		goto exit_add_rule_internal;
	}

	cur_fltr = &m_entry->fltr_info;
	status = ice_add_update_vsi_list(hw, m_entry, cur_fltr, new_fltr);

exit_add_rule_internal:
	ice_release_lock(rule_lock);
	return status;
}

/**
 * ice_remove_vsi_list_rule
 * @hw: pointer to the hardware structure
 * @vsi_list_id: VSI list ID generated as part of allocate resource
 * @lkup_type: switch rule filter lookup type
 *
 * The VSI list should be emptied before this function is called to remove the
 * VSI list.
 */
static enum ice_status
ice_remove_vsi_list_rule(struct ice_hw *hw, u16 vsi_list_id,
			 enum ice_sw_lkup_type lkup_type)
{
	/* Free the vsi_list resource that we allocated. It is assumed that the
	 * list is empty at this point.
	 */
	return ice_aq_alloc_free_vsi_list(hw, &vsi_list_id, lkup_type,
					    ice_aqc_opc_free_res);
}

/**
 * ice_rem_update_vsi_list
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle of the VSI to remove
 * @fm_list: filter management entry for which the VSI list management needs to
 *	     be done
 */
static enum ice_status
ice_rem_update_vsi_list(struct ice_hw *hw, u16 vsi_handle,
			struct ice_fltr_mgmt_list_entry *fm_list)
{
	enum ice_sw_lkup_type lkup_type;
	enum ice_status status = ICE_SUCCESS;
	u16 vsi_list_id;

	if (fm_list->fltr_info.fltr_act != ICE_FWD_TO_VSI_LIST ||
	    fm_list->vsi_count == 0)
		return ICE_ERR_PARAM;

	/* A rule with the VSI being removed does not exist */
	if (!ice_is_bit_set(fm_list->vsi_list_info->vsi_map, vsi_handle))
		return ICE_ERR_DOES_NOT_EXIST;

	lkup_type = fm_list->fltr_info.lkup_type;
	vsi_list_id = fm_list->fltr_info.fwd_id.vsi_list_id;
	status = ice_update_vsi_list_rule(hw, &vsi_handle, 1, vsi_list_id, true,
					  ice_aqc_opc_update_sw_rules,
					  lkup_type);
	if (status)
		return status;

	fm_list->vsi_count--;
	ice_clear_bit(vsi_handle, fm_list->vsi_list_info->vsi_map);

	if (fm_list->vsi_count == 1 && lkup_type != ICE_SW_LKUP_VLAN) {
		struct ice_fltr_info tmp_fltr_info = fm_list->fltr_info;
		struct ice_vsi_list_map_info *vsi_list_info =
			fm_list->vsi_list_info;
		u16 rem_vsi_handle;

		rem_vsi_handle = ice_find_first_bit(vsi_list_info->vsi_map,
						    ICE_MAX_VSI);
		if (!ice_is_vsi_valid(hw, rem_vsi_handle))
			return ICE_ERR_OUT_OF_RANGE;

		/* Make sure VSI list is empty before removing it below */
		status = ice_update_vsi_list_rule(hw, &rem_vsi_handle, 1,
						  vsi_list_id, true,
						  ice_aqc_opc_update_sw_rules,
						  lkup_type);
		if (status)
			return status;

		tmp_fltr_info.fltr_act = ICE_FWD_TO_VSI;
		tmp_fltr_info.fwd_id.hw_vsi_id =
			ice_get_hw_vsi_num(hw, rem_vsi_handle);
		tmp_fltr_info.vsi_handle = rem_vsi_handle;
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr_info);
		if (status) {
			ice_debug(hw, ICE_DBG_SW,
				  "Failed to update pkt fwd rule to FWD_TO_VSI on HW VSI %d, error %d\n",
				  tmp_fltr_info.fwd_id.hw_vsi_id, status);
			return status;
		}

		fm_list->fltr_info = tmp_fltr_info;
	}

	if ((fm_list->vsi_count == 1 && lkup_type != ICE_SW_LKUP_VLAN) ||
	    (fm_list->vsi_count == 0 && lkup_type == ICE_SW_LKUP_VLAN)) {
		struct ice_vsi_list_map_info *vsi_list_info =
			fm_list->vsi_list_info;

		/* Remove the VSI list since it is no longer used */
		status = ice_remove_vsi_list_rule(hw, vsi_list_id, lkup_type);
		if (status) {
			ice_debug(hw, ICE_DBG_SW,
				  "Failed to remove VSI list %d, error %d\n",
				  vsi_list_id, status);
			return status;
		}

		LIST_DEL(&vsi_list_info->list_entry);
		ice_free(hw, vsi_list_info);
		fm_list->vsi_list_info = NULL;
	}

	return status;
}

/**
 * ice_remove_rule_internal - Remove a filter rule of a given type
 *
 * @hw: pointer to the hardware structure
 * @recp_list: recipe list for which the rule needs to removed
 * @f_entry: rule entry containing filter information
 */
static enum ice_status
ice_remove_rule_internal(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
			 struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_mgmt_list_entry *list_elem;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;
	bool remove_rule = false;
	u16 vsi_handle;

	if (!ice_is_vsi_valid(hw, f_entry->fltr_info.vsi_handle))
		return ICE_ERR_PARAM;
	f_entry->fltr_info.fwd_id.hw_vsi_id =
		ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);

	rule_lock = &recp_list->filt_rule_lock;
	ice_acquire_lock(rule_lock);
	list_elem = ice_find_rule_entry(&recp_list->filt_rules,
					&f_entry->fltr_info);
	if (!list_elem) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto exit;
	}

	if (list_elem->fltr_info.fltr_act != ICE_FWD_TO_VSI_LIST) {
		remove_rule = true;
	} else if (!list_elem->vsi_list_info) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto exit;
	} else if (list_elem->vsi_list_info->ref_cnt > 1) {
		/* a ref_cnt > 1 indicates that the vsi_list is being
		 * shared by multiple rules. Decrement the ref_cnt and
		 * remove this rule, but do not modify the list, as it
		 * is in-use by other rules.
		 */
		list_elem->vsi_list_info->ref_cnt--;
		remove_rule = true;
	} else {
		/* a ref_cnt of 1 indicates the vsi_list is only used
		 * by one rule. However, the original removal request is only
		 * for a single VSI. Update the vsi_list first, and only
		 * remove the rule if there are no further VSIs in this list.
		 */
		vsi_handle = f_entry->fltr_info.vsi_handle;
		status = ice_rem_update_vsi_list(hw, vsi_handle, list_elem);
		if (status)
			goto exit;
		/* if VSI count goes to zero after updating the VSI list */
		if (list_elem->vsi_count == 0)
			remove_rule = true;
	}

	if (remove_rule) {
		/* Remove the lookup rule */
		struct ice_aqc_sw_rules_elem *s_rule;

		s_rule = (struct ice_aqc_sw_rules_elem *)
			ice_malloc(hw, ICE_SW_RULE_RX_TX_NO_HDR_SIZE);
		if (!s_rule) {
			status = ICE_ERR_NO_MEMORY;
			goto exit;
		}

		ice_fill_sw_rule(hw, &list_elem->fltr_info, s_rule,
				 ice_aqc_opc_remove_sw_rules);

		status = ice_aq_sw_rules(hw, s_rule,
					 ICE_SW_RULE_RX_TX_NO_HDR_SIZE, 1,
					 ice_aqc_opc_remove_sw_rules, NULL);

		/* Remove a book keeping from the list */
		ice_free(hw, s_rule);

		if (status)
			goto exit;

		LIST_DEL(&list_elem->list_entry);
		ice_free(hw, list_elem);
	}
exit:
	ice_release_lock(rule_lock);
	return status;
}

/**
 * ice_aq_get_res_alloc - get allocated resources
 * @hw: pointer to the HW struct
 * @num_entries: pointer to u16 to store the number of resource entries returned
 * @buf: pointer to user-supplied buffer
 * @buf_size: size of buff
 * @cd: pointer to command details structure or NULL
 *
 * The user-supplied buffer must be large enough to store the resource
 * information for all resource types. Each resource type is an
 * ice_aqc_get_res_resp_data_elem structure.
 */
enum ice_status
ice_aq_get_res_alloc(struct ice_hw *hw, u16 *num_entries, void *buf,
		     u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_res_alloc *resp;
	enum ice_status status;
	struct ice_aq_desc desc;

	if (!buf)
		return ICE_ERR_BAD_PTR;

	if (buf_size < ICE_AQ_GET_RES_ALLOC_BUF_LEN)
		return ICE_ERR_INVAL_SIZE;

	resp = &desc.params.get_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_res_alloc);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);

	if (!status && num_entries)
		*num_entries = LE16_TO_CPU(resp->resp_elem_num);

	return status;
}

/**
 * ice_aq_get_res_descs - get allocated resource descriptors
 * @hw: pointer to the hardware structure
 * @num_entries: number of resource entries in buffer
 * @buf: Indirect buffer to hold data parameters and response
 * @buf_size: size of buffer for indirect commands
 * @res_type: resource type
 * @res_shared: is resource shared
 * @desc_id: input - first desc ID to start; output - next desc ID
 * @cd: pointer to command details structure or NULL
 */
enum ice_status
ice_aq_get_res_descs(struct ice_hw *hw, u16 num_entries,
		     struct ice_aqc_get_allocd_res_desc_resp *buf,
		     u16 buf_size, u16 res_type, bool res_shared, u16 *desc_id,
		     struct ice_sq_cd *cd)
{
	struct ice_aqc_get_allocd_res_desc *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.get_res_desc;

	if (!buf)
		return ICE_ERR_PARAM;

	if (buf_size != (num_entries * sizeof(*buf)))
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_allocd_res_desc);

	cmd->ops.cmd.res = CPU_TO_LE16(((res_type << ICE_AQC_RES_TYPE_S) &
					 ICE_AQC_RES_TYPE_M) | (res_shared ?
					ICE_AQC_RES_TYPE_FLAG_SHARED : 0));
	cmd->ops.cmd.first_desc = CPU_TO_LE16(*desc_id);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status)
		*desc_id = LE16_TO_CPU(cmd->ops.resp.next_desc);

	return status;
}

/**
 * ice_add_mac_rule - Add a MAC address based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 * @sw: pointer to switch info struct for which function add rule
 * @lport: logic port number on which function add rule
 *
 * IMPORTANT: When the ucast_shared flag is set to false and m_list has
 * multiple unicast addresses, the function assumes that all the
 * addresses are unique in a given add_mac call. It doesn't
 * check for duplicates in this case, removing duplicates from a given
 * list should be taken care of in the caller of this function.
 */
static enum ice_status
ice_add_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list,
		 struct ice_switch_info *sw, u8 lport)
{
	struct ice_sw_recipe *recp_list = &sw->recp_list[ICE_SW_LKUP_MAC];
	struct ice_aqc_sw_rules_elem *s_rule, *r_iter;
	struct ice_fltr_list_entry *m_list_itr;
	struct LIST_HEAD_TYPE *rule_head;
	u16 total_elem_left, s_rule_size;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;
	u16 num_unicast = 0;
	u8 elem_sent;

	s_rule = NULL;
	rule_lock = &recp_list->filt_rule_lock;
	rule_head = &recp_list->filt_rules;

	LIST_FOR_EACH_ENTRY(m_list_itr, m_list, ice_fltr_list_entry,
			    list_entry) {
		u8 *add = &m_list_itr->fltr_info.l_data.mac.mac_addr[0];
		u16 vsi_handle;
		u16 hw_vsi_id;

		m_list_itr->fltr_info.flag = ICE_FLTR_TX;
		vsi_handle = m_list_itr->fltr_info.vsi_handle;
		if (!ice_is_vsi_valid(hw, vsi_handle))
			return ICE_ERR_PARAM;
		hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);
		m_list_itr->fltr_info.fwd_id.hw_vsi_id = hw_vsi_id;
		/* update the src in case it is VSI num */
		if (m_list_itr->fltr_info.src_id != ICE_SRC_ID_VSI)
			return ICE_ERR_PARAM;
		m_list_itr->fltr_info.src = hw_vsi_id;
		if (m_list_itr->fltr_info.lkup_type != ICE_SW_LKUP_MAC ||
		    IS_ZERO_ETHER_ADDR(add))
			return ICE_ERR_PARAM;
		if (IS_UNICAST_ETHER_ADDR(add) && !hw->ucast_shared) {
			/* Don't overwrite the unicast address */
			ice_acquire_lock(rule_lock);
			if (ice_find_rule_entry(rule_head,
						&m_list_itr->fltr_info)) {
				ice_release_lock(rule_lock);
				return ICE_ERR_ALREADY_EXISTS;
			}
			ice_release_lock(rule_lock);
			num_unicast++;
		} else if (IS_MULTICAST_ETHER_ADDR(add) ||
			   (IS_UNICAST_ETHER_ADDR(add) && hw->ucast_shared)) {
			m_list_itr->status =
				ice_add_rule_internal(hw, recp_list, lport,
						      m_list_itr);
			if (m_list_itr->status)
				return m_list_itr->status;
		}
	}

	ice_acquire_lock(rule_lock);
	/* Exit if no suitable entries were found for adding bulk switch rule */
	if (!num_unicast) {
		status = ICE_SUCCESS;
		goto ice_add_mac_exit;
	}

	/* Allocate switch rule buffer for the bulk update for unicast */
	s_rule_size = ICE_SW_RULE_RX_TX_ETH_HDR_SIZE;
	s_rule = (struct ice_aqc_sw_rules_elem *)
		ice_calloc(hw, num_unicast, s_rule_size);
	if (!s_rule) {
		status = ICE_ERR_NO_MEMORY;
		goto ice_add_mac_exit;
	}

	r_iter = s_rule;
	LIST_FOR_EACH_ENTRY(m_list_itr, m_list, ice_fltr_list_entry,
			    list_entry) {
		struct ice_fltr_info *f_info = &m_list_itr->fltr_info;
		u8 *mac_addr = &f_info->l_data.mac.mac_addr[0];

		if (IS_UNICAST_ETHER_ADDR(mac_addr)) {
			ice_fill_sw_rule(hw, &m_list_itr->fltr_info, r_iter,
					 ice_aqc_opc_add_sw_rules);
			r_iter = (struct ice_aqc_sw_rules_elem *)
				((u8 *)r_iter + s_rule_size);
		}
	}

	/* Call AQ bulk switch rule update for all unicast addresses */
	r_iter = s_rule;
	/* Call AQ switch rule in AQ_MAX chunk */
	for (total_elem_left = num_unicast; total_elem_left > 0;
	     total_elem_left -= elem_sent) {
		struct ice_aqc_sw_rules_elem *entry = r_iter;

		elem_sent = MIN_T(u8, total_elem_left,
				  (ICE_AQ_MAX_BUF_LEN / s_rule_size));
		status = ice_aq_sw_rules(hw, entry, elem_sent * s_rule_size,
					 elem_sent, ice_aqc_opc_add_sw_rules,
					 NULL);
		if (status)
			goto ice_add_mac_exit;
		r_iter = (struct ice_aqc_sw_rules_elem *)
			((u8 *)r_iter + (elem_sent * s_rule_size));
	}

	/* Fill up rule ID based on the value returned from FW */
	r_iter = s_rule;
	LIST_FOR_EACH_ENTRY(m_list_itr, m_list, ice_fltr_list_entry,
			    list_entry) {
		struct ice_fltr_info *f_info = &m_list_itr->fltr_info;
		u8 *mac_addr = &f_info->l_data.mac.mac_addr[0];
		struct ice_fltr_mgmt_list_entry *fm_entry;

		if (IS_UNICAST_ETHER_ADDR(mac_addr)) {
			f_info->fltr_rule_id =
				LE16_TO_CPU(r_iter->pdata.lkup_tx_rx.index);
			f_info->fltr_act = ICE_FWD_TO_VSI;
			/* Create an entry to track this MAC address */
			fm_entry = (struct ice_fltr_mgmt_list_entry *)
				ice_malloc(hw, sizeof(*fm_entry));
			if (!fm_entry) {
				status = ICE_ERR_NO_MEMORY;
				goto ice_add_mac_exit;
			}
			fm_entry->fltr_info = *f_info;
			fm_entry->vsi_count = 1;
			/* The book keeping entries will get removed when
			 * base driver calls remove filter AQ command
			 */

			LIST_ADD(&fm_entry->list_entry, rule_head);
			r_iter = (struct ice_aqc_sw_rules_elem *)
				((u8 *)r_iter + s_rule_size);
		}
	}

ice_add_mac_exit:
	ice_release_lock(rule_lock);
	if (s_rule)
		ice_free(hw, s_rule);
	return status;
}

/**
 * ice_add_mac - Add a MAC address based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 *
 * Function add MAC rule for logical port from HW struct
 */
enum ice_status ice_add_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list)
{
	if (!m_list || !hw)
		return ICE_ERR_PARAM;

	return ice_add_mac_rule(hw, m_list, hw->switch_info,
				hw->port_info->lport);
}

/**
 * ice_add_vlan_internal - Add one VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @recp_list: recipe list for which rule has to be added
 * @f_entry: filter entry containing one VLAN information
 */
static enum ice_status
ice_add_vlan_internal(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
		      struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_mgmt_list_entry *v_list_itr;
	struct ice_fltr_info *new_fltr, *cur_fltr;
	enum ice_sw_lkup_type lkup_type;
	u16 vsi_list_id = 0, vsi_handle;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;

	if (!ice_is_vsi_valid(hw, f_entry->fltr_info.vsi_handle))
		return ICE_ERR_PARAM;

	f_entry->fltr_info.fwd_id.hw_vsi_id =
		ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);
	new_fltr = &f_entry->fltr_info;

	/* VLAN ID should only be 12 bits */
	if (new_fltr->l_data.vlan.vlan_id > ICE_MAX_VLAN_ID)
		return ICE_ERR_PARAM;

	if (new_fltr->src_id != ICE_SRC_ID_VSI)
		return ICE_ERR_PARAM;

	new_fltr->src = new_fltr->fwd_id.hw_vsi_id;
	lkup_type = new_fltr->lkup_type;
	vsi_handle = new_fltr->vsi_handle;
	rule_lock = &recp_list->filt_rule_lock;
	ice_acquire_lock(rule_lock);
	v_list_itr = ice_find_rule_entry(&recp_list->filt_rules, new_fltr);
	if (!v_list_itr) {
		struct ice_vsi_list_map_info *map_info = NULL;

		if (new_fltr->fltr_act == ICE_FWD_TO_VSI) {
			/* All VLAN pruning rules use a VSI list. Check if
			 * there is already a VSI list containing VSI that we
			 * want to add. If found, use the same vsi_list_id for
			 * this new VLAN rule or else create a new list.
			 */
			map_info = ice_find_vsi_list_entry(recp_list,
							   vsi_handle,
							   &vsi_list_id);
			if (!map_info) {
				status = ice_create_vsi_list_rule(hw,
								  &vsi_handle,
								  1,
								  &vsi_list_id,
								  lkup_type);
				if (status)
					goto exit;
			}
			/* Convert the action to forwarding to a VSI list. */
			new_fltr->fltr_act = ICE_FWD_TO_VSI_LIST;
			new_fltr->fwd_id.vsi_list_id = vsi_list_id;
		}

		status = ice_create_pkt_fwd_rule(hw, recp_list, f_entry);
		if (!status) {
			v_list_itr = ice_find_rule_entry(&recp_list->filt_rules,
							 new_fltr);
			if (!v_list_itr) {
				status = ICE_ERR_DOES_NOT_EXIST;
				goto exit;
			}
			/* reuse VSI list for new rule and increment ref_cnt */
			if (map_info) {
				v_list_itr->vsi_list_info = map_info;
				map_info->ref_cnt++;
			} else {
				v_list_itr->vsi_list_info =
					ice_create_vsi_list_map(hw, &vsi_handle,
								1, vsi_list_id);
			}
		}
	} else if (v_list_itr->vsi_list_info->ref_cnt == 1) {
		/* Update existing VSI list to add new VSI ID only if it used
		 * by one VLAN rule.
		 */
		cur_fltr = &v_list_itr->fltr_info;
		status = ice_add_update_vsi_list(hw, v_list_itr, cur_fltr,
						 new_fltr);
	} else {
		/* If VLAN rule exists and VSI list being used by this rule is
		 * referenced by more than 1 VLAN rule. Then create a new VSI
		 * list appending previous VSI with new VSI and update existing
		 * VLAN rule to point to new VSI list ID
		 */
		struct ice_fltr_info tmp_fltr;
		u16 vsi_handle_arr[2];
		u16 cur_handle;

		/* Current implementation only supports reusing VSI list with
		 * one VSI count. We should never hit below condition
		 */
		if (v_list_itr->vsi_count > 1 &&
		    v_list_itr->vsi_list_info->ref_cnt > 1) {
			ice_debug(hw, ICE_DBG_SW,
				  "Invalid configuration: Optimization to reuse VSI list with more than one VSI is not being done yet\n");
			status = ICE_ERR_CFG;
			goto exit;
		}

		cur_handle =
			ice_find_first_bit(v_list_itr->vsi_list_info->vsi_map,
					   ICE_MAX_VSI);

		/* A rule already exists with the new VSI being added */
		if (cur_handle == vsi_handle) {
			status = ICE_ERR_ALREADY_EXISTS;
			goto exit;
		}

		vsi_handle_arr[0] = cur_handle;
		vsi_handle_arr[1] = vsi_handle;
		status = ice_create_vsi_list_rule(hw, &vsi_handle_arr[0], 2,
						  &vsi_list_id, lkup_type);
		if (status)
			goto exit;

		tmp_fltr = v_list_itr->fltr_info;
		tmp_fltr.fltr_rule_id = v_list_itr->fltr_info.fltr_rule_id;
		tmp_fltr.fwd_id.vsi_list_id = vsi_list_id;
		tmp_fltr.fltr_act = ICE_FWD_TO_VSI_LIST;
		/* Update the previous switch rule to a new VSI list which
		 * includes current VSI that is requested
		 */
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr);
		if (status)
			goto exit;

		/* before overriding VSI list map info. decrement ref_cnt of
		 * previous VSI list
		 */
		v_list_itr->vsi_list_info->ref_cnt--;

		/* now update to newly created list */
		v_list_itr->fltr_info.fwd_id.vsi_list_id = vsi_list_id;
		v_list_itr->vsi_list_info =
			ice_create_vsi_list_map(hw, &vsi_handle_arr[0], 2,
						vsi_list_id);
		v_list_itr->vsi_count++;
	}

exit:
	ice_release_lock(rule_lock);
	return status;
}

/**
 * ice_add_vlan_rule - Add VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN entries and forwarding information
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
ice_add_vlan_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list,
		  struct ice_switch_info *sw)
{
	struct ice_fltr_list_entry *v_list_itr;
	struct ice_sw_recipe *recp_list;

	recp_list = &sw->recp_list[ICE_SW_LKUP_VLAN];
	LIST_FOR_EACH_ENTRY(v_list_itr, v_list, ice_fltr_list_entry,
			    list_entry) {
		if (v_list_itr->fltr_info.lkup_type != ICE_SW_LKUP_VLAN)
			return ICE_ERR_PARAM;
		v_list_itr->fltr_info.flag = ICE_FLTR_TX;
		v_list_itr->status = ice_add_vlan_internal(hw, recp_list,
							   v_list_itr);
		if (v_list_itr->status)
			return v_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_add_vlan - Add a VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN and forwarding information
 *
 * Function add VLAN rule for logical port from HW struct
 */
enum ice_status ice_add_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list)
{
	if (!v_list || !hw)
		return ICE_ERR_PARAM;

	return ice_add_vlan_rule(hw, v_list, hw->switch_info);
}

/**
 * ice_add_mac_vlan - Add MAC and VLAN pair based filter rule
 * @hw: pointer to the hardware structure
 * @mv_list: list of MAC and VLAN filters
 * @sw: pointer to switch info struct for which function add rule
 * @lport: logic port number on which function add rule
 *
 * If the VSI on which the MAC-VLAN pair has to be added has Rx and Tx VLAN
 * pruning bits enabled, then it is the responsibility of the caller to make
 * sure to add a VLAN only filter on the same VSI. Packets belonging to that
 * VLAN won't be received on that VSI otherwise.
 */
static enum ice_status
ice_add_mac_vlan_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *mv_list,
		      struct ice_switch_info *sw, u8 lport)
{
	struct ice_fltr_list_entry *mv_list_itr;
	struct ice_sw_recipe *recp_list;

	if (!mv_list || !hw)
		return ICE_ERR_PARAM;

	recp_list = &sw->recp_list[ICE_SW_LKUP_MAC_VLAN];
	LIST_FOR_EACH_ENTRY(mv_list_itr, mv_list, ice_fltr_list_entry,
			    list_entry) {
		enum ice_sw_lkup_type l_type =
			mv_list_itr->fltr_info.lkup_type;

		if (l_type != ICE_SW_LKUP_MAC_VLAN)
			return ICE_ERR_PARAM;
		mv_list_itr->fltr_info.flag = ICE_FLTR_TX;
		mv_list_itr->status =
			ice_add_rule_internal(hw, recp_list, lport,
					      mv_list_itr);
		if (mv_list_itr->status)
			return mv_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_add_mac_vlan - Add a MAC VLAN address based filter rule
 * @hw: pointer to the hardware structure
 * @mv_list: list of MAC VLAN addresses and forwarding information
 *
 * Function add MAC VLAN rule for logical port from HW struct
 */
enum ice_status
ice_add_mac_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *mv_list)
{
	if (!mv_list || !hw)
		return ICE_ERR_PARAM;

	return ice_add_mac_vlan_rule(hw, mv_list, hw->switch_info,
				     hw->port_info->lport);
}

/**
 * ice_add_eth_mac_rule - Add ethertype and MAC based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ether type MAC filter, MAC is optional
 * @sw: pointer to switch info struct for which function add rule
 * @lport: logic port number on which function add rule
 *
 * This function requires the caller to populate the entries in
 * the filter list with the necessary fields (including flags to
 * indicate Tx or Rx rules).
 */
static enum ice_status
ice_add_eth_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list,
		     struct ice_switch_info *sw, u8 lport)
{
	struct ice_fltr_list_entry *em_list_itr;

	LIST_FOR_EACH_ENTRY(em_list_itr, em_list, ice_fltr_list_entry,
			    list_entry) {
		struct ice_sw_recipe *recp_list;
		enum ice_sw_lkup_type l_type;

		l_type = em_list_itr->fltr_info.lkup_type;
		recp_list = &sw->recp_list[l_type];

		if (l_type != ICE_SW_LKUP_ETHERTYPE_MAC &&
		    l_type != ICE_SW_LKUP_ETHERTYPE)
			return ICE_ERR_PARAM;

		em_list_itr->status = ice_add_rule_internal(hw, recp_list,
							    lport,
							    em_list_itr);
		if (em_list_itr->status)
			return em_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_add_eth_mac - Add a ethertype based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ethertype and forwarding information
 *
 * Function add ethertype rule for logical port from HW struct
 */
enum ice_status
ice_add_eth_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list)
{
	if (!em_list || !hw)
		return ICE_ERR_PARAM;

	return ice_add_eth_mac_rule(hw, em_list, hw->switch_info,
				    hw->port_info->lport);
}

/**
 * ice_remove_eth_mac_rule - Remove an ethertype (or MAC) based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ethertype or ethertype MAC entries
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
ice_remove_eth_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list,
			struct ice_switch_info *sw)
{
	struct ice_fltr_list_entry *em_list_itr, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(em_list_itr, tmp, em_list, ice_fltr_list_entry,
				 list_entry) {
		struct ice_sw_recipe *recp_list;
		enum ice_sw_lkup_type l_type;

		l_type = em_list_itr->fltr_info.lkup_type;

		if (l_type != ICE_SW_LKUP_ETHERTYPE_MAC &&
		    l_type != ICE_SW_LKUP_ETHERTYPE)
			return ICE_ERR_PARAM;

		recp_list = &sw->recp_list[l_type];
		em_list_itr->status = ice_remove_rule_internal(hw, recp_list,
							       em_list_itr);
		if (em_list_itr->status)
			return em_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_remove_eth_mac - remove a ethertype based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ethertype and forwarding information
 *
 */
enum ice_status
ice_remove_eth_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list)
{
	if (!em_list || !hw)
		return ICE_ERR_PARAM;

	return ice_remove_eth_mac_rule(hw, em_list, hw->switch_info);
}

/**
 * ice_rem_sw_rule_info
 * @hw: pointer to the hardware structure
 * @rule_head: pointer to the switch list structure that we want to delete
 */
static void
ice_rem_sw_rule_info(struct ice_hw *hw, struct LIST_HEAD_TYPE *rule_head)
{
	if (!LIST_EMPTY(rule_head)) {
		struct ice_fltr_mgmt_list_entry *entry;
		struct ice_fltr_mgmt_list_entry *tmp;

		LIST_FOR_EACH_ENTRY_SAFE(entry, tmp, rule_head,
					 ice_fltr_mgmt_list_entry, list_entry) {
			LIST_DEL(&entry->list_entry);
			ice_free(hw, entry);
		}
	}
}

/**
 * ice_rem_adv_rule_info
 * @hw: pointer to the hardware structure
 * @rule_head: pointer to the switch list structure that we want to delete
 */
static void
ice_rem_adv_rule_info(struct ice_hw *hw, struct LIST_HEAD_TYPE *rule_head)
{
	struct ice_adv_fltr_mgmt_list_entry *tmp_entry;
	struct ice_adv_fltr_mgmt_list_entry *lst_itr;

	if (LIST_EMPTY(rule_head))
		return;

	LIST_FOR_EACH_ENTRY_SAFE(lst_itr, tmp_entry, rule_head,
				 ice_adv_fltr_mgmt_list_entry, list_entry) {
		LIST_DEL(&lst_itr->list_entry);
		ice_free(hw, lst_itr->lkups);
		ice_free(hw, lst_itr);
	}
}

/**
 * ice_rem_all_sw_rules_info
 * @hw: pointer to the hardware structure
 */
void ice_rem_all_sw_rules_info(struct ice_hw *hw)
{
	struct ice_switch_info *sw = hw->switch_info;
	u8 i;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct LIST_HEAD_TYPE *rule_head;

		rule_head = &sw->recp_list[i].filt_rules;
		if (!sw->recp_list[i].adv_rule)
			ice_rem_sw_rule_info(hw, rule_head);
		else
			ice_rem_adv_rule_info(hw, rule_head);
		if (sw->recp_list[i].adv_rule &&
		    LIST_EMPTY(&sw->recp_list[i].filt_rules))
			sw->recp_list[i].adv_rule = false;
	}
}

/**
 * ice_cfg_dflt_vsi - change state of VSI to set/clear default
 * @pi: pointer to the port_info structure
 * @vsi_handle: VSI handle to set as default
 * @set: true to add the above mentioned switch rule, false to remove it
 * @direction: ICE_FLTR_RX or ICE_FLTR_TX
 *
 * add filter rule to set/unset given VSI as default VSI for the switch
 * (represented by swid)
 */
enum ice_status
ice_cfg_dflt_vsi(struct ice_port_info *pi, u16 vsi_handle, bool set,
		 u8 direction)
{
	struct ice_aqc_sw_rules_elem *s_rule;
	struct ice_fltr_info f_info;
	struct ice_hw *hw = pi->hw;
	enum ice_adminq_opc opcode;
	enum ice_status status;
	u16 s_rule_size;
	u16 hw_vsi_id;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	s_rule_size = set ? ICE_SW_RULE_RX_TX_ETH_HDR_SIZE :
			    ICE_SW_RULE_RX_TX_NO_HDR_SIZE;
	s_rule = (struct ice_aqc_sw_rules_elem *)ice_malloc(hw, s_rule_size);
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;

	ice_memset(&f_info, 0, sizeof(f_info), ICE_NONDMA_MEM);

	f_info.lkup_type = ICE_SW_LKUP_DFLT;
	f_info.flag = direction;
	f_info.fltr_act = ICE_FWD_TO_VSI;
	f_info.fwd_id.hw_vsi_id = hw_vsi_id;

	if (f_info.flag & ICE_FLTR_RX) {
		f_info.src = pi->lport;
		f_info.src_id = ICE_SRC_ID_LPORT;
		if (!set)
			f_info.fltr_rule_id =
				pi->dflt_rx_vsi_rule_id;
	} else if (f_info.flag & ICE_FLTR_TX) {
		f_info.src_id = ICE_SRC_ID_VSI;
		f_info.src = hw_vsi_id;
		if (!set)
			f_info.fltr_rule_id =
				pi->dflt_tx_vsi_rule_id;
	}

	if (set)
		opcode = ice_aqc_opc_add_sw_rules;
	else
		opcode = ice_aqc_opc_remove_sw_rules;

	ice_fill_sw_rule(hw, &f_info, s_rule, opcode);

	status = ice_aq_sw_rules(hw, s_rule, s_rule_size, 1, opcode, NULL);
	if (status || !(f_info.flag & ICE_FLTR_TX_RX))
		goto out;
	if (set) {
		u16 index = LE16_TO_CPU(s_rule->pdata.lkup_tx_rx.index);

		if (f_info.flag & ICE_FLTR_TX) {
			pi->dflt_tx_vsi_num = hw_vsi_id;
			pi->dflt_tx_vsi_rule_id = index;
		} else if (f_info.flag & ICE_FLTR_RX) {
			pi->dflt_rx_vsi_num = hw_vsi_id;
			pi->dflt_rx_vsi_rule_id = index;
		}
	} else {
		if (f_info.flag & ICE_FLTR_TX) {
			pi->dflt_tx_vsi_num = ICE_DFLT_VSI_INVAL;
			pi->dflt_tx_vsi_rule_id = ICE_INVAL_ACT;
		} else if (f_info.flag & ICE_FLTR_RX) {
			pi->dflt_rx_vsi_num = ICE_DFLT_VSI_INVAL;
			pi->dflt_rx_vsi_rule_id = ICE_INVAL_ACT;
		}
	}

out:
	ice_free(hw, s_rule);
	return status;
}

/**
 * ice_find_ucast_rule_entry - Search for a unicast MAC filter rule entry
 * @list_head: head of rule list
 * @f_info: rule information
 *
 * Helper function to search for a unicast rule entry - this is to be used
 * to remove unicast MAC filter that is not shared with other VSIs on the
 * PF switch.
 *
 * Returns pointer to entry storing the rule if found
 */
static struct ice_fltr_mgmt_list_entry *
ice_find_ucast_rule_entry(struct LIST_HEAD_TYPE *list_head,
			  struct ice_fltr_info *f_info)
{
	struct ice_fltr_mgmt_list_entry *list_itr;

	LIST_FOR_EACH_ENTRY(list_itr, list_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		if (!memcmp(&f_info->l_data, &list_itr->fltr_info.l_data,
			    sizeof(f_info->l_data)) &&
		    f_info->fwd_id.hw_vsi_id ==
		    list_itr->fltr_info.fwd_id.hw_vsi_id &&
		    f_info->flag == list_itr->fltr_info.flag)
			return list_itr;
	}
	return NULL;
}

/**
 * ice_remove_mac_rule - remove a MAC based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 * @recp_list: list from which function remove MAC address
 *
 * This function removes either a MAC filter rule or a specific VSI from a
 * VSI list for a multicast MAC address.
 *
 * Returns ICE_ERR_DOES_NOT_EXIST if a given entry was not added by
 * ice_add_mac. Caller should be aware that this call will only work if all
 * the entries passed into m_list were added previously. It will not attempt to
 * do a partial remove of entries that were found.
 */
static enum ice_status
ice_remove_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list,
		    struct ice_sw_recipe *recp_list)
{
	struct ice_fltr_list_entry *list_itr, *tmp;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */

	if (!m_list)
		return ICE_ERR_PARAM;

	rule_lock = &recp_list->filt_rule_lock;
	LIST_FOR_EACH_ENTRY_SAFE(list_itr, tmp, m_list, ice_fltr_list_entry,
				 list_entry) {
		enum ice_sw_lkup_type l_type = list_itr->fltr_info.lkup_type;
		u8 *add = &list_itr->fltr_info.l_data.mac.mac_addr[0];
		u16 vsi_handle;

		if (l_type != ICE_SW_LKUP_MAC)
			return ICE_ERR_PARAM;

		vsi_handle = list_itr->fltr_info.vsi_handle;
		if (!ice_is_vsi_valid(hw, vsi_handle))
			return ICE_ERR_PARAM;

		list_itr->fltr_info.fwd_id.hw_vsi_id =
					ice_get_hw_vsi_num(hw, vsi_handle);
		if (IS_UNICAST_ETHER_ADDR(add) && !hw->ucast_shared) {
			/* Don't remove the unicast address that belongs to
			 * another VSI on the switch, since it is not being
			 * shared...
			 */
			ice_acquire_lock(rule_lock);
			if (!ice_find_ucast_rule_entry(&recp_list->filt_rules,
						       &list_itr->fltr_info)) {
				ice_release_lock(rule_lock);
				return ICE_ERR_DOES_NOT_EXIST;
			}
			ice_release_lock(rule_lock);
		}
		list_itr->status = ice_remove_rule_internal(hw, recp_list,
							    list_itr);
		if (list_itr->status)
			return list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_remove_mac - remove a MAC address based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 *
 */
enum ice_status ice_remove_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list)
{
	struct ice_sw_recipe *recp_list;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC];
	return ice_remove_mac_rule(hw, m_list, recp_list);
}

/**
 * ice_remove_vlan_rule - Remove VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN entries and forwarding information
 * @recp_list: list from which function remove VLAN
 */
static enum ice_status
ice_remove_vlan_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list,
		     struct ice_sw_recipe *recp_list)
{
	struct ice_fltr_list_entry *v_list_itr, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(v_list_itr, tmp, v_list, ice_fltr_list_entry,
				 list_entry) {
		enum ice_sw_lkup_type l_type = v_list_itr->fltr_info.lkup_type;

		if (l_type != ICE_SW_LKUP_VLAN)
			return ICE_ERR_PARAM;
		v_list_itr->status = ice_remove_rule_internal(hw, recp_list,
							      v_list_itr);
		if (v_list_itr->status)
			return v_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_remove_vlan - remove a VLAN address based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN and forwarding information
 *
 */
enum ice_status
ice_remove_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list)
{
	struct ice_sw_recipe *recp_list;

	if (!v_list || !hw)
		return ICE_ERR_PARAM;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_VLAN];
	return ice_remove_vlan_rule(hw, v_list, recp_list);
}

/**
 * ice_remove_mac_vlan_rule - Remove MAC VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of MAC VLAN entries and forwarding information
 * @recp_list: list from which function remove MAC VLAN
 */
static enum ice_status
ice_remove_mac_vlan_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list,
			 struct ice_sw_recipe *recp_list)
{
	struct ice_fltr_list_entry *v_list_itr, *tmp;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC_VLAN];
	LIST_FOR_EACH_ENTRY_SAFE(v_list_itr, tmp, v_list, ice_fltr_list_entry,
				 list_entry) {
		enum ice_sw_lkup_type l_type = v_list_itr->fltr_info.lkup_type;

		if (l_type != ICE_SW_LKUP_MAC_VLAN)
			return ICE_ERR_PARAM;
		v_list_itr->status =
			ice_remove_rule_internal(hw, recp_list,
						 v_list_itr);
		if (v_list_itr->status)
			return v_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_remove_mac_vlan - remove a MAC VLAN address based filter rule
 * @hw: pointer to the hardware structure
 * @mv_list: list of MAC VLAN and forwarding information
 */
enum ice_status
ice_remove_mac_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *mv_list)
{
	struct ice_sw_recipe *recp_list;

	if (!mv_list || !hw)
		return ICE_ERR_PARAM;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC_VLAN];
	return ice_remove_mac_vlan_rule(hw, mv_list, recp_list);
}

/**
 * ice_vsi_uses_fltr - Determine if given VSI uses specified filter
 * @fm_entry: filter entry to inspect
 * @vsi_handle: VSI handle to compare with filter info
 */
static bool
ice_vsi_uses_fltr(struct ice_fltr_mgmt_list_entry *fm_entry, u16 vsi_handle)
{
	return ((fm_entry->fltr_info.fltr_act == ICE_FWD_TO_VSI &&
		 fm_entry->fltr_info.vsi_handle == vsi_handle) ||
		(fm_entry->fltr_info.fltr_act == ICE_FWD_TO_VSI_LIST &&
		 (ice_is_bit_set(fm_entry->vsi_list_info->vsi_map,
				 vsi_handle))));
}

/**
 * ice_add_entry_to_vsi_fltr_list - Add copy of fltr_list_entry to remove list
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @vsi_list_head: pointer to the list to add entry to
 * @fi: pointer to fltr_info of filter entry to copy & add
 *
 * Helper function, used when creating a list of filters to remove from
 * a specific VSI. The entry added to vsi_list_head is a COPY of the
 * original filter entry, with the exception of fltr_info.fltr_act and
 * fltr_info.fwd_id fields. These are set such that later logic can
 * extract which VSI to remove the fltr from, and pass on that information.
 */
static enum ice_status
ice_add_entry_to_vsi_fltr_list(struct ice_hw *hw, u16 vsi_handle,
			       struct LIST_HEAD_TYPE *vsi_list_head,
			       struct ice_fltr_info *fi)
{
	struct ice_fltr_list_entry *tmp;

	/* this memory is freed up in the caller function
	 * once filters for this VSI are removed
	 */
	tmp = (struct ice_fltr_list_entry *)ice_malloc(hw, sizeof(*tmp));
	if (!tmp)
		return ICE_ERR_NO_MEMORY;

	tmp->fltr_info = *fi;

	/* Overwrite these fields to indicate which VSI to remove filter from,
	 * so find and remove logic can extract the information from the
	 * list entries. Note that original entries will still have proper
	 * values.
	 */
	tmp->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	tmp->fltr_info.vsi_handle = vsi_handle;
	tmp->fltr_info.fwd_id.hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	LIST_ADD(&tmp->list_entry, vsi_list_head);

	return ICE_SUCCESS;
}

/**
 * ice_add_to_vsi_fltr_list - Add VSI filters to the list
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @lkup_list_head: pointer to the list that has certain lookup type filters
 * @vsi_list_head: pointer to the list pertaining to VSI with vsi_handle
 *
 * Locates all filters in lkup_list_head that are used by the given VSI,
 * and adds COPIES of those entries to vsi_list_head (intended to be used
 * to remove the listed filters).
 * Note that this means all entries in vsi_list_head must be explicitly
 * deallocated by the caller when done with list.
 */
static enum ice_status
ice_add_to_vsi_fltr_list(struct ice_hw *hw, u16 vsi_handle,
			 struct LIST_HEAD_TYPE *lkup_list_head,
			 struct LIST_HEAD_TYPE *vsi_list_head)
{
	struct ice_fltr_mgmt_list_entry *fm_entry;
	enum ice_status status = ICE_SUCCESS;

	/* check to make sure VSI ID is valid and within boundary */
	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	LIST_FOR_EACH_ENTRY(fm_entry, lkup_list_head,
			    ice_fltr_mgmt_list_entry, list_entry) {
		struct ice_fltr_info *fi;

		fi = &fm_entry->fltr_info;
		if (!fi || !ice_vsi_uses_fltr(fm_entry, vsi_handle))
			continue;

		status = ice_add_entry_to_vsi_fltr_list(hw, vsi_handle,
							vsi_list_head, fi);
		if (status)
			return status;
	}
	return status;
}

/**
 * ice_determine_promisc_mask
 * @fi: filter info to parse
 *
 * Helper function to determine which ICE_PROMISC_ mask corresponds
 * to given filter into.
 */
static u8 ice_determine_promisc_mask(struct ice_fltr_info *fi)
{
	u16 vid = fi->l_data.mac_vlan.vlan_id;
	u8 *macaddr = fi->l_data.mac.mac_addr;
	bool is_tx_fltr = false;
	u8 promisc_mask = 0;

	if (fi->flag == ICE_FLTR_TX)
		is_tx_fltr = true;

	if (IS_BROADCAST_ETHER_ADDR(macaddr))
		promisc_mask |= is_tx_fltr ?
			ICE_PROMISC_BCAST_TX : ICE_PROMISC_BCAST_RX;
	else if (IS_MULTICAST_ETHER_ADDR(macaddr))
		promisc_mask |= is_tx_fltr ?
			ICE_PROMISC_MCAST_TX : ICE_PROMISC_MCAST_RX;
	else if (IS_UNICAST_ETHER_ADDR(macaddr))
		promisc_mask |= is_tx_fltr ?
			ICE_PROMISC_UCAST_TX : ICE_PROMISC_UCAST_RX;
	if (vid)
		promisc_mask |= is_tx_fltr ?
			ICE_PROMISC_VLAN_TX : ICE_PROMISC_VLAN_RX;

	return promisc_mask;
}

/**
 * _ice_get_vsi_promisc - get promiscuous mode of given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to retrieve info from
 * @promisc_mask: pointer to mask to be filled in
 * @vid: VLAN ID of promisc VLAN VSI
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
_ice_get_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 *promisc_mask,
		     u16 *vid, struct ice_switch_info *sw)
{
	struct ice_fltr_mgmt_list_entry *itr;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	*vid = 0;
	*promisc_mask = 0;
	rule_head = &sw->recp_list[ICE_SW_LKUP_PROMISC].filt_rules;
	rule_lock = &sw->recp_list[ICE_SW_LKUP_PROMISC].filt_rule_lock;

	ice_acquire_lock(rule_lock);
	LIST_FOR_EACH_ENTRY(itr, rule_head,
			    ice_fltr_mgmt_list_entry, list_entry) {
		/* Continue if this filter doesn't apply to this VSI or the
		 * VSI ID is not in the VSI map for this filter
		 */
		if (!ice_vsi_uses_fltr(itr, vsi_handle))
			continue;

		*promisc_mask |= ice_determine_promisc_mask(&itr->fltr_info);
	}
	ice_release_lock(rule_lock);

	return ICE_SUCCESS;
}

/**
 * ice_get_vsi_promisc - get promiscuous mode of given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to retrieve info from
 * @promisc_mask: pointer to mask to be filled in
 * @vid: VLAN ID of promisc VLAN VSI
 */
enum ice_status
ice_get_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 *promisc_mask,
		    u16 *vid)
{
	return _ice_get_vsi_promisc(hw, vsi_handle, promisc_mask,
				    vid, hw->switch_info);
}

/**
 * ice_get_vsi_vlan_promisc - get VLAN promiscuous mode of given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to retrieve info from
 * @promisc_mask: pointer to mask to be filled in
 * @vid: VLAN ID of promisc VLAN VSI
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
_ice_get_vsi_vlan_promisc(struct ice_hw *hw, u16 vsi_handle, u8 *promisc_mask,
			  u16 *vid, struct ice_switch_info *sw)
{
	struct ice_fltr_mgmt_list_entry *itr;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	*vid = 0;
	*promisc_mask = 0;
	rule_head = &sw->recp_list[ICE_SW_LKUP_PROMISC_VLAN].filt_rules;
	rule_lock = &sw->recp_list[ICE_SW_LKUP_PROMISC_VLAN].filt_rule_lock;

	ice_acquire_lock(rule_lock);
	LIST_FOR_EACH_ENTRY(itr, rule_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		/* Continue if this filter doesn't apply to this VSI or the
		 * VSI ID is not in the VSI map for this filter
		 */
		if (!ice_vsi_uses_fltr(itr, vsi_handle))
			continue;

		*promisc_mask |= ice_determine_promisc_mask(&itr->fltr_info);
	}
	ice_release_lock(rule_lock);

	return ICE_SUCCESS;
}

/**
 * ice_get_vsi_vlan_promisc - get VLAN promiscuous mode of given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to retrieve info from
 * @promisc_mask: pointer to mask to be filled in
 * @vid: VLAN ID of promisc VLAN VSI
 */
enum ice_status
ice_get_vsi_vlan_promisc(struct ice_hw *hw, u16 vsi_handle, u8 *promisc_mask,
			 u16 *vid)
{
	return _ice_get_vsi_vlan_promisc(hw, vsi_handle, promisc_mask,
					 vid, hw->switch_info);
}

/**
 * ice_remove_promisc - Remove promisc based filter rules
 * @hw: pointer to the hardware structure
 * @recp_id: recipe ID for which the rule needs to removed
 * @v_list: list of promisc entries
 */
static enum ice_status
ice_remove_promisc(struct ice_hw *hw, u8 recp_id,
		   struct LIST_HEAD_TYPE *v_list)
{
	struct ice_fltr_list_entry *v_list_itr, *tmp;
	struct ice_sw_recipe *recp_list;

	recp_list = &hw->switch_info->recp_list[recp_id];
	LIST_FOR_EACH_ENTRY_SAFE(v_list_itr, tmp, v_list, ice_fltr_list_entry,
				 list_entry) {
		v_list_itr->status =
			ice_remove_rule_internal(hw, recp_list, v_list_itr);
		if (v_list_itr->status)
			return v_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * _ice_clear_vsi_promisc - clear specified promiscuous mode(s)
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to clear mode
 * @promisc_mask: mask of promiscuous config bits to clear
 * @vid: VLAN ID to clear VLAN promiscuous
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
_ice_clear_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
		       u16 vid, struct ice_switch_info *sw)
{
	struct ice_fltr_list_entry *fm_entry, *tmp;
	struct LIST_HEAD_TYPE remove_list_head;
	struct ice_fltr_mgmt_list_entry *itr;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;
	u8 recipe_id;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	if (promisc_mask & (ICE_PROMISC_VLAN_RX | ICE_PROMISC_VLAN_TX))
		recipe_id = ICE_SW_LKUP_PROMISC_VLAN;
	else
		recipe_id = ICE_SW_LKUP_PROMISC;

	rule_head = &sw->recp_list[recipe_id].filt_rules;
	rule_lock = &sw->recp_list[recipe_id].filt_rule_lock;

	INIT_LIST_HEAD(&remove_list_head);

	ice_acquire_lock(rule_lock);
	LIST_FOR_EACH_ENTRY(itr, rule_head,
			    ice_fltr_mgmt_list_entry, list_entry) {
		struct ice_fltr_info *fltr_info;
		u8 fltr_promisc_mask = 0;

		if (!ice_vsi_uses_fltr(itr, vsi_handle))
			continue;
		fltr_info = &itr->fltr_info;

		if (recipe_id == ICE_SW_LKUP_PROMISC_VLAN &&
		    vid != fltr_info->l_data.mac_vlan.vlan_id)
			continue;

		fltr_promisc_mask |= ice_determine_promisc_mask(fltr_info);

		/* Skip if filter is not completely specified by given mask */
		if (fltr_promisc_mask & ~promisc_mask)
			continue;

		status = ice_add_entry_to_vsi_fltr_list(hw, vsi_handle,
							&remove_list_head,
							fltr_info);
		if (status) {
			ice_release_lock(rule_lock);
			goto free_fltr_list;
		}
	}
	ice_release_lock(rule_lock);

	status = ice_remove_promisc(hw, recipe_id, &remove_list_head);

free_fltr_list:
	LIST_FOR_EACH_ENTRY_SAFE(fm_entry, tmp, &remove_list_head,
				 ice_fltr_list_entry, list_entry) {
		LIST_DEL(&fm_entry->list_entry);
		ice_free(hw, fm_entry);
	}

	return status;
}

/**
 * ice_clear_vsi_promisc - clear specified promiscuous mode(s) for given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to clear mode
 * @promisc_mask: mask of promiscuous config bits to clear
 * @vid: VLAN ID to clear VLAN promiscuous
 */
enum ice_status
ice_clear_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
		      u8 promisc_mask, u16 vid)
{
	return _ice_clear_vsi_promisc(hw, vsi_handle, promisc_mask,
				      vid, hw->switch_info);
}

/**
 * _ice_set_vsi_promisc - set given VSI to given promiscuous mode(s)
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: mask of promiscuous config bits
 * @vid: VLAN ID to set VLAN promiscuous
 * @lport: logical port number to configure promisc mode
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
_ice_set_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
		     u16 vid, u8 lport, struct ice_switch_info *sw)
{
	enum { UCAST_FLTR = 1, MCAST_FLTR, BCAST_FLTR };
	struct ice_fltr_list_entry f_list_entry;
	struct ice_fltr_info new_fltr;
	enum ice_status status = ICE_SUCCESS;
	bool is_tx_fltr;
	u16 hw_vsi_id;
	int pkt_type;
	u8 recipe_id;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	ice_memset(&new_fltr, 0, sizeof(new_fltr), ICE_NONDMA_MEM);

	if (promisc_mask & (ICE_PROMISC_VLAN_RX | ICE_PROMISC_VLAN_TX)) {
		new_fltr.lkup_type = ICE_SW_LKUP_PROMISC_VLAN;
		new_fltr.l_data.mac_vlan.vlan_id = vid;
		recipe_id = ICE_SW_LKUP_PROMISC_VLAN;
	} else {
		new_fltr.lkup_type = ICE_SW_LKUP_PROMISC;
		recipe_id = ICE_SW_LKUP_PROMISC;
	}

	/* Separate filters must be set for each direction/packet type
	 * combination, so we will loop over the mask value, store the
	 * individual type, and clear it out in the input mask as it
	 * is found.
	 */
	while (promisc_mask) {
		struct ice_sw_recipe *recp_list;
		u8 *mac_addr;

		pkt_type = 0;
		is_tx_fltr = false;

		if (promisc_mask & ICE_PROMISC_UCAST_RX) {
			promisc_mask &= ~ICE_PROMISC_UCAST_RX;
			pkt_type = UCAST_FLTR;
		} else if (promisc_mask & ICE_PROMISC_UCAST_TX) {
			promisc_mask &= ~ICE_PROMISC_UCAST_TX;
			pkt_type = UCAST_FLTR;
			is_tx_fltr = true;
		} else if (promisc_mask & ICE_PROMISC_MCAST_RX) {
			promisc_mask &= ~ICE_PROMISC_MCAST_RX;
			pkt_type = MCAST_FLTR;
		} else if (promisc_mask & ICE_PROMISC_MCAST_TX) {
			promisc_mask &= ~ICE_PROMISC_MCAST_TX;
			pkt_type = MCAST_FLTR;
			is_tx_fltr = true;
		} else if (promisc_mask & ICE_PROMISC_BCAST_RX) {
			promisc_mask &= ~ICE_PROMISC_BCAST_RX;
			pkt_type = BCAST_FLTR;
		} else if (promisc_mask & ICE_PROMISC_BCAST_TX) {
			promisc_mask &= ~ICE_PROMISC_BCAST_TX;
			pkt_type = BCAST_FLTR;
			is_tx_fltr = true;
		}

		/* Check for VLAN promiscuous flag */
		if (promisc_mask & ICE_PROMISC_VLAN_RX) {
			promisc_mask &= ~ICE_PROMISC_VLAN_RX;
		} else if (promisc_mask & ICE_PROMISC_VLAN_TX) {
			promisc_mask &= ~ICE_PROMISC_VLAN_TX;
			is_tx_fltr = true;
		}

		/* Set filter DA based on packet type */
		mac_addr = new_fltr.l_data.mac.mac_addr;
		if (pkt_type == BCAST_FLTR) {
			ice_memset(mac_addr, 0xff, ETH_ALEN, ICE_NONDMA_MEM);
		} else if (pkt_type == MCAST_FLTR ||
			   pkt_type == UCAST_FLTR) {
			/* Use the dummy ether header DA */
			ice_memcpy(mac_addr, dummy_eth_header, ETH_ALEN,
				   ICE_NONDMA_TO_NONDMA);
			if (pkt_type == MCAST_FLTR)
				mac_addr[0] |= 0x1;	/* Set multicast bit */
		}

		/* Need to reset this to zero for all iterations */
		new_fltr.flag = 0;
		if (is_tx_fltr) {
			new_fltr.flag |= ICE_FLTR_TX;
			new_fltr.src = hw_vsi_id;
		} else {
			new_fltr.flag |= ICE_FLTR_RX;
			new_fltr.src = lport;
		}

		new_fltr.fltr_act = ICE_FWD_TO_VSI;
		new_fltr.vsi_handle = vsi_handle;
		new_fltr.fwd_id.hw_vsi_id = hw_vsi_id;
		f_list_entry.fltr_info = new_fltr;
		recp_list = &sw->recp_list[recipe_id];

		status = ice_add_rule_internal(hw, recp_list, lport,
					       &f_list_entry);
		if (status != ICE_SUCCESS)
			goto set_promisc_exit;
	}

set_promisc_exit:
	return status;
}

/**
 * ice_set_vsi_promisc - set given VSI to given promiscuous mode(s)
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: mask of promiscuous config bits
 * @vid: VLAN ID to set VLAN promiscuous
 */
enum ice_status
ice_set_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
		    u16 vid)
{
	return _ice_set_vsi_promisc(hw, vsi_handle, promisc_mask, vid,
				    hw->port_info->lport,
				    hw->switch_info);
}

/**
 * _ice_set_vlan_vsi_promisc
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: mask of promiscuous config bits
 * @rm_vlan_promisc: Clear VLANs VSI promisc mode
 * @lport: logical port number to configure promisc mode
 * @sw: pointer to switch info struct for which function add rule
 *
 * Configure VSI with all associated VLANs to given promiscuous mode(s)
 */
static enum ice_status
_ice_set_vlan_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
			  bool rm_vlan_promisc, u8 lport,
			  struct ice_switch_info *sw)
{
	struct ice_fltr_list_entry *list_itr, *tmp;
	struct LIST_HEAD_TYPE vsi_list_head;
	struct LIST_HEAD_TYPE *vlan_head;
	struct ice_lock *vlan_lock; /* Lock to protect filter rule list */
	enum ice_status status;
	u16 vlan_id;

	INIT_LIST_HEAD(&vsi_list_head);
	vlan_lock = &sw->recp_list[ICE_SW_LKUP_VLAN].filt_rule_lock;
	vlan_head = &sw->recp_list[ICE_SW_LKUP_VLAN].filt_rules;
	ice_acquire_lock(vlan_lock);
	status = ice_add_to_vsi_fltr_list(hw, vsi_handle, vlan_head,
					  &vsi_list_head);
	ice_release_lock(vlan_lock);
	if (status)
		goto free_fltr_list;

	LIST_FOR_EACH_ENTRY(list_itr, &vsi_list_head, ice_fltr_list_entry,
			    list_entry) {
		vlan_id = list_itr->fltr_info.l_data.vlan.vlan_id;
		if (rm_vlan_promisc)
			status =  _ice_clear_vsi_promisc(hw, vsi_handle,
							 promisc_mask,
							 vlan_id, sw);
		else
			status =  _ice_set_vsi_promisc(hw, vsi_handle,
						       promisc_mask, vlan_id,
						       lport, sw);
		if (status)
			break;
	}

free_fltr_list:
	LIST_FOR_EACH_ENTRY_SAFE(list_itr, tmp, &vsi_list_head,
				 ice_fltr_list_entry, list_entry) {
		LIST_DEL(&list_itr->list_entry);
		ice_free(hw, list_itr);
	}
	return status;
}

/**
 * ice_set_vlan_vsi_promisc
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: mask of promiscuous config bits
 * @rm_vlan_promisc: Clear VLANs VSI promisc mode
 *
 * Configure VSI with all associated VLANs to given promiscuous mode(s)
 */
enum ice_status
ice_set_vlan_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
			 bool rm_vlan_promisc)
{
	return _ice_set_vlan_vsi_promisc(hw, vsi_handle, promisc_mask,
					 rm_vlan_promisc, hw->port_info->lport,
					 hw->switch_info);
}

/**
 * ice_remove_vsi_lkup_fltr - Remove lookup type filters for a VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @recp_list: recipe list from which function remove fltr
 * @lkup: switch rule filter lookup type
 */
static void
ice_remove_vsi_lkup_fltr(struct ice_hw *hw, u16 vsi_handle,
			 struct ice_sw_recipe *recp_list,
			 enum ice_sw_lkup_type lkup)
{
	struct ice_fltr_list_entry *fm_entry;
	struct LIST_HEAD_TYPE remove_list_head;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_fltr_list_entry *tmp;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status status;

	INIT_LIST_HEAD(&remove_list_head);
	rule_lock = &recp_list[lkup].filt_rule_lock;
	rule_head = &recp_list[lkup].filt_rules;
	ice_acquire_lock(rule_lock);
	status = ice_add_to_vsi_fltr_list(hw, vsi_handle, rule_head,
					  &remove_list_head);
	ice_release_lock(rule_lock);
	if (status)
		return;

	switch (lkup) {
	case ICE_SW_LKUP_MAC:
		ice_remove_mac_rule(hw, &remove_list_head, &recp_list[lkup]);
		break;
	case ICE_SW_LKUP_VLAN:
		ice_remove_vlan_rule(hw, &remove_list_head, &recp_list[lkup]);
		break;
	case ICE_SW_LKUP_PROMISC:
	case ICE_SW_LKUP_PROMISC_VLAN:
		ice_remove_promisc(hw, lkup, &remove_list_head);
		break;
	case ICE_SW_LKUP_MAC_VLAN:
		ice_remove_mac_vlan(hw, &remove_list_head);
		break;
	case ICE_SW_LKUP_ETHERTYPE:
	case ICE_SW_LKUP_ETHERTYPE_MAC:
		ice_remove_eth_mac(hw, &remove_list_head);
		break;
	case ICE_SW_LKUP_DFLT:
		ice_debug(hw, ICE_DBG_SW,
			  "Remove filters for this lookup type hasn't been implemented yet\n");
		break;
	case ICE_SW_LKUP_LAST:
		ice_debug(hw, ICE_DBG_SW, "Unsupported lookup type\n");
		break;
	}

	LIST_FOR_EACH_ENTRY_SAFE(fm_entry, tmp, &remove_list_head,
				 ice_fltr_list_entry, list_entry) {
		LIST_DEL(&fm_entry->list_entry);
		ice_free(hw, fm_entry);
	}
}

/**
 * ice_remove_vsi_fltr_rule - Remove all filters for a VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @sw: pointer to switch info struct
 */
static void
ice_remove_vsi_fltr_rule(struct ice_hw *hw, u16 vsi_handle,
			 struct ice_switch_info *sw)
{
	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_MAC);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_MAC_VLAN);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_PROMISC);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_VLAN);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_DFLT);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_ETHERTYPE);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_ETHERTYPE_MAC);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_PROMISC_VLAN);
}

/**
 * ice_remove_vsi_fltr - Remove all filters for a VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 */
void ice_remove_vsi_fltr(struct ice_hw *hw, u16 vsi_handle)
{
	ice_remove_vsi_fltr_rule(hw, vsi_handle, hw->switch_info);
}

/**
 * ice_alloc_res_cntr - allocating resource counter
 * @hw: pointer to the hardware structure
 * @type: type of resource
 * @alloc_shared: if set it is shared else dedicated
 * @num_items: number of entries requested for FD resource type
 * @counter_id: counter index returned by AQ call
 */
enum ice_status
ice_alloc_res_cntr(struct ice_hw *hw, u8 type, u8 alloc_shared, u16 num_items,
		   u16 *counter_id)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	enum ice_status status;
	u16 buf_len;

	/* Allocate resource */
	buf_len = sizeof(*buf);
	buf = (struct ice_aqc_alloc_free_res_elem *)
		ice_malloc(hw, buf_len);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	buf->num_elems = CPU_TO_LE16(num_items);
	buf->res_type = CPU_TO_LE16(((type << ICE_AQC_RES_TYPE_S) &
				      ICE_AQC_RES_TYPE_M) | alloc_shared);

	status = ice_aq_alloc_free_res(hw, 1, buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);
	if (status)
		goto exit;

	*counter_id = LE16_TO_CPU(buf->elem[0].e.sw_resp);

exit:
	ice_free(hw, buf);
	return status;
}

/**
 * ice_free_res_cntr - free resource counter
 * @hw: pointer to the hardware structure
 * @type: type of resource
 * @alloc_shared: if set it is shared else dedicated
 * @num_items: number of entries to be freed for FD resource type
 * @counter_id: counter ID resource which needs to be freed
 */
enum ice_status
ice_free_res_cntr(struct ice_hw *hw, u8 type, u8 alloc_shared, u16 num_items,
		  u16 counter_id)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	enum ice_status status;
	u16 buf_len;

	/* Free resource */
	buf_len = sizeof(*buf);
	buf = (struct ice_aqc_alloc_free_res_elem *)
		ice_malloc(hw, buf_len);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	buf->num_elems = CPU_TO_LE16(num_items);
	buf->res_type = CPU_TO_LE16(((type << ICE_AQC_RES_TYPE_S) &
				      ICE_AQC_RES_TYPE_M) | alloc_shared);
	buf->elem[0].e.sw_resp = CPU_TO_LE16(counter_id);

	status = ice_aq_alloc_free_res(hw, 1, buf, buf_len,
				       ice_aqc_opc_free_res, NULL);
	if (status)
		ice_debug(hw, ICE_DBG_SW,
			  "counter resource could not be freed\n");

	ice_free(hw, buf);
	return status;
}

/**
 * ice_alloc_vlan_res_counter - obtain counter resource for VLAN type
 * @hw: pointer to the hardware structure
 * @counter_id: returns counter index
 */
enum ice_status ice_alloc_vlan_res_counter(struct ice_hw *hw, u16 *counter_id)
{
	return ice_alloc_res_cntr(hw, ICE_AQC_RES_TYPE_VLAN_COUNTER,
				  ICE_AQC_RES_TYPE_FLAG_DEDICATED, 1,
				  counter_id);
}

/**
 * ice_free_vlan_res_counter - Free counter resource for VLAN type
 * @hw: pointer to the hardware structure
 * @counter_id: counter index to be freed
 */
enum ice_status ice_free_vlan_res_counter(struct ice_hw *hw, u16 counter_id)
{
	return ice_free_res_cntr(hw, ICE_AQC_RES_TYPE_VLAN_COUNTER,
				 ICE_AQC_RES_TYPE_FLAG_DEDICATED, 1,
				 counter_id);
}

/**
 * ice_alloc_res_lg_act - add large action resource
 * @hw: pointer to the hardware structure
 * @l_id: large action ID to fill it in
 * @num_acts: number of actions to hold with a large action entry
 */
static enum ice_status
ice_alloc_res_lg_act(struct ice_hw *hw, u16 *l_id, u16 num_acts)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	enum ice_status status;
	u16 buf_len;

	if (num_acts > ICE_MAX_LG_ACT || num_acts == 0)
		return ICE_ERR_PARAM;

	/* Allocate resource for large action */
	buf_len = sizeof(*sw_buf);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)
		ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	sw_buf->num_elems = CPU_TO_LE16(1);

	/* If num_acts is 1, use ICE_AQC_RES_TYPE_WIDE_TABLE_1.
	 * If num_acts is 2, use ICE_AQC_RES_TYPE_WIDE_TABLE_3.
	 * If num_acts is greater than 2, then use
	 * ICE_AQC_RES_TYPE_WIDE_TABLE_4.
	 * The num_acts cannot exceed 4. This was ensured at the
	 * beginning of the function.
	 */
	if (num_acts == 1)
		sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_WIDE_TABLE_1);
	else if (num_acts == 2)
		sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_WIDE_TABLE_2);
	else
		sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_WIDE_TABLE_4);

	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);
	if (!status)
		*l_id = LE16_TO_CPU(sw_buf->elem[0].e.sw_resp);

	ice_free(hw, sw_buf);
	return status;
}

/**
 * ice_add_mac_with_sw_marker - add filter with sw marker
 * @hw: pointer to the hardware structure
 * @f_info: filter info structure containing the MAC filter information
 * @sw_marker: sw marker to tag the Rx descriptor with
 */
enum ice_status
ice_add_mac_with_sw_marker(struct ice_hw *hw, struct ice_fltr_info *f_info,
			   u16 sw_marker)
{
	struct ice_fltr_mgmt_list_entry *m_entry;
	struct ice_fltr_list_entry fl_info;
	struct ice_sw_recipe *recp_list;
	struct LIST_HEAD_TYPE l_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status ret;
	bool entry_exists;
	u16 lg_act_id;

	if (f_info->fltr_act != ICE_FWD_TO_VSI)
		return ICE_ERR_PARAM;

	if (f_info->lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	if (sw_marker == ICE_INVAL_SW_MARKER_ID)
		return ICE_ERR_PARAM;

	if (!ice_is_vsi_valid(hw, f_info->vsi_handle))
		return ICE_ERR_PARAM;
	f_info->fwd_id.hw_vsi_id = ice_get_hw_vsi_num(hw, f_info->vsi_handle);

	/* Add filter if it doesn't exist so then the adding of large
	 * action always results in update
	 */

	INIT_LIST_HEAD(&l_head);
	fl_info.fltr_info = *f_info;
	LIST_ADD(&fl_info.list_entry, &l_head);

	entry_exists = false;
	ret = ice_add_mac_rule(hw, &l_head, hw->switch_info,
			       hw->port_info->lport);
	if (ret == ICE_ERR_ALREADY_EXISTS)
		entry_exists = true;
	else if (ret)
		return ret;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC];
	rule_lock = &recp_list->filt_rule_lock;
	ice_acquire_lock(rule_lock);
	/* Get the book keeping entry for the filter */
	m_entry = ice_find_rule_entry(&recp_list->filt_rules, f_info);
	if (!m_entry)
		goto exit_error;

	/* If counter action was enabled for this rule then don't enable
	 * sw marker large action
	 */
	if (m_entry->counter_index != ICE_INVAL_COUNTER_ID) {
		ret = ICE_ERR_PARAM;
		goto exit_error;
	}

	/* if same marker was added before */
	if (m_entry->sw_marker_id == sw_marker) {
		ret = ICE_ERR_ALREADY_EXISTS;
		goto exit_error;
	}

	/* Allocate a hardware table entry to hold large act. Three actions
	 * for marker based large action
	 */
	ret = ice_alloc_res_lg_act(hw, &lg_act_id, 3);
	if (ret)
		goto exit_error;

	if (lg_act_id == ICE_INVAL_LG_ACT_INDEX)
		goto exit_error;

	/* Update the switch rule to add the marker action */
	ret = ice_add_marker_act(hw, m_entry, sw_marker, lg_act_id);
	if (!ret) {
		ice_release_lock(rule_lock);
		return ret;
	}

exit_error:
	ice_release_lock(rule_lock);
	/* only remove entry if it did not exist previously */
	if (!entry_exists)
		ret = ice_remove_mac(hw, &l_head);

	return ret;
}

/**
 * ice_add_mac_with_counter - add filter with counter enabled
 * @hw: pointer to the hardware structure
 * @f_info: pointer to filter info structure containing the MAC filter
 *          information
 */
enum ice_status
ice_add_mac_with_counter(struct ice_hw *hw, struct ice_fltr_info *f_info)
{
	struct ice_fltr_mgmt_list_entry *m_entry;
	struct ice_fltr_list_entry fl_info;
	struct ice_sw_recipe *recp_list;
	struct LIST_HEAD_TYPE l_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status ret;
	bool entry_exist;
	u16 counter_id;
	u16 lg_act_id;

	if (f_info->fltr_act != ICE_FWD_TO_VSI)
		return ICE_ERR_PARAM;

	if (f_info->lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	if (!ice_is_vsi_valid(hw, f_info->vsi_handle))
		return ICE_ERR_PARAM;
	f_info->fwd_id.hw_vsi_id = ice_get_hw_vsi_num(hw, f_info->vsi_handle);
	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC];

	entry_exist = false;

	rule_lock = &recp_list->filt_rule_lock;

	/* Add filter if it doesn't exist so then the adding of large
	 * action always results in update
	 */
	INIT_LIST_HEAD(&l_head);

	fl_info.fltr_info = *f_info;
	LIST_ADD(&fl_info.list_entry, &l_head);

	ret = ice_add_mac_rule(hw, &l_head, hw->switch_info,
			       hw->port_info->lport);
	if (ret == ICE_ERR_ALREADY_EXISTS)
		entry_exist = true;
	else if (ret)
		return ret;

	ice_acquire_lock(rule_lock);
	m_entry = ice_find_rule_entry(&recp_list->filt_rules, f_info);
	if (!m_entry) {
		ret = ICE_ERR_BAD_PTR;
		goto exit_error;
	}

	/* Don't enable counter for a filter for which sw marker was enabled */
	if (m_entry->sw_marker_id != ICE_INVAL_SW_MARKER_ID) {
		ret = ICE_ERR_PARAM;
		goto exit_error;
	}

	/* If a counter was already enabled then don't need to add again */
	if (m_entry->counter_index != ICE_INVAL_COUNTER_ID) {
		ret = ICE_ERR_ALREADY_EXISTS;
		goto exit_error;
	}

	/* Allocate a hardware table entry to VLAN counter */
	ret = ice_alloc_vlan_res_counter(hw, &counter_id);
	if (ret)
		goto exit_error;

	/* Allocate a hardware table entry to hold large act. Two actions for
	 * counter based large action
	 */
	ret = ice_alloc_res_lg_act(hw, &lg_act_id, 2);
	if (ret)
		goto exit_error;

	if (lg_act_id == ICE_INVAL_LG_ACT_INDEX)
		goto exit_error;

	/* Update the switch rule to add the counter action */
	ret = ice_add_counter_act(hw, m_entry, counter_id, lg_act_id);
	if (!ret) {
		ice_release_lock(rule_lock);
		return ret;
	}

exit_error:
	ice_release_lock(rule_lock);
	/* only remove entry if it did not exist previously */
	if (!entry_exist)
		ret = ice_remove_mac(hw, &l_head);

	return ret;
}

/* This is mapping table entry that maps every word within a given protocol
 * structure to the real byte offset as per the specification of that
 * protocol header.
 * for example dst address is 3 words in ethertype header and corresponding
 * bytes are 0, 2, 3 in the actual packet header and src address is at 4, 6, 8
 * IMPORTANT: Every structure part of "ice_prot_hdr" union should have a
 * matching entry describing its field. This needs to be updated if new
 * structure is added to that union.
 */
static const struct ice_prot_ext_tbl_entry ice_prot_ext[ICE_PROTOCOL_LAST] = {
	{ ICE_MAC_OFOS,		{ 0, 2, 4, 6, 8, 10, 12 } },
	{ ICE_MAC_IL,		{ 0, 2, 4, 6, 8, 10, 12 } },
	{ ICE_ETYPE_OL,		{ 0 } },
	{ ICE_VLAN_OFOS,	{ 0, 2 } },
	{ ICE_IPV4_OFOS,	{ 0, 2, 4, 6, 8, 10, 12, 14, 16, 18 } },
	{ ICE_IPV4_IL,		{ 0, 2, 4, 6, 8, 10, 12, 14, 16, 18 } },
	{ ICE_IPV6_OFOS,	{ 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24,
				 26, 28, 30, 32, 34, 36, 38 } },
	{ ICE_IPV6_IL,		{ 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24,
				 26, 28, 30, 32, 34, 36, 38 } },
	{ ICE_TCP_IL,		{ 0, 2 } },
	{ ICE_UDP_OF,		{ 0, 2 } },
	{ ICE_UDP_ILOS,		{ 0, 2 } },
	{ ICE_SCTP_IL,		{ 0, 2 } },
	{ ICE_VXLAN,		{ 8, 10, 12, 14 } },
	{ ICE_GENEVE,		{ 8, 10, 12, 14 } },
	{ ICE_VXLAN_GPE,	{ 8, 10, 12, 14 } },
	{ ICE_NVGRE,		{ 0, 2, 4, 6 } },
	{ ICE_GTP,		{ 8, 10, 12, 14, 16, 18, 20 } },
	{ ICE_PPPOE,		{ 0, 2, 4, 6 } },
	{ ICE_PFCP,		{ 8, 10, 12, 14, 16, 18, 20, 22 } },
	{ ICE_L2TPV3,		{ 0, 2, 4, 6, 8, 10 } },
	{ ICE_ESP,		{ 0, 2, 4, 6 } },
	{ ICE_AH,		{ 0, 2, 4, 6, 8, 10 } },
	{ ICE_NAT_T,		{ 8, 10, 12, 14 } },
};

/* The following table describes preferred grouping of recipes.
 * If a recipe that needs to be programmed is a superset or matches one of the
 * following combinations, then the recipe needs to be chained as per the
 * following policy.
 */

static const struct ice_protocol_entry ice_prot_id_tbl[ICE_PROTOCOL_LAST] = {
	{ ICE_MAC_OFOS,		ICE_MAC_OFOS_HW },
	{ ICE_MAC_IL,		ICE_MAC_IL_HW },
	{ ICE_ETYPE_OL,		ICE_ETYPE_OL_HW },
	{ ICE_VLAN_OFOS,	ICE_VLAN_OL_HW },
	{ ICE_IPV4_OFOS,	ICE_IPV4_OFOS_HW },
	{ ICE_IPV4_IL,		ICE_IPV4_IL_HW },
	{ ICE_IPV6_OFOS,	ICE_IPV6_OFOS_HW },
	{ ICE_IPV6_IL,		ICE_IPV6_IL_HW },
	{ ICE_TCP_IL,		ICE_TCP_IL_HW },
	{ ICE_UDP_OF,		ICE_UDP_OF_HW },
	{ ICE_UDP_ILOS,		ICE_UDP_ILOS_HW },
	{ ICE_SCTP_IL,		ICE_SCTP_IL_HW },
	{ ICE_VXLAN,		ICE_UDP_OF_HW },
	{ ICE_GENEVE,		ICE_UDP_OF_HW },
	{ ICE_VXLAN_GPE,	ICE_UDP_OF_HW },
	{ ICE_NVGRE,		ICE_GRE_OF_HW },
	{ ICE_GTP,		ICE_UDP_OF_HW },
	{ ICE_PPPOE,		ICE_PPPOE_HW },
	{ ICE_PFCP,		ICE_UDP_ILOS_HW },
	{ ICE_L2TPV3,		ICE_L2TPV3_HW },
	{ ICE_ESP,		ICE_ESP_HW },
	{ ICE_AH,		ICE_AH_HW },
	{ ICE_NAT_T,		ICE_UDP_ILOS_HW },
};

/**
 * ice_find_recp - find a recipe
 * @hw: pointer to the hardware structure
 * @lkup_exts: extension sequence to match
 *
 * Returns index of matching recipe, or ICE_MAX_NUM_RECIPES if not found.
 */
static u16 ice_find_recp(struct ice_hw *hw, struct ice_prot_lkup_ext *lkup_exts,
			 enum ice_sw_tunnel_type tun_type)
{
	bool refresh_required = true;
	struct ice_sw_recipe *recp;
	u8 i;

	/* Walk through existing recipes to find a match */
	recp = hw->switch_info->recp_list;
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		/* If recipe was not created for this ID, in SW bookkeeping,
		 * check if FW has an entry for this recipe. If the FW has an
		 * entry update it in our SW bookkeeping and continue with the
		 * matching.
		 */
		if (!recp[i].recp_created)
			if (ice_get_recp_frm_fw(hw,
						hw->switch_info->recp_list, i,
						&refresh_required))
				continue;

		/* Skip inverse action recipes */
		if (recp[i].root_buf && recp[i].root_buf->content.act_ctrl &
		    ICE_AQ_RECIPE_ACT_INV_ACT)
			continue;

		/* if number of words we are looking for match */
		if (lkup_exts->n_val_words == recp[i].lkup_exts.n_val_words) {
			struct ice_fv_word *ar = recp[i].lkup_exts.fv_words;
			struct ice_fv_word *be = lkup_exts->fv_words;
			u16 *cr = recp[i].lkup_exts.field_mask;
			u16 *de = lkup_exts->field_mask;
			bool found = true;
			u8 pe, qr;

			/* ar, cr, and qr are related to the recipe words, while
			 * be, de, and pe are related to the lookup words
			 */
			for (pe = 0; pe < lkup_exts->n_val_words; pe++) {
				for (qr = 0; qr < recp[i].lkup_exts.n_val_words;
				     qr++) {
					if (ar[qr].off == be[pe].off &&
					    ar[qr].prot_id == be[pe].prot_id &&
					    cr[qr] == de[pe])
						/* Found the "pe"th word in the
						 * given recipe
						 */
						break;
				}
				/* After walking through all the words in the
				 * "i"th recipe if "p"th word was not found then
				 * this recipe is not what we are looking for.
				 * So break out from this loop and try the next
				 * recipe
				 */
				if (qr >= recp[i].lkup_exts.n_val_words) {
					found = false;
					break;
				}
			}
			/* If for "i"th recipe the found was never set to false
			 * then it means we found our match
			 */
			if (tun_type == recp[i].tun_type && found)
				return i; /* Return the recipe ID */
		}
	}
	return ICE_MAX_NUM_RECIPES;
}

/**
 * ice_prot_type_to_id - get protocol ID from protocol type
 * @type: protocol type
 * @id: pointer to variable that will receive the ID
 *
 * Returns true if found, false otherwise
 */
static bool ice_prot_type_to_id(enum ice_protocol_type type, u8 *id)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(ice_prot_id_tbl); i++)
		if (ice_prot_id_tbl[i].type == type) {
			*id = ice_prot_id_tbl[i].protocol_id;
			return true;
		}
	return false;
}

/**
 * ice_find_valid_words - count valid words
 * @rule: advanced rule with lookup information
 * @lkup_exts: byte offset extractions of the words that are valid
 *
 * calculate valid words in a lookup rule using mask value
 */
static u8
ice_fill_valid_words(struct ice_adv_lkup_elem *rule,
		     struct ice_prot_lkup_ext *lkup_exts)
{
	u8 j, word, prot_id, ret_val;

	if (!ice_prot_type_to_id(rule->type, &prot_id))
		return 0;

	word = lkup_exts->n_val_words;

	for (j = 0; j < sizeof(rule->m_u) / sizeof(u16); j++)
		if (((u16 *)&rule->m_u)[j] &&
		    rule->type < ARRAY_SIZE(ice_prot_ext)) {
			/* No more space to accommodate */
			if (word >= ICE_MAX_CHAIN_WORDS)
				return 0;
			lkup_exts->fv_words[word].off =
				ice_prot_ext[rule->type].offs[j];
			lkup_exts->fv_words[word].prot_id =
				ice_prot_id_tbl[rule->type].protocol_id;
			lkup_exts->field_mask[word] =
				BE16_TO_CPU(((__be16 *)&rule->m_u)[j]);
			word++;
		}

	ret_val = word - lkup_exts->n_val_words;
	lkup_exts->n_val_words = word;

	return ret_val;
}

/**
 * ice_create_first_fit_recp_def - Create a recipe grouping
 * @hw: pointer to the hardware structure
 * @lkup_exts: an array of protocol header extractions
 * @rg_list: pointer to a list that stores new recipe groups
 * @recp_cnt: pointer to a variable that stores returned number of recipe groups
 *
 * Using first fit algorithm, take all the words that are still not done
 * and start grouping them in 4-word groups. Each group makes up one
 * recipe.
 */
static enum ice_status
ice_create_first_fit_recp_def(struct ice_hw *hw,
			      struct ice_prot_lkup_ext *lkup_exts,
			      struct LIST_HEAD_TYPE *rg_list,
			      u8 *recp_cnt)
{
	struct ice_pref_recipe_group *grp = NULL;
	u8 j;

	*recp_cnt = 0;

	if (!lkup_exts->n_val_words) {
		struct ice_recp_grp_entry *entry;

		entry = (struct ice_recp_grp_entry *)
			ice_malloc(hw, sizeof(*entry));
		if (!entry)
			return ICE_ERR_NO_MEMORY;
		LIST_ADD(&entry->l_entry, rg_list);
		grp = &entry->r_group;
		(*recp_cnt)++;
		grp->n_val_pairs = 0;
	}

	/* Walk through every word in the rule to check if it is not done. If so
	 * then this word needs to be part of a new recipe.
	 */
	for (j = 0; j < lkup_exts->n_val_words; j++)
		if (!ice_is_bit_set(lkup_exts->done, j)) {
			if (!grp ||
			    grp->n_val_pairs == ICE_NUM_WORDS_RECIPE) {
				struct ice_recp_grp_entry *entry;

				entry = (struct ice_recp_grp_entry *)
					ice_malloc(hw, sizeof(*entry));
				if (!entry)
					return ICE_ERR_NO_MEMORY;
				LIST_ADD(&entry->l_entry, rg_list);
				grp = &entry->r_group;
				(*recp_cnt)++;
			}

			grp->pairs[grp->n_val_pairs].prot_id =
				lkup_exts->fv_words[j].prot_id;
			grp->pairs[grp->n_val_pairs].off =
				lkup_exts->fv_words[j].off;
			grp->mask[grp->n_val_pairs] = lkup_exts->field_mask[j];
			grp->n_val_pairs++;
		}

	return ICE_SUCCESS;
}

/**
 * ice_fill_fv_word_index - fill in the field vector indices for a recipe group
 * @hw: pointer to the hardware structure
 * @fv_list: field vector with the extraction sequence information
 * @rg_list: recipe groupings with protocol-offset pairs
 *
 * Helper function to fill in the field vector indices for protocol-offset
 * pairs. These indexes are then ultimately programmed into a recipe.
 */
static enum ice_status
ice_fill_fv_word_index(struct ice_hw *hw, struct LIST_HEAD_TYPE *fv_list,
		       struct LIST_HEAD_TYPE *rg_list)
{
	struct ice_sw_fv_list_entry *fv;
	struct ice_recp_grp_entry *rg;
	struct ice_fv_word *fv_ext;

	if (LIST_EMPTY(fv_list))
		return ICE_SUCCESS;

	fv = LIST_FIRST_ENTRY(fv_list, struct ice_sw_fv_list_entry, list_entry);
	fv_ext = fv->fv_ptr->ew;

	LIST_FOR_EACH_ENTRY(rg, rg_list, ice_recp_grp_entry, l_entry) {
		u8 i;

		for (i = 0; i < rg->r_group.n_val_pairs; i++) {
			struct ice_fv_word *pr;
			bool found = false;
			u16 mask;
			u8 j;

			pr = &rg->r_group.pairs[i];
			mask = rg->r_group.mask[i];

			for (j = 0; j < hw->blk[ICE_BLK_SW].es.fvw; j++)
				if (fv_ext[j].prot_id == pr->prot_id &&
				    fv_ext[j].off == pr->off) {
					found = true;

					/* Store index of field vector */
					rg->fv_idx[i] = j;
					rg->fv_mask[i] = mask;
					break;
				}

			/* Protocol/offset could not be found, caller gave an
			 * invalid pair
			 */
			if (!found)
				return ICE_ERR_PARAM;
		}
	}

	return ICE_SUCCESS;
}

/**
 * ice_find_free_recp_res_idx - find free result indexes for recipe
 * @hw: pointer to hardware structure
 * @profiles: bitmap of profiles that will be associated with the new recipe
 * @free_idx: pointer to variable to receive the free index bitmap
 *
 * The algorithm used here is:
 *	1. When creating a new recipe, create a set P which contains all
 *	   Profiles that will be associated with our new recipe
 *
 *	2. For each Profile p in set P:
 *	    a. Add all recipes associated with Profile p into set R
 *	    b. Optional : PossibleIndexes &= profile[p].possibleIndexes
 *		[initially PossibleIndexes should be 0xFFFFFFFFFFFFFFFF]
 *		i. Or just assume they all have the same possible indexes:
 *			44, 45, 46, 47
 *			i.e., PossibleIndexes = 0x0000F00000000000
 *
 *	3. For each Recipe r in set R:
 *	    a. UsedIndexes |= (bitwise or ) recipe[r].res_indexes
 *	    b. FreeIndexes = UsedIndexes ^ PossibleIndexes
 *
 *	FreeIndexes will contain the bits indicating the indexes free for use,
 *      then the code needs to update the recipe[r].used_result_idx_bits to
 *      indicate which indexes were selected for use by this recipe.
 */
static u16
ice_find_free_recp_res_idx(struct ice_hw *hw, const ice_bitmap_t *profiles,
			   ice_bitmap_t *free_idx)
{
	ice_declare_bitmap(possible_idx, ICE_MAX_FV_WORDS);
	ice_declare_bitmap(recipes, ICE_MAX_NUM_RECIPES);
	ice_declare_bitmap(used_idx, ICE_MAX_FV_WORDS);
	u16 count = 0;
	u16 bit;

	ice_zero_bitmap(possible_idx, ICE_MAX_FV_WORDS);
	ice_zero_bitmap(recipes, ICE_MAX_NUM_RECIPES);
	ice_zero_bitmap(used_idx, ICE_MAX_FV_WORDS);
	ice_zero_bitmap(free_idx, ICE_MAX_FV_WORDS);

	for (count = 0; count < ICE_MAX_FV_WORDS; count++)
		ice_set_bit(count, possible_idx);

	/* For each profile we are going to associate the recipe with, add the
	 * recipes that are associated with that profile. This will give us
	 * the set of recipes that our recipe may collide with. Also, determine
	 * what possible result indexes are usable given this set of profiles.
	 */
	bit = 0;
	while (ICE_MAX_NUM_PROFILES >
	       (bit = ice_find_next_bit(profiles, ICE_MAX_NUM_PROFILES, bit))) {
		ice_or_bitmap(recipes, recipes, profile_to_recipe[bit],
			      ICE_MAX_NUM_RECIPES);
		ice_and_bitmap(possible_idx, possible_idx,
			       hw->switch_info->prof_res_bm[bit],
			       ICE_MAX_FV_WORDS);
		bit++;
	}

	/* For each recipe that our new recipe may collide with, determine
	 * which indexes have been used.
	 */
	for (bit = 0; bit < ICE_MAX_NUM_RECIPES; bit++)
		if (ice_is_bit_set(recipes, bit)) {
			ice_or_bitmap(used_idx, used_idx,
				      hw->switch_info->recp_list[bit].res_idxs,
				      ICE_MAX_FV_WORDS);
		}

	ice_xor_bitmap(free_idx, used_idx, possible_idx, ICE_MAX_FV_WORDS);

	/* return number of free indexes */
	count = 0;
	bit = 0;
	while (ICE_MAX_FV_WORDS >
	       (bit = ice_find_next_bit(free_idx, ICE_MAX_FV_WORDS, bit))) {
		count++;
		bit++;
	}

	return count;
}

/**
 * ice_add_sw_recipe - function to call AQ calls to create switch recipe
 * @hw: pointer to hardware structure
 * @rm: recipe management list entry
 * @profiles: bitmap of profiles that will be associated.
 */
static enum ice_status
ice_add_sw_recipe(struct ice_hw *hw, struct ice_sw_recipe *rm,
		  ice_bitmap_t *profiles)
{
	ice_declare_bitmap(result_idx_bm, ICE_MAX_FV_WORDS);
	struct ice_aqc_recipe_data_elem *tmp;
	struct ice_aqc_recipe_data_elem *buf;
	struct ice_recp_grp_entry *entry;
	enum ice_status status;
	u16 free_res_idx;
	u16 recipe_count;
	u8 chain_idx;
	u8 recps = 0;

	/* When more than one recipe are required, another recipe is needed to
	 * chain them together. Matching a tunnel metadata ID takes up one of
	 * the match fields in the chaining recipe reducing the number of
	 * chained recipes by one.
	 */
	 /* check number of free result indices */
	ice_zero_bitmap(result_idx_bm, ICE_MAX_FV_WORDS);
	free_res_idx = ice_find_free_recp_res_idx(hw, profiles, result_idx_bm);

	ice_debug(hw, ICE_DBG_SW, "Result idx slots: %d, need %d\n",
		  free_res_idx, rm->n_grp_count);

	if (rm->n_grp_count > 1) {
		if (rm->n_grp_count > free_res_idx)
			return ICE_ERR_MAX_LIMIT;

		rm->n_grp_count++;
	}

	if (rm->n_grp_count > ICE_MAX_CHAIN_RECIPE)
		return ICE_ERR_MAX_LIMIT;

	tmp = (struct ice_aqc_recipe_data_elem *)ice_calloc(hw,
							    ICE_MAX_NUM_RECIPES,
							    sizeof(*tmp));
	if (!tmp)
		return ICE_ERR_NO_MEMORY;

	buf = (struct ice_aqc_recipe_data_elem *)
		ice_calloc(hw, rm->n_grp_count, sizeof(*buf));
	if (!buf) {
		status = ICE_ERR_NO_MEMORY;
		goto err_mem;
	}

	ice_zero_bitmap(rm->r_bitmap, ICE_MAX_NUM_RECIPES);
	recipe_count = ICE_MAX_NUM_RECIPES;
	status = ice_aq_get_recipe(hw, tmp, &recipe_count, ICE_SW_LKUP_MAC,
				   NULL);
	if (status || recipe_count == 0)
		goto err_unroll;

	/* Allocate the recipe resources, and configure them according to the
	 * match fields from protocol headers and extracted field vectors.
	 */
	chain_idx = ice_find_first_bit(result_idx_bm, ICE_MAX_FV_WORDS);
	LIST_FOR_EACH_ENTRY(entry, &rm->rg_list, ice_recp_grp_entry, l_entry) {
		u8 i;

		status = ice_alloc_recipe(hw, &entry->rid);
		if (status)
			goto err_unroll;

		/* Clear the result index of the located recipe, as this will be
		 * updated, if needed, later in the recipe creation process.
		 */
		tmp[0].content.result_indx = 0;

		buf[recps] = tmp[0];
		buf[recps].recipe_indx = (u8)entry->rid;
		/* if the recipe is a non-root recipe RID should be programmed
		 * as 0 for the rules to be applied correctly.
		 */
		buf[recps].content.rid = 0;
		ice_memset(&buf[recps].content.lkup_indx, 0,
			   sizeof(buf[recps].content.lkup_indx),
			   ICE_NONDMA_MEM);

		/* All recipes use look-up index 0 to match switch ID. */
		buf[recps].content.lkup_indx[0] = ICE_AQ_SW_ID_LKUP_IDX;
		buf[recps].content.mask[0] =
			CPU_TO_LE16(ICE_AQ_SW_ID_LKUP_MASK);
		/* Setup lkup_indx 1..4 to INVALID/ignore and set the mask
		 * to be 0
		 */
		for (i = 1; i <= ICE_NUM_WORDS_RECIPE; i++) {
			buf[recps].content.lkup_indx[i] = 0x80;
			buf[recps].content.mask[i] = 0;
		}

		for (i = 0; i < entry->r_group.n_val_pairs; i++) {
			buf[recps].content.lkup_indx[i + 1] = entry->fv_idx[i];
			buf[recps].content.mask[i + 1] =
				CPU_TO_LE16(entry->fv_mask[i]);
		}

		if (rm->n_grp_count > 1) {
			/* Checks to see if there really is a valid result index
			 * that can be used.
			 */
			if (chain_idx >= ICE_MAX_FV_WORDS) {
				ice_debug(hw, ICE_DBG_SW,
					  "No chain index available\n");
				status = ICE_ERR_MAX_LIMIT;
				goto err_unroll;
			}

			entry->chain_idx = chain_idx;
			buf[recps].content.result_indx =
				ICE_AQ_RECIPE_RESULT_EN |
				((chain_idx << ICE_AQ_RECIPE_RESULT_DATA_S) &
				 ICE_AQ_RECIPE_RESULT_DATA_M);
			ice_clear_bit(chain_idx, result_idx_bm);
			chain_idx = ice_find_first_bit(result_idx_bm,
						       ICE_MAX_FV_WORDS);
		}

		/* fill recipe dependencies */
		ice_zero_bitmap((ice_bitmap_t *)buf[recps].recipe_bitmap,
				ICE_MAX_NUM_RECIPES);
		ice_set_bit(buf[recps].recipe_indx,
			    (ice_bitmap_t *)buf[recps].recipe_bitmap);
		buf[recps].content.act_ctrl_fwd_priority = rm->priority;
		recps++;
	}

	if (rm->n_grp_count == 1) {
		rm->root_rid = buf[0].recipe_indx;
		ice_set_bit(buf[0].recipe_indx, rm->r_bitmap);
		buf[0].content.rid = rm->root_rid | ICE_AQ_RECIPE_ID_IS_ROOT;
		if (sizeof(buf[0].recipe_bitmap) >= sizeof(rm->r_bitmap)) {
			ice_memcpy(buf[0].recipe_bitmap, rm->r_bitmap,
				   sizeof(buf[0].recipe_bitmap),
				   ICE_NONDMA_TO_NONDMA);
		} else {
			status = ICE_ERR_BAD_PTR;
			goto err_unroll;
		}
		/* Applicable only for ROOT_RECIPE, set the fwd_priority for
		 * the recipe which is getting created if specified
		 * by user. Usually any advanced switch filter, which results
		 * into new extraction sequence, ended up creating a new recipe
		 * of type ROOT and usually recipes are associated with profiles
		 * Switch rule referreing newly created recipe, needs to have
		 * either/or 'fwd' or 'join' priority, otherwise switch rule
		 * evaluation will not happen correctly. In other words, if
		 * switch rule to be evaluated on priority basis, then recipe
		 * needs to have priority, otherwise it will be evaluated last.
		 */
		buf[0].content.act_ctrl_fwd_priority = rm->priority;
	} else {
		struct ice_recp_grp_entry *last_chain_entry;
		u16 rid, i;

		/* Allocate the last recipe that will chain the outcomes of the
		 * other recipes together
		 */
		status = ice_alloc_recipe(hw, &rid);
		if (status)
			goto err_unroll;

		buf[recps].recipe_indx = (u8)rid;
		buf[recps].content.rid = (u8)rid;
		buf[recps].content.rid |= ICE_AQ_RECIPE_ID_IS_ROOT;
		/* the new entry created should also be part of rg_list to
		 * make sure we have complete recipe
		 */
		last_chain_entry = (struct ice_recp_grp_entry *)ice_malloc(hw,
			sizeof(*last_chain_entry));
		if (!last_chain_entry) {
			status = ICE_ERR_NO_MEMORY;
			goto err_unroll;
		}
		last_chain_entry->rid = rid;
		ice_memset(&buf[recps].content.lkup_indx, 0,
			   sizeof(buf[recps].content.lkup_indx),
			   ICE_NONDMA_MEM);
		/* All recipes use look-up index 0 to match switch ID. */
		buf[recps].content.lkup_indx[0] = ICE_AQ_SW_ID_LKUP_IDX;
		buf[recps].content.mask[0] =
			CPU_TO_LE16(ICE_AQ_SW_ID_LKUP_MASK);
		for (i = 1; i <= ICE_NUM_WORDS_RECIPE; i++) {
			buf[recps].content.lkup_indx[i] =
				ICE_AQ_RECIPE_LKUP_IGNORE;
			buf[recps].content.mask[i] = 0;
		}

		i = 1;
		/* update r_bitmap with the recp that is used for chaining */
		ice_set_bit(rid, rm->r_bitmap);
		/* this is the recipe that chains all the other recipes so it
		 * should not have a chaining ID to indicate the same
		 */
		last_chain_entry->chain_idx = ICE_INVAL_CHAIN_IND;
		LIST_FOR_EACH_ENTRY(entry, &rm->rg_list, ice_recp_grp_entry,
				    l_entry) {
			last_chain_entry->fv_idx[i] = entry->chain_idx;
			buf[recps].content.lkup_indx[i] = entry->chain_idx;
			buf[recps].content.mask[i++] = CPU_TO_LE16(0xFFFF);
			ice_set_bit(entry->rid, rm->r_bitmap);
		}
		LIST_ADD(&last_chain_entry->l_entry, &rm->rg_list);
		if (sizeof(buf[recps].recipe_bitmap) >=
		    sizeof(rm->r_bitmap)) {
			ice_memcpy(buf[recps].recipe_bitmap, rm->r_bitmap,
				   sizeof(buf[recps].recipe_bitmap),
				   ICE_NONDMA_TO_NONDMA);
		} else {
			status = ICE_ERR_BAD_PTR;
			goto err_unroll;
		}
		buf[recps].content.act_ctrl_fwd_priority = rm->priority;

		recps++;
		rm->root_rid = (u8)rid;
	}
	status = ice_acquire_change_lock(hw, ICE_RES_WRITE);
	if (status)
		goto err_unroll;

	status = ice_aq_add_recipe(hw, buf, rm->n_grp_count, NULL);
	ice_release_change_lock(hw);
	if (status)
		goto err_unroll;

	/* Every recipe that just got created add it to the recipe
	 * book keeping list
	 */
	LIST_FOR_EACH_ENTRY(entry, &rm->rg_list, ice_recp_grp_entry, l_entry) {
		struct ice_switch_info *sw = hw->switch_info;
		bool is_root, idx_found = false;
		struct ice_sw_recipe *recp;
		u16 idx, buf_idx = 0;

		/* find buffer index for copying some data */
		for (idx = 0; idx < rm->n_grp_count; idx++)
			if (buf[idx].recipe_indx == entry->rid) {
				buf_idx = idx;
				idx_found = true;
			}

		if (!idx_found) {
			status = ICE_ERR_OUT_OF_RANGE;
			goto err_unroll;
		}

		recp = &sw->recp_list[entry->rid];
		is_root = (rm->root_rid == entry->rid);
		recp->is_root = is_root;

		recp->root_rid = entry->rid;
		recp->big_recp = (is_root && rm->n_grp_count > 1);

		ice_memcpy(&recp->ext_words, entry->r_group.pairs,
			   entry->r_group.n_val_pairs *
			   sizeof(struct ice_fv_word),
			   ICE_NONDMA_TO_NONDMA);

		ice_memcpy(recp->r_bitmap, buf[buf_idx].recipe_bitmap,
			   sizeof(recp->r_bitmap), ICE_NONDMA_TO_NONDMA);

		/* Copy non-result fv index values and masks to recipe. This
		 * call will also update the result recipe bitmask.
		 */
		ice_collect_result_idx(&buf[buf_idx], recp);

		/* for non-root recipes, also copy to the root, this allows
		 * easier matching of a complete chained recipe
		 */
		if (!is_root)
			ice_collect_result_idx(&buf[buf_idx],
					       &sw->recp_list[rm->root_rid]);

		recp->n_ext_words = entry->r_group.n_val_pairs;
		recp->chain_idx = entry->chain_idx;
		recp->priority = buf[buf_idx].content.act_ctrl_fwd_priority;
		recp->n_grp_count = rm->n_grp_count;
		recp->tun_type = rm->tun_type;
		recp->recp_created = true;
	}
	rm->root_buf = buf;
	ice_free(hw, tmp);
	return status;

err_unroll:
err_mem:
	ice_free(hw, tmp);
	ice_free(hw, buf);
	return status;
}

/**
 * ice_create_recipe_group - creates recipe group
 * @hw: pointer to hardware structure
 * @rm: recipe management list entry
 * @lkup_exts: lookup elements
 */
static enum ice_status
ice_create_recipe_group(struct ice_hw *hw, struct ice_sw_recipe *rm,
			struct ice_prot_lkup_ext *lkup_exts)
{
	enum ice_status status;
	u8 recp_count = 0;

	rm->n_grp_count = 0;

	/* Create recipes for words that are marked not done by packing them
	 * as best fit.
	 */
	status = ice_create_first_fit_recp_def(hw, lkup_exts,
					       &rm->rg_list, &recp_count);
	if (!status) {
		rm->n_grp_count += recp_count;
		rm->n_ext_words = lkup_exts->n_val_words;
		ice_memcpy(&rm->ext_words, lkup_exts->fv_words,
			   sizeof(rm->ext_words), ICE_NONDMA_TO_NONDMA);
		ice_memcpy(rm->word_masks, lkup_exts->field_mask,
			   sizeof(rm->word_masks), ICE_NONDMA_TO_NONDMA);
	}

	return status;
}

/**
 * ice_get_fv - get field vectors/extraction sequences for spec. lookup types
 * @hw: pointer to hardware structure
 * @lkups: lookup elements or match criteria for the advanced recipe, one
 *	   structure per protocol header
 * @lkups_cnt: number of protocols
 * @bm: bitmap of field vectors to consider
 * @fv_list: pointer to a list that holds the returned field vectors
 */
static enum ice_status
ice_get_fv(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups, u16 lkups_cnt,
	   ice_bitmap_t *bm, struct LIST_HEAD_TYPE *fv_list)
{
	enum ice_status status;
	u8 *prot_ids;
	u16 i;

	if (!lkups_cnt)
		return ICE_SUCCESS;

	prot_ids = (u8 *)ice_calloc(hw, lkups_cnt, sizeof(*prot_ids));
	if (!prot_ids)
		return ICE_ERR_NO_MEMORY;

	for (i = 0; i < lkups_cnt; i++)
		if (!ice_prot_type_to_id(lkups[i].type, &prot_ids[i])) {
			status = ICE_ERR_CFG;
			goto free_mem;
		}

	/* Find field vectors that include all specified protocol types */
	status = ice_get_sw_fv_list(hw, prot_ids, lkups_cnt, bm, fv_list);

free_mem:
	ice_free(hw, prot_ids);
	return status;
}

/**
 * ice_tun_type_match_mask - determine if tun type needs a match mask
 * @tun_type: tunnel type
 * @mask: mask to be used for the tunnel
 */
static bool ice_tun_type_match_word(enum ice_sw_tunnel_type tun_type, u16 *mask)
{
	switch (tun_type) {
	case ICE_SW_TUN_VXLAN_GPE:
	case ICE_SW_TUN_GENEVE:
	case ICE_SW_TUN_VXLAN:
	case ICE_SW_TUN_NVGRE:
	case ICE_SW_TUN_UDP:
	case ICE_ALL_TUNNELS:
		*mask = ICE_TUN_FLAG_MASK;
		return true;

	case ICE_SW_TUN_GENEVE_VLAN:
	case ICE_SW_TUN_VXLAN_VLAN:
		*mask = ICE_TUN_FLAG_MASK & ~ICE_TUN_FLAG_VLAN_MASK;
		return true;

	default:
		*mask = 0;
		return false;
	}
}

/**
 * ice_add_special_words - Add words that are not protocols, such as metadata
 * @rinfo: other information regarding the rule e.g. priority and action info
 * @lkup_exts: lookup word structure
 */
static enum ice_status
ice_add_special_words(struct ice_adv_rule_info *rinfo,
		      struct ice_prot_lkup_ext *lkup_exts)
{
	u16 mask;

	/* If this is a tunneled packet, then add recipe index to match the
	 * tunnel bit in the packet metadata flags.
	 */
	if (ice_tun_type_match_word(rinfo->tun_type, &mask)) {
		if (lkup_exts->n_val_words < ICE_MAX_CHAIN_WORDS) {
			u8 word = lkup_exts->n_val_words++;

			lkup_exts->fv_words[word].prot_id = ICE_META_DATA_ID_HW;
			lkup_exts->fv_words[word].off = ICE_TUN_FLAG_MDID_OFF;
			lkup_exts->field_mask[word] = mask;
		} else {
			return ICE_ERR_MAX_LIMIT;
		}
	}

	return ICE_SUCCESS;
}

/* ice_get_compat_fv_bitmap - Get compatible field vector bitmap for rule
 * @hw: pointer to hardware structure
 * @rinfo: other information regarding the rule e.g. priority and action info
 * @bm: pointer to memory for returning the bitmap of field vectors
 */
static void
ice_get_compat_fv_bitmap(struct ice_hw *hw, struct ice_adv_rule_info *rinfo,
			 ice_bitmap_t *bm)
{
	enum ice_prof_type prof_type;

	ice_zero_bitmap(bm, ICE_MAX_NUM_PROFILES);

	switch (rinfo->tun_type) {
	case ICE_NON_TUN:
		prof_type = ICE_PROF_NON_TUN;
		break;
	case ICE_ALL_TUNNELS:
		prof_type = ICE_PROF_TUN_ALL;
		break;
	case ICE_SW_TUN_VXLAN_GPE:
	case ICE_SW_TUN_GENEVE:
	case ICE_SW_TUN_GENEVE_VLAN:
	case ICE_SW_TUN_VXLAN:
	case ICE_SW_TUN_VXLAN_VLAN:
	case ICE_SW_TUN_UDP:
	case ICE_SW_TUN_GTP:
		prof_type = ICE_PROF_TUN_UDP;
		break;
	case ICE_SW_TUN_NVGRE:
		prof_type = ICE_PROF_TUN_GRE;
		break;
	case ICE_SW_TUN_PPPOE:
		prof_type = ICE_PROF_TUN_PPPOE;
		break;
	case ICE_SW_TUN_PPPOE_PAY:
		ice_set_bit(ICE_PROFID_PPPOE_PAY, bm);
		return;
	case ICE_SW_TUN_PPPOE_IPV4:
		ice_set_bit(ICE_PROFID_PPPOE_IPV4_OTHER, bm);
		ice_set_bit(ICE_PROFID_PPPOE_IPV4_UDP, bm);
		ice_set_bit(ICE_PROFID_PPPOE_IPV4_TCP, bm);
		return;
	case ICE_SW_TUN_PPPOE_IPV4_TCP:
		ice_set_bit(ICE_PROFID_PPPOE_IPV4_TCP, bm);
		return;
	case ICE_SW_TUN_PPPOE_IPV4_UDP:
		ice_set_bit(ICE_PROFID_PPPOE_IPV4_UDP, bm);
		return;
	case ICE_SW_TUN_PPPOE_IPV6:
		ice_set_bit(ICE_PROFID_PPPOE_IPV6_OTHER, bm);
		ice_set_bit(ICE_PROFID_PPPOE_IPV6_UDP, bm);
		ice_set_bit(ICE_PROFID_PPPOE_IPV6_TCP, bm);
		return;
	case ICE_SW_TUN_PPPOE_IPV6_TCP:
		ice_set_bit(ICE_PROFID_PPPOE_IPV6_TCP, bm);
		return;
	case ICE_SW_TUN_PPPOE_IPV6_UDP:
		ice_set_bit(ICE_PROFID_PPPOE_IPV6_UDP, bm);
		return;
	case ICE_SW_TUN_PROFID_IPV6_ESP:
	case ICE_SW_TUN_IPV6_ESP:
		ice_set_bit(ICE_PROFID_IPV6_ESP, bm);
		return;
	case ICE_SW_TUN_PROFID_IPV6_AH:
	case ICE_SW_TUN_IPV6_AH:
		ice_set_bit(ICE_PROFID_IPV6_AH, bm);
		return;
	case ICE_SW_TUN_PROFID_MAC_IPV6_L2TPV3:
	case ICE_SW_TUN_IPV6_L2TPV3:
		ice_set_bit(ICE_PROFID_MAC_IPV6_L2TPV3, bm);
		return;
	case ICE_SW_TUN_PROFID_IPV6_NAT_T:
	case ICE_SW_TUN_IPV6_NAT_T:
		ice_set_bit(ICE_PROFID_IPV6_NAT_T, bm);
		return;
	case ICE_SW_TUN_PROFID_IPV4_PFCP_NODE:
		ice_set_bit(ICE_PROFID_IPV4_PFCP_NODE, bm);
		return;
	case ICE_SW_TUN_PROFID_IPV4_PFCP_SESSION:
		ice_set_bit(ICE_PROFID_IPV4_PFCP_SESSION, bm);
		return;
	case ICE_SW_TUN_PROFID_IPV6_PFCP_NODE:
		ice_set_bit(ICE_PROFID_IPV6_PFCP_NODE, bm);
		return;
	case ICE_SW_TUN_PROFID_IPV6_PFCP_SESSION:
		ice_set_bit(ICE_PROFID_IPV6_PFCP_SESSION, bm);
		return;
	case ICE_SW_TUN_IPV4_NAT_T:
		ice_set_bit(ICE_PROFID_IPV4_NAT_T, bm);
		return;
	case ICE_SW_TUN_IPV4_L2TPV3:
		ice_set_bit(ICE_PROFID_MAC_IPV4_L2TPV3, bm);
		return;
	case ICE_SW_TUN_IPV4_ESP:
		ice_set_bit(ICE_PROFID_IPV4_ESP, bm);
		return;
	case ICE_SW_TUN_IPV4_AH:
		ice_set_bit(ICE_PROFID_IPV4_AH, bm);
		return;
	case ICE_SW_IPV4_TCP:
		ice_set_bit(ICE_PROFID_IPV4_TCP, bm);
		return;
	case ICE_SW_IPV4_UDP:
		ice_set_bit(ICE_PROFID_IPV4_UDP, bm);
		return;
	case ICE_SW_IPV6_TCP:
		ice_set_bit(ICE_PROFID_IPV6_TCP, bm);
		return;
	case ICE_SW_IPV6_UDP:
		ice_set_bit(ICE_PROFID_IPV6_UDP, bm);
		return;
	case ICE_SW_TUN_AND_NON_TUN:
	default:
		prof_type = ICE_PROF_ALL;
		break;
	}

	ice_get_sw_fv_bitmap(hw, prof_type, bm);
}

/**
 * ice_is_prof_rule - determine if rule type is a profile rule
 * @type: the rule type
 *
 * if the rule type is a profile rule, that means that there no field value
 * match required, in this case just a profile hit is required.
 */
bool ice_is_prof_rule(enum ice_sw_tunnel_type type)
{
	switch (type) {
	case ICE_SW_TUN_PROFID_IPV6_ESP:
	case ICE_SW_TUN_PROFID_IPV6_AH:
	case ICE_SW_TUN_PROFID_MAC_IPV6_L2TPV3:
	case ICE_SW_TUN_PROFID_IPV6_NAT_T:
	case ICE_SW_TUN_PROFID_IPV4_PFCP_NODE:
	case ICE_SW_TUN_PROFID_IPV4_PFCP_SESSION:
	case ICE_SW_TUN_PROFID_IPV6_PFCP_NODE:
	case ICE_SW_TUN_PROFID_IPV6_PFCP_SESSION:
		return true;
	default:
		break;
	}

	return false;
}

/**
 * ice_add_adv_recipe - Add an advanced recipe that is not part of the default
 * @hw: pointer to hardware structure
 * @lkups: lookup elements or match criteria for the advanced recipe, one
 *  structure per protocol header
 * @lkups_cnt: number of protocols
 * @rinfo: other information regarding the rule e.g. priority and action info
 * @rid: return the recipe ID of the recipe created
 */
static enum ice_status
ice_add_adv_recipe(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups,
		   u16 lkups_cnt, struct ice_adv_rule_info *rinfo, u16 *rid)
{
	ice_declare_bitmap(fv_bitmap, ICE_MAX_NUM_PROFILES);
	ice_declare_bitmap(profiles, ICE_MAX_NUM_PROFILES);
	struct ice_prot_lkup_ext *lkup_exts;
	struct ice_recp_grp_entry *r_entry;
	struct ice_sw_fv_list_entry *fvit;
	struct ice_recp_grp_entry *r_tmp;
	struct ice_sw_fv_list_entry *tmp;
	enum ice_status status = ICE_SUCCESS;
	struct ice_sw_recipe *rm;
	u8 i;

	if (!ice_is_prof_rule(rinfo->tun_type) && !lkups_cnt)
		return ICE_ERR_PARAM;

	lkup_exts = (struct ice_prot_lkup_ext *)
		ice_malloc(hw, sizeof(*lkup_exts));
	if (!lkup_exts)
		return ICE_ERR_NO_MEMORY;

	/* Determine the number of words to be matched and if it exceeds a
	 * recipe's restrictions
	 */
	for (i = 0; i < lkups_cnt; i++) {
		u16 count;

		if (lkups[i].type >= ICE_PROTOCOL_LAST) {
			status = ICE_ERR_CFG;
			goto err_free_lkup_exts;
		}

		count = ice_fill_valid_words(&lkups[i], lkup_exts);
		if (!count) {
			status = ICE_ERR_CFG;
			goto err_free_lkup_exts;
		}
	}

	rm = (struct ice_sw_recipe *)ice_malloc(hw, sizeof(*rm));
	if (!rm) {
		status = ICE_ERR_NO_MEMORY;
		goto err_free_lkup_exts;
	}

	/* Get field vectors that contain fields extracted from all the protocol
	 * headers being programmed.
	 */
	INIT_LIST_HEAD(&rm->fv_list);
	INIT_LIST_HEAD(&rm->rg_list);

	/* Get bitmap of field vectors (profiles) that are compatible with the
	 * rule request; only these will be searched in the subsequent call to
	 * ice_get_fv.
	 */
	ice_get_compat_fv_bitmap(hw, rinfo, fv_bitmap);

	status = ice_get_fv(hw, lkups, lkups_cnt, fv_bitmap, &rm->fv_list);
	if (status)
		goto err_unroll;

	/* Create any special protocol/offset pairs, such as looking at tunnel
	 * bits by extracting metadata
	 */
	status = ice_add_special_words(rinfo, lkup_exts);
	if (status)
		goto err_free_lkup_exts;

	/* Group match words into recipes using preferred recipe grouping
	 * criteria.
	 */
	status = ice_create_recipe_group(hw, rm, lkup_exts);
	if (status)
		goto err_unroll;

	/* set the recipe priority if specified */
	rm->priority = (u8)rinfo->priority;

	/* Find offsets from the field vector. Pick the first one for all the
	 * recipes.
	 */
	status = ice_fill_fv_word_index(hw, &rm->fv_list, &rm->rg_list);
	if (status)
		goto err_unroll;

	/* An empty FV list means to use all the profiles returned in the
	 * profile bitmap
	 */
	if (LIST_EMPTY(&rm->fv_list)) {
		u16 j;

		for (j = 0; j < ICE_MAX_NUM_PROFILES; j++)
			if (ice_is_bit_set(fv_bitmap, j)) {
				struct ice_sw_fv_list_entry *fvl;

				fvl = (struct ice_sw_fv_list_entry *)
					ice_malloc(hw, sizeof(*fvl));
				if (!fvl)
					goto err_unroll;
				fvl->fv_ptr = NULL;
				fvl->profile_id = j;
				LIST_ADD(&fvl->list_entry, &rm->fv_list);
			}
	}

	/* get bitmap of all profiles the recipe will be associated with */
	ice_zero_bitmap(profiles, ICE_MAX_NUM_PROFILES);
	LIST_FOR_EACH_ENTRY(fvit, &rm->fv_list, ice_sw_fv_list_entry,
			    list_entry) {
		ice_debug(hw, ICE_DBG_SW, "profile: %d\n", fvit->profile_id);
		ice_set_bit((u16)fvit->profile_id, profiles);
	}

	/* Look for a recipe which matches our requested fv / mask list */
	*rid = ice_find_recp(hw, lkup_exts, rinfo->tun_type);
	if (*rid < ICE_MAX_NUM_RECIPES)
		/* Success if found a recipe that match the existing criteria */
		goto err_unroll;

	rm->tun_type = rinfo->tun_type;
	/* Recipe we need does not exist, add a recipe */
	status = ice_add_sw_recipe(hw, rm, profiles);
	if (status)
		goto err_unroll;

	/* Associate all the recipes created with all the profiles in the
	 * common field vector.
	 */
	LIST_FOR_EACH_ENTRY(fvit, &rm->fv_list, ice_sw_fv_list_entry,
			    list_entry) {
		ice_declare_bitmap(r_bitmap, ICE_MAX_NUM_RECIPES);
		u16 j;

		status = ice_aq_get_recipe_to_profile(hw, fvit->profile_id,
						      (u8 *)r_bitmap, NULL);
		if (status)
			goto err_unroll;

		ice_or_bitmap(r_bitmap, r_bitmap, rm->r_bitmap,
			      ICE_MAX_NUM_RECIPES);
		status = ice_acquire_change_lock(hw, ICE_RES_WRITE);
		if (status)
			goto err_unroll;

		status = ice_aq_map_recipe_to_profile(hw, fvit->profile_id,
						      (u8 *)r_bitmap,
						      NULL);
		ice_release_change_lock(hw);

		if (status)
			goto err_unroll;

		/* Update profile to recipe bitmap array */
		ice_cp_bitmap(profile_to_recipe[fvit->profile_id], r_bitmap,
			      ICE_MAX_NUM_RECIPES);

		/* Update recipe to profile bitmap array */
		for (j = 0; j < ICE_MAX_NUM_RECIPES; j++)
			if (ice_is_bit_set(r_bitmap, j))
				ice_set_bit((u16)fvit->profile_id,
					    recipe_to_profile[j]);
	}

	*rid = rm->root_rid;
	ice_memcpy(&hw->switch_info->recp_list[*rid].lkup_exts,
		   lkup_exts, sizeof(*lkup_exts), ICE_NONDMA_TO_NONDMA);
err_unroll:
	LIST_FOR_EACH_ENTRY_SAFE(r_entry, r_tmp, &rm->rg_list,
				 ice_recp_grp_entry, l_entry) {
		LIST_DEL(&r_entry->l_entry);
		ice_free(hw, r_entry);
	}

	LIST_FOR_EACH_ENTRY_SAFE(fvit, tmp, &rm->fv_list, ice_sw_fv_list_entry,
				 list_entry) {
		LIST_DEL(&fvit->list_entry);
		ice_free(hw, fvit);
	}

	if (rm->root_buf)
		ice_free(hw, rm->root_buf);

	ice_free(hw, rm);

err_free_lkup_exts:
	ice_free(hw, lkup_exts);

	return status;
}

/**
 * ice_find_dummy_packet - find dummy packet by tunnel type
 *
 * @lkups: lookup elements or match criteria for the advanced recipe, one
 *	   structure per protocol header
 * @lkups_cnt: number of protocols
 * @tun_type: tunnel type from the match criteria
 * @pkt: dummy packet to fill according to filter match criteria
 * @pkt_len: packet length of dummy packet
 * @offsets: pointer to receive the pointer to the offsets for the packet
 */
static void
ice_find_dummy_packet(struct ice_adv_lkup_elem *lkups, u16 lkups_cnt,
		      enum ice_sw_tunnel_type tun_type, const u8 **pkt,
		      u16 *pkt_len,
		      const struct ice_dummy_pkt_offsets **offsets)
{
	bool tcp = false, udp = false, ipv6 = false, vlan = false;
	bool gre = false;
	u16 i;

	for (i = 0; i < lkups_cnt; i++) {
		if (lkups[i].type == ICE_UDP_ILOS)
			udp = true;
		else if (lkups[i].type == ICE_TCP_IL)
			tcp = true;
		else if (lkups[i].type == ICE_IPV6_OFOS)
			ipv6 = true;
		else if (lkups[i].type == ICE_VLAN_OFOS)
			vlan = true;
		else if (lkups[i].type == ICE_IPV4_OFOS &&
			 lkups[i].h_u.ipv4_hdr.protocol ==
				ICE_IPV4_NVGRE_PROTO_ID &&
			 lkups[i].m_u.ipv4_hdr.protocol ==
				0xFF)
			gre = true;
		else if (lkups[i].type == ICE_PPPOE &&
			 lkups[i].h_u.pppoe_hdr.ppp_prot_id ==
				CPU_TO_BE16(ICE_PPP_IPV6_PROTO_ID) &&
			 lkups[i].m_u.pppoe_hdr.ppp_prot_id ==
				0xFFFF)
			ipv6 = true;
		else if (lkups[i].type == ICE_ETYPE_OL &&
			 lkups[i].h_u.ethertype.ethtype_id ==
				CPU_TO_BE16(ICE_IPV6_ETHER_ID) &&
			 lkups[i].m_u.ethertype.ethtype_id ==
					0xFFFF)
			ipv6 = true;
		else if (lkups[i].type == ICE_IPV4_IL &&
			 lkups[i].h_u.ipv4_hdr.protocol ==
				ICE_TCP_PROTO_ID &&
			 lkups[i].m_u.ipv4_hdr.protocol ==
				0xFF)
			tcp = true;
	}

	if (tun_type == ICE_SW_TUN_IPV4_ESP) {
		*pkt = dummy_ipv4_esp_pkt;
		*pkt_len = sizeof(dummy_ipv4_esp_pkt);
		*offsets = dummy_ipv4_esp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_IPV6_ESP) {
		*pkt = dummy_ipv6_esp_pkt;
		*pkt_len = sizeof(dummy_ipv6_esp_pkt);
		*offsets = dummy_ipv6_esp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_IPV4_AH) {
		*pkt = dummy_ipv4_ah_pkt;
		*pkt_len = sizeof(dummy_ipv4_ah_pkt);
		*offsets = dummy_ipv4_ah_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_IPV6_AH) {
		*pkt = dummy_ipv6_ah_pkt;
		*pkt_len = sizeof(dummy_ipv6_ah_pkt);
		*offsets = dummy_ipv6_ah_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_IPV4_NAT_T) {
		*pkt = dummy_ipv4_nat_pkt;
		*pkt_len = sizeof(dummy_ipv4_nat_pkt);
		*offsets = dummy_ipv4_nat_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_IPV6_NAT_T) {
		*pkt = dummy_ipv6_nat_pkt;
		*pkt_len = sizeof(dummy_ipv6_nat_pkt);
		*offsets = dummy_ipv6_nat_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_IPV4_L2TPV3) {
		*pkt = dummy_ipv4_l2tpv3_pkt;
		*pkt_len = sizeof(dummy_ipv4_l2tpv3_pkt);
		*offsets = dummy_ipv4_l2tpv3_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_IPV6_L2TPV3) {
		*pkt = dummy_ipv6_l2tpv3_pkt;
		*pkt_len = sizeof(dummy_ipv6_l2tpv3_pkt);
		*offsets = dummy_ipv6_l2tpv3_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_GTP) {
		*pkt = dummy_udp_gtp_packet;
		*pkt_len = sizeof(dummy_udp_gtp_packet);
		*offsets = dummy_udp_gtp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_PPPOE && ipv6) {
		*pkt = dummy_pppoe_ipv6_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv6_packet);
		*offsets = dummy_pppoe_packet_offsets;
		return;
	} else if (tun_type == ICE_SW_TUN_PPPOE ||
		tun_type == ICE_SW_TUN_PPPOE_PAY) {
		*pkt = dummy_pppoe_ipv4_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv4_packet);
		*offsets = dummy_pppoe_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_PPPOE_IPV4) {
		*pkt = dummy_pppoe_ipv4_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv4_packet);
		*offsets = dummy_pppoe_packet_ipv4_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_PPPOE_IPV4_TCP) {
		*pkt = dummy_pppoe_ipv4_tcp_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv4_tcp_packet);
		*offsets = dummy_pppoe_ipv4_tcp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_PPPOE_IPV4_UDP) {
		*pkt = dummy_pppoe_ipv4_udp_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv4_udp_packet);
		*offsets = dummy_pppoe_ipv4_udp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_PPPOE_IPV6) {
		*pkt = dummy_pppoe_ipv6_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv6_packet);
		*offsets = dummy_pppoe_packet_ipv6_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_PPPOE_IPV6_TCP) {
		*pkt = dummy_pppoe_ipv6_tcp_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv6_tcp_packet);
		*offsets = dummy_pppoe_packet_ipv6_tcp_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_PPPOE_IPV6_UDP) {
		*pkt = dummy_pppoe_ipv6_udp_packet;
		*pkt_len = sizeof(dummy_pppoe_ipv6_udp_packet);
		*offsets = dummy_pppoe_packet_ipv6_udp_offsets;
		return;
	}

	if (tun_type == ICE_SW_IPV4_TCP) {
		*pkt = dummy_tcp_packet;
		*pkt_len = sizeof(dummy_tcp_packet);
		*offsets = dummy_tcp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_IPV4_UDP) {
		*pkt = dummy_udp_packet;
		*pkt_len = sizeof(dummy_udp_packet);
		*offsets = dummy_udp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_IPV6_TCP) {
		*pkt = dummy_tcp_ipv6_packet;
		*pkt_len = sizeof(dummy_tcp_ipv6_packet);
		*offsets = dummy_tcp_ipv6_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_IPV6_UDP) {
		*pkt = dummy_udp_ipv6_packet;
		*pkt_len = sizeof(dummy_udp_ipv6_packet);
		*offsets = dummy_udp_ipv6_packet_offsets;
		return;
	}

	if (tun_type == ICE_ALL_TUNNELS) {
		*pkt = dummy_gre_udp_packet;
		*pkt_len = sizeof(dummy_gre_udp_packet);
		*offsets = dummy_gre_udp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_NVGRE || gre) {
		if (tcp) {
			*pkt = dummy_gre_tcp_packet;
			*pkt_len = sizeof(dummy_gre_tcp_packet);
			*offsets = dummy_gre_tcp_packet_offsets;
			return;
		}

		*pkt = dummy_gre_udp_packet;
		*pkt_len = sizeof(dummy_gre_udp_packet);
		*offsets = dummy_gre_udp_packet_offsets;
		return;
	}

	if (tun_type == ICE_SW_TUN_VXLAN || tun_type == ICE_SW_TUN_GENEVE ||
	    tun_type == ICE_SW_TUN_VXLAN_GPE || tun_type == ICE_SW_TUN_UDP ||
	    tun_type == ICE_SW_TUN_GENEVE_VLAN ||
	    tun_type == ICE_SW_TUN_VXLAN_VLAN) {
		if (tcp) {
			*pkt = dummy_udp_tun_tcp_packet;
			*pkt_len = sizeof(dummy_udp_tun_tcp_packet);
			*offsets = dummy_udp_tun_tcp_packet_offsets;
			return;
		}

		*pkt = dummy_udp_tun_udp_packet;
		*pkt_len = sizeof(dummy_udp_tun_udp_packet);
		*offsets = dummy_udp_tun_udp_packet_offsets;
		return;
	}

	if (udp && !ipv6) {
		if (vlan) {
			*pkt = dummy_vlan_udp_packet;
			*pkt_len = sizeof(dummy_vlan_udp_packet);
			*offsets = dummy_vlan_udp_packet_offsets;
			return;
		}
		*pkt = dummy_udp_packet;
		*pkt_len = sizeof(dummy_udp_packet);
		*offsets = dummy_udp_packet_offsets;
		return;
	} else if (udp && ipv6) {
		if (vlan) {
			*pkt = dummy_vlan_udp_ipv6_packet;
			*pkt_len = sizeof(dummy_vlan_udp_ipv6_packet);
			*offsets = dummy_vlan_udp_ipv6_packet_offsets;
			return;
		}
		*pkt = dummy_udp_ipv6_packet;
		*pkt_len = sizeof(dummy_udp_ipv6_packet);
		*offsets = dummy_udp_ipv6_packet_offsets;
		return;
	} else if ((tcp && ipv6) || ipv6) {
		if (vlan) {
			*pkt = dummy_vlan_tcp_ipv6_packet;
			*pkt_len = sizeof(dummy_vlan_tcp_ipv6_packet);
			*offsets = dummy_vlan_tcp_ipv6_packet_offsets;
			return;
		}
		*pkt = dummy_tcp_ipv6_packet;
		*pkt_len = sizeof(dummy_tcp_ipv6_packet);
		*offsets = dummy_tcp_ipv6_packet_offsets;
		return;
	}

	if (vlan) {
		*pkt = dummy_vlan_tcp_packet;
		*pkt_len = sizeof(dummy_vlan_tcp_packet);
		*offsets = dummy_vlan_tcp_packet_offsets;
	} else {
		*pkt = dummy_tcp_packet;
		*pkt_len = sizeof(dummy_tcp_packet);
		*offsets = dummy_tcp_packet_offsets;
	}
}

/**
 * ice_fill_adv_dummy_packet - fill a dummy packet with given match criteria
 *
 * @lkups: lookup elements or match criteria for the advanced recipe, one
 *	   structure per protocol header
 * @lkups_cnt: number of protocols
 * @s_rule: stores rule information from the match criteria
 * @dummy_pkt: dummy packet to fill according to filter match criteria
 * @pkt_len: packet length of dummy packet
 * @offsets: offset info for the dummy packet
 */
static enum ice_status
ice_fill_adv_dummy_packet(struct ice_adv_lkup_elem *lkups, u16 lkups_cnt,
			  struct ice_aqc_sw_rules_elem *s_rule,
			  const u8 *dummy_pkt, u16 pkt_len,
			  const struct ice_dummy_pkt_offsets *offsets)
{
	u8 *pkt;
	u16 i;

	/* Start with a packet with a pre-defined/dummy content. Then, fill
	 * in the header values to be looked up or matched.
	 */
	pkt = s_rule->pdata.lkup_tx_rx.hdr;

	ice_memcpy(pkt, dummy_pkt, pkt_len, ICE_NONDMA_TO_NONDMA);

	for (i = 0; i < lkups_cnt; i++) {
		enum ice_protocol_type type;
		u16 offset = 0, len = 0, j;
		bool found = false;

		/* find the start of this layer; it should be found since this
		 * was already checked when search for the dummy packet
		 */
		type = lkups[i].type;
		for (j = 0; offsets[j].type != ICE_PROTOCOL_LAST; j++) {
			if (type == offsets[j].type) {
				offset = offsets[j].offset;
				found = true;
				break;
			}
		}
		/* this should never happen in a correct calling sequence */
		if (!found)
			return ICE_ERR_PARAM;

		switch (lkups[i].type) {
		case ICE_MAC_OFOS:
		case ICE_MAC_IL:
			len = sizeof(struct ice_ether_hdr);
			break;
		case ICE_ETYPE_OL:
			len = sizeof(struct ice_ethtype_hdr);
			break;
		case ICE_VLAN_OFOS:
			len = sizeof(struct ice_vlan_hdr);
			break;
		case ICE_IPV4_OFOS:
		case ICE_IPV4_IL:
			len = sizeof(struct ice_ipv4_hdr);
			break;
		case ICE_IPV6_OFOS:
		case ICE_IPV6_IL:
			len = sizeof(struct ice_ipv6_hdr);
			break;
		case ICE_TCP_IL:
		case ICE_UDP_OF:
		case ICE_UDP_ILOS:
			len = sizeof(struct ice_l4_hdr);
			break;
		case ICE_SCTP_IL:
			len = sizeof(struct ice_sctp_hdr);
			break;
		case ICE_NVGRE:
			len = sizeof(struct ice_nvgre);
			break;
		case ICE_VXLAN:
		case ICE_GENEVE:
		case ICE_VXLAN_GPE:
			len = sizeof(struct ice_udp_tnl_hdr);
			break;

		case ICE_GTP:
			len = sizeof(struct ice_udp_gtp_hdr);
			break;
		case ICE_PPPOE:
			len = sizeof(struct ice_pppoe_hdr);
			break;
		case ICE_ESP:
			len = sizeof(struct ice_esp_hdr);
			break;
		case ICE_NAT_T:
			len = sizeof(struct ice_nat_t_hdr);
			break;
		case ICE_AH:
			len = sizeof(struct ice_ah_hdr);
			break;
		case ICE_L2TPV3:
			len = sizeof(struct ice_l2tpv3_sess_hdr);
			break;
		default:
			return ICE_ERR_PARAM;
		}

		/* the length should be a word multiple */
		if (len % ICE_BYTES_PER_WORD)
			return ICE_ERR_CFG;

		/* We have the offset to the header start, the length, the
		 * caller's header values and mask. Use this information to
		 * copy the data into the dummy packet appropriately based on
		 * the mask. Note that we need to only write the bits as
		 * indicated by the mask to make sure we don't improperly write
		 * over any significant packet data.
		 */
		for (j = 0; j < len / sizeof(u16); j++)
			if (((u16 *)&lkups[i].m_u)[j])
				((u16 *)(pkt + offset))[j] =
					(((u16 *)(pkt + offset))[j] &
					 ~((u16 *)&lkups[i].m_u)[j]) |
					(((u16 *)&lkups[i].h_u)[j] &
					 ((u16 *)&lkups[i].m_u)[j]);
	}

	s_rule->pdata.lkup_tx_rx.hdr_len = CPU_TO_LE16(pkt_len);

	return ICE_SUCCESS;
}

/**
 * ice_fill_adv_packet_tun - fill dummy packet with udp tunnel port
 * @hw: pointer to the hardware structure
 * @tun_type: tunnel type
 * @pkt: dummy packet to fill in
 * @offsets: offset info for the dummy packet
 */
static enum ice_status
ice_fill_adv_packet_tun(struct ice_hw *hw, enum ice_sw_tunnel_type tun_type,
			u8 *pkt, const struct ice_dummy_pkt_offsets *offsets)
{
	u16 open_port, i;

	switch (tun_type) {
	case ICE_SW_TUN_AND_NON_TUN:
	case ICE_SW_TUN_VXLAN_GPE:
	case ICE_SW_TUN_VXLAN:
	case ICE_SW_TUN_VXLAN_VLAN:
	case ICE_SW_TUN_UDP:
		if (!ice_get_open_tunnel_port(hw, TNL_VXLAN, &open_port))
			return ICE_ERR_CFG;
		break;

	case ICE_SW_TUN_GENEVE:
	case ICE_SW_TUN_GENEVE_VLAN:
		if (!ice_get_open_tunnel_port(hw, TNL_GENEVE, &open_port))
			return ICE_ERR_CFG;
		break;

	default:
		/* Nothing needs to be done for this tunnel type */
		return ICE_SUCCESS;
	}

	/* Find the outer UDP protocol header and insert the port number */
	for (i = 0; offsets[i].type != ICE_PROTOCOL_LAST; i++) {
		if (offsets[i].type == ICE_UDP_OF) {
			struct ice_l4_hdr *hdr;
			u16 offset;

			offset = offsets[i].offset;
			hdr = (struct ice_l4_hdr *)&pkt[offset];
			hdr->dst_port = CPU_TO_BE16(open_port);

			return ICE_SUCCESS;
		}
	}

	return ICE_ERR_CFG;
}

/**
 * ice_find_adv_rule_entry - Search a rule entry
 * @hw: pointer to the hardware structure
 * @lkups: lookup elements or match criteria for the advanced recipe, one
 *	   structure per protocol header
 * @lkups_cnt: number of protocols
 * @recp_id: recipe ID for which we are finding the rule
 * @rinfo: other information regarding the rule e.g. priority and action info
 *
 * Helper function to search for a given advance rule entry
 * Returns pointer to entry storing the rule if found
 */
static struct ice_adv_fltr_mgmt_list_entry *
ice_find_adv_rule_entry(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups,
			u16 lkups_cnt, u16 recp_id,
			struct ice_adv_rule_info *rinfo)
{
	struct ice_adv_fltr_mgmt_list_entry *list_itr;
	struct ice_switch_info *sw = hw->switch_info;
	int i;

	LIST_FOR_EACH_ENTRY(list_itr, &sw->recp_list[recp_id].filt_rules,
			    ice_adv_fltr_mgmt_list_entry, list_entry) {
		bool lkups_matched = true;

		if (lkups_cnt != list_itr->lkups_cnt)
			continue;
		for (i = 0; i < list_itr->lkups_cnt; i++)
			if (memcmp(&list_itr->lkups[i], &lkups[i],
				   sizeof(*lkups))) {
				lkups_matched = false;
				break;
			}
		if (rinfo->sw_act.flag == list_itr->rule_info.sw_act.flag &&
		    rinfo->tun_type == list_itr->rule_info.tun_type &&
		    lkups_matched)
			return list_itr;
	}
	return NULL;
}

/**
 * ice_adv_add_update_vsi_list
 * @hw: pointer to the hardware structure
 * @m_entry: pointer to current adv filter management list entry
 * @cur_fltr: filter information from the book keeping entry
 * @new_fltr: filter information with the new VSI to be added
 *
 * Call AQ command to add or update previously created VSI list with new VSI.
 *
 * Helper function to do book keeping associated with adding filter information
 * The algorithm to do the booking keeping is described below :
 * When a VSI needs to subscribe to a given advanced filter
 *	if only one VSI has been added till now
 *		Allocate a new VSI list and add two VSIs
 *		to this list using switch rule command
 *		Update the previously created switch rule with the
 *		newly created VSI list ID
 *	if a VSI list was previously created
 *		Add the new VSI to the previously created VSI list set
 *		using the update switch rule command
 */
static enum ice_status
ice_adv_add_update_vsi_list(struct ice_hw *hw,
			    struct ice_adv_fltr_mgmt_list_entry *m_entry,
			    struct ice_adv_rule_info *cur_fltr,
			    struct ice_adv_rule_info *new_fltr)
{
	enum ice_status status;
	u16 vsi_list_id = 0;

	if (cur_fltr->sw_act.fltr_act == ICE_FWD_TO_Q ||
	    cur_fltr->sw_act.fltr_act == ICE_FWD_TO_QGRP ||
	    cur_fltr->sw_act.fltr_act == ICE_DROP_PACKET)
		return ICE_ERR_NOT_IMPL;

	if ((new_fltr->sw_act.fltr_act == ICE_FWD_TO_Q ||
	     new_fltr->sw_act.fltr_act == ICE_FWD_TO_QGRP) &&
	    (cur_fltr->sw_act.fltr_act == ICE_FWD_TO_VSI ||
	     cur_fltr->sw_act.fltr_act == ICE_FWD_TO_VSI_LIST))
		return ICE_ERR_NOT_IMPL;

	if (m_entry->vsi_count < 2 && !m_entry->vsi_list_info) {
		 /* Only one entry existed in the mapping and it was not already
		  * a part of a VSI list. So, create a VSI list with the old and
		  * new VSIs.
		  */
		struct ice_fltr_info tmp_fltr;
		u16 vsi_handle_arr[2];

		/* A rule already exists with the new VSI being added */
		if (cur_fltr->sw_act.fwd_id.hw_vsi_id ==
		    new_fltr->sw_act.fwd_id.hw_vsi_id)
			return ICE_ERR_ALREADY_EXISTS;

		vsi_handle_arr[0] = cur_fltr->sw_act.vsi_handle;
		vsi_handle_arr[1] = new_fltr->sw_act.vsi_handle;
		status = ice_create_vsi_list_rule(hw, &vsi_handle_arr[0], 2,
						  &vsi_list_id,
						  ICE_SW_LKUP_LAST);
		if (status)
			return status;

		ice_memset(&tmp_fltr, 0, sizeof(tmp_fltr), ICE_NONDMA_MEM);
		tmp_fltr.flag = m_entry->rule_info.sw_act.flag;
		tmp_fltr.fltr_rule_id = cur_fltr->fltr_rule_id;
		tmp_fltr.fltr_act = ICE_FWD_TO_VSI_LIST;
		tmp_fltr.fwd_id.vsi_list_id = vsi_list_id;
		tmp_fltr.lkup_type = ICE_SW_LKUP_LAST;

		/* Update the previous switch rule of "forward to VSI" to
		 * "fwd to VSI list"
		 */
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr);
		if (status)
			return status;

		cur_fltr->sw_act.fwd_id.vsi_list_id = vsi_list_id;
		cur_fltr->sw_act.fltr_act = ICE_FWD_TO_VSI_LIST;
		m_entry->vsi_list_info =
			ice_create_vsi_list_map(hw, &vsi_handle_arr[0], 2,
						vsi_list_id);
	} else {
		u16 vsi_handle = new_fltr->sw_act.vsi_handle;

		if (!m_entry->vsi_list_info)
			return ICE_ERR_CFG;

		/* A rule already exists with the new VSI being added */
		if (ice_is_bit_set(m_entry->vsi_list_info->vsi_map, vsi_handle))
			return ICE_SUCCESS;

		/* Update the previously created VSI list set with
		 * the new VSI ID passed in
		 */
		vsi_list_id = cur_fltr->sw_act.fwd_id.vsi_list_id;

		status = ice_update_vsi_list_rule(hw, &vsi_handle, 1,
						  vsi_list_id, false,
						  ice_aqc_opc_update_sw_rules,
						  ICE_SW_LKUP_LAST);
		/* update VSI list mapping info with new VSI ID */
		if (!status)
			ice_set_bit(vsi_handle,
				    m_entry->vsi_list_info->vsi_map);
	}
	if (!status)
		m_entry->vsi_count++;
	return status;
}

/**
 * ice_add_adv_rule - helper function to create an advanced switch rule
 * @hw: pointer to the hardware structure
 * @lkups: information on the words that needs to be looked up. All words
 * together makes one recipe
 * @lkups_cnt: num of entries in the lkups array
 * @rinfo: other information related to the rule that needs to be programmed
 * @added_entry: this will return recipe_id, rule_id and vsi_handle. should be
 *               ignored is case of error.
 *
 * This function can program only 1 rule at a time. The lkups is used to
 * describe the all the words that forms the "lookup" portion of the recipe.
 * These words can span multiple protocols. Callers to this function need to
 * pass in a list of protocol headers with lookup information along and mask
 * that determines which words are valid from the given protocol header.
 * rinfo describes other information related to this rule such as forwarding
 * IDs, priority of this rule, etc.
 */
enum ice_status
ice_add_adv_rule(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups,
		 u16 lkups_cnt, struct ice_adv_rule_info *rinfo,
		 struct ice_rule_query_data *added_entry)
{
	struct ice_adv_fltr_mgmt_list_entry *m_entry, *adv_fltr = NULL;
	u16 rid = 0, i, pkt_len, rule_buf_sz, vsi_handle;
	const struct ice_dummy_pkt_offsets *pkt_offsets;
	struct ice_aqc_sw_rules_elem *s_rule = NULL;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_switch_info *sw;
	enum ice_status status;
	const u8 *pkt = NULL;
	bool prof_rule;
	u16 word_cnt;
	u32 act = 0;
	u8 q_rgn;

	/* Initialize profile to result index bitmap */
	if (!hw->switch_info->prof_res_bm_init) {
		hw->switch_info->prof_res_bm_init = 1;
		ice_init_prof_result_bm(hw);
	}

	prof_rule = ice_is_prof_rule(rinfo->tun_type);
	if (!prof_rule && !lkups_cnt)
		return ICE_ERR_PARAM;

	/* get # of words we need to match */
	word_cnt = 0;
	for (i = 0; i < lkups_cnt; i++) {
		u16 j, *ptr;

		ptr = (u16 *)&lkups[i].m_u;
		for (j = 0; j < sizeof(lkups->m_u) / sizeof(u16); j++)
			if (ptr[j] != 0)
				word_cnt++;
	}

	if (prof_rule) {
		if (word_cnt > ICE_MAX_CHAIN_WORDS)
			return ICE_ERR_PARAM;
	} else {
		if (!word_cnt || word_cnt > ICE_MAX_CHAIN_WORDS)
			return ICE_ERR_PARAM;
	}

	/* make sure that we can locate a dummy packet */
	ice_find_dummy_packet(lkups, lkups_cnt, rinfo->tun_type, &pkt, &pkt_len,
			      &pkt_offsets);
	if (!pkt) {
		status = ICE_ERR_PARAM;
		goto err_ice_add_adv_rule;
	}

	if (!(rinfo->sw_act.fltr_act == ICE_FWD_TO_VSI ||
	      rinfo->sw_act.fltr_act == ICE_FWD_TO_Q ||
	      rinfo->sw_act.fltr_act == ICE_FWD_TO_QGRP ||
	      rinfo->sw_act.fltr_act == ICE_DROP_PACKET))
		return ICE_ERR_CFG;

	vsi_handle = rinfo->sw_act.vsi_handle;
	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	if (rinfo->sw_act.fltr_act == ICE_FWD_TO_VSI)
		rinfo->sw_act.fwd_id.hw_vsi_id =
			ice_get_hw_vsi_num(hw, vsi_handle);
	if (rinfo->sw_act.flag & ICE_FLTR_TX)
		rinfo->sw_act.src = ice_get_hw_vsi_num(hw, vsi_handle);

	status = ice_add_adv_recipe(hw, lkups, lkups_cnt, rinfo, &rid);
	if (status)
		return status;
	m_entry = ice_find_adv_rule_entry(hw, lkups, lkups_cnt, rid, rinfo);
	if (m_entry) {
		/* we have to add VSI to VSI_LIST and increment vsi_count.
		 * Also Update VSI list so that we can change forwarding rule
		 * if the rule already exists, we will check if it exists with
		 * same vsi_id, if not then add it to the VSI list if it already
		 * exists if not then create a VSI list and add the existing VSI
		 * ID and the new VSI ID to the list
		 * We will add that VSI to the list
		 */
		status = ice_adv_add_update_vsi_list(hw, m_entry,
						     &m_entry->rule_info,
						     rinfo);
		if (added_entry) {
			added_entry->rid = rid;
			added_entry->rule_id = m_entry->rule_info.fltr_rule_id;
			added_entry->vsi_handle = rinfo->sw_act.vsi_handle;
		}
		return status;
	}
	rule_buf_sz = ICE_SW_RULE_RX_TX_NO_HDR_SIZE + pkt_len;
	s_rule = (struct ice_aqc_sw_rules_elem *)ice_malloc(hw, rule_buf_sz);
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;
	act |= ICE_SINGLE_ACT_LAN_ENABLE;
	switch (rinfo->sw_act.fltr_act) {
	case ICE_FWD_TO_VSI:
		act |= (rinfo->sw_act.fwd_id.hw_vsi_id <<
			ICE_SINGLE_ACT_VSI_ID_S) & ICE_SINGLE_ACT_VSI_ID_M;
		act |= ICE_SINGLE_ACT_VSI_FORWARDING | ICE_SINGLE_ACT_VALID_BIT;
		break;
	case ICE_FWD_TO_Q:
		act |= ICE_SINGLE_ACT_TO_Q;
		act |= (rinfo->sw_act.fwd_id.q_id << ICE_SINGLE_ACT_Q_INDEX_S) &
		       ICE_SINGLE_ACT_Q_INDEX_M;
		break;
	case ICE_FWD_TO_QGRP:
		q_rgn = rinfo->sw_act.qgrp_size > 0 ?
			(u8)ice_ilog2(rinfo->sw_act.qgrp_size) : 0;
		act |= ICE_SINGLE_ACT_TO_Q;
		act |= (rinfo->sw_act.fwd_id.q_id << ICE_SINGLE_ACT_Q_INDEX_S) &
		       ICE_SINGLE_ACT_Q_INDEX_M;
		act |= (q_rgn << ICE_SINGLE_ACT_Q_REGION_S) &
		       ICE_SINGLE_ACT_Q_REGION_M;
		break;
	case ICE_DROP_PACKET:
		act |= ICE_SINGLE_ACT_VSI_FORWARDING | ICE_SINGLE_ACT_DROP |
		       ICE_SINGLE_ACT_VALID_BIT;
		break;
	default:
		status = ICE_ERR_CFG;
		goto err_ice_add_adv_rule;
	}

	/* set the rule LOOKUP type based on caller specified 'RX'
	 * instead of hardcoding it to be either LOOKUP_TX/RX
	 *
	 * for 'RX' set the source to be the port number
	 * for 'TX' set the source to be the source HW VSI number (determined
	 * by caller)
	 */
	if (rinfo->rx) {
		s_rule->type = CPU_TO_LE16(ICE_AQC_SW_RULES_T_LKUP_RX);
		s_rule->pdata.lkup_tx_rx.src =
			CPU_TO_LE16(hw->port_info->lport);
	} else {
		s_rule->type = CPU_TO_LE16(ICE_AQC_SW_RULES_T_LKUP_TX);
		s_rule->pdata.lkup_tx_rx.src = CPU_TO_LE16(rinfo->sw_act.src);
	}

	s_rule->pdata.lkup_tx_rx.recipe_id = CPU_TO_LE16(rid);
	s_rule->pdata.lkup_tx_rx.act = CPU_TO_LE32(act);

	status = ice_fill_adv_dummy_packet(lkups, lkups_cnt, s_rule, pkt,
					   pkt_len, pkt_offsets);
	if (status)
		goto err_ice_add_adv_rule;

	if (rinfo->tun_type != ICE_NON_TUN &&
	    rinfo->tun_type != ICE_SW_TUN_AND_NON_TUN) {
		status = ice_fill_adv_packet_tun(hw, rinfo->tun_type,
						 s_rule->pdata.lkup_tx_rx.hdr,
						 pkt_offsets);
		if (status)
			goto err_ice_add_adv_rule;
	}

	status = ice_aq_sw_rules(hw, (struct ice_aqc_sw_rules *)s_rule,
				 rule_buf_sz, 1, ice_aqc_opc_add_sw_rules,
				 NULL);
	if (status)
		goto err_ice_add_adv_rule;
	adv_fltr = (struct ice_adv_fltr_mgmt_list_entry *)
		ice_malloc(hw, sizeof(struct ice_adv_fltr_mgmt_list_entry));
	if (!adv_fltr) {
		status = ICE_ERR_NO_MEMORY;
		goto err_ice_add_adv_rule;
	}

	adv_fltr->lkups = (struct ice_adv_lkup_elem *)
		ice_memdup(hw, lkups, lkups_cnt * sizeof(*lkups),
			   ICE_NONDMA_TO_NONDMA);
	if (!adv_fltr->lkups && !prof_rule) {
		status = ICE_ERR_NO_MEMORY;
		goto err_ice_add_adv_rule;
	}

	adv_fltr->lkups_cnt = lkups_cnt;
	adv_fltr->rule_info = *rinfo;
	adv_fltr->rule_info.fltr_rule_id =
		LE16_TO_CPU(s_rule->pdata.lkup_tx_rx.index);
	sw = hw->switch_info;
	sw->recp_list[rid].adv_rule = true;
	rule_head = &sw->recp_list[rid].filt_rules;

	if (rinfo->sw_act.fltr_act == ICE_FWD_TO_VSI)
		adv_fltr->vsi_count = 1;

	/* Add rule entry to book keeping list */
	LIST_ADD(&adv_fltr->list_entry, rule_head);
	if (added_entry) {
		added_entry->rid = rid;
		added_entry->rule_id = adv_fltr->rule_info.fltr_rule_id;
		added_entry->vsi_handle = rinfo->sw_act.vsi_handle;
	}
err_ice_add_adv_rule:
	if (status && adv_fltr) {
		ice_free(hw, adv_fltr->lkups);
		ice_free(hw, adv_fltr);
	}

	ice_free(hw, s_rule);

	return status;
}

/**
 * ice_adv_rem_update_vsi_list
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle of the VSI to remove
 * @fm_list: filter management entry for which the VSI list management needs to
 *	     be done
 */
static enum ice_status
ice_adv_rem_update_vsi_list(struct ice_hw *hw, u16 vsi_handle,
			    struct ice_adv_fltr_mgmt_list_entry *fm_list)
{
	struct ice_vsi_list_map_info *vsi_list_info;
	enum ice_sw_lkup_type lkup_type;
	enum ice_status status;
	u16 vsi_list_id;

	if (fm_list->rule_info.sw_act.fltr_act != ICE_FWD_TO_VSI_LIST ||
	    fm_list->vsi_count == 0)
		return ICE_ERR_PARAM;

	/* A rule with the VSI being removed does not exist */
	if (!ice_is_bit_set(fm_list->vsi_list_info->vsi_map, vsi_handle))
		return ICE_ERR_DOES_NOT_EXIST;

	lkup_type = ICE_SW_LKUP_LAST;
	vsi_list_id = fm_list->rule_info.sw_act.fwd_id.vsi_list_id;
	status = ice_update_vsi_list_rule(hw, &vsi_handle, 1, vsi_list_id, true,
					  ice_aqc_opc_update_sw_rules,
					  lkup_type);
	if (status)
		return status;

	fm_list->vsi_count--;
	ice_clear_bit(vsi_handle, fm_list->vsi_list_info->vsi_map);
	vsi_list_info = fm_list->vsi_list_info;
	if (fm_list->vsi_count == 1) {
		struct ice_fltr_info tmp_fltr;
		u16 rem_vsi_handle;

		rem_vsi_handle = ice_find_first_bit(vsi_list_info->vsi_map,
						    ICE_MAX_VSI);
		if (!ice_is_vsi_valid(hw, rem_vsi_handle))
			return ICE_ERR_OUT_OF_RANGE;

		/* Make sure VSI list is empty before removing it below */
		status = ice_update_vsi_list_rule(hw, &rem_vsi_handle, 1,
						  vsi_list_id, true,
						  ice_aqc_opc_update_sw_rules,
						  lkup_type);
		if (status)
			return status;

		ice_memset(&tmp_fltr, 0, sizeof(tmp_fltr), ICE_NONDMA_MEM);
		tmp_fltr.flag = fm_list->rule_info.sw_act.flag;
		tmp_fltr.fltr_rule_id = fm_list->rule_info.fltr_rule_id;
		fm_list->rule_info.sw_act.fltr_act = ICE_FWD_TO_VSI;
		tmp_fltr.fltr_act = ICE_FWD_TO_VSI;
		tmp_fltr.fwd_id.hw_vsi_id =
			ice_get_hw_vsi_num(hw, rem_vsi_handle);
		fm_list->rule_info.sw_act.fwd_id.hw_vsi_id =
			ice_get_hw_vsi_num(hw, rem_vsi_handle);
		fm_list->rule_info.sw_act.vsi_handle = rem_vsi_handle;

		/* Update the previous switch rule of "MAC forward to VSI" to
		 * "MAC fwd to VSI list"
		 */
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr);
		if (status) {
			ice_debug(hw, ICE_DBG_SW,
				  "Failed to update pkt fwd rule to FWD_TO_VSI on HW VSI %d, error %d\n",
				  tmp_fltr.fwd_id.hw_vsi_id, status);
			return status;
		}
		fm_list->vsi_list_info->ref_cnt--;

		/* Remove the VSI list since it is no longer used */
		status = ice_remove_vsi_list_rule(hw, vsi_list_id, lkup_type);
		if (status) {
			ice_debug(hw, ICE_DBG_SW,
				  "Failed to remove VSI list %d, error %d\n",
				  vsi_list_id, status);
			return status;
		}

		LIST_DEL(&vsi_list_info->list_entry);
		ice_free(hw, vsi_list_info);
		fm_list->vsi_list_info = NULL;
	}

	return status;
}

/**
 * ice_rem_adv_rule - removes existing advanced switch rule
 * @hw: pointer to the hardware structure
 * @lkups: information on the words that needs to be looked up. All words
 *         together makes one recipe
 * @lkups_cnt: num of entries in the lkups array
 * @rinfo: Its the pointer to the rule information for the rule
 *
 * This function can be used to remove 1 rule at a time. The lkups is
 * used to describe all the words that forms the "lookup" portion of the
 * rule. These words can span multiple protocols. Callers to this function
 * need to pass in a list of protocol headers with lookup information along
 * and mask that determines which words are valid from the given protocol
 * header. rinfo describes other information related to this rule such as
 * forwarding IDs, priority of this rule, etc.
 */
enum ice_status
ice_rem_adv_rule(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups,
		 u16 lkups_cnt, struct ice_adv_rule_info *rinfo)
{
	struct ice_adv_fltr_mgmt_list_entry *list_elem;
	struct ice_prot_lkup_ext lkup_exts;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;
	bool remove_rule = false;
	u16 i, rid, vsi_handle;

	ice_memset(&lkup_exts, 0, sizeof(lkup_exts), ICE_NONDMA_MEM);
	for (i = 0; i < lkups_cnt; i++) {
		u16 count;

		if (lkups[i].type >= ICE_PROTOCOL_LAST)
			return ICE_ERR_CFG;

		count = ice_fill_valid_words(&lkups[i], &lkup_exts);
		if (!count)
			return ICE_ERR_CFG;
	}

	/* Create any special protocol/offset pairs, such as looking at tunnel
	 * bits by extracting metadata
	 */
	status = ice_add_special_words(rinfo, &lkup_exts);
	if (status)
		return status;

	rid = ice_find_recp(hw, &lkup_exts, rinfo->tun_type);
	/* If did not find a recipe that match the existing criteria */
	if (rid == ICE_MAX_NUM_RECIPES)
		return ICE_ERR_PARAM;

	rule_lock = &hw->switch_info->recp_list[rid].filt_rule_lock;
	list_elem = ice_find_adv_rule_entry(hw, lkups, lkups_cnt, rid, rinfo);
	/* the rule is already removed */
	if (!list_elem)
		return ICE_SUCCESS;
	ice_acquire_lock(rule_lock);
	if (list_elem->rule_info.sw_act.fltr_act != ICE_FWD_TO_VSI_LIST) {
		remove_rule = true;
	} else if (list_elem->vsi_count > 1) {
		remove_rule = false;
		vsi_handle = rinfo->sw_act.vsi_handle;
		status = ice_adv_rem_update_vsi_list(hw, vsi_handle, list_elem);
	} else {
		vsi_handle = rinfo->sw_act.vsi_handle;
		status = ice_adv_rem_update_vsi_list(hw, vsi_handle, list_elem);
		if (status) {
			ice_release_lock(rule_lock);
			return status;
		}
		if (list_elem->vsi_count == 0)
			remove_rule = true;
	}
	ice_release_lock(rule_lock);
	if (remove_rule) {
		struct ice_aqc_sw_rules_elem *s_rule;
		u16 rule_buf_sz;

		rule_buf_sz = ICE_SW_RULE_RX_TX_NO_HDR_SIZE;
		s_rule =
			(struct ice_aqc_sw_rules_elem *)ice_malloc(hw,
								   rule_buf_sz);
		if (!s_rule)
			return ICE_ERR_NO_MEMORY;
		s_rule->pdata.lkup_tx_rx.act = 0;
		s_rule->pdata.lkup_tx_rx.index =
			CPU_TO_LE16(list_elem->rule_info.fltr_rule_id);
		s_rule->pdata.lkup_tx_rx.hdr_len = 0;
		status = ice_aq_sw_rules(hw, (struct ice_aqc_sw_rules *)s_rule,
					 rule_buf_sz, 1,
					 ice_aqc_opc_remove_sw_rules, NULL);
		if (status == ICE_SUCCESS || status == ICE_ERR_DOES_NOT_EXIST) {
			struct ice_switch_info *sw = hw->switch_info;

			ice_acquire_lock(rule_lock);
			LIST_DEL(&list_elem->list_entry);
			ice_free(hw, list_elem->lkups);
			ice_free(hw, list_elem);
			ice_release_lock(rule_lock);
			if (LIST_EMPTY(&sw->recp_list[rid].filt_rules))
				sw->recp_list[rid].adv_rule = false;
		}
		ice_free(hw, s_rule);
	}
	return status;
}

/**
 * ice_rem_adv_rule_by_id - removes existing advanced switch rule by ID
 * @hw: pointer to the hardware structure
 * @remove_entry: data struct which holds rule_id, VSI handle and recipe ID
 *
 * This function is used to remove 1 rule at a time. The removal is based on
 * the remove_entry parameter. This function will remove rule for a given
 * vsi_handle with a given rule_id which is passed as parameter in remove_entry
 */
enum ice_status
ice_rem_adv_rule_by_id(struct ice_hw *hw,
		       struct ice_rule_query_data *remove_entry)
{
	struct ice_adv_fltr_mgmt_list_entry *list_itr;
	struct LIST_HEAD_TYPE *list_head;
	struct ice_adv_rule_info rinfo;
	struct ice_switch_info *sw;

	sw = hw->switch_info;
	if (!sw->recp_list[remove_entry->rid].recp_created)
		return ICE_ERR_PARAM;
	list_head = &sw->recp_list[remove_entry->rid].filt_rules;
	LIST_FOR_EACH_ENTRY(list_itr, list_head, ice_adv_fltr_mgmt_list_entry,
			    list_entry) {
		if (list_itr->rule_info.fltr_rule_id ==
		    remove_entry->rule_id) {
			rinfo = list_itr->rule_info;
			rinfo.sw_act.vsi_handle = remove_entry->vsi_handle;
			return ice_rem_adv_rule(hw, list_itr->lkups,
						list_itr->lkups_cnt, &rinfo);
		}
	}
	/* either list is empty or unable to find rule */
	return ICE_ERR_DOES_NOT_EXIST;
}

/**
 * ice_rem_adv_for_vsi - removes existing advanced switch rules for a
 *                       given VSI handle
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle for which we are supposed to remove all the rules.
 *
 * This function is used to remove all the rules for a given VSI and as soon
 * as removing a rule fails, it will return immediately with the error code,
 * else it will return ICE_SUCCESS
 */
enum ice_status ice_rem_adv_rule_for_vsi(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_adv_fltr_mgmt_list_entry *list_itr;
	struct ice_vsi_list_map_info *map_info;
	struct LIST_HEAD_TYPE *list_head;
	struct ice_adv_rule_info rinfo;
	struct ice_switch_info *sw;
	enum ice_status status;
	u16 vsi_list_id = 0;
	u8 rid;

	sw = hw->switch_info;
	for (rid = 0; rid < ICE_MAX_NUM_RECIPES; rid++) {
		if (!sw->recp_list[rid].recp_created)
			continue;
		if (!sw->recp_list[rid].adv_rule)
			continue;
		list_head = &sw->recp_list[rid].filt_rules;
		map_info = NULL;
		LIST_FOR_EACH_ENTRY(list_itr, list_head,
				    ice_adv_fltr_mgmt_list_entry, list_entry) {
			map_info = ice_find_vsi_list_entry(&sw->recp_list[rid],
							   vsi_handle,
							   &vsi_list_id);
			if (!map_info)
				continue;
			rinfo = list_itr->rule_info;
			rinfo.sw_act.vsi_handle = vsi_handle;
			status = ice_rem_adv_rule(hw, list_itr->lkups,
						  list_itr->lkups_cnt, &rinfo);
			if (status)
				return status;
			map_info = NULL;
		}
	}
	return ICE_SUCCESS;
}

/**
 * ice_replay_fltr - Replay all the filters stored by a specific list head
 * @hw: pointer to the hardware structure
 * @list_head: list for which filters needs to be replayed
 * @recp_id: Recipe ID for which rules need to be replayed
 */
static enum ice_status
ice_replay_fltr(struct ice_hw *hw, u8 recp_id, struct LIST_HEAD_TYPE *list_head)
{
	struct ice_fltr_mgmt_list_entry *itr;
	enum ice_status status = ICE_SUCCESS;
	struct ice_sw_recipe *recp_list;
	u8 lport = hw->port_info->lport;
	struct LIST_HEAD_TYPE l_head;

	if (LIST_EMPTY(list_head))
		return status;

	recp_list = &hw->switch_info->recp_list[recp_id];
	/* Move entries from the given list_head to a temporary l_head so that
	 * they can be replayed. Otherwise when trying to re-add the same
	 * filter, the function will return already exists
	 */
	LIST_REPLACE_INIT(list_head, &l_head);

	/* Mark the given list_head empty by reinitializing it so filters
	 * could be added again by *handler
	 */
	LIST_FOR_EACH_ENTRY(itr, &l_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		struct ice_fltr_list_entry f_entry;

		f_entry.fltr_info = itr->fltr_info;
		if (itr->vsi_count < 2 && recp_id != ICE_SW_LKUP_VLAN) {
			status = ice_add_rule_internal(hw, recp_list, lport,
						       &f_entry);
			if (status != ICE_SUCCESS)
				goto end;
			continue;
		}

		/* Add a filter per VSI separately */
		while (1) {
			u16 vsi_handle;

			vsi_handle =
				ice_find_first_bit(itr->vsi_list_info->vsi_map,
						   ICE_MAX_VSI);
			if (!ice_is_vsi_valid(hw, vsi_handle))
				break;

			ice_clear_bit(vsi_handle, itr->vsi_list_info->vsi_map);
			f_entry.fltr_info.vsi_handle = vsi_handle;
			f_entry.fltr_info.fwd_id.hw_vsi_id =
				ice_get_hw_vsi_num(hw, vsi_handle);
			f_entry.fltr_info.fltr_act = ICE_FWD_TO_VSI;
			if (recp_id == ICE_SW_LKUP_VLAN)
				status = ice_add_vlan_internal(hw, recp_list,
							       &f_entry);
			else
				status = ice_add_rule_internal(hw, recp_list,
							       lport,
							       &f_entry);
			if (status != ICE_SUCCESS)
				goto end;
		}
	}
end:
	/* Clear the filter management list */
	ice_rem_sw_rule_info(hw, &l_head);
	return status;
}

/**
 * ice_replay_all_fltr - replay all filters stored in bookkeeping lists
 * @hw: pointer to the hardware structure
 *
 * NOTE: This function does not clean up partially added filters on error.
 * It is up to caller of the function to issue a reset or fail early.
 */
enum ice_status ice_replay_all_fltr(struct ice_hw *hw)
{
	struct ice_switch_info *sw = hw->switch_info;
	enum ice_status status = ICE_SUCCESS;
	u8 i;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct LIST_HEAD_TYPE *head = &sw->recp_list[i].filt_rules;

		status = ice_replay_fltr(hw, i, head);
		if (status != ICE_SUCCESS)
			return status;
	}
	return status;
}

/**
 * ice_replay_vsi_fltr - Replay filters for requested VSI
 * @hw: pointer to the hardware structure
 * @pi: pointer to port information structure
 * @sw: pointer to switch info struct for which function replays filters
 * @vsi_handle: driver VSI handle
 * @recp_id: Recipe ID for which rules need to be replayed
 * @list_head: list for which filters need to be replayed
 *
 * Replays the filter of recipe recp_id for a VSI represented via vsi_handle.
 * It is required to pass valid VSI handle.
 */
static enum ice_status
ice_replay_vsi_fltr(struct ice_hw *hw, struct ice_port_info *pi,
		    struct ice_switch_info *sw, u16 vsi_handle, u8 recp_id,
		    struct LIST_HEAD_TYPE *list_head)
{
	struct ice_fltr_mgmt_list_entry *itr;
	enum ice_status status = ICE_SUCCESS;
	struct ice_sw_recipe *recp_list;
	u16 hw_vsi_id;

	if (LIST_EMPTY(list_head))
		return status;
	recp_list = &sw->recp_list[recp_id];
	hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	LIST_FOR_EACH_ENTRY(itr, list_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		struct ice_fltr_list_entry f_entry;

		f_entry.fltr_info = itr->fltr_info;
		if (itr->vsi_count < 2 && recp_id != ICE_SW_LKUP_VLAN &&
		    itr->fltr_info.vsi_handle == vsi_handle) {
			/* update the src in case it is VSI num */
			if (f_entry.fltr_info.src_id == ICE_SRC_ID_VSI)
				f_entry.fltr_info.src = hw_vsi_id;
			status = ice_add_rule_internal(hw, recp_list,
						       pi->lport,
						       &f_entry);
			if (status != ICE_SUCCESS)
				goto end;
			continue;
		}
		if (!itr->vsi_list_info ||
		    !ice_is_bit_set(itr->vsi_list_info->vsi_map, vsi_handle))
			continue;
		/* Clearing it so that the logic can add it back */
		ice_clear_bit(vsi_handle, itr->vsi_list_info->vsi_map);
		f_entry.fltr_info.vsi_handle = vsi_handle;
		f_entry.fltr_info.fltr_act = ICE_FWD_TO_VSI;
		/* update the src in case it is VSI num */
		if (f_entry.fltr_info.src_id == ICE_SRC_ID_VSI)
			f_entry.fltr_info.src = hw_vsi_id;
		if (recp_id == ICE_SW_LKUP_VLAN)
			status = ice_add_vlan_internal(hw, recp_list, &f_entry);
		else
			status = ice_add_rule_internal(hw, recp_list,
						       pi->lport,
						       &f_entry);
		if (status != ICE_SUCCESS)
			goto end;
	}
end:
	return status;
}

/**
 * ice_replay_vsi_adv_rule - Replay advanced rule for requested VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: driver VSI handle
 * @list_head: list for which filters need to be replayed
 *
 * Replay the advanced rule for the given VSI.
 */
static enum ice_status
ice_replay_vsi_adv_rule(struct ice_hw *hw, u16 vsi_handle,
			struct LIST_HEAD_TYPE *list_head)
{
	struct ice_rule_query_data added_entry = { 0 };
	struct ice_adv_fltr_mgmt_list_entry *adv_fltr;
	enum ice_status status = ICE_SUCCESS;

	if (LIST_EMPTY(list_head))
		return status;
	LIST_FOR_EACH_ENTRY(adv_fltr, list_head, ice_adv_fltr_mgmt_list_entry,
			    list_entry) {
		struct ice_adv_rule_info *rinfo = &adv_fltr->rule_info;
		u16 lk_cnt = adv_fltr->lkups_cnt;

		if (vsi_handle != rinfo->sw_act.vsi_handle)
			continue;
		status = ice_add_adv_rule(hw, adv_fltr->lkups, lk_cnt, rinfo,
					  &added_entry);
		if (status)
			break;
	}
	return status;
}

/**
 * ice_replay_vsi_all_fltr - replay all filters stored in bookkeeping lists
 * @hw: pointer to the hardware structure
 * @pi: pointer to port information structure
 * @vsi_handle: driver VSI handle
 *
 * Replays filters for requested VSI via vsi_handle.
 */
enum ice_status
ice_replay_vsi_all_fltr(struct ice_hw *hw, struct ice_port_info *pi,
			u16 vsi_handle)
{
	struct ice_switch_info *sw = hw->switch_info;
	enum ice_status status;
	u8 i;

	/* Update the recipes that were created */
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct LIST_HEAD_TYPE *head;

		head = &sw->recp_list[i].filt_replay_rules;
		if (!sw->recp_list[i].adv_rule)
			status = ice_replay_vsi_fltr(hw, pi, sw, vsi_handle, i,
						     head);
		else
			status = ice_replay_vsi_adv_rule(hw, vsi_handle, head);
		if (status != ICE_SUCCESS)
			return status;
	}

	return ICE_SUCCESS;
}

/**
 * ice_rm_all_sw_replay_rule - helper function to delete filter replay rules
 * @hw: pointer to the HW struct
 * @sw: pointer to switch info struct for which function removes filters
 *
 * Deletes the filter replay rules for given switch
 */
void ice_rm_sw_replay_rule_info(struct ice_hw *hw, struct ice_switch_info *sw)
{
	u8 i;

	if (!sw)
		return;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		if (!LIST_EMPTY(&sw->recp_list[i].filt_replay_rules)) {
			struct LIST_HEAD_TYPE *l_head;

			l_head = &sw->recp_list[i].filt_replay_rules;
			if (!sw->recp_list[i].adv_rule)
				ice_rem_sw_rule_info(hw, l_head);
			else
				ice_rem_adv_rule_info(hw, l_head);
		}
	}
}

/**
 * ice_rm_all_sw_replay_rule_info - deletes filter replay rules
 * @hw: pointer to the HW struct
 *
 * Deletes the filter replay rules.
 */
void ice_rm_all_sw_replay_rule_info(struct ice_hw *hw)
{
	ice_rm_sw_replay_rule_info(hw, hw->switch_info);
}
