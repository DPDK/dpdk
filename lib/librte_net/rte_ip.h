/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)in.h        8.3 (Berkeley) 1/3/94
 * $FreeBSD: src/sys/netinet/in.h,v 1.82 2003/10/25 09:37:10 ume Exp $
 */

#ifndef _RTE_IP_H_
#define _RTE_IP_H_

/**
 * @file
 *
 * IP-related defines
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IPv4 Header
 */
struct ipv4_hdr {
	uint8_t  version_ihl;		/**< version and header length */
	uint8_t  type_of_service;	/**< type of service */
	uint16_t total_length;		/**< length of packet */
	uint16_t packet_id;		/**< packet ID */
	uint16_t fragment_offset;	/**< fragmentation offset */
	uint8_t  time_to_live;		/**< time to live */
	uint8_t  next_proto_id;		/**< protocol ID */
	uint16_t hdr_checksum;		/**< header checksum */
	uint32_t src_addr;		/**< source address */
	uint32_t dst_addr;		/**< destination address */
} __attribute__((__packed__));

/** Create IPv4 address */
#define IPv4(a,b,c,d) ((uint32_t)(((a) & 0xff) << 24) | \
					   (((b) & 0xff) << 16) | \
					   (((c) & 0xff) << 8)  | \
					   ((d) & 0xff))

/* Fragment Offset * Flags. */
#define	IPV4_HDR_DF_SHIFT	14
#define	IPV4_HDR_MF_SHIFT	13
#define	IPV4_HDR_FO_SHIFT	3

#define	IPV4_HDR_DF_FLAG	(1 << IPV4_HDR_DF_SHIFT)
#define	IPV4_HDR_MF_FLAG	(1 << IPV4_HDR_MF_SHIFT)

#define	IPV4_HDR_OFFSET_MASK	((1 << IPV4_HDR_MF_SHIFT) - 1)

#define	IPV4_HDR_OFFSET_UNITS	8

