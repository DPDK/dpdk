/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define CPT_IQ_FC_LEN  128
#define CPT_IQ_GRP_LEN 16

#define CPT_IQ_NB_DESC_MULTIPLIER 40

/* The effective queue size to software is (CPT_LF_Q_SIZE[SIZE_DIV40] - 1 - 8).
 *
 * CPT requires 320 free entries (+8). And 40 entries are required for
 * allowing CPT to discard packet when the queues are full (+1).
 */
#define CPT_IQ_NB_DESC_SIZE_DIV40(nb_desc)                                     \
	(PLT_DIV_CEIL(nb_desc, CPT_IQ_NB_DESC_MULTIPLIER) + 1 + 8)

#define CPT_IQ_GRP_SIZE(nb_desc)                                               \
	(CPT_IQ_NB_DESC_SIZE_DIV40(nb_desc) * CPT_IQ_GRP_LEN)

#define CPT_LF_MAX_NB_DESC     128000
#define CPT_LF_DEFAULT_NB_DESC 1024

static void
cpt_lf_misc_intr_enb_dis(struct roc_cpt_lf *lf, bool enb)
{
	/* Enable all cpt lf error irqs except RQ_DISABLED and CQ_DISABLED */
	if (enb)
		plt_write64((BIT_ULL(6) | BIT_ULL(5) | BIT_ULL(3) | BIT_ULL(2) |
			     BIT_ULL(1)),
			    lf->rbase + CPT_LF_MISC_INT_ENA_W1S);
	else
		plt_write64((BIT_ULL(6) | BIT_ULL(5) | BIT_ULL(3) | BIT_ULL(2) |
			     BIT_ULL(1)),
			    lf->rbase + CPT_LF_MISC_INT_ENA_W1C);
}

static void
cpt_lf_misc_irq(void *param)
{
	struct roc_cpt_lf *lf = (struct roc_cpt_lf *)param;
	struct dev *dev = lf->dev;
	uint64_t intr;

	intr = plt_read64(lf->rbase + CPT_LF_MISC_INT);
	if (intr == 0)
		return;

	plt_err("Err_irq=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);

	/* Dump lf registers */
	cpt_lf_print(lf);

	/* Clear interrupt */
	plt_write64(intr, lf->rbase + CPT_LF_MISC_INT);
}

static int
cpt_lf_register_misc_irq(struct roc_cpt_lf *lf)
{
	struct plt_pci_device *pci_dev = lf->pci_dev;
	struct plt_intr_handle *handle;
	int rc, vec;

	handle = pci_dev->intr_handle;

	vec = lf->msixoff + CPT_LF_INT_VEC_MISC;
	/* Clear err interrupt */
	cpt_lf_misc_intr_enb_dis(lf, false);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, cpt_lf_misc_irq, lf, vec);
	/* Enable all dev interrupt except for RQ_DISABLED */
	cpt_lf_misc_intr_enb_dis(lf, true);

	return rc;
}

static void
cpt_lf_unregister_misc_irq(struct roc_cpt_lf *lf)
{
	struct plt_pci_device *pci_dev = lf->pci_dev;
	struct plt_intr_handle *handle;
	int vec;

	handle = pci_dev->intr_handle;

	vec = lf->msixoff + CPT_LF_INT_VEC_MISC;
	/* Clear err interrupt */
	cpt_lf_misc_intr_enb_dis(lf, false);
	dev_irq_unregister(handle, cpt_lf_misc_irq, lf, vec);
}

static void
cpt_lf_done_intr_enb_dis(struct roc_cpt_lf *lf, bool enb)
{
	if (enb)
		plt_write64(0x1, lf->rbase + CPT_LF_DONE_INT_ENA_W1S);
	else
		plt_write64(0x1, lf->rbase + CPT_LF_DONE_INT_ENA_W1C);
}

static void
cpt_lf_done_irq(void *param)
{
	struct roc_cpt_lf *lf = param;
	uint64_t done_wait;
	uint64_t intr;

	/* Read the number of completed requests */
	intr = plt_read64(lf->rbase + CPT_LF_DONE);
	if (intr == 0)
		return;

	done_wait = plt_read64(lf->rbase + CPT_LF_DONE_WAIT);

	/* Acknowledge the number of completed requests */
	plt_write64(intr, lf->rbase + CPT_LF_DONE_ACK);

	plt_write64(done_wait, lf->rbase + CPT_LF_DONE_WAIT);
}

