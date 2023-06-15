/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>
#include <rte_interrupts.h>

#include "rte_bbdev_op.h"
#include "rte_bbdev.h"
#include "rte_bbdev_pmd.h"

#define DEV_NAME "BBDEV"

/* Number of supported operation types in *rte_bbdev_op_type*. */
#define BBDEV_OP_TYPE_COUNT 7

/* BBDev library logging ID */
RTE_LOG_REGISTER_DEFAULT(bbdev_logtype, NOTICE);

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, bbdev_logtype, fmt "\n", ##__VA_ARGS__)

#define rte_bbdev_log_debug(fmt, ...) \
	rte_bbdev_log(DEBUG, RTE_STR(__LINE__) ":%s() " fmt, __func__, \
		##__VA_ARGS__)

/* Helper macro to check dev_id is valid */
#define VALID_DEV_OR_RET_ERR(dev, dev_id) do { \
	if (dev == NULL) { \
		rte_bbdev_log(ERR, "device %u is invalid", dev_id); \
		return -ENODEV; \
	} \
} while (0)

/* Helper macro to check dev_ops is valid */
#define VALID_DEV_OPS_OR_RET_ERR(dev, dev_id) do { \
	if (dev->dev_ops == NULL) { \
		rte_bbdev_log(ERR, "NULL dev_ops structure in device %u", \
				dev_id); \
		return -ENODEV; \
	} \
} while (0)

/* Helper macro to check that driver implements required function pointer */
#define VALID_FUNC_OR_RET_ERR(func, dev_id) do { \
	if (func == NULL) { \
		rte_bbdev_log(ERR, "device %u does not support %s", \
				dev_id, #func); \
		return -ENOTSUP; \
	} \
} while (0)

/* Helper macro to check that queue is valid */
#define VALID_QUEUE_OR_RET_ERR(queue_id, dev) do { \
	if (queue_id >= dev->data->num_queues) { \
		rte_bbdev_log(ERR, "Invalid queue_id %u for device %u", \
				queue_id, dev->data->dev_id); \
		return -ERANGE; \
	} \
} while (0)

/* List of callback functions registered by an application */
struct rte_bbdev_callback {
	TAILQ_ENTRY(rte_bbdev_callback) next;  /* Callbacks list */
	rte_bbdev_cb_fn cb_fn;  /* Callback address */
	void *cb_arg;  /* Parameter for callback */
	void *ret_param;  /* Return parameter */
	enum rte_bbdev_event_type event; /* Interrupt event type */
	uint32_t active; /* Callback is executing */
};

/* spinlock for bbdev device callbacks */
static rte_spinlock_t rte_bbdev_cb_lock = RTE_SPINLOCK_INITIALIZER;

/*
 * Global array of all devices. This is not static because it's used by the
 * inline enqueue and dequeue functions
 */
struct rte_bbdev rte_bbdev_devices[RTE_BBDEV_MAX_DEVS];

/* Global array with rte_bbdev_data structures */
static struct rte_bbdev_data *rte_bbdev_data;

/* Memzone name for global bbdev data pool */
static const char *MZ_RTE_BBDEV_DATA = "rte_bbdev_data";

/* Number of currently valid devices */
static uint16_t num_devs;

/* Return pointer to device structure, with validity check */
static struct rte_bbdev *
get_dev(uint16_t dev_id)
{
	if (rte_bbdev_is_valid(dev_id))
		return &rte_bbdev_devices[dev_id];
	return NULL;
}

/* Allocate global data array */
static int
rte_bbdev_data_alloc(void)
{
	const unsigned int flags = 0;
	const struct rte_memzone *mz;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(MZ_RTE_BBDEV_DATA,
				RTE_BBDEV_MAX_DEVS * sizeof(*rte_bbdev_data),
				rte_socket_id(), flags);
	} else
		mz = rte_memzone_lookup(MZ_RTE_BBDEV_DATA);
	if (mz == NULL) {
		rte_bbdev_log(CRIT,
				"Cannot allocate memzone for bbdev port data");
		return -ENOMEM;
	}

	rte_bbdev_data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(rte_bbdev_data, 0,
				RTE_BBDEV_MAX_DEVS * sizeof(*rte_bbdev_data));
	return 0;
}

/*
 * Find data allocated for the device or if not found return first unused bbdev
 * data. If all structures are in use and none is used by the device return
 * NULL.
 */
static struct rte_bbdev_data *
find_bbdev_data(const char *name)
{
	uint16_t data_id;

	for (data_id = 0; data_id < RTE_BBDEV_MAX_DEVS; ++data_id) {
		if (strlen(rte_bbdev_data[data_id].name) == 0) {
			memset(&rte_bbdev_data[data_id], 0,
					sizeof(struct rte_bbdev_data));
			return &rte_bbdev_data[data_id];
		} else if (strncmp(rte_bbdev_data[data_id].name, name,
				RTE_BBDEV_NAME_MAX_LEN) == 0)
			return &rte_bbdev_data[data_id];
	}

	return NULL;
}

