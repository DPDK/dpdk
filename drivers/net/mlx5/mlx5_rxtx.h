/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
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
#include <rte_spinlock.h>
#include <rte_io.h>
#include <rte_bus_pci.h>

#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_mr.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

/* Support tunnel matching. */
#define MLX5_FLOW_TUNNEL 5

struct mlx5_rxq_stats {
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
#endif
	uint64_t idropped; /**< Total of packets dropped when RX ring full. */
	uint64_t rx_nombuf; /**< Total of RX mbuf allocation failures. */
};

struct mlx5_txq_stats {
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes; /**< Total of successfully sent bytes. */
#endif
	uint64_t oerrors; /**< Total number of failed transmitted packets. */
};

struct mlx5_priv;

/* Compressed CQE context. */
struct rxq_zip {
	uint16_t ai; /* Array index. */
	uint16_t ca; /* Current array index. */
	uint16_t na; /* Next array index. */
	uint16_t cq_ci; /* The next CQE. */
	uint32_t cqe_cnt; /* Number of CQEs. */
};

/* Multi-Packet RQ buffer header. */
struct mlx5_mprq_buf {
	struct rte_mempool *mp;
	rte_atomic16_t refcnt; /* Atomically accessed refcnt. */
	uint8_t pad[RTE_PKTMBUF_HEADROOM]; /* Headroom for the first packet. */
} __rte_cache_aligned;

/* Get pointer to the first stride. */
#define mlx5_mprq_buf_addr(ptr) ((ptr) + 1)

/* RX queue descriptor. */
struct mlx5_rxq_data {
	unsigned int csum:1; /* Enable checksum offloading. */
	unsigned int hw_timestamp:1; /* Enable HW timestamp. */
	unsigned int vlan_strip:1; /* Enable VLAN stripping. */
	unsigned int crc_present:1; /* CRC must be subtracted. */
	unsigned int sges_n:2; /* Log 2 of SGEs (max buffers per packet). */
	unsigned int cqe_n:4; /* Log 2 of CQ elements. */
	unsigned int elts_n:4; /* Log 2 of Mbufs. */
	unsigned int rss_hash:1; /* RSS hash result is enabled. */
	unsigned int mark:1; /* Marked flow available on the queue. */
	unsigned int strd_num_n:5; /* Log 2 of the number of stride. */
	unsigned int strd_sz_n:4; /* Log 2 of stride size. */
	unsigned int strd_shift_en:1; /* Enable 2bytes shift on a stride. */
	unsigned int :6; /* Remaining bits. */
	volatile uint32_t *rq_db;
	volatile uint32_t *cq_db;
	uint16_t port_id;
	uint32_t rq_ci;
	uint16_t consumed_strd; /* Number of consumed strides in WQE. */
	uint32_t rq_pi;
	uint32_t cq_ci;
	uint16_t rq_repl_thresh; /* Threshold for buffer replenishment. */
	struct mlx5_mr_ctrl mr_ctrl; /* MR control descriptor. */
	uint16_t mprq_max_memcpy_len; /* Maximum size of packet to memcpy. */
	volatile void *wqes;
	volatile struct mlx5_cqe(*cqes)[];
	struct rxq_zip zip; /* Compressed context. */
	RTE_STD_C11
	union  {
		struct rte_mbuf *(*elts)[];
		struct mlx5_mprq_buf *(*mprq_bufs)[];
	};
	struct rte_mempool *mp;
	struct rte_mempool *mprq_mp; /* Mempool for Multi-Packet RQ. */
	struct mlx5_mprq_buf *mprq_repl; /* Stashed mbuf for replenish. */
	uint16_t idx; /* Queue index. */
	struct mlx5_rxq_stats stats;
	uint64_t mbuf_initializer; /* Default rearm_data for vectorized Rx. */
	struct rte_mbuf fake_mbuf; /* elts padding for vectorized Rx. */
	void *cq_uar; /* CQ user access region. */
	uint32_t cqn; /* CQ number. */
	uint8_t cq_arm_sn; /* CQ arm seq number. */
#ifndef RTE_ARCH_64
	rte_spinlock_t *uar_lock_cq;
	/* CQ (UAR) access lock required for 32bit implementations */
#endif
	uint32_t tunnel; /* Tunnel information. */
} __rte_cache_aligned;

