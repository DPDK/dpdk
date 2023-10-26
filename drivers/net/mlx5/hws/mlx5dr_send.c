/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

struct mlx5dr_send_ring_dep_wqe *
mlx5dr_send_add_new_dep_wqe(struct mlx5dr_send_engine *queue)
{
	struct mlx5dr_send_ring_sq *send_sq = &queue->send_ring->send_sq;
	unsigned int idx = send_sq->head_dep_idx++ & (queue->num_entries - 1);

	memset(&send_sq->dep_wqe[idx].wqe_data.tag, 0, MLX5DR_MATCH_TAG_SZ);

	return &send_sq->dep_wqe[idx];
}

void mlx5dr_send_abort_new_dep_wqe(struct mlx5dr_send_engine *queue)
{
	queue->send_ring->send_sq.head_dep_idx--;
}

void mlx5dr_send_all_dep_wqe(struct mlx5dr_send_engine *queue)
{
	struct mlx5dr_send_ring_sq *send_sq = &queue->send_ring->send_sq;
	struct mlx5dr_send_ste_attr ste_attr = {0};
	struct mlx5dr_send_ring_dep_wqe *dep_wqe;

	ste_attr.send_attr.opmod = MLX5DR_WQE_GTA_OPMOD_STE;
	ste_attr.send_attr.opcode = MLX5DR_WQE_OPCODE_TBL_ACCESS;
	ste_attr.send_attr.len = MLX5DR_WQE_SZ_GTA_CTRL + MLX5DR_WQE_SZ_GTA_DATA;
	ste_attr.gta_opcode = MLX5DR_WQE_GTA_OP_ACTIVATE;

	/* Fence first from previous depend WQEs  */
	ste_attr.send_attr.fence = 1;

	while (send_sq->head_dep_idx != send_sq->tail_dep_idx) {
		dep_wqe = &send_sq->dep_wqe[send_sq->tail_dep_idx++ & (queue->num_entries - 1)];

		/* Notify HW on the last WQE */
		ste_attr.send_attr.notify_hw = (send_sq->tail_dep_idx == send_sq->head_dep_idx);
		ste_attr.send_attr.user_data = dep_wqe->user_data;
		ste_attr.send_attr.rule = dep_wqe->rule;

		ste_attr.rtc_0 = dep_wqe->rtc_0;
		ste_attr.rtc_1 = dep_wqe->rtc_1;
		ste_attr.retry_rtc_0 = dep_wqe->retry_rtc_0;
		ste_attr.retry_rtc_1 = dep_wqe->retry_rtc_1;
		ste_attr.used_id_rtc_0 = &dep_wqe->rule->rtc_0;
		ste_attr.used_id_rtc_1 = &dep_wqe->rule->rtc_1;
		ste_attr.wqe_ctrl = &dep_wqe->wqe_ctrl;
		ste_attr.wqe_data = &dep_wqe->wqe_data;

		mlx5dr_send_ste(queue, &ste_attr);

		/* Fencing is done only on the first WQE */
		ste_attr.send_attr.fence = 0;
	}
}

struct mlx5dr_send_engine_post_ctrl
mlx5dr_send_engine_post_start(struct mlx5dr_send_engine *queue)
{
	struct mlx5dr_send_engine_post_ctrl ctrl;

	ctrl.queue = queue;
	/* Currently only one send ring is supported */
	ctrl.send_ring = &queue->send_ring[0];
	ctrl.num_wqebbs = 0;

	return ctrl;
}

void mlx5dr_send_engine_post_req_wqe(struct mlx5dr_send_engine_post_ctrl *ctrl,
				     char **buf, size_t *len)
{
	struct mlx5dr_send_ring_sq *send_sq = &ctrl->send_ring->send_sq;
	unsigned int idx;

	idx = (send_sq->cur_post + ctrl->num_wqebbs) & send_sq->buf_mask;

	*buf = send_sq->buf + (idx << MLX5_SEND_WQE_SHIFT);
	*len = MLX5_SEND_WQE_BB;

	if (!ctrl->num_wqebbs) {
		*buf += sizeof(struct mlx5dr_wqe_ctrl_seg);
		*len -= sizeof(struct mlx5dr_wqe_ctrl_seg);
	}

	ctrl->num_wqebbs++;
}

static void mlx5dr_send_engine_post_ring(struct mlx5dr_send_ring_sq *sq,
					 struct mlx5dv_devx_uar *uar,
					 struct mlx5dr_wqe_ctrl_seg *wqe_ctrl)
{
	rte_compiler_barrier();
	sq->db[MLX5_SND_DBR] = rte_cpu_to_be_32(sq->cur_post);

	rte_wmb();
	mlx5dr_uar_write64_relaxed(*((uint64_t *)wqe_ctrl), uar->reg_addr);
	rte_wmb();
}

