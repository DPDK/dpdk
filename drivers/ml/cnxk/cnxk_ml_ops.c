/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <mldev_utils.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_io.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_ops.h"

/* ML model macros */
#define CNXK_ML_MODEL_MEMZONE_NAME "ml_cnxk_model_mz"

__rte_hot void
cnxk_ml_set_poll_ptr(struct cnxk_ml_req *req)
{
	plt_write64(ML_CNXK_POLL_JOB_START, req->status);
}

__rte_hot uint64_t
cnxk_ml_get_poll_ptr(struct cnxk_ml_req *req)
{
	return plt_read64(req->status);
}

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

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI)
		cn10k_ml_qp_initialize(cnxk_mldev, qp);

	return qp;

qp_free:
	rte_free(qp);

	return NULL;
}

static int
cnxk_ml_xstats_init(struct cnxk_ml_dev *cnxk_mldev)
{
	uint16_t nb_stats;
	uint16_t stat_id;
	uint16_t model;
	uint16_t layer;
	uint16_t i;

	/* Allocate memory for xstats entries. Don't allocate during reconfigure */
	nb_stats = RTE_DIM(device_xstats) +
		   RTE_DIM(layer_xstats) * ML_CNXK_MAX_MODELS * ML_CNXK_MODEL_MAX_LAYERS +
		   RTE_DIM(model_xstats) * ML_CNXK_MAX_MODELS;
	if (cnxk_mldev->xstats.entries == NULL)
		cnxk_mldev->xstats.entries = rte_zmalloc(
			"cnxk_ml_xstats", sizeof(struct cnxk_ml_xstats_entry) * nb_stats,
			PLT_CACHE_LINE_SIZE);

	if (cnxk_mldev->xstats.entries == NULL)
		return -ENOMEM;

	/* Initialize device xstats */
	stat_id = 0;
	for (i = 0; i < RTE_DIM(device_xstats); i++) {
		cnxk_mldev->xstats.entries[stat_id].map.id = stat_id;
		snprintf(cnxk_mldev->xstats.entries[stat_id].map.name,
			 sizeof(cnxk_mldev->xstats.entries[stat_id].map.name), "%s",
			 device_xstats[i].name);

		cnxk_mldev->xstats.entries[stat_id].mode = RTE_ML_DEV_XSTATS_DEVICE;
		cnxk_mldev->xstats.entries[stat_id].group = CNXK_ML_XSTATS_GROUP_DEVICE;
		cnxk_mldev->xstats.entries[stat_id].type = device_xstats[i].type;
		cnxk_mldev->xstats.entries[stat_id].fn_id = CNXK_ML_XSTATS_FN_DEVICE;
		cnxk_mldev->xstats.entries[stat_id].obj_idx = 0;
		cnxk_mldev->xstats.entries[stat_id].reset_allowed = device_xstats[i].reset_allowed;
		stat_id++;
	}
	cnxk_mldev->xstats.count_mode_device = stat_id;

	/* Initialize model xstats */
	for (model = 0; model < ML_CNXK_MAX_MODELS; model++) {
		cnxk_mldev->xstats.offset_for_model[model] = stat_id;

		for (i = 0; i < RTE_DIM(model_xstats); i++) {
			cnxk_mldev->xstats.entries[stat_id].map.id = stat_id;
			cnxk_mldev->xstats.entries[stat_id].mode = RTE_ML_DEV_XSTATS_MODEL;
			cnxk_mldev->xstats.entries[stat_id].group = CNXK_ML_XSTATS_GROUP_MODEL;
			cnxk_mldev->xstats.entries[stat_id].type = model_xstats[i].type;
			cnxk_mldev->xstats.entries[stat_id].fn_id = CNXK_ML_XSTATS_FN_MODEL;
			cnxk_mldev->xstats.entries[stat_id].obj_idx = model;
			cnxk_mldev->xstats.entries[stat_id].layer_id = -1;
			cnxk_mldev->xstats.entries[stat_id].reset_allowed =
				model_xstats[i].reset_allowed;

			/* Name of xstat is updated during model load */
			snprintf(cnxk_mldev->xstats.entries[stat_id].map.name,
				 sizeof(cnxk_mldev->xstats.entries[stat_id].map.name),
				 "Model-%u-%s", model, model_xstats[i].name);

			stat_id++;
		}

		for (layer = 0; layer < ML_CNXK_MODEL_MAX_LAYERS; layer++) {
			cnxk_mldev->xstats.offset_for_layer[model][layer] = stat_id;

			for (i = 0; i < RTE_DIM(layer_xstats); i++) {
				cnxk_mldev->xstats.entries[stat_id].map.id = stat_id;
				cnxk_mldev->xstats.entries[stat_id].mode = RTE_ML_DEV_XSTATS_MODEL;
				cnxk_mldev->xstats.entries[stat_id].group =
					CNXK_ML_XSTATS_GROUP_LAYER;
				cnxk_mldev->xstats.entries[stat_id].type = layer_xstats[i].type;
				cnxk_mldev->xstats.entries[stat_id].fn_id = CNXK_ML_XSTATS_FN_MODEL;
				cnxk_mldev->xstats.entries[stat_id].obj_idx = model;
				cnxk_mldev->xstats.entries[stat_id].layer_id = layer;
				cnxk_mldev->xstats.entries[stat_id].reset_allowed =
					layer_xstats[i].reset_allowed;

				/* Name of xstat is updated during model load */
				snprintf(cnxk_mldev->xstats.entries[stat_id].map.name,
					 sizeof(cnxk_mldev->xstats.entries[stat_id].map.name),
					 "Layer-%u-%u-%s", model, layer, layer_xstats[i].name);

				stat_id++;
			}

			cnxk_mldev->xstats.count_per_layer[model][layer] = RTE_DIM(layer_xstats);
		}

		cnxk_mldev->xstats.count_per_model[model] =
			RTE_DIM(layer_xstats) + ML_CNXK_MODEL_MAX_LAYERS * RTE_DIM(model_xstats);
	}

	cnxk_mldev->xstats.count_mode_model = stat_id - cnxk_mldev->xstats.count_mode_device;
	cnxk_mldev->xstats.count = stat_id;

	return 0;
}

