/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 */

#ifndef _TXGBE_PTYPE_H_
#define _TXGBE_PTYPE_H_

/**
 * PTID(Packet Type Identifier, 8bits)
 * - Bit 3:0 detailed types.
 * - Bit 5:4 basic types.
 * - Bit 7:6 tunnel types.
 **/
#define TXGBE_PTID_NULL                 0
#define TXGBE_PTID_MAX                  256
#define TXGBE_PTID_MASK                 0xFF
#define TXGBE_PTID_MASK_TUNNEL          0x7F

/* TUN */
#define TXGBE_PTID_TUN_IPV6             0x40
#define TXGBE_PTID_TUN_EI               0x00 /* IP */
#define TXGBE_PTID_TUN_EIG              0x10 /* IP+GRE */
#define TXGBE_PTID_TUN_EIGM             0x20 /* IP+GRE+MAC */
#define TXGBE_PTID_TUN_EIGMV            0x30 /* IP+GRE+MAC+VLAN */

/* PKT for !TUN */
#define TXGBE_PTID_PKT_TUN             (0x80)
#define TXGBE_PTID_PKT_MAC             (0x10)
#define TXGBE_PTID_PKT_IP              (0x20)
#define TXGBE_PTID_PKT_FCOE            (0x30)

/* TYP for PKT=mac */
#define TXGBE_PTID_TYP_MAC             (0x01)
#define TXGBE_PTID_TYP_TS              (0x02) /* time sync */
#define TXGBE_PTID_TYP_FIP             (0x03)
#define TXGBE_PTID_TYP_LLDP            (0x04)
#define TXGBE_PTID_TYP_CNM             (0x05)
#define TXGBE_PTID_TYP_EAPOL           (0x06)
#define TXGBE_PTID_TYP_ARP             (0x07)
#define TXGBE_PTID_TYP_ETF             (0x08)

/* TYP for PKT=ip */
#define TXGBE_PTID_PKT_IPV6            (0x08)
#define TXGBE_PTID_TYP_IPFRAG          (0x01)
#define TXGBE_PTID_TYP_IPDATA          (0x02)
#define TXGBE_PTID_TYP_UDP             (0x03)
#define TXGBE_PTID_TYP_TCP             (0x04)
#define TXGBE_PTID_TYP_SCTP            (0x05)

/* TYP for PKT=fcoe */
#define TXGBE_PTID_PKT_VFT             (0x08)
#define TXGBE_PTID_TYP_FCOE            (0x00)
#define TXGBE_PTID_TYP_FCDATA          (0x01)
#define TXGBE_PTID_TYP_FCRDY           (0x02)
#define TXGBE_PTID_TYP_FCRSP           (0x03)
#define TXGBE_PTID_TYP_FCOTHER         (0x04)

/* packet type non-ip values */
enum txgbe_l2_ptids {
	TXGBE_PTID_L2_ABORTED = (TXGBE_PTID_PKT_MAC),
	TXGBE_PTID_L2_MAC = (TXGBE_PTID_PKT_MAC | TXGBE_PTID_TYP_MAC),
	TXGBE_PTID_L2_TMST = (TXGBE_PTID_PKT_MAC | TXGBE_PTID_TYP_TS),
	TXGBE_PTID_L2_FIP = (TXGBE_PTID_PKT_MAC | TXGBE_PTID_TYP_FIP),
	TXGBE_PTID_L2_LLDP = (TXGBE_PTID_PKT_MAC | TXGBE_PTID_TYP_LLDP),
	TXGBE_PTID_L2_CNM = (TXGBE_PTID_PKT_MAC | TXGBE_PTID_TYP_CNM),
	TXGBE_PTID_L2_EAPOL = (TXGBE_PTID_PKT_MAC | TXGBE_PTID_TYP_EAPOL),
	TXGBE_PTID_L2_ARP = (TXGBE_PTID_PKT_MAC | TXGBE_PTID_TYP_ARP),

