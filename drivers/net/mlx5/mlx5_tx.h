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
#include <rte_trace_point.h>

#include <mlx5_common.h>
#include <mlx5_common_mr.h>

#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_rxtx.h"
#include "mlx5_trace.h"

/* TX burst subroutines return codes. */
enum mlx5_txcmp_code {
	MLX5_TXCMP_CODE_EXIT = 0,
	MLX5_TXCMP_CODE_ERROR,
	MLX5_TXCMP_CODE_SINGLE,
	MLX5_TXCMP_CODE_MULTI,
	MLX5_TXCMP_CODE_TSO,
	MLX5_TXCMP_CODE_EMPW,
};

/*
 * These defines are used to configure Tx burst routine option set supported
 * at compile time. The not specified options are optimized out due to if
 * conditions can be explicitly calculated at compile time.
 * The offloads with bigger runtime check (require more CPU cycles toskip)
 * overhead should have the bigger index - this is needed to select the better
 * matching routine function if no exact match and some offloads are not
 * actually requested.
 */
#define MLX5_TXOFF_CONFIG_MULTI (1u << 0) /* Multi-segment packets.*/
#define MLX5_TXOFF_CONFIG_TSO (1u << 1) /* TCP send offload supported.*/
#define MLX5_TXOFF_CONFIG_SWP (1u << 2) /* Tunnels/SW Parser offloads.*/
#define MLX5_TXOFF_CONFIG_CSUM (1u << 3) /* Check Sums offloaded. */
#define MLX5_TXOFF_CONFIG_INLINE (1u << 4) /* Data inlining supported. */
#define MLX5_TXOFF_CONFIG_VLAN (1u << 5) /* VLAN insertion supported.*/
#define MLX5_TXOFF_CONFIG_METADATA (1u << 6) /* Flow metadata. */
#define MLX5_TXOFF_CONFIG_EMPW (1u << 8) /* Enhanced MPW supported.*/
#define MLX5_TXOFF_CONFIG_MPW (1u << 9) /* Legacy MPW supported.*/
#define MLX5_TXOFF_CONFIG_TXPP (1u << 10) /* Scheduling on timestamp.*/

/* The most common offloads groups. */
#define MLX5_TXOFF_CONFIG_NONE 0
#define MLX5_TXOFF_CONFIG_FULL (MLX5_TXOFF_CONFIG_MULTI | \
				MLX5_TXOFF_CONFIG_TSO | \
				MLX5_TXOFF_CONFIG_SWP | \
				MLX5_TXOFF_CONFIG_CSUM | \
				MLX5_TXOFF_CONFIG_INLINE | \
				MLX5_TXOFF_CONFIG_VLAN | \
				MLX5_TXOFF_CONFIG_METADATA)

#define MLX5_TXOFF_CONFIG(mask) (olx & MLX5_TXOFF_CONFIG_##mask)

#define MLX5_TXOFF_PRE_DECL(func) \
uint16_t mlx5_tx_burst_##func(void *txq, \
			      struct rte_mbuf **pkts, \
			      uint16_t pkts_n)

#define MLX5_TXOFF_DECL(func, olx) \
uint16_t mlx5_tx_burst_##func(void *txq, \
			      struct rte_mbuf **pkts, \
			      uint16_t pkts_n) \
{ \
	return mlx5_tx_burst_tmpl((struct mlx5_txq_data *)txq, \
		    pkts, pkts_n, (olx)); \
}

/* Mbuf dynamic flag offset for inline. */
extern uint64_t rte_net_mlx5_dynf_inline_mask;
#define RTE_MBUF_F_TX_DYNF_NOINLINE rte_net_mlx5_dynf_inline_mask

extern uint32_t mlx5_ptype_table[] __rte_cache_aligned;
extern uint8_t mlx5_cksum_table[1 << 10] __rte_cache_aligned;
extern uint8_t mlx5_swp_types_table[1 << 10] __rte_cache_aligned;

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
	uint16_t rt_timestamp:1; /* Realtime timestamp format. */
	uint16_t wait_on_time:1; /* WQE with timestamp is supported. */
	uint16_t fast_free:1; /* mbuf fast free on Tx is enabled. */
	uint16_t inlen_send; /* Ordinary send data inline size. */
	uint16_t inlen_empw; /* eMPW max packet size to inline. */
	uint16_t inlen_mode; /* Minimal data length to inline. */
	uint8_t tx_aggr_affinity; /* TxQ affinity configuration. */
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
	uint64_t rt_timemask; /* Scheduling timestamp mask. */
	uint64_t ts_mask; /* Timestamp flag dynamic mask. */
	uint64_t ts_last; /* Last scheduled timestamp. */
	int32_t ts_offset; /* Timestamp field dynamic offset. */
	struct mlx5_dev_ctx_shared *sh; /* Shared context. */
	struct mlx5_txq_stats stats; /* TX queue counters. */
	struct mlx5_txq_stats stats_reset; /* stats on last reset. */
	struct mlx5_uar_data uar_data;
	struct rte_mbuf *elts[0];
	/* Storage for queued packets, must be the last field. */
} __rte_cache_aligned;

/* TX queue control descriptor. */
struct mlx5_txq_ctrl {
	LIST_ENTRY(mlx5_txq_ctrl) next; /* Pointer to the next element. */
	uint32_t refcnt; /* Reference counter. */
	unsigned int socket; /* CPU socket ID for allocations. */
	bool is_hairpin; /* Whether TxQ type is Hairpin. */
	unsigned int max_inline_data; /* Max inline data. */
	unsigned int max_tso_header; /* Max TSO header size. */
	struct mlx5_txq_obj *obj; /* Verbs/DevX queue object. */
	struct mlx5_priv *priv; /* Back pointer to private data. */
	off_t uar_mmap_offset; /* UAR mmap offset for non-primary process. */
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
void mlx5_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
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
int mlx5_txq_get_sqn(struct mlx5_txq_ctrl *txq);
void txq_alloc_elts(struct mlx5_txq_ctrl *txq_ctrl);
void txq_free_elts(struct mlx5_txq_ctrl *txq_ctrl);
uint64_t mlx5_get_tx_port_offloads(struct rte_eth_dev *dev);
void mlx5_txq_dynf_timestamp_set(struct rte_eth_dev *dev);
int mlx5_count_aggr_ports(struct rte_eth_dev *dev);
int mlx5_map_aggr_tx_affinity(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			      uint8_t affinity);

/* mlx5_tx.c */

void mlx5_tx_handle_completion(struct mlx5_txq_data *__rte_restrict txq,
			       unsigned int olx __rte_unused);
int mlx5_tx_descriptor_status(void *tx_queue, uint16_t offset);
void mlx5_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		       struct rte_eth_txq_info *qinfo);
int mlx5_tx_burst_mode_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			   struct rte_eth_burst_mode *mode);

/* mlx5_tx_empw.c */

MLX5_TXOFF_PRE_DECL(full_empw);
MLX5_TXOFF_PRE_DECL(none_empw);
MLX5_TXOFF_PRE_DECL(md_empw);
MLX5_TXOFF_PRE_DECL(mt_empw);
MLX5_TXOFF_PRE_DECL(mtsc_empw);
MLX5_TXOFF_PRE_DECL(mti_empw);
MLX5_TXOFF_PRE_DECL(mtv_empw);
MLX5_TXOFF_PRE_DECL(mtiv_empw);
MLX5_TXOFF_PRE_DECL(sc_empw);
MLX5_TXOFF_PRE_DECL(sci_empw);
MLX5_TXOFF_PRE_DECL(scv_empw);
MLX5_TXOFF_PRE_DECL(sciv_empw);
MLX5_TXOFF_PRE_DECL(i_empw);
MLX5_TXOFF_PRE_DECL(v_empw);
MLX5_TXOFF_PRE_DECL(iv_empw);

/* mlx5_tx_nompw.c */

MLX5_TXOFF_PRE_DECL(full);
MLX5_TXOFF_PRE_DECL(none);
MLX5_TXOFF_PRE_DECL(md);
MLX5_TXOFF_PRE_DECL(mt);
MLX5_TXOFF_PRE_DECL(mtsc);
MLX5_TXOFF_PRE_DECL(mti);
MLX5_TXOFF_PRE_DECL(mtv);
MLX5_TXOFF_PRE_DECL(mtiv);
MLX5_TXOFF_PRE_DECL(sc);
MLX5_TXOFF_PRE_DECL(sci);
MLX5_TXOFF_PRE_DECL(scv);
MLX5_TXOFF_PRE_DECL(sciv);
MLX5_TXOFF_PRE_DECL(i);
MLX5_TXOFF_PRE_DECL(v);
MLX5_TXOFF_PRE_DECL(iv);

/* mlx5_tx_txpp.c */

MLX5_TXOFF_PRE_DECL(full_ts_nompw);
MLX5_TXOFF_PRE_DECL(full_ts_nompwi);
MLX5_TXOFF_PRE_DECL(full_ts);
MLX5_TXOFF_PRE_DECL(full_ts_noi);
MLX5_TXOFF_PRE_DECL(none_ts);
MLX5_TXOFF_PRE_DECL(mdi_ts);
MLX5_TXOFF_PRE_DECL(mti_ts);
MLX5_TXOFF_PRE_DECL(mtiv_ts);

/* mlx5_tx_mpw.c */

MLX5_TXOFF_PRE_DECL(none_mpw);
MLX5_TXOFF_PRE_DECL(mci_mpw);
MLX5_TXOFF_PRE_DECL(mc_mpw);
MLX5_TXOFF_PRE_DECL(i_mpw);

static __rte_always_inline struct mlx5_uar_data *
mlx5_tx_bfreg(struct mlx5_txq_data *txq)
{
	return &MLX5_PROC_PRIV(txq->port_id)->uar_table[txq->idx];
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
	mlx5_doorbell_ring(mlx5_tx_bfreg(txq), *(volatile uint64_t *)wqe,
			   txq->wqe_ci, txq->qp_db, 1);
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

/**
 * Set Software Parser flags and offsets in Ethernet Segment of WQE.
 * Flags must be preliminary initialized to zero.
 *
 * @param loc
 *   Pointer to burst routine local context.
 * @param swp_flags
 *   Pointer to store Software Parser flags.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Software Parser offsets packed in dword.
 *   Software Parser flags are set by pointer.
 */
static __rte_always_inline uint32_t
txq_mbuf_to_swp(struct mlx5_txq_local *__rte_restrict loc,
		uint8_t *swp_flags,
		unsigned int olx)
{
	uint64_t ol, tunnel;
	unsigned int idx, off;
	uint32_t set;

	if (!MLX5_TXOFF_CONFIG(SWP))
		return 0;
	ol = loc->mbuf->ol_flags;
	tunnel = ol & RTE_MBUF_F_TX_TUNNEL_MASK;
	/*
	 * Check whether Software Parser is required.
	 * Only customized tunnels may ask for.
	 */
	if (likely(tunnel != RTE_MBUF_F_TX_TUNNEL_UDP && tunnel != RTE_MBUF_F_TX_TUNNEL_IP))
		return 0;
	/*
	 * The index should have:
	 * bit[0:1] = RTE_MBUF_F_TX_L4_MASK
	 * bit[4] = RTE_MBUF_F_TX_IPV6
	 * bit[8] = RTE_MBUF_F_TX_OUTER_IPV6
	 * bit[9] = RTE_MBUF_F_TX_OUTER_UDP
	 */
	idx = (ol & (RTE_MBUF_F_TX_L4_MASK | RTE_MBUF_F_TX_IPV6 | RTE_MBUF_F_TX_OUTER_IPV6)) >> 52;
	idx |= (tunnel == RTE_MBUF_F_TX_TUNNEL_UDP) ? (1 << 9) : 0;
	*swp_flags = mlx5_swp_types_table[idx];
	/*
	 * Set offsets for SW parser. Since ConnectX-5, SW parser just
	 * complements HW parser. SW parser starts to engage only if HW parser
	 * can't reach a header. For the older devices, HW parser will not kick
	 * in if any of SWP offsets is set. Therefore, all of the L3 offsets
	 * should be set regardless of HW offload.
	 */
	off = loc->mbuf->outer_l2_len;
	if (MLX5_TXOFF_CONFIG(VLAN) && ol & RTE_MBUF_F_TX_VLAN)
		off += sizeof(struct rte_vlan_hdr);
	set = (off >> 1) << 8; /* Outer L3 offset. */
	off += loc->mbuf->outer_l3_len;
	if (tunnel == RTE_MBUF_F_TX_TUNNEL_UDP)
		set |= off >> 1; /* Outer L4 offset. */
	if (ol & (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IPV6)) { /* Inner IP. */
		const uint64_t csum = ol & RTE_MBUF_F_TX_L4_MASK;
			off += loc->mbuf->l2_len;
		set |= (off >> 1) << 24; /* Inner L3 offset. */
		if (csum == RTE_MBUF_F_TX_TCP_CKSUM ||
		    csum == RTE_MBUF_F_TX_UDP_CKSUM ||
		    (MLX5_TXOFF_CONFIG(TSO) && ol & RTE_MBUF_F_TX_TCP_SEG)) {
			off += loc->mbuf->l3_len;
			set |= (off >> 1) << 16; /* Inner L4 offset. */
		}
	}
	set = rte_cpu_to_le_32(set);
	return set;
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
	uint8_t is_tunnel = !!(buf->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK);
	const uint64_t ol_flags_mask = RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_L4_MASK |
				       RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_OUTER_IP_CKSUM;

	/*
	 * The index should have:
	 * bit[0] = RTE_MBUF_F_TX_TCP_SEG
	 * bit[2:3] = RTE_MBUF_F_TX_UDP_CKSUM, RTE_MBUF_F_TX_TCP_CKSUM
	 * bit[4] = RTE_MBUF_F_TX_IP_CKSUM
	 * bit[8] = RTE_MBUF_F_TX_OUTER_IP_CKSUM
	 * bit[9] = tunnel
	 */
	idx = ((buf->ol_flags & ol_flags_mask) >> 50) | (!!is_tunnel << 9);
	return mlx5_cksum_table[idx];
}

/**
 * Free the mbufs from the linear array of pointers.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param pkts
 *   Pointer to array of packets to be free.
 * @param pkts_n
 *   Number of packets to be freed.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_free_mbuf(struct mlx5_txq_data *__rte_restrict txq,
		  struct rte_mbuf **__rte_restrict pkts,
		  unsigned int pkts_n,
		  unsigned int olx __rte_unused)
{
	struct rte_mempool *pool = NULL;
	struct rte_mbuf **p_free = NULL;
	struct rte_mbuf *mbuf;
	unsigned int n_free = 0;

	/*
	 * The implemented algorithm eliminates
	 * copying pointers to temporary array
	 * for rte_mempool_put_bulk() calls.
	 */
	MLX5_ASSERT(pkts);
	MLX5_ASSERT(pkts_n);
	/*
	 * Free mbufs directly to the pool in bulk
	 * if fast free offload is engaged
	 */
	if (!MLX5_TXOFF_CONFIG(MULTI) && txq->fast_free) {
		mbuf = *pkts;
		pool = mbuf->pool;
		rte_mempool_put_bulk(pool, (void *)pkts, pkts_n);
		return;
	}
	for (;;) {
		for (;;) {
			/*
			 * Decrement mbuf reference counter, detach
			 * indirect and external buffers if needed.
			 */
			mbuf = rte_pktmbuf_prefree_seg(*pkts);
			if (likely(mbuf != NULL)) {
				MLX5_ASSERT(mbuf == *pkts);
				if (likely(n_free != 0)) {
					if (unlikely(pool != mbuf->pool))
						/* From different pool. */
						break;
				} else {
					/* Start new scan array. */
					pool = mbuf->pool;
					p_free = pkts;
				}
				++n_free;
				++pkts;
				--pkts_n;
				if (unlikely(pkts_n == 0)) {
					mbuf = NULL;
					break;
				}
			} else {
				/*
				 * This happens if mbuf is still referenced.
				 * We can't put it back to the pool, skip.
				 */
				++pkts;
				--pkts_n;
				if (unlikely(n_free != 0))
					/* There is some array to free.*/
					break;
				if (unlikely(pkts_n == 0))
					/* Last mbuf, nothing to free. */
					return;
			}
		}
		for (;;) {
			/*
			 * This loop is implemented to avoid multiple
			 * inlining of rte_mempool_put_bulk().
			 */
			MLX5_ASSERT(pool);
			MLX5_ASSERT(p_free);
			MLX5_ASSERT(n_free);
			/*
			 * Free the array of pre-freed mbufs
			 * belonging to the same memory pool.
			 */
			rte_mempool_put_bulk(pool, (void *)p_free, n_free);
			if (unlikely(mbuf != NULL)) {
				/* There is the request to start new scan. */
				pool = mbuf->pool;
				p_free = pkts++;
				n_free = 1;
				--pkts_n;
				if (likely(pkts_n != 0))
					break;
				/*
				 * This is the last mbuf to be freed.
				 * Do one more loop iteration to complete.
				 * This is rare case of the last unique mbuf.
				 */
				mbuf = NULL;
				continue;
			}
			if (likely(pkts_n == 0))
				return;
			n_free = 0;
			break;
		}
	}
}