/* IPv4 protocols */
#define IPPROTO_IP         0  /**< dummy for IP */
#define IPPROTO_HOPOPTS    0  /**< IP6 hop-by-hop options */
#define IPPROTO_ICMP       1  /**< control message protocol */
#define IPPROTO_IGMP       2  /**< group mgmt protocol */
#define IPPROTO_GGP        3  /**< gateway^2 (deprecated) */
#define IPPROTO_IPV4       4  /**< IPv4 encapsulation */
#define IPPROTO_TCP        6  /**< tcp */
#define IPPROTO_ST         7  /**< Stream protocol II */
#define IPPROTO_EGP        8  /**< exterior gateway protocol */
#define IPPROTO_PIGP       9  /**< private interior gateway */
#define IPPROTO_RCCMON    10  /**< BBN RCC Monitoring */
#define IPPROTO_NVPII     11  /**< network voice protocol*/
#define IPPROTO_PUP       12  /**< pup */
#define IPPROTO_ARGUS     13  /**< Argus */
#define IPPROTO_EMCON     14  /**< EMCON */
#define IPPROTO_XNET      15  /**< Cross Net Debugger */
#define IPPROTO_CHAOS     16  /**< Chaos*/
#define IPPROTO_UDP       17  /**< user datagram protocol */
#define IPPROTO_MUX       18  /**< Multiplexing */
#define IPPROTO_MEAS      19  /**< DCN Measurement Subsystems */
#define IPPROTO_HMP       20  /**< Host Monitoring */
#define IPPROTO_PRM       21  /**< Packet Radio Measurement */
#define IPPROTO_IDP       22  /**< xns idp */
#define IPPROTO_TRUNK1    23  /**< Trunk-1 */
#define IPPROTO_TRUNK2    24  /**< Trunk-2 */
#define IPPROTO_LEAF1     25  /**< Leaf-1 */
#define IPPROTO_LEAF2     26  /**< Leaf-2 */
#define IPPROTO_RDP       27  /**< Reliable Data */
#define IPPROTO_IRTP      28  /**< Reliable Transaction */
#define IPPROTO_TP        29  /**< tp-4 w/ class negotiation */
#define IPPROTO_BLT       30  /**< Bulk Data Transfer */
#define IPPROTO_NSP       31  /**< Network Services */
#define IPPROTO_INP       32  /**< Merit Internodal */
#define IPPROTO_SEP       33  /**< Sequential Exchange */
#define IPPROTO_3PC       34  /**< Third Party Connect */
#define IPPROTO_IDPR      35  /**< InterDomain Policy Routing */
#define IPPROTO_XTP       36  /**< XTP */
#define IPPROTO_DDP       37  /**< Datagram Delivery */
#define IPPROTO_CMTP      38  /**< Control Message Transport */
#define IPPROTO_TPXX      39  /**< TP++ Transport */
#define IPPROTO_IL        40  /**< IL transport protocol */
#define IPPROTO_IPV6      41  /**< IP6 header */
#define IPPROTO_SDRP      42  /**< Source Demand Routing */
#define IPPROTO_ROUTING   43  /**< IP6 routing header */
#define IPPROTO_FRAGMENT  44  /**< IP6 fragmentation header */
#define IPPROTO_IDRP      45  /**< InterDomain Routing*/
#define IPPROTO_RSVP      46  /**< resource reservation */
#define IPPROTO_GRE       47  /**< General Routing Encap. */
#define IPPROTO_MHRP      48  /**< Mobile Host Routing */
#define IPPROTO_BHA       49  /**< BHA */
#define IPPROTO_ESP       50  /**< IP6 Encap Sec. Payload */
#define IPPROTO_AH        51  /**< IP6 Auth Header */
#define IPPROTO_INLSP     52  /**< Integ. Net Layer Security */
#define IPPROTO_SWIPE     53  /**< IP with encryption */
#define IPPROTO_NHRP      54  /**< Next Hop Resolution */
/* 55-57: Unassigned */
#define IPPROTO_ICMPV6    58  /**< ICMP6 */
#define IPPROTO_NONE      59  /**< IP6 no next header */
#define IPPROTO_DSTOPTS   60  /**< IP6 destination option */
#define IPPROTO_AHIP      61  /**< any host internal protocol */
#define IPPROTO_CFTP      62  /**< CFTP */
#define IPPROTO_HELLO     63  /**< "hello" routing protocol */
#define IPPROTO_SATEXPAK  64  /**< SATNET/Backroom EXPAK */
#define IPPROTO_KRYPTOLAN 65  /**< Kryptolan */
#define IPPROTO_RVD       66  /**< Remote Virtual Disk */
#define IPPROTO_IPPC      67  /**< Pluribus Packet Core */
#define IPPROTO_ADFS      68  /**< Any distributed FS */
#define IPPROTO_SATMON    69  /**< Satnet Monitoring */
#define IPPROTO_VISA      70  /**< VISA Protocol */
#define IPPROTO_IPCV      71  /**< Packet Core Utility */
#define IPPROTO_CPNX      72  /**< Comp. Prot. Net. Executive */
#define IPPROTO_CPHB      73  /**< Comp. Prot. HeartBeat */
#define IPPROTO_WSN       74  /**< Wang Span Network */
#define IPPROTO_PVP       75  /**< Packet Video Protocol */
#define IPPROTO_BRSATMON  76  /**< BackRoom SATNET Monitoring */
#define IPPROTO_ND        77  /**< Sun net disk proto (temp.) */
#define IPPROTO_WBMON     78  /**< WIDEBAND Monitoring */
#define IPPROTO_WBEXPAK   79  /**< WIDEBAND EXPAK */
#define IPPROTO_EON       80  /**< ISO cnlp */
#define IPPROTO_VMTP      81  /**< VMTP */
#define IPPROTO_SVMTP     82  /**< Secure VMTP */
#define IPPROTO_VINES     83  /**< Banyon VINES */
#define IPPROTO_TTP       84  /**< TTP */
#define IPPROTO_IGP       85  /**< NSFNET-IGP */
#define IPPROTO_DGP       86  /**< dissimilar gateway prot. */
#define IPPROTO_TCF       87  /**< TCF */
#define IPPROTO_IGRP      88  /**< Cisco/GXS IGRP */
#define IPPROTO_OSPFIGP   89  /**< OSPFIGP */
#define IPPROTO_SRPC      90  /**< Strite RPC protocol */
#define IPPROTO_LARP      91  /**< Locus Address Resoloution */
#define IPPROTO_MTP       92  /**< Multicast Transport */
#define IPPROTO_AX25      93  /**< AX.25 Frames */
#define IPPROTO_IPEIP     94  /**< IP encapsulated in IP */
#define IPPROTO_MICP      95  /**< Mobile Int.ing control */
#define IPPROTO_SCCSP     96  /**< Semaphore Comm. security */
#define IPPROTO_ETHERIP   97  /**< Ethernet IP encapsulation */
#define IPPROTO_ENCAP     98  /**< encapsulation header */
#define IPPROTO_APES      99  /**< any private encr. scheme */
#define IPPROTO_GMTP     100  /**< GMTP */
#define IPPROTO_IPCOMP   108  /**< payload compression (IPComp) */
/* 101-254: Partly Unassigned */
#define IPPROTO_PIM      103  /**< Protocol Independent Mcast */
#define IPPROTO_PGM      113  /**< PGM */
#define IPPROTO_SCTP     132  /**< Stream Control Transport Protocol */
/* 255: Reserved */
/* BSD Private, local use, namespace incursion */
#define IPPROTO_DIVERT   254  /**< divert pseudo-protocol */
#define IPPROTO_RAW      255  /**< raw IP packet */
#define IPPROTO_MAX      256  /**< maximum protocol number */

/*
 * IPv4 address types
 */
#define IPV4_ANY              ((uint32_t)0x00000000) /**< 0.0.0.0 */
#define IPV4_LOOPBACK         ((uint32_t)0x7f000001) /**< 127.0.0.1 */
#define IPV4_BROADCAST        ((uint32_t)0xe0000000) /**< 224.0.0.0 */
#define IPV4_ALLHOSTS_GROUP   ((uint32_t)0xe0000001) /**< 224.0.0.1 */
#define IPV4_ALLRTRS_GROUP    ((uint32_t)0xe0000002) /**< 224.0.0.2 */
#define IPV4_MAX_LOCAL_GROUP  ((uint32_t)0xe00000ff) /**< 224.0.0.255 */

/*
 * IPv4 Multicast-related macros
 */
#define IPV4_MIN_MCAST  IPv4(224, 0, 0, 0)          /**< Minimal IPv4-multicast address */
#define IPV4_MAX_MCAST  IPv4(239, 255, 255, 255)    /**< Maximum IPv4 multicast address */

#define IS_IPV4_MCAST(x) \
	((x) >= IPV4_MIN_MCAST && (x) <= IPV4_MAX_MCAST) /**< check if IPv4 address is multicast */

/**
 * IPv6 Header
 */
struct ipv6_hdr {
	uint32_t vtc_flow;     /**< IP version, traffic class & flow label. */
	uint16_t payload_len;  /**< IP packet length - includes sizeof(ip_header). */
	uint8_t  proto;        /**< Protocol, next header. */
	uint8_t  hop_limits;   /**< Hop limits. */
	uint8_t  src_addr[16]; /**< IP address of source host. */
	uint8_t  dst_addr[16]; /**< IP address of destination host(s). */
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IP_H_ */
