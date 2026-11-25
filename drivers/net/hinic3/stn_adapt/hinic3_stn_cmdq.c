/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_nic_cfg.h"
#include "hinic3_cmd.h"
#include "hinic3_hwif.h"
#include "hinic3_stn_cmdq.h"

#define HINIC3_DEFAULT_DROP_THD_ON			0xFFFF
#define HINIC3_DEFAULT_DROP_THD_OFF			0
#define WQ_PREFETCH_MAX					6
#define WQ_PREFETCH_MIN					1
#define WQ_PREFETCH_THRESHOLD				256

#define RQ_CTXT_CEQ_ATTR_CI_WR_SHIFT			0
#define RQ_CTXT_CEQ_ATTR_INTR_SHIFT			21
#define RQ_CTXT_CEQ_ATTR_INTR_ARM_SHIFT			30
#define RQ_CTXT_CEQ_ATTR_EN_SHIFT			31

#define RQ_CTXT_CEQ_ATTR_CI_WR_MASK			0x1U
#define RQ_CTXT_CEQ_ATTR_INTR_MASK			0x3FFU
#define RQ_CTXT_CEQ_ATTR_INTR_ARM_MASK			0x1U
#define RQ_CTXT_CEQ_ATTR_EN_MASK			0x1U
/* Indicate ucode that this is an interrupt in the DPDK scenario. */
#define RQ_CTXT_INVALID_INTR_NUM			0x1FFU

#define STN_SQ_CTXT_SIZE(num_sqs)	((uint16_t)(sizeof(struct hinic3_stn_qp_ctxt_header) \
						    + (num_sqs) * sizeof(struct hinic3_sq_ctxt)))
#define STN_RQ_CTXT_SIZE(num_rqs)	((uint16_t)(sizeof(struct hinic3_stn_qp_ctxt_header) \
						    + (num_rqs) * sizeof(struct hinic3_rq_ctxt)))

static uint8_t prepare_cmd_buf_clean_tso_lro_space(struct hinic3_nic_dev *nic_dev,
						   struct hinic3_cmd_buf *cmd_buf,
						   enum hinic3_qp_ctxt_type ctxt_type)
{
	struct hinic3_stn_clean_queue_ctxt *ctxt_block = NULL;

	ctxt_block = cmd_buf->buf;
	ctxt_block->cmdq_hdr.num_queues = nic_dev->max_sqs;
	ctxt_block->cmdq_hdr.queue_type = ctxt_type;
	ctxt_block->cmdq_hdr.start_qid = 0;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	hinic3_cpu_to_be32(ctxt_block, sizeof(*ctxt_block));

	cmd_buf->size = sizeof(*ctxt_block);
	return HINIC3_UCODE_CMD_CLEAN_QUEUE_CONTEXT;
}

static void qp_prepare_cmdq_header(struct hinic3_stn_qp_ctxt_header *qp_ctxt_hdr,
				   enum hinic3_qp_ctxt_type ctxt_type, uint16_t num_queues,
				   uint16_t q_id)
{
	qp_ctxt_hdr->queue_type = ctxt_type;
	qp_ctxt_hdr->num_queues = num_queues;
	qp_ctxt_hdr->start_qid = q_id;
	qp_ctxt_hdr->rsvd = 0;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	hinic3_cpu_to_be32(qp_ctxt_hdr, sizeof(*qp_ctxt_hdr));
}

