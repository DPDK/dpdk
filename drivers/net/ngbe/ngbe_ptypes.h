/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#ifndef _NGBE_PTYPE_H_
#define _NGBE_PTYPE_H_

/**
 * PTID(Packet Type Identifier, 8bits)
 * - Bit 3:0 detailed types.
 * - Bit 5:4 basic types.
 * - Bit 7:6 tunnel types.
 **/
#define NGBE_PTID_NULL                 0
#define NGBE_PTID_MAX                  256
#define NGBE_PTID_MASK                 0xFF
#define NGBE_PTID_MASK_TUNNEL          0x7F

/* TUN */
#define NGBE_PTID_TUN_IPV6             0x40
#define NGBE_PTID_TUN_EI               0x00 /* IP */
#define NGBE_PTID_TUN_EIG              0x10 /* IP+GRE */
#define NGBE_PTID_TUN_EIGM             0x20 /* IP+GRE+MAC */
#define NGBE_PTID_TUN_EIGMV            0x30 /* IP+GRE+MAC+VLAN */

/* PKT for !TUN */
#define NGBE_PTID_PKT_TUN             (0x80)
#define NGBE_PTID_PKT_MAC             (0x10)
#define NGBE_PTID_PKT_IP              (0x20)

/* TYP for PKT=mac */
#define NGBE_PTID_TYP_MAC             (0x01)
#define NGBE_PTID_TYP_TS              (0x02) /* time sync */
#define NGBE_PTID_TYP_FIP             (0x03)
#define NGBE_PTID_TYP_LLDP            (0x04)
#define NGBE_PTID_TYP_CNM             (0x05)
#define NGBE_PTID_TYP_EAPOL           (0x06)
#define NGBE_PTID_TYP_ARP             (0x07)
#define NGBE_PTID_TYP_ETF             (0x08)

/* TYP for PKT=ip */
#define NGBE_PTID_PKT_IPV6            (0x08)
#define NGBE_PTID_TYP_IPFRAG          (0x01)
#define NGBE_PTID_TYP_IPDATA          (0x02)
#define NGBE_PTID_TYP_UDP             (0x03)
#define NGBE_PTID_TYP_TCP             (0x04)
#define NGBE_PTID_TYP_SCTP            (0x05)

/* packet type non-ip values */
enum ngbe_l2_ptids {
	NGBE_PTID_L2_ABORTED = (NGBE_PTID_PKT_MAC),
	NGBE_PTID_L2_MAC = (NGBE_PTID_PKT_MAC | NGBE_PTID_TYP_MAC),
	NGBE_PTID_L2_TMST = (NGBE_PTID_PKT_MAC | NGBE_PTID_TYP_TS),
	NGBE_PTID_L2_FIP = (NGBE_PTID_PKT_MAC | NGBE_PTID_TYP_FIP),
	NGBE_PTID_L2_LLDP = (NGBE_PTID_PKT_MAC | NGBE_PTID_TYP_LLDP),
	NGBE_PTID_L2_CNM = (NGBE_PTID_PKT_MAC | NGBE_PTID_TYP_CNM),
	NGBE_PTID_L2_EAPOL = (NGBE_PTID_PKT_MAC | NGBE_PTID_TYP_EAPOL),
	NGBE_PTID_L2_ARP = (NGBE_PTID_PKT_MAC | NGBE_PTID_TYP_ARP),

	NGBE_PTID_L2_IPV4_FRAG = (NGBE_PTID_PKT_IP | NGBE_PTID_TYP_IPFRAG),
	NGBE_PTID_L2_IPV4 = (NGBE_PTID_PKT_IP | NGBE_PTID_TYP_IPDATA),
	NGBE_PTID_L2_IPV4_UDP = (NGBE_PTID_PKT_IP | NGBE_PTID_TYP_UDP),
	NGBE_PTID_L2_IPV4_TCP = (NGBE_PTID_PKT_IP | NGBE_PTID_TYP_TCP),
	NGBE_PTID_L2_IPV4_SCTP = (NGBE_PTID_PKT_IP | NGBE_PTID_TYP_SCTP),
	NGBE_PTID_L2_IPV6_FRAG = (NGBE_PTID_PKT_IP | NGBE_PTID_PKT_IPV6 |
			NGBE_PTID_TYP_IPFRAG),
	NGBE_PTID_L2_IPV6 = (NGBE_PTID_PKT_IP | NGBE_PTID_PKT_IPV6 |
			NGBE_PTID_TYP_IPDATA),
	NGBE_PTID_L2_IPV6_UDP = (NGBE_PTID_PKT_IP | NGBE_PTID_PKT_IPV6 |
			NGBE_PTID_TYP_UDP),
	NGBE_PTID_L2_IPV6_TCP = (NGBE_PTID_PKT_IP | NGBE_PTID_PKT_IPV6 |
			NGBE_PTID_TYP_TCP),
	NGBE_PTID_L2_IPV6_SCTP = (NGBE_PTID_PKT_IP | NGBE_PTID_PKT_IPV6 |
			NGBE_PTID_TYP_SCTP),

