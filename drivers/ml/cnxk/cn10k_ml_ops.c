/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <mldev_utils.h>

#include "cn10k_ml_dev.h"
#include "cn10k_ml_model.h"
#include "cn10k_ml_ops.h"

/* ML model macros */
#define CN10K_ML_MODEL_MEMZONE_NAME "ml_cn10k_model_mz"

/* Debug print width */
#define STR_LEN	  12
#define FIELD_LEN 16
#define LINE_LEN  90

/* ML Job descriptor flags */
#define ML_FLAGS_POLL_COMPL BIT(0)
#define ML_FLAGS_SSO_COMPL  BIT(1)

static void
print_line(FILE *fp, int len)
{
	int i;

	for (i = 0; i < len; i++)
		fprintf(fp, "-");
	fprintf(fp, "\n");
}

static void
qp_memzone_name_get(char *name, int size, int dev_id, int qp_id)
{
	snprintf(name, size, "cn10k_ml_qp_mem_%u:%u", dev_id, qp_id);
}

static int
cn10k_ml_qp_destroy(const struct rte_ml_dev *dev, struct cn10k_ml_qp *qp)
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
cn10k_ml_dev_queue_pair_release(struct rte_ml_dev *dev, uint16_t queue_pair_id)
{
	struct cn10k_ml_qp *qp;
	int ret;

	qp = dev->data->queue_pairs[queue_pair_id];
	if (qp == NULL)
		return -EINVAL;

	ret = cn10k_ml_qp_destroy(dev, qp);
	if (ret) {
		plt_err("Could not destroy queue pair %u", queue_pair_id);
		return ret;
	}

	dev->data->queue_pairs[queue_pair_id] = NULL;

	return 0;
}

static struct cn10k_ml_qp *
cn10k_ml_qp_create(const struct rte_ml_dev *dev, uint16_t qp_id, uint32_t nb_desc, int socket_id)
{
	const struct rte_memzone *qp_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	struct cn10k_ml_qp *qp;
	uint32_t len;
	uint8_t *va;
	uint64_t i;

	/* Allocate queue pair */
	qp = rte_zmalloc_socket("cn10k_ml_pmd_queue_pair", sizeof(struct cn10k_ml_qp), ROC_ALIGN,
				socket_id);
	if (qp == NULL) {
		plt_err("Could not allocate queue pair");
		return NULL;
	}

	/* For request queue */
	len = nb_desc * sizeof(struct cn10k_ml_req);
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
	qp->queue.reqs = (struct cn10k_ml_req *)va;
	qp->queue.head = 0;
	qp->queue.tail = 0;
	qp->queue.wait_cycles = ML_CN10K_CMD_TIMEOUT * plt_tsc_hz();
	qp->nb_desc = nb_desc;

	/* Initialize job command */
	for (i = 0; i < qp->nb_desc; i++) {
		memset(&qp->queue.reqs[i].jd, 0, sizeof(struct cn10k_ml_jd));
		qp->queue.reqs[i].jcmd.w1.s.jobptr = PLT_U64_CAST(&qp->queue.reqs[i].jd);
	}

	return qp;

qp_free:
	rte_free(qp);

	return NULL;
}

