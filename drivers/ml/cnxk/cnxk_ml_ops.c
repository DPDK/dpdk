/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_io.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_ops.h"

int
cnxk_ml_dev_info_get(struct rte_ml_dev *dev, struct rte_ml_dev_info *dev_info)
{
	struct cnxk_ml_dev *cnxk_mldev;

	if (dev == NULL || dev_info == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	memset(dev_info, 0, sizeof(struct rte_ml_dev_info));
	dev_info->driver_name = dev->device->driver->name;
	dev_info->max_models = ML_CNXK_MAX_MODELS;

	return cn10k_ml_dev_info_get(cnxk_mldev, dev_info);
}

static int
cnxk_ml_dev_configure(struct rte_ml_dev *dev, const struct rte_ml_dev_config *conf)
{
	struct rte_ml_dev_info dev_info;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_qp *qp;
	uint16_t model_id;
	uint32_t mz_size;
	uint16_t qp_id;
	int ret;

	if (dev == NULL)
		return -EINVAL;

	/* Get CNXK device handle */
	cnxk_mldev = dev->data->dev_private;

	cnxk_ml_dev_info_get(dev, &dev_info);
	if (conf->nb_models > dev_info.max_models) {
		plt_err("Invalid device config, nb_models > %u\n", dev_info.max_models);
		return -EINVAL;
	}

	if (conf->nb_queue_pairs > dev_info.max_queue_pairs) {
		plt_err("Invalid device config, nb_queue_pairs > %u\n", dev_info.max_queue_pairs);
		return -EINVAL;
	}

	if (cnxk_mldev->state == ML_CNXK_DEV_STATE_PROBED) {
		plt_ml_dbg("Configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);

		/* Load firmware */
		ret = cn10k_ml_fw_load(cnxk_mldev);
		if (ret != 0)
			return ret;
	} else if (cnxk_mldev->state == ML_CNXK_DEV_STATE_CONFIGURED) {
		plt_ml_dbg("Re-configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);
	} else if (cnxk_mldev->state == ML_CNXK_DEV_STATE_STARTED) {
		plt_err("Device can't be reconfigured in started state\n");
		return -ENOTSUP;
	} else if (cnxk_mldev->state == ML_CNXK_DEV_STATE_CLOSED) {
		plt_err("Device can't be reconfigured after close\n");
		return -ENOTSUP;
	}

	/* Configure queue-pairs */
	if (dev->data->queue_pairs == NULL) {
		mz_size = sizeof(dev->data->queue_pairs[0]) * conf->nb_queue_pairs;
		dev->data->queue_pairs =
			rte_zmalloc("cnxk_mldev_queue_pairs", mz_size, RTE_CACHE_LINE_SIZE);
		if (dev->data->queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			plt_err("Failed to get memory for queue_pairs, nb_queue_pairs %u",
				conf->nb_queue_pairs);
			return -ENOMEM;
		}
	} else { /* Re-configure */
		void **queue_pairs;

		/* Release all queue pairs as ML spec doesn't support queue_pair_destroy. */
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
			qp = dev->data->queue_pairs[qp_id];
			if (qp != NULL) {
				ret = cn10k_ml_dev_queue_pair_release(dev, qp_id);
				if (ret < 0)
					return ret;
			}
		}

		queue_pairs = dev->data->queue_pairs;
		queue_pairs =
			rte_realloc(queue_pairs, sizeof(queue_pairs[0]) * conf->nb_queue_pairs,
				    RTE_CACHE_LINE_SIZE);
		if (queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			plt_err("Failed to realloc queue_pairs, nb_queue_pairs = %u",
				conf->nb_queue_pairs);
			ret = -ENOMEM;
			goto error;
		}

		memset(queue_pairs, 0, sizeof(queue_pairs[0]) * conf->nb_queue_pairs);
		dev->data->queue_pairs = queue_pairs;
	}
	dev->data->nb_queue_pairs = conf->nb_queue_pairs;

	/* Allocate ML models */
	if (dev->data->models == NULL) {
		mz_size = sizeof(dev->data->models[0]) * conf->nb_models;
		dev->data->models = rte_zmalloc("cnxk_mldev_models", mz_size, RTE_CACHE_LINE_SIZE);
		if (dev->data->models == NULL) {
			dev->data->nb_models = 0;
			plt_err("Failed to get memory for ml_models, nb_models %u",
				conf->nb_models);
			ret = -ENOMEM;
			goto error;
		}
	} else {
		/* Re-configure */
		void **models;

		/* Stop and unload all models */
		for (model_id = 0; model_id < dev->data->nb_models; model_id++) {
			model = dev->data->models[model_id];
			if (model != NULL) {
				if (model->state == ML_CNXK_MODEL_STATE_STARTED) {
					if (cn10k_ml_model_stop(dev, model_id) != 0)
						plt_err("Could not stop model %u", model_id);
				}
				if (model->state == ML_CNXK_MODEL_STATE_LOADED) {
					if (cn10k_ml_model_unload(dev, model_id) != 0)
						plt_err("Could not unload model %u", model_id);
				}
				dev->data->models[model_id] = NULL;
			}
		}

		models = dev->data->models;
		models = rte_realloc(models, sizeof(models[0]) * conf->nb_models,
				     RTE_CACHE_LINE_SIZE);
		if (models == NULL) {
			dev->data->nb_models = 0;
			plt_err("Failed to realloc ml_models, nb_models = %u", conf->nb_models);
			ret = -ENOMEM;
			goto error;
		}
		memset(models, 0, sizeof(models[0]) * conf->nb_models);
		dev->data->models = models;
	}
	dev->data->nb_models = conf->nb_models;

	ret = cn10k_ml_dev_configure(cnxk_mldev, conf);
	if (ret != 0) {
		plt_err("Failed to configure CN10K ML Device");
		goto error;
	}

	/* Set device capabilities */
	cnxk_mldev->max_nb_layers =
		cnxk_mldev->cn10k_mldev.fw.req->cn10k_req.jd.fw_load.cap.s.max_models;

	cnxk_mldev->nb_models_loaded = 0;
	cnxk_mldev->nb_models_started = 0;
	cnxk_mldev->nb_models_stopped = 0;
	cnxk_mldev->nb_models_unloaded = 0;
	cnxk_mldev->state = ML_CNXK_DEV_STATE_CONFIGURED;

	return 0;

error:
	rte_free(dev->data->queue_pairs);
	rte_free(dev->data->models);

	return ret;
}

static int
cnxk_ml_dev_close(struct rte_ml_dev *dev)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_qp *qp;
	uint16_t model_id;
	uint16_t qp_id;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	if (cn10k_ml_dev_close(cnxk_mldev) != 0)
		plt_err("Failed to close CN10K ML Device");

	/* Stop and unload all models */
	for (model_id = 0; model_id < dev->data->nb_models; model_id++) {
		model = dev->data->models[model_id];
		if (model != NULL) {
			if (model->state == ML_CNXK_MODEL_STATE_STARTED) {
				if (cn10k_ml_model_stop(dev, model_id) != 0)
					plt_err("Could not stop model %u", model_id);
			}
			if (model->state == ML_CNXK_MODEL_STATE_LOADED) {
				if (cn10k_ml_model_unload(dev, model_id) != 0)
					plt_err("Could not unload model %u", model_id);
			}
			dev->data->models[model_id] = NULL;
		}
	}

	rte_free(dev->data->models);

	/* Destroy all queue pairs */
	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		qp = dev->data->queue_pairs[qp_id];
		if (qp != NULL) {
			if (cnxk_ml_qp_destroy(dev, qp) != 0)
				plt_err("Could not destroy queue pair %u", qp_id);
			dev->data->queue_pairs[qp_id] = NULL;
		}
	}

	rte_free(dev->data->queue_pairs);

	cnxk_mldev->state = ML_CNXK_DEV_STATE_CLOSED;

	/* Remove PCI device */
	return rte_dev_remove(dev->device);
}

