/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_XSTATS_H_
#define _CNXK_ML_XSTATS_H_

#include "cnxk_ml_io.h"

struct cnxk_ml_dev;

/* Extended stats types enum */
enum cnxk_ml_xstats_type {
	/* Number of models loaded */
	nb_models_loaded,

	/* Number of models unloaded */
	nb_models_unloaded,

	/* Number of models started */
	nb_models_started,

	/* Number of models stopped */
	nb_models_stopped,

	/* Average inference hardware latency */
	avg_hw_latency,

	/* Minimum hardware latency */
	min_hw_latency,

	/* Maximum hardware latency */
	max_hw_latency,

	/* Average firmware latency */
	avg_fw_latency,

	/* Minimum firmware latency */
	min_fw_latency,

	/* Maximum firmware latency */
	max_fw_latency,

	/* Average runtime latency */
	avg_rt_latency,

	/* Minimum runtime latency */
	min_rt_latency,

	/* Maximum runtime latency */
	max_rt_latency,
};

/* Extended stats function type enum. */
enum cnxk_ml_xstats_fn_type {
	/* Device function */
	CNXK_ML_XSTATS_FN_DEVICE,

	/* Model function */
	CNXK_ML_XSTATS_FN_MODEL,
};

/* Extended stats group */
enum cnxk_ml_xstats_group {
	/* Device stats */
	CNXK_ML_XSTATS_GROUP_DEVICE,

	/* Model stats */
	CNXK_ML_XSTATS_GROUP_MODEL,

	/* Layer stats */
	CNXK_ML_XSTATS_GROUP_LAYER,
};

/* Function pointer to get xstats for a type */
typedef uint64_t (*cnxk_ml_xstats_fn)(struct cnxk_ml_dev *cnxk_mldev, uint16_t obj_idx,
				      int32_t layer_id, enum cnxk_ml_xstats_type stat);

/* Extended stats entry structure */
struct cnxk_ml_xstats_entry {
	/* Name-ID map */
	struct rte_ml_dev_xstats_map map;

	/* xstats mode, device or model */
	enum rte_ml_dev_xstats_mode mode;

	/* xstats group */
	enum cnxk_ml_xstats_group group;

	/* Type of xstats */
	enum cnxk_ml_xstats_type type;

	/* xstats function */
	enum cnxk_ml_xstats_fn_type fn_id;

	/* Object ID, model ID for model stat type */
	uint16_t obj_idx;

	/* Layer ID, valid for model stat type */
	int32_t layer_id;

	/* Allowed to reset the stat */
	uint8_t reset_allowed;

	/* An offset to be taken away to emulate resets */
	uint64_t reset_value;
};

/* Extended stats data */
struct cnxk_ml_xstats {
	/* Pointer to xstats entries */
	struct cnxk_ml_xstats_entry *entries;

	/* Store num stats and offset of the stats for each model */
	uint16_t count_per_model[ML_CNXK_MAX_MODELS];
	uint16_t offset_for_model[ML_CNXK_MAX_MODELS];
	uint16_t count_per_layer[ML_CNXK_MAX_MODELS][ML_CNXK_MODEL_MAX_LAYERS];
	uint16_t offset_for_layer[ML_CNXK_MAX_MODELS][ML_CNXK_MODEL_MAX_LAYERS];
	uint16_t count_mode_device;
	uint16_t count_mode_model;
	uint16_t count;
};

struct cnxk_ml_xstat_info {
	char name[32];
	enum cnxk_ml_xstats_type type;
	uint8_t reset_allowed;
};

/* Device xstats. Note: Device stats are not allowed to be reset. */
static const struct cnxk_ml_xstat_info device_xstats[] = {
	{"nb_models_loaded", nb_models_loaded, 0},
	{"nb_models_unloaded", nb_models_unloaded, 0},
	{"nb_models_started", nb_models_started, 0},
	{"nb_models_stopped", nb_models_stopped, 0},
};

/* Layer xstats */
static const struct cnxk_ml_xstat_info layer_xstats[] = {
	{"Avg-HW-Latency", avg_hw_latency, 1}, {"Min-HW-Latency", min_hw_latency, 1},
	{"Max-HW-Latency", max_hw_latency, 1}, {"Avg-FW-Latency", avg_fw_latency, 1},
	{"Min-FW-Latency", min_fw_latency, 1}, {"Max-FW-Latency", max_fw_latency, 1},
};

/* Model xstats */
static const struct cnxk_ml_xstat_info model_xstats[] = {
	{"Avg-RT-Latency", avg_rt_latency, 1},
	{"Min-RT-Latency", min_rt_latency, 1},
	{"Max-RT-Latency", max_rt_latency, 1},
};

#endif /* _CNXK_ML_XSTATS_H_ */
