/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_nic_cfg.h"
#include "hinic3_cmd.h"
#include "hinic3_hwif.h"
#include "hinic3_htn_cmdq.h"

#define HTN_SQ_CTXT_SIZE(num_sqs)	((uint16_t)(sizeof(struct hinic3_htn_qp_ctxt_header) \
						    + (num_sqs) * sizeof(struct hinic3_sq_ctxt)))
#define HTN_RQ_CTXT_SIZE(num_rqs)	((uint16_t)(sizeof(struct hinic3_htn_qp_ctxt_header) \
						    + (num_rqs) * sizeof(struct hinic3_rq_ctxt)))

static uint8_t prepare_cmd_buf_clean_tso_lro_space(struct hinic3_nic_dev *nic_dev,
						   struct hinic3_cmd_buf *cmd_buf,
						   enum hinic3_qp_ctxt_type ctxt_type)
{
	struct hinic3_htn_clean_queue_ctxt *ctxt_block = NULL;

	ctxt_block = cmd_buf->buf;
	ctxt_block->cmdq_hdr.num_queues = nic_dev->max_sqs;
	ctxt_block->cmdq_hdr.queue_type = ctxt_type;
	ctxt_block->cmdq_hdr.start_qid = 0;
	ctxt_block->cmdq_hdr.dest_func_id = hinic3_global_func_id(nic_dev->hwdev);

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	hinic3_cpu_to_be32(ctxt_block, sizeof(*ctxt_block));

	cmd_buf->size = sizeof(*ctxt_block);
	return HINIC3_HTN_CMD_TSO_LRO_SPACE_CLEAN;
}

static void qp_prepare_cmdq_header(struct hinic3_htn_qp_ctxt_header *qp_ctxt_hdr,
				   enum hinic3_qp_ctxt_type ctxt_type, uint16_t num_queues,
				   uint16_t q_id, uint16_t func_id)
{
	qp_ctxt_hdr->queue_type = ctxt_type;
	qp_ctxt_hdr->num_queues = num_queues;
	qp_ctxt_hdr->start_qid = q_id;
	qp_ctxt_hdr->dest_func_id = func_id;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	hinic3_cpu_to_be32(qp_ctxt_hdr, sizeof(*qp_ctxt_hdr));
}

static uint8_t prepare_cmd_buf_qp_context_multi_store(struct hinic3_nic_dev *nic_dev,
						      struct hinic3_cmd_buf *cmd_buf,
						      enum hinic3_qp_ctxt_type ctxt_type,
						      uint16_t start_qid, uint16_t max_ctxts)
{
	struct hinic3_htn_qp_ctxt_block *qp_ctxt_block = NULL;
	uint16_t func_id;
	uint16_t i;

	qp_ctxt_block = cmd_buf->buf;
	func_id = hinic3_global_func_id(nic_dev->hwdev);
	qp_prepare_cmdq_header(&qp_ctxt_block->cmdq_hdr, ctxt_type,
			       max_ctxts, start_qid, func_id);

	for (i = 0; i < max_ctxts; i++) {
		if (ctxt_type == HINIC3_QP_CTXT_TYPE_RQ)
			hinic3_rq_prepare_ctxt(nic_dev->rxqs[start_qid + i],
					       &qp_ctxt_block->rq_ctxt[i]);
		else
			hinic3_sq_prepare_ctxt(nic_dev->txqs[start_qid + i],
					       start_qid + i,
					       &qp_ctxt_block->sq_ctxt[i]);
	}

	if (ctxt_type == HINIC3_QP_CTXT_TYPE_RQ)
		cmd_buf->size = HTN_RQ_CTXT_SIZE(max_ctxts);
	else
		cmd_buf->size = HTN_SQ_CTXT_SIZE(max_ctxts);

	return HINIC3_HTN_CMD_SQ_RQ_CONTEXT_MULTI_ST;
}