static int
cnxk_ml_dev_start(struct rte_ml_dev *dev)
{
	struct cnxk_ml_dev *cnxk_mldev;
	int ret;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	ret = cn10k_ml_dev_start(cnxk_mldev);
	if (ret != 0) {
		plt_err("Failed to start CN10K ML Device");
		return ret;
	}

	cnxk_mldev->state = ML_CNXK_DEV_STATE_STARTED;

	return 0;
}

static int
cnxk_ml_dev_stop(struct rte_ml_dev *dev)
{
	struct cnxk_ml_dev *cnxk_mldev;
	int ret;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	ret = cn10k_ml_dev_stop(cnxk_mldev);
	if (ret != 0) {
		plt_err("Failed to stop CN10K ML Device");
		return ret;
	}

	cnxk_mldev->state = ML_CNXK_DEV_STATE_CONFIGURED;

	return 0;
}

struct rte_ml_dev_ops cnxk_ml_ops = {
	/* Device control ops */
	.dev_info_get = cnxk_ml_dev_info_get,
	.dev_configure = cnxk_ml_dev_configure,
	.dev_close = cnxk_ml_dev_close,
	.dev_start = cnxk_ml_dev_start,
	.dev_stop = cnxk_ml_dev_stop,
	.dev_dump = cn10k_ml_dev_dump,
	.dev_selftest = cn10k_ml_dev_selftest,

	/* Queue-pair handling ops */
	.dev_queue_pair_setup = cn10k_ml_dev_queue_pair_setup,
	.dev_queue_pair_release = cn10k_ml_dev_queue_pair_release,

	/* Stats ops */
	.dev_stats_get = cn10k_ml_dev_stats_get,
	.dev_stats_reset = cn10k_ml_dev_stats_reset,
	.dev_xstats_names_get = cn10k_ml_dev_xstats_names_get,
	.dev_xstats_by_name_get = cn10k_ml_dev_xstats_by_name_get,
	.dev_xstats_get = cn10k_ml_dev_xstats_get,
	.dev_xstats_reset = cn10k_ml_dev_xstats_reset,

	/* Model ops */
	.model_load = cn10k_ml_model_load,
	.model_unload = cn10k_ml_model_unload,
	.model_start = cn10k_ml_model_start,
	.model_stop = cn10k_ml_model_stop,
	.model_info_get = cn10k_ml_model_info_get,
	.model_params_update = cn10k_ml_model_params_update,

	/* I/O ops */
	.io_quantize = cn10k_ml_io_quantize,
	.io_dequantize = cn10k_ml_io_dequantize,
};
