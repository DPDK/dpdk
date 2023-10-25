/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 * Copyright 2014-2016 6WIND S.A.
 */

#ifndef _RTE_MBUF_PTYPE_H_
#define _RTE_MBUF_PTYPE_H_

/**
 * @file
 * RTE Mbuf Packet Types
 *
 * This file contains declarations for features related to mbuf packet
 * types. The packet type gives information about the data carried by the
 * mbuf, and is stored in the mbuf in a 32 bits field.
 *
 * The 32 bits are divided into several fields to mark packet types. Note that
 * each field is indexical.
 * - Bit 3:0 is for L2 types.
 * - Bit 7:4 is for L3 or outer L3 (for tunneling case) types.
 * - Bit 11:8 is for L4 or outer L4 (for tunneling case) types.
 * - Bit 15:12 is for tunnel types.
 * - Bit 19:16 is for inner L2 types.
 * - Bit 23:20 is for inner L3 types.
 * - Bit 27:24 is for inner L4 types.
 * - Bit 31:28 is reserved.
 *
 * To be compatible with Vector PMD, RTE_PTYPE_L3_IPV4, RTE_PTYPE_L3_IPV4_EXT,
 * RTE_PTYPE_L3_IPV6, RTE_PTYPE_L3_IPV6_EXT, RTE_PTYPE_L4_TCP, RTE_PTYPE_L4_UDP
 * and RTE_PTYPE_L4_SCTP should be kept as below in a contiguous 7 bits.
 *
 * Note that L3 types values are selected for checking IPV4/IPV6 header from
 * performance point of view. Reading annotations of RTE_ETH_IS_IPV4_HDR and
 * RTE_ETH_IS_IPV6_HDR is needed for any future changes of L3 type values.
 *
 * Note that the packet types of the same packet recognized by different
 * hardware may be different, as different hardware may have different
 * capability of packet type recognition.
 *
 * examples:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=0x29
 * | 'version'=6, 'next header'=0x3A
 * | 'ICMPv6 header'>
 * will be recognized on i40e hardware as packet type combination of,
 * RTE_PTYPE_L2_ETHER |
 * RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
 * RTE_PTYPE_TUNNEL_IP |
 * RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
 * RTE_PTYPE_INNER_L4_ICMP.
 *
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=0x2F
 * | 'GRE header'
 * | 'version'=6, 'next header'=0x11
 * | 'UDP header'>
 * will be recognized on i40e hardware as packet type combination of,
 * RTE_PTYPE_L2_ETHER |
 * RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
 * RTE_PTYPE_TUNNEL_GRENAT |
 * RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
 * RTE_PTYPE_INNER_L4_UDP.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * No packet type information.
 */
#define RTE_PTYPE_UNKNOWN                   0x00000000
/**
 * Ethernet packet type.
 * It is used for outer packet for tunneling cases.
 *
 * Packet format:
 * <'ether type'=[0x0800|0x86DD]>
 */
#define RTE_PTYPE_L2_ETHER                  0x00000001
/**
 * Ethernet packet type for time sync.
 *
 * Packet format:
 * <'ether type'=0x88F7>
 */
#define RTE_PTYPE_L2_ETHER_TIMESYNC         0x00000002
/**
 * ARP (Address Resolution Protocol) packet type.
 *
 * Packet format:
 * <'ether type'=0x0806>
 */
#define RTE_PTYPE_L2_ETHER_ARP              0x00000003
/**
 * LLDP (Link Layer Discovery Protocol) packet type.
 *
 * Packet format:
 * <'ether type'=0x88CC>
 */
#define RTE_PTYPE_L2_ETHER_LLDP             0x00000004
/**
 * NSH (Network Service Header) packet type.
 *
 * Packet format:
 * <'ether type'=0x894F>
 */
#define RTE_PTYPE_L2_ETHER_NSH              0x00000005
/**
 * VLAN packet type.
 *
 * Packet format:
 * <'ether type'=[0x8100]>
 */
#define RTE_PTYPE_L2_ETHER_VLAN             0x00000006
/**
 * QinQ packet type.
 *
 * Packet format:
 * <'ether type'=[0x88A8]>
 */
