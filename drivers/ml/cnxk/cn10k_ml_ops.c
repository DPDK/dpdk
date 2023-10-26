/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <mldev_utils.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_ops.h"
#include "cnxk_ml_xstats.h"

/* ML model macros */
#define CN10K_ML_MODEL_MEMZONE_NAME "ml_cn10k_model_mz"

/* ML layer macros */
#define CN10K_ML_LAYER_MEMZONE_NAME "ml_cn10k_layer_mz"

/* ML Job descriptor flags */
#define ML_FLAGS_POLL_COMPL BIT(0)
#define ML_FLAGS_SSO_COMPL  BIT(1)

/* Error message length */
#define ERRMSG_LEN 32

/* Error type database */
static const struct cn10k_ml_etype_db {
	enum cn10k_ml_error_etype etype;
	char name[ERRMSG_LEN];
} ml_etype_db[] = {
	{ML_ETYPE_NO_ERROR, "NO_ERROR"},	{ML_ETYPE_FW_NONFATAL, "FW_NON_FATAL"},
	{ML_ETYPE_HW_NONFATAL, "HW_NON_FATAL"}, {ML_ETYPE_HW_FATAL, "HW_FATAL"},
	{ML_ETYPE_HW_WARNING, "HW_WARNING"},	{ML_ETYPE_DRIVER, "DRIVER_ERROR"},
	{ML_ETYPE_UNKNOWN, "UNKNOWN_ERROR"},
};

/* Hardware non-fatal error subtype database */
static const struct cn10k_ml_stype_db_hw_nf {
	enum cn10k_ml_error_stype_fw_nf stype;
	char msg[ERRMSG_LEN];
} ml_stype_db_hw_nf[] = {
	{ML_FW_ERR_NOERR, "NO ERROR"},
	{ML_FW_ERR_UNLOAD_ID_NOT_FOUND, "UNLOAD MODEL ID NOT FOUND"},
	{ML_FW_ERR_LOAD_LUT_OVERFLOW, "LOAD LUT OVERFLOW"},
	{ML_FW_ERR_ID_IN_USE, "MODEL ID IN USE"},
	{ML_FW_ERR_INVALID_TILEMASK, "INVALID TILEMASK"},
	{ML_FW_ERR_RUN_LUT_OVERFLOW, "RUN LUT OVERFLOW"},
	{ML_FW_ERR_RUN_ID_NOT_FOUND, "RUN MODEL ID NOT FOUND"},
	{ML_FW_ERR_COMMAND_NOTSUP, "COMMAND NOT SUPPORTED"},
	{ML_FW_ERR_DDR_ADDR_RANGE, "DDR ADDRESS OUT OF RANGE"},
	{ML_FW_ERR_NUM_BATCHES_INVALID, "INVALID BATCHES"},
	{ML_FW_ERR_INSSYNC_TIMEOUT, "INSSYNC TIMEOUT"},
};

/* Driver error subtype database */
static const struct cn10k_ml_stype_db_driver {
	enum cn10k_ml_error_stype_driver stype;
	char msg[ERRMSG_LEN];
} ml_stype_db_driver[] = {
	{ML_DRIVER_ERR_NOERR, "NO ERROR"},
	{ML_DRIVER_ERR_UNKNOWN, "UNKNOWN ERROR"},
	{ML_DRIVER_ERR_EXCEPTION, "FW EXCEPTION"},
	{ML_DRIVER_ERR_FW_ERROR, "UNKNOWN FIRMWARE ERROR"},
};

static inline void
cn10k_ml_set_poll_addr(struct cnxk_ml_req *req)
{
	req->status = &req->cn10k_req.status;
}

static inline void
cn10k_ml_set_poll_ptr(struct cnxk_ml_req *req)
{
	plt_write64(ML_CNXK_POLL_JOB_START, req->status);
}

static inline uint64_t
cn10k_ml_get_poll_ptr(struct cnxk_ml_req *req)
{
	return plt_read64(req->status);
}

void
cn10k_ml_qp_initialize(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_qp *qp)
{
	uint64_t i;

	RTE_SET_USED(cnxk_mldev);

	/* Initialize job command */
	for (i = 0; i < qp->nb_desc; i++) {
		memset(&qp->queue.reqs[i].cn10k_req.jd, 0, sizeof(struct cn10k_ml_jd));
		qp->queue.reqs[i].cn10k_req.jcmd.w1.s.jobptr =
			PLT_U64_CAST(&qp->queue.reqs[i].cn10k_req.jd);
	}
}

static void
cn10k_ml_prep_sp_job_descriptor(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer,
				struct cnxk_ml_req *req, enum cn10k_ml_job_type job_type)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cn10k_ml_layer_addr *addr;
	struct cn10k_ml_dev *cn10k_mldev;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	metadata = &layer->glow.metadata;
	addr = &layer->glow.addr;

	memset(&req->cn10k_req.jd, 0, sizeof(struct cn10k_ml_jd));
	req->cn10k_req.jd.hdr.jce.w0.u64 = 0;
	req->cn10k_req.jd.hdr.jce.w1.u64 = PLT_U64_CAST(&req->cn10k_req.status);
	req->cn10k_req.jd.hdr.model_id = layer->index;
	req->cn10k_req.jd.hdr.job_type = job_type;
	req->cn10k_req.jd.hdr.fp_flags = 0x0;
	req->cn10k_req.jd.hdr.result =
		roc_ml_addr_ap2mlip(&cn10k_mldev->roc, &req->cn10k_req.result);

	if (job_type == ML_CN10K_JOB_TYPE_MODEL_START) {
		if (!layer->glow.metadata.model.ocm_relocatable)
			req->cn10k_req.jd.hdr.sp_flags = ML_CN10K_SP_FLAGS_OCM_NONRELOCATABLE;
		else
			req->cn10k_req.jd.hdr.sp_flags = 0x0;

		req->cn10k_req.jd.hdr.sp_flags |= ML_CN10K_SP_FLAGS_EXTENDED_LOAD_JD;
		req->cn10k_req.jd.model_start.extended_args = PLT_U64_CAST(
			roc_ml_addr_ap2mlip(&cn10k_mldev->roc, &req->cn10k_req.extended_args));
		req->cn10k_req.jd.model_start.model_dst_ddr_addr =
			PLT_U64_CAST(roc_ml_addr_ap2mlip(&cn10k_mldev->roc, addr->init_load_addr));
		req->cn10k_req.jd.model_start.model_init_offset = 0x0;
		req->cn10k_req.jd.model_start.model_main_offset = metadata->init_model.file_size;
		req->cn10k_req.jd.model_start.model_finish_offset =
			metadata->init_model.file_size + metadata->main_model.file_size;
		req->cn10k_req.jd.model_start.model_init_size = metadata->init_model.file_size;
		req->cn10k_req.jd.model_start.model_main_size = metadata->main_model.file_size;
		req->cn10k_req.jd.model_start.model_finish_size = metadata->finish_model.file_size;
		req->cn10k_req.jd.model_start.model_wb_offset = metadata->init_model.file_size +
								metadata->main_model.file_size +
								metadata->finish_model.file_size;
		req->cn10k_req.jd.model_start.num_layers = metadata->model.num_layers;
		req->cn10k_req.jd.model_start.num_gather_entries = 0;
		req->cn10k_req.jd.model_start.num_scatter_entries = 0;
		req->cn10k_req.jd.model_start.tilemask = 0; /* Updated after reserving pages */
		req->cn10k_req.jd.model_start.batch_size = layer->batch_size;
		req->cn10k_req.jd.model_start.ocm_wb_base_address =
			0; /* Updated after reserving pages */
		req->cn10k_req.jd.model_start.ocm_wb_range_start =
			metadata->model.ocm_wb_range_start;
		req->cn10k_req.jd.model_start.ocm_wb_range_end = metadata->model.ocm_wb_range_end;
		req->cn10k_req.jd.model_start.ddr_wb_base_address =
			PLT_U64_CAST(roc_ml_addr_ap2mlip(
				&cn10k_mldev->roc, PLT_PTR_ADD(addr->finish_load_addr,
							       metadata->finish_model.file_size)));
		req->cn10k_req.jd.model_start.ddr_wb_range_start =
			metadata->model.ddr_wb_range_start;
		req->cn10k_req.jd.model_start.ddr_wb_range_end = metadata->model.ddr_wb_range_end;
		req->cn10k_req.jd.model_start.input.s.ddr_range_start =
			metadata->model.ddr_input_range_start;
		req->cn10k_req.jd.model_start.input.s.ddr_range_end =
			metadata->model.ddr_input_range_end;
		req->cn10k_req.jd.model_start.output.s.ddr_range_start =
			metadata->model.ddr_output_range_start;
		req->cn10k_req.jd.model_start.output.s.ddr_range_end =
			metadata->model.ddr_output_range_end;

		req->cn10k_req.extended_args.start.ddr_scratch_base_address = PLT_U64_CAST(
			roc_ml_addr_ap2mlip(&cn10k_mldev->roc, addr->scratch_base_addr));
		req->cn10k_req.extended_args.start.ddr_scratch_range_start =
			metadata->model.ddr_scratch_range_start;
		req->cn10k_req.extended_args.start.ddr_scratch_range_end =
			metadata->model.ddr_scratch_range_end;
	}
}

