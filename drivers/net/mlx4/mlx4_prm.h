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

#ifndef MLX4_PRM_H_
#define MLX4_PRM_H_

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx4dv.h>
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* ConnectX-3 Tx queue basic block. */
#define MLX4_TXBB_SHIFT 6
#define MLX4_TXBB_SIZE (1 << MLX4_TXBB_SHIFT)

/* Typical TSO descriptor with 16 gather entries is 352 bytes. */
#define MLX4_MAX_WQE_SIZE 512
#define MLX4_MAX_WQE_TXBBS (MLX4_MAX_WQE_SIZE / MLX4_TXBB_SIZE)

/* Send queue stamping/invalidating information. */
#define MLX4_SQ_STAMP_STRIDE 64
#define MLX4_SQ_STAMP_DWORDS (MLX4_SQ_STAMP_STRIDE / 4)
#define MLX4_SQ_STAMP_SHIFT 31
#define MLX4_SQ_STAMP_VAL 0x7fffffff

/* Work queue element (WQE) flags. */
#define MLX4_BIT_WQE_OWN 0x80000000
#define MLX4_WQE_CTRL_IIP_HDR_CSUM (1 << 28)
#define MLX4_WQE_CTRL_IL4_HDR_CSUM (1 << 27)

#define MLX4_SIZE_TO_TXBBS(size) \
	(RTE_ALIGN((size), (MLX4_TXBB_SIZE)) >> (MLX4_TXBB_SHIFT))

/* CQE checksum flags. */
enum {
	MLX4_CQE_L2_TUNNEL_IPV4 = (int)(1u << 25),
	MLX4_CQE_L2_TUNNEL_L4_CSUM = (int)(1u << 26),
	MLX4_CQE_L2_TUNNEL = (int)(1u << 27),
	MLX4_CQE_L2_VLAN_MASK = (int)(3u << 29),
	MLX4_CQE_L2_TUNNEL_IPOK = (int)(1u << 31),
};

/* CQE status flags. */
#define MLX4_CQE_STATUS_IPV4 (1 << 22)
#define MLX4_CQE_STATUS_IPV4F (1 << 23)
#define MLX4_CQE_STATUS_IPV6 (1 << 24)
#define MLX4_CQE_STATUS_IPV4OPT (1 << 25)
#define MLX4_CQE_STATUS_TCP (1 << 26)
#define MLX4_CQE_STATUS_UDP (1 << 27)
#define MLX4_CQE_STATUS_PTYPE_MASK \
	(MLX4_CQE_STATUS_IPV4 | \
	 MLX4_CQE_STATUS_IPV4F | \
	 MLX4_CQE_STATUS_IPV6 | \
	 MLX4_CQE_STATUS_IPV4OPT | \
	 MLX4_CQE_STATUS_TCP | \
	 MLX4_CQE_STATUS_UDP)

/* Send queue information. */
struct mlx4_sq {
	volatile uint8_t *buf; /**< SQ buffer. */
	volatile uint8_t *eob; /**< End of SQ buffer */
	uint32_t head; /**< SQ head counter in units of TXBBS. */
	uint32_t tail; /**< SQ tail counter in units of TXBBS. */
	uint32_t txbb_cnt; /**< Num of WQEBB in the Q (should be ^2). */
	uint32_t txbb_cnt_mask; /**< txbbs_cnt mask (txbb_cnt is ^2). */
	uint32_t headroom_txbbs; /**< Num of txbbs that should be kept free. */
	volatile uint32_t *db; /**< Pointer to the doorbell. */
	uint32_t doorbell_qpn; /**< qp number to write to the doorbell. */
};

#define mlx4_get_send_wqe(sq, n) ((sq)->buf + ((n) * (MLX4_TXBB_SIZE)))

/* Completion queue events, numbers and masks. */
#define MLX4_CQ_DB_GEQ_N_MASK 0x3
#define MLX4_CQ_DOORBELL 0x20
#define MLX4_CQ_DB_CI_MASK 0xffffff

/* Completion queue information. */
struct mlx4_cq {
	volatile void *cq_uar; /**< CQ user access region. */
	volatile void *cq_db_reg; /**< CQ doorbell register. */
	volatile uint32_t *set_ci_db; /**< Pointer to the CQ doorbell. */
	volatile uint32_t *arm_db; /**< Arming Rx events doorbell. */
	volatile uint8_t *buf; /**< Pointer to the completion queue buffer. */
	uint32_t cqe_cnt; /**< Number of entries in the queue. */
	uint32_t cqe_64:1; /**< CQ entry size is 64 bytes. */
	uint32_t cons_index; /**< Last queue entry that was handled. */
	uint32_t cqn; /**< CQ number. */
	int arm_sn; /**< Rx event counter. */
};

/**
 * Retrieve a CQE entry from a CQ.
 *
 * cqe = cq->buf + cons_index * cqe_size + cqe_offset
 *
 * Where cqe_size is 32 or 64 bytes and cqe_offset is 0 or 32 (depending on
 * cqe_size).
 *
 * @param cq
 *   CQ to retrieve entry from.
 * @param index
 *   Entry index.
 *
 * @return
 *   Pointer to CQE entry.
 */
static inline volatile struct mlx4_cqe *
mlx4_get_cqe(struct mlx4_cq *cq, uint32_t index)
{
	return (volatile struct mlx4_cqe *)(cq->buf +
				   ((index & (cq->cqe_cnt - 1)) <<
				    (5 + cq->cqe_64)) +
				   (cq->cqe_64 << 5));
}

/**
 * Transpose a flag in a value.
 *
 * @param val
 *   Input value.
 * @param from
 *   Flag to retrieve from input value.
 * @param to
 *   Flag to set in output value.
 *
 * @return
 *   Output value with transposed flag enabled if present on input.
 */
static inline uint64_t
mlx4_transpose(uint64_t val, uint64_t from, uint64_t to)
{
	return (from >= to ?
		(val & from) / (from / to) :
		(val & from) * (to / from));
}

#endif /* MLX4_PRM_H_ */