static uint8_t prepare_cmd_buf_qp_context_multi_store(struct hinic3_nic_dev *nic_dev,
						 struct hinic3_cmd_buf *cmd_buf,
						 enum hinic3_qp_ctxt_type ctxt_type,
						 uint16_t start_qid, uint16_t max_ctxts)
{
	struct hinic3_stn_qp_ctxt_block *qp_ctxt_block = NULL;
	uint16_t i;

	qp_ctxt_block = cmd_buf->buf;

	qp_prepare_cmdq_header(&qp_ctxt_block->cmdq_hdr, ctxt_type,
				   max_ctxts, start_qid);

	for (i = 0; i < max_ctxts; i++) {
		if (ctxt_type == HINIC3_QP_CTXT_TYPE_RQ)
			hinic3_rq_prepare_ctxt(nic_dev->rxqs[start_qid + i],
					       &qp_ctxt_block->rq_ctxt[i]);
		else
			hinic3_sq_prepare_ctxt(nic_dev->txqs[start_qid + i], start_qid + i,
					       &qp_ctxt_block->sq_ctxt[i]);
	}

	if (ctxt_type == HINIC3_QP_CTXT_TYPE_RQ)
		cmd_buf->size = STN_RQ_CTXT_SIZE(max_ctxts);
	else
		cmd_buf->size = STN_SQ_CTXT_SIZE(max_ctxts);

	return HINIC3_UCODE_CMD_MODIFY_QUEUE_CTX;
}

static uint8_t prepare_cmd_buf_modify_svlan(struct hinic3_cmd_buf *cmd_buf, uint16_t func_id,
					    uint16_t vlan_tag, uint16_t q_id, uint8_t vlan_mode)
{
	struct hinic3_stn_vlan_ctx *vlan_ctx = NULL;

	cmd_buf->size = sizeof(struct hinic3_stn_vlan_ctx);
	vlan_ctx = (struct hinic3_stn_vlan_ctx *)cmd_buf->buf;

	vlan_ctx->func_id = func_id;
	vlan_ctx->qid = q_id;
	vlan_ctx->vlan_id = vlan_tag;
	vlan_ctx->vlan_sel = 0; /* TPID0 in IPSU */
	vlan_ctx->vlan_mode = vlan_mode;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	hinic3_cpu_to_be32(vlan_ctx, sizeof(struct hinic3_stn_vlan_ctx));
	return HINIC3_UCODE_CMD_MODIFY_VLAN_CTX;
}

static uint8_t prepare_cmd_buf_set_rss_indir_table(struct hinic3_nic_dev *nic_dev __rte_unused,
						   const uint32_t *indir_table,
						   struct hinic3_cmd_buf *cmd_buf)
{
	uint32_t i, size;
	uint32_t *temp = NULL;
	struct nic_rss_indirect_tbl *indir_tbl = NULL;

	indir_tbl = (struct nic_rss_indirect_tbl *)cmd_buf->buf;
	cmd_buf->size = sizeof(struct nic_rss_indirect_tbl);
	memset(indir_tbl, 0, sizeof(*indir_tbl));

	for (i = 0; i < HINIC3_RSS_INDIR_SIZE; i++)
		indir_tbl->entry[i] = (uint16_t)(*(indir_table + i));
	size = (size_t)sizeof(indir_tbl->entry) / sizeof(uint32_t);
	temp = (uint32_t *)indir_tbl->entry;
	for (i = 0; i < size; i++) {
		rte_atomic_thread_fence(rte_memory_order_seq_cst);
		temp[i] = rte_cpu_to_be_32(temp[i]);
	}
	return HINIC3_UCODE_CMD_SET_RSS_INDIR_TABLE;
}

static uint8_t prepare_cmd_buf_get_rss_indir_table(struct hinic3_nic_dev *nic_dev,
						   struct hinic3_cmd_buf *cmd_buf)
{
	(void)nic_dev;
	memset(cmd_buf->buf, 0, cmd_buf->size);

	return HINIC3_UCODE_CMD_GET_RSS_INDIR_TABLE;
}

static void cmd_buf_to_rss_indir_table(const struct hinic3_cmd_buf *cmd_buf, uint32_t *indir_table)
{
	uint32_t i;
	uint16_t *indir_tbl = NULL;

	indir_tbl = (uint16_t *)cmd_buf->buf;
	for (i = 0; i < HINIC3_RSS_INDIR_SIZE; i++)
		indir_table[i] = *(indir_tbl + i);
}

