/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _ICE_FLEX_TYPE_H_
#define _ICE_FLEX_TYPE_H_

#define ICE_FV_OFFSET_INVAL	0x1FF

#pragma pack(1)
/* Extraction Sequence (Field Vector) Table */
struct ice_fv_word {
	u8 prot_id;
	u16 off;		/* Offset within the protocol header */
	u8 resvrd;
};

#pragma pack()

#define ICE_MAX_NUM_PROFILES 256

#define ICE_MAX_FV_WORDS 48
struct ice_fv {
	struct ice_fv_word ew[ICE_MAX_FV_WORDS];
};

/* Packet Type (PTYPE) values */
#define ICE_PTYPE_MAC_PAY			1
#define ICE_MAC_PTP				2
#define ICE_MAC_LLDP				6
#define ICE_MAC_ARP				11
#define ICE_PTYPE_IPV4FRAG_PAY			22
#define ICE_PTYPE_IPV4_PAY			23
#define ICE_PTYPE_IPV4_UDP_PAY			24
#define ICE_PTYPE_IPV4_TCP_PAY			26
#define ICE_PTYPE_IPV4_SCTP_PAY			27
#define ICE_PTYPE_IPV4_ICMP_PAY			28
#define ICE_MAC_IPV4_IPV4_FRAG			29
#define ICE_MAC_IPV4_IPV4_PAY			30
#define ICE_MAC_IPV4_IPV4_UDP_PAY		31
#define ICE_MAC_IPV4_IPV4_TCP			33
#define ICE_MAC_IPV4_IPV4_SCTP			34
#define ICE_MAC_IPV4_IPV4_ICMP			35
#define ICE_MAC_IPV4_IPV6_FRAG			36
#define ICE_MAC_IPV4_IPV6_PAY			37
#define ICE_MAC_IPV4_IPV6_UDP_PAY		38
#define ICE_MAC_IPV4_IPV6_TCP			40
#define ICE_MAC_IPV4_IPV6_SCTP			41
#define ICE_MAC_IPV4_IPV6_ICMPV6		42
#define ICE_MAC_IPV4_TUN_PAY			43
#define ICE_MAC_IPV4_TUN_IPV4_FRAG		44
#define ICE_MAC_IPV4_TUN_IPV4_PAY		45
#define ICE_MAC_IPV4_TUN_IPV4_UDP_PAY		46
#define ICE_MAC_IPV4_TUN_IPV4_TCP		48
#define ICE_MAC_IPV4_TUN_IPV4_SCTP		49
#define ICE_MAC_IPV4_TUN_IPV4_ICMP		50
#define ICE_MAC_IPV4_TUN_IPV6_FRAG		51
#define ICE_MAC_IPV4_TUN_IPV6_PAY		52
#define ICE_MAC_IPV4_TUN_IPV6_UDP_PAY		53
#define ICE_MAC_IPV4_TUN_IPV6_TCP		55
#define ICE_MAC_IPV4_TUN_IPV6_SCTP		56
#define ICE_MAC_IPV4_TUN_IPV6_ICMPV6		57
#define ICE_MAC_IPV4_TUN_ICE_MAC_PAY		58
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV4_FRAG	59
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV4_PAY	60
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV4_UDP_PAY	61
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV4_TCP	63
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV4_SCTP	64
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV4_ICMP	65
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV6_FRAG	66
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV6_PAY	67
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV6_UDP_PAY	68
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV6_TCP	70
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV6_SCTP	71
#define ICE_MAC_IPV4_TUN_ICE_MAC_IPV6_ICMPV6	72
#define ICE_PTYPE_IPV6FRAG_PAY			88
#define ICE_PTYPE_IPV6_PAY			89
#define ICE_PTYPE_IPV6_UDP_PAY			90
#define ICE_PTYPE_IPV6_TCP_PAY			92
#define ICE_PTYPE_IPV6_SCTP_PAY			93
#define ICE_PTYPE_IPV6_ICMP_PAY			94
#define ICE_MAC_IPV6_IPV4_FRAG			95
#define ICE_MAC_IPV6_IPV4_PAY			96
#define ICE_MAC_IPV6_IPV4_UDP_PAY		97
#define ICE_MAC_IPV6_IPV4_TCP			99
#define ICE_MAC_IPV6_IPV4_SCTP			100
#define ICE_MAC_IPV6_IPV4_ICMP			101
#define ICE_MAC_IPV6_IPV6_FRAG			102
#define ICE_MAC_IPV6_IPV6_PAY			103
#define ICE_MAC_IPV6_IPV6_UDP_PAY		104
#define ICE_MAC_IPV6_IPV6_TCP			106
#define ICE_MAC_IPV6_IPV6_SCTP			107
#define ICE_MAC_IPV6_IPV6_ICMPV6		108
#define ICE_MAC_IPV6_TUN_PAY			109
#define ICE_MAC_IPV6_TUN_IPV4_FRAG		110
#define ICE_MAC_IPV6_TUN_IPV4_PAY		111
#define ICE_MAC_IPV6_TUN_IPV4_UDP_PAY		112
#define ICE_MAC_IPV6_TUN_IPV4_TCP		114
#define ICE_MAC_IPV6_TUN_IPV4_SCTP		115
#define ICE_MAC_IPV6_TUN_IPV4_ICMP		116
#define ICE_MAC_IPV6_TUN_IPV6_FRAG		117
#define ICE_MAC_IPV6_TUN_IPV6_PAY		118
#define ICE_MAC_IPV6_TUN_IPV6_UDP_PAY		119
#define ICE_MAC_IPV6_TUN_IPV6_TCP		121
#define ICE_MAC_IPV6_TUN_IPV6_SCTP		122
#define ICE_MAC_IPV6_TUN_IPV6_ICMPV6		123
#define ICE_MAC_IPV6_TUN_MAC_PAY		124
#define ICE_MAC_IPV6_TUN_MAC_IPV4_FRAG		125
#define ICE_MAC_IPV6_TUN_MAC_IPV4_PAY		126
#define ICE_MAC_IPV6_TUN_MAC_IPV4_UDP_PAY	127
#define ICE_MAC_IPV6_TUN_MAC_IPV4_TCP		129
#define ICE_MAC_IPV6_TUN_MAC_IPV4_SCTP		130
#define ICE_MAC_IPV6_TUN_MAC_IPV4_ICMP		131
#define ICE_MAC_IPV6_TUN_MAC_IPV6_FRAG		132
#define ICE_MAC_IPV6_TUN_MAC_IPV6_PAY		133
#define ICE_MAC_IPV6_TUN_MAC_IPV6_UDP_PAY	134
#define ICE_MAC_IPV6_TUN_MAC_IPV6_TCP		136
#define ICE_MAC_IPV6_TUN_MAC_IPV6_SCTP		137
#define ICE_MAC_IPV6_TUN_MAC_IPV6_ICMPV6	138
#define ICE_MAC_IPV4_ESP			160
#define ICE_MAC_IPV6_ESP			161
#define ICE_MAC_IPV4_AH				162
#define ICE_MAC_IPV6_AH				163
#define ICE_MAC_IPV4_NAT_T_ESP			164
#define ICE_MAC_IPV6_NAT_T_ESP			165
#define ICE_MAC_IPV4_NAT_T_IKE			166
#define ICE_MAC_IPV6_NAT_T_IKE			167
#define ICE_MAC_IPV4_NAT_T_KEEP			168
#define ICE_MAC_IPV6_NAT_T_KEEP			169
#define ICE_MAC_CONTROL				278
#define ICE_MAC_PPPOD_PAY			300
#define ICE_MAC_PPPOE_PAY			301
#define ICE_MAC_PPPOE_IPV4_FRAG			302
#define ICE_MAC_PPPOE_IPV4_PAY			303
#define ICE_MAC_PPPOE_IPV4_UDP_PAY		304
#define ICE_MAC_PPPOE_IPV4_TCP			305
#define ICE_MAC_PPPOE_IPV4_SCTP			306
#define ICE_MAC_PPPOE_IPV4_ICMP			307
#define ICE_MAC_PPPOE_IPV6_FRAG			308
#define ICE_MAC_PPPOE_IPV6_PAY			309
#define ICE_MAC_PPPOE_IPV6_UDP_PAY		310
#define ICE_MAC_PPPOE_IPV6_TCP			311
#define ICE_MAC_PPPOE_IPV6_SCTP			312
#define ICE_MAC_PPPOE_IPV6_ICMPV6		313
#define ICE_MAC_IPV4_GTPC_TEID			325
#define ICE_MAC_IPV6_GTPC_TEID			326
#define ICE_MAC_IPV4_GTPC			327
#define ICE_MAC_IPV6_GTPC			328
#define ICE_MAC_IPV4_GTPU			329
#define ICE_MAC_IPV6_GTPU			330
#define ICE_MAC_IPV4_GTPU_IPV4_FRAG		331
#define ICE_MAC_IPV4_GTPU_IPV4_PAY		332
#define ICE_MAC_IPV4_GTPU_IPV4_UDP_PAY		333
#define ICE_MAC_IPV4_GTPU_IPV4_TCP		334
#define ICE_MAC_IPV4_GTPU_IPV4_ICMP		335
#define ICE_MAC_IPV6_GTPU_IPV4_FRAG		336
#define ICE_MAC_IPV6_GTPU_IPV4_PAY		337
#define ICE_MAC_IPV6_GTPU_IPV4_UDP_PAY		338
#define ICE_MAC_IPV6_GTPU_IPV4_TCP		339
#define ICE_MAC_IPV6_GTPU_IPV4_ICMP		340
#define ICE_MAC_IPV4_GTPU_IPV6_FRAG		341
#define ICE_MAC_IPV4_GTPU_IPV6_PAY		342
#define ICE_MAC_IPV4_GTPU_IPV6_UDP_PAY		343
#define ICE_MAC_IPV4_GTPU_IPV6_TCP		344
#define ICE_MAC_IPV4_GTPU_IPV6_ICMPV6		345
#define ICE_MAC_IPV6_GTPU_IPV6_FRAG		346
#define ICE_MAC_IPV6_GTPU_IPV6_PAY		347
#define ICE_MAC_IPV6_GTPU_IPV6_UDP_PAY		348
#define ICE_MAC_IPV6_GTPU_IPV6_TCP		349
#define ICE_MAC_IPV6_GTPU_IPV6_ICMPV6		350
#define ICE_MAC_IPV4_PFCP_NODE			351
#define ICE_MAC_IPV4_PFCP_SESSION		352
#define ICE_MAC_IPV6_PFCP_NODE			353
#define ICE_MAC_IPV6_PFCP_SESSION		354
#define ICE_MAC_IPV4_L2TPV3			360
#define ICE_MAC_IPV6_L2TPV3			361
#define ICE_MAC_IPV4_L2TPV2_CONTROL		396
#define ICE_MAC_IPV6_L2TPV2_CONTROL		397
#define ICE_MAC_IPV4_L2TPV2			398
#define ICE_MAC_IPV6_L2TPV2			399
#define ICE_MAC_IPV4_PPPOL2TPV2			400
#define ICE_MAC_IPV6_PPPOL2TPV2			401
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV4_FRAG	402
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV4_PAY	403
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV4_UDP_PAY	404
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV4_TCP	405
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV4_SCTP	406
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV4_ICMP	407
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV6_FRAG	408
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV6_PAY	409
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV6_UDP_PAY	410
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV6_TCP	411
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV6_SCTP	412
#define ICE_MAC_IPV4_PPPOL2TPV2_IPV6_ICMPV6	413
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV4_FRAG	414
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV4_PAY	415
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV4_UDP_PAY	416
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV4_TCP	417
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV4_SCTP	418
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV4_ICMP	419
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV6_FRAG	420
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV6_PAY	421
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV6_UDP_PAY	422
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV6_TCP	423
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV6_SCTP	424
#define ICE_MAC_IPV6_PPPOL2TPV2_IPV6_ICMPV6	425
#define MAC_IPV4_TUN_IPV4_GTPU_IPV4_FRAG	450
#define MAC_IPV4_TUN_IPV4_GTPU_IPV4_PAY		451
#define MAC_IPV4_TUN_IPV4_GTPU_IPV4_UDP_PAY	452
#define MAC_IPV4_TUN_IPV4_GTPU_IPV4_TCP		453
#define MAC_IPV4_TUN_IPV4_GTPU_IPV4_SCTP	454
#define MAC_IPV4_TUN_IPV4_GTPU_IPV4_ICMP	455
#define MAC_IPV4_TUN_IPV4_GTPU_IPV6_FRAG	456
#define MAC_IPV4_TUN_IPV4_GTPU_IPV6_PAY		457
#define MAC_IPV4_TUN_IPV4_GTPU_IPV6_UDP_PAY	458
#define MAC_IPV4_TUN_IPV4_GTPU_IPV6_TCP		459
#define MAC_IPV4_TUN_IPV4_GTPU_IPV6_SCTP	460
#define MAC_IPV4_TUN_IPV4_GTPU_IPV6_ICMPV6	461
#define MAC_IPV4_TUN_IPV6_GTPU_IPV4_FRAG	462
#define MAC_IPV4_TUN_IPV6_GTPU_IPV4_PAY		463
#define MAC_IPV4_TUN_IPV6_GTPU_IPV4_UDP_PAY	464
#define MAC_IPV4_TUN_IPV6_GTPU_IPV4_TCP		465
#define MAC_IPV4_TUN_IPV6_GTPU_IPV4_SCTP	466
#define MAC_IPV4_TUN_IPV6_GTPU_IPV4_ICMP	467
#define MAC_IPV4_TUN_IPV6_GTPU_IPV6_FRAG	468
#define MAC_IPV4_TUN_IPV6_GTPU_IPV6_PAY		469
#define MAC_IPV4_TUN_IPV6_GTPU_IPV6_UDP_PAY	470
#define MAC_IPV4_TUN_IPV6_GTPU_IPV6_TCP		471
#define MAC_IPV4_TUN_IPV6_GTPU_IPV6_SCTP	472
#define MAC_IPV4_TUN_IPV6_GTPU_IPV6_ICMPV6	473
#define MAC_IPV6_TUN_IPV4_GTPU_IPV4_FRAG	474
#define MAC_IPV6_TUN_IPV4_GTPU_IPV4_PAY		475
#define MAC_IPV6_TUN_IPV4_GTPU_IPV4_UDP_PAY	476
#define MAC_IPV6_TUN_IPV4_GTPU_IPV4_TCP		477
#define MAC_IPV6_TUN_IPV4_GTPU_IPV4_SCTP	478
#define MAC_IPV6_TUN_IPV4_GTPU_IPV4_ICMP	479
#define MAC_IPV6_TUN_IPV4_GTPU_IPV6_FRAG	480
#define MAC_IPV6_TUN_IPV4_GTPU_IPV6_PAY		481
#define MAC_IPV6_TUN_IPV4_GTPU_IPV6_UDP_PAY	482
#define MAC_IPV6_TUN_IPV4_GTPU_IPV6_TCP		483
#define MAC_IPV6_TUN_IPV4_GTPU_IPV6_SCTP	484
#define MAC_IPV6_TUN_IPV4_GTPU_IPV6_ICMPV6	485
#define MAC_IPV6_TUN_IPV6_GTPU_IPV4_FRAG	486
#define MAC_IPV6_TUN_IPV6_GTPU_IPV4_PAY		487
#define MAC_IPV6_TUN_IPV6_GTPU_IPV4_UDP_PAY	488
#define MAC_IPV6_TUN_IPV6_GTPU_IPV4_TCP		489
#define MAC_IPV6_TUN_IPV6_GTPU_IPV4_SCTP	490
#define MAC_IPV6_TUN_IPV6_GTPU_IPV4_ICMP	491
#define MAC_IPV6_TUN_IPV6_GTPU_IPV6_FRAG	492
#define MAC_IPV6_TUN_IPV6_GTPU_IPV6_PAY		493
#define MAC_IPV6_TUN_IPV6_GTPU_IPV6_UDP_PAY	494
#define MAC_IPV6_TUN_IPV6_GTPU_IPV6_TCP		495
#define MAC_IPV6_TUN_IPV6_GTPU_IPV6_SCTP	496
#define MAC_IPV6_TUN_IPV6_GTPU_IPV6_ICMPV6	497

