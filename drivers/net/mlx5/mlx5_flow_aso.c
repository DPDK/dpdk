/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <mlx5_prm.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_eal_paging.h>

#include <mlx5_malloc.h>
#include <mlx5_common_os.h>
#include <mlx5_common_devx.h>

#include "mlx5.h"
#include "mlx5_flow.h"

/**
 * Free MR resources.
 *
 * @param[in] cdev
 *   Pointer to the mlx5 common device.
 * @param[in] mr
 *   MR to free.
 */
static void
mlx5_aso_dereg_mr(struct mlx5_common_device *cdev, struct mlx5_pmd_mr *mr)
{
	void *addr = mr->addr;

	cdev->mr_scache.dereg_mr_cb(mr);
	mlx5_free(addr);
	memset(mr, 0, sizeof(*mr));
}

/**
 * Register Memory Region.
 *
 * @param[in] cdev
 *   Pointer to the mlx5 common device.
 * @param[in] length
 *   Size of MR buffer.
 * @param[in/out] mr
 *   Pointer to MR to create.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_aso_reg_mr(struct mlx5_common_device *cdev, size_t length,
		struct mlx5_pmd_mr *mr)
{
	int ret;

	mr->addr = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, length, 4096,
			       SOCKET_ID_ANY);
	if (!mr->addr) {
		DRV_LOG(ERR, "Failed to create ASO bits mem for MR.");
		return -1;
	}
	ret = cdev->mr_scache.reg_mr_cb(cdev->pd, mr->addr, length, mr);
	if (ret) {
		DRV_LOG(ERR, "Failed to create direct Mkey.");
		mlx5_free(mr->addr);
		return -1;
	}
	return 0;
}

/**
 * Destroy Send Queue used for ASO access.
 *
 * @param[in] sq
 *   ASO SQ to destroy.
 */
static void
mlx5_aso_destroy_sq(struct mlx5_aso_sq *sq)
{
	mlx5_devx_sq_destroy(&sq->sq_obj);
	mlx5_devx_cq_destroy(&sq->cq.cq_obj);
	memset(sq, 0, sizeof(*sq));
}

/**
 * Initialize Send Queue used for ASO access.
 *
 * @param[in] sq
 *   ASO SQ to initialize.
 */
static void
mlx5_aso_age_init_sq(struct mlx5_aso_sq *sq)
{
	volatile struct mlx5_aso_wqe *restrict wqe;
	int i;
	int size = 1 << sq->log_desc_n;
	uint64_t addr;

	/* All the next fields state should stay constant. */
	for (i = 0, wqe = &sq->sq_obj.aso_wqes[0]; i < size; ++i, ++wqe) {
		wqe->general_cseg.sq_ds = rte_cpu_to_be_32((sq->sqn << 8) |
							  (sizeof(*wqe) >> 4));
		wqe->aso_cseg.lkey = rte_cpu_to_be_32(sq->mr.lkey);
		addr = (uint64_t)((uint64_t *)sq->mr.addr + i *
					    MLX5_ASO_AGE_ACTIONS_PER_POOL / 64);
		wqe->aso_cseg.va_h = rte_cpu_to_be_32((uint32_t)(addr >> 32));
		wqe->aso_cseg.va_l_r = rte_cpu_to_be_32((uint32_t)addr | 1u);
		wqe->aso_cseg.operand_masks = rte_cpu_to_be_32
			(0u |
			 (ASO_OPER_LOGICAL_OR << ASO_CSEG_COND_OPER_OFFSET) |
			 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_1_OPER_OFFSET) |
			 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_0_OPER_OFFSET) |
			 (BYTEWISE_64BYTE << ASO_CSEG_DATA_MASK_MODE_OFFSET));
		wqe->aso_cseg.data_mask = RTE_BE64(UINT64_MAX);
	}
}

/**
 * Initialize Send Queue used for ASO flow meter access.
 *
 * @param[in] sq
 *   ASO SQ to initialize.
 */
static void
mlx5_aso_mtr_init_sq(struct mlx5_aso_sq *sq)
{
	volatile struct mlx5_aso_wqe *restrict wqe;
	int i;
	int size = 1 << sq->log_desc_n;

	/* All the next fields state should stay constant. */
	for (i = 0, wqe = &sq->sq_obj.aso_wqes[0]; i < size; ++i, ++wqe) {
		wqe->general_cseg.sq_ds = rte_cpu_to_be_32((sq->sqn << 8) |
							  (sizeof(*wqe) >> 4));
		wqe->aso_cseg.operand_masks = RTE_BE32(0u |
			 (ASO_OPER_LOGICAL_OR << ASO_CSEG_COND_OPER_OFFSET) |
			 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_1_OPER_OFFSET) |
			 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_0_OPER_OFFSET) |
			 (BYTEWISE_64BYTE << ASO_CSEG_DATA_MASK_MODE_OFFSET));
		wqe->general_cseg.flags = RTE_BE32(MLX5_COMP_ALWAYS <<
							 MLX5_COMP_MODE_OFFSET);
	}
}

/*
 * Initialize Send Queue used for ASO connection tracking.
 *
 * @param[in] sq
 *   ASO SQ to initialize.
 */
static void
mlx5_aso_ct_init_sq(struct mlx5_aso_sq *sq)
{
	volatile struct mlx5_aso_wqe *restrict wqe;
	int i;
	int size = 1 << sq->log_desc_n;
	uint64_t addr;

	/* All the next fields state should stay constant. */
	for (i = 0, wqe = &sq->sq_obj.aso_wqes[0]; i < size; ++i, ++wqe) {
		wqe->general_cseg.sq_ds = rte_cpu_to_be_32((sq->sqn << 8) |
							  (sizeof(*wqe) >> 4));
		/* One unique MR for the query data. */
		wqe->aso_cseg.lkey = rte_cpu_to_be_32(sq->mr.lkey);
		/* Magic number 64 represents the length of a ASO CT obj. */
		addr = (uint64_t)((uintptr_t)sq->mr.addr + i * 64);
		wqe->aso_cseg.va_h = rte_cpu_to_be_32((uint32_t)(addr >> 32));
		wqe->aso_cseg.va_l_r = rte_cpu_to_be_32((uint32_t)addr | 1u);
		/*
		 * The values of operand_masks are different for modify
		 * and query.
		 * And data_mask may be different for each modification. In
		 * query, it could be zero and ignored.
		 * CQE generation is always needed, in order to decide when
		 * it is available to create the flow or read the data.
		 */
		wqe->general_cseg.flags = RTE_BE32(MLX5_COMP_ALWAYS <<
						   MLX5_COMP_MODE_OFFSET);
	}
}

