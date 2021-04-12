/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_TX_H_
#define RTE_PMD_MLX5_TX_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_common.h>
#include <rte_spinlock.h>

#include <mlx5_common_mr.h>

#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_mr.h"

/* Mbuf dynamic flag offset for inline. */
extern uint64_t rte_net_mlx5_dynf_inline_mask;

struct mlx5_txq_stats {
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes; /**< Total of successfully sent bytes. */
#endif
	uint64_t oerrors; /**< Total number of failed transmitted packets. */
};

/* TX queue send local data. */
__extension__
struct mlx5_txq_local {
	struct mlx5_wqe *wqe_last; /* last sent WQE pointer. */
	struct rte_mbuf *mbuf; /* first mbuf to process. */
	uint16_t pkts_copy; /* packets copied to elts. */
	uint16_t pkts_sent; /* packets sent. */
	uint16_t pkts_loop; /* packets sent on loop entry. */
	uint16_t elts_free; /* available elts remain. */
	uint16_t wqe_free; /* available wqe remain. */
	uint16_t mbuf_off; /* data offset in current mbuf. */
	uint16_t mbuf_nseg; /* number of remaining mbuf. */
	uint16_t mbuf_free; /* number of inline mbufs to free. */
};

/* TX queue descriptor. */
__extension__
struct mlx5_txq_data {
	uint16_t elts_head; /* Current counter in (*elts)[]. */
	uint16_t elts_tail; /* Counter of first element awaiting completion. */
	uint16_t elts_comp; /* elts index since last completion request. */
	uint16_t elts_s; /* Number of mbuf elements. */
	uint16_t elts_m; /* Mask for mbuf elements indices. */
	/* Fields related to elts mbuf storage. */
	uint16_t wqe_ci; /* Consumer index for work queue. */
	uint16_t wqe_pi; /* Producer index for work queue. */
	uint16_t wqe_s; /* Number of WQ elements. */
	uint16_t wqe_m; /* Mask Number for WQ elements. */
	uint16_t wqe_comp; /* WQE index since last completion request. */
	uint16_t wqe_thres; /* WQE threshold to request completion in CQ. */
	/* WQ related fields. */
	uint16_t cq_ci; /* Consumer index for completion queue. */
	uint16_t cq_pi; /* Production index for completion queue. */
	uint16_t cqe_s; /* Number of CQ elements. */
	uint16_t cqe_m; /* Mask for CQ indices. */
	/* CQ related fields. */
	uint16_t elts_n:4; /* elts[] length (in log2). */
	uint16_t cqe_n:4; /* Number of CQ elements (in log2). */
	uint16_t wqe_n:4; /* Number of WQ elements (in log2). */
	uint16_t tso_en:1; /* When set hardware TSO is enabled. */
	uint16_t tunnel_en:1;
	/* When set TX offload for tunneled packets are supported. */
	uint16_t swp_en:1; /* Whether SW parser is enabled. */
	uint16_t vlan_en:1; /* VLAN insertion in WQE is supported. */
	uint16_t db_nc:1; /* Doorbell mapped to non-cached region. */
	uint16_t db_heu:1; /* Doorbell heuristic write barrier. */
	uint16_t fast_free:1; /* mbuf fast free on Tx is enabled. */
	uint16_t inlen_send; /* Ordinary send data inline size. */
	uint16_t inlen_empw; /* eMPW max packet size to inline. */
	uint16_t inlen_mode; /* Minimal data length to inline. */
	uint32_t qp_num_8s; /* QP number shifted by 8. */
	uint64_t offloads; /* Offloads for Tx Queue. */
	struct mlx5_mr_ctrl mr_ctrl; /* MR control descriptor. */
	struct mlx5_wqe *wqes; /* Work queue. */
	struct mlx5_wqe *wqes_end; /* Work queue array limit. */
#ifdef RTE_LIBRTE_MLX5_DEBUG
	uint32_t *fcqs; /* Free completion queue (debug extended). */
#else
	uint16_t *fcqs; /* Free completion queue. */
#endif
	volatile struct mlx5_cqe *cqes; /* Completion queue. */
	volatile uint32_t *qp_db; /* Work queue doorbell. */
	volatile uint32_t *cq_db; /* Completion queue doorbell. */
	uint16_t port_id; /* Port ID of device. */
	uint16_t idx; /* Queue index. */
	uint64_t ts_mask; /* Timestamp flag dynamic mask. */
	int32_t ts_offset; /* Timestamp field dynamic offset. */
	struct mlx5_dev_ctx_shared *sh; /* Shared context. */
	struct mlx5_txq_stats stats; /* TX queue counters. */
#ifndef RTE_ARCH_64
	rte_spinlock_t *uar_lock;
	/* UAR access lock required for 32bit implementations */
#endif
	struct rte_mbuf *elts[0];
	/* Storage for queued packets, must be the last field. */
} __rte_cache_aligned;

enum mlx5_txq_type {
	MLX5_TXQ_TYPE_STANDARD, /* Standard Tx queue. */
	MLX5_TXQ_TYPE_HAIRPIN, /* Hairpin Tx queue. */
};