void
cnxk_ml_xstats_model_name_update(struct cnxk_ml_dev *cnxk_mldev, uint16_t model_id)
{
	struct cnxk_ml_model *model;
	uint16_t rclk_freq;
	uint16_t sclk_freq;
	uint16_t stat_id;
	char suffix[8];
	uint16_t i;

	model = cnxk_mldev->mldev->data->models[model_id];
	stat_id = cnxk_mldev->xstats.offset_for_model[model_id];

	roc_clk_freq_get(&rclk_freq, &sclk_freq);
	if (sclk_freq == 0)
		rte_strscpy(suffix, "cycles", 7);
	else
		rte_strscpy(suffix, "ns", 3);

	/* Update xstat name based on layer name and sclk availability */
	for (i = 0; i < RTE_DIM(model_xstats); i++) {
		if (model->type == ML_CNXK_MODEL_TYPE_GLOW)
			cn10k_ml_xstat_model_name_set(cnxk_mldev, model, stat_id, i, suffix);
		else
			mvtvm_ml_model_xstat_name_set(cnxk_mldev, model, stat_id, i, suffix);

		stat_id++;
	}
}

static void
cnxk_ml_xstats_uninit(struct cnxk_ml_dev *cnxk_mldev)
{
	rte_free(cnxk_mldev->xstats.entries);
	cnxk_mldev->xstats.entries = NULL;

	cnxk_mldev->xstats.count = 0;
}

static uint64_t
cnxk_ml_dev_xstat_get(struct cnxk_ml_dev *cnxk_mldev, uint16_t obj_idx __rte_unused,
		      int32_t layer_id __rte_unused, enum cnxk_ml_xstats_type type)
{
	switch (type) {
	case nb_models_loaded:
		return cnxk_mldev->nb_models_loaded;
	case nb_models_unloaded:
		return cnxk_mldev->nb_models_unloaded;
	case nb_models_started:
		return cnxk_mldev->nb_models_started;
	case nb_models_stopped:
		return cnxk_mldev->nb_models_stopped;
	default:
		return -1;
	}

	return 0;
}

static uint64_t
cnxk_ml_model_xstat_get(struct cnxk_ml_dev *cnxk_mldev, uint16_t obj_idx, int32_t layer_id,
			enum cnxk_ml_xstats_type type)
{
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	uint16_t rclk_freq; /* MHz */
	uint16_t sclk_freq; /* MHz */
	uint64_t value = 0;

	model = cnxk_mldev->mldev->data->models[obj_idx];
	if (model == NULL)
		return 0;

	if (layer_id >= 0) {
		layer = &model->layer[layer_id];
		goto layer_xstats;
	} else {
		layer = NULL;
		goto model_xstats;
	}

layer_xstats:
	value = cn10k_ml_model_xstat_get(cnxk_mldev, layer, type);
	goto exit_xstats;

model_xstats:
	value = mvtvm_ml_model_xstat_get(cnxk_mldev, model, type);

exit_xstats:
	roc_clk_freq_get(&rclk_freq, &sclk_freq);
	if (sclk_freq != 0) /* return in ns */
		value = (value * 1000ULL) / sclk_freq;

	return value;
}

