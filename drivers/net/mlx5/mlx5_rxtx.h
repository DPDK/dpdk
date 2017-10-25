/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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

#ifndef RTE_PMD_MLX5_RXTX_H_
#define RTE_PMD_MLX5_RXTX_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_atomic.h>

#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

struct mlx5_rxq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
#endif
	uint64_t idropped; /**< Total of packets dropped when RX ring full. */
	uint64_t rx_nombuf; /**< Total of RX mbuf allocation failures. */
};

struct mlx5_txq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes; /**< Total of successfully sent bytes. */
#endif
	uint64_t oerrors; /**< Total number of failed transmitted packets. */
};

struct priv;

/* Memory region queue object. */
struct mlx5_mr {
	LIST_ENTRY(mlx5_mr) next; /**< Pointer to the next element. */
	rte_atomic32_t refcnt; /*<< Reference counter. */
	uint32_t lkey; /*<< rte_cpu_to_be_32(mr->lkey) */
	uintptr_t start; /* Start address of MR */
	uintptr_t end; /* End address of MR */
	struct ibv_mr *mr; /*<< Memory Region. */
	struct rte_mempool *mp; /*<< Memory Pool. */
};

/* Compressed CQE context. */
struct rxq_zip {
	uint16_t ai; /* Array index. */
	uint16_t ca; /* Current array index. */
	uint16_t na; /* Next array index. */
	uint16_t cq_ci; /* The next CQE. */
	uint32_t cqe_cnt; /* Number of CQEs. */
};

/* RX queue descriptor. */
struct mlx5_rxq_data {
	unsigned int csum:1; /* Enable checksum offloading. */
	unsigned int csum_l2tun:1; /* Same for L2 tunnels. */
	unsigned int hw_timestamp:1; /* Enable HW timestamp. */
	unsigned int vlan_strip:1; /* Enable VLAN stripping. */
	unsigned int crc_present:1; /* CRC must be subtracted. */
	unsigned int sges_n:2; /* Log 2 of SGEs (max buffers per packet). */
	unsigned int cqe_n:4; /* Log 2 of CQ elements. */
	unsigned int elts_n:4; /* Log 2 of Mbufs. */
	unsigned int rss_hash:1; /* RSS hash result is enabled. */
	unsigned int mark:1; /* Marked flow available on the queue. */
	unsigned int pending_err:1; /* CQE error needs to be handled. */
	unsigned int :14; /* Remaining bits. */
	volatile uint32_t *rq_db;
	volatile uint32_t *cq_db;
	uint16_t port_id;
	uint16_t rq_ci;
	uint16_t rq_pi;
	uint16_t cq_ci;
	volatile struct mlx5_wqe_data_seg(*wqes)[];
	volatile struct mlx5_cqe(*cqes)[];
	struct rxq_zip zip; /* Compressed context. */
	struct rte_mbuf *(*elts)[];
	struct rte_mempool *mp;
	struct mlx5_rxq_stats stats;
	uint64_t mbuf_initializer; /* Default rearm_data for vectorized Rx. */
	struct rte_mbuf fake_mbuf; /* elts padding for vectorized Rx. */
	void *cq_uar; /* CQ user access region. */
	uint32_t cqn; /* CQ number. */
	uint8_t cq_arm_sn; /* CQ arm seq number. */
} __rte_cache_aligned;

/* Verbs Rx queue elements. */
struct mlx5_rxq_ibv {
	LIST_ENTRY(mlx5_rxq_ibv) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct mlx5_rxq_ctrl *rxq_ctrl; /* Back pointer to parent. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_wq *wq; /* Work Queue. */
	struct ibv_comp_channel *channel;
	struct mlx5_mr *mr; /* Memory Region (for mp). */
};

/* RX queue control descriptor. */
struct mlx5_rxq_ctrl {
	LIST_ENTRY(mlx5_rxq_ctrl) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct priv *priv; /* Back pointer to private data. */
	struct mlx5_rxq_ibv *ibv; /* Verbs elements. */
	struct mlx5_rxq_data rxq; /* Data path structure. */
	unsigned int socket; /* CPU socket ID for allocations. */
	unsigned int irq:1; /* Whether IRQ is enabled. */
};

/* Indirection table. */
struct mlx5_ind_table_ibv {
	LIST_ENTRY(mlx5_ind_table_ibv) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct ibv_rwq_ind_table *ind_table; /**< Indirection table. */
	uint16_t queues_n; /**< Number of queues in the list. */
	uint16_t queues[]; /**< Queue list. */
};

