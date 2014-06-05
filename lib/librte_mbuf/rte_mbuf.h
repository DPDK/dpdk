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
 * This library provide an API to allocate/free mbufs, manipulate
 * control message buffer (ctrlmbuf), which are generic message
 * buffers, and packet buffers (pktmbuf), which are used to carry
 * network packets.
 *
 * To understand the concepts of packet buffers or mbufs, you
 * should read "TCP/IP Illustrated, Volume 2: The Implementation,
 * Addison-Wesley, 1995, ISBN 0-201-63354-X from Richard Stevens"
 * http://www.kohala.com/start/tcpipiv2.html
 *
 * The main modification of this implementation is the use of mbuf for
 * transports other than packets. mbufs can have other types.
 */

#include <stdint.h>
#include <rte_mempool.h>
#include <rte_atomic.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A control message buffer.
 */
struct rte_ctrlmbuf {
	void *data;        /**< Pointer to data. */
	uint32_t data_len; /**< Length of data. */
};


/*
 * Packet Offload Features Flags. It also carry packet type information.
 * Critical resources. Both rx/tx shared these bits. Be cautious on any change
 */
#define PKT_RX_VLAN_PKT      0x0001 /**< RX packet is a 802.1q VLAN packet. */
#define PKT_RX_RSS_HASH      0x0002 /**< RX packet with RSS hash result. */
#define PKT_RX_FDIR          0x0004 /**< RX packet with FDIR infos. */
#define PKT_RX_L4_CKSUM_BAD  0x0008 /**< L4 cksum of RX pkt. is not OK. */
#define PKT_RX_IP_CKSUM_BAD  0x0010 /**< IP cksum of RX pkt. is not OK. */
#define PKT_RX_EIP_CKSUM_BAD 0x0000 /**< External IP header checksum error. */
#define PKT_RX_OVERSIZE      0x0000 /**< Num of desc of an RX pkt oversize. */
#define PKT_RX_HBUF_OVERFLOW 0x0000 /**< Header buffer overflow. */
#define PKT_RX_RECIP_ERR     0x0000 /**< Hardware processing error. */
#define PKT_RX_MAC_ERR       0x0000 /**< MAC error. */
#define PKT_RX_IPV4_HDR      0x0020 /**< RX packet with IPv4 header. */
#define PKT_RX_IPV4_HDR_EXT  0x0040 /**< RX packet with extended IPv4 header. */
#define PKT_RX_IPV6_HDR      0x0080 /**< RX packet with IPv6 header. */
#define PKT_RX_IPV6_HDR_EXT  0x0100 /**< RX packet with extended IPv6 header. */
#define PKT_RX_IEEE1588_PTP  0x0200 /**< RX IEEE1588 L2 Ethernet PT Packet. */
#define PKT_RX_IEEE1588_TMST 0x0400 /**< RX IEEE1588 L2/L4 timestamped packet.*/

#define PKT_TX_VLAN_PKT      0x0800 /**< TX packet is a 802.1q VLAN packet. */
#define PKT_TX_IP_CKSUM      0x1000 /**< IP cksum of TX pkt. computed by NIC. */
#define PKT_TX_IPV4_CSUM     0x1000 /**< Alias of PKT_TX_IP_CKSUM. */
#define PKT_TX_IPV4          PKT_RX_IPV4_HDR /**< IPv4 with no IP checksum offload. */
#define PKT_TX_IPV6          PKT_RX_IPV6_HDR /**< IPv6 packet */
/*
 * Bit 14~13 used for L4 packet type with checksum enabled.
 *     00: Reserved
 *     01: TCP checksum
 *     10: SCTP checksum
 *     11: UDP checksum
 */