/**
 * No inline version to free buffers for optimal call
 * on the tx_burst completion.
 */
static __rte_noinline void
__mlx5_tx_free_mbuf(struct mlx5_txq_data *__rte_restrict txq,
		    struct rte_mbuf **__rte_restrict pkts,
		    unsigned int pkts_n,
		    unsigned int olx __rte_unused)
{
	mlx5_tx_free_mbuf(txq, pkts, pkts_n, olx);
}

/**
 * Free the mbuf from the elts ring buffer till new tail.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param tail
 *   Index in elts to free up to, becomes new elts tail.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_free_elts(struct mlx5_txq_data *__rte_restrict txq,
		  uint16_t tail,
		  unsigned int olx __rte_unused)
{
	uint16_t n_elts = tail - txq->elts_tail;

	MLX5_ASSERT(n_elts);
	MLX5_ASSERT(n_elts <= txq->elts_s);
	/*
	 * Implement a loop to support ring buffer wraparound
	 * with single inlining of mlx5_tx_free_mbuf().
	 */
	do {
		unsigned int part;

		part = txq->elts_s - (txq->elts_tail & txq->elts_m);
		part = RTE_MIN(part, n_elts);
		MLX5_ASSERT(part);
		MLX5_ASSERT(part <= txq->elts_s);
		mlx5_tx_free_mbuf(txq,
				  &txq->elts[txq->elts_tail & txq->elts_m],
				  part, olx);
		txq->elts_tail += part;
		n_elts -= part;
	} while (n_elts);
}

/**
 * Store the mbuf being sent into elts ring buffer.
 * On Tx completion these mbufs will be freed.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param pkts
 *   Pointer to array of packets to be stored.
 * @param pkts_n
 *   Number of packets to be stored.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_copy_elts(struct mlx5_txq_data *__rte_restrict txq,
		  struct rte_mbuf **__rte_restrict pkts,
		  unsigned int pkts_n,
		  unsigned int olx __rte_unused)
{
	unsigned int part;
	struct rte_mbuf **elts = (struct rte_mbuf **)txq->elts;

	MLX5_ASSERT(pkts);
	MLX5_ASSERT(pkts_n);
	part = txq->elts_s - (txq->elts_head & txq->elts_m);
	MLX5_ASSERT(part);
	MLX5_ASSERT(part <= txq->elts_s);
	/* This code is a good candidate for vectorizing with SIMD. */
	rte_memcpy((void *)(elts + (txq->elts_head & txq->elts_m)),
		   (void *)pkts,
		   RTE_MIN(part, pkts_n) * sizeof(struct rte_mbuf *));
	txq->elts_head += pkts_n;
	if (unlikely(part < pkts_n))
		/* The copy is wrapping around the elts array. */
		rte_memcpy((void *)elts, (void *)(pkts + part),
			   (pkts_n - part) * sizeof(struct rte_mbuf *));
}

/**
 * Check if the completion request flag should be set in the last WQE.
 * Both pushed mbufs and WQEs are monitored and the completion request
 * flag is set if any of thresholds is reached.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_request_completion(struct mlx5_txq_data *__rte_restrict txq,
			   struct mlx5_txq_local *__rte_restrict loc,
			   unsigned int olx)
{
	uint16_t head = txq->elts_head;
	unsigned int part;

	part = MLX5_TXOFF_CONFIG(INLINE) ?
	       0 : loc->pkts_sent - loc->pkts_copy;
	head += part;
	if ((uint16_t)(head - txq->elts_comp) >= MLX5_TX_COMP_THRESH ||
	     (MLX5_TXOFF_CONFIG(INLINE) &&
	     (uint16_t)(txq->wqe_ci - txq->wqe_comp) >= txq->wqe_thres)) {
		volatile struct mlx5_wqe *last = loc->wqe_last;

		MLX5_ASSERT(last);
		txq->elts_comp = head;
		if (MLX5_TXOFF_CONFIG(INLINE))
			txq->wqe_comp = txq->wqe_ci;
		/* Request unconditional completion on last WQE. */
		last->cseg.flags = RTE_BE32(MLX5_COMP_ALWAYS <<
					    MLX5_COMP_MODE_OFFSET);
		/* Save elts_head in dedicated free on completion queue. */
#ifdef RTE_LIBRTE_MLX5_DEBUG
		txq->fcqs[txq->cq_pi++ & txq->cqe_m] = head |
			  (last->cseg.opcode >> 8) << 16;
#else
		txq->fcqs[txq->cq_pi++ & txq->cqe_m] = head;
#endif
		/* A CQE slot must always be available. */
		MLX5_ASSERT((txq->cq_pi - txq->cq_ci) <= txq->cqe_s);
	}
}

/**
 * Set completion request flag for all issued WQEs.
 * This routine is intended to be used with enabled fast path tracing
 * and send scheduling on time to provide the detailed report in trace
 * for send completions on every WQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_request_completion_trace(struct mlx5_txq_data *__rte_restrict txq,
				 struct mlx5_txq_local *__rte_restrict loc,
				 unsigned int olx)
{
	uint16_t head = txq->elts_comp;

	while (txq->wqe_comp != txq->wqe_ci) {
		volatile struct mlx5_wqe *wqe;
		uint32_t wqe_n;

		MLX5_ASSERT(loc->wqe_last);
		wqe = txq->wqes + (txq->wqe_comp & txq->wqe_m);
		if (wqe == loc->wqe_last) {
			head = txq->elts_head;
			head +=	MLX5_TXOFF_CONFIG(INLINE) ?
				0 : loc->pkts_sent - loc->pkts_copy;
			txq->elts_comp = head;
		}
		/* Completion request flag was set on cseg constructing. */
#ifdef RTE_LIBRTE_MLX5_DEBUG
		txq->fcqs[txq->cq_pi++ & txq->cqe_m] = head |
			  (wqe->cseg.opcode >> 8) << 16;
#else
		txq->fcqs[txq->cq_pi++ & txq->cqe_m] = head;
#endif
		/* A CQE slot must always be available. */
		MLX5_ASSERT((txq->cq_pi - txq->cq_ci) <= txq->cqe_s);
		/* Advance to the next WQE in the queue. */
		wqe_n = rte_be_to_cpu_32(wqe->cseg.sq_ds) & 0x3F;
		txq->wqe_comp += RTE_ALIGN(wqe_n, 4) / 4;
	}
}

/**
 * Build the Control Segment with specified opcode:
 * - MLX5_OPCODE_SEND
 * - MLX5_OPCODE_ENHANCED_MPSW
 * - MLX5_OPCODE_TSO
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Control Segment.
 * @param ds
 *   Supposed length of WQE in segments.
 * @param opcode
 *   SQ WQE opcode to put into Control Segment.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_cseg_init(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc __rte_unused,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int ds,
		  unsigned int opcode,
		  unsigned int olx)
{
	struct mlx5_wqe_cseg *__rte_restrict cs = &wqe->cseg;

	/* For legacy MPW replace the EMPW by TSO with modifier. */
	if (MLX5_TXOFF_CONFIG(MPW) && opcode == MLX5_OPCODE_ENHANCED_MPSW)
		opcode = MLX5_OPCODE_TSO | MLX5_OPC_MOD_MPW << 24;
	cs->opcode = rte_cpu_to_be_32((txq->wqe_ci << 8) | opcode);
	cs->sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	if (MLX5_TXOFF_CONFIG(TXPP) && __rte_trace_point_fp_is_enabled())
		cs->flags = RTE_BE32(MLX5_COMP_ALWAYS <<
				     MLX5_COMP_MODE_OFFSET);
	else
		cs->flags = RTE_BE32(MLX5_COMP_ONLY_FIRST_ERR <<
				     MLX5_COMP_MODE_OFFSET);
	cs->misc = RTE_BE32(0);
	if (__rte_trace_point_fp_is_enabled() && !loc->pkts_sent)
		rte_pmd_mlx5_trace_tx_entry(txq->port_id, txq->idx);
	rte_pmd_mlx5_trace_tx_wqe((txq->wqe_ci << 8) | opcode);
}

/**
 * Build the Synchronize Queue Segment with specified completion index.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Control Segment.
 * @param wci
 *   Completion index in Clock Queue to wait.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_qseg_init(struct mlx5_txq_data *restrict txq,
		  struct mlx5_txq_local *restrict loc __rte_unused,
		  struct mlx5_wqe *restrict wqe,
		  unsigned int wci,
		  unsigned int olx __rte_unused)
{
	struct mlx5_wqe_qseg *qs;

	qs = RTE_PTR_ADD(wqe, MLX5_WSEG_SIZE);
	qs->max_index = rte_cpu_to_be_32(wci);
	qs->qpn_cqn = rte_cpu_to_be_32(txq->sh->txpp.clock_queue.cq_obj.cq->id);
	qs->reserved0 = RTE_BE32(0);
	qs->reserved1 = RTE_BE32(0);
}

/**
 * Build the Wait on Time Segment with specified timestamp value.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Control Segment.
 * @param ts
 *   Timesatmp value to wait.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_wseg_init(struct mlx5_txq_data *restrict txq,
		  struct mlx5_txq_local *restrict loc __rte_unused,
		  struct mlx5_wqe *restrict wqe,
		  uint64_t ts,
		  unsigned int olx __rte_unused)
{
	struct mlx5_wqe_wseg *ws;

	ws = RTE_PTR_ADD(wqe, MLX5_WSEG_SIZE);
	ws->operation = rte_cpu_to_be_32(MLX5_WAIT_COND_CYCLIC_SMALLER);
	ws->lkey = RTE_BE32(0);
	ws->va_high = RTE_BE32(0);
	ws->va_low = RTE_BE32(0);
	if (txq->rt_timestamp) {
		ts = ts % (uint64_t)NS_PER_S
		   | (ts / (uint64_t)NS_PER_S) << 32;
	}
	ws->value = rte_cpu_to_be_64(ts);
	ws->mask = txq->rt_timemask;
}

/**
 * Build the Ethernet Segment without inlined data.
 * Supports Software Parser, Checksums and VLAN insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_eseg_none(struct mlx5_txq_data *__rte_restrict txq __rte_unused,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;

	/*
	 * Calculate and set check sum flags first, dword field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	es->flags = rte_cpu_to_le_32(csum);
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & RTE_MBUF_DYNFLAG_TX_METADATA ?
		       rte_cpu_to_be_32(*RTE_FLOW_DYNF_METADATA(loc->mbuf)) :
		       0 : 0;
	/* Engage VLAN tag insertion feature if requested. */
	if (MLX5_TXOFF_CONFIG(VLAN) &&
	    loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN) {
		/*
		 * We should get here only if device support
		 * this feature correctly.
		 */
		MLX5_ASSERT(txq->vlan_en);
		es->inline_hdr = rte_cpu_to_be_32(MLX5_ETH_WQE_VLAN_INSERT |
						  loc->mbuf->vlan_tci);
	} else {
		es->inline_hdr = RTE_BE32(0);
	}
}

/**
 * Build the Ethernet Segment with minimal inlined data
 * of MLX5_ESEG_MIN_INLINE_SIZE bytes length. This is
 * used to fill the gap in single WQEBB WQEs.
 * Supports Software Parser, Checksums and VLAN
 * insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param vlan
 *   Length of VLAN tag insertion if any.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_eseg_dmin(struct mlx5_txq_data *__rte_restrict txq __rte_unused,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int vlan,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;
	uint8_t *psrc, *pdst;

	/*
	 * Calculate and set check sum flags first, dword field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	es->flags = rte_cpu_to_le_32(csum);
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & RTE_MBUF_DYNFLAG_TX_METADATA ?
		       rte_cpu_to_be_32(*RTE_FLOW_DYNF_METADATA(loc->mbuf)) :
		       0 : 0;
	psrc = rte_pktmbuf_mtod(loc->mbuf, uint8_t *);
	es->inline_hdr_sz = RTE_BE16(MLX5_ESEG_MIN_INLINE_SIZE);
	es->inline_data = *(unaligned_uint16_t *)psrc;
	psrc +=	sizeof(uint16_t);
	pdst = (uint8_t *)(es + 1);
	if (MLX5_TXOFF_CONFIG(VLAN) && vlan) {
		/* Implement VLAN tag insertion as part inline data. */
		memcpy(pdst, psrc, 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t));
		pdst += 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		psrc +=	2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		/* Insert VLAN ethertype + VLAN tag. */
		*(unaligned_uint32_t *)pdst = rte_cpu_to_be_32
						((RTE_ETHER_TYPE_VLAN << 16) |
						 loc->mbuf->vlan_tci);
		pdst += sizeof(struct rte_vlan_hdr);
		/* Copy the rest two bytes from packet data. */
		MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, sizeof(uint16_t)));
		*(uint16_t *)pdst = *(unaligned_uint16_t *)psrc;
	} else {
		/* Fill the gap in the title WQEBB with inline data. */
		rte_mov16(pdst, psrc);
	}
}

