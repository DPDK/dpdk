/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>

#include "mvtvm_ml_stubs.h"

#include "cnxk_ml_dev.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_xstats.h"

enum cnxk_ml_model_type
mvtvm_ml_model_type_get(struct rte_ml_model_params *params)
{
	RTE_SET_USED(params);

	return ML_CNXK_MODEL_TYPE_UNKNOWN;
}

int
mvtvm_ml_model_get_layer_id(struct cnxk_ml_model *model, const char *layer_name, uint16_t *layer_id)
{
	RTE_SET_USED(model);
	RTE_SET_USED(layer_name);
	RTE_SET_USED(layer_id);

	return -EINVAL;
}

struct cnxk_ml_io_info *
mvtvm_ml_model_io_info_get(struct cnxk_ml_model *model, uint16_t layer_id)
{
	RTE_SET_USED(model);
	RTE_SET_USED(layer_id);

	return NULL;
}

void
mvtvm_ml_layer_print(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer, FILE *fp)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(layer);
	RTE_SET_USED(fp);
}

void
mvtvm_ml_model_xstat_name_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
			      uint16_t stat_id, uint16_t entry, char *suffix)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(model);
	RTE_SET_USED(stat_id);
	RTE_SET_USED(entry);
	RTE_SET_USED(suffix);
}

uint64_t
mvtvm_ml_model_xstat_get(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
			 enum cnxk_ml_xstats_type type)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(model);
	RTE_SET_USED(type);

	return 0;
}

int
mvtvm_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(dev_info);

	return -ENOTSUP;
}

int
mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(conf);

	return 0;
}

int
mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev)
{
	RTE_SET_USED(cnxk_mldev);

	return 0;
}

int
mvtvm_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(fp);

	return -EINVAL;
}

int
mvtvm_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
		    struct cnxk_ml_model *model)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(params);
	RTE_SET_USED(model);

	return -EINVAL;
}

int
mvtvm_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(model);

	return -EINVAL;
}

int
mvtvm_ml_model_start(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(model);

	return -EINVAL;
}

int
mvtvm_ml_model_stop(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(model);

	return -EINVAL;
}