/* Attributes that can modify PTYPE definitions.
 *
 * These values will represent special attributes for PTYPES, which will
 * resolve into metadata packet flags definitions that can be used in the TCAM
 * for identifying a PTYPE with specific characteristics.
 */
enum ice_ptype_attrib_type {
	/* GTP PTYPES */
	ICE_PTYPE_ATTR_GTP_PDU_EH,
	ICE_PTYPE_ATTR_GTP_SESSION,
	ICE_PTYPE_ATTR_GTP_DOWNLINK,
	ICE_PTYPE_ATTR_GTP_UPLINK,
};

struct ice_ptype_attrib_info {
	u16 flags;
	u16 mask;
};

/* TCAM flag definitions */
#define ICE_GTP_PDU			BIT(14)
#define ICE_GTP_PDU_LINK		BIT(13)

/* GTP attributes */
#define ICE_GTP_PDU_FLAG_MASK		(ICE_GTP_PDU)
#define ICE_GTP_PDU_EH			ICE_GTP_PDU

#define ICE_GTP_FLAGS_MASK		(ICE_GTP_PDU | ICE_GTP_PDU_LINK)
#define ICE_GTP_SESSION			0
#define ICE_GTP_DOWNLINK		ICE_GTP_PDU
#define ICE_GTP_UPLINK			(ICE_GTP_PDU | ICE_GTP_PDU_LINK)

