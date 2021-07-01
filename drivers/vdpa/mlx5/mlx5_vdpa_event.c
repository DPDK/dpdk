/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/eventfd.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_io.h>
#include <rte_alarm.h>

#include <mlx5_common.h>
#include <mlx5_glue.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


#define MLX5_VDPA_ERROR_TIME_SEC 3u

void
mlx5_vdpa_event_qp_global_release(struct mlx5_vdpa_priv *priv)
{
	if (priv->uar) {
		mlx5_glue->devx_free_uar(priv->uar);
		priv->uar = NULL;
	}
#ifdef HAVE_IBV_DEVX_EVENT
	if (priv->eventc) {
		union {
			struct mlx5dv_devx_async_event_hdr event_resp;
			uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr)
									 + 128];
		} out;

		/* Clean all pending events. */
		while (mlx5_glue->devx_get_event(priv->eventc, &out.event_resp,
		       sizeof(out.buf)) >=
		       (ssize_t)sizeof(out.event_resp.cookie))
			;
		mlx5_glue->devx_destroy_event_channel(priv->eventc);
		priv->eventc = NULL;
	}
#endif
	priv->eqn = 0;
}

/* Prepare all the global resources for all the event objects.*/
static int
mlx5_vdpa_event_qp_global_prepare(struct mlx5_vdpa_priv *priv)
{
	int flags, ret;

	if (priv->eventc)
		return 0;
	if (mlx5_glue->devx_query_eqn(priv->ctx, 0, &priv->eqn)) {
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
	flags = fcntl(priv->eventc->fd, F_GETFL);
	ret = fcntl(priv->eventc->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		DRV_LOG(ERR, "Failed to change event channel FD.");
		goto error;
	}
	/*
	 * This PMD always claims the write memory barrier on UAR
	 * registers writings, it is safe to allocate UAR with any
	 * memory mapping type.
	 */
	priv->uar = mlx5_devx_alloc_uar(priv->ctx, -1);
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

static inline void __rte_unused
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
	cq->armed = 1;
}

static int
mlx5_vdpa_cq_create(struct mlx5_vdpa_priv *priv, uint16_t log_desc_n,
		    int callfd, struct mlx5_vdpa_cq *cq)
{
	struct mlx5_devx_cq_attr attr = {0};
	size_t pgsize = sysconf(_SC_PAGESIZE);
	uint32_t umem_size;
	uint16_t event_nums[1] = {0};
	uint16_t cq_size = 1 << log_desc_n;
	int ret;

	cq->log_desc_n = log_desc_n;
	umem_size = sizeof(struct mlx5_cqe) * cq_size + sizeof(*cq->db_rec) * 2;
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
	attr.use_first_only = 1;
	attr.overrun_ignore = 0;
	attr.uar_page_id = priv->uar->page_id;
	attr.q_umem_id = cq->umem_obj->umem_id;
	attr.q_umem_offset = 0;
	attr.db_umem_id = cq->umem_obj->umem_id;
	attr.db_umem_offset = sizeof(struct mlx5_cqe) * cq_size;
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
	cq->callfd = callfd;
	/* Init CQ to ones to be in HW owner in the start. */
	cq->cqes[0].op_own = MLX5_CQE_OWNER_MASK;
	cq->cqes[0].wqe_counter = rte_cpu_to_be_16(UINT16_MAX);
	/* First arming. */
	mlx5_vdpa_cq_arm(priv, cq);
	return 0;
error:
	mlx5_vdpa_cq_destroy(cq);
	return -1;
}

static inline uint32_t
mlx5_vdpa_cq_poll(struct mlx5_vdpa_cq *cq)
{
	struct mlx5_vdpa_event_qp *eqp =
				container_of(cq, struct mlx5_vdpa_event_qp, cq);
	const unsigned int cq_size = 1 << cq->log_desc_n;
	union {
		struct {
			uint16_t wqe_counter;
			uint8_t rsvd5;
			uint8_t op_own;
		};
		uint32_t word;
	} last_word;
	uint16_t next_wqe_counter = cq->cq_ci;
	uint16_t cur_wqe_counter;
	uint16_t comp;

	last_word.word = rte_read32(&cq->cqes[0].wqe_counter);
	cur_wqe_counter = rte_be_to_cpu_16(last_word.wqe_counter);
	comp = cur_wqe_counter + (uint16_t)1 - next_wqe_counter;
	if (comp) {
		cq->cq_ci += comp;
		MLX5_ASSERT(MLX5_CQE_OPCODE(last_word.op_own) !=
			    MLX5_CQE_INVALID);
		if (unlikely(!(MLX5_CQE_OPCODE(last_word.op_own) ==
			       MLX5_CQE_RESP_ERR ||
			       MLX5_CQE_OPCODE(last_word.op_own) ==
			       MLX5_CQE_REQ_ERR)))
			cq->errors++;
		rte_io_wmb();
		/* Ring CQ doorbell record. */
		cq->db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
		rte_io_wmb();
		/* Ring SW QP doorbell record. */
		eqp->db_rec[0] = rte_cpu_to_be_32(cq->cq_ci + cq_size);
	}
	return comp;
}

static void
mlx5_vdpa_arm_all_cqs(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_vdpa_cq *cq;
	int i;

	for (i = 0; i < priv->nr_virtqs; i++) {
		cq = &priv->virtqs[i].eqp.cq;
		if (cq->cq && !cq->armed)
			mlx5_vdpa_cq_arm(priv, cq);
	}
}

static void
mlx5_vdpa_timer_sleep(struct mlx5_vdpa_priv *priv, uint32_t max)
{
	if (priv->event_mode == MLX5_VDPA_EVENT_MODE_DYNAMIC_TIMER) {
		switch (max) {
		case 0:
			priv->timer_delay_us += priv->event_us;
			break;
		case 1:
			break;
		default:
			priv->timer_delay_us /= max;
			break;
		}
	}
	usleep(priv->timer_delay_us);
}

static void *
mlx5_vdpa_poll_handle(void *arg)
{
	struct mlx5_vdpa_priv *priv = arg;
	int i;
	struct mlx5_vdpa_cq *cq;
	uint32_t max;
	uint64_t current_tic;

	pthread_mutex_lock(&priv->timer_lock);
	while (!priv->timer_on)
		pthread_cond_wait(&priv->timer_cond, &priv->timer_lock);
	pthread_mutex_unlock(&priv->timer_lock);
	priv->timer_delay_us = priv->event_mode ==
					    MLX5_VDPA_EVENT_MODE_DYNAMIC_TIMER ?
					      MLX5_VDPA_DEFAULT_TIMER_DELAY_US :
								 priv->event_us;
	while (1) {
		max = 0;
		pthread_mutex_lock(&priv->vq_config_lock);
		for (i = 0; i < priv->nr_virtqs; i++) {
			cq = &priv->virtqs[i].eqp.cq;
			if (cq->cq && !cq->armed) {
				uint32_t comp = mlx5_vdpa_cq_poll(cq);

				if (comp) {
					/* Notify guest for descs consuming. */
					if (cq->callfd != -1)
						eventfd_write(cq->callfd,
							      (eventfd_t)1);
					if (comp > max)
						max = comp;
				}
			}
		}
		current_tic = rte_rdtsc();
		if (!max) {
			/* No traffic ? stop timer and load interrupts. */
			if (current_tic - priv->last_traffic_tic >=
			    rte_get_timer_hz() * priv->no_traffic_time_s) {
				DRV_LOG(DEBUG, "Device %s traffic was stopped.",
					priv->vdev->device->name);
				mlx5_vdpa_arm_all_cqs(priv);
				pthread_mutex_unlock(&priv->vq_config_lock);
				pthread_mutex_lock(&priv->timer_lock);
				priv->timer_on = 0;
				while (!priv->timer_on)
					pthread_cond_wait(&priv->timer_cond,
							  &priv->timer_lock);
				pthread_mutex_unlock(&priv->timer_lock);
				priv->timer_delay_us = priv->event_mode ==
					    MLX5_VDPA_EVENT_MODE_DYNAMIC_TIMER ?
					      MLX5_VDPA_DEFAULT_TIMER_DELAY_US :
								 priv->event_us;
				continue;
			}
		} else {
			priv->last_traffic_tic = current_tic;
		}
		pthread_mutex_unlock(&priv->vq_config_lock);
		mlx5_vdpa_timer_sleep(priv, max);
	}
	return NULL;
}

static void
mlx5_vdpa_interrupt_handler(void *cb_arg)
{
	struct mlx5_vdpa_priv *priv = cb_arg;
#ifdef HAVE_IBV_DEVX_EVENT
	union {
		struct mlx5dv_devx_async_event_hdr event_resp;
		uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) + 128];
	} out;

	pthread_mutex_lock(&priv->vq_config_lock);
	while (mlx5_glue->devx_get_event(priv->eventc, &out.event_resp,
					 sizeof(out.buf)) >=
				       (ssize_t)sizeof(out.event_resp.cookie)) {
		struct mlx5_vdpa_cq *cq = (struct mlx5_vdpa_cq *)
					       (uintptr_t)out.event_resp.cookie;
		struct mlx5_vdpa_event_qp *eqp = container_of(cq,
						 struct mlx5_vdpa_event_qp, cq);
		struct mlx5_vdpa_virtq *virtq = container_of(eqp,
						   struct mlx5_vdpa_virtq, eqp);

		if (!virtq->enable)
			continue;
		mlx5_vdpa_cq_poll(cq);
		/* Notify guest for descs consuming. */
		if (cq->callfd != -1)
			eventfd_write(cq->callfd, (eventfd_t)1);
		if (priv->event_mode == MLX5_VDPA_EVENT_MODE_ONLY_INTERRUPT) {
			mlx5_vdpa_cq_arm(priv, cq);
			pthread_mutex_unlock(&priv->vq_config_lock);
			return;
		}
		/* Don't arm again - timer will take control. */
		DRV_LOG(DEBUG, "Device %s virtq %d cq %d event was captured."
			" Timer is %s, cq ci is %u.\n",
			priv->vdev->device->name,
			(int)virtq->index, cq->cq->id,
			priv->timer_on ? "on" : "off", cq->cq_ci);
		cq->armed = 0;
	}
