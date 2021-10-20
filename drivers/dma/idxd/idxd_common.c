/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_log.h>

#include "idxd_internal.h"

#define IDXD_PMD_NAME_STR "dmadev_idxd"

int
idxd_dump(const struct rte_dma_dev *dev, FILE *f)
{
	struct idxd_dmadev *idxd = dev->fp_obj->dev_private;
	unsigned int i;

	fprintf(f, "== IDXD Private Data ==\n");
	fprintf(f, "  Portal: %p\n", idxd->portal);
	fprintf(f, "  Config: { ring_size: %u }\n",
			idxd->qcfg.nb_desc);
	fprintf(f, "  Batch ring (sz = %u, max_batches = %u):\n\t",
			idxd->max_batches + 1, idxd->max_batches);
	for (i = 0; i <= idxd->max_batches; i++) {
		fprintf(f, " %u ", idxd->batch_idx_ring[i]);
		if (i == idxd->batch_idx_read && i == idxd->batch_idx_write)
			fprintf(f, "[rd ptr, wr ptr] ");
		else if (i == idxd->batch_idx_read)
			fprintf(f, "[rd ptr] ");
		else if (i == idxd->batch_idx_write)
			fprintf(f, "[wr ptr] ");
		if (i == idxd->max_batches)
			fprintf(f, "\n");
	}

	fprintf(f, "  Curr batch: start = %u, size = %u\n", idxd->batch_start, idxd->batch_size);
	fprintf(f, "  IDS: avail = %u, returned: %u\n", idxd->ids_avail, idxd->ids_returned);
	return 0;
}

int
idxd_info_get(const struct rte_dma_dev *dev, struct rte_dma_info *info, uint32_t size)
{
	struct idxd_dmadev *idxd = dev->fp_obj->dev_private;

	if (size < sizeof(*info))
		return -EINVAL;

	*info = (struct rte_dma_info) {
			.dev_capa = RTE_DMA_CAPA_MEM_TO_MEM | RTE_DMA_CAPA_HANDLES_ERRORS |
				RTE_DMA_CAPA_OPS_COPY | RTE_DMA_CAPA_OPS_FILL,
			.max_vchans = 1,
			.max_desc = 4096,
			.min_desc = 64,
	};
	if (idxd->sva_support)
		info->dev_capa |= RTE_DMA_CAPA_SVA;
	return 0;
}

int
idxd_configure(struct rte_dma_dev *dev __rte_unused, const struct rte_dma_conf *dev_conf,
		uint32_t conf_sz)
{
	if (sizeof(struct rte_dma_conf) != conf_sz)
		return -EINVAL;

	if (dev_conf->nb_vchans != 1)
		return -EINVAL;
	return 0;
}

int
idxd_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		const struct rte_dma_vchan_conf *qconf, uint32_t qconf_sz)
{
	struct idxd_dmadev *idxd = dev->fp_obj->dev_private;
	uint16_t max_desc = qconf->nb_desc;

	if (sizeof(struct rte_dma_vchan_conf) != qconf_sz)
		return -EINVAL;

	idxd->qcfg = *qconf;

	if (!rte_is_power_of_2(max_desc))
		max_desc = rte_align32pow2(max_desc);
	IDXD_PMD_DEBUG("DMA dev %u using %u descriptors", dev->data->dev_id, max_desc);
	idxd->desc_ring_mask = max_desc - 1;
	idxd->qcfg.nb_desc = max_desc;

	/* in case we are reconfiguring a device, free any existing memory */
	rte_free(idxd->desc_ring);

	/* allocate the descriptor ring at 2x size as batches can't wrap */
	idxd->desc_ring = rte_zmalloc(NULL, sizeof(*idxd->desc_ring) * max_desc * 2, 0);
	if (idxd->desc_ring == NULL)
		return -ENOMEM;
	idxd->desc_iova = rte_mem_virt2iova(idxd->desc_ring);

	idxd->batch_idx_read = 0;
	idxd->batch_idx_write = 0;
	idxd->batch_start = 0;
	idxd->batch_size = 0;
	idxd->ids_returned = 0;
	idxd->ids_avail = 0;

	memset(idxd->batch_comp_ring, 0, sizeof(*idxd->batch_comp_ring) *
			(idxd->max_batches + 1));
	return 0;
}

int
idxd_dmadev_create(const char *name, struct rte_device *dev,
		   const struct idxd_dmadev *base_idxd,
		   const struct rte_dma_dev_ops *ops)
{
	struct idxd_dmadev *idxd = NULL;
	struct rte_dma_dev *dmadev = NULL;
	int ret = 0;

	RTE_BUILD_BUG_ON(sizeof(struct idxd_hw_desc) != 64);
	RTE_BUILD_BUG_ON(offsetof(struct idxd_hw_desc, size) != 32);
	RTE_BUILD_BUG_ON(sizeof(struct idxd_completion) != 32);

	if (!name) {
		IDXD_PMD_ERR("Invalid name of the device!");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Allocate device structure */
	dmadev = rte_dma_pmd_allocate(name, dev->numa_node, sizeof(struct idxd_dmadev));
	if (dmadev == NULL) {
		IDXD_PMD_ERR("Unable to allocate dma device");
		ret = -ENOMEM;
		goto cleanup;
	}
	dmadev->dev_ops = ops;
	dmadev->device = dev;

	idxd = dmadev->data->dev_private;
	*idxd = *base_idxd; /* copy over the main fields already passed in */
	idxd->dmadev = dmadev;

	/* allocate batch index ring and completion ring.
	 * The +1 is because we can never fully use
	 * the ring, otherwise read == write means both full and empty.
	 */
	idxd->batch_comp_ring = rte_zmalloc_socket(NULL, (sizeof(idxd->batch_idx_ring[0]) +
			sizeof(idxd->batch_comp_ring[0]))	* (idxd->max_batches + 1),
			sizeof(idxd->batch_comp_ring[0]), dev->numa_node);
	if (idxd->batch_comp_ring == NULL) {
		IDXD_PMD_ERR("Unable to reserve memory for batch data\n");
		ret = -ENOMEM;
		goto cleanup;
	}
	idxd->batch_idx_ring = (void *)&idxd->batch_comp_ring[idxd->max_batches+1];
	idxd->batch_iova = rte_mem_virt2iova(idxd->batch_comp_ring);

	dmadev->fp_obj->dev_private = idxd;

	idxd->dmadev->state = RTE_DMA_DEV_READY;

	return 0;

cleanup:
	if (dmadev)
		rte_dma_pmd_release(name);

	return ret;
}

int idxd_pmd_logtype;

RTE_LOG_REGISTER_DEFAULT(idxd_pmd_logtype, WARNING);
