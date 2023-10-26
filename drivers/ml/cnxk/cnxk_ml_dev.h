/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_DEV_H_
#define _CNXK_ML_DEV_H_

#include <roc_api.h>

#include "cn10k_ml_dev.h"

/* ML command timeout in seconds */
#define ML_CNXK_CMD_TIMEOUT 5

/* Poll mode job state */
#define ML_CNXK_POLL_JOB_START	0
#define ML_CNXK_POLL_JOB_FINISH 1

/* Device configuration state enum */
enum cnxk_ml_dev_state {
	/* Probed and not configured */
	ML_CNXK_DEV_STATE_PROBED = 0,

	/* Configured */
	ML_CNXK_DEV_STATE_CONFIGURED,

	/* Started */
	ML_CNXK_DEV_STATE_STARTED,

	/* Closed */
	ML_CNXK_DEV_STATE_CLOSED
};

/* Device private data */
struct cnxk_ml_dev {
	/* RTE device */
	struct rte_ml_dev *mldev;

	/* Configuration state */
	enum cnxk_ml_dev_state state;

	/* Number of models loaded */
	uint16_t nb_models_loaded;

	/* Number of models unloaded */
	uint16_t nb_models_unloaded;

	/* Number of models started */
	uint16_t nb_models_started;

	/* Number of models stopped */
	uint16_t nb_models_stopped;

	/* CN10K device structure */
	struct cn10k_ml_dev cn10k_mldev;

	/* Maximum number of layers */
	uint64_t max_nb_layers;
};

#endif /* _CNXK_ML_DEV_H_ */
