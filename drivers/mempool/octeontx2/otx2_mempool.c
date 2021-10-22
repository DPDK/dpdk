/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_atomic.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_io.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_pci.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_mempool.h"

#define OTX2_NPA_DEV_NAME	RTE_STR(otx2_npa_dev_)
#define OTX2_NPA_DEV_NAME_LEN	(sizeof(OTX2_NPA_DEV_NAME) + PCI_PRI_STR_SIZE)

static inline int
npa_lf_alloc(struct otx2_npa_lf *lf)
{
	struct otx2_mbox *mbox = lf->mbox;
	struct npa_lf_alloc_req *req;
	struct npa_lf_alloc_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_npa_lf_alloc(mbox);
	req->aura_sz = lf->aura_sz;
	req->nr_pools = lf->nr_pools;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return NPA_LF_ERR_ALLOC;

	lf->stack_pg_ptrs = rsp->stack_pg_ptrs;
	lf->stack_pg_bytes = rsp->stack_pg_bytes;
	lf->qints = rsp->qints;

	return 0;
}

static int
npa_lf_free(struct otx2_mbox *mbox)
{
	otx2_mbox_alloc_msg_npa_lf_free(mbox);

	return otx2_mbox_process(mbox);
}

static int
npa_lf_init(struct otx2_npa_lf *lf, uintptr_t base, uint8_t aura_sz,
	    uint32_t nr_pools, struct otx2_mbox *mbox)
{
	uint32_t i, bmp_sz;
	int rc;

	/* Sanity checks */
	if (!lf || !base || !mbox || !nr_pools)
		return NPA_LF_ERR_PARAM;

	if (base & AURA_ID_MASK)
		return NPA_LF_ERR_BASE_INVALID;

	if (aura_sz == NPA_AURA_SZ_0 || aura_sz >= NPA_AURA_SZ_MAX)
		return NPA_LF_ERR_PARAM;

	memset(lf, 0x0, sizeof(*lf));
	lf->base = base;
	lf->aura_sz = aura_sz;
	lf->nr_pools = nr_pools;
	lf->mbox = mbox;

	rc = npa_lf_alloc(lf);
	if (rc)
		goto exit;

	bmp_sz = rte_bitmap_get_memory_footprint(nr_pools);

	/* Allocate memory for bitmap */
	lf->npa_bmp_mem = rte_zmalloc("npa_bmp_mem", bmp_sz,
					RTE_CACHE_LINE_SIZE);
	if (lf->npa_bmp_mem == NULL) {
		rc = -ENOMEM;
		goto lf_free;
	}

	/* Initialize pool resource bitmap array */
	lf->npa_bmp = rte_bitmap_init(nr_pools, lf->npa_bmp_mem, bmp_sz);
	if (lf->npa_bmp == NULL) {
		rc = -EINVAL;
		goto bmap_mem_free;
	}

	/* Mark all pools available */
	for (i = 0; i < nr_pools; i++)
		rte_bitmap_set(lf->npa_bmp, i);

	/* Allocate memory for qint context */
	lf->npa_qint_mem = rte_zmalloc("npa_qint_mem",
			sizeof(struct otx2_npa_qint) * nr_pools, 0);
	if (lf->npa_qint_mem == NULL) {
		rc = -ENOMEM;
		goto bmap_free;
	}

	/* Allocate memory for nap_aura_lim memory */
	lf->aura_lim = rte_zmalloc("npa_aura_lim_mem",
			sizeof(struct npa_aura_lim) * nr_pools, 0);
	if (lf->aura_lim == NULL) {
		rc = -ENOMEM;
		goto qint_free;
	}

	/* Init aura start & end limits */
	for (i = 0; i < nr_pools; i++) {
		lf->aura_lim[i].ptr_start = UINT64_MAX;
		lf->aura_lim[i].ptr_end = 0x0ull;
	}

	return 0;

qint_free:
	rte_free(lf->npa_qint_mem);
bmap_free:
	rte_bitmap_free(lf->npa_bmp);
bmap_mem_free:
	rte_free(lf->npa_bmp_mem);
lf_free:
	npa_lf_free(lf->mbox);
exit:
	return rc;
}

