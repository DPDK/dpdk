/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <dlpack/dlpack.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <mldev_utils.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_ops.h"
#include "cnxk_ml_xstats.h"

/* ML model macros */
#define MVTVM_ML_MODEL_MEMZONE_NAME "ml_mvtvm_model_mz"

__rte_hot static void
mvtvm_ml_set_poll_addr(struct cnxk_ml_req *req)
{
	req->status = &req->mvtvm_req.status;
}

void
mvtvm_ml_model_xstat_name_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
			      uint16_t stat_id, uint16_t entry, char *suffix)
{
	snprintf(cnxk_mldev->xstats.entries[stat_id].map.name,
		 sizeof(cnxk_mldev->xstats.entries[stat_id].map.name), "%s-%s-%s",
		 model->mvtvm.metadata.model.name, model_xstats[entry].name, suffix);
}

#define ML_AVG_FOREACH_QP_MVTVM(cnxk_mldev, model, qp_id, value, count)                            \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {        \
			value += model->mvtvm.burst_xstats[qp_id].tvm_rt_latency_tot;              \
			count += model->mvtvm.burst_xstats[qp_id].dequeued_count -                 \
				 model->mvtvm.burst_xstats[qp_id].tvm_rt_reset_count;              \
		}                                                                                  \
		if (count != 0)                                                                    \
			value = value / count;                                                     \
	} while (0)

#define ML_MIN_FOREACH_QP_MVTVM(cnxk_mldev, model, qp_id, value, count)                            \
	do {                                                                                       \
		value = UINT64_MAX;                                                                \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {        \
			value = PLT_MIN(value,                                                     \
					model->mvtvm.burst_xstats[qp_id].tvm_rt_latency_min);      \
			count += model->mvtvm.burst_xstats[qp_id].dequeued_count -                 \
				 model->mvtvm.burst_xstats[qp_id].tvm_rt_reset_count;              \
		}                                                                                  \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

#define ML_MAX_FOREACH_QP_MVTVM(cnxk_mldev, model, qp_id, value, count)                            \
	do {                                                                                       \
		value = 0;                                                                         \
		for (qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {        \
			value = PLT_MAX(value,                                                     \
					model->mvtvm.burst_xstats[qp_id].tvm_rt_latency_max);      \
			count += model->mvtvm.burst_xstats[qp_id].dequeued_count -                 \
				 model->mvtvm.burst_xstats[qp_id].tvm_rt_reset_count;              \
		}                                                                                  \
		if (count == 0)                                                                    \
			value = 0;                                                                 \
	} while (0)

uint64_t
mvtvm_ml_model_xstat_get(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
			 enum cnxk_ml_xstats_type type)
{
	uint64_t count = 0;
	uint64_t value = 0;
	uint32_t qp_id;

	switch (type) {
	case avg_rt_latency:
		ML_AVG_FOREACH_QP_MVTVM(cnxk_mldev, model, qp_id, value, count);
		break;
	case min_rt_latency:
		ML_MIN_FOREACH_QP_MVTVM(cnxk_mldev, model, qp_id, value, count);
		break;
	case max_rt_latency:
		ML_MAX_FOREACH_QP_MVTVM(cnxk_mldev, model, qp_id, value, count);
		break;
	default:
		value = 0;
	}

	return value;
}

int
mvtvm_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info)
{
	struct mvtvm_ml_dev *mvtvm_mldev;

	mvtvm_mldev = &cnxk_mldev->mvtvm_mldev;

	dev_info->max_queue_pairs = mvtvm_mldev->max_nb_qpairs;
	dev_info->max_desc = ML_MVTVM_MAX_DESC_PER_QP;
	dev_info->max_io = ML_MVTVM_MAX_INPUT_OUTPUT;
	dev_info->max_segments = ML_MVTVM_MAX_SEGMENTS;
	dev_info->align_size = RTE_CACHE_LINE_SIZE;

	return 0;
}

int
mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf)
{
	int ret;

	RTE_SET_USED(conf);

	/* Configure TVMDP library */
	ret = tvmdp_configure(cnxk_mldev->mldev->data->nb_models, rte_get_tsc_cycles);
	if (ret != 0)
		plt_err("TVMDP configuration failed, error = %d\n", ret);

	return ret;
}

