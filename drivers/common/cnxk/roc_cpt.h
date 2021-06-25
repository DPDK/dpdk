/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_CPT_H_
#define _ROC_CPT_H_

#include "roc_api.h"

/* Default engine groups */
#define ROC_CPT_DFLT_ENG_GRP_SE	   0UL
#define ROC_CPT_DFLT_ENG_GRP_SE_IE 1UL
#define ROC_CPT_DFLT_ENG_GRP_AE	   2UL

#define ROC_CPT_MAX_LFS 64

struct roc_cpt_lf {
	/* Input parameters */
	uint16_t lf_id;
	uint32_t nb_desc;
	/* End of Input parameters */
	struct plt_pci_device *pci_dev;
	struct dev *dev;
	struct roc_cpt *roc_cpt;
	uintptr_t rbase;
	uintptr_t lmt_base;
	uint16_t msixoff;
	uint16_t pf_func;
	uint64_t *fc_addr;
	uint64_t io_addr;
	uint8_t *iq_vaddr;
} __plt_cache_aligned;

struct roc_cpt {
	struct plt_pci_device *pci_dev;
	struct roc_cpt_lf *lf[ROC_CPT_MAX_LFS];
	uint16_t nb_lf;
	uint16_t nb_lf_avail;
	uintptr_t lmt_base;
	/**< CPT device capabilities */
	union cpt_eng_caps hw_caps[CPT_MAX_ENG_TYPES];
	uint8_t eng_grp[CPT_MAX_ENG_TYPES];

#define ROC_CPT_MEM_SZ (6 * 1024)
	uint8_t reserved[ROC_CPT_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

struct roc_cpt_rxc_time_cfg {
	uint32_t step;
	uint16_t active_limit;
	uint16_t active_thres;
	uint16_t zombie_limit;
	uint16_t zombie_thres;
};

int __roc_api roc_cpt_rxc_time_cfg(struct roc_cpt *roc_cpt,
				   struct roc_cpt_rxc_time_cfg *cfg);
int __roc_api roc_cpt_dev_init(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_dev_fini(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_eng_grp_add(struct roc_cpt *roc_cpt,
				  enum cpt_eng_type eng_type);
int __roc_api roc_cpt_dev_configure(struct roc_cpt *roc_cpt, int nb_lf);
void __roc_api roc_cpt_dev_clear(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_lf_init(struct roc_cpt *roc_cpt, struct roc_cpt_lf *lf);
void __roc_api roc_cpt_lf_fini(struct roc_cpt_lf *lf);
int __roc_api roc_cpt_lf_ctx_flush(struct roc_cpt_lf *lf, uint64_t cptr);
int __roc_api roc_cpt_afs_print(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_lfs_print(struct roc_cpt *roc_cpt);
void __roc_api roc_cpt_iq_disable(struct roc_cpt_lf *lf);

#endif /* _ROC_CPT_H_ */
