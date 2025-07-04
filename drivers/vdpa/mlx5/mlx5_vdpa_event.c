/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/eventfd.h>

#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_io.h>
#include <rte_alarm.h>

#include <mlx5_common.h>
#include <mlx5_common_os.h>
#include <mlx5_common_devx.h>
#include <mlx5_glue.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


#define MLX5_VDPA_ERROR_TIME_SEC 3u

void
mlx5_vdpa_event_qp_global_release(struct mlx5_vdpa_priv *priv)
{
	mlx5_devx_uar_release(&priv->uar);
#ifdef HAVE_IBV_DEVX_EVENT
	if (priv->eventc) {
		mlx5_os_devx_destroy_event_channel(priv->eventc);
		priv->eventc = NULL;
	}
#endif
}

/* Prepare all the global resources for all the event objects.*/
int
mlx5_vdpa_event_qp_global_prepare(struct mlx5_vdpa_priv *priv)
{
	priv->eventc = mlx5_os_devx_create_event_channel(priv->cdev->ctx,
			   MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA);
	if (!priv->eventc) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create event channel %d.",
			rte_errno);
		goto error;
	}
	if (mlx5_devx_uar_prepare(priv->cdev, &priv->uar) != 0) {
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
	mlx5_devx_cq_destroy(&cq->cq_obj);
	memset(cq, 0, sizeof(*cq));
}

static inline void __rte_unused
mlx5_vdpa_cq_arm(struct mlx5_vdpa_priv *priv, struct mlx5_vdpa_cq *cq)
{
	uint32_t arm_sn = cq->arm_sn << MLX5_CQ_SQN_OFFSET;
	uint32_t cq_ci = cq->cq_ci & MLX5_CI_MASK;
	uint32_t doorbell_hi = arm_sn | MLX5_CQ_DBR_CMD_ALL | cq_ci;
	uint64_t doorbell = ((uint64_t)doorbell_hi << 32) | cq->cq_obj.cq->id;
	uint64_t db_be = rte_cpu_to_be_64(doorbell);

	mlx5_doorbell_ring(&priv->uar.cq_db, db_be, doorbell_hi,
			   &cq->cq_obj.db_rec[MLX5_CQ_ARM_DB], 0);
	cq->arm_sn++;
	cq->armed = 1;
}

static int
mlx5_vdpa_cq_create(struct mlx5_vdpa_priv *priv, uint16_t log_desc_n,
		int callfd, struct mlx5_vdpa_virtq *virtq)
{
	struct mlx5_devx_cq_attr attr = {
		.use_first_only = 1,
		.uar_page_id = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
	};
	struct mlx5_vdpa_cq *cq = &virtq->eqp.cq;
	uint16_t event_nums[1] = {0};
	int ret;

	ret = mlx5_devx_cq_create(priv->cdev->ctx, &cq->cq_obj, log_desc_n,
				  &attr, SOCKET_ID_ANY);
	if (ret)
		goto error;
	cq->cq_ci = 0;
	cq->log_desc_n = log_desc_n;
	rte_spinlock_init(&cq->sl);
	/* Subscribe CQ event to the event channel controlled by the driver. */
	ret = mlx5_glue->devx_subscribe_devx_event(priv->eventc,
							cq->cq_obj.cq->obj,
						   sizeof(event_nums),
						   event_nums,
						   (uint64_t)(uintptr_t)virtq);
	if (ret) {
		DRV_LOG(ERR, "Failed to subscribe CQE event.");
		rte_errno = errno;
		goto error;
	}
	cq->callfd = callfd;
	/* Init CQ to ones to be in HW owner in the start. */
	cq->cq_obj.cqes[0].op_own = MLX5_CQE_OWNER_MASK;
	cq->cq_obj.cqes[0].wqe_counter = rte_cpu_to_be_16(UINT16_MAX);
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
	uint16_t next_wqe_counter = eqp->qp_pi;
	uint16_t cur_wqe_counter;
	uint16_t comp;

	last_word.word = rte_read32(&cq->cq_obj.cqes[0].wqe_counter);
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
		cq->cq_obj.db_rec[0] = rte_cpu_to_be_32(cq->cq_ci);
		eqp->qp_pi += comp;
		rte_io_wmb();
		/* Ring SW QP doorbell record. */
		eqp->sw_qp.db_rec[0] = rte_cpu_to_be_32(eqp->qp_pi + cq_size);
	}
	return comp;
}

