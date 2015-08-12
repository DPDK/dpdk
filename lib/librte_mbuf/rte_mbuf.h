/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright 2014 6WIND S.A.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_MBUF_H_
#define _RTE_MBUF_H_

/**
 * @file
 * RTE Mbuf
 *
 * The mbuf library provides the ability to create and destroy buffers
 * that may be used by the RTE application to store message
 * buffers. The message buffers are stored in a mempool, using the
 * RTE mempool library.
 *
 * This library provide an API to allocate/free packet mbufs, which are
 * used to carry network packets.
 *
 * To understand the concepts of packet buffers or mbufs, you
 * should read "TCP/IP Illustrated, Volume 2: The Implementation,
 * Addison-Wesley, 1995, ISBN 0-201-63354-X from Richard Stevens"
 * http://www.kohala.com/start/tcpipiv2.html
 */

#include <stdint.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_memory.h>
#include <rte_atomic.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>

#ifdef __cplusplus
extern "C" {
#endif

/* deprecated options */
#pragma GCC poison RTE_MBUF_SCATTER_GATHER
#pragma GCC poison RTE_MBUF_REFCNT

/*
 * Packet Offload Features Flags. It also carry packet type information.
 * Critical resources. Both rx/tx shared these bits. Be cautious on any change
 *
 * - RX flags start at bit position zero, and get added to the left of previous
 *   flags.
 * - The most-significant 3 bits are reserved for generic mbuf flags
 * - TX flags therefore start at bit position 60 (i.e. 63-3), and new flags get
 *   added to the right of the previously defined flags i.e. they should count
 *   downwards, not upwards.
 *
 * Keep these flags synchronized with rte_get_rx_ol_flag_name() and
 * rte_get_tx_ol_flag_name().
 */
#define PKT_RX_VLAN_PKT      (1ULL << 0)  /**< RX packet is a 802.1q VLAN packet. */
#define PKT_RX_RSS_HASH      (1ULL << 1)  /**< RX packet with RSS hash result. */
#define PKT_RX_FDIR          (1ULL << 2)  /**< RX packet with FDIR match indicate. */
#define PKT_RX_L4_CKSUM_BAD  (1ULL << 3)  /**< L4 cksum of RX pkt. is not OK. */
#define PKT_RX_IP_CKSUM_BAD  (1ULL << 4)  /**< IP cksum of RX pkt. is not OK. */
#define PKT_RX_EIP_CKSUM_BAD (0ULL << 0)  /**< External IP header checksum error. */
#define PKT_RX_OVERSIZE      (0ULL << 0)  /**< Num of desc of an RX pkt oversize. */
#define PKT_RX_HBUF_OVERFLOW (0ULL << 0)  /**< Header buffer overflow. */
#define PKT_RX_RECIP_ERR     (0ULL << 0)  /**< Hardware processing error. */
#define PKT_RX_MAC_ERR       (0ULL << 0)  /**< MAC error. */
#ifndef RTE_NEXT_ABI
#define PKT_RX_IPV4_HDR      (1ULL << 5)  /**< RX packet with IPv4 header. */
#define PKT_RX_IPV4_HDR_EXT  (1ULL << 6)  /**< RX packet with extended IPv4 header. */
#define PKT_RX_IPV6_HDR      (1ULL << 7)  /**< RX packet with IPv6 header. */
#define PKT_RX_IPV6_HDR_EXT  (1ULL << 8)  /**< RX packet with extended IPv6 header. */
#endif /* RTE_NEXT_ABI */
#define PKT_RX_IEEE1588_PTP  (1ULL << 9)  /**< RX IEEE1588 L2 Ethernet PT Packet. */
#define PKT_RX_IEEE1588_TMST (1ULL << 10) /**< RX IEEE1588 L2/L4 timestamped packet.*/
#ifndef RTE_NEXT_ABI
#define PKT_RX_TUNNEL_IPV4_HDR (1ULL << 11) /**< RX tunnel packet with IPv4 header.*/
#define PKT_RX_TUNNEL_IPV6_HDR (1ULL << 12) /**< RX tunnel packet with IPv6 header. */
#endif /* RTE_NEXT_ABI */
#define PKT_RX_FDIR_ID       (1ULL << 13) /**< FD id reported if FDIR match. */
#define PKT_RX_FDIR_FLX      (1ULL << 14) /**< Flexible bytes reported if FDIR match. */
#define PKT_RX_QINQ_PKT      (1ULL << 15)  /**< RX packet with double VLAN stripped. */
/* add new RX flags here */

/* add new TX flags here */

/**
 * Second VLAN insertion (QinQ) flag.
 */
#define PKT_TX_QINQ_PKT    (1ULL << 49)   /**< TX packet with double VLAN inserted. */

/**
 * TCP segmentation offload. To enable this offload feature for a
 * packet to be transmitted on hardware supporting TSO:
 *  - set the PKT_TX_TCP_SEG flag in mbuf->ol_flags (this flag implies
 *    PKT_TX_TCP_CKSUM)
 *  - set the flag PKT_TX_IPV4 or PKT_TX_IPV6
 *  - if it's IPv4, set the PKT_TX_IP_CKSUM flag and write the IP checksum
 *    to 0 in the packet
 *  - fill the mbuf offload information: l2_len, l3_len, l4_len, tso_segsz
 *  - calculate the pseudo header checksum without taking ip_len in account,
 *    and set it in the TCP header. Refer to rte_ipv4_phdr_cksum() and
 *    rte_ipv6_phdr_cksum() that can be used as helpers.
 */
#define PKT_TX_TCP_SEG       (1ULL << 50)

#define PKT_TX_IEEE1588_TMST (1ULL << 51) /**< TX IEEE1588 packet to timestamp. */

/**
 * Bits 52+53 used for L4 packet type with checksum enabled: 00: Reserved,
 * 01: TCP checksum, 10: SCTP checksum, 11: UDP checksum. To use hardware
 * L4 checksum offload, the user needs to:
 *  - fill l2_len and l3_len in mbuf
 *  - set the flags PKT_TX_TCP_CKSUM, PKT_TX_SCTP_CKSUM or PKT_TX_UDP_CKSUM
 *  - set the flag PKT_TX_IPV4 or PKT_TX_IPV6
 *  - calculate the pseudo header checksum and set it in the L4 header (only
 *    for TCP or UDP). See rte_ipv4_phdr_cksum() and rte_ipv6_phdr_cksum().
 *    For SCTP, set the crc field to 0.
 */
#define PKT_TX_L4_NO_CKSUM   (0ULL << 52) /**< Disable L4 cksum of TX pkt. */
#define PKT_TX_TCP_CKSUM     (1ULL << 52) /**< TCP cksum of TX pkt. computed by NIC. */
#define PKT_TX_SCTP_CKSUM    (2ULL << 52) /**< SCTP cksum of TX pkt. computed by NIC. */
#define PKT_TX_UDP_CKSUM     (3ULL << 52) /**< UDP cksum of TX pkt. computed by NIC. */
#define PKT_TX_L4_MASK       (3ULL << 52) /**< Mask for L4 cksum offload request. */

/**
 * Offload the IP checksum in the hardware. The flag PKT_TX_IPV4 should
 * also be set by the application, although a PMD will only check
 * PKT_TX_IP_CKSUM.
 *  - set the IP checksum field in the packet to 0
 *  - fill the mbuf offload information: l2_len, l3_len
 */
#define PKT_TX_IP_CKSUM      (1ULL << 54)

/**
 * Packet is IPv4. This flag must be set when using any offload feature
 * (TSO, L3 or L4 checksum) to tell the NIC that the packet is an IPv4
 * packet. If the packet is a tunneled packet, this flag is related to
 * the inner headers.
 */
#define PKT_TX_IPV4          (1ULL << 55)

/**
 * Packet is IPv6. This flag must be set when using an offload feature
 * (TSO or L4 checksum) to tell the NIC that the packet is an IPv6
 * packet. If the packet is a tunneled packet, this flag is related to
 * the inner headers.
 */
#define PKT_TX_IPV6          (1ULL << 56)

#define PKT_TX_VLAN_PKT      (1ULL << 57) /**< TX packet is a 802.1q VLAN packet. */

/**
 * Offload the IP checksum of an external header in the hardware. The
 * flag PKT_TX_OUTER_IPV4 should also be set by the application, alto ugh
 * a PMD will only check PKT_TX_IP_CKSUM.  The IP checksum field in the
 * packet must be set to 0.
 *  - set the outer IP checksum field in the packet to 0
 *  - fill the mbuf offload information: outer_l2_len, outer_l3_len
 */
#define PKT_TX_OUTER_IP_CKSUM   (1ULL << 58)

/**
 * Packet outer header is IPv4. This flag must be set when using any
 * outer offload feature (L3 or L4 checksum) to tell the NIC that the
 * outer header of the tunneled packet is an IPv4 packet.
 */
#define PKT_TX_OUTER_IPV4   (1ULL << 59)

/**
 * Packet outer header is IPv6. This flag must be set when using any
 * outer offload feature (L4 checksum) to tell the NIC that the outer
 * header of the tunneled packet is an IPv6 packet.
 */
#define PKT_TX_OUTER_IPV6    (1ULL << 60)

#define __RESERVED           (1ULL << 61) /**< reserved for future mbuf use */

#define IND_ATTACHED_MBUF    (1ULL << 62) /**< Indirect attached mbuf */

/* Use final bit of flags to indicate a control mbuf */
#define CTRL_MBUF_FLAG       (1ULL << 63) /**< Mbuf contains control data */

#ifdef RTE_NEXT_ABI
/*
 * 32 bits are divided into several fields to mark packet types. Note that
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
 * | 'version'=4, 'protocol'=6, 'MF'=0>
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
 * | 'version'=4, 'protocol'=17, 'MF'=0>
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
 * | 'version'=4, 'protocol'=132, 'MF'=0>
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
 * | 'version'=4, 'protocol'=1, 'MF'=0>
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
 * | 'version'=4, 'protocol'!=[6|17|132|1], 'MF'=0>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'!=[6|17|44|132|1]>
 */
#define RTE_PTYPE_L4_NONFRAG                0x00000600
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
 * | 'destination port'=4798>
 * or,
 * <'ether type'=0x86DD
 * | 'version'=6, 'next header'=17
 * | 'destination port'=4798>
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
 * | 'version'=4, 'protocol'=6, 'MF'=0>
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
 * | 'version'=4, 'protocol'=17, 'MF'=0>
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
 * | 'version'=4, 'protocol'=132, 'MF'=0>
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
 * | 'version'=4, 'protocol'=1, 'MF'=0>
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
 * | 'version'=4, 'protocol'!=[6|17|132|1], 'MF'=0>
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
 * Check if the (outer) L3 header is IPv4. To avoid comparing IPv4 types one by
 * one, bit 4 is selected to be used for IPv4 only. Then checking bit 4 can
 * determine if it is an IPV4 packet.
 */
#define  RTE_ETH_IS_IPV4_HDR(ptype) ((ptype) & RTE_PTYPE_L3_IPV4)

/**
 * Check if the (outer) L3 header is IPv4. To avoid comparing IPv4 types one by
 * one, bit 6 is selected to be used for IPv4 only. Then checking bit 6 can
 * determine if it is an IPV4 packet.
 */
#define  RTE_ETH_IS_IPV6_HDR(ptype) ((ptype) & RTE_PTYPE_L3_IPV6)

/* Check if it is a tunneling packet */
#define RTE_ETH_IS_TUNNEL_PKT(ptype) ((ptype) & (RTE_PTYPE_TUNNEL_MASK | \
                                                 RTE_PTYPE_INNER_L2_MASK | \
                                                 RTE_PTYPE_INNER_L3_MASK | \
                                                 RTE_PTYPE_INNER_L4_MASK))