static void
mlx5dr_send_wqe_set_tag(struct mlx5dr_wqe_gta_data_seg_ste *wqe_data,
			struct mlx5dr_rule_match_tag *tag,
			bool is_jumbo)
{
	if (is_jumbo) {
		/* Clear previous possibly dirty control */
		memset(wqe_data, 0, MLX5DR_STE_CTRL_SZ);
		memcpy(wqe_data->jumbo, tag->jumbo, MLX5DR_JUMBO_TAG_SZ);
	} else {
		/* Clear previous possibly dirty control and actions */
		memset(wqe_data, 0, MLX5DR_STE_CTRL_SZ + MLX5DR_ACTIONS_SZ);
		memcpy(wqe_data->tag, tag->match, MLX5DR_MATCH_TAG_SZ);
	}
}

void mlx5dr_send_engine_post_end(struct mlx5dr_send_engine_post_ctrl *ctrl,
				 struct mlx5dr_send_engine_post_attr *attr)
{
	struct mlx5dr_wqe_ctrl_seg *wqe_ctrl;
	struct mlx5dr_send_ring_sq *sq;
	uint32_t flags = 0;
	unsigned int idx;

	sq = &ctrl->send_ring->send_sq;
	idx = sq->cur_post & sq->buf_mask;
	sq->last_idx = idx;

	wqe_ctrl = (void *)(sq->buf + (idx << MLX5_SEND_WQE_SHIFT));

	wqe_ctrl->opmod_idx_opcode =
		rte_cpu_to_be_32((attr->opmod << 24) |
				 ((sq->cur_post & 0xffff) << 8) |
				 attr->opcode);
	wqe_ctrl->qpn_ds =
		rte_cpu_to_be_32((attr->len + sizeof(struct mlx5dr_wqe_ctrl_seg)) / 16 |
				 sq->sqn << 8);

	wqe_ctrl->imm = rte_cpu_to_be_32(attr->id);

	flags |= attr->notify_hw ? MLX5_WQE_CTRL_CQ_UPDATE : 0;
	flags |= attr->fence ? MLX5_WQE_CTRL_INITIATOR_SMALL_FENCE : 0;
	wqe_ctrl->flags = rte_cpu_to_be_32(flags);

	sq->wr_priv[idx].id = attr->id;
	sq->wr_priv[idx].retry_id = attr->retry_id;

	sq->wr_priv[idx].rule = attr->rule;
	sq->wr_priv[idx].user_data = attr->user_data;
	sq->wr_priv[idx].num_wqebbs = ctrl->num_wqebbs;

	if (attr->rule) {
		sq->wr_priv[idx].rule->pending_wqes++;
		sq->wr_priv[idx].used_id = attr->used_id;
	}

	sq->cur_post += ctrl->num_wqebbs;

	if (attr->notify_hw)
		mlx5dr_send_engine_post_ring(sq, ctrl->queue->uar, wqe_ctrl);
}

static void mlx5dr_send_wqe(struct mlx5dr_send_engine *queue,
			    struct mlx5dr_send_engine_post_attr *send_attr,
			    struct mlx5dr_wqe_gta_ctrl_seg *send_wqe_ctrl,
			    void *send_wqe_data,
			    void *send_wqe_tag,
			    bool is_jumbo,
			    uint8_t gta_opcode,
			    uint32_t direct_index)
{
	struct mlx5dr_wqe_gta_data_seg_ste *wqe_data;
	struct mlx5dr_wqe_gta_ctrl_seg *wqe_ctrl;
	struct mlx5dr_send_engine_post_ctrl ctrl;
	size_t wqe_len;

	ctrl = mlx5dr_send_engine_post_start(queue);
	mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_ctrl, &wqe_len);
	mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_data, &wqe_len);

	wqe_ctrl->op_dirix = htobe32(gta_opcode << 28 | direct_index);
	memcpy(wqe_ctrl->stc_ix, send_wqe_ctrl->stc_ix, sizeof(send_wqe_ctrl->stc_ix));

	if (send_wqe_data)
		memcpy(wqe_data, send_wqe_data, sizeof(*wqe_data));
	else
		mlx5dr_send_wqe_set_tag(wqe_data, send_wqe_tag, is_jumbo);

	mlx5dr_send_engine_post_end(&ctrl, send_attr);
}

void mlx5dr_send_ste(struct mlx5dr_send_engine *queue,
		     struct mlx5dr_send_ste_attr *ste_attr)
{
	struct mlx5dr_send_engine_post_attr *send_attr = &ste_attr->send_attr;
	uint8_t notify_hw = send_attr->notify_hw;
	uint8_t fence = send_attr->fence;

	if (ste_attr->rtc_1) {
		send_attr->id = ste_attr->rtc_1;
		send_attr->used_id = ste_attr->used_id_rtc_1;
		send_attr->retry_id = ste_attr->retry_rtc_1;
		send_attr->fence = fence;
		send_attr->notify_hw = notify_hw && !ste_attr->rtc_0;
		mlx5dr_send_wqe(queue, send_attr,
				ste_attr->wqe_ctrl,
				ste_attr->wqe_data,
				ste_attr->wqe_tag,
				ste_attr->wqe_tag_is_jumbo,
				ste_attr->gta_opcode,
				ste_attr->direct_index);
	}

