/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_IDEV_PRIV_H_
#define _ROC_IDEV_PRIV_H_

/* Intra device related functions */
struct npa_lf;
struct roc_bphy;
struct roc_cpt;
struct nix_inl_dev;

struct idev_nix_inl_cfg {
	uint64_t meta_aura;
	uintptr_t meta_mempool;
	uint32_t nb_bufs;
	uint32_t buf_sz;
	uint32_t refs;
};

struct idev_cfg {
	uint16_t sso_pf_func;
	uint16_t npa_pf_func;
	struct npa_lf *npa;
	uint16_t npa_refcnt;
	uint32_t max_pools;
	uint16_t lmt_pf_func;
	uint16_t num_lmtlines;
	uint64_t lmt_base_addr;
	struct roc_bphy *bphy;
	struct roc_cpt *cpt;
	struct roc_sso *sso;
	struct roc_mcs_head mcs_list;
	struct nix_inl_dev *nix_inl_dev;
	struct idev_nix_inl_cfg inl_cfg;
	struct roc_nix_list roc_nix_list;
	plt_spinlock_t nix_inl_dev_lock;
	plt_spinlock_t npa_dev_lock;
};

/* Generic */
struct idev_cfg *idev_get_cfg(void);
void idev_set_defaults(struct idev_cfg *idev);

/* idev npa */
uint16_t idev_npa_pffunc_get(void);
struct npa_lf *idev_npa_obj_get(void);
uint32_t idev_npa_maxpools_get(void);
void idev_npa_maxpools_set(uint32_t max_pools);
uint16_t idev_npa_lf_active(struct dev *dev);

/* idev sso */
void idev_sso_pffunc_set(uint16_t sso_pf_func);
uint16_t idev_sso_pffunc_get(void);
struct roc_sso *idev_sso_get(void);
void idev_sso_set(struct roc_sso *sso);

/* idev lmt */
uint16_t idev_lmt_pffunc_get(void);

#endif /* _ROC_IDEV_PRIV_H_ */