static __rte_always_inline void
cn10k_ml_prep_fp_job_descriptor(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_req *req,
				struct rte_ml_op *op)
{
	struct cn10k_ml_dev *cn10k_mldev;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	req->cn10k_req.jd.hdr.jce.w0.u64 = 0;
	req->cn10k_req.jd.hdr.jce.w1.u64 = PLT_U64_CAST(req->status);
	req->cn10k_req.jd.hdr.model_id = op->model_id;
	req->cn10k_req.jd.hdr.job_type = ML_CN10K_JOB_TYPE_MODEL_RUN;
	req->cn10k_req.jd.hdr.fp_flags = ML_FLAGS_POLL_COMPL;
	req->cn10k_req.jd.hdr.sp_flags = 0x0;
	req->cn10k_req.jd.hdr.result =
		roc_ml_addr_ap2mlip(&cn10k_mldev->roc, &req->cn10k_req.result);
	req->cn10k_req.jd.model_run.input_ddr_addr =
		PLT_U64_CAST(roc_ml_addr_ap2mlip(&cn10k_mldev->roc, op->input[0]->addr));
	req->cn10k_req.jd.model_run.output_ddr_addr =
		PLT_U64_CAST(roc_ml_addr_ap2mlip(&cn10k_mldev->roc, op->output[0]->addr));
	req->cn10k_req.jd.model_run.num_batches = op->nb_batches;
}

static int
cn10k_ml_xstats_init(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	uint16_t nb_stats;
	uint16_t stat_id;
	uint16_t model;
	uint16_t i;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	/* Allocate memory for xstats entries. Don't allocate during reconfigure */
	nb_stats = RTE_DIM(device_xstats) + ML_CNXK_MAX_MODELS * RTE_DIM(layer_xstats);
	if (cn10k_mldev->xstats.entries == NULL)
		cn10k_mldev->xstats.entries = rte_zmalloc(
			"cn10k_ml_xstats", sizeof(struct cnxk_ml_xstats_entry) * nb_stats,
			PLT_CACHE_LINE_SIZE);

	if (cn10k_mldev->xstats.entries == NULL)
		return -ENOMEM;

	/* Initialize device xstats */
	stat_id = 0;
	for (i = 0; i < RTE_DIM(device_xstats); i++) {
		cn10k_mldev->xstats.entries[stat_id].map.id = stat_id;
		snprintf(cn10k_mldev->xstats.entries[stat_id].map.name,
			 sizeof(cn10k_mldev->xstats.entries[stat_id].map.name), "%s",
			 device_xstats[i].name);

		cn10k_mldev->xstats.entries[stat_id].mode = RTE_ML_DEV_XSTATS_DEVICE;
		cn10k_mldev->xstats.entries[stat_id].type = device_xstats[i].type;
		cn10k_mldev->xstats.entries[stat_id].fn_id = CNXK_ML_XSTATS_FN_DEVICE;
		cn10k_mldev->xstats.entries[stat_id].obj_idx = 0;
		cn10k_mldev->xstats.entries[stat_id].reset_allowed = device_xstats[i].reset_allowed;
		stat_id++;
	}
	cn10k_mldev->xstats.count_mode_device = stat_id;

	/* Initialize model xstats */
	for (model = 0; model < ML_CNXK_MAX_MODELS; model++) {
		cn10k_mldev->xstats.offset_for_model[model] = stat_id;

		for (i = 0; i < RTE_DIM(layer_xstats); i++) {
			cn10k_mldev->xstats.entries[stat_id].map.id = stat_id;
			cn10k_mldev->xstats.entries[stat_id].mode = RTE_ML_DEV_XSTATS_MODEL;
			cn10k_mldev->xstats.entries[stat_id].type = layer_xstats[i].type;
			cn10k_mldev->xstats.entries[stat_id].fn_id = CNXK_ML_XSTATS_FN_MODEL;
			cn10k_mldev->xstats.entries[stat_id].obj_idx = model;
			cn10k_mldev->xstats.entries[stat_id].reset_allowed =
				layer_xstats[i].reset_allowed;

			/* Name of xstat is updated during model load */
			snprintf(cn10k_mldev->xstats.entries[stat_id].map.name,
				 sizeof(cn10k_mldev->xstats.entries[stat_id].map.name),
				 "Model-%u-%s", model, layer_xstats[i].name);

			stat_id++;
		}

		cn10k_mldev->xstats.count_per_model[model] = RTE_DIM(layer_xstats);
	}

	cn10k_mldev->xstats.count_mode_model = stat_id - cn10k_mldev->xstats.count_mode_device;
	cn10k_mldev->xstats.count = stat_id;

	return 0;
}

static void
cn10k_ml_xstats_uninit(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	rte_free(cn10k_mldev->xstats.entries);
	cn10k_mldev->xstats.entries = NULL;

	cn10k_mldev->xstats.count = 0;
}

static void
cn10k_ml_xstats_model_name_update(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	uint16_t rclk_freq;
	uint16_t sclk_freq;
	uint16_t stat_id;
	char suffix[8];
	uint16_t i;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	model = dev->data->models[model_id];
	stat_id = RTE_DIM(device_xstats) + model_id * RTE_DIM(layer_xstats);

	roc_clk_freq_get(&rclk_freq, &sclk_freq);
	if (sclk_freq == 0)
		strcpy(suffix, "cycles");
	else
		strcpy(suffix, "ns");

	/* Update xstat name based on model name and sclk availability */
	for (i = 0; i < RTE_DIM(layer_xstats); i++) {
		snprintf(cn10k_mldev->xstats.entries[stat_id].map.name,
			 sizeof(cn10k_mldev->xstats.entries[stat_id].map.name), "%s-%s-%s",
			 model->layer[0].glow.metadata.model.name, layer_xstats[i].name, suffix);
		stat_id++;
	}
}

static uint64_t
cn10k_ml_dev_xstat_get(struct rte_ml_dev *dev, uint16_t obj_idx __rte_unused,
		       enum cnxk_ml_xstats_type type)
{
	struct cnxk_ml_dev *cnxk_mldev;

	cnxk_mldev = dev->data->dev_private;

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

#define ML_AVG_FOREACH_QP(dev, model, qp_id, str, value, count)                                    \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			value += model->layer[0].glow.burst_xstats[qp_id].str##_latency_tot;       \
			count += model->layer[0].glow.burst_xstats[qp_id].dequeued_count -         \
				 model->layer[0].glow.burst_xstats[qp_id].str##_reset_count;       \
		}                                                                                  \
		if (count != 0)                                                                    \
			value = value / count;                                                     \
	} while (0)

#define ML_MIN_FOREACH_QP(dev, model, qp_id, str, value, count)                                    \
	do {                                                                                       \
		value = UINT64_MAX;                                                                \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			value = PLT_MIN(                                                           \
				value,                                                             \
				model->layer[0].glow.burst_xstats[qp_id].str##_latency_min);       \
			count += model->layer[0].glow.burst_xstats[qp_id].dequeued_count -         \
				 model->layer[0].glow.burst_xstats[qp_id].str##_reset_count;       \
		}                                                                                  \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

#define ML_MAX_FOREACH_QP(dev, model, qp_id, str, value, count)                                    \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			value = PLT_MAX(                                                           \
				value,                                                             \
				model->layer[0].glow.burst_xstats[qp_id].str##_latency_max);       \
			count += model->layer[0].glow.burst_xstats[qp_id].dequeued_count -         \
				 model->layer[0].glow.burst_xstats[qp_id].str##_reset_count;       \
		}                                                                                  \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

