/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <dev_driver.h>
#include <rte_eal.h>
#include <rte_memzone.h>

#include "rte_compressdev.h"
#include "rte_compressdev_internal.h"
#include "rte_compressdev_pmd.h"

#define RTE_COMPRESSDEV_DETACHED  (0)
#define RTE_COMPRESSDEV_ATTACHED  (1)

static struct rte_compressdev rte_comp_devices[RTE_COMPRESS_MAX_DEVS];

static struct rte_compressdev_global compressdev_globals = {
		.devs			= rte_comp_devices,
		.data			= { NULL },
		.nb_devs		= 0,
		.max_devs		= RTE_COMPRESS_MAX_DEVS
};

const struct rte_compressdev_capabilities *
rte_compressdev_capability_get(uint8_t dev_id,
			enum rte_comp_algorithm algo)
{
	const struct rte_compressdev_capabilities *capability;
	struct rte_compressdev_info dev_info;
	int i = 0;

	if (dev_id >= compressdev_globals.nb_devs) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%d", dev_id);
		return NULL;
	}
	rte_compressdev_info_get(dev_id, &dev_info);

	while ((capability = &dev_info.capabilities[i++])->algo !=
			RTE_COMP_ALGO_UNSPECIFIED){
		if (capability->algo == algo)
			return capability;
	}

	return NULL;
}

const char *
rte_compressdev_get_feature_name(uint64_t flag)
{
	switch (flag) {
	case RTE_COMPDEV_FF_HW_ACCELERATED:
		return "HW_ACCELERATED";
	case RTE_COMPDEV_FF_CPU_SSE:
		return "CPU_SSE";
	case RTE_COMPDEV_FF_CPU_AVX:
		return "CPU_AVX";
	case RTE_COMPDEV_FF_CPU_AVX2:
		return "CPU_AVX2";
	case RTE_COMPDEV_FF_CPU_AVX512:
		return "CPU_AVX512";
	case RTE_COMPDEV_FF_CPU_NEON:
		return "CPU_NEON";
	case RTE_COMPDEV_FF_OP_DONE_IN_DEQUEUE:
		return "OP_DONE_IN_DEQ";
	default:
		return NULL;
	}
}

static struct rte_compressdev *
rte_compressdev_get_dev(uint8_t dev_id)
{
	return &compressdev_globals.devs[dev_id];
}

struct rte_compressdev *
rte_compressdev_pmd_get_named_dev(const char *name)
{
	struct rte_compressdev *dev;
	unsigned int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < compressdev_globals.max_devs; i++) {
		dev = &compressdev_globals.devs[i];

		if ((dev->attached == RTE_COMPRESSDEV_ATTACHED) &&
				(strcmp(dev->data->name, name) == 0))
			return dev;
	}

	return NULL;
}

static unsigned int
rte_compressdev_is_valid_dev(uint8_t dev_id)
{
	struct rte_compressdev *dev = NULL;

	if (dev_id >= compressdev_globals.nb_devs)
		return 0;

	dev = rte_compressdev_get_dev(dev_id);
	if (dev->attached != RTE_COMPRESSDEV_ATTACHED)
		return 0;
	else
		return 1;
}


int
rte_compressdev_get_dev_id(const char *name)
{
	unsigned int i;

	if (name == NULL)
		return -1;

	for (i = 0; i < compressdev_globals.nb_devs; i++)
		if ((strcmp(compressdev_globals.devs[i].data->name, name)
				== 0) &&
				(compressdev_globals.devs[i].attached ==
						RTE_COMPRESSDEV_ATTACHED))
			return i;

	return -1;
}

uint8_t
rte_compressdev_count(void)
{
	return compressdev_globals.nb_devs;
}

uint8_t
rte_compressdev_devices_get(const char *driver_name, uint8_t *devices,
	uint8_t nb_devices)
{
	uint8_t i, count = 0;
	struct rte_compressdev *devs = compressdev_globals.devs;
	uint8_t max_devs = compressdev_globals.max_devs;

	for (i = 0; i < max_devs && count < nb_devices;	i++) {

		if (devs[i].attached == RTE_COMPRESSDEV_ATTACHED) {
			int cmp;

			cmp = strncmp(devs[i].device->driver->name,
					driver_name,
					strlen(driver_name));

			if (cmp == 0)
				devices[count++] = devs[i].data->dev_id;
		}
	}

	return count;
}