	NGBE_PTID_L2_TUN4_MAC = (NGBE_PTID_PKT_TUN |
			NGBE_PTID_TUN_EIGM),
	NGBE_PTID_L2_TUN6_MAC = (NGBE_PTID_PKT_TUN |
			NGBE_PTID_TUN_IPV6 | NGBE_PTID_TUN_EIGM),
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
struct rte_ngbe_ptype {
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

#define RTE_PTYPE_L2_ETHER_FIP              RTE_PTYPE_L2_ETHER
#define RTE_PTYPE_L2_ETHER_CNM              RTE_PTYPE_L2_ETHER
#define RTE_PTYPE_L2_ETHER_EAPOL            RTE_PTYPE_L2_ETHER
#define RTE_PTYPE_L2_ETHER_FILTER           RTE_PTYPE_L2_ETHER

u32 *ngbe_get_supported_ptypes(void);
u32 ngbe_decode_ptype(u8 ptid);
u8 ngbe_encode_ptype(u32 ptype);

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
#define NGBE_PT_ETHER                   0x00
#define NGBE_PT_IPV4                    0x01
#define NGBE_PT_IPV4_TCP                0x11
#define NGBE_PT_IPV4_UDP                0x21
#define NGBE_PT_IPV4_SCTP               0x41
#define NGBE_PT_IPV4_EXT                0x03
#define NGBE_PT_IPV4_EXT_TCP            0x13
#define NGBE_PT_IPV4_EXT_UDP            0x23
#define NGBE_PT_IPV4_EXT_SCTP           0x43
#define NGBE_PT_IPV6                    0x04
#define NGBE_PT_IPV6_TCP                0x14
#define NGBE_PT_IPV6_UDP                0x24
#define NGBE_PT_IPV6_SCTP               0x44
#define NGBE_PT_IPV6_EXT                0x0C
#define NGBE_PT_IPV6_EXT_TCP            0x1C
#define NGBE_PT_IPV6_EXT_UDP            0x2C
#define NGBE_PT_IPV6_EXT_SCTP           0x4C
#define NGBE_PT_IPV4_IPV6               0x05
#define NGBE_PT_IPV4_IPV6_TCP           0x15
#define NGBE_PT_IPV4_IPV6_UDP           0x25
#define NGBE_PT_IPV4_IPV6_SCTP          0x45
#define NGBE_PT_IPV4_EXT_IPV6           0x07
#define NGBE_PT_IPV4_EXT_IPV6_TCP       0x17
#define NGBE_PT_IPV4_EXT_IPV6_UDP       0x27
#define NGBE_PT_IPV4_EXT_IPV6_SCTP      0x47
#define NGBE_PT_IPV4_IPV6_EXT           0x0D
#define NGBE_PT_IPV4_IPV6_EXT_TCP       0x1D
#define NGBE_PT_IPV4_IPV6_EXT_UDP       0x2D
#define NGBE_PT_IPV4_IPV6_EXT_SCTP      0x4D
#define NGBE_PT_IPV4_EXT_IPV6_EXT       0x0F
#define NGBE_PT_IPV4_EXT_IPV6_EXT_TCP   0x1F
#define NGBE_PT_IPV4_EXT_IPV6_EXT_UDP   0x2F
#define NGBE_PT_IPV4_EXT_IPV6_EXT_SCTP  0x4F

#define NGBE_PT_MAX    256

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
#define NGBE_ETF_ID_EAPOL        0
#define NGBE_ETF_ID_FCOE         2
#define NGBE_ETF_ID_1588         3
#define NGBE_ETF_ID_FIP          4
#define NGBE_ETF_ID_LLDP         5
#define NGBE_ETF_ID_LACP         6
#define NGBE_ETF_ID_FC           7
#define NGBE_ETF_ID_MAX          8

#define NGBE_PTID_ETF_MIN  0x18
#define NGBE_PTID_ETF_MAX  0x1F
static inline int ngbe_etflt_id(u8 ptid)
{
	if (ptid >= NGBE_PTID_ETF_MIN && ptid <= NGBE_PTID_ETF_MAX)
		return ptid - NGBE_PTID_ETF_MIN;
	else
		return -1;
}

#endif /* _NGBE_PTYPE_H_ */
