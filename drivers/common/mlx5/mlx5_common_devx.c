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
#include "mlx5_common_log.h"
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

	for (i = 0; i < cq_size; i++, cqe++) {
		cqe->op_own = (MLX5_CQE_INVALID << 4) | MLX5_CQE_OWNER_MASK;
		cqe->validity_iteration_count = MLX5_CQE_VIC_INIT;
	}
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
	uint32_t num_of_cqes = RTE_BIT32(log_desc_n);
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
	umem_size = sizeof(struct mlx5_cqe) * num_of_cqes;
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
	mlx5_cq_init(cq_obj, num_of_cqes);
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

/**
 * Destroy DevX Send Queue.
 *
 * @param[in] sq
 *   DevX SQ to destroy.
 */
void
mlx5_devx_sq_destroy(struct mlx5_devx_sq *sq)
{
	if (sq->sq)
		claim_zero(mlx5_devx_cmd_destroy(sq->sq));
	if (sq->umem_obj)
		claim_zero(mlx5_os_umem_dereg(sq->umem_obj));
	if (sq->umem_buf)
		mlx5_free((void *)(uintptr_t)sq->umem_buf);
}

/**
 * Create Send Queue using DevX API.
 *
 * Get a pointer to partially initialized attributes structure, and updates the
 * following fields:
 *   wq_type
 *   wq_umem_valid
 *   wq_umem_id
 *   wq_umem_offset
 *   dbr_umem_valid
 *   dbr_umem_id
 *   dbr_addr
 *   log_wq_stride
 *   log_wq_sz
 *   log_wq_pg_sz
 * All other fields are updated by caller.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] sq_obj
 *   Pointer to SQ to create.
 * @param[in] log_wqbb_n
 *   Log of number of WQBBs in queue.
 * @param[in] attr
 *   Pointer to SQ attributes structure.
 * @param[in] socket
 *   Socket to use for allocation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_sq_create(void *ctx, struct mlx5_devx_sq *sq_obj, uint16_t log_wqbb_n,
		    struct mlx5_devx_create_sq_attr *attr, int socket)
{
	struct mlx5_devx_obj *sq = NULL;
	struct mlx5dv_devx_umem *umem_obj = NULL;
	void *umem_buf = NULL;
	size_t alignment = MLX5_WQE_BUF_ALIGNMENT;
	uint32_t umem_size, umem_dbrec;
	uint32_t num_of_wqbbs = RTE_BIT32(log_wqbb_n);
	int ret;

	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get WQE buf alignment.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Allocate memory buffer for WQEs and doorbell record. */
	umem_size = MLX5_WQE_SIZE * num_of_wqbbs;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	umem_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
			       alignment, socket);
	if (!umem_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for SQ.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Register allocated buffer in user space with DevX. */
	umem_obj = mlx5_os_umem_reg(ctx, (void *)(uintptr_t)umem_buf, umem_size,
				    IBV_ACCESS_LOCAL_WRITE);
	if (!umem_obj) {
		DRV_LOG(ERR, "Failed to register umem for SQ.");
		rte_errno = errno;
		goto error;
	}
	/* Fill attributes for SQ object creation. */
	attr->wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC;
	attr->wq_attr.wq_umem_valid = 1;
	attr->wq_attr.wq_umem_id = mlx5_os_get_umem_id(umem_obj);
	attr->wq_attr.wq_umem_offset = 0;
	attr->wq_attr.dbr_umem_valid = 1;
	attr->wq_attr.dbr_umem_id = attr->wq_attr.wq_umem_id;
	attr->wq_attr.dbr_addr = umem_dbrec;
	attr->wq_attr.log_wq_stride = rte_log2_u32(MLX5_WQE_SIZE);
	attr->wq_attr.log_wq_sz = log_wqbb_n;
	attr->wq_attr.log_wq_pg_sz = MLX5_LOG_PAGE_SIZE;
	/* Create send queue object with DevX. */
	sq = mlx5_devx_cmd_create_sq(ctx, attr);
	if (!sq) {
		DRV_LOG(ERR, "Can't create DevX SQ object.");
		rte_errno = ENOMEM;
		goto error;
	}
	sq_obj->umem_buf = umem_buf;
	sq_obj->umem_obj = umem_obj;
	sq_obj->sq = sq;
	sq_obj->db_rec = RTE_PTR_ADD(sq_obj->umem_buf, umem_dbrec);
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

/**
 * Destroy DevX Receive Queue resources.
 *
 * @param[in] rq_res
 *   DevX RQ resource to destroy.
 */
static void
mlx5_devx_wq_res_destroy(struct mlx5_devx_wq_res *rq_res)
{
	if (rq_res->umem_obj)
		claim_zero(mlx5_os_umem_dereg(rq_res->umem_obj));
	if (rq_res->umem_buf)
		mlx5_free((void *)(uintptr_t)rq_res->umem_buf);
	memset(rq_res, 0, sizeof(*rq_res));
}

/**
 * Destroy DevX Receive Memory Pool.
 *
 * @param[in] rmp
 *   DevX RMP to destroy.
 */
static void
mlx5_devx_rmp_destroy(struct mlx5_devx_rmp *rmp)
{
	MLX5_ASSERT(rmp->ref_cnt == 0);
	if (rmp->rmp) {
		claim_zero(mlx5_devx_cmd_destroy(rmp->rmp));
		rmp->rmp = NULL;
	}
	mlx5_devx_wq_res_destroy(&rmp->wq);
}

/**
 * Destroy DevX Queue Pair.
 *
 * @param[in] qp
 *   DevX QP to destroy.
 */
void
mlx5_devx_qp_destroy(struct mlx5_devx_qp *qp)
{
	if (qp->qp)
		claim_zero(mlx5_devx_cmd_destroy(qp->qp));
	if (qp->umem_obj)
		claim_zero(mlx5_os_umem_dereg(qp->umem_obj));
	if (qp->umem_buf)
		mlx5_free((void *)(uintptr_t)qp->umem_buf);
}

/**
 * Create Queue Pair using DevX API.
 *
 * Get a pointer to partially initialized attributes structure, and updates the
 * following fields:
 *   wq_umem_id
 *   wq_umem_offset
 *   dbr_umem_valid
 *   dbr_umem_id
 *   dbr_address
 *   log_page_size
 * All other fields are updated by caller.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] qp_obj
 *   Pointer to QP to create.
 * @param[in] queue_size
 *   Size of queue to create.
 * @param[in] attr
 *   Pointer to QP attributes structure.
 * @param[in] socket
 *   Socket to use for allocation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_qp_create(void *ctx, struct mlx5_devx_qp *qp_obj, uint32_t queue_size,
		    struct mlx5_devx_qp_attr *attr, int socket)
{
	struct mlx5_devx_obj *qp = NULL;
	struct mlx5dv_devx_umem *umem_obj = NULL;
	void *umem_buf = NULL;
	size_t alignment = MLX5_WQE_BUF_ALIGNMENT;
	uint32_t umem_size, umem_dbrec;
	int ret;

	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get WQE buf alignment.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Allocate memory buffer for WQEs and doorbell record. */
	umem_size = queue_size;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	umem_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
			       alignment, socket);
	if (!umem_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for QP.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Register allocated buffer in user space with DevX. */
	umem_obj = mlx5_os_umem_reg(ctx, (void *)(uintptr_t)umem_buf, umem_size,
				    IBV_ACCESS_LOCAL_WRITE);
	if (!umem_obj) {
		DRV_LOG(ERR, "Failed to register umem for QP.");
		rte_errno = errno;
		goto error;
	}
	/* Fill attributes for SQ object creation. */
	attr->wq_umem_id = mlx5_os_get_umem_id(umem_obj);
	attr->wq_umem_offset = 0;
	attr->dbr_umem_valid = 1;
	attr->dbr_umem_id = attr->wq_umem_id;
	attr->dbr_address = umem_dbrec;
	attr->log_page_size = MLX5_LOG_PAGE_SIZE;
	/* Create send queue object with DevX. */
	qp = mlx5_devx_cmd_create_qp(ctx, attr);
	if (!qp) {
		DRV_LOG(ERR, "Can't create DevX QP object.");
		rte_errno = ENOMEM;
		goto error;
	}
	qp_obj->umem_buf = umem_buf;
	qp_obj->umem_obj = umem_obj;
	qp_obj->qp = qp;
	qp_obj->db_rec = RTE_PTR_ADD(qp_obj->umem_buf, umem_dbrec);
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

/**
 * Destroy DevX Receive Queue.
 *
 * @param[in] rq
 *   DevX RQ to destroy.
 */
void
mlx5_devx_rq_destroy(struct mlx5_devx_rq *rq)
{
	if (rq->rq) {
		claim_zero(mlx5_devx_cmd_destroy(rq->rq));
		rq->rq = NULL;
		if (rq->rmp)
			rq->rmp->ref_cnt--;
	}
	if (rq->rmp == NULL) {
		mlx5_devx_wq_res_destroy(&rq->wq);
	} else {
		if (rq->rmp->ref_cnt == 0)
			mlx5_devx_rmp_destroy(rq->rmp);
	}
}

/**
 * Create WQ resources using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in] wqe_size
 *   Size of WQE structure.
 * @param[in] log_wqbb_n
 *   Log of number of WQBBs in queue.
 * @param[in] socket
 *   Socket to use for allocation.
 * @param[out] wq_attr
 *   Pointer to WQ attributes structure.
 * @param[out] wq_res
 *   Pointer to WQ resource to create.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_wq_init(void *ctx, uint32_t wqe_size, uint16_t log_wqbb_n, int socket,
		  struct mlx5_devx_wq_attr *wq_attr,
		  struct mlx5_devx_wq_res *wq_res)
{
	struct mlx5dv_devx_umem *umem_obj = NULL;
	void *umem_buf = NULL;
	size_t alignment = MLX5_WQE_BUF_ALIGNMENT;
	uint32_t umem_size, umem_dbrec;
	int ret;

	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get WQE buf alignment.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Allocate memory buffer for WQEs and doorbell record. */
	umem_size = wqe_size * (1 << log_wqbb_n);
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	umem_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
			       alignment, socket);
	if (!umem_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for RQ.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Register allocated buffer in user space with DevX. */
	umem_obj = mlx5_os_umem_reg(ctx, (void *)(uintptr_t)umem_buf,
				    umem_size, 0);
	if (!umem_obj) {
		DRV_LOG(ERR, "Failed to register umem for RQ.");
		rte_errno = errno;
		goto error;
	}
	/* Fill WQ attributes for RQ/RMP object creation. */
	wq_attr->wq_umem_valid = 1;
	wq_attr->wq_umem_id = mlx5_os_get_umem_id(umem_obj);
	wq_attr->wq_umem_offset = 0;
	wq_attr->dbr_umem_valid = 1;
	wq_attr->dbr_umem_id = wq_attr->wq_umem_id;
	wq_attr->dbr_addr = umem_dbrec;
	wq_attr->log_wq_pg_sz = MLX5_LOG_PAGE_SIZE;
	/* Fill attributes for RQ object creation. */
	wq_res->umem_buf = umem_buf;
	wq_res->umem_obj = umem_obj;
	wq_res->db_rec = RTE_PTR_ADD(umem_buf, umem_dbrec);
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

/**
 * Create standalone Receive Queue using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] rq_obj
 *   Pointer to RQ to create.
 * @param[in] wqe_size
 *   Size of WQE structure.
 * @param[in] log_wqbb_n
 *   Log of number of WQBBs in queue.
 * @param[in] attr
 *   Pointer to RQ attributes structure.
 * @param[in] socket
 *   Socket to use for allocation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_rq_std_create(void *ctx, struct mlx5_devx_rq *rq_obj,
			uint32_t wqe_size, uint16_t log_wqbb_n,
			struct mlx5_devx_create_rq_attr *attr, int socket)
{
	struct mlx5_devx_obj *rq;
	int ret;

	ret = mlx5_devx_wq_init(ctx, wqe_size, log_wqbb_n, socket,
				&attr->wq_attr, &rq_obj->wq);
	if (ret != 0)
		return ret;
	/* Create receive queue object with DevX. */
	rq = mlx5_devx_cmd_create_rq(ctx, attr, socket);
	if (!rq) {
		DRV_LOG(ERR, "Can't create DevX RQ object.");
		rte_errno = ENOMEM;
		goto error;
	}
	rq_obj->rq = rq;
	return 0;
error:
	ret = rte_errno;
	mlx5_devx_wq_res_destroy(&rq_obj->wq);
	rte_errno = ret;
	return -rte_errno;
}