static int
cnxk_ml_device_xstats_reset(struct cnxk_ml_dev *cnxk_mldev, const uint16_t stat_ids[],
			    uint16_t nb_ids)
{
	struct cnxk_ml_xstats_entry *xs;
	uint16_t nb_stats;
	uint16_t stat_id;
	uint32_t i;

	if (stat_ids == NULL)
		nb_stats = cnxk_mldev->xstats.count_mode_device;
	else
		nb_stats = nb_ids;

	for (i = 0; i < nb_stats; i++) {
		if (stat_ids == NULL)
			stat_id = i;
		else
			stat_id = stat_ids[i];

		if (stat_id >= cnxk_mldev->xstats.count_mode_device)
			return -EINVAL;

		xs = &cnxk_mldev->xstats.entries[stat_id];
		if (!xs->reset_allowed)
			continue;

		xs->reset_value =
			cnxk_ml_dev_xstat_get(cnxk_mldev, xs->obj_idx, xs->layer_id, xs->type);
	}

	return 0;
}

#define ML_AVG_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, str)                                     \
	do {                                                                                       \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {        \
			layer->glow.burst_xstats[qp_id].str##_latency_tot = 0;                     \
			layer->glow.burst_xstats[qp_id].str##_reset_count =                        \
				layer->glow.burst_xstats[qp_id].dequeued_count;                    \
		}                                                                                  \
	} while (0)

#define ML_MIN_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, str)                                     \
	do {                                                                                       \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++)          \
			layer->glow.burst_xstats[qp_id].str##_latency_min = UINT64_MAX;            \
	} while (0)

#define ML_MAX_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, str)                                     \
	do {                                                                                       \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++)          \
			layer->glow.burst_xstats[qp_id].str##_latency_max = 0;                     \
	} while (0)

static void
cnxk_ml_reset_model_stat(struct cnxk_ml_dev *cnxk_mldev, uint16_t model_id,
			 enum cnxk_ml_xstats_type type)
{
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	uint16_t layer_id = 0;
	uint32_t qp_id;

	model = cnxk_mldev->mldev->data->models[model_id];
	layer = &model->layer[layer_id];

	switch (type) {
	case avg_hw_latency:
		ML_AVG_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, hw);
		break;
	case min_hw_latency:
		ML_MIN_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, hw);
		break;
	case max_hw_latency:
		ML_MAX_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, hw);
		break;
	case avg_fw_latency:
		ML_AVG_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, fw);
		break;
	case min_fw_latency:
		ML_MIN_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, fw);
		break;
	case max_fw_latency:
		ML_MAX_RESET_FOREACH_QP(cnxk_mldev, layer, qp_id, fw);
		break;
	default:
		return;
	}
}

static int
cnxk_ml_model_xstats_reset(struct cnxk_ml_dev *cnxk_mldev, int32_t model_id,
			   const uint16_t stat_ids[], uint16_t nb_ids)
{
	struct cnxk_ml_xstats_entry *xs;
	struct cnxk_ml_model *model;
	int32_t lcl_model_id = 0;
	uint16_t layer_id = 0;
	uint16_t start_id;
	uint16_t end_id;
	int32_t i;
	int32_t j;

	for (i = 0; i < ML_CNXK_MAX_MODELS; i++) {
		if (model_id == -1) {
			model = cnxk_mldev->mldev->data->models[i];
			if (model == NULL) /* skip inactive models */
				continue;
		} else {
			if (model_id != i)
				continue;

			model = cnxk_mldev->mldev->data->models[model_id];
			if (model == NULL) {
				plt_err("Invalid model_id = %d\n", model_id);
				return -EINVAL;
			}
		}

		start_id = cnxk_mldev->xstats.offset_for_layer[i][layer_id];
		end_id = cnxk_mldev->xstats.offset_for_layer[i][layer_id] +
			 cnxk_mldev->xstats.count_per_layer[i][layer_id] - 1;

		if (stat_ids == NULL) {
			for (j = start_id; j <= end_id; j++) {
				xs = &cnxk_mldev->xstats.entries[j];
				cnxk_ml_reset_model_stat(cnxk_mldev, i, xs->type);
			}
		} else {
			for (j = 0; j < nb_ids; j++) {
				if (stat_ids[j] < start_id || stat_ids[j] > end_id) {
					plt_err("Invalid stat_ids[%d] = %d for model_id = %d\n", j,
						stat_ids[j], lcl_model_id);
					return -EINVAL;
				}
				xs = &cnxk_mldev->xstats.entries[stat_ids[j]];
				cnxk_ml_reset_model_stat(cnxk_mldev, i, xs->type);
			}
		}
	}

	return 0;
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

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI)
		return cn10k_ml_dev_info_get(cnxk_mldev, dev_info);
	else
		return mvtvm_ml_dev_info_get(cnxk_mldev, dev_info);

	return 0;
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
		if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI) {
			ret = cn10k_ml_fw_load(cnxk_mldev);
			if (ret != 0)
				return ret;
		}
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

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI) {
		ret = cn10k_ml_dev_configure(cnxk_mldev, conf);
		if (ret != 0) {
			plt_err("Failed to configure CN10K ML Device");
			goto error;
		}
	}

	ret = mvtvm_ml_dev_configure(cnxk_mldev, conf);
	if (ret != 0)
		goto error;

	/* Set device capabilities */
	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI)
		cnxk_mldev->max_nb_layers =
			cnxk_mldev->cn10k_mldev.fw.req->cn10k_req.jd.fw_load.cap.s.max_models;
	else
		cnxk_mldev->max_nb_layers = ML_CNXK_MAX_MODELS;

	cnxk_mldev->mldev->enqueue_burst = cnxk_ml_enqueue_burst;
	cnxk_mldev->mldev->dequeue_burst = cnxk_ml_dequeue_burst;

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI)
		cnxk_mldev->mldev->op_error_get = cn10k_ml_op_error_get;

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

	/* Initialize xstats */
	ret = cnxk_ml_xstats_init(cnxk_mldev);
	if (ret != 0) {
		plt_err("Failed to initialize xstats");
		goto error;
	}

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

	/* Un-initialize xstats */
	cnxk_ml_xstats_uninit(cnxk_mldev);

	if (mvtvm_ml_dev_close(cnxk_mldev) != 0)
		plt_err("Failed to close MVTVM ML Device");

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI) {
		if (cn10k_ml_dev_close(cnxk_mldev) != 0)
			plt_err("Failed to close CN10K ML Device");
	}

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

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI) {
		ret = cn10k_ml_dev_start(cnxk_mldev);
		if (ret != 0) {
			plt_err("Failed to start CN10K ML Device");
			return ret;
		}
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

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI) {
		ret = cn10k_ml_dev_stop(cnxk_mldev);
		if (ret != 0) {
			plt_err("Failed to stop CN10K ML Device");
			return ret;
		}
	}

	cnxk_mldev->state = ML_CNXK_DEV_STATE_CONFIGURED;

	return 0;
}

