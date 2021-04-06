/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NPA_H_
#define _ROC_NPA_H_

#define ROC_AURA_ID_MASK       (BIT_ULL(16) - 1)

struct roc_npa {
	struct plt_pci_device *pci_dev;

#define ROC_NPA_MEM_SZ (1 * 1024)
	uint8_t reserved[ROC_NPA_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

int __roc_api roc_npa_dev_init(struct roc_npa *roc_npa);
int __roc_api roc_npa_dev_fini(struct roc_npa *roc_npa);

/* Debug */
int __roc_api roc_npa_ctx_dump(void);
int __roc_api roc_npa_dump(void);

#endif /* _ROC_NPA_H_ */