#endif

	/* Traffic detected: make sure timer is on. */
	priv->last_traffic_tic = rte_rdtsc();
	pthread_mutex_lock(&priv->timer_lock);
	if (!priv->timer_on) {
		priv->timer_on = 1;
		pthread_cond_signal(&priv->timer_cond);
	}
	pthread_mutex_unlock(&priv->timer_lock);
	pthread_mutex_unlock(&priv->vq_config_lock);
}

static void
mlx5_vdpa_err_interrupt_handler(void *cb_arg __rte_unused)
{
#ifdef HAVE_IBV_DEVX_EVENT
	struct mlx5_vdpa_priv *priv = cb_arg;
	union {
		struct mlx5dv_devx_async_event_hdr event_resp;
		uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) + 128];
	} out;
	uint32_t vq_index, i, version;
	struct mlx5_vdpa_virtq *virtq;
	uint64_t sec;

	pthread_mutex_lock(&priv->vq_config_lock);
	while (mlx5_glue->devx_get_event(priv->err_chnl, &out.event_resp,
					 sizeof(out.buf)) >=
				       (ssize_t)sizeof(out.event_resp.cookie)) {
		vq_index = out.event_resp.cookie & UINT32_MAX;
		version = out.event_resp.cookie >> 32;
		if (vq_index >= priv->nr_virtqs) {
			DRV_LOG(ERR, "Invalid device %s error event virtq %d.",
				priv->vdev->device->name, vq_index);
			continue;
		}
		virtq = &priv->virtqs[vq_index];
		if (!virtq->enable || virtq->version != version)
			continue;
		if (rte_rdtsc() / rte_get_tsc_hz() < MLX5_VDPA_ERROR_TIME_SEC)
			continue;
		virtq->stopped = true;
		/* Query error info. */
		if (mlx5_vdpa_virtq_query(priv, vq_index))
			goto log;
		/* Disable vq. */
		if (mlx5_vdpa_virtq_enable(priv, vq_index, 0)) {
			DRV_LOG(ERR, "Failed to disable virtq %d.", vq_index);
			goto log;
		}
		/* Retry if error happens less than N times in 3 seconds. */
		sec = (rte_rdtsc() - virtq->err_time[0]) / rte_get_tsc_hz();
		if (sec > MLX5_VDPA_ERROR_TIME_SEC) {
			/* Retry. */
			if (mlx5_vdpa_virtq_enable(priv, vq_index, 1))
				DRV_LOG(ERR, "Failed to enable virtq %d.",
					vq_index);
			else
				DRV_LOG(WARNING, "Recover virtq %d: %u.",
					vq_index, ++virtq->n_retry);
		} else {
			/* Retry timeout, give up. */
			DRV_LOG(ERR, "Device %s virtq %d failed to recover.",
				priv->vdev->device->name, vq_index);
		}
log:
		/* Shift in current time to error time log end. */
		for (i = 1; i < RTE_DIM(virtq->err_time); i++)
			virtq->err_time[i - 1] = virtq->err_time[i];
		virtq->err_time[RTE_DIM(virtq->err_time) - 1] = rte_rdtsc();
	}
	pthread_mutex_unlock(&priv->vq_config_lock);
