/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright 2014 6WIND S.A.
 */

/*
 * Snipper from dpdk.org rte_mbuf.h.
 * used to provide BPF programs information about rte_mbuf layout.
 */

#ifndef _MBUF_H_
#define _MBUF_H_

#include <stdint.h>
#include <rte_common.h>
#include <rte_memory.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * RX packet is a 802.1q VLAN packet. This flag was set by PMDs when
 * the packet is recognized as a VLAN, but the behavior between PMDs
 * was not the same. This flag is kept for some time to avoid breaking
 * applications and should be replaced by PKT_RX_VLAN_STRIPPED.
 */
#define PKT_RX_VLAN_PKT      (1ULL << 0)

#define PKT_RX_RSS_HASH      (1ULL << 1)
/**< RX packet with RSS hash result. */
#define PKT_RX_FDIR          (1ULL << 2)
/**< RX packet with FDIR match indicate. */

/**
 * Deprecated.
 * Checking this flag alone is deprecated: check the 2 bits of
 * PKT_RX_L4_CKSUM_MASK.
 * This flag was set when the L4 checksum of a packet was detected as
 * wrong by the hardware.
 */
#define PKT_RX_L4_CKSUM_BAD  (1ULL << 3)

/**
 * Deprecated.
 * Checking this flag alone is deprecated: check the 2 bits of
 * PKT_RX_IP_CKSUM_MASK.
 * This flag was set when the IP checksum of a packet was detected as
 * wrong by the hardware.
 */
#define PKT_RX_IP_CKSUM_BAD  (1ULL << 4)

#define PKT_RX_EIP_CKSUM_BAD (1ULL << 5)
/**< External IP header checksum error. */

/**
 * A vlan has been stripped by the hardware and its tci is saved in
 * mbuf->vlan_tci. This can only happen if vlan stripping is enabled
 * in the RX configuration of the PMD.
 */
#define PKT_RX_VLAN_STRIPPED (1ULL << 6)

/**
 * Mask of bits used to determine the status of RX IP checksum.
 * - PKT_RX_IP_CKSUM_UNKNOWN: no information about the RX IP checksum
 * - PKT_RX_IP_CKSUM_BAD: the IP checksum in the packet is wrong
 * - PKT_RX_IP_CKSUM_GOOD: the IP checksum in the packet is valid
 * - PKT_RX_IP_CKSUM_NONE: the IP checksum is not correct in the packet
 *   data, but the integrity of the IP header is verified.
 */
#define PKT_RX_IP_CKSUM_MASK ((1ULL << 4) | (1ULL << 7))

#define PKT_RX_IP_CKSUM_UNKNOWN 0
#define PKT_RX_IP_CKSUM_BAD     (1ULL << 4)
#define PKT_RX_IP_CKSUM_GOOD    (1ULL << 7)
#define PKT_RX_IP_CKSUM_NONE    ((1ULL << 4) | (1ULL << 7))

/**
 * Mask of bits used to determine the status of RX L4 checksum.
 * - PKT_RX_L4_CKSUM_UNKNOWN: no information about the RX L4 checksum
 * - PKT_RX_L4_CKSUM_BAD: the L4 checksum in the packet is wrong
 * - PKT_RX_L4_CKSUM_GOOD: the L4 checksum in the packet is valid
 * - PKT_RX_L4_CKSUM_NONE: the L4 checksum is not correct in the packet
 *   data, but the integrity of the L4 data is verified.
 */
#define PKT_RX_L4_CKSUM_MASK ((1ULL << 3) | (1ULL << 8))

#define PKT_RX_L4_CKSUM_UNKNOWN 0
#define PKT_RX_L4_CKSUM_BAD     (1ULL << 3)
#define PKT_RX_L4_CKSUM_GOOD    (1ULL << 8)
#define PKT_RX_L4_CKSUM_NONE    ((1ULL << 3) | (1ULL << 8))

#define PKT_RX_IEEE1588_PTP  (1ULL << 9)
/**< RX IEEE1588 L2 Ethernet PT Packet. */
#define PKT_RX_IEEE1588_TMST (1ULL << 10)
/**< RX IEEE1588 L2/L4 timestamped packet.*/
#define PKT_RX_FDIR_ID       (1ULL << 13)
/**< FD id reported if FDIR match. */
#define PKT_RX_FDIR_FLX      (1ULL << 14)
/**< Flexible bytes reported if FDIR match. */

/**
 * The 2 vlans have been stripped by the hardware and their tci are
 * saved in mbuf->vlan_tci (inner) and mbuf->vlan_tci_outer (outer).
 * This can only happen if vlan stripping is enabled in the RX
 * configuration of the PMD. If this flag is set, PKT_RX_VLAN_STRIPPED
 * must also be set.
 */
#define PKT_RX_QINQ_STRIPPED (1ULL << 15)

/**
 * Deprecated.
 * RX packet with double VLAN stripped.
 * This flag is replaced by PKT_RX_QINQ_STRIPPED.
 */
#define PKT_RX_QINQ_PKT      PKT_RX_QINQ_STRIPPED