int
rte_compressdev_socket_id(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id))
		return -1;

	dev = rte_compressdev_get_dev(dev_id);

	return dev->data->socket_id;
}

static inline int
rte_compressdev_data_alloc(uint8_t dev_id, struct rte_compressdev_data **data,
		int socket_id)
{
	char mz_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	int n;

	/* generate memzone name */
	n = snprintf(mz_name, sizeof(mz_name),
			"rte_compressdev_data_%u", dev_id);
	if (n >= (int)sizeof(mz_name))
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(mz_name,
				sizeof(struct rte_compressdev_data),
				socket_id, 0);
	} else
		mz = rte_memzone_lookup(mz_name);

	if (mz == NULL)
		return -ENOMEM;

	*data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(*data, 0, sizeof(struct rte_compressdev_data));

	return 0;
}

static uint8_t
rte_compressdev_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_COMPRESS_MAX_DEVS; dev_id++) {
		if (rte_comp_devices[dev_id].attached ==
				RTE_COMPRESSDEV_DETACHED)
			return dev_id;
	}
	return RTE_COMPRESS_MAX_DEVS;
}

struct rte_compressdev *
rte_compressdev_pmd_allocate(const char *name, int socket_id)
{
	struct rte_compressdev *compressdev;
	uint8_t dev_id;

	if (rte_compressdev_pmd_get_named_dev(name) != NULL) {
		COMPRESSDEV_LOG(ERR,
			"comp device with name %s already allocated!", name);
		return NULL;
	}

	dev_id = rte_compressdev_find_free_device_index();
	if (dev_id == RTE_COMPRESS_MAX_DEVS) {
		COMPRESSDEV_LOG(ERR, "Reached maximum number of comp devices");
		return NULL;
	}
	compressdev = rte_compressdev_get_dev(dev_id);

	if (compressdev->data == NULL) {
		struct rte_compressdev_data *compressdev_data =
				compressdev_globals.data[dev_id];

		int retval = rte_compressdev_data_alloc(dev_id,
				&compressdev_data, socket_id);

		if (retval < 0 || compressdev_data == NULL)
			return NULL;

		compressdev->data = compressdev_data;

		strlcpy(compressdev->data->name, name,
			RTE_COMPRESSDEV_NAME_MAX_LEN);

		compressdev->data->dev_id = dev_id;
		compressdev->data->socket_id = socket_id;
		compressdev->data->dev_started = 0;

		compressdev->attached = RTE_COMPRESSDEV_ATTACHED;

		compressdev_globals.nb_devs++;
	}

	return compressdev;
}

int
rte_compressdev_pmd_release_device(struct rte_compressdev *compressdev)
{
	int ret;

	if (compressdev == NULL)
		return -EINVAL;

	/* Close device only if device operations have been set */
	if (compressdev->dev_ops) {
		ret = rte_compressdev_close(compressdev->data->dev_id);
		if (ret < 0)
			return ret;
	}

	compressdev->attached = RTE_COMPRESSDEV_DETACHED;
	compressdev_globals.nb_devs--;
	return 0;
}

uint16_t
rte_compressdev_queue_pair_count(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	dev = &rte_comp_devices[dev_id];
	return dev->data->nb_queue_pairs;
}