static int
cpt_lf_register_done_irq(struct roc_cpt_lf *lf)
{
	struct plt_pci_device *pci_dev = lf->pci_dev;
	struct plt_intr_handle *handle;
	int rc, vec;

	handle = pci_dev->intr_handle;

	vec = lf->msixoff + CPT_LF_INT_VEC_DONE;

	/* Clear done interrupt */
	cpt_lf_done_intr_enb_dis(lf, false);

	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, cpt_lf_done_irq, lf, vec);

	/* Enable done interrupt */
	cpt_lf_done_intr_enb_dis(lf, true);

	return rc;
}

static void
cpt_lf_unregister_done_irq(struct roc_cpt_lf *lf)
{
	struct plt_pci_device *pci_dev = lf->pci_dev;
	struct plt_intr_handle *handle;
	int vec;

	handle = pci_dev->intr_handle;

	vec = lf->msixoff + CPT_LF_INT_VEC_DONE;

	/* Clear done interrupt */
	cpt_lf_done_intr_enb_dis(lf, false);
	dev_irq_unregister(handle, cpt_lf_done_irq, lf, vec);
}

static int
cpt_lf_register_irqs(struct roc_cpt_lf *lf)
{
	int rc;

	if (lf->msixoff == MSIX_VECTOR_INVALID) {
		plt_err("Invalid CPTLF MSIX vector offset vector: 0x%x",
			lf->msixoff);
		return -EINVAL;
	}

	/* Register lf err interrupt */
	rc = cpt_lf_register_misc_irq(lf);
	if (rc)
		plt_err("Error registering IRQs");

	rc = cpt_lf_register_done_irq(lf);
	if (rc)
		plt_err("Error registering IRQs");

	return rc;
}

static void
cpt_lf_unregister_irqs(struct roc_cpt_lf *lf)
{
	cpt_lf_unregister_misc_irq(lf);
	cpt_lf_unregister_done_irq(lf);
}

static void
cpt_lf_dump(struct roc_cpt_lf *lf)
{
	plt_cpt_dbg("CPT LF");
	plt_cpt_dbg("RBASE: 0x%016" PRIx64, lf->rbase);
	plt_cpt_dbg("LMT_BASE: 0x%016" PRIx64, lf->lmt_base);
	plt_cpt_dbg("MSIXOFF: 0x%x", lf->msixoff);
	plt_cpt_dbg("LF_ID: 0x%x", lf->lf_id);
	plt_cpt_dbg("NB DESC: %d", lf->nb_desc);
	plt_cpt_dbg("FC_ADDR: 0x%016" PRIx64, (uintptr_t)lf->fc_addr);
	plt_cpt_dbg("CQ.VADDR: 0x%016" PRIx64, (uintptr_t)lf->iq_vaddr);

	plt_cpt_dbg("CPT LF REG:");
	plt_cpt_dbg("LF_CTL[0x%016llx]: 0x%016" PRIx64, CPT_LF_CTL,
		    plt_read64(lf->rbase + CPT_LF_CTL));
	plt_cpt_dbg("LF_INPROG[0x%016llx]: 0x%016" PRIx64, CPT_LF_INPROG,
		    plt_read64(lf->rbase + CPT_LF_INPROG));

	plt_cpt_dbg("Q_BASE[0x%016llx]: 0x%016" PRIx64, CPT_LF_Q_BASE,
		    plt_read64(lf->rbase + CPT_LF_Q_BASE));
	plt_cpt_dbg("Q_SIZE[0x%016llx]: 0x%016" PRIx64, CPT_LF_Q_SIZE,
		    plt_read64(lf->rbase + CPT_LF_Q_SIZE));
	plt_cpt_dbg("Q_INST_PTR[0x%016llx]: 0x%016" PRIx64, CPT_LF_Q_INST_PTR,
		    plt_read64(lf->rbase + CPT_LF_Q_INST_PTR));
	plt_cpt_dbg("Q_GRP_PTR[0x%016llx]: 0x%016" PRIx64, CPT_LF_Q_GRP_PTR,
		    plt_read64(lf->rbase + CPT_LF_Q_GRP_PTR));
}