struct ice_ptype_attributes {
	u16 ptype;
	enum ice_ptype_attrib_type attrib;
};

struct ice_meta_sect {
	struct ice_pkg_ver ver;
#define ICE_META_SECT_NAME_SIZE	28
	char name[ICE_META_SECT_NAME_SIZE];
	__le32 track_id;
};

/* Packet Type Groups (PTG) - Inner Most fields (IM) */
#define ICE_PTG_IM_IPV4_TCP		16
#define ICE_PTG_IM_IPV4_UDP		17
#define ICE_PTG_IM_IPV4_SCTP		18
#define ICE_PTG_IM_IPV4_PAY		20
#define ICE_PTG_IM_IPV4_OTHER		21
#define ICE_PTG_IM_IPV6_TCP		32
#define ICE_PTG_IM_IPV6_UDP		33
#define ICE_PTG_IM_IPV6_SCTP		34
#define ICE_PTG_IM_IPV6_OTHER		37
#define ICE_PTG_IM_L2_OTHER		67

struct ice_flex_fields {
	union {
		struct {
			u8 src_ip;
			u8 dst_ip;
			u8 flow_label;	/* valid for IPv6 only */
		} ip_fields;

		struct {
			u8 src_prt;
			u8 dst_prt;
		} tcp_udp_fields;