static uint64_t
cn10k_ml_model_xstat_get(struct rte_ml_dev *dev, uint16_t obj_idx, enum cnxk_ml_xstats_type type)
{
	struct cnxk_ml_model *model;
	uint16_t rclk_freq; /* MHz */
	uint16_t sclk_freq; /* MHz */
	uint64_t count = 0;
	uint64_t value;
	uint32_t qp_id;

	model = dev->data->models[obj_idx];
	if (model == NULL)
		return 0;

	switch (type) {
	case avg_hw_latency:
		ML_AVG_FOREACH_QP(dev, model, qp_id, hw, value, count);
		break;
	case min_hw_latency:
		ML_MIN_FOREACH_QP(dev, model, qp_id, hw, value, count);
		break;
	case max_hw_latency:
		ML_MAX_FOREACH_QP(dev, model, qp_id, hw, value, count);
		break;
	case avg_fw_latency:
		ML_AVG_FOREACH_QP(dev, model, qp_id, fw, value, count);
		break;
	case min_fw_latency:
		ML_MIN_FOREACH_QP(dev, model, qp_id, fw, value, count);
		break;
	case max_fw_latency:
		ML_MAX_FOREACH_QP(dev, model, qp_id, fw, value, count);
		break;
	default:
		value = 0;
	}

	roc_clk_freq_get(&rclk_freq, &sclk_freq);
	if (sclk_freq != 0) /* return in ns */
		value = (value * 1000ULL) / sclk_freq;

	return value;
}

static int
cn10k_ml_device_xstats_reset(struct rte_ml_dev *dev, const uint16_t stat_ids[], uint16_t nb_ids)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_xstats_entry *xs;
	struct cnxk_ml_dev *cnxk_mldev;
	uint16_t nb_stats;
	uint16_t stat_id;
	uint32_t i;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	if (stat_ids == NULL)
		nb_stats = cn10k_mldev->xstats.count_mode_device;
	else
		nb_stats = nb_ids;

	for (i = 0; i < nb_stats; i++) {
		if (stat_ids == NULL)
			stat_id = i;
		else
			stat_id = stat_ids[i];

		if (stat_id >= cn10k_mldev->xstats.count_mode_device)
			return -EINVAL;

		xs = &cn10k_mldev->xstats.entries[stat_id];
		if (!xs->reset_allowed)
			continue;

		xs->reset_value = cn10k_ml_dev_xstat_get(dev, xs->obj_idx, xs->type);
	}

	return 0;
}

#define ML_AVG_RESET_FOREACH_QP(dev, model, qp_id, str)                                            \
	do {                                                                                       \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			model->layer[0].glow.burst_xstats[qp_id].str##_latency_tot = 0;            \
			model->layer[0].glow.burst_xstats[qp_id].str##_reset_count =               \
				model->layer[0].glow.burst_xstats[qp_id].dequeued_count;           \
		}                                                                                  \
	} while (0)

#define ML_MIN_RESET_FOREACH_QP(dev, model, qp_id, str)                                            \
	do {                                                                                       \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++)                        \
			model->layer[0].glow.burst_xstats[qp_id].str##_latency_min = UINT64_MAX;   \
	} while (0)

#define ML_MAX_RESET_FOREACH_QP(dev, model, qp_id, str)                                            \
	do {                                                                                       \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++)                        \
			model->layer[0].glow.burst_xstats[qp_id].str##_latency_max = 0;            \
	} while (0)

static void
cn10k_ml_reset_model_stat(struct rte_ml_dev *dev, uint16_t model_id, enum cnxk_ml_xstats_type type)
{
	struct cnxk_ml_model *model;
	uint32_t qp_id;

	model = dev->data->models[model_id];

	switch (type) {
	case avg_hw_latency:
		ML_AVG_RESET_FOREACH_QP(dev, model, qp_id, hw);
		break;
	case min_hw_latency:
		ML_MIN_RESET_FOREACH_QP(dev, model, qp_id, hw);
		break;
	case max_hw_latency:
		ML_MAX_RESET_FOREACH_QP(dev, model, qp_id, hw);
		break;
	case avg_fw_latency:
		ML_AVG_RESET_FOREACH_QP(dev, model, qp_id, fw);
		break;
	case min_fw_latency:
		ML_MIN_RESET_FOREACH_QP(dev, model, qp_id, fw);
		break;
	case max_fw_latency:
		ML_MAX_RESET_FOREACH_QP(dev, model, qp_id, fw);
		break;
	default:
		return;
	}
}

static int
cn10k_ml_model_xstats_reset(struct rte_ml_dev *dev, int32_t model_id, const uint16_t stat_ids[],
			    uint16_t nb_ids)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_xstats_entry *xs;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	int32_t lcl_model_id = 0;
	uint16_t start_id;
	uint16_t end_id;
	int32_t i;
	int32_t j;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	for (i = 0; i < ML_CNXK_MAX_MODELS; i++) {
		if (model_id == -1) {
			model = dev->data->models[i];
			if (model == NULL) /* Skip inactive models */
				continue;
		} else {
			if (model_id != i)
				continue;

			model = dev->data->models[model_id];
			if (model == NULL) {
				plt_err("Invalid model_id = %d\n", model_id);
				return -EINVAL;
			}
		}

		start_id = cn10k_mldev->xstats.offset_for_model[i];
		end_id = cn10k_mldev->xstats.offset_for_model[i] +
			 cn10k_mldev->xstats.count_per_model[i] - 1;

		if (stat_ids == NULL) {
			for (j = start_id; j <= end_id; j++) {
				xs = &cn10k_mldev->xstats.entries[j];
				cn10k_ml_reset_model_stat(dev, i, xs->type);
			}
		} else {
			for (j = 0; j < nb_ids; j++) {
				if (stat_ids[j] < start_id || stat_ids[j] > end_id) {
					plt_err("Invalid stat_ids[%d] = %d for model_id = %d\n", j,
						stat_ids[j], lcl_model_id);
					return -EINVAL;
				}
				xs = &cn10k_mldev->xstats.entries[stat_ids[j]];
				cn10k_ml_reset_model_stat(dev, i, xs->type);
			}
		}
	}

	return 0;
}

static int
cn10k_ml_cache_model_data(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer)
{
	struct rte_ml_buff_seg seg[2];
	struct rte_ml_buff_seg *inp;
	struct rte_ml_buff_seg *out;
	struct rte_ml_op op;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	uint64_t isize = 0;
	uint64_t osize = 0;
	int ret = 0;
	uint32_t i;

	inp = &seg[0];
	out = &seg[1];

	/* Create input and output buffers. */
	for (i = 0; i < layer->info.nb_inputs; i++)
		isize += layer->info.input[i].sz_q;

	for (i = 0; i < layer->info.nb_outputs; i++)
		osize += layer->info.output[i].sz_q;

	isize = layer->batch_size * isize;
	osize = layer->batch_size * osize;

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", "ml_dummy_io", layer->index);
	mz = plt_memzone_reserve_aligned(str, isize + osize, 0, ML_CN10K_ALIGN_SIZE);
	if (mz == NULL)
		return -ENOMEM;
	memset(mz->addr, 0, isize + osize);

	seg[0].addr = mz->addr;
	seg[0].iova_addr = mz->iova;
	seg[0].length = isize;
	seg[0].next = NULL;

	seg[1].addr = PLT_PTR_ADD(mz->addr, isize);
	seg[1].iova_addr = mz->iova + isize;
	seg[1].length = osize;
	seg[1].next = NULL;

	op.model_id = layer->index;
	op.nb_batches = layer->batch_size;
	op.mempool = NULL;

	op.input = &inp;
	op.output = &out;

	memset(layer->glow.req, 0, sizeof(struct cnxk_ml_req));
	ret = cn10k_ml_inference_sync(cnxk_mldev, &op);
	plt_memzone_free(mz);

	return ret;
}

int
cn10k_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info)
{
	struct cn10k_ml_dev *cn10k_mldev;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	if (cn10k_mldev->hw_queue_lock)
		dev_info->max_queue_pairs = ML_CN10K_MAX_QP_PER_DEVICE_SL;
	else
		dev_info->max_queue_pairs = ML_CN10K_MAX_QP_PER_DEVICE_LF;

	dev_info->max_desc = ML_CN10K_MAX_DESC_PER_QP;
	dev_info->max_io = ML_CN10K_MAX_INPUT_OUTPUT;
	dev_info->max_segments = ML_CN10K_MAX_SEGMENTS;
	dev_info->align_size = ML_CN10K_ALIGN_SIZE;

	return 0;
}

