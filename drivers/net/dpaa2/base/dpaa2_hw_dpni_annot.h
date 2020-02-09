/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016,2019 NXP
 *
 */

/**
 * @file
 *
 * DPNI packet parse results - implementation internal
 */

#ifndef _DPAA2_HW_DPNI_ANNOT_H_
#define _DPAA2_HW_DPNI_ANNOT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Annotation valid bits in FD FRC */
#define DPAA2_FD_FRC_FASV	0x8000
#define DPAA2_FD_FRC_FAEADV	0x4000
#define DPAA2_FD_FRC_FAPRV	0x2000
#define DPAA2_FD_FRC_FAIADV	0x1000
#define DPAA2_FD_FRC_FASWOV	0x0800
#define DPAA2_FD_FRC_FAICFDV	0x0400

/* Annotation bits in FD CTRL */
#define DPAA2_FD_CTRL_ASAL	0x00020000      /* ASAL = 128 */
#define DPAA2_FD_CTRL_PTA	0x00800000
#define DPAA2_FD_CTRL_PTV1	0x00400000

/* Frame annotation status */
struct dpaa2_fas {
	uint8_t reserved;
	uint8_t ppid;
	__le16 ifpid;
	__le32 status;
}  __rte_packed;

/**
 * HW Packet Annotation  Register structures
 */
struct dpaa2_annot_hdr {
	/**<	word1: Frame Annotation Status (8 bytes)*/
	uint64_t word1;

	/**<	word2: Time Stamp (8 bytes)*/
	uint64_t word2;

	/**<	word3: Next Hdr + FAF Extension + FAF (2 + 2 + 4 bytes)*/
	uint64_t word3;

	/**<	word4: Frame Annotation Flags-FAF (8 bytes) */
	uint64_t word4;

	/**<	word5:
	 *	ShimOffset_1 + ShimOffset_2 + IPPIDOffset + EthOffset +
	 *	LLC+SNAPOffset + VLANTCIOffset_1 + VLANTCIOffset_n +
	 *	LastETypeOffset (1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 bytes)
	 */
	uint64_t word5;

	/**<	word6:
	 *	PPPoEOffset + MPLSOffset_1 + MPLSOffset_n + ARPorIPOffset_1
	 *	+ IPOffset_norMInEncapO + GREOffset + L4Offset +
	 *	GTPorESPorIPSecOffset(1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 bytes)
	 */
	uint64_t word6;

	/**<	word7:
	 *	RoutingHdrOfset1 + RoutingHdrOfset2 + NxtHdrOffset
	 *	+ IPv6FragOffset + GrossRunningSum
	 *	+ RunningSum(1 + 1 + 1 + 1 + 2 + 2 bytes)
	 */
	uint64_t word7;

	/**<	word8:
	 *	ParseErrorcode + Soft Parsing Context (1 + 7 bytes)
	 */
	uint64_t word8;
};

/**
 * Internal Macros to get/set Packet annotation header
 */

/** General Macro to define a particular bit position*/
#define BIT_POS(x)			((uint64_t)1 << ((x)))
/** Set a bit in the variable */
#define BIT_SET_AT_POS(var, pos)	((var) |= (pos))
/** Reset the bit in the variable */
#define BIT_RESET_AT_POS(var, pos)	((var) &= ~(pos))
/** Check the bit is set in the variable */
#define BIT_ISSET_AT_POS(var, pos)	(((var) & (pos)) ? 1 : 0)
/**
 * Macrso to define bit position in word3
 */