/**
 * Build the Ethernet Segment with entire packet data inlining. Checks the
 * boundary of WQEBB and ring buffer wrapping, supports Software Parser,
 * Checksums and VLAN insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param vlan
 *   Length of VLAN tag insertion if any.
 * @param inlen
 *   Length of data to inline (VLAN included, if any).
 * @param tso
 *   TSO flag, set mss field from the packet.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment (aligned and wrapped around).
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_eseg_data(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int vlan,
		  unsigned int inlen,
		  unsigned int tso,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;
	uint8_t *psrc, *pdst;
	unsigned int part;

	/*
	 * Calculate and set check sum flags first, dword field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	if (tso) {
		csum <<= 24;
		csum |= loc->mbuf->tso_segsz;
		es->flags = rte_cpu_to_be_32(csum);
	} else {
		es->flags = rte_cpu_to_le_32(csum);
	}
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & RTE_MBUF_DYNFLAG_TX_METADATA ?
		       rte_cpu_to_be_32(*RTE_FLOW_DYNF_METADATA(loc->mbuf)) :
		       0 : 0;
	psrc = rte_pktmbuf_mtod(loc->mbuf, uint8_t *);
	es->inline_hdr_sz = rte_cpu_to_be_16(inlen);
	es->inline_data = *(unaligned_uint16_t *)psrc;
	psrc +=	sizeof(uint16_t);
	pdst = (uint8_t *)(es + 1);
	if (MLX5_TXOFF_CONFIG(VLAN) && vlan) {
		/* Implement VLAN tag insertion as part inline data. */
		memcpy(pdst, psrc, 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t));
		pdst += 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		psrc +=	2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		/* Insert VLAN ethertype + VLAN tag. */
		*(unaligned_uint32_t *)pdst = rte_cpu_to_be_32
						((RTE_ETHER_TYPE_VLAN << 16) |
						 loc->mbuf->vlan_tci);
		pdst += sizeof(struct rte_vlan_hdr);
		/* Copy the rest two bytes from packet data. */
		MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, sizeof(uint16_t)));
		*(uint16_t *)pdst = *(unaligned_uint16_t *)psrc;
		psrc += sizeof(uint16_t);
	} else {
		/* Fill the gap in the title WQEBB with inline data. */
		rte_mov16(pdst, psrc);
		psrc += sizeof(rte_v128u32_t);
	}
	pdst = (uint8_t *)(es + 2);
	MLX5_ASSERT(inlen >= MLX5_ESEG_MIN_INLINE_SIZE);
	MLX5_ASSERT(pdst < (uint8_t *)txq->wqes_end);
	inlen -= MLX5_ESEG_MIN_INLINE_SIZE;
	if (!inlen) {
		MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE));
		return (struct mlx5_wqe_dseg *)pdst;
	}
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, inlen);
	do {
		rte_memcpy(pdst, psrc, part);
		inlen -= part;
		if (likely(!inlen)) {
			/*
			 * If return value is not used by the caller
			 * the code below will be optimized out.
			 */
			pdst += part;
			pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			if (unlikely(pdst >= (uint8_t *)txq->wqes_end))
				pdst = (uint8_t *)txq->wqes;
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		psrc += part;
		part = inlen;
	} while (true);
}

/**
 * Copy data from chain of mbuf to the specified linear buffer.
 * Checksums and VLAN insertion Tx offload features. If data
 * from some mbuf copied completely this mbuf is freed. Local
 * structure is used to keep the byte stream state.
 *
 * @param pdst
 *   Pointer to the destination linear buffer.
 * @param loc
 *   Pointer to burst routine local context.
 * @param len
 *   Length of data to be copied.
 * @param must
 *   Length of data to be copied ignoring no inline hint.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Number of actual copied data bytes. This is always greater than or
 *   equal to must parameter and might be lesser than len in no inline
 *   hint flag is encountered.
 */
static __rte_always_inline unsigned int
mlx5_tx_mseg_memcpy(uint8_t *pdst,
		    struct mlx5_txq_local *__rte_restrict loc,
		    unsigned int len,
		    unsigned int must,
		    unsigned int olx __rte_unused)
{
	struct rte_mbuf *mbuf;
	unsigned int part, dlen, copy = 0;
	uint8_t *psrc;

	MLX5_ASSERT(len);
	do {
		/* Allow zero length packets, must check first. */
		dlen = rte_pktmbuf_data_len(loc->mbuf);
		if (dlen <= loc->mbuf_off) {
			/* Exhausted packet, just free. */
			mbuf = loc->mbuf;
			loc->mbuf = mbuf->next;
			rte_pktmbuf_free_seg(mbuf);
			loc->mbuf_off = 0;
			MLX5_ASSERT(loc->mbuf_nseg > 1);
			MLX5_ASSERT(loc->mbuf);
			--loc->mbuf_nseg;
			if (loc->mbuf->ol_flags & RTE_MBUF_F_TX_DYNF_NOINLINE) {
				unsigned int diff;

				if (copy >= must) {
					/*
					 * We already copied the minimal
					 * requested amount of data.
					 */
					return copy;
				}
				diff = must - copy;
				if (diff <= rte_pktmbuf_data_len(loc->mbuf)) {
					/*
					 * Copy only the minimal required
					 * part of the data buffer. Limit amount
					 * of data to be copied to the length of
					 * available space.
					 */
					len = RTE_MIN(len, diff);
				}
			}
			continue;
		}
		dlen -= loc->mbuf_off;
		psrc = rte_pktmbuf_mtod_offset(loc->mbuf, uint8_t *,
					       loc->mbuf_off);
		part = RTE_MIN(len, dlen);
		rte_memcpy(pdst, psrc, part);
		copy += part;
		loc->mbuf_off += part;
		len -= part;
		if (!len) {
			if (loc->mbuf_off >= rte_pktmbuf_data_len(loc->mbuf)) {
				loc->mbuf_off = 0;
				/* Exhausted packet, just free. */
				mbuf = loc->mbuf;
				loc->mbuf = mbuf->next;
				rte_pktmbuf_free_seg(mbuf);
				loc->mbuf_off = 0;
				MLX5_ASSERT(loc->mbuf_nseg >= 1);
				--loc->mbuf_nseg;
			}
			return copy;
		}
		pdst += part;
	} while (true);
}

/**
 * Build the Ethernet Segment with inlined data from multi-segment packet.
 * Checks the boundary of WQEBB and ring buffer wrapping, supports Software
 * Parser, Checksums and VLAN insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param vlan
 *   Length of VLAN tag insertion if any.
 * @param inlen
 *   Length of data to inline (VLAN included, if any).
 * @param tso
 *   TSO flag, set mss field from the packet.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment (aligned and possible NOT wrapped
 *   around - caller should do wrapping check on its own).
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_eseg_mdat(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int vlan,
		  unsigned int inlen,
		  unsigned int tso,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;
	uint8_t *pdst;
	unsigned int part, tlen = 0;

	/*
	 * Calculate and set check sum flags first, uint32_t field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	if (tso) {
		csum <<= 24;
		csum |= loc->mbuf->tso_segsz;
		es->flags = rte_cpu_to_be_32(csum);
	} else {
		es->flags = rte_cpu_to_le_32(csum);
	}
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & RTE_MBUF_DYNFLAG_TX_METADATA ?
		       rte_cpu_to_be_32(*RTE_FLOW_DYNF_METADATA(loc->mbuf)) :
		       0 : 0;
	MLX5_ASSERT(inlen >= MLX5_ESEG_MIN_INLINE_SIZE);
	pdst = (uint8_t *)&es->inline_data;
	if (MLX5_TXOFF_CONFIG(VLAN) && vlan) {
		/* Implement VLAN tag insertion as part inline data. */
		mlx5_tx_mseg_memcpy(pdst, loc,
				    2 * RTE_ETHER_ADDR_LEN,
				    2 * RTE_ETHER_ADDR_LEN, olx);
		pdst += 2 * RTE_ETHER_ADDR_LEN;
		*(unaligned_uint32_t *)pdst = rte_cpu_to_be_32
						((RTE_ETHER_TYPE_VLAN << 16) |
						 loc->mbuf->vlan_tci);
		pdst += sizeof(struct rte_vlan_hdr);
		tlen += 2 * RTE_ETHER_ADDR_LEN + sizeof(struct rte_vlan_hdr);
	}
	MLX5_ASSERT(pdst < (uint8_t *)txq->wqes_end);
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, inlen - tlen);
	MLX5_ASSERT(part);
	do {
		unsigned int copy;

		/*
		 * Copying may be interrupted inside the routine
		 * if run into no inline hint flag.
		 */
		copy = tso ? inlen : txq->inlen_mode;
		copy = tlen >= copy ? 0 : (copy - tlen);
		copy = mlx5_tx_mseg_memcpy(pdst, loc, part, copy, olx);
		tlen += copy;
		if (likely(inlen <= tlen) || copy < part) {
			es->inline_hdr_sz = rte_cpu_to_be_16(tlen);
			pdst += copy;
			pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		part = inlen - tlen;
	} while (true);
}

/**
 * Build the Data Segment of pointer type.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to WQE to fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_dseg_ptr(struct mlx5_txq_data *__rte_restrict txq,
		 struct mlx5_txq_local *__rte_restrict loc,
		 struct mlx5_wqe_dseg *__rte_restrict dseg,
		 uint8_t *buf,
		 unsigned int len,
		 unsigned int olx __rte_unused)

{
	MLX5_ASSERT(len);
	dseg->bcount = rte_cpu_to_be_32(len);
	dseg->lkey = mlx5_mr_mb2mr(&txq->mr_ctrl, loc->mbuf);
	dseg->pbuf = rte_cpu_to_be_64((uintptr_t)buf);
}

/**
 * Build the Data Segment of pointer type or inline if data length is less than
 * buffer in minimal Data Segment size.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to WQE to fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_dseg_iptr(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe_dseg *__rte_restrict dseg,
		  uint8_t *buf,
		  unsigned int len,
		  unsigned int olx __rte_unused)

{
	uintptr_t dst, src;

	MLX5_ASSERT(len);
	if (len > MLX5_DSEG_MIN_INLINE_SIZE) {
		dseg->bcount = rte_cpu_to_be_32(len);
		dseg->lkey = mlx5_mr_mb2mr(&txq->mr_ctrl, loc->mbuf);
		dseg->pbuf = rte_cpu_to_be_64((uintptr_t)buf);

		return;
	}
	dseg->bcount = rte_cpu_to_be_32(len | MLX5_ETH_WQE_DATA_INLINE);
	/* Unrolled implementation of generic rte_memcpy. */
	dst = (uintptr_t)&dseg->inline_data[0];
	src = (uintptr_t)buf;
	if (len & 0x08) {
#ifdef RTE_ARCH_STRICT_ALIGN
		MLX5_ASSERT(dst == RTE_PTR_ALIGN(dst, sizeof(uint32_t)));
		*(uint32_t *)dst = *(unaligned_uint32_t *)src;
		dst += sizeof(uint32_t);
		src += sizeof(uint32_t);
		*(uint32_t *)dst = *(unaligned_uint32_t *)src;
		dst += sizeof(uint32_t);
		src += sizeof(uint32_t);
#else
		*(uint64_t *)dst = *(unaligned_uint64_t *)src;
		dst += sizeof(uint64_t);
		src += sizeof(uint64_t);
#endif
	}
	if (len & 0x04) {
		*(uint32_t *)dst = *(unaligned_uint32_t *)src;
		dst += sizeof(uint32_t);
		src += sizeof(uint32_t);
	}
	if (len & 0x02) {
		*(uint16_t *)dst = *(unaligned_uint16_t *)src;
		dst += sizeof(uint16_t);
		src += sizeof(uint16_t);
	}
	if (len & 0x01)
		*(uint8_t *)dst = *(uint8_t *)src;
}

/**
 * Build the Data Segment of inlined data from single
 * segment packet, no VLAN insertion.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to WQE to fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment after inlined data.
 *   Ring buffer wraparound check is needed. We do not do it here because it
 *   may not be needed for the last packet in the eMPW session.
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_dseg_empw(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc __rte_unused,
		  struct mlx5_wqe_dseg *__rte_restrict dseg,
		  uint8_t *buf,
		  unsigned int len,
		  unsigned int olx __rte_unused)
{
	unsigned int part;
	uint8_t *pdst;

	if (!MLX5_TXOFF_CONFIG(MPW)) {
		/* Store the descriptor byte counter for eMPW sessions. */
		dseg->bcount = rte_cpu_to_be_32(len | MLX5_ETH_WQE_DATA_INLINE);
		pdst = &dseg->inline_data[0];
	} else {
		/* The entire legacy MPW session counter is stored on close. */
		pdst = (uint8_t *)dseg;
	}
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, len);
	do {
		rte_memcpy(pdst, buf, part);
		len -= part;
		if (likely(!len)) {
			pdst += part;
			if (!MLX5_TXOFF_CONFIG(MPW))
				pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			/* Note: no final wraparound check here. */
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		buf += part;
		part = len;
	} while (true);
}

/**
 * Build the Data Segment of inlined data from single
 * segment packet with VLAN insertion.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to the dseg fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment after inlined data.
 *   Ring buffer wraparound check is needed.
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_dseg_vlan(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc __rte_unused,
		  struct mlx5_wqe_dseg *__rte_restrict dseg,
		  uint8_t *buf,
		  unsigned int len,
		  unsigned int olx __rte_unused)

{
	unsigned int part;
	uint8_t *pdst;

	MLX5_ASSERT(len > MLX5_ESEG_MIN_INLINE_SIZE);
	if (!MLX5_TXOFF_CONFIG(MPW)) {
		/* Store the descriptor byte counter for eMPW sessions. */
		dseg->bcount = rte_cpu_to_be_32
				((len + sizeof(struct rte_vlan_hdr)) |
				 MLX5_ETH_WQE_DATA_INLINE);
		pdst = &dseg->inline_data[0];
	} else {
		/* The entire legacy MPW session counter is stored on close. */
		pdst = (uint8_t *)dseg;
	}
	memcpy(pdst, buf, MLX5_DSEG_MIN_INLINE_SIZE);
	buf += MLX5_DSEG_MIN_INLINE_SIZE;
	pdst += MLX5_DSEG_MIN_INLINE_SIZE;
	len -= MLX5_DSEG_MIN_INLINE_SIZE;
	/* Insert VLAN ethertype + VLAN tag. Pointer is aligned. */
	MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE));
	if (unlikely(pdst >= (uint8_t *)txq->wqes_end))
		pdst = (uint8_t *)txq->wqes;
	*(uint32_t *)pdst = rte_cpu_to_be_32((RTE_ETHER_TYPE_VLAN << 16) |
					      loc->mbuf->vlan_tci);
	pdst += sizeof(struct rte_vlan_hdr);
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, len);
	do {
		rte_memcpy(pdst, buf, part);
		len -= part;
		if (likely(!len)) {
			pdst += part;
			if (!MLX5_TXOFF_CONFIG(MPW))
				pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			/* Note: no final wraparound check here. */
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		buf += part;
		part = len;
	} while (true);
}