static int
npa_lf_fini(struct otx2_npa_lf *lf)
{
	if (!lf)
		return NPA_LF_ERR_PARAM;

	rte_free(lf->aura_lim);
	rte_free(lf->npa_qint_mem);
	rte_bitmap_free(lf->npa_bmp);
	rte_free(lf->npa_bmp_mem);

	return npa_lf_free(lf->mbox);

}

static inline uint32_t
otx2_aura_size_to_u32(uint8_t val)
{
	if (val == NPA_AURA_SZ_0)
		return 128;
	if (val >= NPA_AURA_SZ_MAX)
		return BIT_ULL(20);

	return 1 << (val + 6);
}

static int
parse_max_pools(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);
	if (val < otx2_aura_size_to_u32(NPA_AURA_SZ_128))
		val = 128;
	if (val > otx2_aura_size_to_u32(NPA_AURA_SZ_1M))
		val = BIT_ULL(20);

	*(uint8_t *)extra_args = rte_log2_u32(val) - 6;
	return 0;
}

#define OTX2_MAX_POOLS "max_pools"

static uint8_t
otx2_parse_aura_size(struct rte_devargs *devargs)
{
	uint8_t aura_sz = NPA_AURA_SZ_128;
	struct rte_kvargs *kvlist;

	if (devargs == NULL)
		goto exit;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		goto exit;

	rte_kvargs_process(kvlist, OTX2_MAX_POOLS, &parse_max_pools, &aura_sz);
	otx2_parse_common_devargs(kvlist);
	rte_kvargs_free(kvlist);
exit:
	return aura_sz;
}

static inline int
npa_lf_attach(struct otx2_mbox *mbox)
{
	struct rsrc_attach_req *req;

	req = otx2_mbox_alloc_msg_attach_resources(mbox);
	req->npalf = true;

	return otx2_mbox_process(mbox);
}

static inline int
npa_lf_detach(struct otx2_mbox *mbox)
{
	struct rsrc_detach_req *req;

	req = otx2_mbox_alloc_msg_detach_resources(mbox);
	req->npalf = true;

	return otx2_mbox_process(mbox);
}

static inline int
npa_lf_get_msix_offset(struct otx2_mbox *mbox, uint16_t *npa_msixoff)
{
	struct msix_offset_rsp *msix_rsp;
	int rc;

	/* Get NPA and NIX MSIX vector offsets */
	otx2_mbox_alloc_msg_msix_offset(mbox);

	rc = otx2_mbox_process_msg(mbox, (void *)&msix_rsp);

	*npa_msixoff = msix_rsp->npa_msixoff;

	return rc;
}

/**
 * @internal
 * Finalize NPA LF.
 */
int
otx2_npa_lf_fini(void)
{
	struct otx2_idev_cfg *idev;
	int rc = 0;

	idev = otx2_intra_dev_get_cfg();
	if (idev == NULL)
		return -ENOMEM;

	if (rte_atomic16_add_return(&idev->npa_refcnt, -1) == 0) {
		otx2_npa_unregister_irqs(idev->npa_lf);
		rc |= npa_lf_fini(idev->npa_lf);
		rc |= npa_lf_detach(idev->npa_lf->mbox);
		otx2_npa_set_defaults(idev);
	}

	return rc;
}

/**
 * @internal
 * Initialize NPA LF.
 */
int
otx2_npa_lf_init(struct rte_pci_device *pci_dev, void *otx2_dev)
{
	struct otx2_dev *dev = otx2_dev;
	struct otx2_idev_cfg *idev;
	struct otx2_npa_lf *lf;
	uint16_t npa_msixoff;
	uint32_t nr_pools;
	uint8_t aura_sz;
	int rc;

	idev = otx2_intra_dev_get_cfg();
	if (idev == NULL)
		return -ENOMEM;

	/* Is NPA LF initialized by any another driver? */
	if (rte_atomic16_add_return(&idev->npa_refcnt, 1) == 1) {

		rc = npa_lf_attach(dev->mbox);
		if (rc)
			goto fail;

		rc = npa_lf_get_msix_offset(dev->mbox, &npa_msixoff);
		if (rc)
			goto npa_detach;

		aura_sz = otx2_parse_aura_size(pci_dev->device.devargs);
		nr_pools = otx2_aura_size_to_u32(aura_sz);

		lf = &dev->npalf;
		rc = npa_lf_init(lf, dev->bar2 + (RVU_BLOCK_ADDR_NPA << 20),
					aura_sz, nr_pools, dev->mbox);

		if (rc)
			goto npa_detach;

		lf->pf_func = dev->pf_func;
		lf->npa_msixoff = npa_msixoff;
		lf->intr_handle = pci_dev->intr_handle;
		lf->pci_dev = pci_dev;

		idev->npa_pf_func = dev->pf_func;
		idev->npa_lf = lf;
		rte_smp_wmb();
		rc = otx2_npa_register_irqs(lf);
		if (rc)
			goto npa_fini;

		rte_mbuf_set_platform_mempool_ops("octeontx2_npa");
		otx2_npa_dbg("npa_lf=%p pools=%d sz=%d pf_func=0x%x msix=0x%x",
			     lf, nr_pools, aura_sz, lf->pf_func, npa_msixoff);
	}

	return 0;

npa_fini:
	npa_lf_fini(idev->npa_lf);
npa_detach:
	npa_lf_detach(dev->mbox);
fail:
	rte_atomic16_dec(&idev->npa_refcnt);
	return rc;
}