#endif /* RTE_NEXT_ABI */

/** Alignment constraint of mbuf private area. */
#define RTE_MBUF_PRIV_ALIGN 8

/**
 * Get the name of a RX offload flag
 *
 * @param mask
 *   The mask describing the flag.
 * @return
 *   The name of this flag, or NULL if it's not a valid RX flag.
 */
const char *rte_get_rx_ol_flag_name(uint64_t mask);

/**
 * Get the name of a TX offload flag
 *
 * @param mask
 *   The mask describing the flag. Usually only one bit must be set.
 *   Several bits can be given if they belong to the same mask.
 *   Ex: PKT_TX_L4_MASK.
 * @return
 *   The name of this flag, or NULL if it's not a valid TX flag.
 */
const char *rte_get_tx_ol_flag_name(uint64_t mask);

/**
 * Some NICs need at least 2KB buffer to RX standard Ethernet frame without
 * splitting it into multiple segments.
 * So, for mbufs that planned to be involved into RX/TX, the recommended
 * minimal buffer length is 2KB + RTE_PKTMBUF_HEADROOM.
 */
#define	RTE_MBUF_DEFAULT_DATAROOM	2048
#define	RTE_MBUF_DEFAULT_BUF_SIZE	\
	(RTE_MBUF_DEFAULT_DATAROOM + RTE_PKTMBUF_HEADROOM)

