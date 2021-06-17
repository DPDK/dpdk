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
	fprintf(f, "Config: {ring_size: %u, hdls_disable: %u}\n\n",
			rte_idxd->cfg.ring_size, rte_idxd->cfg.hdls_disable);

	fprintf(f, "max batches: %u\n", rte_idxd->max_batches);
	fprintf(f, "batch idx read: %u\n", rte_idxd->batch_idx_read);
	fprintf(f, "batch idx write: %u\n", rte_idxd->batch_idx_write);
	fprintf(f, "batch idxes:");
	for (i = 0; i < rte_idxd->max_batches + 1; i++)
		fprintf(f, "%u ", rte_idxd->batch_idx_ring[i]);
	fprintf(f, "\n\n");

	fprintf(f, "hdls read: %u\n", rte_idxd->max_batches);
	fprintf(f, "hdls avail: %u\n", rte_idxd->hdls_avail);
	fprintf(f, "batch start: %u\n", rte_idxd->batch_start);
	fprintf(f, "batch size: %u\n", rte_idxd->batch_size);

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

	if (cfg != NULL)
		*cfg = rte_idxd->cfg;
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

	if (config_size != sizeof(*cfg))
		return -EINVAL;

	if (dev->started) {
		IOAT_PMD_ERR("%s: Error, device is started.", __func__);
		return -EAGAIN;
	}

	rte_idxd->cfg = *cfg;

	if (!rte_is_power_of_2(max_desc))
		max_desc = rte_align32pow2(max_desc);
	IOAT_PMD_DEBUG("Rawdev %u using %u descriptors",
			dev->dev_id, max_desc);
	rte_idxd->desc_ring_mask = max_desc - 1;

	/* in case we are reconfiguring a device, free any existing memory */
	rte_free(rte_idxd->desc_ring);
	rte_free(rte_idxd->hdl_ring);
	rte_free(rte_idxd->hdl_ring_flags);

	/* allocate the descriptor ring at 2x size as batches can't wrap */
	rte_idxd->desc_ring = rte_zmalloc(NULL,
			sizeof(*rte_idxd->desc_ring) * max_desc * 2, 0);
	if (rte_idxd->desc_ring == NULL)
		return -ENOMEM;
	rte_idxd->desc_iova = rte_mem_virt2iova(rte_idxd->desc_ring);

	rte_idxd->hdl_ring = rte_zmalloc(NULL,
			sizeof(*rte_idxd->hdl_ring) * max_desc, 0);
	if (rte_idxd->hdl_ring == NULL) {
		rte_free(rte_idxd->desc_ring);
		rte_idxd->desc_ring = NULL;
		return -ENOMEM;
	}
	rte_idxd->hdl_ring_flags = rte_zmalloc(NULL,
			sizeof(*rte_idxd->hdl_ring_flags) * max_desc, 0);
	if (rte_idxd->hdl_ring_flags == NULL) {
		rte_free(rte_idxd->desc_ring);
		rte_free(rte_idxd->hdl_ring);
		rte_idxd->desc_ring = NULL;
		rte_idxd->hdl_ring = NULL;
		return -ENOMEM;
	}
	rte_idxd->hdls_read = rte_idxd->batch_start = 0;
	rte_idxd->batch_size = 0;
	rte_idxd->hdls_avail = 0;

	return 0;
}

int
idxd_rawdev_create(const char *name, struct rte_device *dev,
		   const struct idxd_rawdev *base_idxd,
		   const struct rte_rawdev_ops *ops)
{
	struct idxd_rawdev *idxd;
	struct rte_idxd_rawdev *public;
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

	/* Allocate memory for the primary process or else return the memory
	 * of primary memzone for the secondary process.
	 */
	snprintf(mz_name, sizeof(mz_name), "rawdev%u_private", rawdev->dev_id);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		mz = rte_memzone_lookup(mz_name);
		if (mz == NULL) {
			IOAT_PMD_ERR("Unable lookup memzone for private data\n");
			ret = -ENOMEM;
			goto cleanup;
		}
		rawdev->dev_private = mz->addr;
		rawdev->dev_ops = ops;
		rawdev->device = dev;
		return 0;
	}
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

	public = &idxd->public;
	public->type = RTE_IDXD_DEV;
	public->max_batches = idxd->max_batches;
	public->batch_idx_read = 0;
	public->batch_idx_write = 0;
	/* allocate batch index ring. The +1 is because we can never fully use
	 * the ring, otherwise read == write means both full and empty.
	 */
	public->batch_idx_ring = rte_zmalloc(NULL,
			sizeof(uint16_t) * (idxd->max_batches + 1), 0);
	if (public->batch_idx_ring == NULL) {
		IOAT_PMD_ERR("Unable to reserve memory for batch data\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	return 0;

cleanup:
	if (mz)
		rte_memzone_free(mz);
	if (rawdev)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}