	TXGBE_PTID_L2_IPV4_FRAG = (TXGBE_PTID_PKT_IP | TXGBE_PTID_TYP_IPFRAG),
	TXGBE_PTID_L2_IPV4 = (TXGBE_PTID_PKT_IP | TXGBE_PTID_TYP_IPDATA),
	TXGBE_PTID_L2_IPV4_UDP = (TXGBE_PTID_PKT_IP | TXGBE_PTID_TYP_UDP),
	TXGBE_PTID_L2_IPV4_TCP = (TXGBE_PTID_PKT_IP | TXGBE_PTID_TYP_TCP),
	TXGBE_PTID_L2_IPV4_SCTP = (TXGBE_PTID_PKT_IP | TXGBE_PTID_TYP_SCTP),
	TXGBE_PTID_L2_IPV6_FRAG = (TXGBE_PTID_PKT_IP | TXGBE_PTID_PKT_IPV6 |
			TXGBE_PTID_TYP_IPFRAG),
	TXGBE_PTID_L2_IPV6 = (TXGBE_PTID_PKT_IP | TXGBE_PTID_PKT_IPV6 |
			TXGBE_PTID_TYP_IPDATA),
	TXGBE_PTID_L2_IPV6_UDP = (TXGBE_PTID_PKT_IP | TXGBE_PTID_PKT_IPV6 |
			TXGBE_PTID_TYP_UDP),
	TXGBE_PTID_L2_IPV6_TCP = (TXGBE_PTID_PKT_IP | TXGBE_PTID_PKT_IPV6 |
			TXGBE_PTID_TYP_TCP),
	TXGBE_PTID_L2_IPV6_SCTP = (TXGBE_PTID_PKT_IP | TXGBE_PTID_PKT_IPV6 |
			TXGBE_PTID_TYP_SCTP),

	TXGBE_PTID_L2_FCOE = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_TYP_FCOE),
	TXGBE_PTID_L2_FCOE_FCDATA = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_TYP_FCDATA),
	TXGBE_PTID_L2_FCOE_FCRDY = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_TYP_FCRDY),
	TXGBE_PTID_L2_FCOE_FCRSP = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_TYP_FCRSP),
	TXGBE_PTID_L2_FCOE_FCOTHER = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_TYP_FCOTHER),
	TXGBE_PTID_L2_FCOE_VFT = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_PKT_VFT),
	TXGBE_PTID_L2_FCOE_VFT_FCDATA = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_PKT_VFT | TXGBE_PTID_TYP_FCDATA),
	TXGBE_PTID_L2_FCOE_VFT_FCRDY = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_PKT_VFT | TXGBE_PTID_TYP_FCRDY),
	TXGBE_PTID_L2_FCOE_VFT_FCRSP = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_PKT_VFT | TXGBE_PTID_TYP_FCRSP),
	TXGBE_PTID_L2_FCOE_VFT_FCOTHER = (TXGBE_PTID_PKT_FCOE |
			TXGBE_PTID_PKT_VFT | TXGBE_PTID_TYP_FCOTHER),

	TXGBE_PTID_L2_TUN4_MAC = (TXGBE_PTID_PKT_TUN |
			TXGBE_PTID_TUN_EIGM),
	TXGBE_PTID_L2_TUN6_MAC = (TXGBE_PTID_PKT_TUN |
			TXGBE_PTID_TUN_IPV6 | TXGBE_PTID_TUN_EIGM),
};


/*
 * PTYPE(Packet Type, 32bits)
 * - Bit 3:0 is for L2 types.
 * - Bit 7:4 is for L3 or outer L3 (for tunneling case) types.
 * - Bit 11:8 is for L4 or outer L4 (for tunneling case) types.
 * - Bit 15:12 is for tunnel types.
 * - Bit 19:16 is for inner L2 types.
 * - Bit 23:20 is for inner L3 types.
 * - Bit 27:24 is for inner L4 types.
 * - Bit 31:28 is reserved.
 * please ref to rte_mbuf.h: rte_mbuf.packet_type
 */
struct rte_txgbe_ptype {
	u32 l2:4;  /* outer mac */
	u32 l3:4;  /* outer internet protocol */
	u32 l4:4;  /* outer transport protocol */
	u32 tun:4; /* tunnel protocol */

	u32 el2:4; /* inner mac */
	u32 el3:4; /* inner internet protocol */
	u32 el4:4; /* inner transport protocol */
	u32 rsv:3;
	u32 known:1;
};