static void
cn10k_ml_model_print(struct rte_ml_dev *dev, uint16_t model_id, FILE *fp)
{

	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_ocm *ocm;
	char str[STR_LEN];
	uint8_t i;

	mldev = dev->data->dev_private;
	ocm = &mldev->ocm;
	model = dev->data->models[model_id];

	/* Print debug info */
	print_line(fp, LINE_LEN);
	fprintf(fp, " Model Information (%s)\n", model->metadata.model.name);
	print_line(fp, LINE_LEN);
	fprintf(fp, "%*s : %s\n", FIELD_LEN, "name", model->metadata.model.name);
	fprintf(fp, "%*s : %u.%u.%u.%u\n", FIELD_LEN, "version", model->metadata.model.version[0],
		model->metadata.model.version[1], model->metadata.model.version[2],
		model->metadata.model.version[3]);
	if (strlen(model->name) != 0)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "debug_name", model->name);
	fprintf(fp, "%*s : 0x%016lx\n", FIELD_LEN, "model", PLT_U64_CAST(model));
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "model_id", model->model_id);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "batch_size", model->metadata.model.batch_size);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_layers", model->metadata.model.num_layers);

	/* Print model state */
	if (model->state == ML_CN10K_MODEL_STATE_LOADED)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "loaded");
	if (model->state == ML_CN10K_MODEL_STATE_JOB_ACTIVE)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "job_active");
	if (model->state == ML_CN10K_MODEL_STATE_STARTED)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "started");

	/* Print OCM status */
	fprintf(fp, "%*s : %" PRIu64 " bytes\n", FIELD_LEN, "wb_size",
		model->metadata.model.ocm_wb_range_end - model->metadata.model.ocm_wb_range_start +
			1);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "wb_pages", model->model_mem_map.wb_pages);
	fprintf(fp, "%*s : %" PRIu64 " bytes\n", FIELD_LEN, "scratch_size",
		ocm->size_per_tile - model->metadata.model.ocm_tmp_range_floor);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "scratch_pages", model->model_mem_map.scratch_pages);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_tiles",
		model->metadata.model.tile_end - model->metadata.model.tile_start + 1);

	if (model->state == ML_CN10K_MODEL_STATE_STARTED) {
		fprintf(fp, "%*s : 0x%0*" PRIx64 "\n", FIELD_LEN, "tilemask",
			ML_CN10K_OCM_NUMTILES / 4, model->model_mem_map.tilemask);
		fprintf(fp, "%*s : 0x%x\n", FIELD_LEN, "ocm_wb_start",
			model->model_mem_map.wb_page_start * ML_CN10K_OCM_PAGESIZE);
	}

	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_inputs", model->metadata.model.num_input);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_outputs", model->metadata.model.num_output);
	fprintf(fp, "\n");

	print_line(fp, LINE_LEN);
	fprintf(fp, "%8s  %16s  %12s  %18s  %12s  %14s\n", "input", "input_name", "input_type",
		"model_input_type", "quantize", "format");
	print_line(fp, LINE_LEN);
	for (i = 0; i < model->metadata.model.num_input; i++) {
		fprintf(fp, "%8u  ", i);
		fprintf(fp, "%*s  ", 16, model->metadata.input[i].input_name);
		rte_ml_io_type_to_str(model->metadata.input[i].input_type, str, STR_LEN);
		fprintf(fp, "%*s  ", 12, str);
		rte_ml_io_type_to_str(model->metadata.input[i].model_input_type, str, STR_LEN);
		fprintf(fp, "%*s  ", 18, str);
		fprintf(fp, "%*s", 12, (model->metadata.input[i].quantize == 1 ? "Yes" : "No"));
		rte_ml_io_format_to_str(model->metadata.input[i].shape.format, str, STR_LEN);
		fprintf(fp, "%*s", 16, str);
		fprintf(fp, "\n");
	}
	fprintf(fp, "\n");

	print_line(fp, LINE_LEN);
	fprintf(fp, "%8s  %16s  %12s  %18s  %12s\n", "output", "output_name", "output_type",
		"model_output_type", "dequantize");
	print_line(fp, LINE_LEN);
	for (i = 0; i < model->metadata.model.num_output; i++) {
		fprintf(fp, "%8u  ", i);
		fprintf(fp, "%*s  ", 16, model->metadata.output[i].output_name);
		rte_ml_io_type_to_str(model->metadata.output[i].output_type, str, STR_LEN);
		fprintf(fp, "%*s  ", 12, str);
		rte_ml_io_type_to_str(model->metadata.output[i].model_output_type, str, STR_LEN);
		fprintf(fp, "%*s  ", 18, str);
		fprintf(fp, "%*s", 12, (model->metadata.output[i].dequantize == 1 ? "Yes" : "No"));
		fprintf(fp, "\n");
	}
	fprintf(fp, "\n");
	print_line(fp, LINE_LEN);
	fprintf(fp, "\n");
}

static void
cn10k_ml_prep_sp_job_descriptor(struct cn10k_ml_dev *mldev, struct cn10k_ml_model *model,
				struct cn10k_ml_req *req, enum cn10k_ml_job_type job_type)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cn10k_ml_model_addr *addr;

	metadata = &model->metadata;
	addr = &model->addr;

	memset(&req->jd, 0, sizeof(struct cn10k_ml_jd));
	req->jd.hdr.jce.w0.u64 = 0;
	req->jd.hdr.jce.w1.u64 = PLT_U64_CAST(&req->status);
	req->jd.hdr.model_id = model->model_id;
	req->jd.hdr.job_type = job_type;
	req->jd.hdr.fp_flags = 0x0;
	req->jd.hdr.result = roc_ml_addr_ap2mlip(&mldev->roc, &req->result);

	if (job_type == ML_CN10K_JOB_TYPE_MODEL_START) {
		if (!model->metadata.model.ocm_relocatable)
			req->jd.hdr.sp_flags = ML_CN10K_SP_FLAGS_OCM_NONRELOCATABLE;
		else
			req->jd.hdr.sp_flags = 0x0;
		req->jd.model_start.model_src_ddr_addr =
			PLT_U64_CAST(roc_ml_addr_ap2mlip(&mldev->roc, addr->init_load_addr));
		req->jd.model_start.model_dst_ddr_addr =
			PLT_U64_CAST(roc_ml_addr_ap2mlip(&mldev->roc, addr->init_run_addr));
		req->jd.model_start.model_init_offset = 0x0;
		req->jd.model_start.model_main_offset = metadata->init_model.file_size;
		req->jd.model_start.model_finish_offset =
			metadata->init_model.file_size + metadata->main_model.file_size;
		req->jd.model_start.model_init_size = metadata->init_model.file_size;
		req->jd.model_start.model_main_size = metadata->main_model.file_size;
		req->jd.model_start.model_finish_size = metadata->finish_model.file_size;
		req->jd.model_start.model_wb_offset = metadata->init_model.file_size +
						      metadata->main_model.file_size +
						      metadata->finish_model.file_size;
		req->jd.model_start.num_layers = metadata->model.num_layers;
		req->jd.model_start.num_gather_entries = 0;
		req->jd.model_start.num_scatter_entries = 0;
		req->jd.model_start.tilemask = 0; /* Updated after reserving pages */
		req->jd.model_start.batch_size = model->batch_size;
		req->jd.model_start.ocm_wb_base_address = 0; /* Updated after reserving pages */
		req->jd.model_start.ocm_wb_range_start = metadata->model.ocm_wb_range_start;
		req->jd.model_start.ocm_wb_range_end = metadata->model.ocm_wb_range_end;
		req->jd.model_start.ddr_wb_base_address = PLT_U64_CAST(roc_ml_addr_ap2mlip(
			&mldev->roc,
			PLT_PTR_ADD(addr->finish_load_addr, metadata->finish_model.file_size)));
		req->jd.model_start.ddr_wb_range_start = metadata->model.ddr_wb_range_start;
		req->jd.model_start.ddr_wb_range_end = metadata->model.ddr_wb_range_end;
		req->jd.model_start.input.s.ddr_range_start = metadata->model.ddr_input_range_start;
		req->jd.model_start.input.s.ddr_range_end = metadata->model.ddr_input_range_end;
		req->jd.model_start.output.s.ddr_range_start =
			metadata->model.ddr_output_range_start;
		req->jd.model_start.output.s.ddr_range_end = metadata->model.ddr_output_range_end;
	}
}

