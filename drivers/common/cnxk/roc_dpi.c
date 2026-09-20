/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

#include "roc_api.h"
#include "roc_priv.h"

#define ROC_DPI_DEV_NAME     "roc_dpi_dev_"
#define ROC_DPI_DEV_NAME_LEN (sizeof(ROC_DPI_DEV_NAME) + PCI_PRI_STR_SIZE)

#define DPI_PF_MBOX_SYSFS_ENTRY "dpi_device_config"

static inline int
send_msg_to_pf(struct plt_pci_addr *pci_addr, const char *value, int size)
{
	char buf[255] = {0};
	int res, fd;

	res = snprintf(
		buf, sizeof(buf), "/sys/bus/pci/devices/" PCI_PRI_FMT "/%s",
		pci_addr->domain, pci_addr->bus, DPI_PF_DBDF_DEVICE & 0x7,
		DPI_PF_DBDF_FUNCTION & 0x7, DPI_PF_MBOX_SYSFS_ENTRY);

	if ((res < 0) || ((size_t)res > sizeof(buf)))
		return -ERANGE;

	fd = open(buf, O_WRONLY);
	if (fd < 0)
		return -EACCES;

	res = write(fd, value, size);
	close(fd);
	if (res < 0)
		return -EACCES;

	return 0;
}

int
roc_dpi_wait_queue_idle(struct roc_dpi *roc_dpi)
{
	const uint64_t cyc = (DPI_QUEUE_IDLE_TMO_MS * plt_tsc_hz()) / 1E3;
	const uint64_t start = plt_tsc_cycles();
	uint64_t reg;

	/* Wait for SADDR to become idle */
	reg = plt_read64(roc_dpi->rbase + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(63))) {
		reg = plt_read64(roc_dpi->rbase + DPI_VDMA_SADDR);
		if (plt_tsc_cycles() - start == cyc)
			return -ETIMEDOUT;
	}

	return 0;
}

void
dpi_lf_ena_dis(struct roc_dpi_lf *lf, uint8_t enb)
{
	uint64_t reg;
	int ring_idx;

	for (ring_idx = 0; ring_idx < ROC_DPI_LF_RINGS; ring_idx++) {
		reg = plt_read64(lf->rbase + DPI_LF_RINGX_CFG(ring_idx));

		if (enb)
			reg |= DPI_LF_QCFG_QEN;
		else
			reg &= ~DPI_LF_QCFG_QEN;

		plt_write64(reg, lf->rbase + DPI_LF_RINGX_CFG(ring_idx));
	}
}