static int
rte_compressdev_queue_pairs_config(struct rte_compressdev *dev,
		uint16_t nb_qpairs, int socket_id)
{
	struct rte_compressdev_info dev_info;
	void **qp;
	unsigned int i;

	if ((dev == NULL) || (nb_qpairs < 1)) {
		COMPRESSDEV_LOG(ERR, "invalid param: dev %p, nb_queues %u",
							dev, nb_qpairs);
		return -EINVAL;
	}

	COMPRESSDEV_LOG(DEBUG, "Setup %d queues pairs on device %u",
			nb_qpairs, dev->data->dev_id);

	memset(&dev_info, 0, sizeof(struct rte_compressdev_info));

	if (*dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);

	if ((dev_info.max_nb_queue_pairs != 0) &&
			(nb_qpairs > dev_info.max_nb_queue_pairs)) {
		COMPRESSDEV_LOG(ERR, "Invalid num queue_pairs (%u) for dev %u",
				nb_qpairs, dev->data->dev_id);
		return -EINVAL;
	}

	if (dev->data->queue_pairs == NULL) { /* first time configuration */
		dev->data->queue_pairs = rte_zmalloc_socket(
				"compressdev->queue_pairs",
				sizeof(dev->data->queue_pairs[0]) * nb_qpairs,
				RTE_CACHE_LINE_SIZE, socket_id);

		if (dev->data->queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			COMPRESSDEV_LOG(ERR,
			"failed to get memory for qp meta data, nb_queues %u",
							nb_qpairs);
			return -(ENOMEM);
		}
	} else { /* re-configure */
		int ret;
		uint16_t old_nb_queues = dev->data->nb_queue_pairs;

		qp = dev->data->queue_pairs;

		if (*dev->dev_ops->queue_pair_release == NULL)
			return -ENOTSUP;

		for (i = nb_qpairs; i < old_nb_queues; i++) {
			ret = (*dev->dev_ops->queue_pair_release)(dev, i);
			if (ret < 0)
				return ret;
		}

		qp = rte_realloc(qp, sizeof(qp[0]) * nb_qpairs,
				RTE_CACHE_LINE_SIZE);
		if (qp == NULL) {
			COMPRESSDEV_LOG(ERR,
			"failed to realloc qp meta data, nb_queues %u",
						nb_qpairs);
			return -(ENOMEM);
		}

		if (nb_qpairs > old_nb_queues) {
			uint16_t new_qs = nb_qpairs - old_nb_queues;

			memset(qp + old_nb_queues, 0,
				sizeof(qp[0]) * new_qs);
		}

		dev->data->queue_pairs = qp;

	}
	dev->data->nb_queue_pairs = nb_qpairs;
	return 0;
}

static int
rte_compressdev_queue_pairs_release(struct rte_compressdev *dev)
{
	uint16_t num_qps, i;
	int ret;

	if (dev == NULL) {
		COMPRESSDEV_LOG(ERR, "invalid param: dev %p", dev);
		return -EINVAL;
	}

	num_qps = dev->data->nb_queue_pairs;

	if (num_qps == 0)
		return 0;

	COMPRESSDEV_LOG(DEBUG, "Free %d queues pairs on device %u",
			dev->data->nb_queue_pairs, dev->data->dev_id);

	if (*dev->dev_ops->queue_pair_release == NULL)
		return -ENOTSUP;

	for (i = 0; i < num_qps; i++) {
		ret = (*dev->dev_ops->queue_pair_release)(dev, i);
		if (ret < 0)
			return ret;
	}

	rte_free(dev->data->queue_pairs);
	dev->data->queue_pairs = NULL;
	dev->data->nb_queue_pairs = 0;

	return 0;
}

int
rte_compressdev_configure(uint8_t dev_id, struct rte_compressdev_config *config)
{
	struct rte_compressdev *dev;
	int diag;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];

	if (dev->data->dev_started) {
		COMPRESSDEV_LOG(ERR,
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	if (*dev->dev_ops->dev_configure == NULL)
		return -ENOTSUP;

	/* Setup new number of queue pairs and reconfigure device. */
	diag = rte_compressdev_queue_pairs_config(dev, config->nb_queue_pairs,
			config->socket_id);
	if (diag != 0) {
		COMPRESSDEV_LOG(ERR,
			"dev%d rte_comp_dev_queue_pairs_config = %d",
				dev_id, diag);
		return diag;
	}

	return (*dev->dev_ops->dev_configure)(dev, config);
}

int
rte_compressdev_start(uint8_t dev_id)
{
	struct rte_compressdev *dev;
	int diag;

	COMPRESSDEV_LOG(DEBUG, "Start dev_id=%" PRIu8, dev_id);

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];

	if (*dev->dev_ops->dev_start == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started != 0) {
		COMPRESSDEV_LOG(ERR,
		    "Device with dev_id=%" PRIu8 " already started", dev_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	return 0;
}

void
rte_compressdev_stop(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	if (*dev->dev_ops->dev_stop == NULL)
		return;

	if (dev->data->dev_started == 0) {
		COMPRESSDEV_LOG(ERR,
		    "Device with dev_id=%" PRIu8 " already stopped", dev_id);
		return;
	}

	(*dev->dev_ops->dev_stop)(dev);
	dev->data->dev_started = 0;
}

int
rte_compressdev_close(uint8_t dev_id)
{
	struct rte_compressdev *dev;
	int retval;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -1;
	}

	dev = &rte_comp_devices[dev_id];

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		COMPRESSDEV_LOG(ERR, "Device %u must be stopped before closing",
				dev_id);
		return -EBUSY;
	}

	/* Free queue pairs memory */
	retval = rte_compressdev_queue_pairs_release(dev);

	if (retval < 0)
		return retval;

	if (*dev->dev_ops->dev_close == NULL)
		return -ENOTSUP;
	retval = (*dev->dev_ops->dev_close)(dev);

	if (retval < 0)
		return retval;

	return 0;
}