/* Find lowest device id with no attached device */
static uint16_t
find_free_dev_id(void)
{
	uint16_t i;
	for (i = 0; i < RTE_BBDEV_MAX_DEVS; i++) {
		if (rte_bbdev_devices[i].state == RTE_BBDEV_UNUSED)
			return i;
	}
	return RTE_BBDEV_MAX_DEVS;
}

struct rte_bbdev *
rte_bbdev_allocate(const char *name)
{
	int ret;
	struct rte_bbdev *bbdev;
	uint16_t dev_id;

	if (name == NULL) {
		rte_bbdev_log(ERR, "Invalid null device name");
		return NULL;
	}

	if (rte_bbdev_get_named_dev(name) != NULL) {
		rte_bbdev_log(ERR, "Device \"%s\" is already allocated", name);
		return NULL;
	}

	dev_id = find_free_dev_id();
	if (dev_id == RTE_BBDEV_MAX_DEVS) {
		rte_bbdev_log(ERR, "Reached maximum number of devices");
		return NULL;
	}

	bbdev = &rte_bbdev_devices[dev_id];

	if (rte_bbdev_data == NULL) {
		ret = rte_bbdev_data_alloc();
		if (ret != 0)
			return NULL;
	}

	bbdev->data = find_bbdev_data(name);
	if (bbdev->data == NULL) {
		rte_bbdev_log(ERR,
				"Max BBDevs already allocated in multi-process environment!");
		return NULL;
	}

	__atomic_fetch_add(&bbdev->data->process_cnt, 1, __ATOMIC_RELAXED);
	bbdev->data->dev_id = dev_id;
	bbdev->state = RTE_BBDEV_INITIALIZED;

	ret = snprintf(bbdev->data->name, RTE_BBDEV_NAME_MAX_LEN, "%s", name);
	if ((ret < 0) || (ret >= RTE_BBDEV_NAME_MAX_LEN)) {
		rte_bbdev_log(ERR, "Copying device name \"%s\" failed", name);
		return NULL;
	}

	/* init user callbacks */
	TAILQ_INIT(&(bbdev->list_cbs));

	num_devs++;

	rte_bbdev_log_debug("Initialised device %s (id = %u). Num devices = %u",
			name, dev_id, num_devs);

	return bbdev;
}

int
rte_bbdev_release(struct rte_bbdev *bbdev)
{
	uint16_t dev_id;
	struct rte_bbdev_callback *cb, *next;

	if (bbdev == NULL) {
		rte_bbdev_log(ERR, "NULL bbdev");
		return -ENODEV;
	}
	dev_id = bbdev->data->dev_id;

	/* free all callbacks from the device's list */
	for (cb = TAILQ_FIRST(&bbdev->list_cbs); cb != NULL; cb = next) {

		next = TAILQ_NEXT(cb, next);
		TAILQ_REMOVE(&(bbdev->list_cbs), cb, next);
		rte_free(cb);
	}

	/* clear shared BBDev Data if no process is using the device anymore */
	if (__atomic_fetch_sub(&bbdev->data->process_cnt, 1,
			      __ATOMIC_RELAXED) - 1 == 0)
		memset(bbdev->data, 0, sizeof(*bbdev->data));

	memset(bbdev, 0, sizeof(*bbdev));
	num_devs--;
	bbdev->state = RTE_BBDEV_UNUSED;

	rte_bbdev_log_debug(
			"Un-initialised device id = %u. Num devices = %u",
			dev_id, num_devs);
	return 0;
}

struct rte_bbdev *
rte_bbdev_get_named_dev(const char *name)
{
	unsigned int i;

	if (name == NULL) {
		rte_bbdev_log(ERR, "NULL driver name");
		return NULL;
	}

	for (i = 0; i < RTE_BBDEV_MAX_DEVS; i++) {
		struct rte_bbdev *dev = get_dev(i);
		if (dev && (strncmp(dev->data->name,
				name, RTE_BBDEV_NAME_MAX_LEN) == 0))
			return dev;
	}

	return NULL;
}

uint16_t
rte_bbdev_count(void)
{
	return num_devs;
}

bool
rte_bbdev_is_valid(uint16_t dev_id)
{
	if ((dev_id < RTE_BBDEV_MAX_DEVS) &&
		rte_bbdev_devices[dev_id].state == RTE_BBDEV_INITIALIZED)
		return true;
	return false;
}

