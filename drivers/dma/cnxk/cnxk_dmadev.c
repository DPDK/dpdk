/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2021 Marvell International Ltd.
 */

#include <string.h>
#include <unistd.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mempool.h>
#include <rte_pci.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>

#include <roc_api.h>
#include <cnxk_dmadev.h>

static int
cnxk_dmadev_info_get(const struct rte_dma_dev *dev,
		     struct rte_dma_info *dev_info, uint32_t size)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(size);

	dev_info->max_vchans = 1;
	dev_info->nb_vchans = 1;
	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
		RTE_DMA_CAPA_MEM_TO_DEV | RTE_DMA_CAPA_DEV_TO_MEM |
		RTE_DMA_CAPA_DEV_TO_DEV | RTE_DMA_CAPA_OPS_COPY |
		RTE_DMA_CAPA_OPS_COPY_SG;
	dev_info->max_desc = DPI_MAX_DESC;
	dev_info->min_desc = 1;
	dev_info->max_sges = DPI_MAX_POINTER;

	return 0;
}

static int
cnxk_dmadev_configure(struct rte_dma_dev *dev,
		      const struct rte_dma_conf *conf, uint32_t conf_sz)
{
	struct cnxk_dpi_vf_s *dpivf = NULL;
	int rc = 0;

	RTE_SET_USED(conf);
	RTE_SET_USED(conf);
	RTE_SET_USED(conf_sz);
	RTE_SET_USED(conf_sz);
	dpivf = dev->fp_obj->dev_private;
	rc = roc_dpi_configure(&dpivf->rdpi);
	if (rc < 0)
		plt_err("DMA configure failed err = %d", rc);

	return rc;
}

static int
cnxk_dmadev_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
			const struct rte_dma_vchan_conf *conf,
			uint32_t conf_sz)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	struct cnxk_dpi_compl_s *comp_data;
	union dpi_instr_hdr_s *header = &dpivf->conf.hdr;
	int i;

	RTE_SET_USED(vchan);
	RTE_SET_USED(conf_sz);

	header->s.pt = DPI_HDR_PT_ZBW_CA;

	switch (conf->direction) {
	case RTE_DMA_DIR_DEV_TO_MEM:
		header->s.xtype = DPI_XTYPE_INBOUND;
		header->s.lport = conf->src_port.pcie.coreid;
		header->s.fport = 0;
		header->s.pvfe = 1;
		break;
	case RTE_DMA_DIR_MEM_TO_DEV:
		header->s.xtype = DPI_XTYPE_OUTBOUND;
		header->s.lport = 0;
		header->s.fport = conf->dst_port.pcie.coreid;
		header->s.pvfe = 1;
		break;
	case RTE_DMA_DIR_MEM_TO_MEM:
		header->s.xtype = DPI_XTYPE_INTERNAL_ONLY;
		header->s.lport = 0;
		header->s.fport = 0;
		header->s.pvfe = 0;
		break;
	case RTE_DMA_DIR_DEV_TO_DEV:
		header->s.xtype = DPI_XTYPE_EXTERNAL_ONLY;
		header->s.lport = conf->src_port.pcie.coreid;
		header->s.fport = conf->dst_port.pcie.coreid;
	};

	for (i = 0; i < conf->nb_desc; i++) {
		comp_data = rte_zmalloc(NULL, sizeof(*comp_data), 0);
		if (comp_data == NULL) {
			plt_err("Failed to allocate for comp_data");
			return -ENOMEM;
		}
		dpivf->conf.c_desc.compl_ptr[i] = comp_data;
	};
	dpivf->conf.c_desc.max_cnt = DPI_MAX_DESC;
	dpivf->conf.c_desc.head = 0;
	dpivf->conf.c_desc.tail = 0;

	return 0;
}

static int
cnxk_dmadev_start(struct rte_dma_dev *dev)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;

	dpivf->desc_idx = 0;
	dpivf->num_words = 0;
	roc_dpi_enable(&dpivf->rdpi);

	return 0;
}

static int
cnxk_dmadev_stop(struct rte_dma_dev *dev)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;

	roc_dpi_disable(&dpivf->rdpi);

	return 0;
}

