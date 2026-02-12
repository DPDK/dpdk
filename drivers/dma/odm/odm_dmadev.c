/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <string.h>

#include <bus_pci_driver.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <rte_memcpy.h>
#include <rte_pci.h>

#include "odm.h"

#define PCI_VENDOR_ID_CAVIUM	 0x177D
#define PCI_DEVID_ODYSSEY_ODM_VF 0xA08C
#define PCI_DRIVER_NAME		 dma_odm

static int
odm_dmadev_info_get(const struct rte_dma_dev *dev, struct rte_dma_info *dev_info, uint32_t size)
{
	struct odm_dev *odm = NULL;

	RTE_SET_USED(size);

	odm = dev->fp_obj->dev_private;

	dev_info->max_vchans = odm->max_qs;
	dev_info->nb_vchans = odm->num_qs;
	dev_info->dev_capa =
		(RTE_DMA_CAPA_MEM_TO_MEM | RTE_DMA_CAPA_OPS_COPY | RTE_DMA_CAPA_OPS_COPY_SG);
	dev_info->max_desc = ODM_IRING_MAX_ENTRY;
	dev_info->min_desc = 1;
	dev_info->max_sges = ODM_MAX_POINTER;

	return 0;
}

static int
odm_dmadev_configure(struct rte_dma_dev *dev, const struct rte_dma_conf *conf, uint32_t conf_sz)
{
	struct odm_dev *odm = NULL;

	RTE_SET_USED(conf_sz);

	odm = dev->fp_obj->dev_private;
	odm->num_qs = conf->nb_vchans;

	return 0;
}

static int
odm_dmadev_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
		       const struct rte_dma_vchan_conf *conf, uint32_t conf_sz)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	RTE_SET_USED(conf_sz);
	return odm_vchan_setup(odm, vchan, conf->nb_desc);
}

static int
odm_dmadev_start(struct rte_dma_dev *dev)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	return odm_enable(odm);
}

static int
odm_dmadev_stop(struct rte_dma_dev *dev)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	return odm_disable(odm);
}

static int
odm_dmadev_close(struct rte_dma_dev *dev)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	odm_disable(odm);
	odm_dev_fini(odm);

	return 0;
}

static int
odm_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst, uint32_t length,
		uint64_t flags)
{
	uint16_t pending_submit_len, pending_submit_cnt, iring_sz_available, iring_head;
	const int num_words = ODM_IRING_ENTRY_SIZE_MIN;
	struct odm_dev *odm = dev_private;
	uint64_t *iring_head_ptr;
	struct odm_queue *vq;
	uint64_t h;

	const union odm_instr_hdr_s hdr = {
		.s.ct = ODM_HDR_CT_CW_NC,
		.s.xtype = ODM_XTYPE_INTERNAL,
		.s.nfst = 1,
		.s.nlst = 1,
	};

	vq = &odm->vq[vchan];

	h = length;
	h |= ((uint64_t)length << 32);

	const uint16_t max_iring_words = vq->iring_max_words;

	iring_sz_available = vq->iring_sz_available;
	pending_submit_len = vq->pending_submit_len;
	pending_submit_cnt = vq->pending_submit_cnt;
	iring_head_ptr = vq->iring_mz->addr;
	iring_head = vq->iring_head;

	if (iring_sz_available < num_words)
		return -ENOSPC;

	if ((iring_head + num_words) >= max_iring_words) {

		iring_head_ptr[iring_head] = hdr.u;
		iring_head = (iring_head + 1) % max_iring_words;

		iring_head_ptr[iring_head] = h;
		iring_head = (iring_head + 1) % max_iring_words;

		iring_head_ptr[iring_head] = src;
		iring_head = (iring_head + 1) % max_iring_words;

		iring_head_ptr[iring_head] = dst;
		iring_head = (iring_head + 1) % max_iring_words;
	} else {
		iring_head_ptr[iring_head++] = hdr.u;
		iring_head_ptr[iring_head++] = h;
		iring_head_ptr[iring_head++] = src;
		iring_head_ptr[iring_head++] = dst;
	}

	pending_submit_len += num_words;

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		odm_write64(pending_submit_len, odm->rbase + ODM_VDMA_DBELL(vchan));
		vq->stats.submitted += pending_submit_cnt + 1;
		vq->pending_submit_len = 0;
		vq->pending_submit_cnt = 0;
	} else {
		vq->pending_submit_len = pending_submit_len;
		vq->pending_submit_cnt++;
	}

	vq->iring_head = iring_head;

	vq->iring_sz_available = iring_sz_available - num_words;

	/* No extra space to save. Skip entry in extra space ring. */
	vq->ins_ring_head = (vq->ins_ring_head + 1) % vq->cring_max_entry;

	return vq->desc_idx++;
}