static void
mlx5_vdpa_arm_all_cqs(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_vdpa_virtq *virtq;
	struct mlx5_vdpa_cq *cq;
	int i;

	for (i = 0; i < priv->nr_virtqs; i++) {
		virtq = &priv->virtqs[i];
		pthread_mutex_lock(&virtq->virtq_lock);
		cq = &priv->virtqs[i].eqp.cq;
		if (cq->cq_obj.cq && !cq->armed)
			mlx5_vdpa_cq_arm(priv, cq);
		pthread_mutex_unlock(&virtq->virtq_lock);
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
	if (priv->timer_delay_us)
		usleep(priv->timer_delay_us);
	else
		/* Give-up CPU to improve polling threads scheduling. */
		sched_yield();
}

/* Notify virtio device for specific virtq new traffic. */
static uint32_t
mlx5_vdpa_queue_complete(struct mlx5_vdpa_cq *cq)
{
	uint32_t comp = 0;

	if (cq->cq_obj.cq) {
		comp = mlx5_vdpa_cq_poll(cq);
		if (comp) {
			if (cq->callfd != -1)
				eventfd_write(cq->callfd, (eventfd_t)1);
			cq->armed = 0;
		}
	}
	return comp;
}

/* Notify virtio device for any virtq new traffic. */
static uint32_t
mlx5_vdpa_queues_complete(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_vdpa_virtq *virtq;
	struct mlx5_vdpa_cq *cq;
	uint32_t max = 0;
	uint32_t comp;
	int i;

	for (i = 0; i < priv->nr_virtqs; i++) {
		virtq = &priv->virtqs[i];
		pthread_mutex_lock(&virtq->virtq_lock);
		cq = &virtq->eqp.cq;
		comp = mlx5_vdpa_queue_complete(cq);
		pthread_mutex_unlock(&virtq->virtq_lock);
		if (comp > max)
			max = comp;
	}
	return max;
}

static void
mlx5_vdpa_drain_cq_one(struct mlx5_vdpa_priv *priv,
	struct mlx5_vdpa_virtq *virtq)
{
	struct mlx5_vdpa_cq *cq = &virtq->eqp.cq;

	mlx5_vdpa_queue_complete(cq);
	if (cq->cq_obj.cq) {
		cq->cq_obj.cqes[0].wqe_counter = rte_cpu_to_be_16(UINT16_MAX);
		virtq->eqp.qp_pi = 0;
		if (!cq->armed)
			mlx5_vdpa_cq_arm(priv, cq);
	}
}

void
mlx5_vdpa_drain_cq(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_vdpa_virtq *virtq;
	unsigned int i;

	for (i = 0; i < priv->caps.max_num_virtio_queues; i++) {
		virtq = &priv->virtqs[i];
		mlx5_vdpa_drain_cq_one(priv, virtq);
	}
}

/* Wait on all CQs channel for completion event. */
static struct mlx5_vdpa_virtq *
mlx5_vdpa_event_wait(struct mlx5_vdpa_priv *priv __rte_unused)
{
#ifdef HAVE_IBV_DEVX_EVENT
	union {
		struct mlx5dv_devx_async_event_hdr event_resp;
		uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) + 128];
	} out;
	int ret = mlx5_glue->devx_get_event(priv->eventc, &out.event_resp,
					    sizeof(out.buf));

	if (ret >= 0)
		return (struct mlx5_vdpa_virtq *)
				(uintptr_t)out.event_resp.cookie;
	DRV_LOG(INFO, "Got error in devx_get_event, ret = %d, errno = %d.",
		ret, errno);
