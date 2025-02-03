/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

#ifndef _RTE_IP4_H_
#define _RTE_IP4_H_

/**
 * @file
 *
 * IPv4-related defines
 */

#include <stdint.h>

#ifdef RTE_EXEC_ENV_WINDOWS
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#endif

#include <rte_byteorder.h>
#include <rte_cksum.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IPv4 Header
 */
struct __rte_aligned(2) __rte_packed_begin rte_ipv4_hdr {
	__extension__
	union {
		uint8_t version_ihl;    /**< version and header length */
		struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
			uint8_t ihl:4;     /**< header length */
			uint8_t version:4; /**< version */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint8_t version:4; /**< version */
			uint8_t ihl:4;     /**< header length */
#endif
		};
	};
	uint8_t  type_of_service;	/**< type of service */
	rte_be16_t total_length;	/**< length of packet */
	rte_be16_t packet_id;		/**< packet ID */
	rte_be16_t fragment_offset;	/**< fragmentation offset */
	uint8_t  time_to_live;		/**< time to live */
	uint8_t  next_proto_id;		/**< protocol ID */
	rte_be16_t hdr_checksum;	/**< header checksum */
	rte_be32_t src_addr;		/**< source address */
	rte_be32_t dst_addr;		/**< destination address */
} __rte_packed_end;

/** Create IPv4 address */
#define RTE_IPV4(a, b, c, d) (((uint32_t)((a) & 0xff) << 24) | ((uint32_t)((b) & 0xff) << 16) | \
	((uint32_t)((c) & 0xff) << 8) | ((uint32_t)((d) & 0xff)))

/** Maximal IPv4 packet length (including a header) */
#define RTE_IPV4_MAX_PKT_LEN        65535

/** Internet header length mask for version_ihl field */
#define RTE_IPV4_HDR_IHL_MASK	(0x0f)
/**
 * Internet header length field multiplier (IHL field specifies overall header
 * length in number of 4-byte words)
 */
#define RTE_IPV4_IHL_MULTIPLIER	(4)

/* Type of Service fields */
#define RTE_IPV4_HDR_DSCP_MASK	(0xfc)
#define RTE_IPV4_HDR_ECN_MASK	(0x03)
#define RTE_IPV4_HDR_ECN_CE	RTE_IPV4_HDR_ECN_MASK

/* Fragment Offset * Flags. */
#define	RTE_IPV4_HDR_DF_SHIFT	14
#define	RTE_IPV4_HDR_MF_SHIFT	13
#define	RTE_IPV4_HDR_FO_SHIFT	3

#define	RTE_IPV4_HDR_DF_FLAG	(1 << RTE_IPV4_HDR_DF_SHIFT)
#define	RTE_IPV4_HDR_MF_FLAG	(1 << RTE_IPV4_HDR_MF_SHIFT)

#define	RTE_IPV4_HDR_OFFSET_MASK	((1 << RTE_IPV4_HDR_MF_SHIFT) - 1)

#define	RTE_IPV4_HDR_OFFSET_UNITS	8

/* IPv4 options */
#define RTE_IPV4_HDR_OPT_EOL       0
#define RTE_IPV4_HDR_OPT_NOP       1
#define RTE_IPV4_HDR_OPT_COPIED(v) ((v) & 0x80)
#define RTE_IPV4_HDR_OPT_MAX_LEN   40

/*
 * IPv4 address types
 */
#define RTE_IPV4_ANY              ((uint32_t)0x00000000) /**< 0.0.0.0 */
#define RTE_IPV4_LOOPBACK         ((uint32_t)0x7f000001) /**< 127.0.0.1 */
#define RTE_IPV4_BROADCAST        ((uint32_t)0xe0000000) /**< 224.0.0.0 */
#define RTE_IPV4_ALLHOSTS_GROUP   ((uint32_t)0xe0000001) /**< 224.0.0.1 */
#define RTE_IPV4_ALLRTRS_GROUP    ((uint32_t)0xe0000002) /**< 224.0.0.2 */
#define RTE_IPV4_MAX_LOCAL_GROUP  ((uint32_t)0xe00000ff) /**< 224.0.0.255 */

/*
 * IPv4 Multicast-related macros
 */
#define RTE_IPV4_MIN_MCAST \
	RTE_IPV4(224, 0, 0, 0)          /**< Minimal IPv4-multicast address */
#define RTE_IPV4_MAX_MCAST \
	RTE_IPV4(239, 255, 255, 255)    /**< Maximum IPv4 multicast address */

#define RTE_IS_IPV4_MCAST(x) \
	((x) >= RTE_IPV4_MIN_MCAST && (x) <= RTE_IPV4_MAX_MCAST)
	/**< check if IPv4 address is multicast */