static int
cn10k_ml_dev_info_get(struct rte_ml_dev *dev, struct rte_ml_dev_info *dev_info)
{
	if (dev_info == NULL)
		return -EINVAL;

	memset(dev_info, 0, sizeof(struct rte_ml_dev_info));
	dev_info->driver_name = dev->device->driver->name;
	dev_info->max_models = ML_CN10K_MAX_MODELS;
	dev_info->max_queue_pairs = ML_CN10K_MAX_QP_PER_DEVICE;
	dev_info->max_desc = ML_CN10K_MAX_DESC_PER_QP;
	dev_info->max_segments = ML_CN10K_MAX_SEGMENTS;
	dev_info->min_align_size = ML_CN10K_ALIGN_SIZE;

	return 0;
}

static int
cn10k_ml_dev_configure(struct rte_ml_dev *dev, const struct rte_ml_dev_config *conf)
{
	struct rte_ml_dev_info dev_info;
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_ocm *ocm;
	struct cn10k_ml_qp *qp;
	uint16_t model_id;
	uint32_t mz_size;
	uint16_t tile_id;
	uint16_t qp_id;
	int ret;

	if (dev == NULL || conf == NULL)
		return -EINVAL;

	/* Get CN10K device handle */
	mldev = dev->data->dev_private;

	cn10k_ml_dev_info_get(dev, &dev_info);
	if (conf->nb_models > dev_info.max_models) {
		plt_err("Invalid device config, nb_models > %u\n", dev_info.max_models);
		return -EINVAL;
	}

	if (conf->nb_queue_pairs > dev_info.max_queue_pairs) {
		plt_err("Invalid device config, nb_queue_pairs > %u\n", dev_info.max_queue_pairs);
		return -EINVAL;
	}

	if (mldev->state == ML_CN10K_DEV_STATE_PROBED) {
		plt_ml_dbg("Configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);

		/* Load firmware */
		ret = cn10k_ml_fw_load(mldev);
		if (ret != 0)
			return ret;
	} else if (mldev->state == ML_CN10K_DEV_STATE_CONFIGURED) {
		plt_ml_dbg("Re-configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);
	} else if (mldev->state == ML_CN10K_DEV_STATE_STARTED) {
		plt_err("Device can't be reconfigured in started state\n");
		return -ENOTSUP;
	} else if (mldev->state == ML_CN10K_DEV_STATE_CLOSED) {
		plt_err("Device can't be reconfigured after close\n");
		return -ENOTSUP;
	}

	/* Configure queue-pairs */
	if (dev->data->queue_pairs == NULL) {
		mz_size = sizeof(dev->data->queue_pairs[0]) * conf->nb_queue_pairs;
		dev->data->queue_pairs =
			rte_zmalloc("cn10k_mldev_queue_pairs", mz_size, RTE_CACHE_LINE_SIZE);
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
		dev->data->models = rte_zmalloc("cn10k_mldev_models", mz_size, RTE_CACHE_LINE_SIZE);
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
				if (model->state == ML_CN10K_MODEL_STATE_STARTED) {
					if (cn10k_ml_model_stop(dev, model_id) != 0)
						plt_err("Could not stop model %u", model_id);
				}
				if (model->state == ML_CN10K_MODEL_STATE_LOADED) {
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

	ocm = &mldev->ocm;
	ocm->num_tiles = ML_CN10K_OCM_NUMTILES;
	ocm->size_per_tile = ML_CN10K_OCM_TILESIZE;
	ocm->page_size = ML_CN10K_OCM_PAGESIZE;
	ocm->num_pages = ocm->size_per_tile / ocm->page_size;
	ocm->mask_words = ocm->num_pages / (8 * sizeof(uint8_t));

	for (tile_id = 0; tile_id < ocm->num_tiles; tile_id++)
		ocm->tile_ocm_info[tile_id].last_wb_page = -1;

	rte_spinlock_init(&ocm->lock);

	mldev->nb_models_loaded = 0;
	mldev->state = ML_CN10K_DEV_STATE_CONFIGURED;

	return 0;

error:
	if (dev->data->queue_pairs != NULL)
		rte_free(dev->data->queue_pairs);

	if (dev->data->models != NULL)
		rte_free(dev->data->models);

	return ret;
}

static int
cn10k_ml_dev_close(struct rte_ml_dev *dev)
{
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_qp *qp;
	uint16_t model_id;
	uint16_t qp_id;

	if (dev == NULL)
		return -EINVAL;

	mldev = dev->data->dev_private;

	/* Stop and unload all models */
	for (model_id = 0; model_id < dev->data->nb_models; model_id++) {
		model = dev->data->models[model_id];
		if (model != NULL) {
			if (model->state == ML_CN10K_MODEL_STATE_STARTED) {
				if (cn10k_ml_model_stop(dev, model_id) != 0)
					plt_err("Could not stop model %u", model_id);
			}
			if (model->state == ML_CN10K_MODEL_STATE_LOADED) {
				if (cn10k_ml_model_unload(dev, model_id) != 0)
					plt_err("Could not unload model %u", model_id);
			}
			dev->data->models[model_id] = NULL;
		}
	}

	if (dev->data->models)
		rte_free(dev->data->models);

	/* Destroy all queue pairs */
	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		qp = dev->data->queue_pairs[qp_id];
		if (qp != NULL) {
			if (cn10k_ml_qp_destroy(dev, qp) != 0)
				plt_err("Could not destroy queue pair %u", qp_id);
			dev->data->queue_pairs[qp_id] = NULL;
		}
	}

	if (dev->data->queue_pairs)
		rte_free(dev->data->queue_pairs);

	/* Unload firmware */
	cn10k_ml_fw_unload(mldev);

	/* Clear scratch registers */
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_WORK_PTR);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_FW_CTRL);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C0);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C0);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C1);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C1);

	/* Reset ML_MLR_BASE */
	roc_ml_reg_write64(&mldev->roc, 0, ML_MLR_BASE);
	plt_ml_dbg("ML_MLR_BASE = 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_MLR_BASE));

	mldev->state = ML_CN10K_DEV_STATE_CLOSED;

	/* Remove PCI device */
	return rte_dev_remove(dev->device);
}

