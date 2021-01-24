/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
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

#include <rte_ethdev_driver.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "mlx4.h"
#include "mlx4_prm.h"
#include "mlx4_mr.h"

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
	struct mlx4_priv *priv; /**< Back pointer to private data. */
	struct rte_mempool *mp; /**< Memory pool for allocations. */
	struct ibv_cq *cq; /**< Completion queue. */
	struct ibv_wq *wq; /**< Work queue. */
	struct ibv_comp_channel *channel; /**< Rx completion channel. */
	uint16_t rq_ci; /**< Saved RQ consumer index. */
	uint16_t port_id; /**< Port ID for incoming packets. */
	uint16_t sges_n; /**< Number of segments per packet (log2 value). */
	uint16_t elts_n; /**< Mbuf queue size (log2 value). */
	struct mlx4_mr_ctrl mr_ctrl; /* MR control descriptor. */
	struct rte_mbuf *(*elts)[]; /**< Rx elements. */
	volatile struct mlx4_wqe_data_seg (*wqes)[]; /**< HW queue entries. */
	volatile uint32_t *rq_db; /**< RQ doorbell record. */
	uint32_t csum:1; /**< Enable checksum offloading. */
	uint32_t csum_l2tun:1; /**< Same for L2 tunnels. */
	uint32_t crc_present:1; /**< CRC must be subtracted. */
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
	struct mlx4_priv *priv; /**< Back pointer to private data. */
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
	union {
		volatile struct mlx4_wqe_ctrl_seg *wqe; /**< SQ WQE. */
		volatile uint32_t *eocb; /**< End of completion burst. */
	};
};

/** Tx queue counters. */
struct mlx4_txq_stats {
	unsigned int idx; /**< Mapping index. */
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes; /**< Total of successfully sent bytes. */
	uint64_t odropped; /**< Total number of packets failed to transmit. */
};

/** Tx queue descriptor. */
struct txq {
	struct mlx4_sq msq; /**< Info for directly manipulating the SQ. */
	struct mlx4_cq mcq; /**< Info for directly manipulating the CQ. */
	uint16_t port_id; /**< Port ID of device. */
	unsigned int elts_head; /**< Current index in (*elts)[]. */
	unsigned int elts_tail; /**< First element awaiting completion. */
	int elts_comp_cd; /**< Countdown for next completion. */
	unsigned int elts_comp_cd_init; /**< Initial value for countdown. */
	unsigned int elts_n; /**< (*elts)[] length. */
	struct mlx4_mr_ctrl mr_ctrl; /* MR control descriptor. */
	struct txq_elt (*elts)[]; /**< Tx elements. */
	struct mlx4_txq_stats stats; /**< Tx queue counters. */
	uint32_t max_inline; /**< Max inline send size. */
	uint32_t csum:1; /**< Enable checksum offloading. */
	uint32_t csum_l2tun:1; /**< Same for L2 tunnels. */
	uint32_t lb:1; /**< Whether packets should be looped back by eSwitch. */
	uint8_t *bounce_buf;
	/**< Memory used for storing the first DWORD of data TXBBs. */
	struct mlx4_priv *priv; /**< Back pointer to private data. */
	unsigned int socket; /**< CPU socket ID for allocations. */
	struct ibv_cq *cq; /**< Completion queue. */
	struct ibv_qp *qp; /**< Queue pair. */
	uint8_t data[]; /**< Remaining queue resources. */
};

#define MLX4_TX_BFREG(txq) \
		(MLX4_PROC_PRIV((txq)->port_id)->uar_table[(txq)->stats.idx])

/* mlx4_rxq.c */

extern uint8_t mlx4_rss_hash_key_default[MLX4_RSS_HASH_KEY_SIZE];
int mlx4_rss_init(struct mlx4_priv *priv);
void mlx4_rss_deinit(struct mlx4_priv *priv);
struct mlx4_rss *mlx4_rss_get(struct mlx4_priv *priv, uint64_t fields,
			      const uint8_t key[MLX4_RSS_HASH_KEY_SIZE],
			      uint16_t queues, const uint16_t queue_id[]);