#ifndef RTE_PTYPE_UNKNOWN
#define RTE_PTYPE_UNKNOWN                   0x00000000
#define RTE_PTYPE_L2_ETHER                  0x00000001
#define RTE_PTYPE_L2_ETHER_TIMESYNC         0x00000002
#define RTE_PTYPE_L2_ETHER_ARP              0x00000003
#define RTE_PTYPE_L2_ETHER_LLDP             0x00000004
#define RTE_PTYPE_L2_ETHER_NSH              0x00000005
#define RTE_PTYPE_L2_ETHER_FCOE             0x00000009
#define RTE_PTYPE_L3_IPV4                   0x00000010
#define RTE_PTYPE_L3_IPV4_EXT               0x00000030
#define RTE_PTYPE_L3_IPV6                   0x00000040
#define RTE_PTYPE_L3_IPV4_EXT_UNKNOWN       0x00000090
#define RTE_PTYPE_L3_IPV6_EXT               0x000000c0
#define RTE_PTYPE_L3_IPV6_EXT_UNKNOWN       0x000000e0
#define RTE_PTYPE_L4_TCP                    0x00000100
#define RTE_PTYPE_L4_UDP                    0x00000200
#define RTE_PTYPE_L4_FRAG                   0x00000300
#define RTE_PTYPE_L4_SCTP                   0x00000400
#define RTE_PTYPE_L4_ICMP                   0x00000500
#define RTE_PTYPE_L4_NONFRAG                0x00000600
#define RTE_PTYPE_TUNNEL_IP                 0x00001000
#define RTE_PTYPE_TUNNEL_GRE                0x00002000
#define RTE_PTYPE_TUNNEL_VXLAN              0x00003000
#define RTE_PTYPE_TUNNEL_NVGRE              0x00004000
#define RTE_PTYPE_TUNNEL_GENEVE             0x00005000
#define RTE_PTYPE_TUNNEL_GRENAT             0x00006000
#define RTE_PTYPE_INNER_L2_ETHER            0x00010000
#define RTE_PTYPE_INNER_L2_ETHER_VLAN       0x00020000
#define RTE_PTYPE_INNER_L3_IPV4             0x00100000
#define RTE_PTYPE_INNER_L3_IPV4_EXT         0x00200000
#define RTE_PTYPE_INNER_L3_IPV6             0x00300000
#define RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN 0x00400000
#define RTE_PTYPE_INNER_L3_IPV6_EXT         0x00500000
#define RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN 0x00600000
#define RTE_PTYPE_INNER_L4_TCP              0x01000000
#define RTE_PTYPE_INNER_L4_UDP              0x02000000
#define RTE_PTYPE_INNER_L4_FRAG             0x03000000
#define RTE_PTYPE_INNER_L4_SCTP             0x04000000
#define RTE_PTYPE_INNER_L4_ICMP             0x05000000
#define RTE_PTYPE_INNER_L4_NONFRAG          0x06000000
#endif /* !RTE_PTYPE_UNKNOWN */
#define RTE_PTYPE_L3_IPV4u                  RTE_PTYPE_L3_IPV4_EXT_UNKNOWN
#define RTE_PTYPE_L3_IPV6u                  RTE_PTYPE_L3_IPV6_EXT_UNKNOWN
#define RTE_PTYPE_INNER_L3_IPV4u            RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN
#define RTE_PTYPE_INNER_L3_IPV6u            RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN
#define RTE_PTYPE_L2_ETHER_FIP              RTE_PTYPE_L2_ETHER
#define RTE_PTYPE_L2_ETHER_CNM              RTE_PTYPE_L2_ETHER
#define RTE_PTYPE_L2_ETHER_EAPOL            RTE_PTYPE_L2_ETHER
#define RTE_PTYPE_L2_ETHER_FILTER           RTE_PTYPE_L2_ETHER

u32 *txgbe_get_supported_ptypes(void);
u32 txgbe_decode_ptype(u8 ptid);
u8 txgbe_encode_ptype(u32 ptype);

/**
 * PT(Packet Type, 32bits)
 * - Bit 3:0 is for L2 types.
 * - Bit 7:4 is for L3 or outer L3 (for tunneling case) types.
 * - Bit 11:8 is for L4 or outer L4 (for tunneling case) types.
 * - Bit 15:12 is for tunnel types.
 * - Bit 19:16 is for inner L2 types.
 * - Bit 23:20 is for inner L3 types.
 * - Bit 27:24 is for inner L4 types.
 * - Bit 31:28 is reserved.
 * PT is a more accurate version of PTYPE
 **/
