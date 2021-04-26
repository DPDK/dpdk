/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_kvargs.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_lcore.h>
#include <rte_bus_vdev.h>

#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "skeleton_rawdev.h"

/* Count of instances */
static uint16_t skeldev_init_once;

/**< Rawdev Skeleton dummy driver name */
#define SKELETON_PMD_RAWDEV_NAME rawdev_skeleton

struct queue_buffers {
	void *bufs[SKELETON_QUEUE_MAX_DEPTH];
};

static struct queue_buffers queue_buf[SKELETON_MAX_QUEUES] = {};
static void clear_queue_bufs(int queue_id);

static int skeleton_rawdev_info_get(struct rte_rawdev *dev,
				     rte_rawdev_obj_t dev_info,
				     size_t dev_info_size)
{
	struct skeleton_rawdev *skeldev;
	struct skeleton_rawdev_conf *skeldev_conf;

	SKELETON_PMD_FUNC_TRACE();

	if (!dev_info || dev_info_size != sizeof(*skeldev_conf)) {
		SKELETON_PMD_ERR("Invalid request");
		return -EINVAL;
	}

	skeldev = skeleton_rawdev_get_priv(dev);

	skeldev_conf = dev_info;

	skeldev_conf->num_queues = skeldev->num_queues;
	skeldev_conf->capabilities = skeldev->capabilities;
	skeldev_conf->device_state = skeldev->device_state;
	skeldev_conf->firmware_state = skeldev->fw.firmware_state;

	return 0;
}

static int skeleton_rawdev_configure(const struct rte_rawdev *dev,
				     rte_rawdev_obj_t config,
				     size_t config_size)
{
	struct skeleton_rawdev *skeldev;
	struct skeleton_rawdev_conf *skeldev_conf;

	SKELETON_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	if (config == NULL || config_size != sizeof(*skeldev_conf)) {
		SKELETON_PMD_ERR("Invalid configuration");
		return -EINVAL;
	}

	skeldev_conf = config;
	skeldev = skeleton_rawdev_get_priv(dev);

	if (skeldev_conf->num_queues <= SKELETON_MAX_QUEUES)
		skeldev->num_queues = skeldev_conf->num_queues;
	else
		return -EINVAL;

	skeldev->capabilities = skeldev_conf->capabilities;
	skeldev->num_queues = skeldev_conf->num_queues;

	return 0;
}

static int skeleton_rawdev_start(struct rte_rawdev *dev)
{
	int ret = 0;
	struct skeleton_rawdev *skeldev;
	enum skeleton_firmware_state fw_state;
	enum skeleton_device_state device_state;

	SKELETON_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	skeldev = skeleton_rawdev_get_priv(dev);

	fw_state = skeldev->fw.firmware_state;
	device_state = skeldev->device_state;

	if (fw_state == SKELETON_FW_LOADED &&
		device_state == SKELETON_DEV_STOPPED) {
		skeldev->device_state = SKELETON_DEV_RUNNING;
	} else {
		SKELETON_PMD_ERR("Device not ready for starting");
		ret = -EINVAL;
	}

	return ret;
}

static void skeleton_rawdev_stop(struct rte_rawdev *dev)
{
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	if (dev) {
		skeldev = skeleton_rawdev_get_priv(dev);
		skeldev->device_state = SKELETON_DEV_STOPPED;
	}
}

static void
reset_queues(struct skeleton_rawdev *skeldev)
{
	int i;

	for (i = 0; i < SKELETON_MAX_QUEUES; i++) {
		skeldev->queues[i].depth = SKELETON_QUEUE_DEF_DEPTH;
		skeldev->queues[i].state = SKELETON_QUEUE_DETACH;
	}
}

static void
reset_attribute_table(struct skeleton_rawdev *skeldev)
{
	int i;

	for (i = 0; i < SKELETON_MAX_ATTRIBUTES; i++) {
		if (skeldev->attr[i].name) {
			free(skeldev->attr[i].name);
			skeldev->attr[i].name = NULL;
		}
	}
}

