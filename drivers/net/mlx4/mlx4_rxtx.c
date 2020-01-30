/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

/**
 * @file
 * Data plane functions for mlx4 driver.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_io.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>

#include "mlx4.h"
#include "mlx4_prm.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/**
 * Pointer-value pair structure used in tx_post_send for saving the first
 * DWORD (32 byte) of a TXBB.
 */
struct pv {
	union {
		volatile struct mlx4_wqe_data_seg *dseg;
		volatile uint32_t *dst;
	};
	uint32_t val;
};

/** A helper structure for TSO packet handling. */
struct tso_info {
	/** Pointer to the array of saved first DWORD (32 byte) of a TXBB. */
	struct pv *pv;
	/** Current entry in the pv array. */
	int pv_counter;
	/** Total size of the WQE including padding. */
	uint32_t wqe_size;
	/** Size of TSO header to prepend to each packet to send. */
	uint16_t tso_header_size;
	/** Total size of the TSO segment in the WQE. */
	uint16_t wqe_tso_seg_size;
	/** Raw WQE size in units of 16 Bytes and without padding. */
	uint8_t fence_size;
};

/** A table to translate Rx completion flags to packet type. */
uint32_t mlx4_ptype_table[0x100] __rte_cache_aligned = {
	/*
	 * The index to the array should have:
	 *  bit[7] - MLX4_CQE_L2_TUNNEL
	 *  bit[6] - MLX4_CQE_L2_TUNNEL_IPV4
	 *  bit[5] - MLX4_CQE_STATUS_UDP
	 *  bit[4] - MLX4_CQE_STATUS_TCP
	 *  bit[3] - MLX4_CQE_STATUS_IPV4OPT
	 *  bit[2] - MLX4_CQE_STATUS_IPV6
	 *  bit[1] - MLX4_CQE_STATUS_IPF
	 *  bit[0] - MLX4_CQE_STATUS_IPV4
	 * giving a total of up to 256 entries.
	 */
	/* L2 */
	[0x00] = RTE_PTYPE_L2_ETHER,
	/* L3 */
	[0x01] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG,
	[0x02] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG,
	[0x03] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG,
	[0x04] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG,
	[0x06] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG,
	[0x08] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_NONFRAG,
	[0x09] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_NONFRAG,
	[0x0a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_FRAG,
	[0x0b] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_FRAG,
	/* TCP */
	[0x11] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP,
	[0x14] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP,
	[0x16] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG,
	[0x18] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_TCP,
	[0x19] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_TCP,
	/* UDP */
	[0x21] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP,
	[0x24] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP,
	[0x26] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG,
	[0x28] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_UDP,
	[0x29] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT |
		     RTE_PTYPE_L4_UDP,
	/* Tunneled - L3 IPV6 */
	[0x80] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
	[0x81] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0x82] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0x83] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0x84] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0x86] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0x88] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0x89] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0x8a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0x8b] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_FRAG,
	/* Tunneled - L3 IPV6, TCP */
	[0x91] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP,
	[0x94] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP,
	[0x96] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0x98] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT | RTE_PTYPE_INNER_L4_TCP,
	[0x99] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT | RTE_PTYPE_INNER_L4_TCP,
	/* Tunneled - L3 IPV6, UDP */
	[0xa1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP,
	[0xa4] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP,
	[0xa6] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0xa8] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_UDP,
	[0xa9] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_UDP,
	/* Tunneled - L3 IPV4 */
	[0xc0] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
	[0xc1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0xc2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0xc3] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0xc4] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0xc6] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0xc8] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0xc9] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_NONFRAG,
	[0xca] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0xcb] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_FRAG,
	/* Tunneled - L3 IPV4, TCP */
	[0xd1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP,
	[0xd4] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP,
	[0xd6] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0xd8] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_TCP,
	[0xd9] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_TCP,
	/* Tunneled - L3 IPV4, UDP */
	[0xe1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP,
	[0xe4] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP,
	[0xe6] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG,
	[0xe8] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_UDP,
	[0xe9] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT |
		     RTE_PTYPE_INNER_L4_UDP,
};

/**
 * Stamp TXBB burst so it won't be reused by the HW.
 *
 * Routine is used when freeing WQE used by the chip or when failing
 * building an WQ entry has failed leaving partial information on the queue.
 *
 * @param sq
 *   Pointer to the SQ structure.
 * @param start
 *   Pointer to the first TXBB to stamp.
 * @param end
 *   Pointer to the followed end TXBB to stamp.
 *
 * @return
 *   Stamping burst size in byte units.
 */
static uint32_t
mlx4_txq_stamp_freed_wqe(struct mlx4_sq *sq, volatile uint32_t *start,
			 volatile uint32_t *end)
{
	uint32_t stamp = sq->stamp;
	int32_t size = (intptr_t)end - (intptr_t)start;

	MLX4_ASSERT(start != end);
	/* Hold SQ ring wrap around. */
	if (size < 0) {
		size = (int32_t)sq->size + size;
		do {
			*start = stamp;
			start += MLX4_SQ_STAMP_DWORDS;
		} while (start != (volatile uint32_t *)sq->eob);
		start = (volatile uint32_t *)sq->buf;
		/* Flip invalid stamping ownership. */
		stamp ^= RTE_BE32(1u << MLX4_SQ_OWNER_BIT);
		sq->stamp = stamp;
		if (start == end)
			return size;
	}
	do {
		*start = stamp;
		start += MLX4_SQ_STAMP_DWORDS;
	} while (start != end);
	return (uint32_t)size;
}

/**
 * Manage Tx completions.
 *
 * When sending a burst, mlx4_tx_burst() posts several WRs.
 * To improve performance, a completion event is only required once every
 * MLX4_PMD_TX_PER_COMP_REQ sends. Doing so discards completion information
 * for other WRs, but this information would not be used anyway.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param elts_m
 *   Tx elements number mask.
 * @param sq
 *   Pointer to the SQ structure.
 */
