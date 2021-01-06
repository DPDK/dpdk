/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <stdint.h>

#include <rte_errno.h>
#include <rte_common.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_common_os.h>

#include "mlx5_prm.h"
#include "mlx5_devx_cmds.h"
#include "mlx5_common_utils.h"
#include "mlx5_malloc.h"
#include "mlx5_common.h"
#include "mlx5_common_devx.h"

/**
 * Destroy DevX Completion Queue.
 *
 * @param[in] cq
 *   DevX CQ to destroy.
 */
void
mlx5_devx_cq_destroy(struct mlx5_devx_cq *cq)
{
	if (cq->cq)
		claim_zero(mlx5_devx_cmd_destroy(cq->cq));
	if (cq->umem_obj)
		claim_zero(mlx5_os_umem_dereg(cq->umem_obj));
	if (cq->umem_buf)
		mlx5_free((void *)(uintptr_t)cq->umem_buf);
}

/* Mark all CQEs initially as invalid. */
static void
mlx5_cq_init(struct mlx5_devx_cq *cq_obj, uint16_t cq_size)
{
	volatile struct mlx5_cqe *cqe = cq_obj->cqes;
	uint16_t i;

	for (i = 0; i < cq_size; i++, cqe++)
		cqe->op_own = (MLX5_CQE_INVALID << 4) | MLX5_CQE_OWNER_MASK;
}

/**
 * Create Completion Queue using DevX API.
 *
 * Get a pointer to partially initialized attributes structure, and updates the
 * following fields:
 *   q_umem_valid
 *   q_umem_id
 *   q_umem_offset
 *   db_umem_valid
 *   db_umem_id
 *   db_umem_offset
 *   eqn
 *   log_cq_size
 *   log_page_size
 * All other fields are updated by caller.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] cq_obj
 *   Pointer to CQ to create.
 * @param[in] log_desc_n
 *   Log of number of descriptors in queue.
 * @param[in] attr
 *   Pointer to CQ attributes structure.
 * @param[in] socket
 *   Socket to use for allocation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cq_create(void *ctx, struct mlx5_devx_cq *cq_obj, uint16_t log_desc_n,
		    struct mlx5_devx_cq_attr *attr, int socket)
{
	struct mlx5_devx_obj *cq = NULL;
	struct mlx5dv_devx_umem *umem_obj = NULL;
	void *umem_buf = NULL;
	size_t page_size = rte_mem_page_size();
	size_t alignment = MLX5_CQE_BUF_ALIGNMENT;
	uint32_t umem_size, umem_dbrec;
	uint32_t eqn;
	uint16_t cq_size = 1 << log_desc_n;
	int ret;

	if (page_size == (size_t)-1 || alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get page_size.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Query first EQN. */
	ret = mlx5_glue->devx_query_eqn(ctx, 0, &eqn);
	if (ret) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to query event queue number.");
		return -rte_errno;
	}
	/* Allocate memory buffer for CQEs and doorbell record. */
	umem_size = sizeof(struct mlx5_cqe) * cq_size;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	umem_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
			       alignment, socket);
	if (!umem_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for CQ.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Register allocated buffer in user space with DevX. */
	umem_obj = mlx5_os_umem_reg(ctx, (void *)(uintptr_t)umem_buf, umem_size,
				    IBV_ACCESS_LOCAL_WRITE);
	if (!umem_obj) {
		DRV_LOG(ERR, "Failed to register umem for CQ.");
		rte_errno = errno;
		goto error;
	}
	/* Fill attributes for CQ object creation. */
	attr->q_umem_valid = 1;
	attr->q_umem_id = mlx5_os_get_umem_id(umem_obj);
	attr->q_umem_offset = 0;
	attr->db_umem_valid = 1;
	attr->db_umem_id = attr->q_umem_id;
	attr->db_umem_offset = umem_dbrec;
	attr->eqn = eqn;
	attr->log_cq_size = log_desc_n;
	attr->log_page_size = rte_log2_u32(page_size);
	/* Create completion queue object with DevX. */
	cq = mlx5_devx_cmd_create_cq(ctx, attr);
	if (!cq) {
		DRV_LOG(ERR, "Can't create DevX CQ object.");
		rte_errno  = ENOMEM;
		goto error;
	}
	cq_obj->umem_buf = umem_buf;
	cq_obj->umem_obj = umem_obj;
	cq_obj->cq = cq;
	cq_obj->db_rec = RTE_PTR_ADD(cq_obj->umem_buf, umem_dbrec);
	/* Mark all CQEs initially as invalid. */
	mlx5_cq_init(cq_obj, cq_size);
	return 0;
error:
	ret = rte_errno;
	if (umem_obj)
		claim_zero(mlx5_os_umem_dereg(umem_obj));
	if (umem_buf)
		mlx5_free((void *)(uintptr_t)umem_buf);
	rte_errno = ret;
	return -rte_errno;
}