#endif
	return NULL;
}

static uint32_t
mlx5_vdpa_event_handle(void *arg)
{
	struct mlx5_vdpa_priv *priv = arg;
	struct mlx5_vdpa_virtq *virtq;
	uint32_t max;

	switch (priv->event_mode) {
	case MLX5_VDPA_EVENT_MODE_DYNAMIC_TIMER:
	case MLX5_VDPA_EVENT_MODE_FIXED_TIMER:
		priv->timer_delay_us = priv->event_us;
		while (1) {
			max = mlx5_vdpa_queues_complete(priv);
			if (max == 0 && priv->no_traffic_counter++ >=
			    priv->no_traffic_max) {
				DRV_LOG(DEBUG, "Device %s traffic was stopped.",
					priv->vdev->device->name);
				mlx5_vdpa_arm_all_cqs(priv);
				do {
					virtq = mlx5_vdpa_event_wait(priv);
					if (virtq == NULL)
						break;
					pthread_mutex_lock(
						&virtq->virtq_lock);
					if (mlx5_vdpa_queue_complete(
						&virtq->eqp.cq) > 0) {
						pthread_mutex_unlock(
							&virtq->virtq_lock);
						break;
					}
					pthread_mutex_unlock(
						&virtq->virtq_lock);
				} while (1);
				priv->timer_delay_us = priv->event_us;
				priv->no_traffic_counter = 0;
			} else if (max != 0) {
				priv->no_traffic_counter = 0;
			}
			mlx5_vdpa_timer_sleep(priv, max);
		}
		return 0;
	case MLX5_VDPA_EVENT_MODE_ONLY_INTERRUPT:
		do {
			virtq = mlx5_vdpa_event_wait(priv);
			if (virtq != NULL) {
				pthread_mutex_lock(&virtq->virtq_lock);
				if (mlx5_vdpa_queue_complete(
					&virtq->eqp.cq) > 0)
					mlx5_vdpa_cq_arm(priv, &virtq->eqp.cq);
				pthread_mutex_unlock(&virtq->virtq_lock);
			}
		} while (1);
		return 0;
	default:
		return 0;
	}
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
		pthread_mutex_lock(&virtq->virtq_lock);
		if (!virtq->enable || virtq->version != version)
			goto unlock;
		if (rte_rdtsc() / rte_get_tsc_hz() < MLX5_VDPA_ERROR_TIME_SEC)
			goto unlock;
		virtq->stopped = 1;
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
unlock:
		pthread_mutex_unlock(&virtq->virtq_lock);
	}
#endif
}

int
mlx5_vdpa_err_event_setup(struct mlx5_vdpa_priv *priv)
{
	int ret;
	int flags;

	/* Setup device event channel. */
	priv->err_chnl = mlx5_glue->devx_create_event_channel(priv->cdev->ctx,
							      0);
	if (!priv->err_chnl) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create device event channel %d.",
			rte_errno);
		goto error;
	}
	flags = fcntl(priv->err_chnl->fd, F_GETFL);
	ret = fcntl(priv->err_chnl->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to change device event channel FD.");
		goto error;
	}
	priv->err_intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
	if (priv->err_intr_handle == NULL) {
		DRV_LOG(ERR, "Fail to allocate intr_handle");
		goto error;
	}
	if (rte_intr_fd_set(priv->err_intr_handle, priv->err_chnl->fd))
		goto error;

	if (rte_intr_type_set(priv->err_intr_handle, RTE_INTR_HANDLE_EXT))
		goto error;

	ret = rte_intr_callback_register(priv->err_intr_handle,
					 mlx5_vdpa_err_interrupt_handler,
					 priv);
	if (ret != 0) {
		rte_intr_fd_set(priv->err_intr_handle, 0);
		DRV_LOG(ERR, "Failed to register error interrupt for device %d.",
			priv->vid);
		rte_errno = -ret;
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

	if (!rte_intr_fd_get(priv->err_intr_handle))
		return;
	while (retries-- && ret == -EAGAIN) {
		ret = rte_intr_callback_unregister(priv->err_intr_handle,
					    mlx5_vdpa_err_interrupt_handler,
					    priv);
		if (ret == -EAGAIN) {
			DRV_LOG(DEBUG, "Try again to unregister fd %d "
				"of error interrupt, retries = %d.",
				rte_intr_fd_get(priv->err_intr_handle),
				retries);
			rte_pause();
		}
	}
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
	rte_intr_instance_free(priv->err_intr_handle);
}