static void
prepare_sq_ctxt_drop_and_prefetch(struct hinic3_sq_ctxt *sq_ctxt)
{
	sq_ctxt->pkt_drop_thd = SQ_CTXT_PKT_DROP_THD_SET(HINIC3_DEFAULT_DROP_THD_ON, THD_ON) |
				SQ_CTXT_PKT_DROP_THD_SET(HINIC3_DEFAULT_DROP_THD_OFF, THD_OFF);

	sq_ctxt->pref_cache = SQ_CTXT_PREF_SET(WQ_PREFETCH_MIN, CACHE_MIN) |
			      SQ_CTXT_PREF_SET(WQ_PREFETCH_MAX, CACHE_MAX) |
			      SQ_CTXT_PREF_SET(WQ_PREFETCH_THRESHOLD, CACHE_THRESHOLD);
}

static void
prepare_rq_ctxt_ceq_and_prefetch(struct hinic3_rq_ctxt *rq_ctxt, uint16_t wqe_type,
	uint16_t msix_entry_idx, bool support_rq_sw_compact_cqe, uint8_t intr_disable)
{
	uint16_t ceq_intr = intr_disable ? RQ_CTXT_INVALID_INTR_NUM : msix_entry_idx;

	rq_ctxt->ceq_attr = RQ_CTXT_CEQ_ATTR_SET(intr_disable, EN) |
			    RQ_CTXT_CEQ_ATTR_SET(0, INTR_ARM) |
			    RQ_CTXT_CEQ_ATTR_SET(ceq_intr, INTR);
	if (wqe_type == HINIC3_COMPACT_RQ_WQE && support_rq_sw_compact_cqe) {
		/*
		 * With the SW compact CQE, EN enables the CEQ context and must
		 * be set for the attributes below to take effect; it does not
		 * mask Rx interrupts. Interrupt delivery is controlled by the
		 * INTR field above, where RQ_CTXT_INVALID_INTR_NUM keeps the
		 * queue silent while a valid vector enables the interrupt.
		 */
		rq_ctxt->ceq_attr |= RQ_CTXT_CEQ_ATTR_SET(1, EN);
		rq_ctxt->ceq_attr |= RQ_CTXT_CEQ_ATTR_SET(1, CI_WR);
		rq_ctxt->ceq_attr |= RQ_CTXT_CEQ_ATTR_SET(1, INTR_ARM);
	}
	rq_ctxt->pref_cache = RQ_CTXT_PREF_SET(WQ_PREFETCH_MIN, CACHE_MIN) |
			      RQ_CTXT_PREF_SET(WQ_PREFETCH_MAX, CACHE_MAX) |
			      RQ_CTXT_PREF_SET(WQ_PREFETCH_THRESHOLD, CACHE_THRESHOLD);
}

const struct hinic3_nic_cmdq_ops hinic3_stn_cmdq_ops = {
	.prepare_cmd_buf_clean_tso_lro_space =    prepare_cmd_buf_clean_tso_lro_space,
	.prepare_cmd_buf_qp_context_multi_store = prepare_cmd_buf_qp_context_multi_store,
	.prepare_cmd_buf_modify_svlan =           prepare_cmd_buf_modify_svlan,
	.prepare_cmd_buf_set_rss_indir_table =    prepare_cmd_buf_set_rss_indir_table,
	.prepare_cmd_buf_get_rss_indir_table =    prepare_cmd_buf_get_rss_indir_table,
	.cmd_buf_to_rss_indir_table =             cmd_buf_to_rss_indir_table,
	.prepare_sq_ctxt_drop_and_prefetch =      prepare_sq_ctxt_drop_and_prefetch,
	.prepare_rq_ctxt_ceq_and_prefetch =       prepare_rq_ctxt_ceq_and_prefetch,
};