/* define a set of marker types that can be used to refer to set points in the
 * mbuf */
typedef void    *MARKER[0];   /**< generic marker for a point in a structure */
typedef uint8_t  MARKER8[0];  /**< generic marker with 1B alignment */
typedef uint64_t MARKER64[0]; /**< marker that allows us to overwrite 8 bytes
                               * with a single assignment */

/**
 * The generic rte_mbuf, containing a packet mbuf.
 */
struct rte_mbuf {
	MARKER cacheline0;

	void *buf_addr;           /**< Virtual address of segment buffer. */
	phys_addr_t buf_physaddr; /**< Physical address of segment buffer. */

	uint16_t buf_len;         /**< Length of segment buffer. */

	/* next 6 bytes are initialised on RX descriptor rearm */
	MARKER8 rearm_data;
	uint16_t data_off;

	/**
	 * 16-bit Reference counter.
	 * It should only be accessed using the following functions:
	 * rte_mbuf_refcnt_update(), rte_mbuf_refcnt_read(), and
	 * rte_mbuf_refcnt_set(). The functionality of these functions (atomic,
	 * or non-atomic) is controlled by the CONFIG_RTE_MBUF_REFCNT_ATOMIC
	 * config option.
	 */
	union {
		rte_atomic16_t refcnt_atomic; /**< Atomically accessed refcnt */
		uint16_t refcnt;              /**< Non-atomically accessed refcnt */
	};
	uint8_t nb_segs;          /**< Number of segments. */
	uint8_t port;             /**< Input port. */

	uint64_t ol_flags;        /**< Offload features. */

	/* remaining bytes are set on RX when pulling packet from descriptor */
	MARKER rx_descriptor_fields1;

#ifdef RTE_NEXT_ABI
	/*
	 * The packet type, which is the combination of outer/inner L2, L3, L4
	 * and tunnel types.
	 */
	union {
		uint32_t packet_type; /**< L2/L3/L4 and tunnel information. */
		struct {
			uint32_t l2_type:4; /**< (Outer) L2 type. */
			uint32_t l3_type:4; /**< (Outer) L3 type. */
			uint32_t l4_type:4; /**< (Outer) L4 type. */
			uint32_t tun_type:4; /**< Tunnel type. */
			uint32_t inner_l2_type:4; /**< Inner L2 type. */
			uint32_t inner_l3_type:4; /**< Inner L3 type. */
			uint32_t inner_l4_type:4; /**< Inner L4 type. */
		};
	};

	uint32_t pkt_len;         /**< Total pkt len: sum of all segments. */
	uint16_t data_len;        /**< Amount of data in segment buffer. */
	uint16_t vlan_tci;        /**< VLAN Tag Control Identifier (CPU order) */
#else /* RTE_NEXT_ABI */
	/**
	 * The packet type, which is used to indicate ordinary packet and also
	 * tunneled packet format, i.e. each number is represented a type of
	 * packet.
	 */
	uint16_t packet_type;

	uint16_t data_len;        /**< Amount of data in segment buffer. */
	uint32_t pkt_len;         /**< Total pkt len: sum of all segments. */
	uint16_t vlan_tci;        /**< VLAN Tag Control Identifier (CPU order) */
	uint16_t vlan_tci_outer;  /**< Outer VLAN Tag Control Identifier (CPU order) */
#endif /* RTE_NEXT_ABI */
	union {
		uint32_t rss;     /**< RSS hash result if RSS enabled */
		struct {
			union {
				struct {
					uint16_t hash;
					uint16_t id;
				};
				uint32_t lo;
				/**< Second 4 flexible bytes */
			};
			uint32_t hi;
			/**< First 4 flexible bytes or FD ID, dependent on
			     PKT_RX_FDIR_* flag in ol_flags. */
		} fdir;           /**< Filter identifier if FDIR enabled */
		uint32_t sched;   /**< Hierarchical scheduler */
		uint32_t usr;	  /**< User defined tags. See rte_distributor_process() */
	} hash;                   /**< hash information */

