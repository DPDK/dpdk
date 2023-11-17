/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>

#include <rte_eal.h>
#include <rte_tailq.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "rte_gpudev.h"
#include "gpudev_driver.h"

/* Logging */
RTE_LOG_REGISTER_DEFAULT(gpu_logtype, NOTICE);
#define RTE_LOGTYPE_GPUDEV gpu_logtype

#define GPU_LOG(level, ...) \
	RTE_LOG_LINE(level, GPUDEV, RTE_FMT("gpu: " RTE_FMT_HEAD(__VA_ARGS__ ,), \
		RTE_FMT_TAIL(__VA_ARGS__ ,)))

/* Set any driver error as EPERM */
#define GPU_DRV_RET(function) \
	((function != 0) ? -(rte_errno = EPERM) : (rte_errno = 0))

/* Array of devices */
static struct rte_gpu *gpus;
/* Number of currently valid devices */
static int16_t gpu_max;
/* Number of currently valid devices */
static int16_t gpu_count;

/* Shared memory between processes. */
static const char *GPU_MEMZONE = "rte_gpu_shared";
static struct {
	__extension__ struct rte_gpu_mpshared gpus[0];
} *gpu_shared_mem;

/* Event callback object */
struct rte_gpu_callback {
	TAILQ_ENTRY(rte_gpu_callback) next;
	rte_gpu_callback_t *function;
	void *user_data;
	enum rte_gpu_event event;
};
static rte_rwlock_t gpu_callback_lock = RTE_RWLOCK_INITIALIZER;
static void gpu_free_callbacks(struct rte_gpu *dev);

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
		gpus[dev_id].process_state == RTE_GPU_STATE_INITIALIZED)
		return true;
	return false;
}

static bool
gpu_match_parent(int16_t dev_id, int16_t parent)
{
	if (parent == RTE_GPU_ID_ANY)
		return true;
	return gpus[dev_id].mpshared->info.parent == parent;
}

int16_t
rte_gpu_find_next(int16_t dev_id, int16_t parent)
{
	if (dev_id < 0)
		dev_id = 0;
	while (dev_id < gpu_max &&
			(gpus[dev_id].process_state == RTE_GPU_STATE_UNUSED ||
			!gpu_match_parent(dev_id, parent)))
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
		if (gpus[dev_id].process_state == RTE_GPU_STATE_UNUSED)
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
		if (strncmp(name, dev->mpshared->name, RTE_DEV_NAME_MAX_LEN) == 0)
			return dev;
	}
	return NULL;
}

