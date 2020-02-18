/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_io.h>

#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


void
mlx5_vdpa_event_qp_global_release(struct mlx5_vdpa_priv *priv)
{
	if (priv->uar) {
		mlx5_glue->devx_free_uar(priv->uar);
		priv->uar = NULL;
	}
	if (priv->eventc) {
		mlx5_glue->devx_destroy_event_channel(priv->eventc);
		priv->eventc = NULL;
	}
	priv->eqn = 0;
}

/* Prepare all the global resources for all the event objects.*/
static int
mlx5_vdpa_event_qp_global_prepare(struct mlx5_vdpa_priv *priv)
{
	uint32_t lcore;

	if (priv->eventc)
		return 0;
	lcore = (uint32_t)rte_lcore_to_cpu_id(-1);
	if (mlx5_glue->devx_query_eqn(priv->ctx, lcore, &priv->eqn)) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to query EQ number %d.", rte_errno);
		return -1;
	}
	priv->eventc = mlx5_glue->devx_create_event_channel(priv->ctx,
			   MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA);
	if (!priv->eventc) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create event channel %d.",
			rte_errno);
		goto error;
	}
	priv->uar = mlx5_glue->devx_alloc_uar(priv->ctx, 0);
	if (!priv->uar) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to allocate UAR.");
		goto error;
	}
	return 0;
error:
	mlx5_vdpa_event_qp_global_release(priv);
	return -1;
}

static void
mlx5_vdpa_cq_destroy(struct mlx5_vdpa_cq *cq)
{
	if (cq->cq)
		claim_zero(mlx5_devx_cmd_destroy(cq->cq));
	if (cq->umem_obj)
		claim_zero(mlx5_glue->devx_umem_dereg(cq->umem_obj));
	if (cq->umem_buf)
		rte_free((void *)(uintptr_t)cq->umem_buf);
	memset(cq, 0, sizeof(*cq));
}

static inline void
mlx5_vdpa_cq_arm(struct mlx5_vdpa_priv *priv, struct mlx5_vdpa_cq *cq)
{
	uint32_t arm_sn = cq->arm_sn << MLX5_CQ_SQN_OFFSET;
	uint32_t cq_ci = cq->cq_ci & MLX5_CI_MASK;
	uint32_t doorbell_hi = arm_sn | MLX5_CQ_DBR_CMD_ALL | cq_ci;
	uint64_t doorbell = ((uint64_t)doorbell_hi << 32) | cq->cq->id;
	uint64_t db_be = rte_cpu_to_be_64(doorbell);
	uint32_t *addr = RTE_PTR_ADD(priv->uar->base_addr, MLX5_CQ_DOORBELL);

	rte_io_wmb();
	cq->db_rec[MLX5_CQ_ARM_DB] = rte_cpu_to_be_32(doorbell_hi);
	rte_wmb();
#ifdef RTE_ARCH_64
	*(uint64_t *)addr = db_be;
#else
	*(uint32_t *)addr = db_be;
	rte_io_wmb();
	*((uint32_t *)addr + 1) = db_be >> 32;
#endif
	cq->arm_sn++;
}

