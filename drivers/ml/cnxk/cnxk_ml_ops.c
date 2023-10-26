/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_io.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_ops.h"

/* ML model macros */
#define CNXK_ML_MODEL_MEMZONE_NAME "ml_cnxk_model_mz"

static void
qp_memzone_name_get(char *name, int size, int dev_id, int qp_id)
{
	snprintf(name, size, "cnxk_ml_qp_mem_%u:%u", dev_id, qp_id);
}

static int
cnxk_ml_qp_destroy(const struct rte_ml_dev *dev, struct cnxk_ml_qp *qp)
{
	const struct rte_memzone *qp_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	int ret;

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id, qp->id);
	qp_mem = rte_memzone_lookup(name);
	ret = rte_memzone_free(qp_mem);
	if (ret)
		return ret;

	rte_free(qp);

	return 0;
}

static int
cnxk_ml_dev_queue_pair_release(struct rte_ml_dev *dev, uint16_t queue_pair_id)
{
	struct cnxk_ml_qp *qp;
	int ret;

	qp = dev->data->queue_pairs[queue_pair_id];
	if (qp == NULL)
		return -EINVAL;

	ret = cnxk_ml_qp_destroy(dev, qp);
	if (ret) {
		plt_err("Could not destroy queue pair %u", queue_pair_id);
		return ret;
	}

	dev->data->queue_pairs[queue_pair_id] = NULL;

	return 0;
}

static struct cnxk_ml_qp *
cnxk_ml_qp_create(const struct rte_ml_dev *dev, uint16_t qp_id, uint32_t nb_desc, int socket_id)
{
	const struct rte_memzone *qp_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_qp *qp;
	uint32_t len;
	uint8_t *va;

	cnxk_mldev = dev->data->dev_private;

	/* Allocate queue pair */
	qp = rte_zmalloc_socket("cnxk_ml_pmd_queue_pair", sizeof(struct cnxk_ml_qp), ROC_ALIGN,
				socket_id);
	if (qp == NULL) {
		plt_err("Could not allocate queue pair");
		return NULL;
	}

	/* For request queue */
	len = nb_desc * sizeof(struct cnxk_ml_req);
	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id, qp_id);
	qp_mem = rte_memzone_reserve_aligned(
		name, len, socket_id, RTE_MEMZONE_SIZE_HINT_ONLY | RTE_MEMZONE_256MB, ROC_ALIGN);
	if (qp_mem == NULL) {
		plt_err("Could not reserve memzone: %s", name);
		goto qp_free;
	}

	va = qp_mem->addr;
	memset(va, 0, len);

	/* Initialize Request queue */
	qp->id = qp_id;
	qp->queue.reqs = (struct cnxk_ml_req *)va;
	qp->queue.head = 0;
	qp->queue.tail = 0;
	qp->queue.wait_cycles = ML_CNXK_CMD_TIMEOUT * plt_tsc_hz();
	qp->nb_desc = nb_desc;
	qp->stats.enqueued_count = 0;
	qp->stats.dequeued_count = 0;
	qp->stats.enqueue_err_count = 0;
	qp->stats.dequeue_err_count = 0;

	cn10k_ml_qp_initialize(cnxk_mldev, qp);

	return qp;

qp_free:
	rte_free(qp);

	return NULL;
}