#define RTE_PTYPE_L2_ETHER_QINQ             0x00000007
/**
 * PPPOE packet type.
 *
 * Packet format:
 * <'ether type'=[0x8863|0x8864]>
 */
#define RTE_PTYPE_L2_ETHER_PPPOE            0x00000008
/**
 * FCoE packet type.
 *
 * Packet format:
 * <'ether type'=[0x8906]>
 */
#define RTE_PTYPE_L2_ETHER_FCOE             0x00000009
/**
 * MPLS packet type.
 *
 * Packet format:
 * <'ether type'=[0x8847|0x8848]>
 */
#define RTE_PTYPE_L2_ETHER_MPLS             0x0000000a
/**
 * Mask of layer 2 packet types.
 * It is used for outer packet for tunneling cases.
 */
#define RTE_PTYPE_L2_MASK                   0x0000000f
/**
 * IP (Internet Protocol) version 4 packet type.
 * It is used for outer packet for tunneling cases, and does not contain any
 * header option.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'ihl'=5>
 */
#define RTE_PTYPE_L3_IPV4                   0x00000010
/**
 * IP (Internet Protocol) version 4 packet type.
 * It is used for outer packet for tunneling cases, and contains header
 * options.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'ihl'=[6-15], 'options'>
 */
#define RTE_PTYPE_L3_IPV4_EXT               0x00000030
/**
 * IP (Internet Protocol) version 6 packet type.
 * It is used for outer packet for tunneling cases, and does not contain any
 * extension header.
 *
 * Packet format:
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=0x3B>
 */
#define RTE_PTYPE_L3_IPV6                   0x00000040
/**
 * IP (Internet Protocol) version 4 packet type.
 * It is used for outer packet for tunneling cases, and may or maynot contain
 * header options.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'ihl'=[5-15], <'options'>>
 */
#define RTE_PTYPE_L3_IPV4_EXT_UNKNOWN       0x00000090
/**
 * IP (Internet Protocol) version 6 packet type.
 * It is used for outer packet for tunneling cases, and contains extension
 * headers.
 *
 * Packet format:
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=[0x0|0x2B|0x2C|0x32|0x33|0x3C|0x87],
 *   'extension headers'>
 */
#define RTE_PTYPE_L3_IPV6_EXT               0x000000c0
/**
 * IP (Internet Protocol) version 6 packet type.
 * It is used for outer packet for tunneling cases, and may or maynot contain
 * extension headers.
 *
 * Packet format:
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=[0x3B|0x0|0x2B|0x2C|0x32|0x33|0x3C|0x87],
 *   <'extension headers'>>
 */
#define RTE_PTYPE_L3_IPV6_EXT_UNKNOWN       0x000000e0
/**
 * Mask of layer 3 packet types.
 * It is used for outer packet for tunneling cases.
 */
#define RTE_PTYPE_L3_MASK                   0x000000f0
/**
 * TCP (Transmission Control Protocol) packet type.
 * It is used for outer packet for tunneling cases.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=6, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=6>
 */
#define RTE_PTYPE_L4_TCP                    0x00000100
/**
 * UDP (User Datagram Protocol) packet type.
 * It is used for outer packet for tunneling cases.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17>
 */
#define RTE_PTYPE_L4_UDP                    0x00000200
/**
 * Fragmented IP (Internet Protocol) packet type.
 * It is used for outer packet for tunneling cases.
 *
 * It refers to those packets of any IP types, which can be recognized as
 * fragmented. A fragmented packet cannot be recognized as any other L4 types
 * (RTE_PTYPE_L4_TCP, RTE_PTYPE_L4_UDP, RTE_PTYPE_L4_SCTP, RTE_PTYPE_L4_ICMP,
 * RTE_PTYPE_L4_NONFRAG).
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'MF'=1>
 * or,
 * <'ether type'=0x0800
 * | 'version'=4, 'frag_offset'!=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=44>
 */
#define RTE_PTYPE_L4_FRAG                   0x00000300
/**
 * SCTP (Stream Control Transmission Protocol) packet type.
 * It is used for outer packet for tunneling cases.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=132, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=132>
 */