static int
cnxk_dmadev_close(struct rte_dma_dev *dev)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;

	roc_dpi_disable(&dpivf->rdpi);
	roc_dpi_dev_fini(&dpivf->rdpi);

	return 0;
}

static inline int
__dpi_queue_write(struct roc_dpi *dpi, uint64_t *cmds, int cmd_count)
{
	uint64_t *ptr = dpi->chunk_base;

	if ((cmd_count < DPI_MIN_CMD_SIZE) || (cmd_count > DPI_MAX_CMD_SIZE) ||
	    cmds == NULL)
		return -EINVAL;

	/*
	 * Normally there is plenty of room in the current buffer for the
	 * command
	 */
	if (dpi->chunk_head + cmd_count < dpi->pool_size_m1) {
		ptr += dpi->chunk_head;
		dpi->chunk_head += cmd_count;
		while (cmd_count--)
			*ptr++ = *cmds++;
	} else {
		int count;
		uint64_t *new_buff = dpi->chunk_next;

		dpi->chunk_next =
			(void *)roc_npa_aura_op_alloc(dpi->aura_handle, 0);
		if (!dpi->chunk_next) {
			plt_err("Failed to alloc next buffer from NPA");
			return -ENOMEM;
		}

		/*
		 * Figure out how many cmd words will fit in this buffer.
		 * One location will be needed for the next buffer pointer.
		 */
		count = dpi->pool_size_m1 - dpi->chunk_head;
		ptr += dpi->chunk_head;
		cmd_count -= count;
		while (count--)
			*ptr++ = *cmds++;

		/*
		 * chunk next ptr is 2 DWORDS
		 * second DWORD is reserved.
		 */
		*ptr++ = (uint64_t)new_buff;
		*ptr = 0;

		/*
		 * The current buffer is full and has a link to the next
		 * buffers. Time to write the rest of the commands into the new
		 * buffer.
		 */
		dpi->chunk_base = new_buff;
		dpi->chunk_head = cmd_count;
		ptr = new_buff;
		while (cmd_count--)
			*ptr++ = *cmds++;

		/* queue index may be greater than pool size */
		if (dpi->chunk_head >= dpi->pool_size_m1) {
			new_buff = dpi->chunk_next;
			dpi->chunk_next =
				(void *)roc_npa_aura_op_alloc(dpi->aura_handle,
							      0);
			if (!dpi->chunk_next) {
				plt_err("Failed to alloc next buffer from NPA");
				return -ENOMEM;
			}
			/* Write next buffer address */
			*ptr = (uint64_t)new_buff;
			dpi->chunk_base = new_buff;
			dpi->chunk_head = 0;
		}
	}

	return 0;
}

static int
cnxk_dmadev_copy(void *dev_private, uint16_t vchan, rte_iova_t src,
		 rte_iova_t dst, uint32_t length, uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	union dpi_instr_hdr_s *header = &dpivf->conf.hdr;
	struct cnxk_dpi_compl_s *comp_ptr;
	rte_iova_t fptr, lptr;
	int num_words = 0;
	int rc;

	RTE_SET_USED(vchan);

	comp_ptr = dpivf->conf.c_desc.compl_ptr[dpivf->conf.c_desc.tail];
	comp_ptr->cdata = DPI_REQ_CDATA;
	header->s.ptr = (uint64_t)comp_ptr;
	STRM_INC(dpivf->conf.c_desc);

	header->s.nfst = 1;
	header->s.nlst = 1;

	/*
	 * For inbound case, src pointers are last pointers.
	 * For all other cases, src pointers are first pointers.
	 */
	if (header->s.xtype == DPI_XTYPE_INBOUND) {
		fptr = dst;
		lptr = src;
	} else {
		fptr = src;
		lptr = dst;
	}

	dpivf->cmd[0] = header->u[0];
	dpivf->cmd[1] = header->u[1];
	dpivf->cmd[2] = header->u[2];
	/* word3 is always 0 */
	num_words += 4;
	dpivf->cmd[num_words++] = length;
	dpivf->cmd[num_words++] = fptr;
	dpivf->cmd[num_words++] = length;
	dpivf->cmd[num_words++] = lptr;

	rc = __dpi_queue_write(&dpivf->rdpi, dpivf->cmd, num_words);
	if (!rc) {
		if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
			rte_wmb();
			plt_write64(num_words,
				    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
			dpivf->stats.submitted++;
		}
		dpivf->num_words += num_words;
	}

	return dpivf->desc_idx++;
}