static void
mlx4_txq_complete(struct txq *txq, const unsigned int elts_m,
		  struct mlx4_sq *sq)
{
	unsigned int elts_tail = txq->elts_tail;
	struct mlx4_cq *cq = &txq->mcq;
	volatile struct mlx4_cqe *cqe;
	uint32_t completed;
	uint32_t cons_index = cq->cons_index;
	volatile uint32_t *first_txbb;

	/*
	 * Traverse over all CQ entries reported and handle each WQ entry
	 * reported by them.
	 */
	do {
		cqe = (volatile struct mlx4_cqe *)mlx4_get_cqe(cq, cons_index);
		if (unlikely(!!(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK) ^
		    !!(cons_index & cq->cqe_cnt)))
			break;
#ifdef RTE_LIBRTE_MLX4_DEBUG
		/*
		 * Make sure we read the CQE after we read the ownership bit.
		 */
		rte_io_rmb();
		if (unlikely((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) ==
			     MLX4_CQE_OPCODE_ERROR)) {
			volatile struct mlx4_err_cqe *cqe_err =
				(volatile struct mlx4_err_cqe *)cqe;
			ERROR("%p CQE error - vendor syndrome: 0x%x"
			      " syndrome: 0x%x\n",
			      (void *)txq, cqe_err->vendor_err,
			      cqe_err->syndrome);
			break;
		}
#endif /* RTE_LIBRTE_MLX4_DEBUG */
		cons_index++;
	} while (1);
	completed = (cons_index - cq->cons_index) * txq->elts_comp_cd_init;
	if (unlikely(!completed))
		return;
	/* First stamping address is the end of the last one. */
	first_txbb = (&(*txq->elts)[elts_tail & elts_m])->eocb;
	elts_tail += completed;
	/* The new tail element holds the end address. */
	sq->remain_size += mlx4_txq_stamp_freed_wqe(sq, first_txbb,
		(&(*txq->elts)[elts_tail & elts_m])->eocb);
	/* Update CQ consumer index. */
	cq->cons_index = cons_index;
	*cq->set_ci_db = rte_cpu_to_be_32(cons_index & MLX4_CQ_DB_CI_MASK);
	txq->elts_tail = elts_tail;
}

/**
 * Write Tx data segment to the SQ.
 *
 * @param dseg
 *   Pointer to data segment in SQ.
 * @param lkey
 *   Memory region lkey.
 * @param addr
 *   Data address.
 * @param byte_count
 *   Big endian bytes count of the data to send.
 */
static inline void
mlx4_fill_tx_data_seg(volatile struct mlx4_wqe_data_seg *dseg,
		       uint32_t lkey, uintptr_t addr, rte_be32_t  byte_count)
{
	dseg->addr = rte_cpu_to_be_64(addr);
	dseg->lkey = lkey;
#if RTE_CACHE_LINE_SIZE < 64
	/*
	 * Need a barrier here before writing the byte_count
	 * fields to make sure that all the data is visible
	 * before the byte_count field is set.
	 * Otherwise, if the segment begins a new cacheline,
	 * the HCA prefetcher could grab the 64-byte chunk and
	 * get a valid (!= 0xffffffff) byte count but stale
	 * data, and end up sending the wrong data.
	 */
	rte_io_wmb();
#endif /* RTE_CACHE_LINE_SIZE */
	dseg->byte_count = byte_count;
}

/**
 * Obtain and calculate TSO information needed for assembling a TSO WQE.
 *
 * @param buf
 *   Pointer to the first packet mbuf.
 * @param txq
 *   Pointer to Tx queue structure.
 * @param tinfo
 *   Pointer to a structure to fill the info with.
 *
 * @return
 *   0 on success, negative value upon error.
 */
static inline int
mlx4_tx_burst_tso_get_params(struct rte_mbuf *buf,
			     struct txq *txq,
			     struct tso_info *tinfo)
{
	struct mlx4_sq *sq = &txq->msq;
	const uint8_t tunneled = txq->priv->hw_csum_l2tun &&
				 (buf->ol_flags & PKT_TX_TUNNEL_MASK);

	tinfo->tso_header_size = buf->l2_len + buf->l3_len + buf->l4_len;
	if (tunneled)
		tinfo->tso_header_size +=
				buf->outer_l2_len + buf->outer_l3_len;
	if (unlikely(buf->tso_segsz == 0 ||
		     tinfo->tso_header_size == 0 ||
		     tinfo->tso_header_size > MLX4_MAX_TSO_HEADER ||
		     tinfo->tso_header_size > buf->data_len))
		return -EINVAL;
	/*
	 * Calculate the WQE TSO segment size
	 * Note:
	 * 1. An LSO segment must be padded such that the subsequent data
	 *    segment is 16-byte aligned.
	 * 2. The start address of the TSO segment is always 16 Bytes aligned.
	 */
	tinfo->wqe_tso_seg_size = RTE_ALIGN(sizeof(struct mlx4_wqe_lso_seg) +
					    tinfo->tso_header_size,
					    sizeof(struct mlx4_wqe_data_seg));
	tinfo->fence_size = ((sizeof(struct mlx4_wqe_ctrl_seg) +
			     tinfo->wqe_tso_seg_size) >> MLX4_SEG_SHIFT) +
			     buf->nb_segs;
	tinfo->wqe_size =
		RTE_ALIGN((uint32_t)(tinfo->fence_size << MLX4_SEG_SHIFT),
			  MLX4_TXBB_SIZE);
	/* Validate WQE size and WQE space in the send queue. */
	if (sq->remain_size < tinfo->wqe_size ||
	    tinfo->wqe_size > MLX4_MAX_WQE_SIZE)
		return -ENOMEM;
	/* Init pv. */
	tinfo->pv = (struct pv *)txq->bounce_buf;
	tinfo->pv_counter = 0;
	return 0;
}

