/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Nvidia Inc. All rights reserved.
 */

#include <stddef.h>
#include <rte_eal_paging.h>

#include "mlx5_utils.h"
#include "mlx5_flow.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)

typedef void (*quota_wqe_cmd_t)(volatile struct mlx5_aso_wqe *restrict,
				struct mlx5_quota_ctx *, uint32_t, uint32_t,
				void *);

#define MLX5_ASO_MTR1_INIT_MASK 0xffffffffULL
#define MLX5_ASO_MTR0_INIT_MASK ((MLX5_ASO_MTR1_INIT_MASK) << 32)

static __rte_always_inline bool
is_aso_mtr1_obj(uint32_t qix)
{
	return (qix & 1) != 0;
}

static __rte_always_inline bool
is_quota_sync_queue(const struct mlx5_priv *priv, uint32_t queue)
{
	return queue >= priv->nb_queue - 1;
}

static __rte_always_inline uint32_t
quota_sync_queue(const struct mlx5_priv *priv)
{
	return priv->nb_queue - 1;
}

static __rte_always_inline uint32_t
mlx5_quota_wqe_read_offset(uint32_t qix, uint32_t sq_index)
{
	return 2 * sq_index + (qix & 1);
}

static int32_t
mlx5_quota_fetch_tokens(const struct mlx5_aso_mtr_dseg *rd_buf)
{
	int c_tok = (int)rte_be_to_cpu_32(rd_buf->c_tokens);
	int e_tok = (int)rte_be_to_cpu_32(rd_buf->e_tokens);
	int result;

	DRV_LOG(DEBUG, "c_tokens %d e_tokens %d\n",
		rte_be_to_cpu_32(rd_buf->c_tokens),
		rte_be_to_cpu_32(rd_buf->e_tokens));
	/* Query after SET ignores negative E tokens */
	if (c_tok >= 0 && e_tok < 0)
		result = c_tok;
	/**
	 * If number of tokens in Meter bucket is zero or above,
	 * Meter hardware will use that bucket and can set number of tokens to
	 * negative value.
	 * Quota can discard negative C tokens in query report.
	 * That is a known hardware limitation.
	 * Use case example:
	 *
	 *      C     E   Result
	 *     250   250   500
	 *      50   250   300
	 *    -150   250   100
	 *    -150    50    50 *
	 *    -150  -150  -300
	 *
	 */
	else if (c_tok < 0 && e_tok >= 0 && (c_tok + e_tok) < 0)
		result = e_tok;
	else
		result = c_tok + e_tok;

	return result;
}

static void
mlx5_quota_query_update_async_cmpl(struct mlx5_hw_q_job *job)
{
	struct rte_flow_query_quota *query = job->query.user;

	query->quota = mlx5_quota_fetch_tokens(job->query.hw);
}

void
mlx5_quota_async_completion(struct rte_eth_dev *dev, uint32_t queue,
			    struct mlx5_hw_q_job *job)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	uint32_t qix = MLX5_INDIRECT_ACTION_IDX_GET(job->action);
	struct mlx5_quota *qobj = mlx5_ipool_get(qctx->quota_ipool, qix);

	RTE_SET_USED(queue);
	qobj->state = MLX5_QUOTA_STATE_READY;
	switch (job->type) {
	case MLX5_HW_Q_JOB_TYPE_CREATE:
		break;
	case MLX5_HW_Q_JOB_TYPE_QUERY:
	case MLX5_HW_Q_JOB_TYPE_UPDATE_QUERY:
		mlx5_quota_query_update_async_cmpl(job);
		break;
	default:
		break;
	}
}

static __rte_always_inline void
mlx5_quota_wqe_set_aso_read(volatile struct mlx5_aso_wqe *restrict wqe,
			    struct mlx5_quota_ctx *qctx, uint32_t queue)
{
	struct mlx5_aso_sq *sq = qctx->sq + queue;
	uint32_t sq_mask = (1 << sq->log_desc_n) - 1;
	uint32_t sq_head = sq->head & sq_mask;
	uint64_t rd_addr = (uint64_t)(qctx->read_buf[queue] + 2 * sq_head);