int
cpt_lf_outb_cfg(struct dev *dev, uint16_t sso_pf_func, uint16_t nix_pf_func,
		uint8_t lf_id, bool ena)
{
	struct cpt_inline_ipsec_cfg_msg *req;
	struct mbox *mbox = dev->mbox;

	req = mbox_alloc_msg_cpt_inline_ipsec_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;

	req->dir = CPT_INLINE_OUTBOUND;
	req->slot = lf_id;
	if (ena) {
		req->enable = 1;
		req->sso_pf_func = sso_pf_func;
		req->nix_pf_func = nix_pf_func;
	} else {
		req->enable = 0;
	}

	return mbox_process(mbox);
}

int
roc_cpt_inline_ipsec_cfg(struct dev *cpt_dev, uint8_t lf_id,
			 struct roc_nix *roc_nix)
{
	bool ena = roc_nix ? true : false;
	uint16_t nix_pf_func = 0;
	uint16_t sso_pf_func = 0;

	if (ena) {
		nix_pf_func = roc_nix_get_pf_func(roc_nix);
		sso_pf_func = idev_sso_pffunc_get();
	}

	return cpt_lf_outb_cfg(cpt_dev, sso_pf_func, nix_pf_func, lf_id, ena);
}

int
roc_cpt_inline_ipsec_inb_cfg(struct roc_cpt *roc_cpt, uint16_t param1,
			     uint16_t param2)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct cpt_rx_inline_lf_cfg_msg *req;
	struct mbox *mbox;

	mbox = cpt->dev.mbox;

	req = mbox_alloc_msg_cpt_rx_inline_lf_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;

	req->sso_pf_func = idev_sso_pffunc_get();
	req->param1 = param1;
	req->param2 = param2;

	return mbox_process(mbox);
}

int
roc_cpt_rxc_time_cfg(struct roc_cpt *roc_cpt, struct roc_cpt_rxc_time_cfg *cfg)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct cpt_rxc_time_cfg_req *req;
	struct dev *dev = &cpt->dev;

	req = mbox_alloc_msg_cpt_rxc_time_cfg(dev->mbox);
	if (req == NULL)
		return -ENOSPC;

	req->blkaddr = 0;

	/* The step value is in microseconds. */
	req->step = cfg->step;

	/* The timeout will be: limit * step microseconds */
	req->zombie_limit = cfg->zombie_limit;
	req->zombie_thres = cfg->zombie_thres;

	/* The timeout will be: limit * step microseconds */
	req->active_limit = cfg->active_limit;
	req->active_thres = cfg->active_thres;

	return mbox_process(dev->mbox);
}

int
cpt_get_msix_offset(struct dev *dev, struct msix_offset_rsp **msix_rsp)
{
	struct mbox *mbox = dev->mbox;
	int rc;

	/* Get MSIX vector offsets */
	mbox_alloc_msg_msix_offset(mbox);
	rc = mbox_process_msg(mbox, (void *)msix_rsp);

	return rc;
}

int
cpt_lfs_attach(struct dev *dev, uint8_t blkaddr, bool modify, uint16_t nb_lf)
{
	struct mbox *mbox = dev->mbox;
	struct rsrc_attach_req *req;

	if (blkaddr != RVU_BLOCK_ADDR_CPT0 && blkaddr != RVU_BLOCK_ADDR_CPT1)
		return -EINVAL;

	/* Attach CPT(lf) */
	req = mbox_alloc_msg_attach_resources(mbox);
	if (req == NULL)
		return -ENOSPC;

	req->cptlfs = nb_lf;
	req->modify = modify;
	req->cpt_blkaddr = blkaddr;

	return mbox_process(mbox);
}

int
cpt_lfs_detach(struct dev *dev)
{
	struct mbox *mbox = dev->mbox;
	struct rsrc_detach_req *req;

	req = mbox_alloc_msg_detach_resources(mbox);
	if (req == NULL)
		return -ENOSPC;

	req->cptlfs = 1;
	req->partial = 1;

	return mbox_process(mbox);
}