#endif
}

int
mlx5_vdpa_err_event_setup(struct mlx5_vdpa_priv *priv)
{
	int ret;
	int flags;

	/* Setup device event channel. */
	priv->err_chnl = mlx5_glue->devx_create_event_channel(priv->ctx, 0);
	if (!priv->err_chnl) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create device event channel %d.",
			rte_errno);
		goto error;
	}
	flags = fcntl(priv->err_chnl->fd, F_GETFL);
	ret = fcntl(priv->err_chnl->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		DRV_LOG(ERR, "Failed to change device event channel FD.");
		goto error;
	}
	priv->err_intr_handle.fd = priv->err_chnl->fd;
	priv->err_intr_handle.type = RTE_INTR_HANDLE_EXT;
	if (rte_intr_callback_register(&priv->err_intr_handle,
				       mlx5_vdpa_err_interrupt_handler,
				       priv)) {
		priv->err_intr_handle.fd = 0;
		DRV_LOG(ERR, "Failed to register error interrupt for device %d.",
			priv->vid);
		goto error;
	} else {
		DRV_LOG(DEBUG, "Registered error interrupt for device%d.",
			priv->vid);
	}
	return 0;
error:
	mlx5_vdpa_err_event_unset(priv);
	return -1;
}

void
mlx5_vdpa_err_event_unset(struct mlx5_vdpa_priv *priv)
{
	int retries = MLX5_VDPA_INTR_RETRIES;
	int ret = -EAGAIN;

	if (!priv->err_intr_handle.fd)
		return;
	while (retries-- && ret == -EAGAIN) {
		ret = rte_intr_callback_unregister(&priv->err_intr_handle,
					    mlx5_vdpa_err_interrupt_handler,
					    priv);
		if (ret == -EAGAIN) {
			DRV_LOG(DEBUG, "Try again to unregister fd %d "
				"of error interrupt, retries = %d.",
				priv->err_intr_handle.fd, retries);
			rte_pause();
		}
	}
	memset(&priv->err_intr_handle, 0, sizeof(priv->err_intr_handle));
	if (priv->err_chnl) {
#ifdef HAVE_IBV_DEVX_EVENT
		union {
			struct mlx5dv_devx_async_event_hdr event_resp;
			uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) +
				    128];
		} out;

		/* Clean all pending events. */
		while (mlx5_glue->devx_get_event(priv->err_chnl,
		       &out.event_resp, sizeof(out.buf)) >=
		       (ssize_t)sizeof(out.event_resp.cookie))
			;
