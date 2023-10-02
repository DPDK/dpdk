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

/* Scratch register range for poll mode requests */
#define ML_POLL_REGISTER_SYNC  1023
#define ML_POLL_REGISTER_START 1024
#define ML_POLL_REGISTER_END   2047

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

static void
print_line(FILE *fp, int len)
{
	int i;

	for (i = 0; i < len; i++)
		fprintf(fp, "-");
	fprintf(fp, "\n");
}

static inline void
cn10k_ml_set_poll_addr_ddr(struct cn10k_ml_qp *qp, struct cn10k_ml_req *req, uint64_t idx)
{
	PLT_SET_USED(qp);
	PLT_SET_USED(idx);

	req->compl_W1 = PLT_U64_CAST(&req->status);
}

static inline void
cn10k_ml_set_poll_addr_reg(struct cn10k_ml_qp *qp, struct cn10k_ml_req *req, uint64_t idx)
{
	req->compl_W1 = ML_SCRATCH(qp->block_start + idx % qp->block_size);
}

static inline void
cn10k_ml_set_poll_ptr_ddr(struct roc_ml *roc_ml, struct cn10k_ml_req *req)
{
	PLT_SET_USED(roc_ml);

	plt_write64(ML_CN10K_POLL_JOB_START, req->compl_W1);
}

static inline void
cn10k_ml_set_poll_ptr_reg(struct roc_ml *roc_ml, struct cn10k_ml_req *req)
{
	roc_ml_reg_write64(roc_ml, ML_CN10K_POLL_JOB_START, req->compl_W1);
}

static inline uint64_t
cn10k_ml_get_poll_ptr_ddr(struct roc_ml *roc_ml, struct cn10k_ml_req *req)
{
	PLT_SET_USED(roc_ml);

	return plt_read64(req->compl_W1);
}

static inline uint64_t
cn10k_ml_get_poll_ptr_reg(struct roc_ml *roc_ml, struct cn10k_ml_req *req)
{
	return roc_ml_reg_read64(roc_ml, req->compl_W1);
}

static inline void
cn10k_ml_set_sync_addr(struct cn10k_ml_dev *mldev, struct cn10k_ml_req *req)
{
	if (strcmp(mldev->fw.poll_mem, "ddr") == 0)
		req->compl_W1 = PLT_U64_CAST(&req->status);
	else if (strcmp(mldev->fw.poll_mem, "register") == 0)
		req->compl_W1 = ML_SCRATCH(ML_POLL_REGISTER_SYNC);
}

static inline void
cn10k_ml_enq_barrier_ddr(void)
{
}

static inline void
cn10k_ml_deq_barrier_ddr(void)
{
}

static inline void
cn10k_ml_enq_barrier_register(void)
{
	dmb_st;
}

static inline void
cn10k_ml_deq_barrier_register(void)
{
	dsb_st;
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
	qp->stats.enqueued_count = 0;
	qp->stats.dequeued_count = 0;
	qp->stats.enqueue_err_count = 0;
	qp->stats.dequeue_err_count = 0;
	qp->block_size =
		(ML_POLL_REGISTER_END - ML_POLL_REGISTER_START + 1) / dev->data->nb_queue_pairs;
	qp->block_start = ML_POLL_REGISTER_START + qp_id * qp->block_size;

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
	uint8_t j;

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
		fprintf(fp, "%*s : 0x%" PRIx64 "\n", FIELD_LEN, "ocm_wb_start",
			model->model_mem_map.wb_page_start * mldev->ocm.page_size);
	}

	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_inputs", model->metadata.model.num_input);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_outputs", model->metadata.model.num_output);
	fprintf(fp, "\n");

	print_line(fp, LINE_LEN);
	fprintf(fp, "%8s  %16s  %12s  %18s  %12s\n", "input", "input_name", "input_type",
		"model_input_type", "quantize");
	print_line(fp, LINE_LEN);
	for (i = 0; i < model->metadata.model.num_input; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			fprintf(fp, "%8u  ", i);
			fprintf(fp, "%*s  ", 16, model->metadata.input1[i].input_name);
			rte_ml_io_type_to_str(model->metadata.input1[i].input_type, str, STR_LEN);
			fprintf(fp, "%*s  ", 12, str);
			rte_ml_io_type_to_str(model->metadata.input1[i].model_input_type, str,
					      STR_LEN);
			fprintf(fp, "%*s  ", 18, str);
			fprintf(fp, "%*s", 12,
				(model->metadata.input1[i].quantize == 1 ? "Yes" : "No"));
			fprintf(fp, "\n");
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;

			fprintf(fp, "%8u  ", i);
			fprintf(fp, "%*s  ", 16, model->metadata.input2[j].input_name);
			rte_ml_io_type_to_str(model->metadata.input2[j].input_type, str, STR_LEN);
			fprintf(fp, "%*s  ", 12, str);
			rte_ml_io_type_to_str(model->metadata.input2[j].model_input_type, str,
					      STR_LEN);
			fprintf(fp, "%*s  ", 18, str);
			fprintf(fp, "%*s", 12,
				(model->metadata.input2[j].quantize == 1 ? "Yes" : "No"));
			fprintf(fp, "\n");
		}
	}
	fprintf(fp, "\n");

	print_line(fp, LINE_LEN);
	fprintf(fp, "%8s  %16s  %12s  %18s  %12s\n", "output", "output_name", "output_type",
		"model_output_type", "dequantize");
	print_line(fp, LINE_LEN);
	for (i = 0; i < model->metadata.model.num_output; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			fprintf(fp, "%8u  ", i);
			fprintf(fp, "%*s  ", 16, model->metadata.output1[i].output_name);
			rte_ml_io_type_to_str(model->metadata.output1[i].output_type, str, STR_LEN);
			fprintf(fp, "%*s  ", 12, str);
			rte_ml_io_type_to_str(model->metadata.output1[i].model_output_type, str,
					      STR_LEN);
			fprintf(fp, "%*s  ", 18, str);
			fprintf(fp, "%*s", 12,
				(model->metadata.output1[i].dequantize == 1 ? "Yes" : "No"));
			fprintf(fp, "\n");
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;
			fprintf(fp, "%8u  ", i);
			fprintf(fp, "%*s  ", 16, model->metadata.output2[j].output_name);
			rte_ml_io_type_to_str(model->metadata.output2[j].output_type, str, STR_LEN);
			fprintf(fp, "%*s  ", 12, str);
			rte_ml_io_type_to_str(model->metadata.output2[j].model_output_type, str,
					      STR_LEN);
			fprintf(fp, "%*s  ", 18, str);
			fprintf(fp, "%*s", 12,
				(model->metadata.output2[j].dequantize == 1 ? "Yes" : "No"));
			fprintf(fp, "\n");
		}
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

		req->jd.hdr.sp_flags |= ML_CN10K_SP_FLAGS_EXTENDED_LOAD_JD;
		req->jd.model_start.extended_args =
			PLT_U64_CAST(roc_ml_addr_ap2mlip(&mldev->roc, &req->extended_args));
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

		req->extended_args.start.ddr_scratch_base_address = PLT_U64_CAST(
			roc_ml_addr_ap2mlip(&mldev->roc, model->addr.scratch_base_addr));
		req->extended_args.start.ddr_scratch_range_start =
			metadata->model.ddr_scratch_range_start;
		req->extended_args.start.ddr_scratch_range_end =
			metadata->model.ddr_scratch_range_end;
	}
}