int
cn10k_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cn10k_ml_ocm *ocm;
	uint16_t tile_id;
	int ret;

	RTE_SET_USED(conf);

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	ocm = &cn10k_mldev->ocm;
	ocm->num_tiles = ML_CN10K_OCM_NUMTILES;
	ocm->size_per_tile = ML_CN10K_OCM_TILESIZE;
	ocm->page_size = cn10k_mldev->ocm_page_size;
	ocm->num_pages = ocm->size_per_tile / ocm->page_size;
	ocm->mask_words = ocm->num_pages / (8 * sizeof(uint8_t));

	/* Allocate memory for ocm_mask */
	ocm->ocm_mask =
		rte_zmalloc("ocm_mask", ocm->mask_words * ocm->num_tiles, RTE_CACHE_LINE_SIZE);
	if (ocm->ocm_mask == NULL) {
		plt_err("Unable to allocate memory for OCM mask");
		return -ENOMEM;
	}

	for (tile_id = 0; tile_id < ocm->num_tiles; tile_id++) {
		ocm->tile_ocm_info[tile_id].ocm_mask = ocm->ocm_mask + tile_id * ocm->mask_words;
		ocm->tile_ocm_info[tile_id].last_wb_page = -1;
	}

	rte_spinlock_init(&ocm->lock);

	/* Initialize xstats */
	ret = cn10k_ml_xstats_init(cnxk_mldev->mldev);
	if (ret != 0) {
		plt_err("Failed to initialize xstats");
		return ret;
	}

	/* Set JCMDQ enqueue function */
	if (cn10k_mldev->hw_queue_lock == 1)
		cn10k_mldev->ml_jcmdq_enqueue = roc_ml_jcmdq_enqueue_sl;
	else
		cn10k_mldev->ml_jcmdq_enqueue = roc_ml_jcmdq_enqueue_lf;

	/* Set polling function pointers */
	cn10k_mldev->set_poll_addr = cn10k_ml_set_poll_addr;
	cn10k_mldev->set_poll_ptr = cn10k_ml_set_poll_ptr;
	cn10k_mldev->get_poll_ptr = cn10k_ml_get_poll_ptr;

	cnxk_mldev->mldev->enqueue_burst = cn10k_ml_enqueue_burst;
	cnxk_mldev->mldev->dequeue_burst = cn10k_ml_dequeue_burst;
	cnxk_mldev->mldev->op_error_get = cn10k_ml_op_error_get;

	return 0;
}

int
cn10k_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev)
{
	struct cn10k_ml_dev *cn10k_mldev;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	/* Release ocm_mask memory */
	rte_free(cn10k_mldev->ocm.ocm_mask);

	/* Un-initialize xstats */
	cn10k_ml_xstats_uninit(cnxk_mldev->mldev);

	/* Unload firmware */
	cn10k_ml_fw_unload(cnxk_mldev);

	/* Clear scratch registers */
	roc_ml_reg_write64(&cn10k_mldev->roc, 0, ML_SCRATCH_WORK_PTR);
	roc_ml_reg_write64(&cn10k_mldev->roc, 0, ML_SCRATCH_FW_CTRL);
	roc_ml_reg_write64(&cn10k_mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C0);
	roc_ml_reg_write64(&cn10k_mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C0);
	roc_ml_reg_write64(&cn10k_mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C1);
	roc_ml_reg_write64(&cn10k_mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C1);

	/* Reset ML_MLR_BASE */
	roc_ml_reg_write64(&cn10k_mldev->roc, 0, ML_MLR_BASE);
	plt_ml_dbg("ML_MLR_BASE = 0x%016lx", roc_ml_reg_read64(&cn10k_mldev->roc, ML_MLR_BASE));

	return 0;
}

int
cn10k_ml_dev_start(struct cnxk_ml_dev *cnxk_mldev)
{
	struct cn10k_ml_dev *cn10k_mldev;
	uint64_t reg_val64;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	reg_val64 = roc_ml_reg_read64(&cn10k_mldev->roc, ML_CFG);
	reg_val64 |= ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&cn10k_mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&cn10k_mldev->roc, ML_CFG));

	return 0;
}

int
cn10k_ml_dev_stop(struct cnxk_ml_dev *cnxk_mldev)
{
	struct cn10k_ml_dev *cn10k_mldev;
	uint64_t reg_val64;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	reg_val64 = roc_ml_reg_read64(&cn10k_mldev->roc, ML_CFG);
	reg_val64 &= ~ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&cn10k_mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&cn10k_mldev->roc, ML_CFG));

	return 0;
}

int
cn10k_ml_dev_stats_get(struct rte_ml_dev *dev, struct rte_ml_dev_stats *stats)
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

void
cn10k_ml_dev_stats_reset(struct rte_ml_dev *dev)
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

int
cn10k_ml_dev_xstats_names_get(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode,
			      int32_t model_id, struct rte_ml_dev_xstats_map *xstats_map,
			      uint32_t size)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	uint32_t xstats_mode_count;
	uint32_t idx = 0;
	uint32_t i;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	xstats_mode_count = 0;
	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		xstats_mode_count = cn10k_mldev->xstats.count_mode_device;
		break;
	case RTE_ML_DEV_XSTATS_MODEL:
		if (model_id >= ML_CNXK_MAX_MODELS)
			break;
		xstats_mode_count = cn10k_mldev->xstats.count_per_model[model_id];
		break;
	default:
		return -EINVAL;
	};

	if (xstats_mode_count > size || xstats_map == NULL)
		return xstats_mode_count;

	for (i = 0; i < cn10k_mldev->xstats.count && idx < size; i++) {
		if (cn10k_mldev->xstats.entries[i].mode != mode)
			continue;

		if (mode != RTE_ML_DEV_XSTATS_DEVICE &&
		    model_id != cn10k_mldev->xstats.entries[i].obj_idx)
			continue;

		rte_strscpy(xstats_map[idx].name, cn10k_mldev->xstats.entries[i].map.name,
			    RTE_ML_STR_MAX);
		xstats_map[idx].id = cn10k_mldev->xstats.entries[i].map.id;
		idx++;
	}

	return idx;
}

int
cn10k_ml_dev_xstats_by_name_get(struct rte_ml_dev *dev, const char *name, uint16_t *stat_id,
				uint64_t *value)
{
	struct cnxk_ml_xstats_entry *xs;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	cnxk_ml_xstats_fn fn;
	uint32_t i;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	for (i = 0; i < cn10k_mldev->xstats.count; i++) {
		xs = &cn10k_mldev->xstats.entries[i];
		if (strncmp(xs->map.name, name, RTE_ML_STR_MAX) == 0) {
			if (stat_id != NULL)
				*stat_id = xs->map.id;

			switch (xs->fn_id) {
			case CNXK_ML_XSTATS_FN_DEVICE:
				fn = cn10k_ml_dev_xstat_get;
				break;
			case CNXK_ML_XSTATS_FN_MODEL:
				fn = cn10k_ml_model_xstat_get;
				break;
			default:
				plt_err("Unexpected xstat fn_id = %d", xs->fn_id);
				return -EINVAL;
			}

			*value = fn(dev, xs->obj_idx, xs->type) - xs->reset_value;

			return 0;
		}
	}

	if (stat_id != NULL)
		*stat_id = (uint16_t)-1;

	return -EINVAL;
}

int
cn10k_ml_dev_xstats_get(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
			const uint16_t stat_ids[], uint64_t values[], uint16_t nb_ids)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_xstats_entry *xs;
	struct cnxk_ml_dev *cnxk_mldev;
	uint32_t xstats_mode_count;
	cnxk_ml_xstats_fn fn;
	uint64_t val;
	uint32_t idx;
	uint32_t i;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	xstats_mode_count = 0;

	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		xstats_mode_count = cn10k_mldev->xstats.count_mode_device;
		break;
	case RTE_ML_DEV_XSTATS_MODEL:
		if (model_id >= ML_CNXK_MAX_MODELS)
			return -EINVAL;
		xstats_mode_count = cn10k_mldev->xstats.count_per_model[model_id];
		break;
	default:
		return -EINVAL;
	};

	idx = 0;
	for (i = 0; i < nb_ids && idx < xstats_mode_count; i++) {
		xs = &cn10k_mldev->xstats.entries[stat_ids[i]];
		if (stat_ids[i] > cn10k_mldev->xstats.count || xs->mode != mode)
			continue;

		if (mode == RTE_ML_DEV_XSTATS_MODEL && model_id != xs->obj_idx) {
			plt_err("Invalid stats_id[%d] = %d for model_id = %d\n", i, stat_ids[i],
				model_id);
			return -EINVAL;
		}

		switch (xs->fn_id) {
		case CNXK_ML_XSTATS_FN_DEVICE:
			fn = cn10k_ml_dev_xstat_get;
			break;
		case CNXK_ML_XSTATS_FN_MODEL:
			fn = cn10k_ml_model_xstat_get;
			break;
		default:
			plt_err("Unexpected xstat fn_id = %d", xs->fn_id);
			return -EINVAL;
		}

		val = fn(dev, xs->obj_idx, xs->type);
		if (values)
			values[idx] = val;

		idx++;
	}

	return idx;
}

int
cn10k_ml_dev_xstats_reset(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode,
			  int32_t model_id, const uint16_t stat_ids[], uint16_t nb_ids)
{
	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		return cn10k_ml_device_xstats_reset(dev, stat_ids, nb_ids);
	case RTE_ML_DEV_XSTATS_MODEL:
		return cn10k_ml_model_xstats_reset(dev, model_id, stat_ids, nb_ids);
	};

	return 0;
}