	uint32_t seqn; /**< Sequence number. See also rte_reorder_insert() */
#ifdef RTE_NEXT_ABI
	uint16_t vlan_tci_outer;  /**< Outer VLAN Tag Control Identifier (CPU order) */
#endif /* RTE_NEXT_ABI */

	/* second cache line - fields only used in slow path or on TX */
	MARKER cacheline1 __rte_cache_aligned;

	union {
		void *userdata;   /**< Can be used for external metadata */
		uint64_t udata64; /**< Allow 8-byte userdata on 32-bit */
	};

	struct rte_mempool *pool; /**< Pool from which mbuf was allocated. */
	struct rte_mbuf *next;    /**< Next segment of scattered packet. */

	/* fields to support TX offloads */
	union {
		uint64_t tx_offload;       /**< combined for easy fetch */
		struct {
			uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
			uint64_t l3_len:9; /**< L3 (IP) Header Length. */
			uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
			uint64_t tso_segsz:16; /**< TCP TSO segment size */

			/* fields for TX offloading of tunnels */
			uint64_t outer_l3_len:9; /**< Outer L3 (IP) Hdr Length. */
			uint64_t outer_l2_len:7; /**< Outer L2 (MAC) Hdr Length. */

			/* uint64_t unused:8; */
		};
	};

	/** Size of the application private data. In case of an indirect
	 * mbuf, it stores the direct mbuf private data size. */
	uint16_t priv_size;

	/** Timesync flags for use with IEEE1588. */
	uint16_t timesync;
} __rte_cache_aligned;

static inline uint16_t rte_pktmbuf_priv_size(struct rte_mempool *mp);

/**
 * Return the mbuf owning the data buffer address of an indirect mbuf.
 *
 * @param mi
 *   The pointer to the indirect mbuf.
 * @return
 *   The address of the direct mbuf corresponding to buffer_addr.
 */
static inline struct rte_mbuf *
rte_mbuf_from_indirect(struct rte_mbuf *mi)
{
	return (struct rte_mbuf *)RTE_PTR_SUB(mi->buf_addr, sizeof(*mi) + mi->priv_size);
}

/**
 * Return the buffer address embedded in the given mbuf.
 *
 * @param md
 *   The pointer to the mbuf.
 * @return
 *   The address of the data buffer owned by the mbuf.
 */
static inline char *
rte_mbuf_to_baddr(struct rte_mbuf *md)
{
	char *buffer_addr;
	buffer_addr = (char *)md + sizeof(*md) + rte_pktmbuf_priv_size(md->pool);
	return buffer_addr;
}

/**
 * Returns TRUE if given mbuf is indirect, or FALSE otherwise.
 */
#define RTE_MBUF_INDIRECT(mb)   ((mb)->ol_flags & IND_ATTACHED_MBUF)

/**
 * Returns TRUE if given mbuf is direct, or FALSE otherwise.
 */
#define RTE_MBUF_DIRECT(mb)     (!RTE_MBUF_INDIRECT(mb))

/**
 * Private data in case of pktmbuf pool.
 *
 * A structure that contains some pktmbuf_pool-specific data that are
 * appended after the mempool structure (in private data).
 */
struct rte_pktmbuf_pool_private {
	uint16_t mbuf_data_room_size; /**< Size of data space in each mbuf. */
	uint16_t mbuf_priv_size;      /**< Size of private area in each mbuf. */
};

#ifdef RTE_LIBRTE_MBUF_DEBUG

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, is_h) rte_mbuf_sanity_check(m, is_h)

/**  check mbuf type in debug mode if mbuf pointer is not null */
#define __rte_mbuf_sanity_check_raw(m, is_h)	do {       \
	if ((m) != NULL)                                   \
		rte_mbuf_sanity_check(m, is_h);          \
} while (0)

/**  MBUF asserts in debug mode */
#define RTE_MBUF_ASSERT(exp)                                         \
if (!(exp)) {                                                        \
	rte_panic("line%d\tassert \"" #exp "\" failed\n", __LINE__); \
}

#else /*  RTE_LIBRTE_MBUF_DEBUG */

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, is_h) do { } while (0)

/**  check mbuf type in debug mode if mbuf pointer is not null */
#define __rte_mbuf_sanity_check_raw(m, is_h) do { } while (0)

/**  MBUF asserts in debug mode */
#define RTE_MBUF_ASSERT(exp)                do { } while (0)

#endif /*  RTE_LIBRTE_MBUF_DEBUG */

#ifdef RTE_MBUF_REFCNT_ATOMIC

/**
 * Reads the value of an mbuf's refcnt.
 * @param m
 *   Mbuf to read
 * @return
 *   Reference count number.
 */
static inline uint16_t
rte_mbuf_refcnt_read(const struct rte_mbuf *m)
{
	return (uint16_t)(rte_atomic16_read(&m->refcnt_atomic));
}

/**
 * Sets an mbuf's refcnt to a defined value.
 * @param m
 *   Mbuf to update
 * @param new_value
 *   Value set
 */
static inline void
rte_mbuf_refcnt_set(struct rte_mbuf *m, uint16_t new_value)
{
	rte_atomic16_set(&m->refcnt_atomic, new_value);
}