/* Verbs Rx queue elements. */
struct mlx5_rxq_ibv {
	LIST_ENTRY(mlx5_rxq_ibv) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct mlx5_rxq_ctrl *rxq_ctrl; /* Back pointer to parent. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_wq *wq; /* Work Queue. */
	struct ibv_comp_channel *channel;
};

/* RX queue control descriptor. */
struct mlx5_rxq_ctrl {
	struct mlx5_rxq_data rxq; /* Data path structure. */
	LIST_ENTRY(mlx5_rxq_ctrl) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct mlx5_rxq_ibv *ibv; /* Verbs elements. */
	struct mlx5_priv *priv; /* Back pointer to private data. */
	unsigned int socket; /* CPU socket ID for allocations. */
	unsigned int irq:1; /* Whether IRQ is enabled. */
	uint32_t flow_mark_n; /* Number of Mark/Flag flows using this Queue. */
	uint32_t flow_tunnels_n[MLX5_FLOW_TUNNEL]; /* Tunnels counters. */
};

/* Indirection table. */
struct mlx5_ind_table_ibv {
	LIST_ENTRY(mlx5_ind_table_ibv) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct ibv_rwq_ind_table *ind_table; /**< Indirection table. */
	uint32_t queues_n; /**< Number of queues in the list. */
	uint16_t queues[]; /**< Queue list. */
};

/* Hash Rx queue. */
struct mlx5_hrxq {
	LIST_ENTRY(mlx5_hrxq) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct mlx5_ind_table_ibv *ind_table; /* Indirection table. */
	struct ibv_qp *qp; /* Verbs queue pair. */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	void *action; /* DV QP action pointer. */
#endif
	uint64_t hash_fields; /* Verbs Hash fields. */
	uint32_t rss_key_len; /* Hash key length in bytes. */
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
#ifndef NDEBUG
	uint16_t cq_pi; /* Producer index for completion queue. */
#endif
	uint16_t wqe_ci; /* Consumer index for work queue. */
	uint16_t wqe_pi; /* Producer index for work queue. */
	uint16_t elts_n:4; /* (*elts)[] length (in log2). */
	uint16_t cqe_n:4; /* Number of CQ elements (in log2). */
	uint16_t wqe_n:4; /* Number of of WQ elements (in log2). */
	uint16_t tso_en:1; /* When set hardware TSO is enabled. */
	uint16_t tunnel_en:1;
	/* When set TX offload for tunneled packets are supported. */
	uint16_t swp_en:1; /* Whether SW parser is enabled. */
	uint16_t mpw_hdr_dseg:1; /* Enable DSEGs in the title WQEBB. */
	uint16_t max_inline; /* Multiple of RTE_CACHE_LINE_SIZE to inline. */
	uint16_t inline_max_packet_sz; /* Max packet size for inlining. */
	uint32_t qp_num_8s; /* QP number shifted by 8. */
	uint64_t offloads; /* Offloads for Tx Queue. */
	struct mlx5_mr_ctrl mr_ctrl; /* MR control descriptor. */
	volatile struct mlx5_cqe (*cqes)[]; /* Completion queue. */
	volatile void *wqes; /* Work queue (use volatile to write into). */
	volatile uint32_t *qp_db; /* Work queue doorbell. */
	volatile uint32_t *cq_db; /* Completion queue doorbell. */
	struct rte_mbuf *(*elts)[]; /* TX elements. */
	uint16_t port_id; /* Port ID of device. */
	uint16_t idx; /* Queue index. */
	struct mlx5_txq_stats stats; /* TX queue counters. */
#ifndef RTE_ARCH_64
	rte_spinlock_t *uar_lock;
	/* UAR access lock required for 32bit implementations */
#endif
} __rte_cache_aligned;

/* Verbs Rx queue elements. */
struct mlx5_txq_ibv {
	LIST_ENTRY(mlx5_txq_ibv) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	struct mlx5_txq_ctrl *txq_ctrl; /* Pointer to the control queue. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_qp *qp; /* Queue Pair. */
};