	if (ste_attr->rtc_0) {
		send_attr->id = ste_attr->rtc_0;
		send_attr->used_id = ste_attr->used_id_rtc_0;
		send_attr->retry_id = ste_attr->retry_rtc_0;
		send_attr->fence = fence && !ste_attr->rtc_1;
		send_attr->notify_hw = notify_hw;
		mlx5dr_send_wqe(queue, send_attr,
				ste_attr->wqe_ctrl,
				ste_attr->wqe_data,
				ste_attr->wqe_tag,
				ste_attr->wqe_tag_is_jumbo,
				ste_attr->gta_opcode,
				ste_attr->direct_index);
	}

	/* Restore to ortginal requested values */
	send_attr->notify_hw = notify_hw;
	send_attr->fence = fence;
}

static
int mlx5dr_send_wqe_fw(struct ibv_context *ibv_ctx,
		       uint32_t pd_num,
		       struct mlx5dr_send_engine_post_attr *send_attr,
		       struct mlx5dr_wqe_gta_ctrl_seg *send_wqe_ctrl,
		       void *send_wqe_match_data,
		       void *send_wqe_match_tag,
		       void *send_wqe_range_data,
		       void *send_wqe_range_tag,
		       bool is_jumbo,
		       uint8_t gta_opcode)
{
	bool has_range = send_wqe_range_data || send_wqe_range_tag;
	bool has_match = send_wqe_match_data || send_wqe_match_tag;
	struct mlx5dr_wqe_gta_data_seg_ste gta_wqe_data0 = {0};
	struct mlx5dr_wqe_gta_data_seg_ste gta_wqe_data1 = {0};
	struct mlx5dr_wqe_gta_ctrl_seg gta_wqe_ctrl = {0};
	struct mlx5dr_cmd_generate_wqe_attr attr = {0};
	struct mlx5dr_wqe_ctrl_seg wqe_ctrl = {0};
	struct mlx5_cqe64 cqe;
	uint32_t flags = 0;
	int ret;

	/* Set WQE control */
	wqe_ctrl.opmod_idx_opcode =
		rte_cpu_to_be_32((send_attr->opmod << 24) | send_attr->opcode);
	wqe_ctrl.qpn_ds =
		rte_cpu_to_be_32((send_attr->len + sizeof(struct mlx5dr_wqe_ctrl_seg)) / 16);
	flags |= send_attr->notify_hw ? MLX5_WQE_CTRL_CQ_UPDATE : 0;
	wqe_ctrl.flags = rte_cpu_to_be_32(flags);
	wqe_ctrl.imm = rte_cpu_to_be_32(send_attr->id);

	/* Set GTA WQE CTRL */
	memcpy(gta_wqe_ctrl.stc_ix, send_wqe_ctrl->stc_ix, sizeof(send_wqe_ctrl->stc_ix));
	gta_wqe_ctrl.op_dirix = htobe32(gta_opcode << 28);

	/* Set GTA match WQE DATA */
	if (has_match) {
		if (send_wqe_match_data)
			memcpy(&gta_wqe_data0, send_wqe_match_data, sizeof(gta_wqe_data0));
		else
			mlx5dr_send_wqe_set_tag(&gta_wqe_data0, send_wqe_match_tag, is_jumbo);

		gta_wqe_data0.rsvd1_definer = htobe32(send_attr->match_definer_id << 8);
		attr.gta_data_0 = (uint8_t *)&gta_wqe_data0;
	}

	/* Set GTA range WQE DATA */
	if (has_range) {
		if (send_wqe_range_data)
			memcpy(&gta_wqe_data1, send_wqe_range_data, sizeof(gta_wqe_data1));
		else
			mlx5dr_send_wqe_set_tag(&gta_wqe_data1, send_wqe_range_tag, false);

		gta_wqe_data1.rsvd1_definer = htobe32(send_attr->range_definer_id << 8);
		attr.gta_data_1 = (uint8_t *)&gta_wqe_data1;
	}

	attr.pdn = pd_num;
	attr.wqe_ctrl = (uint8_t *)&wqe_ctrl;
	attr.gta_ctrl = (uint8_t *)&gta_wqe_ctrl;

send_wqe:
	ret = mlx5dr_cmd_generate_wqe(ibv_ctx, &attr, &cqe);
	if (ret) {
		DR_LOG(ERR, "Failed to write WQE using command");
		return ret;
	}

	if ((mlx5dv_get_cqe_opcode(&cqe) == MLX5_CQE_REQ) &&
	    (rte_be_to_cpu_32(cqe.byte_cnt) >> 31 == 0)) {
		*send_attr->used_id = send_attr->id;
		return 0;
	}

	/* Retry if rule failed */
	if (send_attr->retry_id) {
		wqe_ctrl.imm = rte_cpu_to_be_32(send_attr->retry_id);
		send_attr->id = send_attr->retry_id;
		send_attr->retry_id = 0;
		goto send_wqe;
	}

	return -1;
}

