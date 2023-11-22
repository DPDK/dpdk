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

/* Hardware non-fatal error subtype database */
static struct cnxk_ml_error_db ml_stype_db_hw_nf[] = {
	{ML_CN10K_FW_ERR_NOERR, "NO ERROR"},
	{ML_CN10K_FW_ERR_UNLOAD_ID_NOT_FOUND, "UNLOAD MODEL ID NOT FOUND"},
	{ML_CN10K_FW_ERR_LOAD_LUT_OVERFLOW, "LOAD LUT OVERFLOW"},
	{ML_CN10K_FW_ERR_ID_IN_USE, "MODEL ID IN USE"},
	{ML_CN10K_FW_ERR_INVALID_TILEMASK, "INVALID TILEMASK"},
	{ML_CN10K_FW_ERR_RUN_LUT_OVERFLOW, "RUN LUT OVERFLOW"},
	{ML_CN10K_FW_ERR_RUN_ID_NOT_FOUND, "RUN MODEL ID NOT FOUND"},
	{ML_CN10K_FW_ERR_COMMAND_NOTSUP, "COMMAND NOT SUPPORTED"},
	{ML_CN10K_FW_ERR_DDR_ADDR_RANGE, "DDR ADDRESS OUT OF RANGE"},
	{ML_CN10K_FW_ERR_NUM_BATCHES_INVALID, "INVALID BATCHES"},
	{ML_CN10K_FW_ERR_INSSYNC_TIMEOUT, "INSSYNC TIMEOUT"},
};

/* Driver error subtype database */
static struct cnxk_ml_error_db ml_stype_db_driver[] = {
	{ML_CN10K_DRIVER_ERR_NOERR, "NO ERROR"},
	{ML_CN10K_DRIVER_ERR_UNKNOWN, "UNKNOWN ERROR"},
	{ML_CN10K_DRIVER_ERR_EXCEPTION, "FW EXCEPTION"},
	{ML_CN10K_DRIVER_ERR_FW_ERROR, "UNKNOWN FIRMWARE ERROR"},
};

__rte_hot void
cn10k_ml_set_poll_addr(struct cnxk_ml_req *req)
{
	req->status = &req->cn10k_req.status;
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
				uint16_t index, void *input, void *output, uint16_t nb_batches)
{
	struct cn10k_ml_dev *cn10k_mldev;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	req->cn10k_req.jd.hdr.jce.w0.u64 = 0;
	req->cn10k_req.jd.hdr.jce.w1.u64 = PLT_U64_CAST(req->status);
	req->cn10k_req.jd.hdr.model_id = index;
	req->cn10k_req.jd.hdr.job_type = ML_CN10K_JOB_TYPE_MODEL_RUN;
	req->cn10k_req.jd.hdr.fp_flags = ML_FLAGS_POLL_COMPL;
	req->cn10k_req.jd.hdr.sp_flags = 0x0;
	req->cn10k_req.jd.hdr.result =
		roc_ml_addr_ap2mlip(&cn10k_mldev->roc, &req->cn10k_req.result);
	req->cn10k_req.jd.model_run.input_ddr_addr =
		PLT_U64_CAST(roc_ml_addr_ap2mlip(&cn10k_mldev->roc, input));
	req->cn10k_req.jd.model_run.output_ddr_addr =
		PLT_U64_CAST(roc_ml_addr_ap2mlip(&cn10k_mldev->roc, output));
	req->cn10k_req.jd.model_run.num_batches = nb_batches;
}

static void
cn10k_ml_xstats_layer_name_update(struct cnxk_ml_dev *cnxk_mldev, uint16_t model_id,
				  uint16_t layer_id)
{
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	uint16_t rclk_freq;
	uint16_t sclk_freq;
	uint16_t stat_id;
	char suffix[8];
	uint16_t i;

	model = cnxk_mldev->mldev->data->models[model_id];
	layer = &model->layer[layer_id];
	stat_id = cnxk_mldev->xstats.offset_for_layer[model_id][layer_id];

	roc_clk_freq_get(&rclk_freq, &sclk_freq);
	if (sclk_freq == 0)
		strcpy(suffix, "cycles");
	else
		strcpy(suffix, "ns");

	/* Update xstat name based on layer name and sclk availability */
	for (i = 0; i < RTE_DIM(layer_xstats); i++) {
		snprintf(cnxk_mldev->xstats.entries[stat_id].map.name,
			 sizeof(cnxk_mldev->xstats.entries[stat_id].map.name), "%s-%s-%s",
			 layer->glow.metadata.model.name, layer_xstats[i].name, suffix);
		stat_id++;
	}
}

