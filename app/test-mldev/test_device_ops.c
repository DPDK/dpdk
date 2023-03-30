/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mldev.h>

#include "test_device_ops.h"

static bool
test_device_cap_check(struct ml_options *opt)
{
	if (!ml_test_cap_check(opt))
		return false;

	return true;
}

static int
test_device_opt_check(struct ml_options *opt)
{
	int ret;

	/* check common opts */
	ret = ml_test_opt_check(opt);
	if (ret != 0)
		return ret;

	return 0;
}

static void
test_device_opt_dump(struct ml_options *opt)
{
	/* dump common opts */
	ml_test_opt_dump(opt);
}

static int
test_device_setup(struct ml_test *test, struct ml_options *opt)
{
	struct test_device *t;
	void *test_device;
	int ret = 0;

	/* allocate for test structure */
	test_device = rte_zmalloc_socket(test->name, sizeof(struct test_device),
					 RTE_CACHE_LINE_SIZE, opt->socket_id);
	if (test_device == NULL) {
		ml_err("failed to allocate memory for test_model");
		ret = -ENOMEM;
		goto error;
	}
	test->test_priv = test_device;
	t = ml_test_priv(test);

	t->cmn.result = ML_TEST_FAILED;
	t->cmn.opt = opt;

	/* get device info */
	ret = rte_ml_dev_info_get(opt->dev_id, &t->cmn.dev_info);
	if (ret < 0) {
		ml_err("failed to get device info");
		goto error;
	}

	return 0;

error:
	rte_free(test_device);

	return ret;
}

static void
test_device_destroy(struct ml_test *test, struct ml_options *opt)
{
	struct test_device *t;

	RTE_SET_USED(opt);

	t = ml_test_priv(test);
	rte_free(t);
}

static int
test_device_reconfigure(struct ml_test *test, struct ml_options *opt)
{
	struct rte_ml_dev_config dev_config;
	struct rte_ml_dev_qp_conf qp_conf;
	struct test_device *t;
	uint16_t qp_id = 0;
	int ret = 0;

	t = ml_test_priv(test);

	/* configure with default options */
	ret = ml_test_device_configure(test, opt);
	if (ret != 0)
		return ret;

	/* setup one queue pair with nb_desc = 1 */
	qp_conf.nb_desc = 1;
	qp_conf.cb = NULL;

	ret = rte_ml_dev_queue_pair_setup(opt->dev_id, qp_id, &qp_conf, opt->socket_id);
	if (ret != 0) {
		ml_err("Failed to setup ML device queue-pair, dev_id = %d, qp_id = %u\n",
		       opt->dev_id, qp_id);
		goto error;
	}

	/* start device */
	ret = ml_test_device_start(test, opt);
	if (ret != 0)
		goto error;

	/* stop device */
	ret = ml_test_device_stop(test, opt);
	if (ret != 0) {
		ml_err("Failed to stop device");
		goto error;
	}

	/* reconfigure device based on dev_info */
	dev_config.socket_id = opt->socket_id;
	dev_config.nb_models = t->cmn.dev_info.max_models;
	dev_config.nb_queue_pairs = t->cmn.dev_info.max_queue_pairs;
	ret = rte_ml_dev_configure(opt->dev_id, &dev_config);
	if (ret != 0) {
		ml_err("Failed to reconfigure ML device, dev_id = %d\n", opt->dev_id);
		return ret;
	}

	/* setup queue pairs */
	for (qp_id = 0; qp_id < t->cmn.dev_info.max_queue_pairs; qp_id++) {
		qp_conf.nb_desc = t->cmn.dev_info.max_desc;
		qp_conf.cb = NULL;

		ret = rte_ml_dev_queue_pair_setup(opt->dev_id, qp_id, &qp_conf, opt->socket_id);
		if (ret != 0) {
			ml_err("Failed to setup ML device queue-pair, dev_id = %d, qp_id = %u\n",
			       opt->dev_id, qp_id);
			goto error;
		}
	}

	/* start device */
	ret = ml_test_device_start(test, opt);
	if (ret != 0)
		goto error;

	/* stop device */
	ret = ml_test_device_stop(test, opt);
	if (ret != 0)
		goto error;

	/* close device */
	ret = ml_test_device_close(test, opt);
	if (ret != 0)
		return ret;

	return 0;

error:
	ml_test_device_close(test, opt);

	return ret;
}

static int
test_device_driver(struct ml_test *test, struct ml_options *opt)
{
	struct test_device *t;
	int ret = 0;

	t = ml_test_priv(test);

	/* sub-test: device reconfigure */
	ret = test_device_reconfigure(test, opt);
	if (ret != 0) {
		printf("\n");
		printf("Model Device Reconfigure Test: " CLRED "%s" CLNRM "\n", "Failed");
		goto error;
	} else {
		printf("\n");
		printf("Model Device Reconfigure Test: " CLYEL "%s" CLNRM "\n", "Passed");
	}

	printf("\n");

	t->cmn.result = ML_TEST_SUCCESS;

	return 0;

error:
	t->cmn.result = ML_TEST_FAILED;
	return -1;
}

static int
test_device_result(struct ml_test *test, struct ml_options *opt)
{
	struct test_device *t;

	RTE_SET_USED(opt);

	t = ml_test_priv(test);

	return t->cmn.result;
}

static const struct ml_test_ops device_ops = {
	.cap_check = test_device_cap_check,
	.opt_check = test_device_opt_check,
	.opt_dump = test_device_opt_dump,
	.test_setup = test_device_setup,
	.test_destroy = test_device_destroy,
	.test_driver = test_device_driver,
	.test_result = test_device_result,
};

ML_TEST_REGISTER(device_ops);