static int
cpt_available_lfs_get(struct dev *dev, uint16_t *nb_lf)
{
	struct mbox *mbox = dev->mbox;
	struct free_rsrcs_rsp *rsp;
	int rc;

	mbox_alloc_msg_free_rsrc_cnt(mbox);

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return -EIO;

	*nb_lf = PLT_MAX((uint16_t)rsp->cpt, (uint16_t)rsp->cpt1);
	return 0;
}

int
cpt_lfs_alloc(struct dev *dev, uint8_t eng_grpmsk, uint8_t blkaddr,
	      bool inl_dev_sso)
{
	struct cpt_lf_alloc_req_msg *req;
	struct mbox *mbox = dev->mbox;

	if (blkaddr != RVU_BLOCK_ADDR_CPT0 && blkaddr != RVU_BLOCK_ADDR_CPT1)
		return -EINVAL;

	req = mbox_alloc_msg_cpt_lf_alloc(mbox);
	if (!req)
		return -ENOSPC;

	req->nix_pf_func = 0;
	if (inl_dev_sso && nix_inl_dev_pffunc_get())
		req->sso_pf_func = nix_inl_dev_pffunc_get();
	else
		req->sso_pf_func = idev_sso_pffunc_get();
	req->eng_grpmsk = eng_grpmsk;
	req->blkaddr = blkaddr;

	return mbox_process(mbox);
}

int
cpt_lfs_free(struct dev *dev)
{
	mbox_alloc_msg_cpt_lf_free(dev->mbox);

	return mbox_process(dev->mbox);
}