/**
 * Create Send Queue used for ASO access.
 *
 * @param[in] cdev
 *   Pointer to the mlx5 common device.
 * @param[in/out] sq
 *   Pointer to SQ to create.
 * @param[in] uar
 *   User Access Region object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_aso_sq_create(struct mlx5_common_device *cdev, struct mlx5_aso_sq *sq,
		   void *uar)
{
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(uar),
	};
	struct mlx5_devx_create_sq_attr sq_attr = {
		.user_index = 0xFFFF,
		.wq_attr = (struct mlx5_devx_wq_attr){
			.pd = cdev->pdn,
			.uar_page = mlx5_os_get_devx_uar_page_id(uar),
		},
		.ts_format =
			mlx5_ts_format_conv(cdev->config.hca_attr.sq_ts_format),
	};
	struct mlx5_devx_modify_sq_attr modify_attr = {
		.state = MLX5_SQC_STATE_RDY,
	};
	uint16_t log_wqbb_n;
	int ret;

	if (mlx5_devx_cq_create(cdev->ctx, &sq->cq.cq_obj,
				MLX5_ASO_QUEUE_LOG_DESC, &cq_attr,
				SOCKET_ID_ANY))
		goto error;
	sq->cq.cq_ci = 0;
	sq->cq.log_desc_n = MLX5_ASO_QUEUE_LOG_DESC;
	sq->log_desc_n = MLX5_ASO_QUEUE_LOG_DESC;
	sq_attr.cqn = sq->cq.cq_obj.cq->id;
	/* for mlx5_aso_wqe that is twice the size of mlx5_wqe */
	log_wqbb_n = sq->log_desc_n + 1;
	ret = mlx5_devx_sq_create(cdev->ctx, &sq->sq_obj, log_wqbb_n, &sq_attr,
				  SOCKET_ID_ANY);
	if (ret) {
		DRV_LOG(ERR, "Can't create SQ object.");
		rte_errno = ENOMEM;
		goto error;
	}
	ret = mlx5_devx_cmd_modify_sq(sq->sq_obj.sq, &modify_attr);
	if (ret) {
		DRV_LOG(ERR, "Can't change SQ state to ready.");
		rte_errno = ENOMEM;
		goto error;
	}
	sq->pi = 0;
	sq->head = 0;
	sq->tail = 0;
	sq->sqn = sq->sq_obj.sq->id;
	rte_spinlock_init(&sq->sqsl);
	return 0;
error:
	mlx5_aso_destroy_sq(sq);
	return -1;
}

/**
 * API to create and initialize Send Queue used for ASO access.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 * @param[in] aso_opc_mod
 *   Mode of ASO feature.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_aso_queue_init(struct mlx5_dev_ctx_shared *sh,
		    enum mlx5_access_aso_opc_mod aso_opc_mod)
{
	uint32_t sq_desc_n = 1 << MLX5_ASO_QUEUE_LOG_DESC;
	struct mlx5_common_device *cdev = sh->cdev;

	switch (aso_opc_mod) {
	case ASO_OPC_MOD_FLOW_HIT:
		if (mlx5_aso_reg_mr(cdev, (MLX5_ASO_AGE_ACTIONS_PER_POOL / 8) *
				    sq_desc_n, &sh->aso_age_mng->aso_sq.mr))
			return -1;
		if (mlx5_aso_sq_create(cdev, &sh->aso_age_mng->aso_sq,
				       sh->tx_uar.obj)) {
			mlx5_aso_dereg_mr(cdev, &sh->aso_age_mng->aso_sq.mr);
			return -1;
		}
		mlx5_aso_age_init_sq(&sh->aso_age_mng->aso_sq);
		break;
	case ASO_OPC_MOD_POLICER:
		if (mlx5_aso_sq_create(cdev, &sh->mtrmng->pools_mng.sq,
				       sh->tx_uar.obj))
			return -1;
		mlx5_aso_mtr_init_sq(&sh->mtrmng->pools_mng.sq);
		break;
	case ASO_OPC_MOD_CONNECTION_TRACKING:
		/* 64B per object for query. */
		if (mlx5_aso_reg_mr(cdev, 64 * sq_desc_n,
				    &sh->ct_mng->aso_sq.mr))
			return -1;
		if (mlx5_aso_sq_create(cdev, &sh->ct_mng->aso_sq,
				       sh->tx_uar.obj)) {
			mlx5_aso_dereg_mr(cdev, &sh->ct_mng->aso_sq.mr);
			return -1;
		}
		mlx5_aso_ct_init_sq(&sh->ct_mng->aso_sq);
		break;
	default:
		DRV_LOG(ERR, "Unknown ASO operation mode");
		return -1;
	}
	return 0;
}

/**
 * API to destroy Send Queue used for ASO access.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 * @param[in] aso_opc_mod
 *   Mode of ASO feature.
 */
void
mlx5_aso_queue_uninit(struct mlx5_dev_ctx_shared *sh,
		      enum mlx5_access_aso_opc_mod aso_opc_mod)
{
	struct mlx5_aso_sq *sq;

	switch (aso_opc_mod) {
	case ASO_OPC_MOD_FLOW_HIT:
		mlx5_aso_dereg_mr(sh->cdev, &sh->aso_age_mng->aso_sq.mr);
		sq = &sh->aso_age_mng->aso_sq;
		break;
	case ASO_OPC_MOD_POLICER:
		sq = &sh->mtrmng->pools_mng.sq;
		break;
	case ASO_OPC_MOD_CONNECTION_TRACKING:
		mlx5_aso_dereg_mr(sh->cdev, &sh->ct_mng->aso_sq.mr);
		sq = &sh->ct_mng->aso_sq;
		break;
	default:
		DRV_LOG(ERR, "Unknown ASO operation mode");
		return;
	}
	mlx5_aso_destroy_sq(sq);
}

/**
 * Write a burst of WQEs to ASO SQ.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 * @param[in] n
 *   Index of the last valid pool.
 *
 * @return
 *   Number of WQEs in burst.
 */