#define RTE_PTYPE_L4_SCTP                   0x00000400
/**
 * ICMP (Internet Control Message Protocol) packet type.
 * It is used for outer packet for tunneling cases.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=1, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=1>
 */
#define RTE_PTYPE_L4_ICMP                   0x00000500
/**
 * Non-fragmented IP (Internet Protocol) packet type.
 * It is used for outer packet for tunneling cases.
 *
 * It refers to those packets of any IP types, while cannot be recognized as
 * any of above L4 types (RTE_PTYPE_L4_TCP, RTE_PTYPE_L4_UDP,
 * RTE_PTYPE_L4_FRAG, RTE_PTYPE_L4_SCTP, RTE_PTYPE_L4_ICMP).
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'!=[6|17|132|1], 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'!=[6|17|44|132|1]>
 */
#define RTE_PTYPE_L4_NONFRAG                0x00000600
/**
 * IGMP (Internet Group Management Protocol) packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=2, 'MF'=0, 'frag_offset'=0>
 */
#define RTE_PTYPE_L4_IGMP                   0x00000700
/**
 * Mask of layer 4 packet types.
 * It is used for outer packet for tunneling cases.
 */
#define RTE_PTYPE_L4_MASK                   0x00000f00
/**
 * IP (Internet Protocol) in IP (Internet Protocol) tunneling packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=[4|41]>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=[4|41]>
 */
#define RTE_PTYPE_TUNNEL_IP                 0x00001000
/**
 * GRE (Generic Routing Encapsulation) tunneling packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=47>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=47>
 */
#define RTE_PTYPE_TUNNEL_GRE                0x00002000
/**
 * VXLAN (Virtual eXtensible Local Area Network) tunneling packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17
 * | 'destination port'=4789>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=4789>
 */
#define RTE_PTYPE_TUNNEL_VXLAN              0x00003000
/**
 * NVGRE (Network Virtualization using Generic Routing Encapsulation) tunneling
 * packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=47
 * | 'protocol type'=0x6558>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=47
 * | 'protocol type'=0x6558'>
 */
#define RTE_PTYPE_TUNNEL_NVGRE              0x00004000
/**
 * GENEVE (Generic Network Virtualization Encapsulation) tunneling packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17
 * | 'destination port'=6081>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=6081>
 */
#define RTE_PTYPE_TUNNEL_GENEVE             0x00005000
/**
 * Tunneling packet type of Teredo, VXLAN (Virtual eXtensible Local Area
 * Network) or GRE (Generic Routing Encapsulation) could be recognized as this
 * packet type, if they can not be recognized independently as of hardware
 * capability.
 */
#define RTE_PTYPE_TUNNEL_GRENAT             0x00006000
/**
 * GTP-C (GPRS Tunnelling Protocol) control tunneling packet type.
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17
 * | 'destination port'=2123>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=2123>
 * or,
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17
 * | 'source port'=2123>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'source port'=2123>
 */
#define RTE_PTYPE_TUNNEL_GTPC               0x00007000
/**
 * GTP-U (GPRS Tunnelling Protocol) user data tunneling packet type.
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17
 * | 'destination port'=2152>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=2152>
 */
#define RTE_PTYPE_TUNNEL_GTPU               0x00008000
/**
 * ESP (IP Encapsulating Security Payload) tunneling packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=50>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=50>
 */
#define RTE_PTYPE_TUNNEL_ESP                0x00009000
/**
 * L2TP (Layer 2 Tunneling Protocol) tunneling packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17>
 * | 'destination port'=1701>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=1701>
 * or,
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=115>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'protocol'=115>
 */
#define RTE_PTYPE_TUNNEL_L2TP               0x0000a000
/**
 * VXLAN-GPE (VXLAN Generic Protocol Extension) tunneling packet type.
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17
 * | 'destination port'=4790>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=4790>
 */
#define RTE_PTYPE_TUNNEL_VXLAN_GPE          0x0000b000
/**
 * MPLS-in-GRE tunneling packet type (RFC 4023).
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=47
 * | 'protocol'=0x8847>
 * or,
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=47
 * | 'protocol'=0x8848>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'protocol'=47
 * | 'protocol'=0x8847>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=47
 * | 'protocol'=0x8848>
 */
