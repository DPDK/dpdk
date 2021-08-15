/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell.
 */

#ifndef _ROC_BPHY_
#define _ROC_BPHY_

#include "roc_api.h"
#include "roc_bphy_irq.h"

struct roc_bphy {
	struct plt_pci_device *pci_dev;
} __plt_cache_aligned;

int __roc_api roc_bphy_dev_init(struct roc_bphy *roc_bphy);
int __roc_api roc_bphy_dev_fini(struct roc_bphy *roc_bphy);
__roc_api uint16_t roc_bphy_npa_pf_func_get(void);
__roc_api uint16_t roc_bphy_sso_pf_func_get(void);

#endif /* _ROC_BPHY_ */