/* IPv4 default fields values */
#define RTE_IPV4_MIN_IHL    (0x5)
#define RTE_IPV4_VHL_DEF    ((IPVERSION << 4) | RTE_IPV4_MIN_IHL)

/**
 * Get the length of an IPv4 header.
 *
 * @param ipv4_hdr
 *   Pointer to the IPv4 header.
 * @return
 *   The length of the IPv4 header (with options if present) in bytes.
 */
static inline uint8_t
rte_ipv4_hdr_len(const struct rte_ipv4_hdr *ipv4_hdr)
{
	return (uint8_t)((ipv4_hdr->version_ihl & RTE_IPV4_HDR_IHL_MASK) *
		RTE_IPV4_IHL_MULTIPLIER);
}

/**
 * Process the IPv4 checksum of an IPv4 header.
 *
 * The checksum field must be set to 0 by the caller.
 *
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @return
 *   The complemented checksum to set in the IP packet.
 */
static inline uint16_t
rte_ipv4_cksum(const struct rte_ipv4_hdr *ipv4_hdr)
{
	uint16_t cksum;
	cksum = rte_raw_cksum(ipv4_hdr, rte_ipv4_hdr_len(ipv4_hdr));
	return (uint16_t)~cksum;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Process the IPv4 checksum of an IPv4 header without any extensions.
 *
 * The checksum field does NOT have to be set by the caller, the field
 * is skipped by the calculation.
 *
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @return
 *   The complemented checksum to set in the IP packet.
 */
__rte_experimental
static inline uint16_t
rte_ipv4_cksum_simple(const struct rte_ipv4_hdr *ipv4_hdr)
{
	const uint16_t *v16_h;
	uint32_t ip_cksum;

	/*
	 * Compute the sum of successive 16-bit words of the IPv4 header,
	 * skipping the checksum field of the header.
	 */
	v16_h = (const uint16_t *)ipv4_hdr;
	ip_cksum = v16_h[0] + v16_h[1] + v16_h[2] + v16_h[3] +
		v16_h[4] + v16_h[6] + v16_h[7] + v16_h[8] + v16_h[9];

	/* reduce 32 bit checksum to 16 bits and complement it */
	ip_cksum = (ip_cksum & 0xffff) + (ip_cksum >> 16);
	ip_cksum = (ip_cksum & 0xffff) + (ip_cksum >> 16);
	return (uint16_t)(~ip_cksum);
}

/**
 * Process the pseudo-header checksum of an IPv4 header.
 *
 * The checksum field must be set to 0 by the caller.
 *
 * Depending on the ol_flags, the pseudo-header checksum expected by the
 * drivers is not the same. For instance, when TSO is enabled, the IP
 * payload length must not be included in the packet.
 *
 * When ol_flags is 0, it computes the standard pseudo-header checksum.
 *
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @param ol_flags
 *   The ol_flags of the associated mbuf.
 * @return
 *   The non-complemented checksum to set in the L4 header.
 */
static inline uint16_t
rte_ipv4_phdr_cksum(const struct rte_ipv4_hdr *ipv4_hdr, uint64_t ol_flags)
{
	struct ipv4_psd_header {
		uint32_t src_addr; /* IP address of source host. */
		uint32_t dst_addr; /* IP address of destination host. */
		uint8_t  zero;     /* zero. */
		uint8_t  proto;    /* L4 protocol type. */
		uint16_t len;      /* L4 length. */
	} psd_hdr;

	uint32_t l3_len;

	psd_hdr.src_addr = ipv4_hdr->src_addr;
	psd_hdr.dst_addr = ipv4_hdr->dst_addr;
	psd_hdr.zero = 0;
	psd_hdr.proto = ipv4_hdr->next_proto_id;
	if (ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) {
		psd_hdr.len = 0;
	} else {
		l3_len = rte_be_to_cpu_16(ipv4_hdr->total_length);
		psd_hdr.len = rte_cpu_to_be_16((uint16_t)(l3_len -
			rte_ipv4_hdr_len(ipv4_hdr)));
	}
	return rte_raw_cksum(&psd_hdr, sizeof(psd_hdr));
}

/**
 * @internal Calculate the non-complemented IPv4 L4 checksum
 */
static inline uint16_t
__rte_ipv4_udptcp_cksum(const struct rte_ipv4_hdr *ipv4_hdr, const void *l4_hdr)
{
	uint32_t cksum;
	uint32_t l3_len, l4_len;
	uint8_t ip_hdr_len;

	ip_hdr_len = rte_ipv4_hdr_len(ipv4_hdr);
	l3_len = rte_be_to_cpu_16(ipv4_hdr->total_length);
	if (l3_len < ip_hdr_len)
		return 0;

	l4_len = l3_len - ip_hdr_len;

	cksum = rte_raw_cksum(l4_hdr, l4_len);
	cksum += rte_ipv4_phdr_cksum(ipv4_hdr, 0);

	cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);

	return (uint16_t)cksum;
}