int
cn10k_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cn10k_ml_fw *fw;

	uint32_t head_loc;
	uint32_t tail_loc;
	uint32_t bufsize;
	char *head_ptr;
	int core_id;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	fw = &cn10k_mldev->fw;

	/* Dump OCM state */
	cn10k_ml_ocm_print(cnxk_mldev, fp);

	if (roc_env_is_asim())
		return 0;

	/* Dump debug buffer */
	for (core_id = 0; core_id <= 1; core_id++) {
		bufsize = fw->req->cn10k_req.jd.fw_load.debug.debug_buffer_size;
		if (core_id == 0) {
			head_loc =
				roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_DBG_BUFFER_HEAD_C0);
			tail_loc =
				roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_DBG_BUFFER_TAIL_C0);
			head_ptr =
				PLT_PTR_CAST(fw->req->cn10k_req.jd.fw_load.debug.core0_debug_ptr);
			head_ptr = roc_ml_addr_mlip2ap(&cn10k_mldev->roc, head_ptr);
		} else {
			head_loc =
				roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_DBG_BUFFER_HEAD_C1);
			tail_loc =
				roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_DBG_BUFFER_TAIL_C1);
			head_ptr =
				PLT_PTR_CAST(fw->req->cn10k_req.jd.fw_load.debug.core1_debug_ptr);
			head_ptr = roc_ml_addr_mlip2ap(&cn10k_mldev->roc, head_ptr);
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
		bufsize = fw->req->cn10k_req.jd.fw_load.debug.exception_state_size;
		if ((core_id == 0) &&
		    (roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_EXCEPTION_SP_C0) != 0)) {
			head_ptr = PLT_PTR_CAST(
				fw->req->cn10k_req.jd.fw_load.debug.core0_exception_buffer);
			fprintf(fp, "ML_SCRATCH_EXCEPTION_SP_C0 = 0x%016lx",
				roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_EXCEPTION_SP_C0));
			head_ptr = roc_ml_addr_mlip2ap(&cn10k_mldev->roc, head_ptr);
			fprintf(fp, "%.*s", bufsize, head_ptr);
		} else if ((core_id == 1) && (roc_ml_reg_read64(&cn10k_mldev->roc,
								ML_SCRATCH_EXCEPTION_SP_C1) != 0)) {
			head_ptr = PLT_PTR_CAST(
				fw->req->cn10k_req.jd.fw_load.debug.core1_exception_buffer);
			fprintf(fp, "ML_SCRATCH_EXCEPTION_SP_C1 = 0x%016lx",
				roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_EXCEPTION_SP_C1));
			head_ptr = roc_ml_addr_mlip2ap(&cn10k_mldev->roc, head_ptr);
			fprintf(fp, "%.*s", bufsize, head_ptr);
		}
	}

	return 0;
}

int
cn10k_ml_dev_selftest(struct cnxk_ml_dev *cnxk_mldev)
{
	struct cn10k_ml_dev *cn10k_mldev;
	const struct plt_memzone *mz;
	struct cnxk_ml_req *req;
	uint64_t timeout_cycle;
	bool timeout;
	int ret;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	mz = plt_memzone_reserve_aligned("dev_selftest", sizeof(struct cnxk_ml_req), 0,
					 ML_CN10K_ALIGN_SIZE);
	if (mz == NULL) {
		plt_err("Could not allocate reserved memzone");
		return -ENOMEM;
	}
	req = mz->addr;

	/* Prepare load completion structure */
	memset(&req->cn10k_req.jd, 0, sizeof(struct cn10k_ml_jd));
	req->cn10k_req.jd.hdr.jce.w1.u64 = PLT_U64_CAST(&req->cn10k_req.status);
	req->cn10k_req.jd.hdr.job_type = ML_CN10K_JOB_TYPE_FIRMWARE_SELFTEST;
	req->cn10k_req.jd.hdr.result =
		roc_ml_addr_ap2mlip(&cn10k_mldev->roc, &req->cn10k_req.result);
	req->cn10k_req.jd.fw_load.flags = cn10k_ml_fw_flags_get(&cn10k_mldev->fw);
	plt_write64(ML_CNXK_POLL_JOB_START, &req->cn10k_req.status);
	plt_wmb();

	/* Enqueue firmware selftest request through scratch registers */
	timeout = true;
	timeout_cycle = plt_tsc_cycles() + ML_CNXK_CMD_TIMEOUT * plt_tsc_hz();
	roc_ml_scratch_enqueue(&cn10k_mldev->roc, &req->cn10k_req.jd);

	plt_rmb();
	do {
		if (roc_ml_scratch_is_done_bit_set(&cn10k_mldev->roc) &&
		    (plt_read64(&req->cn10k_req.status) == ML_CNXK_POLL_JOB_FINISH)) {
			timeout = false;
			break;
		}
	} while (plt_tsc_cycles() < timeout_cycle);

	/* Check firmware selftest status, clean-up and exit */
	ret = 0;
	if (timeout) {
		ret = -ETIME;
	} else {
		if (req->cn10k_req.result.error_code != 0)
			ret = -1;
	}

	plt_memzone_free(mz);

	return ret;
}

int
cn10k_ml_layer_load(void *device, uint16_t model_id, const char *layer_name, uint8_t *buffer,
		    size_t size, uint16_t *index)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	size_t layer_object_size = 0;
	size_t layer_scratch_size;
	size_t layer_xstats_size;
	uint8_t *base_dma_addr;
	uint16_t scratch_pages;
	uint16_t layer_id = 0;
	uint16_t wb_pages;
	uint64_t mz_size;
	uint16_t idx;
	int qp_id;
	int ret;

	PLT_SET_USED(size);
	PLT_SET_USED(layer_name);

	cnxk_mldev = (struct cnxk_ml_dev *)device;
	if (cnxk_mldev == NULL) {
		plt_err("Invalid device = %p", device);
		return -EINVAL;
	}

	model = cnxk_mldev->mldev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	layer = &model->layer[layer_id];

	ret = cn10k_ml_model_metadata_check(buffer, size);
	if (ret != 0)
		return ret;

	/* Get index */
	for (idx = 0; idx < cnxk_mldev->max_nb_layers; idx++) {
		if (!cnxk_mldev->index_map[idx].active) {
			layer->index = idx;
			break;
		}
	}

	if (idx >= cnxk_mldev->max_nb_layers) {
		plt_err("No slots available for model layers, model_id = %u, layer_id = %u",
			model->model_id, layer_id);
		return -1;
	}

	layer->model = model;

	/* Get WB and scratch pages, check if model can be loaded. */
	ret = cn10k_ml_model_ocm_pages_count(cnxk_mldev, layer, buffer, &wb_pages, &scratch_pages);
	if (ret < 0)
		return ret;

	/* Compute layer memzone size */
	metadata = (struct cn10k_ml_model_metadata *)buffer;
	layer_object_size = metadata->init_model.file_size + metadata->main_model.file_size +
			    metadata->finish_model.file_size + metadata->weights_bias.file_size;
	layer_object_size = PLT_ALIGN_CEIL(layer_object_size, ML_CN10K_ALIGN_SIZE);
	layer_scratch_size = PLT_ALIGN_CEIL(metadata->model.ddr_scratch_range_end -
						    metadata->model.ddr_scratch_range_start + 1,
					    ML_CN10K_ALIGN_SIZE);
	layer_xstats_size = (cnxk_mldev->mldev->data->nb_queue_pairs + 1) *
			    sizeof(struct cn10k_ml_layer_xstats);

	/* Allocate memzone for model data */
	mz_size = layer_object_size + layer_scratch_size +
		  PLT_ALIGN_CEIL(sizeof(struct cnxk_ml_req), ML_CN10K_ALIGN_SIZE) +
		  layer_xstats_size;
	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u_%u", CN10K_ML_LAYER_MEMZONE_NAME,
		 model->model_id, layer_id);
	mz = plt_memzone_reserve_aligned(str, mz_size, 0, ML_CN10K_ALIGN_SIZE);
	if (!mz) {
		plt_err("plt_memzone_reserve failed : %s", str);
		return -ENOMEM;
	}

	/* Copy metadata to internal buffer */
	rte_memcpy(&layer->glow.metadata, buffer, sizeof(struct cn10k_ml_model_metadata));
	cn10k_ml_model_metadata_update(&layer->glow.metadata);

	/* Set layer name */
	rte_memcpy(layer->name, layer->glow.metadata.model.name, MRVL_ML_MODEL_NAME_LEN);

	/* Enable support for batch_size of 256 */
	if (layer->glow.metadata.model.batch_size == 0)
		layer->batch_size = 256;
	else
		layer->batch_size = layer->glow.metadata.model.batch_size;

	/* Set DMA base address */
	base_dma_addr = mz->addr;
	cn10k_ml_layer_addr_update(layer, buffer, base_dma_addr);

	/* Set scratch base address */
	layer->glow.addr.scratch_base_addr = PLT_PTR_ADD(base_dma_addr, layer_object_size);

	/* Update internal I/O data structure */
	cn10k_ml_layer_io_info_set(&layer->info, &layer->glow.metadata);

	/* Initialize model_mem_map */
	memset(&layer->glow.ocm_map, 0, sizeof(struct cn10k_ml_ocm_layer_map));
	layer->glow.ocm_map.ocm_reserved = false;
	layer->glow.ocm_map.tilemask = 0;
	layer->glow.ocm_map.wb_page_start = -1;
	layer->glow.ocm_map.wb_pages = wb_pages;
	layer->glow.ocm_map.scratch_pages = scratch_pages;

	/* Set slow-path request address and state */
	layer->glow.req = PLT_PTR_ADD(mz->addr, layer_object_size + layer_scratch_size);

	/* Reset burst and sync stats */
	layer->glow.burst_xstats = PLT_PTR_ADD(
		layer->glow.req, PLT_ALIGN_CEIL(sizeof(struct cnxk_ml_req), ML_CN10K_ALIGN_SIZE));
	for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs + 1; qp_id++) {
		layer->glow.burst_xstats[qp_id].hw_latency_tot = 0;
		layer->glow.burst_xstats[qp_id].hw_latency_min = UINT64_MAX;
		layer->glow.burst_xstats[qp_id].hw_latency_max = 0;
		layer->glow.burst_xstats[qp_id].fw_latency_tot = 0;
		layer->glow.burst_xstats[qp_id].fw_latency_min = UINT64_MAX;
		layer->glow.burst_xstats[qp_id].fw_latency_max = 0;
		layer->glow.burst_xstats[qp_id].hw_reset_count = 0;
		layer->glow.burst_xstats[qp_id].fw_reset_count = 0;
		layer->glow.burst_xstats[qp_id].dequeued_count = 0;
	}

	layer->glow.sync_xstats =
		PLT_PTR_ADD(layer->glow.burst_xstats, cnxk_mldev->mldev->data->nb_queue_pairs *
							      sizeof(struct cn10k_ml_layer_xstats));

	/* Update xstats names */
	cn10k_ml_xstats_model_name_update(cnxk_mldev->mldev, idx);

	layer->state = ML_CNXK_LAYER_STATE_LOADED;
	cnxk_mldev->index_map[idx].model_id = model->model_id;
	cnxk_mldev->index_map[idx].layer_id = layer_id;
	cnxk_mldev->index_map[idx].active = true;
	*index = idx;

	return 0;
}

