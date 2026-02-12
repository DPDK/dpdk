/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "base/hinic3_compat.h"
#include "base/hinic3_cmd.h"
#include "base/hinic3_cmdq.h"
#include "base/hinic3_hw_comm.h"
#include "base/hinic3_nic_cfg.h"
#include "hinic3_nic_io.h"
#include "hinic3_rx.h"
#include "hinic3_tx.h"

#define HINIC3_DEAULT_TX_CI_PENDING_LIMIT   3
#define HINIC3_DEAULT_TX_CI_COALESCING_TIME 16
#define HINIC3_DEAULT_DROP_THD_ON	    0xFFFF
#define HINIC3_DEAULT_DROP_THD_OFF	    0

#define WQ_PREFETCH_MAX	      6
#define WQ_PREFETCH_MIN	      1
#define WQ_PREFETCH_THRESHOLD 256

#define HINIC3_Q_CTXT_MAX \
	((uint16_t)(((HINIC3_CMDQ_BUF_SIZE - 8) - RTE_PKTMBUF_HEADROOM) / 64))

enum hinic3_qp_ctxt_type {
	HINIC3_QP_CTXT_TYPE_SQ,
	HINIC3_QP_CTXT_TYPE_RQ,
};

struct hinic3_qp_ctxt_header {
	uint16_t num_queues;
	uint16_t queue_type;
	uint16_t start_qid;
	uint16_t rsvd;
};

struct hinic3_sq_ctxt {
	uint32_t ci_pi;
	uint32_t drop_mode_sp;    /**< Packet drop mode and special flags. */
	uint32_t wq_pfn_hi_owner; /**< High PFN and ownership flag. */
	uint32_t wq_pfn_lo;	     /**< Low bits of work queue PFN. */

	uint32_t rsvd0;	  /**< Reserved field 0. */
	uint32_t pkt_drop_thd; /**< Packet drop threshold. */
	uint32_t global_sq_id;
	uint32_t vlan_ceq_attr; /**< VLAN and CEQ attributes. */

	uint32_t pref_cache;	       /**< Cache prefetch settings for the queue. */
	uint32_t pref_ci_owner;     /**< Prefetch settings for CI and ownership. */
	uint32_t pref_wq_pfn_hi_ci; /**< Prefetch settings for high PFN and CI. */
	uint32_t pref_wq_pfn_lo;    /**< Prefetch settings for low PFN. */

	uint32_t rsvd8;	     /**< Reserved field 8. */
	uint32_t rsvd9;	     /**< Reserved field 9. */
	uint32_t wq_block_pfn_hi; /**< High bits of work queue block PFN. */
	uint32_t wq_block_pfn_lo; /**< Low bits of work queue block PFN. */
};

struct hinic3_rq_ctxt {
	uint32_t ci_pi;
	uint32_t ceq_attr;		  /**< Completion event queue attributes. */
	uint32_t wq_pfn_hi_type_owner; /**< High PFN, WQE type and ownership flag. */
	uint32_t wq_pfn_lo;		  /**< Low bits of work queue PFN. */

	uint32_t rsvd[3];	 /**< Reserved field. */
	uint32_t cqe_sge_len; /**< CQE scatter/gather element length. */

	uint32_t pref_cache;	       /**< Cache prefetch settings for the queue. */
	uint32_t pref_ci_owner;     /**< Prefetch settings for CI and ownership. */
	uint32_t pref_wq_pfn_hi_ci; /**< Prefetch settings for high PFN and CI. */
	uint32_t pref_wq_pfn_lo;    /**< Prefetch settings for low PFN. */

	uint32_t pi_paddr_hi;     /**< High 32-bits of PI DMA address. */
	uint32_t pi_paddr_lo;     /**< Low 32-bits of PI DMA address. */
	uint32_t wq_block_pfn_hi; /**< High bits of work queue block PFN. */
	uint32_t wq_block_pfn_lo; /**< Low bits of work queue block PFN. */
};

struct hinic3_sq_ctxt_block {
	struct hinic3_qp_ctxt_header cmdq_hdr;
	struct hinic3_sq_ctxt sq_ctxt[HINIC3_Q_CTXT_MAX];
};

struct hinic3_rq_ctxt_block {
	struct hinic3_qp_ctxt_header cmdq_hdr;
	struct hinic3_rq_ctxt rq_ctxt[HINIC3_Q_CTXT_MAX];
};

struct hinic3_clean_queue_ctxt {
	struct hinic3_qp_ctxt_header cmdq_hdr;
	uint32_t rsvd;
};