static inline void
odm_dmadev_fill_sg(uint64_t *cmd, const struct rte_dma_sge *src, const struct rte_dma_sge *dst,
		   uint16_t nb_src, uint16_t nb_dst, union odm_instr_hdr_s *hdr)
{
	int i = 0, j = 0;
	uint64_t h = 0;

	cmd[j++] = hdr->u;
	/* When nb_src is even */
	if (!(nb_src & 0x1)) {
		/* Fill the iring with src pointers */
		for (i = 1; i < nb_src; i += 2) {
			h = ((uint64_t)src[i].length << 32) | src[i - 1].length;
			cmd[j++] = h;
			cmd[j++] = src[i - 1].addr;
			cmd[j++] = src[i].addr;
		}

		/* Fill the iring with dst pointers */
		for (i = 1; i < nb_dst; i += 2) {
			h = ((uint64_t)dst[i].length << 32) | dst[i - 1].length;
			cmd[j++] = h;
			cmd[j++] = dst[i - 1].addr;
			cmd[j++] = dst[i].addr;
		}

		/* Handle the last dst pointer when nb_dst is odd */
		if (nb_dst & 0x1) {
			h = dst[nb_dst - 1].length;
			cmd[j++] = h;
			cmd[j++] = dst[nb_dst - 1].addr;
			cmd[j++] = 0;
		}
	} else {
		/* When nb_src is odd */

		/* Fill the iring with src pointers */
		for (i = 1; i < nb_src; i += 2) {
			h = ((uint64_t)src[i].length << 32) | src[i - 1].length;
			cmd[j++] = h;
			cmd[j++] = src[i - 1].addr;
			cmd[j++] = src[i].addr;
		}

		/* Handle the last src pointer */
		h = ((uint64_t)dst[0].length << 32) | src[nb_src - 1].length;
		cmd[j++] = h;
		cmd[j++] = src[nb_src - 1].addr;
		cmd[j++] = dst[0].addr;

		/* Fill the iring with dst pointers */
		for (i = 2; i < nb_dst; i += 2) {
			h = ((uint64_t)dst[i].length << 32) | dst[i - 1].length;
			cmd[j++] = h;
			cmd[j++] = dst[i - 1].addr;
			cmd[j++] = dst[i].addr;
		}

		/* Handle the last dst pointer when nb_dst is even */
		if (!(nb_dst & 0x1)) {
			h = dst[nb_dst - 1].length;
			cmd[j++] = h;
			cmd[j++] = dst[nb_dst - 1].addr;
			cmd[j++] = 0;
		}
	}
}