	wqe->aso_cseg.lkey = rte_cpu_to_be_32(qctx->mr.lkey);
	wqe->aso_cseg.va_h = rte_cpu_to_be_32((uint32_t)(rd_addr >> 32));
	wqe->aso_cseg.va_l_r = rte_cpu_to_be_32(((uint32_t)rd_addr) |
						MLX5_ASO_CSEG_READ_ENABLE);
}

#define MLX5_ASO_MTR1_ADD_MASK 0x00000F00ULL
#define MLX5_ASO_MTR1_SET_MASK 0x000F0F00ULL
#define MLX5_ASO_MTR0_ADD_MASK ((MLX5_ASO_MTR1_ADD_MASK) << 32)
#define MLX5_ASO_MTR0_SET_MASK ((MLX5_ASO_MTR1_SET_MASK) << 32)

static __rte_always_inline void
mlx5_quota_wqe_set_mtr_tokens(volatile struct mlx5_aso_wqe *restrict wqe,
			      uint32_t qix, void *arg)
{
	volatile struct mlx5_aso_mtr_dseg *mtr_dseg;
	const struct rte_flow_update_quota *conf = arg;
	bool set_op = (conf->op == RTE_FLOW_UPDATE_QUOTA_SET);

	if (is_aso_mtr1_obj(qix)) {
		wqe->aso_cseg.data_mask = set_op ?
					  RTE_BE64(MLX5_ASO_MTR1_SET_MASK) :
					  RTE_BE64(MLX5_ASO_MTR1_ADD_MASK);
		mtr_dseg = wqe->aso_dseg.mtrs + 1;
	} else {
		wqe->aso_cseg.data_mask = set_op ?
					  RTE_BE64(MLX5_ASO_MTR0_SET_MASK) :
					  RTE_BE64(MLX5_ASO_MTR0_ADD_MASK);
		mtr_dseg = wqe->aso_dseg.mtrs;
	}
	if (set_op) {
		/* prevent using E tokens when C tokens exhausted */
		mtr_dseg->e_tokens = -1;
		mtr_dseg->c_tokens = rte_cpu_to_be_32(conf->quota);
	} else {
		mtr_dseg->e_tokens = rte_cpu_to_be_32(conf->quota);
	}
}

static __rte_always_inline void
mlx5_quota_wqe_query(volatile struct mlx5_aso_wqe *restrict wqe,
		     struct mlx5_quota_ctx *qctx, __rte_unused uint32_t qix,
		     uint32_t queue, __rte_unused void *arg)
{
	mlx5_quota_wqe_set_aso_read(wqe, qctx, queue);
	wqe->aso_cseg.data_mask = 0ull; /* clear MTR ASO data modification */
}

static __rte_always_inline void
mlx5_quota_wqe_update(volatile struct mlx5_aso_wqe *restrict wqe,
		      __rte_unused struct mlx5_quota_ctx *qctx, uint32_t qix,
		      __rte_unused uint32_t queue, void *arg)
{
	mlx5_quota_wqe_set_mtr_tokens(wqe, qix, arg);
	wqe->aso_cseg.va_l_r = 0; /* clear READ flag */
}

static __rte_always_inline void
mlx5_quota_wqe_query_update(volatile struct mlx5_aso_wqe *restrict wqe,
			    struct mlx5_quota_ctx *qctx, uint32_t qix,
			    uint32_t queue, void *arg)
{
	mlx5_quota_wqe_set_aso_read(wqe, qctx, queue);
	mlx5_quota_wqe_set_mtr_tokens(wqe, qix, arg);
}

