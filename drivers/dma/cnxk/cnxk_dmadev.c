/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2021 Marvell International Ltd.
 */

#include <cnxk_dmadev.h>

static int cnxk_stats_reset(struct rte_dma_dev *dev, uint16_t vchan);

static int
cnxk_dmadev_info_get(const struct rte_dma_dev *dev, struct rte_dma_info *dev_info, uint32_t size)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	RTE_SET_USED(size);

	dev_info->max_vchans = CNXK_DPI_MAX_VCHANS_PER_QUEUE;
	dev_info->nb_vchans = dpivf->num_vchans;
	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM | RTE_DMA_CAPA_MEM_TO_DEV |
			     RTE_DMA_CAPA_DEV_TO_MEM | RTE_DMA_CAPA_DEV_TO_DEV |
			     RTE_DMA_CAPA_OPS_COPY | RTE_DMA_CAPA_OPS_COPY_SG |
			     RTE_DMA_CAPA_M2D_AUTO_FREE;
	dev_info->max_desc = CNXK_DPI_MAX_DESC;
	dev_info->min_desc = CNXK_DPI_MIN_DESC;
	dev_info->max_sges = CNXK_DPI_MAX_POINTER;

	return 0;
}

static int
cnxk_dmadev_vchan_free(struct cnxk_dpi_vf_s *dpivf, uint16_t vchan)
{
	struct cnxk_dpi_conf *dpi_conf;
	uint16_t num_vchans;
	uint16_t max_desc;
	int i, j;

	if (vchan == RTE_DMA_ALL_VCHAN) {
		num_vchans = dpivf->num_vchans;
		i = 0;
	} else {
		if (vchan >= CNXK_DPI_MAX_VCHANS_PER_QUEUE)
			return -EINVAL;

		num_vchans = vchan + 1;
		i = vchan;
	}

	for (; i < num_vchans; i++) {
		dpi_conf = &dpivf->conf[i];
		max_desc = dpi_conf->c_desc.max_cnt + 1;
		if (dpi_conf->c_desc.compl_ptr) {
			for (j = 0; j < max_desc; j++)
				rte_free(dpi_conf->c_desc.compl_ptr[j]);
		}

		rte_free(dpi_conf->c_desc.compl_ptr);
		dpi_conf->c_desc.compl_ptr = NULL;
	}

	return 0;
}

static int
cnxk_dmadev_chunk_pool_create(struct rte_dma_dev *dev, uint32_t nb_chunks, uint32_t chunk_sz)
{
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	struct cnxk_dpi_vf_s *dpivf = NULL;
	int rc;

	dpivf = dev->fp_obj->dev_private;
	/* Create chunk pool. */
	snprintf(pool_name, sizeof(pool_name), "cnxk_dma_chunk_pool%d", dev->data->dev_id);

	nb_chunks += (CNXK_DPI_POOL_MAX_CACHE_SZ * rte_lcore_count());
	dpivf->chunk_pool = rte_mempool_create_empty(
		pool_name, nb_chunks, chunk_sz, CNXK_DPI_POOL_MAX_CACHE_SZ, 0, rte_socket_id(), 0);

	if (dpivf->chunk_pool == NULL) {
		plt_err("Unable to create chunkpool.");
		return -ENOMEM;
	}

	rc = rte_mempool_set_ops_byname(dpivf->chunk_pool, rte_mbuf_platform_mempool_ops(), NULL);
	if (rc < 0) {
		plt_err("Unable to set chunkpool ops");
		goto free;
	}

	rc = rte_mempool_populate_default(dpivf->chunk_pool);
	if (rc < 0) {
		plt_err("Unable to set populate chunkpool.");
		goto free;
	}
	dpivf->aura = roc_npa_aura_handle_to_aura(dpivf->chunk_pool->pool_id);

	return 0;

free:
	rte_mempool_free(dpivf->chunk_pool);
	return rc;
}

static int
cnxk_dmadev_configure(struct rte_dma_dev *dev, const struct rte_dma_conf *conf, uint32_t conf_sz)
{
	struct cnxk_dpi_vf_s *dpivf = NULL;

	RTE_SET_USED(conf_sz);
	dpivf = dev->fp_obj->dev_private;

	/* After config function, vchan setup function has to be called.
	 * Free up vchan memory if any, before configuring num_vchans.
	 */
	cnxk_dmadev_vchan_free(dpivf, RTE_DMA_ALL_VCHAN);
	dpivf->num_vchans = conf->nb_vchans;

	return 0;
}

