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

#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"

static void *
mlx5_vdpa_c_thread_handle(void *arg)
{
	/* To be added later. */
	return arg;
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
	for (thrd_idx = 0; thrd_idx < conf_thread_mng.max_thrds;
		thrd_idx++) {
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