#define SQ_CTXT_SIZE(num_sqs)                         \
	((uint16_t)(sizeof(struct hinic3_qp_ctxt_header) + \
	       (num_sqs) * sizeof(struct hinic3_sq_ctxt)))

#define RQ_CTXT_SIZE(num_rqs)                         \
	((uint16_t)(sizeof(struct hinic3_qp_ctxt_header) + \
	       (num_rqs) * sizeof(struct hinic3_rq_ctxt)))

#define CI_IDX_HIGH_SHIFH 12

#define CI_HIGN_IDX(val) ((val) >> CI_IDX_HIGH_SHIFH)

#define SQ_CTXT_PI_IDX_SHIFT 0
#define SQ_CTXT_CI_IDX_SHIFT 16

#define SQ_CTXT_PI_IDX_MASK 0xFFFFU
#define SQ_CTXT_CI_IDX_MASK 0xFFFFU

#define SQ_CTXT_CI_PI_SET(val, member) \
	(((val) & SQ_CTXT_##member##_MASK) << SQ_CTXT_##member##_SHIFT)

#define SQ_CTXT_MODE_SP_FLAG_SHIFT  0
#define SQ_CTXT_MODE_PKT_DROP_SHIFT 1

#define SQ_CTXT_MODE_SP_FLAG_MASK  0x1U
#define SQ_CTXT_MODE_PKT_DROP_MASK 0x1U

#define SQ_CTXT_MODE_SET(val, member)           \
	(((val) & SQ_CTXT_MODE_##member##_MASK) \
	 << SQ_CTXT_MODE_##member##_SHIFT)

#define SQ_CTXT_WQ_PAGE_HI_PFN_SHIFT 0
#define SQ_CTXT_WQ_PAGE_OWNER_SHIFT  23

#define SQ_CTXT_WQ_PAGE_HI_PFN_MASK 0xFFFFFU
#define SQ_CTXT_WQ_PAGE_OWNER_MASK  0x1U

#define SQ_CTXT_WQ_PAGE_SET(val, member)           \
	(((val) & SQ_CTXT_WQ_PAGE_##member##_MASK) \
	 << SQ_CTXT_WQ_PAGE_##member##_SHIFT)

#define SQ_CTXT_PKT_DROP_THD_ON_SHIFT  0
#define SQ_CTXT_PKT_DROP_THD_OFF_SHIFT 16

#define SQ_CTXT_PKT_DROP_THD_ON_MASK  0xFFFFU
#define SQ_CTXT_PKT_DROP_THD_OFF_MASK 0xFFFFU

#define SQ_CTXT_PKT_DROP_THD_SET(val, member)       \
	(((val) & SQ_CTXT_PKT_DROP_##member##_MASK) \
	 << SQ_CTXT_PKT_DROP_##member##_SHIFT)

#define SQ_CTXT_GLOBAL_SQ_ID_SHIFT 0

#define SQ_CTXT_GLOBAL_SQ_ID_MASK 0x1FFFU

#define SQ_CTXT_GLOBAL_QUEUE_ID_SET(val, member) \
	(((val) & SQ_CTXT_##member##_MASK) << SQ_CTXT_##member##_SHIFT)

#define SQ_CTXT_VLAN_TAG_SHIFT	       0
#define SQ_CTXT_VLAN_TYPE_SEL_SHIFT    16
#define SQ_CTXT_VLAN_INSERT_MODE_SHIFT 19
#define SQ_CTXT_VLAN_CEQ_EN_SHIFT      23

#define SQ_CTXT_VLAN_TAG_MASK	      0xFFFFU
#define SQ_CTXT_VLAN_TYPE_SEL_MASK    0x7U
#define SQ_CTXT_VLAN_INSERT_MODE_MASK 0x3U
#define SQ_CTXT_VLAN_CEQ_EN_MASK      0x1U

#define SQ_CTXT_VLAN_CEQ_SET(val, member)       \
	(((val) & SQ_CTXT_VLAN_##member##_MASK) \
	 << SQ_CTXT_VLAN_##member##_SHIFT)

#define SQ_CTXT_PREF_CACHE_THRESHOLD_SHIFT 0
#define SQ_CTXT_PREF_CACHE_MAX_SHIFT	   14
#define SQ_CTXT_PREF_CACHE_MIN_SHIFT	   25

#define SQ_CTXT_PREF_CACHE_THRESHOLD_MASK 0x3FFFU
#define SQ_CTXT_PREF_CACHE_MAX_MASK	  0x7FFU
#define SQ_CTXT_PREF_CACHE_MIN_MASK	  0x7FU

#define SQ_CTXT_PREF_CI_HI_SHIFT 0
#define SQ_CTXT_PREF_OWNER_SHIFT 4

#define SQ_CTXT_PREF_CI_HI_MASK 0xFU
#define SQ_CTXT_PREF_OWNER_MASK 0x1U

#define SQ_CTXT_PREF_WQ_PFN_HI_SHIFT 0
#define SQ_CTXT_PREF_CI_LOW_SHIFT    20

#define SQ_CTXT_PREF_WQ_PFN_HI_MASK 0xFFFFFU
#define SQ_CTXT_PREF_CI_LOW_MASK    0xFFFU

#define SQ_CTXT_PREF_SET(val, member)           \
	(((val) & SQ_CTXT_PREF_##member##_MASK) \
	 << SQ_CTXT_PREF_##member##_SHIFT)

#define SQ_CTXT_WQ_BLOCK_PFN_HI_SHIFT 0

#define SQ_CTXT_WQ_BLOCK_PFN_HI_MASK 0x7FFFFFU

#define SQ_CTXT_WQ_BLOCK_SET(val, member)           \
	(((val) & SQ_CTXT_WQ_BLOCK_##member##_MASK) \
	 << SQ_CTXT_WQ_BLOCK_##member##_SHIFT)

#define RQ_CTXT_PI_IDX_SHIFT 0
#define RQ_CTXT_CI_IDX_SHIFT 16

#define RQ_CTXT_PI_IDX_MASK 0xFFFFU
#define RQ_CTXT_CI_IDX_MASK 0xFFFFU

#define RQ_CTXT_CI_PI_SET(val, member) \
	(((val) & RQ_CTXT_##member##_MASK) << RQ_CTXT_##member##_SHIFT)

#define RQ_CTXT_CEQ_ATTR_INTR_SHIFT	21
#define RQ_CTXT_CEQ_ATTR_INTR_ARM_SHIFT 30
#define RQ_CTXT_CEQ_ATTR_EN_SHIFT	31

#define RQ_CTXT_CEQ_ATTR_INTR_MASK     0x3FFU
#define RQ_CTXT_CEQ_ATTR_INTR_ARM_MASK 0x1U
#define RQ_CTXT_CEQ_ATTR_EN_MASK       0x1U

#define RQ_CTXT_CEQ_ATTR_SET(val, member)           \
	(((val) & RQ_CTXT_CEQ_ATTR_##member##_MASK) \
	 << RQ_CTXT_CEQ_ATTR_##member##_SHIFT)

#define RQ_CTXT_WQ_PAGE_HI_PFN_SHIFT   0
#define RQ_CTXT_WQ_PAGE_WQE_TYPE_SHIFT 28
#define RQ_CTXT_WQ_PAGE_OWNER_SHIFT    31

#define RQ_CTXT_WQ_PAGE_HI_PFN_MASK   0xFFFFFU
#define RQ_CTXT_WQ_PAGE_WQE_TYPE_MASK 0x3U
#define RQ_CTXT_WQ_PAGE_OWNER_MASK    0x1U

#define RQ_CTXT_WQ_PAGE_SET(val, member)           \
	(((val) & RQ_CTXT_WQ_PAGE_##member##_MASK) \
	 << RQ_CTXT_WQ_PAGE_##member##_SHIFT)

#define RQ_CTXT_CQE_LEN_SHIFT 28

#define RQ_CTXT_CQE_LEN_MASK 0x3U

#define RQ_CTXT_CQE_LEN_SET(val, member) \
	(((val) & RQ_CTXT_##member##_MASK) << RQ_CTXT_##member##_SHIFT)

#define RQ_CTXT_PREF_CACHE_THRESHOLD_SHIFT 0
#define RQ_CTXT_PREF_CACHE_MAX_SHIFT	   14
#define RQ_CTXT_PREF_CACHE_MIN_SHIFT	   25

#define RQ_CTXT_PREF_CACHE_THRESHOLD_MASK 0x3FFFU
#define RQ_CTXT_PREF_CACHE_MAX_MASK	  0x7FFU
#define RQ_CTXT_PREF_CACHE_MIN_MASK	  0x7FU

#define RQ_CTXT_PREF_CI_HI_SHIFT 0
#define RQ_CTXT_PREF_OWNER_SHIFT 4

#define RQ_CTXT_PREF_CI_HI_MASK 0xFU
#define RQ_CTXT_PREF_OWNER_MASK 0x1U

#define RQ_CTXT_PREF_WQ_PFN_HI_SHIFT 0
#define RQ_CTXT_PREF_CI_LOW_SHIFT    20

#define RQ_CTXT_PREF_WQ_PFN_HI_MASK 0xFFFFFU
#define RQ_CTXT_PREF_CI_LOW_MASK    0xFFFU

#define RQ_CTXT_PREF_SET(val, member)           \
	(((val) & RQ_CTXT_PREF_##member##_MASK) \
	 << RQ_CTXT_PREF_##member##_SHIFT)

#define RQ_CTXT_WQ_BLOCK_PFN_HI_SHIFT 0

#define RQ_CTXT_WQ_BLOCK_PFN_HI_MASK 0x7FFFFFU

#define RQ_CTXT_WQ_BLOCK_SET(val, member)           \
	(((val) & RQ_CTXT_WQ_BLOCK_##member##_MASK) \
	 << RQ_CTXT_WQ_BLOCK_##member##_SHIFT)

#define SIZE_16BYTES(size) (RTE_ALIGN((size), 16) >> 4)

#define WQ_PAGE_PFN_SHIFT  12
#define WQ_BLOCK_PFN_SHIFT 9

#define WQ_PAGE_PFN(page_addr)	((page_addr) >> WQ_PAGE_PFN_SHIFT)
#define WQ_BLOCK_PFN(page_addr) ((page_addr) >> WQ_BLOCK_PFN_SHIFT)

/**
 * Prepare the command queue header and converted it to big-endian format.
 *
 * @param[out] qp_ctxt_hdr
 * Pointer to command queue context header structure to be initialized.
 * @param[in] ctxt_type
 * Type of context (SQ/RQ) to be set in header.
 * @param[in] num_queues
 * Number of queues.
 * @param[in] q_id
 * Starting queue ID for this context.
 */
static void
hinic3_qp_prepare_cmdq_header(struct hinic3_qp_ctxt_header *qp_ctxt_hdr,
			      enum hinic3_qp_ctxt_type ctxt_type,
			      uint16_t num_queues, uint16_t q_id)
{
	qp_ctxt_hdr->queue_type = ctxt_type;
	qp_ctxt_hdr->num_queues = num_queues;
	qp_ctxt_hdr->start_qid = q_id;
	qp_ctxt_hdr->rsvd = 0;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	hinic3_cpu_to_be32(qp_ctxt_hdr, sizeof(*qp_ctxt_hdr));
}

/**
 * Initialize context structure for specified TXQ by configuring various queue
 * parameters (e.g., ci, pi, work queue page addresses).
 *
 * @param[in] sq
 * Pointer to TXQ structure.
 * @param[in] sq_id
 * ID of TXQ being configured.
 * @param[out] sq_ctxt
 * Pointer to structure that will hold TXQ context.
 */
static void
hinic3_sq_prepare_ctxt(struct hinic3_txq *sq, uint16_t sq_id,
		       struct hinic3_sq_ctxt *sq_ctxt)
{
	uint64_t wq_page_addr, wq_page_pfn, wq_block_pfn;
	uint32_t wq_page_pfn_hi, wq_page_pfn_lo, wq_block_pfn_hi, wq_block_pfn_lo;
	uint16_t pi_start, ci_start;

	ci_start = sq->cons_idx & sq->q_mask;
	pi_start = sq->prod_idx & sq->q_mask;

	/* Read the first page from hardware table. */
	wq_page_addr = sq->queue_buf_paddr;

	wq_page_pfn = WQ_PAGE_PFN(wq_page_addr);
	wq_page_pfn_hi = upper_32_bits(wq_page_pfn);
	wq_page_pfn_lo = lower_32_bits(wq_page_pfn);

	/* Use 0-level CLA. */
	wq_block_pfn = WQ_BLOCK_PFN(wq_page_addr);
	wq_block_pfn_hi = upper_32_bits(wq_block_pfn);
	wq_block_pfn_lo = lower_32_bits(wq_block_pfn);

	sq_ctxt->ci_pi = SQ_CTXT_CI_PI_SET(ci_start, CI_IDX) |
			 SQ_CTXT_CI_PI_SET(pi_start, PI_IDX);

	sq_ctxt->drop_mode_sp = SQ_CTXT_MODE_SET(0, SP_FLAG) |
				SQ_CTXT_MODE_SET(0, PKT_DROP);

	sq_ctxt->wq_pfn_hi_owner = SQ_CTXT_WQ_PAGE_SET(wq_page_pfn_hi, HI_PFN) |
				   SQ_CTXT_WQ_PAGE_SET(1, OWNER);

	sq_ctxt->wq_pfn_lo = wq_page_pfn_lo;

	sq_ctxt->pkt_drop_thd =
		SQ_CTXT_PKT_DROP_THD_SET(HINIC3_DEAULT_DROP_THD_ON, THD_ON) |
		SQ_CTXT_PKT_DROP_THD_SET(HINIC3_DEAULT_DROP_THD_OFF, THD_OFF);

	sq_ctxt->global_sq_id =
		SQ_CTXT_GLOBAL_QUEUE_ID_SET(sq_id, GLOBAL_SQ_ID);

	/* Insert c-vlan in default. */
	sq_ctxt->vlan_ceq_attr = SQ_CTXT_VLAN_CEQ_SET(0, CEQ_EN) |
				 SQ_CTXT_VLAN_CEQ_SET(1, INSERT_MODE);

	sq_ctxt->rsvd0 = 0;

	sq_ctxt->pref_cache =
		SQ_CTXT_PREF_SET(WQ_PREFETCH_MIN, CACHE_MIN) |
		SQ_CTXT_PREF_SET(WQ_PREFETCH_MAX, CACHE_MAX) |
		SQ_CTXT_PREF_SET(WQ_PREFETCH_THRESHOLD, CACHE_THRESHOLD);

	sq_ctxt->pref_ci_owner =
		SQ_CTXT_PREF_SET(CI_HIGN_IDX(ci_start), CI_HI) |
		SQ_CTXT_PREF_SET(1, OWNER);

	sq_ctxt->pref_wq_pfn_hi_ci =
		SQ_CTXT_PREF_SET(ci_start, CI_LOW) |
		SQ_CTXT_PREF_SET(wq_page_pfn_hi, WQ_PFN_HI);

	sq_ctxt->pref_wq_pfn_lo = wq_page_pfn_lo;

	sq_ctxt->wq_block_pfn_hi =
		SQ_CTXT_WQ_BLOCK_SET(wq_block_pfn_hi, PFN_HI);

	sq_ctxt->wq_block_pfn_lo = wq_block_pfn_lo;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	hinic3_cpu_to_be32(sq_ctxt, sizeof(*sq_ctxt));
}

/**
 * Initialize context structure for specified RXQ by configuring various queue
 * parameters (e.g., ci, pi, work queue page addresses).
 *
 * @param[in] rq
 * Pointer to RXQ structure.
 * @param[out] rq_ctxt
 * Pointer to structure that will hold RXQ context.
 */
static void
hinic3_rq_prepare_ctxt(struct hinic3_rxq *rq, struct hinic3_rq_ctxt *rq_ctxt)
{
	uint64_t wq_page_addr, wq_page_pfn, wq_block_pfn;
	uint32_t wq_page_pfn_hi, wq_page_pfn_lo, wq_block_pfn_hi, wq_block_pfn_lo;
	uint16_t pi_start, ci_start;
	uint16_t wqe_type = rq->wqebb_shift - HINIC3_RQ_WQEBB_SHIFT;
	uint8_t intr_disable;

	/* RQ depth is in unit of 8 Bytes. */
	ci_start = (uint16_t)((rq->cons_idx & rq->q_mask) << wqe_type);
	pi_start = (uint16_t)((rq->prod_idx & rq->q_mask) << wqe_type);

	/* Read the first page from hardware table. */
	wq_page_addr = rq->queue_buf_paddr;

	wq_page_pfn = WQ_PAGE_PFN(wq_page_addr);
	wq_page_pfn_hi = upper_32_bits(wq_page_pfn);
	wq_page_pfn_lo = lower_32_bits(wq_page_pfn);

	/* Use 0-level CLA. */
	wq_block_pfn = WQ_BLOCK_PFN(wq_page_addr);
	wq_block_pfn_hi = upper_32_bits(wq_block_pfn);
	wq_block_pfn_lo = lower_32_bits(wq_block_pfn);

	rq_ctxt->ci_pi = RQ_CTXT_CI_PI_SET(ci_start, CI_IDX) |
			 RQ_CTXT_CI_PI_SET(pi_start, PI_IDX);

	/* RQ doesn't need ceq, msix_entry_idx set 1, but mask not enable. */
	intr_disable = rq->dp_intr_en ? 0 : 1;
	rq_ctxt->ceq_attr = RQ_CTXT_CEQ_ATTR_SET(intr_disable, EN) |
			    RQ_CTXT_CEQ_ATTR_SET(0, INTR_ARM) |
			    RQ_CTXT_CEQ_ATTR_SET(rq->msix_entry_idx, INTR);

	/* Use 32Byte WQE with SGE for CQE in default. */
	rq_ctxt->wq_pfn_hi_type_owner =
		RQ_CTXT_WQ_PAGE_SET(wq_page_pfn_hi, HI_PFN) |
		RQ_CTXT_WQ_PAGE_SET(1, OWNER);

	switch (wqe_type) {
	case HINIC3_EXTEND_RQ_WQE:
		/* Use 32Byte WQE with SGE for CQE. */
		rq_ctxt->wq_pfn_hi_type_owner |=
			RQ_CTXT_WQ_PAGE_SET(0, WQE_TYPE);
		break;
	case HINIC3_NORMAL_RQ_WQE:
		/* Use 16Byte WQE with 32Bytes SGE for CQE. */
		rq_ctxt->wq_pfn_hi_type_owner |=
			RQ_CTXT_WQ_PAGE_SET(2, WQE_TYPE);
		rq_ctxt->cqe_sge_len = RQ_CTXT_CQE_LEN_SET(1, CQE_LEN);
		break;
	default:
		PMD_DRV_LOG(INFO, "Invalid rq wqe type: %u", wqe_type);
	}

	rq_ctxt->wq_pfn_lo = wq_page_pfn_lo;

	rq_ctxt->pref_cache =
		RQ_CTXT_PREF_SET(WQ_PREFETCH_MIN, CACHE_MIN) |
		RQ_CTXT_PREF_SET(WQ_PREFETCH_MAX, CACHE_MAX) |
		RQ_CTXT_PREF_SET(WQ_PREFETCH_THRESHOLD, CACHE_THRESHOLD);

	rq_ctxt->pref_ci_owner =
		RQ_CTXT_PREF_SET(CI_HIGN_IDX(ci_start), CI_HI) |
		RQ_CTXT_PREF_SET(1, OWNER);

	rq_ctxt->pref_wq_pfn_hi_ci =
		RQ_CTXT_PREF_SET(wq_page_pfn_hi, WQ_PFN_HI) |
		RQ_CTXT_PREF_SET(ci_start, CI_LOW);

	rq_ctxt->pref_wq_pfn_lo = wq_page_pfn_lo;

	rq_ctxt->pi_paddr_hi = upper_32_bits(rq->pi_dma_addr);
	rq_ctxt->pi_paddr_lo = lower_32_bits(rq->pi_dma_addr);

	rq_ctxt->wq_block_pfn_hi =
		RQ_CTXT_WQ_BLOCK_SET(wq_block_pfn_hi, PFN_HI);

	rq_ctxt->wq_block_pfn_lo = wq_block_pfn_lo;
	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	hinic3_cpu_to_be32(rq_ctxt, sizeof(*rq_ctxt));
}

/**
 * Allocate a command buffer, prepare context for each SQ queue by setting
 * various parameters, send context data to hardware. It processes SQ queues in
 * batches, with each batch not exceeding `HINIC3_Q_CTXT_MAX` SQ contexts.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 *
 * @return
 * 0 on success, a negative error code on failure.
 * - -ENOMEM if the memory allocation for the command buffer fails.
 * - -EFAULT if the hardware returns an error while processing the context data.
 */
static int
init_sq_ctxts(struct hinic3_nic_dev *nic_dev)
{
	struct hinic3_sq_ctxt_block *sq_ctxt_block = NULL;
	struct hinic3_sq_ctxt *sq_ctxt = NULL;
	struct hinic3_cmd_buf *cmd_buf = NULL;
	struct hinic3_txq *sq = NULL;
	uint64_t out_param = 0;
	uint16_t q_id, curr_id, max_ctxts, i;
	int err = 0;

	cmd_buf = hinic3_alloc_cmd_buf(nic_dev->hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf for sq ctx failed");
		return -ENOMEM;
	}

	q_id = 0;
	while (q_id < nic_dev->num_sqs) {
		sq_ctxt_block = cmd_buf->buf;
		sq_ctxt = sq_ctxt_block->sq_ctxt;

		max_ctxts = (nic_dev->num_sqs - q_id) > HINIC3_Q_CTXT_MAX
				    ? HINIC3_Q_CTXT_MAX
				    : (nic_dev->num_sqs - q_id);

		hinic3_qp_prepare_cmdq_header(&sq_ctxt_block->cmdq_hdr,
					      HINIC3_QP_CTXT_TYPE_SQ,
					      max_ctxts, q_id);

		for (i = 0; i < max_ctxts; i++) {
			curr_id = q_id + i;
			sq = nic_dev->txqs[curr_id];
			hinic3_sq_prepare_ctxt(sq, curr_id, &sq_ctxt[i]);
		}

		cmd_buf->size = SQ_CTXT_SIZE(max_ctxts);
		rte_atomic_thread_fence(rte_memory_order_seq_cst);
		err = hinic3_cmdq_direct_resp(nic_dev->hwdev, HINIC3_MOD_L2NIC,
					      HINIC3_UCODE_CMD_MODIFY_QUEUE_CTX,
					      cmd_buf, &out_param, 0);
		if (err || out_param != 0) {
			PMD_DRV_LOG(ERR,
				    "Set SQ ctxts failed, err: %d, out_param: %" PRIu64,
				    err, out_param);

			err = -EFAULT;
			break;
		}

		q_id += max_ctxts;
	}

	hinic3_free_cmd_buf(cmd_buf);
	return err;
}

/**
 * Initialize context for all RQ in device.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 *
 * @return
 * 0 on success, a negative error code on failure.
 * - -ENOMEM if the memory allocation for the command buffer fails.
 * - -EFAULT if the hardware returns an error while processing the context data.
 */
static int
init_rq_ctxts(struct hinic3_nic_dev *nic_dev)
{
	struct hinic3_rq_ctxt_block *rq_ctxt_block = NULL;
	struct hinic3_rq_ctxt *rq_ctxt = NULL;
	struct hinic3_cmd_buf *cmd_buf = NULL;
	struct hinic3_rxq *rq = NULL;
	uint64_t out_param = 0;
	uint16_t q_id, curr_id, max_ctxts, i;
	int err = 0;

	cmd_buf = hinic3_alloc_cmd_buf(nic_dev->hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf for rq ctx failed");
		return -ENOMEM;
	}

	q_id = 0;
	while (q_id < nic_dev->num_rqs) {
		rq_ctxt_block = cmd_buf->buf;
		rq_ctxt = rq_ctxt_block->rq_ctxt;

		max_ctxts = (nic_dev->num_rqs - q_id) > HINIC3_Q_CTXT_MAX
				    ? HINIC3_Q_CTXT_MAX
				    : (nic_dev->num_rqs - q_id);

		hinic3_qp_prepare_cmdq_header(&rq_ctxt_block->cmdq_hdr,
					      HINIC3_QP_CTXT_TYPE_RQ,
					      max_ctxts, q_id);

		for (i = 0; i < max_ctxts; i++) {
			curr_id = q_id + i;
			rq = nic_dev->rxqs[curr_id];
			hinic3_rq_prepare_ctxt(rq, &rq_ctxt[i]);
		}

		cmd_buf->size = RQ_CTXT_SIZE(max_ctxts);
		rte_atomic_thread_fence(rte_memory_order_seq_cst);
		err = hinic3_cmdq_direct_resp(nic_dev->hwdev, HINIC3_MOD_L2NIC,
					      HINIC3_UCODE_CMD_MODIFY_QUEUE_CTX,
					      cmd_buf, &out_param, 0);
		if (err || out_param != 0) {
			PMD_DRV_LOG(ERR,
				    "Set RQ ctxts failed, err: %d, out_param: %" PRIu64,
				    err, out_param);
			err = -EFAULT;
			break;
		}

		q_id += max_ctxts;
	}

	hinic3_free_cmd_buf(cmd_buf);
	return err;
}

/**
 * Allocate memory for command buffer, construct related command request, send a
 * command to hardware to clean up queue offload context.
 *
 * @param[in] nic_dev
 * Pointer to NIC device structure.
 * @param[in] ctxt_type
 * The type of queue context to clean.
 * The queue context type that determines which queue type to clean up.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
static int
clean_queue_offload_ctxt(struct hinic3_nic_dev *nic_dev,
			 enum hinic3_qp_ctxt_type ctxt_type)
{
	struct hinic3_clean_queue_ctxt *ctxt_block = NULL;
	struct hinic3_cmd_buf *cmd_buf;
	uint64_t out_param = 0;
	int err;

	cmd_buf = hinic3_alloc_cmd_buf(nic_dev->hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf for LRO/TSO space failed");
		return -ENOMEM;
	}

	/* Construct related command request. */
	ctxt_block = cmd_buf->buf;
	/* Assumed max_rqs must be equal to max_sqs. */
	ctxt_block->cmdq_hdr.num_queues = nic_dev->max_sqs;
	ctxt_block->cmdq_hdr.queue_type = ctxt_type;
	ctxt_block->cmdq_hdr.start_qid = 0;
	/*
	 * Add a memory barrier to ensure that instructions are not out of order
	 * due to compilation optimization.
	 */
	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	hinic3_cpu_to_be32(ctxt_block, sizeof(*ctxt_block));

	cmd_buf->size = sizeof(*ctxt_block);

	/* Send a command to hardware to clean up queue offload context. */
	err = hinic3_cmdq_direct_resp(nic_dev->hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_UCODE_CMD_CLEAN_QUEUE_CONTEXT,
				      cmd_buf, &out_param, 0);
	if ((err) || (out_param)) {
		PMD_DRV_LOG(ERR,
			    "Clean queue offload ctxts failed, err: %d, out_param: %" PRIu64,
			    err, out_param);
		err = -EFAULT;
	}

	hinic3_free_cmd_buf(cmd_buf);
	return err;
}

static int
clean_qp_offload_ctxt(struct hinic3_nic_dev *nic_dev)
{
	/* Clean LRO/TSO context space. */
	return (clean_queue_offload_ctxt(nic_dev, HINIC3_QP_CTXT_TYPE_SQ) ||
		clean_queue_offload_ctxt(nic_dev, HINIC3_QP_CTXT_TYPE_RQ));
}

void
hinic3_get_func_rx_buf_size(struct hinic3_nic_dev *nic_dev)
{
	struct hinic3_rxq *rxq = NULL;
	uint16_t q_id;
	uint16_t buf_size = 0;

	for (q_id = 0; q_id < nic_dev->num_rqs; q_id++) {
		rxq = nic_dev->rxqs[q_id];

		if (rxq == NULL)
			continue;

		if (q_id == 0)
			buf_size = rxq->buf_len;

		buf_size = buf_size > rxq->buf_len ? rxq->buf_len : buf_size;
	}

	nic_dev->rx_buff_len = buf_size;
}

int
hinic3_init_qp_ctxts(struct hinic3_nic_dev *nic_dev)
{
	struct hinic3_hwdev *hwdev = NULL;
	struct hinic3_sq_attr sq_attr;
	uint32_t rq_depth = 0;
	uint32_t sq_depth = 0;
	uint16_t q_id;
	int err;

	if (!nic_dev)
		return -EINVAL;

	hwdev = nic_dev->hwdev;

	err = init_sq_ctxts(nic_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init SQ ctxts failed");
		return err;
	}

	err = init_rq_ctxts(nic_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init RQ ctxts failed");
		return err;
	}

	err = clean_qp_offload_ctxt(nic_dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Clean qp offload ctxts failed");
		return err;
	}

	if (nic_dev->num_rqs != 0)
		rq_depth = ((uint32_t)nic_dev->rxqs[0]->q_depth)
			   << nic_dev->rxqs[0]->wqe_type;

	if (nic_dev->num_sqs != 0)
		sq_depth = nic_dev->txqs[0]->q_depth;

	err = hinic3_set_root_ctxt(hwdev, rq_depth, sq_depth,
				   nic_dev->rx_buff_len);
	if (err) {
		PMD_DRV_LOG(ERR, "Set root context failed");
		return err;
	}

	/* Configure CI tables for each SQ. */
	for (q_id = 0; q_id < nic_dev->num_sqs; q_id++) {
		sq_attr.ci_dma_base = nic_dev->txqs[q_id]->ci_dma_base >> 0x2;
		sq_attr.pending_limit = HINIC3_DEAULT_TX_CI_PENDING_LIMIT;
		sq_attr.coalescing_time = HINIC3_DEAULT_TX_CI_COALESCING_TIME;
		sq_attr.intr_en = 0;
		sq_attr.intr_idx = 0; /**< Tx doesn't need interrupt. */
		sq_attr.l2nic_sqn = q_id;
		sq_attr.dma_attr_off = 0;
		err = hinic3_set_ci_table(hwdev, &sq_attr);
		if (err) {
			PMD_DRV_LOG(ERR, "Set ci table failed");
			goto set_cons_idx_table_err;
		}
	}

	return 0;

set_cons_idx_table_err:
	hinic3_clean_root_ctxt(hwdev);
	return err;
}

void
hinic3_free_qp_ctxts(struct hinic3_hwdev *hwdev)
{
	if (!hwdev)
		return;

	hinic3_clean_root_ctxt(hwdev);
}

void
hinic3_update_driver_feature(struct hinic3_nic_dev *nic_dev, uint64_t s_feature)
{
	if (!nic_dev)
		return;

	nic_dev->feature_cap = s_feature;

	PMD_DRV_LOG(DEBUG, "Update nic feature to 0x%" PRIx64, nic_dev->feature_cap);
}

uint64_t
hinic3_get_driver_feature(struct hinic3_nic_dev *nic_dev)
{
	return nic_dev->feature_cap;
}
