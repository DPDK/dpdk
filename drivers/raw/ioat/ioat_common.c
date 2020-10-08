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
idxd_dev_dump(struct rte_rawdev *dev, FILE *f)
{
	struct idxd_rawdev *idxd = dev->dev_private;
	struct rte_idxd_rawdev *rte_idxd = &idxd->public;
	int i;

	fprintf(f, "Raw Device #%d\n", dev->dev_id);
	fprintf(f, "Driver: %s\n\n", dev->driver_name);

	fprintf(f, "Portal: %p\n", rte_idxd->portal);
	fprintf(f, "Batch Ring size: %u\n", rte_idxd->batch_ring_sz);
	fprintf(f, "Comp Handle Ring size: %u\n\n", rte_idxd->hdl_ring_sz);

	fprintf(f, "Next batch: %u\n", rte_idxd->next_batch);
	fprintf(f, "Next batch to be completed: %u\n", rte_idxd->next_completed);
	for (i = 0; i < rte_idxd->batch_ring_sz; i++) {
		struct rte_idxd_desc_batch *b = &rte_idxd->batch_ring[i];
		fprintf(f, "Batch %u @%p: submitted=%u, op_count=%u, hdl_end=%u\n",
				i, b, b->submitted, b->op_count, b->hdl_end);
	}

	fprintf(f, "\n");
	fprintf(f, "Next free hdl: %u\n", rte_idxd->next_free_hdl);
	fprintf(f, "Last completed hdl: %u\n", rte_idxd->last_completed_hdl);
	fprintf(f, "Next returned hdl: %u\n", rte_idxd->next_ret_hdl);

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

	RTE_BUILD_BUG_ON(sizeof(struct rte_idxd_hw_desc) != 64);
	RTE_BUILD_BUG_ON(offsetof(struct rte_idxd_hw_desc, size) != 32);
	RTE_BUILD_BUG_ON(sizeof(struct rte_idxd_completion) != 32);

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