uint16_t
rte_bbdev_find_next(uint16_t dev_id)
{
	dev_id++;
	for (; dev_id < RTE_BBDEV_MAX_DEVS; dev_id++)
		if (rte_bbdev_is_valid(dev_id))
			break;
	return dev_id;
}

int
rte_bbdev_setup_queues(uint16_t dev_id, uint16_t num_queues, int socket_id)
{
	unsigned int i;
	int ret;
	struct rte_bbdev_driver_info dev_info;
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	if (dev->data->started) {
		rte_bbdev_log(ERR,
				"Device %u cannot be configured when started",
				dev_id);
		return -EBUSY;
	}

	/* Get device driver information to get max number of queues */
	VALID_FUNC_OR_RET_ERR(dev->dev_ops->info_get, dev_id);
	memset(&dev_info, 0, sizeof(dev_info));
	dev->dev_ops->info_get(dev, &dev_info);

	if ((num_queues == 0) || (num_queues > dev_info.max_num_queues)) {
		rte_bbdev_log(ERR,
				"Device %u supports 0 < N <= %u queues, not %u",
				dev_id, dev_info.max_num_queues, num_queues);
		return -EINVAL;
	}

	/* If re-configuration, get driver to free existing internal memory */
	if (dev->data->queues != NULL) {
		VALID_FUNC_OR_RET_ERR(dev->dev_ops->queue_release, dev_id);
		for (i = 0; i < dev->data->num_queues; i++) {
			int ret = dev->dev_ops->queue_release(dev, i);
			if (ret < 0) {
				rte_bbdev_log(ERR,
						"Device %u queue %u release failed",
						dev_id, i);
				return ret;
			}
		}
		/* Call optional device close */
		if (dev->dev_ops->close) {
			ret = dev->dev_ops->close(dev);
			if (ret < 0) {
				rte_bbdev_log(ERR,
						"Device %u couldn't be closed",
						dev_id);
				return ret;
			}
		}
		rte_free(dev->data->queues);
	}

	/* Allocate queue pointers */
	dev->data->queues = rte_calloc_socket(DEV_NAME, num_queues,
			sizeof(dev->data->queues[0]), RTE_CACHE_LINE_SIZE,
				dev->data->socket_id);
	if (dev->data->queues == NULL) {
		rte_bbdev_log(ERR,
				"calloc of %u queues for device %u on socket %i failed",
				num_queues, dev_id, dev->data->socket_id);
		return -ENOMEM;
	}

	dev->data->num_queues = num_queues;

	/* Call optional device configuration */
	if (dev->dev_ops->setup_queues) {
		ret = dev->dev_ops->setup_queues(dev, num_queues, socket_id);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Device %u memory configuration failed",
					dev_id);
			goto error;
		}
	}

	rte_bbdev_log_debug("Device %u set up with %u queues", dev_id,
			num_queues);
	return 0;

error:
	dev->data->num_queues = 0;
	rte_free(dev->data->queues);
	dev->data->queues = NULL;
	return ret;
}

int
rte_bbdev_intr_enable(uint16_t dev_id)
{
	int ret;
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	if (dev->data->started) {
		rte_bbdev_log(ERR,
				"Device %u cannot be configured when started",
				dev_id);
		return -EBUSY;
	}

	if (dev->dev_ops->intr_enable) {
		ret = dev->dev_ops->intr_enable(dev);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Device %u interrupts configuration failed",
					dev_id);
			return ret;
		}
		rte_bbdev_log_debug("Enabled interrupts for dev %u", dev_id);
		return 0;
	}

	rte_bbdev_log(ERR, "Device %u doesn't support interrupts", dev_id);
	return -ENOTSUP;
}