/* Hash Rx queue. */
struct mlx5_hrxq {
	LIST_ENTRY(mlx5_hrxq) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct mlx5_ind_table_ibv *ind_table; /* Indirection table. */
	struct ibv_qp *qp; /* Verbs queue pair. */
	uint64_t hash_fields; /* Verbs Hash fields. */
	uint8_t rss_key_len; /* Hash key length in bytes. */
	uint8_t rss_key[]; /* Hash key. */
};

/* TX queue descriptor. */
__extension__
struct mlx5_txq_data {
	uint16_t elts_head; /* Current counter in (*elts)[]. */
	uint16_t elts_tail; /* Counter of first element awaiting completion. */
	uint16_t elts_comp; /* Counter since last completion request. */
	uint16_t mpw_comp; /* WQ index since last completion request. */
	uint16_t cq_ci; /* Consumer index for completion queue. */
	uint16_t cq_pi; /* Producer index for completion queue. */
	uint16_t wqe_ci; /* Consumer index for work queue. */
	uint16_t wqe_pi; /* Producer index for work queue. */
	uint16_t elts_n:4; /* (*elts)[] length (in log2). */
	uint16_t cqe_n:4; /* Number of CQ elements (in log2). */
	uint16_t wqe_n:4; /* Number of of WQ elements (in log2). */
	uint16_t inline_en:1; /* When set inline is enabled. */
	uint16_t tso_en:1; /* When set hardware TSO is enabled. */
	uint16_t tunnel_en:1;
	/* When set TX offload for tunneled packets are supported. */
	uint16_t mpw_hdr_dseg:1; /* Enable DSEGs in the title WQEBB. */
	uint16_t max_inline; /* Multiple of RTE_CACHE_LINE_SIZE to inline. */
	uint16_t inline_max_packet_sz; /* Max packet size for inlining. */
	uint16_t mr_cache_idx; /* Index of last hit entry. */
	uint32_t qp_num_8s; /* QP number shifted by 8. */
	uint32_t flags; /* Flags for Tx Queue. */
	volatile struct mlx5_cqe (*cqes)[]; /* Completion queue. */
	volatile void *wqes; /* Work queue (use volatile to write into). */
	volatile uint32_t *qp_db; /* Work queue doorbell. */
	volatile uint32_t *cq_db; /* Completion queue doorbell. */
	volatile void *bf_reg; /* Blueflame register. */
	struct mlx5_mr *mp2mr[MLX5_PMD_TX_MP_CACHE]; /* MR translation table. */
	struct rte_mbuf *(*elts)[]; /* TX elements. */
	struct mlx5_txq_stats stats; /* TX queue counters. */
} __rte_cache_aligned;

/* Verbs Rx queue elements. */
struct mlx5_txq_ibv {
	LIST_ENTRY(mlx5_txq_ibv) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_qp *qp; /* Queue Pair. */
};

/* TX queue control descriptor. */
struct mlx5_txq_ctrl {
	LIST_ENTRY(mlx5_txq_ctrl) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct priv *priv; /* Back pointer to private data. */
	unsigned int socket; /* CPU socket ID for allocations. */
	unsigned int max_inline_data; /* Max inline data. */
	unsigned int max_tso_header; /* Max TSO header size. */
	struct mlx5_txq_ibv *ibv; /* Verbs queue object. */
	struct mlx5_txq_data txq; /* Data path structure. */
	off_t uar_mmap_offset; /* UAR mmap offset for non-primary process. */
};

/* mlx5_rxq.c */

extern uint8_t rss_hash_default_key[];
extern const size_t rss_hash_default_key_len;

void mlx5_rxq_cleanup(struct mlx5_rxq_ctrl *);
int mlx5_rx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_rxconf *, struct rte_mempool *);
void mlx5_rx_queue_release(void *);
int priv_rx_intr_vec_enable(struct priv *priv);
void priv_rx_intr_vec_disable(struct priv *priv);
int mlx5_rx_intr_enable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int mlx5_rx_intr_disable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
struct mlx5_rxq_ibv *mlx5_priv_rxq_ibv_new(struct priv *, uint16_t);
struct mlx5_rxq_ibv *mlx5_priv_rxq_ibv_get(struct priv *, uint16_t);
int mlx5_priv_rxq_ibv_release(struct priv *, struct mlx5_rxq_ibv *);
int mlx5_priv_rxq_ibv_releasable(struct priv *, struct mlx5_rxq_ibv *);
int mlx5_priv_rxq_ibv_verify(struct priv *);
struct mlx5_rxq_ctrl *mlx5_priv_rxq_new(struct priv *, uint16_t,
					uint16_t, unsigned int,
					struct rte_mempool *);