static __rte_always_inline void
cn10k_ml_prep_fp_job_descriptor(struct rte_ml_dev *dev, struct cn10k_ml_req *req,
				struct rte_ml_op *op)
{
	struct cn10k_ml_dev *mldev;

	mldev = dev->data->dev_private;

	req->jd.hdr.jce.w0.u64 = 0;
	req->jd.hdr.jce.w1.u64 = req->compl_W1;
	req->jd.hdr.model_id = op->model_id;
	req->jd.hdr.job_type = ML_CN10K_JOB_TYPE_MODEL_RUN;
	req->jd.hdr.fp_flags = ML_FLAGS_POLL_COMPL;
	req->jd.hdr.sp_flags = 0x0;
	req->jd.hdr.result = roc_ml_addr_ap2mlip(&mldev->roc, &req->result);
	req->jd.model_run.input_ddr_addr =
		PLT_U64_CAST(roc_ml_addr_ap2mlip(&mldev->roc, op->input[0]->addr));
	req->jd.model_run.output_ddr_addr =
		PLT_U64_CAST(roc_ml_addr_ap2mlip(&mldev->roc, op->output[0]->addr));
	req->jd.model_run.num_batches = op->nb_batches;
}

struct xstat_info {
	char name[32];
	enum cn10k_ml_xstats_type type;
	uint8_t reset_allowed;
};

/* Note: Device stats are not allowed to be reset. */
static const struct xstat_info device_stats[] = {
	{"nb_models_loaded", nb_models_loaded, 0},
	{"nb_models_unloaded", nb_models_unloaded, 0},
	{"nb_models_started", nb_models_started, 0},
	{"nb_models_stopped", nb_models_stopped, 0},
};

static const struct xstat_info model_stats[] = {
	{"Avg-HW-Latency", avg_hw_latency, 1}, {"Min-HW-Latency", min_hw_latency, 1},
	{"Max-HW-Latency", max_hw_latency, 1}, {"Avg-FW-Latency", avg_fw_latency, 1},
	{"Min-FW-Latency", min_fw_latency, 1}, {"Max-FW-Latency", max_fw_latency, 1},
};

static int
cn10k_ml_xstats_init(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	uint16_t nb_stats;
	uint16_t stat_id;
	uint16_t model;
	uint16_t i;

	mldev = dev->data->dev_private;

	/* Allocate memory for xstats entries. Don't allocate during reconfigure */
	nb_stats = RTE_DIM(device_stats) + ML_CN10K_MAX_MODELS * RTE_DIM(model_stats);
	if (mldev->xstats.entries == NULL)
		mldev->xstats.entries = rte_zmalloc("cn10k_ml_xstats",
						    sizeof(struct cn10k_ml_xstats_entry) * nb_stats,
						    PLT_CACHE_LINE_SIZE);

	if (mldev->xstats.entries == NULL)
		return -ENOMEM;

	/* Initialize device xstats */
	stat_id = 0;
	for (i = 0; i < RTE_DIM(device_stats); i++) {
		mldev->xstats.entries[stat_id].map.id = stat_id;
		snprintf(mldev->xstats.entries[stat_id].map.name,
			 sizeof(mldev->xstats.entries[stat_id].map.name), "%s",
			 device_stats[i].name);

		mldev->xstats.entries[stat_id].mode = RTE_ML_DEV_XSTATS_DEVICE;
		mldev->xstats.entries[stat_id].type = device_stats[i].type;
		mldev->xstats.entries[stat_id].fn_id = CN10K_ML_XSTATS_FN_DEVICE;
		mldev->xstats.entries[stat_id].obj_idx = 0;
		mldev->xstats.entries[stat_id].reset_allowed = device_stats[i].reset_allowed;
		stat_id++;
	}
	mldev->xstats.count_mode_device = stat_id;

	/* Initialize model xstats */
	for (model = 0; model < ML_CN10K_MAX_MODELS; model++) {
		mldev->xstats.offset_for_model[model] = stat_id;

		for (i = 0; i < RTE_DIM(model_stats); i++) {
			mldev->xstats.entries[stat_id].map.id = stat_id;
			mldev->xstats.entries[stat_id].mode = RTE_ML_DEV_XSTATS_MODEL;
			mldev->xstats.entries[stat_id].type = model_stats[i].type;
			mldev->xstats.entries[stat_id].fn_id = CN10K_ML_XSTATS_FN_MODEL;
			mldev->xstats.entries[stat_id].obj_idx = model;
			mldev->xstats.entries[stat_id].reset_allowed = model_stats[i].reset_allowed;

			/* Name of xstat is updated during model load */
			snprintf(mldev->xstats.entries[stat_id].map.name,
				 sizeof(mldev->xstats.entries[stat_id].map.name), "Model-%u-%s",
				 model, model_stats[i].name);

			stat_id++;
		}

		mldev->xstats.count_per_model[model] = RTE_DIM(model_stats);
	}

	mldev->xstats.count_mode_model = stat_id - mldev->xstats.count_mode_device;
	mldev->xstats.count = stat_id;

	return 0;
}

