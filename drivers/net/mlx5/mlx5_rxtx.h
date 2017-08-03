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

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5_hw.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

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
	uint64_t odropped; /**< Total of packets not sent when TX ring full. */
};

/* Flow director queue structure. */
struct fdir_queue {
	struct ibv_qp *qp; /* Associated RX QP. */
	struct ibv_exp_rwq_ind_table *ind_table; /* Indirection table. */
	struct ibv_exp_wq *wq; /* Work queue. */
	struct ibv_cq *cq; /* Completion queue. */
};

struct priv;

/* Compressed CQE context. */
struct rxq_zip {
	uint16_t ai; /* Array index. */
	uint16_t ca; /* Current array index. */
	uint16_t na; /* Next array index. */
	uint16_t cq_ci; /* The next CQE. */
	uint32_t cqe_cnt; /* Number of CQEs. */
};

/* RX queue descriptor. */
struct rxq {
	unsigned int csum:1; /* Enable checksum offloading. */
	unsigned int csum_l2tun:1; /* Same for L2 tunnels. */
	unsigned int vlan_strip:1; /* Enable VLAN stripping. */
	unsigned int crc_present:1; /* CRC must be subtracted. */
	unsigned int sges_n:2; /* Log 2 of SGEs (max buffers per packet). */
	unsigned int cqe_n:4; /* Log 2 of CQ elements. */
	unsigned int elts_n:4; /* Log 2 of Mbufs. */
	unsigned int port_id:8;
	unsigned int rss_hash:1; /* RSS hash result is enabled. */
	unsigned int mark:1; /* Marked flow available on the queue. */
	unsigned int pending_err:1; /* CQE error needs to be handled. */
	unsigned int trim_elts:1; /* Whether elts needs clean-up. */
	unsigned int :6; /* Remaining bits. */
	volatile uint32_t *rq_db;
	volatile uint32_t *cq_db;
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
} __rte_cache_aligned;

/* RX queue control descriptor. */
struct rxq_ctrl {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_exp_wq *wq; /* Work Queue. */
	struct fdir_queue *fdir_queue; /* Flow director queue. */
	struct ibv_mr *mr; /* Memory Region (for mp). */
	struct ibv_comp_channel *channel;
	unsigned int socket; /* CPU socket ID for allocations. */
	struct rxq rxq; /* Data path structure. */
};

/* Hash RX queue types. */
enum hash_rxq_type {
	HASH_RXQ_TCPV4,
	HASH_RXQ_UDPV4,
	HASH_RXQ_IPV4,
	HASH_RXQ_TCPV6,
	HASH_RXQ_UDPV6,
	HASH_RXQ_IPV6,
	HASH_RXQ_ETH,
};

/* Flow structure with Ethernet specification. It is packed to prevent padding
 * between attr and spec as this layout is expected by libibverbs. */
struct flow_attr_spec_eth {
	struct ibv_exp_flow_attr attr;
	struct ibv_exp_flow_spec_eth spec;
} __attribute__((packed));

/* Define a struct flow_attr_spec_eth object as an array of at least
 * "size" bytes. Room after the first index is normally used to store
 * extra flow specifications. */
#define FLOW_ATTR_SPEC_ETH(name, size) \
	struct flow_attr_spec_eth name \
		[((size) / sizeof(struct flow_attr_spec_eth)) + \
		 !!((size) % sizeof(struct flow_attr_spec_eth))]

/* Initialization data for hash RX queue. */
struct hash_rxq_init {
	uint64_t hash_fields; /* Fields that participate in the hash. */
	uint64_t dpdk_rss_hf; /* Matching DPDK RSS hash fields. */
	unsigned int flow_priority; /* Flow priority to use. */
	union {
		struct {
			enum ibv_exp_flow_spec_type type;
			uint16_t size;
		} hdr;
		struct ibv_exp_flow_spec_tcp_udp tcp_udp;
		struct ibv_exp_flow_spec_ipv4 ipv4;
		struct ibv_exp_flow_spec_ipv6 ipv6;
		struct ibv_exp_flow_spec_eth eth;
	} flow_spec; /* Flow specification template. */
	const struct hash_rxq_init *underlayer; /* Pointer to underlayer. */
};