static uint16_t
mlx5_aso_sq_enqueue_burst(struct mlx5_dev_ctx_shared *sh, uint16_t n)
{
	struct mlx5_aso_age_mng *mng = sh->aso_age_mng;
	volatile struct mlx5_aso_wqe *wqe;
	struct mlx5_aso_sq *sq = &mng->aso_sq;
	struct mlx5_aso_age_pool *pool;
	uint16_t size = 1 << sq->log_desc_n;
	uint16_t mask = size - 1;
	uint16_t max;
	uint16_t start_head = sq->head;

	max = RTE_MIN(size - (uint16_t)(sq->head - sq->tail), n - sq->next);
	if (unlikely(!max))
		return 0;
	sq->elts[start_head & mask].burst_size = max;
	do {
		wqe = &sq->sq_obj.aso_wqes[sq->head & mask];
		rte_prefetch0(&sq->sq_obj.aso_wqes[(sq->head + 1) & mask]);
		/* Fill next WQE. */
		rte_rwlock_read_lock(&mng->resize_rwl);
		pool = mng->pools[sq->next];
		rte_rwlock_read_unlock(&mng->resize_rwl);
		sq->elts[sq->head & mask].pool = pool;
		wqe->general_cseg.misc =
				rte_cpu_to_be_32(((struct mlx5_devx_obj *)
						 (pool->flow_hit_aso_obj))->id);
		wqe->general_cseg.flags = RTE_BE32(MLX5_COMP_ONLY_FIRST_ERR <<
							 MLX5_COMP_MODE_OFFSET);
		wqe->general_cseg.opcode = rte_cpu_to_be_32
						(MLX5_OPCODE_ACCESS_ASO |
						 (ASO_OPC_MOD_FLOW_HIT <<
						  WQE_CSEG_OPC_MOD_OFFSET) |
						 (sq->pi <<
						  WQE_CSEG_WQE_INDEX_OFFSET));
		sq->pi += 2; /* Each WQE contains 2 WQEBB's. */
		sq->head++;
		sq->next++;
		max--;
	} while (max);
	wqe->general_cseg.flags = RTE_BE32(MLX5_COMP_ALWAYS <<
							 MLX5_COMP_MODE_OFFSET);
	mlx5_doorbell_ring(&sh->tx_uar.bf_db, *(volatile uint64_t *)wqe,
			   sq->pi, &sq->sq_obj.db_rec[MLX5_SND_DBR],
			   !sh->tx_uar.dbnc);
	return sq->elts[start_head & mask].burst_size;
}

/**
 * Debug utility function. Dump contents of error CQE and WQE.
 *
 * @param[in] cqe
 *   Error CQE to dump.
 * @param[in] wqe
 *   Error WQE to dump.
 */
static void
mlx5_aso_dump_err_objs(volatile uint32_t *cqe, volatile uint32_t *wqe)
{
	int i;

	DRV_LOG(ERR, "Error cqe:");
	for (i = 0; i < 16; i += 4)
		DRV_LOG(ERR, "%08X %08X %08X %08X", cqe[i], cqe[i + 1],
			cqe[i + 2], cqe[i + 3]);
	DRV_LOG(ERR, "\nError wqe:");
	for (i = 0; i < (int)sizeof(struct mlx5_aso_wqe) / 4; i += 4)
		DRV_LOG(ERR, "%08X %08X %08X %08X", wqe[i], wqe[i + 1],
			wqe[i + 2], wqe[i + 3]);
}

/**
 * Handle case of error CQE.
 *
 * @param[in] sq
 *   ASO SQ to use.
 */
static void
mlx5_aso_cqe_err_handle(struct mlx5_aso_sq *sq)
{
	struct mlx5_aso_cq *cq = &sq->cq;
	uint32_t idx = cq->cq_ci & ((1 << cq->log_desc_n) - 1);
	volatile struct mlx5_err_cqe *cqe =
			(volatile struct mlx5_err_cqe *)&cq->cq_obj.cqes[idx];

	cq->errors++;
	idx = rte_be_to_cpu_16(cqe->wqe_counter) & (1u << sq->log_desc_n);
	mlx5_aso_dump_err_objs((volatile uint32_t *)cqe,
			       (volatile uint32_t *)&sq->sq_obj.aso_wqes[idx]);
}

/**
 * Update ASO objects upon completion.
 *
 * @param[in] sh
 *   Shared device context.
 * @param[in] n
 *   Number of completed ASO objects.
 */
static void
mlx5_aso_age_action_update(struct mlx5_dev_ctx_shared *sh, uint16_t n)
{
	struct mlx5_aso_age_mng *mng = sh->aso_age_mng;
	struct mlx5_aso_sq *sq = &mng->aso_sq;
	struct mlx5_age_info *age_info;
	const uint16_t size = 1 << sq->log_desc_n;
	const uint16_t mask = size - 1;
	const uint64_t curr = MLX5_CURR_TIME_SEC;
	uint16_t expected = AGE_CANDIDATE;
	uint16_t i;

	for (i = 0; i < n; ++i) {
		uint16_t idx = (sq->tail + i) & mask;
		struct mlx5_aso_age_pool *pool = sq->elts[idx].pool;
		uint64_t diff = curr - pool->time_of_last_age_check;
		uint64_t *addr = sq->mr.addr;
		int j;

		addr += idx * MLX5_ASO_AGE_ACTIONS_PER_POOL / 64;
		pool->time_of_last_age_check = curr;
		for (j = 0; j < MLX5_ASO_AGE_ACTIONS_PER_POOL; j++) {
			struct mlx5_aso_age_action *act = &pool->actions[j];
			struct mlx5_age_param *ap = &act->age_params;
			uint8_t byte;
			uint8_t offset;
			uint8_t *u8addr;
			uint8_t hit;

			if (__atomic_load_n(&ap->state, __ATOMIC_RELAXED) !=
					    AGE_CANDIDATE)
				continue;
			byte = 63 - (j / 8);
			offset = j % 8;
			u8addr = (uint8_t *)addr;
			hit = (u8addr[byte] >> offset) & 0x1;
			if (hit) {
				__atomic_store_n(&ap->sec_since_last_hit, 0,
						 __ATOMIC_RELAXED);
			} else {
				struct mlx5_priv *priv;

				__atomic_fetch_add(&ap->sec_since_last_hit,
						   diff, __ATOMIC_RELAXED);
				/* If timeout passed add to aged-out list. */
				if (ap->sec_since_last_hit <= ap->timeout)
					continue;
				priv =
				rte_eth_devices[ap->port_id].data->dev_private;
				age_info = GET_PORT_AGE_INFO(priv);
				rte_spinlock_lock(&age_info->aged_sl);
				if (__atomic_compare_exchange_n(&ap->state,
								&expected,
								AGE_TMOUT,
								false,
							       __ATOMIC_RELAXED,
							    __ATOMIC_RELAXED)) {
					LIST_INSERT_HEAD(&age_info->aged_aso,
							 act, next);
					MLX5_AGE_SET(age_info,
						     MLX5_AGE_EVENT_NEW);
				}
				rte_spinlock_unlock(&age_info->aged_sl);
			}
		}
	}
	mlx5_age_event_prepare(sh);
}

/**
 * Handle completions from WQEs sent to ASO SQ.
 *
 * @param[in] sh
 *   Shared device context.
 *
 * @return
 *   Number of CQEs handled.
 */
static uint16_t
mlx5_aso_completion_handle(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_aso_age_mng *mng = sh->aso_age_mng;
	struct mlx5_aso_sq *sq = &mng->aso_sq;
	struct mlx5_aso_cq *cq = &sq->cq;
	volatile struct mlx5_cqe *restrict cqe;
	const unsigned int cq_size = 1 << cq->log_desc_n;
	const unsigned int mask = cq_size - 1;
	uint32_t idx;
	uint32_t next_idx = cq->cq_ci & mask;
	const uint16_t max = (uint16_t)(sq->head - sq->tail);
	uint16_t i = 0;
	int ret;
	if (unlikely(!max))
		return 0;
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
		if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
			if (likely(ret == MLX5_CQE_STATUS_HW_OWN))
				break;
			mlx5_aso_cqe_err_handle(sq);
		} else {
			i += sq->elts[(sq->tail + i) & mask].burst_size;
		}
		cq->cq_ci++;
	} while (1);
	if (likely(i)) {
		mlx5_aso_age_action_update(sh, i);
		sq->tail += i;
		rte_io_wmb();
		cq->cq_obj.db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
	}
	return i;
}

