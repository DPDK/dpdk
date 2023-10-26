/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_DEV_H_
#define _CNXK_ML_DEV_H_

#include <roc_api.h>

#include "cn10k_ml_dev.h"

#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
#include "mvtvm_ml_dev.h"
#endif

#include "cnxk_ml_xstats.h"

/* ML command timeout in seconds */
#define ML_CNXK_CMD_TIMEOUT 5

/* Poll mode job state */
#define ML_CNXK_POLL_JOB_START	0
#define ML_CNXK_POLL_JOB_FINISH 1

/* Error types enumeration */
enum cnxk_ml_error_etype {
	/* 0x0 */ ML_CNXK_ETYPE_NO_ERROR = 0, /* No error */
	/* 0x1 */ ML_CNXK_ETYPE_FW_NONFATAL,  /* Firmware non-fatal error */
	/* 0x2 */ ML_CNXK_ETYPE_HW_NONFATAL,  /* Hardware non-fatal error */
	/* 0x3 */ ML_CNXK_ETYPE_HW_FATAL,     /* Hardware fatal error */
	/* 0x4 */ ML_CNXK_ETYPE_HW_WARNING,   /* Hardware warning */
	/* 0x5 */ ML_CNXK_ETYPE_DRIVER,	      /* Driver specific error */
	/* 0x6 */ ML_CNXK_ETYPE_UNKNOWN,      /* Unknown error */
};

struct cnxk_ml_error_db {
	uint64_t code;
	char str[RTE_ML_STR_MAX];
};

/* Device type */
enum cnxk_ml_dev_type {
	/* PCI based Marvell's ML HW accelerator device */
	CNXK_ML_DEV_TYPE_PCI,

	/* Generic Virtual device */
	CNXK_ML_DEV_TYPE_VDEV,
};

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

/* Index to model and layer ID map */
struct cnxk_ml_index_map {
	/* Model ID */
	uint16_t model_id;

	/* Layer ID */
	uint16_t layer_id;

	/* Layer status */
	bool active;
};

/* Device private data */
struct cnxk_ml_dev {
	/* RTE device */
	struct rte_ml_dev *mldev;

	/* Device type */
	enum cnxk_ml_dev_type type;

	/* Configuration state */
	enum cnxk_ml_dev_state state;

	/* Extended stats data */
	struct cnxk_ml_xstats xstats;

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

#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
	/* MVTVM device structure */
	struct mvtvm_ml_dev mvtvm_mldev;
#endif

	/* Maximum number of layers */
	uint64_t max_nb_layers;

	/* Index map */
	struct cnxk_ml_index_map *index_map;
};

extern struct cnxk_ml_error_db ml_etype_db[];

#endif /* _CNXK_ML_DEV_H_ */