static void
cn10k_ml_xstats_uninit(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;

	mldev = dev->data->dev_private;

	rte_free(mldev->xstats.entries);
	mldev->xstats.entries = NULL;

	mldev->xstats.count = 0;
}

static void
cn10k_ml_xstats_model_name_update(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	uint16_t rclk_freq;
	uint16_t sclk_freq;
	uint16_t stat_id;
	char suffix[8];
	uint16_t i;

	mldev = dev->data->dev_private;
	model = dev->data->models[model_id];
	stat_id = RTE_DIM(device_stats) + model_id * RTE_DIM(model_stats);

	roc_clk_freq_get(&rclk_freq, &sclk_freq);
	if (sclk_freq == 0)
		strcpy(suffix, "cycles");
	else
		strcpy(suffix, "ns");

	/* Update xstat name based on model name and sclk availability */
	for (i = 0; i < RTE_DIM(model_stats); i++) {
		snprintf(mldev->xstats.entries[stat_id].map.name,
			 sizeof(mldev->xstats.entries[stat_id].map.name), "%s-%s-%s",
			 model->metadata.model.name, model_stats[i].name, suffix);
		stat_id++;
	}
}

static uint64_t
cn10k_ml_dev_xstat_get(struct rte_ml_dev *dev, uint16_t obj_idx __rte_unused,
		       enum cn10k_ml_xstats_type type)
{
	struct cn10k_ml_dev *mldev;

	mldev = dev->data->dev_private;

	switch (type) {
	case nb_models_loaded:
		return mldev->nb_models_loaded;
	case nb_models_unloaded:
		return mldev->nb_models_unloaded;
	case nb_models_started:
		return mldev->nb_models_started;
	case nb_models_stopped:
		return mldev->nb_models_stopped;
	default:
		return -1;
	}

	return 0;
}

#define ML_AVG_FOREACH_QP(dev, model, qp_id, str, value, count)                                    \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			value += model->burst_stats[qp_id].str##_latency_tot;                      \
			count += model->burst_stats[qp_id].dequeued_count -                        \
				 model->burst_stats[qp_id].str##_reset_count;                      \
		}                                                                                  \
		if (count != 0)                                                                    \
			value = value / count;                                                     \
	} while (0)

#define ML_MIN_FOREACH_QP(dev, model, qp_id, str, value, count)                                    \
	do {                                                                                       \
		value = UINT64_MAX;                                                                \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			value = PLT_MIN(value, model->burst_stats[qp_id].str##_latency_min);       \
			count += model->burst_stats[qp_id].dequeued_count -                        \
				 model->burst_stats[qp_id].str##_reset_count;                      \
		}                                                                                  \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

#define ML_MAX_FOREACH_QP(dev, model, qp_id, str, value, count)                                    \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			value = PLT_MAX(value, model->burst_stats[qp_id].str##_latency_max);       \
			count += model->burst_stats[qp_id].dequeued_count -                        \
				 model->burst_stats[qp_id].str##_reset_count;                      \
		}                                                                                  \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

static uint64_t
cn10k_ml_model_xstat_get(struct rte_ml_dev *dev, uint16_t obj_idx, enum cn10k_ml_xstats_type type)
{
	struct cn10k_ml_model *model;
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
	struct cn10k_ml_xstats_entry *xs;
	struct cn10k_ml_dev *mldev;
	uint16_t nb_stats;
	uint16_t stat_id;
	uint32_t i;

	mldev = dev->data->dev_private;

	if (stat_ids == NULL)
		nb_stats = mldev->xstats.count_mode_device;
	else
		nb_stats = nb_ids;

	for (i = 0; i < nb_stats; i++) {
		if (stat_ids == NULL)
			stat_id = i;
		else
			stat_id = stat_ids[i];

		if (stat_id >= mldev->xstats.count_mode_device)
			return -EINVAL;

		xs = &mldev->xstats.entries[stat_id];
		if (!xs->reset_allowed)
			continue;

		xs->reset_value = cn10k_ml_dev_xstat_get(dev, xs->obj_idx, xs->type);
	}

	return 0;
}

#define ML_AVG_RESET_FOREACH_QP(dev, model, qp_id, str)                                            \
	do {                                                                                       \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {                      \
			model->burst_stats[qp_id].str##_latency_tot = 0;                           \
			model->burst_stats[qp_id].str##_reset_count =                              \
				model->burst_stats[qp_id].dequeued_count;                          \
		}                                                                                  \
	} while (0)

#define ML_MIN_RESET_FOREACH_QP(dev, model, qp_id, str)                                            \
	do {                                                                                       \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++)                        \
			model->burst_stats[qp_id].str##_latency_min = UINT64_MAX;                  \
	} while (0)

#define ML_MAX_RESET_FOREACH_QP(dev, model, qp_id, str)                                            \
	do {                                                                                       \
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++)                        \
			model->burst_stats[qp_id].str##_latency_max = 0;                           \
	} while (0)