/**
 * Adds given value to an mbuf's refcnt and returns its new value.
 * @param m
 *   Mbuf to update
 * @param value
 *   Value to add/subtract
 * @return
 *   Updated value
 */
static inline uint16_t
rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	/*
	 * The atomic_add is an expensive operation, so we don't want to
	 * call it in the case where we know we are the uniq holder of
	 * this mbuf (i.e. ref_cnt == 1). Otherwise, an atomic
	 * operation has to be used because concurrent accesses on the
	 * reference counter can occur.
	 */
	if (likely(rte_mbuf_refcnt_read(m) == 1)) {
		rte_mbuf_refcnt_set(m, 1 + value);
		return 1 + value;
	}

	return (uint16_t)(rte_atomic16_add_return(&m->refcnt_atomic, value));
}

#else /* ! RTE_MBUF_REFCNT_ATOMIC */

/**
 * Adds given value to an mbuf's refcnt and returns its new value.
 */
static inline uint16_t
rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	m->refcnt = (uint16_t)(m->refcnt + value);
	return m->refcnt;
}

/**
 * Reads the value of an mbuf's refcnt.
 */
static inline uint16_t
rte_mbuf_refcnt_read(const struct rte_mbuf *m)
{
	return m->refcnt;
}

/**
 * Sets an mbuf's refcnt to the defined value.
 */
static inline void
rte_mbuf_refcnt_set(struct rte_mbuf *m, uint16_t new_value)
{
	m->refcnt = new_value;
}

#endif /* RTE_MBUF_REFCNT_ATOMIC */

/** Mbuf prefetch */
#define RTE_MBUF_PREFETCH_TO_FREE(m) do {       \
	if ((m) != NULL)                        \
		rte_prefetch0(m);               \
} while (0)


/**
 * Sanity checks on an mbuf.
 *
 * Check the consistency of the given mbuf. The function will cause a
 * panic if corruption is detected.
 *
 * @param m
 *   The mbuf to be checked.
 * @param is_header
 *   True if the mbuf is a packet header, false if it is a sub-segment
 *   of a packet (in this case, some fields like nb_segs are not checked)
 */
void
rte_mbuf_sanity_check(const struct rte_mbuf *m, int is_header);

/**
 * @internal Allocate a new mbuf from mempool *mp*.
 * The use of that function is reserved for RTE internal needs.
 * Please use rte_pktmbuf_alloc().
 *
 * @param mp
 *   The mempool from which mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
static inline struct rte_mbuf *__rte_mbuf_raw_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;
	void *mb = NULL;
	if (rte_mempool_get(mp, &mb) < 0)
		return NULL;
	m = (struct rte_mbuf *)mb;
	RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
	rte_mbuf_refcnt_set(m, 1);
	return m;
}

/**
 * @internal Put mbuf back into its original mempool.
 * The use of that function is reserved for RTE internal needs.
 * Please use rte_pktmbuf_free().
 *
 * @param m
 *   The mbuf to be freed.
 */
static inline void __attribute__((always_inline))
__rte_mbuf_raw_free(struct rte_mbuf *m)
{
	RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
	rte_mempool_put(m->pool, m);
}

/* Operations on ctrl mbuf */

/**
 * The control mbuf constructor.
 *
 * This function initializes some fields in an mbuf structure that are
 * not modified by the user once created (mbuf type, origin pool, buffer
 * start address, and so on). This function is given as a callback function
 * to rte_mempool_create() at pool creation time.
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer comes from the ``init_arg``
 *   parameter of rte_mempool_create().
 * @param m
 *   The mbuf to initialize.
 * @param i
 *   The index of the mbuf in the pool table.
 */
void rte_ctrlmbuf_init(struct rte_mempool *mp, void *opaque_arg,
		void *m, unsigned i);

/**
 * Allocate a new mbuf (type is ctrl) from mempool *mp*.
 *
 * This new mbuf is initialized with data pointing to the beginning of
 * buffer, and with a length of zero.
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
#define rte_ctrlmbuf_alloc(mp) rte_pktmbuf_alloc(mp)

/**
 * Free a control mbuf back into its original mempool.
 *
 * @param m
 *   The control mbuf to be freed.
 */
#define rte_ctrlmbuf_free(m) rte_pktmbuf_free(m)

/**
 * A macro that returns the pointer to the carried data.
 *
 * The value that can be read or assigned.
 *
 * @param m
 *   The control mbuf.
 */
#define rte_ctrlmbuf_data(m) ((char *)((m)->buf_addr) + (m)->data_off)

/**
 * A macro that returns the length of the carried data.
 *
 * The value that can be read or assigned.
 *
 * @param m
 *   The control mbuf.
 */
#define rte_ctrlmbuf_len(m) rte_pktmbuf_data_len(m)

/**
 * Tests if an mbuf is a control mbuf
 *
 * @param m
 *   The mbuf to be tested
 * @return
 *   - True (1) if the mbuf is a control mbuf
 *   - False(0) otherwise
 */
static inline int
rte_is_ctrlmbuf(struct rte_mbuf *m)
{
	return !!(m->ol_flags & CTRL_MBUF_FLAG);
}

/* Operations on pkt mbuf */

/**
 * The packet mbuf constructor.
 *
 * This function initializes some fields in the mbuf structure that are
 * not modified by the user once created (origin pool, buffer start
 * address, and so on). This function is given as a callback function to
 * rte_mempool_create() at pool creation time.
 *
 * @param mp
 *   The mempool from which mbufs originate.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer comes from the ``init_arg``
 *   parameter of rte_mempool_create().
 * @param m
 *   The mbuf to initialize.
 * @param i
 *   The index of the mbuf in the pool table.
 */