static __rte_always_inline void
mlx5_quota_set_init_wqe(volatile struct mlx5_aso_wqe *restrict wqe,
			__rte_unused struct mlx5_quota_ctx *qctx, uint32_t qix,
			__rte_unused uint32_t queue, void *arg)
{
	volatile struct mlx5_aso_mtr_dseg *mtr_dseg;
	const struct rte_flow_action_quota *conf = arg;
	const struct mlx5_quota *qobj = mlx5_ipool_get(qctx->quota_ipool, qix + 1);

	if (is_aso_mtr1_obj(qix)) {
		wqe->aso_cseg.data_mask =
			rte_cpu_to_be_64(MLX5_ASO_MTR1_INIT_MASK);
		mtr_dseg = wqe->aso_dseg.mtrs + 1;
	} else {
		wqe->aso_cseg.data_mask =
			rte_cpu_to_be_64(MLX5_ASO_MTR0_INIT_MASK);
		mtr_dseg = wqe->aso_dseg.mtrs;
	}
	mtr_dseg->e_tokens = -1;
	mtr_dseg->c_tokens = rte_cpu_to_be_32(conf->quota);
	mtr_dseg->v_bo_sc_bbog_mm |= rte_cpu_to_be_32
		(qobj->mode << ASO_DSEG_MTR_MODE);
}

static __rte_always_inline void
mlx5_quota_cmd_completed_status(struct mlx5_aso_sq *sq, uint16_t n)
{
	uint16_t i, mask = (1 << sq->log_desc_n) - 1;

	for (i = 0; i < n; i++) {
		uint8_t state = MLX5_QUOTA_STATE_WAIT;
		struct mlx5_quota *quota_obj =
			sq->elts[(sq->tail + i) & mask].quota_obj;

		__atomic_compare_exchange_n(&quota_obj->state, &state,
					    MLX5_QUOTA_STATE_READY, false,
					    __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	}
}

static void
mlx5_quota_cmd_completion_handle(struct mlx5_aso_sq *sq)
{
	struct mlx5_aso_cq *cq = &sq->cq;
	volatile struct mlx5_cqe *restrict cqe;
	const unsigned int cq_size = 1 << cq->log_desc_n;
	const unsigned int mask = cq_size - 1;
	uint32_t idx;
	uint32_t next_idx = cq->cq_ci & mask;
	uint16_t max;
	uint16_t n = 0;
	int ret;

	MLX5_ASSERT(rte_spinlock_is_locked(&sq->sqsl));
	max = (uint16_t)(sq->head - sq->tail);
	if (unlikely(!max))
		return;
	do {
		idx = next_idx;
		next_idx = (cq->cq_ci + 1) & mask;
		rte_prefetch0(&cq->cq_obj.cqes[next_idx]);
		cqe = &cq->cq_obj.cqes[idx];
		ret = check_cqe(cqe, cq_size, cq->cq_ci);
		/*
		 * Be sure owner read is done before any other cookie field or
		 * opaque field.
		 */
		rte_io_rmb();
		if (ret != MLX5_CQE_STATUS_SW_OWN) {
			if (likely(ret == MLX5_CQE_STATUS_HW_OWN))
				break;
			mlx5_aso_cqe_err_handle(sq);
		} else {
			n++;
		}
		cq->cq_ci++;
	} while (1);
	if (likely(n)) {
		mlx5_quota_cmd_completed_status(sq, n);
		sq->tail += n;
		rte_io_wmb();
		cq->cq_obj.db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
	}
}

static int
mlx5_quota_cmd_wait_cmpl(struct mlx5_aso_sq *sq, struct mlx5_quota *quota_obj)
{
	uint32_t poll_cqe_times = MLX5_MTR_POLL_WQE_CQE_TIMES;

	do {
		rte_spinlock_lock(&sq->sqsl);
		mlx5_quota_cmd_completion_handle(sq);
		rte_spinlock_unlock(&sq->sqsl);
		if (__atomic_load_n(&quota_obj->state, __ATOMIC_RELAXED) ==
		    MLX5_QUOTA_STATE_READY)
			return 0;
	} while (poll_cqe_times -= MLX5_ASO_WQE_CQE_RESPONSE_DELAY);
	DRV_LOG(ERR, "QUOTA: failed to poll command CQ");
	return -1;
}

static int
mlx5_quota_cmd_wqe(struct rte_eth_dev *dev, struct mlx5_quota *quota_obj,
		   quota_wqe_cmd_t wqe_cmd, uint32_t qix, uint32_t queue,
		   struct mlx5_hw_q_job *job, bool push, void *arg)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	struct mlx5_aso_sq *sq = qctx->sq + queue;
	uint32_t head, sq_mask = (1 << sq->log_desc_n) - 1;
	bool sync_queue = is_quota_sync_queue(priv, queue);
	volatile struct mlx5_aso_wqe *restrict wqe;
	int ret = 0;

	if (sync_queue)
		rte_spinlock_lock(&sq->sqsl);
	head = sq->head & sq_mask;
	wqe = &sq->sq_obj.aso_wqes[head];
	wqe_cmd(wqe, qctx, qix, queue, arg);
	wqe->general_cseg.misc = rte_cpu_to_be_32(qctx->devx_obj->id + (qix >> 1));
	wqe->general_cseg.opcode = rte_cpu_to_be_32
		(ASO_OPC_MOD_POLICER << WQE_CSEG_OPC_MOD_OFFSET |
		 sq->pi << WQE_CSEG_WQE_INDEX_OFFSET | MLX5_OPCODE_ACCESS_ASO);
	sq->head++;
	sq->pi += 2; /* Each WQE contains 2 WQEBB */
	if (push) {
		mlx5_doorbell_ring(&sh->tx_uar.bf_db, *(volatile uint64_t *)wqe,
				   sq->pi, &sq->sq_obj.db_rec[MLX5_SND_DBR],
				   !sh->tx_uar.dbnc);
		sq->db_pi = sq->pi;
	}
	sq->db = wqe;
	job->query.hw = qctx->read_buf[queue] +
			mlx5_quota_wqe_read_offset(qix, head);
	sq->elts[head].quota_obj = sync_queue ?
				   quota_obj : (typeof(quota_obj))job;
	if (sync_queue) {
		rte_spinlock_unlock(&sq->sqsl);
		ret = mlx5_quota_cmd_wait_cmpl(sq, quota_obj);
	}
	return ret;
}