static int
cnxk_ml_dev_dump(struct rte_ml_dev *dev, FILE *fp)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	uint16_t model_id;

	if ((dev == NULL) || (fp == NULL))
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	/* Dump model info */
	for (model_id = 0; model_id < cnxk_mldev->mldev->data->nb_models; model_id++) {
		model = cnxk_mldev->mldev->data->models[model_id];
		if (model != NULL)
			cnxk_ml_model_dump(cnxk_mldev, model, fp);
	}

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_PCI)
		return cn10k_ml_dev_dump(cnxk_mldev, fp);
	else
		return mvtvm_ml_dev_dump(cnxk_mldev, fp);

	return 0;
}

static int
cnxk_ml_dev_selftest(struct rte_ml_dev *dev)
{
	struct cnxk_ml_dev *cnxk_mldev;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_VDEV)
		return -ENOTSUP;

	return cn10k_ml_dev_selftest(cnxk_mldev);
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
cnxk_ml_dev_stats_get(struct rte_ml_dev *dev, struct rte_ml_dev_stats *stats)
{
	struct cnxk_ml_qp *qp;
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		qp = dev->data->queue_pairs[qp_id];
		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;
		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}

	return 0;
}

static void
cnxk_ml_dev_stats_reset(struct rte_ml_dev *dev)
{
	struct cnxk_ml_qp *qp;
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		qp = dev->data->queue_pairs[qp_id];
		qp->stats.enqueued_count = 0;
		qp->stats.dequeued_count = 0;
		qp->stats.enqueue_err_count = 0;
		qp->stats.dequeue_err_count = 0;
	}
}

static int
cnxk_ml_dev_xstats_names_get(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode,
			     int32_t model_id, struct rte_ml_dev_xstats_map *xstats_map,
			     uint32_t size)
{
	struct cnxk_ml_xstats_entry *xs;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	uint32_t xstats_mode_count;
	uint16_t layer_id;
	uint32_t idx = 0;
	uint32_t i;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;
	xstats_mode_count = 0;

	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		xstats_mode_count = cnxk_mldev->xstats.count_mode_device;
		break;
	case RTE_ML_DEV_XSTATS_MODEL:
		if (model_id >= ML_CNXK_MAX_MODELS)
			break;

		model = cnxk_mldev->mldev->data->models[model_id];
		for (layer_id = 0; layer_id < model->nb_layers; layer_id++) {
			if (model->layer[layer_id].type == ML_CNXK_LAYER_TYPE_MRVL)
				xstats_mode_count +=
					cnxk_mldev->xstats.count_per_layer[model_id][layer_id];
		}

		if ((model->type == ML_CNXK_MODEL_TYPE_TVM) &&
		    (model->subtype != ML_CNXK_MODEL_SUBTYPE_TVM_MRVL))
			xstats_mode_count += RTE_DIM(model_xstats);
		break;
	default:
		return -EINVAL;
	};

	if (xstats_mode_count > size || xstats_map == NULL)
		return xstats_mode_count;

	for (i = 0; i < cnxk_mldev->xstats.count && idx < size; i++) {
		xs = &cnxk_mldev->xstats.entries[i];
		if (xs->mode != mode)
			continue;

		if (mode == RTE_ML_DEV_XSTATS_MODEL) {
			if (model_id != xs->obj_idx)
				continue;

			model = cnxk_mldev->mldev->data->models[model_id];
			if ((model->type == ML_CNXK_MODEL_TYPE_GLOW ||
			     model->subtype == ML_CNXK_MODEL_SUBTYPE_TVM_MRVL) &&
			    xs->group == CNXK_ML_XSTATS_GROUP_MODEL)
				continue;

			if (model->type == ML_CNXK_MODEL_TYPE_TVM &&
			    model->layer[xs->layer_id].type == ML_CNXK_LAYER_TYPE_LLVM)
				continue;
		}

		rte_strscpy(xstats_map[idx].name, xs->map.name, RTE_ML_STR_MAX);
		xstats_map[idx].id = xs->map.id;
		idx++;
	}

	return idx;
}