/* TX queue control descriptor. */
struct mlx5_txq_ctrl {
	struct mlx5_txq_data txq; /* Data path structure. */
	LIST_ENTRY(mlx5_txq_ctrl) next; /* Pointer to the next element. */
	rte_atomic32_t refcnt; /* Reference counter. */
	unsigned int socket; /* CPU socket ID for allocations. */
	unsigned int max_inline_data; /* Max inline data. */
	unsigned int max_tso_header; /* Max TSO header size. */
	struct mlx5_txq_ibv *ibv; /* Verbs queue object. */
	struct mlx5_priv *priv; /* Back pointer to private data. */
	off_t uar_mmap_offset; /* UAR mmap offset for non-primary process. */
	void *bf_reg; /* BlueFlame register from Verbs. */
};

#define MLX5_TX_BFREG(txq) \
		(MLX5_PROC_PRIV((txq)->port_id)->uar_table[(txq)->idx])

/* mlx5_rxq.c */

extern uint8_t rss_hash_default_key[];

int mlx5_check_mprq_support(struct rte_eth_dev *dev);
int mlx5_rxq_mprq_enabled(struct mlx5_rxq_data *rxq);
int mlx5_mprq_enabled(struct rte_eth_dev *dev);
int mlx5_mprq_free_mp(struct rte_eth_dev *dev);
int mlx5_mprq_alloc_mp(struct rte_eth_dev *dev);
void mlx5_rxq_cleanup(struct mlx5_rxq_ctrl *rxq_ctrl);
int mlx5_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			unsigned int socket, const struct rte_eth_rxconf *conf,
			struct rte_mempool *mp);