/**
 * Process the IPv4 UDP or TCP checksum.
 *
 * The layer 4 checksum must be set to 0 in the L4 header by the caller.
 *
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @param l4_hdr
 *   The pointer to the beginning of the L4 header.
 * @return
 *   The complemented checksum to set in the L4 header.
 */
static inline uint16_t
rte_ipv4_udptcp_cksum(const struct rte_ipv4_hdr *ipv4_hdr, const void *l4_hdr)
{
	uint16_t cksum = __rte_ipv4_udptcp_cksum(ipv4_hdr, l4_hdr);

	cksum = ~cksum;

	/*
	 * Per RFC 768: If the computed checksum is zero for UDP,
	 * it is transmitted as all ones
	 * (the equivalent in one's complement arithmetic).
	 */
	if (cksum == 0 && ipv4_hdr->next_proto_id == IPPROTO_UDP)
		cksum = 0xffff;

	return cksum;
}

/**
 * @internal Calculate the non-complemented IPv4 L4 checksum of a packet
 */
static inline uint16_t
__rte_ipv4_udptcp_cksum_mbuf(const struct rte_mbuf *m,
			     const struct rte_ipv4_hdr *ipv4_hdr,
			     uint16_t l4_off)
{
	uint16_t raw_cksum;
	uint32_t cksum;
	uint16_t len;

	if (unlikely(l4_off > m->pkt_len))
		return 0; /* invalid params, return a dummy value */

	len = rte_be_to_cpu_16(ipv4_hdr->total_length) - (uint16_t)rte_ipv4_hdr_len(ipv4_hdr);

	if (rte_raw_cksum_mbuf(m, l4_off, len, &raw_cksum))
		return 0;

	cksum = raw_cksum + rte_ipv4_phdr_cksum(ipv4_hdr, 0);

	cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);

	return (uint16_t)cksum;
}

/**
 * Compute the IPv4 UDP/TCP checksum of a packet.
 *
 * @param m
 *   The pointer to the mbuf.
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @param l4_off
 *   The offset in bytes to start L4 checksum.
 * @return
 *   The complemented checksum to set in the L4 header.
 */
static inline uint16_t
rte_ipv4_udptcp_cksum_mbuf(const struct rte_mbuf *m,
			   const struct rte_ipv4_hdr *ipv4_hdr, uint16_t l4_off)
{
	uint16_t cksum = __rte_ipv4_udptcp_cksum_mbuf(m, ipv4_hdr, l4_off);

	cksum = ~cksum;

	/*
	 * Per RFC 768: If the computed checksum is zero for UDP,
	 * it is transmitted as all ones
	 * (the equivalent in one's complement arithmetic).
	 */
	if (cksum == 0 && ipv4_hdr->next_proto_id == IPPROTO_UDP)
		cksum = 0xffff;

	return cksum;
}

/**
 * Validate the IPv4 UDP or TCP checksum.
 *
 * In case of UDP, the caller must first check if udp_hdr->dgram_cksum is 0
 * (i.e. no checksum).
 *
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @param l4_hdr
 *   The pointer to the beginning of the L4 header.
 * @return
 *   Return 0 if the checksum is correct, else -1.
 */
static inline int
rte_ipv4_udptcp_cksum_verify(const struct rte_ipv4_hdr *ipv4_hdr,
			     const void *l4_hdr)
{
	uint16_t cksum = __rte_ipv4_udptcp_cksum(ipv4_hdr, l4_hdr);

	if (cksum != 0xffff)
		return -1;

	return 0;
}

/**
 * Verify the IPv4 UDP/TCP checksum of a packet.
 *
 * In case of UDP, the caller must first check if udp_hdr->dgram_cksum is 0
 * (i.e. no checksum).
 *
 * @param m
 *   The pointer to the mbuf.
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @param l4_off
 *   The offset in bytes to start L4 checksum.
 * @return
 *   Return 0 if the checksum is correct, else -1.
 */
static inline int
rte_ipv4_udptcp_cksum_mbuf_verify(const struct rte_mbuf *m,
				  const struct rte_ipv4_hdr *ipv4_hdr,
				  uint16_t l4_off)
{
	uint16_t cksum = __rte_ipv4_udptcp_cksum_mbuf(m, ipv4_hdr, l4_off);

	if (cksum != 0xffff)
		return -1;

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IP_H_ */
