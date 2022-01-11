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

	idxd->stats.submitted += idxd->batch_size;

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

static enum rte_dma_status_code
get_comp_status(struct idxd_completion *c)
{
	uint8_t st = c->status;
	switch (st) {
	/* successful descriptors are not written back normally */
	case IDXD_COMP_STATUS_INCOMPLETE:
	case IDXD_COMP_STATUS_SUCCESS:
		return RTE_DMA_STATUS_SUCCESSFUL;
	case IDXD_COMP_STATUS_INVALID_OPCODE:
		return RTE_DMA_STATUS_INVALID_OPCODE;
	case IDXD_COMP_STATUS_INVALID_SIZE:
		return RTE_DMA_STATUS_INVALID_LENGTH;
	case IDXD_COMP_STATUS_SKIPPED:
		return RTE_DMA_STATUS_NOT_ATTEMPTED;
	default:
		return RTE_DMA_STATUS_ERROR_UNKNOWN;
	}
}

int
idxd_vchan_status(const struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		enum rte_dma_vchan_status *status)
{
	struct idxd_dmadev *idxd = dev->fp_obj->dev_private;
	uint16_t last_batch_write = idxd->batch_idx_write == 0 ? idxd->max_batches :
			idxd->batch_idx_write - 1;
	uint8_t bstatus = (idxd->batch_comp_ring[last_batch_write].status != 0);

	/* An IDXD device will always be either active or idle.
	 * RTE_DMA_VCHAN_HALTED_ERROR is therefore not supported by IDXD.
	 */
	*status = bstatus ? RTE_DMA_VCHAN_IDLE : RTE_DMA_VCHAN_ACTIVE;

	return 0;
}

static __rte_always_inline int
batch_ok(struct idxd_dmadev *idxd, uint16_t max_ops, enum rte_dma_status_code *status)
{
	uint16_t ret;
	uint8_t bstatus;

	if (max_ops == 0)
		return 0;

	/* first check if there are any unreturned handles from last time */
	if (idxd->ids_avail != idxd->ids_returned) {
		ret = RTE_MIN((uint16_t)(idxd->ids_avail - idxd->ids_returned), max_ops);
		idxd->ids_returned += ret;
		if (status)
			memset(status, RTE_DMA_STATUS_SUCCESSFUL, ret * sizeof(*status));
		return ret;
	}

	if (idxd->batch_idx_read == idxd->batch_idx_write)
		return 0;

	bstatus = idxd->batch_comp_ring[idxd->batch_idx_read].status;
	/* now check if next batch is complete and successful */
	if (bstatus == IDXD_COMP_STATUS_SUCCESS) {
		/* since the batch idx ring stores the start of each batch, pre-increment to lookup
		 * start of next batch.
		 */
		if (++idxd->batch_idx_read > idxd->max_batches)
			idxd->batch_idx_read = 0;
		idxd->ids_avail = idxd->batch_idx_ring[idxd->batch_idx_read];

		ret = RTE_MIN((uint16_t)(idxd->ids_avail - idxd->ids_returned), max_ops);
		idxd->ids_returned += ret;
		if (status)
			memset(status, RTE_DMA_STATUS_SUCCESSFUL, ret * sizeof(*status));
		return ret;
	}
	/* check if batch is incomplete */
	else if (bstatus == IDXD_COMP_STATUS_INCOMPLETE)
		return 0;

	return -1; /* error case */
}