int
mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev)
{
	int ret;

	RTE_SET_USED(cnxk_mldev);

	/* Close TVMDP library configuration */
	ret = tvmdp_close();
	if (ret != 0)
		plt_err("TVMDP close failed, error = %d\n", ret);

	return ret;
}

int
mvtvm_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(fp);

	return 0;
}

int
mvtvm_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
		    struct cnxk_ml_model *model)
{
	struct mvtvm_ml_model_object object[ML_MVTVM_MODEL_OBJECT_MAX];
	struct tvmrt_glow_callback *callback;
	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	size_t model_object_size = 0;
	size_t model_xstats_size = 0;
	uint16_t nb_mrvl_layers;
	uint16_t nb_llvm_layers;
	uint8_t layer_id = 0;
	uint64_t mz_size = 0;
	int ret;

	RTE_SET_USED(cnxk_mldev);

	ret = mvtvm_ml_model_blob_parse(params, object);
	if (ret != 0)
		return ret;

	model_object_size = RTE_ALIGN_CEIL(object[0].size, RTE_CACHE_LINE_MIN_SIZE) +
			    RTE_ALIGN_CEIL(object[1].size, RTE_CACHE_LINE_MIN_SIZE) +
			    RTE_ALIGN_CEIL(object[2].size, RTE_CACHE_LINE_MIN_SIZE);

	model_xstats_size =
		cnxk_mldev->mldev->data->nb_queue_pairs * sizeof(struct mvtvm_ml_model_xstats);

	mz_size += model_object_size + model_xstats_size;

	/* Allocate memzone for model object */
	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", MVTVM_ML_MODEL_MEMZONE_NAME, model->model_id);
	mz = plt_memzone_reserve_aligned(str, mz_size, 0, ML_CN10K_ALIGN_SIZE);
	if (!mz) {
		plt_err("plt_memzone_reserve failed : %s", str);
		return -ENOMEM;
	}

	/* Copy mod.so */
	model->mvtvm.object.so.addr = mz->addr;
	model->mvtvm.object.so.size = object[0].size;
	rte_memcpy(model->mvtvm.object.so.name, object[0].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.so.addr, object[0].buffer, object[0].size);
	rte_free(object[0].buffer);

	/* Copy mod.json */
	model->mvtvm.object.json.addr =
		RTE_PTR_ADD(model->mvtvm.object.so.addr,
			    RTE_ALIGN_CEIL(model->mvtvm.object.so.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.object.json.size = object[1].size;
	rte_memcpy(model->mvtvm.object.json.name, object[1].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.json.addr, object[1].buffer, object[1].size);
	rte_free(object[1].buffer);

	/* Copy mod.params */
	model->mvtvm.object.params.addr =
		RTE_PTR_ADD(model->mvtvm.object.json.addr,
			    RTE_ALIGN_CEIL(model->mvtvm.object.json.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.object.params.size = object[2].size;
	rte_memcpy(model->mvtvm.object.params.name, object[2].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.params.addr, object[2].buffer, object[2].size);
	rte_free(object[2].buffer);

	/* Get metadata - stage 1 */
	ret = tvmdp_model_metadata_get_stage1(model->mvtvm.object.json.addr,
					      model->mvtvm.object.json.size,
					      &model->mvtvm.metadata);
	if (ret != 0) {
		plt_err("TVMDP: Failed to parse metadata - stage 1, model_id = %u, error = %d",
			model->model_id, ret);
		goto error;
	}

	/* Set model fields */
	plt_strlcpy(model->name, model->mvtvm.metadata.model.name, TVMDP_NAME_STRLEN);
	model->batch_size = 1;
	model->nb_layers = model->mvtvm.metadata.model.nb_layers;

	/* Update layer info */
	nb_mrvl_layers = 0;
	nb_llvm_layers = 0;
	for (layer_id = 0; layer_id < model->mvtvm.metadata.model.nb_layers; layer_id++) {
		rte_strscpy(model->layer[layer_id].name,
			    model->mvtvm.metadata.model.layer[layer_id].name, TVMDP_NAME_STRLEN);
		if (strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "mrvl") == 0 ||
		    strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "MRVL") == 0) {
			model->layer[layer_id].type = ML_CNXK_LAYER_TYPE_MRVL;
			nb_mrvl_layers++;
		} else if (strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "llvm") == 0 ||
			   strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "LLVM") == 0) {
			model->layer[layer_id].type = ML_CNXK_LAYER_TYPE_LLVM;
			nb_llvm_layers++;
		}
	}

