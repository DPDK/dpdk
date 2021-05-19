/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <mlx5_prm.h>
#include <rte_malloc.h>
#include <rte_cycles.h>

#include <mlx5_malloc.h>
#include <mlx5_common_os.h>

#include "mlx5.h"
#include "mlx5_flow.h"

/**
 * Destroy Completion Queue used for ASO access.
 *
 * @param[in] cq
 *   ASO CQ to destroy.
 */
static void
mlx5_aso_cq_destroy(struct mlx5_aso_cq *cq)
{
	if (cq->cq)
		claim_zero(mlx5_devx_cmd_destroy(cq->cq));
	if (cq->umem_obj)
		claim_zero(mlx5_glue->devx_umem_dereg(cq->umem_obj));
	if (cq->umem_buf)
		mlx5_free((void *)(uintptr_t)cq->umem_buf);
	memset(cq, 0, sizeof(*cq));
}

/**
 * Create Completion Queue used for ASO access.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] cq
 *   Pointer to CQ to create.
 * @param[in] log_desc_n
 *   Log of number of descriptors in queue.
 * @param[in] socket
 *   Socket to use for allocation.
 * @param[in] uar_page_id
 *   UAR page ID to use.
 * @param[in] eqn
 *   EQ number.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_aso_cq_create(void *ctx, struct mlx5_aso_cq *cq, uint16_t log_desc_n,
		   int socket, int uar_page_id, uint32_t eqn)
{
	struct mlx5_devx_cq_attr attr = { 0 };
	size_t pgsize = sysconf(_SC_PAGESIZE);
	uint32_t umem_size;
	uint16_t cq_size = 1 << log_desc_n;

	cq->log_desc_n = log_desc_n;
	umem_size = sizeof(struct mlx5_cqe) * cq_size + sizeof(*cq->db_rec) * 2;
	cq->umem_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
				   4096, socket);
	if (!cq->umem_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for CQ.");
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	cq->umem_obj = mlx5_glue->devx_umem_reg(ctx,
						(void *)(uintptr_t)cq->umem_buf,
						umem_size,
						IBV_ACCESS_LOCAL_WRITE);
	if (!cq->umem_obj) {
		DRV_LOG(ERR, "Failed to register umem for aso CQ.");
		goto error;
	}
	attr.q_umem_valid = 1;
	attr.db_umem_valid = 1;
	attr.use_first_only = 0;
	attr.overrun_ignore = 0;
	attr.uar_page_id = uar_page_id;
	attr.q_umem_id = mlx5_os_get_umem_id(cq->umem_obj);
	attr.q_umem_offset = 0;
	attr.db_umem_id = attr.q_umem_id;
	attr.db_umem_offset = sizeof(struct mlx5_cqe) * cq_size;
	attr.eqn = eqn;
	attr.log_cq_size = log_desc_n;
	attr.log_page_size = rte_log2_u32(pgsize);
	cq->cq = mlx5_devx_cmd_create_cq(ctx, &attr);
	if (!cq->cq)
		goto error;
	cq->db_rec = RTE_PTR_ADD(cq->umem_buf, (uintptr_t)attr.db_umem_offset);
	cq->cq_ci = 0;
	memset((void *)(uintptr_t)cq->umem_buf, 0xFF, attr.db_umem_offset);
	return 0;
error:
	mlx5_aso_cq_destroy(cq);
	return -1;
}

/**
 * Free MR resources.
 *
 * @param[in] mr
 *   MR to free.
 */
static void
mlx5_aso_devx_dereg_mr(struct mlx5_aso_devx_mr *mr)
{
	claim_zero(mlx5_devx_cmd_destroy(mr->mkey));
	if (!mr->is_indirect && mr->umem)
		claim_zero(mlx5_glue->devx_umem_dereg(mr->umem));
	mlx5_free(mr->buf);
	memset(mr, 0, sizeof(*mr));
}