static int
cn10k_ml_dev_start(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	uint64_t reg_val64;

	mldev = dev->data->dev_private;

	reg_val64 = roc_ml_reg_read64(&mldev->roc, ML_CFG);
	reg_val64 |= ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_CFG));

	mldev->state = ML_CN10K_DEV_STATE_STARTED;

	return 0;
}

static int
cn10k_ml_dev_stop(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	uint64_t reg_val64;

	mldev = dev->data->dev_private;

	reg_val64 = roc_ml_reg_read64(&mldev->roc, ML_CFG);
	reg_val64 &= ~ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_CFG));

	mldev->state = ML_CN10K_DEV_STATE_CONFIGURED;

	return 0;
}

static int
cn10k_ml_dev_queue_pair_setup(struct rte_ml_dev *dev, uint16_t queue_pair_id,
			      const struct rte_ml_dev_qp_conf *qp_conf, int socket_id)
{
	struct rte_ml_dev_info dev_info;
	struct cn10k_ml_qp *qp;
	uint32_t nb_desc;

	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		plt_err("Queue-pair id = %u (>= max queue pairs supported, %u)\n", queue_pair_id,
			dev->data->nb_queue_pairs);
		return -EINVAL;
	}

	if (dev->data->queue_pairs[queue_pair_id] != NULL)
		cn10k_ml_dev_queue_pair_release(dev, queue_pair_id);

	cn10k_ml_dev_info_get(dev, &dev_info);
	if ((qp_conf->nb_desc > dev_info.max_desc) || (qp_conf->nb_desc == 0)) {
		plt_err("Could not setup queue pair for %u descriptors", qp_conf->nb_desc);
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
	qp = cn10k_ml_qp_create(dev, queue_pair_id, nb_desc, socket_id);
	if (qp == NULL) {
		plt_err("Could not create queue pair %u", queue_pair_id);
		return -ENOMEM;
	}
	dev->data->queue_pairs[queue_pair_id] = qp;

	return 0;
}

static int
cn10k_ml_dev_dump(struct rte_ml_dev *dev, FILE *fp)
{
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_fw *fw;

	uint32_t head_loc;
	uint32_t tail_loc;
	uint16_t model_id;
	uint32_t bufsize;
	char *head_ptr;
	int core_id;

	if (roc_env_is_asim())
		return 0;

	mldev = dev->data->dev_private;
	fw = &mldev->fw;

	/* Dump model info */
	for (model_id = 0; model_id < dev->data->nb_models; model_id++) {
		model = dev->data->models[model_id];
		if (model != NULL) {
			cn10k_ml_model_print(dev, model_id, fp);
			fprintf(fp, "\n");
		}
	}

	/* Dump OCM state */
	cn10k_ml_ocm_print(dev, fp);

	/* Dump debug buffer */
	for (core_id = 0; core_id <= 1; core_id++) {
		bufsize = fw->req->jd.fw_load.debug.debug_buffer_size;
		if (core_id == 0) {
			head_loc = roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_DBG_BUFFER_HEAD_C0);
			tail_loc = roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_DBG_BUFFER_TAIL_C0);
			head_ptr = PLT_PTR_CAST(fw->req->jd.fw_load.debug.core0_debug_ptr);
			head_ptr = roc_ml_addr_mlip2ap(&mldev->roc, head_ptr);
		} else {
			head_loc = roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_DBG_BUFFER_HEAD_C1);
			tail_loc = roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_DBG_BUFFER_TAIL_C1);
			head_ptr = PLT_PTR_CAST(fw->req->jd.fw_load.debug.core1_debug_ptr);
			head_ptr = roc_ml_addr_mlip2ap(&mldev->roc, head_ptr);
		}
		if (head_loc < tail_loc) {
			fprintf(fp, "%.*s\n", tail_loc - head_loc, &head_ptr[head_loc]);
		} else if (head_loc >= tail_loc + 1) {
			fprintf(fp, "%.*s\n", bufsize - tail_loc, &head_ptr[head_loc]);
			fprintf(fp, "%.*s\n", tail_loc, &head_ptr[0]);
		}
	}

	/* Dump exception info */
	for (core_id = 0; core_id <= 1; core_id++) {
		bufsize = fw->req->jd.fw_load.debug.exception_state_size;
		if ((core_id == 0) &&
		    (roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_EXCEPTION_SP_C0) != 0)) {
			head_ptr = PLT_PTR_CAST(fw->req->jd.fw_load.debug.core0_exception_buffer);
			fprintf(fp, "ML_SCRATCH_EXCEPTION_SP_C0 = 0x%016lx",
				roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_EXCEPTION_SP_C0));
			head_ptr = roc_ml_addr_mlip2ap(&mldev->roc, head_ptr);
			fprintf(fp, "%.*s", bufsize, head_ptr);
		} else if ((core_id == 1) &&
			   (roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_EXCEPTION_SP_C1) != 0)) {
			head_ptr = PLT_PTR_CAST(fw->req->jd.fw_load.debug.core1_exception_buffer);
			fprintf(fp, "ML_SCRATCH_EXCEPTION_SP_C1 = 0x%016lx",
				roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_EXCEPTION_SP_C1));
			head_ptr = roc_ml_addr_mlip2ap(&mldev->roc, head_ptr);
			fprintf(fp, "%.*s", bufsize, head_ptr);
		}
	}

	return 0;
}