#define NEXT_HDR(var)			((uint64_t)(var) & 0xFFFF000000000000)
#define FAF_EXTN_IPV6_ROUTE_HDR_PRESENT(var)	BIT_POS(16)
#define FAF_EXTN_RESERVED(var)		((uint64_t)(var) & 0x00007FFF00000000)
#define FAF_USER_DEFINED_RESERVED(var)	((uint64_t)(var) & 0x00000000FF000000)
#define SHIM_SHELL_SOFT_PARSING_ERRROR		BIT_POS(23)
#define PARSING_ERROR				BIT_POS(22)
#define L2_ETH_MAC_PRESENT			BIT_POS(21)
#define L2_ETH_MAC_UNICAST			BIT_POS(20)
#define L2_ETH_MAC_MULTICAST			BIT_POS(19)
#define L2_ETH_MAC_BROADCAST			BIT_POS(18)
#define L2_ETH_FRAME_IS_BPDU			BIT_POS(17)
#define L2_ETH_FCOE_PRESENT			BIT_POS(16)
#define L2_ETH_FIP_PRESENT			BIT_POS(15)
#define L2_ETH_PARSING_ERROR			BIT_POS(14)
#define L2_LLC_SNAP_PRESENT			BIT_POS(13)
#define L2_UNKNOWN_LLC_OUI			BIT_POS(12)
#define L2_LLC_SNAP_ERROR			BIT_POS(11)
#define L2_VLAN_1_PRESENT			BIT_POS(10)
#define L2_VLAN_N_PRESENT			BIT_POS(9)
#define L2_VLAN_CFI_BIT_PRESENT			BIT_POS(8)
#define L2_VLAN_PARSING_ERROR			BIT_POS(7)
#define L2_PPPOE_PPP_PRESENT			BIT_POS(6)
#define L2_PPPOE_PPP_PARSING_ERROR		BIT_POS(5)
#define L2_MPLS_1_PRESENT			BIT_POS(4)
#define L2_MPLS_N_PRESENT			BIT_POS(3)
#define L2_MPLS_PARSING_ERROR			BIT_POS(2)
#define L2_ARP_PRESENT				BIT_POS(1)
#define L2_ARP_PARSING_ERROR			BIT_POS(0)
/**
 * Macrso to define bit position in word4
 */
#define L2_UNKNOWN_PROTOCOL			BIT_POS(63)
#define L2_SOFT_PARSING_ERROR			BIT_POS(62)
#define L3_IPV4_1_PRESENT			BIT_POS(61)
#define L3_IPV4_1_UNICAST			BIT_POS(60)
#define L3_IPV4_1_MULTICAST			BIT_POS(59)
#define L3_IPV4_1_BROADCAST			BIT_POS(58)
#define L3_IPV4_N_PRESENT			BIT_POS(57)
#define L3_IPV4_N_UNICAST			BIT_POS(56)
#define L3_IPV4_N_MULTICAST			BIT_POS(55)
#define L3_IPV4_N_BROADCAST			BIT_POS(54)
#define L3_IPV6_1_PRESENT			BIT_POS(53)
#define L3_IPV6_1_UNICAST			BIT_POS(52)
#define L3_IPV6_1_MULTICAST			BIT_POS(51)
#define L3_IPV6_N_PRESENT			BIT_POS(50)
#define L3_IPV6_N_UNICAST			BIT_POS(49)
#define L3_IPV6_N_MULTICAST			BIT_POS(48)
#define L3_IP_1_OPT_PRESENT			BIT_POS(47)
#define L3_IP_1_UNKNOWN_PROTOCOL		BIT_POS(46)
#define L3_IP_1_MORE_FRAGMENT			BIT_POS(45)
#define L3_IP_1_FIRST_FRAGMENT			BIT_POS(44)
#define L3_IP_1_PARSING_ERROR			BIT_POS(43)
#define L3_IP_N_OPT_PRESENT			BIT_POS(42)
#define L3_IP_N_UNKNOWN_PROTOCOL		BIT_POS(41)
#define L3_IP_N_MORE_FRAGMENT			BIT_POS(40)
#define L3_IP_N_FIRST_FRAGMENT			BIT_POS(39)
#define L3_PROTO_ICMP_PRESENT			BIT_POS(38)
#define L3_PROTO_IGMP_PRESENT			BIT_POS(37)
#define L3_PROTO_ICMPV6_PRESENT			BIT_POS(36)
#define L3_PROTO_UDP_LIGHT_PRESENT		BIT_POS(35)
#define L3_IP_N_PARSING_ERROR			BIT_POS(34)
#define L3_MIN_ENCAP_PRESENT			BIT_POS(33)
#define L3_MIN_ENCAP_SBIT_PRESENT		BIT_POS(32)
#define L3_MIN_ENCAP_PARSING_ERROR		BIT_POS(31)
#define L3_PROTO_GRE_PRESENT			BIT_POS(30)
#define L3_PROTO_GRE_RBIT_PRESENT		BIT_POS(29)
#define L3_PROTO_GRE_PARSING_ERROR		BIT_POS(28)
#define L3_IP_UNKNOWN_PROTOCOL			BIT_POS(27)
#define L3_SOFT_PARSING_ERROR			BIT_POS(26)
#define L3_PROTO_UDP_PRESENT			BIT_POS(25)
#define L3_PROTO_UDP_PARSING_ERROR		BIT_POS(24)
#define L3_PROTO_TCP_PRESENT			BIT_POS(23)
#define L3_PROTO_TCP_OPT_PRESENT		BIT_POS(22)
#define L3_PROTO_TCP_CTRL_BIT_6_TO_11_PRESENT	BIT_POS(21)
#define L3_PROTO_TCP_CTRL_BIT_3_TO_5_PRESENT	BIT_POS(20)
#define L3_PROTO_TCP_PARSING_ERROR		BIT_POS(19)
#define L3_PROTO_IPSEC_PRESENT			BIT_POS(18)
#define L3_PROTO_IPSEC_ESP_PRESENT		BIT_POS(17)
#define L3_PROTO_IPSEC_AH_PRESENT		BIT_POS(16)
#define L3_PROTO_IPSEC_PARSING_ERROR		BIT_POS(15)
#define L3_PROTO_SCTP_PRESENT			BIT_POS(14)
#define L3_PROTO_SCTP_PARSING_ERROR		BIT_POS(13)
#define L3_PROTO_DCCP_PRESENT			BIT_POS(12)
#define L3_PROTO_DCCP_PARSING_ERROR		BIT_POS(11)
#define L4_UNKNOWN_PROTOCOL			BIT_POS(10)
#define L4_SOFT_PARSING_ERROR			BIT_POS(9)
#define L3_PROTO_GTP_PRESENT			BIT_POS(8)
#define L3_PROTO_GTP_PARSING_ERROR		BIT_POS(7)
#define L3_PROTO_ESP_PRESENT			BIT_POS(6)
#define L3_PROTO_ESP_PARSING_ERROR		BIT_POS(5)
#define L3_PROTO_ISCSI_PRESENT			BIT_POS(4)
#define L3_PROTO_CAPWAN__CTRL_PRESENT		BIT_POS(3)
#define L3_PROTO_CAPWAN__DATA_PRESENT		BIT_POS(2)
#define L5_SOFT_PARSING_ERROR			BIT_POS(1)
#define L3_IPV6_ROUTE_HDR_PRESENT		BIT_POS(0)

