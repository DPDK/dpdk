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

#define HINIC3_DEAULT_TX_CI_PENDING_LIMIT	3
#define HINIC3_DEAULT_TX_CI_COALESCING_TIME	16
#define HINIC3_DEAULT_DROP_THD_ON		0xFFFF
#define HINIC3_DEAULT_DROP_THD_OFF		0

#define WQ_PREFETCH_MAX				6
#define WQ_PREFETCH_MIN				1
#define WQ_PREFETCH_THRESHOLD			256

#define CI_IDX_HIGH_SHIFH			12

#define CI_HIGN_IDX(val) ((val) >> CI_IDX_HIGH_SHIFH)

#define SQ_CTXT_PI_IDX_SHIFT			0
#define SQ_CTXT_CI_IDX_SHIFT			16

#define SQ_CTXT_PI_IDX_MASK			0xFFFFU
#define SQ_CTXT_CI_IDX_MASK			0xFFFFU

#define SQ_CTXT_CI_PI_SET(val, member)			\
	(((val) & SQ_CTXT_##member##_MASK) << SQ_CTXT_##member##_SHIFT)

#define SQ_CTXT_MODE_SP_FLAG_SHIFT		0
#define SQ_CTXT_MODE_PKT_DROP_SHIFT		1

#define SQ_CTXT_MODE_SP_FLAG_MASK		0x1U
#define SQ_CTXT_MODE_PKT_DROP_MASK		0x1U

#define SQ_CTXT_MODE_SET(val, member)			\
	(((val) & SQ_CTXT_MODE_##member##_MASK)		\
	 << SQ_CTXT_MODE_##member##_SHIFT)

#define SQ_CTXT_WQ_PAGE_HI_PFN_SHIFT		0
#define SQ_CTXT_WQ_PAGE_OWNER_SHIFT		23

#define SQ_CTXT_WQ_PAGE_HI_PFN_MASK		0xFFFFFU
#define SQ_CTXT_WQ_PAGE_OWNER_MASK		0x1U

#define SQ_CTXT_WQ_PAGE_SET(val, member)		\
	(((val) & SQ_CTXT_WQ_PAGE_##member##_MASK)	\
	 << SQ_CTXT_WQ_PAGE_##member##_SHIFT)

#define SQ_CTXT_PKT_DROP_THD_ON_SHIFT		0
#define SQ_CTXT_PKT_DROP_THD_OFF_SHIFT		16

#define SQ_CTXT_PKT_DROP_THD_ON_MASK		0xFFFFU
#define SQ_CTXT_PKT_DROP_THD_OFF_MASK		0xFFFFU

#define SQ_CTXT_PKT_DROP_THD_SET(val, member)		\
	(((val) & SQ_CTXT_PKT_DROP_##member##_MASK)	\
	 << SQ_CTXT_PKT_DROP_##member##_SHIFT)

#define SQ_CTXT_GLOBAL_SQ_ID_SHIFT		0

#define SQ_CTXT_GLOBAL_SQ_ID_MASK		0x1FFFU

#define SQ_CTXT_GLOBAL_QUEUE_ID_SET(val, member) \
	(((val) & SQ_CTXT_##member##_MASK) << SQ_CTXT_##member##_SHIFT)

#define SQ_CTXT_VLAN_TAG_SHIFT			0
#define SQ_CTXT_VLAN_TYPE_SEL_SHIFT		16
#define SQ_CTXT_VLAN_INSERT_MODE_SHIFT		19
#define SQ_CTXT_VLAN_CEQ_EN_SHIFT		23

#define SQ_CTXT_VLAN_TAG_MASK			0xFFFFU
#define SQ_CTXT_VLAN_TYPE_SEL_MASK		0x7U
#define SQ_CTXT_VLAN_INSERT_MODE_MASK		0x3U
#define SQ_CTXT_VLAN_CEQ_EN_MASK		0x1U

#define SQ_CTXT_VLAN_CEQ_SET(val, member)		\
	(((val) & SQ_CTXT_VLAN_##member##_MASK)		\
	 << SQ_CTXT_VLAN_##member##_SHIFT)

#define SQ_CTXT_PREF_CACHE_THRESHOLD_SHIFT	0
#define SQ_CTXT_PREF_CACHE_MAX_SHIFT		14
#define SQ_CTXT_PREF_CACHE_MIN_SHIFT		25

#define SQ_CTXT_PREF_CACHE_THRESHOLD_MASK	0x3FFFU
#define SQ_CTXT_PREF_CACHE_MAX_MASK		0x7FFU
#define SQ_CTXT_PREF_CACHE_MIN_MASK		0x7FU

#define SQ_CTXT_PREF_CI_HI_SHIFT		0
#define SQ_CTXT_PREF_OWNER_SHIFT		4

#define SQ_CTXT_PREF_CI_HI_MASK			0xFU
#define SQ_CTXT_PREF_OWNER_MASK			0x1U

#define SQ_CTXT_PREF_WQ_PFN_HI_SHIFT		0
#define SQ_CTXT_PREF_CI_LOW_SHIFT		20

#define SQ_CTXT_PREF_WQ_PFN_HI_MASK		0xFFFFFU
#define SQ_CTXT_PREF_CI_LOW_MASK		0xFFFU

#define SQ_CTXT_PREF_SET(val, member)			\
	(((val) & SQ_CTXT_PREF_##member##_MASK)		\
	 << SQ_CTXT_PREF_##member##_SHIFT)

#define SQ_CTXT_WQ_BLOCK_PFN_HI_SHIFT		0

#define SQ_CTXT_WQ_BLOCK_PFN_HI_MASK		0x7FFFFFU

#define SQ_CTXT_WQ_BLOCK_SET(val, member)		\
	(((val) & SQ_CTXT_WQ_BLOCK_##member##_MASK)	\
	 << SQ_CTXT_WQ_BLOCK_##member##_SHIFT)

#define RQ_CTXT_PI_IDX_SHIFT			0
#define RQ_CTXT_CI_IDX_SHIFT			16

#define RQ_CTXT_PI_IDX_MASK			0xFFFFU
#define RQ_CTXT_CI_IDX_MASK			0xFFFFU

#define RQ_CTXT_CI_PI_SET(val, member)			\
	(((val) & RQ_CTXT_##member##_MASK) << RQ_CTXT_##member##_SHIFT)

#define RQ_CTXT_CEQ_ATTR_INTR_SHIFT		21
#define RQ_CTXT_CEQ_ATTR_INTR_ARM_SHIFT		30
#define RQ_CTXT_CEQ_ATTR_EN_SHIFT		31

#define RQ_CTXT_CEQ_ATTR_INTR_MASK		0x3FFU
#define RQ_CTXT_CEQ_ATTR_INTR_ARM_MASK		0x1U
#define RQ_CTXT_CEQ_ATTR_EN_MASK		0x1U

#define RQ_CTXT_CEQ_ATTR_SET(val, member)		\
	(((val) & RQ_CTXT_CEQ_ATTR_##member##_MASK)	\
	 << RQ_CTXT_CEQ_ATTR_##member##_SHIFT)

#define RQ_CTXT_WQ_PAGE_HI_PFN_SHIFT		0
#define RQ_CTXT_WQ_PAGE_WQE_TYPE_SHIFT		28
#define RQ_CTXT_WQ_PAGE_OWNER_SHIFT		31

#define RQ_CTXT_WQ_PAGE_HI_PFN_MASK		0xFFFFFU
#define RQ_CTXT_WQ_PAGE_WQE_TYPE_MASK		0x3U
#define RQ_CTXT_WQ_PAGE_OWNER_MASK		0x1U

#define RQ_CTXT_WQ_PAGE_SET(val, member)		\
	(((val) & RQ_CTXT_WQ_PAGE_##member##_MASK)	\
	 << RQ_CTXT_WQ_PAGE_##member##_SHIFT)

#define RQ_CTXT_CQE_LEN_SHIFT			28

#define RQ_CTXT_CQE_LEN_MASK			0x3U

#define RQ_CTXT_CQE_LEN_SET(val, member)		\
	(((val) & RQ_CTXT_##member##_MASK) << RQ_CTXT_##member##_SHIFT)

#define RQ_CTXT_PREF_CACHE_THRESHOLD_SHIFT	0
#define RQ_CTXT_PREF_CACHE_MAX_SHIFT		14
#define RQ_CTXT_PREF_CACHE_MIN_SHIFT		25

#define RQ_CTXT_PREF_CACHE_THRESHOLD_MASK	0x3FFFU
#define RQ_CTXT_PREF_CACHE_MAX_MASK		0x7FFU
#define RQ_CTXT_PREF_CACHE_MIN_MASK		0x7FU

#define RQ_CTXT_PREF_CI_HI_SHIFT		0
#define RQ_CTXT_PREF_OWNER_SHIFT		4

#define RQ_CTXT_PREF_CI_HI_MASK			0xFU
#define RQ_CTXT_PREF_OWNER_MASK			0x1U

#define RQ_CTXT_PREF_WQ_PFN_HI_SHIFT		0
#define RQ_CTXT_PREF_CI_LOW_SHIFT		20

#define RQ_CTXT_PREF_WQ_PFN_HI_MASK		0xFFFFFU
#define RQ_CTXT_PREF_CI_LOW_MASK		0xFFFU

#define RQ_CTXT_PREF_SET(val, member)			\
	(((val) & RQ_CTXT_PREF_##member##_MASK)		\
	 << RQ_CTXT_PREF_##member##_SHIFT)

#define RQ_CTXT_WQ_BLOCK_PFN_HI_SHIFT		0

#define RQ_CTXT_WQ_BLOCK_PFN_HI_MASK		0x7FFFFFU

#define RQ_CTXT_WQ_BLOCK_SET(val, member)		\
	(((val) & RQ_CTXT_WQ_BLOCK_##member##_MASK)	\
	 << RQ_CTXT_WQ_BLOCK_##member##_SHIFT)

#define SIZE_16BYTES(size) (RTE_ALIGN((size), 16) >> 4)

#define WQ_PAGE_PFN_SHIFT			12
#define WQ_BLOCK_PFN_SHIFT			9

#define WQ_PAGE_PFN(page_addr)	((page_addr) >> WQ_PAGE_PFN_SHIFT)
#define WQ_BLOCK_PFN(page_addr) ((page_addr) >> WQ_BLOCK_PFN_SHIFT)

#define CQE_CTX_CI_ADDR_SHIFT			4

void
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

void
hinic3_rq_prepare_ctxt(struct hinic3_rxq *rq, struct hinic3_rq_ctxt *rq_ctxt)
{
	uint64_t wq_page_addr, wq_page_pfn, wq_block_pfn;
	uint32_t wq_page_pfn_hi, wq_page_pfn_lo, wq_block_pfn_hi, wq_block_pfn_lo;
	uint16_t pi_start, ci_start;
	uint16_t wqe_type = rq->wqe_type;
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
	case HINIC3_COMPACT_RQ_WQE:
		/* Use 8Byte WQE without SGE for CQE. */
		rq_ctxt->wq_pfn_hi_type_owner |= RQ_CTXT_WQ_PAGE_SET(3, WQE_TYPE);
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
	struct hinic3_cmd_buf *cmd_buf = NULL;
	uint64_t out_param = 0;
	uint16_t q_id, max_ctxts;
	uint8_t cmd;
	int err = 0;

	cmd_buf = hinic3_alloc_cmd_buf(nic_dev->hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf for sq ctx failed");
		return -ENOMEM;
	}

	q_id = 0;
	while (q_id < nic_dev->num_sqs) {
		max_ctxts = (nic_dev->num_sqs - q_id) > HINIC3_Q_CTXT_MAX
				    ? HINIC3_Q_CTXT_MAX
				    : (nic_dev->num_sqs - q_id);
		cmd = nic_dev->cmdq_ops->prepare_cmd_buf_qp_context_multi_store(nic_dev, cmd_buf,
			HINIC3_QP_CTXT_TYPE_SQ, q_id, max_ctxts);
		rte_atomic_thread_fence(rte_memory_order_seq_cst);
		err = hinic3_cmdq_direct_resp(nic_dev->hwdev, HINIC3_MOD_L2NIC,
					      cmd, cmd_buf, &out_param, 0);
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
	struct hinic3_cmd_buf *cmd_buf = NULL;
	uint64_t out_param = 0;
	uint16_t q_id, max_ctxts;
	uint8_t cmd;
	int err = 0;

	cmd_buf = hinic3_alloc_cmd_buf(nic_dev->hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf for rq ctx failed");
		return -ENOMEM;
	}

	q_id = 0;
	while (q_id < nic_dev->num_rqs) {
		max_ctxts = (nic_dev->num_rqs - q_id) > HINIC3_Q_CTXT_MAX
				    ? HINIC3_Q_CTXT_MAX
				    : (nic_dev->num_rqs - q_id);
		cmd = nic_dev->cmdq_ops->prepare_cmd_buf_qp_context_multi_store(nic_dev, cmd_buf,
			HINIC3_QP_CTXT_TYPE_RQ, q_id, max_ctxts);
		rte_atomic_thread_fence(rte_memory_order_seq_cst);
		err = hinic3_cmdq_direct_resp(nic_dev->hwdev, HINIC3_MOD_L2NIC,
					      cmd, cmd_buf, &out_param, 0);
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
	struct hinic3_cmd_buf *cmd_buf;
	uint64_t out_param = 0;
	uint8_t cmd;
	int err;

	cmd_buf = hinic3_alloc_cmd_buf(nic_dev->hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf for LRO/TSO space failed");
		return -ENOMEM;
	}

	cmd = nic_dev->cmdq_ops->prepare_cmd_buf_clean_tso_lro_space(nic_dev, cmd_buf, ctxt_type);

	/* Send a command to hardware to clean up queue offload context. */
	err = hinic3_cmdq_direct_resp(nic_dev->hwdev, HINIC3_MOD_L2NIC,
				      cmd, cmd_buf, &out_param, 0);
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

#define HINIC3_RX_CQE_TIMER_LOOP		15
#define HINIC3_RX_CQE_COALESCE_NUM		63

int
hinic3_init_rq_cqe_ctxts(struct hinic3_nic_dev *nic_dev)
{
	struct hinic3_hwdev *hwdev = NULL;
	struct hinic3_rxq *rxq = NULL;
	struct hinic3_rq_cqe_ctx cqe_ctx = { 0 };
	rte_iova_t rq_ci_paddr;
	uint16_t out_size = sizeof(cqe_ctx);
	uint16_t q_id = 0;
	uint16_t cmd;
	int err;

	if (!nic_dev)
		return -EINVAL;

	hwdev = nic_dev->hwdev;

	if (hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_CMDQ)
		cmd = HINIC3_NIC_CMD_SET_RQ_CI_CTX_HTN;
	else
		cmd = HINIC3_NIC_CMD_SET_RQ_CI_CTX;

	while (q_id < nic_dev->num_rqs) {
		rxq = nic_dev->rxqs[q_id];
		if (rxq->wqe_type == HINIC3_COMPACT_RQ_WQE) {
			rq_ci_paddr = rxq->rq_ci_paddr >> CQE_CTX_CI_ADDR_SHIFT;
			cqe_ctx.ci_addr_hi = upper_32_bits(rq_ci_paddr);
			cqe_ctx.ci_addr_lo = lower_32_bits(rq_ci_paddr);
			cqe_ctx.threshold_cqe_num = HINIC3_RX_CQE_COALESCE_NUM;
			cqe_ctx.timer_loop = HINIC3_RX_CQE_TIMER_LOOP;
		} else {
			cqe_ctx.threshold_cqe_num = 0;
			cqe_ctx.timer_loop = 0;
		}

		cqe_ctx.cqe_type = (rxq->wqe_type == HINIC3_COMPACT_RQ_WQE);
		cqe_ctx.msix_entry_idx = rxq->msix_entry_idx;
		cqe_ctx.rq_id = q_id;

		err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC, cmd,
					      &cqe_ctx, sizeof(cqe_ctx),
					      &cqe_ctx, &out_size);
		if (err || !out_size || cqe_ctx.msg_head.status) {
			PMD_DRV_LOG(ERR, "Set rq cqe context failed, qid: %d, err: %d, status: 0x%x, out_size: 0x%x",
				    q_id, err, cqe_ctx.msg_head.status, out_size);
			return -EFAULT;
		}
		q_id++;
	}

	return 0;
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

	if (HINIC3_SUPPORT_RX_HW_COMPACT_CQE(nic_dev)) {
		/* Init Rxq CQE context. */
		err = hinic3_init_rq_cqe_ctxts(nic_dev);
		if (err) {
			PMD_DRV_LOG(ERR, "Set rq cqe context failed");
			goto set_cqe_ctx_fail;
		}
	}

	return 0;

set_cqe_ctx_fail:
set_cons_idx_table_err:
	hinic3_clean_root_ctxt(hwdev);
	return err;
}

int
hinic3_set_rq_enable(struct hinic3_nic_dev *nic_dev, uint16_t q_id, bool enable)
{
	struct hinic3_hwdev *hwdev = NULL;
	struct hinic3_rq_enable msg;
	uint16_t out_size = sizeof(msg);
	int err;

	if (!nic_dev)
		return -EINVAL;

	hwdev = nic_dev->hwdev;

	memset(&msg, 0, sizeof(msg));
	msg.rq_enable = enable;
	msg.rq_id = q_id;
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC, HINIC3_NIC_CMD_SET_RQ_ENABLE_HTN,
				      &msg, sizeof(msg), &msg, &out_size);
	if (err || !out_size || msg.msg_head.status) {
		PMD_DRV_LOG(ERR, "Set rq enable failed, qid: %u, enable: %d, err: %d, status: 0x%x, out_size: 0x%x",
			    q_id, enable, err, msg.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
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