static inline uint16_t
batch_completed(struct idxd_dmadev *idxd, uint16_t max_ops, bool *has_error)
{
	uint16_t i;
	uint16_t b_start, b_end, next_batch;

	int ret = batch_ok(idxd, max_ops, NULL);
	if (ret >= 0)
		return ret;

	/* ERROR case, not successful, not incomplete */
	/* Get the batch size, and special case size 1.
	 * once we identify the actual failure job, return other jobs, then update
	 * the batch ring indexes to make it look like the first job of the batch has failed.
	 * Subsequent calls here will always return zero packets, and the error must be cleared by
	 * calling the completed_status() function.
	 */
	next_batch = (idxd->batch_idx_read + 1);
	if (next_batch > idxd->max_batches)
		next_batch = 0;
	b_start = idxd->batch_idx_ring[idxd->batch_idx_read];
	b_end = idxd->batch_idx_ring[next_batch];

	if (b_end - b_start == 1) { /* not a batch */
		*has_error = true;
		return 0;
	}

	for (i = b_start; i < b_end; i++) {
		struct idxd_completion *c = (void *)&idxd->desc_ring[i & idxd->desc_ring_mask];
		if (c->status > IDXD_COMP_STATUS_SUCCESS) /* ignore incomplete(0) and success(1) */
			break;
	}
	ret = RTE_MIN((uint16_t)(i - idxd->ids_returned), max_ops);
	if (ret < max_ops)
		*has_error = true; /* we got up to the point of error */
	idxd->ids_avail = idxd->ids_returned += ret;

	/* to ensure we can call twice and just return 0, set start of batch to where we finished */
	idxd->batch_comp_ring[idxd->batch_idx_read].completed_size -= ret;
	idxd->batch_idx_ring[idxd->batch_idx_read] += ret;
	if (idxd->batch_idx_ring[next_batch] - idxd->batch_idx_ring[idxd->batch_idx_read] == 1) {
		/* copy over the descriptor status to the batch ring as if no batch */
		uint16_t d_idx = idxd->batch_idx_ring[idxd->batch_idx_read] & idxd->desc_ring_mask;
		struct idxd_completion *desc_comp = (void *)&idxd->desc_ring[d_idx];
		idxd->batch_comp_ring[idxd->batch_idx_read].status = desc_comp->status;
	}

	return ret;
}

static uint16_t
batch_completed_status(struct idxd_dmadev *idxd, uint16_t max_ops, enum rte_dma_status_code *status)
{
	uint16_t next_batch;

	int ret = batch_ok(idxd, max_ops, status);
	if (ret >= 0)
		return ret;

	/* ERROR case, not successful, not incomplete */
	/* Get the batch size, and special case size 1.
	 */
	next_batch = (idxd->batch_idx_read + 1);
	if (next_batch > idxd->max_batches)
		next_batch = 0;
	const uint16_t b_start = idxd->batch_idx_ring[idxd->batch_idx_read];
	const uint16_t b_end = idxd->batch_idx_ring[next_batch];
	const uint16_t b_len = b_end - b_start;
	if (b_len == 1) {/* not a batch */
		*status = get_comp_status(&idxd->batch_comp_ring[idxd->batch_idx_read]);
		if (status != RTE_DMA_STATUS_SUCCESSFUL)
			idxd->stats.errors++;
		idxd->ids_avail++;
		idxd->ids_returned++;
		idxd->batch_idx_read = next_batch;
		return 1;
	}

	/* not a single-element batch, need to process more.
	 * Scenarios:
	 * 1. max_ops >= batch_size - can fit everything, simple case
	 *   - loop through completed ops and then add on any not-attempted ones
	 * 2. max_ops < batch_size - can't fit everything, more complex case
	 *   - loop through completed/incomplete and stop when hit max_ops
	 *   - adjust the batch descriptor to update where we stopped, with appropriate bcount
	 *   - if bcount is to be exactly 1, update the batch descriptor as it will be treated as
	 *     non-batch next time.
	 */
	const uint16_t bcount = idxd->batch_comp_ring[idxd->batch_idx_read].completed_size;
	for (ret = 0; ret < b_len && ret < max_ops; ret++) {
		struct idxd_completion *c = (void *)
				&idxd->desc_ring[(b_start + ret) & idxd->desc_ring_mask];
		status[ret] = (ret < bcount) ? get_comp_status(c) : RTE_DMA_STATUS_NOT_ATTEMPTED;
		if (status[ret] != RTE_DMA_STATUS_SUCCESSFUL)
			idxd->stats.errors++;
	}
	idxd->ids_avail = idxd->ids_returned += ret;

	/* everything fit */
	if (ret == b_len) {
		idxd->batch_idx_read = next_batch;
		return ret;
	}

	/* set up for next time, update existing batch descriptor & start idx at batch_idx_read */
	idxd->batch_idx_ring[idxd->batch_idx_read] += ret;
	if (ret > bcount) {
		/* we have only incomplete ones - set batch completed size to 0 */
		struct idxd_completion *comp = &idxd->batch_comp_ring[idxd->batch_idx_read];
		comp->completed_size = 0;
		/* if there is only one descriptor left, job skipped so set flag appropriately */
		if (b_len - ret == 1)
			comp->status = IDXD_COMP_STATUS_SKIPPED;
	} else {
		struct idxd_completion *comp = &idxd->batch_comp_ring[idxd->batch_idx_read];
		comp->completed_size -= ret;
		/* if there is only one descriptor left, copy status info straight to desc */
		if (comp->completed_size == 1) {
			struct idxd_completion *c = (void *)
					&idxd->desc_ring[(b_start + ret) & idxd->desc_ring_mask];
			comp->status = c->status;
			/* individual descs can be ok without writeback, but not batches */
			if (comp->status == IDXD_COMP_STATUS_INCOMPLETE)
				comp->status = IDXD_COMP_STATUS_SUCCESS;
		} else if (bcount == b_len) {
			/* check if we still have an error, and clear flag if not */
			uint16_t i;
			for (i = b_start + ret; i < b_end; i++) {
				struct idxd_completion *c = (void *)
						&idxd->desc_ring[i & idxd->desc_ring_mask];
				if (c->status > IDXD_COMP_STATUS_SUCCESS)
					break;
			}
			if (i == b_end) /* no errors */
				comp->status = IDXD_COMP_STATUS_SUCCESS;
		}
	}

	return ret;
}