#define PKT_TX_L4_MASK       0x6000 /**< Mask bits for L4 checksum offload request. */
#define PKT_TX_L4_NO_CKSUM   0x0000 /**< Disable L4 cksum of TX pkt. */
#define PKT_TX_TCP_CKSUM     0x2000 /**< TCP cksum of TX pkt. computed by NIC. */
#define PKT_TX_SCTP_CKSUM    0x4000 /**< SCTP cksum of TX pkt. computed by NIC. */
#define PKT_TX_UDP_CKSUM     0x6000 /**< UDP cksum of TX pkt. computed by NIC. */
/* Bit 15 */
#define PKT_TX_IEEE1588_TMST 0x8000 /**< TX IEEE1588 packet to timestamp. */

/**
 * Bit Mask to indicate what bits required for building TX context
 */
#define PKT_TX_OFFLOAD_MASK (PKT_TX_VLAN_PKT | PKT_TX_IP_CKSUM | PKT_TX_L4_MASK)

/** Offload features */
union rte_vlan_macip {
	uint32_t data;
	struct {
		uint16_t l3_len:9; /**< L3 (IP) Header Length. */
		uint16_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint16_t vlan_tci;
		/**< VLAN Tag Control Identifier (CPU order). */
	} f;
};

/*
 * Compare mask for vlan_macip_len.data,
 * should be in sync with rte_vlan_macip.f layout.
 * */
#define TX_VLAN_CMP_MASK        0xFFFF0000  /**< VLAN length - 16-bits. */
#define TX_MAC_LEN_CMP_MASK     0x0000FE00  /**< MAC length - 7-bits. */
#define TX_IP_LEN_CMP_MASK      0x000001FF  /**< IP  length - 9-bits. */
/**< MAC+IP  length. */
#define TX_MACIP_LEN_CMP_MASK   (TX_MAC_LEN_CMP_MASK | TX_IP_LEN_CMP_MASK)

/**
 * A packet message buffer.
 */
struct rte_pktmbuf {
	/* valid for any segment */
	struct rte_mbuf *next;  /**< Next segment of scattered packet. */
	void* data;             /**< Start address of data in segment buffer. */
	uint16_t data_len;      /**< Amount of data in segment buffer. */

	/* these fields are valid for first segment only */
	uint8_t nb_segs;        /**< Number of segments. */
	uint8_t in_port;        /**< Input port. */
	uint32_t pkt_len;       /**< Total pkt len: sum of all segment data_len. */

	/* offload features */
	union rte_vlan_macip vlan_macip;
	union {
		uint32_t rss;       /**< RSS hash result if RSS enabled */
		struct {
			uint16_t hash;
			uint16_t id;
		} fdir;             /**< Filter identifier if FDIR enabled */
		uint32_t sched;     /**< Hierarchical scheduler */
	} hash;                 /**< hash information */
};

/**
 * This enum indicates the mbuf type.
 */
enum rte_mbuf_type {
	RTE_MBUF_CTRL,  /**< Control mbuf. */
	RTE_MBUF_PKT,   /**< Packet mbuf. */
};

/**
 * The generic rte_mbuf, containing a packet mbuf or a control mbuf.
 */
struct rte_mbuf {
	struct rte_mempool *pool; /**< Pool from which mbuf was allocated. */
	void *buf_addr;           /**< Virtual address of segment buffer. */
	phys_addr_t buf_physaddr; /**< Physical address of segment buffer. */
	uint16_t buf_len;         /**< Length of segment buffer. */
#ifdef RTE_MBUF_SCATTER_GATHER
	/**
	 * 16-bit Reference counter.
	 * It should only be accessed using the following functions:
	 * rte_mbuf_refcnt_update(), rte_mbuf_refcnt_read(), and
	 * rte_mbuf_refcnt_set(). The functionality of these functions (atomic,
	 * or non-atomic) is controlled by the CONFIG_RTE_MBUF_REFCNT_ATOMIC
	 * config option.
	 */
	union {
		rte_atomic16_t refcnt_atomic;   /**< Atomically accessed refcnt */
		uint16_t refcnt;                /**< Non-atomically accessed refcnt */
	};
#else
	uint16_t refcnt_reserved;     /**< Do not use this field */
#endif
	uint8_t type;                 /**< Type of mbuf. */
	uint8_t reserved;             /**< Unused field. Required for padding. */
	uint16_t ol_flags;            /**< Offload features. */