void mlx5dr_send_stes_fw(struct mlx5dr_send_engine *queue,
			 struct mlx5dr_send_ste_attr *ste_attr)
{
	struct mlx5dr_send_engine_post_attr *send_attr = &ste_attr->send_attr;
	struct mlx5dr_rule *rule = send_attr->rule;
	struct ibv_context *ibv_ctx;
	struct mlx5dr_context *ctx;
	uint16_t queue_id;
	uint32_t pdn;
	int ret;

	ctx = rule->matcher->tbl->ctx;
	queue_id = queue - ctx->send_queue;
	ibv_ctx = ctx->ibv_ctx;
	pdn = ctx->pd_num;

	/* Writing through FW can't HW fence, therefore we drain the queue */
	if (send_attr->fence)
		mlx5dr_send_queue_action(ctx,
					 queue_id,
					 MLX5DR_SEND_QUEUE_ACTION_DRAIN_SYNC);

	if (ste_attr->rtc_1) {
		send_attr->id = ste_attr->rtc_1;
		send_attr->used_id = ste_attr->used_id_rtc_1;
		send_attr->retry_id = ste_attr->retry_rtc_1;
		ret = mlx5dr_send_wqe_fw(ibv_ctx, pdn, send_attr,
					 ste_attr->wqe_ctrl,
					 ste_attr->wqe_data,
					 ste_attr->wqe_tag,
					 ste_attr->range_wqe_data,
					 ste_attr->range_wqe_tag,
					 ste_attr->wqe_tag_is_jumbo,
					 ste_attr->gta_opcode);
		if (ret)
			goto fail_rule;
	}

	if (ste_attr->rtc_0) {
		send_attr->id = ste_attr->rtc_0;
		send_attr->used_id = ste_attr->used_id_rtc_0;
		send_attr->retry_id = ste_attr->retry_rtc_0;
		ret = mlx5dr_send_wqe_fw(ibv_ctx, pdn, send_attr,
					 ste_attr->wqe_ctrl,
					 ste_attr->wqe_data,
					 ste_attr->wqe_tag,
					 ste_attr->range_wqe_data,
					 ste_attr->range_wqe_tag,
					 ste_attr->wqe_tag_is_jumbo,
					 ste_attr->gta_opcode);
		if (ret)
			goto fail_rule;
	}

	/* Increase the status, this only works on good flow as the enum
	 * is arrange it away creating -> created -> deleting -> deleted
	 */
	rule->status++;
	mlx5dr_send_engine_gen_comp(queue, send_attr->user_data, RTE_FLOW_OP_SUCCESS);
	return;

fail_rule:
	rule->status = !rule->rtc_0 && !rule->rtc_1 ?
		MLX5DR_RULE_STATUS_FAILED : MLX5DR_RULE_STATUS_FAILING;
	mlx5dr_send_engine_gen_comp(queue, send_attr->user_data, RTE_FLOW_OP_ERROR);
}

static void mlx5dr_send_engine_retry_post_send(struct mlx5dr_send_engine *queue,
					       struct mlx5dr_send_ring_priv *priv,
					       uint16_t wqe_cnt)
{
	struct mlx5dr_send_engine_post_attr send_attr = {0};
	struct mlx5dr_wqe_gta_data_seg_ste *wqe_data;
	struct mlx5dr_wqe_gta_ctrl_seg *wqe_ctrl;
	struct mlx5dr_send_engine_post_ctrl ctrl;
	struct mlx5dr_send_ring_sq *send_sq;
	unsigned int idx;
	size_t wqe_len;
	char *p;

	send_attr.rule = priv->rule;
	send_attr.opcode = MLX5DR_WQE_OPCODE_TBL_ACCESS;
	send_attr.opmod = MLX5DR_WQE_GTA_OPMOD_STE;
	send_attr.len = MLX5_SEND_WQE_BB * 2 - sizeof(struct mlx5dr_wqe_ctrl_seg);
	send_attr.notify_hw = 1;
	send_attr.fence = 0;
	send_attr.user_data = priv->user_data;
	send_attr.id = priv->retry_id;
	send_attr.used_id = priv->used_id;

	ctrl = mlx5dr_send_engine_post_start(queue);
	mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_ctrl, &wqe_len);
	mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_data, &wqe_len);

	send_sq = &ctrl.send_ring->send_sq;
	idx = wqe_cnt & send_sq->buf_mask;
	p = send_sq->buf + (idx << MLX5_SEND_WQE_SHIFT);

	/* Copy old gta ctrl */
	memcpy(wqe_ctrl, p + sizeof(struct mlx5dr_wqe_ctrl_seg),
	       MLX5_SEND_WQE_BB - sizeof(struct mlx5dr_wqe_ctrl_seg));

	idx = (wqe_cnt + 1) & send_sq->buf_mask;
	p = send_sq->buf + (idx << MLX5_SEND_WQE_SHIFT);

	/* Copy old gta data */
	memcpy(wqe_data, p, MLX5_SEND_WQE_BB);

	mlx5dr_send_engine_post_end(&ctrl, &send_attr);
}