/* Initialization data for indirection table. */
struct ind_table_init {
	unsigned int max_size; /* Maximum number of WQs. */
	/* Hash RX queues using this table. */
	unsigned int hash_types;
	unsigned int hash_types_n;
};

/* Initialization data for special flows. */
struct special_flow_init {
	uint8_t dst_mac_val[6];
	uint8_t dst_mac_mask[6];
	unsigned int hash_types;
	unsigned int per_vlan:1;
};

enum hash_rxq_flow_type {
	HASH_RXQ_FLOW_TYPE_PROMISC,
	HASH_RXQ_FLOW_TYPE_ALLMULTI,
	HASH_RXQ_FLOW_TYPE_BROADCAST,
	HASH_RXQ_FLOW_TYPE_IPV6MULTI,
	HASH_RXQ_FLOW_TYPE_MAC,
};

#ifndef NDEBUG
static inline const char *
hash_rxq_flow_type_str(enum hash_rxq_flow_type flow_type)
{
	switch (flow_type) {
	case HASH_RXQ_FLOW_TYPE_PROMISC:
		return "promiscuous";
	case HASH_RXQ_FLOW_TYPE_ALLMULTI:
		return "allmulticast";
	case HASH_RXQ_FLOW_TYPE_BROADCAST:
		return "broadcast";
	case HASH_RXQ_FLOW_TYPE_IPV6MULTI:
		return "IPv6 multicast";
	case HASH_RXQ_FLOW_TYPE_MAC:
		return "MAC";
	}
	return NULL;
}
#endif /* NDEBUG */

struct hash_rxq {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_qp *qp; /* Hash RX QP. */
	enum hash_rxq_type type; /* Hash RX queue type. */
	/* MAC flow steering rules, one per VLAN ID. */
	struct ibv_exp_flow *mac_flow
		[MLX5_MAX_MAC_ADDRESSES][MLX5_MAX_VLAN_IDS];
	struct ibv_exp_flow *special_flow
		[MLX5_MAX_SPECIAL_FLOWS][MLX5_MAX_VLAN_IDS];
};

/* TX queue descriptor. */
__extension__
struct txq {
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
	uint32_t qp_num_8s; /* QP number shifted by 8. */
	uint32_t flags; /* Flags for Tx Queue. */
	volatile struct mlx5_cqe (*cqes)[]; /* Completion queue. */
	volatile void *wqes; /* Work queue (use volatile to write into). */
	volatile uint32_t *qp_db; /* Work queue doorbell. */
	volatile uint32_t *cq_db; /* Completion queue doorbell. */
	volatile void *bf_reg; /* Blueflame register. */
	struct {
		uintptr_t start; /* Start address of MR */
		uintptr_t end; /* End address of MR */
		struct ibv_mr *mr; /* Memory Region (for mp). */
		uint32_t lkey; /* htonl(mr->lkey) */
	} mp2mr[MLX5_PMD_TX_MP_CACHE]; /* MP to MR translation table. */
	uint16_t mr_cache_idx; /* Index of last hit entry. */
	struct rte_mbuf *(*elts)[]; /* TX elements. */
	struct mlx5_txq_stats stats; /* TX queue counters. */
} __rte_cache_aligned;

/* TX queue control descriptor. */
struct txq_ctrl {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_qp *qp; /* Queue Pair. */
	unsigned int socket; /* CPU socket ID for allocations. */
	struct txq txq; /* Data path structure. */
};

/* mlx5_rxq.c */

extern const struct hash_rxq_init hash_rxq_init[];
extern const unsigned int hash_rxq_init_n;

extern uint8_t rss_hash_default_key[];
extern const size_t rss_hash_default_key_len;

size_t priv_flow_attr(struct priv *, struct ibv_exp_flow_attr *,
		      size_t, enum hash_rxq_type);