int
cn10k_ml_model_load(struct rte_ml_dev *dev, struct rte_ml_model_params *params, uint16_t *model_id)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	size_t model_data_size;
	size_t model_info_size;
	uint8_t *base_dma_addr;
	uint16_t scratch_pages;
	uint16_t wb_pages;
	uint64_t mz_size;
	uint16_t idx;
	bool found;
	int ret;

	ret = cn10k_ml_model_metadata_check(params->addr, params->size);
	if (ret != 0)
		return ret;

	mldev = dev->data->dev_private;

	/* Find model ID */
	found = false;
	for (idx = 0; idx < dev->data->nb_models; idx++) {
		if (dev->data->models[idx] == NULL) {
			found = true;
			break;
		}
	}

	if (!found) {
		plt_err("No slots available to load new model");
		return -ENOMEM;
	}

	/* Get WB and scratch pages, check if model can be loaded. */
	ret = cn10k_ml_model_ocm_pages_count(mldev, idx, params->addr, &wb_pages, &scratch_pages);
	if (ret < 0)
		return ret;

	/* Compute memzone size */
	metadata = (struct cn10k_ml_model_metadata *)params->addr;
	model_data_size = metadata->init_model.file_size + metadata->main_model.file_size +
			  metadata->finish_model.file_size + metadata->weights_bias.file_size;
	model_data_size = PLT_ALIGN_CEIL(model_data_size, ML_CN10K_ALIGN_SIZE);
	model_info_size = sizeof(struct rte_ml_model_info) +
			  metadata->model.num_input * sizeof(struct rte_ml_io_info) +
			  metadata->model.num_output * sizeof(struct rte_ml_io_info);
	model_info_size = PLT_ALIGN_CEIL(model_info_size, ML_CN10K_ALIGN_SIZE);

	mz_size = PLT_ALIGN_CEIL(sizeof(struct cn10k_ml_model), ML_CN10K_ALIGN_SIZE) +
		  2 * model_data_size + model_info_size +
		  PLT_ALIGN_CEIL(sizeof(struct cn10k_ml_req), ML_CN10K_ALIGN_SIZE);

	/* Allocate memzone for model object and model data */
	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", CN10K_ML_MODEL_MEMZONE_NAME, idx);
	mz = plt_memzone_reserve_aligned(str, mz_size, 0, ML_CN10K_ALIGN_SIZE);
	if (!mz) {
		plt_err("plt_memzone_reserve failed : %s", str);
		return -ENOMEM;
	}

	model = mz->addr;
	model->mldev = mldev;
	model->model_id = idx;

	rte_memcpy(&model->metadata, params->addr, sizeof(struct cn10k_ml_model_metadata));
	cn10k_ml_model_metadata_update(&model->metadata);

	/* Enable support for batch_size of 256 */
	if (model->metadata.model.batch_size == 0)
		model->batch_size = 256;
	else
		model->batch_size = model->metadata.model.batch_size;

	/* Set DMA base address */
	base_dma_addr = PLT_PTR_ADD(
		mz->addr, PLT_ALIGN_CEIL(sizeof(struct cn10k_ml_model), ML_CN10K_ALIGN_SIZE));
	cn10k_ml_model_addr_update(model, params->addr, base_dma_addr);

	/* Copy data from load to run. run address to be used by MLIP */
	rte_memcpy(model->addr.base_dma_addr_run, model->addr.base_dma_addr_load, model_data_size);

	/* Initialize model_mem_map */
	memset(&model->model_mem_map, 0, sizeof(struct cn10k_ml_ocm_model_map));
	model->model_mem_map.ocm_reserved = false;
	model->model_mem_map.tilemask = 0;
	model->model_mem_map.wb_page_start = -1;
	model->model_mem_map.wb_pages = wb_pages;
	model->model_mem_map.scratch_pages = scratch_pages;

	/* Set model info */
	model->info = PLT_PTR_ADD(base_dma_addr, 2 * model_data_size);
	cn10k_ml_model_info_set(dev, model);

	/* Set slow-path request address and state */
	model->req = PLT_PTR_ADD(model->info, model_info_size);

	plt_spinlock_init(&model->lock);
	model->state = ML_CN10K_MODEL_STATE_LOADED;
	dev->data->models[idx] = model;
	mldev->nb_models_loaded++;

	*model_id = idx;

	return 0;
}