static int
gpu_shared_mem_init(void)
{
	const struct rte_memzone *memzone;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		memzone = rte_memzone_reserve(GPU_MEMZONE,
				sizeof(*gpu_shared_mem) +
				sizeof(*gpu_shared_mem->gpus) * gpu_max,
				SOCKET_ID_ANY, 0);
	} else {
		memzone = rte_memzone_lookup(GPU_MEMZONE);
	}
	if (memzone == NULL) {
		GPU_LOG(ERR, "cannot initialize shared memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	gpu_shared_mem = memzone->addr;
	return 0;
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

	/* initialize shared memory before adding first device */
	if (gpu_shared_mem == NULL && gpu_shared_mem_init() < 0)
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

	dev->mpshared = &gpu_shared_mem->gpus[dev_id];
	memset(dev->mpshared, 0, sizeof(*dev->mpshared));

	if (rte_strscpy(dev->mpshared->name, name, RTE_DEV_NAME_MAX_LEN) < 0) {
		GPU_LOG(ERR, "device name too long: %s", name);
		rte_errno = ENAMETOOLONG;
		return NULL;
	}
	dev->mpshared->info.name = dev->mpshared->name;
	dev->mpshared->info.dev_id = dev_id;
	dev->mpshared->info.numa_node = -1;
	dev->mpshared->info.parent = RTE_GPU_ID_NONE;
	TAILQ_INIT(&dev->callbacks);
	rte_atomic_fetch_add_explicit(&dev->mpshared->process_refcnt, 1, rte_memory_order_relaxed);

	gpu_count++;
	GPU_LOG(DEBUG, "new device %s (id %d) of total %d",
			name, dev_id, gpu_count);
	return dev;
}

struct rte_gpu *
rte_gpu_attach(const char *name)
{
	int16_t dev_id;
	struct rte_gpu *dev;
	struct rte_gpu_mpshared *shared_dev;

	if (rte_eal_process_type() != RTE_PROC_SECONDARY) {
		GPU_LOG(ERR, "only secondary process can attach device");
		rte_errno = EPERM;
		return NULL;
	}
	if (name == NULL) {
		GPU_LOG(ERR, "attach device without a name");
		rte_errno = EINVAL;
		return NULL;
	}

	/* implicit initialization of library before adding first device */
	if (gpus == NULL && rte_gpu_init(RTE_GPU_DEFAULT_MAX) < 0)
		return NULL;

	/* initialize shared memory before adding first device */
	if (gpu_shared_mem == NULL && gpu_shared_mem_init() < 0)
		return NULL;

	for (dev_id = 0; dev_id < gpu_max; dev_id++) {
		shared_dev = &gpu_shared_mem->gpus[dev_id];
		if (strncmp(name, shared_dev->name, RTE_DEV_NAME_MAX_LEN) == 0)
			break;
	}
	if (dev_id >= gpu_max) {
		GPU_LOG(ERR, "device with name %s not found", name);
		rte_errno = ENOENT;
		return NULL;
	}
	dev = &gpus[dev_id];
	memset(dev, 0, sizeof(*dev));

	TAILQ_INIT(&dev->callbacks);
	dev->mpshared = shared_dev;
	rte_atomic_fetch_add_explicit(&dev->mpshared->process_refcnt, 1, rte_memory_order_relaxed);

	gpu_count++;
	GPU_LOG(DEBUG, "attached device %s (id %d) of total %d",
			name, dev_id, gpu_count);
	return dev;
}

int16_t
rte_gpu_add_child(const char *name, int16_t parent, uint64_t child_context)
{
	struct rte_gpu *dev;

	if (!rte_gpu_is_valid(parent)) {
		GPU_LOG(ERR, "add child to invalid parent ID %d", parent);
		rte_errno = ENODEV;
		return -rte_errno;
	}

	dev = rte_gpu_allocate(name);
	if (dev == NULL)
		return -rte_errno;

	dev->mpshared->info.parent = parent;
	dev->mpshared->info.context = child_context;

	rte_gpu_complete_new(dev);
	return dev->mpshared->info.dev_id;
}

void
rte_gpu_complete_new(struct rte_gpu *dev)
{
	if (dev == NULL)
		return;

	dev->process_state = RTE_GPU_STATE_INITIALIZED;
	rte_gpu_notify(dev, RTE_GPU_EVENT_NEW);
}

int
rte_gpu_release(struct rte_gpu *dev)
{
	int16_t dev_id, child;

	if (dev == NULL) {
		rte_errno = ENODEV;
		return -rte_errno;
	}
	dev_id = dev->mpshared->info.dev_id;
	RTE_GPU_FOREACH_CHILD(child, dev_id) {
		GPU_LOG(ERR, "cannot release device %d with child %d",
				dev_id, child);
		rte_errno = EBUSY;
		return -rte_errno;
	}

	GPU_LOG(DEBUG, "free device %s (id %d)",
			dev->mpshared->info.name, dev->mpshared->info.dev_id);
	rte_gpu_notify(dev, RTE_GPU_EVENT_DEL);

	gpu_free_callbacks(dev);
	dev->process_state = RTE_GPU_STATE_UNUSED;
	rte_atomic_fetch_sub_explicit(&dev->mpshared->process_refcnt, 1, rte_memory_order_relaxed);
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
rte_gpu_callback_register(int16_t dev_id, enum rte_gpu_event event,
		rte_gpu_callback_t *function, void *user_data)
{
	int16_t next_dev, last_dev;
	struct rte_gpu_callback_list *callbacks;
	struct rte_gpu_callback *callback;

	if (!rte_gpu_is_valid(dev_id) && dev_id != RTE_GPU_ID_ANY) {
		GPU_LOG(ERR, "register callback of invalid ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	if (function == NULL) {
		GPU_LOG(ERR, "cannot register callback without function");
		rte_errno = EINVAL;
		return -rte_errno;
	}

	if (dev_id == RTE_GPU_ID_ANY) {
		next_dev = 0;
		last_dev = gpu_max - 1;
	} else {
		next_dev = last_dev = dev_id;
	}

	rte_rwlock_write_lock(&gpu_callback_lock);
	do {
		callbacks = &gpus[next_dev].callbacks;

		/* check if not already registered */
		TAILQ_FOREACH(callback, callbacks, next) {
			if (callback->event == event &&
					callback->function == function &&
					callback->user_data == user_data) {
				GPU_LOG(INFO, "callback already registered");
				rte_rwlock_write_unlock(&gpu_callback_lock);
				return 0;
			}
		}

		callback = malloc(sizeof(*callback));
		if (callback == NULL) {
			GPU_LOG(ERR, "cannot allocate callback");
			rte_rwlock_write_unlock(&gpu_callback_lock);
			rte_errno = ENOMEM;
			return -rte_errno;
		}
		callback->function = function;
		callback->user_data = user_data;
		callback->event = event;
		TAILQ_INSERT_TAIL(callbacks, callback, next);

	} while (++next_dev <= last_dev);
	rte_rwlock_write_unlock(&gpu_callback_lock);

	return 0;
}

int
rte_gpu_callback_unregister(int16_t dev_id, enum rte_gpu_event event,
		rte_gpu_callback_t *function, void *user_data)
{
	int16_t next_dev, last_dev;
	struct rte_gpu_callback_list *callbacks;
	struct rte_gpu_callback *callback, *nextcb;

	if (!rte_gpu_is_valid(dev_id) && dev_id != RTE_GPU_ID_ANY) {
		GPU_LOG(ERR, "unregister callback of invalid ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	if (function == NULL) {
		GPU_LOG(ERR, "cannot unregister callback without function");
		rte_errno = EINVAL;
		return -rte_errno;
	}

	if (dev_id == RTE_GPU_ID_ANY) {
		next_dev = 0;
		last_dev = gpu_max - 1;
	} else {
		next_dev = last_dev = dev_id;
	}

	rte_rwlock_write_lock(&gpu_callback_lock);
	do {
		callbacks = &gpus[next_dev].callbacks;
		RTE_TAILQ_FOREACH_SAFE(callback, callbacks, next, nextcb) {
			if (callback->event != event ||
					callback->function != function ||
					(callback->user_data != user_data &&
					user_data != (void *)-1))
				continue;
			TAILQ_REMOVE(callbacks, callback, next);
			free(callback);
		}
	} while (++next_dev <= last_dev);
	rte_rwlock_write_unlock(&gpu_callback_lock);

	return 0;
}

static void
gpu_free_callbacks(struct rte_gpu *dev)
{
	struct rte_gpu_callback_list *callbacks;
	struct rte_gpu_callback *callback, *nextcb;

	callbacks = &dev->callbacks;
	rte_rwlock_write_lock(&gpu_callback_lock);
	RTE_TAILQ_FOREACH_SAFE(callback, callbacks, next, nextcb) {
		TAILQ_REMOVE(callbacks, callback, next);
		free(callback);
	}
	rte_rwlock_write_unlock(&gpu_callback_lock);
}

void
rte_gpu_notify(struct rte_gpu *dev, enum rte_gpu_event event)
{
	int16_t dev_id;
	struct rte_gpu_callback *callback;

	dev_id = dev->mpshared->info.dev_id;
	rte_rwlock_read_lock(&gpu_callback_lock);
	TAILQ_FOREACH(callback, &dev->callbacks, next) {
		if (callback->event != event || callback->function == NULL)
			continue;
		callback->function(dev_id, event, callback->user_data);
	}
	rte_rwlock_read_unlock(&gpu_callback_lock);
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
		*info = dev->mpshared->info;
		return 0;
	}
	return GPU_DRV_RET(dev->ops.dev_info_get(dev, info));
}

void *
rte_gpu_mem_alloc(int16_t dev_id, size_t size, unsigned int align)
{
	struct rte_gpu *dev;
	void *ptr;
	int ret;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "alloc mem for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return NULL;
	}

	if (dev->ops.mem_alloc == NULL) {
		GPU_LOG(ERR, "mem allocation not supported");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (size == 0) /* dry-run */
		return NULL;

	if (align && !rte_is_power_of_2(align)) {
		GPU_LOG(ERR, "requested alignment is not a power of two %u", align);
		rte_errno = EINVAL;
		return NULL;
	}

	ret = dev->ops.mem_alloc(dev, size, align, &ptr);

	switch (ret) {
	case 0:
		return ptr;
	case -ENOMEM:
	case -E2BIG:
		rte_errno = -ret;
		return NULL;
	default:
		rte_errno = -EPERM;
		return NULL;
	}
}

int
rte_gpu_mem_free(int16_t dev_id, void *ptr)
{
	struct rte_gpu *dev;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "free mem for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}

	if (dev->ops.mem_free == NULL) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (ptr == NULL) /* dry-run */
		return 0;

	return GPU_DRV_RET(dev->ops.mem_free(dev, ptr));
}

int
rte_gpu_mem_register(int16_t dev_id, size_t size, void *ptr)
{
	struct rte_gpu *dev;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "alloc mem for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}

	if (dev->ops.mem_register == NULL) {
		GPU_LOG(ERR, "mem registration not supported");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (ptr == NULL || size == 0) /* dry-run  */
		return 0;

	return GPU_DRV_RET(dev->ops.mem_register(dev, size, ptr));
}

int
rte_gpu_mem_unregister(int16_t dev_id, void *ptr)
{
	struct rte_gpu *dev;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "unregister mem for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}

	if (dev->ops.mem_unregister == NULL) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (ptr == NULL) /* dry-run */
		return 0;

	return GPU_DRV_RET(dev->ops.mem_unregister(dev, ptr));
}

void *
rte_gpu_mem_cpu_map(int16_t dev_id, size_t size, void *ptr)
{
	struct rte_gpu *dev;
	void *ptr_out;
	int ret;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "mem CPU map for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return NULL;
	}

	if (dev->ops.mem_cpu_map == NULL) {
		GPU_LOG(ERR, "mem CPU map not supported");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (ptr == NULL || size == 0) /* dry-run  */
		return NULL;

	ret = GPU_DRV_RET(dev->ops.mem_cpu_map(dev, size, ptr, &ptr_out));

	switch (ret) {
	case 0:
		return ptr_out;
	case -ENOMEM:
	case -E2BIG:
		rte_errno = -ret;
		return NULL;
	default:
		rte_errno = -EPERM;
		return NULL;
	}
}

int
rte_gpu_mem_cpu_unmap(int16_t dev_id, void *ptr)
{
	struct rte_gpu *dev;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "cpu_unmap mem for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}

	if (dev->ops.mem_cpu_unmap == NULL) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (ptr == NULL) /* dry-run */
		return 0;

	return GPU_DRV_RET(dev->ops.mem_cpu_unmap(dev, ptr));
}

int
rte_gpu_wmb(int16_t dev_id)
{
	struct rte_gpu *dev;

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "memory barrier for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}

	if (dev->ops.wmb == NULL) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return GPU_DRV_RET(dev->ops.wmb(dev));
}

int
rte_gpu_comm_create_flag(uint16_t dev_id, struct rte_gpu_comm_flag *devflag,
		enum rte_gpu_comm_flag_type mtype)
{
	size_t flag_size;
	int ret;

	if (devflag == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (mtype != RTE_GPU_COMM_FLAG_CPU) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	flag_size = sizeof(uint32_t);

	devflag->ptr = rte_zmalloc(NULL, flag_size, 0);
	if (devflag->ptr == NULL) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	ret = rte_gpu_mem_register(dev_id, flag_size, devflag->ptr);
	if (ret < 0) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	devflag->mtype = mtype;
	devflag->dev_id = dev_id;

	return 0;
}

int
rte_gpu_comm_destroy_flag(struct rte_gpu_comm_flag *devflag)
{
	int ret;

	if (devflag == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	ret = rte_gpu_mem_unregister(devflag->dev_id, devflag->ptr);
	if (ret < 0) {
		rte_errno = EINVAL;
		return -1;
	}

	rte_free(devflag->ptr);

	return 0;
}

int
rte_gpu_comm_set_flag(struct rte_gpu_comm_flag *devflag, uint32_t val)
{
	if (devflag == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	if (devflag->mtype != RTE_GPU_COMM_FLAG_CPU) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	RTE_GPU_VOLATILE(*devflag->ptr) = val;

	return 0;
}

int
rte_gpu_comm_get_flag_value(struct rte_gpu_comm_flag *devflag, uint32_t *val)
{
	if (devflag == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (devflag->mtype != RTE_GPU_COMM_FLAG_CPU) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	*val = RTE_GPU_VOLATILE(*devflag->ptr);

	return 0;
}

struct rte_gpu_comm_list *
rte_gpu_comm_create_list(uint16_t dev_id,
		uint32_t num_comm_items)
{
	struct rte_gpu_comm_list *comm_list;
	uint32_t idx_l;
	int ret;
	struct rte_gpu *dev;
	struct rte_gpu_info info;

	if (num_comm_items == 0) {
		rte_errno = EINVAL;
		return NULL;
	}

	dev = gpu_get_by_id(dev_id);
	if (dev == NULL) {
		GPU_LOG(ERR, "memory barrier for invalid device ID %d", dev_id);
		rte_errno = ENODEV;
		return NULL;
	}

	ret = rte_gpu_info_get(dev_id, &info);
	if (ret < 0) {
		rte_errno = ENODEV;
		return NULL;
	}

	comm_list = rte_zmalloc(NULL,
			sizeof(struct rte_gpu_comm_list) * num_comm_items, 0);
	if (comm_list == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	ret = rte_gpu_mem_register(dev_id,
			sizeof(struct rte_gpu_comm_list) * num_comm_items, comm_list);
	if (ret < 0) {
		rte_errno = ENOMEM;
		return NULL;
	}

	/*
	 * Use GPU memory CPU map feature if enabled in the driver
	 * to allocate the status flags of the list.
	 * Allocating this flag in GPU memory will reduce
	 * the latency when GPU workload is polling this flag.
	 */
	comm_list[0].status_d = rte_gpu_mem_alloc(dev_id,
			sizeof(enum rte_gpu_comm_list_status) * num_comm_items,
			info.page_size);
	if (ret < 0) {
		rte_errno = ENOMEM;
		return NULL;
	}

	comm_list[0].status_h = rte_gpu_mem_cpu_map(dev_id,
			sizeof(enum rte_gpu_comm_list_status) * num_comm_items,
			comm_list[0].status_d);
	if (comm_list[0].status_h == NULL) {
		/*
		 * If CPU mapping is not supported by driver
		 * use regular CPU registered memory.
		 */
		comm_list[0].status_h = rte_zmalloc(NULL,
				sizeof(enum rte_gpu_comm_list_status) * num_comm_items, 0);
		if (comm_list[0].status_h == NULL) {
			rte_errno = ENOMEM;
			return NULL;
		}

		ret = rte_gpu_mem_register(dev_id,
				sizeof(enum rte_gpu_comm_list_status) * num_comm_items,
				comm_list[0].status_h);
		if (ret < 0) {
			rte_errno = ENOMEM;
			return NULL;
		}

		comm_list[0].status_d = comm_list[0].status_h;
	}

	for (idx_l = 0; idx_l < num_comm_items; idx_l++) {
		comm_list[idx_l].pkt_list = rte_zmalloc(NULL,
				sizeof(struct rte_gpu_comm_pkt) * RTE_GPU_COMM_LIST_PKTS_MAX, 0);
		if (comm_list[idx_l].pkt_list == NULL) {
			rte_errno = ENOMEM;
			return NULL;
		}

		ret = rte_gpu_mem_register(dev_id,
				sizeof(struct rte_gpu_comm_pkt) * RTE_GPU_COMM_LIST_PKTS_MAX,
				comm_list[idx_l].pkt_list);
		if (ret < 0) {
			rte_errno = ENOMEM;
			return NULL;
		}

		comm_list[idx_l].num_pkts = 0;
		comm_list[idx_l].dev_id = dev_id;

		comm_list[idx_l].mbufs = rte_zmalloc(NULL,
				sizeof(struct rte_mbuf *) * RTE_GPU_COMM_LIST_PKTS_MAX, 0);
		if (comm_list[idx_l].mbufs == NULL) {
			rte_errno = ENOMEM;
			return NULL;
		}

		if (idx_l > 0) {
			comm_list[idx_l].status_h = &(comm_list[0].status_h[idx_l]);
			comm_list[idx_l].status_d = &(comm_list[0].status_d[idx_l]);

			ret = rte_gpu_comm_set_status(&comm_list[idx_l], RTE_GPU_COMM_LIST_FREE);
			if (ret < 0) {
				rte_errno = ENOMEM;
				return NULL;
			}
		}
	}

	return comm_list;
}

int
rte_gpu_comm_destroy_list(struct rte_gpu_comm_list *comm_list,
		uint32_t num_comm_items)
{
	uint32_t idx_l;
	int ret;
	uint16_t dev_id;

	if (comm_list == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	dev_id = comm_list[0].dev_id;

	for (idx_l = 0; idx_l < num_comm_items; idx_l++) {
		ret = rte_gpu_mem_unregister(dev_id, comm_list[idx_l].pkt_list);
		if (ret < 0) {
			rte_errno = EINVAL;
			return -1;
		}

		rte_free(comm_list[idx_l].pkt_list);
		rte_free(comm_list[idx_l].mbufs);
	}

	ret = rte_gpu_mem_unregister(dev_id, comm_list);
	if (ret < 0) {
		rte_errno = EINVAL;
		return -1;
	}

	ret = rte_gpu_mem_cpu_unmap(dev_id, comm_list[0].status_d);
	if (ret == 0) {
		rte_gpu_mem_free(dev_id, comm_list[0].status_d);
	} else {
		rte_gpu_mem_unregister(dev_id, comm_list[0].status_h);
		rte_free(comm_list[0].status_h);
	}

	rte_free(comm_list);

	return 0;
}

int
rte_gpu_comm_populate_list_pkts(struct rte_gpu_comm_list *comm_list_item,
		struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
	uint32_t idx;
	int ret;

	if (comm_list_item == NULL || comm_list_item->pkt_list == NULL ||
			mbufs == NULL || num_mbufs > RTE_GPU_COMM_LIST_PKTS_MAX) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	for (idx = 0; idx < num_mbufs; idx++) {
		/* support only unchained mbufs */
		if (unlikely((mbufs[idx]->nb_segs > 1) ||
				(mbufs[idx]->next != NULL) ||
				(mbufs[idx]->data_len != mbufs[idx]->pkt_len))) {
			rte_errno = ENOTSUP;
			return -rte_errno;
		}
		comm_list_item->pkt_list[idx].addr =
				rte_pktmbuf_mtod_offset(mbufs[idx], uintptr_t, 0);
		comm_list_item->pkt_list[idx].size = mbufs[idx]->pkt_len;
		comm_list_item->mbufs[idx] = mbufs[idx];
	}

	RTE_GPU_VOLATILE(comm_list_item->num_pkts) = num_mbufs;
	rte_gpu_wmb(comm_list_item->dev_id);
	ret = rte_gpu_comm_set_status(comm_list_item, RTE_GPU_COMM_LIST_READY);
	if (ret < 0) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	return 0;
}

int
rte_gpu_comm_set_status(struct rte_gpu_comm_list *comm_list_item,
		enum rte_gpu_comm_list_status status)
{
	if (comm_list_item == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	RTE_GPU_VOLATILE(comm_list_item->status_h[0]) = status;

	return 0;
}

int
rte_gpu_comm_get_status(struct rte_gpu_comm_list *comm_list_item,
		enum rte_gpu_comm_list_status *status)
{
	if (comm_list_item == NULL || status == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	*status = RTE_GPU_VOLATILE(comm_list_item->status_h[0]);

	return 0;
}

int
rte_gpu_comm_cleanup_list(struct rte_gpu_comm_list *comm_list_item)
{
	uint32_t idx = 0;
	enum rte_gpu_comm_list_status status;
	int ret;

	if (comm_list_item == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	ret = rte_gpu_comm_get_status(comm_list_item, &status);
	if (ret < 0) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	if (status == RTE_GPU_COMM_LIST_READY) {
		GPU_LOG(ERR, "packet list is still in progress");
		rte_errno = EINVAL;
		return -rte_errno;
	}

	for (idx = 0; idx < RTE_GPU_COMM_LIST_PKTS_MAX; idx++) {
		if (comm_list_item->pkt_list[idx].addr == 0)
			break;

		comm_list_item->pkt_list[idx].addr = 0;
		comm_list_item->pkt_list[idx].size = 0;
		comm_list_item->mbufs[idx] = NULL;
	}

	ret = rte_gpu_comm_set_status(comm_list_item, RTE_GPU_COMM_LIST_FREE);
	if (ret < 0) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	RTE_GPU_VOLATILE(comm_list_item->num_pkts) = 0;
	rte_mb();

	return 0;
}