int
rte_bbdev_queue_configure(uint16_t dev_id, uint16_t queue_id,
		const struct rte_bbdev_queue_conf *conf)
{
	int ret = 0;
	struct rte_bbdev_driver_info dev_info;
	struct rte_bbdev *dev = get_dev(dev_id);
	const struct rte_bbdev_op_cap *p;
	struct rte_bbdev_queue_conf *stored_conf;
	const char *op_type_str;
	unsigned int max_priority;
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	VALID_QUEUE_OR_RET_ERR(queue_id, dev);

	if (dev->data->queues[queue_id].started || dev->data->started) {
		rte_bbdev_log(ERR,
				"Queue %u of device %u cannot be configured when started",
				queue_id, dev_id);
		return -EBUSY;
	}

	VALID_FUNC_OR_RET_ERR(dev->dev_ops->queue_release, dev_id);
	VALID_FUNC_OR_RET_ERR(dev->dev_ops->queue_setup, dev_id);

	/* Get device driver information to verify config is valid */
	VALID_FUNC_OR_RET_ERR(dev->dev_ops->info_get, dev_id);
	memset(&dev_info, 0, sizeof(dev_info));
	dev->dev_ops->info_get(dev, &dev_info);

	/* Check configuration is valid */
	if (conf != NULL) {
		if ((conf->op_type == RTE_BBDEV_OP_NONE) &&
				(dev_info.capabilities[0].type ==
				RTE_BBDEV_OP_NONE)) {
			ret = 1;
		} else {
			for (p = dev_info.capabilities;
					p->type != RTE_BBDEV_OP_NONE; p++) {
				if (conf->op_type == p->type) {
					ret = 1;
					break;
				}
			}
		}
		if (ret == 0) {
			rte_bbdev_log(ERR, "Invalid operation type");
			return -EINVAL;
		}
		if (conf->queue_size > dev_info.queue_size_lim) {
			rte_bbdev_log(ERR,
					"Size (%u) of queue %u of device %u must be: <= %u",
					conf->queue_size, queue_id, dev_id,
					dev_info.queue_size_lim);
			return -EINVAL;
		}
		if (!rte_is_power_of_2(conf->queue_size)) {
			rte_bbdev_log(ERR,
					"Size (%u) of queue %u of device %u must be a power of 2",
					conf->queue_size, queue_id, dev_id);
			return -EINVAL;
		}
		if ((uint8_t)conf->op_type >= RTE_BBDEV_OP_TYPE_SIZE_MAX) {
			rte_bbdev_log(ERR,
					"Invalid operation type (%u) ", conf->op_type);
			return -EINVAL;
		}
		max_priority = dev_info.queue_priority[conf->op_type];
		if (conf->priority > max_priority) {
			rte_bbdev_log(ERR,
					"Priority (%u) of queue %u of bbdev %u must be <= %u",
					conf->priority, queue_id, dev_id, max_priority);
			return -EINVAL;
		}
	}

	/* Release existing queue (in case of queue reconfiguration) */
	if (dev->data->queues[queue_id].queue_private != NULL) {
		ret = dev->dev_ops->queue_release(dev, queue_id);
		if (ret < 0) {
			rte_bbdev_log(ERR, "Device %u queue %u release failed",
					dev_id, queue_id);
			return ret;
		}
	}

	/* Get driver to setup the queue */
	ret = dev->dev_ops->queue_setup(dev, queue_id, (conf != NULL) ?
			conf : &dev_info.default_queue_conf);
	if (ret < 0) {
		/* This may happen when trying different priority levels */
		rte_bbdev_log(INFO,
				"Device %u queue %u setup failed",
				dev_id, queue_id);
		return ret;
	}

	/* Store configuration */
	stored_conf = &dev->data->queues[queue_id].conf;
	memcpy(stored_conf,
			(conf != NULL) ? conf : &dev_info.default_queue_conf,
			sizeof(*stored_conf));

	op_type_str = rte_bbdev_op_type_str(stored_conf->op_type);
	if (op_type_str == NULL)
		return -EINVAL;

	rte_bbdev_log_debug("Configured dev%uq%u (size=%u, type=%s, prio=%u)",
			dev_id, queue_id, stored_conf->queue_size, op_type_str,
			stored_conf->priority);

	return 0;
}

int
rte_bbdev_start(uint16_t dev_id)
{
	int i;
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	if (dev->data->started) {
		rte_bbdev_log_debug("Device %u is already started", dev_id);
		return 0;
	}

	if (dev->dev_ops->start) {
		int ret = dev->dev_ops->start(dev);
		if (ret < 0) {
			rte_bbdev_log(ERR, "Device %u start failed", dev_id);
			return ret;
		}
	}

	/* Store new state */
	for (i = 0; i < dev->data->num_queues; i++)
		if (!dev->data->queues[i].conf.deferred_start)
			dev->data->queues[i].started = true;
	dev->data->started = true;

	rte_bbdev_log_debug("Started device %u", dev_id);
	return 0;
}

int
rte_bbdev_stop(uint16_t dev_id)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	if (!dev->data->started) {
		rte_bbdev_log_debug("Device %u is already stopped", dev_id);
		return 0;
	}

	if (dev->dev_ops->stop)
		dev->dev_ops->stop(dev);
	dev->data->started = false;

	rte_bbdev_log_debug("Stopped device %u", dev_id);
	return 0;
}

