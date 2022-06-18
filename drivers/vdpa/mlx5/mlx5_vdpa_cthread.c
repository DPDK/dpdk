/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_io.h>
#include <rte_alarm.h>
#include <rte_tailq.h>
#include <rte_ring_elem.h>
#include <rte_ring_peek.h>

#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"

static inline uint32_t
mlx5_vdpa_c_thrd_ring_dequeue_bulk(struct rte_ring *r,
	void **obj, uint32_t n, uint32_t *avail)
{
	uint32_t m;

	m = rte_ring_dequeue_bulk_elem_start(r, obj,
		sizeof(struct mlx5_vdpa_task), n, avail);
	n = (m == n) ? n : 0;
	rte_ring_dequeue_elem_finish(r, n);
	return n;
}

static inline uint32_t
mlx5_vdpa_c_thrd_ring_enqueue_bulk(struct rte_ring *r,
	void * const *obj, uint32_t n, uint32_t *free)
{
	uint32_t m;

	m = rte_ring_enqueue_bulk_elem_start(r, n, free);
	n = (m == n) ? n : 0;
	rte_ring_enqueue_elem_finish(r, obj,
		sizeof(struct mlx5_vdpa_task), n);
	return n;
}

bool
mlx5_vdpa_task_add(struct mlx5_vdpa_priv *priv,
		uint32_t thrd_idx,
		enum mlx5_vdpa_task_type task_type,
		uint32_t *remaining_cnt, uint32_t *err_cnt,
		void **task_data, uint32_t num)
{
	struct rte_ring *rng = conf_thread_mng.cthrd[thrd_idx].rng;
	struct mlx5_vdpa_task task[MLX5_VDPA_TASKS_PER_DEV];
	uint32_t *data = (uint32_t *)task_data;
	uint32_t i;

	MLX5_ASSERT(num <= MLX5_VDPA_TASKS_PER_DEV);
	for (i = 0 ; i < num; i++) {
		task[i].priv = priv;
		/* To be added later. */
		task[i].type = task_type;
		task[i].remaining_cnt = remaining_cnt;
		task[i].err_cnt = err_cnt;
		if (data)
			task[i].idx = data[i];
	}
	if (!mlx5_vdpa_c_thrd_ring_enqueue_bulk(rng, (void **)&task, num, NULL))
		return -1;
	for (i = 0 ; i < num; i++)
		if (task[i].remaining_cnt)
			__atomic_fetch_add(task[i].remaining_cnt, 1,
				__ATOMIC_RELAXED);
	/* wake up conf thread. */
	pthread_mutex_lock(&conf_thread_mng.cthrd_lock);
	pthread_cond_signal(&conf_thread_mng.cthrd[thrd_idx].c_cond);
	pthread_mutex_unlock(&conf_thread_mng.cthrd_lock);
	return 0;
}

bool
mlx5_vdpa_c_thread_wait_bulk_tasks_done(uint32_t *remaining_cnt,
		uint32_t *err_cnt, uint32_t sleep_time)
{
	/* Check and wait all tasks done. */
	while (__atomic_load_n(remaining_cnt,
		__ATOMIC_RELAXED) != 0) {
		rte_delay_us_sleep(sleep_time);
	}
	if (__atomic_load_n(err_cnt,
		__ATOMIC_RELAXED)) {
		DRV_LOG(ERR, "Tasks done with error.");
		return true;
	}
	return false;
}