static inline char*
otx2_npa_dev_to_name(struct rte_pci_device *pci_dev, char *name)
{
	snprintf(name, OTX2_NPA_DEV_NAME_LEN,
		 OTX2_NPA_DEV_NAME  PCI_PRI_FMT,
		 pci_dev->addr.domain, pci_dev->addr.bus,
		 pci_dev->addr.devid, pci_dev->addr.function);

	return name;
}

static int
otx2_npa_init(struct rte_pci_device *pci_dev)
{
	char name[OTX2_NPA_DEV_NAME_LEN];
	const struct rte_memzone *mz;
	struct otx2_dev *dev;
	int rc = -ENOMEM;

	mz = rte_memzone_reserve_aligned(otx2_npa_dev_to_name(pci_dev, name),
					 sizeof(*dev), SOCKET_ID_ANY,
					 0, OTX2_ALIGN);
	if (mz == NULL)
		goto error;

	dev = mz->addr;

	/* Initialize the base otx2_dev object */
	rc = otx2_dev_init(pci_dev, dev);
	if (rc)
		goto malloc_fail;

	/* Grab the NPA LF if required */
	rc = otx2_npa_lf_init(pci_dev, dev);
	if (rc)
		goto dev_uninit;

	dev->drv_inited = true;
	return 0;

dev_uninit:
	otx2_npa_lf_fini();
	otx2_dev_fini(pci_dev, dev);
malloc_fail:
	rte_memzone_free(mz);
error:
	otx2_err("Failed to initialize npa device rc=%d", rc);
	return rc;
}

static int
otx2_npa_fini(struct rte_pci_device *pci_dev)
{
	char name[OTX2_NPA_DEV_NAME_LEN];
	const struct rte_memzone *mz;
	struct otx2_dev *dev;

	mz = rte_memzone_lookup(otx2_npa_dev_to_name(pci_dev, name));
	if (mz == NULL)
		return -EINVAL;

	dev = mz->addr;
	if (!dev->drv_inited)
		goto dev_fini;

	dev->drv_inited = false;
	otx2_npa_lf_fini();

dev_fini:
	if (otx2_npa_lf_active(dev)) {
		otx2_info("%s: common resource in use by other devices",
			  pci_dev->name);
		return -EAGAIN;
	}

	otx2_dev_fini(pci_dev, dev);
	rte_memzone_free(mz);

	return 0;
}

static int
npa_remove(struct rte_pci_device *pci_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return otx2_npa_fini(pci_dev);
}

static int
npa_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_drv);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return otx2_npa_init(pci_dev);
}

static const struct rte_pci_id pci_npa_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
					PCI_DEVID_OCTEONTX2_RVU_NPA_PF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
					PCI_DEVID_OCTEONTX2_RVU_NPA_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_npa = {
	.id_table = pci_npa_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = npa_probe,
	.remove = npa_remove,
};

RTE_PMD_REGISTER_PCI(mempool_octeontx2, pci_npa);
RTE_PMD_REGISTER_PCI_TABLE(mempool_octeontx2, pci_npa_map);
RTE_PMD_REGISTER_KMOD_DEP(mempool_octeontx2, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(mempool_octeontx2,
			      OTX2_MAX_POOLS "=<128-1048576>"
			      OTX2_NPA_LOCK_MASK "=<1-65535>");
