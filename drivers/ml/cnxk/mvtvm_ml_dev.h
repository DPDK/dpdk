/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _MVTVM_ML_DEV_H_
#define _MVTVM_ML_DEV_H_

#include <rte_mldev_core.h>

/* Device status */
extern int cnxk_ml_dev_initialized;

/* CNXK Device ops */
extern struct rte_ml_dev_ops cnxk_ml_ops;

/* Marvell MVTVM ML PMD device name */
#define MLDEV_NAME_MVTVM_PMD ml_mvtvm

/* Maximum number of descriptors per queue-pair */
#define ML_MVTVM_MAX_DESC_PER_QP 1024

/* Maximum number of inputs / outputs per model */
#define ML_MVTVM_MAX_INPUT_OUTPUT 32

/* Maximum number of segments for IO data */
#define ML_MVTVM_MAX_SEGMENTS 1

/* Device private data */
struct mvtvm_ml_dev {
	/* Virtual device */
	struct rte_vdev_device *vdev;

	/* Maximum number of queue pairs */
	uint16_t max_nb_qpairs;

	/* Enable / disable model data caching */
	int cache_model_data;
};

#endif /* _MVTVM_ML_DEV_H_ */