static int
odm_dmadev_copy_sg(void *dev_private, uint16_t vchan, const struct rte_dma_sge *src,
		   const struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst, uint64_t flags)
{
	uint16_t pending_submit_len, pending_submit_cnt, iring_head, ins_ring_head;
	uint16_t iring_sz_available, i, nb, num_words;
	uint64_t cmd[ODM_IRING_ENTRY_SIZE_MAX];
	struct odm_dev *odm = dev_private;
	uint32_t s_sz = 0, d_sz = 0;
	uint64_t *iring_head_ptr;
	struct odm_queue *vq;
	union odm_instr_hdr_s hdr = {
		.s.ct = ODM_HDR_CT_CW_NC,
		.s.xtype = ODM_XTYPE_INTERNAL,
	};

	vq = &odm->vq[vchan];
	const uint16_t max_iring_words = vq->iring_max_words;

	iring_head_ptr = vq->iring_mz->addr;
	iring_head = vq->iring_head;
	iring_sz_available = vq->iring_sz_available;
	ins_ring_head = vq->ins_ring_head;
	pending_submit_len = vq->pending_submit_len;
	pending_submit_cnt = vq->pending_submit_cnt;

	if (unlikely(nb_src > 4 || nb_dst > 4))
		return -EINVAL;

	for (i = 0; i < nb_src; i++)
		s_sz += src[i].length;

	for (i = 0; i < nb_dst; i++)
		d_sz += dst[i].length;

	if (s_sz != d_sz)
		return -EINVAL;

	nb = nb_src + nb_dst;
	hdr.s.nfst = nb_src;
	hdr.s.nlst = nb_dst;
	num_words = 1 + 3 * (nb / 2 + (nb & 0x1));

	if (iring_sz_available < num_words)
		return -ENOSPC;

	if ((iring_head + num_words) >= max_iring_words) {
		uint16_t words_avail = max_iring_words - iring_head;
		uint16_t words_pend = num_words - words_avail;

		if (unlikely(words_avail + words_pend > ODM_IRING_ENTRY_SIZE_MAX))
			return -ENOSPC;

		odm_dmadev_fill_sg(cmd, src, dst, nb_src, nb_dst, &hdr);
		rte_memcpy((void *)&iring_head_ptr[iring_head], (void *)cmd, words_avail * 8);
		rte_memcpy((void *)iring_head_ptr, (void *)&cmd[words_avail], words_pend * 8);
		iring_head = words_pend;
	} else {
		odm_dmadev_fill_sg(&iring_head_ptr[iring_head], src, dst, nb_src, nb_dst, &hdr);
		iring_head += num_words;
	}

	pending_submit_len += num_words;

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		odm_write64(pending_submit_len, odm->rbase + ODM_VDMA_DBELL(vchan));
		vq->stats.submitted += pending_submit_cnt + 1;
		vq->pending_submit_len = 0;
		vq->pending_submit_cnt = 0;
	} else {
		vq->pending_submit_len = pending_submit_len;
		vq->pending_submit_cnt++;
	}

	vq->iring_head = iring_head;

	vq->iring_sz_available = iring_sz_available - num_words;

	/* Save extra space used for the instruction. */
	vq->extra_ins_sz[ins_ring_head] = num_words - 4;

	vq->ins_ring_head = (ins_ring_head + 1) % vq->cring_max_entry;

	return vq->desc_idx++;
}

