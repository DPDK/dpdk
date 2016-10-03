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

#ifndef _RTE_ETHER_H_
#define _RTE_ETHER_H_

/**
 * @file
 *
 * Ethernet Helpers in RTE
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include <rte_memcpy.h>
#include <rte_random.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>

#define ETHER_ADDR_LEN  6 /**< Length of Ethernet address. */
#define ETHER_TYPE_LEN  2 /**< Length of Ethernet type field. */
#define ETHER_CRC_LEN   4 /**< Length of Ethernet CRC. */
#define ETHER_HDR_LEN   \
	(ETHER_ADDR_LEN * 2 + ETHER_TYPE_LEN) /**< Length of Ethernet header. */
#define ETHER_MIN_LEN   64    /**< Minimum frame len, including CRC. */
#define ETHER_MAX_LEN   1518  /**< Maximum frame len, including CRC. */
#define ETHER_MTU       \
	(ETHER_MAX_LEN - ETHER_HDR_LEN - ETHER_CRC_LEN) /**< Ethernet MTU. */

#define ETHER_MAX_VLAN_FRAME_LEN \
	(ETHER_MAX_LEN + 4) /**< Maximum VLAN frame length, including CRC. */

#define ETHER_MAX_JUMBO_FRAME_LEN \
	0x3F00 /**< Maximum Jumbo frame length, including CRC. */

#define ETHER_MAX_VLAN_ID  4095 /**< Maximum VLAN ID. */

#define ETHER_MIN_MTU 68 /**< Minimum MTU for IPv4 packets, see RFC 791. */

/**
 * Ethernet address:
 * A universally administered address is uniquely assigned to a device by its
 * manufacturer. The first three octets (in transmission order) contain the
 * Organizationally Unique Identifier (OUI). The following three (MAC-48 and
 * EUI-48) octets are assigned by that organization with the only constraint
 * of uniqueness.
 * A locally administered address is assigned to a device by a network
 * administrator and does not contain OUIs.
 * See http://standards.ieee.org/regauth/groupmac/tutorial.html
 */
struct ether_addr {
	uint8_t addr_bytes[ETHER_ADDR_LEN]; /**< Addr bytes in tx order */
} __attribute__((__packed__));

#define ETHER_LOCAL_ADMIN_ADDR 0x02 /**< Locally assigned Eth. address. */
#define ETHER_GROUP_ADDR       0x01 /**< Multicast or broadcast Eth. address. */

/**
 * Check if two Ethernet addresses are the same.
 *
 * @param ea1
 *  A pointer to the first ether_addr structure containing
 *  the ethernet address.
 * @param ea2
 *  A pointer to the second ether_addr structure containing
 *  the ethernet address.
 *
 * @return
 *  True  (1) if the given two ethernet address are the same;
 *  False (0) otherwise.
 */
static inline int is_same_ether_addr(const struct ether_addr *ea1,
				     const struct ether_addr *ea2)
{
	int i;
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		if (ea1->addr_bytes[i] != ea2->addr_bytes[i])
			return 0;
	return 1;
}

/**
 * Check if an Ethernet address is filled with zeros.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is filled with zeros;
 *   false (0) otherwise.
 */
static inline int is_zero_ether_addr(const struct ether_addr *ea)
{
	int i;
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		if (ea->addr_bytes[i] != 0x00)
			return 0;
	return 1;
}

/**
 * Check if an Ethernet address is a unicast address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a unicast address;
 *   false (0) otherwise.
 */
static inline int is_unicast_ether_addr(const struct ether_addr *ea)
{
	return (ea->addr_bytes[0] & ETHER_GROUP_ADDR) == 0;
}

/**
 * Check if an Ethernet address is a multicast address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a multicast address;
 *   false (0) otherwise.
 */
static inline int is_multicast_ether_addr(const struct ether_addr *ea)
{
	return ea->addr_bytes[0] & ETHER_GROUP_ADDR;
}

/**
 * Check if an Ethernet address is a broadcast address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a broadcast address;
 *   false (0) otherwise.
 */
