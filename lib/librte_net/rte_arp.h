/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013 6WIND S.A.
 */

#ifndef _RTE_ARP_H_
#define _RTE_ARP_H_

/**
 * @file
 *
 * ARP-related defines
 */

#include <stdint.h>
#include <rte_ether.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ARP header IPv4 payload.
 */
struct arp_ipv4 {
	struct ether_addr arp_sha;  /**< sender hardware address */
	uint32_t          arp_sip;  /**< sender IP address */
	struct ether_addr arp_tha;  /**< target hardware address */
	uint32_t          arp_tip;  /**< target IP address */
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

	struct arp_ipv4 arp_data;
} __attribute__((__packed__));

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Make a RARP packet based on MAC addr.
 *
 * @param mpool
 *   Pointer to the rte_mempool
 * @param mac
 *   Pointer to the MAC addr
 *
 * @return
 *   - RARP packet pointer on success, or NULL on error
 */
struct rte_mbuf * __rte_experimental
rte_net_make_rarp_packet(struct rte_mempool *mpool,
		const struct ether_addr *mac);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ARP_H_ */
