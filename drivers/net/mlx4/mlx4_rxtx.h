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
#include <sys/queue.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx4dv.h>
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "mlx4.h"
#include "mlx4_prm.h"

/** Rx queue counters. */
struct mlx4_rxq_stats {
	unsigned int idx; /**< Mapping index. */
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
	uint64_t idropped; /**< Total of packets dropped when Rx ring full. */
	uint64_t rx_nombuf; /**< Total of Rx mbuf allocation failures. */
};

/** Rx queue descriptor. */
struct rxq {
	struct priv *priv; /**< Back pointer to private data. */
	struct rte_mempool *mp; /**< Memory pool for allocations. */
	struct mlx4_mr *mr; /**< Memory region. */
	struct ibv_cq *cq; /**< Completion queue. */
	struct ibv_wq *wq; /**< Work queue. */
	struct ibv_comp_channel *channel; /**< Rx completion channel. */
	uint16_t rq_ci; /**< Saved RQ consumer index. */
	uint16_t port_id; /**< Port ID for incoming packets. */
	uint16_t sges_n; /**< Number of segments per packet (log2 value). */
	uint16_t elts_n; /**< Mbuf queue size (log2 value). */
	struct rte_mbuf *(*elts)[]; /**< Rx elements. */
	volatile struct mlx4_wqe_data_seg (*wqes)[]; /**< HW queue entries. */
	volatile uint32_t *rq_db; /**< RQ doorbell record. */
	uint32_t csum:1; /**< Enable checksum offloading. */
	uint32_t csum_l2tun:1; /**< Same for L2 tunnels. */
	uint32_t l2tun_offload:1; /**< L2 tunnel offload is enabled. */
	struct mlx4_cq mcq;  /**< Info for directly manipulating the CQ. */
	struct mlx4_rxq_stats stats; /**< Rx queue counters. */
	unsigned int socket; /**< CPU socket ID for allocations. */
	uint32_t usecnt; /**< Number of users relying on queue resources. */
	uint8_t data[]; /**< Remaining queue resources. */
};

/** Shared flow target for Rx queues. */
struct mlx4_rss {
	LIST_ENTRY(mlx4_rss) next; /**< Next entry in list. */
	struct priv *priv; /**< Back pointer to private data. */
	uint32_t refcnt; /**< Reference count for this object. */
	uint32_t usecnt; /**< Number of users relying on @p qp and @p ind. */
	struct ibv_qp *qp; /**< Queue pair. */
	struct ibv_rwq_ind_table *ind; /**< Indirection table. */
	uint64_t fields; /**< Fields for RSS processing (Verbs format). */
	uint8_t key[MLX4_RSS_HASH_KEY_SIZE]; /**< Hash key to use. */
	uint16_t queues; /**< Number of target queues. */
	uint16_t queue_id[]; /**< Target queues. */
};

/** Tx element. */
struct txq_elt {
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
	struct mlx4_sq msq; /**< Info for directly manipulating the SQ. */
	struct mlx4_cq mcq; /**< Info for directly manipulating the CQ. */
	unsigned int elts_head; /**< Current index in (*elts)[]. */
	unsigned int elts_tail; /**< First element awaiting completion. */
	unsigned int elts_comp; /**< Number of packets awaiting completion. */
	int elts_comp_cd; /**< Countdown for next completion. */
	unsigned int elts_comp_cd_init; /**< Initial value for countdown. */
	unsigned int elts_n; /**< (*elts)[] length. */
	struct txq_elt (*elts)[]; /**< Tx elements. */
	struct mlx4_txq_stats stats; /**< Tx queue counters. */
	uint32_t max_inline; /**< Max inline send size. */
	uint32_t csum:1; /**< Enable checksum offloading. */
	uint32_t csum_l2tun:1; /**< Same for L2 tunnels. */
	uint32_t lb:1; /**< Whether packets should be looped back by eSwitch. */
	uint8_t *bounce_buf;
	/**< Memory used for storing the first DWORD of data TXBBs. */
	struct {
		const struct rte_mempool *mp; /**< Cached memory pool. */
		struct mlx4_mr *mr; /**< Memory region (for mp). */
		uint32_t lkey; /**< mr->lkey copy. */
	} mp2mr[MLX4_PMD_TX_MP_CACHE]; /**< MP to MR translation table. */
	struct priv *priv; /**< Back pointer to private data. */
	unsigned int socket; /**< CPU socket ID for allocations. */
	struct ibv_cq *cq; /**< Completion queue. */
	struct ibv_qp *qp; /**< Queue pair. */
	uint8_t data[]; /**< Remaining queue resources. */
};

/* mlx4_rxq.c */

uint8_t mlx4_rss_hash_key_default[MLX4_RSS_HASH_KEY_SIZE];
int mlx4_rss_init(struct priv *priv);
void mlx4_rss_deinit(struct priv *priv);
struct mlx4_rss *mlx4_rss_get(struct priv *priv, uint64_t fields,
			      uint8_t key[MLX4_RSS_HASH_KEY_SIZE],
			      uint16_t queues, const uint16_t queue_id[]);
void mlx4_rss_put(struct mlx4_rss *rss);
int mlx4_rss_attach(struct mlx4_rss *rss);
void mlx4_rss_detach(struct mlx4_rss *rss);
int mlx4_rxq_attach(struct rxq *rxq);
void mlx4_rxq_detach(struct rxq *rxq);
int mlx4_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			uint16_t desc, unsigned int socket,
			const struct rte_eth_rxconf *conf,
			struct rte_mempool *mp);
void mlx4_rx_queue_release(void *dpdk_rxq);

/* mlx4_rxtx.c */

uint16_t mlx4_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts,
		       uint16_t pkts_n);
uint16_t mlx4_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts,
		       uint16_t pkts_n);
uint16_t mlx4_tx_burst_removed(void *dpdk_txq, struct rte_mbuf **pkts,
			       uint16_t pkts_n);
uint16_t mlx4_rx_burst_removed(void *dpdk_rxq, struct rte_mbuf **pkts,
			       uint16_t pkts_n);

/* mlx4_txq.c */

int mlx4_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			uint16_t desc, unsigned int socket,
			const struct rte_eth_txconf *conf);
void mlx4_tx_queue_release(void *dpdk_txq);

/**
 * Get memory region (MR) <-> memory pool (MP) association from txq->mp2mr[].
 * Call mlx4_txq_add_mr() if MP is not registered yet.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param[in] mp
 *   Memory pool for which a memory region lkey must be returned.
 *
 * @return
 *   mr->lkey on success, (uint32_t)-1 on failure.
 */
static inline uint32_t
mlx4_txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
{
	unsigned int i;

	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (unlikely(txq->mp2mr[i].mp == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq->mp2mr[i].mp == mp) {
			/* MP found MP. */
			return txq->mp2mr[i].lkey;
		}
	}
	return mlx4_txq_add_mr(txq, mp, i);
}

#endif /* MLX4_RXTX_H_ */