static int
cpt_hardware_caps_get(struct dev *dev, struct roc_cpt *roc_cpt)
{
	struct cpt_caps_rsp_msg *rsp;
	int ret;

	mbox_alloc_msg_cpt_caps_get(dev->mbox);

	ret = mbox_process_msg(dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

	roc_cpt->cpt_revision = rsp->cpt_revision;
	mbox_memcpy(roc_cpt->hw_caps, rsp->eng_caps,
		    sizeof(union cpt_eng_caps) * CPT_MAX_ENG_TYPES);

	return 0;
}

static uint32_t
cpt_lf_iq_mem_calc(uint32_t nb_desc)
{
	uint32_t len;

	/* Space for instruction group memory */
	len = CPT_IQ_GRP_SIZE(nb_desc);

	/* Align to 128B */
	len = PLT_ALIGN(len, ROC_ALIGN);

	/* Space for FC */
	len += CPT_IQ_FC_LEN;

	/* For instruction queues */
	len += PLT_ALIGN(CPT_IQ_NB_DESC_SIZE_DIV40(nb_desc) *
				 CPT_IQ_NB_DESC_MULTIPLIER *
				 sizeof(struct cpt_inst_s),
			 ROC_ALIGN);

	return len;
}

static inline void
cpt_iq_init(struct roc_cpt_lf *lf)
{
	union cpt_lf_q_size lf_q_size = {.u = 0x0};
	union cpt_lf_q_base lf_q_base = {.u = 0x0};
	uintptr_t addr;

	lf->io_addr = lf->rbase + CPT_LF_NQX(0);

	/* Disable command queue */
	roc_cpt_iq_disable(lf);

	/* Set command queue base address */
	addr = (uintptr_t)lf->iq_vaddr +
	       PLT_ALIGN(CPT_IQ_GRP_SIZE(lf->nb_desc), ROC_ALIGN);

	lf_q_base.u = addr;

	plt_write64(lf_q_base.u, lf->rbase + CPT_LF_Q_BASE);

	/* Set command queue size */
	lf_q_size.s.size_div40 = CPT_IQ_NB_DESC_SIZE_DIV40(lf->nb_desc);
	plt_write64(lf_q_size.u, lf->rbase + CPT_LF_Q_SIZE);

	lf->fc_addr = (uint64_t *)addr;
	lf->fc_hyst_bits = plt_log2_u32(lf->nb_desc) / 2;
	lf->fc_thresh = lf->nb_desc - (lf->nb_desc % (1 << lf->fc_hyst_bits));
}

int
roc_cpt_dev_configure(struct roc_cpt *roc_cpt, int nb_lf)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	uint8_t blkaddr[ROC_CPT_MAX_BLKS];
	struct msix_offset_rsp *rsp;
	uint8_t eng_grpmsk;
	int blknum = 0;
	int rc, i;

	blkaddr[0] = RVU_BLOCK_ADDR_CPT0;
	blkaddr[1] = RVU_BLOCK_ADDR_CPT1;

	if ((roc_cpt->cpt_revision == ROC_CPT_REVISION_ID_98XX) &&
	    (cpt->dev.pf_func & 0x1))
		blknum = (blknum + 1) % ROC_CPT_MAX_BLKS;

	/* Request LF resources */
	rc = cpt_lfs_attach(&cpt->dev, blkaddr[blknum], true, nb_lf);

	/* Request LFs from another block if current block has less LFs */
	if (roc_cpt->cpt_revision == ROC_CPT_REVISION_ID_98XX && rc == ENOSPC) {
		blknum = (blknum + 1) % ROC_CPT_MAX_BLKS;
		rc = cpt_lfs_attach(&cpt->dev, blkaddr[blknum], true, nb_lf);
	}
	if (rc) {
		plt_err("Could not attach LFs");
		return rc;
	}

	for (i = 0; i < nb_lf; i++)
		cpt->lf_blkaddr[i] = blkaddr[blknum];

	eng_grpmsk = (1 << roc_cpt->eng_grp[CPT_ENG_TYPE_AE]) |
		     (1 << roc_cpt->eng_grp[CPT_ENG_TYPE_SE]) |
		     (1 << roc_cpt->eng_grp[CPT_ENG_TYPE_IE]);

	rc = cpt_lfs_alloc(&cpt->dev, eng_grpmsk, blkaddr[blknum], false);
	if (rc)
		goto lfs_detach;

	rc = cpt_get_msix_offset(&cpt->dev, &rsp);
	if (rc)
		goto lfs_free;

	for (i = 0; i < nb_lf; i++)
		cpt->lf_msix_off[i] =
			(cpt->lf_blkaddr[i] == RVU_BLOCK_ADDR_CPT1) ?
				rsp->cpt1_lf_msixoff[i] :
				rsp->cptlf_msixoff[i];

	roc_cpt->nb_lf = nb_lf;

	return 0;

lfs_free:
	cpt_lfs_free(&cpt->dev);
lfs_detach:
	cpt_lfs_detach(&cpt->dev);
	return rc;
}

uint64_t
cpt_get_blkaddr(struct dev *dev)
{
	uint64_t reg;
	uint64_t off;

	/* Reading the discovery register to know which CPT is the LF
	 * attached to. Assume CPT LF's of only one block are attached
	 * to a pffunc.
	 */
	if (dev_is_vf(dev))
		off = RVU_VF_BLOCK_ADDRX_DISC(RVU_BLOCK_ADDR_CPT1);
	else
		off = RVU_PF_BLOCK_ADDRX_DISC(RVU_BLOCK_ADDR_CPT1);

	reg = plt_read64(dev->bar2 + off);

	return reg & 0x1FFULL ? RVU_BLOCK_ADDR_CPT1 : RVU_BLOCK_ADDR_CPT0;
}

int
cpt_lf_init(struct roc_cpt_lf *lf)
{
	struct dev *dev = lf->dev;
	uint64_t blkaddr;
	void *iq_mem;
	int rc;

	if (lf->nb_desc == 0 || lf->nb_desc > CPT_LF_MAX_NB_DESC)
		lf->nb_desc = CPT_LF_DEFAULT_NB_DESC;

	/* Allocate memory for instruction queue for CPT LF. */
	iq_mem = plt_zmalloc(cpt_lf_iq_mem_calc(lf->nb_desc), ROC_ALIGN);
	if (iq_mem == NULL)
		return -ENOMEM;
	plt_atomic_thread_fence(__ATOMIC_ACQ_REL);

	blkaddr = cpt_get_blkaddr(dev);
	lf->rbase = dev->bar2 + ((blkaddr << 20) | (lf->lf_id << 12));
	lf->iq_vaddr = iq_mem;
	lf->lmt_base = dev->lmt_base;
	lf->pf_func = dev->pf_func;

	/* Initialize instruction queue */
	cpt_iq_init(lf);

	rc = cpt_lf_register_irqs(lf);
	if (rc)
		goto disable_iq;

	return 0;

disable_iq:
	roc_cpt_iq_disable(lf);
	plt_free(iq_mem);
	return rc;
}