static uint8_t prepare_cmd_buf_modify_svlan(struct hinic3_cmd_buf *cmd_buf,
			uint16_t func_id, uint16_t vlan_tag, uint16_t q_id, uint8_t vlan_mode)
{
	struct hinic3_htn_vlan_ctx *vlan_ctx = NULL;

	cmd_buf->size = sizeof(struct hinic3_htn_vlan_ctx);
	vlan_ctx = (struct hinic3_htn_vlan_ctx *)cmd_buf->buf;

	vlan_ctx->dest_func_id = func_id;
	vlan_ctx->start_qid = q_id;
	vlan_ctx->vlan_tag = vlan_tag;
	vlan_ctx->vlan_sel = 0; /* TPID0 in IPSU */
	vlan_ctx->vlan_mode = vlan_mode;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	hinic3_cpu_to_be32(vlan_ctx, sizeof(struct hinic3_htn_vlan_ctx));
	return HINIC3_HTN_CMD_SVLAN_MODIFY;
}

static void prepare_rss_indir_table_cmd_header(struct hinic3_nic_dev *nic_dev,
					       struct hinic3_cmd_buf *cmd_buf)
{
	struct hinic3_rss_cmd_header *header = cmd_buf->buf;

	header->dest_func_id = hinic3_global_func_id(nic_dev->hwdev);

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	hinic3_cpu_to_be32(header, sizeof(*header));
}

static uint8_t prepare_cmd_buf_set_rss_indir_table(struct hinic3_nic_dev *nic_dev,
						   const uint32_t *indir_table,
						   struct hinic3_cmd_buf *cmd_buf)
{
	uint32_t i;
	uint8_t *indir_tbl = NULL;

	indir_tbl = (uint8_t *)cmd_buf->buf + sizeof(struct hinic3_rss_cmd_header);
	cmd_buf->size = sizeof(struct hinic3_rss_cmd_header) + HINIC3_RSS_INDIR_SIZE;
	memset(indir_tbl, 0, HINIC3_RSS_INDIR_SIZE);

	prepare_rss_indir_table_cmd_header(nic_dev, cmd_buf);

	for (i = 0; i < HINIC3_RSS_INDIR_SIZE; i++)
		indir_tbl[i] = (uint8_t)(*(indir_table + i));

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	hinic3_cpu_to_be32(indir_tbl, HINIC3_RSS_INDIR_SIZE);

	return HINIC3_HTN_CMD_SET_RSS_INDIR_TABLE;
}

static uint8_t prepare_cmd_buf_get_rss_indir_table(struct hinic3_nic_dev *nic_dev,
						   struct hinic3_cmd_buf *cmd_buf)
{
	memset(cmd_buf->buf, 0, cmd_buf->size);
	prepare_rss_indir_table_cmd_header(nic_dev, cmd_buf);

	return HINIC3_HTN_CMD_GET_RSS_INDIR_TABLE;
}

static void cmd_buf_to_rss_indir_table(const struct hinic3_cmd_buf *cmd_buf, uint32_t *indir_table)
{
	uint32_t i;
	uint8_t *indir_tbl = NULL;

	indir_tbl = (uint8_t *)cmd_buf->buf;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	hinic3_be32_to_cpu(cmd_buf->buf, HINIC3_RSS_INDIR_SIZE);
	for (i = 0; i < HINIC3_RSS_INDIR_SIZE; i++)
		indir_table[i] = *(indir_tbl + i);
}

struct hinic3_nic_cmdq_ops *hinic3_nic_cmdq_get_htn_ops(void)
{
	static struct hinic3_nic_cmdq_ops cmdq_ops = {
		.prepare_cmd_buf_clean_tso_lro_space =    prepare_cmd_buf_clean_tso_lro_space,
		.prepare_cmd_buf_qp_context_multi_store = prepare_cmd_buf_qp_context_multi_store,
		.prepare_cmd_buf_modify_svlan =           prepare_cmd_buf_modify_svlan,
		.prepare_cmd_buf_set_rss_indir_table =    prepare_cmd_buf_set_rss_indir_table,
		.prepare_cmd_buf_get_rss_indir_table =    prepare_cmd_buf_get_rss_indir_table,
		.cmd_buf_to_rss_indir_table =             cmd_buf_to_rss_indir_table,
	};

	return &cmdq_ops;
}