#define TXGBE_PT_ETHER                   0x00
#define TXGBE_PT_IPV4                    0x01
#define TXGBE_PT_IPV4_TCP                0x11
#define TXGBE_PT_IPV4_UDP                0x21
#define TXGBE_PT_IPV4_SCTP               0x41
#define TXGBE_PT_IPV4_EXT                0x03
#define TXGBE_PT_IPV4_EXT_TCP            0x13
#define TXGBE_PT_IPV4_EXT_UDP            0x23
#define TXGBE_PT_IPV4_EXT_SCTP           0x43
#define TXGBE_PT_IPV6                    0x04
#define TXGBE_PT_IPV6_TCP                0x14
#define TXGBE_PT_IPV6_UDP                0x24
#define TXGBE_PT_IPV6_SCTP               0x44
#define TXGBE_PT_IPV6_EXT                0x0C
#define TXGBE_PT_IPV6_EXT_TCP            0x1C
#define TXGBE_PT_IPV6_EXT_UDP            0x2C
#define TXGBE_PT_IPV6_EXT_SCTP           0x4C
#define TXGBE_PT_IPV4_IPV6               0x05
#define TXGBE_PT_IPV4_IPV6_TCP           0x15
#define TXGBE_PT_IPV4_IPV6_UDP           0x25
#define TXGBE_PT_IPV4_IPV6_SCTP          0x45
#define TXGBE_PT_IPV4_EXT_IPV6           0x07
#define TXGBE_PT_IPV4_EXT_IPV6_TCP       0x17
#define TXGBE_PT_IPV4_EXT_IPV6_UDP       0x27
#define TXGBE_PT_IPV4_EXT_IPV6_SCTP      0x47
#define TXGBE_PT_IPV4_IPV6_EXT           0x0D
#define TXGBE_PT_IPV4_IPV6_EXT_TCP       0x1D
#define TXGBE_PT_IPV4_IPV6_EXT_UDP       0x2D
#define TXGBE_PT_IPV4_IPV6_EXT_SCTP      0x4D
#define TXGBE_PT_IPV4_EXT_IPV6_EXT       0x0F
#define TXGBE_PT_IPV4_EXT_IPV6_EXT_TCP   0x1F
#define TXGBE_PT_IPV4_EXT_IPV6_EXT_UDP   0x2F
#define TXGBE_PT_IPV4_EXT_IPV6_EXT_SCTP  0x4F

#define TXGBE_PT_NVGRE                   0x00
#define TXGBE_PT_NVGRE_IPV4              0x01
#define TXGBE_PT_NVGRE_IPV4_TCP          0x11
#define TXGBE_PT_NVGRE_IPV4_UDP          0x21
#define TXGBE_PT_NVGRE_IPV4_SCTP         0x41
#define TXGBE_PT_NVGRE_IPV4_EXT          0x03
#define TXGBE_PT_NVGRE_IPV4_EXT_TCP      0x13
#define TXGBE_PT_NVGRE_IPV4_EXT_UDP      0x23
#define TXGBE_PT_NVGRE_IPV4_EXT_SCTP     0x43
#define TXGBE_PT_NVGRE_IPV6              0x04
#define TXGBE_PT_NVGRE_IPV6_TCP          0x14
#define TXGBE_PT_NVGRE_IPV6_UDP          0x24
#define TXGBE_PT_NVGRE_IPV6_SCTP         0x44
#define TXGBE_PT_NVGRE_IPV6_EXT          0x0C
#define TXGBE_PT_NVGRE_IPV6_EXT_TCP      0x1C
#define TXGBE_PT_NVGRE_IPV6_EXT_UDP      0x2C
#define TXGBE_PT_NVGRE_IPV6_EXT_SCTP     0x4C
#define TXGBE_PT_NVGRE_IPV4_IPV6         0x05
#define TXGBE_PT_NVGRE_IPV4_IPV6_TCP     0x15
#define TXGBE_PT_NVGRE_IPV4_IPV6_UDP     0x25
#define TXGBE_PT_NVGRE_IPV4_IPV6_EXT     0x0D
#define TXGBE_PT_NVGRE_IPV4_IPV6_EXT_TCP 0x1D
#define TXGBE_PT_NVGRE_IPV4_IPV6_EXT_UDP 0x2D