static inline int is_broadcast_ether_addr(const struct ether_addr *ea)
{
	const unaligned_uint16_t *ea_words = (const unaligned_uint16_t *)ea;

	return (ea_words[0] == 0xFFFF && ea_words[1] == 0xFFFF &&
		ea_words[2] == 0xFFFF);
}

/**
 * Check if an Ethernet address is a universally assigned address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a universally assigned address;
 *   false (0) otherwise.
 */
static inline int is_universal_ether_addr(const struct ether_addr *ea)
{
	return (ea->addr_bytes[0] & ETHER_LOCAL_ADMIN_ADDR) == 0;
}

/**
 * Check if an Ethernet address is a locally assigned address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a locally assigned address;
 *   false (0) otherwise.
 */
static inline int is_local_admin_ether_addr(const struct ether_addr *ea)
{
	return (ea->addr_bytes[0] & ETHER_LOCAL_ADMIN_ADDR) != 0;
}

/**
 * Check if an Ethernet address is a valid address. Checks that the address is a
 * unicast address and is not filled with zeros.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is valid;
 *   false (0) otherwise.
 */
static inline int is_valid_assigned_ether_addr(const struct ether_addr *ea)
{
	return is_unicast_ether_addr(ea) && (!is_zero_ether_addr(ea));
}

/**
 * Generate a random Ethernet address that is locally administered
 * and not multicast.
 * @param addr
 *   A pointer to Ethernet address.
 */
static inline void eth_random_addr(uint8_t *addr)
{
	uint64_t rand = rte_rand();
	uint8_t *p = (uint8_t *)&rand;

	rte_memcpy(addr, p, ETHER_ADDR_LEN);
	addr[0] &= ~ETHER_GROUP_ADDR;       /* clear multicast bit */
	addr[0] |= ETHER_LOCAL_ADMIN_ADDR;  /* set local assignment bit */
}

/**
 * Fast copy an Ethernet address.
 *
 * @param ea_from
 *   A pointer to a ether_addr structure holding the Ethernet address to copy.
 * @param ea_to
 *   A pointer to a ether_addr structure where to copy the Ethernet address.
 */
static inline void ether_addr_copy(const struct ether_addr *ea_from,
				   struct ether_addr *ea_to)
{
#ifdef __INTEL_COMPILER
	uint16_t *from_words = (uint16_t *)(ea_from->addr_bytes);
	uint16_t *to_words   = (uint16_t *)(ea_to->addr_bytes);

	to_words[0] = from_words[0];
	to_words[1] = from_words[1];
	to_words[2] = from_words[2];
#else
	/*
	 * Use the common way, because of a strange gcc warning.
	 */
	*ea_to = *ea_from;
#endif
}

#define ETHER_ADDR_FMT_SIZE         18
/**
 * Format 48bits Ethernet address in pattern xx:xx:xx:xx:xx:xx.
 *
 * @param buf
 *   A pointer to buffer contains the formatted MAC address.
 * @param size
 *   The format buffer size.
 * @param eth_addr
 *   A pointer to a ether_addr structure.
 */
static inline void
ether_format_addr(char *buf, uint16_t size,
		  const struct ether_addr *eth_addr)
{
	snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X",
		 eth_addr->addr_bytes[0],
		 eth_addr->addr_bytes[1],
		 eth_addr->addr_bytes[2],
		 eth_addr->addr_bytes[3],
		 eth_addr->addr_bytes[4],
		 eth_addr->addr_bytes[5]);
}

/**
 * Ethernet header: Contains the destination address, source address
 * and frame type.
 */
struct ether_hdr {
	struct ether_addr d_addr; /**< Destination address. */
	struct ether_addr s_addr; /**< Source address. */
	uint16_t ether_type;      /**< Frame type. */
} __attribute__((__packed__));

/**
 * Ethernet VLAN Header.
 * Contains the 16-bit VLAN Tag Control Identifier and the Ethernet type
 * of the encapsulated frame.
 */