static int skeleton_rawdev_close(struct rte_rawdev *dev)
{
	int ret = 0, i;
	struct skeleton_rawdev *skeldev;
	enum skeleton_firmware_state fw_state;
	enum skeleton_device_state device_state;

	SKELETON_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	skeldev = skeleton_rawdev_get_priv(dev);

	fw_state = skeldev->fw.firmware_state;
	device_state = skeldev->device_state;

	reset_queues(skeldev);
	reset_attribute_table(skeldev);

	switch (fw_state) {
	case SKELETON_FW_LOADED:
		if (device_state == SKELETON_DEV_RUNNING) {
			SKELETON_PMD_ERR("Cannot close running device");
			ret = -EINVAL;
		} else {
			/* Probably call fw reset here */
			skeldev->fw.firmware_state = SKELETON_FW_READY;
		}
		break;
	case SKELETON_FW_READY:
		SKELETON_PMD_DEBUG("Device already in stopped state");
		break;
	case SKELETON_FW_ERROR:
	default:
		SKELETON_PMD_DEBUG("Device in impossible state");
		ret = -EINVAL;
		break;
	}

	/* Clear all allocated queues */
	for (i = 0; i < SKELETON_MAX_QUEUES; i++)
		clear_queue_bufs(i);

	return ret;
}

static int skeleton_rawdev_reset(struct rte_rawdev *dev)
{
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	skeldev = skeleton_rawdev_get_priv(dev);

	SKELETON_PMD_DEBUG("Resetting device");
	skeldev->fw.firmware_state = SKELETON_FW_READY;

	return 0;
}

static int skeleton_rawdev_queue_def_conf(struct rte_rawdev *dev,
					  uint16_t queue_id,
					  rte_rawdev_obj_t queue_conf,
					  size_t conf_size)
{
	struct skeleton_rawdev *skeldev;
	struct skeleton_rawdev_queue *skelq;

	SKELETON_PMD_FUNC_TRACE();

	if (!dev || !queue_conf ||
			conf_size != sizeof(struct skeleton_rawdev_queue))
		return -EINVAL;

	skeldev = skeleton_rawdev_get_priv(dev);
	skelq = &skeldev->queues[queue_id];

	if (queue_id < SKELETON_MAX_QUEUES)
		rte_memcpy(queue_conf, skelq,
			sizeof(struct skeleton_rawdev_queue));

	return 0;
}

static void
clear_queue_bufs(int queue_id)
{
	int i;

	/* Clear buffers for queue_id */
	for (i = 0; i < SKELETON_QUEUE_MAX_DEPTH; i++)
		queue_buf[queue_id].bufs[i] = NULL;
}

static int skeleton_rawdev_queue_setup(struct rte_rawdev *dev,
				       uint16_t queue_id,
				       rte_rawdev_obj_t queue_conf,
				       size_t conf_size)
{
	int ret = 0;
	struct skeleton_rawdev *skeldev;
	struct skeleton_rawdev_queue *q;

	SKELETON_PMD_FUNC_TRACE();

	if (!dev || !queue_conf ||
			conf_size != sizeof(struct skeleton_rawdev_queue))
		return -EINVAL;

	skeldev = skeleton_rawdev_get_priv(dev);
	q = &skeldev->queues[queue_id];

	if (skeldev->num_queues > queue_id &&
	    q->depth < SKELETON_QUEUE_MAX_DEPTH) {
		rte_memcpy(q, queue_conf,
			   sizeof(struct skeleton_rawdev_queue));
		clear_queue_bufs(queue_id);
	} else {
		SKELETON_PMD_ERR("Invalid queue configuration");
		ret = -EINVAL;
	}

	return ret;
}

static int skeleton_rawdev_queue_release(struct rte_rawdev *dev,
					 uint16_t queue_id)
{
	int ret = 0;
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	skeldev = skeleton_rawdev_get_priv(dev);

	if (skeldev->num_queues > queue_id) {
		skeldev->queues[queue_id].state = SKELETON_QUEUE_DETACH;
		skeldev->queues[queue_id].depth = SKELETON_QUEUE_DEF_DEPTH;
		clear_queue_bufs(queue_id);
	} else {
		SKELETON_PMD_ERR("Invalid queue configuration");
		ret = -EINVAL;
	}

	return ret;
}