static void
mlx5_quota_destroy_sq(struct mlx5_priv *priv)
{
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	uint32_t i, nb_queues = priv->nb_queue;

	if (!qctx->sq)
		return;
	for (i = 0; i < nb_queues; i++)
		mlx5_aso_destroy_sq(qctx->sq + i);
	mlx5_free(qctx->sq);
}

static __rte_always_inline void
mlx5_quota_wqe_init_common(struct mlx5_aso_sq *sq,
			   volatile struct mlx5_aso_wqe *restrict wqe)
{
#define ASO_MTR_DW0 RTE_BE32(1 << ASO_DSEG_VALID_OFFSET                  | \
			     MLX5_FLOW_COLOR_GREEN << ASO_DSEG_SC_OFFSET)

	memset((void *)(uintptr_t)wqe, 0, sizeof(*wqe));
	wqe->general_cseg.sq_ds = rte_cpu_to_be_32((sq->sqn << 8) |
						   (sizeof(*wqe) >> 4));
	wqe->aso_cseg.operand_masks = RTE_BE32
	(0u | (ASO_OPER_LOGICAL_OR << ASO_CSEG_COND_OPER_OFFSET) |
	 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_1_OPER_OFFSET) |
	 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_0_OPER_OFFSET) |
	 (BYTEWISE_64BYTE << ASO_CSEG_DATA_MASK_MODE_OFFSET));
	wqe->general_cseg.flags = RTE_BE32
	(MLX5_COMP_ALWAYS << MLX5_COMP_MODE_OFFSET);
	wqe->aso_dseg.mtrs[0].v_bo_sc_bbog_mm = ASO_MTR_DW0;
	/**
	 * ASO Meter tokens auto-update must be disabled in quota action.
	 * Tokens auto-update is disabled when Meter when *IR values set to
	 * ((0x1u << 16) | (0x1Eu << 24)) **NOT** 0x00
	 */
	wqe->aso_dseg.mtrs[0].cbs_cir = RTE_BE32((0x1u << 16) | (0x1Eu << 24));
	wqe->aso_dseg.mtrs[0].ebs_eir = RTE_BE32((0x1u << 16) | (0x1Eu << 24));
	wqe->aso_dseg.mtrs[1].v_bo_sc_bbog_mm = ASO_MTR_DW0;
	wqe->aso_dseg.mtrs[1].cbs_cir = RTE_BE32((0x1u << 16) | (0x1Eu << 24));
	wqe->aso_dseg.mtrs[1].ebs_eir = RTE_BE32((0x1u << 16) | (0x1Eu << 24));