#endif
		mlx5_glue->devx_destroy_event_channel(priv->err_chnl);
		priv->err_chnl = NULL;
	}
}

int
mlx5_vdpa_cqe_event_setup(struct mlx5_vdpa_priv *priv)
{
	int ret;

	if (!priv->eventc)
		/* All virtqs are in poll mode. */
		return 0;
	if (priv->event_mode != MLX5_VDPA_EVENT_MODE_ONLY_INTERRUPT) {
		pthread_mutex_init(&priv->timer_lock, NULL);
		pthread_cond_init(&priv->timer_cond, NULL);
		priv->timer_on = 0;
		ret = pthread_create(&priv->timer_tid, NULL,
				     mlx5_vdpa_poll_handle, (void *)priv);
		if (ret) {
			DRV_LOG(ERR, "Failed to create timer thread.");
			return -1;
		}
	}
	priv->intr_handle.fd = priv->eventc->fd;
	priv->intr_handle.type = RTE_INTR_HANDLE_EXT;
	if (rte_intr_callback_register(&priv->intr_handle,
				       mlx5_vdpa_interrupt_handler, priv)) {
		priv->intr_handle.fd = 0;
		DRV_LOG(ERR, "Failed to register CQE interrupt %d.", rte_errno);
		goto error;
	}
	return 0;
error:
	mlx5_vdpa_cqe_event_unset(priv);
	return -1;
}

void
mlx5_vdpa_cqe_event_unset(struct mlx5_vdpa_priv *priv)
{
	int retries = MLX5_VDPA_INTR_RETRIES;
	int ret = -EAGAIN;
	void *status;

	if (priv->intr_handle.fd) {
		while (retries-- && ret == -EAGAIN) {
			ret = rte_intr_callback_unregister(&priv->intr_handle,
						    mlx5_vdpa_interrupt_handler,
						    priv);
			if (ret == -EAGAIN) {
				DRV_LOG(DEBUG, "Try again to unregister fd %d "
					"of CQ interrupt, retries = %d.",
					priv->intr_handle.fd, retries);
				rte_pause();
			}
		}
		memset(&priv->intr_handle, 0, sizeof(priv->intr_handle));
	}
	if (priv->timer_tid) {
		pthread_cancel(priv->timer_tid);
		pthread_join(priv->timer_tid, &status);
	}
	priv->timer_tid = 0;
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
	attr.ts_format = mlx5_ts_format_conv(priv->qp_ts_format);
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
	attr.ts_format = mlx5_ts_format_conv(priv->qp_ts_format);
	attr.dbr_address = RTE_BIT64(log_desc_n) * MLX5_WSEG_SIZE;
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
