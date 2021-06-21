/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_BPHY_CGX_H_
#define _ROC_BPHY_CGX_H_

#include <pthread.h>

#include "roc_api.h"

struct roc_bphy_cgx {
	uint64_t bar0_pa;
	void *bar0_va;
	uint64_t lmac_bmap;
	unsigned int id;
	/* serialize access to the whole structure */
	pthread_mutex_t lock;
} __plt_cache_aligned;

__roc_api int roc_bphy_cgx_dev_init(struct roc_bphy_cgx *roc_cgx);
__roc_api int roc_bphy_cgx_dev_fini(struct roc_bphy_cgx *roc_cgx);

#endif /* _ROC_BPHY_CGX_H_ */