void mlx5_rx_queue_release(void *dpdk_rxq);
int mlx5_rx_intr_vec_enable(struct rte_eth_dev *dev);
void mlx5_rx_intr_vec_disable(struct rte_eth_dev *dev);
int mlx5_rx_intr_enable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int mlx5_rx_intr_disable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
struct mlx5_rxq_ibv *mlx5_rxq_ibv_new(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_rxq_ibv *mlx5_rxq_ibv_get(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_rxq_ibv_release(struct mlx5_rxq_ibv *rxq_ibv);
int mlx5_rxq_ibv_releasable(struct mlx5_rxq_ibv *rxq_ibv);
struct mlx5_rxq_ibv *mlx5_rxq_ibv_drop_new(struct rte_eth_dev *dev);
void mlx5_rxq_ibv_drop_release(struct rte_eth_dev *dev);
int mlx5_rxq_ibv_verify(struct rte_eth_dev *dev);
struct mlx5_rxq_ctrl *mlx5_rxq_new(struct rte_eth_dev *dev, uint16_t idx,
				   uint16_t desc, unsigned int socket,
				   const struct rte_eth_rxconf *conf,
				   struct rte_mempool *mp);
struct mlx5_rxq_ctrl *mlx5_rxq_get(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_rxq_release(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_rxq_releasable(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_rxq_verify(struct rte_eth_dev *dev);
int rxq_alloc_elts(struct mlx5_rxq_ctrl *rxq_ctrl);
int rxq_alloc_mprq_buf(struct mlx5_rxq_ctrl *rxq_ctrl);
struct mlx5_ind_table_ibv *mlx5_ind_table_ibv_new(struct rte_eth_dev *dev,
						  const uint16_t *queues,
						  uint32_t queues_n);
struct mlx5_ind_table_ibv *mlx5_ind_table_ibv_get(struct rte_eth_dev *dev,
						  const uint16_t *queues,
						  uint32_t queues_n);
int mlx5_ind_table_ibv_release(struct rte_eth_dev *dev,
			       struct mlx5_ind_table_ibv *ind_tbl);
int mlx5_ind_table_ibv_verify(struct rte_eth_dev *dev);
struct mlx5_ind_table_ibv *mlx5_ind_table_ibv_drop_new(struct rte_eth_dev *dev);
void mlx5_ind_table_ibv_drop_release(struct rte_eth_dev *dev);
struct mlx5_hrxq *mlx5_hrxq_new(struct rte_eth_dev *dev,
				const uint8_t *rss_key, uint32_t rss_key_len,
				uint64_t hash_fields,
				const uint16_t *queues, uint32_t queues_n,
				int tunnel __rte_unused);
struct mlx5_hrxq *mlx5_hrxq_get(struct rte_eth_dev *dev,
				const uint8_t *rss_key, uint32_t rss_key_len,
				uint64_t hash_fields,
				const uint16_t *queues, uint32_t queues_n);
int mlx5_hrxq_release(struct rte_eth_dev *dev, struct mlx5_hrxq *hxrq);
int mlx5_hrxq_ibv_verify(struct rte_eth_dev *dev);
struct mlx5_hrxq *mlx5_hrxq_drop_new(struct rte_eth_dev *dev);
void mlx5_hrxq_drop_release(struct rte_eth_dev *dev);
uint64_t mlx5_get_rx_port_offloads(void);
uint64_t mlx5_get_rx_queue_offloads(struct rte_eth_dev *dev);

/* mlx5_txq.c */

int mlx5_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			unsigned int socket, const struct rte_eth_txconf *conf);
void mlx5_tx_queue_release(void *dpdk_txq);
int mlx5_tx_uar_init_secondary(struct rte_eth_dev *dev, int fd);
struct mlx5_txq_ibv *mlx5_txq_ibv_new(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_txq_ibv *mlx5_txq_ibv_get(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_txq_ibv_release(struct mlx5_txq_ibv *txq_ibv);
int mlx5_txq_ibv_releasable(struct mlx5_txq_ibv *txq_ibv);
int mlx5_txq_ibv_verify(struct rte_eth_dev *dev);
struct mlx5_txq_ctrl *mlx5_txq_new(struct rte_eth_dev *dev, uint16_t idx,
				   uint16_t desc, unsigned int socket,
				   const struct rte_eth_txconf *conf);
struct mlx5_txq_ctrl *mlx5_txq_get(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_txq_release(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_txq_releasable(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_txq_verify(struct rte_eth_dev *dev);
void txq_alloc_elts(struct mlx5_txq_ctrl *txq_ctrl);
uint64_t mlx5_get_tx_port_offloads(struct rte_eth_dev *dev);

/* mlx5_rxtx.c */

extern uint32_t mlx5_ptype_table[];
extern uint8_t mlx5_cksum_table[];
extern uint8_t mlx5_swp_types_table[];

void mlx5_set_ptype_table(void);
void mlx5_set_cksum_table(void);
void mlx5_set_swp_types_table(void);
uint16_t mlx5_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts,
		       uint16_t pkts_n);
uint16_t mlx5_tx_burst_mpw(void *dpdk_txq, struct rte_mbuf **pkts,
			   uint16_t pkts_n);
uint16_t mlx5_tx_burst_mpw_inline(void *dpdk_txq, struct rte_mbuf **pkts,
				  uint16_t pkts_n);
uint16_t mlx5_tx_burst_empw(void *dpdk_txq, struct rte_mbuf **pkts,
			    uint16_t pkts_n);
uint16_t mlx5_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n);
void mlx5_mprq_buf_free_cb(void *addr, void *opaque);
void mlx5_mprq_buf_free(struct mlx5_mprq_buf *buf);
uint16_t mlx5_rx_burst_mprq(void *dpdk_rxq, struct rte_mbuf **pkts,
			    uint16_t pkts_n);
uint16_t removed_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts,
			  uint16_t pkts_n);
uint16_t removed_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts,
			  uint16_t pkts_n);
int mlx5_rx_descriptor_status(void *rx_queue, uint16_t offset);
int mlx5_tx_descriptor_status(void *tx_queue, uint16_t offset);
uint32_t mlx5_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id);

/* Vectorized version of mlx5_rxtx.c */
int mlx5_check_raw_vec_tx_support(struct rte_eth_dev *dev);
int mlx5_check_vec_tx_support(struct rte_eth_dev *dev);
int mlx5_rxq_check_vec_support(struct mlx5_rxq_data *rxq_data);
int mlx5_check_vec_rx_support(struct rte_eth_dev *dev);
uint16_t mlx5_tx_burst_raw_vec(void *dpdk_txq, struct rte_mbuf **pkts,
			       uint16_t pkts_n);
uint16_t mlx5_tx_burst_vec(void *dpdk_txq, struct rte_mbuf **pkts,
			   uint16_t pkts_n);
uint16_t mlx5_rx_burst_vec(void *dpdk_txq, struct rte_mbuf **pkts,
			   uint16_t pkts_n);

/* mlx5_mr.c */

void mlx5_mr_flush_local_cache(struct mlx5_mr_ctrl *mr_ctrl);
uint32_t mlx5_rx_addr2mr_bh(struct mlx5_rxq_data *rxq, uintptr_t addr);
uint32_t mlx5_tx_mb2mr_bh(struct mlx5_txq_data *txq, struct rte_mbuf *mb);
uint32_t mlx5_tx_update_ext_mp(struct mlx5_txq_data *txq, uintptr_t addr,
			       struct rte_mempool *mp);
int mlx5_dma_map(struct rte_pci_device *pdev, void *addr, uint64_t iova,
		 size_t len);
int mlx5_dma_unmap(struct rte_pci_device *pdev, void *addr, uint64_t iova,
		   size_t len);

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
	volatile uint8_t (*buf)[sizeof(cqe->rsvd1)] = &cqe->rsvd1;
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
			DRV_LOG(ERR,
				"unexpected CQE error %u (0x%02x) syndrome"
				" 0x%02x",
				op_code, op_code, syndrome);
			rte_hexdump(stderr, "MLX5 Error CQE:",
				    (const void *)((uintptr_t)err_cqe),
				    sizeof(*cqe));
		}
		return 1;
	} else if ((op_code != MLX5_CQE_RESP_SEND) &&
		   (op_code != MLX5_CQE_REQ)) {
		if (!check_cqe_seen(cqe)) {
			DRV_LOG(ERR, "unexpected CQE opcode %u (0x%02x)",
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
			DRV_LOG(ERR, "unexpected error CQE, Tx stopped");
			rte_hexdump(stderr, "MLX5 TXQ:",
				    (const void *)((uintptr_t)txq->wqes),
				    ((1 << txq->wqe_n) *
				     MLX5_WQE_SIZE));
		}
		return;
	}
#endif /* NDEBUG */
	++cq_ci;
	rte_cio_rmb();
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
mlx5_mb2mp(struct rte_mbuf *buf)
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
mlx5_rx_addr2mr(struct mlx5_rxq_data *rxq, uintptr_t addr)
{
	struct mlx5_mr_ctrl *mr_ctrl = &rxq->mr_ctrl;
	uint32_t lkey;

	/* Linear search on MR cache array. */
	lkey = mlx5_mr_lookup_cache(mr_ctrl->cache, &mr_ctrl->mru,
				    MLX5_MR_CACHE_N, addr);
	if (likely(lkey != UINT32_MAX))
		return lkey;
	/* Take slower bottom-half (Binary Search) on miss. */
	return mlx5_rx_addr2mr_bh(rxq, addr);
}

#define mlx5_rx_mb2mr(rxq, mb) mlx5_rx_addr2mr(rxq, (uintptr_t)((mb)->buf_addr))

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
	lkey = mlx5_mr_lookup_cache(mr_ctrl->cache, &mr_ctrl->mru,
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
	uint64_t *dst = MLX5_TX_BFREG(txq);
	volatile uint64_t *src = ((volatile uint64_t *)wqe);

	rte_cio_wmb();
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
 * Convert mbuf to Verb SWP.
 *
 * @param txq_data
 *   Pointer to the Tx queue.
 * @param buf
 *   Pointer to the mbuf.
 * @param offsets
 *   Pointer to the SWP header offsets.
 * @param swp_types
 *   Pointer to the SWP header types.
 */
static __rte_always_inline void
txq_mbuf_to_swp(struct mlx5_txq_data *txq, struct rte_mbuf *buf,
		uint8_t *offsets, uint8_t *swp_types)
{
	const uint64_t vlan = buf->ol_flags & PKT_TX_VLAN_PKT;
	const uint64_t tunnel = buf->ol_flags & PKT_TX_TUNNEL_MASK;
	const uint64_t tso = buf->ol_flags & PKT_TX_TCP_SEG;
	const uint64_t csum_flags = buf->ol_flags & PKT_TX_L4_MASK;
	const uint64_t inner_ip =
		buf->ol_flags & (PKT_TX_IPV4 | PKT_TX_IPV6);
	const uint64_t ol_flags_mask = PKT_TX_L4_MASK | PKT_TX_IPV6 |
				       PKT_TX_OUTER_IPV6;
	uint16_t idx;
	uint16_t off;

	if (likely(!txq->swp_en || (tunnel != PKT_TX_TUNNEL_UDP &&
				    tunnel != PKT_TX_TUNNEL_IP)))
		return;
	/*
	 * The index should have:
	 * bit[0:1] = PKT_TX_L4_MASK
	 * bit[4] = PKT_TX_IPV6
	 * bit[8] = PKT_TX_OUTER_IPV6
	 * bit[9] = PKT_TX_OUTER_UDP
	 */
	idx = (buf->ol_flags & ol_flags_mask) >> 52;
	if (tunnel == PKT_TX_TUNNEL_UDP)
		idx |= 1 << 9;
	*swp_types = mlx5_swp_types_table[idx];
	/*
	 * Set offsets for SW parser. Since ConnectX-5, SW parser just
	 * complements HW parser. SW parser starts to engage only if HW parser
	 * can't reach a header. For the older devices, HW parser will not kick
	 * in if any of SWP offsets is set. Therefore, all of the L3 offsets
	 * should be set regardless of HW offload.
	 */
	off = buf->outer_l2_len + (vlan ? sizeof(struct vlan_hdr) : 0);
	offsets[1] = off >> 1; /* Outer L3 offset. */
	off += buf->outer_l3_len;
	if (tunnel == PKT_TX_TUNNEL_UDP)
		offsets[0] = off >> 1; /* Outer L4 offset. */
	if (inner_ip) {
		off += buf->l2_len;
		offsets[3] = off >> 1; /* Inner L3 offset. */
		if (csum_flags == PKT_TX_TCP_CKSUM || tso ||
		    csum_flags == PKT_TX_UDP_CKSUM) {
			off += buf->l3_len;
			offsets[2] = off >> 1; /* Inner L4 offset. */
		}
	}
}

/**
 * Convert the Checksum offloads to Verbs.
 *
 * @param buf
 *   Pointer to the mbuf.
 *
 * @return
 *   Converted checksum flags.
 */
static __rte_always_inline uint8_t
txq_ol_cksum_to_cs(struct rte_mbuf *buf)
{
	uint32_t idx;
	uint8_t is_tunnel = !!(buf->ol_flags & PKT_TX_TUNNEL_MASK);
	const uint64_t ol_flags_mask = PKT_TX_TCP_SEG | PKT_TX_L4_MASK |
				       PKT_TX_IP_CKSUM | PKT_TX_OUTER_IP_CKSUM;

	/*
	 * The index should have:
	 * bit[0] = PKT_TX_TCP_SEG
	 * bit[2:3] = PKT_TX_UDP_CKSUM, PKT_TX_TCP_CKSUM
	 * bit[4] = PKT_TX_IP_CKSUM
	 * bit[8] = PKT_TX_OUTER_IP_CKSUM
	 * bit[9] = tunnel
	 */
	idx = ((buf->ol_flags & ol_flags_mask) >> 50) | (!!is_tunnel << 9);
	return mlx5_cksum_table[idx];
}

/**
 * Count the number of contiguous single segment packets.
 *
 * @param pkts
 *   Pointer to array of packets.
 * @param pkts_n
 *   Number of packets.
 *
 * @return
 *   Number of contiguous single segment packets.
 */
static __rte_always_inline unsigned int
txq_count_contig_single_seg(struct rte_mbuf **pkts, uint16_t pkts_n)
{
	unsigned int pos;

	if (!pkts_n)
		return 0;
	/* Count the number of contiguous single segment packets. */
	for (pos = 0; pos < pkts_n; ++pos)
		if (NB_SEGS(pkts[pos]) > 1)
			break;
	return pos;
}

/**
 * Count the number of contiguous multi-segment packets.
 *
 * @param pkts
 *   Pointer to array of packets.
 * @param pkts_n
 *   Number of packets.
 *
 * @return
 *   Number of contiguous multi-segment packets.
 */
static __rte_always_inline unsigned int
txq_count_contig_multi_seg(struct rte_mbuf **pkts, uint16_t pkts_n)
{
	unsigned int pos;

	if (!pkts_n)
		return 0;
	/* Count the number of contiguous multi-segment packets. */
	for (pos = 0; pos < pkts_n; ++pos)
		if (NB_SEGS(pkts[pos]) == 1)
			break;
	return pos;
}

#endif /* RTE_PMD_MLX5_RXTX_H_ */