int
cn10k_ml_model_unload(struct rte_ml_dev *dev, uint16_t model_id)
{
	char str[RTE_MEMZONE_NAMESIZE];
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;

	mldev = dev->data->dev_private;
	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	if (model->state != ML_CN10K_MODEL_STATE_LOADED) {
		plt_err("Cannot unload. Model in use.");
		return -EBUSY;
	}

	dev->data->models[model_id] = NULL;
	mldev->nb_models_loaded--;

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", CN10K_ML_MODEL_MEMZONE_NAME, model_id);
	return plt_memzone_free(plt_memzone_lookup(str));
}

int
cn10k_ml_model_start(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_ocm *ocm;
	struct cn10k_ml_req *req;

	bool job_enqueued;
	bool job_dequeued;
	uint8_t num_tiles;
	uint64_t tilemask;
	int wb_page_start;
	int tile_start;
	int tile_end;
	bool locked;
	int ret = 0;

	mldev = dev->data->dev_private;
	ocm = &mldev->ocm;
	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	/* Prepare JD */
	req = model->req;
	cn10k_ml_prep_sp_job_descriptor(mldev, model, req, ML_CN10K_JOB_TYPE_MODEL_START);
	req->result.error_code = 0x0;
	req->result.user_ptr = NULL;

	plt_write64(ML_CN10K_POLL_JOB_START, &req->status);
	plt_wmb();

	num_tiles = model->metadata.model.tile_end - model->metadata.model.tile_start + 1;

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			if (model->state == ML_CN10K_MODEL_STATE_STARTED) {
				plt_ml_dbg("Model already started, model = 0x%016lx",
					   PLT_U64_CAST(model));
				plt_spinlock_unlock(&model->lock);
				return 1;
			}

			if (model->state == ML_CN10K_MODEL_STATE_JOB_ACTIVE) {
				plt_err("A slow-path job is active for the model = 0x%016lx",
					PLT_U64_CAST(model));
				plt_spinlock_unlock(&model->lock);
				return -EBUSY;
			}

			model->state = ML_CN10K_MODEL_STATE_JOB_ACTIVE;
			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	while (!model->model_mem_map.ocm_reserved) {
		if (plt_spinlock_trylock(&ocm->lock) != 0) {
			wb_page_start = cn10k_ml_ocm_tilemask_find(
				dev, num_tiles, model->model_mem_map.wb_pages,
				model->model_mem_map.scratch_pages, &tilemask);

			if (wb_page_start == -1) {
				plt_err("Free pages not available on OCM tiles");
				plt_err("Failed to start model = 0x%016lx, name = %s",
					PLT_U64_CAST(model), model->metadata.model.name);

				plt_spinlock_unlock(&ocm->lock);
				return -ENOMEM;
			}

			model->model_mem_map.tilemask = tilemask;
			model->model_mem_map.wb_page_start = wb_page_start;

			cn10k_ml_ocm_reserve_pages(
				dev, model->model_id, model->model_mem_map.tilemask,
				model->model_mem_map.wb_page_start, model->model_mem_map.wb_pages,
				model->model_mem_map.scratch_pages);
			model->model_mem_map.ocm_reserved = true;
			plt_spinlock_unlock(&ocm->lock);
		}
	}

	/* Update JD */
	cn10k_ml_ocm_tilecount(model->model_mem_map.tilemask, &tile_start, &tile_end);
	req->jd.model_start.tilemask = GENMASK_ULL(tile_end, tile_start);
	req->jd.model_start.ocm_wb_base_address =
		model->model_mem_map.wb_page_start * ocm->page_size;

	job_enqueued = false;
	job_dequeued = false;
	do {
		if (!job_enqueued) {
			req->timeout = plt_tsc_cycles() + ML_CN10K_CMD_TIMEOUT * plt_tsc_hz();
			job_enqueued = roc_ml_scratch_enqueue(&mldev->roc, &req->jd);
		}

		if (job_enqueued && !job_dequeued)
			job_dequeued = roc_ml_scratch_dequeue(&mldev->roc, &req->jd);

		if (job_dequeued)
			break;
	} while (plt_tsc_cycles() < req->timeout);

	if (job_dequeued) {
		if (plt_read64(&req->status) == ML_CN10K_POLL_JOB_FINISH) {
			if (req->result.error_code == 0)
				ret = 0;
			else
				ret = -1;
		}
	} else { /* Reset scratch registers */
		roc_ml_scratch_queue_reset(&mldev->roc);
		ret = -ETIME;
	}

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			if (ret == 0)
				model->state = ML_CN10K_MODEL_STATE_STARTED;
			else
				model->state = ML_CN10K_MODEL_STATE_UNKNOWN;

			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	if (model->state == ML_CN10K_MODEL_STATE_UNKNOWN) {
		while (model->model_mem_map.ocm_reserved) {
			if (plt_spinlock_trylock(&ocm->lock) != 0) {
				cn10k_ml_ocm_free_pages(dev, model->model_id);
				model->model_mem_map.ocm_reserved = false;
				model->model_mem_map.tilemask = 0x0;
				plt_spinlock_unlock(&ocm->lock);
			}
		}
	}

	return ret;
}

