/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_DEV_H_
#define _CN10K_ML_DEV_H_

#include <roc_api.h>

/* Marvell OCTEON CN10K ML PMD device name */
#define MLDEV_NAME_CN10K_PMD ml_cn10k

/* Device alignment size */
#define ML_CN10K_ALIGN_SIZE 128

/* Maximum number of models per device */
#define ML_CN10K_MAX_MODELS 16

/* Maximum number of queue-pairs per device */
#define ML_CN10K_MAX_QP_PER_DEVICE 1

/* Maximum number of descriptors per queue-pair */
#define ML_CN10K_MAX_DESC_PER_QP 1024

/* Maximum number of segments for IO data */
#define ML_CN10K_MAX_SEGMENTS 1

/* Device private data */
struct cn10k_ml_dev {
	/* Device ROC */
	struct roc_ml roc;
};

#endif /* _CN10K_ML_DEV_H_ */