int
mlx5_vdpa_cqe_event_setup(struct mlx5_vdpa_priv *priv)
{
	int ret;
	rte_thread_attr_t attr;
	char name[RTE_THREAD_INTERNAL_NAME_SIZE];

	if (!priv->eventc)
		/* All virtqs are in poll mode. */
		return 0;
	ret = rte_thread_attr_init(&attr);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to initialize thread attributes");
		goto out;
	}
	if (priv->event_core != -1)
		CPU_SET(priv->event_core, &attr.cpuset);
	else
		attr.cpuset = rte_lcore_cpuset(rte_get_main_lcore());
	ret = rte_thread_create(&priv->timer_tid,
			&attr, mlx5_vdpa_event_handle, priv);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create timer thread.");
		goto out;
	}
	snprintf(name, sizeof(name), "vmlx5-%d", priv->vid);
	rte_thread_set_prefixed_name(priv->timer_tid, name);
out:
	if (ret != 0)
		return -1;
	return 0;
}

void
mlx5_vdpa_cqe_event_unset(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_vdpa_virtq *virtq;
	int i;

	if (priv->timer_tid.opaque_id != 0) {
		pthread_cancel((pthread_t)priv->timer_tid.opaque_id);
		rte_thread_join(priv->timer_tid, NULL);
		/* The mutex may stay locked after event thread cancel, initiate it. */
		for (i = 0; i < priv->nr_virtqs; i++) {
			virtq = &priv->virtqs[i];
			pthread_mutex_init(&virtq->virtq_lock, NULL);
		}
	}
	priv->timer_tid.opaque_id = 0;
}

void
mlx5_vdpa_event_qp_destroy(struct mlx5_vdpa_event_qp *eqp)
{
	mlx5_devx_qp_destroy(&eqp->sw_qp);
	if (eqp->fw_qp)
		claim_zero(mlx5_devx_cmd_destroy(eqp->fw_qp));
	mlx5_vdpa_cq_destroy(&eqp->cq);
	memset(eqp, 0, sizeof(*eqp));
}

static int
mlx5_vdpa_qps2rts(struct mlx5_vdpa_event_qp *eqp)
{
	if (mlx5_devx_cmd_modify_qp_state(eqp->fw_qp, MLX5_CMD_OP_RST2INIT_QP,
					  eqp->sw_qp.qp->id)) {
		DRV_LOG(ERR, "Failed to modify FW QP to INIT state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->sw_qp.qp,
			MLX5_CMD_OP_RST2INIT_QP, eqp->fw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify SW QP to INIT state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->fw_qp, MLX5_CMD_OP_INIT2RTR_QP,
					  eqp->sw_qp.qp->id)) {
		DRV_LOG(ERR, "Failed to modify FW QP to RTR state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->sw_qp.qp,
			MLX5_CMD_OP_INIT2RTR_QP, eqp->fw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify SW QP to RTR state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->fw_qp, MLX5_CMD_OP_RTR2RTS_QP,
					  eqp->sw_qp.qp->id)) {
		DRV_LOG(ERR, "Failed to modify FW QP to RTS state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->sw_qp.qp, MLX5_CMD_OP_RTR2RTS_QP,
					  eqp->fw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify SW QP to RTS state(%u).",
			rte_errno);
		return -1;
	}
	return 0;
}