void
cn10k_ml_xstat_model_name_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
			      uint16_t stat_id, uint16_t entry, char *suffix)
{
	snprintf(cnxk_mldev->xstats.entries[stat_id].map.name,
		 sizeof(cnxk_mldev->xstats.entries[stat_id].map.name), "%s-%s-%s",
		 model->glow.metadata.model.name, model_xstats[entry].name, suffix);
}

#define ML_AVG_FOREACH_QP(cnxk_mldev, layer, qp_id, str, value, count)                             \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {        \
			value += layer->glow.burst_xstats[qp_id].str##_latency_tot;                \
			count += layer->glow.burst_xstats[qp_id].dequeued_count -                  \
				 layer->glow.burst_xstats[qp_id].str##_reset_count;                \
		}                                                                                  \
		value += layer->glow.sync_xstats->str##_latency_tot;                               \
		count += layer->glow.sync_xstats->dequeued_count -                                 \
			 layer->glow.sync_xstats->str##_reset_count;                               \
		if (count != 0)                                                                    \
			value = value / count;                                                     \
	} while (0)

#define ML_MIN_FOREACH_QP(cnxk_mldev, layer, qp_id, str, value, count)                             \
	do {                                                                                       \
		value = UINT64_MAX;                                                                \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {        \
			value = PLT_MIN(value, layer->glow.burst_xstats[qp_id].str##_latency_min); \
			count += layer->glow.burst_xstats[qp_id].dequeued_count -                  \
				 layer->glow.burst_xstats[qp_id].str##_reset_count;                \
		}                                                                                  \
		value = PLT_MIN(value, layer->glow.sync_xstats->str##_latency_min);                \
		count += layer->glow.sync_xstats->dequeued_count -                                 \
			 layer->glow.sync_xstats->str##_reset_count;                               \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

#define ML_MAX_FOREACH_QP(cnxk_mldev, layer, qp_id, str, value, count)                             \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {        \
			value = PLT_MAX(value, layer->glow.burst_xstats[qp_id].str##_latency_max); \
			count += layer->glow.burst_xstats[qp_id].dequeued_count -                  \
				 layer->glow.burst_xstats[qp_id].str##_reset_count;                \
		}                                                                                  \
		value = PLT_MAX(value, layer->glow.sync_xstats->str##_latency_max);                \
		count += layer->glow.sync_xstats->dequeued_count -                                 \
			 layer->glow.sync_xstats->str##_reset_count;                               \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

uint64_t
cn10k_ml_model_xstat_get(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer,
			 enum cnxk_ml_xstats_type type)
{
	uint64_t count = 0;
	uint64_t value = 0;
	uint32_t qp_id;

	switch (type) {
	case avg_hw_latency:
		ML_AVG_FOREACH_QP(cnxk_mldev, layer, qp_id, hw, value, count);
		break;
	case min_hw_latency:
		ML_MIN_FOREACH_QP(cnxk_mldev, layer, qp_id, hw, value, count);
		break;
	case max_hw_latency:
		ML_MAX_FOREACH_QP(cnxk_mldev, layer, qp_id, hw, value, count);
		break;
	case avg_fw_latency:
		ML_AVG_FOREACH_QP(cnxk_mldev, layer, qp_id, fw, value, count);
		break;
	case min_fw_latency:
		ML_MIN_FOREACH_QP(cnxk_mldev, layer, qp_id, fw, value, count);
		break;
	case max_fw_latency:
		ML_MAX_FOREACH_QP(cnxk_mldev, layer, qp_id, fw, value, count);
		break;
	default:
		value = 0;
	}

	return value;
}

static int
cn10k_ml_cache_model_data(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer)
{
	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	uint64_t isize = 0;
	uint64_t osize = 0;
	int ret = 0;

	/* Create input and output buffers. */
	isize = layer->info.total_input_sz_q;
	osize = layer->info.total_output_sz_q;

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", "ml_dummy_io", layer->index);
	mz = plt_memzone_reserve_aligned(str, isize + osize, 0, ML_CN10K_ALIGN_SIZE);
	if (mz == NULL)
		return -ENOMEM;
	memset(mz->addr, 0, isize + osize);

	memset(layer->glow.req, 0, sizeof(struct cnxk_ml_req));
	ret = cn10k_ml_inference_sync(cnxk_mldev, layer->index, mz->addr,
				      PLT_PTR_ADD(mz->addr, isize), 1);
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

	/* Set JCMDQ enqueue function */
	if (cn10k_mldev->hw_queue_lock == 1)
		cn10k_mldev->ml_jcmdq_enqueue = roc_ml_jcmdq_enqueue_sl;
	else
		cn10k_mldev->ml_jcmdq_enqueue = roc_ml_jcmdq_enqueue_lf;

	return 0;
}

int
cn10k_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev)
{
	struct cn10k_ml_dev *cn10k_mldev;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;

	/* Release ocm_mask memory */
	rte_free(cn10k_mldev->ocm.ocm_mask);

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
	uint16_t layer_id;
	uint16_t wb_pages;
	uint64_t mz_size;
	uint16_t idx;
	int qp_id;
	int ret;

	PLT_SET_USED(size);

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

	ret = cn10k_ml_model_get_layer_id(model, layer_name, &layer_id);
	if (ret != 0)
		return ret;

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
	cn10k_ml_xstats_layer_name_update(cnxk_mldev, model_id, layer_id);

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

	/* Set model sub type */
	model->subtype = ML_CNXK_MODEL_SUBTYPE_GLOW_MRVL;

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
	layer->type = ML_CNXK_LAYER_TYPE_MRVL;
	ret = cn10k_ml_layer_load(cnxk_mldev, model->model_id, NULL, params->addr, params->size,
				  &layer->index);
	if (ret != 0) {
		plt_err("Model layer load failed: model_id = %u, layer_id = %u", model->model_id,
			0);
		return ret;
	}

	cn10k_ml_model_info_set(cnxk_mldev, model, &model->layer[0].info, &model->glow.metadata);

	/* Set fast-path functions */
	model->enqueue_single = cn10k_ml_enqueue_single;
	model->result_update = cn10k_ml_result_update;
	model->set_error_code = cn10k_ml_set_error_code;
	model->set_poll_addr = cn10k_ml_set_poll_addr;

	return 0;
}

int
cn10k_ml_layer_unload(void *device, uint16_t model_id, const char *layer_name)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;

	char str[RTE_MEMZONE_NAMESIZE];
	uint16_t layer_id;
	int ret;

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

	ret = cn10k_ml_model_get_layer_id(model, layer_name, &layer_id);
	if (ret != 0)
		return ret;

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

	uint16_t layer_id;
	bool job_enqueued;
	bool job_dequeued;
	uint8_t num_tiles;
	uint64_t tilemask;
	int wb_page_start;
	int tile_start;
	int tile_end;
	bool locked;
	int ret = 0;

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

	ret = cn10k_ml_model_get_layer_id(model, layer_name, &layer_id);
	if (ret != 0)
		return ret;

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
		if (cn10k_mldev->cache_model_data && model->type == ML_CNXK_MODEL_TYPE_GLOW)
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

	uint16_t layer_id;
	bool job_enqueued;
	bool job_dequeued;
	bool locked;
	int ret = 0;

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

	ret = cn10k_ml_model_get_layer_id(model, layer_name, &layer_id);
	if (ret != 0)
		return ret;

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

__rte_hot void
cn10k_ml_result_update(struct cnxk_ml_dev *cnxk_mldev, int qp_id, void *request)
{
	union cn10k_ml_error_code *error_code;
	struct cn10k_ml_layer_xstats *xstats;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cn10k_ml_result *result;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	struct cnxk_ml_req *req;
	struct cnxk_ml_qp *qp;
	struct rte_ml_op *op;
	uint64_t hw_latency;
	uint64_t fw_latency;
	uint16_t model_id;
	uint16_t layer_id;
	uint16_t idx;

	req = (struct cnxk_ml_req *)request;
	result = &req->cn10k_req.result;
	op = req->op;
	if (likely(result->error_code == 0)) {
		idx = req->cn10k_req.jd.hdr.model_id;
		model_id = cnxk_mldev->index_map[idx].model_id;
		layer_id = cnxk_mldev->index_map[idx].layer_id;
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
		if (error_code->s.etype == ML_CNXK_ETYPE_DRIVER) {
			cn10k_mldev = &cnxk_mldev->cn10k_mldev;

			/* Check for exception */
			if ((roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_EXCEPTION_SP_C0) !=
			     0) ||
			    (roc_ml_reg_read64(&cn10k_mldev->roc, ML_SCRATCH_EXCEPTION_SP_C1) != 0))
				error_code->s.stype = ML_CN10K_DRIVER_ERR_EXCEPTION;
			else if ((roc_ml_reg_read64(&cn10k_mldev->roc, ML_CORE_INT_LO) != 0) ||
				 (roc_ml_reg_read64(&cn10k_mldev->roc, ML_CORE_INT_HI) != 0))
				error_code->s.stype = ML_CN10K_DRIVER_ERR_FW_ERROR;
			else
				error_code->s.stype = ML_CN10K_DRIVER_ERR_UNKNOWN;
		}

		op->impl_opaque = result->error_code;
		op->status = RTE_ML_OP_STATUS_ERROR;
	}

	op->user_ptr = result->user_ptr;
}

__rte_hot void
cn10k_ml_set_error_code(struct cnxk_ml_req *req, uint64_t etype, uint64_t stype)
{
	union cn10k_ml_error_code *error_code;

	error_code = (union cn10k_ml_error_code *)&req->cn10k_req.result.error_code;
	error_code->s.etype = etype;
	error_code->s.stype = stype;
}

__rte_hot bool
cn10k_ml_enqueue_single(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_op *op, uint16_t layer_id,
			struct cnxk_ml_qp *qp, uint64_t head)
{
	union cn10k_ml_error_code *error_code;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_queue *queue;
	struct cnxk_ml_req *req;

	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	queue = &qp->queue;
	req = &queue->reqs[head];

	model = cnxk_mldev->mldev->data->models[op->model_id];
	model->set_poll_addr(req);
	cn10k_ml_prep_fp_job_descriptor(cnxk_mldev, req, model->layer[layer_id].index,
					op->input[0]->addr, op->output[0]->addr, op->nb_batches);

	memset(&req->cn10k_req.result, 0, sizeof(struct cn10k_ml_result));
	error_code = (union cn10k_ml_error_code *)&req->cn10k_req.result.error_code;
	error_code->s.etype = ML_CNXK_ETYPE_UNKNOWN;
	req->cn10k_req.result.user_ptr = op->user_ptr;

	cnxk_ml_set_poll_ptr(req);
	if (unlikely(!cn10k_mldev->ml_jcmdq_enqueue(&cn10k_mldev->roc, &req->cn10k_req.jcmd)))
		return false;

	req->timeout = plt_tsc_cycles() + queue->wait_cycles;
	req->op = op;

	return true;
}

__rte_hot int
cn10k_ml_op_error_get(struct rte_ml_dev *dev, struct rte_ml_op *op, struct rte_ml_op_error *error)
{
	union cn10k_ml_error_code *error_code;

	PLT_SET_USED(dev);

	error_code = (union cn10k_ml_error_code *)&op->impl_opaque;

	/* Copy sub error message */
	if (error_code->s.etype == ML_CNXK_ETYPE_HW_NONFATAL) {
		if (error_code->s.stype < PLT_DIM(ml_stype_db_hw_nf))
			snprintf(error->message, RTE_ML_STR_MAX, "%s : %s",
				 ml_etype_db[error_code->s.etype].str,
				 ml_stype_db_hw_nf[error_code->s.stype].str);
		else
			snprintf(error->message, RTE_ML_STR_MAX, "%s : UNKNOWN ERROR",
				 ml_etype_db[error_code->s.etype].str);
	} else if (error_code->s.etype == ML_CNXK_ETYPE_DRIVER) {
		snprintf(error->message, RTE_ML_STR_MAX, "%s : %s",
			 ml_etype_db[error_code->s.etype].str,
			 ml_stype_db_driver[error_code->s.stype].str);
	} else {
		snprintf(error->message, RTE_ML_STR_MAX, "%s",
			 ml_etype_db[error_code->s.etype].str);
	}

	error->errcode = error_code->u64;

	return 0;
}

__rte_hot int
cn10k_ml_inference_sync(void *device, uint16_t index, void *input, void *output,
			uint16_t nb_batches)
{
	union cn10k_ml_error_code *error_code;
	struct cn10k_ml_dev *cn10k_mldev;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;
	struct cnxk_ml_req *req;
	struct rte_ml_op op;
	uint16_t model_id;
	uint16_t layer_id;
	bool timeout;
	int ret = 0;

	cnxk_mldev = (struct cnxk_ml_dev *)device;
	cn10k_mldev = &cnxk_mldev->cn10k_mldev;
	model_id = cnxk_mldev->index_map[index].model_id;
	layer_id = cnxk_mldev->index_map[index].layer_id;
	model = cnxk_mldev->mldev->data->models[model_id];
	layer = &model->layer[layer_id];
	req = layer->glow.req;

	op.model_id = index;
	op.impl_opaque = 0;

	cn10k_ml_set_poll_addr(req);
	cn10k_ml_prep_fp_job_descriptor(cnxk_mldev, req, index, input, output, nb_batches);

	memset(&req->cn10k_req.result, 0, sizeof(struct cn10k_ml_result));
	error_code = (union cn10k_ml_error_code *)&req->cn10k_req.result.error_code;
	error_code->s.etype = ML_CNXK_ETYPE_UNKNOWN;
	req->cn10k_req.result.user_ptr = NULL;

	cnxk_ml_set_poll_ptr(req);
	req->cn10k_req.jcmd.w1.s.jobptr = PLT_U64_CAST(&req->cn10k_req.jd);

	timeout = true;
	req->timeout = plt_tsc_cycles() + ML_CNXK_CMD_TIMEOUT * plt_tsc_hz();
	do {
		if (cn10k_mldev->ml_jcmdq_enqueue(&cn10k_mldev->roc, &req->cn10k_req.jcmd)) {
			req->op = &op;
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
		if (cnxk_ml_get_poll_ptr(req) == ML_CNXK_POLL_JOB_FINISH) {
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

int
cn10k_ml_io_alloc(void *device, uint16_t model_id, const char *layer_name, uint64_t **input_qbuffer,
		  uint64_t **output_qbuffer)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	struct cnxk_ml_layer *layer;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	uint64_t output_size;
	uint64_t input_size;
	uint16_t layer_id;
	int ret;

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

	ret = cn10k_ml_model_get_layer_id(model, layer_name, &layer_id);
	if (ret != 0)
		return ret;

	layer = &model->layer[layer_id];
	input_size = PLT_ALIGN_CEIL(layer->info.total_input_sz_q, ML_CN10K_ALIGN_SIZE);
	output_size = PLT_ALIGN_CEIL(layer->info.total_output_sz_q, ML_CN10K_ALIGN_SIZE);

	sprintf(str, "cn10k_ml_io_mz_%u_%u", model_id, layer_id);
	mz = plt_memzone_reserve_aligned(str, input_size + output_size, 0, ML_CN10K_ALIGN_SIZE);
	if (mz == NULL) {
		plt_err("io_alloc failed: Unable to allocate memory: model_id = %u, layer_name = %s",
			model_id, layer_name);
		return -ENOMEM;
	}

	*input_qbuffer = mz->addr;
	*output_qbuffer = PLT_PTR_ADD(mz->addr, input_size);

	return 0;
}

int
cn10k_ml_io_free(void *device, uint16_t model_id, const char *layer_name)
{
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	uint16_t layer_id;
	int ret;

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

	ret = cn10k_ml_model_get_layer_id(model, layer_name, &layer_id);
	if (ret != 0)
		return ret;

	sprintf(str, "cn10k_ml_io_mz_%u_%u", model_id, layer_id);
	mz = plt_memzone_lookup(str);
	if (mz == NULL) {
		plt_err("io_free failed: Memzone not found: model_id = %u, layer_name = %s",
			model_id, layer_name);
		return -EINVAL;
	}

	return plt_memzone_free(mz);
}

int
cn10k_ml_malloc(const char *name, size_t size, uint32_t align, void **addr)
{
	const struct plt_memzone *mz;

	mz = plt_memzone_reserve_aligned(name, size, 0, align);
	if (mz == NULL) {
		plt_err("ml_malloc failed: Unable to allocate memory: name = %s", name);
		return -ENOMEM;
	}

	*addr = mz->addr;

	return 0;
}

int
cn10k_ml_free(const char *name)
{
	const struct plt_memzone *mz;

	mz = plt_memzone_lookup(name);
	if (mz == NULL) {
		plt_err("ml_free failed: Memzone not found: name = %s", name);
		return -EINVAL;
	}

	return plt_memzone_free(mz);
}