static int
dmadev_src_buf_aura_get(struct rte_mempool *sb_mp, const char *mp_ops_name)
{
	struct rte_mempool_ops *ops;

	if (sb_mp == NULL)
		return 0;

	ops = rte_mempool_get_ops(sb_mp->ops_index);
	if (strcmp(ops->name, mp_ops_name) != 0)
		return -EINVAL;

	return roc_npa_aura_handle_to_aura(sb_mp->pool_id);
}

static int
cn9k_dmadev_setup_hdr(union cnxk_dpi_instr_cmd *header, const struct rte_dma_vchan_conf *conf)
{
	int aura;

	header->cn9k.pt = DPI_HDR_PT_ZBW_CA;

	switch (conf->direction) {
	case RTE_DMA_DIR_DEV_TO_MEM:
		header->cn9k.xtype = DPI_XTYPE_INBOUND;
		header->cn9k.lport = conf->src_port.pcie.coreid;
		header->cn9k.fport = 0;
		header->cn9k.pvfe = conf->src_port.pcie.vfen;
		if (header->cn9k.pvfe) {
			header->cn9k.func = conf->src_port.pcie.pfid << 12;
			header->cn9k.func |= conf->src_port.pcie.vfid;
		}
		break;
	case RTE_DMA_DIR_MEM_TO_DEV:
		header->cn9k.xtype = DPI_XTYPE_OUTBOUND;
		header->cn9k.lport = 0;
		header->cn9k.fport = conf->dst_port.pcie.coreid;
		header->cn9k.pvfe = conf->dst_port.pcie.vfen;
		if (header->cn9k.pvfe) {
			header->cn9k.func = conf->dst_port.pcie.pfid << 12;
			header->cn9k.func |= conf->dst_port.pcie.vfid;
		}
		aura = dmadev_src_buf_aura_get(conf->auto_free.m2d.pool, "cn9k_mempool_ops");
		if (aura < 0)
			return aura;
		header->cn9k.aura = aura;
		header->cn9k.ii = 1;
		break;
	case RTE_DMA_DIR_MEM_TO_MEM:
		header->cn9k.xtype = DPI_XTYPE_INTERNAL_ONLY;
		header->cn9k.lport = 0;
		header->cn9k.fport = 0;
		header->cn9k.pvfe = 0;
		break;
	case RTE_DMA_DIR_DEV_TO_DEV:
		header->cn9k.xtype = DPI_XTYPE_EXTERNAL_ONLY;
		header->cn9k.lport = conf->src_port.pcie.coreid;
		header->cn9k.fport = conf->dst_port.pcie.coreid;
		header->cn9k.pvfe = 0;
	};

	return 0;
}

static int
cn10k_dmadev_setup_hdr(union cnxk_dpi_instr_cmd *header, const struct rte_dma_vchan_conf *conf)
{
	int aura;

	header->cn10k.pt = DPI_HDR_PT_ZBW_CA;

	switch (conf->direction) {
	case RTE_DMA_DIR_DEV_TO_MEM:
		header->cn10k.xtype = DPI_XTYPE_INBOUND;
		header->cn10k.lport = conf->src_port.pcie.coreid;
		header->cn10k.fport = 0;
		header->cn10k.pvfe = conf->src_port.pcie.vfen;
		if (header->cn10k.pvfe) {
			header->cn10k.func = conf->src_port.pcie.pfid << 12;
			header->cn10k.func |= conf->src_port.pcie.vfid;
		}
		break;
	case RTE_DMA_DIR_MEM_TO_DEV:
		header->cn10k.xtype = DPI_XTYPE_OUTBOUND;
		header->cn10k.lport = 0;
		header->cn10k.fport = conf->dst_port.pcie.coreid;
		header->cn10k.pvfe = conf->dst_port.pcie.vfen;
		if (header->cn10k.pvfe) {
			header->cn10k.func = conf->dst_port.pcie.pfid << 12;
			header->cn10k.func |= conf->dst_port.pcie.vfid;
		}
		aura = dmadev_src_buf_aura_get(conf->auto_free.m2d.pool, "cn10k_mempool_ops");
		if (aura < 0)
			return aura;
		header->cn10k.aura = aura;
		break;
	case RTE_DMA_DIR_MEM_TO_MEM:
		header->cn10k.xtype = DPI_XTYPE_INTERNAL_ONLY;
		header->cn10k.lport = 0;
		header->cn10k.fport = 0;
		header->cn10k.pvfe = 0;
		break;
	case RTE_DMA_DIR_DEV_TO_DEV:
		header->cn10k.xtype = DPI_XTYPE_EXTERNAL_ONLY;
		header->cn10k.lport = conf->src_port.pcie.coreid;
		header->cn10k.fport = conf->dst_port.pcie.coreid;
		header->cn10k.pvfe = 0;
	};

	return 0;
}