int
roc_cpt_lf_init(struct roc_cpt *roc_cpt, struct roc_cpt_lf *lf)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	int rc;

	lf->dev = &cpt->dev;
	lf->roc_cpt = roc_cpt;
	lf->msixoff = cpt->lf_msix_off[lf->lf_id];
	lf->pci_dev = cpt->pci_dev;

	rc = cpt_lf_init(lf);
	if (rc)
		return rc;

	/* LF init successful */
	roc_cpt->lf[lf->lf_id] = lf;
	return rc;
}

int
roc_cpt_dev_init(struct roc_cpt *roc_cpt)
{
	struct plt_pci_device *pci_dev;
	uint16_t nb_lf_avail;
	struct dev *dev;
	struct cpt *cpt;
	int rc;

	if (roc_cpt == NULL || roc_cpt->pci_dev == NULL)
		return -EINVAL;

	PLT_STATIC_ASSERT(sizeof(struct cpt) <= ROC_CPT_MEM_SZ);

	cpt = roc_cpt_to_cpt_priv(roc_cpt);
	memset(cpt, 0, sizeof(*cpt));
	pci_dev = roc_cpt->pci_dev;
	dev = &cpt->dev;

	/* Initialize device  */
	rc = dev_init(dev, pci_dev);
	if (rc) {
		plt_err("Failed to init roc device");
		goto fail;
	}

	cpt->pci_dev = pci_dev;
	roc_cpt->lmt_base = dev->lmt_base;

	rc = cpt_hardware_caps_get(dev, roc_cpt);
	if (rc) {
		plt_err("Could not determine hardware capabilities");
		goto fail;
	}

	rc = cpt_available_lfs_get(&cpt->dev, &nb_lf_avail);
	if (rc) {
		plt_err("Could not get available lfs");
		goto fail;
	}

	/* Reserve 1 CPT LF for inline inbound */
	nb_lf_avail = PLT_MIN(nb_lf_avail, (uint16_t)(ROC_CPT_MAX_LFS - 1));

	roc_cpt->nb_lf_avail = nb_lf_avail;

	dev->roc_cpt = roc_cpt;

	/* Set it to idev if not already present */
	if (!roc_idev_cpt_get())
		roc_idev_cpt_set(roc_cpt);

	return 0;

fail:
	return rc;
}

int
roc_cpt_lf_ctx_flush(struct roc_cpt_lf *lf, void *cptr, bool inval)
{
	union cpt_lf_ctx_flush reg;

	if (lf == NULL) {
		plt_err("Could not trigger CTX flush");
		return -ENOTSUP;
	}

	reg.u = 0;
	reg.s.inval = inval;
	reg.s.cptr = (uintptr_t)cptr >> 7;

	plt_write64(reg.u, lf->rbase + CPT_LF_CTX_FLUSH);

	return 0;
}

int
roc_cpt_lf_ctx_reload(struct roc_cpt_lf *lf, void *cptr)
{
	union cpt_lf_ctx_reload reg;

	if (lf == NULL) {
		plt_err("Could not trigger CTX reload");
		return -ENOTSUP;
	}

	reg.u = 0;
	reg.s.cptr = (uintptr_t)cptr >> 7;

	plt_write64(reg.u, lf->rbase + CPT_LF_CTX_RELOAD);

	return 0;
}

void
cpt_lf_fini(struct roc_cpt_lf *lf)
{
	/* Unregister IRQ's */
	cpt_lf_unregister_irqs(lf);

	/* Disable IQ */
	roc_cpt_iq_disable(lf);

	/* Free memory */
	plt_free(lf->iq_vaddr);
	lf->iq_vaddr = NULL;
}