int
rte_bbdev_close(uint16_t dev_id)
{
	int ret;
	uint16_t i;
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	if (dev->data->started) {
		ret = rte_bbdev_stop(dev_id);
		if (ret < 0) {
			rte_bbdev_log(ERR, "Device %u stop failed", dev_id);
			return ret;
		}
	}

	/* Free memory used by queues */
	for (i = 0; i < dev->data->num_queues; i++) {
		ret = dev->dev_ops->queue_release(dev, i);
		if (ret < 0) {
			rte_bbdev_log(ERR, "Device %u queue %u release failed",
					dev_id, i);
			return ret;
		}
	}
	rte_free(dev->data->queues);

	if (dev->dev_ops->close) {
		ret = dev->dev_ops->close(dev);
		if (ret < 0) {
			rte_bbdev_log(ERR, "Device %u close failed", dev_id);
			return ret;
		}
	}

	/* Clear configuration */
	dev->data->queues = NULL;
	dev->data->num_queues = 0;

	rte_bbdev_log_debug("Closed device %u", dev_id);
	return 0;
}

int
rte_bbdev_queue_start(uint16_t dev_id, uint16_t queue_id)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	VALID_QUEUE_OR_RET_ERR(queue_id, dev);

	if (dev->data->queues[queue_id].started) {
		rte_bbdev_log_debug("Queue %u of device %u already started",
				queue_id, dev_id);
		return 0;
	}

	if (dev->dev_ops->queue_start) {
		int ret = dev->dev_ops->queue_start(dev, queue_id);
		if (ret < 0) {
			rte_bbdev_log(ERR, "Device %u queue %u start failed",
					dev_id, queue_id);
			return ret;
		}
	}
	dev->data->queues[queue_id].started = true;

	rte_bbdev_log_debug("Started queue %u of device %u", queue_id, dev_id);
	return 0;
}

int
rte_bbdev_queue_stop(uint16_t dev_id, uint16_t queue_id)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	VALID_QUEUE_OR_RET_ERR(queue_id, dev);

	if (!dev->data->queues[queue_id].started) {
		rte_bbdev_log_debug("Queue %u of device %u already stopped",
				queue_id, dev_id);
		return 0;
	}

	if (dev->dev_ops->queue_stop) {
		int ret = dev->dev_ops->queue_stop(dev, queue_id);
		if (ret < 0) {
			rte_bbdev_log(ERR, "Device %u queue %u stop failed",
					dev_id, queue_id);
			return ret;
		}
	}
	dev->data->queues[queue_id].started = false;

	rte_bbdev_log_debug("Stopped queue %u of device %u", queue_id, dev_id);
	return 0;
}

/* Get device statistics */
static void
get_stats_from_queues(struct rte_bbdev *dev, struct rte_bbdev_stats *stats)
{
	unsigned int q_id;
	for (q_id = 0; q_id < dev->data->num_queues; q_id++) {
		struct rte_bbdev_stats *q_stats =
				&dev->data->queues[q_id].queue_stats;

		stats->enqueued_count += q_stats->enqueued_count;
		stats->dequeued_count += q_stats->dequeued_count;
		stats->enqueue_err_count += q_stats->enqueue_err_count;
		stats->dequeue_err_count += q_stats->dequeue_err_count;
		stats->enqueue_warn_count += q_stats->enqueue_warn_count;
		stats->dequeue_warn_count += q_stats->dequeue_warn_count;
	}
	rte_bbdev_log_debug("Got stats on %u", dev->data->dev_id);
}

static void
reset_stats_in_queues(struct rte_bbdev *dev)
{
	unsigned int q_id;
	for (q_id = 0; q_id < dev->data->num_queues; q_id++) {
		struct rte_bbdev_stats *q_stats =
				&dev->data->queues[q_id].queue_stats;

		memset(q_stats, 0, sizeof(*q_stats));
	}
	rte_bbdev_log_debug("Reset stats on %u", dev->data->dev_id);
}

int
rte_bbdev_stats_get(uint16_t dev_id, struct rte_bbdev_stats *stats)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	if (stats == NULL) {
		rte_bbdev_log(ERR, "NULL stats structure");
		return -EINVAL;
	}

	memset(stats, 0, sizeof(*stats));
	if (dev->dev_ops->stats_get != NULL)
		dev->dev_ops->stats_get(dev, stats);
	else
		get_stats_from_queues(dev, stats);

	rte_bbdev_log_debug("Retrieved stats of device %u", dev_id);
	return 0;
}

int
rte_bbdev_stats_reset(uint16_t dev_id)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);

	if (dev->dev_ops->stats_reset != NULL)
		dev->dev_ops->stats_reset(dev);
	else
		reset_stats_in_queues(dev);

	rte_bbdev_log_debug("Reset stats of device %u", dev_id);
	return 0;
}

int
rte_bbdev_info_get(uint16_t dev_id, struct rte_bbdev_info *dev_info)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_FUNC_OR_RET_ERR(dev->dev_ops->info_get, dev_id);

	if (dev_info == NULL) {
		rte_bbdev_log(ERR, "NULL dev info structure");
		return -EINVAL;
	}

	/* Copy data maintained by device interface layer */
	memset(dev_info, 0, sizeof(*dev_info));
	dev_info->dev_name = dev->data->name;
	dev_info->num_queues = dev->data->num_queues;
	dev_info->device = dev->device;
	dev_info->socket_id = dev->data->socket_id;
	dev_info->started = dev->data->started;

	/* Copy data maintained by device driver layer */
	dev->dev_ops->info_get(dev, &dev_info->drv);

	rte_bbdev_log_debug("Retrieved info of device %u", dev_id);
	return 0;
}