static void *
mlx5_vdpa_c_thread_handle(void *arg)
{
	struct mlx5_vdpa_conf_thread_mng *multhrd = arg;
	pthread_t thread_id = pthread_self();
	struct mlx5_vdpa_virtq *virtq;
	struct mlx5_vdpa_priv *priv;
	struct mlx5_vdpa_task task;
	struct rte_ring *rng;
	uint64_t features;
	uint32_t thrd_idx;
	uint32_t task_num;
	int ret;

	for (thrd_idx = 0; thrd_idx < multhrd->max_thrds;
		thrd_idx++)
		if (multhrd->cthrd[thrd_idx].tid == thread_id)
			break;
	if (thrd_idx >= multhrd->max_thrds)
		return NULL;
	rng = multhrd->cthrd[thrd_idx].rng;
	while (1) {
		task_num = mlx5_vdpa_c_thrd_ring_dequeue_bulk(rng,
			(void **)&task, 1, NULL);
		if (!task_num) {
			/* No task and condition wait. */
			pthread_mutex_lock(&multhrd->cthrd_lock);
			pthread_cond_wait(
				&multhrd->cthrd[thrd_idx].c_cond,
				&multhrd->cthrd_lock);
			pthread_mutex_unlock(&multhrd->cthrd_lock);
			continue;
		}
		priv = task.priv;
		if (priv == NULL)
			continue;
		switch (task.type) {
		case MLX5_VDPA_TASK_REG_MR:
			ret = mlx5_vdpa_register_mr(priv, task.idx);
			if (ret) {
				DRV_LOG(ERR,
				"Failed to register mr %d.", task.idx);
				__atomic_fetch_add(task.err_cnt, 1,
				__ATOMIC_RELAXED);
			}
			break;
		case MLX5_VDPA_TASK_SETUP_VIRTQ:
			virtq = &priv->virtqs[task.idx];
			pthread_mutex_lock(&virtq->virtq_lock);
			ret = mlx5_vdpa_virtq_setup(priv,
				task.idx, false);
			if (ret) {
				DRV_LOG(ERR,
					"Failed to setup virtq %d.", task.idx);
				__atomic_fetch_add(
					task.err_cnt, 1, __ATOMIC_RELAXED);
			}
			virtq->enable = 1;
			pthread_mutex_unlock(&virtq->virtq_lock);
			break;
		case MLX5_VDPA_TASK_STOP_VIRTQ:
			virtq = &priv->virtqs[task.idx];
			pthread_mutex_lock(&virtq->virtq_lock);
			ret = mlx5_vdpa_virtq_stop(priv,
					task.idx);
			if (ret) {
				DRV_LOG(ERR,
				"Failed to stop virtq %d.",
				task.idx);
				__atomic_fetch_add(
					task.err_cnt, 1,
					__ATOMIC_RELAXED);
				pthread_mutex_unlock(&virtq->virtq_lock);
				break;
			}
			ret = rte_vhost_get_negotiated_features(
				priv->vid, &features);
			if (ret) {
				DRV_LOG(ERR,
		"Failed to get negotiated features virtq %d.",
				task.idx);
				__atomic_fetch_add(
					task.err_cnt, 1,
					__ATOMIC_RELAXED);
				pthread_mutex_unlock(&virtq->virtq_lock);
				break;
			}
			if (RTE_VHOST_NEED_LOG(features))
				rte_vhost_log_used_vring(
				priv->vid, task.idx, 0,
			    MLX5_VDPA_USED_RING_LEN(virtq->vq_size));
			pthread_mutex_unlock(&virtq->virtq_lock);
			break;
		case MLX5_VDPA_TASK_DEV_CLOSE_NOWAIT:
			mlx5_vdpa_virtq_unreg_intr_handle_all(priv);
			pthread_mutex_lock(&priv->steer_update_lock);
			mlx5_vdpa_steer_unset(priv);
			pthread_mutex_unlock(&priv->steer_update_lock);
			mlx5_vdpa_virtqs_release(priv, false);
			mlx5_vdpa_drain_cq(priv);
			if (priv->lm_mr.addr)
				mlx5_os_wrapped_mkey_destroy(
					&priv->lm_mr);
			if (!priv->connected)
				mlx5_vdpa_dev_cache_clean(priv);
			priv->vid = 0;
			__atomic_store_n(
				&priv->dev_close_progress, 0,
				__ATOMIC_RELAXED);
			break;
		case MLX5_VDPA_TASK_PREPARE_VIRTQ:
			ret = mlx5_vdpa_virtq_single_resource_prepare(
					priv, task.idx);
			if (ret) {
				DRV_LOG(ERR,
				"Failed to prepare virtq %d.",
				task.idx);
				__atomic_fetch_add(
				task.err_cnt, 1,
				__ATOMIC_RELAXED);
			}
			break;
		default:
			DRV_LOG(ERR, "Invalid vdpa task type %d.",
			task.type);
			break;
		}
		if (task.remaining_cnt)
			__atomic_fetch_sub(task.remaining_cnt,
			1, __ATOMIC_RELAXED);
	}
	return NULL;
}

static void
mlx5_vdpa_c_thread_destroy(uint32_t thrd_idx, bool need_unlock)
{
	if (conf_thread_mng.cthrd[thrd_idx].tid) {
		pthread_cancel(conf_thread_mng.cthrd[thrd_idx].tid);
		pthread_join(conf_thread_mng.cthrd[thrd_idx].tid, NULL);
		conf_thread_mng.cthrd[thrd_idx].tid = 0;
		if (need_unlock)
			pthread_mutex_init(&conf_thread_mng.cthrd_lock, NULL);
	}
	if (conf_thread_mng.cthrd[thrd_idx].rng) {
		rte_ring_free(conf_thread_mng.cthrd[thrd_idx].rng);
		conf_thread_mng.cthrd[thrd_idx].rng = NULL;
	}
}