#define DPAA2_L3_IPv4 (L3_IPV4_1_PRESENT | L3_IPV4_1_UNICAST | \
	L3_IP_1_UNKNOWN_PROTOCOL | L3_IP_UNKNOWN_PROTOCOL)

#define DPAA2_L3_IPv6 (L3_IPV6_1_PRESENT | L3_IPV6_1_UNICAST | \
	L3_IP_1_UNKNOWN_PROTOCOL | L3_IP_UNKNOWN_PROTOCOL)

#define DPAA2_L3_IPv4_TCP (L3_IPV4_1_PRESENT | L3_IPV4_1_UNICAST | \
	L3_PROTO_TCP_PRESENT | L3_PROTO_TCP_CTRL_BIT_6_TO_11_PRESENT | \
	L4_UNKNOWN_PROTOCOL)

#define DPAA2_L3_IPv4_UDP (L3_IPV4_1_PRESENT | L3_IPV4_1_UNICAST | \
	L3_PROTO_UDP_PRESENT | L4_UNKNOWN_PROTOCOL)

#define DPAA2_L3_IPv6_TCP (L3_IPV6_1_PRESENT | L3_IPV6_1_UNICAST | \
	L3_PROTO_TCP_PRESENT | L3_PROTO_TCP_CTRL_BIT_6_TO_11_PRESENT | \
	L4_UNKNOWN_PROTOCOL)

#define DPAA2_L3_IPv6_UDP (L3_IPV6_1_PRESENT | L3_IPV6_1_UNICAST | \
	L3_PROTO_UDP_PRESENT | L4_UNKNOWN_PROTOCOL)

/**
 * Macros to get values in word5
 */
#define SHIM_OFFSET_1(var)		((uint64_t)(var) & 0xFF00000000000000)
#define SHIM_OFFSET_2(var)		((uint64_t)(var) & 0x00FF000000000000)
#define IP_PID_OFFSET(var)		((uint64_t)(var) & 0x0000FF0000000000)
#define ETH_OFFSET(var)			((uint64_t)(var) & 0x000000FF00000000)
#define LLC_SNAP_OFFSET(var)		((uint64_t)(var) & 0x00000000FF000000)
#define VLAN_TCI_OFFSET_1(var)		((uint64_t)(var) & 0x0000000000FF0000)
#define VLAN_TCI_OFFSET_N(var)		((uint64_t)(var) & 0x000000000000FF00)
#define LAST_ETYPE_OFFSET(var)		((uint64_t)(var) & 0x00000000000000FF)

/**
 * Macros to get values in word6
 */