#undef ASO_MTR_DW0
}

static void
mlx5_quota_init_sq(struct mlx5_aso_sq *sq)
{
	uint32_t i, size = 1 << sq->log_desc_n;

	for (i = 0; i < size; i++)
		mlx5_quota_wqe_init_common(sq, sq->sq_obj.aso_wqes + i);
}

static int
mlx5_quota_alloc_sq(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	uint32_t i, nb_queues = priv->nb_queue;

	qctx->sq = mlx5_malloc(MLX5_MEM_ZERO,
			       sizeof(qctx->sq[0]) * nb_queues,
			       0, SOCKET_ID_ANY);
	if (!qctx->sq) {
		DRV_LOG(DEBUG, "QUOTA: failed to allocate SQ pool");
		return -ENOMEM;
	}
	for (i = 0; i < nb_queues; i++) {
		int ret = mlx5_aso_sq_create
				(sh->cdev, qctx->sq + i, sh->tx_uar.obj,
				 rte_log2_u32(priv->hw_q[i].size));
		if (ret) {
			DRV_LOG(DEBUG, "QUOTA: failed to allocate SQ[%u]", i);
			return -ENOMEM;
		}
		mlx5_quota_init_sq(qctx->sq + i);
	}
	return 0;
}

static void
mlx5_quota_destroy_read_buf(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;

	if (qctx->mr.lkey) {
		void *addr = qctx->mr.addr;
		sh->cdev->mr_scache.dereg_mr_cb(&qctx->mr);
		mlx5_free(addr);
	}
	if (qctx->read_buf)
		mlx5_free(qctx->read_buf);
}

static int
mlx5_quota_alloc_read_buf(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	uint32_t i, nb_queues = priv->nb_queue;
	uint32_t sq_size_sum;
	size_t page_size = rte_mem_page_size();
	struct mlx5_aso_mtr_dseg *buf;
	size_t rd_buf_size;
	int ret;

	for (i = 0, sq_size_sum = 0; i < nb_queues; i++)
		sq_size_sum += priv->hw_q[i].size;
	/* ACCESS MTR ASO WQE reads 2 MTR objects */
	rd_buf_size = 2 * sq_size_sum * sizeof(buf[0]);
	buf = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO, rd_buf_size,
			  page_size, SOCKET_ID_ANY);
	if (!buf) {
		DRV_LOG(DEBUG, "QUOTA: failed to allocate MTR ASO READ buffer [1]");
		return -ENOMEM;
	}
	ret = sh->cdev->mr_scache.reg_mr_cb(sh->cdev->pd, buf,
					    rd_buf_size, &qctx->mr);
	if (ret) {
		DRV_LOG(DEBUG, "QUOTA: failed to register MTR ASO READ MR");
		return -errno;
	}
	qctx->read_buf = mlx5_malloc(MLX5_MEM_ZERO,
				     sizeof(qctx->read_buf[0]) * nb_queues,
				     0, SOCKET_ID_ANY);
	if (!qctx->read_buf) {
		DRV_LOG(DEBUG, "QUOTA: failed to allocate MTR ASO READ buffer [2]");
		return -ENOMEM;
	}
	for (i = 0; i < nb_queues; i++) {
		qctx->read_buf[i] = buf;
		buf += 2 * priv->hw_q[i].size;
	}
	return 0;
}

