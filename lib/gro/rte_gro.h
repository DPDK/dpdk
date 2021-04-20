/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_GRO_H_
#define _RTE_GRO_H_

/**
 * @file
 * Interface to GRO library
 */

#include <stdint.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_GRO_MAX_BURST_ITEM_NUM 128U
/**< the max number of packets that rte_gro_reassemble_burst()
 * can process in each invocation.
 */
#define RTE_GRO_TYPE_MAX_NUM 64
/**< the max number of supported GRO types */
#define RTE_GRO_TYPE_SUPPORT_NUM 2
/**< the number of currently supported GRO types */

#define RTE_GRO_TCP_IPV4_INDEX 0
#define RTE_GRO_TCP_IPV4 (1ULL << RTE_GRO_TCP_IPV4_INDEX)
/**< TCP/IPv4 GRO flag */
#define RTE_GRO_IPV4_VXLAN_TCP_IPV4_INDEX 1
#define RTE_GRO_IPV4_VXLAN_TCP_IPV4 (1ULL << RTE_GRO_IPV4_VXLAN_TCP_IPV4_INDEX)
/**< VxLAN TCP/IPv4 GRO flag. */
#define RTE_GRO_UDP_IPV4_INDEX 2
#define RTE_GRO_UDP_IPV4 (1ULL << RTE_GRO_UDP_IPV4_INDEX)
/**< UDP/IPv4 GRO flag */
#define RTE_GRO_IPV4_VXLAN_UDP_IPV4_INDEX 3
#define RTE_GRO_IPV4_VXLAN_UDP_IPV4 (1ULL << RTE_GRO_IPV4_VXLAN_UDP_IPV4_INDEX)
/**< VxLAN UDP/IPv4 GRO flag. */

/**
 * Structure used to create GRO context objects or used to pass
 * application-determined parameters to rte_gro_reassemble_burst().
 */
struct rte_gro_param {
	uint64_t gro_types;
	/**< desired GRO types */
	uint16_t max_flow_num;
	/**< max flow number */
	uint16_t max_item_per_flow;
	/**< max packet number per flow */
	uint16_t socket_id;
	/**< socket index for allocating GRO related data structures,
	 * like reassembly tables. When use rte_gro_reassemble_burst(),
	 * applications don't need to set this value.
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This function create a GRO context object, which is used to merge
 * packets in rte_gro_reassemble().
 *
 * @param param
 *  applications use it to pass needed parameters to create a GRO
 *  context object.
 *
 * @return
 *  if create successfully, return a pointer which points to the GRO
 *  context object. Otherwise, return NULL.
 */
void *rte_gro_ctx_create(const struct rte_gro_param *param);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This function destroys a GRO context object.
 *
 * @param ctx
 *  pointer points to a GRO context object.
 */
void rte_gro_ctx_destroy(void *ctx);

/**
 * This is one of the main reassembly APIs, which merges numbers of
 * packets at a time. It doesn't check if input packets have correct
 * checksums and doesn't re-calculate checksums for merged packets.
 * It assumes the packets are complete (i.e., MF==0 && frag_off==0),
 * when IP fragmentation is possible (i.e., DF==0). The GROed packets
 * are returned as soon as the function finishes.
 *
 * @param pkts
 *  Pointer array pointing to the packets to reassemble. Besides, it
 *  keeps MBUF addresses for the GROed packets.
 * @param nb_pkts
 *  The number of packets to reassemble
 * @param param
 *  Application-determined parameters for reassembling packets.
 *
 * @return
 *  The number of packets after been GROed. If no packets are merged,
 *  the return value is equals to nb_pkts.
 */
uint16_t rte_gro_reassemble_burst(struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		const struct rte_gro_param *param);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Reassembly function, which tries to merge input packets with the
 * existed packets in the reassembly tables of a given GRO context.
 * It doesn't check if input packets have correct checksums and doesn't
 * re-calculate checksums for merged packets. Additionally, it assumes
 * the packets are complete (i.e., MF==0 && frag_off==0), when IP
 * fragmentation is possible (i.e., DF==0).
 *
 * If the input packets have invalid parameters (e.g. no data payload,
 * unsupported GRO types), they are returned to applications. Otherwise,
 * they are either merged or inserted into the table. Applications need
 * to flush packets from the tables by flush API, if they want to get the
 * GROed packets.
 *
 * @param pkts
 *  Packets to reassemble. It's also used to store the unprocessed packets.
 * @param nb_pkts
 *  The number of packets to reassemble
 * @param ctx
 *  GRO context object pointer
 *
 * @return
 *  The number of unprocessed packets.
 */
uint16_t rte_gro_reassemble(struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		void *ctx);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This function flushes the timeout packets from the reassembly tables
 * of desired GRO types. The max number of flushed packets is the
 * element number of 'out'.
 *
 * Additionally, the flushed packets may have incorrect checksums, since
 * this function doesn't re-calculate checksums for merged packets.
 *
 * @param ctx
 *  GRO context object pointer.
 * @param timeout_cycles
 *  The max TTL for packets in reassembly tables, measured in nanosecond.
 * @param gro_types
 *  This function flushes packets whose GRO types are specified by
 *  gro_types.
 * @param out
 *  Pointer array used to keep flushed packets.
 * @param max_nb_out
 *  The element number of 'out'. It's also the max number of timeout
 *  packets that can be flushed finally.
 *
 * @return
 *  The number of flushed packets.
 */
uint16_t rte_gro_timeout_flush(void *ctx,
		uint64_t timeout_cycles,
		uint64_t gro_types,
		struct rte_mbuf **out,
		uint16_t max_nb_out);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This function returns the number of packets in all reassembly tables
 * of a given GRO context.
 *
 * @param ctx
 *  GRO context object pointer.
 *
 * @return
 *  The number of packets in the tables.
 */
uint64_t rte_gro_get_pkt_count(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRO_H_ */
