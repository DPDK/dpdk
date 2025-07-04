/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_GSO_H_
#define _RTE_GSO_H_

/**
 * @file
 * Interface to GSO library
 */

#include <stdint.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimum GSO segment size for TCP based packets. */
#define RTE_GSO_SEG_SIZE_MIN (sizeof(struct rte_ether_hdr) + \
		sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) + 1)

/* Minimum GSO segment size for UDP based packets. */
#define RTE_GSO_UDP_SEG_SIZE_MIN (sizeof(struct rte_ether_hdr) + \
		sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + 1)

/* GSO flags for rte_gso_ctx. */
#define RTE_GSO_FLAG_IPID_FIXED (1ULL << 0)
/**< Use fixed IP ids for output GSO segments. Setting
 * 0 indicates using incremental IP ids.
 */

/**
 * GSO context structure.
 */
struct rte_gso_ctx {
	struct rte_mempool *direct_pool;
	/**< MBUF pool for allocating direct buffers, which are used
	 * to store packet headers for GSO segments.
	 */
	struct rte_mempool *indirect_pool;
	/**< MBUF pool for allocating indirect buffers, which are used
	 * to locate packet payloads for GSO segments. The indirect
	 * buffer doesn't contain any data, but simply points to an
	 * offset within the packet to segment.
	 */
	uint64_t flag;
	/**< flag that controls specific attributes of output segments,
	 * such as the type of IP ID generated (i.e. fixed or incremental).
	 */
	uint32_t gso_types;
	/**< the bit mask of required GSO types. The GSO library
	 * uses the same macros as that of describing device TX
	 * offloading capabilities (i.e. RTE_ETH_TX_OFFLOAD_*_TSO) for
	 * gso_types.
	 *
	 * For example, if applications want to segment TCP/IPv4
	 * packets, set RTE_ETH_TX_OFFLOAD_TCP_TSO in gso_types.
	 */
	uint16_t gso_size;
	/**< maximum size of an output GSO segment, including packet
	 * header and payload, measured in bytes. Must exceed
	 * RTE_GSO_SEG_SIZE_MIN.
	 */
};

/**
 * Segmentation function, which supports processing of both single- and
 * multi- MBUF packets.
 *
 * Note that we refer to the packets that are segmented from the input
 * packet as 'GSO segments'. rte_gso_segment() doesn't check if the
 * input packet has correct checksums, and doesn't update checksums for
 * output GSO segments. Additionally, it doesn't process IP fragment
 * packets.
 *
 * Before calling rte_gso_segment(), applications must set proper ol_flags
 * for the packet. The GSO library uses the same macros as that of TSO.
 * For example, set RTE_MBUF_F_TX_TCP_SEG and RTE_MBUF_F_TX_IPV4 in ol_flags to segment
 * a TCP/IPv4 packet. If rte_gso_segment() succeeds, the RTE_MBUF_F_TX_TCP_SEG
 * flag is removed for all GSO segments and the input packet.
 *
 * Each of the newly-created GSO segments is organized as a two-segment
 * MBUF, where the first segment is a standard MBUF, which stores a copy
 * of packet header, and the second is an indirect MBUF which points to
 * a section of data in the input packet. Since each GSO segment has
 * multiple MBUFs (i.e. typically 2 MBUFs), the driver of the interface which
 * the GSO segments are sent to should support transmission of multi-segment
 * packets.
 *
 * If the input packet is GSO'd, all the indirect segments are attached to the
 * input packet.
 *
 * rte_gso_segment() will not free the input packet no matter whether it is
 * GSO'd or not, the application should free it after calling rte_gso_segment().
 *
 * If the memory space in pkts_out or MBUF pools is insufficient, this
 * function fails, and it returns (-1) * errno. Otherwise, GSO succeeds,
 * and this function returns the number of output GSO segments filled in
 * pkts_out.
 *
 * @param pkt
 *  The packet mbuf to segment.
 * @param ctx
 *  GSO context object pointer.
 * @param pkts_out
 *  Pointer array used to store the MBUF addresses of output GSO
 *  segments, when rte_gso_segment() succeeds.
 * @param nb_pkts_out
 *  The max number of items that pkts_out can keep.
 *
 * @return
 *  - The number of GSO segments filled in pkts_out on success.
 *  - Return 0 if it does not need to be GSO'd.
 *  - Return -ENOMEM if run out of memory in MBUF pools.
 *  - Return -ENOTSUP for protocols that can not be segmented.
 *  - Return -EINVAL for invalid parameters.
 */
int rte_gso_segment(struct rte_mbuf *pkt,
		const struct rte_gso_ctx *ctx,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out);
#ifdef __cplusplus
}
#endif

#endif /* _RTE_GSO_H_ */