		struct {
			u8 src_ip;
			u8 dst_ip;
			u8 src_prt;
			u8 dst_prt;
		} ip_tcp_udp_fields;

		struct {
			u8 src_prt;
			u8 dst_prt;
			u8 flow_label;	/* valid for IPv6 only */
			u8 spi;
		} ip_esp_fields;

		struct {
			u32 offset;
			u32 length;
		} off_len;
	} fields;
};

#define ICE_XLT1_DFLT_GRP	0
#define ICE_XLT1_TABLE_SIZE	1024

/* package labels */
struct ice_label {
	__le16 value;
#define ICE_PKG_LABEL_SIZE	64
	char name[ICE_PKG_LABEL_SIZE];
};

struct ice_label_section {
	__le16 count;
	struct ice_label label[STRUCT_HACK_VAR_LEN];
};

#define ICE_MAX_LABELS_IN_BUF ICE_MAX_ENTRIES_IN_BUF( \
	ice_struct_size((struct ice_label_section *)0, label, 1) - \
	sizeof(struct ice_label), sizeof(struct ice_label))

struct ice_sw_fv_section {
	__le16 count;
	__le16 base_offset;
	struct ice_fv fv[STRUCT_HACK_VAR_LEN];
};

struct ice_sw_fv_list_entry {
	struct LIST_ENTRY_TYPE list_entry;
	u32 profile_id;
	struct ice_fv *fv_ptr;
};