/**
 * Create Receive Memory Pool using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] rq_obj
 *   Pointer to RQ to create.
 * @param[in] wqe_size
 *   Size of WQE structure.
 * @param[in] log_wqbb_n
 *   Log of number of WQBBs in queue.
 * @param[in] attr
 *   Pointer to RQ attributes structure.
 * @param[in] socket
 *   Socket to use for allocation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_rmp_create(void *ctx, struct mlx5_devx_rmp *rmp_obj,
		     uint32_t wqe_size, uint16_t log_wqbb_n,
		     struct mlx5_devx_wq_attr *wq_attr, int socket)
{
	struct mlx5_devx_create_rmp_attr rmp_attr = { 0 };
	int ret;

	if (rmp_obj->rmp != NULL)
		return 0;
	rmp_attr.wq_attr = *wq_attr;
	ret = mlx5_devx_wq_init(ctx, wqe_size, log_wqbb_n, socket,
				&rmp_attr.wq_attr, &rmp_obj->wq);
	if (ret != 0)
		return ret;
	rmp_attr.state = MLX5_RMPC_STATE_RDY;
	rmp_attr.basic_cyclic_rcv_wqe =
		wq_attr->wq_type != MLX5_WQ_TYPE_CYCLIC_STRIDING_RQ;
	/* Create receive memory pool object with DevX. */
	rmp_obj->rmp = mlx5_devx_cmd_create_rmp(ctx, &rmp_attr, socket);
	if (rmp_obj->rmp == NULL) {
		DRV_LOG(ERR, "Can't create DevX RMP object.");
		rte_errno = ENOMEM;
		goto error;
	}
	return 0;