/**
 * Fill the TSO WQE data segments with info on buffers to transmit .
 *
 * @param buf
 *   Pointer to the first packet mbuf.
 * @param txq
 *   Pointer to Tx queue structure.
 * @param tinfo
 *   Pointer to TSO info to use.
 * @param dseg
 *   Pointer to the first data segment in the TSO WQE.
 * @param ctrl
 *   Pointer to the control segment in the TSO WQE.
 *
 * @return
 *   0 on success, negative value upon error.
 */
static inline volatile struct mlx4_wqe_ctrl_seg *
mlx4_tx_burst_fill_tso_dsegs(struct rte_mbuf *buf,
			     struct txq *txq,
			     struct tso_info *tinfo,
			     volatile struct mlx4_wqe_data_seg *dseg,
			     volatile struct mlx4_wqe_ctrl_seg *ctrl)
{
	uint32_t lkey;
	int nb_segs = buf->nb_segs;
	int nb_segs_txbb;
	struct mlx4_sq *sq = &txq->msq;
	struct rte_mbuf *sbuf = buf;
	struct pv *pv = tinfo->pv;
	int *pv_counter = &tinfo->pv_counter;
	volatile struct mlx4_wqe_ctrl_seg *ctrl_next =
			(volatile struct mlx4_wqe_ctrl_seg *)
				((volatile uint8_t *)ctrl + tinfo->wqe_size);
	uint16_t data_len = sbuf->data_len - tinfo->tso_header_size;
	uintptr_t data_addr = rte_pktmbuf_mtod_offset(sbuf, uintptr_t,
						      tinfo->tso_header_size);

	do {
		/* how many dseg entries do we have in the current TXBB ? */
		nb_segs_txbb = (MLX4_TXBB_SIZE -
				((uintptr_t)dseg & (MLX4_TXBB_SIZE - 1))) >>
			       MLX4_SEG_SHIFT;
		switch (nb_segs_txbb) {
#ifdef RTE_LIBRTE_MLX4_DEBUG
		default:
			/* Should never happen. */
			rte_panic("%p: Invalid number of SGEs(%d) for a TXBB",
			(void *)txq, nb_segs_txbb);
			/* rte_panic never returns. */
			break;
#endif /* RTE_LIBRTE_MLX4_DEBUG */
		case 4:
			/* Memory region key for this memory pool. */
			lkey = mlx4_tx_mb2mr(txq, sbuf);
			if (unlikely(lkey == (uint32_t)-1))
				goto err;
			dseg->addr = rte_cpu_to_be_64(data_addr);
			dseg->lkey = lkey;
			/*
			 * This data segment starts at the beginning of a new
			 * TXBB, so we need to postpone its byte_count writing
			 * for later.
			 */
			pv[*pv_counter].dseg = dseg;
			/*
			 * Zero length segment is treated as inline segment
			 * with zero data.
			 */
			pv[(*pv_counter)++].val =
				rte_cpu_to_be_32(data_len ?
						 data_len :
						 0x80000000);
			if (--nb_segs == 0)
				return ctrl_next;
			/* Prepare next buf info */
			sbuf = sbuf->next;
			dseg++;
			data_len = sbuf->data_len;
			data_addr = rte_pktmbuf_mtod(sbuf, uintptr_t);
			/* fallthrough */
		case 3:
			lkey = mlx4_tx_mb2mr(txq, sbuf);
			if (unlikely(lkey == (uint32_t)-1))
				goto err;
			mlx4_fill_tx_data_seg(dseg, lkey, data_addr,
					rte_cpu_to_be_32(data_len ?
							 data_len :
							 0x80000000));
			if (--nb_segs == 0)
				return ctrl_next;
			/* Prepare next buf info */
			sbuf = sbuf->next;
			dseg++;
			data_len = sbuf->data_len;
			data_addr = rte_pktmbuf_mtod(sbuf, uintptr_t);
			/* fallthrough */
		case 2:
			lkey = mlx4_tx_mb2mr(txq, sbuf);
			if (unlikely(lkey == (uint32_t)-1))
				goto err;
			mlx4_fill_tx_data_seg(dseg, lkey, data_addr,
					rte_cpu_to_be_32(data_len ?
							 data_len :
							 0x80000000));
			if (--nb_segs == 0)
				return ctrl_next;
			/* Prepare next buf info */
			sbuf = sbuf->next;
			dseg++;
			data_len = sbuf->data_len;
			data_addr = rte_pktmbuf_mtod(sbuf, uintptr_t);
			/* fallthrough */
		case 1:
			lkey = mlx4_tx_mb2mr(txq, sbuf);
			if (unlikely(lkey == (uint32_t)-1))
				goto err;
			mlx4_fill_tx_data_seg(dseg, lkey, data_addr,
					rte_cpu_to_be_32(data_len ?
							 data_len :
							 0x80000000));
			if (--nb_segs == 0)
				return ctrl_next;
			/* Prepare next buf info */
			sbuf = sbuf->next;
			dseg++;
			data_len = sbuf->data_len;
			data_addr = rte_pktmbuf_mtod(sbuf, uintptr_t);
			/* fallthrough */
		}
		/* Wrap dseg if it points at the end of the queue. */
		if ((volatile uint8_t *)dseg >= sq->eob)
			dseg = (volatile struct mlx4_wqe_data_seg *)
					((volatile uint8_t *)dseg - sq->size);
	} while (true);
err:
	return NULL;
}