void rte_pktmbuf_init(struct rte_mempool *mp, void *opaque_arg,
		      void *m, unsigned i);


/**
 * A  packet mbuf pool constructor.
 *
 * This function initializes the mempool private data in the case of a
 * pktmbuf pool. This private data is needed by the driver. The
 * function is given as a callback function to rte_mempool_create() at
 * pool creation. It can be extended by the user, for example, to
 * provide another packet size.
 *
 * @param mp
 *   The mempool from which mbufs originate.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer comes from the ``init_arg``
 *   parameter of rte_mempool_create().
 */
void rte_pktmbuf_pool_init(struct rte_mempool *mp, void *opaque_arg);

/**
 * Create a mbuf pool.
 *
 * This function creates and initializes a packet mbuf pool. It is
 * a wrapper to rte_mempool_create() with the proper packet constructor
 * and mempool constructor.
 *
 * @param name
 *   The name of the mbuf pool.
 * @param n
 *   The number of elements in the mbuf pool. The optimum size (in terms
 *   of memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param cache_size
 *   Size of the per-core object cache. See rte_mempool_create() for
 *   details.
 * @param priv_size
 *   Size of application private are between the rte_mbuf structure
 *   and the data buffer. This value must be aligned to RTE_MBUF_PRIV_ALIGN.
 * @param data_room_size
 *   Size of data buffer in each mbuf, including RTE_PKTMBUF_HEADROOM.
 * @param socket_id
 *   The socket identifier where the memory should be allocated. The
 *   value can be *SOCKET_ID_ANY* if there is no NUMA constraint for the
 *   reserved zone.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - cache size provided is too large, or priv_size is not aligned.
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_mempool *
rte_pktmbuf_pool_create(const char *name, unsigned n,
	unsigned cache_size, uint16_t priv_size, uint16_t data_room_size,
	int socket_id);

/**
 * Get the data room size of mbufs stored in a pktmbuf_pool
 *
 * The data room size is the amount of data that can be stored in a
 * mbuf including the headroom (RTE_PKTMBUF_HEADROOM).
 *
 * @param mp
 *   The packet mbuf pool.
 * @return
 *   The data room size of mbufs stored in this mempool.
 */
static inline uint16_t
rte_pktmbuf_data_room_size(struct rte_mempool *mp)
{
	struct rte_pktmbuf_pool_private *mbp_priv;

	mbp_priv = (struct rte_pktmbuf_pool_private *)rte_mempool_get_priv(mp);
	return mbp_priv->mbuf_data_room_size;
}

/**
 * Get the application private size of mbufs stored in a pktmbuf_pool
 *
 * The private size of mbuf is a zone located between the rte_mbuf
 * structure and the data buffer where an application can store data
 * associated to a packet.
 *
 * @param mp
 *   The packet mbuf pool.
 * @return
 *   The private size of mbufs stored in this mempool.
 */
static inline uint16_t
rte_pktmbuf_priv_size(struct rte_mempool *mp)
{
	struct rte_pktmbuf_pool_private *mbp_priv;

	mbp_priv = (struct rte_pktmbuf_pool_private *)rte_mempool_get_priv(mp);
	return mbp_priv->mbuf_priv_size;
}

/**
 * Reset the fields of a packet mbuf to their default values.
 *
 * The given mbuf must have only one segment.
 *
 * @param m
 *   The packet mbuf to be resetted.
 */
static inline void rte_pktmbuf_reset(struct rte_mbuf *m)
{
	m->next = NULL;
	m->pkt_len = 0;
	m->tx_offload = 0;
	m->vlan_tci = 0;
	m->vlan_tci_outer = 0;
	m->nb_segs = 1;
	m->port = 0xff;

	m->ol_flags = 0;
	m->packet_type = 0;
	m->data_off = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
			RTE_PKTMBUF_HEADROOM : m->buf_len;

	m->data_len = 0;
	__rte_mbuf_sanity_check(m, 1);
}

/**
 * Allocate a new mbuf from a mempool.
 *
 * This new mbuf contains one segment, which has a length of 0. The pointer
 * to data is initialized to have some bytes of headroom in the buffer
 * (if buffer size allows).
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
static inline struct rte_mbuf *rte_pktmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;
	if ((m = __rte_mbuf_raw_alloc(mp)) != NULL)
		rte_pktmbuf_reset(m);
	return m;
}

/**
 * Attach packet mbuf to another packet mbuf.
 *
 * After attachment we refer the mbuf we attached as 'indirect',
 * while mbuf we attached to as 'direct'.
 * Right now, not supported:
 *  - attachment for already indirect mbuf (e.g. - mi has to be direct).
 *  - mbuf we trying to attach (mi) is used by someone else
 *    e.g. it's reference counter is greater then 1.
 *
 * @param mi
 *   The indirect packet mbuf.
 * @param m
 *   The packet mbuf we're attaching to.
 */
static inline void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *m)
{
	struct rte_mbuf *md;

	RTE_MBUF_ASSERT(RTE_MBUF_DIRECT(mi) &&
	    rte_mbuf_refcnt_read(mi) == 1);

	/* if m is not direct, get the mbuf that embeds the data */
	if (RTE_MBUF_DIRECT(m))
		md = m;
	else
		md = rte_mbuf_from_indirect(m);

	rte_mbuf_refcnt_update(md, 1);
	mi->priv_size = m->priv_size;
	mi->buf_physaddr = m->buf_physaddr;
	mi->buf_addr = m->buf_addr;
	mi->buf_len = m->buf_len;

	mi->next = m->next;
	mi->data_off = m->data_off;
	mi->data_len = m->data_len;
	mi->port = m->port;
	mi->vlan_tci = m->vlan_tci;
	mi->vlan_tci_outer = m->vlan_tci_outer;
	mi->tx_offload = m->tx_offload;
	mi->hash = m->hash;

	mi->next = NULL;
	mi->pkt_len = mi->data_len;
	mi->nb_segs = 1;
	mi->ol_flags = m->ol_flags | IND_ATTACHED_MBUF;
	mi->packet_type = m->packet_type;

	__rte_mbuf_sanity_check(mi, 1);
	__rte_mbuf_sanity_check(m, 0);
}

