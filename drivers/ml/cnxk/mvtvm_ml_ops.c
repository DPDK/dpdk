/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <dlpack/dlpack.h>
#include <jansson.h>

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

/* Shared memory file descriptor name */
#define ML_MODEL_SHMFD_NAME "mvtvm_shmfd"

/* Shared memory file descriptor path */
#define ML_MODEL_SHMFD_PATH "/proc/%d/fd/%d"

static int
mvtvm_ml_tvm_func_get(struct cnxk_ml_model *model, TVMModuleHandle module, const char *name,
		      TVMFunctionHandle *func)
{
	int ret;

	ret = TVMModGetFunction(module, name, 0, func);
	if (ret != 0) {
		plt_err("Model load failed, model_id = %u, ret = %d, msg = %s", model->model_id,
			ret, TVMGetLastError());
		return ret;
	}

	if (*func == NULL) {
		ret = -ENOENT;
		plt_err("Model load failed, model_id = %u, function '%s' not found",
			model->model_id, name);
	}

	return ret;
}

static int
mvtvm_ml_tvm_func_call(struct cnxk_ml_model *model, TVMFunctionHandle func, const char *name,
		       TVMValue *values, int *types, int num_args, TVMValue *ret_val, int *ret_type,
		       int ret_type_code)
{
	int ret;

	ret = TVMFuncCall(func, values, types, num_args, ret_val, ret_type);
	if (ret != 0) {
		plt_err("Error calling TVM function '%s', model_id = %u, ret = %d, msg = %s", name,
			model->model_id, ret, TVMGetLastError());
		return ret;
	}

	if (*ret_type != ret_type_code) {
		ret = -EINVAL;
		plt_err("TVM function '%s' returned unexpected type, model_id = %u, expected = %d, "
			"actual = %d",
			name, model->model_id, ret_type_code, *ret_type);
	}

	return ret;
}

static void
mvtvm_ml_tvm_func_free(TVMFunctionHandle *func)
{
	if ((func != NULL) && (*func != NULL)) {
		TVMFuncFree(*func);
		*func = NULL;
	}
}