	if ((nb_llvm_layers == 0) && (nb_mrvl_layers == 0)) {
		plt_err("Invalid model, nb_llvm_layers = %u, nb_mrvl_layers = %u", nb_llvm_layers,
			nb_mrvl_layers);
		goto error;
	}

	/* Set model subtype */
	if ((nb_llvm_layers == 0) && (nb_mrvl_layers == 1))
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_MRVL;
	else if ((nb_llvm_layers > 0) && (nb_mrvl_layers == 0))
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_LLVM;
	else
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_HYBRID;

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_VDEV &&
	    model->subtype != ML_CNXK_MODEL_SUBTYPE_TVM_LLVM) {
		plt_err("Unsupported model sub-type");
		return -ENOTSUP;
	}

	/* Set callback function array */
	if (model->subtype != ML_CNXK_MODEL_SUBTYPE_TVM_LLVM) {
		callback = &model->mvtvm.cb;
		callback->tvmrt_glow_layer_load = cn10k_ml_layer_load;
		callback->tvmrt_glow_layer_unload = cn10k_ml_layer_unload;
		callback->tvmrt_io_alloc = cn10k_ml_io_alloc;
		callback->tvmrt_io_free = cn10k_ml_io_free;
		callback->tvmrt_malloc = cn10k_ml_malloc;
		callback->tvmrt_free = cn10k_ml_free;
		callback->tvmrt_quantize = mvtvm_ml_io_quantize;
		callback->tvmrt_dequantize = mvtvm_ml_io_dequantize;
		callback->tvmrt_inference = cn10k_ml_inference_sync;
	} else {
		callback = NULL;
	}

	/* Initialize model in TVMDP */
	ret = tvmdp_model_load(cnxk_mldev, model->model_id, (void *)(&model->mvtvm.object),
			       callback);
	if (ret != 0) {
		plt_err("TVMDP: Model load failed, model_id = %u, error = %d", model->model_id,
			ret);
		goto error;
	}

	/* Get model metadata - stage 2 */
	ret = tvmdp_model_metadata_get_stage2(model->model_id, &model->mvtvm.metadata);
	if (ret != 0) {
		plt_err("TVMDP: Failed to get metadata, model_id = %u, error = %d\n",
			model->model_id, ret);
		goto error;
	}

	/* Update model I/O data */
	mvtvm_ml_model_io_info_set(model);

	/* Set model info */
	mvtvm_ml_model_info_set(cnxk_mldev, model);

	/* Update model xstats name */
	cnxk_ml_xstats_model_name_update(cnxk_mldev, model->model_id);

	model->mvtvm.burst_xstats = RTE_PTR_ADD(
		model->mvtvm.object.params.addr,
		RTE_ALIGN_CEIL(model->mvtvm.object.params.size, RTE_CACHE_LINE_MIN_SIZE));

	for (int qp_id = 0; qp_id < cnxk_mldev->mldev->data->nb_queue_pairs; qp_id++) {
		model->mvtvm.burst_xstats[qp_id].tvm_rt_latency_tot = 0;
		model->mvtvm.burst_xstats[qp_id].tvm_rt_latency = 0;
		model->mvtvm.burst_xstats[qp_id].tvm_rt_latency_min = UINT64_MAX;
		model->mvtvm.burst_xstats[qp_id].tvm_rt_latency_max = 0;
		model->mvtvm.burst_xstats[qp_id].tvm_rt_reset_count = 0;
		model->mvtvm.burst_xstats[qp_id].dequeued_count = 0;
	}

	/* Set model specific fast path functions */
	if (model->subtype == ML_CNXK_MODEL_SUBTYPE_TVM_MRVL) {
		model->enqueue_single = cn10k_ml_enqueue_single;
		model->result_update = cn10k_ml_result_update;
		model->set_error_code = cn10k_ml_set_error_code;
		model->set_poll_addr = cn10k_ml_set_poll_addr;
	} else {
		model->enqueue_single = mvtvm_ml_enqueue_single;
		model->result_update = mvtvm_ml_result_update;
		model->set_error_code = mvtvm_ml_set_error_code;
		model->set_poll_addr = mvtvm_ml_set_poll_addr;
	}

	return 0;

