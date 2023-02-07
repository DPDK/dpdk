/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_MODEL_H_
#define _CN10K_ML_MODEL_H_

#include <rte_mldev.h>

#include <roc_api.h>

#include "cn10k_ml_dev.h"

/* Model state */
enum cn10k_ml_model_state {
	ML_CN10K_MODEL_STATE_LOADED,
	ML_CN10K_MODEL_STATE_JOB_ACTIVE,
	ML_CN10K_MODEL_STATE_STARTED,
	ML_CN10K_MODEL_STATE_UNKNOWN,
};

/* Model Object */
struct cn10k_ml_model {
	/* Device reference */
	struct cn10k_ml_dev *mldev;

	/* Name */
	char name[RTE_ML_STR_MAX];

	/* ID */
	uint16_t model_id;

	/* Spinlock, used to update model state */
	plt_spinlock_t lock;

	/* State */
	enum cn10k_ml_model_state state;
};

#endif /* _CN10K_ML_MODEL_H_ */