int
cn10k_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
		    struct cnxk_ml_model *model)
{
	struct cnxk_ml_layer *layer;
	int ret;

	/* Metadata check */
	ret = cn10k_ml_model_metadata_check(params->addr, params->size);
	if (ret != 0)
		return ret;

	/* Copy metadata to internal buffer */
	rte_memcpy(&model->glow.metadata, params->addr, sizeof(struct cn10k_ml_model_metadata));
	cn10k_ml_model_metadata_update(&model->glow.metadata);

	/* Set model name */
	rte_memcpy(model->name, (char *)model->glow.metadata.model.name, 64);

	/* Enable support for batch_size of 256 */
	if (model->glow.metadata.model.batch_size == 0)
		model->batch_size = 256;
	else
		model->batch_size = model->glow.metadata.model.batch_size;

	/* Since the number of layers that the driver would be handling for glow models is
	 * always 1. consider the entire model as a model with single layer. This would
	 * ignore the num_layers from metadata.
	 */
	model->nb_layers = 1;

	/* Load layer and get the index */
	layer = &model->layer[0];
	ret = cn10k_ml_layer_load(cnxk_mldev, model->model_id, NULL, params->addr, params->size,
				  &layer->index);
	if (ret != 0) {
		plt_err("Model layer load failed: model_id = %u, layer_id = %u", model->model_id,
			0);
		return ret;
	}

	cn10k_ml_model_info_set(cnxk_mldev, model, &model->layer[0].info, &model->glow.metadata);

	return 0;
}

int
cn10k_ml_layer_unload(void *device, uint16_t model_id, const char *layer_name)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;

	char str[RTE_MEMZONE_NAMESIZE];
	uint16_t layer_id = 0;
	int ret;

	PLT_SET_USED(layer_name);

	cnxk_mldev = (struct cnxk_ml_dev *)device;
	if (cnxk_mldev == NULL) {
		plt_err("Invalid device = %p", device);
		return -EINVAL;
	}

	model = cnxk_mldev->mldev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	layer = &model->layer[layer_id];

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u_%u", CN10K_ML_LAYER_MEMZONE_NAME,
		 model->model_id, layer_id);
	ret = plt_memzone_free(plt_memzone_lookup(str));

	layer->state = ML_CNXK_LAYER_STATE_UNKNOWN;
	cnxk_mldev->index_map[layer->index].active = false;

	return ret;
}

int
cn10k_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	return cn10k_ml_layer_unload(cnxk_mldev, model->model_id, NULL);
}

int
cn10k_ml_layer_start(void *device, uint16_t model_id, const char *layer_name)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	struct cn10k_ml_ocm *ocm;
	struct cnxk_ml_req *req;

	uint16_t layer_id = 0;
	bool job_enqueued;
	bool job_dequeued;
	uint8_t num_tiles;
	uint64_t tilemask;
	int wb_page_start;
	int tile_start;
	int tile_end;
	bool locked;
	int ret = 0;

	PLT_SET_USED(layer_name);

	cnxk_mldev = (struct cnxk_ml_dev *)device;
	if (cnxk_mldev == NULL) {
		plt_err("Invalid device = %p", device);
		return -EINVAL;
	}

	model = cnxk_mldev->mldev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	layer = &model->layer[layer_id];
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	ocm = &cn10k_mldev->ocm;

	/* Prepare JD */
	req = layer->glow.req;
	cn10k_ml_prep_sp_job_descriptor(cnxk_mldev, layer, req, ML_CN10K_JOB_TYPE_MODEL_START);
	req->cn10k_req.result.error_code = 0x0;
	req->cn10k_req.result.user_ptr = NULL;

	plt_write64(ML_CNXK_POLL_JOB_START, &req->cn10k_req.status);
	plt_wmb();

	num_tiles = layer->glow.metadata.model.tile_end - layer->glow.metadata.model.tile_start + 1;

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			if (layer->state == ML_CNXK_LAYER_STATE_STARTED) {
				plt_ml_dbg("Layer already started, model_id = %u, layer_id = %u",
					   model->model_id, layer_id);
				plt_spinlock_unlock(&model->lock);
				return 1;
			}

			if (layer->state == ML_CNXK_LAYER_STATE_JOB_ACTIVE) {
				plt_err("A slow-path job is active for the model_id = %u",
					model->model_id);
				plt_spinlock_unlock(&model->lock);
				return -EBUSY;
			}

			layer->state = ML_CNXK_LAYER_STATE_JOB_ACTIVE;
			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	while (!layer->glow.ocm_map.ocm_reserved) {
		if (plt_spinlock_trylock(&ocm->lock) != 0) {
			wb_page_start = cn10k_ml_ocm_tilemask_find(
				cnxk_mldev, num_tiles, layer->glow.ocm_map.wb_pages,
				layer->glow.ocm_map.scratch_pages, &tilemask);

			if (wb_page_start == -1) {
				plt_err("Free pages not available on OCM tiles");
				plt_err("Failed to start layer, model_id = %u, layer_id = %u",
					model->model_id, layer_id);
				plt_spinlock_unlock(&ocm->lock);
				return -ENOMEM;
			}

			layer->glow.ocm_map.tilemask = tilemask;
			layer->glow.ocm_map.wb_page_start = wb_page_start;

			cn10k_ml_ocm_reserve_pages(
				cnxk_mldev, model->model_id, layer_id, layer->glow.ocm_map.tilemask,
				layer->glow.ocm_map.wb_page_start, layer->glow.ocm_map.wb_pages,
				layer->glow.ocm_map.scratch_pages);
			layer->glow.ocm_map.ocm_reserved = true;
			plt_spinlock_unlock(&ocm->lock);
		}
	}

	/* Update JD */
	cn10k_ml_ocm_tilecount(layer->glow.ocm_map.tilemask, &tile_start, &tile_end);
	req->cn10k_req.jd.model_start.tilemask = GENMASK_ULL(tile_end, tile_start);
	req->cn10k_req.jd.model_start.ocm_wb_base_address =
		layer->glow.ocm_map.wb_page_start * ocm->page_size;

	job_enqueued = false;
	job_dequeued = false;
	do {
		if (!job_enqueued) {
			req->timeout = plt_tsc_cycles() + ML_CNXK_CMD_TIMEOUT * plt_tsc_hz();
			job_enqueued =
				roc_ml_scratch_enqueue(&cn10k_mldev->roc, &req->cn10k_req.jd);
		}

		if (job_enqueued && !job_dequeued)
			job_dequeued =
				roc_ml_scratch_dequeue(&cn10k_mldev->roc, &req->cn10k_req.jd);

		if (job_dequeued)
			break;
	} while (plt_tsc_cycles() < req->timeout);

	if (job_dequeued) {
		if (plt_read64(&req->cn10k_req.status) == ML_CNXK_POLL_JOB_FINISH) {
			if (req->cn10k_req.result.error_code == 0)
				ret = 0;
			else
				ret = -1;
		}
	} else { /* Reset scratch registers */
		roc_ml_scratch_queue_reset(&cn10k_mldev->roc);
		ret = -ETIME;
	}

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			if (ret == 0)
				layer->state = ML_CNXK_LAYER_STATE_STARTED;
			else
				layer->state = ML_CNXK_LAYER_STATE_UNKNOWN;

			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	if (layer->state == ML_CNXK_LAYER_STATE_UNKNOWN) {
		while (layer->glow.ocm_map.ocm_reserved) {
			if (plt_spinlock_trylock(&ocm->lock) != 0) {
				cn10k_ml_ocm_free_pages(cnxk_mldev, model->model_id, layer_id);
				layer->glow.ocm_map.ocm_reserved = false;
				layer->glow.ocm_map.tilemask = 0x0;
				plt_spinlock_unlock(&ocm->lock);
			}
		}
	}

	if (ret < 0) {
		cn10k_ml_layer_stop(device, model_id, layer_name);
	} else {
		if (cn10k_mldev->cache_model_data)
			ret = cn10k_ml_cache_model_data(cnxk_mldev, layer);
	}

	return ret;
}