error:
	rte_memzone_free(mz);

	return ret;
}

int
mvtvm_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	int ret;

	RTE_SET_USED(cnxk_mldev);

	/* Initialize model in TVMDP */
	ret = tvmdp_model_unload(model->model_id);
	if (ret != 0) {
		plt_err("TVMDP: Model unload failed, model_id = %u, error = %d", model->model_id,
			ret);
		return ret;
	}

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", MVTVM_ML_MODEL_MEMZONE_NAME, model->model_id);
	mz = rte_memzone_lookup(str);
	if (mz == NULL) {
		plt_err("Memzone lookup failed for TVM model: model_id = %u, mz = %s",
			model->model_id, str);
		return -EINVAL;
	}

	return plt_memzone_free(mz);
}

int
mvtvm_ml_model_start(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct cnxk_ml_layer *layer;

	uint16_t layer_id = 0;
	int ret = 0;

next_layer:
	layer = &model->layer[layer_id];
	if (layer->type == ML_CNXK_LAYER_TYPE_MRVL) {
		ret = cn10k_ml_layer_start(cnxk_mldev, model->model_id, layer->name);
		if (ret != 0) {
			plt_err("Layer start failed, model_id = %u, layer_name = %s, error = %d",
				model->model_id, layer->name, ret);
			return ret;
		}
	}
	layer_id++;

	if (layer_id < model->nb_layers)
		goto next_layer;

	return 0;
}

int
mvtvm_ml_model_stop(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct cnxk_ml_layer *layer;

	uint16_t layer_id = 0;
	int ret = 0;

next_layer:
	layer = &model->layer[layer_id];
	if (layer->type == ML_CNXK_LAYER_TYPE_MRVL) {
		ret = cn10k_ml_layer_stop(cnxk_mldev, model->model_id, layer->name);
		if (ret != 0) {
			plt_err("Layer stop failed, model_id = %u, layer_name = %s, error = %d",
				model->model_id, layer->name, ret);
			return ret;
		}
	}
	layer_id++;

	if (layer_id < model->nb_layers)
		goto next_layer;

	return 0;
}

int
mvtvm_ml_io_quantize(void *device, uint16_t model_id, const char *layer_name,
		     const DLTensor **deq_tensor, void *qbuffer)
{
	struct cnxk_ml_io_info *info = NULL;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	uint16_t layer_id = 0;
	uint8_t *lcl_dbuffer;
	uint8_t *lcl_qbuffer;
	uint32_t i;
	int ret;

#ifdef CNXK_ML_DEV_DEBUG
	if ((device == NULL) || (deq_tensor == NULL) || (qbuffer == NULL))
		return -EINVAL;
#endif

	cnxk_mldev = (struct cnxk_ml_dev *)device;

	model = cnxk_mldev->mldev->data->models[model_id];
#ifdef CNXK_ML_DEV_DEBUG
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}
#endif

	/* Get layer id */
	for (layer_id = 0; layer_id < model->mvtvm.metadata.model.nb_layers; layer_id++) {
		if (strcmp(model->layer[layer_id].name, layer_name) == 0)
			break;
	}