int
rte_bbdev_queue_info_get(uint16_t dev_id, uint16_t queue_id,
		struct rte_bbdev_queue_info *queue_info)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	VALID_QUEUE_OR_RET_ERR(queue_id, dev);

	if (queue_info == NULL) {
		rte_bbdev_log(ERR, "NULL queue info structure");
		return -EINVAL;
	}

	/* Copy data to output */
	memset(queue_info, 0, sizeof(*queue_info));
	queue_info->conf = dev->data->queues[queue_id].conf;
	queue_info->started = dev->data->queues[queue_id].started;

	rte_bbdev_log_debug("Retrieved info of queue %u of device %u",
			queue_id, dev_id);
	return 0;
}

/* Calculate size needed to store bbdev_op, depending on type */
static unsigned int
get_bbdev_op_size(enum rte_bbdev_op_type type)
{
	unsigned int result = 0;
	switch (type) {
	case RTE_BBDEV_OP_NONE:
		result = RTE_MAX(sizeof(struct rte_bbdev_dec_op),
				sizeof(struct rte_bbdev_enc_op));
		break;
	case RTE_BBDEV_OP_TURBO_DEC:
		result = sizeof(struct rte_bbdev_dec_op);
		break;
	case RTE_BBDEV_OP_TURBO_ENC:
		result = sizeof(struct rte_bbdev_enc_op);
		break;
	case RTE_BBDEV_OP_LDPC_DEC:
		result = sizeof(struct rte_bbdev_dec_op);
		break;
	case RTE_BBDEV_OP_LDPC_ENC:
		result = sizeof(struct rte_bbdev_enc_op);
		break;
	case RTE_BBDEV_OP_FFT:
		result = sizeof(struct rte_bbdev_fft_op);
		break;
	case RTE_BBDEV_OP_MLDTS:
		result = sizeof(struct rte_bbdev_mldts_op);
		break;
	default:
		break;
	}

	return result;
}

/* Initialise a bbdev_op structure */
static void
bbdev_op_init(struct rte_mempool *mempool, void *arg, void *element,
		__rte_unused unsigned int n)
{
	enum rte_bbdev_op_type type = *(enum rte_bbdev_op_type *)arg;

	if (type == RTE_BBDEV_OP_TURBO_DEC || type == RTE_BBDEV_OP_LDPC_DEC) {
		struct rte_bbdev_dec_op *op = element;
		memset(op, 0, mempool->elt_size);
		op->mempool = mempool;
	} else if (type == RTE_BBDEV_OP_TURBO_ENC ||
			type == RTE_BBDEV_OP_LDPC_ENC) {
		struct rte_bbdev_enc_op *op = element;
		memset(op, 0, mempool->elt_size);
		op->mempool = mempool;
	} else if (type == RTE_BBDEV_OP_FFT) {
		struct rte_bbdev_fft_op *op = element;
		memset(op, 0, mempool->elt_size);
		op->mempool = mempool;
	} else if (type == RTE_BBDEV_OP_MLDTS) {
		struct rte_bbdev_mldts_op *op = element;
		memset(op, 0, mempool->elt_size);
		op->mempool = mempool;
	}
}

struct rte_mempool *
rte_bbdev_op_pool_create(const char *name, enum rte_bbdev_op_type type,
		unsigned int num_elements, unsigned int cache_size,
		int socket_id)
{
	struct rte_bbdev_op_pool_private *priv;
	struct rte_mempool *mp;
	const char *op_type_str;

	if (name == NULL) {
		rte_bbdev_log(ERR, "NULL name for op pool");
		return NULL;
	}

	if (type >= BBDEV_OP_TYPE_COUNT) {
		rte_bbdev_log(ERR,
				"Invalid op type (%u), should be less than %u",
				type, BBDEV_OP_TYPE_COUNT);
		return NULL;
	}

	mp = rte_mempool_create(name, num_elements, get_bbdev_op_size(type),
			cache_size, sizeof(struct rte_bbdev_op_pool_private),
			NULL, NULL, bbdev_op_init, &type, socket_id, 0);
	if (mp == NULL) {
		rte_bbdev_log(ERR,
				"Failed to create op pool %s (num ops=%u, op size=%u) with error: %s",
				name, num_elements, get_bbdev_op_size(type),
				rte_strerror(rte_errno));
		return NULL;
	}

	op_type_str = rte_bbdev_op_type_str(type);
	if (op_type_str == NULL)
		return NULL;

	rte_bbdev_log_debug(
			"Op pool %s created for %u ops (type=%s, cache=%u, socket=%u, size=%u)",
			name, num_elements, op_type_str, cache_size, socket_id,
			get_bbdev_op_size(type));

	priv = (struct rte_bbdev_op_pool_private *)rte_mempool_get_priv(mp);
	priv->type = type;

	return mp;
}

