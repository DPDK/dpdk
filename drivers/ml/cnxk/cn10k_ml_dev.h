/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_DEV_H_
#define _CN10K_ML_DEV_H_

#include <roc_api.h>

/* Marvell OCTEON CN10K ML PMD device name */
#define MLDEV_NAME_CN10K_PMD ml_cn10k

/* Device private data */
struct cn10k_ml_dev {
	/* Device ROC */
	struct roc_ml roc;
};

#endif /* _CN10K_ML_DEV_H_ */
