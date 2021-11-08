/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "rte_gpudev.h"
#include "gpudev_driver.h"

/* Logging */
RTE_LOG_REGISTER_DEFAULT(gpu_logtype, NOTICE);
#define GPU_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, gpu_logtype, RTE_FMT("gpu: " \
		RTE_FMT_HEAD(__VA_ARGS__, ) "\n", RTE_FMT_TAIL(__VA_ARGS__, )))

/* Set any driver error as EPERM */
#define GPU_DRV_RET(function) \
	((function != 0) ? -(rte_errno = EPERM) : (rte_errno = 0))

/* Array of devices */
static struct rte_gpu *gpus;
/* Number of currently valid devices */
static int16_t gpu_max;
/* Number of currently valid devices */
static int16_t gpu_count;

int
rte_gpu_init(size_t dev_max)
{
	if (dev_max == 0 || dev_max > INT16_MAX) {
		GPU_LOG(ERR, "invalid array size");
		rte_errno = EINVAL;
		return -rte_errno;
	}

	/* No lock, it must be called before or during first probing. */
	if (gpus != NULL) {
		GPU_LOG(ERR, "already initialized");
		rte_errno = EBUSY;
		return -rte_errno;
	}

	gpus = calloc(dev_max, sizeof(struct rte_gpu));
	if (gpus == NULL) {
		GPU_LOG(ERR, "cannot initialize library");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	gpu_max = dev_max;
	return 0;
}

uint16_t
rte_gpu_count_avail(void)
{
	return gpu_count;
}

bool
rte_gpu_is_valid(int16_t dev_id)
{
	if (dev_id >= 0 && dev_id < gpu_max &&
		gpus[dev_id].state == RTE_GPU_STATE_INITIALIZED)
		return true;
	return false;
}

int16_t
rte_gpu_find_next(int16_t dev_id)
{
	if (dev_id < 0)
		dev_id = 0;
	while (dev_id < gpu_max &&
			gpus[dev_id].state == RTE_GPU_STATE_UNUSED)
		dev_id++;

	if (dev_id >= gpu_max)
		return RTE_GPU_ID_NONE;
	return dev_id;
}

static int16_t
gpu_find_free_id(void)
{
	int16_t dev_id;

	for (dev_id = 0; dev_id < gpu_max; dev_id++) {
		if (gpus[dev_id].state == RTE_GPU_STATE_UNUSED)
			return dev_id;
	}
	return RTE_GPU_ID_NONE;
}

static struct rte_gpu *
gpu_get_by_id(int16_t dev_id)
{
	if (!rte_gpu_is_valid(dev_id))
		return NULL;
	return &gpus[dev_id];
}

struct rte_gpu *
rte_gpu_get_by_name(const char *name)
{
	int16_t dev_id;
	struct rte_gpu *dev;

	if (name == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	RTE_GPU_FOREACH(dev_id) {
		dev = &gpus[dev_id];
		if (strncmp(name, dev->name, RTE_DEV_NAME_MAX_LEN) == 0)
			return dev;
	}
	return NULL;
}

struct rte_gpu *
rte_gpu_allocate(const char *name)
{
	int16_t dev_id;
	struct rte_gpu *dev;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		GPU_LOG(ERR, "only primary process can allocate device");
		rte_errno = EPERM;
		return NULL;
	}
	if (name == NULL) {
		GPU_LOG(ERR, "allocate device without a name");
		rte_errno = EINVAL;
		return NULL;
	}

	/* implicit initialization of library before adding first device */
	if (gpus == NULL && rte_gpu_init(RTE_GPU_DEFAULT_MAX) < 0)
		return NULL;

	if (rte_gpu_get_by_name(name) != NULL) {
		GPU_LOG(ERR, "device with name %s already exists", name);
		rte_errno = EEXIST;
		return NULL;
	}
	dev_id = gpu_find_free_id();
	if (dev_id == RTE_GPU_ID_NONE) {
		GPU_LOG(ERR, "reached maximum number of devices");
		rte_errno = ENOENT;
		return NULL;
	}

	dev = &gpus[dev_id];
	memset(dev, 0, sizeof(*dev));

	if (rte_strscpy(dev->name, name, RTE_DEV_NAME_MAX_LEN) < 0) {
		GPU_LOG(ERR, "device name too long: %s", name);
		rte_errno = ENAMETOOLONG;
		return NULL;
	}
	dev->info.name = dev->name;
	dev->info.dev_id = dev_id;
	dev->info.numa_node = -1;

	gpu_count++;
	GPU_LOG(DEBUG, "new device %s (id %d) of total %d",
			name, dev_id, gpu_count);
	return dev;
}

void
rte_gpu_complete_new(struct rte_gpu *dev)
{
	if (dev == NULL)
		return;

	dev->state = RTE_GPU_STATE_INITIALIZED;
}

int
rte_gpu_release(struct rte_gpu *dev)
{
	if (dev == NULL) {
		rte_errno = ENODEV;
		return -rte_errno;
	}

	GPU_LOG(DEBUG, "free device %s (id %d)",
			dev->info.name, dev->info.dev_id);
	dev->state = RTE_GPU_STATE_UNUSED;
	gpu_count--;

	return 0;
}

int
rte_gpu_close(int16_t dev_id)
{
	int firsterr, binerr;
	int *lasterr = &firsterr;
	struct rte_gpu *dev;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "close invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}

	if (dev->ops.dev_close != NULL) {
		*lasterr = GPU_DRV_RET(dev->ops.dev_close(dev));
		if (*lasterr != 0)
			lasterr = &binerr;
	}

	*lasterr = rte_gpu_release(dev);

	rte_errno = -firsterr;
	return firsterr;
}

int
rte_gpu_info_get(int16_t dev_id, struct rte_gpu_info *info)
{
	struct rte_gpu *dev;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "query invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	if (info == NULL) {
		GPU_LOG(ERR, "query without storage");
		rte_errno = EINVAL;
		return -rte_errno;
	}

	if (dev->ops.dev_info_get == NULL) {
		*info = dev->info;
		return 0;
	}
	return GPU_DRV_RET(dev->ops.dev_info_get(dev, info));
}