struct vlan_hdr {
	uint16_t vlan_tci; /**< Priority (3) + CFI (1) + Identifier Code (12) */
	uint16_t eth_proto;/**< Ethernet type of encapsulated frame. */
} __attribute__((__packed__));

/**
 * VXLAN protocol header.
 * Contains the 8-bit flag, 24-bit VXLAN Network Identifier and
 * Reserved fields (24 bits and 8 bits)
 */
struct vxlan_hdr {
	uint32_t vx_flags; /**< flag (8) + Reserved (24). */
	uint32_t vx_vni;   /**< VNI (24) + Reserved (8). */
} __attribute__((__packed__));

/* Ethernet frame types */
#define ETHER_TYPE_IPv4 0x0800 /**< IPv4 Protocol. */
#define ETHER_TYPE_IPv6 0x86DD /**< IPv6 Protocol. */
#define ETHER_TYPE_ARP  0x0806 /**< Arp Protocol. */
#define ETHER_TYPE_RARP 0x8035 /**< Reverse Arp Protocol. */
#define ETHER_TYPE_VLAN 0x8100 /**< IEEE 802.1Q VLAN tagging. */
#define ETHER_TYPE_QINQ 0x88A8 /**< IEEE 802.1ad QinQ tagging. */
#define ETHER_TYPE_1588 0x88F7 /**< IEEE 802.1AS 1588 Precise Time Protocol. */
#define ETHER_TYPE_SLOW 0x8809 /**< Slow protocols (LACP and Marker). */
#define ETHER_TYPE_TEB  0x6558 /**< Transparent Ethernet Bridging. */

#define ETHER_VXLAN_HLEN (sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr))
/**< VXLAN tunnel header length. */

/**
 * Extract VLAN tag information into mbuf
 *
 * Software version of VLAN stripping
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   - 0: Success
 *   - 1: not a vlan packet
 */
static inline int rte_vlan_strip(struct rte_mbuf *m)
{
	struct ether_hdr *eh
		 = rte_pktmbuf_mtod(m, struct ether_hdr *);

	if (eh->ether_type != rte_cpu_to_be_16(ETHER_TYPE_VLAN))
		return -1;

	struct vlan_hdr *vh = (struct vlan_hdr *)(eh + 1);
	m->ol_flags |= PKT_RX_VLAN_PKT;
	m->vlan_tci = rte_be_to_cpu_16(vh->vlan_tci);

	/* Copy ether header over rather than moving whole packet */
	memmove(rte_pktmbuf_adj(m, sizeof(struct vlan_hdr)),
		eh, 2 * ETHER_ADDR_LEN);

	return 0;
}

/**
 * Insert VLAN tag into mbuf.
 *
 * Software version of VLAN unstripping
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   - 0: On success
 *   -EPERM: mbuf is is shared overwriting would be unsafe
 *   -ENOSPC: not enough headroom in mbuf
 */
static inline int rte_vlan_insert(struct rte_mbuf **m)
{
	struct ether_hdr *oh, *nh;
	struct vlan_hdr *vh;

	/* Can't insert header if mbuf is shared */
	if (rte_mbuf_refcnt_read(*m) > 1) {
		struct rte_mbuf *copy;

		copy = rte_pktmbuf_clone(*m, (*m)->pool);
		if (unlikely(copy == NULL))
			return -ENOMEM;
		rte_pktmbuf_free(*m);
		*m = copy;
	}

	oh = rte_pktmbuf_mtod(*m, struct ether_hdr *);
	nh = (struct ether_hdr *)
		rte_pktmbuf_prepend(*m, sizeof(struct vlan_hdr));
	if (nh == NULL)
		return -ENOSPC;

	memmove(nh, oh, 2 * ETHER_ADDR_LEN);
	nh->ether_type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);

	vh = (struct vlan_hdr *) (nh + 1);
	vh->vlan_tci = rte_cpu_to_be_16((*m)->vlan_tci);

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHER_H_ */