static int
mlx5_vdpa_cq_create(struct mlx5_vdpa_priv *priv, uint16_t log_desc_n,
		    int callfd, struct mlx5_vdpa_cq *cq)
{
	struct mlx5_devx_cq_attr attr;
	size_t pgsize = sysconf(_SC_PAGESIZE);
	uint32_t umem_size;
	int ret;
	uint16_t event_nums[1] = {0};

	cq->log_desc_n = log_desc_n;
	umem_size = sizeof(struct mlx5_cqe) * (1 << log_desc_n) +
							sizeof(*cq->db_rec) * 2;
	cq->umem_buf = rte_zmalloc(__func__, umem_size, 4096);
	if (!cq->umem_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for CQ.");
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	cq->umem_obj = mlx5_glue->devx_umem_reg(priv->ctx,
						(void *)(uintptr_t)cq->umem_buf,
						umem_size,
						IBV_ACCESS_LOCAL_WRITE);
	if (!cq->umem_obj) {
		DRV_LOG(ERR, "Failed to register umem for CQ.");
		goto error;
	}
	attr.q_umem_valid = 1;
	attr.db_umem_valid = 1;
	attr.use_first_only = 0;
	attr.overrun_ignore = 0;
	attr.uar_page_id = priv->uar->page_id;
	attr.q_umem_id = cq->umem_obj->umem_id;
	attr.q_umem_offset = 0;
	attr.db_umem_id = cq->umem_obj->umem_id;
	attr.db_umem_offset = sizeof(struct mlx5_cqe) * (1 << log_desc_n);
	attr.eqn = priv->eqn;
	attr.log_cq_size = log_desc_n;
	attr.log_page_size = rte_log2_u32(pgsize);
	cq->cq = mlx5_devx_cmd_create_cq(priv->ctx, &attr);
	if (!cq->cq)
		goto error;
	cq->db_rec = RTE_PTR_ADD(cq->umem_buf, (uintptr_t)attr.db_umem_offset);
	cq->cq_ci = 0;
	rte_spinlock_init(&cq->sl);
	/* Subscribe CQ event to the event channel controlled by the driver. */
	ret = mlx5_glue->devx_subscribe_devx_event(priv->eventc, cq->cq->obj,
						   sizeof(event_nums),
						   event_nums,
						   (uint64_t)(uintptr_t)cq);
	if (ret) {
		DRV_LOG(ERR, "Failed to subscribe CQE event.");
		rte_errno = errno;
		goto error;
	}
	/* Subscribe CQ event to the guest FD only if it is not in poll mode. */
	if (callfd != -1) {
		ret = mlx5_glue->devx_subscribe_devx_event_fd(priv->eventc,
							      callfd,
							      cq->cq->obj, 0);
		if (ret) {
			DRV_LOG(ERR, "Failed to subscribe CQE event fd.");
			rte_errno = errno;
			goto error;
		}
	}
	/* First arming. */
	mlx5_vdpa_cq_arm(priv, cq);
	return 0;
error:
	mlx5_vdpa_cq_destroy(cq);
	return -1;
}

static inline void __rte_unused
mlx5_vdpa_cq_poll(struct mlx5_vdpa_priv *priv __rte_unused,
		  struct mlx5_vdpa_cq *cq)
{
	struct mlx5_vdpa_event_qp *eqp =
				container_of(cq, struct mlx5_vdpa_event_qp, cq);
	const unsigned int cq_size = 1 << cq->log_desc_n;
	const unsigned int cq_mask = cq_size - 1;
	int ret;

	do {
		volatile struct mlx5_cqe *cqe = cq->cqes + (cq->cq_ci &
							    cq_mask);

		ret = check_cqe(cqe, cq_size, cq->cq_ci);
		switch (ret) {
		case MLX5_CQE_STATUS_ERR:
			cq->errors++;
			/*fall-through*/
		case MLX5_CQE_STATUS_SW_OWN:
			cq->cq_ci++;
			break;
		case MLX5_CQE_STATUS_HW_OWN:
		default:
			break;
		}
	} while (ret != MLX5_CQE_STATUS_HW_OWN);
	rte_io_wmb();
	/* Ring CQ doorbell record. */
	cq->db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
	rte_io_wmb();
	/* Ring SW QP doorbell record. */
	eqp->db_rec[0] = rte_cpu_to_be_32(cq->cq_ci + cq_size);
}

static void
mlx5_vdpa_interrupt_handler(void *cb_arg)
{
#ifndef HAVE_IBV_DEVX_EVENT
	(void)cb_arg;
	return;
#else
	struct mlx5_vdpa_priv *priv = cb_arg;
	union {
		struct mlx5dv_devx_async_event_hdr event_resp;
		uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) + 128];
	} out;

	while (mlx5_glue->devx_get_event(priv->eventc, &out.event_resp,
					 sizeof(out.buf)) >=
				       (ssize_t)sizeof(out.event_resp.cookie)) {
		struct mlx5_vdpa_cq *cq = (struct mlx5_vdpa_cq *)
					       (uintptr_t)out.event_resp.cookie;
		rte_spinlock_lock(&cq->sl);
		mlx5_vdpa_cq_poll(priv, cq);
		mlx5_vdpa_cq_arm(priv, cq);
		rte_spinlock_unlock(&cq->sl);
		DRV_LOG(DEBUG, "CQ %d event: new cq_ci = %u.", cq->cq->id,
			cq->cq_ci);
	}
#endif /* HAVE_IBV_DEVX_ASYNC */
}

int
mlx5_vdpa_cqe_event_setup(struct mlx5_vdpa_priv *priv)
{
	int flags = fcntl(priv->eventc->fd, F_GETFL);
	int ret = fcntl(priv->eventc->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		DRV_LOG(ERR, "Failed to change event channel FD.");
		rte_errno = errno;
		return -rte_errno;
	}
	priv->intr_handle.fd = priv->eventc->fd;
	priv->intr_handle.type = RTE_INTR_HANDLE_EXT;
	if (rte_intr_callback_register(&priv->intr_handle,
				       mlx5_vdpa_interrupt_handler, priv)) {
		priv->intr_handle.fd = 0;
		DRV_LOG(ERR, "Failed to register CQE interrupt %d.", rte_errno);
		return -rte_errno;
	}
	return 0;
}

void
mlx5_vdpa_cqe_event_unset(struct mlx5_vdpa_priv *priv)
{
	int retries = MLX5_VDPA_INTR_RETRIES;
	int ret = -EAGAIN;

	if (priv->intr_handle.fd) {
		while (retries-- && ret == -EAGAIN) {
			ret = rte_intr_callback_unregister(&priv->intr_handle,
						    mlx5_vdpa_interrupt_handler,
						    priv);
			if (ret == -EAGAIN) {
				DRV_LOG(DEBUG, "Try again to unregister fd %d "
					"of CQ interrupt, retries = %d.",
					priv->intr_handle.fd, retries);
				usleep(MLX5_VDPA_INTR_RETRIES_USEC);
			}
		}
		memset(&priv->intr_handle, 0, sizeof(priv->intr_handle));
	}
}