static int
cnxk_dmadev_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
			const struct rte_dma_vchan_conf *conf, uint32_t conf_sz)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	union cnxk_dpi_instr_cmd *header;
	uint16_t max_desc;
	uint32_t size;
	int i, ret;

	RTE_SET_USED(conf_sz);

	header = (union cnxk_dpi_instr_cmd *)&dpi_conf->cmd.u;

	if (dpivf->is_cn10k)
		ret = cn10k_dmadev_setup_hdr(header, conf);
	else
		ret = cn9k_dmadev_setup_hdr(header, conf);

	if (ret)
		return ret;

	/* Free up descriptor memory before allocating. */
	cnxk_dmadev_vchan_free(dpivf, vchan);

	max_desc = conf->nb_desc;
	if (!rte_is_power_of_2(max_desc))
		max_desc = rte_align32pow2(max_desc);

	if (max_desc > CNXK_DPI_MAX_DESC)
		max_desc = CNXK_DPI_MAX_DESC;

	size = (max_desc * sizeof(struct cnxk_dpi_compl_s *));
	dpi_conf->c_desc.compl_ptr = rte_zmalloc(NULL, size, 0);

	if (dpi_conf->c_desc.compl_ptr == NULL) {
		plt_err("Failed to allocate for comp_data");
		return -ENOMEM;
	}

	for (i = 0; i < max_desc; i++) {
		dpi_conf->c_desc.compl_ptr[i] =
			rte_zmalloc(NULL, sizeof(struct cnxk_dpi_compl_s), 0);
		if (!dpi_conf->c_desc.compl_ptr[i]) {
			plt_err("Failed to allocate for descriptor memory");
			return -ENOMEM;
		}

		dpi_conf->c_desc.compl_ptr[i]->cdata = CNXK_DPI_REQ_CDATA;
	}

	dpi_conf->c_desc.max_cnt = (max_desc - 1);

	return 0;
}

static int
cnxk_dmadev_start(struct rte_dma_dev *dev)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	struct cnxk_dpi_conf *dpi_conf;
	uint32_t chunks, nb_desc = 0;
	int i, j, rc = 0;
	void *chunk;

	for (i = 0; i < dpivf->num_vchans; i++) {
		dpi_conf = &dpivf->conf[i];
		dpi_conf->c_desc.head = 0;
		dpi_conf->c_desc.tail = 0;
		dpi_conf->pnum_words = 0;
		dpi_conf->pending = 0;
		dpi_conf->desc_idx = 0;
		for (j = 0; j < dpi_conf->c_desc.max_cnt + 1; j++) {
			if (dpi_conf->c_desc.compl_ptr[j])
				dpi_conf->c_desc.compl_ptr[j]->cdata = CNXK_DPI_REQ_CDATA;
		}
		nb_desc += dpi_conf->c_desc.max_cnt + 1;
		cnxk_stats_reset(dev, i);
		dpi_conf->completed_offset = 0;
	}

	chunks = CNXK_DPI_CHUNKS_FROM_DESC(CNXK_DPI_QUEUE_BUF_SIZE, nb_desc);
	rc = cnxk_dmadev_chunk_pool_create(dev, chunks, CNXK_DPI_QUEUE_BUF_SIZE);
	if (rc < 0) {
		plt_err("DMA pool configure failed err = %d", rc);
		goto done;
	}

	rc = rte_mempool_get(dpivf->chunk_pool, &chunk);
	if (rc < 0) {
		plt_err("DMA failed to get chunk pointer err = %d", rc);
		rte_mempool_free(dpivf->chunk_pool);
		goto done;
	}

	rc = roc_dpi_configure(&dpivf->rdpi, CNXK_DPI_QUEUE_BUF_SIZE, dpivf->aura, (uint64_t)chunk);
	if (rc < 0) {
		plt_err("DMA configure failed err = %d", rc);
		rte_mempool_free(dpivf->chunk_pool);
		goto done;
	}

	dpivf->chunk_base = chunk;
	dpivf->chunk_head = 0;
	dpivf->chunk_size_m1 = (CNXK_DPI_QUEUE_BUF_SIZE >> 3) - 2;

	roc_dpi_enable(&dpivf->rdpi);