#define TXGBE_PT_VXLAN                   0x80
#define TXGBE_PT_VXLAN_IPV4              0x81
#define TXGBE_PT_VXLAN_IPV4_TCP          0x91
#define TXGBE_PT_VXLAN_IPV4_UDP          0xA1
#define TXGBE_PT_VXLAN_IPV4_SCTP         0xC1
#define TXGBE_PT_VXLAN_IPV4_EXT          0x83
#define TXGBE_PT_VXLAN_IPV4_EXT_TCP      0x93
#define TXGBE_PT_VXLAN_IPV4_EXT_UDP      0xA3
#define TXGBE_PT_VXLAN_IPV4_EXT_SCTP     0xC3
#define TXGBE_PT_VXLAN_IPV6              0x84
#define TXGBE_PT_VXLAN_IPV6_TCP          0x94
#define TXGBE_PT_VXLAN_IPV6_UDP          0xA4
#define TXGBE_PT_VXLAN_IPV6_SCTP         0xC4
#define TXGBE_PT_VXLAN_IPV6_EXT          0x8C
#define TXGBE_PT_VXLAN_IPV6_EXT_TCP      0x9C
#define TXGBE_PT_VXLAN_IPV6_EXT_UDP      0xAC
#define TXGBE_PT_VXLAN_IPV6_EXT_SCTP     0xCC
#define TXGBE_PT_VXLAN_IPV4_IPV6         0x85
#define TXGBE_PT_VXLAN_IPV4_IPV6_TCP     0x95
#define TXGBE_PT_VXLAN_IPV4_IPV6_UDP     0xA5
#define TXGBE_PT_VXLAN_IPV4_IPV6_EXT     0x8D
#define TXGBE_PT_VXLAN_IPV4_IPV6_EXT_TCP 0x9D
#define TXGBE_PT_VXLAN_IPV4_IPV6_EXT_UDP 0xAD

#define TXGBE_PT_MAX    256
extern const u32 txgbe_ptype_table[TXGBE_PT_MAX];
extern const u32 txgbe_ptype_table_tn[TXGBE_PT_MAX];


/* ether type filter list: one static filter per filter consumer. This is
 *                 to avoid filter collisions later. Add new filters
 *                 here!!
 *      EAPOL 802.1x (0x888e): Filter 0
 *      FCoE (0x8906):   Filter 2
 *      1588 (0x88f7):   Filter 3
 *      FIP  (0x8914):   Filter 4
 *      LLDP (0x88CC):   Filter 5
 *      LACP (0x8809):   Filter 6
 *      FC   (0x8808):   Filter 7
 */
#define TXGBE_ETF_ID_EAPOL        0
#define TXGBE_ETF_ID_FCOE         2
#define TXGBE_ETF_ID_1588         3
#define TXGBE_ETF_ID_FIP          4
#define TXGBE_ETF_ID_LLDP         5
#define TXGBE_ETF_ID_LACP         6
#define TXGBE_ETF_ID_FC           7
#define TXGBE_ETF_ID_MAX          8

#define TXGBE_PTID_ETF_MIN  0x18
#define TXGBE_PTID_ETF_MAX  0x1F
static inline int txgbe_etflt_id(u8 ptid)
{
	if (ptid >= TXGBE_PTID_ETF_MIN && ptid <= TXGBE_PTID_ETF_MAX)
		return ptid - TXGBE_PTID_ETF_MIN;
	else
		return -1;
}

struct txgbe_udphdr {
	__be16	source;
	__be16	dest;
	__be16	len;
	__be16	check;
};

struct txgbe_vxlanhdr {
	__be32 vx_flags;
	__be32 vx_vni;
};

struct txgbe_genevehdr {
	u8 opt_len:6;
	u8 ver:2;
	u8 rsvd1:6;
	u8 critical:1;
	u8 oam:1;
	__be16 proto_type;

	u8 vni[3];
	u8 rsvd2;
};

struct txgbe_nvgrehdr {
	__be16 flags;
	__be16 proto;
	__be32 tni;
};

struct txgbe_grehdr {
	__be16 flags;
	__be16 proto;
};

#endif /* _TXGBE_PTYPE_H_ */