int
cn10k_ml_model_stop(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_ocm *ocm;
	struct cn10k_ml_req *req;

	bool job_enqueued;
	bool job_dequeued;
	bool locked;
	int ret = 0;

	mldev = dev->data->dev_private;
	ocm = &mldev->ocm;
	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	/* Prepare JD */
	req = model->req;
	cn10k_ml_prep_sp_job_descriptor(mldev, model, req, ML_CN10K_JOB_TYPE_MODEL_STOP);
	req->result.error_code = 0x0;
	req->result.user_ptr = NULL;

	plt_write64(ML_CN10K_POLL_JOB_START, &req->status);
	plt_wmb();

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			if (model->state == ML_CN10K_MODEL_STATE_LOADED) {
				plt_ml_dbg("Model not started, model = 0x%016lx",
					   PLT_U64_CAST(model));
				plt_spinlock_unlock(&model->lock);
				return 1;
			}

			if (model->state == ML_CN10K_MODEL_STATE_JOB_ACTIVE) {
				plt_err("A slow-path job is active for the model = 0x%016lx",
					PLT_U64_CAST(model));
				plt_spinlock_unlock(&model->lock);
				return -EBUSY;
			}

			model->state = ML_CN10K_MODEL_STATE_JOB_ACTIVE;
			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	while (model->model_mem_map.ocm_reserved) {
		if (plt_spinlock_trylock(&ocm->lock) != 0) {
			cn10k_ml_ocm_free_pages(dev, model->model_id);
			model->model_mem_map.ocm_reserved = false;
			model->model_mem_map.tilemask = 0x0;
			plt_spinlock_unlock(&ocm->lock);
		}
	}

	job_enqueued = false;
	job_dequeued = false;
	do {
		if (!job_enqueued) {
			req->timeout = plt_tsc_cycles() + ML_CN10K_CMD_TIMEOUT * plt_tsc_hz();
			job_enqueued = roc_ml_scratch_enqueue(&mldev->roc, &req->jd);
		}

		if (job_enqueued && !job_dequeued)
			job_dequeued = roc_ml_scratch_dequeue(&mldev->roc, &req->jd);

		if (job_dequeued)
			break;
	} while (plt_tsc_cycles() < req->timeout);

	if (job_dequeued) {
		if (plt_read64(&req->status) == ML_CN10K_POLL_JOB_FINISH) {
			if (req->result.error_code == 0x0)
				ret = 0;
			else
				ret = -1;
		}
	} else {
		roc_ml_scratch_queue_reset(&mldev->roc);
		ret = -ETIME;
	}

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			model->state = ML_CN10K_MODEL_STATE_LOADED;
			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	return ret;
}

static int
cn10k_ml_model_info_get(struct rte_ml_dev *dev, uint16_t model_id,
			struct rte_ml_model_info *model_info)
{
	struct cn10k_ml_model *model;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	rte_memcpy(model_info, model->info, sizeof(struct rte_ml_model_info));
	model_info->input_info = ((struct rte_ml_model_info *)model->info)->input_info;
	model_info->output_info = ((struct rte_ml_model_info *)model->info)->output_info;

	return 0;
}

static int
cn10k_ml_model_params_update(struct rte_ml_dev *dev, uint16_t model_id, void *buffer)
{
	struct cn10k_ml_model *model;
	size_t size;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	if (model->state == ML_CN10K_MODEL_STATE_UNKNOWN)
		return -1;
	else if (model->state != ML_CN10K_MODEL_STATE_LOADED)
		return -EBUSY;

	size = model->metadata.init_model.file_size + model->metadata.main_model.file_size +
	       model->metadata.finish_model.file_size + model->metadata.weights_bias.file_size;

	/* Update model weights & bias */
	rte_memcpy(model->addr.wb_load_addr, buffer, model->metadata.weights_bias.file_size);

	/* Copy data from load to run. run address to be used by MLIP */
	rte_memcpy(model->addr.base_dma_addr_run, model->addr.base_dma_addr_load, size);

	return 0;
}

static int
cn10k_ml_io_input_size_get(struct rte_ml_dev *dev, uint16_t model_id, uint32_t nb_batches,
			   uint64_t *input_qsize, uint64_t *input_dsize)
{
	struct cn10k_ml_model *model;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	if (input_qsize != NULL)
		*input_qsize = PLT_U64_CAST(model->addr.total_input_sz_q *
					    PLT_DIV_CEIL(nb_batches, model->batch_size));

	if (input_dsize != NULL)
		*input_dsize = PLT_U64_CAST(model->addr.total_input_sz_d *
					    PLT_DIV_CEIL(nb_batches, model->batch_size));

	return 0;
}

static int
cn10k_ml_io_output_size_get(struct rte_ml_dev *dev, uint16_t model_id, uint32_t nb_batches,
			    uint64_t *output_qsize, uint64_t *output_dsize)
{
	struct cn10k_ml_model *model;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	if (output_qsize != NULL)
		*output_qsize = PLT_U64_CAST(model->addr.total_output_sz_q *
					     PLT_DIV_CEIL(nb_batches, model->batch_size));

	if (output_dsize != NULL)
		*output_dsize = PLT_U64_CAST(model->addr.total_output_sz_d *
					     PLT_DIV_CEIL(nb_batches, model->batch_size));

	return 0;
}