/**
 * Detach an indirect packet mbuf.
 *
 *  - restore original mbuf address and length values.
 *  - reset pktmbuf data and data_len to their default values.
 *  All other fields of the given packet mbuf will be left intact.
 *
 * @param m
 *   The indirect attached packet mbuf.
 */
static inline void rte_pktmbuf_detach(struct rte_mbuf *m)
{
	struct rte_mempool *mp = m->pool;
	uint32_t mbuf_size, buf_len, priv_size;

	priv_size = rte_pktmbuf_priv_size(mp);
	mbuf_size = sizeof(struct rte_mbuf) + priv_size;
	buf_len = rte_pktmbuf_data_room_size(mp);

	m->priv_size = priv_size;
	m->buf_addr = (char *)m + mbuf_size;
	m->buf_physaddr = rte_mempool_virt2phy(mp, m) + mbuf_size;
	m->buf_len = (uint16_t)buf_len;
	m->data_off = RTE_MIN(RTE_PKTMBUF_HEADROOM, (uint16_t)m->buf_len);
	m->data_len = 0;
	m->ol_flags = 0;
}

static inline struct rte_mbuf* __attribute__((always_inline))
__rte_pktmbuf_prefree_seg(struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 0);

	if (likely(rte_mbuf_refcnt_update(m, -1) == 0)) {

		/* if this is an indirect mbuf, then
		 *  - detach mbuf
		 *  - free attached mbuf segment
		 */
		if (RTE_MBUF_INDIRECT(m)) {
			struct rte_mbuf *md = rte_mbuf_from_indirect(m);
			rte_pktmbuf_detach(m);
			if (rte_mbuf_refcnt_update(md, -1) == 0)
				__rte_mbuf_raw_free(md);
		}
		return m;
	}
	return NULL;
}

/**
 * Free a segment of a packet mbuf into its original mempool.
 *
 * Free an mbuf, without parsing other segments in case of chained
 * buffers.
 *
 * @param m
 *   The packet mbuf segment to be freed.
 */
static inline void __attribute__((always_inline))
rte_pktmbuf_free_seg(struct rte_mbuf *m)
{
	if (likely(NULL != (m = __rte_pktmbuf_prefree_seg(m)))) {
		m->next = NULL;
		__rte_mbuf_raw_free(m);
	}
}

/**
 * Free a packet mbuf back into its original mempool.
 *
 * Free an mbuf, and all its segments in case of chained buffers. Each
 * segment is added back into its original mempool.
 *
 * @param m
 *   The packet mbuf to be freed.
 */
static inline void rte_pktmbuf_free(struct rte_mbuf *m)
{
	struct rte_mbuf *m_next;

	__rte_mbuf_sanity_check(m, 1);

	while (m != NULL) {
		m_next = m->next;
		rte_pktmbuf_free_seg(m);
		m = m_next;
	}
}

/**
 * Creates a "clone" of the given packet mbuf.
 *
 * Walks through all segments of the given packet mbuf, and for each of them:
 *  - Creates a new packet mbuf from the given pool.
 *  - Attaches newly created mbuf to the segment.
 * Then updates pkt_len and nb_segs of the "clone" packet mbuf to match values
 * from the original packet mbuf.
 *
 * @param md
 *   The packet mbuf to be cloned.
 * @param mp
 *   The mempool from which the "clone" mbufs are allocated.
 * @return
 *   - The pointer to the new "clone" mbuf on success.
 *   - NULL if allocation fails.
 */
static inline struct rte_mbuf *rte_pktmbuf_clone(struct rte_mbuf *md,
		struct rte_mempool *mp)
{
	struct rte_mbuf *mc, *mi, **prev;
	uint32_t pktlen;
	uint8_t nseg;

	if (unlikely ((mc = rte_pktmbuf_alloc(mp)) == NULL))
		return NULL;

	mi = mc;
	prev = &mi->next;
	pktlen = md->pkt_len;
	nseg = 0;

	do {
		nseg++;
		rte_pktmbuf_attach(mi, md);
		*prev = mi;
		prev = &mi->next;
	} while ((md = md->next) != NULL &&
	    (mi = rte_pktmbuf_alloc(mp)) != NULL);

	*prev = NULL;
	mc->nb_segs = nseg;
	mc->pkt_len = pktlen;

	/* Allocation of new indirect segment failed */
	if (unlikely (mi == NULL)) {
		rte_pktmbuf_free(mc);
		return NULL;
	}

	__rte_mbuf_sanity_check(mc, 1);
	return mc;
}

/**
 * Adds given value to the refcnt of all packet mbuf segments.
 *
 * Walks through all segments of given packet mbuf and for each of them
 * invokes rte_mbuf_refcnt_update().
 *
 * @param m
 *   The packet mbuf whose refcnt to be updated.
 * @param v
 *   The value to add to the mbuf's segments refcnt.
 */
static inline void rte_pktmbuf_refcnt_update(struct rte_mbuf *m, int16_t v)
{
	__rte_mbuf_sanity_check(m, 1);

	do {
		rte_mbuf_refcnt_update(m, v);
	} while ((m = m->next) != NULL);
}