void
roc_cpt_lf_fini(struct roc_cpt_lf *lf)
{
	if (lf == NULL)
		return;
	lf->roc_cpt->lf[lf->lf_id] = NULL;
	cpt_lf_fini(lf);
}

int
roc_cpt_dev_fini(struct roc_cpt *roc_cpt)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);

	if (cpt == NULL)
		return -EINVAL;

	/* Remove idev references */
	if (roc_idev_cpt_get() == roc_cpt)
		roc_idev_cpt_set(NULL);

	roc_cpt->nb_lf_avail = 0;

	roc_cpt->lmt_base = 0;

	return dev_fini(&cpt->dev, cpt->pci_dev);
}

void
roc_cpt_dev_clear(struct roc_cpt *roc_cpt)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	int i;

	if (cpt == NULL)
		return;

	for (i = 0; i < roc_cpt->nb_lf; i++)
		cpt->lf_msix_off[i] = 0;

	roc_cpt->nb_lf = 0;

	cpt_lfs_free(&cpt->dev);

	cpt_lfs_detach(&cpt->dev);
}

int
roc_cpt_eng_grp_add(struct roc_cpt *roc_cpt, enum cpt_eng_type eng_type)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct dev *dev = &cpt->dev;
	struct cpt_eng_grp_req *req;
	struct cpt_eng_grp_rsp *rsp;
	int ret;

	req = mbox_alloc_msg_cpt_eng_grp_get(dev->mbox);
	if (req == NULL)
		return -EIO;

	switch (eng_type) {
	case CPT_ENG_TYPE_AE:
	case CPT_ENG_TYPE_SE:
	case CPT_ENG_TYPE_IE:
		break;
	default:
		return -EINVAL;
	}

	req->eng_type = eng_type;
	ret = mbox_process_msg(dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

	if (rsp->eng_grp_num > 8) {
		plt_err("Invalid CPT engine group");
		return -ENOTSUP;
	}

	roc_cpt->eng_grp[eng_type] = rsp->eng_grp_num;

	return rsp->eng_grp_num;
}

void
roc_cpt_iq_disable(struct roc_cpt_lf *lf)
{
	volatile union cpt_lf_q_grp_ptr grp_ptr = {.u = 0x0};
	volatile union cpt_lf_inprog lf_inprog = {.u = 0x0};
	union cpt_lf_ctl lf_ctl = {.u = 0x0};
	int timeout = 20;
	int cnt;

	/* Disable instructions enqueuing */
	plt_write64(lf_ctl.u, lf->rbase + CPT_LF_CTL);

	/* Wait for instruction queue to become empty */
	do {
		lf_inprog.u = plt_read64(lf->rbase + CPT_LF_INPROG);
		if (!lf_inprog.s.inflight)
			break;

		plt_delay_ms(20);
		if (timeout-- < 0) {
			plt_err("CPT LF %d is still busy", lf->lf_id);
			break;
		}

	} while (1);

	/* Disable executions in the LF's queue.
	 * The queue should be empty at this point
	 */
	lf_inprog.s.eena = 0x0;
	plt_write64(lf_inprog.u, lf->rbase + CPT_LF_INPROG);

	/* Wait for instruction queue to become empty */
	cnt = 0;
	do {
		lf_inprog.u = plt_read64(lf->rbase + CPT_LF_INPROG);
		if (lf_inprog.s.grb_partial)
			cnt = 0;
		else
			cnt++;
		grp_ptr.u = plt_read64(lf->rbase + CPT_LF_Q_GRP_PTR);
	} while ((cnt < 10) && (grp_ptr.s.nq_ptr != grp_ptr.s.dq_ptr));

	cnt = 0;
	do {
		lf_inprog.u = plt_read64(lf->rbase + CPT_LF_INPROG);
		if ((lf_inprog.s.inflight == 0) && (lf_inprog.s.gwb_cnt < 40) &&
		    ((lf_inprog.s.grb_cnt == 0) || (lf_inprog.s.grb_cnt == 40)))
			cnt++;
		else
			cnt = 0;
	} while (cnt < 10);
}

