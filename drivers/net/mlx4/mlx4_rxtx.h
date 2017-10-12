/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef MLX4_RXTX_H_
#define MLX4_RXTX_H_

#include <stdint.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "mlx4.h"

/** Rx queue counters. */
struct mlx4_rxq_stats {
	unsigned int idx; /**< Mapping index. */
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
	uint64_t idropped; /**< Total of packets dropped when Rx ring full. */
	uint64_t rx_nombuf; /**< Total of Rx mbuf allocation failures. */
};

/** Rx element. */
struct rxq_elt {
	struct ibv_recv_wr wr; /**< Work request. */
	struct ibv_sge sge; /**< Scatter/gather element. */
	struct rte_mbuf *buf; /**< Buffer. */
};

/** Rx queue descriptor. */
struct rxq {
	struct priv *priv; /**< Back pointer to private data. */
	struct rte_mempool *mp; /**< Memory pool for allocations. */
	struct ibv_mr *mr; /**< Memory region (for mp). */
	struct ibv_cq *cq; /**< Completion queue. */
	struct ibv_qp *qp; /**< Queue pair. */
	struct ibv_comp_channel *channel; /**< Rx completion channel. */
	unsigned int port_id; /**< Port ID for incoming packets. */
	unsigned int elts_n; /**< (*elts)[] length. */
	unsigned int elts_head; /**< Current index in (*elts)[]. */
	struct rxq_elt (*elts)[]; /**< Rx elements. */
	struct mlx4_rxq_stats stats; /**< Rx queue counters. */
	unsigned int socket; /**< CPU socket ID for allocations. */
};

/** Tx element. */
struct txq_elt {
	struct ibv_send_wr wr; /**< Work request. */
	struct ibv_sge sge; /**< Scatter/gather element. */
	struct rte_mbuf *buf; /**< Buffer. */
};

/** Rx queue counters. */
struct mlx4_txq_stats {
	unsigned int idx; /**< Mapping index. */
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes; /**< Total of successfully sent bytes. */
	uint64_t odropped; /**< Total of packets not sent when Tx ring full. */
};

/** Tx queue descriptor. */
struct txq {
	struct priv *priv; /**< Back pointer to private data. */
	struct {
		const struct rte_mempool *mp; /**< Cached memory pool. */
		struct ibv_mr *mr; /**< Memory region (for mp). */
		uint32_t lkey; /**< mr->lkey copy. */
	} mp2mr[MLX4_PMD_TX_MP_CACHE]; /**< MP to MR translation table. */
	struct ibv_cq *cq; /**< Completion queue. */
	struct ibv_qp *qp; /**< Queue pair. */
	uint32_t max_inline; /**< Max inline send size. */
	unsigned int elts_n; /**< (*elts)[] length. */
	struct txq_elt (*elts)[]; /**< Tx elements. */
	unsigned int elts_head; /**< Current index in (*elts)[]. */
	unsigned int elts_tail; /**< First element awaiting completion. */
	unsigned int elts_comp; /**< Number of completion requests. */
	unsigned int elts_comp_cd; /**< Countdown for next completion. */
	unsigned int elts_comp_cd_init; /**< Initial value for countdown. */
	struct mlx4_txq_stats stats; /**< Tx queue counters. */
	unsigned int socket; /**< CPU socket ID for allocations. */
};

/* mlx4_rxq.c */

void mlx4_rxq_cleanup(struct rxq *rxq);
int mlx4_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			uint16_t desc, unsigned int socket,
			const struct rte_eth_rxconf *conf,
			struct rte_mempool *mp);
void mlx4_rx_queue_release(void *dpdk_rxq);

/* mlx4_rxtx.c */

uint32_t mlx4_txq_mp2mr(struct txq *txq, struct rte_mempool *mp);
uint16_t mlx4_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts,
		       uint16_t pkts_n);
uint16_t mlx4_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts,
		       uint16_t pkts_n);
uint16_t mlx4_tx_burst_removed(void *dpdk_txq, struct rte_mbuf **pkts,
			       uint16_t pkts_n);
uint16_t mlx4_rx_burst_removed(void *dpdk_rxq, struct rte_mbuf **pkts,
			       uint16_t pkts_n);

/* mlx4_txq.c */

void mlx4_txq_cleanup(struct txq *txq);
int mlx4_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			uint16_t desc, unsigned int socket,
			const struct rte_eth_txconf *conf);
void mlx4_tx_queue_release(void *dpdk_txq);

#endif /* MLX4_RXTX_H_ */