/**
 * Get the headroom in a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the headroom.
 */
static inline uint16_t rte_pktmbuf_headroom(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	return m->data_off;
}

/**
 * Get the tailroom of a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the tailroom.
 */
static inline uint16_t rte_pktmbuf_tailroom(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	return (uint16_t)(m->buf_len - rte_pktmbuf_headroom(m) -
			  m->data_len);
}

/**
 * Get the last segment of the packet.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The last segment of the given mbuf.
 */
static inline struct rte_mbuf *rte_pktmbuf_lastseg(struct rte_mbuf *m)
{
	struct rte_mbuf *m2 = (struct rte_mbuf *)m;

	__rte_mbuf_sanity_check(m, 1);
	while (m2->next != NULL)
		m2 = m2->next;
	return m2;
}

/**
 * A macro that points to an offset into the data in the mbuf.
 *
 * The returned pointer is cast to type t. Before using this
 * function, the user must ensure that the first segment is large
 * enough to accommodate its data.
 *
 * @param m
 *   The packet mbuf.
 * @param o
 *   The offset into the mbuf data.
 * @param t
 *   The type to cast the result into.
 */
#define rte_pktmbuf_mtod_offset(m, t, o)	\
	((t)((char *)(m)->buf_addr + (m)->data_off + (o)))

/**
 * A macro that points to the start of the data in the mbuf.
 *
 * The returned pointer is cast to type t. Before using this
 * function, the user must ensure that the first segment is large
 * enough to accommodate its data.
 *
 * @param m
 *   The packet mbuf.
 * @param t
 *   The type to cast the result into.
 */
#define rte_pktmbuf_mtod(m, t) rte_pktmbuf_mtod_offset(m, t, 0)

/**
 * A macro that returns the length of the packet.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_pkt_len(m) ((m)->pkt_len)

/**
 * A macro that returns the length of the segment.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_data_len(m) ((m)->data_len)

/**
 * Prepend len bytes to an mbuf data area.
 *
 * Returns a pointer to the new
 * data start address. If there is not enough headroom in the first
 * segment, the function will return NULL, without modifying the mbuf.
 *
 * @param m
 *   The pkt mbuf.
 * @param len
 *   The amount of data to prepend (in bytes).
 * @return
 *   A pointer to the start of the newly prepended data, or
 *   NULL if there is not enough headroom space in the first segment
 */
static inline char *rte_pktmbuf_prepend(struct rte_mbuf *m,
					uint16_t len)
{
	__rte_mbuf_sanity_check(m, 1);

	if (unlikely(len > rte_pktmbuf_headroom(m)))
		return NULL;

	m->data_off -= len;
	m->data_len = (uint16_t)(m->data_len + len);
	m->pkt_len  = (m->pkt_len + len);

	return (char *)m->buf_addr + m->data_off;
}

/**
 * Append len bytes to an mbuf.
 *
 * Append len bytes to an mbuf and return a pointer to the start address
 * of the added data. If there is not enough tailroom in the last
 * segment, the function will return NULL, without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to append (in bytes).
 * @return
 *   A pointer to the start of the newly appended data, or
 *   NULL if there is not enough tailroom space in the last segment
 */
static inline char *rte_pktmbuf_append(struct rte_mbuf *m, uint16_t len)
{
	void *tail;
	struct rte_mbuf *m_last;

	__rte_mbuf_sanity_check(m, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > rte_pktmbuf_tailroom(m_last)))
		return NULL;

	tail = (char *)m_last->buf_addr + m_last->data_off + m_last->data_len;
	m_last->data_len = (uint16_t)(m_last->data_len + len);
	m->pkt_len  = (m->pkt_len + len);
	return (char*) tail;
}

/**
 * Remove len bytes at the beginning of an mbuf.
 *
 * Returns a pointer to the start address of the new data area. If the
 * length is greater than the length of the first segment, then the
 * function will fail and return NULL, without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to remove (in bytes).
 * @return
 *   A pointer to the new start of the data.
 */
static inline char *rte_pktmbuf_adj(struct rte_mbuf *m, uint16_t len)
{
	__rte_mbuf_sanity_check(m, 1);

	if (unlikely(len > m->data_len))
		return NULL;

	m->data_len = (uint16_t)(m->data_len - len);
	m->data_off += len;
	m->pkt_len  = (m->pkt_len - len);
	return (char *)m->buf_addr + m->data_off;
}

/**
 * Remove len bytes of data at the end of the mbuf.
 *
 * If the length is greater than the length of the last segment, the
 * function will fail and return -1 without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to remove (in bytes).
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static inline int rte_pktmbuf_trim(struct rte_mbuf *m, uint16_t len)
{
	struct rte_mbuf *m_last;

	__rte_mbuf_sanity_check(m, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > m_last->data_len))
		return -1;

	m_last->data_len = (uint16_t)(m_last->data_len - len);
	m->pkt_len  = (m->pkt_len - len);
	return 0;
}

/**
 * Test if mbuf data is contiguous.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   - 1, if all data is contiguous (one segment).
 *   - 0, if there is several segments.
 */
static inline int rte_pktmbuf_is_contiguous(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	return !!(m->nb_segs == 1);
}

/**
 * Dump an mbuf structure to the console.
 *
 * Dump all fields for the given packet mbuf and all its associated
 * segments (in the case of a chained buffer).
 *
 * @param f
 *   A pointer to a file for output
 * @param m
 *   The packet mbuf.
 * @param dump_len
 *   If dump_len != 0, also dump the "dump_len" first data bytes of
 *   the packet.
 */
void rte_pktmbuf_dump(FILE *f, const struct rte_mbuf *m, unsigned dump_len);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_H_ */