/**
 * When packets are coalesced by a hardware or virtual driver, this flag
 * can be set in the RX mbuf, meaning that the m->tso_segsz field is
 * valid and is set to the segment size of original packets.
 */
#define PKT_RX_LRO           (1ULL << 16)

/**
 * Indicate that the timestamp field in the mbuf is valid.
 */
#define PKT_RX_TIMESTAMP     (1ULL << 17)

/* add new RX flags here */

/* add new TX flags here */

/**
 * Offload the MACsec. This flag must be set by the application to enable
 * this offload feature for a packet to be transmitted.
 */
#define PKT_TX_MACSEC        (1ULL << 44)

/**
 * Bits 45:48 used for the tunnel type.
 * When doing Tx offload like TSO or checksum, the HW needs to configure the
 * tunnel type into the HW descriptors.
 */
#define PKT_TX_TUNNEL_VXLAN   (0x1ULL << 45)
#define PKT_TX_TUNNEL_GRE     (0x2ULL << 45)
#define PKT_TX_TUNNEL_IPIP    (0x3ULL << 45)
#define PKT_TX_TUNNEL_GENEVE  (0x4ULL << 45)
/**< TX packet with MPLS-in-UDP RFC 7510 header. */
#define PKT_TX_TUNNEL_MPLSINUDP (0x5ULL << 45)
/* add new TX TUNNEL type here */
#define PKT_TX_TUNNEL_MASK    (0xFULL << 45)

/**
 * Second VLAN insertion (QinQ) flag.
 */
#define PKT_TX_QINQ_PKT    (1ULL << 49)
/**< TX packet with double VLAN inserted. */

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

#define PKT_TX_IEEE1588_TMST (1ULL << 51)
/**< TX IEEE1588 packet to timestamp. */

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
#define PKT_TX_L4_NO_CKSUM   (0ULL << 52)
/**< Disable L4 cksum of TX pkt. */
#define PKT_TX_TCP_CKSUM     (1ULL << 52)
/**< TCP cksum of TX pkt. computed by NIC. */
#define PKT_TX_SCTP_CKSUM    (2ULL << 52)
/**< SCTP cksum of TX pkt. computed by NIC. */
#define PKT_TX_UDP_CKSUM     (3ULL << 52)
/**< UDP cksum of TX pkt. computed by NIC. */
#define PKT_TX_L4_MASK       (3ULL << 52)
/**< Mask for L4 cksum offload request. */

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

#define PKT_TX_VLAN_PKT      (1ULL << 57)
/**< TX packet is a 802.1q VLAN packet. */

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

/**
 * Bitmask of all supported packet Tx offload features flags,
 * which can be set for packet.
 */
#define PKT_TX_OFFLOAD_MASK (    \
		PKT_TX_IP_CKSUM |        \
		PKT_TX_L4_MASK |         \
		PKT_TX_OUTER_IP_CKSUM |  \
		PKT_TX_TCP_SEG |         \
		PKT_TX_IEEE1588_TMST |	 \
		PKT_TX_QINQ_PKT |        \
		PKT_TX_VLAN_PKT |        \
		PKT_TX_TUNNEL_MASK |	 \
		PKT_TX_MACSEC)

#define __RESERVED           (1ULL << 61) /**< reserved for future mbuf use */

#define IND_ATTACHED_MBUF    (1ULL << 62) /**< Indirect attached mbuf */

/* Use final bit of flags to indicate a control mbuf */
#define CTRL_MBUF_FLAG       (1ULL << 63) /**< Mbuf contains control data */

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
 * Dump the list of RX offload flags in a buffer
 *
 * @param mask
 *   The mask describing the RX flags.
 * @param buf
 *   The output buffer.
 * @param buflen
 *   The length of the buffer.
 * @return
 *   0 on success, (-1) on error.
 */
int rte_get_rx_ol_flag_list(uint64_t mask, char *buf, size_t buflen);

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
 * Dump the list of TX offload flags in a buffer
 *
 * @param mask
 *   The mask describing the TX flags.
 * @param buf
 *   The output buffer.
 * @param buflen
 *   The length of the buffer.
 * @return
 *   0 on success, (-1) on error.
 */
int rte_get_tx_ol_flag_list(uint64_t mask, char *buf, size_t buflen);

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
 * mbuf.
 */
__extension__
typedef void    *MARKER[0];   /**< generic marker for a point in a structure */
__extension__
typedef uint8_t  MARKER8[0];  /**< generic marker with 1B alignment */
__extension__
typedef uint64_t MARKER64[0];
/**< marker that allows us to overwrite 8 bytes with a single assignment */

typedef struct {
	volatile int16_t cnt; /**< An internal counter value. */
} rte_atomic16_t;

/**
 * The generic rte_mbuf, containing a packet mbuf.
 */
struct rte_mbuf {
	MARKER cacheline0;

	void *buf_addr;           /**< Virtual address of segment buffer. */
	/**
	 * Physical address of segment buffer.
	 * Force alignment to 8-bytes, so as to ensure we have the exact
	 * same mbuf cacheline0 layout for 32-bit and 64-bit. This makes
	 * working on vector drivers easier.
	 */
	phys_addr_t buf_physaddr __rte_aligned(sizeof(phys_addr_t));