done:
	return rc;
}

static int
cnxk_dmadev_stop(struct rte_dma_dev *dev)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	uint64_t reg;

	reg = plt_read64(dpivf->rdpi.rbase + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(63)))
		reg = plt_read64(dpivf->rdpi.rbase + DPI_VDMA_SADDR);

	roc_dpi_disable(&dpivf->rdpi);
	rte_mempool_free(dpivf->chunk_pool);
	dpivf->chunk_pool = NULL;
	dpivf->chunk_base = NULL;
	dpivf->chunk_size_m1 = 0;

	return 0;
}

static int
cnxk_dmadev_close(struct rte_dma_dev *dev)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;

	roc_dpi_disable(&dpivf->rdpi);
	cnxk_dmadev_vchan_free(dpivf, RTE_DMA_ALL_VCHAN);
	roc_dpi_dev_fini(&dpivf->rdpi);

	/* Clear all flags as we close the device. */
	dpivf->flag = 0;

	return 0;
}

static uint16_t
cnxk_dmadev_completed(void *dev_private, uint16_t vchan, const uint16_t nb_cpls, uint16_t *last_idx,
		      bool *has_error)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	struct cnxk_dpi_cdesc_data_s *c_desc = &dpi_conf->c_desc;
	struct cnxk_dpi_compl_s *comp_ptr;
	int cnt;

	for (cnt = 0; cnt < nb_cpls; cnt++) {
		comp_ptr = c_desc->compl_ptr[c_desc->head];

		if (comp_ptr->cdata) {
			if (comp_ptr->cdata == CNXK_DPI_REQ_CDATA)
				break;
			*has_error = 1;
			dpi_conf->stats.errors++;
			CNXK_DPI_STRM_INC(*c_desc, head);
			break;
		}

		comp_ptr->cdata = CNXK_DPI_REQ_CDATA;
		CNXK_DPI_STRM_INC(*c_desc, head);
	}

	dpi_conf->stats.completed += cnt;
	*last_idx = (dpi_conf->completed_offset + dpi_conf->stats.completed - 1) & 0xffff;

	return cnt;
}

static uint16_t
cnxk_dmadev_completed_status(void *dev_private, uint16_t vchan, const uint16_t nb_cpls,
			     uint16_t *last_idx, enum rte_dma_status_code *status)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	struct cnxk_dpi_cdesc_data_s *c_desc = &dpi_conf->c_desc;
	struct cnxk_dpi_compl_s *comp_ptr;
	int cnt;

	for (cnt = 0; cnt < nb_cpls; cnt++) {
		comp_ptr = c_desc->compl_ptr[c_desc->head];
		status[cnt] = comp_ptr->cdata;
		if (status[cnt]) {
			if (status[cnt] == CNXK_DPI_REQ_CDATA)
				break;

			dpi_conf->stats.errors++;
		}
		comp_ptr->cdata = CNXK_DPI_REQ_CDATA;
		CNXK_DPI_STRM_INC(*c_desc, head);
	}

	dpi_conf->stats.completed += cnt;
	*last_idx = (dpi_conf->completed_offset + dpi_conf->stats.completed - 1) & 0xffff;

	return cnt;
}

static uint16_t
cnxk_damdev_burst_capacity(const void *dev_private, uint16_t vchan)
{
	const struct cnxk_dpi_vf_s *dpivf = (const struct cnxk_dpi_vf_s *)dev_private;
	const struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	uint16_t burst_cap;

	burst_cap = dpi_conf->c_desc.max_cnt -
		    ((dpi_conf->stats.submitted - dpi_conf->stats.completed) + dpi_conf->pending) +
		    1;

	return burst_cap;
}

