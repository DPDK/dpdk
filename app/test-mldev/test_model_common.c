/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mldev.h>

#include "ml_common.h"
#include "test_model_common.h"

int
ml_model_load(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct rte_ml_model_params model_params;
	int ret;

	RTE_SET_USED(test);

	if (model->state == MODEL_LOADED)
		return 0;

	if (model->state != MODEL_INITIAL)
		return -EINVAL;

	/* read model binary */
	ret = ml_read_file(opt->filelist[fid].model, &model_params.size,
			   (char **)&model_params.addr);
	if (ret != 0)
		return ret;

	/* load model to device */
	ret = rte_ml_model_load(opt->dev_id, &model_params, &model->id);
	if (ret != 0) {
		ml_err("Failed to load model : %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		free(model_params.addr);
		return ret;
	}

	/* release buffer */
	free(model_params.addr);

	/* get model info */
	ret = rte_ml_model_info_get(opt->dev_id, model->id, &model->info);
	if (ret != 0) {
		ml_err("Failed to get model info : %s\n", opt->filelist[fid].model);
		return ret;
	}

	model->state = MODEL_LOADED;

	return 0;
}

int
ml_model_unload(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	if (model->state == MODEL_INITIAL)
		return 0;

	if (model->state != MODEL_LOADED)
		return -EINVAL;

	/* unload model */
	ret = rte_ml_model_unload(opt->dev_id, model->id);
	if (ret != 0) {
		ml_err("Failed to unload model: %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		return ret;
	}

	model->state = MODEL_INITIAL;

	return 0;
}

int
ml_model_start(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	if (model->state == MODEL_STARTED)
		return 0;

	if (model->state != MODEL_LOADED)
		return -EINVAL;

	/* start model */
	ret = rte_ml_model_start(opt->dev_id, model->id);
	if (ret != 0) {
		ml_err("Failed to start model : %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		return ret;
	}

	model->state = MODEL_STARTED;

	return 0;
}

int
ml_model_stop(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	if (model->state == MODEL_LOADED)
		return 0;

	if (model->state != MODEL_STARTED)
		return -EINVAL;

	/* stop model */
	ret = rte_ml_model_stop(opt->dev_id, model->id);
	if (ret != 0) {
		ml_err("Failed to stop model: %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		return ret;
	}

	model->state = MODEL_LOADED;

	return 0;
}