#pragma pack(1)
/* The BOOST TCAM stores the match packet header in reverse order, meaning
 * the fields are reversed; in addition, this means that the normally big endian
 * fields of the packet are now little endian.
 */
struct ice_boost_key_value {
#define ICE_BOOST_REMAINING_HV_KEY	15
	u8 remaining_hv_key[ICE_BOOST_REMAINING_HV_KEY];
	__le16 hv_dst_port_key;
	__le16 hv_src_port_key;
	u8 tcam_search_key;
};
#pragma pack()

struct ice_boost_key {
	struct ice_boost_key_value key;
	struct ice_boost_key_value key2;
};

/* package Boost TCAM entry */
struct ice_boost_tcam_entry {
	__le16 addr;
	__le16 reserved;
	/* break up the 40 bytes of key into different fields */
	struct ice_boost_key key;
	u8 boost_hit_index_group;
	/* The following contains bitfields which are not on byte boundaries.
	 * These fields are currently unused by driver software.
	 */
#define ICE_BOOST_BIT_FIELDS		43
	u8 bit_fields[ICE_BOOST_BIT_FIELDS];
};

struct ice_boost_tcam_section {
	__le16 count;
	__le16 reserved;
	struct ice_boost_tcam_entry tcam[STRUCT_HACK_VAR_LEN];
};

