/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RX_H_
#define RTE_PMD_MLX5_RX_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_common.h>
#include <rte_spinlock.h>

#include <mlx5_common_mr.h>

#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "rte_pmd_mlx5.h"

/* Support tunnel matching. */
#define MLX5_FLOW_TUNNEL 10

#define RXQ_PORT(rxq_ctrl) LIST_FIRST(&(rxq_ctrl)->owners)->priv
#define RXQ_DEV(rxq_ctrl) ETH_DEV(RXQ_PORT(rxq_ctrl))
#define RXQ_PORT_ID(rxq_ctrl) PORT_ID(RXQ_PORT(rxq_ctrl))

/* First entry must be NULL for comparison. */
#define mlx5_mr_btree_len(bt) ((bt)->len - 1)

struct mlx5_rxq_stats {
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
#endif
	uint64_t idropped; /**< Total of packets dropped when RX ring full. */
	uint64_t rx_nombuf; /**< Total of RX mbuf allocation failures. */
};

/* Compressed CQE context. */
struct rxq_zip {
	uint16_t cqe_cnt; /* Number of CQEs. */
	uint16_t ai; /* Array index. */
	uint32_t ca; /* Current array index. */
	uint32_t na; /* Next array index. */
	uint32_t cq_ci; /* The next CQE. */
};

/* Get pointer to the first stride. */
#define mlx5_mprq_buf_addr(ptr, strd_n) (RTE_PTR_ADD((ptr), \
				sizeof(struct mlx5_mprq_buf) + \
				(strd_n) * \
				sizeof(struct rte_mbuf_ext_shared_info) + \
				RTE_PKTMBUF_HEADROOM))

#define MLX5_MIN_SINGLE_STRIDE_LOG_NUM_BYTES 6
#define MLX5_MIN_SINGLE_WQE_LOG_NUM_STRIDES 9

enum mlx5_rxq_err_state {
	MLX5_RXQ_ERR_STATE_NO_ERROR = 0,
	MLX5_RXQ_ERR_STATE_NEED_RESET,
	MLX5_RXQ_ERR_STATE_NEED_READY,
	MLX5_RXQ_ERR_STATE_IGNORE,
};

enum mlx5_rqx_code {
	MLX5_RXQ_CODE_EXIT = 0,
	MLX5_RXQ_CODE_NOMBUF,
	MLX5_RXQ_CODE_DROPPED,
};

struct mlx5_eth_rxseg {
	struct rte_mempool *mp; /**< Memory pool to allocate segment from. */
	uint16_t length; /**< Segment data length, configures split point. */
	uint16_t offset; /**< Data offset from beginning of mbuf data buffer. */
	uint32_t reserved; /**< Reserved field. */
};