struct mlx5_rxq_ctrl *mlx5_priv_rxq_get(struct priv *, uint16_t);
int mlx5_priv_rxq_release(struct priv *, uint16_t);
int mlx5_priv_rxq_releasable(struct priv *, uint16_t);
int mlx5_priv_rxq_verify(struct priv *);
int rxq_alloc_elts(struct mlx5_rxq_ctrl *);
struct mlx5_ind_table_ibv *mlx5_priv_ind_table_ibv_new(struct priv *,
						       uint16_t [],
						       uint16_t);
struct mlx5_ind_table_ibv *mlx5_priv_ind_table_ibv_get(struct priv *,
						       uint16_t [],
						       uint16_t);
int mlx5_priv_ind_table_ibv_release(struct priv *, struct mlx5_ind_table_ibv *);
int mlx5_priv_ind_table_ibv_verify(struct priv *);
struct mlx5_hrxq *mlx5_priv_hrxq_new(struct priv *, uint8_t *, uint8_t,
				     uint64_t, uint16_t [], uint16_t);
struct mlx5_hrxq *mlx5_priv_hrxq_get(struct priv *, uint8_t *, uint8_t,
				     uint64_t, uint16_t [], uint16_t);
int mlx5_priv_hrxq_release(struct priv *, struct mlx5_hrxq *);
int mlx5_priv_hrxq_ibv_verify(struct priv *);

/* mlx5_txq.c */

int mlx5_tx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_txconf *);
void mlx5_tx_queue_release(void *);
int priv_tx_uar_remap(struct priv *priv, int fd);
struct mlx5_txq_ibv *mlx5_priv_txq_ibv_new(struct priv *, uint16_t);
struct mlx5_txq_ibv *mlx5_priv_txq_ibv_get(struct priv *, uint16_t);
int mlx5_priv_txq_ibv_release(struct priv *, struct mlx5_txq_ibv *);
int mlx5_priv_txq_ibv_releasable(struct priv *, struct mlx5_txq_ibv *);
int mlx5_priv_txq_ibv_verify(struct priv *);
struct mlx5_txq_ctrl *mlx5_priv_txq_new(struct priv *, uint16_t,
					uint16_t, unsigned int,
					const struct rte_eth_txconf *);
struct mlx5_txq_ctrl *mlx5_priv_txq_get(struct priv *, uint16_t);
int mlx5_priv_txq_release(struct priv *, uint16_t);
int mlx5_priv_txq_releasable(struct priv *, uint16_t);
int mlx5_priv_txq_verify(struct priv *);
void txq_alloc_elts(struct mlx5_txq_ctrl *);

/* mlx5_rxtx.c */

extern uint32_t mlx5_ptype_table[];