void
roc_cpt_iq_enable(struct roc_cpt_lf *lf)
{
	union cpt_lf_inprog lf_inprog;
	union cpt_lf_ctl lf_ctl;

	/* Disable command queue */
	roc_cpt_iq_disable(lf);

	/* Enable instruction queue enqueuing */
	lf_ctl.u = plt_read64(lf->rbase + CPT_LF_CTL);
	lf_ctl.s.ena = 1;
	lf_ctl.s.fc_ena = 1;
	lf_ctl.s.fc_up_crossing = 0;
	lf_ctl.s.fc_hyst_bits = lf->fc_hyst_bits;
	plt_write64(lf_ctl.u, lf->rbase + CPT_LF_CTL);

	/* Enable command queue execution */
	lf_inprog.u = plt_read64(lf->rbase + CPT_LF_INPROG);
	lf_inprog.s.eena = 1;
	plt_write64(lf_inprog.u, lf->rbase + CPT_LF_INPROG);

	cpt_lf_dump(lf);
}

int
roc_cpt_lmtline_init(struct roc_cpt *roc_cpt, struct roc_cpt_lmtline *lmtline,
		     int lf_id)
{
	struct roc_cpt_lf *lf;

	lf = roc_cpt->lf[lf_id];
	if (lf == NULL)
		return -ENOTSUP;

	lmtline->io_addr = lf->io_addr;
	if (roc_model_is_cn10k())
		lmtline->io_addr |= ROC_CN10K_CPT_INST_DW_M1 << 4;

	lmtline->fc_addr = lf->fc_addr;
	lmtline->lmt_base = lf->lmt_base;

	return 0;
}

int
roc_cpt_ctx_write(struct roc_cpt_lf *lf, void *sa_dptr, void *sa_cptr,
		  uint16_t sa_len)
{
	uintptr_t lmt_base = lf->lmt_base;
	uint64_t lmt_arg, io_addr;
	struct cpt_inst_s *inst;
	union cpt_res_s *res;
	uint16_t lmt_id;
	uint64_t *dptr;
	int i;

	ROC_LMT_CPT_BASE_ID_GET(lmt_base, lmt_id);
	inst = (struct cpt_inst_s *)lmt_base;

	memset(inst, 0, sizeof(struct cpt_inst_s));

	res = plt_zmalloc(sizeof(*res), ROC_CPT_RES_ALIGN);
	if (res == NULL) {
		plt_err("Couldn't allocate memory for result address");
		return -ENOMEM;
	}

	dptr = plt_zmalloc(sa_len, 8);
	if (dptr == NULL) {
		plt_err("Couldn't allocate memory for SA dptr");
		plt_free(res);
		return -ENOMEM;
	}

	for (i = 0; i < (sa_len / 8); i++)
		dptr[i] = plt_cpu_to_be_64(((uint64_t *)sa_dptr)[i]);

	/* Fill CPT_INST_S for WRITE_SA microcode op */
	res->cn10k.compcode = CPT_COMP_NOT_DONE;
	inst->res_addr = (uint64_t)res;
	inst->dptr = (uint64_t)dptr;
	inst->w4.s.param2 = sa_len >> 3;
	inst->w4.s.dlen = sa_len;
	inst->w4.s.opcode_major = ROC_IE_OT_MAJOR_OP_WRITE_SA;
	inst->w4.s.opcode_minor = ROC_IE_OT_MINOR_OP_WRITE_SA;
	inst->w7.s.cptr = (uint64_t)sa_cptr;
	inst->w7.s.ctx_val = 1;
	inst->w7.s.egrp = ROC_CPT_DFLT_ENG_GRP_SE_IE;

	lmt_arg = ROC_CN10K_CPT_LMT_ARG | (uint64_t)lmt_id;
	io_addr = lf->io_addr | ROC_CN10K_CPT_INST_DW_M1 << 4;

	roc_lmt_submit_steorl(lmt_arg, io_addr);
	plt_wmb();

	/* Wait until CPT instruction completes */
	while (res->cn10k.compcode == CPT_COMP_NOT_DONE)
		plt_delay_ms(1);

	plt_free(res);
	plt_free(dptr);

	return 0;
}