#define ICE_MAX_BST_TCAMS_IN_BUF ICE_MAX_ENTRIES_IN_BUF( \
	ice_struct_size((struct ice_boost_tcam_section *)0, tcam, 1) - \
	sizeof(struct ice_boost_tcam_entry), \
	sizeof(struct ice_boost_tcam_entry))

struct ice_xlt1_section {
	__le16 count;
	__le16 offset;
	u8 value[STRUCT_HACK_VAR_LEN];
};

struct ice_xlt2_section {
	__le16 count;
	__le16 offset;
	__le16 value[STRUCT_HACK_VAR_LEN];
};

struct ice_prof_redir_section {
	__le16 count;
	__le16 offset;
	u8 redir_value[STRUCT_HACK_VAR_LEN];
};

/* Tunnel enabling */

enum ice_tunnel_type {
	TNL_VXLAN = 0,
	TNL_GENEVE,
	TNL_GRETAP,
	TNL_GTP,
	TNL_GTPC,
	TNL_GTPU,
	TNL_ECPRI,
	TNL_LAST = 0xFF,
	TNL_ALL = 0xFF,
};

struct ice_tunnel_type_scan {
	enum ice_tunnel_type type;
	const char *label_prefix;
};

struct ice_tunnel_entry {
	enum ice_tunnel_type type;
	u16 boost_addr;
	u16 port;
	u16 ref;
	struct ice_boost_tcam_entry *boost_entry;
	u8 valid;
	u8 in_use;
	u8 marked;
};

#define ICE_TUNNEL_MAX_ENTRIES	16

struct ice_tunnel_table {
	struct ice_tunnel_entry tbl[ICE_TUNNEL_MAX_ENTRIES];
	u16 count;
};

struct ice_dvm_entry {
	u16 boost_addr;
	u16 enable;
	struct ice_boost_tcam_entry *boost_entry;
};

#define ICE_DVM_MAX_ENTRIES	48

struct ice_dvm_table {
	struct ice_dvm_entry tbl[ICE_DVM_MAX_ENTRIES];
	u16 count;
};

struct ice_pkg_es {
	__le16 count;
	__le16 offset;
	struct ice_fv_word es[STRUCT_HACK_VAR_LEN];
};

struct ice_es {
	u32 sid;
	u16 count;
	u16 fvw;
	u16 *ref_count;
	u32 *mask_ena;
	struct LIST_HEAD_TYPE prof_map;
	struct ice_fv_word *t;
	struct ice_lock prof_map_lock;	/* protect access to profiles list */
	u8 *written;
	u8 reverse; /* set to true to reverse FV order */
};

