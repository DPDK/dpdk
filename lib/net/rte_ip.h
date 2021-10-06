/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

#ifndef _RTE_IP_H_
#define _RTE_IP_H_

/**
 * @file
 *
 * IP-related defines
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
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IPv4 Header
 */
struct rte_ipv4_hdr {
	uint8_t  version_ihl;		/**< version and header length */
	uint8_t  type_of_service;	/**< type of service */
	rte_be16_t total_length;	/**< length of packet */
	rte_be16_t packet_id;		/**< packet ID */
	rte_be16_t fragment_offset;	/**< fragmentation offset */
	uint8_t  time_to_live;		/**< time to live */
	uint8_t  next_proto_id;		/**< protocol ID */
	rte_be16_t hdr_checksum;	/**< header checksum */
	rte_be32_t src_addr;		/**< source address */
	rte_be32_t dst_addr;		/**< destination address */
} __rte_packed;

/** Create IPv4 address */
#define RTE_IPV4(a, b, c, d) ((uint32_t)(((a) & 0xff) << 24) | \
					   (((b) & 0xff) << 16) | \
					   (((c) & 0xff) << 8)  | \
					   ((d) & 0xff))

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
 * @internal Calculate a sum of all words in the buffer.
 * Helper routine for the rte_raw_cksum().
 *
 * @param buf
 *   Pointer to the buffer.
 * @param len
 *   Length of the buffer.
 * @param sum
 *   Initial value of the sum.
 * @return
 *   sum += Sum of all words in the buffer.
 */
static inline uint32_t
__rte_raw_cksum(const void *buf, size_t len, uint32_t sum)
{
	/* workaround gcc strict-aliasing warning */
	uintptr_t ptr = (uintptr_t)buf;
	typedef uint16_t __attribute__((__may_alias__)) u16_p;
	const u16_p *u16_buf = (const u16_p *)ptr;

	while (len >= (sizeof(*u16_buf) * 4)) {
		sum += u16_buf[0];
		sum += u16_buf[1];
		sum += u16_buf[2];
		sum += u16_buf[3];
		len -= sizeof(*u16_buf) * 4;
		u16_buf += 4;
	}
	while (len >= sizeof(*u16_buf)) {
		sum += *u16_buf;
		len -= sizeof(*u16_buf);
		u16_buf += 1;
	}

	/* if length is in odd bytes */
	if (len == 1) {
		uint16_t left = 0;
		*(uint8_t *)&left = *(const uint8_t *)u16_buf;
		sum += left;
	}

	return sum;
}

/**
 * @internal Reduce a sum to the non-complemented checksum.
 * Helper routine for the rte_raw_cksum().
 *
 * @param sum
 *   Value of the sum.
 * @return
 *   The non-complemented checksum.
 */
static inline uint16_t
__rte_raw_cksum_reduce(uint32_t sum)
{
	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	return (uint16_t)sum;
}

/**
 * Process the non-complemented checksum of a buffer.
 *
 * @param buf
 *   Pointer to the buffer.
 * @param len
 *   Length of the buffer.
 * @return
 *   The non-complemented checksum.
 */
static inline uint16_t
rte_raw_cksum(const void *buf, size_t len)
{
	uint32_t sum;

	sum = __rte_raw_cksum(buf, len, 0);
	return __rte_raw_cksum_reduce(sum);
}

/**
 * Compute the raw (non complemented) checksum of a packet.
 *
 * @param m
 *   The pointer to the mbuf.
 * @param off
 *   The offset in bytes to start the checksum.
 * @param len
 *   The length in bytes of the data to checksum.
 * @param cksum
 *   A pointer to the checksum, filled on success.
 * @return
 *   0 on success, -1 on error (bad length or offset).
 */