/**
 * Periodically read CQEs and send WQEs to ASO SQ.
 *
 * @param[in] arg
 *   Shared device context containing the ASO SQ.
 */
static void
mlx5_flow_aso_alarm(void *arg)
{
	struct mlx5_dev_ctx_shared *sh = arg;
	struct mlx5_aso_sq *sq = &sh->aso_age_mng->aso_sq;
	uint32_t us = 100u;
	uint16_t n;

	rte_rwlock_read_lock(&sh->aso_age_mng->resize_rwl);
	n = sh->aso_age_mng->next;
	rte_rwlock_read_unlock(&sh->aso_age_mng->resize_rwl);
	mlx5_aso_completion_handle(sh);
	if (sq->next == n) {
		/* End of loop: wait 1 second. */
		us = US_PER_S;
		sq->next = 0;
	}
	mlx5_aso_sq_enqueue_burst(sh, n);
	if (rte_eal_alarm_set(us, mlx5_flow_aso_alarm, sh))
		DRV_LOG(ERR, "Cannot reinitialize aso alarm.");
}

/**
 * API to start ASO access using ASO SQ.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_aso_flow_hit_queue_poll_start(struct mlx5_dev_ctx_shared *sh)
{
	if (rte_eal_alarm_set(US_PER_S, mlx5_flow_aso_alarm, sh)) {
		DRV_LOG(ERR, "Cannot reinitialize ASO age alarm.");
		return -rte_errno;
	}
	return 0;
}

/**
 * API to stop ASO access using ASO SQ.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_aso_flow_hit_queue_poll_stop(struct mlx5_dev_ctx_shared *sh)
{
	int retries = 1024;

	if (!sh->aso_age_mng->aso_sq.sq_obj.sq)
		return -EINVAL;
	rte_errno = 0;
	while (--retries) {
		rte_eal_alarm_cancel(mlx5_flow_aso_alarm, sh);
		if (rte_errno != EINPROGRESS)
			break;
		rte_pause();
	}
	return -rte_errno;
}

static uint16_t
mlx5_aso_mtr_sq_enqueue_single(struct mlx5_dev_ctx_shared *sh,
			       struct mlx5_aso_sq *sq,
			       struct mlx5_aso_mtr *aso_mtr)
{
	volatile struct mlx5_aso_wqe *wqe = NULL;
	struct mlx5_flow_meter_info *fm = NULL;
	struct mlx5_flow_meter_profile *fmp;
	uint16_t size = 1 << sq->log_desc_n;
	uint16_t mask = size - 1;
	uint16_t res;
	uint32_t dseg_idx = 0;
	struct mlx5_aso_mtr_pool *pool = NULL;

	rte_spinlock_lock(&sq->sqsl);
	res = size - (uint16_t)(sq->head - sq->tail);
	if (unlikely(!res)) {
		DRV_LOG(ERR, "Fail: SQ is full and no free WQE to send");
		rte_spinlock_unlock(&sq->sqsl);
		return 0;
	}
	wqe = &sq->sq_obj.aso_wqes[sq->head & mask];
	rte_prefetch0(&sq->sq_obj.aso_wqes[(sq->head + 1) & mask]);
	/* Fill next WQE. */
	fm = &aso_mtr->fm;
	sq->elts[sq->head & mask].mtr = aso_mtr;
	pool = container_of(aso_mtr, struct mlx5_aso_mtr_pool,
			mtrs[aso_mtr->offset]);
	wqe->general_cseg.misc = rte_cpu_to_be_32(pool->devx_obj->id +
			(aso_mtr->offset >> 1));
	wqe->general_cseg.opcode = rte_cpu_to_be_32(MLX5_OPCODE_ACCESS_ASO |
			(ASO_OPC_MOD_POLICER <<
			WQE_CSEG_OPC_MOD_OFFSET) |
			sq->pi << WQE_CSEG_WQE_INDEX_OFFSET);
	/* There are 2 meters in one ASO cache line. */
	dseg_idx = aso_mtr->offset & 0x1;
	wqe->aso_cseg.data_mask =
		RTE_BE64(MLX5_IFC_FLOW_METER_PARAM_MASK << (32 * !dseg_idx));
	if (fm->is_enable) {
		wqe->aso_dseg.mtrs[dseg_idx].cbs_cir =
			fm->profile->srtcm_prm.cbs_cir;
		wqe->aso_dseg.mtrs[dseg_idx].ebs_eir =
			fm->profile->srtcm_prm.ebs_eir;
	} else {
		wqe->aso_dseg.mtrs[dseg_idx].cbs_cir =
			RTE_BE32(MLX5_IFC_FLOW_METER_DISABLE_CBS_CIR_VAL);
		wqe->aso_dseg.mtrs[dseg_idx].ebs_eir = 0;
	}
	fmp = fm->profile;
	if (fmp->profile.packet_mode)
		wqe->aso_dseg.mtrs[dseg_idx].v_bo_sc_bbog_mm =
				RTE_BE32((1 << ASO_DSEG_VALID_OFFSET) |
				(MLX5_FLOW_COLOR_GREEN << ASO_DSEG_SC_OFFSET) |
				(MLX5_METER_MODE_PKT << ASO_DSEG_MTR_MODE));
	else
		wqe->aso_dseg.mtrs[dseg_idx].v_bo_sc_bbog_mm =
				RTE_BE32((1 << ASO_DSEG_VALID_OFFSET) |
				(MLX5_FLOW_COLOR_GREEN << ASO_DSEG_SC_OFFSET));
	switch (fmp->profile.alg) {
	case RTE_MTR_SRTCM_RFC2697:
		/* Only needed for RFC2697. */
		if (fm->profile->srtcm_prm.ebs_eir)
			wqe->aso_dseg.mtrs[dseg_idx].v_bo_sc_bbog_mm |=
					RTE_BE32(1 << ASO_DSEG_BO_OFFSET);
		break;
	case RTE_MTR_TRTCM_RFC2698:
		wqe->aso_dseg.mtrs[dseg_idx].v_bo_sc_bbog_mm |=
				RTE_BE32(1 << ASO_DSEG_BBOG_OFFSET);
		break;
	case RTE_MTR_TRTCM_RFC4115:
	default:
		break;
	}
	/*
	 * Note:
	 * Due to software performance reason, the token fields will not be
	 * set when posting the WQE to ASO SQ. It will be filled by the HW
	 * automatically.
	 */
	sq->head++;
	sq->pi += 2;/* Each WQE contains 2 WQEBB's. */
	mlx5_doorbell_ring(&sh->tx_uar.bf_db, *(volatile uint64_t *)wqe,
			   sq->pi, &sq->sq_obj.db_rec[MLX5_SND_DBR],
			   !sh->tx_uar.dbnc);
	rte_spinlock_unlock(&sq->sqsl);
	return 1;
}