/* PTYPE Group management */

/* Note: XLT1 table takes 13-bit as input, and results in an 8-bit packet type
 * group (PTG) ID as output.
 *
 * Note: PTG 0 is the default packet type group and it is assumed that all PTYPE
 * are a part of this group until moved to a new PTG.
 */
#define ICE_DEFAULT_PTG	0

struct ice_ptg_entry {
	struct ice_ptg_ptype *first_ptype;
	u8 in_use;
};

struct ice_ptg_ptype {
	struct ice_ptg_ptype *next_ptype;
	u8 ptg;
};

#define ICE_MAX_TCAM_PER_PROFILE	64
#define ICE_MAX_PTG_PER_PROFILE		64

struct ice_prof_map {
	struct LIST_ENTRY_TYPE list;
	u64 profile_cookie;
	u64 context;
	u8 prof_id;
	u8 ptg_cnt;
	u8 ptg[ICE_MAX_PTG_PER_PROFILE];
	struct ice_ptype_attrib_info attr[ICE_MAX_PTG_PER_PROFILE];
};

#define ICE_INVALID_TCAM	0xFFFF

struct ice_tcam_inf {
	u16 tcam_idx;
	struct ice_ptype_attrib_info attr;
	u8 ptg;
	u8 prof_id;
	u8 in_use;
};

struct ice_vsig_prof {
	struct LIST_ENTRY_TYPE list;
	u64 profile_cookie;
	u8 prof_id;
	u8 tcam_count;
	struct ice_tcam_inf tcam[ICE_MAX_TCAM_PER_PROFILE];
};

struct ice_vsig_entry {
	struct LIST_HEAD_TYPE prop_lst;
	struct ice_vsig_vsi *first_vsi;
	u8 in_use;
};

struct ice_vsig_vsi {
	struct ice_vsig_vsi *next_vsi;
	u32 prop_mask;
	u16 changed;
	u16 vsig;
};

#define ICE_XLT1_CNT	1024
#define ICE_MAX_PTGS	256

/* XLT1 Table */
struct ice_xlt1 {
	struct ice_ptg_entry *ptg_tbl;
	struct ice_ptg_ptype *ptypes;
	u8 *t;
	u32 sid;
	u16 count;
};

#define ICE_XLT2_CNT	768
#define ICE_MAX_VSIGS	768

/* VSIG bit layout:
 * [0:12]: incremental VSIG index 1 to ICE_MAX_VSIGS
 * [13:15]: PF number of device
 */
#define ICE_VSIG_IDX_M	(0x1FFF)
#define ICE_PF_NUM_S	13
#define ICE_PF_NUM_M	(0x07 << ICE_PF_NUM_S)
#define ICE_VSIG_VALUE(vsig, pf_id) \
	((u16)((((u16)(vsig)) & ICE_VSIG_IDX_M) | \
	       (((u16)(pf_id) << ICE_PF_NUM_S) & ICE_PF_NUM_M)))
#define ICE_DEFAULT_VSIG	0

/* XLT2 Table */
struct ice_xlt2 {
	struct ice_vsig_entry *vsig_tbl;
	struct ice_vsig_vsi *vsis;
	u16 *t;
	u32 sid;
	u16 count;
};

/* Extraction sequence - list of match fields:
 * protocol ID, offset, profile length
 */
union ice_match_fld {
	struct {
		u8 prot_id;
		u8 offset;
		u8 length;
		u8 reserved; /* must be zero */
	} fld;
	u32 val;
};

#define ICE_MATCH_LIST_SZ	20
#pragma pack(1)
struct ice_match {
	u8 count;
	union ice_match_fld list[ICE_MATCH_LIST_SZ];
};

/* Profile ID Management */
struct ice_prof_id_key {
	__le16 flags;
	u8 xlt1;
	__le16 xlt2_cdid;
};

/* Keys are made up of two values, each one-half the size of the key.
 * For TCAM, the entire key is 80 bits wide (or 2, 40-bit wide values)
 */
