/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_kvargs.h>
#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <bus_vdev_driver.h>

#include <roc_api.h>

#include "cnxk_ml_dev.h"

#define MVTVM_ML_DEV_MAX_QPS	      "max_qps"
#define MVTVM_ML_DEV_CACHE_MODEL_DATA "cache_model_data"

#define MVTVM_ML_DEV_MAX_QPS_DEFAULT	      32
#define CN10K_ML_DEV_CACHE_MODEL_DATA_DEFAULT 1

static const char *const valid_args[] = {MVTVM_ML_DEV_MAX_QPS, MVTVM_ML_DEV_CACHE_MODEL_DATA, NULL};

static int
parse_integer_arg(const char *key __rte_unused, const char *value, void *extra_args)
{
	int *i = (int *)extra_args;

	*i = atoi(value);
	if (*i < 0) {
		plt_err("Argument has to be positive.");
		return -EINVAL;
	}

	return 0;
}

static int
parse_uint_arg(const char *key __rte_unused, const char *value, void *extra_args)
{
	int i;
	char *end;
	errno = 0;

	i = strtol(value, &end, 10);
	if (*end != 0 || errno != 0 || i < 0)
		return -EINVAL;

	*((uint32_t *)extra_args) = i;

	return 0;
}

static int
mvtvm_mldev_parse_devargs(const char *args, struct mvtvm_ml_dev *mvtvm_mldev)
{
	bool cache_model_data_set = false;
	struct rte_kvargs *kvlist = NULL;
	bool max_qps_set = false;
	int ret = 0;

	if (args == NULL)
		goto check_args;

	kvlist = rte_kvargs_parse(args, valid_args);
	if (kvlist == NULL) {
		plt_err("Error parsing %s devargs\n", "MLDEV_NAME_MVTVM_PMD");
		return -EINVAL;
	}

	if (rte_kvargs_count(kvlist, MVTVM_ML_DEV_MAX_QPS) == 1) {
		ret = rte_kvargs_process(kvlist, MVTVM_ML_DEV_MAX_QPS, &parse_uint_arg,
					 &mvtvm_mldev->max_nb_qpairs);
		if (ret < 0) {
			plt_err("Error processing arguments, key = %s\n", MVTVM_ML_DEV_MAX_QPS);
			ret = -EINVAL;
			goto exit;
		}
		max_qps_set = true;
	}

	if (rte_kvargs_count(kvlist, MVTVM_ML_DEV_CACHE_MODEL_DATA) == 1) {
		ret = rte_kvargs_process(kvlist, MVTVM_ML_DEV_CACHE_MODEL_DATA, &parse_integer_arg,
					 &mvtvm_mldev->cache_model_data);
		if (ret < 0) {
			plt_err("Error processing arguments, key = %s\n",
				MVTVM_ML_DEV_CACHE_MODEL_DATA);
			ret = -EINVAL;
			goto exit;
		}
		cache_model_data_set = true;
	}

check_args:
	if (!max_qps_set)
		mvtvm_mldev->max_nb_qpairs = MVTVM_ML_DEV_MAX_QPS_DEFAULT;
	plt_ml_dbg("ML: %s = %u", MVTVM_ML_DEV_MAX_QPS, mvtvm_mldev->max_nb_qpairs);

	if (!cache_model_data_set) {
		mvtvm_mldev->cache_model_data = CN10K_ML_DEV_CACHE_MODEL_DATA_DEFAULT;
	} else {
		if ((mvtvm_mldev->cache_model_data < 0) || (mvtvm_mldev->cache_model_data > 1)) {
			plt_err("Invalid argument, %s = %d\n", MVTVM_ML_DEV_CACHE_MODEL_DATA,
				mvtvm_mldev->cache_model_data);
			ret = -EINVAL;
			goto exit;
		}
	}
	plt_ml_dbg("ML: %s = %d", MVTVM_ML_DEV_CACHE_MODEL_DATA, mvtvm_mldev->cache_model_data);

exit:
	rte_kvargs_free(kvlist);

	return ret;
}

static int
mvtvm_ml_vdev_probe(struct rte_vdev_device *vdev)
{
	struct rte_ml_dev_pmd_init_params init_params;
	struct mvtvm_ml_dev *mvtvm_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	struct rte_ml_dev *dev;
	const char *input_args;
	const char *name;
	int ret = 0;

	if (cnxk_ml_dev_initialized == 1) {
		plt_err("ML CNXK device already initialized!");
		plt_err("Not creating ml_mvtvm vdev!");
		return 0;
	}

	init_params = (struct rte_ml_dev_pmd_init_params){
		.socket_id = rte_socket_id(), .private_data_size = sizeof(struct cnxk_ml_dev)};

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);

	dev = rte_ml_dev_pmd_create(name, &vdev->device, &init_params);
	if (dev == NULL) {
		ret = -EFAULT;
		goto error_exit;
	}

	cnxk_mldev = dev->data->dev_private;
	cnxk_mldev->mldev = dev;
	mvtvm_mldev = &cnxk_mldev->mvtvm_mldev;
	mvtvm_mldev->vdev = vdev;

	ret = mvtvm_mldev_parse_devargs(input_args, mvtvm_mldev);
	if (ret < 0)
		goto error_exit;

	dev->dev_ops = &cnxk_ml_ops;
	dev->enqueue_burst = NULL;
	dev->dequeue_burst = NULL;
	dev->op_error_get = NULL;

	cnxk_ml_dev_initialized = 1;
	cnxk_mldev->type = CNXK_ML_DEV_TYPE_VDEV;

	return 0;

error_exit:
	plt_err("Could not create device: ml_mvtvm");

	return ret;
}

static int
mvtvm_ml_vdev_remove(struct rte_vdev_device *vdev)
{
	struct rte_ml_dev *dev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	dev = rte_ml_dev_pmd_get_named_dev(name);
	if (dev == NULL)
		return -ENODEV;

	return rte_ml_dev_pmd_destroy(dev);
}

static struct rte_vdev_driver mvtvm_mldev_pmd = {.probe = mvtvm_ml_vdev_probe,
						 .remove = mvtvm_ml_vdev_remove};

RTE_PMD_REGISTER_VDEV(MLDEV_NAME_MVTVM_PMD, mvtvm_mldev_pmd);

RTE_PMD_REGISTER_PARAM_STRING(MLDEV_NAME_MVTVM_PMD,
			      MVTVM_ML_DEV_MAX_QPS "=<int>" MVTVM_ML_DEV_CACHE_MODEL_DATA "=<0|1>");