/* RX queue descriptor. */
struct mlx5_rxq_data {
	unsigned int csum:1; /* Enable checksum offloading. */
	unsigned int hw_timestamp:1; /* Enable HW timestamp. */
	unsigned int rt_timestamp:1; /* Realtime timestamp format. */
	unsigned int vlan_strip:1; /* Enable VLAN stripping. */
	unsigned int crc_present:1; /* CRC must be subtracted. */
	unsigned int sges_n:3; /* Log 2 of SGEs (max buffers per packet). */
	unsigned int cqe_n:4; /* Log 2 of CQ elements. */
	unsigned int elts_n:4; /* Log 2 of Mbufs. */
	unsigned int rss_hash:1; /* RSS hash result is enabled. */
	unsigned int mark:1; /* Marked flow available on the queue. */
	unsigned int log_strd_num:5; /* Log 2 of the number of stride. */
	unsigned int log_strd_sz:4; /* Log 2 of stride size. */
	unsigned int strd_shift_en:1; /* Enable 2bytes shift on a stride. */
	unsigned int err_state:2; /* enum mlx5_rxq_err_state. */
	unsigned int strd_scatter_en:1; /* Scattered packets from a stride. */
	unsigned int lro:1; /* Enable LRO. */
	unsigned int dynf_meta:1; /* Dynamic metadata is configured. */
	unsigned int mcqe_format:3; /* CQE compression format. */
	unsigned int shared:1; /* Shared RXQ. */
	unsigned int delay_drop:1; /* Enable delay drop. */
	unsigned int cqe_comp_layout:1; /* CQE Compression Layout*/
	unsigned int cq_ci:24;
	volatile uint32_t *rq_db;
	volatile uint32_t *cq_db;
	uint16_t port_id;
	uint32_t elts_ci;
	uint32_t rq_ci;
	uint16_t consumed_strd; /* Number of consumed strides in WQE. */
	uint32_t rq_pi;
	uint16_t rq_repl_thresh; /* Threshold for buffer replenishment. */
	uint32_t byte_mask;
	union {
		struct rxq_zip zip; /* Compressed context. */
		uint16_t decompressed;
		/* Number of ready mbufs decompressed from the CQ. */
	};
	struct mlx5_mr_ctrl mr_ctrl; /* MR control descriptor. */
	uint16_t mprq_max_memcpy_len; /* Maximum size of packet to memcpy. */
	volatile void *wqes;
	volatile struct mlx5_cqe(*cqes)[];
	struct mlx5_cqe title_cqe; /* Title CQE for CQE compression. */
	struct rte_mbuf *(*elts)[];
	struct rte_mbuf title_pkt; /* Title packet for CQE compression. */
	struct mlx5_mprq_buf *(*mprq_bufs)[];
	struct rte_mempool *mp;
	struct rte_mempool *mprq_mp; /* Mempool for Multi-Packet RQ. */
	struct mlx5_mprq_buf *mprq_repl; /* Stashed mbuf for replenish. */
	struct mlx5_dev_ctx_shared *sh; /* Shared context. */
	uint16_t idx; /* Queue index. */
	struct mlx5_rxq_stats stats;
	struct mlx5_rxq_stats stats_reset; /* stats on last reset. */
	rte_xmm_t mbuf_initializer; /* Default rearm/flags for vectorized Rx. */
	struct rte_mbuf fake_mbuf; /* elts padding for vectorized Rx. */
	struct mlx5_uar_data uar_data; /* CQ doorbell. */
	uint32_t cqn; /* CQ number. */
	uint8_t cq_arm_sn; /* CQ arm seq number. */
	uint64_t mark_flag; /* ol_flags to set with marks. */
	uint32_t tunnel; /* Tunnel information. */
	int timestamp_offset; /* Dynamic mbuf field for timestamp. */
	uint64_t timestamp_rx_flag; /* Dynamic mbuf flag for timestamp. */
	uint64_t flow_meta_mask;
	int32_t flow_meta_offset;
	uint32_t flow_meta_port_mask;
	uint32_t rxseg_n; /* Number of split segment descriptions. */
	struct mlx5_eth_rxseg rxseg[MLX5_MAX_RXQ_NSEG];
	/* Buffer split segment descriptions - sizes, offsets, pools. */
} __rte_cache_aligned;

/* RX queue control descriptor. */
struct mlx5_rxq_ctrl {
	struct mlx5_rxq_data rxq; /* Data path structure. */
	LIST_ENTRY(mlx5_rxq_ctrl) next; /* Pointer to the next element. */
	LIST_HEAD(priv, mlx5_rxq_priv) owners; /* Owner rxq list. */
	struct mlx5_rxq_obj *obj; /* Verbs/DevX elements. */
	struct mlx5_dev_ctx_shared *sh; /* Shared context. */
	bool is_hairpin; /* Whether RxQ type is Hairpin. */
	unsigned int socket; /* CPU socket ID for allocations. */
	LIST_ENTRY(mlx5_rxq_ctrl) share_entry; /* Entry in shared RXQ list. */
	uint32_t share_group; /* Group ID of shared RXQ. */
	uint16_t share_qid; /* Shared RxQ ID in group. */
	unsigned int started:1; /* Whether (shared) RXQ has been started. */
	unsigned int irq:1; /* Whether IRQ is enabled. */
	uint32_t flow_tunnels_n[MLX5_FLOW_TUNNEL]; /* Tunnels counters. */
	uint32_t wqn; /* WQ number. */
	uint32_t rxseg_n; /* Number of split segment descriptions. */
	struct rte_eth_rxseg_split rxseg[MLX5_MAX_RXQ_NSEG];
	/* Saved original buffer split segment configuration. */
	uint16_t dump_file_n; /* Number of dump files. */
};

