/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mldev.h>

#include "test_model_ops.h"

static bool
test_model_ops_cap_check(struct ml_options *opt)
{
	if (!ml_test_cap_check(opt))
		return false;

	return true;
}

static int
test_model_ops_opt_check(struct ml_options *opt)
{
	uint32_t i;
	int ret;

	/* check common opts */
	ret = ml_test_opt_check(opt);
	if (ret != 0)
		return ret;

	/* check model file availability */
	for (i = 0; i < opt->nb_filelist; i++) {
		if (access(opt->filelist[i].model, F_OK) == -1) {
			ml_err("Model file not available: id = %u, file = %s", i,
			       opt->filelist[i].model);
			return -ENOENT;
		}
	}

	return 0;
}

static void
test_model_ops_opt_dump(struct ml_options *opt)
{
	uint32_t i;

	/* dump common opts */
	ml_test_opt_dump(opt);

	/* dump test specific opts */
	ml_dump_begin("models");
	for (i = 0; i < opt->nb_filelist; i++)
		ml_dump_list("model", i, opt->filelist[i].model);
	ml_dump_end;
}

static int
test_model_ops_setup(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;
	void *test_model_ops;
	int ret = 0;
	uint32_t i;

	/* allocate model ops test structure */
	test_model_ops = rte_zmalloc_socket(test->name, sizeof(struct test_model_ops),
					    RTE_CACHE_LINE_SIZE, opt->socket_id);
	if (test_model_ops == NULL) {
		ml_err("Failed to allocate memory for test_model");
		ret = -ENOMEM;
		goto error;
	}
	test->test_priv = test_model_ops;
	t = ml_test_priv(test);

	t->cmn.result = ML_TEST_FAILED;
	t->cmn.opt = opt;

	/* get device info */
	ret = rte_ml_dev_info_get(opt->dev_id, &t->cmn.dev_info);
	if (ret < 0) {
		ml_err("Failed to get device info");
		goto error;
	}

	/* set model initial state */
	for (i = 0; i < opt->nb_filelist; i++)
		t->model[i].state = MODEL_INITIAL;

	return 0;

error:
	rte_free(test_model_ops);

	return ret;
}

static void
test_model_ops_destroy(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;

	RTE_SET_USED(opt);

	t = ml_test_priv(test);
	rte_free(t);
}

static int
test_model_ops_mldev_setup(struct ml_test *test, struct ml_options *opt)
{
	int ret;

	ret = ml_test_device_configure(test, opt);
	if (ret != 0)
		return ret;

	ret = ml_test_device_start(test, opt);
	if (ret != 0)
		goto error;

	return 0;

error:
	ml_test_device_close(test, opt);

	return ret;
}

static int
test_model_ops_mldev_destroy(struct ml_test *test, struct ml_options *opt)
{
	int ret;

	ret = ml_test_device_stop(test, opt);
	if (ret != 0)
		goto error;

	ret = ml_test_device_close(test, opt);
	if (ret != 0)
		return ret;

	return 0;

error:
	ml_test_device_close(test, opt);

	return ret;
}