static __rte_always_inline int
mlx5_quota_check_ready(struct mlx5_quota *qobj, struct rte_flow_error *error)
{
	uint8_t state = MLX5_QUOTA_STATE_READY;
	bool verdict = __atomic_compare_exchange_n
		(&qobj->state, &state, MLX5_QUOTA_STATE_WAIT, false,
		 __ATOMIC_RELAXED, __ATOMIC_RELAXED);

	if (!verdict)
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL, "action is busy");
	return 0;
}

int
mlx5_quota_query(struct rte_eth_dev *dev, uint32_t queue,
		 const struct rte_flow_action_handle *handle,
		 struct rte_flow_query_quota *query,
		 struct mlx5_hw_q_job *async_job, bool push,
		 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	uint32_t work_queue = !is_quota_sync_queue(priv, queue) ?
			      queue : quota_sync_queue(priv);
	uint32_t id = MLX5_INDIRECT_ACTION_IDX_GET(handle);
	uint32_t qix = id - 1;
	struct mlx5_quota *qobj = mlx5_ipool_get(qctx->quota_ipool, id);
	struct mlx5_hw_q_job sync_job;
	int ret;

	if (!qobj)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "invalid query handle");
	ret = mlx5_quota_check_ready(qobj, error);
	if (ret)
		return ret;
	ret = mlx5_quota_cmd_wqe(dev, qobj, mlx5_quota_wqe_query, qix, work_queue,
				 async_job ? async_job : &sync_job, push, NULL);
	if (ret) {
		__atomic_store_n(&qobj->state, MLX5_QUOTA_STATE_READY,
				 __ATOMIC_RELAXED);
		return rte_flow_error_set(error, EAGAIN,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL, "try again");
	}
	if (is_quota_sync_queue(priv, queue))
		query->quota = mlx5_quota_fetch_tokens(sync_job.query.hw);
	return 0;
}

int
mlx5_quota_query_update(struct rte_eth_dev *dev, uint32_t queue,
			struct rte_flow_action_handle *handle,
			const struct rte_flow_action *update,
			struct rte_flow_query_quota *query,
			struct mlx5_hw_q_job *async_job, bool push,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	const struct rte_flow_update_quota *conf = update->conf;
	uint32_t work_queue = !is_quota_sync_queue(priv, queue) ?
			       queue : quota_sync_queue(priv);
	uint32_t id = MLX5_INDIRECT_ACTION_IDX_GET(handle);
	uint32_t qix = id - 1;
	struct mlx5_quota *qobj = mlx5_ipool_get(qctx->quota_ipool, id);
	struct mlx5_hw_q_job sync_job;
	quota_wqe_cmd_t wqe_cmd = query ?
				  mlx5_quota_wqe_query_update :
				  mlx5_quota_wqe_update;
	int ret;

	if (conf->quota > MLX5_MTR_MAX_TOKEN_VALUE)
		return rte_flow_error_set(error, E2BIG,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL, "update value too big");
	if (!qobj)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "invalid query_update handle");
	if (conf->op == RTE_FLOW_UPDATE_QUOTA_ADD &&
	    qobj->last_update == RTE_FLOW_UPDATE_QUOTA_ADD)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL, "cannot add twice");
	ret = mlx5_quota_check_ready(qobj, error);
	if (ret)
		return ret;
	ret = mlx5_quota_cmd_wqe(dev, qobj, wqe_cmd, qix, work_queue,
				 async_job ? async_job : &sync_job, push,
				 (void *)(uintptr_t)update->conf);
	if (ret) {
		__atomic_store_n(&qobj->state, MLX5_QUOTA_STATE_READY,
				 __ATOMIC_RELAXED);
		return rte_flow_error_set(error, EAGAIN,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL, "try again");
	}
	qobj->last_update = conf->op;
	if (query && is_quota_sync_queue(priv, queue))
		query->quota = mlx5_quota_fetch_tokens(sync_job.query.hw);
	return 0;
}