int
rte_bbdev_callback_register(uint16_t dev_id, enum rte_bbdev_event_type event,
		rte_bbdev_cb_fn cb_fn, void *cb_arg)
{
	struct rte_bbdev_callback *user_cb;
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	if (event >= RTE_BBDEV_EVENT_MAX) {
		rte_bbdev_log(ERR,
				"Invalid event type (%u), should be less than %u",
				event, RTE_BBDEV_EVENT_MAX);
		return -EINVAL;
	}

	if (cb_fn == NULL) {
		rte_bbdev_log(ERR, "NULL callback function");
		return -EINVAL;
	}

	rte_spinlock_lock(&rte_bbdev_cb_lock);

	TAILQ_FOREACH(user_cb, &(dev->list_cbs), next) {
		if (user_cb->cb_fn == cb_fn &&
				user_cb->cb_arg == cb_arg &&
				user_cb->event == event)
			break;
	}

	/* create a new callback. */
	if (user_cb == NULL) {
		user_cb = rte_zmalloc("INTR_USER_CALLBACK",
				sizeof(struct rte_bbdev_callback), 0);
		if (user_cb != NULL) {
			user_cb->cb_fn = cb_fn;
			user_cb->cb_arg = cb_arg;
			user_cb->event = event;
			TAILQ_INSERT_TAIL(&(dev->list_cbs), user_cb, next);
		}
	}

	rte_spinlock_unlock(&rte_bbdev_cb_lock);
	return (user_cb == NULL) ? -ENOMEM : 0;
}

int
rte_bbdev_callback_unregister(uint16_t dev_id, enum rte_bbdev_event_type event,
		rte_bbdev_cb_fn cb_fn, void *cb_arg)
{
	int ret = 0;
	struct rte_bbdev_callback *cb, *next;
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);

	if (event >= RTE_BBDEV_EVENT_MAX) {
		rte_bbdev_log(ERR,
				"Invalid event type (%u), should be less than %u",
				event, RTE_BBDEV_EVENT_MAX);
		return -EINVAL;
	}

	if (cb_fn == NULL) {
		rte_bbdev_log(ERR,
				"NULL callback function cannot be unregistered");
		return -EINVAL;
	}

	dev = &rte_bbdev_devices[dev_id];
	rte_spinlock_lock(&rte_bbdev_cb_lock);

	for (cb = TAILQ_FIRST(&dev->list_cbs); cb != NULL; cb = next) {

		next = TAILQ_NEXT(cb, next);

		if (cb->cb_fn != cb_fn || cb->event != event ||
				(cb_arg != (void *)-1 && cb->cb_arg != cb_arg))
			continue;

		/* If this callback is not executing right now, remove it. */
		if (cb->active == 0) {
			TAILQ_REMOVE(&(dev->list_cbs), cb, next);
			rte_free(cb);
		} else
			ret = -EAGAIN;
	}

	rte_spinlock_unlock(&rte_bbdev_cb_lock);
	return ret;
}

void
rte_bbdev_pmd_callback_process(struct rte_bbdev *dev,
	enum rte_bbdev_event_type event, void *ret_param)
{
	struct rte_bbdev_callback *cb_lst;
	struct rte_bbdev_callback dev_cb;

	if (dev == NULL) {
		rte_bbdev_log(ERR, "NULL device");
		return;
	}

	if (dev->data == NULL) {
		rte_bbdev_log(ERR, "NULL data structure");
		return;
	}

	if (event >= RTE_BBDEV_EVENT_MAX) {
		rte_bbdev_log(ERR,
				"Invalid event type (%u), should be less than %u",
				event, RTE_BBDEV_EVENT_MAX);
		return;
	}

	rte_spinlock_lock(&rte_bbdev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->list_cbs), next) {
		if (cb_lst->cb_fn == NULL || cb_lst->event != event)
			continue;
		dev_cb = *cb_lst;
		cb_lst->active = 1;
		if (ret_param != NULL)
			dev_cb.ret_param = ret_param;

		rte_spinlock_unlock(&rte_bbdev_cb_lock);
		dev_cb.cb_fn(dev->data->dev_id, dev_cb.event,
				dev_cb.cb_arg, dev_cb.ret_param);
		rte_spinlock_lock(&rte_bbdev_cb_lock);
		cb_lst->active = 0;
	}
	rte_spinlock_unlock(&rte_bbdev_cb_lock);
}

