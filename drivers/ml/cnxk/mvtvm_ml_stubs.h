/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _MVTVM_ML_STUBS_H_
#define _MVTVM_ML_STUBS_H_

#include <rte_mldev.h>

#include "cnxk_ml_xstats.h"

struct cnxk_ml_dev;
struct cnxk_ml_model;
struct cnxk_ml_layer;

enum cnxk_ml_model_type mvtvm_ml_model_type_get(struct rte_ml_model_params *params);
int mvtvm_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info);
int mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf);
int mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev);
int mvtvm_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp);
int mvtvm_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
			struct cnxk_ml_model *model);
int mvtvm_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int mvtvm_ml_model_start(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int mvtvm_ml_model_stop(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);

int mvtvm_ml_model_get_layer_id(struct cnxk_ml_model *model, const char *layer_name,
				uint16_t *layer_id);
struct cnxk_ml_io_info *mvtvm_ml_model_io_info_get(struct cnxk_ml_model *model, uint16_t layer_id);
void mvtvm_ml_layer_print(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer, FILE *fp);
void mvtvm_ml_model_xstat_name_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
				   uint16_t stat_id, uint16_t entry, char *suffix);
uint64_t mvtvm_ml_model_xstat_get(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
				  enum cnxk_ml_xstats_type type);

#endif /* _MVTVM_ML_STUBS_H_ */