/**
 * Build the Ethernet Segment with optionally inlined data with
 * VLAN insertion and following Data Segments (if any) from
 * multi-segment packet. Used by ordinary send and TSO.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet/Data Segments.
 * @param vlan
 *   Length of VLAN header to insert, 0 means no VLAN insertion.
 * @param inlen
 *   Data length to inline. For TSO this parameter specifies exact value,
 *   for ordinary send routine can be aligned by caller to provide better WQE
 *   space saving and data buffer start address alignment.
 *   This length includes VLAN header being inserted.
 * @param tso
 *   Zero means ordinary send, inlined data can be extended,
 *   otherwise this is TSO, inlined data length is fixed.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Actual size of built WQE in segments.
 */
static __rte_always_inline unsigned int
mlx5_tx_mseg_build(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_txq_local *__rte_restrict loc,
		   struct mlx5_wqe *__rte_restrict wqe,
		   unsigned int vlan,
		   unsigned int inlen,
		   unsigned int tso,
		   unsigned int olx __rte_unused)
{
	struct mlx5_wqe_dseg *__rte_restrict dseg;
	unsigned int ds;

	MLX5_ASSERT((rte_pktmbuf_pkt_len(loc->mbuf) + vlan) >= inlen);
	loc->mbuf_nseg = NB_SEGS(loc->mbuf);
	loc->mbuf_off = 0;

	dseg = mlx5_tx_eseg_mdat(txq, loc, wqe, vlan, inlen, tso, olx);
	if (!loc->mbuf_nseg)
		goto dseg_done;
	/*
	 * There are still some mbuf remaining, not inlined.
	 * The first mbuf may be partially inlined and we
	 * must process the possible non-zero data offset.
	 */
	if (loc->mbuf_off) {
		unsigned int dlen;
		uint8_t *dptr;

		/*
		 * Exhausted packets must be dropped before.
		 * Non-zero offset means there are some data
		 * remained in the packet.
		 */
		MLX5_ASSERT(loc->mbuf_off < rte_pktmbuf_data_len(loc->mbuf));
		MLX5_ASSERT(rte_pktmbuf_data_len(loc->mbuf));
		dptr = rte_pktmbuf_mtod_offset(loc->mbuf, uint8_t *,
					       loc->mbuf_off);
		dlen = rte_pktmbuf_data_len(loc->mbuf) - loc->mbuf_off;
		/*
		 * Build the pointer/minimal Data Segment.
		 * Do ring buffer wrapping check in advance.
		 */
		if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
			dseg = (struct mlx5_wqe_dseg *)txq->wqes;
		mlx5_tx_dseg_iptr(txq, loc, dseg, dptr, dlen, olx);
		/* Store the mbuf to be freed on completion. */
		MLX5_ASSERT(loc->elts_free);
		txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
		--loc->elts_free;
		++dseg;
		if (--loc->mbuf_nseg == 0)
			goto dseg_done;
		loc->mbuf = loc->mbuf->next;
		loc->mbuf_off = 0;
	}
	do {
		if (unlikely(!rte_pktmbuf_data_len(loc->mbuf))) {
			struct rte_mbuf *mbuf;

			/* Zero length segment found, just skip. */
			mbuf = loc->mbuf;
			loc->mbuf = loc->mbuf->next;
			rte_pktmbuf_free_seg(mbuf);
			if (--loc->mbuf_nseg == 0)
				break;
		} else {
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
			mlx5_tx_dseg_iptr
				(txq, loc, dseg,
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 rte_pktmbuf_data_len(loc->mbuf), olx);
			MLX5_ASSERT(loc->elts_free);
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
			--loc->elts_free;
			++dseg;
			if (--loc->mbuf_nseg == 0)
				break;
			loc->mbuf = loc->mbuf->next;
		}
	} while (true);

dseg_done:
	/* Calculate actual segments used from the dseg pointer. */
	if ((uintptr_t)wqe < (uintptr_t)dseg)
		ds = ((uintptr_t)dseg - (uintptr_t)wqe) / MLX5_WSEG_SIZE;
	else
		ds = (((uintptr_t)dseg - (uintptr_t)wqe) +
		      txq->wqe_s * MLX5_WQE_SIZE) / MLX5_WSEG_SIZE;
	return ds;
}

/**
 * The routine checks timestamp flag in the current packet,
 * and push WAIT WQE into the queue if scheduling is required.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param elts
 *   Number of free elements in elts buffer to be checked, for zero
 *   value the check is optimized out by compiler.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_SINGLE - continue processing with the packet.
 *   MLX5_TXCMP_CODE_MULTI - the WAIT inserted, continue processing.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_schedule_send(struct mlx5_txq_data *restrict txq,
		      struct mlx5_txq_local *restrict loc,
		      uint16_t elts,
		      unsigned int olx)
{
	if (MLX5_TXOFF_CONFIG(TXPP) &&
	    loc->mbuf->ol_flags & txq->ts_mask) {
		struct mlx5_dev_ctx_shared *sh;
		struct mlx5_wqe *wqe;
		uint64_t ts;

		/*
		 * Estimate the required space quickly and roughly.
		 * We would like to ensure the packet can be pushed
		 * to the queue and we won't get the orphan WAIT WQE.
		 */
		if (loc->wqe_free <= MLX5_WQE_SIZE_MAX / MLX5_WQE_SIZE ||
		    loc->elts_free < elts)
			return MLX5_TXCMP_CODE_EXIT;
		/* Convert the timestamp into completion to wait. */
		ts = *RTE_MBUF_DYNFIELD(loc->mbuf, txq->ts_offset, uint64_t *);
		if (txq->ts_last && ts < txq->ts_last)
			__atomic_fetch_add(&txq->sh->txpp.err_ts_order,
					   1, __ATOMIC_RELAXED);
		txq->ts_last = ts;
		wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		sh = txq->sh;
		if (txq->wait_on_time) {
			/* The wait on time capability should be used. */
			ts -= sh->txpp.skew;
			rte_pmd_mlx5_trace_tx_wait(ts);
			mlx5_tx_cseg_init(txq, loc, wqe,
					  1 + sizeof(struct mlx5_wqe_wseg) /
					      MLX5_WSEG_SIZE,
					  MLX5_OPCODE_WAIT |
					  MLX5_OPC_MOD_WAIT_TIME << 24, olx);
			mlx5_tx_wseg_init(txq, loc, wqe, ts, olx);
		} else {
			/* Legacy cross-channel operation should be used. */
			int32_t wci;

			wci = mlx5_txpp_convert_tx_ts(sh, ts);
			if (unlikely(wci < 0))
				return MLX5_TXCMP_CODE_SINGLE;
			/* Build the WAIT WQE with specified completion. */
			rte_pmd_mlx5_trace_tx_wait(ts - sh->txpp.skew);
			mlx5_tx_cseg_init(txq, loc, wqe,
					  1 + sizeof(struct mlx5_wqe_qseg) /
					      MLX5_WSEG_SIZE,
					  MLX5_OPCODE_WAIT |
					  MLX5_OPC_MOD_WAIT_CQ_PI << 24, olx);
			mlx5_tx_qseg_init(txq, loc, wqe, wci, olx);
		}
		++txq->wqe_ci;
		--loc->wqe_free;
		return MLX5_TXCMP_CODE_MULTI;
	}
	return MLX5_TXCMP_CODE_SINGLE;
}