static int
mlx5_vdpa_c_thread_create(int cpu_core)
{
	const struct sched_param sp = {
		.sched_priority = sched_get_priority_max(SCHED_RR),
	};
	rte_cpuset_t cpuset;
	pthread_attr_t attr;
	uint32_t thrd_idx;
	uint32_t ring_num;
	char name[32];
	int ret;

	pthread_mutex_lock(&conf_thread_mng.cthrd_lock);
	pthread_attr_init(&attr);
	ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
	if (ret) {
		DRV_LOG(ERR, "Failed to set thread sched policy = RR.");
		goto c_thread_err;
	}
	ret = pthread_attr_setschedparam(&attr, &sp);
	if (ret) {
		DRV_LOG(ERR, "Failed to set thread priority.");
		goto c_thread_err;
	}
	ring_num = MLX5_VDPA_MAX_TASKS_PER_THRD / conf_thread_mng.max_thrds;
	if (!ring_num) {
		DRV_LOG(ERR, "Invalid ring number for thread.");
		goto c_thread_err;
	}
	for (thrd_idx = 0; thrd_idx < conf_thread_mng.max_thrds;
		thrd_idx++) {
		snprintf(name, sizeof(name), "vDPA-mthread-ring-%d",
			thrd_idx);
		conf_thread_mng.cthrd[thrd_idx].rng = rte_ring_create_elem(name,
			sizeof(struct mlx5_vdpa_task), ring_num,
			rte_socket_id(),
			RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ |
			RING_F_EXACT_SZ);
		if (!conf_thread_mng.cthrd[thrd_idx].rng) {
			DRV_LOG(ERR,
			"Failed to create vdpa multi-threads %d ring.",
			thrd_idx);
			goto c_thread_err;
		}
		ret = pthread_create(&conf_thread_mng.cthrd[thrd_idx].tid,
				&attr, mlx5_vdpa_c_thread_handle,
				(void *)&conf_thread_mng);
		if (ret) {
			DRV_LOG(ERR, "Failed to create vdpa multi-threads %d.",
					thrd_idx);
			goto c_thread_err;
		}
		CPU_ZERO(&cpuset);
		if (cpu_core != -1)
			CPU_SET(cpu_core, &cpuset);
		else
			cpuset = rte_lcore_cpuset(rte_get_main_lcore());
		ret = pthread_setaffinity_np(
				conf_thread_mng.cthrd[thrd_idx].tid,
				sizeof(cpuset), &cpuset);
		if (ret) {
			DRV_LOG(ERR, "Failed to set thread affinity for "
			"vdpa multi-threads %d.", thrd_idx);
			goto c_thread_err;
		}
		snprintf(name, sizeof(name), "vDPA-mthread-%d", thrd_idx);
		ret = pthread_setname_np(
				conf_thread_mng.cthrd[thrd_idx].tid, name);
		if (ret)
			DRV_LOG(ERR, "Failed to set vdpa multi-threads name %s.",
					name);
		else
			DRV_LOG(DEBUG, "Thread name: %s.", name);
		pthread_cond_init(&conf_thread_mng.cthrd[thrd_idx].c_cond,
			NULL);
	}
	pthread_mutex_unlock(&conf_thread_mng.cthrd_lock);
	return 0;
c_thread_err:
	for (thrd_idx = 0; thrd_idx < conf_thread_mng.max_thrds;
		thrd_idx++)
		mlx5_vdpa_c_thread_destroy(thrd_idx, false);
	pthread_mutex_unlock(&conf_thread_mng.cthrd_lock);
	return -1;
}

int
mlx5_vdpa_mult_threads_create(int cpu_core)
{
	pthread_mutex_init(&conf_thread_mng.cthrd_lock, NULL);
	if (mlx5_vdpa_c_thread_create(cpu_core)) {
		DRV_LOG(ERR, "Cannot create vDPA configuration threads.");
		mlx5_vdpa_mult_threads_destroy(false);
		return -1;
	}
	return 0;
}

void
mlx5_vdpa_mult_threads_destroy(bool need_unlock)
{
	uint32_t thrd_idx;

	if (!conf_thread_mng.initializer_priv)
		return;
	for (thrd_idx = 0; thrd_idx < conf_thread_mng.max_thrds;
		thrd_idx++)
		mlx5_vdpa_c_thread_destroy(thrd_idx, need_unlock);
	pthread_mutex_destroy(&conf_thread_mng.cthrd_lock);
	memset(&conf_thread_mng, 0, sizeof(struct mlx5_vdpa_conf_thread_mng));
}