	union {
		struct rte_ctrlmbuf ctrl;
		struct rte_pktmbuf pkt;
	};

	union {
		uint8_t metadata[0];
		uint16_t metadata16[0];
		uint32_t metadata32[0];
		uint64_t metadata64[0];
	};
} __rte_cache_aligned;

#define RTE_MBUF_METADATA_UINT8(mbuf, offset)              \
	(mbuf->metadata[offset])
#define RTE_MBUF_METADATA_UINT16(mbuf, offset)             \
	(mbuf->metadata16[offset/sizeof(uint16_t)])
#define RTE_MBUF_METADATA_UINT32(mbuf, offset)             \
	(mbuf->metadata32[offset/sizeof(uint32_t)])
#define RTE_MBUF_METADATA_UINT64(mbuf, offset)             \
	(mbuf->metadata64[offset/sizeof(uint64_t)])

#define RTE_MBUF_METADATA_UINT8_PTR(mbuf, offset)          \
	(&mbuf->metadata[offset])
#define RTE_MBUF_METADATA_UINT16_PTR(mbuf, offset)         \
	(&mbuf->metadata16[offset/sizeof(uint16_t)])
#define RTE_MBUF_METADATA_UINT32_PTR(mbuf, offset)         \
	(&mbuf->metadata32[offset/sizeof(uint32_t)])
#define RTE_MBUF_METADATA_UINT64_PTR(mbuf, offset)         \
	(&mbuf->metadata64[offset/sizeof(uint64_t)])

/**
 * Given the buf_addr returns the pointer to corresponding mbuf.
 */
#define RTE_MBUF_FROM_BADDR(ba)     (((struct rte_mbuf *)(ba)) - 1)

/**
 * Given the pointer to mbuf returns an address where it's  buf_addr
 * should point to.
 */
#define RTE_MBUF_TO_BADDR(mb)       (((struct rte_mbuf *)(mb)) + 1)

/**
 * Returns TRUE if given mbuf is indirect, or FALSE otherwise.
 */
#define RTE_MBUF_INDIRECT(mb)   (RTE_MBUF_FROM_BADDR((mb)->buf_addr) != (mb))

/**
 * Returns TRUE if given mbuf is direct, or FALSE otherwise.
 */
#define RTE_MBUF_DIRECT(mb)     (RTE_MBUF_FROM_BADDR((mb)->buf_addr) == (mb))


/**
 * Private data in case of pktmbuf pool.
 *
 * A structure that contains some pktmbuf_pool-specific data that are
 * appended after the mempool structure (in private data).
 */
struct rte_pktmbuf_pool_private {
	uint16_t mbuf_data_room_size; /**< Size of data space in each mbuf.*/
};

#ifdef RTE_LIBRTE_MBUF_DEBUG

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, t, is_h) rte_mbuf_sanity_check(m, t, is_h)

/**  check mbuf type in debug mode if mbuf pointer is not null */
#define __rte_mbuf_sanity_check_raw(m, t, is_h)	do {       \
	if ((m) != NULL)                                   \
		rte_mbuf_sanity_check(m, t, is_h);          \
} while (0)

/**  MBUF asserts in debug mode */
#define RTE_MBUF_ASSERT(exp)                                         \
if (!(exp)) {                                                        \
	rte_panic("line%d\tassert \"" #exp "\" failed\n", __LINE__); \
}

#else /*  RTE_LIBRTE_MBUF_DEBUG */

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, t, is_h) do { } while(0)

/**  check mbuf type in debug mode if mbuf pointer is not null */
#define __rte_mbuf_sanity_check_raw(m, t, is_h) do { } while(0)