void mlx5dr_send_engine_flush_queue(struct mlx5dr_send_engine *queue)
{
	struct mlx5dr_send_ring_sq *sq = &queue->send_ring[0].send_sq;
	struct mlx5dr_wqe_ctrl_seg *wqe_ctrl;

	wqe_ctrl = (void *)(sq->buf + (sq->last_idx << MLX5_SEND_WQE_SHIFT));

	wqe_ctrl->flags |= rte_cpu_to_be_32(MLX5_WQE_CTRL_CQ_UPDATE);

	mlx5dr_send_engine_post_ring(sq, queue->uar, wqe_ctrl);
}

static void mlx5dr_send_engine_update_rule(struct mlx5dr_send_engine *queue,
					   struct mlx5dr_send_ring_priv *priv,
					   uint16_t wqe_cnt,
					   enum rte_flow_op_status *status)
{
	priv->rule->pending_wqes--;

	if (*status == RTE_FLOW_OP_ERROR) {
		if (priv->retry_id) {
			mlx5dr_send_engine_retry_post_send(queue, priv, wqe_cnt);
			return;
		}
		/* Some part of the rule failed */
		priv->rule->status = MLX5DR_RULE_STATUS_FAILING;
		*priv->used_id = 0;
	} else {
		*priv->used_id = priv->id;
	}

	/* Update rule status for the last completion */
	if (!priv->rule->pending_wqes) {
		if (unlikely(priv->rule->status == MLX5DR_RULE_STATUS_FAILING)) {
			/* Rule completely failed and doesn't require cleanup */
			if (!priv->rule->rtc_0 && !priv->rule->rtc_1)
				priv->rule->status = MLX5DR_RULE_STATUS_FAILED;

			*status = RTE_FLOW_OP_ERROR;
		} else {
			/* Increase the status, this only works on good flow as the enum
			 * is arrange it away creating -> created -> deleting -> deleted
			 */
			priv->rule->status++;
			*status = RTE_FLOW_OP_SUCCESS;
			/* Rule was deleted now we can safely release action STEs */
			if (priv->rule->status == MLX5DR_RULE_STATUS_DELETED)
				mlx5dr_rule_free_action_ste_idx(priv->rule);
		}
	}
}

static void mlx5dr_send_engine_update(struct mlx5dr_send_engine *queue,
				      struct mlx5_cqe64 *cqe,
				      struct mlx5dr_send_ring_priv *priv,
				      struct rte_flow_op_result res[],
				      int64_t *i,
				      uint32_t res_nb,
				      uint16_t wqe_cnt)
{
	enum rte_flow_op_status status;

	if (!cqe || (likely(rte_be_to_cpu_32(cqe->byte_cnt) >> 31 == 0) &&
	    likely(mlx5dv_get_cqe_opcode(cqe) == MLX5_CQE_REQ))) {
		status = RTE_FLOW_OP_SUCCESS;
	} else {
		status = RTE_FLOW_OP_ERROR;
	}

	if (priv->user_data) {
		if (priv->rule) {
			mlx5dr_send_engine_update_rule(queue, priv, wqe_cnt, &status);
			/* Completion is provided on the last rule WQE */
			if (priv->rule->pending_wqes)
				return;
		}

		if (*i < res_nb) {
			res[*i].user_data = priv->user_data;
			res[*i].status = status;
			(*i)++;
			mlx5dr_send_engine_dec_rule(queue);
		} else {
			mlx5dr_send_engine_gen_comp(queue, priv->user_data, status);
		}
	}
}

static void mlx5dr_send_engine_poll_cq(struct mlx5dr_send_engine *queue,
				       struct mlx5dr_send_ring *send_ring,
				       struct rte_flow_op_result res[],
				       int64_t *i,
				       uint32_t res_nb)
{
	struct mlx5dr_send_ring_cq *cq = &send_ring->send_cq;
	struct mlx5dr_send_ring_sq *sq = &send_ring->send_sq;
	uint32_t cq_idx = cq->cons_index & cq->ncqe_mask;
	struct mlx5dr_send_ring_priv *priv;
	struct mlx5_cqe64 *cqe;
	uint32_t offset_cqe64;
	uint8_t cqe_opcode;
	uint8_t cqe_owner;
	uint16_t wqe_cnt;
	uint8_t sw_own;

	offset_cqe64 = RTE_CACHE_LINE_SIZE - sizeof(struct mlx5_cqe64);
	cqe = (void *)(cq->buf + (cq_idx << cq->cqe_log_sz) + offset_cqe64);

	sw_own = (cq->cons_index & cq->ncqe) ? 1 : 0;
	cqe_opcode = mlx5dv_get_cqe_opcode(cqe);
	cqe_owner = mlx5dv_get_cqe_owner(cqe);

	if (cqe_opcode == MLX5_CQE_INVALID ||
	    cqe_owner != sw_own)
		return;

	if (unlikely(mlx5dv_get_cqe_opcode(cqe) != MLX5_CQE_REQ))
		queue->err = true;

	rte_io_rmb();

	wqe_cnt = be16toh(cqe->wqe_counter) & sq->buf_mask;

	while (cq->poll_wqe != wqe_cnt) {
		priv = &sq->wr_priv[cq->poll_wqe];
		mlx5dr_send_engine_update(queue, NULL, priv, res, i, res_nb, 0);
		cq->poll_wqe = (cq->poll_wqe + priv->num_wqebbs) & sq->buf_mask;
	}

	priv = &sq->wr_priv[wqe_cnt];
	cq->poll_wqe = (wqe_cnt + priv->num_wqebbs) & sq->buf_mask;
	mlx5dr_send_engine_update(queue, cqe, priv, res, i, res_nb, wqe_cnt);
	cq->cons_index++;
}