/* TX queue control descriptor. */
struct mlx5_txq_ctrl {
	LIST_ENTRY(mlx5_txq_ctrl) next; /* Pointer to the next element. */
	uint32_t refcnt; /* Reference counter. */
	unsigned int socket; /* CPU socket ID for allocations. */
	enum mlx5_txq_type type; /* The txq ctrl type. */
	unsigned int max_inline_data; /* Max inline data. */
	unsigned int max_tso_header; /* Max TSO header size. */
	struct mlx5_txq_obj *obj; /* Verbs/DevX queue object. */
	struct mlx5_priv *priv; /* Back pointer to private data. */
	off_t uar_mmap_offset; /* UAR mmap offset for non-primary process. */
	void *bf_reg; /* BlueFlame register from Verbs. */
	uint16_t dump_file_n; /* Number of dump files. */
	struct rte_eth_hairpin_conf hairpin_conf; /* Hairpin configuration. */
	uint32_t hairpin_status; /* Hairpin binding status. */
	struct mlx5_txq_data txq; /* Data path structure. */
	/* Must be the last field in the structure, contains elts[]. */
};

/* mlx5_txq.c */

int mlx5_tx_queue_start(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_tx_queue_stop(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_tx_queue_start_primary(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_tx_queue_stop_primary(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			unsigned int socket, const struct rte_eth_txconf *conf);
int mlx5_tx_hairpin_queue_setup
	(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
	 const struct rte_eth_hairpin_conf *hairpin_conf);
void mlx5_tx_queue_release(void *dpdk_txq);
void txq_uar_init(struct mlx5_txq_ctrl *txq_ctrl);
int mlx5_tx_uar_init_secondary(struct rte_eth_dev *dev, int fd);
void mlx5_tx_uar_uninit_secondary(struct rte_eth_dev *dev);
int mlx5_txq_obj_verify(struct rte_eth_dev *dev);
struct mlx5_txq_ctrl *mlx5_txq_new(struct rte_eth_dev *dev, uint16_t idx,
				   uint16_t desc, unsigned int socket,
				   const struct rte_eth_txconf *conf);
struct mlx5_txq_ctrl *mlx5_txq_hairpin_new
	(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
	 const struct rte_eth_hairpin_conf *hairpin_conf);
struct mlx5_txq_ctrl *mlx5_txq_get(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_txq_release(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_txq_releasable(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_txq_verify(struct rte_eth_dev *dev);
void txq_alloc_elts(struct mlx5_txq_ctrl *txq_ctrl);
void txq_free_elts(struct mlx5_txq_ctrl *txq_ctrl);
uint64_t mlx5_get_tx_port_offloads(struct rte_eth_dev *dev);
void mlx5_txq_dynf_timestamp_set(struct rte_eth_dev *dev);

/* mlx5_tx.c */

uint16_t removed_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts,
			  uint16_t pkts_n);
int mlx5_tx_descriptor_status(void *tx_queue, uint16_t offset);
void mlx5_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		       struct rte_eth_txq_info *qinfo);
int mlx5_tx_burst_mode_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			   struct rte_eth_burst_mode *mode);

/* mlx5_mr.c */

uint32_t mlx5_tx_mb2mr_bh(struct mlx5_txq_data *txq, struct rte_mbuf *mb);
uint32_t mlx5_tx_update_ext_mp(struct mlx5_txq_data *txq, uintptr_t addr,
			       struct rte_mempool *mp);

static __rte_always_inline uint64_t *
mlx5_tx_bfreg(struct mlx5_txq_data *txq)
{
	return MLX5_PROC_PRIV(txq->port_id)->uar_table[txq->idx];
}

/**
 * Provide safe 64bit store operation to mlx5 UAR region for both 32bit and
 * 64bit architectures.
 *
 * @param val
 *   value to write in CPU endian format.
 * @param addr
 *   Address to write to.
 * @param lock
 *   Address of the lock to use for that UAR access.
 */
static __rte_always_inline void
__mlx5_uar_write64_relaxed(uint64_t val, void *addr,
			   rte_spinlock_t *lock __rte_unused)
{
#ifdef RTE_ARCH_64
	*(uint64_t *)addr = val;
#else /* !RTE_ARCH_64 */
	rte_spinlock_lock(lock);
	*(uint32_t *)addr = val;
	rte_io_wmb();
	*((uint32_t *)addr + 1) = val >> 32;
	rte_spinlock_unlock(lock);
#endif
}

/**
 * Provide safe 64bit store operation to mlx5 UAR region for both 32bit and
 * 64bit architectures while guaranteeing the order of execution with the
 * code being executed.
 *
 * @param val
 *   value to write in CPU endian format.
 * @param addr
 *   Address to write to.
 * @param lock
 *   Address of the lock to use for that UAR access.
 */
static __rte_always_inline void
__mlx5_uar_write64(uint64_t val, void *addr, rte_spinlock_t *lock)
{
	rte_io_wmb();
	__mlx5_uar_write64_relaxed(val, addr, lock);
}

/* Assist macros, used instead of directly calling the functions they wrap. */
#ifdef RTE_ARCH_64
#define mlx5_uar_write64_relaxed(val, dst, lock) \
		__mlx5_uar_write64_relaxed(val, dst, NULL)
#define mlx5_uar_write64(val, dst, lock) __mlx5_uar_write64(val, dst, NULL)
#else
#define mlx5_uar_write64_relaxed(val, dst, lock) \
		__mlx5_uar_write64_relaxed(val, dst, lock)
#define mlx5_uar_write64(val, dst, lock) __mlx5_uar_write64(val, dst, lock)
#endif

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
mlx5_tx_mb2mr(struct mlx5_txq_data *txq, struct rte_mbuf *mb)
{
	struct mlx5_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	uintptr_t addr = (uintptr_t)mb->buf_addr;
	uint32_t lkey;

	/* Check generation bit to see if there's any change on existing MRs. */
	if (unlikely(*mr_ctrl->dev_gen_ptr != mr_ctrl->cur_gen))
		mlx5_mr_flush_local_cache(mr_ctrl);
	/* Linear search on MR cache array. */
	lkey = mlx5_mr_lookup_lkey(mr_ctrl->cache, &mr_ctrl->mru,
				   MLX5_MR_CACHE_N, addr);
	if (likely(lkey != UINT32_MAX))
		return lkey;
	/* Take slower bottom-half on miss. */
	return mlx5_tx_mb2mr_bh(txq, mb);
}

/**
 * Ring TX queue doorbell and flush the update if requested.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param wqe
 *   Pointer to the last WQE posted in the NIC.
 * @param cond
 *   Request for write memory barrier after BlueFlame update.
 */
static __rte_always_inline void
mlx5_tx_dbrec_cond_wmb(struct mlx5_txq_data *txq, volatile struct mlx5_wqe *wqe,
		       int cond)
{
	uint64_t *dst = mlx5_tx_bfreg(txq);
	volatile uint64_t *src = ((volatile uint64_t *)wqe);

	rte_io_wmb();
	*txq->qp_db = rte_cpu_to_be_32(txq->wqe_ci);
	/* Ensure ordering between DB record and BF copy. */
	rte_wmb();
	mlx5_uar_write64_relaxed(*src, dst, txq->uar_lock);
	if (cond)
		rte_wmb();
}

/**
 * Ring TX queue doorbell and flush the update by write memory barrier.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param wqe
 *   Pointer to the last WQE posted in the NIC.
 */
static __rte_always_inline void
mlx5_tx_dbrec(struct mlx5_txq_data *txq, volatile struct mlx5_wqe *wqe)
{
	mlx5_tx_dbrec_cond_wmb(txq, wqe, 1);
}

/**
 * Convert timestamp from mbuf format to linear counter
 * of Clock Queue completions (24 bits).
 *
 * @param sh
 *   Pointer to the device shared context to fetch Tx
 *   packet pacing timestamp and parameters.
 * @param ts
 *   Timestamp from mbuf to convert.
 * @return
 *   positive or zero value - completion ID to wait.
 *   negative value - conversion error.
 */
static __rte_always_inline int32_t
mlx5_txpp_convert_tx_ts(struct mlx5_dev_ctx_shared *sh, uint64_t mts)
{
	uint64_t ts, ci;
	uint32_t tick;

	do {
		/*
		 * Read atomically two uint64_t fields and compare lsb bits.
		 * It there is no match - the timestamp was updated in
		 * the service thread, data should be re-read.
		 */
		rte_compiler_barrier();
		ci = __atomic_load_n(&sh->txpp.ts.ci_ts, __ATOMIC_RELAXED);
		ts = __atomic_load_n(&sh->txpp.ts.ts, __ATOMIC_RELAXED);
		rte_compiler_barrier();
		if (!((ts ^ ci) << (64 - MLX5_CQ_INDEX_WIDTH)))
			break;
	} while (true);
	/* Perform the skew correction, positive value to send earlier. */
	mts -= sh->txpp.skew;
	mts -= ts;
	if (unlikely(mts >= UINT64_MAX / 2)) {
		/* We have negative integer, mts is in the past. */
		__atomic_fetch_add(&sh->txpp.err_ts_past,
				   1, __ATOMIC_RELAXED);
		return -1;
	}
	tick = sh->txpp.tick;
	MLX5_ASSERT(tick);
	/* Convert delta to completions, round up. */
	mts = (mts + tick - 1) / tick;
	if (unlikely(mts >= (1 << MLX5_CQ_INDEX_WIDTH) / 2 - 1)) {
		/* We have mts is too distant future. */
		__atomic_fetch_add(&sh->txpp.err_ts_future,
				   1, __ATOMIC_RELAXED);
		return -1;
	}
	mts <<= 64 - MLX5_CQ_INDEX_WIDTH;
	ci += mts;
	ci >>= 64 - MLX5_CQ_INDEX_WIDTH;
	return ci;
}

#endif /* RTE_PMD_MLX5_TX_H_ */