/**
 * Fill the packet's l2, l3 and l4 headers to the WQE.
 *
 * This will be used as the header for each TSO segment that is transmitted.
 *
 * @param buf
 *   Pointer to the first packet mbuf.
 * @param txq
 *   Pointer to Tx queue structure.
 * @param tinfo
 *   Pointer to TSO info to use.
 * @param ctrl
 *   Pointer to the control segment in the TSO WQE.
 *
 * @return
 *   0 on success, negative value upon error.
 */
static inline volatile struct mlx4_wqe_data_seg *
mlx4_tx_burst_fill_tso_hdr(struct rte_mbuf *buf,
			   struct txq *txq,
			   struct tso_info *tinfo,
			   volatile struct mlx4_wqe_ctrl_seg *ctrl)
{
	volatile struct mlx4_wqe_lso_seg *tseg =
		(volatile struct mlx4_wqe_lso_seg *)(ctrl + 1);
	struct mlx4_sq *sq = &txq->msq;
	struct pv *pv = tinfo->pv;
	int *pv_counter = &tinfo->pv_counter;
	int remain_size = tinfo->tso_header_size;
	char *from = rte_pktmbuf_mtod(buf, char *);
	uint16_t txbb_avail_space;
	/* Union to overcome volatile constraints when copying TSO header. */
	union {
		volatile uint8_t *vto;
		uint8_t *to;
	} thdr = { .vto = (volatile uint8_t *)tseg->header, };

	/*
	 * TSO data always starts at offset 20 from the beginning of the TXBB
	 * (16 byte ctrl + 4byte TSO desc). Since each TXBB is 64Byte aligned
	 * we can write the first 44 TSO header bytes without worry for TxQ
	 * wrapping or overwriting the first TXBB 32bit word.
	 */
	txbb_avail_space = MLX4_TXBB_SIZE -
			   (sizeof(struct mlx4_wqe_ctrl_seg) +
			    sizeof(struct mlx4_wqe_lso_seg));
	while (remain_size >= (int)(txbb_avail_space + sizeof(uint32_t))) {
		/* Copy to end of txbb. */
		rte_memcpy(thdr.to, from, txbb_avail_space);
		from += txbb_avail_space;
		thdr.to += txbb_avail_space;
		/* New TXBB, Check for TxQ wrap. */
		if (thdr.to >= sq->eob)
			thdr.vto = sq->buf;
		/* New TXBB, stash the first 32bits for later use. */
		pv[*pv_counter].dst = (volatile uint32_t *)thdr.to;
		pv[(*pv_counter)++].val = *(uint32_t *)from,
		from += sizeof(uint32_t);
		thdr.to += sizeof(uint32_t);
		remain_size -= txbb_avail_space + sizeof(uint32_t);
		/* Avail space in new TXBB is TXBB size - 4 */
		txbb_avail_space = MLX4_TXBB_SIZE - sizeof(uint32_t);
	}
	if (remain_size > txbb_avail_space) {
		rte_memcpy(thdr.to, from, txbb_avail_space);
		from += txbb_avail_space;
		thdr.to += txbb_avail_space;
		remain_size -= txbb_avail_space;
		/* New TXBB, Check for TxQ wrap. */
		if (thdr.to >= sq->eob)
			thdr.vto = sq->buf;
		pv[*pv_counter].dst = (volatile uint32_t *)thdr.to;
		rte_memcpy(&pv[*pv_counter].val, from, remain_size);
		(*pv_counter)++;
	} else if (remain_size) {
		rte_memcpy(thdr.to, from, remain_size);
	}
	tseg->mss_hdr_size = rte_cpu_to_be_32((buf->tso_segsz << 16) |
					      tinfo->tso_header_size);
	/* Calculate data segment location */
	return (volatile struct mlx4_wqe_data_seg *)
				((uintptr_t)tseg + tinfo->wqe_tso_seg_size);
}

/**
 * Write data segments and header for TSO uni/multi segment packet.
 *
 * @param buf
 *   Pointer to the first packet mbuf.
 * @param txq
 *   Pointer to Tx queue structure.
 * @param ctrl
 *   Pointer to the WQE control segment.
 *
 * @return
 *   Pointer to the next WQE control segment on success, NULL otherwise.
 */
static volatile struct mlx4_wqe_ctrl_seg *
mlx4_tx_burst_tso(struct rte_mbuf *buf, struct txq *txq,
		  volatile struct mlx4_wqe_ctrl_seg *ctrl)
{
	volatile struct mlx4_wqe_data_seg *dseg;
	volatile struct mlx4_wqe_ctrl_seg *ctrl_next;
	struct mlx4_sq *sq = &txq->msq;
	struct tso_info tinfo;
	struct pv *pv;
	int pv_counter;
	int ret;

	ret = mlx4_tx_burst_tso_get_params(buf, txq, &tinfo);
	if (unlikely(ret))
		goto error;
	dseg = mlx4_tx_burst_fill_tso_hdr(buf, txq, &tinfo, ctrl);
	if (unlikely(dseg == NULL))
		goto error;
	if ((uintptr_t)dseg >= (uintptr_t)sq->eob)
		dseg = (volatile struct mlx4_wqe_data_seg *)
					((uintptr_t)dseg - sq->size);
	ctrl_next = mlx4_tx_burst_fill_tso_dsegs(buf, txq, &tinfo, dseg, ctrl);
	if (unlikely(ctrl_next == NULL))
		goto error;
	/* Write the first DWORD of each TXBB save earlier. */
	if (likely(tinfo.pv_counter)) {
		pv = tinfo.pv;
		pv_counter = tinfo.pv_counter;
		/* Need a barrier here before writing the first TXBB word. */
		rte_io_wmb();
		do {
			--pv_counter;
			*pv[pv_counter].dst = pv[pv_counter].val;
		} while (pv_counter > 0);
	}
	ctrl->fence_size = tinfo.fence_size;
	sq->remain_size -= tinfo.wqe_size;
	return ctrl_next;
error:
	txq->stats.odropped++;
	return NULL;
}