#ifdef CNXK_ML_DEV_DEBUG
	if (layer_id == model->mvtvm.metadata.model.nb_layers) {
		plt_err("Invalid layer name: %s", layer_name);
		return -EINVAL;
	}

	if (model->layer[layer_id].type != ML_CNXK_LAYER_TYPE_MRVL) {
		plt_err("Invalid layer name / type: %s", layer_name);
		return -EINVAL;
	}
#endif

	info = &model->layer[layer_id].info;
	lcl_qbuffer = (uint8_t *)qbuffer;

	for (i = 0; i < info->nb_inputs; i++) {
		lcl_dbuffer = PLT_PTR_ADD(deq_tensor[i]->data, deq_tensor[i]->byte_offset);

		ret = cnxk_ml_io_quantize_single(&info->input[i], lcl_dbuffer, lcl_qbuffer);
		if (ret < 0)
			return ret;

		lcl_qbuffer += info->input[i].sz_q;
	}

	return 0;
}

int
mvtvm_ml_io_dequantize(void *device, uint16_t model_id, const char *layer_name, void *qbuffer,
		       const DLTensor **deq_tensor)
{
	struct cnxk_ml_io_info *info = NULL;
	struct cnxk_ml_dev *cnxk_mldev;
	struct cnxk_ml_model *model;
	uint16_t layer_id = 0;
	uint8_t *lcl_dbuffer;
	uint8_t *lcl_qbuffer;
	uint32_t i;
	int ret;

#ifdef CNXK_ML_DEV_DEBUG
	if ((device == NULL) || (deq_tensor == NULL) || (qbuffer == NULL))
		return -EINVAL;
#endif

	cnxk_mldev = (struct cnxk_ml_dev *)device;

	model = cnxk_mldev->mldev->data->models[model_id];
#ifdef CNXK_ML_DEV_DEBUG
	if (model == NULL) {
		plt_err("Invalid model_id = %u", model_id);
		return -EINVAL;
	}
#endif

	for (layer_id = 0; layer_id < model->mvtvm.metadata.model.nb_layers; layer_id++) {
		if (strcmp(model->layer[layer_id].name, layer_name) == 0)
			break;
	}

#ifdef CNXK_ML_DEV_DEBUG
	if (layer_id == model->mvtvm.metadata.model.nb_layers) {
		plt_err("Invalid layer name: %s", layer_name);
		return -EINVAL;
	}

	if (model->layer[layer_id].type != ML_CNXK_LAYER_TYPE_MRVL) {
		plt_err("Invalid layer name / type: %s", layer_name);
		return -EINVAL;
	}
#endif

	info = &model->layer[layer_id].info;
	lcl_qbuffer = (uint8_t *)qbuffer;

	for (i = 0; i < info->nb_outputs; i++) {
		lcl_dbuffer = PLT_PTR_ADD(deq_tensor[i]->data, deq_tensor[i]->byte_offset);

		ret = cnxk_ml_io_dequantize_single(&info->output[i], lcl_qbuffer, lcl_dbuffer);
		if (ret < 0)
			return ret;

		lcl_qbuffer += info->output[i].sz_q;
	}

	return 0;
}