static int
odm_dmadev_fill(void *dev_private, uint16_t vchan, uint64_t pattern, rte_iova_t dst,
		uint32_t length, uint64_t flags)
{
	uint16_t pending_submit_len, pending_submit_cnt, iring_sz_available, iring_head;
	const int num_words = ODM_IRING_ENTRY_SIZE_MIN;
	struct odm_dev *odm = dev_private;
	uint64_t *iring_head_ptr;
	struct odm_queue *vq;
	uint64_t h;

	vq = &odm->vq[vchan];

	union odm_instr_hdr_s hdr = {
		.s.ct = ODM_HDR_CT_CW_NC,
		.s.nfst = 0,
		.s.nlst = 1,
	};

	h = (uint64_t)length;

	switch (pattern) {
	case 0:
		hdr.s.xtype = ODM_XTYPE_FILL0;
		break;
	case 0xffffffffffffffff:
		hdr.s.xtype = ODM_XTYPE_FILL1;
		break;
	default:
		return -ENOTSUP;
	}

	const uint16_t max_iring_words = vq->iring_max_words;

	iring_sz_available = vq->iring_sz_available;
	pending_submit_len = vq->pending_submit_len;
	pending_submit_cnt = vq->pending_submit_cnt;
	iring_head_ptr = vq->iring_mz->addr;
	iring_head = vq->iring_head;

	if (iring_sz_available < num_words)
		return -ENOSPC;

	if ((iring_head + num_words) >= max_iring_words) {

		iring_head_ptr[iring_head] = hdr.u;
		iring_head = (iring_head + 1) % max_iring_words;

		iring_head_ptr[iring_head] = h;
		iring_head = (iring_head + 1) % max_iring_words;

		iring_head_ptr[iring_head] = dst;
		iring_head = (iring_head + 1) % max_iring_words;

		iring_head_ptr[iring_head] = 0;
		iring_head = (iring_head + 1) % max_iring_words;
	} else {
		iring_head_ptr[iring_head] = hdr.u;
		iring_head_ptr[iring_head + 1] = h;
		iring_head_ptr[iring_head + 2] = dst;
		iring_head_ptr[iring_head + 3] = 0;
		iring_head += num_words;
	}

	pending_submit_len += num_words;

	if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
		rte_wmb();
		odm_write64(pending_submit_len, odm->rbase + ODM_VDMA_DBELL(vchan));
		vq->stats.submitted += pending_submit_cnt + 1;
		vq->pending_submit_len = 0;
		vq->pending_submit_cnt = 0;
	} else {
		vq->pending_submit_len = pending_submit_len;
		vq->pending_submit_cnt++;
	}

	vq->iring_head = iring_head;
	vq->iring_sz_available = iring_sz_available - num_words;

	/* No extra space to save. Skip entry in extra space ring. */
	vq->ins_ring_head = (vq->ins_ring_head + 1) % vq->cring_max_entry;

	vq->iring_sz_available = iring_sz_available - num_words;

	return vq->desc_idx++;
}

static uint16_t
odm_dmadev_completed(void *dev_private, uint16_t vchan, const uint16_t nb_cpls, uint16_t *last_idx,
		     bool *has_error)
{
	const union odm_cmpl_ent_s cmpl_zero = {0};
	uint16_t cring_head, iring_sz_available;
	struct odm_dev *odm = dev_private;
	union odm_cmpl_ent_s cmpl;
	struct odm_queue *vq;
	uint64_t nb_err = 0;
	uint32_t *cmpl_ptr;
	int cnt;

	vq = &odm->vq[vchan];
	const uint32_t *base_addr = vq->cring_mz->addr;
	const uint16_t cring_max_entry = vq->cring_max_entry;

	cring_head = vq->cring_head;
	iring_sz_available = vq->iring_sz_available;

	if (unlikely(vq->stats.submitted == vq->stats.completed)) {
		*last_idx = (vq->stats.completed_offset + vq->stats.completed - 1) & 0xFFFF;
		return 0;
	}

	for (cnt = 0; cnt < nb_cpls; cnt++) {
		cmpl_ptr = RTE_PTR_ADD(base_addr, cring_head * sizeof(cmpl));
		cmpl.u = rte_atomic_load_explicit((RTE_ATOMIC(uint32_t) *)cmpl_ptr,
						  rte_memory_order_relaxed);
		if (!cmpl.s.valid)
			break;

		if (cmpl.s.cmp_code)
			nb_err++;

		/* Free space for enqueue */
		iring_sz_available += 4 + vq->extra_ins_sz[cring_head];

		/* Clear instruction extra space */
		vq->extra_ins_sz[cring_head] = 0;

		rte_atomic_store_explicit((RTE_ATOMIC(uint32_t) *)cmpl_ptr, cmpl_zero.u,
					  rte_memory_order_relaxed);
		cring_head = (cring_head + 1) % cring_max_entry;
	}

	vq->stats.errors += nb_err;

	if (unlikely(has_error != NULL && nb_err))
		*has_error = true;

	vq->cring_head = cring_head;
	vq->iring_sz_available = iring_sz_available;

	vq->stats.completed += cnt;

	*last_idx = (vq->stats.completed_offset + vq->stats.completed - 1) & 0xFFFF;

	return cnt;
}

