/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_rawdev_pmd.h>
#include <rte_memzone.h>
#include <rte_common.h>
#include <rte_string_fns.h>

#include "ioat_private.h"

static const char * const xstat_names[] = {
		"failed_enqueues", "successful_enqueues",
		"copies_started", "copies_completed"
};

int
ioat_xstats_get(const struct rte_rawdev *dev, const unsigned int ids[],
		uint64_t values[], unsigned int n)
{
	const struct rte_ioat_rawdev *ioat = dev->dev_private;
	const uint64_t *stats = (const void *)&ioat->xstats;
	unsigned int i;

	for (i = 0; i < n; i++) {
		if (ids[i] > sizeof(ioat->xstats)/sizeof(*stats))
			values[i] = 0;
		else
			values[i] = stats[ids[i]];
	}
	return n;
}

int
ioat_xstats_get_names(const struct rte_rawdev *dev,
		struct rte_rawdev_xstats_name *names,
		unsigned int size)
{
	unsigned int i;

	RTE_SET_USED(dev);
	if (size < RTE_DIM(xstat_names))
		return RTE_DIM(xstat_names);

	for (i = 0; i < RTE_DIM(xstat_names); i++)
		strlcpy(names[i].name, xstat_names[i], sizeof(names[i]));

	return RTE_DIM(xstat_names);
}

int
ioat_xstats_reset(struct rte_rawdev *dev, const uint32_t *ids, uint32_t nb_ids)
{
	struct rte_ioat_rawdev *ioat = dev->dev_private;
	uint64_t *stats = (void *)&ioat->xstats;
	unsigned int i;

	if (!ids) {
		memset(&ioat->xstats, 0, sizeof(ioat->xstats));
		return 0;
	}

	for (i = 0; i < nb_ids; i++)
		if (ids[i] < sizeof(ioat->xstats)/sizeof(*stats))
			stats[ids[i]] = 0;

	return 0;
}

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
idxd_dev_info_get(struct rte_rawdev *dev, rte_rawdev_obj_t dev_info,
		size_t info_size)
{
	struct rte_ioat_rawdev_config *cfg = dev_info;
	struct idxd_rawdev *idxd = dev->dev_private;
	struct rte_idxd_rawdev *rte_idxd = &idxd->public;

	if (info_size != sizeof(*cfg))
		return -EINVAL;

	if (cfg != NULL) {
		cfg->ring_size = rte_idxd->hdl_ring_sz;
		cfg->hdls_disable = rte_idxd->hdls_disable;
	}
	return 0;
}

int
idxd_dev_configure(const struct rte_rawdev *dev,
		rte_rawdev_obj_t config, size_t config_size)
{
	struct idxd_rawdev *idxd = dev->dev_private;
	struct rte_idxd_rawdev *rte_idxd = &idxd->public;
	struct rte_ioat_rawdev_config *cfg = config;
	uint16_t max_desc = cfg->ring_size;
	uint16_t max_batches = max_desc / BATCH_SIZE;
	uint16_t i;

	if (config_size != sizeof(*cfg))
		return -EINVAL;

	if (dev->started) {
		IOAT_PMD_ERR("%s: Error, device is started.", __func__);
		return -EAGAIN;
	}

	rte_idxd->hdls_disable = cfg->hdls_disable;

	/* limit the batches to what can be stored in hardware */
	if (max_batches > idxd->max_batches) {
		IOAT_PMD_DEBUG("Ring size of %u is too large for this device, need to limit to %u batches of %u",
				max_desc, idxd->max_batches, BATCH_SIZE);
		max_batches = idxd->max_batches;
		max_desc = max_batches * BATCH_SIZE;
	}
	if (!rte_is_power_of_2(max_desc))
		max_desc = rte_align32pow2(max_desc);
	IOAT_PMD_DEBUG("Rawdev %u using %u descriptors in %u batches",
			dev->dev_id, max_desc, max_batches);

	/* in case we are reconfiguring a device, free any existing memory */
	rte_free(rte_idxd->batch_ring);
	rte_free(rte_idxd->hdl_ring);

	rte_idxd->batch_ring = rte_zmalloc(NULL,
			sizeof(*rte_idxd->batch_ring) * max_batches, 0);
	if (rte_idxd->batch_ring == NULL)
		return -ENOMEM;

	rte_idxd->hdl_ring = rte_zmalloc(NULL,
			sizeof(*rte_idxd->hdl_ring) * max_desc, 0);
	if (rte_idxd->hdl_ring == NULL) {
		rte_free(rte_idxd->batch_ring);
		rte_idxd->batch_ring = NULL;
		return -ENOMEM;
	}
	rte_idxd->batch_ring_sz = max_batches;
	rte_idxd->hdl_ring_sz = max_desc;

	for (i = 0; i < rte_idxd->batch_ring_sz; i++) {
		struct rte_idxd_desc_batch *b = &rte_idxd->batch_ring[i];
		b->batch_desc.completion = rte_mem_virt2iova(&b->comp);
		b->batch_desc.desc_addr = rte_mem_virt2iova(&b->null_desc);
		b->batch_desc.op_flags = (idxd_op_batch << IDXD_CMD_OP_SHIFT) |
				IDXD_FLAG_COMPLETION_ADDR_VALID |
				IDXD_FLAG_REQUEST_COMPLETION;
	}

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
	idxd->public.type = RTE_IDXD_DEV;
	idxd->rawdev = rawdev;
	idxd->mz = mz;

	return 0;

cleanup:
	if (rawdev)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}
