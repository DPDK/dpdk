/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_ops.h"

/* ML model macros */
#define MVTVM_ML_MODEL_MEMZONE_NAME "ml_mvtvm_model_mz"

int
mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf)
{
	int ret;

	RTE_SET_USED(conf);

	/* Configure TVMDP library */
	ret = tvmdp_configure(cnxk_mldev->mldev->data->nb_models, rte_get_tsc_cycles);
	if (ret != 0)
		plt_err("TVMDP configuration failed, error = %d\n", ret);

	return ret;
}

int
mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev)
{
	int ret;

	RTE_SET_USED(cnxk_mldev);

	/* Close TVMDP library configuration */
	ret = tvmdp_close();
	if (ret != 0)
		plt_err("TVMDP close failed, error = %d\n", ret);

	return ret;
}

int
mvtvm_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
		    struct cnxk_ml_model *model)
{
	struct mvtvm_ml_model_object object[ML_MVTVM_MODEL_OBJECT_MAX];
	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	size_t model_object_size = 0;
	uint64_t mz_size = 0;
	int ret;

	RTE_SET_USED(cnxk_mldev);

	ret = mvtvm_ml_model_blob_parse(params, object);
	if (ret != 0)
		return ret;

	model_object_size = RTE_ALIGN_CEIL(object[0].size, RTE_CACHE_LINE_MIN_SIZE) +
			    RTE_ALIGN_CEIL(object[1].size, RTE_CACHE_LINE_MIN_SIZE) +
			    RTE_ALIGN_CEIL(object[2].size, RTE_CACHE_LINE_MIN_SIZE);
	mz_size += model_object_size;

	/* Allocate memzone for model object */
	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", MVTVM_ML_MODEL_MEMZONE_NAME, model->model_id);
	mz = plt_memzone_reserve_aligned(str, mz_size, 0, ML_CN10K_ALIGN_SIZE);
	if (!mz) {
		plt_err("plt_memzone_reserve failed : %s", str);
		return -ENOMEM;
	}

	/* Copy mod.so */
	model->mvtvm.object.so.addr = mz->addr;
	model->mvtvm.object.so.size = object[0].size;
	rte_memcpy(model->mvtvm.object.so.name, object[0].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.so.addr, object[0].buffer, object[0].size);
	rte_free(object[0].buffer);

	/* Copy mod.json */
	model->mvtvm.object.json.addr =
		RTE_PTR_ADD(model->mvtvm.object.so.addr,
			    RTE_ALIGN_CEIL(model->mvtvm.object.so.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.object.json.size = object[1].size;
	rte_memcpy(model->mvtvm.object.json.name, object[1].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.json.addr, object[1].buffer, object[1].size);
	rte_free(object[1].buffer);

	/* Copy mod.params */
	model->mvtvm.object.params.addr =
		RTE_PTR_ADD(model->mvtvm.object.json.addr,
			    RTE_ALIGN_CEIL(model->mvtvm.object.json.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.object.params.size = object[2].size;
	rte_memcpy(model->mvtvm.object.params.name, object[2].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.params.addr, object[2].buffer, object[2].size);
	rte_free(object[2].buffer);

	return 0;
}
