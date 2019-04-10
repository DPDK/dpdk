/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
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
#include "mlx4_autoconf.h"

/* ConnectX-3 Tx queue basic block. */
#define MLX4_TXBB_SHIFT 6
#define MLX4_TXBB_SIZE (1 << MLX4_TXBB_SHIFT)

/* Typical TSO descriptor with 16 gather entries is 352 bytes. */
#define MLX4_MAX_SGE 32
#define MLX4_MAX_WQE_SIZE \
	(MLX4_MAX_SGE * sizeof(struct mlx4_wqe_data_seg) + \
	 sizeof(struct mlx4_wqe_ctrl_seg))
#define MLX4_SEG_SHIFT 4

/* Send queue stamping/invalidating information. */
#define MLX4_SQ_STAMP_STRIDE 64
#define MLX4_SQ_STAMP_DWORDS (MLX4_SQ_STAMP_STRIDE / 4)
#define MLX4_SQ_OWNER_BIT 31
#define MLX4_SQ_STAMP_VAL 0x7fffffff

/* Work queue element (WQE) flags. */
#define MLX4_WQE_CTRL_IIP_HDR_CSUM (1 << 28)
#define MLX4_WQE_CTRL_IL4_HDR_CSUM (1 << 27)
#define MLX4_WQE_CTRL_RR (1 << 6)

/* CQE checksum flags. */
enum {
	MLX4_CQE_L2_TUNNEL_IPV4 = (int)(1u << 25),
	MLX4_CQE_L2_TUNNEL_L4_CSUM = (int)(1u << 26),
	MLX4_CQE_L2_TUNNEL = (int)(1u << 27),
	MLX4_CQE_L2_VLAN_MASK = (int)(3u << 29),
	MLX4_CQE_L2_TUNNEL_IPOK = (int)(1u << 31),
};

/* CQE status flags. */
#define MLX4_CQE_STATUS_IPV6F (1 << 12)
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
	uint32_t size; /**< SQ size includes headroom. */
	uint32_t remain_size; /**< Remaining WQE room in SQ (bytes). */
	uint32_t owner_opcode;
	/**< Default owner opcode with HW valid owner bit. */
	uint32_t stamp; /**< Stamp value with an invalid HW owner bit. */
	uint32_t *db; /**< Pointer to the doorbell. */
	off_t uar_mmap_offset; /* UAR mmap offset for non-primary process. */
	uint32_t doorbell_qpn; /**< qp number to write to the doorbell. */
};

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

#ifndef HAVE_IBV_MLX4_WQE_LSO_SEG
/*
 * WQE LSO segment structure.
 * Defined here as backward compatibility for rdma-core v17 and below.
 * Similar definition is found in infiniband/mlx4dv.h in rdma-core v18
 * and above.
 */
struct mlx4_wqe_lso_seg {
	rte_be32_t mss_hdr_size;
	rte_be32_t header[];
};
#endif

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