static int
mvtvm_ml_model_run(struct cnxk_ml_model *model, struct rte_ml_op *op, struct cnxk_ml_req *req)
{
	uint8_t i;

	rte_memcpy(req->mvtvm_req.input_tensor, model->mvtvm.input_tensor,
		   model->mvtvm.metadata.model.num_input * sizeof(DLTensor));
	for (i = 0; i < model->mvtvm.metadata.model.num_input; i++) {
		req->mvtvm_req.input_tensor[i].data = op->input[i]->addr;
		req->mvtvm_req.input_tensor[i].byte_offset = 0;
	}

	rte_memcpy(req->mvtvm_req.output_tensor, model->mvtvm.output_tensor,
		   model->mvtvm.metadata.model.num_output * sizeof(DLTensor));
	for (i = 0; i < model->mvtvm.metadata.model.num_output; i++) {
		req->mvtvm_req.output_tensor[i].data = op->output[i]->addr;
		req->mvtvm_req.output_tensor[i].byte_offset = 0;
	}

	tvmdp_model_run(model->model_id, model->mvtvm.metadata.model.num_input,
			req->mvtvm_req.input_tensor, model->mvtvm.metadata.model.num_output,
			req->mvtvm_req.output_tensor, &req->mvtvm_req.result,
			&req->mvtvm_req.status);

	plt_write64(ML_CNXK_POLL_JOB_FINISH, req->status);

	return 0;
}

__rte_hot void
mvtvm_ml_set_error_code(struct cnxk_ml_req *req, uint64_t etype, uint64_t stype)
{
	RTE_SET_USED(stype);

	req->mvtvm_req.result.error_code = etype;
}

__rte_hot bool
mvtvm_ml_enqueue_single(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_op *op, uint16_t layer_id,
			struct cnxk_ml_qp *qp, uint64_t head)
{
	struct cnxk_ml_model *model;
	struct cnxk_ml_queue *queue;
	struct cnxk_ml_req *req;

	RTE_SET_USED(layer_id);

	queue = &qp->queue;
	req = &queue->reqs[head];
	model = cnxk_mldev->mldev->data->models[op->model_id];

	model->set_poll_addr(req);
	memset(&req->mvtvm_req.result, 0, sizeof(struct mvtvm_ml_result));
	req->mvtvm_req.result.error_code = 0x0;
	req->mvtvm_req.result.user_ptr = op->user_ptr;

	cnxk_ml_set_poll_ptr(req);
	mvtvm_ml_model_run(model, op, req);
	req->timeout = plt_tsc_cycles() + queue->wait_cycles;
	req->op = op;

	return true;
}

__rte_hot void
mvtvm_ml_result_update(struct cnxk_ml_dev *cnxk_mldev, int qp_id, void *request)
{
	struct mvtvm_ml_model_xstats *xstats;
	struct mvtvm_ml_result *result;
	struct cnxk_ml_model *model;
	struct cnxk_ml_req *req;
	uint64_t tvm_rt_latency;
	struct cnxk_ml_qp *qp;
	struct rte_ml_op *op;

	req = (struct cnxk_ml_req *)request;
	result = &req->mvtvm_req.result;
	op = req->op;
	qp = cnxk_mldev->mldev->data->queue_pairs[qp_id];
	op->impl_opaque = result->error_code;

	if (likely(result->error_code == 0)) {
		qp->stats.dequeued_count++;
		op->status = RTE_ML_OP_STATUS_SUCCESS;

		model = cnxk_mldev->mldev->data->models[op->model_id];
		xstats = &model->mvtvm.burst_xstats[qp_id];

		if (unlikely(xstats->dequeued_count == xstats->tvm_rt_reset_count)) {
			xstats->tvm_rt_latency_min = UINT64_MAX;
			xstats->tvm_rt_latency_max = 0;
		}
		tvm_rt_latency = result->stats.end_ns - result->stats.start_ns;
		xstats->tvm_rt_latency = tvm_rt_latency;
		xstats->tvm_rt_latency_tot += tvm_rt_latency;
		xstats->tvm_rt_latency_min = RTE_MIN(xstats->tvm_rt_latency_min, tvm_rt_latency);
		xstats->tvm_rt_latency_max = RTE_MAX(xstats->tvm_rt_latency_max, tvm_rt_latency);
		xstats->dequeued_count++;
	} else {
		qp->stats.dequeue_err_count++;
		op->status = RTE_ML_OP_STATUS_ERROR;
	}
}