static uint16_t
odm_dmadev_completed_status(void *dev_private, uint16_t vchan, const uint16_t nb_cpls,
			    uint16_t *last_idx, enum rte_dma_status_code *status)
{
	const union odm_cmpl_ent_s cmpl_zero = {0};
	uint16_t cring_head, iring_sz_available;
	struct odm_dev *odm = dev_private;
	union odm_cmpl_ent_s cmpl;
	struct odm_queue *vq;
	uint32_t *cmpl_ptr;
	int cnt;

	vq = &odm->vq[vchan];
	const uint32_t *base_addr = vq->cring_mz->addr;
	const uint16_t cring_max_entry = vq->cring_max_entry;

	cring_head = vq->cring_head;
	iring_sz_available = vq->iring_sz_available;

	if (vq->stats.submitted == vq->stats.completed) {
		*last_idx = (vq->stats.completed_offset + vq->stats.completed - 1) & 0xFFFF;
		return 0;
	}

#ifdef ODM_DEBUG
	ODM_LOG(DEBUG, "cring_head: 0x%" PRIx16, cring_head);
	ODM_LOG(DEBUG, "Submitted: 0x%" PRIx64, vq->stats.submitted);
	ODM_LOG(DEBUG, "Completed: 0x%" PRIx64, vq->stats.completed);
	ODM_LOG(DEBUG, "Hardware count: 0x%" PRIx64, odm_read64(odm->rbase + ODM_VDMA_CNT(vchan)));
#endif

	for (cnt = 0; cnt < nb_cpls; cnt++) {
		cmpl_ptr = RTE_PTR_ADD(base_addr, cring_head * sizeof(cmpl));
		cmpl.u = rte_atomic_load_explicit((RTE_ATOMIC(uint32_t) *)cmpl_ptr,
						  rte_memory_order_relaxed);
		if (!cmpl.s.valid)
			break;

		status[cnt] = cmpl.s.cmp_code;

		if (cmpl.s.cmp_code)
			vq->stats.errors++;

		/* Free space for enqueue */
		iring_sz_available += 4 + vq->extra_ins_sz[cring_head];

		/* Clear instruction extra space */
		vq->extra_ins_sz[cring_head] = 0;

		rte_atomic_store_explicit((RTE_ATOMIC(uint32_t) *)cmpl_ptr, cmpl_zero.u,
					  rte_memory_order_relaxed);
		cring_head = (cring_head + 1) % cring_max_entry;
	}

	vq->cring_head = cring_head;
	vq->iring_sz_available = iring_sz_available;

	vq->stats.completed += cnt;

	*last_idx = (vq->stats.completed_offset + vq->stats.completed - 1) & 0xFFFF;

	return cnt;
}

static int
odm_dmadev_submit(void *dev_private, uint16_t vchan)
{
	struct odm_dev *odm = dev_private;
	uint16_t pending_submit_len;
	struct odm_queue *vq;

	vq = &odm->vq[vchan];
	pending_submit_len = vq->pending_submit_len;

	if (pending_submit_len == 0)
		return 0;

	rte_wmb();
	odm_write64(pending_submit_len, odm->rbase + ODM_VDMA_DBELL(vchan));
	vq->pending_submit_len = 0;
	vq->stats.submitted += vq->pending_submit_cnt;
	vq->pending_submit_cnt = 0;

	return 0;
}

static uint16_t
odm_dmadev_burst_capacity(const void *dev_private, uint16_t vchan __rte_unused)
{
	const struct odm_dev *odm = dev_private;
	const struct odm_queue *vq;

	vq = &odm->vq[vchan];
	return (vq->iring_sz_available / ODM_IRING_ENTRY_SIZE_MIN);
}