static void mlx5dr_send_engine_poll_cqs(struct mlx5dr_send_engine *queue,
					struct rte_flow_op_result res[],
					int64_t *polled,
					uint32_t res_nb)
{
	int j;

	for (j = 0; j < MLX5DR_NUM_SEND_RINGS; j++) {
		mlx5dr_send_engine_poll_cq(queue, &queue->send_ring[j],
					   res, polled, res_nb);

		*queue->send_ring[j].send_cq.db =
			htobe32(queue->send_ring[j].send_cq.cons_index & 0xffffff);
	}
}

static void mlx5dr_send_engine_poll_list(struct mlx5dr_send_engine *queue,
					 struct rte_flow_op_result res[],
					 int64_t *polled,
					 uint32_t res_nb)
{
	struct mlx5dr_completed_poll *comp = &queue->completed;

	while (comp->ci != comp->pi) {
		if (*polled < res_nb) {
			res[*polled].status =
				comp->entries[comp->ci].status;
			res[*polled].user_data =
				comp->entries[comp->ci].user_data;
			(*polled)++;
			comp->ci = (comp->ci + 1) & comp->mask;
			mlx5dr_send_engine_dec_rule(queue);
		} else {
			return;
		}
	}
}

static int mlx5dr_send_engine_poll(struct mlx5dr_send_engine *queue,
				   struct rte_flow_op_result res[],
				   uint32_t res_nb)
{
	int64_t polled = 0;

	mlx5dr_send_engine_poll_list(queue, res, &polled, res_nb);

	if (polled >= res_nb)
		return polled;

	mlx5dr_send_engine_poll_cqs(queue, res, &polled, res_nb);

	return polled;
}

int mlx5dr_send_queue_poll(struct mlx5dr_context *ctx,
			   uint16_t queue_id,
			   struct rte_flow_op_result res[],
			   uint32_t res_nb)
{
	return mlx5dr_send_engine_poll(&ctx->send_queue[queue_id],
				       res, res_nb);
}

static int mlx5dr_send_ring_create_sq_obj(struct mlx5dr_context *ctx,
					  struct mlx5dr_send_engine *queue,
					  struct mlx5dr_send_ring_sq *sq,
					  struct mlx5dr_send_ring_cq *cq,
					  size_t log_wq_sz)
{
	struct mlx5dr_cmd_sq_create_attr attr = {0};
	int err;

	attr.cqn = cq->cqn;
	attr.pdn = ctx->pd_num;
	attr.page_id = queue->uar->page_id;
	attr.dbr_id = sq->db_umem->umem_id;
	attr.wq_id = sq->buf_umem->umem_id;
	attr.log_wq_sz = log_wq_sz;
	if (ctx->caps->sq_ts_format == MLX5_HCA_CAP_TIMESTAMP_FORMAT_FR)
		attr.ts_format = MLX5_QPC_TIMESTAMP_FORMAT_FREE_RUNNING;
	else
		attr.ts_format = MLX5_QPC_TIMESTAMP_FORMAT_DEFAULT;

	sq->obj = mlx5dr_cmd_sq_create(ctx->ibv_ctx, &attr);
	if (!sq->obj)
		return rte_errno;

	sq->sqn = sq->obj->id;

	err = mlx5dr_cmd_sq_modify_rdy(sq->obj);
	if (err)
		goto free_sq;

	return 0;

free_sq:
	mlx5dr_cmd_destroy_obj(sq->obj);

	return err;
}

static int mlx5dr_send_ring_open_sq(struct mlx5dr_context *ctx,
				    struct mlx5dr_send_engine *queue,
				    struct mlx5dr_send_ring_sq *sq,
				    struct mlx5dr_send_ring_cq *cq)
{
	size_t sq_log_buf_sz;
	size_t buf_aligned;
	size_t sq_buf_sz;
	size_t page_size;
	size_t buf_sz;
	int err;

	buf_sz = queue->num_entries * MAX_WQES_PER_RULE;
	sq_log_buf_sz = log2above(buf_sz);
	sq_buf_sz = 1 << (sq_log_buf_sz + log2above(MLX5_SEND_WQE_BB));
	sq->reg_addr = queue->uar->reg_addr;

