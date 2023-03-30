/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_common.h>
#include <rte_launch.h>

#include "ml_common.h"
#include "test_inference_common.h"

static int
test_inference_interleave_driver(struct ml_test *test, struct ml_options *opt)
{
	struct test_inference *t;
	uint16_t fid = 0;
	int ret = 0;

	t = ml_test_priv(test);

	ret = ml_inference_mldev_setup(test, opt);
	if (ret != 0)
		return ret;

	ret = ml_inference_mem_setup(test, opt);
	if (ret != 0)
		return ret;

	/* load and start all models */
	for (fid = 0; fid < opt->nb_filelist; fid++) {
		ret = ml_model_load(test, opt, &t->model[fid], fid);
		if (ret != 0)
			goto error;

		ret = ml_model_start(test, opt, &t->model[fid], fid);
		if (ret != 0)
			goto error;

		ret = ml_inference_iomem_setup(test, opt, fid);
		if (ret != 0)
			goto error;
	}

	/* launch inference requests */
	ret = ml_inference_launch_cores(test, opt, 0, opt->nb_filelist - 1);
	if (ret != 0) {
		ml_err("failed to launch cores");
		goto error;
	}

	rte_eal_mp_wait_lcore();

	/* stop and unload all models */
	for (fid = 0; fid < opt->nb_filelist; fid++) {
		ret = ml_inference_result(test, opt, fid);
		if (ret != ML_TEST_SUCCESS)
			goto error;

		ml_inference_iomem_destroy(test, opt, fid);
	}

	ml_inference_stats_get(test, opt);

	for (fid = 0; fid < opt->nb_filelist; fid++) {
		ret = ml_model_stop(test, opt, &t->model[fid], fid);
		if (ret != 0)
			goto error;

		ret = ml_model_unload(test, opt, &t->model[fid], fid);
		if (ret != 0)
			goto error;
	}

	ml_inference_mem_destroy(test, opt);

	ret = ml_inference_mldev_destroy(test, opt);
	if (ret != 0)
		return ret;

	t->cmn.result = ML_TEST_SUCCESS;

	return 0;

error:
	ml_inference_mem_destroy(test, opt);
	for (fid = 0; fid < opt->nb_filelist; fid++) {
		ml_inference_iomem_destroy(test, opt, fid);
		ml_model_stop(test, opt, &t->model[fid], fid);
		ml_model_unload(test, opt, &t->model[fid], fid);
	}

	t->cmn.result = ML_TEST_FAILED;

	return ret;
}

static int
test_inference_interleave_result(struct ml_test *test, struct ml_options *opt)
{
	struct test_inference *t;

	RTE_SET_USED(opt);

	t = ml_test_priv(test);

	return t->cmn.result;
}

static const struct ml_test_ops inference_interleave = {
	.cap_check = test_inference_cap_check,
	.opt_check = test_inference_opt_check,
	.opt_dump = test_inference_opt_dump,
	.test_setup = test_inference_setup,
	.test_destroy = test_inference_destroy,
	.test_driver = test_inference_interleave_driver,
	.test_result = test_inference_interleave_result,
};

ML_TEST_REGISTER(inference_interleave);
