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
struct rte_arp_ipv4 {
	struct rte_ether_addr arp_sha;  /**< sender hardware address */
	rte_be32_t            arp_sip;  /**< sender IP address */
	struct rte_ether_addr arp_tha;  /**< target hardware address */
	rte_be32_t            arp_tip;  /**< target IP address */
} __rte_packed __rte_aligned(2);

/**
 * ARP header.
 */
struct rte_arp_hdr {
	rte_be16_t arp_hardware; /**< format of hardware address */
#define RTE_ARP_HRD_ETHER     1  /**< ARP Ethernet address format */

	rte_be16_t arp_protocol; /**< format of protocol address */
	uint8_t    arp_hlen;     /**< length of hardware address */
	uint8_t    arp_plen;     /**< length of protocol address */
	rte_be16_t arp_opcode;   /**< ARP opcode (command) */
#define	RTE_ARP_OP_REQUEST    1  /**< request to resolve address */
#define	RTE_ARP_OP_REPLY      2  /**< response to previous request */
#define	RTE_ARP_OP_REVREQUEST 3  /**< request proto addr given hardware */
#define	RTE_ARP_OP_REVREPLY   4  /**< response giving protocol address */
#define	RTE_ARP_OP_INVREQUEST 8  /**< request to identify peer */
#define	RTE_ARP_OP_INVREPLY   9  /**< response identifying peer */

	struct rte_arp_ipv4 arp_data;
} __rte_packed __rte_aligned(2);

/**
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
struct rte_mbuf *
rte_net_make_rarp_packet(struct rte_mempool *mpool,
		const struct rte_ether_addr *mac);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ARP_H_ */