	page_size = sysconf(_SC_PAGESIZE);
	buf_aligned = align(sq_buf_sz, page_size);
	err = posix_memalign((void **)&sq->buf, page_size, buf_aligned);
	if (err) {
		rte_errno = ENOMEM;
		return err;
	}
	memset(sq->buf, 0, buf_aligned);

	err = posix_memalign((void **)&sq->db, 8, 8);
	if (err)
		goto free_buf;

	sq->buf_umem = mlx5_glue->devx_umem_reg(ctx->ibv_ctx, sq->buf, sq_buf_sz, 0);

	if (!sq->buf_umem) {
		err = errno;
		goto free_db;
	}

	sq->db_umem = mlx5_glue->devx_umem_reg(ctx->ibv_ctx, sq->db, 8, 0);
	if (!sq->db_umem) {
		err = errno;
		goto free_buf_umem;
	}

	err = mlx5dr_send_ring_create_sq_obj(ctx, queue, sq, cq, sq_log_buf_sz);

	if (err)
		goto free_db_umem;

	sq->wr_priv = simple_malloc(sizeof(*sq->wr_priv) * buf_sz);
	if (!sq->wr_priv) {
		err = ENOMEM;
		goto destroy_sq_obj;
	}

	sq->dep_wqe = simple_calloc(queue->num_entries, sizeof(*sq->dep_wqe));
	if (!sq->dep_wqe) {
		err = ENOMEM;
		goto destroy_wr_priv;
	}

	sq->buf_mask = buf_sz - 1;

	return 0;

destroy_wr_priv:
	simple_free(sq->wr_priv);
destroy_sq_obj:
	mlx5dr_cmd_destroy_obj(sq->obj);
free_db_umem:
	mlx5_glue->devx_umem_dereg(sq->db_umem);
free_buf_umem:
	mlx5_glue->devx_umem_dereg(sq->buf_umem);
free_db:
	free(sq->db);
free_buf:
	free(sq->buf);
	rte_errno = err;
	return err;
}

static void mlx5dr_send_ring_close_sq(struct mlx5dr_send_ring_sq *sq)
{
	simple_free(sq->dep_wqe);
	mlx5dr_cmd_destroy_obj(sq->obj);
	mlx5_glue->devx_umem_dereg(sq->db_umem);
	mlx5_glue->devx_umem_dereg(sq->buf_umem);
	simple_free(sq->wr_priv);
	free(sq->db);
	free(sq->buf);
}

static int mlx5dr_send_ring_open_cq(struct mlx5dr_context *ctx,
				    struct mlx5dr_send_engine *queue,
				    struct mlx5dr_send_ring_cq *cq)
{
	struct mlx5dv_cq mlx5_cq = {0};
	struct mlx5dv_obj obj;
	struct ibv_cq *ibv_cq;
	size_t cq_size;
	int err;

	cq_size = queue->num_entries;
	ibv_cq = mlx5_glue->create_cq(ctx->ibv_ctx, cq_size, NULL, NULL, 0);
	if (!ibv_cq) {
		DR_LOG(ERR, "Failed to create CQ");
		rte_errno = errno;
		return rte_errno;
	}

	obj.cq.in = ibv_cq;
	obj.cq.out = &mlx5_cq;
	err = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_CQ);
	if (err) {
		err = errno;
		goto close_cq;
	}

	cq->buf = mlx5_cq.buf;
	cq->db = mlx5_cq.dbrec;
	cq->ncqe = mlx5_cq.cqe_cnt;
	cq->cqe_sz = mlx5_cq.cqe_size;
	cq->cqe_log_sz = log2above(cq->cqe_sz);
	cq->ncqe_mask = cq->ncqe - 1;
	cq->buf_sz = cq->cqe_sz * cq->ncqe;
	cq->cqn = mlx5_cq.cqn;
	cq->ibv_cq = ibv_cq;

	return 0;

close_cq:
	mlx5_glue->destroy_cq(ibv_cq);
	rte_errno = err;
	return err;
}

static void mlx5dr_send_ring_close_cq(struct mlx5dr_send_ring_cq *cq)
{
	mlx5_glue->destroy_cq(cq->ibv_cq);
}

static void mlx5dr_send_ring_close(struct mlx5dr_send_ring *ring)
{
	mlx5dr_send_ring_close_sq(&ring->send_sq);
	mlx5dr_send_ring_close_cq(&ring->send_cq);
}

static int mlx5dr_send_ring_open(struct mlx5dr_context *ctx,
				 struct mlx5dr_send_engine *queue,
				 struct mlx5dr_send_ring *ring)
{
	int err;

	err = mlx5dr_send_ring_open_cq(ctx, queue, &ring->send_cq);
	if (err)
		return err;

	err = mlx5dr_send_ring_open_sq(ctx, queue, &ring->send_sq, &ring->send_cq);
	if (err)
		goto close_cq;

	return err;

close_cq:
	mlx5dr_send_ring_close_cq(&ring->send_cq);

	return err;
}

static void __mlx5dr_send_rings_close(struct mlx5dr_send_engine *queue,
				      uint16_t i)
{
	while (i--)
		mlx5dr_send_ring_close(&queue->send_ring[i]);
}