static uint16_t skeleton_rawdev_queue_count(struct rte_rawdev *dev)
{
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	skeldev = skeleton_rawdev_get_priv(dev);
	return skeldev->num_queues;
}

static int skeleton_rawdev_get_attr(struct rte_rawdev *dev,
				    const char *attr_name,
				    uint64_t *attr_value)
{
	int i;
	uint8_t done = 0;
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	if (!dev || !attr_name || !attr_value) {
		SKELETON_PMD_ERR("Invalid arguments for getting attributes");
		return -EINVAL;
	}

	skeldev = skeleton_rawdev_get_priv(dev);

	for (i = 0; i < SKELETON_MAX_ATTRIBUTES; i++) {
		if (!skeldev->attr[i].name)
			continue;

		if (!strncmp(skeldev->attr[i].name, attr_name,
			    SKELETON_ATTRIBUTE_NAME_MAX)) {
			*attr_value = skeldev->attr[i].value;
			done = 1;
			SKELETON_PMD_DEBUG("Attribute (%s) Value (%" PRIu64 ")",
					   attr_name, *attr_value);
			break;
		}
	}

	if (done)
		return 0;

	/* Attribute not found */
	return -EINVAL;
}

static int skeleton_rawdev_set_attr(struct rte_rawdev *dev,
				     const char *attr_name,
				     const uint64_t attr_value)
{
	int i;
	uint8_t done = 0;
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	if (!dev || !attr_name) {
		SKELETON_PMD_ERR("Invalid arguments for setting attributes");
		return -EINVAL;
	}

	skeldev = skeleton_rawdev_get_priv(dev);

	/* Check if attribute already exists */
	for (i = 0; i < SKELETON_MAX_ATTRIBUTES; i++) {
		if (!skeldev->attr[i].name)
			break;

		if (!strncmp(skeldev->attr[i].name, attr_name,
			     SKELETON_ATTRIBUTE_NAME_MAX)) {
			/* Update value */
			skeldev->attr[i].value = attr_value;
			done = 1;
			break;
		}
	}

	if (!done) {
		if (i < (SKELETON_MAX_ATTRIBUTES - 1)) {
			/* There is still space to insert one more */
			skeldev->attr[i].name = strdup(attr_name);
			if (!skeldev->attr[i].name)
				return -ENOMEM;

			skeldev->attr[i].value = attr_value;
			return 0;
		}
	}

	return -EINVAL;
}

static int skeleton_rawdev_enqueue_bufs(struct rte_rawdev *dev,
					struct rte_rawdev_buf **buffers,
					unsigned int count,
					rte_rawdev_obj_t context)
{
	unsigned int i;
	uint16_t q_id;
	RTE_SET_USED(dev);

	/* context is essentially the queue_id which is
	 * transferred as opaque object through the library layer. This can
	 * help in complex implementation which require more information than
	 * just an integer - for example, a queue-pair.
	 */
	q_id = *((int *)context);

	for (i = 0; i < count; i++)
		queue_buf[q_id].bufs[i] = buffers[i]->buf_addr;

	return i;
}

static int skeleton_rawdev_dequeue_bufs(struct rte_rawdev *dev,
					struct rte_rawdev_buf **buffers,
					unsigned int count,
					rte_rawdev_obj_t context)
{
	unsigned int i;
	uint16_t q_id;
	RTE_SET_USED(dev);

	/* context is essentially the queue_id which is
	 * transferred as opaque object through the library layer. This can
	 * help in complex implementation which require more information than
	 * just an integer - for example, a queue-pair.
	 */
	q_id = *((int *)context);

	for (i = 0; i < count; i++)
		buffers[i]->buf_addr = queue_buf[q_id].bufs[i];

	return i;
}

static int skeleton_rawdev_dump(struct rte_rawdev *dev, FILE *f)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(f);

	return 0;
}

static int skeleton_rawdev_firmware_status_get(struct rte_rawdev *dev,
					       rte_rawdev_obj_t status_info)
{
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	skeldev = skeleton_rawdev_get_priv(dev);

	RTE_FUNC_PTR_OR_ERR_RET(dev, -EINVAL);

	if (status_info)
		memcpy(status_info, &skeldev->fw.firmware_state,
			sizeof(enum skeleton_firmware_state));

	return 0;
}