/**
 * Write data segments of multi-segment packet.
 *
 * @param buf
 *   Pointer to the first packet mbuf.
 * @param txq
 *   Pointer to Tx queue structure.
 * @param ctrl
 *   Pointer to the WQE control segment.
 *
 * @return
 *   Pointer to the next WQE control segment on success, NULL otherwise.
 */
static volatile struct mlx4_wqe_ctrl_seg *
mlx4_tx_burst_segs(struct rte_mbuf *buf, struct txq *txq,
		   volatile struct mlx4_wqe_ctrl_seg *ctrl)
{
	struct pv *pv = (struct pv *)txq->bounce_buf;
	struct mlx4_sq *sq = &txq->msq;
	struct rte_mbuf *sbuf = buf;
	uint32_t lkey;
	int pv_counter = 0;
	int nb_segs = buf->nb_segs;
	uint32_t wqe_size;
	volatile struct mlx4_wqe_data_seg *dseg =
		(volatile struct mlx4_wqe_data_seg *)(ctrl + 1);

	ctrl->fence_size = 1 + nb_segs;
	wqe_size = RTE_ALIGN((uint32_t)(ctrl->fence_size << MLX4_SEG_SHIFT),
			     MLX4_TXBB_SIZE);
	/* Validate WQE size and WQE space in the send queue. */
	if (sq->remain_size < wqe_size ||
	    wqe_size > MLX4_MAX_WQE_SIZE)
		return NULL;
	/*
	 * Fill the data segments with buffer information.
	 * First WQE TXBB head segment is always control segment,
	 * so jump to tail TXBB data segments code for the first
	 * WQE data segments filling.
	 */
	goto txbb_tail_segs;
txbb_head_seg:
	/* Memory region key (big endian) for this memory pool. */
	lkey = mlx4_tx_mb2mr(txq, sbuf);
	if (unlikely(lkey == (uint32_t)-1)) {
		DEBUG("%p: unable to get MP <-> MR association",
		      (void *)txq);
		return NULL;
	}
	/* Handle WQE wraparound. */
	if (dseg >=
		(volatile struct mlx4_wqe_data_seg *)sq->eob)
		dseg = (volatile struct mlx4_wqe_data_seg *)
			sq->buf;
	dseg->addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(sbuf, uintptr_t));
	dseg->lkey = lkey;
	/*
	 * This data segment starts at the beginning of a new
	 * TXBB, so we need to postpone its byte_count writing
	 * for later.
	 */
	pv[pv_counter].dseg = dseg;
	/*
	 * Zero length segment is treated as inline segment
	 * with zero data.
	 */
	pv[pv_counter++].val = rte_cpu_to_be_32(sbuf->data_len ?
						sbuf->data_len : 0x80000000);
	sbuf = sbuf->next;
	dseg++;
	nb_segs--;
txbb_tail_segs:
	/* Jump to default if there are more than two segments remaining. */
	switch (nb_segs) {
	default:
		lkey = mlx4_tx_mb2mr(txq, sbuf);
		if (unlikely(lkey == (uint32_t)-1)) {
			DEBUG("%p: unable to get MP <-> MR association",
			      (void *)txq);
			return NULL;
		}
		mlx4_fill_tx_data_seg(dseg, lkey,
				      rte_pktmbuf_mtod(sbuf, uintptr_t),
				      rte_cpu_to_be_32(sbuf->data_len ?
						       sbuf->data_len :
						       0x80000000));
		sbuf = sbuf->next;
		dseg++;
		nb_segs--;
		/* fallthrough */
	case 2:
		lkey = mlx4_tx_mb2mr(txq, sbuf);
		if (unlikely(lkey == (uint32_t)-1)) {
			DEBUG("%p: unable to get MP <-> MR association",
			      (void *)txq);
			return NULL;
		}
		mlx4_fill_tx_data_seg(dseg, lkey,
				      rte_pktmbuf_mtod(sbuf, uintptr_t),
				      rte_cpu_to_be_32(sbuf->data_len ?
						       sbuf->data_len :
						       0x80000000));
		sbuf = sbuf->next;
		dseg++;
		nb_segs--;
		/* fallthrough */
	case 1:
		lkey = mlx4_tx_mb2mr(txq, sbuf);
		if (unlikely(lkey == (uint32_t)-1)) {
			DEBUG("%p: unable to get MP <-> MR association",
			      (void *)txq);
			return NULL;
		}
		mlx4_fill_tx_data_seg(dseg, lkey,
				      rte_pktmbuf_mtod(sbuf, uintptr_t),
				      rte_cpu_to_be_32(sbuf->data_len ?
						       sbuf->data_len :
						       0x80000000));
		nb_segs--;
		if (nb_segs) {
			sbuf = sbuf->next;
			dseg++;
			goto txbb_head_seg;
		}
		/* fallthrough */
	case 0:
		break;
	}
	/* Write the first DWORD of each TXBB save earlier. */
	if (pv_counter) {
		/* Need a barrier here before writing the byte_count. */
		rte_io_wmb();
		for (--pv_counter; pv_counter  >= 0; pv_counter--)
			pv[pv_counter].dseg->byte_count = pv[pv_counter].val;
	}
	sq->remain_size -= wqe_size;
	/* Align next WQE address to the next TXBB. */
	return (volatile struct mlx4_wqe_ctrl_seg *)
		((volatile uint8_t *)ctrl + wqe_size);
}

