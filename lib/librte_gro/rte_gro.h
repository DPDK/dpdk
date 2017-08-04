/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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
#define RTE_GRO_TYPE_SUPPORT_NUM 1
/**< the number of currently supported GRO types */

#define RTE_GRO_TCP_IPV4_INDEX 0
#define RTE_GRO_TCP_IPV4 (1ULL << RTE_GRO_TCP_IPV4_INDEX)
/**< TCP/IPv4 GRO flag */

/**
 * A structure which is used to create GRO context objects or tell
 * rte_gro_reassemble_burst() what reassembly rules are demanded.
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
 * packets at a time. It assumes that all inputted packets are with
 * correct checksums. That is, applications should guarantee all
 * inputted packets are correct. Besides, it doesn't re-calculate
 * checksums for merged packets. If inputted packets are IP fragmented,
 * this function assumes them are complete (i.e. with L4 header). After
 * finishing processing, it returns all GROed packets to applications
 * immediately.
 *
 * @param pkts
 *  a pointer array which points to the packets to reassemble. Besides,
 *  it keeps mbuf addresses for the GROed packets.
 * @param nb_pkts
 *  the number of packets to reassemble.
 * @param param
 *  applications use it to tell rte_gro_reassemble_burst() what rules
 *  are demanded.
 *
 * @return
 *  the number of packets after been GROed. If no packets are merged,
 *  the returned value is nb_pkts.
 */
uint16_t rte_gro_reassemble_burst(struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		const struct rte_gro_param *param);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Reassembly function, which tries to merge inputted packets with
 * the packets in the reassembly tables of a given GRO context. This
 * function assumes all inputted packets are with correct checksums.
 * And it won't update checksums if two packets are merged. Besides,
 * if inputted packets are IP fragmented, this function assumes they
 * are complete packets (i.e. with L4 header).
 *
 * If the inputted packets don't have data or are with unsupported GRO
 * types etc., they won't be processed and are returned to applications.
 * Otherwise, the inputted packets are either merged or inserted into
 * the table. If applications want get packets in the table, they need
 * to call flush API.
 *
 * @param pkts
 *  packet to reassemble. Besides, after this function finishes, it
 *  keeps the unprocessed packets (e.g. without data or unsupported
 *  GRO types).
 * @param nb_pkts
 *  the number of packets to reassemble.
 * @param ctx
 *  a pointer points to a GRO context object.
 *
 * @return
 *  return the number of unprocessed packets (e.g. without data or
 *  unsupported GRO types). If all packets are processed (merged or
 *  inserted into the table), return 0.
 */
uint16_t rte_gro_reassemble(struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		void *ctx);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This function flushes the timeout packets from reassembly tables of
 * desired GRO types. The max number of flushed timeout packets is the
 * element number of the array which is used to keep the flushed packets.
 *
 * Besides, this function won't re-calculate checksums for merged
 * packets in the tables. That is, the returned packets may be with
 * wrong checksums.
 *
 * @param ctx
 *  a pointer points to a GRO context object.
 * @param timeout_cycles
 *  max TTL for packets in reassembly tables, measured in nanosecond.
 * @param gro_types
 *  this function only flushes packets which belong to the GRO types
 *  specified by gro_types.
 * @param out
 *  a pointer array that is used to keep flushed timeout packets.
 * @param max_nb_out
 *  the element number of out. It's also the max number of timeout
 *  packets that can be flushed finally.
 *
 * @return
 *  the number of flushed packets. If no packets are flushed, return 0.
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
 *  pointer points to a GRO context object.
 *
 * @return
 *  the number of packets in all reassembly tables.
 */
uint64_t rte_gro_get_pkt_count(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRO_H_ */