#define PPPOE_OFFSET(var)		((uint64_t)(var) & 0xFF00000000000000)
#define MPLS_OFFSET_1(var)		((uint64_t)(var) & 0x00FF000000000000)
#define MPLS_OFFSET_N(var)		((uint64_t)(var) & 0x0000FF0000000000)
#define ARP_OR_IP_OFFSET_1(var)		((uint64_t)(var) & 0x000000FF00000000)
#define IP_N_OR_MIN_ENCAP_OFFSET(var)	((uint64_t)(var) & 0x00000000FF000000)
#define GRE_OFFSET(var)			((uint64_t)(var) & 0x0000000000FF0000)
#define L4_OFFSET(var)			((uint64_t)(var) & 0x000000000000FF00)
#define GTP_OR_ESP_OR_IPSEC_OFFSET(var)	((uint64_t)(var) & 0x00000000000000FF)

/**
 * Macros to get values in word7
 */
#define IPV6_ROUTING_HDR_OFFSET_1(var)	((uint64_t)(var) & 0xFF00000000000000)
#define IPV6_ROUTING_HDR_OFFSET_2(var)	((uint64_t)(var) & 0x00FF000000000000)
#define NEXT_HDR_OFFSET(var)		((uint64_t)(var) & 0x0000FF0000000000)
#define IPV6_FRAG_OFFSET(var)		((uint64_t)(var) & 0x000000FF00000000)
#define GROSS_RUNNING_SUM(var)		((uint64_t)(var) & 0x00000000FFFF0000)
#define RUNNING_SUM(var)		((uint64_t)(var) & 0x000000000000FFFF)

/**
 * Macros to get values in word8
 */
#define PARSE_ERROR_CODE(var)		((uint64_t)(var) & 0xFF00000000000000)
#define SOFT_PARSING_CONTEXT(var)	((uint64_t)(var) & 0x00FFFFFFFFFFFFFF)

/*FAEAD offset in anmotation area*/
#define DPAA2_FD_HW_ANNOT_FAEAD_OFFSET	0x58

struct dpaa2_faead {
	uint32_t fqid;
	uint32_t ctrl;
};

/*FAEAD bits */
/*A2 OMB contains valid data*/
#define DPAA2_ANNOT_FAEAD_A2V		0x20000000
/*egress confirmation FQID in FAEAD contains valid data*/
#define DPAA2_ANNOT_FAEAD_A4V		0x08000000
/*UPD is valid*/
#define DPAA2_ANNOT_FAEAD_UPDV		0x00001000
/*EBDD is valid*/
#define DPAA2_ANNOT_FAEAD_EBDDV		0x00002000
/*EBDD (External Buffer Deallocation Disable) */
#define DPAA2_ANNOT_FAEAD_EBDD		0x00000020
/*UPD (Update prepended data)*/
#define DPAA2_ANNOT_FAEAD_UPD		0x00000010

/* Debug frame, otherwise supposed to be discarded */
#define DPAA2_ETH_FAS_DISC	      0x80000000
/* MACSEC frame */
#define DPAA2_ETH_FAS_MS		0x40000000
#define DPAA2_ETH_FAS_PTP	       BIT_POS(59)
/* Ethernet multicast frame */
#define DPAA2_ETH_FAS_MC		0x04000000
/* Ethernet broadcast frame */
#define DPAA2_ETH_FAS_BC		0x02000000
#define DPAA2_ETH_FAS_KSE	       0x00040000
#define DPAA2_ETH_FAS_EOFHE	     0x00020000
#define DPAA2_ETH_FAS_MNLE	      0x00010000
#define DPAA2_ETH_FAS_TIDE	      0x00008000
#define DPAA2_ETH_FAS_PIEE	      0x00004000
/* Frame length error */
#define DPAA2_ETH_FAS_FLE	       0x00002000
/* Frame physical error; our favourite pastime */
#define DPAA2_ETH_FAS_FPE	       0x00001000
#define DPAA2_ETH_FAS_PTE	       0x00000080
#define DPAA2_ETH_FAS_ISP	       0x00000040
#define DPAA2_ETH_FAS_PHE	       0x00000020
#define DPAA2_ETH_FAS_BLE	       0x00000010
/* L3 csum validation performed */
#define DPAA2_ETH_FAS_L3CV	      0x00000008
/* L3 csum error */
#define DPAA2_ETH_FAS_L3CE	      0x00000004
/* L4 csum validation performed */
#define DPAA2_ETH_FAS_L4CV	      0x00000002
/* L4 csum error */
#define DPAA2_ETH_FAS_L4CE	      0x00000001

#ifdef __cplusplus
}
#endif

#endif