static int
cnxk_ml_dev_xstats_by_name_get(struct rte_ml_dev *dev, const char *name, uint16_t *stat_id,
			       uint64_t *value)
{
	struct cnxk_ml_xstats_entry *xs;
	struct cnxk_ml_dev *cnxk_mldev;
	cnxk_ml_xstats_fn fn;
	uint32_t i;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	for (i = 0; i < cnxk_mldev->xstats.count; i++) {
		xs = &cnxk_mldev->xstats.entries[i];
		if (strncmp(xs->map.name, name, RTE_ML_STR_MAX) == 0) {
			if (stat_id != NULL)
				*stat_id = xs->map.id;

			switch (xs->fn_id) {
			case CNXK_ML_XSTATS_FN_DEVICE:
				fn = cnxk_ml_dev_xstat_get;
				break;
			case CNXK_ML_XSTATS_FN_MODEL:
				fn = cnxk_ml_model_xstat_get;
				break;
			default:
				plt_err("Unexpected xstat fn_id = %d", xs->fn_id);
				return -EINVAL;
			}

			*value = fn(cnxk_mldev, xs->obj_idx, xs->layer_id, xs->type) -
				 xs->reset_value;

			return 0;
		}
	}

	if (stat_id != NULL)
		*stat_id = (uint16_t)-1;

	return -EINVAL;
}

static int
cnxk_ml_dev_xstats_get(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
		       const uint16_t stat_ids[], uint64_t values[], uint16_t nb_ids)
{
	struct cnxk_ml_xstats_entry *xs;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	uint32_t xstats_mode_count;
	cnxk_ml_xstats_fn fn;
	uint16_t layer_id;
	uint64_t val;
	uint32_t idx;
	uint32_t i;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;
	xstats_mode_count = 0;

	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		xstats_mode_count = cnxk_mldev->xstats.count_mode_device;
		break;
	case RTE_ML_DEV_XSTATS_MODEL:
		if (model_id >= ML_CNXK_MAX_MODELS)
			return -EINVAL;

		model = cnxk_mldev->mldev->data->models[model_id];
		for (layer_id = 0; layer_id < model->nb_layers; layer_id++)
			xstats_mode_count += cnxk_mldev->xstats.count_per_layer[model_id][layer_id];

		if ((model->type == ML_CNXK_MODEL_TYPE_TVM) &&
		    (model->subtype != ML_CNXK_MODEL_SUBTYPE_TVM_MRVL))
			xstats_mode_count += RTE_DIM(model_xstats);
		break;
	default:
		return -EINVAL;
	};

	idx = 0;
	for (i = 0; i < nb_ids && idx < xstats_mode_count; i++) {
		xs = &cnxk_mldev->xstats.entries[stat_ids[i]];
		if (stat_ids[i] > cnxk_mldev->xstats.count || xs->mode != mode)
			continue;

		if (mode == RTE_ML_DEV_XSTATS_MODEL) {
			if (model_id != xs->obj_idx)
				continue;

			model = cnxk_mldev->mldev->data->models[xs->obj_idx];
			if ((model->type == ML_CNXK_MODEL_TYPE_GLOW ||
			     model->subtype == ML_CNXK_MODEL_SUBTYPE_TVM_MRVL) &&
			    xs->group == CNXK_ML_XSTATS_GROUP_MODEL)
				continue;

			if (xs->layer_id == -1 && xs->group == CNXK_ML_XSTATS_GROUP_LAYER)
				continue;
		}

		switch (xs->fn_id) {
		case CNXK_ML_XSTATS_FN_DEVICE:
			fn = cnxk_ml_dev_xstat_get;
			break;
		case CNXK_ML_XSTATS_FN_MODEL:
			fn = cnxk_ml_model_xstat_get;
			break;
		default:
			plt_err("Unexpected xstat fn_id = %d", xs->fn_id);
			return -EINVAL;
		}

		val = fn(cnxk_mldev, xs->obj_idx, xs->layer_id, xs->type);
		if (values)
			values[idx] = val;

		idx++;
	}

	return idx;
}