int
cn10k_ml_model_start(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct cnxk_ml_layer *layer;
	int ret;

	layer = &model->layer[0];
	ret = cn10k_ml_layer_start(cnxk_mldev, model->model_id, layer->name);
	if (ret != 0) {
		plt_err("CN10K Model start failed, model_id = %u, error = %d", model->model_id,
			ret);
		return ret;
	}

	cnxk_mldev->nb_models_started++;
	model->state = ML_CNXK_MODEL_STATE_STARTED;

	return 0;
}

int
cn10k_ml_layer_stop(void *device, uint16_t model_id, const char *layer_name)
{
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	struct cn10k_ml_ocm *ocm;
	struct cnxk_ml_req *req;

	uint16_t layer_id = 0;
	bool job_enqueued;
	bool job_dequeued;
	bool locked;
	int ret = 0;

	PLT_SET_USED(layer_name);

	cnxk_mldev = (struct cnxk_ml_dev *)device;
	if (cnxk_mldev == NULL) {
		plt_err("Invalid device = %p", device);
		return -EINVAL;
	}

	model = cnxk_mldev->mldev->data->models[model_id];
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	layer = &model->layer[layer_id];
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	ocm = &cn10k_mldev->ocm;

	/* Prepare JD */
	req = layer->glow.req;
	cn10k_ml_prep_sp_job_descriptor(cnxk_mldev, layer, req, ML_CN10K_JOB_TYPE_MODEL_STOP);
	req->cn10k_req.result.error_code = 0x0;
	req->cn10k_req.result.user_ptr = NULL;

	plt_write64(ML_CNXK_POLL_JOB_START, &req->cn10k_req.status);
	plt_wmb();

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			if (layer->state == ML_CNXK_LAYER_STATE_LOADED) {
				plt_ml_dbg("Layer not started, model_id = %u, layer_id = %u",
					   model->model_id, layer_id);
				plt_spinlock_unlock(&model->lock);
				return 1;
			}

			if (layer->state == ML_CNXK_LAYER_STATE_JOB_ACTIVE) {
				plt_err("A slow-path job is active for the layer, model_id = %u, layer_id = %u",
					model->model_id, layer_id);
				plt_spinlock_unlock(&model->lock);
				return -EBUSY;
			}

			layer->state = ML_CNXK_LAYER_STATE_JOB_ACTIVE;
			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	while (layer->glow.ocm_map.ocm_reserved) {
		if (plt_spinlock_trylock(&ocm->lock) != 0) {
			cn10k_ml_ocm_free_pages(cnxk_mldev, model->model_id, layer_id);
			layer->glow.ocm_map.ocm_reserved = false;
			layer->glow.ocm_map.tilemask = 0x0;
			plt_spinlock_unlock(&ocm->lock);
		}
	}

	job_enqueued = false;
	job_dequeued = false;
	do {
		if (!job_enqueued) {
			req->timeout = plt_tsc_cycles() + ML_CNXK_CMD_TIMEOUT * plt_tsc_hz();
			job_enqueued =
				roc_ml_scratch_enqueue(&cn10k_mldev->roc, &req->cn10k_req.jd);
		}

		if (job_enqueued && !job_dequeued)
			job_dequeued =
				roc_ml_scratch_dequeue(&cn10k_mldev->roc, &req->cn10k_req.jd);

		if (job_dequeued)
			break;
	} while (plt_tsc_cycles() < req->timeout);

	if (job_dequeued) {
		if (plt_read64(&req->cn10k_req.status) == ML_CNXK_POLL_JOB_FINISH) {
			if (req->cn10k_req.result.error_code == 0x0)
				ret = 0;
			else
				ret = -1;
		}
	} else {
		roc_ml_scratch_queue_reset(&cn10k_mldev->roc);
		ret = -ETIME;
	}

	locked = false;
	while (!locked) {
		if (plt_spinlock_trylock(&model->lock) != 0) {
			if (ret == 0)
				layer->state = ML_CNXK_LAYER_STATE_LOADED;
			else
				layer->state = ML_CNXK_LAYER_STATE_UNKNOWN;

			plt_spinlock_unlock(&model->lock);
			locked = true;
		}
	}

	return ret;
}

int
cn10k_ml_model_stop(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct cnxk_ml_layer *layer;
	int ret;

	layer = &model->layer[0];
	ret = cn10k_ml_layer_stop(cnxk_mldev, model->model_id, layer->name);
	if (ret != 0) {
		plt_err("CN10K Model stop failed, model_id = %u, error = %d", model->model_id, ret);
		return ret;
	}

	cnxk_mldev->nb_models_stopped++;
	model->state = ML_CNXK_MODEL_STATE_LOADED;

	return 0;
}

int
cn10k_ml_model_params_update(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
			     void *buffer)
{
	struct cnxk_ml_layer *layer;

	RTE_SET_USED(cnxk_mldev);

	if (model->state == ML_CNXK_MODEL_STATE_UNKNOWN)
		return -1;
	else if (model->state != ML_CNXK_MODEL_STATE_LOADED)
		return -EBUSY;

	layer = &model->layer[0];

	/* Update model weights & bias */
	rte_memcpy(layer->glow.addr.wb_load_addr, buffer,
		   layer->glow.metadata.weights_bias.file_size);

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

static __rte_always_inline void
cn10k_ml_result_update(struct cnxk_ml_dev *cnxk_mldev, int qp_id, struct cnxk_ml_req *req)
{
	union cn10k_ml_error_code *error_code;
	struct cn10k_ml_layer_xstats *xstats;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cn10k_ml_result *result;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	struct cnxk_ml_qp *qp;
	struct rte_ml_op *op;
	uint64_t hw_latency;
	uint64_t fw_latency;
	uint16_t model_id;
	uint16_t layer_id;

	result = &req->cn10k_req.result;
	op = req->op;

	if (likely(result->error_code == 0)) {
		model_id = cnxk_mldev->index_map[op->model_id].model_id;
		layer_id = cnxk_mldev->index_map[op->model_id].layer_id;
		model = cnxk_mldev->mldev->data->models[model_id];
		layer = &model->layer[layer_id];
		if (likely(qp_id >= 0)) {
			qp = cnxk_mldev->mldev->data->queue_pairs[qp_id];
			qp->stats.dequeued_count++;
			xstats = &layer->glow.burst_xstats[qp_id];
		} else {
			xstats = layer->glow.sync_xstats;
		}

		if (unlikely(xstats->dequeued_count == xstats->hw_reset_count)) {
			xstats->hw_latency_min = UINT64_MAX;
			xstats->hw_latency_max = 0;
		}

		if (unlikely(xstats->dequeued_count == xstats->fw_reset_count)) {
			xstats->fw_latency_min = UINT64_MAX;
			xstats->fw_latency_max = 0;
		}

		hw_latency = result->stats.hw_end - result->stats.hw_start;
		fw_latency = result->stats.fw_end - result->stats.fw_start - hw_latency;

		xstats->hw_latency_tot += hw_latency;
		xstats->hw_latency_min = PLT_MIN(xstats->hw_latency_min, hw_latency);
		xstats->hw_latency_max = PLT_MAX(xstats->hw_latency_max, hw_latency);
		xstats->fw_latency_tot += fw_latency;
		xstats->fw_latency_min = PLT_MIN(xstats->fw_latency_min, fw_latency);
		xstats->fw_latency_max = PLT_MAX(xstats->fw_latency_max, fw_latency);
		xstats->dequeued_count++;

		op->impl_opaque = result->error_code;
		op->status = RTE_ML_OP_STATUS_SUCCESS;
	} else {
		if (likely(qp_id >= 0)) {
			qp = cnxk_mldev->mldev->data->queue_pairs[qp_id];
			qp->stats.dequeue_err_count++;
		}

		/* Handle driver error */
		error_code = (union cn10k_ml_error_code *)&result->error_code;
		if (error_code->s.etype == ML_ETYPE_DRIVER) {
			cn10k_mldev = &cnxk_mldev->cn10k_mldev;

			/* Check for exception */
			if ((roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_EXCEPTION_SP_C0) !=
			     0) ||
			    (roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_EXCEPTION_SP_C1) != 0))
				error_code->s.stype = ML_DRIVER_ERR_EXCEPTION;
			else if ((roc_ml_reg_read64(&cn10k_mldev->roc, ML_CORE_INT_LO) != 0) ||
				 (roc_ml_reg_read64(&cn10k_mldev->roc, ML_CORE_INT_HI) != 0))
				error_code->s.stype = ML_DRIVER_ERR_FW_ERROR;
			else
				error_code->s.stype = ML_DRIVER_ERR_UNKNOWN;
		}

		op->impl_opaque = result->error_code;
		op->status = RTE_ML_OP_STATUS_ERROR;
	}

	op->user_ptr = result->user_ptr;
}

