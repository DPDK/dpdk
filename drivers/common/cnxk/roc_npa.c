/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline int
npa_attach(struct mbox *mbox)
{
	struct rsrc_attach_req *req;

	req = mbox_alloc_msg_attach_resources(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->modify = true;
	req->npalf = true;

	return mbox_process(mbox);
}

static inline int
npa_detach(struct mbox *mbox)
{
	struct rsrc_detach_req *req;

	req = mbox_alloc_msg_detach_resources(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->partial = true;
	req->npalf = true;

	return mbox_process(mbox);
}

static inline int
npa_get_msix_offset(struct mbox *mbox, uint16_t *npa_msixoff)
{
	struct msix_offset_rsp *msix_rsp;
	int rc;

	/* Get NPA MSIX vector offsets */
	mbox_alloc_msg_msix_offset(mbox);
	rc = mbox_process_msg(mbox, (void *)&msix_rsp);
	if (rc == 0)
		*npa_msixoff = msix_rsp->npa_msixoff;

	return rc;
}

static inline int
npa_lf_alloc(struct npa_lf *lf)
{
	struct mbox *mbox = lf->mbox;
	struct npa_lf_alloc_req *req;
	struct npa_lf_alloc_rsp *rsp;
	int rc;

	req = mbox_alloc_msg_npa_lf_alloc(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->aura_sz = lf->aura_sz;
	req->nr_pools = lf->nr_pools;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return NPA_ERR_ALLOC;

	lf->stack_pg_ptrs = rsp->stack_pg_ptrs;
	lf->stack_pg_bytes = rsp->stack_pg_bytes;
	lf->qints = rsp->qints;

	return 0;
}

static int
npa_lf_free(struct mbox *mbox)
{
	mbox_alloc_msg_npa_lf_free(mbox);
	return mbox_process(mbox);
}

static inline uint32_t
aura_size_to_u32(uint8_t val)
{
	if (val == NPA_AURA_SZ_0)
		return 128;
	if (val >= NPA_AURA_SZ_MAX)
		return BIT_ULL(20);

	return 1 << (val + 6);
}

static inline void
pool_count_aura_sz_get(uint32_t *nr_pools, uint8_t *aura_sz)
{
	uint32_t val;

	val = roc_idev_npa_maxpools_get();
	if (val < aura_size_to_u32(NPA_AURA_SZ_128))
		val = 128;
	if (val > aura_size_to_u32(NPA_AURA_SZ_1M))
		val = BIT_ULL(20);

	roc_idev_npa_maxpools_set(val);
	*nr_pools = val;
	*aura_sz = plt_log2_u32(val) - 6;
}

static int
npa_dev_init(struct npa_lf *lf, uintptr_t base, struct mbox *mbox)
{
	uint32_t i, bmp_sz, nr_pools;
	uint8_t aura_sz;
	int rc;

	/* Sanity checks */
	if (!lf || !base || !mbox)
		return NPA_ERR_PARAM;

	if (base & ROC_AURA_ID_MASK)
		return NPA_ERR_BASE_INVALID;

	pool_count_aura_sz_get(&nr_pools, &aura_sz);
	if (aura_sz == NPA_AURA_SZ_0 || aura_sz >= NPA_AURA_SZ_MAX)
		return NPA_ERR_PARAM;

	memset(lf, 0x0, sizeof(*lf));
	lf->base = base;
	lf->aura_sz = aura_sz;
	lf->nr_pools = nr_pools;
	lf->mbox = mbox;

	rc = npa_lf_alloc(lf);
	if (rc)
		goto exit;

	bmp_sz = plt_bitmap_get_memory_footprint(nr_pools);

	/* Allocate memory for bitmap */
	lf->npa_bmp_mem = plt_zmalloc(bmp_sz, ROC_ALIGN);
	if (lf->npa_bmp_mem == NULL) {
		rc = NPA_ERR_ALLOC;
		goto lf_free;
	}

	/* Initialize pool resource bitmap array */
	lf->npa_bmp = plt_bitmap_init(nr_pools, lf->npa_bmp_mem, bmp_sz);
	if (lf->npa_bmp == NULL) {
		rc = NPA_ERR_PARAM;
		goto bmap_mem_free;
	}

	/* Mark all pools available */
	for (i = 0; i < nr_pools; i++)
		plt_bitmap_set(lf->npa_bmp, i);

	/* Allocate memory for qint context */
	lf->npa_qint_mem = plt_zmalloc(sizeof(struct npa_qint) * nr_pools, 0);
	if (lf->npa_qint_mem == NULL) {
		rc = NPA_ERR_ALLOC;
		goto bmap_free;
	}

	/* Allocate memory for nap_aura_lim memory */
	lf->aura_lim = plt_zmalloc(sizeof(struct npa_aura_lim) * nr_pools, 0);
	if (lf->aura_lim == NULL) {
		rc = NPA_ERR_ALLOC;
		goto qint_free;
	}

	/* Init aura start & end limits */
	for (i = 0; i < nr_pools; i++) {
		lf->aura_lim[i].ptr_start = UINT64_MAX;
		lf->aura_lim[i].ptr_end = 0x0ull;
	}

	return 0;

qint_free:
	plt_free(lf->npa_qint_mem);
bmap_free:
	plt_bitmap_free(lf->npa_bmp);
bmap_mem_free:
	plt_free(lf->npa_bmp_mem);
lf_free:
	npa_lf_free(lf->mbox);
exit:
	return rc;
}

static int
npa_dev_fini(struct npa_lf *lf)
{
	if (!lf)
		return NPA_ERR_PARAM;

	plt_free(lf->aura_lim);
	plt_free(lf->npa_qint_mem);
	plt_bitmap_free(lf->npa_bmp);
	plt_free(lf->npa_bmp_mem);

	return npa_lf_free(lf->mbox);
}

int
npa_lf_init(struct dev *dev, struct plt_pci_device *pci_dev)
{
	struct idev_cfg *idev;
	uint16_t npa_msixoff;
	struct npa_lf *lf;
	int rc;

	idev = idev_get_cfg();
	if (idev == NULL)
		return NPA_ERR_ALLOC;

	/* Not the first PCI device */
	if (__atomic_fetch_add(&idev->npa_refcnt, 1, __ATOMIC_SEQ_CST) != 0)
		return 0;

	rc = npa_attach(dev->mbox);
	if (rc)
		goto fail;

	rc = npa_get_msix_offset(dev->mbox, &npa_msixoff);
	if (rc)
		goto npa_detach;

	lf = &dev->npa;
	rc = npa_dev_init(lf, dev->bar2 + (RVU_BLOCK_ADDR_NPA << 20),
			  dev->mbox);
	if (rc)
		goto npa_detach;

	lf->pf_func = dev->pf_func;
	lf->npa_msixoff = npa_msixoff;
	lf->intr_handle = &pci_dev->intr_handle;
	lf->pci_dev = pci_dev;

	idev->npa_pf_func = dev->pf_func;
	idev->npa = lf;
	plt_wmb();

	rc = npa_register_irqs(lf);
	if (rc)
		goto npa_fini;

	plt_npa_dbg("npa=%p max_pools=%d pf_func=0x%x msix=0x%x", lf,
		    roc_idev_npa_maxpools_get(), lf->pf_func, npa_msixoff);

	return 0;

npa_fini:
	npa_dev_fini(idev->npa);
npa_detach:
	npa_detach(dev->mbox);
fail:
	__atomic_fetch_sub(&idev->npa_refcnt, 1, __ATOMIC_SEQ_CST);
	return rc;
}

int
npa_lf_fini(void)
{
	struct idev_cfg *idev;
	int rc = 0;

	idev = idev_get_cfg();
	if (idev == NULL)
		return NPA_ERR_ALLOC;

	/* Not the last PCI device */
	if (__atomic_sub_fetch(&idev->npa_refcnt, 1, __ATOMIC_SEQ_CST) != 0)
		return 0;

	npa_unregister_irqs(idev->npa);
	rc |= npa_dev_fini(idev->npa);
	rc |= npa_detach(idev->npa->mbox);
	idev_set_defaults(idev);

	return rc;
}

int
roc_npa_dev_init(struct roc_npa *roc_npa)
{
	struct plt_pci_device *pci_dev;
	struct npa *npa;
	struct dev *dev;
	int rc;

	if (roc_npa == NULL || roc_npa->pci_dev == NULL)
		return NPA_ERR_PARAM;

	PLT_STATIC_ASSERT(sizeof(struct npa) <= ROC_NPA_MEM_SZ);
	npa = roc_npa_to_npa_priv(roc_npa);
	memset(npa, 0, sizeof(*npa));
	pci_dev = roc_npa->pci_dev;
	dev = &npa->dev;

	/* Initialize device  */
	rc = dev_init(dev, pci_dev);
	if (rc) {
		plt_err("Failed to init roc device");
		goto fail;
	}

	npa->pci_dev = pci_dev;
	dev->drv_inited = true;
fail:
	return rc;
}

int
roc_npa_dev_fini(struct roc_npa *roc_npa)
{
	struct npa *npa = roc_npa_to_npa_priv(roc_npa);

	if (npa == NULL)
		return NPA_ERR_PARAM;

	npa->dev.drv_inited = false;
	return dev_fini(&npa->dev, npa->pci_dev);
}
