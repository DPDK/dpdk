/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Intel Corporation
 */

#include <x86intrin.h>

#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_prefetch.h>

#include "idxd_internal.h"

#define IDXD_PMD_NAME_STR "dmadev_idxd"

static __rte_always_inline rte_iova_t
__desc_idx_to_iova(struct idxd_dmadev *idxd, uint16_t n)
{
	return idxd->desc_iova + (n * sizeof(struct idxd_hw_desc));
}

static __rte_always_inline void
__idxd_movdir64b(volatile void *dst, const struct idxd_hw_desc *src)
{
	asm volatile (".byte 0x66, 0x0f, 0x38, 0xf8, 0x02"
			:
			: "a" (dst), "d" (src)
			: "memory");
}

static __rte_always_inline void
__submit(struct idxd_dmadev *idxd)
{
	rte_prefetch1(&idxd->batch_comp_ring[idxd->batch_idx_read]);

	if (idxd->batch_size == 0)
		return;

	/* write completion to batch comp ring */
	rte_iova_t comp_addr = idxd->batch_iova +
			(idxd->batch_idx_write * sizeof(struct idxd_completion));

	if (idxd->batch_size == 1) {
		/* submit batch directly */
		struct idxd_hw_desc desc =
				idxd->desc_ring[idxd->batch_start & idxd->desc_ring_mask];
		desc.completion = comp_addr;
		desc.op_flags |= IDXD_FLAG_REQUEST_COMPLETION;
		_mm_sfence(); /* fence before writing desc to device */
		__idxd_movdir64b(idxd->portal, &desc);
	} else {
		const struct idxd_hw_desc batch_desc = {
				.op_flags = (idxd_op_batch << IDXD_CMD_OP_SHIFT) |
				IDXD_FLAG_COMPLETION_ADDR_VALID |
				IDXD_FLAG_REQUEST_COMPLETION,
				.desc_addr = __desc_idx_to_iova(idxd,
						idxd->batch_start & idxd->desc_ring_mask),
				.completion = comp_addr,
				.size = idxd->batch_size,
		};
		_mm_sfence(); /* fence before writing desc to device */
		__idxd_movdir64b(idxd->portal, &batch_desc);
	}

	if (++idxd->batch_idx_write > idxd->max_batches)
		idxd->batch_idx_write = 0;

	idxd->batch_start += idxd->batch_size;
	idxd->batch_size = 0;
	idxd->batch_idx_ring[idxd->batch_idx_write] = idxd->batch_start;
	_mm256_store_si256((void *)&idxd->batch_comp_ring[idxd->batch_idx_write],
			_mm256_setzero_si256());
}

static __rte_always_inline int
__idxd_write_desc(struct idxd_dmadev *idxd,
		const uint32_t op_flags,
		const rte_iova_t src,
		const rte_iova_t dst,
		const uint32_t size,
		const uint32_t flags)
{
	uint16_t mask = idxd->desc_ring_mask;
	uint16_t job_id = idxd->batch_start + idxd->batch_size;
	/* we never wrap batches, so we only mask the start and allow start+size to overflow */
	uint16_t write_idx = (idxd->batch_start & mask) + idxd->batch_size;

	/* first check batch ring space then desc ring space */
	if ((idxd->batch_idx_read == 0 && idxd->batch_idx_write == idxd->max_batches) ||
			idxd->batch_idx_write + 1 == idxd->batch_idx_read)
		return -ENOSPC;
	if (((write_idx + 1) & mask) == (idxd->ids_returned & mask))
		return -ENOSPC;

	/* write desc. Note: descriptors don't wrap, but the completion address does */
	const uint64_t op_flags64 = (uint64_t)(op_flags | IDXD_FLAG_COMPLETION_ADDR_VALID) << 32;
	const uint64_t comp_addr = __desc_idx_to_iova(idxd, write_idx & mask);
	_mm256_store_si256((void *)&idxd->desc_ring[write_idx],
			_mm256_set_epi64x(dst, src, comp_addr, op_flags64));
	_mm256_store_si256((void *)&idxd->desc_ring[write_idx].size,
			_mm256_set_epi64x(0, 0, 0, size));

	idxd->batch_size++;

	rte_prefetch0_write(&idxd->desc_ring[write_idx + 1]);

	if (flags & RTE_DMA_OP_FLAG_SUBMIT)
		__submit(idxd);

	return job_id;
}

int
idxd_enqueue_copy(void *dev_private, uint16_t qid __rte_unused, rte_iova_t src,
		rte_iova_t dst, unsigned int length, uint64_t flags)
{
	/* we can take advantage of the fact that the fence flag in dmadev and DSA are the same,
	 * but check it at compile time to be sure.
	 */
	RTE_BUILD_BUG_ON(RTE_DMA_OP_FLAG_FENCE != IDXD_FLAG_FENCE);
	uint32_t memmove = (idxd_op_memmove << IDXD_CMD_OP_SHIFT) |
			IDXD_FLAG_CACHE_CONTROL | (flags & IDXD_FLAG_FENCE);
	return __idxd_write_desc(dev_private, memmove, src, dst, length,
			flags);
}

int
idxd_enqueue_fill(void *dev_private, uint16_t qid __rte_unused, uint64_t pattern,
		rte_iova_t dst, unsigned int length, uint64_t flags)
{
	uint32_t fill = (idxd_op_fill << IDXD_CMD_OP_SHIFT) |
			IDXD_FLAG_CACHE_CONTROL | (flags & IDXD_FLAG_FENCE);
	return __idxd_write_desc(dev_private, fill, pattern, dst, length,
			flags);
}

int
idxd_submit(void *dev_private, uint16_t qid __rte_unused)
{
	__submit(dev_private);
	return 0;
}

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

	dmadev->fp_obj->copy = idxd_enqueue_copy;
	dmadev->fp_obj->fill = idxd_enqueue_fill;
	dmadev->fp_obj->submit = idxd_submit;

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