error:
	ret = rte_errno;
	mlx5_devx_wq_res_destroy(&rmp_obj->wq);
	rte_errno = ret;
	return -rte_errno;
}

/**
 * Create Shared Receive Queue based on RMP using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] rq_obj
 *   Pointer to RQ to create.
 * @param[in] wqe_size
 *   Size of WQE structure.
 * @param[in] log_wqbb_n
 *   Log of number of WQBBs in queue.
 * @param[in] attr
 *   Pointer to RQ attributes structure.
 * @param[in] socket
 *   Socket to use for allocation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_rq_shared_create(void *ctx, struct mlx5_devx_rq *rq_obj,
			   uint32_t wqe_size, uint16_t log_wqbb_n,
			   struct mlx5_devx_create_rq_attr *attr, int socket)
{
	struct mlx5_devx_obj *rq;
	int ret;

	ret = mlx5_devx_rmp_create(ctx, rq_obj->rmp, wqe_size, log_wqbb_n,
				   &attr->wq_attr, socket);
	if (ret != 0)
		return ret;
	attr->mem_rq_type = MLX5_RQC_MEM_RQ_TYPE_MEMORY_RQ_RMP;
	attr->rmpn = rq_obj->rmp->rmp->id;
	attr->flush_in_error_en = 0;
	memset(&attr->wq_attr, 0, sizeof(attr->wq_attr));
	/* Create receive queue object with DevX. */
	rq = mlx5_devx_cmd_create_rq(ctx, attr, socket);
	if (!rq) {
		DRV_LOG(ERR, "Can't create DevX RMP RQ object.");
		rte_errno = ENOMEM;
		goto error;
	}
	rq_obj->rq = rq;
	rq_obj->rmp->ref_cnt++;
	return 0;