static int skeleton_rawdev_firmware_version_get(
					struct rte_rawdev *dev,
					rte_rawdev_obj_t version_info)
{
	struct skeleton_rawdev *skeldev;
	struct skeleton_firmware_version_info *vi;

	SKELETON_PMD_FUNC_TRACE();

	skeldev = skeleton_rawdev_get_priv(dev);
	vi = version_info;

	vi->major = skeldev->fw.firmware_version.major;
	vi->minor = skeldev->fw.firmware_version.minor;
	vi->subrel = skeldev->fw.firmware_version.subrel;

	return 0;
}

static int skeleton_rawdev_firmware_load(struct rte_rawdev *dev,
					 rte_rawdev_obj_t firmware_buf)
{
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	skeldev = skeleton_rawdev_get_priv(dev);

	/* firmware_buf is a mmaped, possibly DMA'able area, buffer. Being
	 * dummy, all this does is check if firmware_buf is not NULL and
	 * sets the state of the firmware.
	 */
	if (!firmware_buf)
		return -EINVAL;

	skeldev->fw.firmware_state = SKELETON_FW_LOADED;

	return 0;
}

static int skeleton_rawdev_firmware_unload(struct rte_rawdev *dev)
{
	struct skeleton_rawdev *skeldev;

	SKELETON_PMD_FUNC_TRACE();

	skeldev = skeleton_rawdev_get_priv(dev);

	skeldev->fw.firmware_state = SKELETON_FW_READY;

	return 0;
}

static const struct rte_rawdev_ops skeleton_rawdev_ops = {
	.dev_info_get = skeleton_rawdev_info_get,
	.dev_configure = skeleton_rawdev_configure,
	.dev_start = skeleton_rawdev_start,
	.dev_stop = skeleton_rawdev_stop,
	.dev_close = skeleton_rawdev_close,
	.dev_reset = skeleton_rawdev_reset,

	.queue_def_conf = skeleton_rawdev_queue_def_conf,
	.queue_setup = skeleton_rawdev_queue_setup,
	.queue_release = skeleton_rawdev_queue_release,
	.queue_count = skeleton_rawdev_queue_count,

	.attr_get = skeleton_rawdev_get_attr,
	.attr_set = skeleton_rawdev_set_attr,

	.enqueue_bufs = skeleton_rawdev_enqueue_bufs,
	.dequeue_bufs = skeleton_rawdev_dequeue_bufs,

	.dump = skeleton_rawdev_dump,

	.xstats_get = NULL,
	.xstats_get_names = NULL,
	.xstats_get_by_name = NULL,
	.xstats_reset = NULL,

	.firmware_status_get = skeleton_rawdev_firmware_status_get,
	.firmware_version_get = skeleton_rawdev_firmware_version_get,
	.firmware_load = skeleton_rawdev_firmware_load,
	.firmware_unload = skeleton_rawdev_firmware_unload,

	.dev_selftest = test_rawdev_skeldev,
};

static int
skeleton_rawdev_create(const char *name,
		       struct rte_vdev_device *vdev,
		       int socket_id)
{
	int ret = 0, i;
	struct rte_rawdev *rawdev = NULL;
	struct skeleton_rawdev *skeldev = NULL;

	if (!name) {
		SKELETON_PMD_ERR("Invalid name of the device!");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct skeleton_rawdev),
					 socket_id);
	if (rawdev == NULL) {
		SKELETON_PMD_ERR("Unable to allocate rawdevice");
		ret = -EINVAL;
		goto cleanup;
	}

	ret = rawdev->dev_id; /* return the rawdev id of new device */

	rawdev->dev_ops = &skeleton_rawdev_ops;
	rawdev->device = &vdev->device;

	skeldev = skeleton_rawdev_get_priv(rawdev);

	skeldev->device_id = SKELETON_DEVICE_ID;
	skeldev->vendor_id = SKELETON_VENDOR_ID;
	skeldev->capabilities = SKELETON_DEFAULT_CAPA;

	memset(&skeldev->fw, 0, sizeof(struct skeleton_firmware));

	skeldev->fw.firmware_state = SKELETON_FW_READY;
	skeldev->fw.firmware_version.major = SKELETON_MAJOR_VER;
	skeldev->fw.firmware_version.minor = SKELETON_MINOR_VER;
	skeldev->fw.firmware_version.subrel = SKELETON_SUB_VER;

	skeldev->device_state = SKELETON_DEV_STOPPED;

	/* Reset/set to default queue configuration for this device */
	for (i = 0; i < SKELETON_MAX_QUEUES; i++) {
		skeldev->queues[i].state = SKELETON_QUEUE_DETACH;
		skeldev->queues[i].depth = SKELETON_QUEUE_DEF_DEPTH;
	}

	/* Clear all allocated queue buffers */
	for (i = 0; i < SKELETON_MAX_QUEUES; i++)
		clear_queue_bufs(i);

	return ret;