uint16_t
idxd_completed(void *dev_private, uint16_t qid __rte_unused, uint16_t max_ops,
		uint16_t *last_idx, bool *has_error)
{
	struct idxd_dmadev *idxd = dev_private;
	uint16_t batch, ret = 0;

	do {
		batch = batch_completed(idxd, max_ops - ret, has_error);
		ret += batch;
	} while (batch > 0 && *has_error == false);

	idxd->stats.completed += ret;
	*last_idx = idxd->ids_returned - 1;
	return ret;
}

uint16_t
idxd_completed_status(void *dev_private, uint16_t qid __rte_unused, uint16_t max_ops,
		uint16_t *last_idx, enum rte_dma_status_code *status)
{
	struct idxd_dmadev *idxd = dev_private;
	uint16_t batch, ret = 0;

	do {
		batch = batch_completed_status(idxd, max_ops - ret, &status[ret]);
		ret += batch;
	} while (batch > 0);

	idxd->stats.completed += ret;
	*last_idx = idxd->ids_returned - 1;
	return ret;
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
idxd_stats_get(const struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		struct rte_dma_stats *stats, uint32_t stats_sz)
{
	struct idxd_dmadev *idxd = dev->fp_obj->dev_private;
	if (stats_sz < sizeof(*stats))
		return -EINVAL;
	*stats = idxd->stats;
	return 0;
}

int
idxd_stats_reset(struct rte_dma_dev *dev, uint16_t vchan __rte_unused)
{
	struct idxd_dmadev *idxd = dev->fp_obj->dev_private;
	idxd->stats = (struct rte_dma_stats){0};
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

uint16_t
idxd_burst_capacity(const void *dev_private, uint16_t vchan __rte_unused)
{
	const struct idxd_dmadev *idxd = dev_private;
	uint16_t write_idx = idxd->batch_start + idxd->batch_size;
	uint16_t used_space;

	/* Check for space in the batch ring */
	if ((idxd->batch_idx_read == 0 && idxd->batch_idx_write == idxd->max_batches) ||
			idxd->batch_idx_write + 1 == idxd->batch_idx_read)
		return 0;

	/* Subtract and mask to get in correct range */
	used_space = (write_idx - idxd->ids_returned) & idxd->desc_ring_mask;

	const int ret = RTE_MIN((idxd->desc_ring_mask - used_space),
			(idxd->max_batch_size - idxd->batch_size));
	return ret < 0 ? 0 : (uint16_t)ret;
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
	dmadev->fp_obj->completed = idxd_completed;
	dmadev->fp_obj->completed_status = idxd_completed_status;
	dmadev->fp_obj->burst_capacity = idxd_burst_capacity;

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