/**
 * DPDK callback for Tx.
 *
 * @param dpdk_txq
 *   Generic pointer to Tx queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx4_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	unsigned int elts_head = txq->elts_head;
	const unsigned int elts_n = txq->elts_n;
	const unsigned int elts_m = elts_n - 1;
	unsigned int bytes_sent = 0;
	unsigned int i;
	unsigned int max = elts_head - txq->elts_tail;
	struct mlx4_sq *sq = &txq->msq;
	volatile struct mlx4_wqe_ctrl_seg *ctrl;
	struct txq_elt *elt;

	MLX4_ASSERT(txq->elts_comp_cd != 0);
	if (likely(max >= txq->elts_comp_cd_init))
		mlx4_txq_complete(txq, elts_m, sq);
	max = elts_n - max;
	MLX4_ASSERT(max >= 1);
	MLX4_ASSERT(max <= elts_n);
	/* Always leave one free entry in the ring. */
	--max;
	if (max > pkts_n)
		max = pkts_n;
	elt = &(*txq->elts)[elts_head & elts_m];
	/* First Tx burst element saves the next WQE control segment. */
	ctrl = elt->wqe;
	for (i = 0; (i != max); ++i) {
		struct rte_mbuf *buf = pkts[i];
		struct txq_elt *elt_next = &(*txq->elts)[++elts_head & elts_m];
		uint32_t owner_opcode = sq->owner_opcode;
		volatile struct mlx4_wqe_data_seg *dseg =
				(volatile struct mlx4_wqe_data_seg *)(ctrl + 1);
		volatile struct mlx4_wqe_ctrl_seg *ctrl_next;
		union {
			uint32_t flags;
			uint16_t flags16[2];
		} srcrb;
		uint32_t lkey;
		bool tso = txq->priv->tso && (buf->ol_flags & PKT_TX_TCP_SEG);

		/* Clean up old buffer. */
		if (likely(elt->buf != NULL)) {
			struct rte_mbuf *tmp = elt->buf;

#ifdef RTE_LIBRTE_MLX4_DEBUG
			/* Poisoning. */
			memset(&elt->buf, 0x66, sizeof(struct rte_mbuf *));
#endif
			/* Faster than rte_pktmbuf_free(). */
			do {
				struct rte_mbuf *next = tmp->next;

				rte_pktmbuf_free_seg(tmp);
				tmp = next;
			} while (tmp != NULL);
		}
		RTE_MBUF_PREFETCH_TO_FREE(elt_next->buf);
		if (tso) {
			/* Change opcode to TSO */
			owner_opcode &= ~MLX4_OPCODE_CONFIG_CMD;
			owner_opcode |= MLX4_OPCODE_LSO | MLX4_WQE_CTRL_RR;
			ctrl_next = mlx4_tx_burst_tso(buf, txq, ctrl);
			if (!ctrl_next) {
				elt->buf = NULL;
				break;
			}
		} else if (buf->nb_segs == 1) {
			/* Validate WQE space in the send queue. */
			if (sq->remain_size < MLX4_TXBB_SIZE) {
				elt->buf = NULL;
				break;
			}
			lkey = mlx4_tx_mb2mr(txq, buf);
			if (unlikely(lkey == (uint32_t)-1)) {
				/* MR does not exist. */
				DEBUG("%p: unable to get MP <-> MR association",
				      (void *)txq);
				elt->buf = NULL;
				break;
			}
			mlx4_fill_tx_data_seg(dseg++, lkey,
					      rte_pktmbuf_mtod(buf, uintptr_t),
					      rte_cpu_to_be_32(buf->data_len));
			/* Set WQE size in 16-byte units. */
			ctrl->fence_size = 0x2;
			sq->remain_size -= MLX4_TXBB_SIZE;
			/* Align next WQE address to the next TXBB. */
			ctrl_next = ctrl + 0x4;
		} else {
			ctrl_next = mlx4_tx_burst_segs(buf, txq, ctrl);
			if (!ctrl_next) {
				elt->buf = NULL;
				break;
			}
		}
		/* Hold SQ ring wrap around. */
		if ((volatile uint8_t *)ctrl_next >= sq->eob) {
			ctrl_next = (volatile struct mlx4_wqe_ctrl_seg *)
				((volatile uint8_t *)ctrl_next - sq->size);
			/* Flip HW valid ownership. */
			sq->owner_opcode ^= 1u << MLX4_SQ_OWNER_BIT;
		}
		/*
		 * For raw Ethernet, the SOLICIT flag is used to indicate
		 * that no ICRC should be calculated.
		 */
		if (--txq->elts_comp_cd == 0) {
			/* Save the completion burst end address. */
			elt_next->eocb = (volatile uint32_t *)ctrl_next;
			txq->elts_comp_cd = txq->elts_comp_cd_init;
			srcrb.flags = RTE_BE32(MLX4_WQE_CTRL_SOLICIT |
					       MLX4_WQE_CTRL_CQ_UPDATE);
		} else {
			srcrb.flags = RTE_BE32(MLX4_WQE_CTRL_SOLICIT);
		}
		/* Enable HW checksum offload if requested */
		if (txq->csum &&
		    (buf->ol_flags &
		     (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM))) {
			const uint64_t is_tunneled = (buf->ol_flags &
						      (PKT_TX_TUNNEL_GRE |
						       PKT_TX_TUNNEL_VXLAN));

			if (is_tunneled && txq->csum_l2tun) {
				owner_opcode |= MLX4_WQE_CTRL_IIP_HDR_CSUM |
						MLX4_WQE_CTRL_IL4_HDR_CSUM;
				if (buf->ol_flags & PKT_TX_OUTER_IP_CKSUM)
					srcrb.flags |=
					    RTE_BE32(MLX4_WQE_CTRL_IP_HDR_CSUM);
			} else {
				srcrb.flags |=
					RTE_BE32(MLX4_WQE_CTRL_IP_HDR_CSUM |
						MLX4_WQE_CTRL_TCP_UDP_CSUM);
			}
		}
		if (txq->lb) {
			/*
			 * Copy destination MAC address to the WQE, this allows
			 * loopback in eSwitch, so that VFs and PF can
			 * communicate with each other.
			 */
			srcrb.flags16[0] = *(rte_pktmbuf_mtod(buf, uint16_t *));
			ctrl->imm = *(rte_pktmbuf_mtod_offset(buf, uint32_t *,
					      sizeof(uint16_t)));
		} else {
			ctrl->imm = 0;
		}
		ctrl->srcrb_flags = srcrb.flags;
		/*
		 * Make sure descriptor is fully written before
		 * setting ownership bit (because HW can start
		 * executing as soon as we do).
		 */
		rte_io_wmb();
		ctrl->owner_opcode = rte_cpu_to_be_32(owner_opcode);
		elt->buf = buf;
		bytes_sent += buf->pkt_len;
		ctrl = ctrl_next;
		elt = elt_next;
	}
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Save WQE address of the next Tx burst element. */
	elt->wqe = ctrl;
	/* Increment send statistics counters. */
	txq->stats.opackets += i;
	txq->stats.obytes += bytes_sent;
	/* Make sure that descriptors are written before doorbell record. */
	rte_wmb();
	/* Ring QP doorbell. */
	rte_write32(txq->msq.doorbell_qpn, MLX4_TX_BFREG(txq));
	txq->elts_head += i;
	return i;
}