static int
cn10k_ml_io_quantize(struct rte_ml_dev *dev, uint16_t model_id, uint16_t nb_batches, void *dbuffer,
		     void *qbuffer)
{
	struct cn10k_ml_model *model;
	uint8_t *lcl_dbuffer;
	uint8_t *lcl_qbuffer;
	uint32_t batch_id;
	uint32_t i;
	int ret;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	lcl_dbuffer = dbuffer;
	lcl_qbuffer = qbuffer;
	batch_id = 0;

next_batch:
	for (i = 0; i < model->metadata.model.num_input; i++) {
		if (model->metadata.input[i].input_type ==
		    model->metadata.input[i].model_input_type) {
			rte_memcpy(lcl_qbuffer, lcl_dbuffer, model->addr.input[i].sz_d);
		} else {
			switch (model->metadata.input[i].model_input_type) {
			case RTE_ML_IO_TYPE_INT8:
				ret = rte_ml_io_float32_to_int8(model->metadata.input[i].qscale,
								model->addr.input[i].nb_elements,
								lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT8:
				ret = rte_ml_io_float32_to_uint8(model->metadata.input[i].qscale,
								 model->addr.input[i].nb_elements,
								 lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_INT16:
				ret = rte_ml_io_float32_to_int16(model->metadata.input[i].qscale,
								 model->addr.input[i].nb_elements,
								 lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT16:
				ret = rte_ml_io_float32_to_uint16(model->metadata.input[i].qscale,
								  model->addr.input[i].nb_elements,
								  lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_FP16:
				ret = rte_ml_io_float32_to_float16(model->addr.input[i].nb_elements,
								   lcl_dbuffer, lcl_qbuffer);
				break;
			default:
				plt_err("Unsupported model_input_type[%u] : %u", i,
					model->metadata.input[i].model_input_type);
				ret = -ENOTSUP;
			}
			if (ret < 0)
				return ret;
		}

		lcl_dbuffer += model->addr.input[i].sz_d;
		lcl_qbuffer += model->addr.input[i].sz_q;
	}

	batch_id++;
	if (batch_id < PLT_DIV_CEIL(nb_batches, model->batch_size))
		goto next_batch;

	return 0;
}

static int
cn10k_ml_io_dequantize(struct rte_ml_dev *dev, uint16_t model_id, uint16_t nb_batches,
		       void *qbuffer, void *dbuffer)
{
	struct cn10k_ml_model *model;
	uint8_t *lcl_qbuffer;
	uint8_t *lcl_dbuffer;
	uint32_t batch_id;
	uint32_t i;
	int ret;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	lcl_dbuffer = dbuffer;
	lcl_qbuffer = qbuffer;
	batch_id = 0;

next_batch:
	for (i = 0; i < model->metadata.model.num_output; i++) {
		if (model->metadata.output[i].output_type ==
		    model->metadata.output[i].model_output_type) {
			rte_memcpy(lcl_dbuffer, lcl_qbuffer, model->addr.output[i].sz_q);
		} else {
			switch (model->metadata.output[i].model_output_type) {
			case RTE_ML_IO_TYPE_INT8:
				ret = rte_ml_io_int8_to_float32(model->metadata.output[i].dscale,
								model->addr.output[i].nb_elements,
								lcl_qbuffer, lcl_dbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT8:
				ret = rte_ml_io_uint8_to_float32(model->metadata.output[i].dscale,
								 model->addr.output[i].nb_elements,
								 lcl_qbuffer, lcl_dbuffer);
				break;
			case RTE_ML_IO_TYPE_INT16:
				ret = rte_ml_io_int16_to_float32(model->metadata.output[i].dscale,
								 model->addr.output[i].nb_elements,
								 lcl_qbuffer, lcl_dbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT16:
				ret = rte_ml_io_uint16_to_float32(model->metadata.output[i].dscale,
								  model->addr.output[i].nb_elements,
								  lcl_qbuffer, lcl_dbuffer);
				break;
			case RTE_ML_IO_TYPE_FP16:
				ret = rte_ml_io_float16_to_float32(
					model->addr.output[i].nb_elements, lcl_qbuffer,
					lcl_dbuffer);
				break;
			default:
				plt_err("Unsupported model_output_type[%u] : %u", i,
					model->metadata.output[i].model_output_type);
				ret = -ENOTSUP;
			}
			if (ret < 0)
				return ret;
		}

		lcl_qbuffer += model->addr.output[i].sz_q;
		lcl_dbuffer += model->addr.output[i].sz_d;
	}

	batch_id++;
	if (batch_id < PLT_DIV_CEIL(nb_batches, model->batch_size))
		goto next_batch;

	return 0;
}

struct rte_ml_dev_ops cn10k_ml_ops = {
	/* Device control ops */
	.dev_info_get = cn10k_ml_dev_info_get,
	.dev_configure = cn10k_ml_dev_configure,
	.dev_close = cn10k_ml_dev_close,
	.dev_start = cn10k_ml_dev_start,
	.dev_stop = cn10k_ml_dev_stop,
	.dev_dump = cn10k_ml_dev_dump,

	/* Queue-pair handling ops */
	.dev_queue_pair_setup = cn10k_ml_dev_queue_pair_setup,
	.dev_queue_pair_release = cn10k_ml_dev_queue_pair_release,

	/* Model ops */
	.model_load = cn10k_ml_model_load,
	.model_unload = cn10k_ml_model_unload,
	.model_start = cn10k_ml_model_start,
	.model_stop = cn10k_ml_model_stop,
	.model_info_get = cn10k_ml_model_info_get,
	.model_params_update = cn10k_ml_model_params_update,

	/* I/O ops */
	.io_input_size_get = cn10k_ml_io_input_size_get,
	.io_output_size_get = cn10k_ml_io_output_size_get,
	.io_quantize = cn10k_ml_io_quantize,
	.io_dequantize = cn10k_ml_io_dequantize,
};