int
rte_compressdev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
		uint32_t max_inflight_ops, int socket_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		COMPRESSDEV_LOG(ERR, "Invalid queue_pair_id=%d", queue_pair_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		COMPRESSDEV_LOG(ERR,
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	if (max_inflight_ops == 0) {
		COMPRESSDEV_LOG(ERR,
			"Invalid maximum number of inflight operations");
		return -EINVAL;
	}

	if (*dev->dev_ops->queue_pair_setup == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->queue_pair_setup)(dev, queue_pair_id,
			max_inflight_ops, socket_id);
}

uint16_t
rte_compressdev_dequeue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct rte_compressdev *dev = &rte_comp_devices[dev_id];

	nb_ops = (*dev->dequeue_burst)
			(dev->data->queue_pairs[qp_id], ops, nb_ops);

	return nb_ops;
}

uint16_t
rte_compressdev_enqueue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct rte_compressdev *dev = &rte_comp_devices[dev_id];

	return (*dev->enqueue_burst)(
			dev->data->queue_pairs[qp_id], ops, nb_ops);
}

int
rte_compressdev_stats_get(uint8_t dev_id, struct rte_compressdev_stats *stats)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%d", dev_id);
		return -ENODEV;
	}

	if (stats == NULL) {
		COMPRESSDEV_LOG(ERR, "Invalid stats ptr");
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];
	memset(stats, 0, sizeof(*stats));

	if (*dev->dev_ops->stats_get == NULL)
		return -ENOTSUP;
	(*dev->dev_ops->stats_get)(dev, stats);
	return 0;
}

void
rte_compressdev_stats_reset(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	if (*dev->dev_ops->stats_reset == NULL)
		return;
	(*dev->dev_ops->stats_reset)(dev);
}


void
rte_compressdev_info_get(uint8_t dev_id, struct rte_compressdev_info *dev_info)
{
	struct rte_compressdev *dev;

	if (dev_id >= compressdev_globals.nb_devs) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%d", dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	memset(dev_info, 0, sizeof(struct rte_compressdev_info));

	if (*dev->dev_ops->dev_infos_get == NULL)
		return;
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);

	dev_info->driver_name = dev->device->driver->name;
}

int
rte_compressdev_private_xform_create(uint8_t dev_id,
		const struct rte_comp_xform *xform,
		void **priv_xform)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_get_dev(dev_id);

	if (xform == NULL || priv_xform == NULL || dev == NULL)
		return -EINVAL;

	if (*dev->dev_ops->private_xform_create == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->private_xform_create)(dev, xform, priv_xform);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to create private_xform: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}

int
rte_compressdev_private_xform_free(uint8_t dev_id, void *priv_xform)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_get_dev(dev_id);

	if (dev == NULL || priv_xform == NULL)
		return -EINVAL;

	if (*dev->dev_ops->private_xform_free == NULL)
		return -ENOTSUP;
	ret = dev->dev_ops->private_xform_free(dev, priv_xform);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to free private xform: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}

int
rte_compressdev_stream_create(uint8_t dev_id,
		const struct rte_comp_xform *xform,
		void **stream)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_get_dev(dev_id);

	if (xform == NULL || dev == NULL || stream == NULL)
		return -EINVAL;

	if (*dev->dev_ops->stream_create == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->stream_create)(dev, xform, stream);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to create stream: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}


int
rte_compressdev_stream_free(uint8_t dev_id, void *stream)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_get_dev(dev_id);

	if (dev == NULL || stream == NULL)
		return -EINVAL;

	if (*dev->dev_ops->stream_free == NULL)
		return -ENOTSUP;
	ret = dev->dev_ops->stream_free(dev, stream);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to free stream: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}

const char *
rte_compressdev_name_get(uint8_t dev_id)
{
	struct rte_compressdev *dev = rte_compressdev_get_dev(dev_id);

	if (dev == NULL)
		return NULL;

	return dev->data->name;
}

RTE_LOG_REGISTER_DEFAULT(compressdev_logtype, NOTICE);
