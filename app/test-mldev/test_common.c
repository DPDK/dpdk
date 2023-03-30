/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_mldev.h>

#include "ml_common.h"
#include "test_common.h"

bool
ml_test_cap_check(struct ml_options *opt)
{
	struct rte_ml_dev_info dev_info;

	rte_ml_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_models == 0) {
		ml_err("Not enough mldev models supported = %u", dev_info.max_models);
		return false;
	}

	return true;
}

int
ml_test_opt_check(struct ml_options *opt)
{
	uint16_t dev_count;
	int socket_id;

	RTE_SET_USED(opt);

	dev_count = rte_ml_dev_count();
	if (dev_count == 0) {
		ml_err("No ML devices found");
		return -ENODEV;
	}

	if (opt->dev_id >= dev_count) {
		ml_err("Invalid option dev_id = %d", opt->dev_id);
		return -EINVAL;
	}

	socket_id = rte_ml_dev_socket_id(opt->dev_id);
	if (!((opt->socket_id != SOCKET_ID_ANY) || (opt->socket_id != socket_id))) {
		ml_err("Invalid option, socket_id = %d\n", opt->socket_id);
		return -EINVAL;
	}

	return 0;
}

void
ml_test_opt_dump(struct ml_options *opt)
{
	ml_options_dump(opt);
}

int
ml_test_device_configure(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	struct rte_ml_dev_config dev_config;
	int ret;

	ret = rte_ml_dev_info_get(opt->dev_id, &t->dev_info);
	if (ret != 0) {
		ml_err("Failed to get mldev info, dev_id = %d\n", opt->dev_id);
		return ret;
	}

	/* configure device */
	dev_config.socket_id = opt->socket_id;
	dev_config.nb_models = t->dev_info.max_models;
	dev_config.nb_queue_pairs = opt->queue_pairs;
	ret = rte_ml_dev_configure(opt->dev_id, &dev_config);
	if (ret != 0) {
		ml_err("Failed to configure ml device, dev_id = %d\n", opt->dev_id);
		return ret;
	}

	return 0;
}

int
ml_test_device_close(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	int ret = 0;

	RTE_SET_USED(t);

	/* close device */
	ret = rte_ml_dev_close(opt->dev_id);
	if (ret != 0)
		ml_err("Failed to close ML device, dev_id = %d\n", opt->dev_id);

	return ret;
}

int
ml_test_device_start(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	/* start device */
	ret = rte_ml_dev_start(opt->dev_id);
	if (ret != 0) {
		ml_err("Failed to start ml device, dev_id = %d\n", opt->dev_id);
		return ret;
	}

	return 0;
}

int
ml_test_device_stop(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	int ret = 0;

	RTE_SET_USED(t);

	/* stop device */
	ret = rte_ml_dev_stop(opt->dev_id);
	if (ret != 0)
		ml_err("Failed to stop ML device, dev_id = %d\n", opt->dev_id);

	return ret;
}
