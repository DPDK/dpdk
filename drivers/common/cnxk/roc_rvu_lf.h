/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _ROC_RVU_LF_H_
#define _ROC_RVU_LF_H_

#include "roc_platform.h"

struct roc_rvu_lf {
	TAILQ_ENTRY(roc_rvu_lf) next;
	struct plt_pci_device *pci_dev;
	uint8_t idx;
#define ROC_RVU_MEM_SZ (6 * 1024)
	uint8_t reserved[ROC_RVU_MEM_SZ] __plt_cache_aligned;
};

TAILQ_HEAD(roc_rvu_lf_head, roc_rvu_lf);

/* Dev */
int __roc_api roc_rvu_lf_dev_init(struct roc_rvu_lf *roc_rvu_lf);
int __roc_api roc_rvu_lf_dev_fini(struct roc_rvu_lf *roc_rvu_lf);

typedef void (*roc_rvu_lf_intr_cb_fn)(void *cb_arg);
int __roc_api roc_rvu_lf_irq_register(struct roc_rvu_lf *roc_rvu_lf, unsigned int irq,
				      roc_rvu_lf_intr_cb_fn cb, void *cb_arg);
int __roc_api roc_rvu_lf_irq_unregister(struct roc_rvu_lf *roc_rvu_lf, unsigned int irq,
					roc_rvu_lf_intr_cb_fn cb, void *cb_arg);
#endif /* _ROC_RVU_LF_H_ */