int
rte_bbdev_queue_intr_enable(uint16_t dev_id, uint16_t queue_id)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);
	VALID_QUEUE_OR_RET_ERR(queue_id, dev);
	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);
	VALID_FUNC_OR_RET_ERR(dev->dev_ops->queue_intr_enable, dev_id);
	return dev->dev_ops->queue_intr_enable(dev, queue_id);
}

int
rte_bbdev_queue_intr_disable(uint16_t dev_id, uint16_t queue_id)
{
	struct rte_bbdev *dev = get_dev(dev_id);
	VALID_DEV_OR_RET_ERR(dev, dev_id);
	VALID_QUEUE_OR_RET_ERR(queue_id, dev);
	VALID_DEV_OPS_OR_RET_ERR(dev, dev_id);
	VALID_FUNC_OR_RET_ERR(dev->dev_ops->queue_intr_disable, dev_id);
	return dev->dev_ops->queue_intr_disable(dev, queue_id);
}

int
rte_bbdev_queue_intr_ctl(uint16_t dev_id, uint16_t queue_id, int epfd, int op,
		void *data)
{
	uint32_t vec;
	struct rte_bbdev *dev = get_dev(dev_id);
	struct rte_intr_handle *intr_handle;
	int ret;

	VALID_DEV_OR_RET_ERR(dev, dev_id);
	VALID_QUEUE_OR_RET_ERR(queue_id, dev);

	intr_handle = dev->intr_handle;
	if (intr_handle == NULL) {
		rte_bbdev_log(ERR, "Device %u intr handle unset\n", dev_id);
		return -ENOTSUP;
	}

	if (queue_id >= RTE_MAX_RXTX_INTR_VEC_ID) {
		rte_bbdev_log(ERR, "Device %u queue_id %u is too big\n",
				dev_id, queue_id);
		return -ENOTSUP;
	}

	vec = rte_intr_vec_list_index_get(intr_handle, queue_id);
	ret = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);
	if (ret && (ret != -EEXIST)) {
		rte_bbdev_log(ERR,
				"dev %u q %u int ctl error op %d epfd %d vec %u\n",
				dev_id, queue_id, op, epfd, vec);
		return ret;
	}

	return 0;
}


const char *
rte_bbdev_op_type_str(enum rte_bbdev_op_type op_type)
{
	static const char * const op_types[] = {
		"RTE_BBDEV_OP_NONE",
		"RTE_BBDEV_OP_TURBO_DEC",
		"RTE_BBDEV_OP_TURBO_ENC",
		"RTE_BBDEV_OP_LDPC_DEC",
		"RTE_BBDEV_OP_LDPC_ENC",
		"RTE_BBDEV_OP_FFT",
		"RTE_BBDEV_OP_MLDTS",
	};

	if (op_type < BBDEV_OP_TYPE_COUNT)
		return op_types[op_type];

	rte_bbdev_log(ERR, "Invalid operation type");
	return NULL;
}

const char *
rte_bbdev_device_status_str(enum rte_bbdev_device_status status)
{
	static const char * const dev_sta_string[] = {
		"RTE_BBDEV_DEV_NOSTATUS",
		"RTE_BBDEV_DEV_NOT_SUPPORTED",
		"RTE_BBDEV_DEV_RESET",
		"RTE_BBDEV_DEV_CONFIGURED",
		"RTE_BBDEV_DEV_ACTIVE",
		"RTE_BBDEV_DEV_FATAL_ERR",
		"RTE_BBDEV_DEV_RESTART_REQ",
		"RTE_BBDEV_DEV_RECONFIG_REQ",
		"RTE_BBDEV_DEV_CORRECT_ERR",
	};

	/* Cast from enum required for clang. */
	if ((uint8_t)status < sizeof(dev_sta_string) / sizeof(char *))
		return dev_sta_string[status];

	rte_bbdev_log(ERR, "Invalid device status");
	return NULL;
}

const char *
rte_bbdev_enqueue_status_str(enum rte_bbdev_enqueue_status status)
{
	static const char * const enq_sta_string[] = {
		"RTE_BBDEV_ENQ_STATUS_NONE",
		"RTE_BBDEV_ENQ_STATUS_QUEUE_FULL",
		"RTE_BBDEV_ENQ_STATUS_RING_FULL",
		"RTE_BBDEV_ENQ_STATUS_INVALID_OP",
	};

	/* Cast from enum required for clang. */
	if ((uint8_t)status < sizeof(enq_sta_string) / sizeof(char *))
		return enq_sta_string[status];

	rte_bbdev_log(ERR, "Invalid enqueue status");
	return NULL;
}
