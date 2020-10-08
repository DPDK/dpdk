/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_string_fns.h>
#include <rte_rawdev_pmd.h>

#include "ioat_private.h"

/** Name of the device driver */
#define IDXD_PMD_RAWDEV_NAME rawdev_idxd
/* takes a work queue(WQ) as parameter */
#define IDXD_ARG_WQ		"wq"

static const char * const valid_args[] = {
	IDXD_ARG_WQ,
	NULL
};

struct idxd_vdev_args {
	uint8_t device_id;
	uint8_t wq_id;
};

static int
idxd_rawdev_parse_wq(const char *key __rte_unused, const char *value,
			  void *extra_args)
{
	struct idxd_vdev_args *args = (struct idxd_vdev_args *)extra_args;
	int dev, wq, bytes = -1;
	int read = sscanf(value, "%d.%d%n", &dev, &wq, &bytes);

	if (read != 2 || bytes != (int)strlen(value)) {
		IOAT_PMD_ERR("Error parsing work-queue id. Must be in <dev_id>.<queue_id> format");
		return -EINVAL;
	}

	if (dev >= UINT8_MAX || wq >= UINT8_MAX) {
		IOAT_PMD_ERR("Device or work queue id out of range");
		return -EINVAL;
	}

	args->device_id = dev;
	args->wq_id = wq;

	return 0;
}

static int
idxd_vdev_parse_params(struct rte_kvargs *kvlist, struct idxd_vdev_args *args)
{
	if (rte_kvargs_count(kvlist, IDXD_ARG_WQ) == 1) {
		if (rte_kvargs_process(kvlist, IDXD_ARG_WQ,
				&idxd_rawdev_parse_wq, args) < 0) {
			IOAT_PMD_ERR("Error parsing %s", IDXD_ARG_WQ);
			goto free;
		}
	} else {
		IOAT_PMD_ERR("%s is a mandatory arg", IDXD_ARG_WQ);
		return -EINVAL;
	}

	return 0;

free:
	if (kvlist)
		rte_kvargs_free(kvlist);
	return -EINVAL;
}

static int
idxd_rawdev_probe_vdev(struct rte_vdev_device *vdev)
{
	struct rte_kvargs *kvlist;
	struct idxd_vdev_args vdev_args;
	const char *name;
	int ret = 0;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	IOAT_PMD_INFO("Initializing pmd_idxd for %s", name);

	kvlist = rte_kvargs_parse(rte_vdev_device_args(vdev), valid_args);
	if (kvlist == NULL) {
		IOAT_PMD_ERR("Invalid kvargs key");
		return -EINVAL;
	}

	ret = idxd_vdev_parse_params(kvlist, &vdev_args);
	if (ret) {
		IOAT_PMD_ERR("Failed to parse kvargs");
		return -EINVAL;
	}

	return 0;
}

static int
idxd_rawdev_remove_vdev(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	IOAT_PMD_INFO("Remove DSA vdev %p", name);

	return 0;
}

struct rte_vdev_driver idxd_rawdev_drv_vdev = {
	.probe = idxd_rawdev_probe_vdev,
	.remove = idxd_rawdev_remove_vdev,
};

RTE_PMD_REGISTER_VDEV(IDXD_PMD_RAWDEV_NAME, idxd_rawdev_drv_vdev);
RTE_PMD_REGISTER_PARAM_STRING(IDXD_PMD_RAWDEV_NAME,
			      "wq=<string>");