void mlx4_rss_put(struct mlx4_rss *rss);
int mlx4_rss_attach(struct mlx4_rss *rss);
void mlx4_rss_detach(struct mlx4_rss *rss);
int mlx4_rxq_attach(struct rxq *rxq);
void mlx4_rxq_detach(struct rxq *rxq);
uint64_t mlx4_get_rx_port_offloads(struct mlx4_priv *priv);
uint64_t mlx4_get_rx_queue_offloads(struct mlx4_priv *priv);
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

int mlx4_tx_uar_init_secondary(struct rte_eth_dev *dev, int fd);
void mlx4_tx_uar_uninit_secondary(struct rte_eth_dev *dev);
uint64_t mlx4_get_tx_port_offloads(struct mlx4_priv *priv);
int mlx4_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			uint16_t desc, unsigned int socket,
			const struct rte_eth_txconf *conf);
void mlx4_tx_queue_release(void *dpdk_txq);

/* mlx4_mr.c */

void mlx4_mr_flush_local_cache(struct mlx4_mr_ctrl *mr_ctrl);
uint32_t mlx4_rx_addr2mr_bh(struct rxq *rxq, uintptr_t addr);
uint32_t mlx4_tx_mb2mr_bh(struct txq *txq, struct rte_mbuf *mb);
uint32_t mlx4_tx_update_ext_mp(struct txq *txq, uintptr_t addr,
			       struct rte_mempool *mp);

/**
 * Get Memory Pool (MP) from mbuf. If mbuf is indirect, the pool from which the
 * cloned mbuf is allocated is returned instead.
 *
 * @param buf
 *   Pointer to mbuf.
 *
 * @return
 *   Memory pool where data is located for given mbuf.
 */
static inline struct rte_mempool *
mlx4_mb2mp(struct rte_mbuf *buf)
{
	if (unlikely(RTE_MBUF_CLONED(buf)))
		return rte_mbuf_from_indirect(buf)->pool;
	return buf->pool;
}

/**
 * Query LKey from a packet buffer for Rx. No need to flush local caches for Rx
 * as mempool is pre-configured and static.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 * @param addr
 *   Address to search.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static __rte_always_inline uint32_t
mlx4_rx_addr2mr(struct rxq *rxq, uintptr_t addr)
{
	struct mlx4_mr_ctrl *mr_ctrl = &rxq->mr_ctrl;
	uint32_t lkey;

	/* Linear search on MR cache array. */
	lkey = mlx4_mr_lookup_cache(mr_ctrl->cache, &mr_ctrl->mru,
				    MLX4_MR_CACHE_N, addr);
	if (likely(lkey != UINT32_MAX))
		return lkey;
	/* Take slower bottom-half (Binary Search) on miss. */
	return mlx4_rx_addr2mr_bh(rxq, addr);
}

#define mlx4_rx_mb2mr(rxq, mb) mlx4_rx_addr2mr(rxq, (uintptr_t)((mb)->buf_addr))

/**
 * Query LKey from a packet buffer for Tx. If not found, add the mempool.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param addr
 *   Address to search.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static __rte_always_inline uint32_t
mlx4_tx_mb2mr(struct txq *txq, struct rte_mbuf *mb)
{
	struct mlx4_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	uintptr_t addr = (uintptr_t)mb->buf_addr;
	uint32_t lkey;

	/* Check generation bit to see if there's any change on existing MRs. */
	if (unlikely(*mr_ctrl->dev_gen_ptr != mr_ctrl->cur_gen))
		mlx4_mr_flush_local_cache(mr_ctrl);
	/* Linear search on MR cache array. */
	lkey = mlx4_mr_lookup_cache(mr_ctrl->cache, &mr_ctrl->mru,
				    MLX4_MR_CACHE_N, addr);
	if (likely(lkey != UINT32_MAX))
		return lkey;
	/* Take slower bottom-half on miss. */
	return mlx4_tx_mb2mr_bh(txq, mb);
}

#endif /* MLX4_RXTX_H_ */