__rte_hot uint16_t
cn10k_ml_enqueue_burst(struct rte_ml_dev *dev, uint16_t qp_id, struct rte_ml_op **ops,
		       uint16_t nb_ops)
{
	union cn10k_ml_error_code *error_code;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_queue *queue;
	struct cnxk_ml_req *req;
	struct cnxk_ml_qp *qp;
	struct rte_ml_op *op;

	uint16_t count;
	uint64_t head;
	bool enqueued;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	qp = dev->data->queue_pairs[qp_id];
	queue = &qp->queue;

	head = queue->head;
	nb_ops = PLT_MIN(nb_ops, queue_free_count(head, queue->tail, qp->nb_desc));
	count = 0;

	if (unlikely(nb_ops == 0))
		return 0;

enqueue_req:
	op = ops[count];
	req = &queue->reqs[head];

	cn10k_mldev->set_poll_addr(req);
	cn10k_ml_prep_fp_job_descriptor(cnxk_mldev, req, op);

	memset(&req->cn10k_req.result, 0, sizeof(struct cn10k_ml_result));
	error_code = (union cn10k_ml_error_code *)&req->cn10k_req.result.error_code;
	error_code->s.etype = ML_ETYPE_UNKNOWN;
	req->cn10k_req.result.user_ptr = op->user_ptr;

	cn10k_mldev->set_poll_ptr(req);
	enqueued = cn10k_mldev->ml_jcmdq_enqueue(&cn10k_mldev->roc, &req->cn10k_req.jcmd);
	if (unlikely(!enqueued))
		goto jcmdq_full;

	req->timeout = plt_tsc_cycles() + queue->wait_cycles;
	req->op = op;

	queue_index_advance(&head, qp->nb_desc);
	count++;

	if (count < nb_ops)
		goto enqueue_req;

jcmdq_full:
	queue->head = head;
	qp->stats.enqueued_count += count;

	return count;
}

__rte_hot uint16_t
cn10k_ml_dequeue_burst(struct rte_ml_dev *dev, uint16_t qp_id, struct rte_ml_op **ops,
		       uint16_t nb_ops)
{
	union cn10k_ml_error_code *error_code;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_queue *queue;
	struct cnxk_ml_req *req;
	struct cnxk_ml_qp *qp;

	uint64_t status;
	uint16_t count;
	uint64_t tail;

	cnxk_mldev = dev->data->dev_private;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	qp = dev->data->queue_pairs[qp_id];
	queue = &qp->queue;

	tail = queue->tail;
	nb_ops = PLT_MIN(nb_ops, queue_pending_count(queue->head, tail, qp->nb_desc));
	count = 0;

	if (unlikely(nb_ops == 0))
		goto empty_or_active;

dequeue_req:
	req = &queue->reqs[tail];
	status = cn10k_mldev->get_poll_ptr(req);
	if (unlikely(status != ML_CNXK_POLL_JOB_FINISH)) {
		if (plt_tsc_cycles() < req->timeout) {
			goto empty_or_active;
		} else { /* Timeout, set indication of driver error */
			error_code = (union cn10k_ml_error_code *)&req->cn10k_req.result.error_code;
			error_code->s.etype = ML_ETYPE_DRIVER;
		}
	}

	cn10k_ml_result_update(cnxk_mldev, qp_id, req);
	ops[count] = req->op;

	queue_index_advance(&tail, qp->nb_desc);
	count++;

	if (count < nb_ops)
		goto dequeue_req;

empty_or_active:
	queue->tail = tail;

	return count;
}

__rte_hot int
cn10k_ml_op_error_get(struct rte_ml_dev *dev, struct rte_ml_op *op, struct rte_ml_op_error *error)
{
	union cn10k_ml_error_code *error_code;
	char msg[RTE_ML_STR_MAX];

	PLT_SET_USED(dev);

	error_code = (union cn10k_ml_error_code *)&op->impl_opaque;

	/* Copy error message */
	plt_strlcpy(msg, ml_etype_db[error_code->s.etype].name, sizeof(msg));

	/* Copy sub error message */
	if (error_code->s.etype == ML_ETYPE_HW_NONFATAL) {
		strcat(msg, " : ");
		if (error_code->s.stype < PLT_DIM(ml_stype_db_hw_nf))
			strcat(msg, ml_stype_db_hw_nf[error_code->s.stype].msg);
		else
			strcat(msg, "UNKNOWN ERROR");
	}

	if (error_code->s.etype == ML_ETYPE_DRIVER) {
		strcat(msg, " : ");
		strcat(msg, ml_stype_db_driver[error_code->s.stype].msg);
	}

	plt_strlcpy(error->message, msg, sizeof(error->message));
	error->errcode = error_code->u64;

	return 0;
}

__rte_hot int
cn10k_ml_inference_sync(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_op *op)
{
	union cn10k_ml_error_code *error_code;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	struct cnxk_ml_req *req;
	uint16_t model_id;
	uint16_t layer_id;
	bool timeout;
	int ret = 0;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	model_id = cnxk_mldev->index_map[op->model_id].model_id;
	layer_id = cnxk_mldev->index_map[op->model_id].layer_id;
	model = cnxk_mldev->mldev->data->models[model_id];
	layer = &model->layer[layer_id];
	req = layer->glow.req;

	cn10k_ml_set_poll_addr(req);
	cn10k_ml_prep_fp_job_descriptor(cnxk_mldev, req, op);

	memset(&req->cn10k_req.result, 0, sizeof(struct cn10k_ml_result));
	error_code = (union cn10k_ml_error_code *)&req->cn10k_req.result.error_code;
	error_code->s.etype = ML_ETYPE_UNKNOWN;
	req->cn10k_req.result.user_ptr = op->user_ptr;

	cn10k_mldev->set_poll_ptr(req);
	req->cn10k_req.jcmd.w1.s.jobptr = PLT_U64_CAST(&req->cn10k_req.jd);

	timeout = true;
	req->timeout = plt_tsc_cycles() + ML_CNXK_CMD_TIMEOUT * plt_tsc_hz();
	do {
		if (cn10k_mldev->ml_jcmdq_enqueue(&cn10k_mldev->roc, &req->cn10k_req.jcmd)) {
			req->op = op;
			timeout = false;
			break;
		}
	} while (plt_tsc_cycles() < req->timeout);

	if (timeout) {
		ret = -EBUSY;
		goto error_enqueue;
	}

	timeout = true;
	do {
		if (cn10k_mldev->get_poll_ptr(req) == ML_CNXK_POLL_JOB_FINISH) {
			timeout = false;
			break;
		}
	} while (plt_tsc_cycles() < req->timeout);

	if (timeout)
		ret = -ETIME;
	else
		cn10k_ml_result_update(cnxk_mldev, -1, req);

error_enqueue:
	return ret;
}