int
mlx5_vdpa_qps2rst2rts(struct mlx5_vdpa_event_qp *eqp)
{
	if (mlx5_devx_cmd_modify_qp_state(eqp->fw_qp, MLX5_CMD_OP_QP_2RST,
					  eqp->sw_qp.qp->id)) {
		DRV_LOG(ERR, "Failed to modify FW QP to RST state(%u).",
			rte_errno);
		return -1;
	}
	if (mlx5_devx_cmd_modify_qp_state(eqp->sw_qp.qp,
			MLX5_CMD_OP_QP_2RST, eqp->fw_qp->id)) {
		DRV_LOG(ERR, "Failed to modify SW QP to RST state(%u).",
			rte_errno);
		return -1;
	}
	return mlx5_vdpa_qps2rts(eqp);
}

int
mlx5_vdpa_event_qp_prepare(struct mlx5_vdpa_priv *priv, uint16_t desc_n,
	int callfd, struct mlx5_vdpa_virtq *virtq, bool reset)
{
	struct mlx5_vdpa_event_qp *eqp = &virtq->eqp;
	struct mlx5_devx_qp_attr attr = {0};
	uint16_t log_desc_n = rte_log2_u32(desc_n);
	uint32_t ret;

	if (eqp->cq.cq_obj.cq != NULL && log_desc_n == eqp->cq.log_desc_n) {
		/* Reuse existing resources. */
		eqp->cq.callfd = callfd;
		mlx5_vdpa_drain_cq_one(priv, virtq);
		/* FW will set event qp to error state in q destroy. */
		if (reset && !mlx5_vdpa_qps2rst2rts(eqp))
			rte_write32(rte_cpu_to_be_32(RTE_BIT32(log_desc_n)),
					&eqp->sw_qp.db_rec[0]);
		return 0;
	}
	if (eqp->fw_qp)
		mlx5_vdpa_event_qp_destroy(eqp);
	if (mlx5_vdpa_cq_create(priv, log_desc_n, callfd, virtq) ||
		!eqp->cq.cq_obj.cq)
		return -1;
	attr.pd = priv->cdev->pdn;
	attr.ts_format =
		mlx5_ts_format_conv(priv->cdev->config.hca_attr.qp_ts_format);
	eqp->fw_qp = mlx5_devx_cmd_create_qp(priv->cdev->ctx, &attr);
	if (!eqp->fw_qp) {
		DRV_LOG(ERR, "Failed to create FW QP(%u).", rte_errno);
		goto error;
	}
	attr.uar_index = mlx5_os_get_devx_uar_page_id(priv->uar.obj);
	attr.cqn = eqp->cq.cq_obj.cq->id;
	attr.num_of_receive_wqes = RTE_BIT32(log_desc_n);
	attr.log_rq_stride = rte_log2_u32(MLX5_WSEG_SIZE);
	attr.num_of_send_wqbbs = 0; /* No need SQ. */
	attr.ts_format =
		mlx5_ts_format_conv(priv->cdev->config.hca_attr.qp_ts_format);
	ret = mlx5_devx_qp_create(priv->cdev->ctx, &(eqp->sw_qp),
				  attr.num_of_receive_wqes * MLX5_WSEG_SIZE,
				  &attr, SOCKET_ID_ANY);
	if (ret) {
		DRV_LOG(ERR, "Failed to create SW QP(%u).", rte_errno);
		goto error;
	}
	if (mlx5_vdpa_qps2rts(eqp))
		goto error;
	eqp->qp_pi = 0;
	/* First ringing. */
	if (eqp->sw_qp.db_rec)
		rte_write32(rte_cpu_to_be_32(RTE_BIT32(log_desc_n)),
			&eqp->sw_qp.db_rec[0]);
	return 0;
error:
	mlx5_vdpa_event_qp_destroy(eqp);
	return -1;
}