/**
 * Register Memory Region.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in] length
 *   Size of MR buffer.
 * @param[in/out] mr
 *   Pointer to MR to create.
 * @param[in] socket
 *   Socket to use for allocation.
 * @param[in] pdn
 *   Protection Domain number to use.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_aso_devx_reg_mr(void *ctx, size_t length, struct mlx5_aso_devx_mr *mr,
		     int socket, int pdn)
{
	struct mlx5_devx_mkey_attr mkey_attr;

	mr->buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, length, 4096,
			      socket);
	if (!mr->buf) {
		DRV_LOG(ERR, "Failed to create ASO bits mem for MR by Devx.");
		return -1;
	}
	mr->umem = mlx5_glue->devx_umem_reg(ctx, mr->buf, length,
						 IBV_ACCESS_LOCAL_WRITE);
	if (!mr->umem) {
		DRV_LOG(ERR, "Failed to register Umem for MR by Devx.");
		goto error;
	}
	mkey_attr.addr = (uintptr_t)mr->buf;
	mkey_attr.size = length;
	mkey_attr.umem_id = mlx5_os_get_umem_id(mr->umem);
	mkey_attr.pd = pdn;
	mkey_attr.pg_access = 1;
	mkey_attr.klm_array = NULL;
	mkey_attr.klm_num = 0;
	mkey_attr.relaxed_ordering_read = 0;
	mkey_attr.relaxed_ordering_write = 0;
	mr->mkey = mlx5_devx_cmd_mkey_create(ctx, &mkey_attr);
	if (!mr->mkey) {
		DRV_LOG(ERR, "Failed to create direct Mkey.");
		goto error;
	}
	mr->length = length;
	mr->is_indirect = false;
	return 0;
error:
	if (mr->umem)
		claim_zero(mlx5_glue->devx_umem_dereg(mr->umem));
	mlx5_free(mr->buf);
	return -1;
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
	if (sq->wqe_umem) {
		mlx5_glue->devx_umem_dereg(sq->wqe_umem);
		sq->wqe_umem = NULL;
	}
	if (sq->umem_buf) {
		mlx5_free((void *)(uintptr_t)sq->umem_buf);
		sq->umem_buf = NULL;
	}
	if (sq->sq) {
		mlx5_devx_cmd_destroy(sq->sq);
		sq->sq = NULL;
	}
	if (sq->cq.cq)
		mlx5_aso_cq_destroy(&sq->cq);
	mlx5_aso_devx_dereg_mr(&sq->mr);
	memset(sq, 0, sizeof(*sq));
}

/**
 * Initialize Send Queue used for ASO access.
 *
 * @param[in] sq
 *   ASO SQ to initialize.
 */