static int
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
	uint64_t i;
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
				ret = cnxk_ml_dev_queue_pair_release(dev, qp_id);
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
					if (cnxk_ml_model_stop(dev, model_id) != 0)
						plt_err("Could not stop model %u", model_id);
				}
				if (model->state == ML_CNXK_MODEL_STATE_LOADED) {
					if (cnxk_ml_model_unload(dev, model_id) != 0)
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

	/* Allocate and initialize index_map */
	if (cnxk_mldev->index_map == NULL) {
		cnxk_mldev->index_map =
			rte_zmalloc("cnxk_ml_index_map",
				    sizeof(struct cnxk_ml_index_map) * cnxk_mldev->max_nb_layers,
				    RTE_CACHE_LINE_SIZE);
		if (cnxk_mldev->index_map == NULL) {
			plt_err("Failed to get memory for index_map, nb_layers %" PRIu64,
				cnxk_mldev->max_nb_layers);
			ret = -ENOMEM;
			goto error;
		}
	}

	for (i = 0; i < cnxk_mldev->max_nb_layers; i++)
		cnxk_mldev->index_map[i].active = false;

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

	if (cnxk_mldev->index_map)
		rte_free(cnxk_mldev->index_map);

	/* Stop and unload all models */
	for (model_id = 0; model_id < dev->data->nb_models; model_id++) {
		model = dev->data->models[model_id];
		if (model != NULL) {
			if (model->state == ML_CNXK_MODEL_STATE_STARTED) {
				if (cnxk_ml_model_stop(dev, model_id) != 0)
					plt_err("Could not stop model %u", model_id);
			}
			if (model->state == ML_CNXK_MODEL_STATE_LOADED) {
				if (cnxk_ml_model_unload(dev, model_id) != 0)
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

static int
cnxk_ml_dev_queue_pair_setup(struct rte_ml_dev *dev, uint16_t queue_pair_id,
			     const struct rte_ml_dev_qp_conf *qp_conf, int socket_id)
{
	struct rte_ml_dev_info dev_info;
	struct cnxk_ml_qp *qp;
	uint32_t nb_desc;

	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		plt_err("Queue-pair id = %u (>= max queue pairs supported, %u)\n", queue_pair_id,
			dev->data->nb_queue_pairs);
		return -EINVAL;
	}

	if (dev->data->queue_pairs[queue_pair_id] != NULL)
		cnxk_ml_dev_queue_pair_release(dev, queue_pair_id);

	cnxk_ml_dev_info_get(dev, &dev_info);
	if (qp_conf->nb_desc == 0) {
		plt_err("Could not setup queue pair for %u descriptors", qp_conf->nb_desc);
		return -EINVAL;
	} else if (qp_conf->nb_desc > dev_info.max_desc) {
		plt_err("Could not setup queue pair for %u descriptors (> %u)", qp_conf->nb_desc,
			dev_info.max_desc);
		return -EINVAL;
	}
	plt_ml_dbg("Creating queue-pair, queue_pair_id = %u, nb_desc = %u", queue_pair_id,
		   qp_conf->nb_desc);

	/* As the number of usable descriptors is 1 less than the queue size being created, we
	 * increment the size of queue by 1 than the requested size, except when the requested size
	 * is equal to the maximum possible size.
	 */
	nb_desc =
		(qp_conf->nb_desc == dev_info.max_desc) ? dev_info.max_desc : qp_conf->nb_desc + 1;
	qp = cnxk_ml_qp_create(dev, queue_pair_id, nb_desc, socket_id);
	if (qp == NULL) {
		plt_err("Could not create queue pair %u", queue_pair_id);
		return -ENOMEM;
	}
	dev->data->queue_pairs[queue_pair_id] = qp;

	return 0;
}

static int
cnxk_ml_model_load(struct rte_ml_dev *dev, struct rte_ml_model_params *params, uint16_t *model_id)
{
	struct rte_ml_dev_info dev_info;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	uint64_t model_info_size;
	uint16_t lcl_model_id;
	uint64_t mz_size;
	bool found;
	int ret;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	/* Find model ID */
	found = false;
	for (lcl_model_id = 0; lcl_model_id < dev->data->nb_models; lcl_model_id++) {
		if (dev->data->models[lcl_model_id] == NULL) {
			found = true;
			break;
		}
	}

	if (!found) {
		plt_err("No slots available to load new model");
		return -ENOMEM;
	}

	/* Compute memzone size */
	cnxk_ml_dev_info_get(dev, &dev_info);
	mz_size = PLT_ALIGN_CEIL(sizeof(struct cnxk_ml_model), dev_info.align_size);
	model_info_size = sizeof(struct rte_ml_model_info) +
			  ML_CNXK_MODEL_MAX_INPUT_OUTPUT * sizeof(struct rte_ml_io_info) +
			  ML_CNXK_MODEL_MAX_INPUT_OUTPUT * sizeof(struct rte_ml_io_info);
	model_info_size = PLT_ALIGN_CEIL(model_info_size, dev_info.align_size);
	mz_size += model_info_size;

	/* Allocate memzone for model object */
	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", CNXK_ML_MODEL_MEMZONE_NAME, lcl_model_id);
	mz = plt_memzone_reserve_aligned(str, mz_size, 0, dev_info.align_size);
	if (!mz) {
		plt_err("Failed to allocate memory for cnxk_ml_model: %s", str);
		return -ENOMEM;
	}

	model = mz->addr;
	model->cnxk_mldev = cnxk_mldev;
	model->model_id = lcl_model_id;
	model->info = PLT_PTR_ADD(
		model, PLT_ALIGN_CEIL(sizeof(struct cnxk_ml_model), dev_info.align_size));
	dev->data->models[lcl_model_id] = model;

	ret = cn10k_ml_model_load(cnxk_mldev, params, model);
	if (ret != 0)
		goto error;

	plt_spinlock_init(&model->lock);
	model->state = ML_CNXK_MODEL_STATE_LOADED;
	cnxk_mldev->nb_models_loaded++;

	*model_id = lcl_model_id;

	return 0;

error:
	rte_memzone_free(mz);

	return ret;
}

int
cnxk_ml_model_unload(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;

	char str[RTE_MEMZONE_NAMESIZE];
	int ret;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	model = dev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	if (model->state != ML_CNXK_MODEL_STATE_LOADED) {
		plt_err("Cannot unload. Model in use.");
		return -EBUSY;
	}

	ret = cn10k_ml_model_unload(cnxk_mldev, model);
	if (ret != 0)
		return ret;

	dev->data->models[model_id] = NULL;
	cnxk_mldev->nb_models_unloaded++;

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", CNXK_ML_MODEL_MEMZONE_NAME, model_id);
	return plt_memzone_free(plt_memzone_lookup(str));
}

static int
cnxk_ml_model_start(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	model = dev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	return cn10k_ml_model_start(cnxk_mldev, model);
}

int
cnxk_ml_model_stop(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	model = dev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	return cn10k_ml_model_stop(cnxk_mldev, model);
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
	.dev_queue_pair_setup = cnxk_ml_dev_queue_pair_setup,
	.dev_queue_pair_release = cnxk_ml_dev_queue_pair_release,

	/* Stats ops */
	.dev_stats_get = cn10k_ml_dev_stats_get,
	.dev_stats_reset = cn10k_ml_dev_stats_reset,
	.dev_xstats_names_get = cn10k_ml_dev_xstats_names_get,
	.dev_xstats_by_name_get = cn10k_ml_dev_xstats_by_name_get,
	.dev_xstats_get = cn10k_ml_dev_xstats_get,
	.dev_xstats_reset = cn10k_ml_dev_xstats_reset,

	/* Model ops */
	.model_load = cnxk_ml_model_load,
	.model_unload = cnxk_ml_model_unload,
	.model_start = cnxk_ml_model_start,
	.model_stop = cnxk_ml_model_stop,
	.model_info_get = cn10k_ml_model_info_get,
	.model_params_update = cn10k_ml_model_params_update,

	/* I/O ops */
	.io_quantize = cn10k_ml_io_quantize,
	.io_dequantize = cn10k_ml_io_dequantize,
};