static int
odm_stats_get(const struct rte_dma_dev *dev, uint16_t vchan, struct rte_dma_stats *rte_stats,
	      uint32_t size)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	if (size < sizeof(rte_stats))
		return -EINVAL;
	if (rte_stats == NULL)
		return -EINVAL;

	if (vchan != RTE_DMA_ALL_VCHAN) {
		struct rte_dma_stats *stats = (struct rte_dma_stats *)&odm->vq[vchan].stats;

		*rte_stats = *stats;
	} else {
		int i;

		for (i = 0; i < odm->num_qs; i++) {
			struct rte_dma_stats *stats = (struct rte_dma_stats *)&odm->vq[i].stats;

			rte_stats->submitted += stats->submitted;
			rte_stats->completed += stats->completed;
			rte_stats->errors += stats->errors;
		}
	}

	return 0;
}

static void
odm_vq_stats_reset(struct vq_stats *vq_stats)
{
	vq_stats->completed_offset += vq_stats->completed;
	vq_stats->completed = 0;
	vq_stats->errors = 0;
	vq_stats->submitted = 0;
}

static int
odm_stats_reset(struct rte_dma_dev *dev, uint16_t vchan)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;
	struct vq_stats *vq_stats;
	int i;

	if (vchan != RTE_DMA_ALL_VCHAN) {
		vq_stats = &odm->vq[vchan].stats;
		odm_vq_stats_reset(vq_stats);
	} else {
		for (i = 0; i < odm->num_qs; i++) {
			vq_stats = &odm->vq[i].stats;
			odm_vq_stats_reset(vq_stats);
		}
	}

	return 0;
}

static const struct rte_dma_dev_ops odm_dmadev_ops = {
	.dev_close = odm_dmadev_close,
	.dev_configure = odm_dmadev_configure,
	.dev_info_get = odm_dmadev_info_get,
	.dev_start = odm_dmadev_start,
	.dev_stop = odm_dmadev_stop,
	.stats_get = odm_stats_get,
	.stats_reset = odm_stats_reset,
	.vchan_setup = odm_dmadev_vchan_setup,
};

static int
odm_dmadev_probe(struct rte_pci_driver *pci_drv __rte_unused, struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];
	struct odm_dev *odm = NULL;
	struct rte_dma_dev *dmadev;
	int rc;

	if (!pci_dev->mem_resource[0].addr)
		return -ENODEV;

	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dmadev = rte_dma_pmd_allocate(name, pci_dev->device.numa_node, sizeof(*odm));
	if (dmadev == NULL) {
		ODM_LOG(ERR, "DMA device allocation failed for %s", name);
		return -ENOMEM;
	}

	ODM_LOG(INFO, "DMA device %s probed", name);
	odm = dmadev->data->dev_private;

	dmadev->device = &pci_dev->device;
	dmadev->fp_obj->dev_private = odm;
	dmadev->dev_ops = &odm_dmadev_ops;

	dmadev->fp_obj->copy = odm_dmadev_copy;
	dmadev->fp_obj->copy_sg = odm_dmadev_copy_sg;
	dmadev->fp_obj->fill = odm_dmadev_fill;
	dmadev->fp_obj->submit = odm_dmadev_submit;
	dmadev->fp_obj->completed = odm_dmadev_completed;
	dmadev->fp_obj->completed_status = odm_dmadev_completed_status;
	dmadev->fp_obj->burst_capacity = odm_dmadev_burst_capacity;

	odm->pci_dev = pci_dev;

	rc = odm_dev_init(odm);
	if (rc < 0)
		goto dma_pmd_release;

	return 0;

dma_pmd_release:
	rte_dma_pmd_release(name);

	return rc;
}

static int
odm_dmadev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];

	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	return rte_dma_pmd_release(name);
}

static const struct rte_pci_id odm_dma_pci_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_ODYSSEY_ODM_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver odm_dmadev = {
	.id_table = odm_dma_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = odm_dmadev_probe,
	.remove = odm_dmadev_remove,
};

RTE_PMD_REGISTER_PCI(PCI_DRIVER_NAME, odm_dmadev);
RTE_PMD_REGISTER_PCI_TABLE(PCI_DRIVER_NAME, odm_dma_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(PCI_DRIVER_NAME, "vfio-pci");
RTE_LOG_REGISTER_DEFAULT(odm_logtype, NOTICE);