static void
mvtvm_ml_tvm_mod_free(TVMModuleHandle *mod)
{
	if ((mod != NULL) && (*mod != NULL)) {
		TVMModFree(*mod);
		*mod = NULL;
	}
}

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
		 sizeof(cnxk_mldev->xstats.entries[stat_id].map.name), "%s-%s-%s", model->name,
		 model_xstats[entry].name, suffix);
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
	dev_info->max_io = ML_CNXK_MODEL_MAX_INPUT_OUTPUT;
	dev_info->max_segments = ML_MVTVM_MAX_SEGMENTS;
	dev_info->align_size = RTE_CACHE_LINE_SIZE;

	return 0;
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
	char path[PATH_MAX];
	const struct plt_memzone *mz;
	size_t model_object_size = 0;
	size_t model_xstats_size = 0;
	uint64_t mz_size = 0;
	TVMFunctionHandle create_fn = NULL;
	TVMFunctionHandle register_cb_fn = NULL;
	TVMModuleHandle module_so = NULL;
	TVMByteArray tvm_params;
	TVMValue ret_value = {0};
	TVMValue arg_values[4] = {0};
	TVMValue tvm_arg_values[1] = {0};
	int ret_type = kTVMNullptr;
	int arg_types[4] = {0};
	int tvm_arg_types[1] = {0};
	DLDevice device;
	int ret;

	RTE_SET_USED(cnxk_mldev);
	model->mvtvm.fd = -1;

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
	model->mvtvm.so.buffer = mz->addr;
	model->mvtvm.so.size = object[0].size;
	rte_memcpy(model->mvtvm.so.name, object[0].name, RTE_ML_STR_MAX);
	rte_memcpy(model->mvtvm.so.buffer, object[0].buffer, object[0].size);
	rte_free(object[0].buffer);

	/* Copy mod.json */
	model->mvtvm.json.buffer =
		RTE_PTR_ADD(model->mvtvm.so.buffer,
			    RTE_ALIGN_CEIL(model->mvtvm.so.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.json.size = object[1].size;
	rte_memcpy(model->mvtvm.json.name, object[1].name, RTE_ML_STR_MAX);
	rte_memcpy(model->mvtvm.json.buffer, object[1].buffer, object[1].size);
	rte_free(object[1].buffer);

	/* Copy mod.params */
	model->mvtvm.params.buffer =
		RTE_PTR_ADD(model->mvtvm.json.buffer,
			    RTE_ALIGN_CEIL(model->mvtvm.json.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.params.size = object[2].size;
	rte_memcpy(model->mvtvm.params.name, object[2].name, RTE_ML_STR_MAX);
	rte_memcpy(model->mvtvm.params.buffer, object[2].buffer, object[2].size);
	rte_free(object[2].buffer);

	ret = mvtvm_ml_model_json_parse(model);
	if (ret != 0)
		goto error;

	if (cnxk_mldev->type == CNXK_ML_DEV_TYPE_VDEV &&
	    model->subtype != ML_CNXK_MODEL_SUBTYPE_TVM_LLVM) {
		plt_err("Unsupported model sub-type");
		ret = -ENOTSUP;
		goto error;
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

	/* Initialize model in TVM runtime */
	if (model->mvtvm.graph_module != NULL) {
		ret = -EBUSY;
		plt_err("TVM runtime: Model load failed, model_id = %u, error = %d",
			model->model_id, ret);
		goto error;
	}

	snprintf(path, sizeof(path), "%s_%d_%u", ML_MODEL_SHMFD_NAME, getpid(), model->model_id);
	model->mvtvm.fd = memfd_create(path, 0);
	if (model->mvtvm.fd < 0) {
		ret = (errno == 0) ? -EIO : -errno;
		plt_err("TVM runtime: Model load failed, model_id = %u, error = %d",
			model->model_id, ret);
		goto error;
	}

	if (write(model->mvtvm.fd, model->mvtvm.so.buffer, model->mvtvm.so.size) !=
	    (ssize_t)model->mvtvm.so.size) {
		ret = (errno == 0) ? -EIO : -errno;
		plt_err("TVM runtime: Model load failed, model_id = %u, error = %d",
			model->model_id, ret);
		goto error;
	}

	if (lseek(model->mvtvm.fd, 0, SEEK_SET) < 0) {
		ret = (errno == 0) ? -EIO : -errno;
		plt_err("TVM runtime: Model load failed, model_id = %u, error = %d",
			model->model_id, ret);
		goto error;
	}

	snprintf(path, sizeof(path), ML_MODEL_SHMFD_PATH, getpid(), model->mvtvm.fd);
	ret = TVMModLoadFromFile(path, "so", &module_so);
	if (ret != 0) {
		plt_err("TVM runtime: Model load failed, model_id = %u, ret = %d, msg = %s",
			model->model_id, ret, TVMGetLastError());
		goto error;
	}

	/* Set device info */
	device.device_type = kDLCPU;
	device.device_id = 0;

	if (callback != NULL) {
		ret = mvtvm_ml_tvm_func_get(model, module_so, "register_cb", &register_cb_fn);
		if (ret != 0)
			goto error;

		arg_values[0].v_handle = callback;
		arg_types[0] = kTVMOpaqueHandle;
		arg_values[1].v_handle = cnxk_mldev;
		arg_types[1] = kTVMOpaqueHandle;
		arg_values[2].v_int64 = model->model_id;
		arg_types[2] = kDLInt;

		ret = mvtvm_ml_tvm_func_call(model, register_cb_fn, "register_cb", arg_values,
					     arg_types, 3, &ret_value, &ret_type, kTVMNullptr);
		if (ret != 0)
			goto error;
	}

	ret = TVMFuncGetGlobal("tvm.graph_executor.create", &create_fn);
	if (ret != 0) {
		plt_err("TVM runtime: Model load failed, model_id = %u, ret = %d, msg = %s",
			model->model_id, ret, TVMGetLastError());
		goto error;
	}

	arg_values[0].v_str = (const char *)model->mvtvm.json.buffer;
	arg_types[0] = kTVMStr;
	arg_values[1].v_handle = module_so;
	arg_types[1] = kTVMModuleHandle;
	arg_values[2].v_int64 = device.device_type;
	arg_types[2] = kDLInt;
	arg_values[3].v_int64 = device.device_id;
	arg_types[3] = kDLInt;

	ret = mvtvm_ml_tvm_func_call(model, create_fn, "tvm.graph_executor.create", arg_values,
				     arg_types, 4, &ret_value, &ret_type, kTVMModuleHandle);
	if (ret != 0)
		goto error;
	model->mvtvm.graph_module = ret_value.v_handle;

	ret = mvtvm_ml_tvm_func_get(model, model->mvtvm.graph_module, "load_params",
				    &model->mvtvm.load_params);
	if (ret != 0)
		goto error;
	ret = mvtvm_ml_tvm_func_get(model, model->mvtvm.graph_module, "set_input_zero_copy",
				    &model->mvtvm.set_input_zero_copy);
	if (ret != 0)
		goto error;
	ret = mvtvm_ml_tvm_func_get(model, model->mvtvm.graph_module, "set_output_zero_copy",
				    &model->mvtvm.set_output_zero_copy);
	if (ret != 0)
		goto error;
	ret = mvtvm_ml_tvm_func_get(model, model->mvtvm.graph_module, "run", &model->mvtvm.run);
	if (ret != 0)
		goto error;

	mvtvm_ml_tvm_func_free(&register_cb_fn);
	mvtvm_ml_tvm_mod_free(&module_so);

	/* Load model parameters into TVM runtime */
	tvm_params.data = (const char *)model->mvtvm.params.buffer;
	tvm_params.size = model->mvtvm.params.size;
	tvm_arg_values[0].v_handle = &tvm_params;
	tvm_arg_types[0] = kTVMBytes;

	ret = mvtvm_ml_tvm_func_call(model, model->mvtvm.load_params, "load_params", tvm_arg_values,
				     tvm_arg_types, 1, &ret_value, &ret_type, kTVMNullptr);
	if (ret != 0)
		goto error;

	/* Update model I/O data */
	mvtvm_ml_model_io_info_set(model);

	/* Set model info */
	mvtvm_ml_model_info_set(cnxk_mldev, model);

	/* Update model xstats name */
	cnxk_ml_xstats_model_name_update(cnxk_mldev, model->model_id);

	model->mvtvm.burst_xstats =
		RTE_PTR_ADD(model->mvtvm.params.buffer,
			    RTE_ALIGN_CEIL(model->mvtvm.params.size, RTE_CACHE_LINE_MIN_SIZE));

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
		model->op_error_get = cn10k_ml_op_error_get;
	} else {
		model->enqueue_single = mvtvm_ml_enqueue_single;
		model->result_update = mvtvm_ml_result_update;
		model->set_error_code = mvtvm_ml_set_error_code;
		model->set_poll_addr = mvtvm_ml_set_poll_addr;
		model->op_error_get = mvtvm_ml_op_error_get;
	}

	return 0;

error:
	mvtvm_ml_tvm_func_free(&register_cb_fn);
	if (model != NULL) {
		mvtvm_ml_tvm_func_free(&model->mvtvm.load_params);
		mvtvm_ml_tvm_func_free(&model->mvtvm.set_input_zero_copy);
		mvtvm_ml_tvm_func_free(&model->mvtvm.set_output_zero_copy);
		mvtvm_ml_tvm_func_free(&model->mvtvm.run);
		mvtvm_ml_tvm_mod_free(&model->mvtvm.graph_module);
		if (model->mvtvm.fd >= 0)
			close(model->mvtvm.fd);
		memset(&model->mvtvm, 0, sizeof(model->mvtvm));
		model->mvtvm.fd = -1;
	}
	mvtvm_ml_tvm_mod_free(&module_so);
	plt_memzone_free(mz);

	return ret;
}

int
mvtvm_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;

	RTE_SET_USED(cnxk_mldev);

	/* Unload model from TVM runtime */
	if (model->model_id >= cnxk_mldev->mldev->data->nb_models)
		return -EINVAL;

	if (model->mvtvm.graph_module == NULL)
		return -EINVAL;

	mvtvm_ml_tvm_func_free(&model->mvtvm.load_params);
	mvtvm_ml_tvm_func_free(&model->mvtvm.set_input_zero_copy);
	mvtvm_ml_tvm_func_free(&model->mvtvm.set_output_zero_copy);
	mvtvm_ml_tvm_func_free(&model->mvtvm.run);
	mvtvm_ml_tvm_mod_free(&model->mvtvm.graph_module);
	if (model->mvtvm.fd >= 0)
		close(model->mvtvm.fd);
	memset(&model->mvtvm, 0, sizeof(model->mvtvm));
	model->mvtvm.fd = -1;

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", MVTVM_ML_MODEL_MEMZONE_NAME, model->model_id);
	mz = plt_memzone_lookup(str);
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
	for (layer_id = 0; layer_id < model->nb_layers; layer_id++) {
		if (strcmp(model->layer[layer_id].name, layer_name) == 0)
			break;
	}

#ifdef CNXK_ML_DEV_DEBUG
	if (layer_id == model->nb_layers) {
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

	for (layer_id = 0; layer_id < model->nb_layers; layer_id++) {
		if (strcmp(model->layer[layer_id].name, layer_name) == 0)
			break;
	}

#ifdef CNXK_ML_DEV_DEBUG
	if (layer_id == model->nb_layers) {
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
	struct mvtvm_ml_result *run_result;
	TVMValue arg_values[2] = {0};
	int arg_types[2] = {0};
	TVMValue ret_value = {0};
	int ret_type = kTVMNullptr;
	int ret = 0;

	rte_memcpy(req->mvtvm_req.input_tensor, model->mvtvm.input_tensor,
		   model->mvtvm.info.nb_inputs * sizeof(DLTensor));
	for (i = 0; i < model->mvtvm.info.nb_inputs; i++) {
		req->mvtvm_req.input_tensor[i].data = op->input[i]->addr;
		req->mvtvm_req.input_tensor[i].byte_offset = 0;
	}

	rte_memcpy(req->mvtvm_req.output_tensor, model->mvtvm.output_tensor,
		   model->mvtvm.info.nb_outputs * sizeof(DLTensor));
	for (i = 0; i < model->mvtvm.info.nb_outputs; i++) {
		req->mvtvm_req.output_tensor[i].data = op->output[i]->addr;
		req->mvtvm_req.output_tensor[i].byte_offset = 0;
	}

	run_result = &req->mvtvm_req.result;
	run_result->stats.start_ns = rte_get_tsc_cycles();
	run_result->error_code = 0;

	for (i = 0; i < model->mvtvm.info.nb_inputs; i++) {
		arg_values[0].v_int64 = i;
		arg_types[0] = kDLInt;
		arg_values[1].v_handle = &req->mvtvm_req.input_tensor[i];
		arg_types[1] = kTVMDLTensorHandle;
		ret = mvtvm_ml_tvm_func_call(model, model->mvtvm.set_input_zero_copy,
					     "set_input_zero_copy", arg_values, arg_types, 2,
					     &ret_value, &ret_type, kTVMNullptr);
		if (ret != 0)
			goto out;
	}

	for (i = 0; i < model->mvtvm.info.nb_outputs; i++) {
		arg_values[0].v_int64 = i;
		arg_types[0] = kDLInt;
		arg_values[1].v_handle = &req->mvtvm_req.output_tensor[i];
		arg_types[1] = kTVMDLTensorHandle;
		ret = mvtvm_ml_tvm_func_call(model, model->mvtvm.set_output_zero_copy,
					     "set_output_zero_copy", arg_values, arg_types, 2,
					     &ret_value, &ret_type, kTVMNullptr);
		if (ret != 0)
			goto out;
	}

	ret = mvtvm_ml_tvm_func_call(model, model->mvtvm.run, "run", NULL, NULL, 0, &ret_value,
				     &ret_type, kTVMNullptr);
	if (ret != 0)
		goto out;

out:
	run_result->stats.end_ns = rte_get_tsc_cycles();
	req->mvtvm_req.status = 0x1;

	plt_write64(ML_CNXK_POLL_JOB_FINISH, req->status);

	if (ret != 0)
		run_result->error_code = -EIO;

	return 0;
}

__rte_hot void
mvtvm_ml_set_error_code(struct cnxk_ml_req *req, uint64_t etype, uint64_t stype)
{
	RTE_SET_USED(stype);

	req->mvtvm_req.result.error_code = etype;
}

__rte_hot int
mvtvm_ml_op_error_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_op *op,
		      struct rte_ml_op_error *error)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(op);
	RTE_SET_USED(error);

	return 0;
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