/**  MBUF asserts in debug mode */
#define RTE_MBUF_ASSERT(exp)                do { } while(0)

#endif /*  RTE_LIBRTE_MBUF_DEBUG */

#ifdef RTE_MBUF_SCATTER_GATHER
#ifdef RTE_MBUF_REFCNT_ATOMIC

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
	return (uint16_t)(rte_atomic16_add_return(&m->refcnt_atomic, value));
}

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

#else /* ! RTE_MBUF_SCATTER_GATHER */

/** Mbuf prefetch */
#define RTE_MBUF_PREFETCH_TO_FREE(m) do { } while(0)

#define rte_mbuf_refcnt_set(m,v) do { } while(0)

#endif /* RTE_MBUF_SCATTER_GATHER */


/**
 * Sanity checks on an mbuf.
 *
 * Check the consistency of the given mbuf. The function will cause a
 * panic if corruption is detected.
 *
 * @param m
 *   The mbuf to be checked.
 * @param t
 *   The expected type of the mbuf.
 * @param is_header
 *   True if the mbuf is a packet header, false if it is a sub-segment
 *   of a packet (in this case, some fields like nb_segs are not checked)
 */
void
rte_mbuf_sanity_check(const struct rte_mbuf *m, enum rte_mbuf_type t,
		      int is_header);

/**
 * @internal Allocate a new mbuf from mempool *mp*.
 * The use of that function is reserved for RTE internal needs.
 * Please use either rte_ctrlmbuf_alloc() or rte_pktmbuf_alloc().
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
#ifdef RTE_MBUF_SCATTER_GATHER
	RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
	rte_mbuf_refcnt_set(m, 1);
#endif /* RTE_MBUF_SCATTER_GATHER */
	return (m);
}

/**
 * @internal Put mbuf back into its original mempool.
 * The use of that function is reserved for RTE internal needs.
 * Please use either rte_ctrlmbuf_free() or rte_pktmbuf_free().
 *
 * @param m
 *   The mbuf to be freed.
 */
static inline void __attribute__((always_inline))
__rte_mbuf_raw_free(struct rte_mbuf *m)
{
#ifdef RTE_MBUF_SCATTER_GATHER
	RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
#endif /* RTE_MBUF_SCATTER_GATHER */
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
static inline struct rte_mbuf *rte_ctrlmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;
	if ((m = __rte_mbuf_raw_alloc(mp)) != NULL) {
		m->ctrl.data = m->buf_addr;
		m->ctrl.data_len = 0;
		__rte_mbuf_sanity_check(m, RTE_MBUF_CTRL, 0);
	}
	return (m);
}

/**
 * Free a control mbuf back into its original mempool.
 *
 * @param m
 *   The control mbuf to be freed.
 */
static inline void rte_ctrlmbuf_free(struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, RTE_MBUF_CTRL, 0);
#ifdef RTE_MBUF_SCATTER_GATHER
	if (rte_mbuf_refcnt_update(m, -1) == 0)
#endif /* RTE_MBUF_SCATTER_GATHER */
		__rte_mbuf_raw_free(m);
}

/**
 * A macro that returns the pointer to the carried data.
 *
 * The value that can be read or assigned.
 *
 * @param m
 *   The control mbuf.
 */
#define rte_ctrlmbuf_data(m) ((m)->ctrl.data)

/**
 * A macro that returns the length of the carried data.
 *
 * The value that can be read or assigned.
 *
 * @param m
 *   The control mbuf.
 */
#define rte_ctrlmbuf_len(m) ((m)->ctrl.data_len)

/* Operations on pkt mbuf */

/**
 * The packet mbuf constructor.
 *
 * This function initializes some fields in the mbuf structure that are not
 * modified by the user once created (mbuf type, origin pool, buffer start
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
 * Reset the fields of a packet mbuf to their default values.
 *
 * The given mbuf must have only one segment.
 *
 * @param m
 *   The packet mbuf to be resetted.
 */
