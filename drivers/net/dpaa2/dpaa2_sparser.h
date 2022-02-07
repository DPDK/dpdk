/* * SPDX-License-Identifier: BSD-3-Clause
 *   Copyright 2018-2019 NXP
 */

/**
 * @file	dpaa2_sparser.h
 *
 * @brief	Soft parser related macros & functions support for DPAA2 device
 *	framework based applications.
 *
 */

#ifndef _DPAA2_SPARSER_H
#define _DPAA2_SPARSER_H

#define WRIOP_SS_INITIALIZER(priv)				\
do {								\
	/* Base offset of parse profile memory in WRIOP */	\
	(priv)->ss_offset = 0x20;				\
	(priv)->ss_iova	= (size_t)NULL;			\
	(priv)->ss_param_iova = (size_t)NULL;			\
} while (0)

/**************************************************************************/
/*
 * @enum   parser_starting_hxs_code
 * @Description PARSER Starting HXS code
 */
/***************************************************************************/
enum parser_starting_hxs_code {
	/** Ethernet Starting HXS coding */
	PARSER_ETH_STARTING_HXS = 0x0000,
	/** LLC+SNAP Starting HXS coding */
	PARSER_LLC_SNAP_STARTING_HXS = 0x0001,
	/** VLAN Starting HXS coding */
	PARSER_VLAN_STARTING_HXS = 0x0002,
	/** PPPoE+PPP Starting HXS coding */
	PARSER_PPPOE_PPP_STARTING_HXS = 0x0003,
	/** MPLS Starting HXS coding */
	PARSER_MPLS_STARTING_HXS = 0x0004,
	/** ARP Starting HXS coding */
	PARSER_ARP_STARTING_HXS = 0x0005,
	/** IP Starting HXS coding */
	PARSER_IP_STARTING_HXS  = 0x0006,
	/** IPv4 Starting HXS coding */
	PARSER_IPV4_STARTING_HXS = 0x0007,
	/** IPv6 Starting HXS coding */
	PARSER_IPV6_STARTING_HXS = 0x0008,
	/** GRE Starting HXS coding */
	PARSER_GRE_STARTING_HXS = 0x0009,
	/** MinEncap Starting HXS coding */
	PARSER_MINENCAP_STARTING_HXS = 0x000A,
	/** Other L3 Shell Starting HXS coding */
	PARSER_OTHER_L3_SHELL_STARTING_HXS = 0x000B,
	/** TCP Starting HXS coding */
	PARSER_TCP_STARTING_HXS = 0x000C,
	/** UDP Starting HXS coding */
	PARSER_UDP_STARTING_HXS = 0x000D,
	/** IPSec Starting HXS coding */
	PARSER_IPSEC_STARTING_HXS = 0x000E,
	/** SCTP Starting HXS coding */
	PARSER_SCTP_STARTING_HXS = 0x000F,
	/** DCCP Starting HXS coding */
	PARSER_DCCP_STARTING_HXS = 0x0010,
	/** Other L4 Shell Starting HXS coding */
	PARSER_OTHER_L4_SHELL_STARTING_HXS = 0x0011,
	/** GTP Starting HXS coding */
	PARSER_GTP_STARTING_HXS = 0x0012,
	/** ESP Starting HXS coding */
	PARSER_ESP_STARTING_HXS = 0x0013,
	/** VXLAN Starting HXS coding */
	PARSER_VXLAN_STARTING_HXS = 0x0014,
	/** L5 (and above) Shell Starting HXS coding */
	PARSER_L5_SHELL_STARTING_HXS = 0x001E,
	/** Final Shell Starting HXS coding */
	PARSER_FINAL_SHELL_STARTING_HXS = 0x001F
};

/**************************************************************************/
/*
 * @Description    struct dpni_drv_sparser_param - Structure representing the
 *			information needed to activate(enable) a Soft Parser.
 */
/***************************************************************************/