#define ICE_TCAM_KEY_VAL_SZ	5
#define ICE_TCAM_KEY_SZ		(2 * ICE_TCAM_KEY_VAL_SZ)

struct ice_prof_tcam_entry {
	__le16 addr;
	u8 key[ICE_TCAM_KEY_SZ];
	u8 prof_id;
};

#pragma pack()

struct ice_prof_id_section {
	__le16 count;
	struct ice_prof_tcam_entry entry[STRUCT_HACK_VAR_LEN];
};

struct ice_prof_tcam {
	u32 sid;
	u16 count;
	u16 max_prof_id;
	struct ice_prof_tcam_entry *t;
	u8 cdid_bits; /* # CDID bits to use in key, 0, 2, 4, or 8 */
};

struct ice_prof_redir {
	u8 *t;
	u32 sid;
	u16 count;
};

struct ice_mask {
	u16 mask;	/* 16-bit mask */
	u16 idx;	/* index */
	u16 ref;	/* reference count */
	u8 in_use;	/* non-zero if used */
};

struct ice_masks {
	struct ice_lock lock;  /* lock to protect this structure */
	u16 first;	/* first mask owned by the PF */
	u16 count;	/* number of masks owned by the PF */
#define ICE_PROF_MASK_COUNT 32
	struct ice_mask masks[ICE_PROF_MASK_COUNT];
};

/* Tables per block */
struct ice_blk_info {
	struct ice_xlt1 xlt1;
	struct ice_xlt2 xlt2;
	struct ice_prof_tcam prof;
	struct ice_prof_redir prof_redir;
	struct ice_es es;
	struct ice_masks masks;
	u8 overwrite; /* set to true to allow overwrite of table entries */
	u8 is_list_init;
};

enum ice_chg_type {
	ICE_TCAM_NONE = 0,
	ICE_PTG_ES_ADD,
	ICE_TCAM_ADD,
	ICE_VSIG_ADD,
	ICE_VSIG_REM,
	ICE_VSI_MOVE,
};

struct ice_chs_chg {
	struct LIST_ENTRY_TYPE list_entry;
	enum ice_chg_type type;

	u8 add_ptg;
	u8 add_vsig;
	u8 add_tcam_idx;
	u8 add_prof;
	u16 ptype;
	u8 ptg;
	u8 prof_id;
	u16 vsi;
	u16 vsig;
	u16 orig_vsig;
	u16 tcam_idx;
	struct ice_ptype_attrib_info attr;
};

#define ICE_FLOW_PTYPE_MAX		ICE_XLT1_CNT

enum ice_prof_type {
	ICE_PROF_INVALID = 0x0,
	ICE_PROF_NON_TUN = 0x1,
	ICE_PROF_TUN_UDP = 0x2,
	ICE_PROF_TUN_GRE = 0x4,
	ICE_PROF_TUN_GTPU = 0x8,
	ICE_PROF_TUN_GTPC = 0x10,
	ICE_PROF_TUN_PPPOE = 0x20,
	ICE_PROF_TUN_ALL = 0x3E,
	ICE_PROF_ALL = 0xFF,
};

/* Number of bits/bytes contained in meta init entry. Note, this should be a
 * multiple of 32 bits.
 */
#define ICE_META_INIT_BITS	192
#define ICE_META_INIT_DW_CNT	(ICE_META_INIT_BITS / (sizeof(__le32) * \
				 BITS_PER_BYTE))

/* The meta init Flag field starts at this bit */
#define ICE_META_FLAGS_ST		123

/* The entry and bit to check for Double VLAN Mode (DVM) support */
#define ICE_META_VLAN_MODE_ENTRY	0
#define ICE_META_FLAG_VLAN_MODE		60
#define ICE_META_VLAN_MODE_BIT		(ICE_META_FLAGS_ST + \
					 ICE_META_FLAG_VLAN_MODE)

struct ice_meta_init_entry {
	__le32 bm[ICE_META_INIT_DW_CNT];
};

struct ice_meta_init_section {
	__le16 count;
	__le16 offset;
	struct ice_meta_init_entry entry[1];
};
#endif /* _ICE_FLEX_TYPE_H_ */