static void
mlx5_aso_mtrs_status_update(struct mlx5_aso_sq *sq, uint16_t aso_mtrs_nums)
{
	uint16_t size = 1 << sq->log_desc_n;
	uint16_t mask = size - 1;
	uint16_t i;
	struct mlx5_aso_mtr *aso_mtr = NULL;
	uint8_t exp_state = ASO_METER_WAIT;

	for (i = 0; i < aso_mtrs_nums; ++i) {
		aso_mtr = sq->elts[(sq->tail + i) & mask].mtr;
		MLX5_ASSERT(aso_mtr);
		(void)__atomic_compare_exchange_n(&aso_mtr->state,
				&exp_state, ASO_METER_READY,
				false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	}
}

static void
mlx5_aso_mtr_completion_handle(struct mlx5_aso_sq *sq)
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

	rte_spinlock_lock(&sq->sqsl);
	max = (uint16_t)(sq->head - sq->tail);
	if (unlikely(!max)) {
		rte_spinlock_unlock(&sq->sqsl);
		return;
	}
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
		mlx5_aso_mtrs_status_update(sq, n);
		sq->tail += n;
		rte_io_wmb();
		cq->cq_obj.db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
	}
	rte_spinlock_unlock(&sq->sqsl);
}

/**
 * Update meter parameter by send WQE.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] priv
 *   Pointer to mlx5 private data structure.
 * @param[in] fm
 *   Pointer to flow meter to be modified.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_aso_meter_update_by_wqe(struct mlx5_dev_ctx_shared *sh,
			struct mlx5_aso_mtr *mtr)
{
	struct mlx5_aso_sq *sq = &sh->mtrmng->pools_mng.sq;
	uint32_t poll_wqe_times = MLX5_MTR_POLL_WQE_CQE_TIMES;

	do {
		mlx5_aso_mtr_completion_handle(sq);
		if (mlx5_aso_mtr_sq_enqueue_single(sh, sq, mtr))
			return 0;
		/* Waiting for wqe resource. */
		rte_delay_us_sleep(MLX5_ASO_WQE_CQE_RESPONSE_DELAY);
	} while (--poll_wqe_times);
	DRV_LOG(ERR, "Fail to send WQE for ASO meter offset %d",
			mtr->offset);
	return -1;
}

/**
 * Wait for meter to be ready.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] priv
 *   Pointer to mlx5 private data structure.
 * @param[in] fm
 *   Pointer to flow meter to be modified.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_aso_mtr_wait(struct mlx5_dev_ctx_shared *sh,
			struct mlx5_aso_mtr *mtr)
{
	struct mlx5_aso_sq *sq = &sh->mtrmng->pools_mng.sq;
	uint32_t poll_cqe_times = MLX5_MTR_POLL_WQE_CQE_TIMES;

	if (__atomic_load_n(&mtr->state, __ATOMIC_RELAXED) ==
					    ASO_METER_READY)
		return 0;
	do {
		mlx5_aso_mtr_completion_handle(sq);
		if (__atomic_load_n(&mtr->state, __ATOMIC_RELAXED) ==
					    ASO_METER_READY)
			return 0;
		/* Waiting for CQE ready. */
		rte_delay_us_sleep(MLX5_ASO_WQE_CQE_RESPONSE_DELAY);
	} while (--poll_cqe_times);
	DRV_LOG(ERR, "Fail to poll CQE ready for ASO meter offset %d",
			mtr->offset);
	return -1;
}

/*
 * Post a WQE to the ASO CT SQ to modify the context.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 * @param[in] ct
 *   Pointer to the generic CT structure related to the context.
 * @param[in] profile
 *   Pointer to configuration profile.
 *
 * @return
 *   1 on success (WQE number), 0 on failure.
 */