void mlx5_set_ptype_table(void);
uint16_t mlx5_tx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_mpw(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_mpw_inline(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_empw(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_rx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t removed_tx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t removed_rx_burst(void *, struct rte_mbuf **, uint16_t);
int mlx5_rx_descriptor_status(void *, uint16_t);
int mlx5_tx_descriptor_status(void *, uint16_t);

/* Vectorized version of mlx5_rxtx.c */
int priv_check_raw_vec_tx_support(struct priv *);
int priv_check_vec_tx_support(struct priv *);
int rxq_check_vec_support(struct mlx5_rxq_data *);
int priv_check_vec_rx_support(struct priv *);
uint16_t mlx5_tx_burst_raw_vec(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_vec(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_rx_burst_vec(void *, struct rte_mbuf **, uint16_t);

/* mlx5_mr.c */

void mlx5_mp2mr_iter(struct rte_mempool *, void *);
struct mlx5_mr *priv_txq_mp2mr_reg(struct priv *priv, struct mlx5_txq_data *,
				   struct rte_mempool *, unsigned int);
struct mlx5_mr *mlx5_txq_mp2mr_reg(struct mlx5_txq_data *, struct rte_mempool *,
				   unsigned int);

#ifndef NDEBUG
/**
 * Verify or set magic value in CQE.
 *
 * @param cqe
 *   Pointer to CQE.
 *
 * @return
 *   0 the first time.
 */
static inline int
check_cqe_seen(volatile struct mlx5_cqe *cqe)
{
	static const uint8_t magic[] = "seen";
	volatile uint8_t (*buf)[sizeof(cqe->rsvd0)] = &cqe->rsvd0;
	int ret = 1;
	unsigned int i;

	for (i = 0; i < sizeof(magic) && i < sizeof(*buf); ++i)
		if (!ret || (*buf)[i] != magic[i]) {
			ret = 0;
			(*buf)[i] = magic[i];
		}
	return ret;
}
#endif /* NDEBUG */

/**
 * Check whether CQE is valid.
 *
 * @param cqe
 *   Pointer to CQE.
 * @param cqes_n
 *   Size of completion queue.
 * @param ci
 *   Consumer index.
 *
 * @return
 *   0 on success, 1 on failure.
 */
static __rte_always_inline int
check_cqe(volatile struct mlx5_cqe *cqe,
	  unsigned int cqes_n, const uint16_t ci)
{
	uint16_t idx = ci & cqes_n;
	uint8_t op_own = cqe->op_own;
	uint8_t op_owner = MLX5_CQE_OWNER(op_own);
	uint8_t op_code = MLX5_CQE_OPCODE(op_own);

	if (unlikely((op_owner != (!!(idx))) || (op_code == MLX5_CQE_INVALID)))
		return 1; /* No CQE. */
#ifndef NDEBUG
	if ((op_code == MLX5_CQE_RESP_ERR) ||
	    (op_code == MLX5_CQE_REQ_ERR)) {
		volatile struct mlx5_err_cqe *err_cqe = (volatile void *)cqe;
		uint8_t syndrome = err_cqe->syndrome;

		if ((syndrome == MLX5_CQE_SYNDROME_LOCAL_LENGTH_ERR) ||
		    (syndrome == MLX5_CQE_SYNDROME_REMOTE_ABORTED_ERR))
			return 0;
		if (!check_cqe_seen(cqe)) {
			ERROR("unexpected CQE error %u (0x%02x)"
			      " syndrome 0x%02x",
			      op_code, op_code, syndrome);
			rte_hexdump(stderr, "MLX5 Error CQE:",
				    (const void *)((uintptr_t)err_cqe),
				    sizeof(*err_cqe));
		}
		return 1;
	} else if ((op_code != MLX5_CQE_RESP_SEND) &&
		   (op_code != MLX5_CQE_REQ)) {
		if (!check_cqe_seen(cqe)) {
			ERROR("unexpected CQE opcode %u (0x%02x)",
			      op_code, op_code);
			rte_hexdump(stderr, "MLX5 CQE:",
				    (const void *)((uintptr_t)cqe),
				    sizeof(*cqe));
		}
		return 1;
	}
#endif /* NDEBUG */
	return 0;
}

/**
 * Return the address of the WQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param  wqe_ci
 *   WQE consumer index.
 *
 * @return
 *   WQE address.
 */
static inline uintptr_t *
tx_mlx5_wqe(struct mlx5_txq_data *txq, uint16_t ci)
{
	ci &= ((1 << txq->wqe_n) - 1);
	return (uintptr_t *)((uintptr_t)txq->wqes + ci * MLX5_WQE_SIZE);
}

/**
 * Manage TX completions.
 *
 * When sending a burst, mlx5_tx_burst() posts several WRs.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
static __rte_always_inline void
mlx5_tx_complete(struct mlx5_txq_data *txq)
{
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	const unsigned int cqe_n = 1 << txq->cqe_n;
	const unsigned int cqe_cnt = cqe_n - 1;
	uint16_t elts_free = txq->elts_tail;
	uint16_t elts_tail;
	uint16_t cq_ci = txq->cq_ci;
	volatile struct mlx5_cqe *cqe = NULL;
	volatile struct mlx5_wqe_ctrl *ctrl;
	struct rte_mbuf *m, *free[elts_n];
	struct rte_mempool *pool = NULL;
	unsigned int blk_n = 0;

	cqe = &(*txq->cqes)[cq_ci & cqe_cnt];
	if (unlikely(check_cqe(cqe, cqe_n, cq_ci)))
		return;
#ifndef NDEBUG
	if ((MLX5_CQE_OPCODE(cqe->op_own) == MLX5_CQE_RESP_ERR) ||
	    (MLX5_CQE_OPCODE(cqe->op_own) == MLX5_CQE_REQ_ERR)) {
		if (!check_cqe_seen(cqe)) {
			ERROR("unexpected error CQE, TX stopped");
			rte_hexdump(stderr, "MLX5 TXQ:",
				    (const void *)((uintptr_t)txq->wqes),
				    ((1 << txq->wqe_n) *
				     MLX5_WQE_SIZE));
		}
		return;
	}
#endif /* NDEBUG */
	++cq_ci;
	txq->wqe_pi = rte_be_to_cpu_16(cqe->wqe_counter);
	ctrl = (volatile struct mlx5_wqe_ctrl *)
		tx_mlx5_wqe(txq, txq->wqe_pi);
	elts_tail = ctrl->ctrl3;
	assert((elts_tail & elts_m) < (1 << txq->wqe_n));
	/* Free buffers. */
	while (elts_free != elts_tail) {
		m = rte_pktmbuf_prefree_seg((*txq->elts)[elts_free++ & elts_m]);
		if (likely(m != NULL)) {
			if (likely(m->pool == pool)) {
				free[blk_n++] = m;
			} else {
				if (likely(pool != NULL))
					rte_mempool_put_bulk(pool,
							     (void *)free,
							     blk_n);
				free[0] = m;
				pool = m->pool;
				blk_n = 1;
			}
		}
	}
	if (blk_n)
		rte_mempool_put_bulk(pool, (void *)free, blk_n);
#ifndef NDEBUG
	elts_free = txq->elts_tail;
	/* Poisoning. */
	while (elts_free != elts_tail) {
		memset(&(*txq->elts)[elts_free & elts_m],
		       0x66,
		       sizeof((*txq->elts)[elts_free & elts_m]));
		++elts_free;
	}
#endif
	txq->cq_ci = cq_ci;
	txq->elts_tail = elts_tail;
	/* Update the consumer index. */
	rte_compiler_barrier();
	*txq->cq_db = rte_cpu_to_be_32(cq_ci);
}

/**
 * Get Memory Pool (MP) from mbuf. If mbuf is indirect, the pool from which
 * the cloned mbuf is allocated is returned instead.
 *
 * @param buf
 *   Pointer to mbuf.
 *
 * @return
 *   Memory pool where data is located for given mbuf.
 */
static struct rte_mempool *
mlx5_tx_mb2mp(struct rte_mbuf *buf)
{
	if (unlikely(RTE_MBUF_INDIRECT(buf)))
		return rte_mbuf_from_indirect(buf)->pool;
	return buf->pool;
}

/**
 * Get Memory Region (MR) <-> rte_mbuf association from txq->mp2mr[].
 * Add MP to txq->mp2mr[] if it's not registered yet. If mp2mr[] is full,
 * remove an entry first.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 *
 * @return
 *   mr->lkey on success, (uint32_t)-1 on failure.
 */
static __rte_always_inline uint32_t
mlx5_tx_mb2mr(struct mlx5_txq_data *txq, struct rte_mbuf *mb)
{
	uint16_t i = txq->mr_cache_idx;
	uintptr_t addr = rte_pktmbuf_mtod(mb, uintptr_t);
	struct mlx5_mr *mr;

	assert(i < RTE_DIM(txq->mp2mr));
	if (likely(txq->mp2mr[i]->start <= addr && txq->mp2mr[i]->end >= addr))
		return txq->mp2mr[i]->lkey;
	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (unlikely(txq->mp2mr[i]->mr == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq->mp2mr[i]->start <= addr &&
		    txq->mp2mr[i]->end >= addr) {
			assert(txq->mp2mr[i]->lkey != (uint32_t)-1);
			assert(rte_cpu_to_be_32(txq->mp2mr[i]->mr->lkey) ==
			       txq->mp2mr[i]->lkey);
			txq->mr_cache_idx = i;
			return txq->mp2mr[i]->lkey;
		}
	}
	txq->mr_cache_idx = 0;
	mr = mlx5_txq_mp2mr_reg(txq, mlx5_tx_mb2mp(mb), i);
	/*
	 * Request the reference to use in this queue, the original one is
	 * kept by the control plane.
	 */
	if (mr) {
		rte_atomic32_inc(&mr->refcnt);
		return mr->lkey;
	}
	return (uint32_t)-1;
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
	uint64_t *dst = (uint64_t *)((uintptr_t)txq->bf_reg);
	volatile uint64_t *src = ((volatile uint64_t *)wqe);

	rte_io_wmb();
	*txq->qp_db = rte_cpu_to_be_32(txq->wqe_ci);
	/* Ensure ordering between DB record and BF copy. */
	rte_wmb();
	*dst = *src;
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

#endif /* RTE_PMD_MLX5_RXTX_H_ */