/* RX queue private data. */
struct mlx5_rxq_priv {
	uint16_t idx; /* Queue index. */
	uint32_t refcnt; /* Reference counter. */
	struct mlx5_rxq_ctrl *ctrl; /* Shared Rx Queue. */
	LIST_ENTRY(mlx5_rxq_priv) owner_entry; /* Entry in shared rxq_ctrl. */
	struct mlx5_priv *priv; /* Back pointer to private data. */
	struct mlx5_devx_rq devx_rq;
	struct rte_eth_hairpin_conf hairpin_conf; /* Hairpin configuration. */
	uint32_t hairpin_status; /* Hairpin binding status. */
	uint32_t lwm:16;
	uint32_t lwm_event_pending:1;
	uint32_t lwm_devx_subscribed:1;
};

/* External RX queue descriptor. */
struct mlx5_external_rxq {
	uint32_t hw_id; /* Queue index in the Hardware. */
	uint32_t refcnt; /* Reference counter. */
};

/* mlx5_rxq.c */

extern uint8_t rss_hash_default_key[];

unsigned int mlx5_rxq_cqe_num(struct mlx5_rxq_data *rxq_data);
int mlx5_mprq_free_mp(struct rte_eth_dev *dev);
int mlx5_mprq_alloc_mp(struct rte_eth_dev *dev);
int mlx5_rx_queue_start(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_rx_queue_stop(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_rx_queue_start_primary(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_rx_queue_stop_primary(struct rte_eth_dev *dev, uint16_t queue_id);
int mlx5_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			unsigned int socket, const struct rte_eth_rxconf *conf,
			struct rte_mempool *mp);
int mlx5_rx_hairpin_queue_setup
	(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
	 const struct rte_eth_hairpin_conf *hairpin_conf);
void mlx5_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
int mlx5_rx_intr_vec_enable(struct rte_eth_dev *dev);
void mlx5_rx_intr_vec_disable(struct rte_eth_dev *dev);
int mlx5_rx_intr_enable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int mlx5_rx_intr_disable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int mlx5_rxq_obj_verify(struct rte_eth_dev *dev);
struct mlx5_rxq_ctrl *mlx5_rxq_new(struct rte_eth_dev *dev, uint16_t idx,
				   uint16_t desc, unsigned int socket,
				   const struct rte_eth_rxconf *conf,
				   const struct rte_eth_rxseg_split *rx_seg,
				   uint16_t n_seg, bool is_extmem);
struct mlx5_rxq_ctrl *mlx5_rxq_hairpin_new
	(struct rte_eth_dev *dev, struct mlx5_rxq_priv *rxq, uint16_t desc,
	 const struct rte_eth_hairpin_conf *hairpin_conf);
struct mlx5_rxq_priv *mlx5_rxq_ref(struct rte_eth_dev *dev, uint16_t idx);
uint32_t mlx5_rxq_deref(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_rxq_priv *mlx5_rxq_get(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_rxq_ctrl *mlx5_rxq_ctrl_get(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_rxq_data *mlx5_rxq_data_get(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_external_rxq *mlx5_ext_rxq_ref(struct rte_eth_dev *dev,
					   uint16_t idx);
uint32_t mlx5_ext_rxq_deref(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_external_rxq *mlx5_ext_rxq_get(struct rte_eth_dev *dev,
					   uint16_t idx);
int mlx5_rxq_release(struct rte_eth_dev *dev, uint16_t idx);
int mlx5_rxq_verify(struct rte_eth_dev *dev);
int mlx5_ext_rxq_verify(struct rte_eth_dev *dev);
int rxq_alloc_elts(struct mlx5_rxq_ctrl *rxq_ctrl);
int mlx5_ind_table_obj_verify(struct rte_eth_dev *dev);
struct mlx5_ind_table_obj *mlx5_ind_table_obj_get(struct rte_eth_dev *dev,
						  const uint16_t *queues,
						  uint32_t queues_n);
struct mlx5_ind_table_obj *mlx5_ind_table_obj_new(struct rte_eth_dev *dev,
						  const uint16_t *queues,
						  uint32_t queues_n,
						  bool standalone,
						  bool ref_qs);
int mlx5_ind_table_obj_release(struct rte_eth_dev *dev,
			       struct mlx5_ind_table_obj *ind_tbl,
			       bool deref_rxqs);
int mlx5_ind_table_obj_setup(struct rte_eth_dev *dev,
			     struct mlx5_ind_table_obj *ind_tbl,
			     bool ref_qs);
int mlx5_ind_table_obj_modify(struct rte_eth_dev *dev,
			      struct mlx5_ind_table_obj *ind_tbl,
			      uint16_t *queues, const uint32_t queues_n,
			      bool standalone,
			      bool ref_new_qs, bool deref_old_qs);
int mlx5_ind_table_obj_attach(struct rte_eth_dev *dev,
			      struct mlx5_ind_table_obj *ind_tbl);
int mlx5_ind_table_obj_detach(struct rte_eth_dev *dev,
			      struct mlx5_ind_table_obj *ind_tbl);
struct mlx5_list_entry *mlx5_hrxq_create_cb(void *tool_ctx, void *cb_ctx);
int mlx5_hrxq_match_cb(void *tool_ctx, struct mlx5_list_entry *entry,
		       void *cb_ctx);
void mlx5_hrxq_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry);
struct mlx5_list_entry *mlx5_hrxq_clone_cb(void *tool_ctx,
					   struct mlx5_list_entry *entry,
					   void *cb_ctx __rte_unused);
void mlx5_hrxq_clone_free_cb(void *tool_ctx __rte_unused,
			     struct mlx5_list_entry *entry);
struct mlx5_hrxq *mlx5_hrxq_get(struct rte_eth_dev *dev,
		       struct mlx5_flow_rss_desc *rss_desc);
int mlx5_hrxq_obj_release(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq);
int mlx5_hrxq_release(struct rte_eth_dev *dev, uint32_t hxrq_idx);
uint32_t mlx5_hrxq_verify(struct rte_eth_dev *dev);
bool mlx5_rxq_is_hairpin(struct rte_eth_dev *dev, uint16_t idx);
const struct rte_eth_hairpin_conf *mlx5_rxq_get_hairpin_conf
	(struct rte_eth_dev *dev, uint16_t idx);
struct mlx5_hrxq *mlx5_drop_action_create(struct rte_eth_dev *dev);
void mlx5_drop_action_destroy(struct rte_eth_dev *dev);
uint64_t mlx5_get_rx_port_offloads(void);
uint64_t mlx5_get_rx_queue_offloads(struct rte_eth_dev *dev);
void mlx5_rxq_timestamp_set(struct rte_eth_dev *dev);
int mlx5_hrxq_modify(struct rte_eth_dev *dev, uint32_t hxrq_idx,
		     const uint8_t *rss_key, uint32_t rss_key_len,
		     uint64_t hash_fields, bool symmetric_hash_function,
		     const uint16_t *queues, uint32_t queues_n);

/* mlx5_rx.c */

uint16_t mlx5_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n);
void mlx5_rxq_initialize(struct mlx5_rxq_data *rxq);
__rte_noinline int mlx5_rx_err_handle(struct mlx5_rxq_data *rxq, uint8_t vec,
				      uint16_t err_n, uint16_t *skip_cnt);
void mlx5_mprq_buf_free(struct mlx5_mprq_buf *buf);
uint16_t mlx5_rx_burst_mprq(void *dpdk_rxq, struct rte_mbuf **pkts,
			    uint16_t pkts_n);
int mlx5_rx_descriptor_status(void *rx_queue, uint16_t offset);
uint32_t mlx5_rx_queue_count(void *rx_queue);
void mlx5_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		       struct rte_eth_rxq_info *qinfo);
int mlx5_rx_burst_mode_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
			   struct rte_eth_burst_mode *mode);
int mlx5_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc);
void mlx5_dev_interrupt_handler_lwm(void *args);
int mlx5_rx_queue_lwm_set(struct rte_eth_dev *dev, uint16_t rx_queue_id,
			  uint8_t lwm);
int mlx5_rx_queue_lwm_query(struct rte_eth_dev *dev, uint16_t *rx_queue_id,
			    uint8_t *lwm);

/* Vectorized version of mlx5_rx.c */
int mlx5_rxq_check_vec_support(struct mlx5_rxq_data *rxq_data);
int mlx5_check_vec_rx_support(struct rte_eth_dev *dev);
uint16_t mlx5_rx_burst_vec(void *dpdk_rxq, struct rte_mbuf **pkts,
			   uint16_t pkts_n);
uint16_t mlx5_rx_burst_mprq_vec(void *dpdk_rxq, struct rte_mbuf **pkts,
				uint16_t pkts_n);

static int mlx5_rxq_mprq_enabled(struct mlx5_rxq_data *rxq);

/**
 * Query LKey for an address on Rx. No need to flush local caches
 * as the Rx mempool database entries are valid for the lifetime of the queue.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 * @param addr
 *   Address to search.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 *   This function always succeeds on valid input.
 */
static __rte_always_inline uint32_t
mlx5_rx_addr2mr(struct mlx5_rxq_data *rxq, uintptr_t addr)
{
	struct mlx5_mr_ctrl *mr_ctrl = &rxq->mr_ctrl;
	struct rte_mempool *mp;
	uint32_t lkey;

	/* Linear search on MR cache array. */
	lkey = mlx5_mr_lookup_lkey(mr_ctrl->cache, &mr_ctrl->mru,
				   MLX5_MR_CACHE_N, addr);
	if (likely(lkey != UINT32_MAX))
		return lkey;
	mp = mlx5_rxq_mprq_enabled(rxq) ? rxq->mprq_mp : rxq->mp;
	return mlx5_mr_mempool2mr_bh(mr_ctrl, mp, addr);
}

/**
 * Query LKey from a packet buffer for Rx. No need to flush local caches
 * as the Rx mempool database entries are valid for the lifetime of the queue.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 * @param mb
 *   Buffer to search the address of.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 *   This function always succeeds on valid input.
 */
static __rte_always_inline uint32_t
mlx5_rx_mb2mr(struct mlx5_rxq_data *rxq, struct rte_mbuf *mb)
{
	struct mlx5_mr_ctrl *mr_ctrl = &rxq->mr_ctrl;
	uintptr_t addr = (uintptr_t)mb->buf_addr;
	uint32_t lkey;

	/* Linear search on MR cache array. */
	lkey = mlx5_mr_lookup_lkey(mr_ctrl->cache, &mr_ctrl->mru,
				   MLX5_MR_CACHE_N, addr);
	if (likely(lkey != UINT32_MAX))
		return lkey;
	/* Slower search in the mempool database on miss. */
	return mlx5_mr_mempool2mr_bh(mr_ctrl, mb->pool, addr);
}

/**
 * Set timestamp in mbuf dynamic field.
 *
 * @param mbuf
 *   Structure to write into.
 * @param offset
 *   Dynamic field offset in mbuf structure.
 * @param timestamp
 *   Value to write.
 */
static __rte_always_inline void
mlx5_timestamp_set(struct rte_mbuf *mbuf, int offset,
		rte_mbuf_timestamp_t timestamp)
{
	*RTE_MBUF_DYNFIELD(mbuf, offset, rte_mbuf_timestamp_t *) = timestamp;
}

/**
 * Replace MPRQ buffer.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 * @param rq_idx
 *   RQ index to replace.
 */
static __rte_always_inline void
mprq_buf_replace(struct mlx5_rxq_data *rxq, uint16_t rq_idx)
{
	const uint32_t strd_n = RTE_BIT32(rxq->log_strd_num);
	struct mlx5_mprq_buf *rep = rxq->mprq_repl;
	volatile struct mlx5_wqe_data_seg *wqe =
		&((volatile struct mlx5_wqe_mprq *)rxq->wqes)[rq_idx].dseg;
	struct mlx5_mprq_buf *buf = (*rxq->mprq_bufs)[rq_idx];
	void *addr;

	if (__atomic_load_n(&buf->refcnt, __ATOMIC_RELAXED) > 1) {
		MLX5_ASSERT(rep != NULL);
		/* Replace MPRQ buf. */
		(*rxq->mprq_bufs)[rq_idx] = rep;
		/* Replace WQE. */
		addr = mlx5_mprq_buf_addr(rep, strd_n);
		wqe->addr = rte_cpu_to_be_64((uintptr_t)addr);
		/* If there's only one MR, no need to replace LKey in WQE. */
		if (unlikely(mlx5_mr_btree_len(&rxq->mr_ctrl.cache_bh) > 1))
			wqe->lkey = mlx5_rx_addr2mr(rxq, (uintptr_t)addr);
		/* Stash a mbuf for next replacement. */
		if (likely(!rte_mempool_get(rxq->mprq_mp, (void **)&rep)))
			rxq->mprq_repl = rep;
		else
			rxq->mprq_repl = NULL;
		/* Release the old buffer. */
		mlx5_mprq_buf_free(buf);
	} else if (unlikely(rxq->mprq_repl == NULL)) {
		struct mlx5_mprq_buf *rep;

		/*
		 * Currently, the MPRQ mempool is out of buffer
		 * and doing memcpy regardless of the size of Rx
		 * packet. Retry allocation to get back to
		 * normal.
		 */
		if (!rte_mempool_get(rxq->mprq_mp, (void **)&rep))
			rxq->mprq_repl = rep;
	}
}

/**
 * Attach or copy MPRQ buffer content to a packet.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 * @param pkt
 *   Pointer to a packet to fill.
 * @param len
 *   Packet length.
 * @param buf
 *   Pointer to a MPRQ buffer to take the data from.
 * @param strd_idx
 *   Stride index to start from.
 * @param strd_cnt
 *   Number of strides to consume.
 */
static __rte_always_inline enum mlx5_rqx_code
mprq_buf_to_pkt(struct mlx5_rxq_data *rxq, struct rte_mbuf *pkt, uint32_t len,
		struct mlx5_mprq_buf *buf, uint16_t strd_idx, uint16_t strd_cnt)
{
	const uint32_t strd_n = RTE_BIT32(rxq->log_strd_num);
	const uint16_t strd_sz = RTE_BIT32(rxq->log_strd_sz);
	const uint16_t strd_shift =
		MLX5_MPRQ_STRIDE_SHIFT_BYTE * rxq->strd_shift_en;
	const int32_t hdrm_overlap =
		len + RTE_PKTMBUF_HEADROOM - strd_cnt * strd_sz;
	const uint32_t offset = strd_idx * strd_sz + strd_shift;
	void *addr = RTE_PTR_ADD(mlx5_mprq_buf_addr(buf, strd_n), offset);

	/*
	 * Memcpy packets to the target mbuf if:
	 * - The size of packet is smaller than mprq_max_memcpy_len.
	 * - Out of buffer in the Mempool for Multi-Packet RQ.
	 * - The packet's stride overlaps a headroom and scatter is off.
	 */
	if (len <= rxq->mprq_max_memcpy_len ||
	    rxq->mprq_repl == NULL ||
	    (hdrm_overlap > 0 && !rxq->strd_scatter_en)) {
		if (likely(len <=
			   (uint32_t)(pkt->buf_len - RTE_PKTMBUF_HEADROOM))) {
			rte_memcpy(rte_pktmbuf_mtod(pkt, void *),
				   addr, len);
			DATA_LEN(pkt) = len;
		} else if (rxq->strd_scatter_en) {
			struct rte_mbuf *prev = pkt;
			uint32_t seg_len = RTE_MIN(len, (uint32_t)
				(pkt->buf_len - RTE_PKTMBUF_HEADROOM));
			uint32_t rem_len = len - seg_len;

			rte_memcpy(rte_pktmbuf_mtod(pkt, void *),
				   addr, seg_len);
			DATA_LEN(pkt) = seg_len;
			while (rem_len) {
				struct rte_mbuf *next =
					rte_pktmbuf_alloc(rxq->mp);

				if (unlikely(next == NULL))
					return MLX5_RXQ_CODE_NOMBUF;
				NEXT(prev) = next;
				SET_DATA_OFF(next, 0);
				addr = RTE_PTR_ADD(addr, seg_len);
				seg_len = RTE_MIN(rem_len, (uint32_t)
					(next->buf_len - RTE_PKTMBUF_HEADROOM));
				rte_memcpy
					(rte_pktmbuf_mtod(next, void *),
					 addr, seg_len);
				DATA_LEN(next) = seg_len;
				rem_len -= seg_len;
				prev = next;
				++NB_SEGS(pkt);
			}
		} else {
			return MLX5_RXQ_CODE_DROPPED;
		}
	} else {
		rte_iova_t buf_iova;
		struct rte_mbuf_ext_shared_info *shinfo;
		uint16_t buf_len = strd_cnt * strd_sz;
		void *buf_addr;

		/* Increment the refcnt of the whole chunk. */
		__atomic_fetch_add(&buf->refcnt, 1, __ATOMIC_RELAXED);
		MLX5_ASSERT(__atomic_load_n(&buf->refcnt,
			    __ATOMIC_RELAXED) <= strd_n + 1);
		buf_addr = RTE_PTR_SUB(addr, RTE_PKTMBUF_HEADROOM);
		/*
		 * MLX5 device doesn't use iova but it is necessary in a
		 * case where the Rx packet is transmitted via a
		 * different PMD.
		 */
		buf_iova = rte_mempool_virt2iova(buf) +
			   RTE_PTR_DIFF(buf_addr, buf);
		shinfo = &buf->shinfos[strd_idx];
		rte_mbuf_ext_refcnt_set(shinfo, 1);
		/*
		 * RTE_MBUF_F_EXTERNAL will be set to pkt->ol_flags when
		 * attaching the stride to mbuf and more offload flags
		 * will be added below by calling rxq_cq_to_mbuf().
		 * Other fields will be overwritten.
		 */
		rte_pktmbuf_attach_extbuf(pkt, buf_addr, buf_iova,
					  buf_len, shinfo);
		/* Set mbuf head-room. */
		SET_DATA_OFF(pkt, RTE_PKTMBUF_HEADROOM);
		MLX5_ASSERT(pkt->ol_flags & RTE_MBUF_F_EXTERNAL);
		MLX5_ASSERT(rte_pktmbuf_tailroom(pkt) >=
			len - (hdrm_overlap > 0 ? hdrm_overlap : 0));
		DATA_LEN(pkt) = len;
		/*
		 * Copy the last fragment of a packet (up to headroom
		 * size bytes) in case there is a stride overlap with
		 * a next packet's headroom. Allocate a separate mbuf
		 * to store this fragment and link it. Scatter is on.
		 */
		if (hdrm_overlap > 0) {
			MLX5_ASSERT(rxq->strd_scatter_en);
			struct rte_mbuf *seg =
				rte_pktmbuf_alloc(rxq->mp);

			if (unlikely(seg == NULL))
				return MLX5_RXQ_CODE_NOMBUF;
			SET_DATA_OFF(seg, 0);
			rte_memcpy(rte_pktmbuf_mtod(seg, void *),
				RTE_PTR_ADD(addr, len - hdrm_overlap),
				hdrm_overlap);
			DATA_LEN(seg) = hdrm_overlap;
			DATA_LEN(pkt) = len - hdrm_overlap;
			NEXT(pkt) = seg;
			NB_SEGS(pkt) = 2;
		}
	}
	return MLX5_RXQ_CODE_EXIT;
}

/**
 * Check whether Multi-Packet RQ can be enabled for the device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
static __rte_always_inline int
mlx5_check_mprq_support(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->config.mprq.enabled &&
	    priv->rxqs_n >= priv->config.mprq.min_rxqs_num)
		return 1;
	return -ENOTSUP;
}

/**
 * Check whether Multi-Packet RQ is enabled for the Rx queue.
 *
 *  @param rxq
 *     Pointer to receive queue structure.
 *
 * @return
 *   0 if disabled, otherwise enabled.
 */
static __rte_always_inline int
mlx5_rxq_mprq_enabled(struct mlx5_rxq_data *rxq)
{
	return rxq->log_strd_num > 0;
}

/**
 * Check whether Multi-Packet RQ is enabled for the device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 if disabled, otherwise enabled.
 */
static __rte_always_inline int
mlx5_mprq_enabled(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t i;
	uint16_t n = 0;
	uint16_t n_ibv = 0;

	if (mlx5_check_mprq_support(dev) < 0)
		return 0;
	/* All the configured queues should be enabled. */
	for (i = 0; i < priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_rxq_ctrl_get(dev, i);

		if (rxq_ctrl == NULL || rxq_ctrl->is_hairpin)
			continue;
		n_ibv++;
		if (mlx5_rxq_mprq_enabled(&rxq_ctrl->rxq))
			++n;
	}
	/* Multi-Packet RQ can't be partially configured. */
	MLX5_ASSERT(n == 0 || n == n_ibv);
	return n == n_ibv;
}

/**
 * Check whether given RxQ is external.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param queue_idx
 *   Rx queue index.
 *
 * @return
 *   True if is external RxQ, otherwise false.
 */
static __rte_always_inline bool
mlx5_is_external_rxq(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_external_rxq *rxq;

	if (!priv->ext_rxqs || queue_idx < RTE_PMD_MLX5_EXTERNAL_RX_QUEUE_ID_MIN)
		return false;
	rxq = &priv->ext_rxqs[queue_idx - RTE_PMD_MLX5_EXTERNAL_RX_QUEUE_ID_MIN];
	return !!__atomic_load_n(&rxq->refcnt, __ATOMIC_RELAXED);
}

#define LWM_COOKIE_RXQID_OFFSET 0
#define LWM_COOKIE_RXQID_MASK 0xffff
#define LWM_COOKIE_PORTID_OFFSET 16
#define LWM_COOKIE_PORTID_MASK 0xffff

#endif /* RTE_PMD_MLX5_RX_H_ */
