/*   BSD LICENSE
 *
 *   Copyright(c) 2013 6WIND.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef _RTE_ARP_H_
#define _RTE_ARP_H_

/**
 * @file
 *
 * ARP-related defines
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ARP header IPv4 payload.
 */
struct arp_ipv4 {
	uint8_t  arp_sha[6]; /* sender hardware address */
	uint8_t  arp_sip[4]; /* sender IP address */
	uint8_t  arp_tha[6]; /* target hardware address */
	uint8_t  arp_tip[4]; /* target IP address */
} __attribute__((__packed__));

/**
 * ARP header.
 */
struct arp_hdr {
	uint16_t arp_hrd;    /* format of hardware address */
#define ARP_HRD_ETHER     1  /* ARP Ethernet address format */

	uint16_t arp_pro;    /* format of protocol address */
	uint8_t  arp_hln;    /* length of hardware address */
	uint8_t  arp_pln;    /* length of protocol address */
	uint16_t arp_op;     /* ARP opcode (command) */
#define	ARP_OP_REQUEST    1 /* request to resolve address */
#define	ARP_OP_REPLY      2 /* response to previous request */
#define	ARP_OP_REVREQUEST 3 /* request proto addr given hardware */
#define	ARP_OP_REVREPLY   4 /* response giving protocol address */
#define	ARP_OP_INVREQUEST 8 /* request to identify peer */
#define	ARP_OP_INVREPLY   9 /* response identifying peer */

	union {
		struct arp_ipv4 arp_ip;
	} arp_data;
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ARP_H_ */