/**
 * Tx one packet function for multi-segment TSO. Supports all
 * types of Tx offloads, uses MLX5_OPCODE_TSO to build WQEs,
 * sends one packet per WQE.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_packet_multi_tso(struct mlx5_txq_data *__rte_restrict txq,
			struct mlx5_txq_local *__rte_restrict loc,
			unsigned int olx)
{
	struct mlx5_wqe *__rte_restrict wqe;
	unsigned int ds, dlen, inlen, ntcp, vlan = 0;

	MLX5_ASSERT(loc->elts_free >= NB_SEGS(loc->mbuf));
	if (MLX5_TXOFF_CONFIG(TXPP)) {
		enum mlx5_txcmp_code wret;

		/* Generate WAIT for scheduling if requested. */
		wret = mlx5_tx_schedule_send(txq, loc, 0, olx);
		if (wret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (wret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
	}
	/*
	 * Calculate data length to be inlined to estimate
	 * the required space in WQE ring buffer.
	 */
	dlen = rte_pktmbuf_pkt_len(loc->mbuf);
	if (MLX5_TXOFF_CONFIG(VLAN) && loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN)
		vlan = sizeof(struct rte_vlan_hdr);
	inlen = loc->mbuf->l2_len + vlan +
		loc->mbuf->l3_len + loc->mbuf->l4_len;
	if (unlikely((!inlen || !loc->mbuf->tso_segsz)))
		return MLX5_TXCMP_CODE_ERROR;
	if (loc->mbuf->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
		inlen += loc->mbuf->outer_l2_len + loc->mbuf->outer_l3_len;
	/* Packet must contain all TSO headers. */
	if (unlikely(inlen > MLX5_MAX_TSO_HEADER ||
		     inlen <= MLX5_ESEG_MIN_INLINE_SIZE ||
		     inlen > (dlen + vlan)))
		return MLX5_TXCMP_CODE_ERROR;
	/*
	 * Check whether there are enough free WQEBBs:
	 * - Control Segment
	 * - Ethernet Segment
	 * - First Segment of inlined Ethernet data
	 * - ... data continued ...
	 * - Data Segments of pointer/min inline type
	 */
	ds = NB_SEGS(loc->mbuf) + 2 + (inlen -
				       MLX5_ESEG_MIN_INLINE_SIZE +
				       MLX5_WSEG_SIZE +
				       MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
	if (unlikely(loc->wqe_free < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_EXIT;
	/* Check for maximal WQE size. */
	if (unlikely((MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE) < ds))
		return MLX5_TXCMP_CODE_ERROR;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes/packets counters. */
	ntcp = (dlen - (inlen - vlan) + loc->mbuf->tso_segsz - 1) /
		loc->mbuf->tso_segsz;
	/*
	 * One will be added for mbuf itself at the end of the mlx5_tx_burst
	 * from loc->pkts_sent field.
	 */
	--ntcp;
	txq->stats.opackets += ntcp;
	txq->stats.obytes += dlen + vlan + ntcp * inlen;
#endif
	wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
	loc->wqe_last = wqe;
	mlx5_tx_cseg_init(txq, loc, wqe, 0, MLX5_OPCODE_TSO, olx);
	rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
	ds = mlx5_tx_mseg_build(txq, loc, wqe, vlan, inlen, 1, olx);
	wqe->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
	return MLX5_TXCMP_CODE_MULTI;
}

/**
 * Tx one packet function for multi-segment SEND. Supports all types of Tx
 * offloads, uses MLX5_OPCODE_SEND to build WQEs, sends one packet per WQE,
 * without any data inlining in Ethernet Segment.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_packet_multi_send(struct mlx5_txq_data *__rte_restrict txq,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	struct mlx5_wqe_dseg *__rte_restrict dseg;
	struct mlx5_wqe *__rte_restrict wqe;
	unsigned int ds, nseg;

	MLX5_ASSERT(NB_SEGS(loc->mbuf) > 1);
	MLX5_ASSERT(loc->elts_free >= NB_SEGS(loc->mbuf));
	if (MLX5_TXOFF_CONFIG(TXPP)) {
		enum mlx5_txcmp_code wret;

		/* Generate WAIT for scheduling if requested. */
		wret = mlx5_tx_schedule_send(txq, loc, 0, olx);
		if (wret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (wret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
	}
	/*
	 * No inline at all, it means the CPU cycles saving is prioritized at
	 * configuration, we should not copy any packet data to WQE.
	 */
	nseg = NB_SEGS(loc->mbuf);
	ds = 2 + nseg;
	if (unlikely(loc->wqe_free < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_EXIT;
	/* Check for maximal WQE size. */
	if (unlikely((MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE) < ds))
		return MLX5_TXCMP_CODE_ERROR;
	/*
	 * Some Tx offloads may cause an error if packet is not long enough,
	 * check against assumed minimal length.
	 */
	if (rte_pktmbuf_pkt_len(loc->mbuf) <= MLX5_ESEG_MIN_INLINE_SIZE)
		return MLX5_TXCMP_CODE_ERROR;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes counter. */
	txq->stats.obytes += rte_pktmbuf_pkt_len(loc->mbuf);
	if (MLX5_TXOFF_CONFIG(VLAN) &&
	    loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN)
		txq->stats.obytes += sizeof(struct rte_vlan_hdr);
#endif
	/*
	 * SEND WQE, one WQEBB:
	 * - Control Segment, SEND opcode
	 * - Ethernet Segment, optional VLAN, no inline
	 * - Data Segments, pointer only type
	 */
	wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
	loc->wqe_last = wqe;
	mlx5_tx_cseg_init(txq, loc, wqe, ds, MLX5_OPCODE_SEND, olx);
	rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
	mlx5_tx_eseg_none(txq, loc, wqe, olx);
	dseg = &wqe->dseg[0];
	do {
		if (unlikely(!rte_pktmbuf_data_len(loc->mbuf))) {
			struct rte_mbuf *mbuf;

			/*
			 * Zero length segment found, have to correct total
			 * size of WQE in segments.
			 * It is supposed to be rare occasion, so in normal
			 * case (no zero length segments) we avoid extra
			 * writing to the Control Segment.
			 */
			--ds;
			wqe->cseg.sq_ds -= RTE_BE32(1);
			mbuf = loc->mbuf;
			loc->mbuf = mbuf->next;
			rte_pktmbuf_free_seg(mbuf);
			if (--nseg == 0)
				break;
		} else {
			mlx5_tx_dseg_ptr
				(txq, loc, dseg,
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 rte_pktmbuf_data_len(loc->mbuf), olx);
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
			--loc->elts_free;
			if (--nseg == 0)
				break;
			++dseg;
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
			loc->mbuf = loc->mbuf->next;
		}
	} while (true);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
	return MLX5_TXCMP_CODE_MULTI;
}

/**
 * Tx one packet function for multi-segment SEND. Supports all
 * types of Tx offloads, uses MLX5_OPCODE_SEND to build WQEs,
 * sends one packet per WQE, with data inlining in
 * Ethernet Segment and minimal Data Segments.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_packet_multi_inline(struct mlx5_txq_data *__rte_restrict txq,
			    struct mlx5_txq_local *__rte_restrict loc,
			    unsigned int olx)
{
	struct mlx5_wqe *__rte_restrict wqe;
	unsigned int ds, inlen, dlen, vlan = 0;

	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
	MLX5_ASSERT(NB_SEGS(loc->mbuf) > 1);
	MLX5_ASSERT(loc->elts_free >= NB_SEGS(loc->mbuf));
	/*
	 * First calculate data length to be inlined
	 * to estimate the required space for WQE.
	 */
	dlen = rte_pktmbuf_pkt_len(loc->mbuf);
	if (MLX5_TXOFF_CONFIG(VLAN) && loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN)
		vlan = sizeof(struct rte_vlan_hdr);
	inlen = dlen + vlan;
	/* Check against minimal length. */
	if (inlen <= MLX5_ESEG_MIN_INLINE_SIZE)
		return MLX5_TXCMP_CODE_ERROR;
	MLX5_ASSERT(txq->inlen_send >= MLX5_ESEG_MIN_INLINE_SIZE);
	if (inlen > txq->inlen_send ||
	    loc->mbuf->ol_flags & RTE_MBUF_F_TX_DYNF_NOINLINE) {
		struct rte_mbuf *mbuf;
		unsigned int nxlen;
		uintptr_t start;

		mbuf = loc->mbuf;
		nxlen = rte_pktmbuf_data_len(mbuf);
		/*
		 * Packet length exceeds the allowed inline data length,
		 * check whether the minimal inlining is required.
		 */
		if (txq->inlen_mode) {
			MLX5_ASSERT(txq->inlen_mode >=
				    MLX5_ESEG_MIN_INLINE_SIZE);
			MLX5_ASSERT(txq->inlen_mode <= txq->inlen_send);
			inlen = RTE_MIN(txq->inlen_mode, inlen);
		} else if (vlan && !txq->vlan_en) {
			/*
			 * VLAN insertion is requested and hardware does not
			 * support the offload, will do with software inline.
			 */
			inlen = MLX5_ESEG_MIN_INLINE_SIZE;
		} else if (mbuf->ol_flags & RTE_MBUF_F_TX_DYNF_NOINLINE ||
			   nxlen > txq->inlen_send) {
			return mlx5_tx_packet_multi_send(txq, loc, olx);
		} else if (nxlen <= MLX5_ESEG_MIN_INLINE_SIZE) {
			inlen = MLX5_ESEG_MIN_INLINE_SIZE;
		} else {
			goto do_first;
		}
		if (mbuf->ol_flags & RTE_MBUF_F_TX_DYNF_NOINLINE)
			goto do_build;
		/*
		 * Now we know the minimal amount of data is requested
		 * to inline. Check whether we should inline the buffers
		 * from the chain beginning to eliminate some mbufs.
		 */
		if (unlikely(nxlen <= txq->inlen_send)) {
			/* We can inline first mbuf at least. */
			if (nxlen < inlen) {
				unsigned int smlen;

				/* Scan mbufs till inlen filled. */
				do {
					smlen = nxlen;
					mbuf = NEXT(mbuf);
					MLX5_ASSERT(mbuf);
					nxlen = rte_pktmbuf_data_len(mbuf);
					nxlen += smlen;
				} while (unlikely(nxlen < inlen));
				if (unlikely(nxlen > txq->inlen_send)) {
					/* We cannot inline entire mbuf. */
					smlen = inlen - smlen;
					start = rte_pktmbuf_mtod_offset
						    (mbuf, uintptr_t, smlen);
					goto do_align;
				}
			}
do_first:
			do {
				inlen = nxlen;
				mbuf = NEXT(mbuf);
				/* There should be not end of packet. */
				MLX5_ASSERT(mbuf);
				if (mbuf->ol_flags & RTE_MBUF_F_TX_DYNF_NOINLINE)
					break;
				nxlen = inlen + rte_pktmbuf_data_len(mbuf);
			} while (unlikely(nxlen < txq->inlen_send));
		}
		start = rte_pktmbuf_mtod(mbuf, uintptr_t);
		/*
		 * Check whether we can do inline to align start
		 * address of data buffer to cacheline.
		 */
do_align:
		start = (~start + 1) & (RTE_CACHE_LINE_SIZE - 1);
		if (unlikely(start)) {
			start += inlen;
			if (start <= txq->inlen_send)
				inlen = start;
		}
	}
	/*
	 * Check whether there are enough free WQEBBs:
	 * - Control Segment
	 * - Ethernet Segment
	 * - First Segment of inlined Ethernet data
	 * - ... data continued ...
	 * - Data Segments of pointer/min inline type
	 *
	 * Estimate the number of Data Segments conservatively,
	 * supposing no any mbufs is being freed during inlining.
	 */
do_build:
	if (MLX5_TXOFF_CONFIG(TXPP)) {
		enum mlx5_txcmp_code wret;

		/* Generate WAIT for scheduling if requested. */
		wret = mlx5_tx_schedule_send(txq, loc, 0, olx);
		if (wret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (wret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
	}
	MLX5_ASSERT(inlen <= txq->inlen_send);
	ds = NB_SEGS(loc->mbuf) + 2 + (inlen -
				       MLX5_ESEG_MIN_INLINE_SIZE +
				       MLX5_WSEG_SIZE +
				       MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
	if (unlikely(loc->wqe_free < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_EXIT;
	/* Check for maximal WQE size. */
	if (unlikely((MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE) < ds)) {
		/*  Check if we can adjust the inline length. */
		if (unlikely(txq->inlen_mode)) {
			ds = NB_SEGS(loc->mbuf) + 2 +
				(txq->inlen_mode -
				MLX5_ESEG_MIN_INLINE_SIZE +
				MLX5_WSEG_SIZE +
				MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
			if (unlikely((MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE) < ds))
				return MLX5_TXCMP_CODE_ERROR;
		}
		/* We have lucky opportunity to adjust. */
		inlen = RTE_MIN(inlen, MLX5_WQE_SIZE_MAX -
				       MLX5_WSEG_SIZE * 2 -
				       MLX5_WSEG_SIZE * NB_SEGS(loc->mbuf) -
				       MLX5_WSEG_SIZE +
				       MLX5_ESEG_MIN_INLINE_SIZE);
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes/packets counters. */
	txq->stats.obytes += dlen + vlan;
#endif
	wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
	loc->wqe_last = wqe;
	mlx5_tx_cseg_init(txq, loc, wqe, 0, MLX5_OPCODE_SEND, olx);
	rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
	ds = mlx5_tx_mseg_build(txq, loc, wqe, vlan, inlen, 0, olx);
	wqe->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
	return MLX5_TXCMP_CODE_MULTI;
}

/**
 * Tx burst function for multi-segment packets. Supports all
 * types of Tx offloads, uses MLX5_OPCODE_SEND/TSO to build WQEs,
 * sends one packet per WQE. Function stops sending if it
 * encounters the single-segment packet.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 *   MLX5_TXCMP_CODE_SINGLE - single-segment packet encountered.
 *   MLX5_TXCMP_CODE_TSO - TSO single-segment packet encountered.
 * Local context variables updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_mseg(struct mlx5_txq_data *__rte_restrict txq,
		   struct rte_mbuf **__rte_restrict pkts,
		   unsigned int pkts_n,
		   struct mlx5_txq_local *__rte_restrict loc,
		   unsigned int olx)
{
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		enum mlx5_txcmp_code ret;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) > 1);
		/*
		 * Estimate the number of free elts quickly but conservatively.
		 * Some segment may be fully inlined and freed,
		 * ignore this here - precise estimation is costly.
		 */
		if (loc->elts_free < NB_SEGS(loc->mbuf))
			return MLX5_TXCMP_CODE_EXIT;
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    unlikely(loc->mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			/* Proceed with multi-segment TSO. */
			ret = mlx5_tx_packet_multi_tso(txq, loc, olx);
		} else if (MLX5_TXOFF_CONFIG(INLINE)) {
			/* Proceed with multi-segment SEND with inlining. */
			ret = mlx5_tx_packet_multi_inline(txq, loc, olx);
		} else {
			/* Proceed with multi-segment SEND w/o inlining. */
			ret = mlx5_tx_packet_multi_send(txq, loc, olx);
		}
		if (ret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (ret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
		/* WQE is built, go to the next packet. */
		++loc->pkts_sent;
		--pkts_n;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		if (pkts_n > 1)
			rte_prefetch0(*pkts);
		if (likely(NB_SEGS(loc->mbuf) > 1))
			continue;
		/* Here ends the series of multi-segment packets. */
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    unlikely(loc->mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG))
			return MLX5_TXCMP_CODE_TSO;
		return MLX5_TXCMP_CODE_SINGLE;
	}
	MLX5_ASSERT(false);
}

/**
 * Tx burst function for single-segment packets with TSO.
 * Supports all types of Tx offloads, except multi-packets.
 * Uses MLX5_OPCODE_TSO to build WQEs, sends one packet per WQE.
 * Function stops sending if it encounters the multi-segment
 * packet or packet without TSO requested.
 *
 * The routine is responsible for storing processed mbuf into elts ring buffer
 * and update elts_head if inline offloads is requested due to possible early
 * freeing of the inlined mbufs (can not store pkts array in elts as a batch).
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 *   MLX5_TXCMP_CODE_SINGLE - single-segment packet encountered.
 *   MLX5_TXCMP_CODE_MULTI - multi-segment packet encountered.
 * Local context variables updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_tso(struct mlx5_txq_data *__rte_restrict txq,
		  struct rte_mbuf **__rte_restrict pkts,
		  unsigned int pkts_n,
		  struct mlx5_txq_local *__rte_restrict loc,
		  unsigned int olx)
{
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe_dseg *__rte_restrict dseg;
		struct mlx5_wqe *__rte_restrict wqe;
		unsigned int ds, dlen, hlen, ntcp, vlan = 0;
		uint8_t *dptr;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, 1, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		dlen = rte_pktmbuf_data_len(loc->mbuf);
		if (MLX5_TXOFF_CONFIG(VLAN) &&
		    loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN) {
			vlan = sizeof(struct rte_vlan_hdr);
		}
		/*
		 * First calculate the WQE size to check
		 * whether we have enough space in ring buffer.
		 */
		hlen = loc->mbuf->l2_len + vlan +
		       loc->mbuf->l3_len + loc->mbuf->l4_len;
		if (unlikely((!hlen || !loc->mbuf->tso_segsz)))
			return MLX5_TXCMP_CODE_ERROR;
		if (loc->mbuf->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
			hlen += loc->mbuf->outer_l2_len +
				loc->mbuf->outer_l3_len;
		/* Segment must contain all TSO headers. */
		if (unlikely(hlen > MLX5_MAX_TSO_HEADER ||
			     hlen <= MLX5_ESEG_MIN_INLINE_SIZE ||
			     hlen > (dlen + vlan)))
			return MLX5_TXCMP_CODE_ERROR;
		/*
		 * Check whether there are enough free WQEBBs:
		 * - Control Segment
		 * - Ethernet Segment
		 * - First Segment of inlined Ethernet data
		 * - ... data continued ...
		 * - Finishing Data Segment of pointer type
		 */
		ds = 4 + (hlen - MLX5_ESEG_MIN_INLINE_SIZE +
			  MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
		if (loc->wqe_free < ((ds + 3) / 4))
			return MLX5_TXCMP_CODE_EXIT;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Update sent data bytes/packets counters. */
		ntcp = (dlen + vlan - hlen +
			loc->mbuf->tso_segsz - 1) /
			loc->mbuf->tso_segsz;
		/*
		 * One will be added for mbuf itself at the end
		 * of the mlx5_tx_burst from loc->pkts_sent field.
		 */
		--ntcp;
		txq->stats.opackets += ntcp;
		txq->stats.obytes += dlen + vlan + ntcp * hlen;
#endif
		/*
		 * Build the TSO WQE:
		 * - Control Segment
		 * - Ethernet Segment with hlen bytes inlined
		 * - Data Segment of pointer type
		 */
		wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		loc->wqe_last = wqe;
		mlx5_tx_cseg_init(txq, loc, wqe, ds, MLX5_OPCODE_TSO, olx);
		rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
		dseg = mlx5_tx_eseg_data(txq, loc, wqe, vlan, hlen, 1, olx);
		dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *) + hlen - vlan;
		dlen -= hlen - vlan;
		mlx5_tx_dseg_ptr(txq, loc, dseg, dptr, dlen, olx);
		/*
		 * WQE is built, update the loop parameters
		 * and go to the next packet.
		 */
		txq->wqe_ci += (ds + 3) / 4;
		loc->wqe_free -= (ds + 3) / 4;
		if (MLX5_TXOFF_CONFIG(INLINE))
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
		--loc->elts_free;
		++loc->pkts_sent;
		--pkts_n;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		if (pkts_n > 1)
			rte_prefetch0(*pkts);
		if (MLX5_TXOFF_CONFIG(MULTI) &&
		    unlikely(NB_SEGS(loc->mbuf) > 1))
			return MLX5_TXCMP_CODE_MULTI;
		if (likely(!(loc->mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG)))
			return MLX5_TXCMP_CODE_SINGLE;
		/* Continue with the next TSO packet. */
	}
	MLX5_ASSERT(false);
}

/**
 * Analyze the packet and select the best method to send.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 * @param newp
 *   The predefined flag whether do complete check for
 *   multi-segment packets and TSO.
 *
 * @return
 *  MLX5_TXCMP_CODE_MULTI - multi-segment packet encountered.
 *  MLX5_TXCMP_CODE_TSO - TSO required, use TSO/LSO.
 *  MLX5_TXCMP_CODE_SINGLE - single-segment packet, use SEND.
 *  MLX5_TXCMP_CODE_EMPW - single-segment packet, use MPW.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_able_to_empw(struct mlx5_txq_data *__rte_restrict txq,
		     struct mlx5_txq_local *__rte_restrict loc,
		     unsigned int olx,
		     bool newp)
{
	/* Check for multi-segment packet. */
	if (newp &&
	    MLX5_TXOFF_CONFIG(MULTI) &&
	    unlikely(NB_SEGS(loc->mbuf) > 1))
		return MLX5_TXCMP_CODE_MULTI;
	/* Check for TSO packet. */
	if (newp &&
	    MLX5_TXOFF_CONFIG(TSO) &&
	    unlikely(loc->mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG))
		return MLX5_TXCMP_CODE_TSO;
	/* Check if eMPW is enabled at all. */
	if (!MLX5_TXOFF_CONFIG(EMPW))
		return MLX5_TXCMP_CODE_SINGLE;
	/* Check if eMPW can be engaged. */
	if (MLX5_TXOFF_CONFIG(VLAN) &&
	    unlikely(loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN) &&
		(!MLX5_TXOFF_CONFIG(INLINE) ||
		 unlikely((rte_pktmbuf_data_len(loc->mbuf) +
			   sizeof(struct rte_vlan_hdr)) > txq->inlen_empw))) {
		/*
		 * eMPW does not support VLAN insertion offload, we have to
		 * inline the entire packet but packet is too long for inlining.
		 */
		return MLX5_TXCMP_CODE_SINGLE;
	}
	return MLX5_TXCMP_CODE_EMPW;
}

/**
 * Check the next packet attributes to match with the eMPW batch ones.
 * In addition, for legacy MPW the packet length is checked either.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param es
 *   Pointer to Ethernet Segment of eMPW batch.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dlen
 *   Length of previous packet in MPW descriptor.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *  true - packet match with eMPW batch attributes.
 *  false - no match, eMPW should be restarted.
 */
static __rte_always_inline bool
mlx5_tx_match_empw(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_wqe_eseg *__rte_restrict es,
		   struct mlx5_txq_local *__rte_restrict loc,
		   uint32_t dlen,
		   unsigned int olx)
{
	uint8_t swp_flags = 0;

	/* Compare the checksum flags, if any. */
	if (MLX5_TXOFF_CONFIG(CSUM) &&
	    txq_ol_cksum_to_cs(loc->mbuf) != es->cs_flags)
		return false;
	/* Compare the Software Parser offsets and flags. */
	if (MLX5_TXOFF_CONFIG(SWP) &&
	    (es->swp_offs != txq_mbuf_to_swp(loc, &swp_flags, olx) ||
	     es->swp_flags != swp_flags))
		return false;
	/* Fill metadata field if needed. */
	if (MLX5_TXOFF_CONFIG(METADATA) &&
		es->metadata != (loc->mbuf->ol_flags & RTE_MBUF_DYNFLAG_TX_METADATA ?
				 rte_cpu_to_be_32(*RTE_FLOW_DYNF_METADATA(loc->mbuf)) : 0))
		return false;
	/* Legacy MPW can send packets with the same length only. */
	if (MLX5_TXOFF_CONFIG(MPW) &&
	    dlen != rte_pktmbuf_data_len(loc->mbuf))
		return false;
	/* There must be no VLAN packets in eMPW loop. */
	if (MLX5_TXOFF_CONFIG(VLAN))
		MLX5_ASSERT(!(loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN));
	/* Check if the scheduling is requested. */
	if (MLX5_TXOFF_CONFIG(TXPP) &&
	    loc->mbuf->ol_flags & txq->ts_mask)
		return false;
	return true;
}

/**
 * Update send loop variables and WQE for eMPW loop without data inlining.
 * Number of Data Segments is equal to the number of sent packets.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param ds
 *   Number of packets/Data Segments/Packets.
 * @param slen
 *   Accumulated statistics, bytes sent.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *  true - packet match with eMPW batch attributes.
 *  false - no match, eMPW should be restarted.
 */
static __rte_always_inline void
mlx5_tx_sdone_empw(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_txq_local *__rte_restrict loc,
		   unsigned int ds,
		   unsigned int slen,
		   unsigned int olx __rte_unused)
{
	MLX5_ASSERT(!MLX5_TXOFF_CONFIG(INLINE));
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes counter. */
	 txq->stats.obytes += slen;
#else
	(void)slen;
#endif
	loc->elts_free -= ds;
	loc->pkts_sent += ds;
	ds += 2;
	loc->wqe_last->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
}

/**
 * Update send loop variables and WQE for eMPW loop with data inlining.
 * Gets the size of pushed descriptors and data to the WQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param len
 *   Total size of descriptor/data in bytes.
 * @param slen
 *   Accumulated statistics, data bytes sent.
 * @param wqem
 *   The base WQE for the eMPW/MPW descriptor.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *  true - packet match with eMPW batch attributes.
 *  false - no match, eMPW should be restarted.
 */
static __rte_always_inline void
mlx5_tx_idone_empw(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_txq_local *__rte_restrict loc,
		   unsigned int len,
		   unsigned int slen,
		   struct mlx5_wqe *__rte_restrict wqem,
		   unsigned int olx __rte_unused)
{
	struct mlx5_wqe_dseg *dseg = &wqem->dseg[0];

	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes counter. */
	 txq->stats.obytes += slen;
#else
	(void)slen;
#endif
	if (MLX5_TXOFF_CONFIG(MPW) && dseg->bcount == RTE_BE32(0)) {
		/*
		 * If the legacy MPW session contains the inline packets
		 * we should set the only inline data segment length
		 * and align the total length to the segment size.
		 */
		MLX5_ASSERT(len > sizeof(dseg->bcount));
		dseg->bcount = rte_cpu_to_be_32((len - sizeof(dseg->bcount)) |
						MLX5_ETH_WQE_DATA_INLINE);
		len = (len + MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE + 2;
	} else {
		/*
		 * The session is not legacy MPW or contains the
		 * data buffer pointer segments.
		 */
		MLX5_ASSERT((len % MLX5_WSEG_SIZE) == 0);
		len = len / MLX5_WSEG_SIZE + 2;
	}
	wqem->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | len);
	txq->wqe_ci += (len + 3) / 4;
	loc->wqe_free -= (len + 3) / 4;
	loc->wqe_last = wqem;
}

/**
 * The set of Tx burst functions for single-segment packets without TSO
 * and with Multi-Packet Writing feature support.
 * Supports all types of Tx offloads, except multi-packets and TSO.
 *
 * Uses MLX5_OPCODE_EMPW to build WQEs if possible and sends as many packet
 * per WQE as it can. If eMPW is not configured or packet can not be sent with
 * eMPW (VLAN insertion) the ordinary SEND opcode is used and only one packet
 * placed in WQE.
 *
 * Functions stop sending if it encounters the multi-segment packet or packet
 * with TSO requested.
 *
 * The routines are responsible for storing processed mbuf into elts ring buffer
 * and update elts_head if inlining offload is requested. Otherwise the copying
 * mbufs to elts can be postponed and completed at the end of burst routine.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 *   MLX5_TXCMP_CODE_MULTI - multi-segment packet encountered.
 *   MLX5_TXCMP_CODE_TSO - TSO packet encountered.
 *   MLX5_TXCMP_CODE_SINGLE - used inside functions set.
 *   MLX5_TXCMP_CODE_EMPW - used inside functions set.
 *
 * Local context variables updated.
 *
 *
 * The routine sends packets with MLX5_OPCODE_EMPW
 * without inlining, this is dedicated optimized branch.
 * No VLAN insertion is supported.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_empw_simple(struct mlx5_txq_data *__rte_restrict txq,
			  struct rte_mbuf **__rte_restrict pkts,
			  unsigned int pkts_n,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	/*
	 * Subroutine is the part of mlx5_tx_burst_single() and sends
	 * single-segment packet with eMPW opcode without data inlining.
	 */
	MLX5_ASSERT(!MLX5_TXOFF_CONFIG(INLINE));
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(EMPW));
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe_dseg *__rte_restrict dseg;
		struct mlx5_wqe_eseg *__rte_restrict eseg;
		enum mlx5_txcmp_code ret;
		unsigned int part, loop;
		unsigned int slen = 0;

next_empw:
		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		part = RTE_MIN(pkts_n, MLX5_TXOFF_CONFIG(MPW) ?
				       MLX5_MPW_MAX_PACKETS :
				       MLX5_EMPW_MAX_PACKETS);
		if (unlikely(loc->elts_free < part)) {
			/* We have no enough elts to save all mbufs. */
			if (unlikely(loc->elts_free < MLX5_EMPW_MIN_PACKETS))
				return MLX5_TXCMP_CODE_EXIT;
			/* But we still able to send at least minimal eMPW. */
			part = loc->elts_free;
		}
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, 0, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		/* Check whether we have enough WQEs */
		if (unlikely(loc->wqe_free < ((2 + part + 3) / 4))) {
			if (unlikely(loc->wqe_free <
				((2 + MLX5_EMPW_MIN_PACKETS + 3) / 4)))
				return MLX5_TXCMP_CODE_EXIT;
			part = (loc->wqe_free * 4) - 2;
		}
		if (likely(part > 1))
			rte_prefetch0(*pkts);
		loc->wqe_last = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		/*
		 * Build eMPW title WQEBB:
		 * - Control Segment, eMPW opcode
		 * - Ethernet Segment, no inline
		 */
		mlx5_tx_cseg_init(txq, loc, loc->wqe_last, part + 2,
				  MLX5_OPCODE_ENHANCED_MPSW, olx);
		mlx5_tx_eseg_none(txq, loc, loc->wqe_last,
				  olx & ~MLX5_TXOFF_CONFIG_VLAN);
		eseg = &loc->wqe_last->eseg;
		dseg = &loc->wqe_last->dseg[0];
		loop = part;
		/* Store the packet length for legacy MPW. */
		if (MLX5_TXOFF_CONFIG(MPW))
			eseg->mss = rte_cpu_to_be_16
					(rte_pktmbuf_data_len(loc->mbuf));
		for (;;) {
			uint32_t dlen = rte_pktmbuf_data_len(loc->mbuf);
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			slen += dlen;
#endif
			rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
			mlx5_tx_dseg_ptr
				(txq, loc, dseg,
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 dlen, olx);
			if (unlikely(--loop == 0))
				break;
			loc->mbuf = *pkts++;
			if (likely(loop > 1))
				rte_prefetch0(*pkts);
			ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
			/*
			 * Unroll the completion code to avoid
			 * returning variable value - it results in
			 * unoptimized sequent checking in caller.
			 */
			if (ret == MLX5_TXCMP_CODE_MULTI) {
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_MULTI;
			}
			MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
			if (ret == MLX5_TXCMP_CODE_TSO) {
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_TSO;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE) {
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_SINGLE;
			}
			if (ret != MLX5_TXCMP_CODE_EMPW) {
				MLX5_ASSERT(false);
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				return MLX5_TXCMP_CODE_ERROR;
			}
			/*
			 * Check whether packet parameters coincide
			 * within assumed eMPW batch:
			 * - check sum settings
			 * - metadata value
			 * - software parser settings
			 * - packets length (legacy MPW only)
			 * - scheduling is not required
			 */
			if (!mlx5_tx_match_empw(txq, eseg, loc, dlen, olx)) {
				MLX5_ASSERT(loop);
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				pkts_n -= part;
				goto next_empw;
			}
			/* Packet attributes match, continue the same eMPW. */
			++dseg;
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
		}
		/* eMPW is built successfully, update loop parameters. */
		MLX5_ASSERT(!loop);
		MLX5_ASSERT(pkts_n >= part);
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Update sent data bytes counter. */
		txq->stats.obytes += slen;
#endif
		loc->elts_free -= part;
		loc->pkts_sent += part;
		txq->wqe_ci += (2 + part + 3) / 4;
		loc->wqe_free -= (2 + part + 3) / 4;
		pkts_n -= part;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
		if (unlikely(ret != MLX5_TXCMP_CODE_EMPW))
			return ret;
		/* Continue sending eMPW batches. */
	}
	MLX5_ASSERT(false);
}

/**
 * The routine sends packets with MLX5_OPCODE_EMPW
 * with inlining, optionally supports VLAN insertion.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_empw_inline(struct mlx5_txq_data *__rte_restrict txq,
			  struct rte_mbuf **__rte_restrict pkts,
			  unsigned int pkts_n,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	/*
	 * Subroutine is the part of mlx5_tx_burst_single() and sends
	 * single-segment packet with eMPW opcode with data inlining.
	 */
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(EMPW));
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe_dseg *__rte_restrict dseg;
		struct mlx5_wqe *__rte_restrict wqem;
		enum mlx5_txcmp_code ret;
		unsigned int room, part, nlim;
		unsigned int slen = 0;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		/*
		 * Limits the amount of packets in one WQE
		 * to improve CQE latency generation.
		 */
		nlim = RTE_MIN(pkts_n, MLX5_TXOFF_CONFIG(MPW) ?
				       MLX5_MPW_INLINE_MAX_PACKETS :
				       MLX5_EMPW_MAX_PACKETS);
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, nlim, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		/* Check whether we have minimal amount WQEs */
		if (unlikely(loc->wqe_free <
			    ((2 + MLX5_EMPW_MIN_PACKETS + 3) / 4)))
			return MLX5_TXCMP_CODE_EXIT;
		if (likely(pkts_n > 1))
			rte_prefetch0(*pkts);
		wqem = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		/*
		 * Build eMPW title WQEBB:
		 * - Control Segment, eMPW opcode, zero DS
		 * - Ethernet Segment, no inline
		 */
		mlx5_tx_cseg_init(txq, loc, wqem, 0,
				  MLX5_OPCODE_ENHANCED_MPSW, olx);
		mlx5_tx_eseg_none(txq, loc, wqem,
				  olx & ~MLX5_TXOFF_CONFIG_VLAN);
		dseg = &wqem->dseg[0];
		/* Store the packet length for legacy MPW. */
		if (MLX5_TXOFF_CONFIG(MPW))
			wqem->eseg.mss = rte_cpu_to_be_16
					 (rte_pktmbuf_data_len(loc->mbuf));
		room = RTE_MIN(MLX5_WQE_SIZE_MAX / MLX5_WQE_SIZE,
			       loc->wqe_free) * MLX5_WQE_SIZE -
					MLX5_WQE_CSEG_SIZE -
					MLX5_WQE_ESEG_SIZE;
		/* Limit the room for legacy MPW sessions for performance. */
		if (MLX5_TXOFF_CONFIG(MPW))
			room = RTE_MIN(room,
				       RTE_MAX(txq->inlen_empw +
					       sizeof(dseg->bcount) +
					       (MLX5_TXOFF_CONFIG(VLAN) ?
					       sizeof(struct rte_vlan_hdr) : 0),
					       MLX5_MPW_INLINE_MAX_PACKETS *
					       MLX5_WQE_DSEG_SIZE));
		/* Build WQE till we have space, packets and resources. */
		part = room;
		for (;;) {
			uint32_t dlen = rte_pktmbuf_data_len(loc->mbuf);
			uint8_t *dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *);
			unsigned int tlen;

			MLX5_ASSERT(room >= MLX5_WQE_DSEG_SIZE);
			MLX5_ASSERT((room % MLX5_WQE_DSEG_SIZE) == 0);
			MLX5_ASSERT((uintptr_t)dseg < (uintptr_t)txq->wqes_end);
			/*
			 * Some Tx offloads may cause an error if packet is not
			 * long enough, check against assumed minimal length.
			 */
			if (unlikely(dlen <= MLX5_ESEG_MIN_INLINE_SIZE)) {
				part -= room;
				if (unlikely(!part))
					return MLX5_TXCMP_CODE_ERROR;
				/*
				 * We have some successfully built
				 * packet Data Segments to send.
				 */
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				return MLX5_TXCMP_CODE_ERROR;
			}
			/* Inline or not inline - that's the Question. */
			if (dlen > txq->inlen_empw ||
			    loc->mbuf->ol_flags & RTE_MBUF_F_TX_DYNF_NOINLINE)
				goto pointer_empw;
			if (MLX5_TXOFF_CONFIG(MPW)) {
				if (dlen > txq->inlen_send)
					goto pointer_empw;
				tlen = dlen;
				if (part == room) {
					/* Open new inline MPW session. */
					tlen += sizeof(dseg->bcount);
					dseg->bcount = RTE_BE32(0);
					dseg = RTE_PTR_ADD
						(dseg, sizeof(dseg->bcount));
				} else {
					/*
					 * No pointer and inline descriptor
					 * intermix for legacy MPW sessions.
					 */
					if (wqem->dseg[0].bcount)
						break;
				}
			} else {
				tlen = sizeof(dseg->bcount) + dlen;
			}
			/* Inline entire packet, optional VLAN insertion. */
			if (MLX5_TXOFF_CONFIG(VLAN) &&
			    loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN) {
				/*
				 * The packet length must be checked in
				 * mlx5_tx_able_to_empw() and packet
				 * fits into inline length guaranteed.
				 */
				MLX5_ASSERT((dlen +
					     sizeof(struct rte_vlan_hdr)) <=
					    txq->inlen_empw);
				tlen += sizeof(struct rte_vlan_hdr);
				if (room < tlen)
					break;
				rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
				dseg = mlx5_tx_dseg_vlan(txq, loc, dseg,
							 dptr, dlen, olx);
#ifdef MLX5_PMD_SOFT_COUNTERS
				/* Update sent data bytes counter. */
				slen +=	sizeof(struct rte_vlan_hdr);
#endif
			} else {
				if (room < tlen)
					break;
				rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
				dseg = mlx5_tx_dseg_empw(txq, loc, dseg,
							 dptr, dlen, olx);
			}
			if (!MLX5_TXOFF_CONFIG(MPW))
				tlen = RTE_ALIGN(tlen, MLX5_WSEG_SIZE);
			MLX5_ASSERT(room >= tlen);
			room -= tlen;
			/*
			 * Packet data are completely inline,
			 * we can try to free the packet.
			 */
			if (likely(loc->pkts_sent == loc->mbuf_free)) {
				/*
				 * All the packets from the burst beginning
				 * are inline, we can free mbufs directly
				 * from the origin array on tx_burst exit().
				 */
				loc->mbuf_free++;
				goto next_mbuf;
			}
			/*
			 * In order no to call rte_pktmbuf_free_seg() here,
			 * in the most inner loop (that might be very
			 * expensive) we just save the mbuf in elts.
			 */
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
			loc->elts_free--;
			goto next_mbuf;
pointer_empw:
			/*
			 * No pointer and inline descriptor
			 * intermix for legacy MPW sessions.
			 */
			if (MLX5_TXOFF_CONFIG(MPW) &&
			    part != room &&
			    wqem->dseg[0].bcount == RTE_BE32(0))
				break;
			/*
			 * Not inlinable VLAN packets are
			 * proceeded outside of this routine.
			 */
			MLX5_ASSERT(room >= MLX5_WQE_DSEG_SIZE);
			if (MLX5_TXOFF_CONFIG(VLAN))
				MLX5_ASSERT(!(loc->mbuf->ol_flags &
					    RTE_MBUF_F_TX_VLAN));
			rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
			mlx5_tx_dseg_ptr(txq, loc, dseg, dptr, dlen, olx);
			/* We have to store mbuf in elts.*/
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
			loc->elts_free--;
			room -= MLX5_WQE_DSEG_SIZE;
			/* Ring buffer wraparound is checked at the loop end.*/
			++dseg;
next_mbuf:
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			slen += dlen;
#endif
			loc->pkts_sent++;
			pkts_n--;
			if (unlikely(!pkts_n || !loc->elts_free)) {
				/*
				 * We have no resources/packets to
				 * continue build descriptors.
				 */
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				return MLX5_TXCMP_CODE_EXIT;
			}
			loc->mbuf = *pkts++;
			if (likely(pkts_n > 1))
				rte_prefetch0(*pkts);
			ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
			/*
			 * Unroll the completion code to avoid
			 * returning variable value - it results in
			 * unoptimized sequent checking in caller.
			 */
			if (ret == MLX5_TXCMP_CODE_MULTI) {
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_MULTI;
			}
			MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
			if (ret == MLX5_TXCMP_CODE_TSO) {
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_TSO;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE) {
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_SINGLE;
			}
			if (ret != MLX5_TXCMP_CODE_EMPW) {
				MLX5_ASSERT(false);
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				return MLX5_TXCMP_CODE_ERROR;
			}
			/* Check if we have minimal room left. */
			nlim--;
			if (unlikely(!nlim || room < MLX5_WQE_DSEG_SIZE))
				break;
			/*
			 * Check whether packet parameters coincide
			 * within assumed eMPW batch:
			 * - check sum settings
			 * - metadata value
			 * - software parser settings
			 * - packets length (legacy MPW only)
			 * - scheduling is not required
			 */
			if (!mlx5_tx_match_empw(txq, &wqem->eseg,
						loc, dlen, olx))
				break;
			/* Packet attributes match, continue the same eMPW. */
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
		}
		/*
		 * We get here to close an existing eMPW
		 * session and start the new one.
		 */
		MLX5_ASSERT(pkts_n);
		part -= room;
		if (unlikely(!part))
			return MLX5_TXCMP_CODE_EXIT;
		mlx5_tx_idone_empw(txq, loc, part, slen, wqem, olx);
		if (unlikely(!loc->elts_free ||
			     !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		/* Continue the loop with new eMPW session. */
	}
	MLX5_ASSERT(false);
}

/**
 * The routine sends packets with ordinary MLX5_OPCODE_SEND.
 * Data inlining and VLAN insertion are supported.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_single_send(struct mlx5_txq_data *__rte_restrict txq,
			  struct rte_mbuf **__rte_restrict pkts,
			  unsigned int pkts_n,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	/*
	 * Subroutine is the part of mlx5_tx_burst_single()
	 * and sends single-segment packet with SEND opcode.
	 */
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe *__rte_restrict wqe;
		enum mlx5_txcmp_code ret;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		MLX5_ASSERT(loc->elts_free);
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, 0, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		if (MLX5_TXOFF_CONFIG(INLINE)) {
			unsigned int inlen, vlan = 0;

			inlen = rte_pktmbuf_data_len(loc->mbuf);
			if (MLX5_TXOFF_CONFIG(VLAN) &&
			    loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN) {
				vlan = sizeof(struct rte_vlan_hdr);
				inlen += vlan;
			}
			/*
			 * If inlining is enabled at configuration time
			 * the limit must be not less than minimal size.
			 * Otherwise we would do extra check for data
			 * size to avoid crashes due to length overflow.
			 */
			MLX5_ASSERT(txq->inlen_send >=
				    MLX5_ESEG_MIN_INLINE_SIZE);
			if (inlen <= txq->inlen_send) {
				unsigned int seg_n, wqe_n;

				rte_prefetch0(rte_pktmbuf_mtod
						(loc->mbuf, uint8_t *));
				/* Check against minimal length. */
				if (inlen <= MLX5_ESEG_MIN_INLINE_SIZE)
					return MLX5_TXCMP_CODE_ERROR;
				if (loc->mbuf->ol_flags &
				    RTE_MBUF_F_TX_DYNF_NOINLINE) {
					/*
					 * The hint flag not to inline packet
					 * data is set. Check whether we can
					 * follow the hint.
					 */
					if ((!MLX5_TXOFF_CONFIG(EMPW) &&
					      txq->inlen_mode) ||
					    (MLX5_TXOFF_CONFIG(MPW) &&
					     txq->inlen_mode)) {
						if (inlen <= txq->inlen_send)
							goto single_inline;
						/*
						 * The hardware requires the
						 * minimal inline data header.
						 */
						goto single_min_inline;
					}
					if (MLX5_TXOFF_CONFIG(VLAN) &&
					    vlan && !txq->vlan_en) {
						/*
						 * We must insert VLAN tag
						 * by software means.
						 */
						goto single_part_inline;
					}
					goto single_no_inline;
				}
single_inline:
				/*
				 * Completely inlined packet data WQE:
				 * - Control Segment, SEND opcode
				 * - Ethernet Segment, no VLAN insertion
				 * - Data inlined, VLAN optionally inserted
				 * - Alignment to MLX5_WSEG_SIZE
				 * Have to estimate amount of WQEBBs
				 */
				seg_n = (inlen + 3 * MLX5_WSEG_SIZE -
					 MLX5_ESEG_MIN_INLINE_SIZE +
					 MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
				/* Check if there are enough WQEBBs. */
				wqe_n = (seg_n + 3) / 4;
				if (wqe_n > loc->wqe_free)
					return MLX5_TXCMP_CODE_EXIT;
				wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
				loc->wqe_last = wqe;
				mlx5_tx_cseg_init(txq, loc, wqe, seg_n,
						  MLX5_OPCODE_SEND, olx);
				rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
				mlx5_tx_eseg_data(txq, loc, wqe,
						  vlan, inlen, 0, olx);
				txq->wqe_ci += wqe_n;
				loc->wqe_free -= wqe_n;
				/*
				 * Packet data are completely inlined,
				 * free the packet immediately.
				 */
				rte_pktmbuf_free_seg(loc->mbuf);
			} else if ((!MLX5_TXOFF_CONFIG(EMPW) ||
				     MLX5_TXOFF_CONFIG(MPW)) &&
					txq->inlen_mode) {
				/*
				 * If minimal inlining is requested the eMPW
				 * feature should be disabled due to data is
				 * inlined into Ethernet Segment, which can
				 * not contain inlined data for eMPW due to
				 * segment shared for all packets.
				 */
				struct mlx5_wqe_dseg *__rte_restrict dseg;
				unsigned int ds;
				uint8_t *dptr;

				/*
				 * The inline-mode settings require
				 * to inline the specified amount of
				 * data bytes to the Ethernet Segment.
				 * We should check the free space in
				 * WQE ring buffer to inline partially.
				 */
single_min_inline:
				MLX5_ASSERT(txq->inlen_send >= txq->inlen_mode);
				MLX5_ASSERT(inlen > txq->inlen_mode);
				MLX5_ASSERT(txq->inlen_mode >=
					    MLX5_ESEG_MIN_INLINE_SIZE);
				/*
				 * Check whether there are enough free WQEBBs:
				 * - Control Segment
				 * - Ethernet Segment
				 * - First Segment of inlined Ethernet data
				 * - ... data continued ...
				 * - Finishing Data Segment of pointer type
				 */
				ds = (MLX5_WQE_CSEG_SIZE +
				      MLX5_WQE_ESEG_SIZE +
				      MLX5_WQE_DSEG_SIZE +
				      txq->inlen_mode -
				      MLX5_ESEG_MIN_INLINE_SIZE +
				      MLX5_WQE_DSEG_SIZE +
				      MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
				if (loc->wqe_free < ((ds + 3) / 4))
					return MLX5_TXCMP_CODE_EXIT;
				/*
				 * Build the ordinary SEND WQE:
				 * - Control Segment
				 * - Ethernet Segment, inline inlen_mode bytes
				 * - Data Segment of pointer type
				 */
				wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
				loc->wqe_last = wqe;
				mlx5_tx_cseg_init(txq, loc, wqe, ds,
						  MLX5_OPCODE_SEND, olx);
				rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
				dseg = mlx5_tx_eseg_data(txq, loc, wqe, vlan,
							 txq->inlen_mode,
							 0, olx);
				dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *) +
				       txq->inlen_mode - vlan;
				inlen -= txq->inlen_mode;
				mlx5_tx_dseg_ptr(txq, loc, dseg,
						 dptr, inlen, olx);
				/*
				 * WQE is built, update the loop parameters
				 * and got to the next packet.
				 */
				txq->wqe_ci += (ds + 3) / 4;
				loc->wqe_free -= (ds + 3) / 4;
				/* We have to store mbuf in elts.*/
				MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
				txq->elts[txq->elts_head++ & txq->elts_m] =
						loc->mbuf;
				--loc->elts_free;
			} else {
				uint8_t *dptr;
				unsigned int dlen;

				/*
				 * Partially inlined packet data WQE, we have
				 * some space in title WQEBB, we can fill it
				 * with some packet data. It takes one WQEBB,
				 * it is available, no extra space check:
				 * - Control Segment, SEND opcode
				 * - Ethernet Segment, no VLAN insertion
				 * - MLX5_ESEG_MIN_INLINE_SIZE bytes of Data
				 * - Data Segment, pointer type
				 *
				 * We also get here if VLAN insertion is not
				 * supported by HW, the inline is enabled.
				 */
single_part_inline:
				wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
				loc->wqe_last = wqe;
				mlx5_tx_cseg_init(txq, loc, wqe, 4,
						  MLX5_OPCODE_SEND, olx);
				rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
				mlx5_tx_eseg_dmin(txq, loc, wqe, vlan, olx);
				dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *) +
				       MLX5_ESEG_MIN_INLINE_SIZE - vlan;
				/*
				 * The length check is performed above, by
				 * comparing with txq->inlen_send. We should
				 * not get overflow here.
				 */
				MLX5_ASSERT(inlen > MLX5_ESEG_MIN_INLINE_SIZE);
				dlen = inlen - MLX5_ESEG_MIN_INLINE_SIZE;
				mlx5_tx_dseg_ptr(txq, loc, &wqe->dseg[1],
						 dptr, dlen, olx);
				++txq->wqe_ci;
				--loc->wqe_free;
				/* We have to store mbuf in elts.*/
				MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
				txq->elts[txq->elts_head++ & txq->elts_m] =
						loc->mbuf;
				--loc->elts_free;
			}
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			txq->stats.obytes += vlan +
					rte_pktmbuf_data_len(loc->mbuf);
#endif
		} else {
			/*
			 * No inline at all, it means the CPU cycles saving
			 * is prioritized at configuration, we should not
			 * copy any packet data to WQE.
			 *
			 * SEND WQE, one WQEBB:
			 * - Control Segment, SEND opcode
			 * - Ethernet Segment, optional VLAN, no inline
			 * - Data Segment, pointer type
			 */
single_no_inline:
			wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
			loc->wqe_last = wqe;
			mlx5_tx_cseg_init(txq, loc, wqe, 3,
					  MLX5_OPCODE_SEND, olx);
			rte_pmd_mlx5_trace_tx_push(loc->mbuf, txq->wqe_ci);
			mlx5_tx_eseg_none(txq, loc, wqe, olx);
			mlx5_tx_dseg_ptr
				(txq, loc, &wqe->dseg[0],
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 rte_pktmbuf_data_len(loc->mbuf), olx);
			++txq->wqe_ci;
			--loc->wqe_free;
			/*
			 * We should not store mbuf pointer in elts
			 * if no inlining is configured, this is done
			 * by calling routine in a batch copy.
			 */
			if (MLX5_TXOFF_CONFIG(INLINE))
				txq->elts[txq->elts_head++ & txq->elts_m] =
							loc->mbuf;
			--loc->elts_free;
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			txq->stats.obytes += rte_pktmbuf_data_len(loc->mbuf);
			if (MLX5_TXOFF_CONFIG(VLAN) &&
			    loc->mbuf->ol_flags & RTE_MBUF_F_TX_VLAN)
				txq->stats.obytes +=
					sizeof(struct rte_vlan_hdr);
#endif
		}
		++loc->pkts_sent;
		--pkts_n;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		if (pkts_n > 1)
			rte_prefetch0(*pkts);
		ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
		if (unlikely(ret != MLX5_TXCMP_CODE_SINGLE))
			return ret;
	}
	MLX5_ASSERT(false);
}

static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_single(struct mlx5_txq_data *__rte_restrict txq,
		     struct rte_mbuf **__rte_restrict pkts,
		     unsigned int pkts_n,
		     struct mlx5_txq_local *__rte_restrict loc,
		     unsigned int olx)
{
	enum mlx5_txcmp_code ret;

	ret = mlx5_tx_able_to_empw(txq, loc, olx, false);
	if (ret == MLX5_TXCMP_CODE_SINGLE)
		goto ordinary_send;
	MLX5_ASSERT(ret == MLX5_TXCMP_CODE_EMPW);
	for (;;) {
		/* Optimize for inline/no inline eMPW send. */
		ret = (MLX5_TXOFF_CONFIG(INLINE)) ?
			mlx5_tx_burst_empw_inline
				(txq, pkts, pkts_n, loc, olx) :
			mlx5_tx_burst_empw_simple
				(txq, pkts, pkts_n, loc, olx);
		if (ret != MLX5_TXCMP_CODE_SINGLE)
			return ret;
		/* The resources to send one packet should remain. */
		MLX5_ASSERT(loc->elts_free && loc->wqe_free);
ordinary_send:
		ret = mlx5_tx_burst_single_send(txq, pkts, pkts_n, loc, olx);
		MLX5_ASSERT(ret != MLX5_TXCMP_CODE_SINGLE);
		if (ret != MLX5_TXCMP_CODE_EMPW)
			return ret;
		/* The resources to send one packet should remain. */
		MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	}
}

/**
 * DPDK Tx callback template. This is configured template used to generate
 * routines optimized for specified offload setup.
 * One of this generated functions is chosen at SQ configuration time.
 *
 * @param txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param olx
 *   Configured offloads mask, presents the bits of MLX5_TXOFF_CONFIG_xxx
 *   values. Should be static to take compile time static configuration
 *   advantages.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static __rte_always_inline uint16_t
mlx5_tx_burst_tmpl(struct mlx5_txq_data *__rte_restrict txq,
		   struct rte_mbuf **__rte_restrict pkts,
		   uint16_t pkts_n,
		   unsigned int olx)
{
	struct mlx5_txq_local loc;
	enum mlx5_txcmp_code ret;
	unsigned int part;

	MLX5_ASSERT(txq->elts_s >= (uint16_t)(txq->elts_head - txq->elts_tail));
	MLX5_ASSERT(txq->wqe_s >= (uint16_t)(txq->wqe_ci - txq->wqe_pi));
	if (unlikely(!pkts_n))
		return 0;
	if (MLX5_TXOFF_CONFIG(INLINE))
		loc.mbuf_free = 0;
	loc.pkts_sent = 0;
	loc.pkts_copy = 0;
	loc.wqe_last = NULL;

send_loop:
	loc.pkts_loop = loc.pkts_sent;
	/*
	 * Check if there are some CQEs, if any:
	 * - process an encountered errors
	 * - process the completed WQEs
	 * - free related mbufs
	 * - doorbell the NIC about processed CQEs
	 */
	rte_prefetch0(*(pkts + loc.pkts_sent));
	mlx5_tx_handle_completion(txq, olx);
	/*
	 * Calculate the number of available resources - elts and WQEs.
	 * There are two possible different scenarios:
	 * - no data inlining into WQEs, one WQEBB may contains up to
	 *   four packets, in this case elts become scarce resource
	 * - data inlining into WQEs, one packet may require multiple
	 *   WQEBBs, the WQEs become the limiting factor.
	 */
	MLX5_ASSERT(txq->elts_s >= (uint16_t)(txq->elts_head - txq->elts_tail));
	loc.elts_free = txq->elts_s -
				(uint16_t)(txq->elts_head - txq->elts_tail);
	MLX5_ASSERT(txq->wqe_s >= (uint16_t)(txq->wqe_ci - txq->wqe_pi));
	loc.wqe_free = txq->wqe_s -
				(uint16_t)(txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!loc.elts_free || !loc.wqe_free))
		goto burst_exit;
	for (;;) {
		/*
		 * Fetch the packet from array. Usually this is the first
		 * packet in series of multi/single segment packets.
		 */
		loc.mbuf = *(pkts + loc.pkts_sent);
		/* Dedicated branch for multi-segment packets. */
		if (MLX5_TXOFF_CONFIG(MULTI) &&
		    unlikely(NB_SEGS(loc.mbuf) > 1)) {
			/*
			 * Multi-segment packet encountered.
			 * Hardware is able to process it only
			 * with SEND/TSO opcodes, one packet
			 * per WQE, do it in dedicated routine.
			 */
enter_send_multi:
			MLX5_ASSERT(loc.pkts_sent >= loc.pkts_copy);
			part = loc.pkts_sent - loc.pkts_copy;
			if (!MLX5_TXOFF_CONFIG(INLINE) && part) {
				/*
				 * There are some single-segment mbufs not
				 * stored in elts. The mbufs must be in the
				 * same order as WQEs, so we must copy the
				 * mbufs to elts here, before the coming
				 * multi-segment packet mbufs is appended.
				 */
				mlx5_tx_copy_elts(txq, pkts + loc.pkts_copy,
						  part, olx);
				loc.pkts_copy = loc.pkts_sent;
			}
			MLX5_ASSERT(pkts_n > loc.pkts_sent);
			ret = mlx5_tx_burst_mseg(txq, pkts, pkts_n, &loc, olx);
			if (!MLX5_TXOFF_CONFIG(INLINE))
				loc.pkts_copy = loc.pkts_sent;
			/*
			 * These returned code checks are supposed
			 * to be optimized out due to routine inlining.
			 */
			if (ret == MLX5_TXCMP_CODE_EXIT) {
				/*
				 * The routine returns this code when
				 * all packets are sent or there is no
				 * enough resources to complete request.
				 */
				break;
			}
			if (ret == MLX5_TXCMP_CODE_ERROR) {
				/*
				 * The routine returns this code when some error
				 * in the incoming packets format occurred.
				 */
				txq->stats.oerrors++;
				break;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE) {
				/*
				 * The single-segment packet was encountered
				 * in the array, try to send it with the
				 * best optimized way, possible engaging eMPW.
				 */
				goto enter_send_single;
			}
			if (MLX5_TXOFF_CONFIG(TSO) &&
			    ret == MLX5_TXCMP_CODE_TSO) {
				/*
				 * The single-segment TSO packet was
				 * encountered in the array.
				 */
				goto enter_send_tso;
			}
			/* We must not get here. Something is going wrong. */
			MLX5_ASSERT(false);
			txq->stats.oerrors++;
			break;
		}
		/* Dedicated branch for single-segment TSO packets. */
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    unlikely(loc.mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			/*
			 * TSO might require special way for inlining
			 * (dedicated parameters) and is sent with
			 * MLX5_OPCODE_TSO opcode only, provide this
			 * in dedicated branch.
			 */
enter_send_tso:
			MLX5_ASSERT(NB_SEGS(loc.mbuf) == 1);
			MLX5_ASSERT(pkts_n > loc.pkts_sent);
			ret = mlx5_tx_burst_tso(txq, pkts, pkts_n, &loc, olx);
			/*
			 * These returned code checks are supposed
			 * to be optimized out due to routine inlining.
			 */
			if (ret == MLX5_TXCMP_CODE_EXIT)
				break;
			if (ret == MLX5_TXCMP_CODE_ERROR) {
				txq->stats.oerrors++;
				break;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE)
				goto enter_send_single;
			if (MLX5_TXOFF_CONFIG(MULTI) &&
			    ret == MLX5_TXCMP_CODE_MULTI) {
				/*
				 * The multi-segment packet was
				 * encountered in the array.
				 */
				goto enter_send_multi;
			}
			/* We must not get here. Something is going wrong. */
			MLX5_ASSERT(false);
			txq->stats.oerrors++;
			break;
		}
		/*
		 * The dedicated branch for the single-segment packets
		 * without TSO. Often these ones can be sent using
		 * MLX5_OPCODE_EMPW with multiple packets in one WQE.
		 * The routine builds the WQEs till it encounters
		 * the TSO or multi-segment packet (in case if these
		 * offloads are requested at SQ configuration time).
		 */
enter_send_single:
		MLX5_ASSERT(pkts_n > loc.pkts_sent);
		ret = mlx5_tx_burst_single(txq, pkts, pkts_n, &loc, olx);
		/*
		 * These returned code checks are supposed
		 * to be optimized out due to routine inlining.
		 */
		if (ret == MLX5_TXCMP_CODE_EXIT)
			break;
		if (ret == MLX5_TXCMP_CODE_ERROR) {
			txq->stats.oerrors++;
			break;
		}
		if (MLX5_TXOFF_CONFIG(MULTI) &&
		    ret == MLX5_TXCMP_CODE_MULTI) {
			/*
			 * The multi-segment packet was
			 * encountered in the array.
			 */
			goto enter_send_multi;
		}
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    ret == MLX5_TXCMP_CODE_TSO) {
			/*
			 * The single-segment TSO packet was
			 * encountered in the array.
			 */
			goto enter_send_tso;
		}
		/* We must not get here. Something is going wrong. */
		MLX5_ASSERT(false);
		txq->stats.oerrors++;
		break;
	}
	/*
	 * Main Tx loop is completed, do the rest:
	 * - set completion request if thresholds are reached
	 * - doorbell the hardware
	 * - copy the rest of mbufs to elts (if any)
	 */
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE) ||
		    loc.pkts_sent >= loc.pkts_copy);
	/* Take a shortcut if nothing is sent. */
	if (unlikely(loc.pkts_sent == loc.pkts_loop))
		goto burst_exit;
	/* Request CQE generation if limits are reached. */
	if (MLX5_TXOFF_CONFIG(TXPP) && __rte_trace_point_fp_is_enabled())
		mlx5_tx_request_completion_trace(txq, &loc, olx);
	else
		mlx5_tx_request_completion(txq, &loc, olx);
	/*
	 * Ring QP doorbell immediately after WQE building completion
	 * to improve latencies. The pure software related data treatment
	 * can be completed after doorbell. Tx CQEs for this SQ are
	 * processed in this thread only by the polling.
	 *
	 * The rdma core library can map doorbell register in two ways,
	 * depending on the environment variable "MLX5_SHUT_UP_BF":
	 *
	 * - as regular cached memory, the variable is either missing or
	 *   set to zero. This type of mapping may cause the significant
	 *   doorbell register writing latency and requires explicit memory
	 *   write barrier to mitigate this issue and prevent write combining.
	 *
	 * - as non-cached memory, the variable is present and set to not "0"
	 *   value. This type of mapping may cause performance impact under
	 *   heavy loading conditions but the explicit write memory barrier is
	 *   not required and it may improve core performance.
	 *
	 * - the legacy behaviour (prior 19.08 release) was to use some
	 *   heuristics to decide whether write memory barrier should
	 *   be performed. This behavior is supported with specifying
	 *   tx_db_nc=2, write barrier is skipped if application provides
	 *   the full recommended burst of packets, it supposes the next
	 *   packets are coming and the write barrier will be issued on
	 *   the next burst (after descriptor writing, at least).
	 */
	mlx5_doorbell_ring(mlx5_tx_bfreg(txq),
			   *(volatile uint64_t *)loc.wqe_last, txq->wqe_ci,
			   txq->qp_db, !txq->db_nc &&
			   (!txq->db_heu || pkts_n % MLX5_TX_DEFAULT_BURST));
	/* Not all of the mbufs may be stored into elts yet. */
	part = MLX5_TXOFF_CONFIG(INLINE) ? 0 : loc.pkts_sent - loc.pkts_copy;
	if (!MLX5_TXOFF_CONFIG(INLINE) && part) {
		/*
		 * There are some single-segment mbufs not stored in elts.
		 * It can be only if the last packet was single-segment.
		 * The copying is gathered into one place due to it is
		 * a good opportunity to optimize that with SIMD.
		 * Unfortunately if inlining is enabled the gaps in pointer
		 * array may happen due to early freeing of the inlined mbufs.
		 */
		mlx5_tx_copy_elts(txq, pkts + loc.pkts_copy, part, olx);
		loc.pkts_copy = loc.pkts_sent;
	}
	MLX5_ASSERT(txq->elts_s >= (uint16_t)(txq->elts_head - txq->elts_tail));
	MLX5_ASSERT(txq->wqe_s >= (uint16_t)(txq->wqe_ci - txq->wqe_pi));
	if (pkts_n > loc.pkts_sent) {
		/*
		 * If burst size is large there might be no enough CQE
		 * fetched from completion queue and no enough resources
		 * freed to send all the packets.
		 */
		goto send_loop;
	}
burst_exit:
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += loc.pkts_sent;
#endif
	if (MLX5_TXOFF_CONFIG(INLINE) && loc.mbuf_free)
		__mlx5_tx_free_mbuf(txq, pkts, loc.mbuf_free, olx);
	/* Trace productive bursts only. */
	if (__rte_trace_point_fp_is_enabled() && loc.pkts_sent)
		rte_pmd_mlx5_trace_tx_exit(loc.pkts_sent, pkts_n);
	return loc.pkts_sent;
}

#endif /* RTE_PMD_MLX5_TX_H_ */