static int
cnxk_dmadev_copy_sg(void *dev_private, uint16_t vchan,
		    const struct rte_dma_sge *src,
		    const struct rte_dma_sge *dst,
		    uint16_t nb_src, uint16_t nb_dst, uint64_t flags)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	union dpi_instr_hdr_s *header = &dpivf->conf.hdr;
	const struct rte_dma_sge *fptr, *lptr;
	struct cnxk_dpi_compl_s *comp_ptr;
	int num_words = 0;
	int i, rc;

	RTE_SET_USED(vchan);

	comp_ptr = dpivf->conf.c_desc.compl_ptr[dpivf->conf.c_desc.tail];
	comp_ptr->cdata = DPI_REQ_CDATA;
	header->s.ptr = (uint64_t)comp_ptr;
	STRM_INC(dpivf->conf.c_desc);

	/*
	 * For inbound case, src pointers are last pointers.
	 * For all other cases, src pointers are first pointers.
	 */
	if (header->s.xtype == DPI_XTYPE_INBOUND) {
		header->s.nfst = nb_dst & 0xf;
		header->s.nlst = nb_src & 0xf;
		fptr = &dst[0];
		lptr = &src[0];
	} else {
		header->s.nfst = nb_src & 0xf;
		header->s.nlst = nb_dst & 0xf;
		fptr = &src[0];
		lptr = &dst[0];
	}

	dpivf->cmd[0] = header->u[0];
	dpivf->cmd[1] = header->u[1];
	dpivf->cmd[2] = header->u[2];
	num_words += 4;
	for (i = 0; i < header->s.nfst; i++) {
		dpivf->cmd[num_words++] = (uint64_t)fptr->length;
		dpivf->cmd[num_words++] = fptr->addr;
		fptr++;
	}

	for (i = 0; i < header->s.nlst; i++) {
		dpivf->cmd[num_words++] = (uint64_t)lptr->length;
		dpivf->cmd[num_words++] = lptr->addr;
		lptr++;
	}

	rc = __dpi_queue_write(&dpivf->rdpi, dpivf->cmd, num_words);
	if (!rc) {
		if (flags & RTE_DMA_OP_FLAG_SUBMIT) {
			rte_wmb();
			plt_write64(num_words,
				    dpivf->rdpi.rbase + DPI_VDMA_DBELL);
			dpivf->stats.submitted += nb_src;
		}
		dpivf->num_words += num_words;
	}

	return dpivf->desc_idx++;
}

static uint16_t
cnxk_dmadev_completed(void *dev_private, uint16_t vchan, const uint16_t nb_cpls,
		      uint16_t *last_idx, bool *has_error)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	int cnt;

	RTE_SET_USED(vchan);
	for (cnt = 0; cnt < nb_cpls; cnt++) {
		struct cnxk_dpi_compl_s *comp_ptr =
			dpivf->conf.c_desc.compl_ptr[cnt];

		if (comp_ptr->cdata) {
			*has_error = 1;
			dpivf->stats.errors++;
			break;
		}
	}

	*last_idx = cnt - 1;
	dpivf->conf.c_desc.tail = cnt;
	dpivf->stats.completed += cnt;

	return cnt;
}

static uint16_t
cnxk_dmadev_completed_status(void *dev_private, uint16_t vchan,
			     const uint16_t nb_cpls, uint16_t *last_idx,
			     enum rte_dma_status_code *status)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	int cnt;

	RTE_SET_USED(vchan);
	RTE_SET_USED(last_idx);
	for (cnt = 0; cnt < nb_cpls; cnt++) {
		struct cnxk_dpi_compl_s *comp_ptr =
			dpivf->conf.c_desc.compl_ptr[cnt];
		status[cnt] = comp_ptr->cdata;
		if (comp_ptr->cdata)
			dpivf->stats.errors++;
	}

	*last_idx = cnt - 1;
	dpivf->conf.c_desc.tail = 0;
	dpivf->stats.completed += cnt;

	return cnt;
}