static void
cn10k_ml_reset_model_stat(struct rte_ml_dev *dev, uint16_t model_id, enum cn10k_ml_xstats_type type)
{
	struct cn10k_ml_model *model;
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
	struct cn10k_ml_xstats_entry *xs;
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	int32_t lcl_model_id = 0;
	uint16_t start_id;
	uint16_t end_id;
	int32_t i;
	int32_t j;

	mldev = dev->data->dev_private;
	for (i = 0; i < ML_CN10K_MAX_MODELS; i++) {
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

		start_id = mldev->xstats.offset_for_model[i];
		end_id = mldev->xstats.offset_for_model[i] + mldev->xstats.count_per_model[i] - 1;

		if (stat_ids == NULL) {
			for (j = start_id; j <= end_id; j++) {
				xs = &mldev->xstats.entries[j];
				cn10k_ml_reset_model_stat(dev, i, xs->type);
			}
		} else {
			for (j = 0; j < nb_ids; j++) {
				if (stat_ids[j] < start_id || stat_ids[j] > end_id) {
					plt_err("Invalid stat_ids[%d] = %d for model_id = %d\n", j,
						stat_ids[j], lcl_model_id);
					return -EINVAL;
				}
				xs = &mldev->xstats.entries[stat_ids[j]];
				cn10k_ml_reset_model_stat(dev, i, xs->type);
			}
		}
	}

	return 0;
}

static int
cn10k_ml_cache_model_data(struct rte_ml_dev *dev, uint16_t model_id)
{
	struct rte_ml_model_info *info;
	struct cn10k_ml_model *model;
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

	model = dev->data->models[model_id];
	info = (struct rte_ml_model_info *)model->info;
	inp = &seg[0];
	out = &seg[1];

	/* Create input and output buffers. */
	for (i = 0; i < info->nb_inputs; i++)
		isize += info->input_info[i].size;

	for (i = 0; i < info->nb_outputs; i++)
		osize += info->output_info[i].size;

	isize = model->batch_size * isize;
	osize = model->batch_size * osize;

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", "ml_dummy_io", model_id);
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

	op.model_id = model_id;
	op.nb_batches = model->batch_size;
	op.mempool = NULL;

	op.input = &inp;
	op.output = &out;

	memset(model->req, 0, sizeof(struct cn10k_ml_req));
	ret = cn10k_ml_inference_sync(dev, &op);
	plt_memzone_free(mz);

	return ret;
}