#define RTE_PTYPE_TUNNEL_MPLS_IN_GRE       0x0000c000
/**
 * MPLS-in-UDP tunneling packet type (RFC 7510).
 *
 * Packet format:
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17
 * | 'destination port'=6635>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=6635>
 */
#define RTE_PTYPE_TUNNEL_MPLS_IN_UDP      0x0000d000
/**
 * Mask of tunneling packet types.
 */
#define RTE_PTYPE_TUNNEL_MASK               0x0000f000
/**
 * Ethernet packet type.
 * It is used for inner packet type only.
 *
 * Packet format (inner only):
 * <'ether type'=[0x800|0x86DD]>
 */
#define RTE_PTYPE_INNER_L2_ETHER            0x00010000
/**
 * Ethernet packet type with VLAN (Virtual Local Area Network) tag.
 *
 * Packet format (inner only):
 * <'ether type'=[0x800|0x86DD], vlan=[1-4095]>
 */
#define RTE_PTYPE_INNER_L2_ETHER_VLAN       0x00020000
/**
 * QinQ packet type.
 *
 * Packet format:
 * <'ether type'=[0x88A8]>
 */
#define RTE_PTYPE_INNER_L2_ETHER_QINQ       0x00030000
/**
 * Mask of inner layer 2 packet types.
 */
#define RTE_PTYPE_INNER_L2_MASK             0x000f0000
/**
 * IP (Internet Protocol) version 4 packet type.
 * It is used for inner packet only, and does not contain any header option.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'ihl'=5>
 */
#define RTE_PTYPE_INNER_L3_IPV4             0x00100000
/**
 * IP (Internet Protocol) version 4 packet type.
 * It is used for inner packet only, and contains header options.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'ihl'=[6-15], 'options'>
 */
#define RTE_PTYPE_INNER_L3_IPV4_EXT         0x00200000
/**
 * IP (Internet Protocol) version 6 packet type.
 * It is used for inner packet only, and does not contain any extension header.
 *
 * Packet format (inner only):
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=0x3B>
 */
#define RTE_PTYPE_INNER_L3_IPV6             0x00300000
/**
 * IP (Internet Protocol) version 4 packet type.
 * It is used for inner packet only, and may or maynot contain header options.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'ihl'=[5-15], <'options'>>
 */
#define RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN 0x00400000
/**
 * IP (Internet Protocol) version 6 packet type.
 * It is used for inner packet only, and contains extension headers.
 *
 * Packet format (inner only):
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=[0x0|0x2B|0x2C|0x32|0x33|0x3C|0x87],
 *   'extension headers'>
 */
#define RTE_PTYPE_INNER_L3_IPV6_EXT         0x00500000
/**
 * IP (Internet Protocol) version 6 packet type.
 * It is used for inner packet only, and may or maynot contain extension
 * headers.
 *
 * Packet format (inner only):
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=[0x3B|0x0|0x2B|0x2C|0x32|0x33|0x3C|0x87],
 *   <'extension headers'>>
 */
#define RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN 0x00600000
/**
 * Mask of inner layer 3 packet types.
 */
#define RTE_PTYPE_INNER_L3_MASK             0x00f00000
/**
 * TCP (Transmission Control Protocol) packet type.
 * It is used for inner packet only.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=6, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=6>
 */
#define RTE_PTYPE_INNER_L4_TCP              0x01000000
/**
 * UDP (User Datagram Protocol) packet type.
 * It is used for inner packet only.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=17, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17>
 */
#define RTE_PTYPE_INNER_L4_UDP              0x02000000
/**
 * Fragmented IP (Internet Protocol) packet type.
 * It is used for inner packet only, and may or maynot have layer 4 packet.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'MF'=1>
 * or,
 * <'ether type'=0x0800
 * | 'version'=4, 'frag_offset'!=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=44>
 */
#define RTE_PTYPE_INNER_L4_FRAG             0x03000000
/**
 * SCTP (Stream Control Transmission Protocol) packet type.
 * It is used for inner packet only.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=132, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=132>
 */