static int
cnxk_ml_dev_xstats_reset(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
			 const uint16_t stat_ids[], uint16_t nb_ids)
{
	struct cnxk_ml_dev *cnxk_mldev;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		return cnxk_ml_device_xstats_reset(cnxk_mldev, stat_ids, nb_ids);
	case RTE_ML_DEV_XSTATS_MODEL:
		return cnxk_ml_model_xstats_reset(cnxk_mldev, model_id, stat_ids, nb_ids);
	};

	return 0;
}

static int
cnxk_ml_model_load(struct rte_ml_dev *dev, struct rte_ml_model_params *params, uint16_t *model_id)
{
	struct rte_ml_dev_info dev_info;
	struct cnxk_ml_dev *cnxk_mldev;
	enum cnxk_ml_model_type type;
	struct cnxk_ml_model *model;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	uint16_t max_scratch_pages;
	struct cn10k_ml_ocm *ocm;
	uint64_t model_info_size;
	uint16_t total_wb_pages;
	uint16_t lcl_model_id;
	uint16_t layer_id;
	uint64_t mz_size;
	bool found;
	int ret;

	if (dev == NULL)
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;

	type = cnxk_ml_model_get_type(params);
	if (type == ML_CNXK_MODEL_TYPE_INVALID) {
		plt_err("Invalid / unsupported model type");
		return -EINVAL;
	}

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_VDEV && type != ML_CNXK_MODEL_TYPE_TVM) {
		plt_err("Unsupported model type");
		return -ENOTSUP;
	}

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
	model->type = type;
	model->model_id = lcl_model_id;
	model->info = PLT_PTR_ADD(
		model, PLT_ALIGN_CEIL(sizeof(struct cnxk_ml_model), dev_info.align_size));
	dev->data->models[lcl_model_id] = model;

	if (type == ML_CNXK_MODEL_TYPE_GLOW)
		ret = cn10k_ml_model_load(cnxk_mldev, params, model);
	else
		ret = mvtvm_ml_model_load(cnxk_mldev, params, model);
	if (ret != 0)
		goto error;

	max_scratch_pages = 0;
	total_wb_pages = 0;
	layer_id = 0;

	ocm = &cnxk_mldev->cn10k_mldev.ocm;

	if (model->type == ML_CNXK_MODEL_TYPE_GLOW) {
		total_wb_pages = total_wb_pages + model->layer[layer_id].glow.ocm_map.wb_pages;
		max_scratch_pages = PLT_MAX(max_scratch_pages,
					    model->layer[layer_id].glow.ocm_map.scratch_pages);
#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
	} else {
		for (layer_id = 0; layer_id < model->mvtvm.metadata.model.nb_layers; layer_id++) {
			if (model->layer[layer_id].type == ML_CNXK_LAYER_TYPE_MRVL) {
				total_wb_pages = total_wb_pages +
						 model->layer[layer_id].glow.ocm_map.wb_pages;
				max_scratch_pages =
					PLT_MAX(max_scratch_pages,
						model->layer[layer_id].glow.ocm_map.scratch_pages);
			}
		}
#endif
	}

	if ((total_wb_pages + max_scratch_pages) > ocm->num_pages) {
		plt_err("model_id = %u: total_wb_pages (%u) + scratch_pages (%u) >  %u\n",
			lcl_model_id, total_wb_pages, max_scratch_pages, ocm->num_pages);

		if (model->type == ML_CNXK_MODEL_TYPE_GLOW) {
			plt_ml_dbg("layer_id = %u: wb_pages = %u, scratch_pages = %u\n", layer_id,
				   model->layer[layer_id].glow.ocm_map.wb_pages,
				   model->layer[layer_id].glow.ocm_map.scratch_pages);
#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
		} else {
			for (layer_id = 0; layer_id < model->mvtvm.metadata.model.nb_layers;
			     layer_id++) {
				if (model->layer[layer_id].type == ML_CNXK_LAYER_TYPE_MRVL) {
					plt_ml_dbg(
						"layer_id = %u: wb_pages = %u, scratch_pages = %u\n",
						layer_id,
						model->layer[layer_id].glow.ocm_map.wb_pages,
						model->layer[layer_id].glow.ocm_map.scratch_pages);
				}
			}
#endif
		}

		if (model->type == ML_CNXK_MODEL_TYPE_GLOW)
			cn10k_ml_model_unload(cnxk_mldev, model);
#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
		else {
			mvtvm_ml_model_unload(cnxk_mldev, model);
			return -ENOMEM;
		}
#endif
	}
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
	int ret = 0;

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

	if (model->type == ML_CNXK_MODEL_TYPE_GLOW)
		ret = cn10k_ml_model_unload(cnxk_mldev, model);
	else
		ret = mvtvm_ml_model_unload(cnxk_mldev, model);
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

	if (model->type == ML_CNXK_MODEL_TYPE_GLOW)
		return cn10k_ml_model_start(cnxk_mldev, model);
	else
		return mvtvm_ml_model_start(cnxk_mldev, model);

	return 0;
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

	if (model->type == ML_CNXK_MODEL_TYPE_GLOW)
		return cn10k_ml_model_stop(cnxk_mldev, model);
	else
		return mvtvm_ml_model_stop(cnxk_mldev, model);

	return 0;
}