static uint16_t
mlx5_aso_ct_sq_enqueue_single(struct mlx5_dev_ctx_shared *sh,
			      struct mlx5_aso_ct_action *ct,
			      const struct rte_flow_action_conntrack *profile)
{
	volatile struct mlx5_aso_wqe *wqe = NULL;
	struct mlx5_aso_sq *sq = &sh->ct_mng->aso_sq;
	uint16_t size = 1 << sq->log_desc_n;
	uint16_t mask = size - 1;
	uint16_t res;
	struct mlx5_aso_ct_pool *pool;
	void *desg;
	void *orig_dir;
	void *reply_dir;

	rte_spinlock_lock(&sq->sqsl);
	/* Prevent other threads to update the index. */
	res = size - (uint16_t)(sq->head - sq->tail);
	if (unlikely(!res)) {
		rte_spinlock_unlock(&sq->sqsl);
		DRV_LOG(ERR, "Fail: SQ is full and no free WQE to send");
		return 0;
	}
	wqe = &sq->sq_obj.aso_wqes[sq->head & mask];
	rte_prefetch0(&sq->sq_obj.aso_wqes[(sq->head + 1) & mask]);
	/* Fill next WQE. */
	MLX5_ASO_CT_UPDATE_STATE(ct, ASO_CONNTRACK_WAIT);
	sq->elts[sq->head & mask].ct = ct;
	sq->elts[sq->head & mask].query_data = NULL;
	pool = container_of(ct, struct mlx5_aso_ct_pool, actions[ct->offset]);
	/* Each WQE will have a single CT object. */
	wqe->general_cseg.misc = rte_cpu_to_be_32(pool->devx_obj->id +
						  ct->offset);
	wqe->general_cseg.opcode = rte_cpu_to_be_32(MLX5_OPCODE_ACCESS_ASO |
			(ASO_OPC_MOD_CONNECTION_TRACKING <<
			 WQE_CSEG_OPC_MOD_OFFSET) |
			sq->pi << WQE_CSEG_WQE_INDEX_OFFSET);
	wqe->aso_cseg.operand_masks = rte_cpu_to_be_32
			(0u |
			 (ASO_OPER_LOGICAL_OR << ASO_CSEG_COND_OPER_OFFSET) |
			 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_1_OPER_OFFSET) |
			 (ASO_OP_ALWAYS_TRUE << ASO_CSEG_COND_0_OPER_OFFSET) |
			 (BYTEWISE_64BYTE << ASO_CSEG_DATA_MASK_MODE_OFFSET));
	wqe->aso_cseg.data_mask = UINT64_MAX;
	/* To make compiler happy. */
	desg = (void *)(uintptr_t)wqe->aso_dseg.data;
	MLX5_SET(conn_track_aso, desg, valid, 1);
	MLX5_SET(conn_track_aso, desg, state, profile->state);
	MLX5_SET(conn_track_aso, desg, freeze_track, !profile->enable);
	MLX5_SET(conn_track_aso, desg, connection_assured,
		 profile->live_connection);
	MLX5_SET(conn_track_aso, desg, sack_permitted, profile->selective_ack);
	MLX5_SET(conn_track_aso, desg, challenged_acked,
		 profile->challenge_ack_passed);
	/* Heartbeat, retransmission_counter, retranmission_limit_exceeded: 0 */
	MLX5_SET(conn_track_aso, desg, heartbeat, 0);
	MLX5_SET(conn_track_aso, desg, max_ack_window,
		 profile->max_ack_window);
	MLX5_SET(conn_track_aso, desg, retransmission_counter, 0);
	MLX5_SET(conn_track_aso, desg, retranmission_limit_exceeded, 0);
	MLX5_SET(conn_track_aso, desg, retranmission_limit,
		 profile->retransmission_limit);
	MLX5_SET(conn_track_aso, desg, reply_direction_tcp_scale,
		 profile->reply_dir.scale);
	MLX5_SET(conn_track_aso, desg, reply_direction_tcp_close_initiated,
		 profile->reply_dir.close_initiated);
	/* Both directions will use the same liberal mode. */
	MLX5_SET(conn_track_aso, desg, reply_direction_tcp_liberal_enabled,
		 profile->liberal_mode);
	MLX5_SET(conn_track_aso, desg, reply_direction_tcp_data_unacked,
		 profile->reply_dir.data_unacked);
	MLX5_SET(conn_track_aso, desg, reply_direction_tcp_max_ack,
		 profile->reply_dir.last_ack_seen);
	MLX5_SET(conn_track_aso, desg, original_direction_tcp_scale,
		 profile->original_dir.scale);
	MLX5_SET(conn_track_aso, desg, original_direction_tcp_close_initiated,
		 profile->original_dir.close_initiated);
	MLX5_SET(conn_track_aso, desg, original_direction_tcp_liberal_enabled,
		 profile->liberal_mode);
	MLX5_SET(conn_track_aso, desg, original_direction_tcp_data_unacked,
		 profile->original_dir.data_unacked);
	MLX5_SET(conn_track_aso, desg, original_direction_tcp_max_ack,
		 profile->original_dir.last_ack_seen);
	MLX5_SET(conn_track_aso, desg, last_win, profile->last_window);
	MLX5_SET(conn_track_aso, desg, last_dir, profile->last_direction);
	MLX5_SET(conn_track_aso, desg, last_index, profile->last_index);
	MLX5_SET(conn_track_aso, desg, last_seq, profile->last_seq);
	MLX5_SET(conn_track_aso, desg, last_ack, profile->last_ack);
	MLX5_SET(conn_track_aso, desg, last_end, profile->last_end);
	orig_dir = MLX5_ADDR_OF(conn_track_aso, desg, original_dir);
	MLX5_SET(tcp_window_params, orig_dir, sent_end,
		 profile->original_dir.sent_end);
	MLX5_SET(tcp_window_params, orig_dir, reply_end,
		 profile->original_dir.reply_end);
	MLX5_SET(tcp_window_params, orig_dir, max_win,
		 profile->original_dir.max_win);
	MLX5_SET(tcp_window_params, orig_dir, max_ack,
		 profile->original_dir.max_ack);
	reply_dir = MLX5_ADDR_OF(conn_track_aso, desg, reply_dir);
	MLX5_SET(tcp_window_params, reply_dir, sent_end,
		 profile->reply_dir.sent_end);
	MLX5_SET(tcp_window_params, reply_dir, reply_end,
		 profile->reply_dir.reply_end);
	MLX5_SET(tcp_window_params, reply_dir, max_win,
		 profile->reply_dir.max_win);
	MLX5_SET(tcp_window_params, reply_dir, max_ack,
		 profile->reply_dir.max_ack);
	sq->head++;
	sq->pi += 2; /* Each WQE contains 2 WQEBB's. */
	mlx5_doorbell_ring(&sh->tx_uar.bf_db, *(volatile uint64_t *)wqe,
			   sq->pi, &sq->sq_obj.db_rec[MLX5_SND_DBR],
			   !sh->tx_uar.dbnc);
	rte_spinlock_unlock(&sq->sqsl);
	return 1;
}

/*
 * Update the status field of CTs to indicate ready to be used by flows.
 * A continuous number of CTs since last update.
 *
 * @param[in] sq
 *   Pointer to ASO CT SQ.
 * @param[in] num
 *   Number of CT structures to be updated.
 *
 * @return
 *   0 on success, a negative value.
 */
static void
mlx5_aso_ct_status_update(struct mlx5_aso_sq *sq, uint16_t num)
{
	uint16_t size = 1 << sq->log_desc_n;
	uint16_t mask = size - 1;
	uint16_t i;
	struct mlx5_aso_ct_action *ct = NULL;
	uint16_t idx;

	for (i = 0; i < num; i++) {
		idx = (uint16_t)((sq->tail + i) & mask);
		ct = sq->elts[idx].ct;
		MLX5_ASSERT(ct);
		MLX5_ASO_CT_UPDATE_STATE(ct, ASO_CONNTRACK_READY);
		if (sq->elts[idx].query_data)
			rte_memcpy(sq->elts[idx].query_data,
				   (char *)((uintptr_t)sq->mr.addr + idx * 64),
				   64);
	}
}

/*
 * Post a WQE to the ASO CT SQ to query the current context.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 * @param[in] ct
 *   Pointer to the generic CT structure related to the context.
 * @param[in] data
 *   Pointer to data area to be filled.
 *
 * @return
 *   1 on success (WQE number), 0 on failure.
 */
static int
mlx5_aso_ct_sq_query_single(struct mlx5_dev_ctx_shared *sh,
			    struct mlx5_aso_ct_action *ct, char *data)
{
	volatile struct mlx5_aso_wqe *wqe = NULL;
	struct mlx5_aso_sq *sq = &sh->ct_mng->aso_sq;
	uint16_t size = 1 << sq->log_desc_n;
	uint16_t mask = size - 1;
	uint16_t res;
	uint16_t wqe_idx;
	struct mlx5_aso_ct_pool *pool;
	enum mlx5_aso_ct_state state =
				__atomic_load_n(&ct->state, __ATOMIC_RELAXED);