struct dpni_drv_sparser_param {
	/* The "custom_header_first" must be set if the custom header to parse
	 * is the first header in the packet, otherwise "custom_header_first"
	 * must be cleared.
	 */
	uint8_t             custom_header_first;
	/* Hard HXS on which a soft parser is activated. This must be
	 * configured.
	 * if the header to parse is not the first header in the packet.
	 */
	enum parser_starting_hxs_code   link_to_hard_hxs;
	/* Soft Sequence Start PC */
	uint16_t            start_pc;
	/* Soft Sequence byte-code */
	uint8_t             *byte_code;
	/* Soft Sequence size */
	uint16_t            size;
	/* Pointer to the Parameters Array of the SP */
	uint8_t             *param_array;
	/* Parameters offset */
	uint8_t             param_offset;
	/* Parameters size */
	uint8_t             param_size;
};

struct sp_parse_result {
	/* Next header */
	uint16_t    nxt_hdr;
	/* Frame Attribute Flags Extension */
	uint16_t    frame_attribute_flags_extension;
	/* Frame Attribute Flags (part 1) */
	uint32_t    frame_attribute_flags_1;
	/* Frame Attribute Flags (part 2) */
	uint32_t    frame_attribute_flags_2;
	/* Frame Attribute Flags (part 3) */
	uint32_t    frame_attribute_flags_3;
	/* Shim Offset 1 */
	uint8_t     shim_offset_1;
	/* Shim Offset 2 */
	uint8_t     shim_offset_2;
	/* Outer IP protocol field offset */
	uint8_t     ip_1_pid_offset;
	/* Ethernet offset */
	uint8_t     eth_offset;
	/* LLC+SNAP offset */
	uint8_t     llc_snap_offset;
	/* First VLAN's TCI field offset*/
	uint8_t     vlan_tci1_offset;
	/* Last VLAN's TCI field offset*/
	uint8_t     vlan_tcin_offset;
	/* Last Ethertype offset*/
	uint8_t     last_etype_offset;
	/* PPPoE offset */
	uint8_t     pppoe_offset;
	/* First MPLS offset */
	uint8_t     mpls_offset_1;
	/* Last MPLS offset */
	uint8_t     mpls_offset_n;
	/* Layer 3 (Outer IP, ARP, FCoE or FIP) offset */
	uint8_t     l3_offset;
	/* Inner IP or MinEncap offset*/
	uint8_t     ipn_or_minencap_offset;
	/* GRE offset */
	uint8_t     gre_offset;
	/* Layer 4 offset*/
	uint8_t     l4_offset;
	/* Layer 5 offset */
	uint8_t     l5_offset;
	/* Routing header offset of 1st IPv6 header */
	uint8_t     routing_hdr_offset1;
	/* Routing header offset of 2nd IPv6 header */
	uint8_t     routing_hdr_offset2;
	/* Next header offset */
	uint8_t     nxt_hdr_offset;
	/* IPv6 fragmentable part offset */
	uint8_t     ipv6_frag_offset;
	/* Frame's untouched running sum, input to parser */
	uint16_t    gross_running_sum;
	/* Running Sum */
	uint16_t    running_sum;
	/* Parse Error code */
	uint8_t     parse_error_code;
	/* Offset to the next header field before IPv6 fragment extension */
	uint8_t     nxt_hdr_before_ipv6_frag_ext;
	/* Inner IP Protocol field offset */
	uint8_t     ip_n_pid_offset;
	/* Reserved for Soft parsing context*/
	uint8_t     soft_parsing_context[21];
};

struct frame_attr {
	const char *fld_name;
	uint8_t     faf_offset;
	uint32_t    fld_mask;
};

struct frame_attr_ext {
	const char *fld_name;
	uint8_t     faf_ext_offset;
	uint16_t    fld_mask;
};


struct parse_err {
	uint16_t    code;
	const char *err_name;
};

/* Macro definitions */
#define IS_ONE_BIT_FIELD(_mask)                 \
(!((_mask) & ((_mask) - 1)) || (_mask == 1))

int dpaa2_eth_load_wriop_soft_parser(struct dpaa2_dev_priv *priv,
		enum dpni_soft_sequence_dest dest);
int dpaa2_eth_enable_wriop_soft_parser(struct dpaa2_dev_priv *priv,
		enum dpni_soft_sequence_dest dest);
#endif /* _DPAA2_SPARSER_H_ */