static int
cnxk_ml_model_info_get(struct rte_ml_dev *dev, uint16_t model_id,
		       struct rte_ml_model_info *model_info)
{
	struct rte_ml_model_info *info;
	struct cnxk_ml_model *model;

	if ((dev == NULL) || (model_info == NULL))
		return -EINVAL;

	model = dev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	info = (struct rte_ml_model_info *)model->info;
	rte_memcpy(model_info, info, sizeof(struct rte_ml_model_info));
	model_info->input_info = info->input_info;
	model_info->output_info = info->output_info;

	return 0;
}

static int
cnxk_ml_model_params_update(struct rte_ml_dev *dev, uint16_t model_id, void *buffer)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;

	if ((dev == NULL) || (buffer == NULL))
		return -EINVAL;

	cnxk_mldev = dev->data->dev_private;
	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_VDEV)
		return -ENOTSUP;

	model = dev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	return cn10k_ml_model_params_update(cnxk_mldev, model, buffer);
}

static int
cnxk_ml_io_quantize(struct rte_ml_dev *dev, uint16_t model_id, struct rte_ml_buff_seg **dbuffer,
		    struct rte_ml_buff_seg **qbuffer)
{
	struct cnxk_ml_io_info *info = NULL;
	struct cnxk_ml_model *model;
	uint8_t *lcl_dbuffer;
	uint8_t *lcl_qbuffer;
	uint64_t d_offset;
	uint64_t q_offset;
	uint32_t i;
	int ret;

	if ((dev == NULL) || (dbuffer == NULL) || (qbuffer == NULL))
		return -EINVAL;

	model = dev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	if (model->type == ML_CNXK_MODEL_TYPE_GLOW)
		info = cn10k_ml_model_io_info_get(model, 0);
	else
		info = mvtvm_ml_model_io_info_get(model, 0);

	if (info == NULL)
		return -EINVAL;

	d_offset = 0;
	q_offset = 0;
	for (i = 0; i < info->nb_inputs; i++) {
		if (model->type == ML_CNXK_MODEL_TYPE_TVM) {
			lcl_dbuffer = dbuffer[i]->addr;
			lcl_qbuffer = qbuffer[i]->addr;
		} else {
			lcl_dbuffer = RTE_PTR_ADD(dbuffer[0]->addr, d_offset);
			lcl_qbuffer = RTE_PTR_ADD(qbuffer[0]->addr, q_offset);
		}

		ret = cnxk_ml_io_quantize_single(&info->input[i], lcl_dbuffer, lcl_qbuffer);
		if (ret < 0)
			return ret;

		if (model->type == ML_CNXK_MODEL_TYPE_GLOW) {
			d_offset += info->input[i].sz_d;
			q_offset += info->input[i].sz_q;
		}
	}

	return 0;
}

static int
cnxk_ml_io_dequantize(struct rte_ml_dev *dev, uint16_t model_id, struct rte_ml_buff_seg **qbuffer,
		      struct rte_ml_buff_seg **dbuffer)
{
	struct cnxk_ml_io_info *info = NULL;
	struct cnxk_ml_model *model;
	uint8_t *lcl_qbuffer;
	uint8_t *lcl_dbuffer;
	uint64_t q_offset;
	uint64_t d_offset;
	uint32_t i;
	int ret;

	if ((dev == NULL) || (qbuffer == NULL) || (dbuffer == NULL))
		return -EINVAL;

	model = dev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	if (model->type == ML_CNXK_MODEL_TYPE_GLOW)
		info = cn10k_ml_model_io_info_get(model, model->nb_layers - 1);
	else
		info = mvtvm_ml_model_io_info_get(model, model->nb_layers - 1);

	if (info == NULL)
		return -EINVAL;

	q_offset = 0;
	d_offset = 0;
	for (i = 0; i < info->nb_outputs; i++) {
		if (model->type == ML_CNXK_MODEL_TYPE_TVM) {
			lcl_qbuffer = qbuffer[i]->addr;
			lcl_dbuffer = dbuffer[i]->addr;
		} else {
			lcl_qbuffer = RTE_PTR_ADD(qbuffer[0]->addr, q_offset);
			lcl_dbuffer = RTE_PTR_ADD(dbuffer[0]->addr, d_offset);
		}

		ret = cnxk_ml_io_dequantize_single(&info->output[i], lcl_qbuffer, lcl_dbuffer);
		if (ret < 0)
			return ret;

		if (model->type == ML_CNXK_MODEL_TYPE_GLOW) {
			q_offset += info->output[i].sz_q;
			d_offset += info->output[i].sz_d;
		}
	}

	return 0;
}

static __rte_always_inline void
queue_index_advance(uint64_t *index, uint64_t nb_desc)
{
	*index = (*index + 1) % nb_desc;
}