int priv_create_hash_rxqs(struct priv *);
void priv_destroy_hash_rxqs(struct priv *);
int priv_allow_flow_type(struct priv *, enum hash_rxq_flow_type);
int priv_rehash_flows(struct priv *);
void rxq_cleanup(struct rxq_ctrl *);
int mlx5_rx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_rxconf *, struct rte_mempool *);
void mlx5_rx_queue_release(void *);
uint16_t mlx5_rx_burst_secondary_setup(void *, struct rte_mbuf **, uint16_t);
int priv_rx_intr_vec_enable(struct priv *priv);
void priv_rx_intr_vec_disable(struct priv *priv);
#ifdef HAVE_UPDATE_CQ_CI
int mlx5_rx_intr_enable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int mlx5_rx_intr_disable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
#endif /* HAVE_UPDATE_CQ_CI */

/* mlx5_txq.c */

void txq_cleanup(struct txq_ctrl *);
int txq_ctrl_setup(struct rte_eth_dev *, struct txq_ctrl *, uint16_t,
		   unsigned int, const struct rte_eth_txconf *);
int mlx5_tx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_txconf *);
void mlx5_tx_queue_release(void *);
uint16_t mlx5_tx_burst_secondary_setup(void *, struct rte_mbuf **, uint16_t);

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
int rxq_check_vec_support(struct rxq *);
int priv_check_vec_rx_support(struct priv *);
void priv_prep_vec_rx_function(struct priv *);
uint16_t mlx5_tx_burst_raw_vec(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_vec(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_rx_burst_vec(void *, struct rte_mbuf **, uint16_t);

/* mlx5_mr.c */

struct ibv_mr *mlx5_mp2mr(struct ibv_pd *, struct rte_mempool *);
void txq_mp2mr_iter(struct rte_mempool *, void *);
uint32_t txq_mp2mr_reg(struct txq *, struct rte_mempool *, unsigned int);

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
		if (!check_cqe_seen(cqe))
			ERROR("unexpected CQE error %u (0x%02x)"
			      " syndrome 0x%02x",
			      op_code, op_code, syndrome);
		return 1;
	} else if ((op_code != MLX5_CQE_RESP_SEND) &&
		   (op_code != MLX5_CQE_REQ)) {
		if (!check_cqe_seen(cqe))
			ERROR("unexpected CQE opcode %u (0x%02x)",
			      op_code, op_code);
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
tx_mlx5_wqe(struct txq *txq, uint16_t ci)
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
mlx5_tx_complete(struct txq *txq)
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
		if (!check_cqe_seen(cqe))
			ERROR("unexpected error CQE, TX stopped");
		return;
	}
#endif /* NDEBUG */
	++cq_ci;
	txq->wqe_pi = ntohs(cqe->wqe_counter);
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
	rte_wmb();
	*txq->cq_db = htonl(cq_ci);
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
mlx5_tx_mb2mr(struct txq *txq, struct rte_mbuf *mb)
{
	uint16_t i = txq->mr_cache_idx;
	uintptr_t addr = rte_pktmbuf_mtod(mb, uintptr_t);

	assert(i < RTE_DIM(txq->mp2mr));
	if (likely(txq->mp2mr[i].start <= addr && txq->mp2mr[i].end >= addr))
		return txq->mp2mr[i].lkey;
	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (unlikely(txq->mp2mr[i].mr == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq->mp2mr[i].start <= addr &&
		    txq->mp2mr[i].end >= addr) {
			assert(txq->mp2mr[i].lkey != (uint32_t)-1);
			assert(htonl(txq->mp2mr[i].mr->lkey) ==
			       txq->mp2mr[i].lkey);
			txq->mr_cache_idx = i;
			return txq->mp2mr[i].lkey;
		}
	}
	txq->mr_cache_idx = 0;
	return txq_mp2mr_reg(txq, mlx5_tx_mb2mp(mb), i);
}

/**
 * Ring TX queue doorbell.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param wqe
 *   Pointer to the last WQE posted in the NIC.
 */
static __rte_always_inline void
mlx5_tx_dbrec(struct txq *txq, volatile struct mlx5_wqe *wqe)
{
	uint64_t *dst = (uint64_t *)((uintptr_t)txq->bf_reg);
	volatile uint64_t *src = ((volatile uint64_t *)wqe);

	rte_wmb();
	*txq->qp_db = htonl(txq->wqe_ci);
	/* Ensure ordering between DB record and BF copy. */
	rte_wmb();
	*dst = *src;
}

#endif /* RTE_PMD_MLX5_RXTX_H_ */
