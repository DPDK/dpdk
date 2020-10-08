/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_rawdev_pmd.h>
#include <rte_memzone.h>
#include <rte_common.h>

#include "ioat_private.h"

int
idxd_rawdev_close(struct rte_rawdev *dev __rte_unused)
{
	return 0;
}

int
idxd_rawdev_create(const char *name, struct rte_device *dev,
		   const struct idxd_rawdev *base_idxd,
		   const struct rte_rawdev_ops *ops)
{
	struct idxd_rawdev *idxd;
	struct rte_rawdev *rawdev = NULL;
	const struct rte_memzone *mz = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	int ret = 0;

	if (!name) {
		IOAT_PMD_ERR("Invalid name of the device!");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct idxd_rawdev),
					 dev->numa_node);
	if (rawdev == NULL) {
		IOAT_PMD_ERR("Unable to allocate raw device");
		ret = -ENOMEM;
		goto cleanup;
	}

	snprintf(mz_name, sizeof(mz_name), "rawdev%u_private", rawdev->dev_id);
	mz = rte_memzone_reserve(mz_name, sizeof(struct idxd_rawdev),
			dev->numa_node, RTE_MEMZONE_IOVA_CONTIG);
	if (mz == NULL) {
		IOAT_PMD_ERR("Unable to reserve memzone for private data\n");
		ret = -ENOMEM;
		goto cleanup;
	}
	rawdev->dev_private = mz->addr;
	rawdev->dev_ops = ops;
	rawdev->device = dev;
	rawdev->driver_name = IOAT_PMD_RAWDEV_NAME_STR;

	idxd = rawdev->dev_private;
	*idxd = *base_idxd; /* copy over the main fields already passed in */
	idxd->rawdev = rawdev;
	idxd->mz = mz;

	return 0;

cleanup:
	if (rawdev)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}
