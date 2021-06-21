/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell.
 */

#ifndef _ROC_BPHY_
#define _ROC_BPHY_

#include "roc_api.h"

struct roc_bphy {
	struct plt_pci_device *pci_dev;
} __plt_cache_aligned;

int __roc_api roc_bphy_dev_init(struct roc_bphy *roc_bphy);
int __roc_api roc_bphy_dev_fini(struct roc_bphy *roc_bphy);

#endif /* _ROC_BPHY_ */