static int
cn10k_ml_dev_info_get(struct rte_ml_dev *dev, struct rte_ml_dev_info *dev_info)
{
	struct cn10k_ml_dev *mldev;

	if (dev_info == NULL)
		return -EINVAL;

	mldev = dev->data->dev_private;

	memset(dev_info, 0, sizeof(struct rte_ml_dev_info));
	dev_info->driver_name = dev->device->driver->name;
	dev_info->max_models = ML_CN10K_MAX_MODELS;
	if (mldev->hw_queue_lock)
		dev_info->max_queue_pairs = ML_CN10K_MAX_QP_PER_DEVICE_SL;
	else
		dev_info->max_queue_pairs = ML_CN10K_MAX_QP_PER_DEVICE_LF;

	if (strcmp(mldev->fw.poll_mem, "register") == 0)
		dev_info->max_desc = ML_CN10K_MAX_DESC_PER_QP / dev_info->max_queue_pairs;
	else if (strcmp(mldev->fw.poll_mem, "ddr") == 0)
		dev_info->max_desc = ML_CN10K_MAX_DESC_PER_QP;

	dev_info->max_io = ML_CN10K_MAX_INPUT_OUTPUT;
	dev_info->max_segments = ML_CN10K_MAX_SEGMENTS;
	dev_info->align_size = ML_CN10K_ALIGN_SIZE;

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
	ocm->page_size = mldev->ocm_page_size;
	ocm->num_pages = ocm->size_per_tile / ocm->page_size;
	ocm->mask_words = ocm->num_pages / (8 * sizeof(uint8_t));

	/* Allocate memory for ocm_mask */
	ocm->ocm_mask =
		rte_zmalloc("ocm_mask", ocm->mask_words * ocm->num_tiles, RTE_CACHE_LINE_SIZE);
	if (ocm->ocm_mask == NULL) {
		plt_err("Unable to allocate memory for OCM mask");
		ret = -ENOMEM;
		goto error;
	}

	for (tile_id = 0; tile_id < ocm->num_tiles; tile_id++) {
		ocm->tile_ocm_info[tile_id].ocm_mask = ocm->ocm_mask + tile_id * ocm->mask_words;
		ocm->tile_ocm_info[tile_id].last_wb_page = -1;
	}

	rte_spinlock_init(&ocm->lock);

	/* Initialize xstats */
	ret = cn10k_ml_xstats_init(dev);
	if (ret != 0) {
		plt_err("Failed to initialize xstats");
		goto error;
	}

	/* Set JCMDQ enqueue function */
	if (mldev->hw_queue_lock == 1)
		mldev->ml_jcmdq_enqueue = roc_ml_jcmdq_enqueue_sl;
	else
		mldev->ml_jcmdq_enqueue = roc_ml_jcmdq_enqueue_lf;

	/* Set polling function pointers */
	if (strcmp(mldev->fw.poll_mem, "ddr") == 0) {
		mldev->set_poll_addr = cn10k_ml_set_poll_addr_ddr;
		mldev->set_poll_ptr = cn10k_ml_set_poll_ptr_ddr;
		mldev->get_poll_ptr = cn10k_ml_get_poll_ptr_ddr;
	} else if (strcmp(mldev->fw.poll_mem, "register") == 0) {
		mldev->set_poll_addr = cn10k_ml_set_poll_addr_reg;
		mldev->set_poll_ptr = cn10k_ml_set_poll_ptr_reg;
		mldev->get_poll_ptr = cn10k_ml_get_poll_ptr_reg;
	}

	/* Set barrier function pointers */
	if (strcmp(mldev->fw.poll_mem, "ddr") == 0) {
		mldev->set_enq_barrier = cn10k_ml_enq_barrier_ddr;
		mldev->set_deq_barrier = cn10k_ml_deq_barrier_ddr;
	} else if (strcmp(mldev->fw.poll_mem, "register") == 0) {
		mldev->set_enq_barrier = cn10k_ml_enq_barrier_register;
		mldev->set_deq_barrier = cn10k_ml_deq_barrier_register;
	}

	dev->enqueue_burst = cn10k_ml_enqueue_burst;
	dev->dequeue_burst = cn10k_ml_dequeue_burst;
	dev->op_error_get = cn10k_ml_op_error_get;

	mldev->nb_models_loaded = 0;
	mldev->nb_models_started = 0;
	mldev->nb_models_stopped = 0;
	mldev->nb_models_unloaded = 0;
	mldev->state = ML_CN10K_DEV_STATE_CONFIGURED;

	return 0;

error:
	rte_free(dev->data->queue_pairs);

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

	/* Release ocm_mask memory */
	rte_free(mldev->ocm.ocm_mask);

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

	rte_free(dev->data->queue_pairs);

	/* Un-initialize xstats */
	cn10k_ml_xstats_uninit(dev);

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
cn10k_ml_dev_stats_get(struct rte_ml_dev *dev, struct rte_ml_dev_stats *stats)
{
	struct cn10k_ml_qp *qp;
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
cn10k_ml_dev_stats_reset(struct rte_ml_dev *dev)
{
	struct cn10k_ml_qp *qp;
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
cn10k_ml_dev_xstats_names_get(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode,
			      int32_t model_id, struct rte_ml_dev_xstats_map *xstats_map,
			      uint32_t size)
{
	struct cn10k_ml_dev *mldev;
	uint32_t xstats_mode_count;
	uint32_t idx = 0;
	uint32_t i;

	mldev = dev->data->dev_private;

	xstats_mode_count = 0;
	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		xstats_mode_count = mldev->xstats.count_mode_device;
		break;
	case RTE_ML_DEV_XSTATS_MODEL:
		if (model_id >= ML_CN10K_MAX_MODELS)
			break;
		xstats_mode_count = mldev->xstats.count_per_model[model_id];
		break;
	default:
		return -EINVAL;
	};

	if (xstats_mode_count > size || xstats_map == NULL)
		return xstats_mode_count;

	for (i = 0; i < mldev->xstats.count && idx < size; i++) {
		if (mldev->xstats.entries[i].mode != mode)
			continue;

		if (mode != RTE_ML_DEV_XSTATS_DEVICE &&
		    model_id != mldev->xstats.entries[i].obj_idx)
			continue;

		strncpy(xstats_map[idx].name, mldev->xstats.entries[i].map.name, RTE_ML_STR_MAX);
		xstats_map[idx].id = mldev->xstats.entries[i].map.id;
		idx++;
	}

	return idx;
}

static int
cn10k_ml_dev_xstats_by_name_get(struct rte_ml_dev *dev, const char *name, uint16_t *stat_id,
				uint64_t *value)
{
	struct cn10k_ml_xstats_entry *xs;
	struct cn10k_ml_dev *mldev;
	cn10k_ml_xstats_fn fn;
	uint32_t i;

	mldev = dev->data->dev_private;
	for (i = 0; i < mldev->xstats.count; i++) {
		xs = &mldev->xstats.entries[i];
		if (strncmp(xs->map.name, name, RTE_ML_STR_MAX) == 0) {
			if (stat_id != NULL)
				*stat_id = xs->map.id;

			switch (xs->fn_id) {
			case CN10K_ML_XSTATS_FN_DEVICE:
				fn = cn10k_ml_dev_xstat_get;
				break;
			case CN10K_ML_XSTATS_FN_MODEL:
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

static int
cn10k_ml_dev_xstats_get(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
			const uint16_t stat_ids[], uint64_t values[], uint16_t nb_ids)
{
	struct cn10k_ml_xstats_entry *xs;
	struct cn10k_ml_dev *mldev;
	uint32_t xstats_mode_count;
	cn10k_ml_xstats_fn fn;
	uint64_t val;
	uint32_t idx;
	uint32_t i;

	mldev = dev->data->dev_private;
	xstats_mode_count = 0;

	switch (mode) {
	case RTE_ML_DEV_XSTATS_DEVICE:
		xstats_mode_count = mldev->xstats.count_mode_device;
		break;
	case RTE_ML_DEV_XSTATS_MODEL:
		if (model_id >= ML_CN10K_MAX_MODELS)
			return -EINVAL;
		xstats_mode_count = mldev->xstats.count_per_model[model_id];
		break;
	default:
		return -EINVAL;
	};

	idx = 0;
	for (i = 0; i < nb_ids && idx < xstats_mode_count; i++) {
		xs = &mldev->xstats.entries[stat_ids[i]];
		if (stat_ids[i] > mldev->xstats.count || xs->mode != mode)
			continue;

		if (mode == RTE_ML_DEV_XSTATS_MODEL && model_id != xs->obj_idx) {
			plt_err("Invalid stats_id[%d] = %d for model_id = %d\n", i, stat_ids[i],
				model_id);
			return -EINVAL;
		}

		switch (xs->fn_id) {
		case CN10K_ML_XSTATS_FN_DEVICE:
			fn = cn10k_ml_dev_xstat_get;
			break;
		case CN10K_ML_XSTATS_FN_MODEL:
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

static int
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

static int
cn10k_ml_dev_selftest(struct rte_ml_dev *dev)
{
	const struct plt_memzone *mz;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_req *req;
	uint64_t timeout_cycle;
	bool timeout;
	int ret;

	mldev = dev->data->dev_private;
	mz = plt_memzone_reserve_aligned("dev_selftest", sizeof(struct cn10k_ml_req), 0,
					 ML_CN10K_ALIGN_SIZE);
	if (mz == NULL) {
		plt_err("Could not allocate reserved memzone");
		return -ENOMEM;
	}
	req = mz->addr;

	/* Prepare load completion structure */
	memset(&req->jd, 0, sizeof(struct cn10k_ml_jd));
	req->jd.hdr.jce.w1.u64 = PLT_U64_CAST(&req->status);
	req->jd.hdr.job_type = ML_CN10K_JOB_TYPE_FIRMWARE_SELFTEST;
	req->jd.hdr.result = roc_ml_addr_ap2mlip(&mldev->roc, &req->result);
	req->jd.fw_load.flags = cn10k_ml_fw_flags_get(&mldev->fw);
	plt_write64(ML_CN10K_POLL_JOB_START, &req->status);
	plt_wmb();

	/* Enqueue firmware selftest request through scratch registers */
	timeout = true;
	timeout_cycle = plt_tsc_cycles() + ML_CN10K_CMD_TIMEOUT * plt_tsc_hz();
	roc_ml_scratch_enqueue(&mldev->roc, &req->jd);

	plt_rmb();
	do {
		if (roc_ml_scratch_is_done_bit_set(&mldev->roc) &&
		    (plt_read64(&req->status) == ML_CN10K_POLL_JOB_FINISH)) {
			timeout = false;
			break;
		}
	} while (plt_tsc_cycles() < timeout_cycle);

	/* Check firmware selftest status, clean-up and exit */
	ret = 0;
	if (timeout) {
		ret = -ETIME;
	} else {
		if (req->result.error_code.u64 != 0)
			ret = -1;
	}

	plt_memzone_free(mz);

	return ret;
}

int
cn10k_ml_model_load(struct rte_ml_dev *dev, struct rte_ml_model_params *params, uint16_t *model_id)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;

	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	size_t model_scratch_size;
	size_t model_stats_size;
	size_t model_data_size;
	size_t model_info_size;
	uint8_t *base_dma_addr;
	uint16_t scratch_pages;
	uint16_t wb_pages;
	uint64_t mz_size;
	uint16_t idx;
	bool found;
	int qp_id;
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
	model_scratch_size = PLT_ALIGN_CEIL(metadata->model.ddr_scratch_range_end -
						    metadata->model.ddr_scratch_range_start + 1,
					    ML_CN10K_ALIGN_SIZE);
	model_data_size = PLT_ALIGN_CEIL(model_data_size, ML_CN10K_ALIGN_SIZE);
	model_info_size = sizeof(struct rte_ml_model_info) +
			  metadata->model.num_input * sizeof(struct rte_ml_io_info) +
			  metadata->model.num_output * sizeof(struct rte_ml_io_info);
	model_info_size = PLT_ALIGN_CEIL(model_info_size, ML_CN10K_ALIGN_SIZE);
	model_stats_size = (dev->data->nb_queue_pairs + 1) * sizeof(struct cn10k_ml_model_stats);

	mz_size = PLT_ALIGN_CEIL(sizeof(struct cn10k_ml_model), ML_CN10K_ALIGN_SIZE) +
		  2 * model_data_size + model_scratch_size + model_info_size +
		  PLT_ALIGN_CEIL(sizeof(struct cn10k_ml_req), ML_CN10K_ALIGN_SIZE) +
		  model_stats_size;

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
	model->addr.scratch_base_addr = PLT_PTR_ADD(base_dma_addr, 2 * model_data_size);

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
	model->info = PLT_PTR_ADD(model->addr.scratch_base_addr, model_scratch_size);
	cn10k_ml_model_info_set(dev, model);

	/* Set slow-path request address and state */
	model->req = PLT_PTR_ADD(model->info, model_info_size);

	/* Reset burst and sync stats */
	model->burst_stats = PLT_PTR_ADD(
		model->req, PLT_ALIGN_CEIL(sizeof(struct cn10k_ml_req), ML_CN10K_ALIGN_SIZE));
	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs + 1; qp_id++) {
		model->burst_stats[qp_id].hw_latency_tot = 0;
		model->burst_stats[qp_id].hw_latency_min = UINT64_MAX;
		model->burst_stats[qp_id].hw_latency_max = 0;
		model->burst_stats[qp_id].fw_latency_tot = 0;
		model->burst_stats[qp_id].fw_latency_min = UINT64_MAX;
		model->burst_stats[qp_id].fw_latency_max = 0;
		model->burst_stats[qp_id].hw_reset_count = 0;
		model->burst_stats[qp_id].fw_reset_count = 0;
		model->burst_stats[qp_id].dequeued_count = 0;
	}
	model->sync_stats =
		PLT_PTR_ADD(model->burst_stats,
			    dev->data->nb_queue_pairs * sizeof(struct cn10k_ml_model_stats));

	plt_spinlock_init(&model->lock);
	model->state = ML_CN10K_MODEL_STATE_LOADED;
	dev->data->models[idx] = model;
	mldev->nb_models_loaded++;

	/* Update xstats names */
	cn10k_ml_xstats_model_name_update(dev, idx);

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
	mldev->nb_models_unloaded++;

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
	req->result.error_code.u64 = 0x0;
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
			if (req->result.error_code.u64 == 0)
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
			if (ret == 0) {
				model->state = ML_CN10K_MODEL_STATE_STARTED;
				mldev->nb_models_started++;
			} else {
				model->state = ML_CN10K_MODEL_STATE_UNKNOWN;
			}

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

	if (ret < 0) { /* Call unload to update model and FW state, ignore error */
		rte_ml_model_stop(dev->data->dev_id, model_id);
	} else {
		if (mldev->cache_model_data && roc_model_is_cn10ka())
			ret = cn10k_ml_cache_model_data(dev, model_id);
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
	req->result.error_code.u64 = 0x0;
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
			if (req->result.error_code.u64 == 0x0)
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
			mldev->nb_models_stopped++;
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
cn10k_ml_io_quantize(struct rte_ml_dev *dev, uint16_t model_id, struct rte_ml_buff_seg **dbuffer,
		     struct rte_ml_buff_seg **qbuffer)
{
	struct cn10k_ml_model *model;
	uint8_t model_input_type;
	uint8_t *lcl_dbuffer;
	uint8_t *lcl_qbuffer;
	uint8_t input_type;
	float qscale;
	uint32_t i;
	uint32_t j;
	int ret;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	lcl_dbuffer = dbuffer[0]->addr;
	lcl_qbuffer = qbuffer[0]->addr;

	for (i = 0; i < model->metadata.model.num_input; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			input_type = model->metadata.input1[i].input_type;
			model_input_type = model->metadata.input1[i].model_input_type;
			qscale = model->metadata.input1[i].qscale;
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;
			input_type = model->metadata.input2[j].input_type;
			model_input_type = model->metadata.input2[j].model_input_type;
			qscale = model->metadata.input2[j].qscale;
		}

		if (input_type == model_input_type) {
			rte_memcpy(lcl_qbuffer, lcl_dbuffer, model->addr.input[i].sz_d);
		} else {
			switch (model->metadata.input1[i].model_input_type) {
			case RTE_ML_IO_TYPE_INT8:
				ret = rte_ml_io_float32_to_int8(qscale,
								model->addr.input[i].nb_elements,
								lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT8:
				ret = rte_ml_io_float32_to_uint8(qscale,
								 model->addr.input[i].nb_elements,
								 lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_INT16:
				ret = rte_ml_io_float32_to_int16(qscale,
								 model->addr.input[i].nb_elements,
								 lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT16:
				ret = rte_ml_io_float32_to_uint16(qscale,
								  model->addr.input[i].nb_elements,
								  lcl_dbuffer, lcl_qbuffer);
				break;
			case RTE_ML_IO_TYPE_FP16:
				ret = rte_ml_io_float32_to_float16(model->addr.input[i].nb_elements,
								   lcl_dbuffer, lcl_qbuffer);
				break;
			default:
				plt_err("Unsupported model_input_type[%u] : %u", i,
					model->metadata.input1[i].model_input_type);
				ret = -ENOTSUP;
			}
			if (ret < 0)
				return ret;
		}

		lcl_dbuffer += model->addr.input[i].sz_d;
		lcl_qbuffer += model->addr.input[i].sz_q;
	}

	return 0;
}

static int
cn10k_ml_io_dequantize(struct rte_ml_dev *dev, uint16_t model_id, struct rte_ml_buff_seg **qbuffer,
		       struct rte_ml_buff_seg **dbuffer)
{
	struct cn10k_ml_model *model;
	uint8_t model_output_type;
	uint8_t *lcl_qbuffer;
	uint8_t *lcl_dbuffer;
	uint8_t output_type;
	float dscale;
	uint32_t i;
	uint32_t j;
	int ret;

	model = dev->data->models[model_id];

	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}

	lcl_dbuffer = dbuffer[0]->addr;
	lcl_qbuffer = qbuffer[0]->addr;

	for (i = 0; i < model->metadata.model.num_output; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			output_type = model->metadata.output1[i].output_type;
			model_output_type = model->metadata.output1[i].model_output_type;
			dscale = model->metadata.output1[i].dscale;
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;
			output_type = model->metadata.output2[j].output_type;
			model_output_type = model->metadata.output2[j].model_output_type;
			dscale = model->metadata.output2[j].dscale;
		}

		if (output_type == model_output_type) {
			rte_memcpy(lcl_dbuffer, lcl_qbuffer, model->addr.output[i].sz_q);
		} else {
			switch (model->metadata.output1[i].model_output_type) {
			case RTE_ML_IO_TYPE_INT8:
				ret = rte_ml_io_int8_to_float32(dscale,
								model->addr.output[i].nb_elements,
								lcl_qbuffer, lcl_dbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT8:
				ret = rte_ml_io_uint8_to_float32(dscale,
								 model->addr.output[i].nb_elements,
								 lcl_qbuffer, lcl_dbuffer);
				break;
			case RTE_ML_IO_TYPE_INT16:
				ret = rte_ml_io_int16_to_float32(dscale,
								 model->addr.output[i].nb_elements,
								 lcl_qbuffer, lcl_dbuffer);
				break;
			case RTE_ML_IO_TYPE_UINT16:
				ret = rte_ml_io_uint16_to_float32(dscale,
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
					model->metadata.output1[i].model_output_type);
				ret = -ENOTSUP;
			}
			if (ret < 0)
				return ret;
		}

		lcl_qbuffer += model->addr.output[i].sz_q;
		lcl_dbuffer += model->addr.output[i].sz_d;
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

static __rte_always_inline void
cn10k_ml_result_update(struct rte_ml_dev *dev, int qp_id, struct cn10k_ml_result *result,
		       struct rte_ml_op *op)
{
	struct cn10k_ml_model_stats *stats;
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_qp *qp;
	uint64_t hw_latency;
	uint64_t fw_latency;

	if (likely(result->error_code.u64 == 0)) {
		model = dev->data->models[op->model_id];
		if (likely(qp_id >= 0)) {
			qp = dev->data->queue_pairs[qp_id];
			qp->stats.dequeued_count++;
			stats = &model->burst_stats[qp_id];
		} else {
			stats = model->sync_stats;
		}

		if (unlikely(stats->dequeued_count == stats->hw_reset_count)) {
			stats->hw_latency_min = UINT64_MAX;
			stats->hw_latency_max = 0;
		}

		if (unlikely(stats->dequeued_count == stats->fw_reset_count)) {
			stats->fw_latency_min = UINT64_MAX;
			stats->fw_latency_max = 0;
		}

		hw_latency = result->stats.hw_end - result->stats.hw_start;
		fw_latency = result->stats.fw_end - result->stats.fw_start - hw_latency;

		stats->hw_latency_tot += hw_latency;
		stats->hw_latency_min = PLT_MIN(stats->hw_latency_min, hw_latency);
		stats->hw_latency_max = PLT_MAX(stats->hw_latency_max, hw_latency);
		stats->fw_latency_tot += fw_latency;
		stats->fw_latency_min = PLT_MIN(stats->fw_latency_min, fw_latency);
		stats->fw_latency_max = PLT_MAX(stats->fw_latency_max, fw_latency);
		stats->dequeued_count++;

		op->impl_opaque = result->error_code.u64;
		op->status = RTE_ML_OP_STATUS_SUCCESS;
	} else {
		if (likely(qp_id >= 0)) {
			qp = dev->data->queue_pairs[qp_id];
			qp->stats.dequeue_err_count++;
		}

		/* Handle driver error */
		if (result->error_code.s.etype == ML_ETYPE_DRIVER) {
			mldev = dev->data->dev_private;

			/* Check for exception */
			if ((roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_EXCEPTION_SP_C0) != 0) ||
			    (roc_ml_reg_read64(&mldev->roc, ML_SCRATCH_EXCEPTION_SP_C1) != 0))
				result->error_code.s.stype = ML_DRIVER_ERR_EXCEPTION;
			else if ((roc_ml_reg_read64(&mldev->roc, ML_CORE_INT_LO) != 0) ||
				 (roc_ml_reg_read64(&mldev->roc, ML_CORE_INT_HI) != 0))
				result->error_code.s.stype = ML_DRIVER_ERR_FW_ERROR;
			else
				result->error_code.s.stype = ML_DRIVER_ERR_UNKNOWN;
		}

		op->impl_opaque = result->error_code.u64;
		op->status = RTE_ML_OP_STATUS_ERROR;
	}

	op->user_ptr = result->user_ptr;
}

__rte_hot uint16_t
cn10k_ml_enqueue_burst(struct rte_ml_dev *dev, uint16_t qp_id, struct rte_ml_op **ops,
		       uint16_t nb_ops)
{
	struct cn10k_ml_queue *queue;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_req *req;
	struct cn10k_ml_qp *qp;
	struct rte_ml_op *op;

	uint16_t count;
	uint64_t head;
	bool enqueued;

	mldev = dev->data->dev_private;
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

	mldev->set_poll_addr(qp, req, head);
	cn10k_ml_prep_fp_job_descriptor(dev, req, op);

	memset(&req->result, 0, sizeof(struct cn10k_ml_result));
	req->result.error_code.s.etype = ML_ETYPE_UNKNOWN;
	req->result.user_ptr = op->user_ptr;
	mldev->set_enq_barrier();

	mldev->set_poll_ptr(&mldev->roc, req);
	enqueued = mldev->ml_jcmdq_enqueue(&mldev->roc, &req->jcmd);
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
	struct cn10k_ml_queue *queue;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_req *req;
	struct cn10k_ml_qp *qp;

	uint64_t status;
	uint16_t count;
	uint64_t tail;

	mldev = dev->data->dev_private;
	qp = dev->data->queue_pairs[qp_id];
	queue = &qp->queue;

	tail = queue->tail;
	nb_ops = PLT_MIN(nb_ops, queue_pending_count(queue->head, tail, qp->nb_desc));
	count = 0;

	if (unlikely(nb_ops == 0))
		goto empty_or_active;

dequeue_req:
	req = &queue->reqs[tail];
	status = mldev->get_poll_ptr(&mldev->roc, req);
	if (unlikely(status != ML_CN10K_POLL_JOB_FINISH)) {
		if (plt_tsc_cycles() < req->timeout)
			goto empty_or_active;
		else /* Timeout, set indication of driver error */
			req->result.error_code.s.etype = ML_ETYPE_DRIVER;
	}

	mldev->set_deq_barrier();
	cn10k_ml_result_update(dev, qp_id, &req->result, req->op);
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
cn10k_ml_inference_sync(struct rte_ml_dev *dev, struct rte_ml_op *op)
{
	struct cn10k_ml_model *model;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_req *req;
	bool timeout;
	int ret = 0;

	mldev = dev->data->dev_private;
	model = dev->data->models[op->model_id];
	req = model->req;

	cn10k_ml_set_sync_addr(mldev, req);
	cn10k_ml_prep_fp_job_descriptor(dev, req, op);

	memset(&req->result, 0, sizeof(struct cn10k_ml_result));
	req->result.error_code.s.etype = ML_ETYPE_UNKNOWN;
	req->result.user_ptr = op->user_ptr;

	mldev->set_poll_ptr(&mldev->roc, req);
	req->jcmd.w1.s.jobptr = PLT_U64_CAST(&req->jd);

	timeout = true;
	req->timeout = plt_tsc_cycles() + ML_CN10K_CMD_TIMEOUT * plt_tsc_hz();
	do {
		if (mldev->ml_jcmdq_enqueue(&mldev->roc, &req->jcmd)) {
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
		if (mldev->get_poll_ptr(&mldev->roc, req) == ML_CN10K_POLL_JOB_FINISH) {
			timeout = false;
			break;
		}
	} while (plt_tsc_cycles() < req->timeout);

	if (timeout)
		ret = -ETIME;
	else
		cn10k_ml_result_update(dev, -1, &req->result, req->op);

error_enqueue:
	return ret;
}

struct rte_ml_dev_ops cn10k_ml_ops = {
	/* Device control ops */
	.dev_info_get = cn10k_ml_dev_info_get,
	.dev_configure = cn10k_ml_dev_configure,
	.dev_close = cn10k_ml_dev_close,
	.dev_start = cn10k_ml_dev_start,
	.dev_stop = cn10k_ml_dev_stop,
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
