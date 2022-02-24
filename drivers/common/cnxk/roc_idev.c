/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

struct idev_cfg *
idev_get_cfg(void)
{
	static const char name[] = "roc_cn10k_intra_device_conf";
	const struct plt_memzone *mz;
	struct idev_cfg *idev;

	mz = plt_memzone_lookup(name);
	if (mz != NULL)
		return mz->addr;

	/* Request for the first time */
	mz = plt_memzone_reserve_cache_align(name, sizeof(struct idev_cfg));
	if (mz != NULL) {
		idev = mz->addr;
		idev_set_defaults(idev);
		return idev;
	}
	return NULL;
}

void
idev_set_defaults(struct idev_cfg *idev)
{
	idev->sso_pf_func = 0;
	idev->npa = NULL;
	idev->npa_pf_func = 0;
	idev->max_pools = 128;
	idev->lmt_pf_func = 0;
	idev->lmt_base_addr = 0;
	idev->num_lmtlines = 0;
	idev->bphy = NULL;
	idev->cpt = NULL;
	idev->nix_inl_dev = NULL;
	plt_spinlock_init(&idev->nix_inl_dev_lock);
	__atomic_store_n(&idev->npa_refcnt, 0, __ATOMIC_RELEASE);
}

uint16_t
idev_sso_pffunc_get(void)
{
	struct idev_cfg *idev;
	uint16_t sso_pf_func;

	idev = idev_get_cfg();
	sso_pf_func = 0;
	if (idev != NULL)
		sso_pf_func = __atomic_load_n(&idev->sso_pf_func,
					      __ATOMIC_ACQUIRE);

	return sso_pf_func;
}

void
idev_sso_pffunc_set(uint16_t sso_pf_func)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev != NULL)
		__atomic_store_n(&idev->sso_pf_func, sso_pf_func,
				 __ATOMIC_RELEASE);
}

uint16_t
idev_npa_pffunc_get(void)
{
	struct idev_cfg *idev;
	uint16_t npa_pf_func;

	idev = idev_get_cfg();
	npa_pf_func = 0;
	if (idev != NULL)
		npa_pf_func = idev->npa_pf_func;

	return npa_pf_func;
}

struct npa_lf *
idev_npa_obj_get(void)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev && __atomic_load_n(&idev->npa_refcnt, __ATOMIC_ACQUIRE))
		return idev->npa;

	return NULL;
}

uint32_t
roc_idev_npa_maxpools_get(void)
{
	struct idev_cfg *idev;
	uint32_t max_pools;

	idev = idev_get_cfg();
	max_pools = 0;
	if (idev != NULL)
		max_pools = idev->max_pools;

	return max_pools;
}

void
roc_idev_npa_maxpools_set(uint32_t max_pools)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev != NULL)
		__atomic_store_n(&idev->max_pools, max_pools, __ATOMIC_RELEASE);
}

uint16_t
idev_npa_lf_active(struct dev *dev)
{
	struct idev_cfg *idev;

	/* Check if npalf is actively used on this dev */
	idev = idev_get_cfg();
	if (!idev || !idev->npa || idev->npa->mbox != dev->mbox)
		return 0;

	return __atomic_load_n(&idev->npa_refcnt, __ATOMIC_ACQUIRE);
}

uint16_t
idev_lmt_pffunc_get(void)
{
	struct idev_cfg *idev;
	uint16_t lmt_pf_func;

	idev = idev_get_cfg();
	lmt_pf_func = 0;
	if (idev != NULL)
		lmt_pf_func = idev->lmt_pf_func;

	return lmt_pf_func;
}

uint64_t
roc_idev_lmt_base_addr_get(void)
{
	uint64_t lmt_base_addr;
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	lmt_base_addr = 0;
	if (idev != NULL)
		lmt_base_addr = idev->lmt_base_addr;

	return lmt_base_addr;
}

uint16_t
roc_idev_num_lmtlines_get(void)
{
	struct idev_cfg *idev;
	uint16_t num_lmtlines;

	idev = idev_get_cfg();
	num_lmtlines = 0;
	if (idev != NULL)
		num_lmtlines = idev->num_lmtlines;

	return num_lmtlines;
}

struct roc_cpt *
roc_idev_cpt_get(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		return idev->cpt;

	return NULL;
}

uint64_t *
roc_nix_inl_outb_ring_base_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev;

	if (!idev || !idev->nix_inl_dev)
		return NULL;

	inl_dev = idev->nix_inl_dev;

	return (uint64_t *)&inl_dev->sa_soft_exp_ring[nix->outb_se_ring_base];
}

void
roc_idev_cpt_set(struct roc_cpt *cpt)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		__atomic_store_n(&idev->cpt, cpt, __ATOMIC_RELEASE);
}

struct roc_nix *
roc_idev_npa_nix_get(void)
{
	struct npa_lf *npa_lf = idev_npa_obj_get();
	struct dev *dev;

	if (!npa_lf)
		return NULL;

	dev = container_of(npa_lf, struct dev, npa);
	return dev->roc_nix;
}

struct roc_sso *
idev_sso_get(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		return __atomic_load_n(&idev->sso, __ATOMIC_ACQUIRE);

	return NULL;
}

void
idev_sso_set(struct roc_sso *sso)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		__atomic_store_n(&idev->sso, sso, __ATOMIC_RELEASE);
}