static inline int
rte_raw_cksum_mbuf(const struct rte_mbuf *m, uint32_t off, uint32_t len,
	uint16_t *cksum)
{
	const struct rte_mbuf *seg;
	const char *buf;
	uint32_t sum, tmp;
	uint32_t seglen, done;

	/* easy case: all data in the first segment */
	if (off + len <= rte_pktmbuf_data_len(m)) {
		*cksum = rte_raw_cksum(rte_pktmbuf_mtod_offset(m,
				const char *, off), len);
		return 0;
	}

	if (unlikely(off + len > rte_pktmbuf_pkt_len(m)))
		return -1; /* invalid params, return a dummy value */

	/* else browse the segment to find offset */
	seglen = 0;
	for (seg = m; seg != NULL; seg = seg->next) {
		seglen = rte_pktmbuf_data_len(seg);
		if (off < seglen)
			break;
		off -= seglen;
	}
	RTE_ASSERT(seg != NULL);
	if (seg == NULL)
		return -1;
	seglen -= off;
	buf = rte_pktmbuf_mtod_offset(seg, const char *, off);
	if (seglen >= len) {
		/* all in one segment */
		*cksum = rte_raw_cksum(buf, len);
		return 0;
	}

	/* hard case: process checksum of several segments */
	sum = 0;
	done = 0;
	for (;;) {
		tmp = __rte_raw_cksum(buf, seglen, 0);
		if (done & 1)
			tmp = rte_bswap16((uint16_t)tmp);
		sum += tmp;
		done += seglen;
		if (done == len)
			break;
		seg = seg->next;
		buf = rte_pktmbuf_mtod(seg, const char *);
		seglen = rte_pktmbuf_data_len(seg);
		if (seglen > len - done)
			seglen = len - done;
	}

	*cksum = __rte_raw_cksum_reduce(sum);
	return 0;
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
	if (ol_flags & PKT_TX_TCP_SEG) {
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
 * The IP and layer 4 checksum must be set to 0 in the packet by
 * the caller.
 *
 * @param ipv4_hdr
 *   The pointer to the contiguous IPv4 header.
 * @param l4_hdr
 *   The pointer to the beginning of the L4 header.
 * @return
 *   The complemented checksum to set in the IP packet.
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
__rte_experimental
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
 * IPv6 Header
 */
struct rte_ipv6_hdr {
	rte_be32_t vtc_flow;	/**< IP version, traffic class & flow label. */
	rte_be16_t payload_len;	/**< IP payload size, including ext. headers */
	uint8_t  proto;		/**< Protocol, next header. */
	uint8_t  hop_limits;	/**< Hop limits. */
	uint8_t  src_addr[16];	/**< IP address of source host. */
	uint8_t  dst_addr[16];	/**< IP address of destination host(s). */
} __rte_packed;

/* IPv6 vtc_flow: IPv / TC / flow_label */
#define RTE_IPV6_HDR_FL_SHIFT 0
#define RTE_IPV6_HDR_TC_SHIFT 20
#define RTE_IPV6_HDR_FL_MASK	((1u << RTE_IPV6_HDR_TC_SHIFT) - 1)
#define RTE_IPV6_HDR_TC_MASK	(0xff << RTE_IPV6_HDR_TC_SHIFT)
#define RTE_IPV6_HDR_DSCP_MASK	(0xfc << RTE_IPV6_HDR_TC_SHIFT)
#define RTE_IPV6_HDR_ECN_MASK	(0x03 << RTE_IPV6_HDR_TC_SHIFT)
#define RTE_IPV6_HDR_ECN_CE	RTE_IPV6_HDR_ECN_MASK

#define RTE_IPV6_MIN_MTU 1280 /**< Minimum MTU for IPv6, see RFC 8200. */

/**
 * Process the pseudo-header checksum of an IPv6 header.
 *
 * Depending on the ol_flags, the pseudo-header checksum expected by the
 * drivers is not the same. For instance, when TSO is enabled, the IPv6
 * payload length must not be included in the packet.
 *
 * When ol_flags is 0, it computes the standard pseudo-header checksum.
 *
 * @param ipv6_hdr
 *   The pointer to the contiguous IPv6 header.
 * @param ol_flags
 *   The ol_flags of the associated mbuf.
 * @return
 *   The non-complemented checksum to set in the L4 header.
 */
static inline uint16_t
rte_ipv6_phdr_cksum(const struct rte_ipv6_hdr *ipv6_hdr, uint64_t ol_flags)
{
	uint32_t sum;
	struct {
		rte_be32_t len;   /* L4 length. */
		rte_be32_t proto; /* L4 protocol - top 3 bytes must be zero */
	} psd_hdr;

	psd_hdr.proto = (uint32_t)(ipv6_hdr->proto << 24);
	if (ol_flags & PKT_TX_TCP_SEG) {
		psd_hdr.len = 0;
	} else {
		psd_hdr.len = ipv6_hdr->payload_len;
	}

	sum = __rte_raw_cksum(ipv6_hdr->src_addr,
		sizeof(ipv6_hdr->src_addr) + sizeof(ipv6_hdr->dst_addr),
		0);
	sum = __rte_raw_cksum(&psd_hdr, sizeof(psd_hdr), sum);
	return __rte_raw_cksum_reduce(sum);
}

/**
 * @internal Calculate the non-complemented IPv4 L4 checksum
 */
static inline uint16_t
__rte_ipv6_udptcp_cksum(const struct rte_ipv6_hdr *ipv6_hdr, const void *l4_hdr)
{
	uint32_t cksum;
	uint32_t l4_len;

	l4_len = rte_be_to_cpu_16(ipv6_hdr->payload_len);

	cksum = rte_raw_cksum(l4_hdr, l4_len);
	cksum += rte_ipv6_phdr_cksum(ipv6_hdr, 0);

	cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);

	return (uint16_t)cksum;
}

/**
 * Process the IPv6 UDP or TCP checksum.
 *
 * The IPv4 header should not contains options. The layer 4 checksum
 * must be set to 0 in the packet by the caller.
 *
 * @param ipv6_hdr
 *   The pointer to the contiguous IPv6 header.
 * @param l4_hdr
 *   The pointer to the beginning of the L4 header.
 * @return
 *   The complemented checksum to set in the IP packet.
 */
static inline uint16_t
rte_ipv6_udptcp_cksum(const struct rte_ipv6_hdr *ipv6_hdr, const void *l4_hdr)
{
	uint16_t cksum = __rte_ipv6_udptcp_cksum(ipv6_hdr, l4_hdr);

	cksum = ~cksum;

	/*
	 * Per RFC 768: If the computed checksum is zero for UDP,
	 * it is transmitted as all ones
	 * (the equivalent in one's complement arithmetic).
	 */
	if (cksum == 0 && ipv6_hdr->proto == IPPROTO_UDP)
		cksum = 0xffff;

	return cksum;
}

/**
 * Validate the IPv6 UDP or TCP checksum.
 *
 * In case of UDP, the caller must first check if udp_hdr->dgram_cksum is 0:
 * this is either invalid or means no checksum in some situations. See 8.1
 * (Upper-Layer Checksums) in RFC 8200.
 *
 * @param ipv6_hdr
 *   The pointer to the contiguous IPv6 header.
 * @param l4_hdr
 *   The pointer to the beginning of the L4 header.
 * @return
 *   Return 0 if the checksum is correct, else -1.
 */
__rte_experimental
static inline int
rte_ipv6_udptcp_cksum_verify(const struct rte_ipv6_hdr *ipv6_hdr,
			     const void *l4_hdr)
{
	uint16_t cksum = __rte_ipv6_udptcp_cksum(ipv6_hdr, l4_hdr);

	if (cksum != 0xffff)
		return -1;

	return 0;
}

/** IPv6 fragment extension header. */
#define	RTE_IPV6_EHDR_MF_SHIFT	0
#define	RTE_IPV6_EHDR_MF_MASK	1
#define	RTE_IPV6_EHDR_FO_SHIFT	3
#define	RTE_IPV6_EHDR_FO_MASK	(~((1 << RTE_IPV6_EHDR_FO_SHIFT) - 1))
#define	RTE_IPV6_EHDR_FO_ALIGN	(1 << RTE_IPV6_EHDR_FO_SHIFT)

#define RTE_IPV6_FRAG_USED_MASK	(RTE_IPV6_EHDR_MF_MASK | RTE_IPV6_EHDR_FO_MASK)

#define RTE_IPV6_GET_MF(x)	((x) & RTE_IPV6_EHDR_MF_MASK)
#define RTE_IPV6_GET_FO(x)	((x) >> RTE_IPV6_EHDR_FO_SHIFT)

#define RTE_IPV6_SET_FRAG_DATA(fo, mf)	\
	(((fo) & RTE_IPV6_EHDR_FO_MASK) | ((mf) & RTE_IPV6_EHDR_MF_MASK))

struct rte_ipv6_fragment_ext {
	uint8_t next_header;	/**< Next header type */
	uint8_t reserved;	/**< Reserved */
	rte_be16_t frag_data;	/**< All fragmentation data */
	rte_be32_t id;		/**< Packet ID */
} __rte_packed;

/* IPv6 fragment extension header size */
#define RTE_IPV6_FRAG_HDR_SIZE	sizeof(struct rte_ipv6_fragment_ext)

/**
 * Parse next IPv6 header extension
 *
 * This function checks if proto number is an IPv6 extensions and parses its
 * data if so, providing information on next header and extension length.
 *
 * @param p
 *   Pointer to an extension raw data.
 * @param proto
 *   Protocol number extracted from the "next header" field from
 *   the IPv6 header or the previous extension.
 * @param ext_len
 *   Extension data length.
 * @return
 *   next protocol number if proto is an IPv6 extension, -EINVAL otherwise
 */
__rte_experimental
static inline int
rte_ipv6_get_next_ext(const uint8_t *p, int proto, size_t *ext_len)
{
	int next_proto;

	switch (proto) {
	case IPPROTO_AH:
		next_proto = *p++;
		*ext_len = (*p + 2) * sizeof(uint32_t);
		break;

	case IPPROTO_HOPOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_DSTOPTS:
		next_proto = *p++;
		*ext_len = (*p + 1) * sizeof(uint64_t);
		break;

	case IPPROTO_FRAGMENT:
		next_proto = *p;
		*ext_len = RTE_IPV6_FRAG_HDR_SIZE;
		break;

	default:
		return -EINVAL;
	}

	return next_proto;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IP_H_ */