void
mlx5_vdpa_event_qp_destroy(struct mlx5_vdpa_event_qp *eqp)
{
	if (eqp->sw_qp)
		claim_zero(mlx5_devx_cmd_destroy(eqp->sw_qp));
	if (eqp->umem_obj)
		claim_zero(mlx5_glue->devx_umem_dereg(eqp->umem_obj));
	if (eqp->umem_buf)
		rte_free(eqp->umem_buf);
	if (eqp->fw_qp)
		claim_zero(mlx5_devx_cmd_destroy(eqp->fw_qp));
	mlx5_vdpa_cq_destroy(&eqp->cq);
	memset(eqp, 0, sizeof(*eqp));
}

static int
mlx5_vdpa_qps2rts(struct mlx5_vdpa_event_qp *eqp)
{
	if (mlx5_devx_cmd_modify_qp_state(eqp->fw_qp, MLX5_CMD_OP_RST2INIT_QP,
					  eqp->sw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify FW QP to INIT state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->sw_qp, MLX5_CMD_OP_RST2INIT_QP,
					  eqp->fw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify SW QP to INIT state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->fw_qp, MLX5_CMD_OP_INIT2RTR_QP,
					  eqp->sw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify FW QP to RTR state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->sw_qp, MLX5_CMD_OP_INIT2RTR_QP,
					  eqp->fw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify SW QP to RTR state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->fw_qp, MLX5_CMD_OP_RTR2RTS_QP,
					  eqp->sw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify FW QP to RTS state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->sw_qp, MLX5_CMD_OP_RTR2RTS_QP,
					  eqp->fw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify SW QP to RTS state(%u).",
			rte_errno);
		return -1;
	}
	return 0;
}

int
mlx5_vdpa_event_qp_create(struct mlx5_vdpa_priv *priv, uint16_t desc_n,
			  int callfd, struct mlx5_vdpa_event_qp *eqp)
{
	struct mlx5_devx_qp_attr attr = {0};
	uint16_t log_desc_n = rte_log2_u32(desc_n);
	uint32_t umem_size = (1 << log_desc_n) * MLX5_WSEG_SIZE +
						       sizeof(*eqp->db_rec) * 2;

	if (mlx5_vdpa_event_qp_global_prepare(priv))
		return -1;
	if (mlx5_vdpa_cq_create(priv, log_desc_n, callfd, &eqp->cq))
		return -1;
	attr.pd = priv->pdn;
	eqp->fw_qp = mlx5_devx_cmd_create_qp(priv->ctx, &attr);
	if (!eqp->fw_qp) {
		DRV_LOG(ERR, "Failed to create FW QP(%u).", rte_errno);
		goto error;
	}
	eqp->umem_buf = rte_zmalloc(__func__, umem_size, 4096);
	if (!eqp->umem_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for SW QP.");
		rte_errno = ENOMEM;
		goto error;
	}
	eqp->umem_obj = mlx5_glue->devx_umem_reg(priv->ctx,
					       (void *)(uintptr_t)eqp->umem_buf,
					       umem_size,
					       IBV_ACCESS_LOCAL_WRITE);
	if (!eqp->umem_obj) {
		DRV_LOG(ERR, "Failed to register umem for SW QP.");
		goto error;
	}
	attr.uar_index = priv->uar->page_id;
	attr.cqn = eqp->cq.cq->id;
	attr.log_page_size = rte_log2_u32(sysconf(_SC_PAGESIZE));
	attr.rq_size = 1 << log_desc_n;
	attr.log_rq_stride = rte_log2_u32(MLX5_WSEG_SIZE);
	attr.sq_size = 0; /* No need SQ. */
	attr.dbr_umem_valid = 1;
	attr.wq_umem_id = eqp->umem_obj->umem_id;
	attr.wq_umem_offset = 0;
	attr.dbr_umem_id = eqp->umem_obj->umem_id;
	attr.dbr_address = (1 << log_desc_n) * MLX5_WSEG_SIZE;
	eqp->sw_qp = mlx5_devx_cmd_create_qp(priv->ctx, &attr);
	if (!eqp->sw_qp) {
		DRV_LOG(ERR, "Failed to create SW QP(%u).", rte_errno);
		goto error;
	}
	eqp->db_rec = RTE_PTR_ADD(eqp->umem_buf, (uintptr_t)attr.dbr_address);
	if (mlx5_vdpa_qps2rts(eqp))
		goto error;
	/* First ringing. */
	rte_write32(rte_cpu_to_be_32(1 << log_desc_n), &eqp->db_rec[0]);
	return 0;
error:
	mlx5_vdpa_event_qp_destroy(eqp);
	return -1;
}