	if (state == ASO_CONNTRACK_FREE) {
		DRV_LOG(ERR, "Fail: No context to query");
		return -1;
	} else if (state == ASO_CONNTRACK_WAIT) {
		return 0;
	}
	rte_spinlock_lock(&sq->sqsl);
	res = size - (uint16_t)(sq->head - sq->tail);
	if (unlikely(!res)) {
		rte_spinlock_unlock(&sq->sqsl);
		DRV_LOG(ERR, "Fail: SQ is full and no free WQE to send");
		return 0;
	}
	MLX5_ASO_CT_UPDATE_STATE(ct, ASO_CONNTRACK_QUERY);
	wqe = &sq->sq_obj.aso_wqes[sq->head & mask];
	/* Confirm the location and address of the prefetch instruction. */
	rte_prefetch0(&sq->sq_obj.aso_wqes[(sq->head + 1) & mask]);
	/* Fill next WQE. */
	wqe_idx = sq->head & mask;
	sq->elts[wqe_idx].ct = ct;
	sq->elts[wqe_idx].query_data = data;
	pool = container_of(ct, struct mlx5_aso_ct_pool, actions[ct->offset]);
	/* Each WQE will have a single CT object. */
	wqe->general_cseg.misc = rte_cpu_to_be_32(pool->devx_obj->id +
						  ct->offset);
	wqe->general_cseg.opcode = rte_cpu_to_be_32(MLX5_OPCODE_ACCESS_ASO |
			(ASO_OPC_MOD_CONNECTION_TRACKING <<
			 WQE_CSEG_OPC_MOD_OFFSET) |
			sq->pi << WQE_CSEG_WQE_INDEX_OFFSET);
	/*
	 * There is no write request is required.
	 * ASO_OPER_LOGICAL_AND and ASO_OP_ALWAYS_FALSE are both 0.
	 * "BYTEWISE_64BYTE" is needed for a whole context.
	 * Set to 0 directly to reduce an endian swap. (Modify should rewrite.)
	 * "data_mask" is ignored.
	 * Buffer address was already filled during initialization.
	 */
	wqe->aso_cseg.operand_masks = rte_cpu_to_be_32(BYTEWISE_64BYTE <<
					ASO_CSEG_DATA_MASK_MODE_OFFSET);
	wqe->aso_cseg.data_mask = 0;
	sq->head++;
	/*
	 * Each WQE contains 2 WQEBB's, even though
	 * data segment is not used in this case.
	 */
	sq->pi += 2;
	mlx5_doorbell_ring(&sh->tx_uar.bf_db, *(volatile uint64_t *)wqe,
			   sq->pi, &sq->sq_obj.db_rec[MLX5_SND_DBR],
			   !sh->tx_uar.dbnc);
	rte_spinlock_unlock(&sq->sqsl);
	return 1;
}

/*
 * Handle completions from WQEs sent to ASO CT.
 *
 * @param[in] mng
 *   Pointer to the CT pools management structure.
 */
static void
mlx5_aso_ct_completion_handle(struct mlx5_aso_ct_pools_mng *mng)
{
	struct mlx5_aso_sq *sq = &mng->aso_sq;
	struct mlx5_aso_cq *cq = &sq->cq;
	volatile struct mlx5_cqe *restrict cqe;
	const uint32_t cq_size = 1 << cq->log_desc_n;
	const uint32_t mask = cq_size - 1;
	uint32_t idx;
	uint32_t next_idx;
	uint16_t max;
	uint16_t n = 0;
	int ret;

	rte_spinlock_lock(&sq->sqsl);
	max = (uint16_t)(sq->head - sq->tail);
	if (unlikely(!max)) {
		rte_spinlock_unlock(&sq->sqsl);
		return;
	}
	next_idx = cq->cq_ci & mask;
	do {
		idx = next_idx;
		next_idx = (cq->cq_ci + 1) & mask;
		/* Need to confirm the position of the prefetch. */
		rte_prefetch0(&cq->cq_obj.cqes[next_idx]);
		cqe = &cq->cq_obj.cqes[idx];
		ret = check_cqe(cqe, cq_size, cq->cq_ci);
		/*
		 * Be sure owner read is done before any other cookie field or
		 * opaque field.
		 */
		rte_io_rmb();
		if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
			if (likely(ret == MLX5_CQE_STATUS_HW_OWN))
				break;
			mlx5_aso_cqe_err_handle(sq);
		} else {
			n++;
		}
		cq->cq_ci++;
	} while (1);
	if (likely(n)) {
		mlx5_aso_ct_status_update(sq, n);
		sq->tail += n;
		rte_io_wmb();
		cq->cq_obj.db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
	}
	rte_spinlock_unlock(&sq->sqsl);
}

/*
 * Update connection tracking ASO context by sending WQE.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 * @param[in] ct
 *   Pointer to connection tracking offload object.
 * @param[in] profile
 *   Pointer to connection tracking TCP parameter.
 *
 * @return
 *   0 on success, -1 on failure.
 */
int
mlx5_aso_ct_update_by_wqe(struct mlx5_dev_ctx_shared *sh,
			  struct mlx5_aso_ct_action *ct,
			  const struct rte_flow_action_conntrack *profile)
{
	uint32_t poll_wqe_times = MLX5_CT_POLL_WQE_CQE_TIMES;
	struct mlx5_aso_ct_pool *pool;

	MLX5_ASSERT(ct);
	do {
		mlx5_aso_ct_completion_handle(sh->ct_mng);
		if (mlx5_aso_ct_sq_enqueue_single(sh, ct, profile))
			return 0;
		/* Waiting for wqe resource. */
		rte_delay_us_sleep(10u);
	} while (--poll_wqe_times);
	pool = container_of(ct, struct mlx5_aso_ct_pool, actions[ct->offset]);
	DRV_LOG(ERR, "Fail to send WQE for ASO CT %d in pool %d",
		ct->offset, pool->index);
	return -1;
}

/*
 * The routine is used to wait for WQE completion to continue with queried data.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 * @param[in] ct
 *   Pointer to connection tracking offload object.
 *
 * @return
 *   0 on success, -1 on failure.
 */
int
mlx5_aso_ct_wait_ready(struct mlx5_dev_ctx_shared *sh,
		       struct mlx5_aso_ct_action *ct)
{
	struct mlx5_aso_ct_pools_mng *mng = sh->ct_mng;
	uint32_t poll_cqe_times = MLX5_CT_POLL_WQE_CQE_TIMES;
	struct mlx5_aso_ct_pool *pool;

	if (__atomic_load_n(&ct->state, __ATOMIC_RELAXED) ==
	    ASO_CONNTRACK_READY)
		return 0;
	do {
		mlx5_aso_ct_completion_handle(mng);
		if (__atomic_load_n(&ct->state, __ATOMIC_RELAXED) ==
		    ASO_CONNTRACK_READY)
			return 0;
		/* Waiting for CQE ready, consider should block or sleep. */
		rte_delay_us_sleep(MLX5_ASO_WQE_CQE_RESPONSE_DELAY);
	} while (--poll_cqe_times);
	pool = container_of(ct, struct mlx5_aso_ct_pool, actions[ct->offset]);
	DRV_LOG(ERR, "Fail to poll CQE for ASO CT %d in pool %d",
		ct->offset, pool->index);
	return -1;
}

/*
 * Convert the hardware conntrack data format into the profile.
 *
 * @param[in] profile
 *   Pointer to conntrack profile to be filled after query.
 * @param[in] wdata
 *   Pointer to data fetched from hardware.
 */