static inline void rte_pktmbuf_reset(struct rte_mbuf *m)
{
	uint32_t buf_ofs;

	m->pkt.next = NULL;
	m->pkt.pkt_len = 0;
	m->pkt.vlan_macip.data = 0;
	m->pkt.nb_segs = 1;
	m->pkt.in_port = 0xff;

	m->ol_flags = 0;
	buf_ofs = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
			RTE_PKTMBUF_HEADROOM : m->buf_len;
	m->pkt.data = (char*) m->buf_addr + buf_ofs;

	m->pkt.data_len = 0;
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);
}

/**
 * Allocate a new mbuf (type is pkt) from a mempool.
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
	return (m);
}

#ifdef RTE_MBUF_SCATTER_GATHER

/**
 * Attach packet mbuf to another packet mbuf.
 * After attachment we refer the mbuf we attached as 'indirect',
 * while mbuf we attached to as 'direct'.
 * Right now, not supported:
 *  - attachment to indirect mbuf (e.g. - md  has to be direct).
 *  - attachment for already indirect mbuf (e.g. - mi has to be direct).
 *  - mbuf we trying to attach (mi) is used by someone else
 *    e.g. it's reference counter is greater then 1.
 *
 * @param mi
 *   The indirect packet mbuf.
 * @param md
 *   The direct packet mbuf.
 */

static inline void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *md)
{
	RTE_MBUF_ASSERT(RTE_MBUF_DIRECT(md) &&
	    RTE_MBUF_DIRECT(mi) &&
	    rte_mbuf_refcnt_read(mi) == 1);

	rte_mbuf_refcnt_update(md, 1);
	mi->buf_physaddr = md->buf_physaddr;
	mi->buf_addr = md->buf_addr;
	mi->buf_len = md->buf_len;

	mi->pkt = md->pkt;

	mi->pkt.next = NULL;
	mi->pkt.pkt_len = mi->pkt.data_len;
	mi->pkt.nb_segs = 1;
	mi->ol_flags = md->ol_flags;

	__rte_mbuf_sanity_check(mi, RTE_MBUF_PKT, 1);
	__rte_mbuf_sanity_check(md, RTE_MBUF_PKT, 0);
}

/**
 * Detach an indirect packet mbuf -
 *  - restore original mbuf address and length values.
 *  - reset pktmbuf data and data_len to their default values.
 *  All other fields of the given packet mbuf will be left intact.
 *
 * @param m
 *   The indirect attached packet mbuf.
 */

static inline void rte_pktmbuf_detach(struct rte_mbuf *m)
{
	const struct rte_mempool *mp = m->pool;
	void *buf = RTE_MBUF_TO_BADDR(m);
	uint32_t buf_ofs;
	uint32_t buf_len = mp->elt_size - sizeof(*m);
	m->buf_physaddr = rte_mempool_virt2phy(mp, m) + sizeof (*m);

	m->buf_addr = buf;
	m->buf_len = (uint16_t)buf_len;

	buf_ofs = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
			RTE_PKTMBUF_HEADROOM : m->buf_len;
	m->pkt.data = (char*) m->buf_addr + buf_ofs;

	m->pkt.data_len = 0;
}

#endif /* RTE_MBUF_SCATTER_GATHER */


static inline struct rte_mbuf* __attribute__((always_inline))
__rte_pktmbuf_prefree_seg(struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 0);

#ifdef RTE_MBUF_SCATTER_GATHER
	if (likely (rte_mbuf_refcnt_read(m) == 1) ||
			likely (rte_mbuf_refcnt_update(m, -1) == 0)) {
		struct rte_mbuf *md = RTE_MBUF_FROM_BADDR(m->buf_addr);

		rte_mbuf_refcnt_set(m, 0);

		/* if this is an indirect mbuf, then
		 *  - detach mbuf
		 *  - free attached mbuf segment
		 */
		if (unlikely (md != m)) {
			rte_pktmbuf_detach(m);
			if (rte_mbuf_refcnt_update(md, -1) == 0)
				__rte_mbuf_raw_free(md);
		}
#endif
		return(m);
#ifdef RTE_MBUF_SCATTER_GATHER
	}
	return (NULL);