/**
 * Translate Rx completion flags to packet type.
 *
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @return
 *   Packet type for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_pkt_type(volatile struct mlx4_cqe *cqe,
		   uint32_t l2tun_offload)
{
	uint8_t idx = 0;
	uint32_t pinfo = rte_be_to_cpu_32(cqe->vlan_my_qpn);
	uint32_t status = rte_be_to_cpu_32(cqe->status);

	/*
	 * The index to the array should have:
	 *  bit[7] - MLX4_CQE_L2_TUNNEL
	 *  bit[6] - MLX4_CQE_L2_TUNNEL_IPV4
	 */
	if (l2tun_offload && (pinfo & MLX4_CQE_L2_TUNNEL))
		idx |= ((pinfo & MLX4_CQE_L2_TUNNEL) >> 20) |
		       ((pinfo & MLX4_CQE_L2_TUNNEL_IPV4) >> 19);
	/*
	 * The index to the array should have:
	 *  bit[5] - MLX4_CQE_STATUS_UDP
	 *  bit[4] - MLX4_CQE_STATUS_TCP
	 *  bit[3] - MLX4_CQE_STATUS_IPV4OPT
	 *  bit[2] - MLX4_CQE_STATUS_IPV6
	 *  bit[1] - MLX4_CQE_STATUS_IPF
	 *  bit[0] - MLX4_CQE_STATUS_IPV4
	 * giving a total of up to 256 entries.
	 */
	idx |= ((status & MLX4_CQE_STATUS_PTYPE_MASK) >> 22);
	if (status & MLX4_CQE_STATUS_IPV6)
		idx |= ((status & MLX4_CQE_STATUS_IPV6F) >> 11);
	return mlx4_ptype_table[idx];
}

/**
 * Translate Rx completion flags to offload flags.
 *
 * @param flags
 *   Rx completion flags returned by mlx4_cqe_flags().
 * @param csum
 *   Whether Rx checksums are enabled.
 * @param csum_l2tun
 *   Whether Rx L2 tunnel checksums are enabled.
 *
 * @return
 *   Offload flags (ol_flags) in mbuf format.
 */
static inline uint32_t
rxq_cq_to_ol_flags(uint32_t flags, int csum, int csum_l2tun)
{
	uint32_t ol_flags = 0;

	if (csum)
		ol_flags |=
			mlx4_transpose(flags,
				       MLX4_CQE_STATUS_IP_HDR_CSUM_OK,
				       PKT_RX_IP_CKSUM_GOOD) |
			mlx4_transpose(flags,
				       MLX4_CQE_STATUS_TCP_UDP_CSUM_OK,
				       PKT_RX_L4_CKSUM_GOOD);
	if ((flags & MLX4_CQE_L2_TUNNEL) && csum_l2tun)
		ol_flags |=
			mlx4_transpose(flags,
				       MLX4_CQE_L2_TUNNEL_IPOK,
				       PKT_RX_IP_CKSUM_GOOD) |
			mlx4_transpose(flags,
				       MLX4_CQE_L2_TUNNEL_L4_CSUM,
				       PKT_RX_L4_CKSUM_GOOD);
	return ol_flags;
}

/**
 * Extract checksum information from CQE flags.
 *
 * @param cqe
 *   Pointer to CQE structure.
 * @param csum
 *   Whether Rx checksums are enabled.
 * @param csum_l2tun
 *   Whether Rx L2 tunnel checksums are enabled.
 *
 * @return
 *   CQE checksum information.
 */
static inline uint32_t
mlx4_cqe_flags(volatile struct mlx4_cqe *cqe, int csum, int csum_l2tun)
{
	uint32_t flags = 0;

	/*
	 * The relevant bits are in different locations on their
	 * CQE fields therefore we can join them in one 32bit
	 * variable.
	 */
	if (csum)
		flags = (rte_be_to_cpu_32(cqe->status) &
			 MLX4_CQE_STATUS_IPV4_CSUM_OK);
	if (csum_l2tun)
		flags |= (rte_be_to_cpu_32(cqe->vlan_my_qpn) &
			  (MLX4_CQE_L2_TUNNEL |
			   MLX4_CQE_L2_TUNNEL_IPOK |
			   MLX4_CQE_L2_TUNNEL_L4_CSUM |
			   MLX4_CQE_L2_TUNNEL_IPV4));
	return flags;
}

/**
 * Poll one CQE from CQ.
 *
 * @param rxq
 *   Pointer to the receive queue structure.
 * @param[out] out
 *   Just polled CQE.
 *
 * @return
 *   Number of bytes of the CQE, 0 in case there is no completion.
 */