#define RTE_PTYPE_INNER_L4_SCTP             0x04000000
/**
 * ICMP (Internet Control Message Protocol) packet type.
 * It is used for inner packet only.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'=1, 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=1>
 */
#define RTE_PTYPE_INNER_L4_ICMP             0x05000000
/**
 * Non-fragmented IP (Internet Protocol) packet type.
 * It is used for inner packet only, and may or maynot have other unknown layer
 * 4 packet types.
 *
 * Packet format (inner only):
 * <'ether type'=0x0800
 * | 'version'=4, 'protocol'!=[6|17|132|1], 'MF'=0, 'frag_offset'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'!=[6|17|44|132|1]>
 */
#define RTE_PTYPE_INNER_L4_NONFRAG          0x06000000
/**
 * Mask of inner layer 4 packet types.
 */
#define RTE_PTYPE_INNER_L4_MASK             0x0f000000
/**
 * All valid layer masks.
 */
#define RTE_PTYPE_ALL_MASK                  0x0fffffff

/**
 * Check if the (outer) L3 header is IPv4. To avoid comparing IPv4 types one by
 * one, bit 4 is selected to be used for IPv4 only. Then checking bit 4 can
 * determine if it is an IPV4 packet.
 */
#define  RTE_ETH_IS_IPV4_HDR(ptype) ((ptype) & RTE_PTYPE_L3_IPV4)

/**
 * Check if the (outer) L3 header is IPv6. To avoid comparing IPv6 types one by
 * one, bit 6 is selected to be used for IPv6 only. Then checking bit 6 can
 * determine if it is an IPV6 packet.
 */
#define  RTE_ETH_IS_IPV6_HDR(ptype) ((ptype) & RTE_PTYPE_L3_IPV6)

/* Check if it is a tunneling packet */
#define RTE_ETH_IS_TUNNEL_PKT(ptype) ((ptype) &				\
	(RTE_PTYPE_TUNNEL_MASK |					\
		RTE_PTYPE_INNER_L2_MASK |				\
		RTE_PTYPE_INNER_L3_MASK |				\
		RTE_PTYPE_INNER_L4_MASK))

/**
 * Get the name of the l2 packet type
 *
 * @param ptype
 *   The packet type value.
 * @return
 *   A non-null string describing the packet type.
 */
const char *rte_get_ptype_l2_name(uint32_t ptype);

/**
 * Get the name of the l3 packet type
 *
 * @param ptype
 *   The packet type value.
 * @return
 *   A non-null string describing the packet type.
 */
const char *rte_get_ptype_l3_name(uint32_t ptype);

/**
 * Get the name of the l4 packet type
 *
 * @param ptype
 *   The packet type value.
 * @return
 *   A non-null string describing the packet type.
 */
const char *rte_get_ptype_l4_name(uint32_t ptype);

/**
 * Get the name of the tunnel packet type
 *
 * @param ptype
 *   The packet type value.
 * @return
 *   A non-null string describing the packet type.
 */
const char *rte_get_ptype_tunnel_name(uint32_t ptype);

/**
 * Get the name of the inner_l2 packet type
 *
 * @param ptype
 *   The packet type value.
 * @return
 *   A non-null string describing the packet type.
 */
const char *rte_get_ptype_inner_l2_name(uint32_t ptype);

/**
 * Get the name of the inner_l3 packet type
 *
 * @param ptype
 *   The packet type value.
 * @return
 *   A non-null string describing the packet type.
 */
const char *rte_get_ptype_inner_l3_name(uint32_t ptype);

/**
 * Get the name of the inner_l4 packet type
 *
 * @param ptype
 *   The packet type value.
 * @return
 *   A non-null string describing the packet type.
 */
const char *rte_get_ptype_inner_l4_name(uint32_t ptype);

/**
 * Write the packet type name into the buffer
 *
 * @param ptype
 *   The packet type value.
 * @param buf
 *   The buffer where the string is written.
 * @param buflen
 *   The length of the buffer.
 * @return
 *   - 0 on success
 *   - (-1) if the buffer is too small
 */
int rte_get_ptype_name(uint32_t ptype, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_PTYPE_H_ */