#endif
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
	if (likely(NULL != (m = __rte_pktmbuf_prefree_seg(m))))
		__rte_mbuf_raw_free(m);
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

	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);

	while (m != NULL) {
		m_next = m->pkt.next;
		rte_pktmbuf_free_seg(m);
		m = m_next;
	}
}

#ifdef RTE_MBUF_SCATTER_GATHER

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
		return (NULL);

	mi = mc;
	prev = &mi->pkt.next;
	pktlen = md->pkt.pkt_len;
	nseg = 0;

	do {
		nseg++;
		rte_pktmbuf_attach(mi, md);
		*prev = mi;
		prev = &mi->pkt.next;
	} while ((md = md->pkt.next) != NULL &&
	    (mi = rte_pktmbuf_alloc(mp)) != NULL);

	*prev = NULL;
	mc->pkt.nb_segs = nseg;
	mc->pkt.pkt_len = pktlen;

	/* Allocation of new indirect segment failed */
	if (unlikely (mi == NULL)) {
		rte_pktmbuf_free(mc);
		return (NULL);
	}

	__rte_mbuf_sanity_check(mc, RTE_MBUF_PKT, 1);
	return (mc);
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
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);

	do {
		rte_mbuf_refcnt_update(m, v);
	} while ((m = m->pkt.next) != NULL);
}

#endif /* RTE_MBUF_SCATTER_GATHER */

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
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);
	return (uint16_t) ((char*) m->pkt.data - (char*) m->buf_addr);
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
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);
	return (uint16_t)(m->buf_len - rte_pktmbuf_headroom(m) -
			  m->pkt.data_len);
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

	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);
	while (m2->pkt.next != NULL)
		m2 = m2->pkt.next;
	return m2;
}

/**
 * A macro that points to the start of the data in the mbuf.
 *
 * The returned pointer is cast to type t. Before using this
 * function, the user must ensure that m_headlen(m) is large enough to
 * read its data.
 *
 * @param m
 *   The packet mbuf.
 * @param t
 *   The type to cast the result into.
 */
#define rte_pktmbuf_mtod(m, t) ((t)((m)->pkt.data))

/**
 * A macro that returns the length of the packet.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_pkt_len(m) ((m)->pkt.pkt_len)

/**
 * A macro that returns the length of the segment.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_data_len(m) ((m)->pkt.data_len)

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
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);

	if (unlikely(len > rte_pktmbuf_headroom(m)))
		return NULL;

	m->pkt.data = (char*) m->pkt.data - len;
	m->pkt.data_len = (uint16_t)(m->pkt.data_len + len);
	m->pkt.pkt_len  = (m->pkt.pkt_len + len);

	return (char*) m->pkt.data;
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

	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > rte_pktmbuf_tailroom(m_last)))
		return NULL;

	tail = (char*) m_last->pkt.data + m_last->pkt.data_len;
	m_last->pkt.data_len = (uint16_t)(m_last->pkt.data_len + len);
	m->pkt.pkt_len  = (m->pkt.pkt_len + len);
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
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);

	if (unlikely(len > m->pkt.data_len))
		return NULL;

	m->pkt.data_len = (uint16_t)(m->pkt.data_len - len);
	m->pkt.data = ((char*) m->pkt.data + len);
	m->pkt.pkt_len  = (m->pkt.pkt_len - len);
	return (char*) m->pkt.data;
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

	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > m_last->pkt.data_len))
		return -1;

	m_last->pkt.data_len = (uint16_t)(m_last->pkt.data_len - len);
	m->pkt.pkt_len  = (m->pkt.pkt_len - len);
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
	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);
	return !!(m->pkt.nb_segs == 1);
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