/* Sub-test A: (load -> start -> stop -> unload) x n */
static int
test_model_ops_subtest_a(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;
	int ret = 0;
	uint32_t i;

	t = ml_test_priv(test);

	/* load + start + stop + unload */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_load(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;

		ret = ml_model_start(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;

		ret = ml_model_stop(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;

		ret = ml_model_unload(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

error:
	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_stop(test, opt, &t->model[i], i);

	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_unload(test, opt, &t->model[i], i);

	return ret;
}

/* Sub-test B: load x n -> start x n -> stop x n -> unload x n */
static int
test_model_ops_subtest_b(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;
	int ret = 0;
	uint32_t i;

	t = ml_test_priv(test);

	/* load */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_load(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	/* start */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_start(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	/* stop */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_stop(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	/* unload */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_unload(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	return 0;

error:
	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_stop(test, opt, &t->model[i], i);

	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_unload(test, opt, &t->model[i], i);

	return ret;
}

/* Sub-test C: load x n + (start  + stop) x n + unload x n */
static int
test_model_ops_subtest_c(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;
	int ret = 0;
	uint32_t i;

	t = ml_test_priv(test);

	/* load */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_load(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	/* start + stop */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_start(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;

		ret = ml_model_stop(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	/* unload */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_unload(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	return 0;

error:
	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_stop(test, opt, &t->model[i], i);

	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_unload(test, opt, &t->model[i], i);

	return ret;
}

/* Sub-test D: (load + start) x n -> (stop + unload) x n */
static int
test_model_ops_subtest_d(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;
	int ret = 0;
	uint32_t i;

	t = ml_test_priv(test);

	/* load + start */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_load(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;

		ret = ml_model_start(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	/* stop + unload */
	for (i = 0; i < opt->nb_filelist; i++) {
		ret = ml_model_stop(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;

		ret = ml_model_unload(test, opt, &t->model[i], i);
		if (ret != 0)
			goto error;
	}

	return 0;

error:
	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_stop(test, opt, &t->model[i], i);

	for (i = 0; i < opt->nb_filelist; i++)
		ml_model_unload(test, opt, &t->model[i], i);

	return ret;
}

static int
test_model_ops_driver(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;
	int ret = 0;

	t = ml_test_priv(test);

	/* device setup */
	ret = test_model_ops_mldev_setup(test, opt);
	if (ret != 0)
		return ret;

	printf("\n");

	/* sub-test A */
	ret = test_model_ops_subtest_a(test, opt);
	if (ret != 0) {
		printf("Model Ops Sub-test A: " CLRED "%s" CLNRM "\n", "Failed");
		goto error;
	} else {
		printf("Model Ops Sub-test A: " CLYEL "%s" CLNRM "\n", "Passed");
	}

	/* sub-test B */
	ret = test_model_ops_subtest_b(test, opt);
	if (ret != 0) {
		printf("Model Ops Sub-test B: " CLRED "%s" CLNRM "\n", "Failed");
		goto error;
	} else {
		printf("Model Ops Sub-test B: " CLYEL "%s" CLNRM "\n", "Passed");
	}

	/* sub-test C */
	ret = test_model_ops_subtest_c(test, opt);
	if (ret != 0) {
		printf("Model Ops Sub-test C: " CLRED "%s" CLNRM "\n", "Failed");
		goto error;
	} else {
		printf("Model Ops Sub-test C: " CLYEL "%s" CLNRM "\n", "Passed");
	}

	/* sub-test D */
	ret = test_model_ops_subtest_d(test, opt);
	if (ret != 0) {
		printf("Model Ops Sub-test D: " CLRED "%s" CLNRM "\n", "Failed");
		goto error;
	} else {
		printf("Model Ops Sub-test D: " CLYEL "%s" CLNRM "\n", "Passed");
	}

	printf("\n");

	/* device destroy */
	ret = test_model_ops_mldev_destroy(test, opt);
	if (ret != 0)
		return ret;

	t->cmn.result = ML_TEST_SUCCESS;

	return 0;

error:
	test_model_ops_mldev_destroy(test, opt);

	t->cmn.result = ML_TEST_FAILED;

	return ret;
}

static int
test_model_ops_result(struct ml_test *test, struct ml_options *opt)
{
	struct test_model_ops *t;

	RTE_SET_USED(opt);

	t = ml_test_priv(test);

	return t->cmn.result;
}

static const struct ml_test_ops model_ops = {
	.cap_check = test_model_ops_cap_check,
	.opt_check = test_model_ops_opt_check,
	.opt_dump = test_model_ops_opt_dump,
	.test_setup = test_model_ops_setup,
	.test_destroy = test_model_ops_destroy,
	.test_driver = test_model_ops_driver,
	.test_result = test_model_ops_result,
};

ML_TEST_REGISTER(model_ops);