	/* next 8 bytes are initialised on RX descriptor rearm */
	MARKER64 rearm_data;
	uint16_t data_off;

	/**
	 * Reference counter. Its size should at least equal to the size
	 * of port field (16 bits), to support zero-copy broadcast.
	 * It should only be accessed using the following functions:
	 * rte_mbuf_refcnt_update(), rte_mbuf_refcnt_read(), and
	 * rte_mbuf_refcnt_set(). The functionality of these functions (atomic,
	 * or non-atomic) is controlled by the CONFIG_RTE_MBUF_REFCNT_ATOMIC
	 * config option.
	 */
	RTE_STD_C11
	union {
		rte_atomic16_t refcnt_atomic; /**< Atomically accessed refcnt */
		uint16_t refcnt;
		/**< Non-atomically accessed refcnt */
	};
	uint16_t nb_segs;         /**< Number of segments. */

	/** Input port (16 bits to support more than 256 virtual ports). */
	uint16_t port;

	uint64_t ol_flags;        /**< Offload features. */

	/* remaining bytes are set on RX when pulling packet from descriptor */
	MARKER rx_descriptor_fields1;

	/*
	 * The packet type, which is the combination of outer/inner L2, L3, L4
	 * and tunnel types. The packet_type is about data really present in the
	 * mbuf. Example: if vlan stripping is enabled, a received vlan packet
	 * would have RTE_PTYPE_L2_ETHER and not RTE_PTYPE_L2_VLAN because the
	 * vlan is stripped from the data.
	 */
	RTE_STD_C11
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
	/** VLAN TCI (CPU order), valid if PKT_RX_VLAN_STRIPPED is set. */
	uint16_t vlan_tci;

	union {
		uint32_t rss;     /**< RSS hash result if RSS enabled */
		struct {
			RTE_STD_C11
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
			 *   PKT_RX_FDIR_* flag in ol_flags.
			 */
		} fdir;           /**< Filter identifier if FDIR enabled */
		struct {
			uint32_t lo;
			uint32_t hi;
		} sched;          /**< Hierarchical scheduler */
		uint32_t usr;
		/**< User defined tags. See rte_distributor_process() */
	} hash;                   /**< hash information */

	/** Outer VLAN TCI (CPU order), valid if PKT_RX_QINQ_STRIPPED is set. */
	uint16_t vlan_tci_outer;

	uint16_t buf_len;         /**< Length of segment buffer. */

	/** Valid if PKT_RX_TIMESTAMP is set. The unit and time reference
	 * are not normalized but are always the same for a given port.
	 */
	uint64_t timestamp;

	/* second cache line - fields only used in slow path or on TX */
	MARKER cacheline1 __rte_cache_min_aligned;

	RTE_STD_C11
	union {
		void *userdata;   /**< Can be used for external metadata */
		uint64_t udata64; /**< Allow 8-byte userdata on 32-bit */
	};

	struct rte_mempool *pool; /**< Pool from which mbuf was allocated. */
	struct rte_mbuf *next;    /**< Next segment of scattered packet. */

	/* fields to support TX offloads */
	RTE_STD_C11
	union {
		uint64_t tx_offload;       /**< combined for easy fetch */
		__extension__
		struct {
			uint64_t l2_len:7;
			/**< L2 (MAC) Header Length for non-tunneling pkt.
			 * Outer_L4_len + ... + Inner_L2_len for tunneling pkt.
			 */
			uint64_t l3_len:9; /**< L3 (IP) Header Length. */
			uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
			uint64_t tso_segsz:16; /**< TCP TSO segment size */

			/* fields for TX offloading of tunnels */
			uint64_t outer_l3_len:9;
			/**< Outer L3 (IP) Hdr Length. */
			uint64_t outer_l2_len:7;
			/**< Outer L2 (MAC) Hdr Length. */

			/* uint64_t unused:8; */
		};
	};

	/** Size of the application private data. In case of an indirect
	 * mbuf, it stores the direct mbuf private data size.
	 */
	uint16_t priv_size;

	/** Timesync flags for use with IEEE1588. */
	uint16_t timesync;

	/** Sequence number. See also rte_reorder_insert(). */
	uint32_t seqn;

} __rte_cache_aligned;


/**
 * Returns TRUE if given mbuf is cloned by mbuf indirection, or FALSE
 * otherwise.
 *
 * If a mbuf has its data in another mbuf and references it by mbuf
 * indirection, this mbuf can be defined as a cloned mbuf.
 */
#define RTE_MBUF_CLONED(mb)     ((mb)->ol_flags & IND_ATTACHED_MBUF)

/**
 * Returns TRUE if given mbuf is direct, or FALSE otherwise.
 *
 * If a mbuf embeds its own data after the rte_mbuf structure, this mbuf
 * can be defined as a direct mbuf.
 */
#define RTE_MBUF_DIRECT(mb)     (!RTE_MBUF_CLONED(mb))

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

#ifdef __cplusplus
}
#endif

#endif /* _MBUF_H_ */