static void mlx5dr_send_rings_close(struct mlx5dr_send_engine *queue)
{
	__mlx5dr_send_rings_close(queue, queue->rings);
}

static int mlx5dr_send_rings_open(struct mlx5dr_context *ctx,
				  struct mlx5dr_send_engine *queue)
{
	uint16_t i;
	int err;

	for (i = 0; i < queue->rings; i++) {
		err = mlx5dr_send_ring_open(ctx, queue, &queue->send_ring[i]);
		if (err)
			goto free_rings;
	}

	return 0;

free_rings:
	__mlx5dr_send_rings_close(queue, i);

	return err;
}

void mlx5dr_send_queue_close(struct mlx5dr_send_engine *queue)
{
	mlx5dr_send_rings_close(queue);
	simple_free(queue->completed.entries);
	mlx5_glue->devx_free_uar(queue->uar);
}

int mlx5dr_send_queue_open(struct mlx5dr_context *ctx,
			   struct mlx5dr_send_engine *queue,
			   uint16_t queue_size)
{
	struct mlx5dv_devx_uar *uar;
	int err;

#ifdef MLX5DV_UAR_ALLOC_TYPE_NC
	uar = mlx5_glue->devx_alloc_uar(ctx->ibv_ctx, MLX5_IB_UAPI_UAR_ALLOC_TYPE_NC);
	if (!uar) {
		rte_errno = errno;
		return rte_errno;
	}
#else
	uar = NULL;
	rte_errno = ENOTSUP;
	return rte_errno;
#endif

	queue->uar = uar;
	queue->rings = MLX5DR_NUM_SEND_RINGS;
	queue->num_entries = roundup_pow_of_two(queue_size);
	queue->used_entries = 0;
	queue->th_entries = queue->num_entries;

	queue->completed.entries = simple_calloc(queue->num_entries,
						 sizeof(queue->completed.entries[0]));
	if (!queue->completed.entries) {
		rte_errno = ENOMEM;
		goto free_uar;
	}
	queue->completed.pi = 0;
	queue->completed.ci = 0;
	queue->completed.mask = queue->num_entries - 1;

	err = mlx5dr_send_rings_open(ctx, queue);
	if (err)
		goto free_completed_entries;

	return 0;

free_completed_entries:
	simple_free(queue->completed.entries);
free_uar:
	mlx5_glue->devx_free_uar(uar);
	return rte_errno;
}

static void __mlx5dr_send_queues_close(struct mlx5dr_context *ctx, uint16_t queues)
{
	struct mlx5dr_send_engine *queue;

	while (queues--) {
		queue = &ctx->send_queue[queues];

		mlx5dr_send_queue_close(queue);
	}
}

void mlx5dr_send_queues_close(struct mlx5dr_context *ctx)
{
	__mlx5dr_send_queues_close(ctx, ctx->queues);
	simple_free(ctx->send_queue);
}

int mlx5dr_send_queues_open(struct mlx5dr_context *ctx,
			    uint16_t queues,
			    uint16_t queue_size)
{
	int err = 0;
	uint32_t i;

	/* Open one extra queue for control path */
	ctx->queues = queues + 1;

	ctx->send_queue = simple_calloc(ctx->queues, sizeof(*ctx->send_queue));
	if (!ctx->send_queue) {
		rte_errno = ENOMEM;
		return rte_errno;
	}

	for (i = 0; i < ctx->queues; i++) {
		err = mlx5dr_send_queue_open(ctx, &ctx->send_queue[i], queue_size);
		if (err)
			goto close_send_queues;
	}

	return 0;

close_send_queues:
	 __mlx5dr_send_queues_close(ctx, i);

	simple_free(ctx->send_queue);

	return err;
}

int mlx5dr_send_queue_action(struct mlx5dr_context *ctx,
			     uint16_t queue_id,
			     uint32_t actions)
{
	struct mlx5dr_send_ring_sq *send_sq;
	struct mlx5dr_send_engine *queue;
	bool wait_comp = false;
	int64_t polled = 0;

	queue = &ctx->send_queue[queue_id];
	send_sq = &queue->send_ring->send_sq;

	switch (actions) {
	case MLX5DR_SEND_QUEUE_ACTION_DRAIN_SYNC:
		wait_comp = true;
		/* FALLTHROUGH */
	case MLX5DR_SEND_QUEUE_ACTION_DRAIN_ASYNC:
		if (send_sq->head_dep_idx != send_sq->tail_dep_idx)
			/* Send dependent WQEs to drain the queue */
			mlx5dr_send_all_dep_wqe(queue);
		else
			/* Signal on the last posted WQE */
			mlx5dr_send_engine_flush_queue(queue);

		/* Poll queue until empty */
		while (wait_comp && !mlx5dr_send_engine_empty(queue))
			mlx5dr_send_engine_poll_cqs(queue, NULL, &polled, 0);

		break;
	default:
		rte_errno = EINVAL;
		return -rte_errno;
	}

	return 0;
}