static __rte_always_inline uint64_t
queue_pending_count(uint64_t head, uint64_t tail, uint64_t nb_desc)
{
	return (nb_desc + head - tail) % nb_desc;
}

static __rte_always_inline uint64_t
queue_free_count(uint64_t head, uint64_t tail, uint64_t nb_desc)
{
	return nb_desc - queue_pending_count(head, tail, nb_desc) - 1;
}

__rte_hot uint16_t
cnxk_ml_enqueue_burst(struct rte_ml_dev *dev, uint16_t qp_id, struct rte_ml_op **ops,
		      uint16_t nb_ops)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_queue *queue;
	struct cnxk_ml_qp *qp;
	struct rte_ml_op *op;

	uint16_t layer_id = 0;
	uint16_t count;
	uint64_t head;

	cnxk_mldev = dev->data->dev_private;
	qp = dev->data->queue_pairs[qp_id];
	queue = &qp->queue;

	head = queue->head;
	nb_ops = PLT_MIN(nb_ops, queue_free_count(head, queue->tail, qp->nb_desc));
	count = 0;

	if (unlikely(nb_ops == 0))
		return 0;

enqueue_req:
	op = ops[count];
	model = cnxk_mldev->mldev->data->models[op->model_id];

	if (unlikely(!model->enqueue_single(cnxk_mldev, op, layer_id, qp, head)))
		goto jcmdq_full;

	queue_index_advance(&head, qp->nb_desc);
	count++;

	if (count < nb_ops)
		goto enqueue_req;

jcmdq_full:
	queue->head = head;
	qp->stats.enqueued_count += count;
	rte_wmb();

	return count;
}

__rte_hot uint16_t
cnxk_ml_dequeue_burst(struct rte_ml_dev *dev, uint16_t qp_id, struct rte_ml_op **ops,
		      uint16_t nb_ops)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_queue *queue;
	struct cnxk_ml_model *model;
	struct cnxk_ml_req *req;
	struct cnxk_ml_qp *qp;

	uint64_t status;
	uint16_t count;
	uint64_t tail;

	cnxk_mldev = dev->data->dev_private;
	qp = dev->data->queue_pairs[qp_id];
	queue = &qp->queue;

	tail = queue->tail;
	nb_ops = PLT_MIN(nb_ops, queue_pending_count(queue->head, tail, qp->nb_desc));
	count = 0;

	if (unlikely(nb_ops == 0))
		goto empty_or_active;

dequeue_req:

	req = &queue->reqs[tail];
	model = cnxk_mldev->mldev->data->models[req->op->model_id];

	status = cnxk_ml_get_poll_ptr(req);
	if (unlikely(status != ML_CNXK_POLL_JOB_FINISH)) {
		if (plt_tsc_cycles() < req->timeout)
			goto empty_or_active;
		else /* Timeout, set indication of driver error */
			model->set_error_code(req, ML_CNXK_ETYPE_DRIVER, 0);
	}

	model->result_update(cnxk_mldev, qp->id, req);

	ops[count] = req->op;
	queue_index_advance(&tail, qp->nb_desc);
	count++;

	if (count < nb_ops)
		goto dequeue_req;

empty_or_active:
	queue->tail = tail;

	return count;
}

struct rte_ml_dev_ops cnxk_ml_ops = {
	/* Device control ops */
	.dev_info_get = cnxk_ml_dev_info_get,
	.dev_configure = cnxk_ml_dev_configure,
	.dev_close = cnxk_ml_dev_close,
	.dev_start = cnxk_ml_dev_start,
	.dev_stop = cnxk_ml_dev_stop,
	.dev_dump = cnxk_ml_dev_dump,
	.dev_selftest = cnxk_ml_dev_selftest,

	/* Queue-pair handling ops */
	.dev_queue_pair_setup = cnxk_ml_dev_queue_pair_setup,
	.dev_queue_pair_release = cnxk_ml_dev_queue_pair_release,

	/* Stats ops */
	.dev_stats_get = cnxk_ml_dev_stats_get,
	.dev_stats_reset = cnxk_ml_dev_stats_reset,
	.dev_xstats_names_get = cnxk_ml_dev_xstats_names_get,
	.dev_xstats_by_name_get = cnxk_ml_dev_xstats_by_name_get,
	.dev_xstats_get = cnxk_ml_dev_xstats_get,
	.dev_xstats_reset = cnxk_ml_dev_xstats_reset,

	/* Model ops */
	.model_load = cnxk_ml_model_load,
	.model_unload = cnxk_ml_model_unload,
	.model_start = cnxk_ml_model_start,
	.model_stop = cnxk_ml_model_stop,
	.model_info_get = cnxk_ml_model_info_get,
	.model_params_update = cnxk_ml_model_params_update,

	/* I/O ops */
	.io_quantize = cnxk_ml_io_quantize,
	.io_dequantize = cnxk_ml_io_dequantize,
};
