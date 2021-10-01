/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_CPT_PRIV_H_
#define _ROC_CPT_PRIV_H_

struct cpt {
	struct plt_pci_device *pci_dev;
	struct dev dev;
	uint16_t lf_msix_off[ROC_CPT_MAX_LFS];
	uint8_t lf_blkaddr[ROC_CPT_MAX_LFS];
} __plt_cache_aligned;

static inline struct cpt *
roc_cpt_to_cpt_priv(struct roc_cpt *roc_cpt)
{
	return (struct cpt *)&roc_cpt->reserved[0];
}

int cpt_lfs_attach(struct dev *dev, uint8_t blkaddr, bool modify,
		   uint16_t nb_lf);
int cpt_lfs_detach(struct dev *dev);
int cpt_lfs_alloc(struct dev *dev, uint8_t eng_grpmsk, uint8_t blk,
		  bool inl_dev_sso);
int cpt_lfs_free(struct dev *dev);
int cpt_lf_init(struct roc_cpt_lf *lf);
void cpt_lf_fini(struct roc_cpt_lf *lf);

int cpt_lf_outb_cfg(struct dev *dev, uint16_t sso_pf_func, uint16_t nix_pf_func,
		    uint8_t lf_id, bool ena);
int cpt_get_msix_offset(struct dev *dev, struct msix_offset_rsp **msix_rsp);
uint64_t cpt_get_blkaddr(struct dev *dev);
void cpt_lf_print(struct roc_cpt_lf *lf);

#endif /* _ROC_CPT_PRIV_H_ */