cleanup:
	if (rawdev)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
skeleton_rawdev_destroy(const char *name)
{
	int ret;
	struct rte_rawdev *rdev;

	if (!name) {
		SKELETON_PMD_ERR("Invalid device name");
		return -EINVAL;
	}

	rdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rdev) {
		SKELETON_PMD_ERR("Invalid device name (%s)", name);
		return -EINVAL;
	}

	/* rte_rawdev_close is called by pmd_release */
	ret = rte_rawdev_pmd_release(rdev);
	if (ret)
		SKELETON_PMD_DEBUG("Device cleanup failed");

	return 0;
}

static int
skeldev_get_selftest(const char *key __rte_unused,
		     const char *value,
		     void *opaque)
{
	int *flag = opaque;
	*flag = atoi(value);
	return 0;
}

static int
skeldev_parse_vdev_args(struct rte_vdev_device *vdev)
{
	int selftest = 0;
	const char *name;
	const char *params;

	static const char *const args[] = {
		SKELETON_SELFTEST_ARG,
		NULL
	};

	name = rte_vdev_device_name(vdev);

	params = rte_vdev_device_args(vdev);
	if (params != NULL && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (!kvlist) {
			SKELETON_PMD_INFO(
				"Ignoring unsupported params supplied '%s'",
				name);
		} else {
			int ret = rte_kvargs_process(kvlist,
					SKELETON_SELFTEST_ARG,
					skeldev_get_selftest, &selftest);
			if (ret != 0 || (selftest < 0 || selftest > 1)) {
				SKELETON_PMD_ERR("%s: Error in parsing args",
						 name);
				rte_kvargs_free(kvlist);
				ret = -1; /* enforce if selftest is invalid */
				return ret;
			}
		}

		rte_kvargs_free(kvlist);
	}

	return selftest;
}

static int
skeleton_rawdev_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	int selftest = 0, ret = 0;


	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	/* More than one instance is not supported */
	if (skeldev_init_once) {
		SKELETON_PMD_ERR("Multiple instance not supported for %s",
				 name);
		return -EINVAL;
	}

	SKELETON_PMD_INFO("Init %s on NUMA node %d", name, rte_socket_id());

	selftest = skeldev_parse_vdev_args(vdev);
	/* In case of invalid argument, selftest != 1; ignore other values */

	ret = skeleton_rawdev_create(name, vdev, rte_socket_id());
	if (ret >= 0) {
		/* In case command line argument for 'selftest' was passed;
		 * if invalid arguments were passed, execution continues but
		 * without selftest.
		 */
		if (selftest == 1)
			test_rawdev_skeldev(ret);
	}

	/* Device instance created; Second instance not possible */
	skeldev_init_once = 1;

	return ret < 0 ? ret : 0;
}

static int
skeleton_rawdev_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	int ret;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -1;

	SKELETON_PMD_INFO("Closing %s on NUMA node %d", name, rte_socket_id());

	ret = skeleton_rawdev_destroy(name);
	if (!ret)
		skeldev_init_once = 0;

	return ret;
}

static struct rte_vdev_driver skeleton_pmd_drv = {
	.probe = skeleton_rawdev_probe,
	.remove = skeleton_rawdev_remove
};

RTE_PMD_REGISTER_VDEV(SKELETON_PMD_RAWDEV_NAME, skeleton_pmd_drv);
RTE_LOG_REGISTER_DEFAULT(skeleton_pmd_logtype, INFO);