struct rte_flow_action_handle *
mlx5_quota_alloc(struct rte_eth_dev *dev, uint32_t queue,
		 const struct rte_flow_action_quota *conf,
		 struct mlx5_hw_q_job *job, bool push,
		 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	uint32_t id;
	struct mlx5_quota *qobj;
	uintptr_t handle = (uintptr_t)MLX5_INDIRECT_ACTION_TYPE_QUOTA <<
			   MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t work_queue = !is_quota_sync_queue(priv, queue) ?
			      queue : quota_sync_queue(priv);
	struct mlx5_hw_q_job sync_job;
	uint8_t state = MLX5_QUOTA_STATE_FREE;
	bool verdict;
	int ret;

	qobj = mlx5_ipool_malloc(qctx->quota_ipool, &id);
	if (!qobj) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "quota: failed to allocate quota object");
		return NULL;
	}
	verdict = __atomic_compare_exchange_n
		(&qobj->state, &state, MLX5_QUOTA_STATE_WAIT, false,
		 __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	if (!verdict) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "quota: new quota object has invalid state");
		return NULL;
	}
	switch (conf->mode) {
	case RTE_FLOW_QUOTA_MODE_L2:
		qobj->mode = MLX5_METER_MODE_L2_LEN;
		break;
	case RTE_FLOW_QUOTA_MODE_PACKET:
		qobj->mode = MLX5_METER_MODE_PKT;
		break;
	default:
		qobj->mode = MLX5_METER_MODE_IP_LEN;
	}
	ret = mlx5_quota_cmd_wqe(dev, qobj, mlx5_quota_set_init_wqe, id - 1,
				 work_queue, job ? job : &sync_job, push,
				 (void *)(uintptr_t)conf);
	if (ret) {
		mlx5_ipool_free(qctx->quota_ipool, id);
		__atomic_store_n(&qobj->state, MLX5_QUOTA_STATE_FREE,
				 __ATOMIC_RELAXED);
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "quota: WR failure");
		return 0;
	}
	return (struct rte_flow_action_handle *)(handle | id);
}

int
mlx5_flow_quota_destroy(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	int ret;

	if (qctx->dr_action) {
		ret = mlx5dr_action_destroy(qctx->dr_action);
		if (ret)
			DRV_LOG(ERR, "QUOTA: failed to destroy DR action");
	}
	if (!priv->shared_host) {
		if (qctx->quota_ipool)
			mlx5_ipool_destroy(qctx->quota_ipool);
		mlx5_quota_destroy_sq(priv);
		mlx5_quota_destroy_read_buf(priv);
		if (qctx->devx_obj) {
			ret = mlx5_devx_cmd_destroy(qctx->devx_obj);
			if (ret)
				DRV_LOG(ERR,
					"QUOTA: failed to destroy MTR ASO object");
		}
	}
	memset(qctx, 0, sizeof(*qctx));
	return 0;
}

#define MLX5_QUOTA_IPOOL_TRUNK_SIZE (1u << 12)
#define MLX5_QUOTA_IPOOL_CACHE_SIZE (1u << 13)