error:
	ret = rte_errno;
	mlx5_devx_rq_destroy(rq_obj);
	rte_errno = ret;
	return -rte_errno;
}

/**
 * Create Receive Queue using DevX API. Shared RQ is created only if rmp set.
 *
 * Get a pointer to partially initialized attributes structure, and updates the
 * following fields:
 *   wq_umem_valid
 *   wq_umem_id
 *   wq_umem_offset
 *   dbr_umem_valid
 *   dbr_umem_id
 *   dbr_addr
 *   log_wq_pg_sz
 * All other fields are updated by caller.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in/out] rq_obj
 *   Pointer to RQ to create.
 * @param[in] wqe_size
 *   Size of WQE structure.
 * @param[in] log_wqbb_n
 *   Log of number of WQBBs in queue.
 * @param[in] attr
 *   Pointer to RQ attributes structure.
 * @param[in] socket
 *   Socket to use for allocation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_rq_create(void *ctx, struct mlx5_devx_rq *rq_obj,
		    uint32_t wqe_size, uint16_t log_wqbb_n,
		    struct mlx5_devx_create_rq_attr *attr, int socket)
{
	if (rq_obj->rmp == NULL)
		return mlx5_devx_rq_std_create(ctx, rq_obj, wqe_size,
					       log_wqbb_n, attr, socket);
	return mlx5_devx_rq_shared_create(ctx, rq_obj, wqe_size,
					  log_wqbb_n, attr, socket);
}

/**
 * Change QP state to RTS.
 *
 * @param[in] qp
 *   DevX QP to change.
 * @param[in] remote_qp_id
 *   The remote QP ID for MLX5_CMD_OP_INIT2RTR_QP operation.
 *
 * @return
 *	 0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_qp2rts(struct mlx5_devx_qp *qp, uint32_t remote_qp_id)
{
	if (mlx5_devx_cmd_modify_qp_state(qp->qp, MLX5_CMD_OP_RST2INIT_QP,
					  remote_qp_id)) {
		DRV_LOG(ERR, "Failed to modify QP to INIT state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(qp->qp, MLX5_CMD_OP_INIT2RTR_QP,
					  remote_qp_id)) {
		DRV_LOG(ERR, "Failed to modify QP to RTR state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(qp->qp, MLX5_CMD_OP_RTR2RTS_QP,
					  remote_qp_id)) {
		DRV_LOG(ERR, "Failed to modify QP to RTS state(%u).",
			rte_errno);
		return -1;
	}
	return 0;
}
