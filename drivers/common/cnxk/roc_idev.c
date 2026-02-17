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
	idev->halo_ena = 0;
	idev->lmt_pf_func = 0;
	idev->lmt_base_addr = 0;
	idev->num_lmtlines = 0;
	idev->bphy = NULL;
	idev->cpt = NULL;
	TAILQ_INIT(&idev->rvu_lf_list);
	TAILQ_INIT(&idev->mcs_list);
	idev->nix_inl_dev = NULL;
	TAILQ_INIT(&idev->roc_nix_list);
	plt_spinlock_init(&idev->nix_inl_dev_lock);
	plt_spinlock_init(&idev->npa_dev_lock);
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

int
roc_idev_npa_halo_ena_get(void)
{
	struct idev_cfg *idev;
	int halo_ena;

	idev = idev_get_cfg();
	halo_ena = 0;
	if (idev != NULL)
		halo_ena = __atomic_load_n(&idev->halo_ena, __ATOMIC_ACQUIRE);

	return halo_ena;
}

void
roc_idev_npa_halo_ena_set(int halo_ena)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev != NULL)
		__atomic_store_n(&idev->halo_ena, halo_ena, __ATOMIC_RELEASE);
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

struct roc_rvu_lf *
roc_idev_rvu_lf_get(uint8_t rvu_lf_idx)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_rvu_lf *rvu_lf = NULL;

	if (idev != NULL) {
		TAILQ_FOREACH(rvu_lf, &idev->rvu_lf_list, next) {
			if (rvu_lf->idx == rvu_lf_idx)
				return rvu_lf;
		}
	}

	return NULL;
}

void
roc_idev_rvu_lf_set(struct roc_rvu_lf *rvu)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_rvu_lf *rvu_lf_iter = NULL;

	if (idev != NULL) {
		TAILQ_FOREACH(rvu_lf_iter, &idev->rvu_lf_list, next) {
			if (rvu_lf_iter->idx == rvu->idx)
				return;
		}
		TAILQ_INSERT_TAIL(&idev->rvu_lf_list, rvu, next);
	}
}

void
roc_idev_rvu_lf_free(struct roc_rvu_lf *rvu)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_rvu_lf *rvu_lf_iter = NULL;

	if (idev != NULL) {
		TAILQ_FOREACH(rvu_lf_iter, &idev->rvu_lf_list, next) {
			if (rvu_lf_iter->idx == rvu->idx)
				TAILQ_REMOVE(&idev->rvu_lf_list, rvu, next);
		}
	}
}

struct roc_mcs *
roc_idev_mcs_get(uint8_t mcs_idx)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_mcs *mcs = NULL;

	if (idev != NULL) {
		TAILQ_FOREACH(mcs, &idev->mcs_list, next) {
			if (mcs->idx == mcs_idx)
				return mcs;
		}
	}

	return NULL;
}

void
roc_idev_mcs_set(struct roc_mcs *mcs)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_mcs *mcs_iter = NULL;

	if (idev != NULL) {
		TAILQ_FOREACH(mcs_iter, &idev->mcs_list, next) {
			if (mcs_iter->idx == mcs->idx)
				return;
		}
		TAILQ_INSERT_TAIL(&idev->mcs_list, mcs, next);
	}
}

void
roc_idev_mcs_free(struct roc_mcs *mcs)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct roc_mcs *mcs_iter = NULL;

	if (idev != NULL) {
		TAILQ_FOREACH(mcs_iter, &idev->mcs_list, next) {
			if (mcs_iter->idx == mcs->idx)
				TAILQ_REMOVE(&idev->mcs_list, mcs, next);
		}
	}
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

struct roc_nix_list *
roc_idev_nix_list_get(void)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev != NULL)
		return &idev->roc_nix_list;
	return NULL;
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

void
idev_dma_cs_offset_set(uint8_t offset)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		idev->dma_cs_offset = offset;
}

uint8_t
idev_dma_cs_offset_get(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		return idev->dma_cs_offset;

	return 0;
}

uint64_t
roc_idev_nix_inl_meta_aura_get(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		return idev->inl_cfg.meta_aura;
	return 0;
}

uint8_t
roc_idev_nix_rx_inject_get(uint16_t port)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev != NULL && port < PLT_MAX_ETHPORTS)
		return idev->inl_rx_inj_cfg.rx_inject_en[port];

	return 0;
}

void
roc_idev_nix_rx_inject_set(uint16_t port, uint8_t enable)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev != NULL && port < PLT_MAX_ETHPORTS)
		__atomic_store_n(&idev->inl_rx_inj_cfg.rx_inject_en[port], enable,
				 __ATOMIC_RELEASE);
}

uint16_t *
roc_idev_nix_rx_chan_base_get(void)
{
	struct idev_cfg *idev = idev_get_cfg();

	if (idev != NULL)
		return (uint16_t *)&idev->inl_rx_inj_cfg.chan;

	return NULL;
}

void
roc_idev_nix_rx_chan_set(uint16_t port, uint16_t chan)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (idev != NULL && port < PLT_MAX_ETHPORTS)
		__atomic_store_n(&idev->inl_rx_inj_cfg.chan[port], chan, __ATOMIC_RELEASE);
}

uint16_t
roc_idev_nix_inl_dev_pffunc_get(void)
{
	return nix_inl_dev_pffunc_get();
}