static unsigned int
mlx4_cq_poll_one(struct rxq *rxq, volatile struct mlx4_cqe **out)
{
	int ret = 0;
	volatile struct mlx4_cqe *cqe = NULL;
	struct mlx4_cq *cq = &rxq->mcq;

	cqe = (volatile struct mlx4_cqe *)mlx4_get_cqe(cq, cq->cons_index);
	if (!!(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK) ^
	    !!(cq->cons_index & cq->cqe_cnt))
		goto out;
	/*
	 * Make sure we read CQ entry contents after we've checked the
	 * ownership bit.
	 */
	rte_rmb();
	MLX4_ASSERT(!(cqe->owner_sr_opcode & MLX4_CQE_IS_SEND_MASK));
	MLX4_ASSERT((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) !=
		    MLX4_CQE_OPCODE_ERROR);
	ret = rte_be_to_cpu_32(cqe->byte_cnt);
	++cq->cons_index;
out:
	*out = cqe;
	return ret;
}

/**
 * DPDK callback for Rx with scattered packets support.
 *
 * @param dpdk_rxq
 *   Generic pointer to Rx queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx4_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = dpdk_rxq;
	const uint32_t wr_cnt = (1 << rxq->elts_n) - 1;
	const uint16_t sges_n = rxq->sges_n;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf *seg = NULL;
	unsigned int i = 0;
	uint32_t rq_ci = rxq->rq_ci << sges_n;
	int len = 0;

	while (pkts_n) {
		volatile struct mlx4_cqe *cqe;
		uint32_t idx = rq_ci & wr_cnt;
		struct rte_mbuf *rep = (*rxq->elts)[idx];
		volatile struct mlx4_wqe_data_seg *scat = &(*rxq->wqes)[idx];

		/* Update the 'next' pointer of the previous segment. */
		if (pkt)
			seg->next = rep;
		seg = rep;
		rte_prefetch0(seg);
		rte_prefetch0(scat);
		rep = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(rep == NULL)) {
			++rxq->stats.rx_nombuf;
			if (!pkt) {
				/*
				 * No buffers before we even started,
				 * bail out silently.
				 */
				break;
			}
			while (pkt != seg) {
				MLX4_ASSERT(pkt != (*rxq->elts)[idx]);
				rep = pkt->next;
				pkt->next = NULL;
				pkt->nb_segs = 1;
				rte_mbuf_raw_free(pkt);
				pkt = rep;
			}
			break;
		}
		if (!pkt) {
			/* Looking for the new packet. */
			len = mlx4_cq_poll_one(rxq, &cqe);
			if (!len) {
				rte_mbuf_raw_free(rep);
				break;
			}
			if (unlikely(len < 0)) {
				/* Rx error, packet is likely too large. */
				rte_mbuf_raw_free(rep);
				++rxq->stats.idropped;
				goto skip;
			}
			pkt = seg;
			MLX4_ASSERT(len >= (rxq->crc_present << 2));
			/* Update packet information. */
			pkt->packet_type =
				rxq_cq_to_pkt_type(cqe, rxq->l2tun_offload);
			pkt->ol_flags = PKT_RX_RSS_HASH;
			pkt->hash.rss = cqe->immed_rss_invalid;
			if (rxq->crc_present)
				len -= RTE_ETHER_CRC_LEN;
			pkt->pkt_len = len;
			if (rxq->csum | rxq->csum_l2tun) {
				uint32_t flags =
					mlx4_cqe_flags(cqe,
						       rxq->csum,
						       rxq->csum_l2tun);

				pkt->ol_flags =
					rxq_cq_to_ol_flags(flags,
							   rxq->csum,
							   rxq->csum_l2tun);
			}
		}
		rep->nb_segs = 1;
		rep->port = rxq->port_id;
		rep->data_len = seg->data_len;
		rep->data_off = seg->data_off;
		(*rxq->elts)[idx] = rep;
		/*
		 * Fill NIC descriptor with the new buffer. The lkey and size
		 * of the buffers are already known, only the buffer address
		 * changes.
		 */
		scat->addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(rep, uintptr_t));
		/* If there's only one MR, no need to replace LKey in WQE. */
		if (unlikely(mlx4_mr_btree_len(&rxq->mr_ctrl.cache_bh) > 1))
			scat->lkey = mlx4_rx_mb2mr(rxq, rep);
		if (len > seg->data_len) {
			len -= seg->data_len;
			++pkt->nb_segs;
			++rq_ci;
			continue;
		}
		/* The last segment. */
		seg->data_len = len;
		/* Increment bytes counter. */
		rxq->stats.ibytes += pkt->pkt_len;
		/* Return packet. */
		*(pkts++) = pkt;
		pkt = NULL;
		--pkts_n;
		++i;
skip:
		/* Align consumer index to the next stride. */
		rq_ci >>= sges_n;
		++rq_ci;
		rq_ci <<= sges_n;
	}
	if (unlikely(i == 0 && (rq_ci >> sges_n) == rxq->rq_ci))
		return 0;
	/* Update the consumer index. */
	rxq->rq_ci = rq_ci >> sges_n;
	rte_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
	*rxq->mcq.set_ci_db =
		rte_cpu_to_be_32(rxq->mcq.cons_index & MLX4_CQ_DB_CI_MASK);
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
	return i;
}

/**
 * Dummy DPDK callback for Tx.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_txq
 *   Generic pointer to Tx queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx4_tx_burst_removed(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_txq;
	(void)pkts;
	(void)pkts_n;
	rte_mb();
	return 0;
}

/**
 * Dummy DPDK callback for Rx.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_rxq
 *   Generic pointer to Rx queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx4_rx_burst_removed(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_rxq;
	(void)pkts;
	(void)pkts_n;
	rte_mb();
	return 0;
}