static inline void
mlx5_aso_ct_obj_analyze(struct rte_flow_action_conntrack *profile,
			char *wdata)
{
	void *o_dir = MLX5_ADDR_OF(conn_track_aso, wdata, original_dir);
	void *r_dir = MLX5_ADDR_OF(conn_track_aso, wdata, reply_dir);

	/* MLX5_GET16 should be taken into consideration. */
	profile->state = (enum rte_flow_conntrack_state)
			 MLX5_GET(conn_track_aso, wdata, state);
	profile->enable = !MLX5_GET(conn_track_aso, wdata, freeze_track);
	profile->selective_ack = MLX5_GET(conn_track_aso, wdata,
					  sack_permitted);
	profile->live_connection = MLX5_GET(conn_track_aso, wdata,
					    connection_assured);
	profile->challenge_ack_passed = MLX5_GET(conn_track_aso, wdata,
						 challenged_acked);
	profile->max_ack_window = MLX5_GET(conn_track_aso, wdata,
					   max_ack_window);
	profile->retransmission_limit = MLX5_GET(conn_track_aso, wdata,
						 retranmission_limit);
	profile->last_window = MLX5_GET(conn_track_aso, wdata, last_win);
	profile->last_direction = MLX5_GET(conn_track_aso, wdata, last_dir);
	profile->last_index = (enum rte_flow_conntrack_tcp_last_index)
			      MLX5_GET(conn_track_aso, wdata, last_index);
	profile->last_seq = MLX5_GET(conn_track_aso, wdata, last_seq);
	profile->last_ack = MLX5_GET(conn_track_aso, wdata, last_ack);
	profile->last_end = MLX5_GET(conn_track_aso, wdata, last_end);
	profile->liberal_mode = MLX5_GET(conn_track_aso, wdata,
				reply_direction_tcp_liberal_enabled) |
				MLX5_GET(conn_track_aso, wdata,
				original_direction_tcp_liberal_enabled);
	/* No liberal in the RTE structure profile. */
	profile->reply_dir.scale = MLX5_GET(conn_track_aso, wdata,
					    reply_direction_tcp_scale);
	profile->reply_dir.close_initiated = MLX5_GET(conn_track_aso, wdata,
					reply_direction_tcp_close_initiated);
	profile->reply_dir.data_unacked = MLX5_GET(conn_track_aso, wdata,
					reply_direction_tcp_data_unacked);
	profile->reply_dir.last_ack_seen = MLX5_GET(conn_track_aso, wdata,
					reply_direction_tcp_max_ack);
	profile->reply_dir.sent_end = MLX5_GET(tcp_window_params,
					       r_dir, sent_end);
	profile->reply_dir.reply_end = MLX5_GET(tcp_window_params,
						r_dir, reply_end);
	profile->reply_dir.max_win = MLX5_GET(tcp_window_params,
					      r_dir, max_win);
	profile->reply_dir.max_ack = MLX5_GET(tcp_window_params,
					      r_dir, max_ack);
	profile->original_dir.scale = MLX5_GET(conn_track_aso, wdata,
					       original_direction_tcp_scale);
	profile->original_dir.close_initiated = MLX5_GET(conn_track_aso, wdata,
					original_direction_tcp_close_initiated);
	profile->original_dir.data_unacked = MLX5_GET(conn_track_aso, wdata,
					original_direction_tcp_data_unacked);
	profile->original_dir.last_ack_seen = MLX5_GET(conn_track_aso, wdata,
					original_direction_tcp_max_ack);
	profile->original_dir.sent_end = MLX5_GET(tcp_window_params,
						  o_dir, sent_end);
	profile->original_dir.reply_end = MLX5_GET(tcp_window_params,
						   o_dir, reply_end);
	profile->original_dir.max_win = MLX5_GET(tcp_window_params,
						 o_dir, max_win);
	profile->original_dir.max_ack = MLX5_GET(tcp_window_params,
						 o_dir, max_ack);
}

/*
 * Query connection tracking information parameter by send WQE.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] ct
 *   Pointer to connection tracking offload object.
 * @param[out] profile
 *   Pointer to connection tracking TCP information.
 *
 * @return
 *   0 on success, -1 on failure.
 */
int
mlx5_aso_ct_query_by_wqe(struct mlx5_dev_ctx_shared *sh,
			 struct mlx5_aso_ct_action *ct,
			 struct rte_flow_action_conntrack *profile)
{
	uint32_t poll_wqe_times = MLX5_CT_POLL_WQE_CQE_TIMES;
	struct mlx5_aso_ct_pool *pool;
	char out_data[64 * 2];
	int ret;

	MLX5_ASSERT(ct);
	do {
		mlx5_aso_ct_completion_handle(sh->ct_mng);
		ret = mlx5_aso_ct_sq_query_single(sh, ct, out_data);
		if (ret < 0)
			return ret;
		else if (ret > 0)
			goto data_handle;
		/* Waiting for wqe resource or state. */
		else
			rte_delay_us_sleep(10u);
	} while (--poll_wqe_times);
	pool = container_of(ct, struct mlx5_aso_ct_pool, actions[ct->offset]);
	DRV_LOG(ERR, "Fail to send WQE for ASO CT %d in pool %d",
		ct->offset, pool->index);
	return -1;
data_handle:
	ret = mlx5_aso_ct_wait_ready(sh, ct);
	if (!ret)
		mlx5_aso_ct_obj_analyze(profile, out_data);
	return ret;
}

/*
 * Make sure the conntrack context is synchronized with hardware before
 * creating a flow rule that uses it.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 * @param[in] ct
 *   Pointer to connection tracking offload object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_aso_ct_available(struct mlx5_dev_ctx_shared *sh,
		      struct mlx5_aso_ct_action *ct)
{
	struct mlx5_aso_ct_pools_mng *mng = sh->ct_mng;
	uint32_t poll_cqe_times = MLX5_CT_POLL_WQE_CQE_TIMES;
	enum mlx5_aso_ct_state state =
				__atomic_load_n(&ct->state, __ATOMIC_RELAXED);

	if (state == ASO_CONNTRACK_FREE) {
		rte_errno = ENXIO;
		return -rte_errno;
	} else if (state == ASO_CONNTRACK_READY ||
		   state == ASO_CONNTRACK_QUERY) {
		return 0;
	}
	do {
		mlx5_aso_ct_completion_handle(mng);
		state = __atomic_load_n(&ct->state, __ATOMIC_RELAXED);
		if (state == ASO_CONNTRACK_READY ||
		    state == ASO_CONNTRACK_QUERY)
			return 0;
		/* Waiting for CQE ready, consider should block or sleep. */
		rte_delay_us_sleep(MLX5_ASO_WQE_CQE_RESPONSE_DELAY);
	} while (--poll_cqe_times);
	rte_errno = EBUSY;
	return -rte_errno;
}