static int
cnxk_dmadev_submit(void *dev_private, uint16_t vchan __rte_unused)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;

	rte_wmb();
	plt_write64(dpivf->num_words, dpivf->rdpi.rbase + DPI_VDMA_DBELL);
	dpivf->stats.submitted++;

	return 0;
}

static int
cnxk_stats_get(const struct rte_dma_dev *dev, uint16_t vchan,
	       struct rte_dma_stats *rte_stats, uint32_t size)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	struct rte_dma_stats *stats = &dpivf->stats;

	RTE_SET_USED(vchan);

	if (size < sizeof(rte_stats))
		return -EINVAL;
	if (rte_stats == NULL)
		return -EINVAL;

	*rte_stats = *stats;
	return 0;
}

static int
cnxk_stats_reset(struct rte_dma_dev *dev, uint16_t vchan __rte_unused)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;

	dpivf->stats = (struct rte_dma_stats){0};
	return 0;
}

static const struct rte_dma_dev_ops cnxk_dmadev_ops = {
	.dev_close = cnxk_dmadev_close,
	.dev_configure = cnxk_dmadev_configure,
	.dev_info_get = cnxk_dmadev_info_get,
	.dev_start = cnxk_dmadev_start,
	.dev_stop = cnxk_dmadev_stop,
	.stats_get = cnxk_stats_get,
	.stats_reset = cnxk_stats_reset,
	.vchan_setup = cnxk_dmadev_vchan_setup,
};

static int
cnxk_dmadev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		  struct rte_pci_device *pci_dev)
{
	struct cnxk_dpi_vf_s *dpivf = NULL;
	char name[RTE_DEV_NAME_MAX_LEN];
	struct rte_dma_dev *dmadev;
	struct roc_dpi *rdpi = NULL;
	int rc;

	if (!pci_dev->mem_resource[0].addr)
		return -ENODEV;

	rc = roc_plt_init();
	if (rc) {
		plt_err("Failed to initialize platform model, rc=%d", rc);
		return rc;
	}
	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dmadev = rte_dma_pmd_allocate(name, pci_dev->device.numa_node,
				      sizeof(*dpivf));
	if (dmadev == NULL) {
		plt_err("dma device allocation failed for %s", name);
		return -ENOMEM;
	}

	dpivf = dmadev->data->dev_private;

	dmadev->device = &pci_dev->device;
	dmadev->fp_obj->dev_private = dpivf;
	dmadev->dev_ops = &cnxk_dmadev_ops;

	dmadev->fp_obj->copy = cnxk_dmadev_copy;
	dmadev->fp_obj->copy_sg = cnxk_dmadev_copy_sg;
	dmadev->fp_obj->submit = cnxk_dmadev_submit;
	dmadev->fp_obj->completed = cnxk_dmadev_completed;
	dmadev->fp_obj->completed_status = cnxk_dmadev_completed_status;

	rdpi = &dpivf->rdpi;

	rdpi->pci_dev = pci_dev;
	rc = roc_dpi_dev_init(rdpi);
	if (rc < 0)
		goto err_out_free;

	return 0;

err_out_free:
	if (dmadev)
		rte_dma_pmd_release(name);

	return rc;
}

static int
cnxk_dmadev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];

	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	return rte_dma_pmd_release(name);
}

static const struct rte_pci_id cnxk_dma_pci_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_CNXK_DPI_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cnxk_dmadev = {
	.id_table  = cnxk_dma_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe     = cnxk_dmadev_probe,
	.remove    = cnxk_dmadev_remove,
};

RTE_PMD_REGISTER_PCI(cnxk_dmadev_pci_driver, cnxk_dmadev);
RTE_PMD_REGISTER_PCI_TABLE(cnxk_dmadev_pci_driver, cnxk_dma_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(cnxk_dmadev_pci_driver, "vfio-pci");