static int
cnxk_dmadev_submit(void *dev_private, uint16_t vchan)
{
	struct cnxk_dpi_vf_s *dpivf = dev_private;
	struct cnxk_dpi_conf *dpi_conf = &dpivf->conf[vchan];
	uint32_t num_words = dpi_conf->pnum_words;

	if (!dpi_conf->pnum_words)
		return 0;

	rte_wmb();
	plt_write64(num_words, dpivf->rdpi.rbase + DPI_VDMA_DBELL);

	dpi_conf->stats.submitted += dpi_conf->pending;
	dpi_conf->pnum_words = 0;
	dpi_conf->pending = 0;

	return 0;
}

static int
cnxk_stats_get(const struct rte_dma_dev *dev, uint16_t vchan, struct rte_dma_stats *rte_stats,
	       uint32_t size)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	struct cnxk_dpi_conf *dpi_conf;
	int i;

	if (size < sizeof(rte_stats))
		return -EINVAL;
	if (rte_stats == NULL)
		return -EINVAL;

	/* Stats of all vchans requested. */
	if (vchan == RTE_DMA_ALL_VCHAN) {
		for (i = 0; i < dpivf->num_vchans; i++) {
			dpi_conf = &dpivf->conf[i];
			rte_stats->submitted += dpi_conf->stats.submitted;
			rte_stats->completed += dpi_conf->stats.completed;
			rte_stats->errors += dpi_conf->stats.errors;
		}

		goto done;
	}

	if (vchan >= CNXK_DPI_MAX_VCHANS_PER_QUEUE)
		return -EINVAL;

	dpi_conf = &dpivf->conf[vchan];
	*rte_stats = dpi_conf->stats;

done:
	return 0;
}

static int
cnxk_stats_reset(struct rte_dma_dev *dev, uint16_t vchan)
{
	struct cnxk_dpi_vf_s *dpivf = dev->fp_obj->dev_private;
	struct cnxk_dpi_conf *dpi_conf;
	int i;

	/* clear stats of all vchans. */
	if (vchan == RTE_DMA_ALL_VCHAN) {
		for (i = 0; i < dpivf->num_vchans; i++) {
			dpi_conf = &dpivf->conf[i];
			dpi_conf->completed_offset += dpi_conf->stats.completed;
			dpi_conf->stats = (struct rte_dma_stats){0};
		}

		return 0;
	}

	if (vchan >= CNXK_DPI_MAX_VCHANS_PER_QUEUE)
		return -EINVAL;

	dpi_conf = &dpivf->conf[vchan];
	dpi_conf->completed_offset += dpi_conf->stats.completed;
	dpi_conf->stats = (struct rte_dma_stats){0};

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
cnxk_dmadev_probe(struct rte_pci_driver *pci_drv __rte_unused, struct rte_pci_device *pci_dev)
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

	dmadev = rte_dma_pmd_allocate(name, pci_dev->device.numa_node, sizeof(*dpivf));
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
	dmadev->fp_obj->burst_capacity = cnxk_damdev_burst_capacity;

	if (roc_model_is_cn10k()) {
		dpivf->is_cn10k = true;
		dmadev->fp_obj->copy = cn10k_dmadev_copy;
		dmadev->fp_obj->copy_sg = cn10k_dmadev_copy_sg;
	}

	rdpi = &dpivf->rdpi;

	rdpi->pci_dev = pci_dev;
	rc = roc_dpi_dev_init(rdpi);
	if (rc < 0)
		goto err_out_free;

	dmadev->state = RTE_DMA_DEV_READY;

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
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNXK_DPI_VF)},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cnxk_dmadev = {
	.id_table = cnxk_dma_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cnxk_dmadev_probe,
	.remove = cnxk_dmadev_remove,
};

RTE_PMD_REGISTER_PCI(cnxk_dmadev_pci_driver, cnxk_dmadev);
RTE_PMD_REGISTER_PCI_TABLE(cnxk_dmadev_pci_driver, cnxk_dma_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(cnxk_dmadev_pci_driver, "vfio-pci");