static void
mlx5_aso_init_sq(struct mlx5_aso_sq *sq)
{
	volatile struct mlx5_aso_wqe *restrict wqe;
	int i;
	int size = 1 << sq->log_desc_n;
	uint64_t addr;

	/* All the next fields state should stay constant. */
	for (i = 0, wqe = &sq->wqes[0]; i < size; ++i, ++wqe) {
		wqe->general_cseg.sq_ds = rte_cpu_to_be_32((sq->sqn << 8) |
							  (sizeof(*wqe) >> 4));
		wqe->aso_cseg.lkey = rte_cpu_to_be_32(sq->mr.mkey->id);
		addr = (uint64_t)((uint64_t *)sq->mr.buf + i *
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
 * Create Send Queue used for ASO access.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] sq
 *   Pointer to SQ to create.
 * @param[in] socket
 *   Socket to use for allocation.
 * @param[in] uar
 *   User Access Region object.
 * @param[in] pdn
 *   Protection Domain number to use.
 * @param[in] eqn
 *   EQ number.
 * @param[in] log_desc_n
 *   Log of number of descriptors in queue.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_aso_sq_create(void *ctx, struct mlx5_aso_sq *sq, int socket,
		   struct mlx5dv_devx_uar *uar, uint32_t pdn,
		   uint32_t eqn,  uint16_t log_desc_n, uint32_t ts_format)
{
	struct mlx5_devx_create_sq_attr attr = { 0 };
	struct mlx5_devx_modify_sq_attr modify_attr = { 0 };
	size_t pgsize = sysconf(_SC_PAGESIZE);
	struct mlx5_devx_wq_attr *wq_attr = &attr.wq_attr;
	uint32_t sq_desc_n = 1 << log_desc_n;
	uint32_t wq_size = sizeof(struct mlx5_aso_wqe) * sq_desc_n;
	int ret;

	if (mlx5_aso_devx_reg_mr(ctx, (MLX5_ASO_AGE_ACTIONS_PER_POOL / 8) *
				 sq_desc_n, &sq->mr, socket, pdn))
		return -1;
	if (mlx5_aso_cq_create(ctx, &sq->cq, log_desc_n, socket,
				mlx5_os_get_devx_uar_page_id(uar), eqn))
		goto error;
	sq->log_desc_n = log_desc_n;
	sq->umem_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, wq_size +
				   sizeof(*sq->db_rec) * 2, 4096, socket);
	if (!sq->umem_buf) {
		DRV_LOG(ERR, "Can't allocate wqe buffer.");
		rte_errno = ENOMEM;
		goto error;
	}
	sq->wqe_umem = mlx5_glue->devx_umem_reg(ctx,
						(void *)(uintptr_t)sq->umem_buf,
						wq_size +
						sizeof(*sq->db_rec) * 2,
						IBV_ACCESS_LOCAL_WRITE);
	if (!sq->wqe_umem) {
		DRV_LOG(ERR, "Failed to register umem for SQ.");
		rte_errno = ENOMEM;
		goto error;
	}
	attr.state = MLX5_SQC_STATE_RST;
	attr.tis_lst_sz = 0;
	attr.tis_num = 0;
	attr.user_index = 0xFFFF;
	attr.cqn = sq->cq.cq->id;
	attr.ts_format = mlx5_ts_format_conv(ts_format);
	wq_attr->uar_page = mlx5_os_get_devx_uar_page_id(uar);
	wq_attr->pd = pdn;
	wq_attr->wq_type = MLX5_WQ_TYPE_CYCLIC;
	wq_attr->log_wq_pg_sz = rte_log2_u32(pgsize);
	wq_attr->wq_umem_id = mlx5_os_get_umem_id(sq->wqe_umem);
	wq_attr->wq_umem_offset = 0;
	wq_attr->wq_umem_valid = 1;
	wq_attr->log_wq_stride = 6;
	wq_attr->log_wq_sz = rte_log2_u32(wq_size) - 6;
	wq_attr->dbr_umem_id = wq_attr->wq_umem_id;
	wq_attr->dbr_addr = wq_size;
	wq_attr->dbr_umem_valid = 1;
	sq->sq = mlx5_devx_cmd_create_sq(ctx, &attr);
	if (!sq->sq) {
		DRV_LOG(ERR, "Can't create sq object.");
		rte_errno  = ENOMEM;
		goto error;
	}
	modify_attr.state = MLX5_SQC_STATE_RDY;
	ret = mlx5_devx_cmd_modify_sq(sq->sq, &modify_attr);
	if (ret) {
		DRV_LOG(ERR, "Can't change sq state to ready.");
		rte_errno  = ENOMEM;
		goto error;
	}
	sq->pi = 0;
	sq->head = 0;
	sq->tail = 0;
	sq->sqn = sq->sq->id;
	sq->db_rec = RTE_PTR_ADD(sq->umem_buf, (uintptr_t)(wq_attr->dbr_addr));
	sq->uar_addr = (volatile uint64_t *)((uint8_t *)uar->base_addr + 0x800);
	mlx5_aso_init_sq(sq);
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
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_aso_queue_init(struct mlx5_dev_ctx_shared *sh)
{
	return mlx5_aso_sq_create(sh->ctx, &sh->aso_age_mng->aso_sq, 0,
				  sh->tx_uar, sh->pdn, sh->eqn,
				  MLX5_ASO_QUEUE_LOG_DESC, sh->sq_ts_format);
}

/**
 * API to destroy Send Queue used for ASO access.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 */
void
mlx5_aso_queue_uninit(struct mlx5_dev_ctx_shared *sh)
{
	mlx5_aso_destroy_sq(&sh->aso_age_mng->aso_sq);
}

/**
 * Write a burst of WQEs to ASO SQ.
 *
 * @param[in] mng
 *   ASO management data, contains the SQ.
 * @param[in] n
 *   Index of the last valid pool.
 *
 * @return
 *   Number of WQEs in burst.
 */
static uint16_t
mlx5_aso_sq_enqueue_burst(struct mlx5_aso_age_mng *mng, uint16_t n)
{
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
		wqe = &sq->wqes[sq->head & mask];
		rte_prefetch0(&sq->wqes[(sq->head + 1) & mask]);
		/* Fill next WQE. */
		rte_spinlock_lock(&mng->resize_sl);
		pool = mng->pools[sq->next];
		rte_spinlock_unlock(&mng->resize_sl);
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
	rte_io_wmb();
	sq->db_rec[MLX5_SND_DBR] = rte_cpu_to_be_32(sq->pi);
	rte_wmb();
	*sq->uar_addr = *(volatile uint64_t *)wqe; /* Assume 64 bit ARCH.*/
	rte_wmb();
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
				(volatile struct mlx5_err_cqe *)&cq->cqes[idx];

	cq->errors++;
	idx = rte_be_to_cpu_16(cqe->wqe_counter) & (1u << sq->log_desc_n);
	mlx5_aso_dump_err_objs((volatile uint32_t *)cqe,
				 (volatile uint32_t *)&sq->wqes[idx]);
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
		uint64_t *addr = sq->mr.buf;
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
		rte_prefetch0(&cq->cqes[next_idx]);
		cqe = &cq->cqes[idx];
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
		cq->db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
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

	rte_spinlock_lock(&sh->aso_age_mng->resize_sl);
	n = sh->aso_age_mng->next;
	rte_spinlock_unlock(&sh->aso_age_mng->resize_sl);
	mlx5_aso_completion_handle(sh);
	if (sq->next == n) {
		/* End of loop: wait 1 second. */
		us = US_PER_S;
		sq->next = 0;
	}
	mlx5_aso_sq_enqueue_burst(sh->aso_age_mng, n);
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
mlx5_aso_queue_start(struct mlx5_dev_ctx_shared *sh)
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
mlx5_aso_queue_stop(struct mlx5_dev_ctx_shared *sh)
{
	int retries = 1024;

	if (!sh->aso_age_mng->aso_sq.sq)
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