static int
mlx5_quota_init_guest(struct mlx5_priv *priv)
{
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	struct rte_eth_dev *host_dev = priv->shared_host;
	struct mlx5_priv *host_priv = host_dev->data->dev_private;

	/**
	 * Shared quota object can be used in flow rules only.
	 * DR5 flow action needs access to ASO abjects.
	 */
	qctx->devx_obj = host_priv->quota_ctx.devx_obj;
	return 0;
}

static int
mlx5_quota_init_host(struct mlx5_priv *priv, uint32_t nb_quotas)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	struct mlx5_indexed_pool_config quota_ipool_cfg = {
		.size = sizeof(struct mlx5_quota),
		.trunk_size = RTE_MIN(nb_quotas, MLX5_QUOTA_IPOOL_TRUNK_SIZE),
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.max_idx = nb_quotas,
		.free = mlx5_free,
		.type = "mlx5_flow_quota_index_pool"
	};
	int ret;

	if (!nb_quotas) {
		DRV_LOG(DEBUG, "QUOTA: cannot create quota with 0 objects");
		return -EINVAL;
	}
	if (!priv->mtr_en || !sh->meter_aso_en) {
		DRV_LOG(DEBUG, "QUOTA: no MTR support");
		return -ENOTSUP;
	}
	qctx->devx_obj = mlx5_devx_cmd_create_flow_meter_aso_obj
		(sh->cdev->ctx, sh->cdev->pdn, rte_log2_u32(nb_quotas >> 1));
	if (!qctx->devx_obj) {
		DRV_LOG(DEBUG, "QUOTA: cannot allocate MTR ASO objects");
		return -ENOMEM;
	}
	ret = mlx5_quota_alloc_read_buf(priv);
	if (ret)
		return ret;
	ret = mlx5_quota_alloc_sq(priv);
	if (ret)
		return ret;
	if (nb_quotas < MLX5_QUOTA_IPOOL_TRUNK_SIZE)
		quota_ipool_cfg.per_core_cache = 0;
	else if (nb_quotas < MLX5_HW_IPOOL_SIZE_THRESHOLD)
		quota_ipool_cfg.per_core_cache = MLX5_HW_IPOOL_CACHE_MIN;
	else
		quota_ipool_cfg.per_core_cache = MLX5_QUOTA_IPOOL_CACHE_SIZE;
	qctx->quota_ipool = mlx5_ipool_create(&quota_ipool_cfg);
	if (!qctx->quota_ipool) {
		DRV_LOG(DEBUG, "QUOTA: failed to allocate quota pool");
		return -ENOMEM;
	}
	return 0;
}

int
mlx5_flow_quota_init(struct rte_eth_dev *dev, uint32_t nb_quotas)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_quota_ctx *qctx = &priv->quota_ctx;
	uint32_t flags = MLX5DR_ACTION_FLAG_HWS_RX | MLX5DR_ACTION_FLAG_HWS_TX;
	int reg_id = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, NULL);
	int ret;

	if (reg_id < 0) {
		DRV_LOG(DEBUG, "QUOTA: MRT register not available");
		return -ENOTSUP;
	}
	if (!priv->shared_host)
		ret = mlx5_quota_init_host(priv, nb_quotas);
	else
		ret = mlx5_quota_init_guest(priv);
	if (ret)
		goto err;
	if (priv->sh->config.dv_esw_en && priv->master)
		flags |= MLX5DR_ACTION_FLAG_HWS_FDB;
	qctx->dr_action = mlx5dr_action_create_aso_meter
		(priv->dr_ctx, (struct mlx5dr_devx_obj *)qctx->devx_obj,
		 reg_id - REG_C_0, flags);
	if (!qctx->dr_action) {
		DRV_LOG(DEBUG, "QUOTA: failed to create DR action");
		ret = -ENOMEM;
		goto err;
	}
	return 0;
err:
	mlx5_flow_quota_destroy(dev);
	return ret;
}

#endif /* defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H) */