int
dpi_lf_reset(struct roc_dpi_lf *lf)
{
	uint64_t start_cycle;
	uint64_t wait_cycles;
	uintptr_t reg_addr;
	int ring_idx;

	wait_cycles = (DPI_LF_RESET_TMO_US * plt_tsc_hz()) / 1000000;

	for (ring_idx = 0; ring_idx < ROC_DPI_LF_RINGS; ring_idx++) {
		reg_addr = lf->rbase + DPI_LF_RINGX_RST(ring_idx);
		plt_write64(DPI_LF_QUEUE_RST, reg_addr);

		start_cycle = plt_tsc_cycles();
		while (plt_read64(reg_addr) & DPI_LF_QUEUE_RST) {
			if (plt_tsc_cycles() - start_cycle >= wait_cycles) {
				plt_err("DPI LF[%u]: ring[%u] reset timed out", lf->slot, ring_idx);
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

int
roc_dpi_access_pair_group_create(struct roc_dpi_lf *lf, plt_uuid_t domain_id, plt_uuid_t token,
				 int16_t *group_id)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_access_group_alloc_req *req;
	struct dpi_lf_access_group_alloc_rsp *rsp;
	int rc;

	req = mbox_alloc_msg_dpi_lf_access_group_alloc(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->dpi_blkaddr = lf->blk_addr;
	mbox_memcpy(req->lf_handle, domain_id, sizeof(plt_uuid_t));
	mbox_memcpy(req->access_key, token, sizeof(plt_uuid_t));

	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc)
		goto exit;

	*group_id = rsp->group_id;
	lf->group_id = rsp->group_id;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_access_pair_group_destroy(struct roc_dpi_lf *lf, int16_t group_id)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_access_group_free_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_access_group_free(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->dpi_blkaddr = lf->blk_addr;
	req->group_id = group_id;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_access_pair_group_join(struct roc_dpi_lf *lf, plt_uuid_t domain_id, plt_uuid_t token,
			       int16_t group_id)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_access_group_join_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_access_group_join(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	mbox_memcpy(req->lf_handle, domain_id, sizeof(plt_uuid_t));
	mbox_memcpy(req->access_key, token, sizeof(plt_uuid_t));
	req->dpi_blkaddr = lf->blk_addr;
	req->group_id = group_id;

	rc = mbox_process(mbox);
	if (rc)
		goto exit;

	lf->group_id = group_id;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_access_pair_group_leave(struct roc_dpi_lf *lf, int16_t group_id)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_access_group_leave_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_access_group_leave(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->dpi_blkaddr = lf->blk_addr;
	req->group_id = group_id;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_access_pair_group_handler_get(struct roc_dpi_lf *lf, int16_t group_id, plt_uuid_t domain_id,
				      uint16_t *handler)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_access_group_handle_get_req *req;
	struct dpi_lf_access_group_handle_get_rsp *rsp;
	int rc;

	req = mbox_alloc_msg_dpi_lf_access_group_handle_get(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	mbox_memcpy(req->lf_handle, domain_id, sizeof(plt_uuid_t));
	req->dpi_blkaddr = lf->blk_addr;
	req->group_id = group_id;

	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc)
		goto exit;

	*handler = rsp->handle;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_reset(struct roc_dpi *dpi)
{
	uint16_t i;
	int rc = 0;

	if (roc_model_is_cn20k()) {
		for (i = 0; i < dpi->nr_lfs; i++) {
			rc |= dpi_lf_reset(&dpi->lfs[i]);
			if (rc)
				plt_err("Reset failed for DPI LF - %u", i);
		}
	}

	return rc;
}

int
roc_dpi_enable(struct roc_dpi *roc_dpi)
{
	uint16_t i;

	if (roc_model_is_cn20k()) {
		for (i = 0; i < roc_dpi->nr_lfs; i++)
			dpi_lf_ena_dis(&roc_dpi->lfs[i], true);
	} else {
		plt_write64(0x1, roc_dpi->rbase + DPI_VDMA_EN);
	}

	return 0;
}

int
roc_dpi_disable(struct roc_dpi *roc_dpi)
{
	uint16_t i;

	if (roc_model_is_cn20k()) {
		for (i = 0; i < roc_dpi->nr_lfs; i++)
			dpi_lf_ena_dis(&roc_dpi->lfs[i], false);
	} else {
		plt_write64(0x0, roc_dpi->rbase + DPI_VDMA_EN);
	}

	return 0;
}

int
roc_dpi_configure(struct roc_dpi *roc_dpi, uint32_t chunk_sz, uint64_t aura, uint64_t chunk_base)
{
	struct plt_pci_device *pci_dev;
	dpi_mbox_msg_t mbox_msg;
	int rc;

	if (!roc_dpi) {
		plt_err("roc_dpi is NULL");
		return -EINVAL;
	}

	pci_dev = roc_dpi->pci_dev;

	roc_dpi_disable(roc_dpi);
	rc = roc_dpi_wait_queue_idle(roc_dpi);
	if (rc)
		return rc;

	plt_write64(0x0, roc_dpi->rbase + DPI_VDMA_REQQ_CTL);
	plt_write64(chunk_base, roc_dpi->rbase + DPI_VDMA_SADDR);
	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	/* DPI PF driver expects vfid starts from index 0 */
	mbox_msg.s.vfid = roc_dpi->vfid;
	mbox_msg.s.pri = roc_dpi->priority;
	mbox_msg.s.cmd = DPI_QUEUE_OPEN;
	mbox_msg.s.csize = chunk_sz;
	mbox_msg.s.aura = aura;
	mbox_msg.s.sso_pf_func = idev_sso_pffunc_get();
	mbox_msg.s.npa_pf_func = idev_npa_pffunc_get();
	mbox_msg.s.wqecsoff = idev_dma_cs_offset_get();
	if (mbox_msg.s.wqecsoff)
		mbox_msg.s.wqecs = 1;

	rc = send_msg_to_pf(&pci_dev->addr, (const char *)&mbox_msg, sizeof(dpi_mbox_msg_t));
	if (rc < 0)
		plt_err("Failed to send mbox message %d to DPI PF, err %d", mbox_msg.s.cmd, rc);

	return rc;
}

int
roc_dpi_configure_v2(struct roc_dpi *roc_dpi, uint32_t chunk_sz, uint64_t aura, uint64_t chunk_base)
{
	struct plt_pci_device *pci_dev;
	dpi_mbox_msg_t mbox_msg;
	int rc;

	if (!roc_dpi) {
		plt_err("roc_dpi is NULL");
		return -EINVAL;
	}

	pci_dev = roc_dpi->pci_dev;

	roc_dpi_disable(roc_dpi);

	rc = roc_dpi_wait_queue_idle(roc_dpi);
	if (rc)
		return rc;

	plt_write64(0x0, roc_dpi->rbase + DPI_VDMA_REQQ_CTL);
	plt_write64(chunk_base, roc_dpi->rbase + DPI_VDMA_SADDR);
	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	/* DPI PF driver expects vfid starts from index 0 */
	mbox_msg.s.vfid = roc_dpi->vfid;
	mbox_msg.s.pri = roc_dpi->priority;
	mbox_msg.s.cmd = DPI_QUEUE_OPEN_V2;
	mbox_msg.s.csize = chunk_sz / 8;
	mbox_msg.s.aura = aura;
	mbox_msg.s.sso_pf_func = idev_sso_pffunc_get();
	mbox_msg.s.npa_pf_func = idev_npa_pffunc_get();

	rc = send_msg_to_pf(&pci_dev->addr, (const char *)&mbox_msg,
			    sizeof(dpi_mbox_msg_t));
	if (rc < 0)
		plt_err("Failed to send mbox message %d to DPI PF, err %d",
			mbox_msg.s.cmd, rc);

	return rc;
}

int
dpi_lf_attach(struct dev *dev, uint8_t blk_addr, bool modify, uint16_t nb_lf)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct dpi_rsrc_attach_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_attach_resources(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->dpi_lfs = nb_lf;
	req->dpilfs = 1;
	req->modify = modify;
	req->dpi_blkaddr = blk_addr;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
dpi_lf_detach(struct dev *dev)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	uint8_t blk_addr = RVU_BLOCK_ADDR_DPI0;
	struct dpi_rsrc_detach *req;
	int rc;

	req = mbox_alloc_msg_dpi_detach_resources(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->dpi_blkaddr = blk_addr;
	req->dpilfs = 1;
	req->partial = 1;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
dpi_chan_tbl_alloc(struct dev *dev, uint8_t blk_addr, uint16_t tbl_sz)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct dpi_lf_chan_tbl_alloc_req *req;
	struct dpi_lf_chan_tbl_alloc_rsp *rsp;
	int rc;

	req = mbox_alloc_msg_dpi_lf_chan_tbl_alloc(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->tbl_size = tbl_sz;
	req->dpi_blkaddr = blk_addr;

	rc = mbox_process_msg(mbox, (void **)&rsp);
	if (rc)
		goto exit;
	rc = rsp->tbl_num;
exit:
	mbox_put(mbox);
	return rc;
}

int
dpi_chan_tbl_free(struct dev *dev, uint8_t blk_addr, uint16_t tbl_num)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct dpi_lf_chan_tbl_free_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_chan_tbl_free(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->tbl_num = tbl_num;
	req->dpi_blkaddr = blk_addr;

	rc = mbox_process(mbox);

exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_lf_chan_tbl_select(struct roc_dpi_lf *lf)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_chan_tbl_select_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_chan_tbl_select(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->lf_slot = lf->slot;
	req->chan_tbl = lf->chan_tbl;
	req->dpi_blkaddr = lf->blk_addr;
	req->ena = true;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_lf_chan_tbl_ena_dis(struct roc_dpi_lf *lf, bool ena)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_chan_tbl_ena_dis_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_chan_tbl_ena_dis(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->lf_slot = lf->slot;
	req->dpi_blkaddr = lf->blk_addr;
	req->ena_dis = ena;

	rc = mbox_process(mbox);

exit:
	mbox_put(mbox);
	return rc;
}

int
dpi_chan_tbl_update(struct dev *dev, uint8_t blk_addr, uint16_t chan_tbl, uint64_t *tbl,
		    uint16_t off, uint16_t nb_entries)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct dpi_lf_chan_tbl_update_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_chan_tbl_update(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	mbox_memcpy(req->config, tbl, nb_entries * sizeof(uint64_t));

	req->chan_tbl = chan_tbl;
	req->num_entries = nb_entries;
	req->idx_offset = off;
	req->dpi_blkaddr = blk_addr;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_lf_chan_tbl_update(struct roc_dpi_lf *lf, uint64_t *config, uint16_t offset,
			   uint16_t entries)
{
	if (entries > DPI_LF_CHAN_TBL_UPDATE_SIZE)
		return -EINVAL;

	return dpi_chan_tbl_update(lf->dev, lf->blk_addr, lf->chan_tbl, config, offset, entries);
}

int
dpi_chan_tbl_ena_dis(struct dev *dev, uint32_t blkaddr, uint16_t lfid, uint16_t chan_tbl,
		     bool enable)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	struct dpi_lf_chan_tbl_select_req *req;
	int rc;

	req = mbox_alloc_msg_dpi_lf_chan_tbl_select(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->chan_tbl = chan_tbl;
	req->lf_slot = lfid;
	req->ena = enable;
	req->dpi_blkaddr = blkaddr;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_dpi_lf_chan_tbl_alloc(struct roc_dpi_lf *lf, uint16_t tbl_sz)
{
	int rc;

	rc = dpi_chan_tbl_alloc(lf->dev, lf->blk_addr, tbl_sz);
	if (rc < 0)
		return rc;

	lf->chan_tbl_sz = tbl_sz;
	lf->chan_tbl = rc;
	return 0;
}

int
roc_dpi_lf_chan_tbl_free(struct roc_dpi_lf *lf)
{
	return dpi_chan_tbl_free(lf->dev, lf->blk_addr, lf->chan_tbl);
}

int
roc_dpi_lf_pffunc_cfg(struct roc_dpi_lf *lf)
{
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_pf_func_cfg_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_dpi_lf_pf_func_cfg(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	req->dpi_blkaddr = lf->blk_addr;
	req->sso_pf_func = idev_sso_pffunc_get();
	req->npa_pf_func = idev_npa_pffunc_get();
	req->lf_slot = lf->slot;

	rc = mbox_process(mbox);

exit:
	mbox_put(mbox);
	return rc;
}

static int
dpi_lf_queue_configure(struct roc_dpi_lf_que *que, struct roc_dpi_lf_ring_cfg *rcfg)
{
	char nm[ROC_DPI_DEV_NAME_LEN] = {'\0'};
	struct roc_dpi_lf *lf = que->lf;
	const struct plt_memzone *mz;
	uint64_t reg;

	snprintf(nm, sizeof(nm), "%s_%u_%u_%x", "dpi_lf_q", lf->slot, rcfg->ring_idx,
		 lf->dev->pf_func);
	mz = plt_memzone_reserve_aligned(nm, que->qsize * que->cmd_len, 0, 128);
	if (!mz) {
		plt_err("Cannot alloc buffer for DPI LF ring command buffer: %s", nm);
		return -ENOMEM;
	}

	que->mz = mz;
	que->cmd_base = (uint64_t *)mz->addr;

	reg = plt_read64(lf->rbase + DPI_LF_RINGX_CFG(rcfg->ring_idx));

	if (rcfg->isize)
		reg |= DPI_LF_QCFG_ISIZE;
	else
		reg &= ~DPI_LF_QCFG_ISIZE;

	reg |= (((uint64_t)que->first_skip << 20) | ((uint64_t)que->later_skip << 28));
	reg |= BIT_ULL(7);

	plt_write64(reg, lf->rbase + DPI_LF_RINGX_CFG(rcfg->ring_idx));

	reg = plt_read64(lf->rbase + DPI_LF_RINGX_BASE(rcfg->ring_idx));
	reg = (uint64_t)que->cmd_base;
	reg |= ((((que->mz->len >> 10) - 1) & DPI_LF_QSIZE_MASK) << DPI_LF_QSIZE_SHIFT);
	plt_write64(reg, lf->rbase + DPI_LF_RINGX_BASE(rcfg->ring_idx));

	return 0;
}

int
roc_dpi_lf_ring_init(struct roc_dpi_lf_que *que, struct roc_dpi_lf_ring_cfg *rcfg)
{
	struct roc_dpi_lf *lf = que->lf;
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_ring_cfg_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_dpi_lf_ring_cfg(mbox);
	if (req == NULL)
		goto fail;

	req->dpi_blkaddr = lf->blk_addr;
	req->lf_slot = lf->slot;
	req->xtype = rcfg->xtype;
	req->rport = rcfg->rport;
	req->wport = rcfg->wport;
	req->ring_idx = rcfg->ring_idx;
	req->pri = rcfg->pri;

	rc = mbox_process(mbox);
	if (rc)
		goto fail;

	rc = dpi_lf_queue_configure(que, rcfg);

fail:
	mbox_put(mbox);
	return rc;
}

void
roc_dpi_lf_ring_fini(struct roc_dpi_lf_que *que)
{
	if (que->mz) {
		plt_memzone_free(que->mz);
		que->mz = NULL;
	}
}

int
roc_dpi_lf_ring_chan_cfg(struct roc_dpi_lf_que *que, union roc_dpi_lf_ccfg *ccfg)
{
	struct roc_dpi_lf *lf = que->lf;
	struct mbox *mbox = mbox_get(lf->dev->mbox);
	struct dpi_lf_chan_cfg_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_dpi_lf_chan_cfg(mbox);
	if (req == NULL)
		goto fail;

	req->dpi_blkaddr = lf->blk_addr;
	req->def_config = ccfg->u;
	req->lf_slot = lf->slot;
	req->ring_idx = que == lf->queue ? 0 : 1;

	rc = mbox_process(mbox);

fail:
	mbox_put(mbox);
	return rc;
}

int
dpi_lf_init(struct roc_dpi_lf *lf, struct dev *dev, uint8_t slot)
{
	uint8_t blk_addr = RVU_BLOCK_ADDR_DPI0;

	lf->dev = dev;
	lf->slot = slot;
	lf->rbase = dev->bar2 + (RVU_BLOCK_ADDR_DPI0 << 20 | slot << 12);
	lf->queue[0].lf = lf;
	lf->queue[1].lf = lf;
	lf->blk_addr = blk_addr;
	return 0;
}

int
roc_dpi_rsrc_init(struct roc_dpi *roc_dpi)
{
	struct plt_pci_device *pci_dev = roc_dpi->pci_dev;
	struct dpi *dpi = roc_dpi_to_dpi_priv(roc_dpi);
	uint8_t blk_addr = RVU_BLOCK_ADDR_DPI0;
	char name[ROC_DPI_DEV_NAME_LEN];
	const struct plt_memzone *mz;
	struct dev *dev = &dpi->dev;
	uint16_t slot;
	int rc;

	mz = plt_memzone_reserve_cache_align(plt_pci_dev_name(name, ROC_DPI_DEV_NAME, pci_dev),
					     roc_dpi->nr_lfs * sizeof(struct roc_dpi_lf));
	if (!mz)
		return -ENOMEM;

	roc_dpi->mz = mz;
	roc_dpi->lfs = mz->addr;

	rc = dpi_lf_attach(dev, blk_addr, true, roc_dpi->nr_lfs);
	if (rc) {
		plt_err("Could not attach LFs");
		plt_memzone_free(mz);
		roc_dpi->mz = NULL;
		roc_dpi->lfs = NULL;
		return rc;
	}

	for (slot = 0; slot < roc_dpi->nr_lfs; slot++)
		dpi_lf_init(&(roc_dpi->lfs[slot]), dev, slot);

	return rc;
}

int
roc_dpi_rsrc_fini(struct roc_dpi *roc_dpi)
{
	struct dpi *dpi = roc_dpi_to_dpi_priv(roc_dpi);
	struct dev *dev = &dpi->dev;
	struct roc_dpi_lf_que *que;
	struct roc_dpi_lf *lf;
	uint16_t slot, qid;
	int rc;

	roc_dpi_disable(roc_dpi);

	for (slot = 0; slot < roc_dpi->nr_lfs; slot++) {
		lf = &roc_dpi->lfs[slot];

		for (qid = 0; qid < ROC_DPI_LF_RINGS; qid++) {
			que = &lf->queue[qid];
			if (que->mz) {
				plt_memzone_free(que->mz);
				que->mz = NULL;
			}
		}
	}

	rc = dpi_lf_detach(dev);
	plt_memzone_free(roc_dpi->mz);
	roc_dpi->mz = NULL;
	roc_dpi->lfs = NULL;

	return rc;
}

int
roc_dpi_dev_init(struct roc_dpi *roc_dpi, uint8_t offset)
{
	struct plt_pci_device *pci_dev = roc_dpi->pci_dev;
	struct dpi *dpi = roc_dpi_to_dpi_priv(roc_dpi);
	struct dev *dev = &dpi->dev;
	uint16_t vfid;
	int rc = 0;

	roc_dpi->rbase = pci_dev->mem_resource[0].addr;

	if (roc_model_is_cn20k()) {
		rc = dev_init(dev, pci_dev);
		if (rc)
			plt_err("Failed to init dpi roc device");
	} else {
		vfid = ((pci_dev->addr.devid & 0x1F) << 3) | (pci_dev->addr.function & 0x7);
		vfid -= 1;
		roc_dpi->vfid = vfid;
		idev_dma_cs_offset_set(offset);
	}

	return rc;
}

int
roc_dpi_dev_fini(struct roc_dpi *roc_dpi)
{
	struct plt_pci_device *pci_dev = roc_dpi->pci_dev;
	struct dpi *dpi = roc_dpi_to_dpi_priv(roc_dpi);
	struct dev *dev = &dpi->dev;
	dpi_mbox_msg_t mbox_msg;
	int rc;

	if (roc_model_is_cn20k()) {
		rc = dev_fini(dev, pci_dev);
		return rc;
	}

	rc = roc_dpi_wait_queue_idle(roc_dpi);
	if (rc)
		return rc;

	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	mbox_msg.s.vfid = roc_dpi->vfid;
	mbox_msg.s.cmd = DPI_QUEUE_CLOSE;

	rc = send_msg_to_pf(&pci_dev->addr, (const char *)&mbox_msg, sizeof(dpi_mbox_msg_t));
	if (rc < 0)
		plt_err("Failed to send mbox message %d to DPI PF, err %d", mbox_msg.s.cmd, rc);

	return rc;
}
