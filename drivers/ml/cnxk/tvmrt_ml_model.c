/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Marvell.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include <rte_mldev.h>

#include <mldev_utils.h>

#include <roc_api.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_utils.h"

/* Objects list */
static char tvmrt_object_list[ML_TVMRT_MODEL_OBJECT_MAX][RTE_ML_STR_MAX] = {"mod.so", "mod.json",
									    "mod.params"};

enum cnxk_ml_model_type
tvmrt_ml_model_type_get(struct rte_ml_model_params *params)
{
	bool object_found[ML_TVMRT_MODEL_OBJECT_MAX] = {false, false, false};
	enum cnxk_ml_model_type model_type;
	struct archive_entry *entry;
	struct archive *a;
	uint8_t i;
	int ret;

	/* Assume as archive and check for read status */
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	model_type = ML_CNXK_MODEL_TYPE_UNKNOWN;
	ret = archive_read_open_memory(a, params->addr, params->size);
	if (ret != ARCHIVE_OK)
		goto cleanup;

	/* Parse buffer for available objects */
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		for (i = 0; i < ML_TVMRT_MODEL_OBJECT_MAX; i++) {
			if (!object_found[i] &&
			    (strcmp(archive_entry_pathname(entry), tvmrt_object_list[i]) == 0))
				object_found[i] = true;
		}
		archive_read_data_skip(a);
	}

	/* Check if all objects are available */
	for (i = 0; i < ML_TVMRT_MODEL_OBJECT_MAX; i++) {
		if (!object_found[i]) {
			plt_err("Object %s not found in archive!", tvmrt_object_list[i]);
			model_type = ML_CNXK_MODEL_TYPE_INVALID;
			goto cleanup;
		}
	}

	model_type = ML_CNXK_MODEL_TYPE_TVM;

cleanup:
	archive_read_close(a);
	archive_read_free(a);

	return model_type;
}

int
tvmrt_ml_model_blob_parse(struct rte_ml_model_params *params, struct tvmrt_ml_model_object *object)
{
	bool object_found[ML_TVMRT_MODEL_OBJECT_MAX] = {false, false, false};
	struct archive_entry *entry;
	struct archive *a;
	uint8_t i;
	int ret;

	memset(object, 0, ML_TVMRT_MODEL_OBJECT_MAX * sizeof(*object));

	/* Open archive */
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	ret = archive_read_open_memory(a, params->addr, params->size);
	if (ret != ARCHIVE_OK) {
		ret = archive_errno(a);
		goto cleanup;
	}

	/* Read archive */
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		for (i = 0; i < ML_TVMRT_MODEL_OBJECT_MAX; i++) {
			if (!object_found[i] &&
			    (strcmp(archive_entry_pathname(entry), tvmrt_object_list[i]) == 0)) {
				memcpy(object[i].name, tvmrt_object_list[i], RTE_ML_STR_MAX);
				object[i].size = archive_entry_size(entry);
				object[i].buffer = rte_malloc(NULL, object[i].size, 0);
				if (object[i].buffer == NULL) {
					ret = -ENOMEM;
					goto error;
				}

				if (archive_read_data(a, object[i].buffer, object[i].size) !=
				    object[i].size) {
					plt_err("Failed to read object from model archive: %s",
						object[i].name);
					ret = -EINVAL;
					goto error;
				}
				object_found[i] = true;
			}
		}
		archive_read_data_skip(a);
	}

	/* Check if all objects are parsed */
	for (i = 0; i < ML_TVMRT_MODEL_OBJECT_MAX; i++) {
		if (!object_found[i]) {
			plt_err("Object %s not found in archive!", tvmrt_object_list[i]);
			ret = -EINVAL;
			goto error;
		}
	}
	ret = 0;
	goto cleanup;

error:
	for (i = 0; i < ML_TVMRT_MODEL_OBJECT_MAX; i++) {
		if (object[i].buffer != NULL)
			rte_free(object[i].buffer);
	}

cleanup:
	archive_read_close(a);
	archive_read_free(a);

	return ret;
}

int
tvmrt_ml_model_get_layer_id(struct cnxk_ml_model *model, const char *layer_name, uint16_t *layer_id)
{
	uint16_t i;

	for (i = 0; i < model->nb_layers; i++) {
		if (strcmp(model->layer[i].name, layer_name) == 0)
			break;
	}

	if (i == model->nb_layers) {
		plt_err("Invalid layer name: %s", layer_name);
		return -EINVAL;
	}

	if (model->layer[i].type != ML_CNXK_LAYER_TYPE_MRVL) {
		plt_err("Invalid layer type, name: %s type: %d", layer_name, model->layer[i].type);
		return -EINVAL;
	}

	*layer_id = i;

	return 0;
}

static void
tvmrt_ml_param_names_free(struct tvmrt_ml_param_names *param_names)
{
	size_t i;

	for (i = 0; i < param_names->count; i++)
		free(param_names->name[i]);

	free(param_names->name);
	param_names->name = NULL;
	param_names->count = 0;
}

static int
tvmrt_ml_blob_read_u64(const uint8_t *blob, size_t blob_size, size_t *offset, uint64_t *value)
{
	if (*offset + sizeof(*value) > blob_size)
		return -EINVAL;

	memcpy(value, blob + *offset, sizeof(*value));
	*offset += sizeof(*value);

	return 0;
}

static int
tvmrt_ml_param_names_parse(const uint8_t *blob, size_t blob_size,
			   struct tvmrt_ml_param_names *param_names)
{
	uint64_t magic = 0;
	uint64_t reserved = 0;
	uint64_t count = 0;
	size_t offset = 0;
	int ret;
	size_t i;

	memset(param_names, 0, sizeof(*param_names));

	ret = tvmrt_ml_blob_read_u64(blob, blob_size, &offset, &magic);
	if (ret != 0)
		return ret;

	if (magic != TVM_NDARRAY_LIST_MAGIC)
		return -EINVAL;

	ret = tvmrt_ml_blob_read_u64(blob, blob_size, &offset, &reserved);
	if (ret != 0)
		return ret;

	ret = tvmrt_ml_blob_read_u64(blob, blob_size, &offset, &count);
	if (ret != 0)
		return ret;

	if (count > SIZE_MAX / sizeof(*param_names->name))
		return -EINVAL;

	param_names->name = calloc(count, sizeof(*param_names->name));
	if (count != 0 && param_names->name == NULL)
		return -ENOMEM;

	param_names->count = count;
	for (i = 0; i < count; i++) {
		uint64_t name_len = 0;

		ret = tvmrt_ml_blob_read_u64(blob, blob_size, &offset, &name_len);
		if (ret != 0)
			goto error;

		if (name_len == SIZE_MAX) {
			ret = -EINVAL;
			goto error;
		}

		if (offset + name_len > blob_size) {
			ret = -EINVAL;
			goto error;
		}

		param_names->name[i] = calloc(name_len + 1, 1);
		if (param_names->name[i] == NULL) {
			ret = -ENOMEM;
			goto error;
		}

		memcpy(param_names->name[i], blob + offset, name_len);
		offset += name_len;
	}

	return 0;

error:
	tvmrt_ml_param_names_free(param_names);
	return ret;
}

static int
tvmrt_ml_dtype_parse(const char *dtype_str, DLDataType *dtype)
{
	const char *lanes_str;
	char base[32] = {0};
	long bits;
	long lanes = 1;

	if (dtype_str == NULL || dtype == NULL)
		return -EINVAL;

	lanes_str = strchr(dtype_str, 'x');
	if (lanes_str != NULL) {
		size_t base_len = lanes_str - dtype_str;

		if (base_len >= sizeof(base))
			return -EINVAL;
		memcpy(base, dtype_str, base_len);
		lanes = strtol(lanes_str + 1, NULL, 10);
		if (lanes <= 0 || lanes > UINT16_MAX)
			return -EINVAL;
	} else {
		snprintf(base, sizeof(base), "%s", dtype_str);
	}

	memset(dtype, 0, sizeof(*dtype));
	dtype->lanes = (uint16_t)lanes;

	if (strncmp(base, "int", 3) == 0) {
		bits = strtol(base + 3, NULL, 10);
		dtype->code = kDLInt;
	} else if (strncmp(base, "uint", 4) == 0) {
		bits = strtol(base + 4, NULL, 10);
		dtype->code = kDLUInt;
	} else if (strncmp(base, "float", 5) == 0) {
		bits = strtol(base + 5, NULL, 10);
		dtype->code = kDLFloat;
	} else if (strncmp(base, "bfloat", 6) == 0) {
		bits = strtol(base + 6, NULL, 10);
		dtype->code = kDLBfloat;
	} else if (strcmp(base, "bool") == 0) {
		bits = 1;
		dtype->code = kDLUInt;
	} else {
		return -EINVAL;
	}

	if (bits <= 0 || bits > UINT8_MAX)
		return -EINVAL;

	dtype->bits = (uint8_t)bits;

	return 0;
}

static int
tvmrt_ml_model_io_set(struct cnxk_ml_io *io, const char *name, json_t *shape, const char *dtype_str,
		      DLDevice device)
{
	size_t i;
	int ret;

	memset(io, 0, sizeof(*io));
	if (name != NULL)
		snprintf(io->name, sizeof(io->name), "%s", name);

	if (!json_is_array(shape))
		return -EINVAL;

	io->nb_dims = json_array_size(shape);
	if (io->nb_dims > ML_CNXK_MODEL_MAX_DIMS)
		return -ENOTSUP;

	for (i = 0; i < (size_t)io->nb_dims; i++) {
		json_t *dim = json_array_get(shape, i);

		if (!json_is_integer(dim))
			return -EINVAL;
		io->shape_i64[i] = json_integer_value(dim);
	}

	ret = tvmrt_ml_dtype_parse(dtype_str, &io->datatype);
	if (ret != 0)
		return ret;

	io->model_datatype = io->datatype;
	io->scale = 1.0f;
	io->device = device;

	return 0;
}


static int
tvmrt_ml_json_graph_get_arrays(json_t *json_parsed, json_t **nodes, json_t **arg_nodes,
			       json_t **heads, json_t **node_row_ptr, json_t **shape_values,
			       json_t **dtype_values)
{
	json_t *attrs;
	json_t *shape_attr;
	json_t *dtype_attr;

	*nodes = json_object_get(json_parsed, "nodes");
	*arg_nodes = json_object_get(json_parsed, "arg_nodes");
	*heads = json_object_get(json_parsed, "heads");
	*node_row_ptr = json_object_get(json_parsed, "node_row_ptr");
	attrs = json_object_get(json_parsed, "attrs");

	if (!json_is_array(*nodes) || !json_is_array(*arg_nodes) || !json_is_array(*heads) ||
	    !json_is_array(*node_row_ptr) || !json_is_object(attrs))
		return -EINVAL;

	if (json_array_size(*node_row_ptr) != json_array_size(*nodes) + 1)
		return -EINVAL;

	shape_attr = json_object_get(attrs, "shape");
	dtype_attr = json_object_get(attrs, "dltype");
	if (!json_is_array(shape_attr) || json_array_size(shape_attr) < 2 ||
	    !json_is_array(dtype_attr) || json_array_size(dtype_attr) < 2)
		return -EINVAL;

	*shape_values = json_array_get(shape_attr, 1);
	*dtype_values = json_array_get(dtype_attr, 1);

	if (!json_is_array(*shape_values) || !json_is_array(*dtype_values))
		return -EINVAL;

	return 0;
}

int
tvmrt_ml_model_json_parse(struct cnxk_ml_model *model)
{
	struct tvmrt_ml_param_names param_names;
	json_error_t json_error;
	json_t *json_parsed;
	json_t *json_nodes;
	json_t *json_arg_nodes;
	json_t *json_heads;
	json_t *json_node_row_ptr;
	json_t *json_shape_values;
	json_t *json_dtype_values;
	uint16_t nb_mrvl_layers;
	uint16_t nb_llvm_layers;
	DLDevice device;
	int ret;
	size_t i;
	size_t j;

	/* Parse param names from params buffer */
	ret = tvmrt_ml_param_names_parse(model->tvmrt.params.buffer, model->tvmrt.params.size,
					 &param_names);
	if (ret != 0)
		return ret;

	/* Load JSON graph (single load for both stages) */
	json_parsed = json_loadb((const char *)model->tvmrt.json.buffer, model->tvmrt.json.size, 0,
				 &json_error);
	if (json_parsed == NULL) {
		tvmrt_ml_param_names_free(&param_names);
		plt_err("TVM runtime: Failed to parse JSON graph, model_id = %u", model->model_id);
		return -EINVAL;
	}

	/* Parse nodes to extract layer info */
	json_nodes = json_object_get(json_parsed, "nodes");
	if (!json_is_array(json_nodes)) {
		ret = -EINVAL;
		plt_err("TVM runtime: Failed to parse JSON nodes, model_id = %u, error = %d",
			model->model_id, ret);
		goto error;
	}

	model->nb_layers = 0;
	nb_mrvl_layers = 0;
	nb_llvm_layers = 0;
	for (i = 0; i < json_array_size(json_nodes); i++) {
		json_t *node = json_array_get(json_nodes, i);
		json_t *op = json_object_get(node, "op");
		json_t *name;
		json_t *attrs;
		json_t *compiler;

		if (!json_is_string(op) || strcmp(json_string_value(op), "tvm_op") != 0)
			continue;

		if (model->nb_layers >= ML_CNXK_MODEL_MAX_LAYERS) {
			ret = -ENOTSUP;
			plt_err("TVM runtime: Number of layers exceeds maximum (%u), model_id = %u, error = %d",
				ML_CNXK_MODEL_MAX_LAYERS, model->model_id, ret);
			goto error;
		}

		name = json_object_get(node, "name");
		if (!json_is_string(name)) {
			ret = -EINVAL;
			plt_err("TVM runtime: Failed to parse JSON node name, model_id = %u, error = %d",
				model->model_id, ret);
			goto error;
		}

		if (json_string_value(name) != NULL)
			snprintf(model->layer[model->nb_layers].name,
				 sizeof(model->layer[model->nb_layers].name), "%s",
				 json_string_value(name));
		else
			snprintf(model->layer[model->nb_layers].name,
				 sizeof(model->layer[model->nb_layers].name), "tvm_layer_%u",
				 model->nb_layers);

		attrs = json_object_get(node, "attrs");
		compiler = json_is_object(attrs) ? json_object_get(attrs, "Compiler") : NULL;
		if (json_is_string(compiler)) {
			if (strcmp(json_string_value(compiler), "mrvl") == 0 ||
			    strcmp(json_string_value(compiler), "MRVL") == 0) {
				model->layer[model->nb_layers].type = ML_CNXK_LAYER_TYPE_MRVL;
				nb_mrvl_layers++;
			} else {
				model->layer[model->nb_layers].type = ML_CNXK_LAYER_TYPE_UNKNOWN;
			}
		} else {
			model->layer[model->nb_layers].type = ML_CNXK_LAYER_TYPE_LLVM;
			nb_llvm_layers++;
		}
		model->nb_layers++;
	}

	/* Set model fields */
	snprintf(model->name, sizeof(model->name), "tvm_model_%u", model->model_id);
	model->batch_size = 1;

	/* Validate layer counts */
	if ((nb_llvm_layers == 0) && (nb_mrvl_layers == 0)) {
		plt_err("Invalid model, nb_llvm_layers = %u, nb_mrvl_layers = %u", nb_llvm_layers,
			nb_mrvl_layers);
		ret = -EINVAL;
		goto error;
	}

	if (nb_llvm_layers + nb_mrvl_layers != model->nb_layers) {
		plt_err("Invalid model, nb_llvm_layers = %u, nb_mrvl_layers = %u, nb_layers = %u",
			nb_llvm_layers, nb_mrvl_layers, model->nb_layers);
		ret = -EINVAL;
		goto error;
	}

	/* Set model subtype */
	if ((nb_llvm_layers == 0) && (nb_mrvl_layers == 1))
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_MRVL;
	else if ((nb_llvm_layers > 0) && (nb_mrvl_layers == 0))
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_LLVM;
	else
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_HYBRID;

	/* Parse I/O info from the same JSON graph */
	device.device_type = kDLCPU;
	device.device_id = 0;

	ret = tvmrt_ml_json_graph_get_arrays(json_parsed, &json_nodes, &json_arg_nodes, &json_heads,
					     &json_node_row_ptr, &json_shape_values,
					     &json_dtype_values);
	if (ret == 0) {
		model->tvmrt.info.nb_inputs = 0;
		model->tvmrt.info.nb_outputs = 0;

		for (i = 0; i < json_array_size(json_arg_nodes); i++) {
			json_t *arg_node_idx = json_array_get(json_arg_nodes, i);
			json_t *node;
			json_t *shape;
			json_t *dtype;
			json_t *name;
			json_int_t node_id;

			if (!json_is_integer(arg_node_idx)) {
				ret = -EINVAL;
				break;
			}

			node_id = json_integer_value(arg_node_idx);
			node = json_array_get(json_nodes, node_id);
			shape = json_array_get(json_shape_values, node_id);
			dtype = json_array_get(json_dtype_values, node_id);
			name = json_object_get(node, "name");
			if (!json_is_object(node) || !json_is_array(shape) ||
			    !json_is_string(dtype) || !json_is_string(name)) {
				ret = -EINVAL;
				break;
			}

			for (j = 0; j < param_names.count; j++) {
				if (strcmp(param_names.name[j], json_string_value(name)) == 0)
					break;
			}

			if (j < param_names.count)
				continue;

			if (model->tvmrt.info.nb_inputs >= ML_CNXK_MODEL_MAX_INPUT_OUTPUT) {
				ret = -ENOTSUP;
				break;
			}

			ret = tvmrt_ml_model_io_set(
				&model->tvmrt.info.input[model->tvmrt.info.nb_inputs],
				json_string_value(name), shape, json_string_value(dtype), device);
			if (ret != 0)
				break;

			model->tvmrt.info.nb_inputs++;
		}

		for (i = 0; ret == 0 && i < json_array_size(json_heads); i++) {
			json_t *head = json_array_get(json_heads, i);
			json_t *node;
			json_t *shape;
			json_t *dtype;
			json_t *name;
			json_t *node_id_json;
			json_t *output_idx_json;
			json_t *entry_base_json;
			json_t *entry_limit_json;
			json_int_t node_id;
			json_int_t output_idx;
			json_int_t entry_base;
			json_int_t entry_limit;
			size_t entry_id;

			if (!json_is_array(head) || json_array_size(head) < 2) {
				ret = -EINVAL;
				break;
			}

			node_id_json = json_array_get(head, 0);
			output_idx_json = json_array_get(head, 1);
			if (!json_is_integer(node_id_json) || !json_is_integer(output_idx_json)) {
				ret = -EINVAL;
				break;
			}

			node_id = json_integer_value(node_id_json);
			output_idx = json_integer_value(output_idx_json);
			if (node_id < 0 || output_idx < 0) {
				ret = -EINVAL;
				break;
			}

			node = json_array_get(json_nodes, (size_t)node_id);
			entry_base_json = json_array_get(json_node_row_ptr, (size_t)node_id);
			entry_limit_json = json_array_get(json_node_row_ptr, (size_t)node_id + 1);
			if (!json_is_object(node) || !json_is_integer(entry_base_json) ||
			    !json_is_integer(entry_limit_json)) {
				ret = -EINVAL;
				break;
			}

			entry_base = json_integer_value(entry_base_json);
			entry_limit = json_integer_value(entry_limit_json);
			if (entry_base < 0 || entry_limit < entry_base) {
				ret = -EINVAL;
				break;
			}

			if (output_idx >= (entry_limit - entry_base)) {
				ret = -EINVAL;
				break;
			}

			entry_id = (size_t)entry_base + (size_t)output_idx;
			shape = json_array_get(json_shape_values, entry_id);
			dtype = json_array_get(json_dtype_values, entry_id);
			name = json_object_get(node, "name");
			if (!json_is_array(shape) || !json_is_string(dtype) ||
			    !json_is_string(name)) {
				ret = -EINVAL;
				break;
			}

			if (model->tvmrt.info.nb_outputs >= ML_CNXK_MODEL_MAX_INPUT_OUTPUT) {
				ret = -ENOTSUP;
				break;
			}

			ret = tvmrt_ml_model_io_set(
				&model->tvmrt.info.output[model->tvmrt.info.nb_outputs],
				json_string_value(name), shape, json_string_value(dtype), device);
			if (ret != 0)
				break;

			model->tvmrt.info.nb_outputs++;
		}
	}

	if (ret != 0)
		plt_err("TVM runtime: Failed to get metadata, model_id = %u, error = %d",
			model->model_id, ret);

	json_decref(json_parsed);
	tvmrt_ml_param_names_free(&param_names);
	return ret;

error:
	json_decref(json_parsed);
	tvmrt_ml_param_names_free(&param_names);
	return ret;
}

static enum rte_ml_io_type
tvmrt_ml_io_type_map(DLDataType dltype)
{
	switch (dltype.code) {
	case kDLInt:
		if (dltype.bits == 8)
			return RTE_ML_IO_TYPE_INT8;
		else if (dltype.bits == 16)
			return RTE_ML_IO_TYPE_INT16;
		else if (dltype.bits == 32)
			return RTE_ML_IO_TYPE_INT32;
		else if (dltype.bits == 64)
			return RTE_ML_IO_TYPE_INT64;
		break;
	case kDLUInt:
		if (dltype.bits == 8)
			return RTE_ML_IO_TYPE_UINT8;
		else if (dltype.bits == 16)
			return RTE_ML_IO_TYPE_UINT16;
		else if (dltype.bits == 32)
			return RTE_ML_IO_TYPE_UINT32;
		else if (dltype.bits == 64)
			return RTE_ML_IO_TYPE_UINT64;
		break;
	case kDLFloat:
		if (dltype.bits == 8)
			return RTE_ML_IO_TYPE_FP8;
		else if (dltype.bits == 16)
			return RTE_ML_IO_TYPE_FP16;
		else if (dltype.bits == 32)
			return RTE_ML_IO_TYPE_FP32;
		break;
	case kDLBfloat:
		if (dltype.bits == 16)
			return RTE_ML_IO_TYPE_BFLOAT16;
		break;
	default:
		return RTE_ML_IO_TYPE_UNKNOWN;
	}

	return RTE_ML_IO_TYPE_UNKNOWN;
}

void
tvmrt_ml_model_io_info_set(struct cnxk_ml_model *model)
{
	int32_t i;
	uint32_t j;

	if (model->subtype == ML_CNXK_MODEL_SUBTYPE_TVM_MRVL)
		goto tvm_mrvl_model;

	/* Inputs, set for layer_id = 0 */
	model->tvmrt.info.total_input_sz_d = 0;
	model->tvmrt.info.total_input_sz_q = 0;
	for (i = 0; i < model->tvmrt.info.nb_inputs; i++) {
		model->tvmrt.info.input[i].dtype =
			tvmrt_ml_io_type_map(model->tvmrt.info.input[i].datatype);
		model->tvmrt.info.input[i].qtype =
			tvmrt_ml_io_type_map(model->tvmrt.info.input[i].model_datatype);

		model->tvmrt.info.input[i].nb_elements = 1;
		for (j = 0; j < model->tvmrt.info.input[i].nb_dims; j++) {
			model->tvmrt.info.input[i].shape[j] =
				PLT_U32_CAST(model->tvmrt.info.input[i].shape_i64[j]);
			model->tvmrt.info.input[i].nb_elements *=
				model->tvmrt.info.input[i].shape[j];
		}

		model->tvmrt.info.input[i].sz_d =
			model->tvmrt.info.input[i].nb_elements *
			rte_ml_io_type_size_get(model->tvmrt.info.input[i].dtype);
		model->tvmrt.info.input[i].sz_q =
			model->tvmrt.info.input[i].nb_elements *
			rte_ml_io_type_size_get(model->tvmrt.info.input[i].qtype);

		model->tvmrt.info.total_input_sz_d += model->tvmrt.info.input[i].sz_d;
		model->tvmrt.info.total_input_sz_q += model->tvmrt.info.input[i].sz_q;

		model->tvmrt.input_tensor[i].device = model->tvmrt.info.input[i].device;
		model->tvmrt.input_tensor[i].ndim = model->tvmrt.info.input[i].nb_dims;
		model->tvmrt.input_tensor[i].dtype = model->tvmrt.info.input[i].datatype;
		model->tvmrt.input_tensor[i].shape = model->tvmrt.info.input[i].shape_i64;
		model->tvmrt.input_tensor[i].strides = NULL;
		model->tvmrt.input_tensor[i].byte_offset = 0;

		plt_ml_dbg("model_id = %u, input[%u] - sz_d = %u sz_q = %u", model->model_id, i,
			   model->tvmrt.info.input[i].sz_d, model->tvmrt.info.input[i].sz_q);
	}

	/* Outputs, set for nb_layers - 1 */
	model->tvmrt.info.total_output_sz_d = 0;
	model->tvmrt.info.total_output_sz_q = 0;
	for (i = 0; i < model->tvmrt.info.nb_outputs; i++) {
		model->tvmrt.info.output[i].dtype =
			tvmrt_ml_io_type_map(model->tvmrt.info.output[i].datatype);
		model->tvmrt.info.output[i].qtype =
			tvmrt_ml_io_type_map(model->tvmrt.info.output[i].model_datatype);

		model->tvmrt.info.output[i].nb_elements = 1;
		for (j = 0; j < model->tvmrt.info.output[i].nb_dims; j++) {
			model->tvmrt.info.output[i].shape[j] =
				PLT_U32_CAST(model->tvmrt.info.output[i].shape_i64[j]);
			model->tvmrt.info.output[i].nb_elements *=
				model->tvmrt.info.output[i].shape[j];
		}

		model->tvmrt.info.output[i].sz_d =
			model->tvmrt.info.output[i].nb_elements *
			rte_ml_io_type_size_get(model->tvmrt.info.output[i].dtype);
		model->tvmrt.info.output[i].sz_q =
			model->tvmrt.info.output[i].nb_elements *
			rte_ml_io_type_size_get(model->tvmrt.info.output[i].qtype);

		model->tvmrt.info.total_output_sz_d += model->tvmrt.info.output[i].sz_d;
		model->tvmrt.info.total_output_sz_q += model->tvmrt.info.output[i].sz_q;

		model->tvmrt.output_tensor[i].device = model->tvmrt.info.output[i].device;
		model->tvmrt.output_tensor[i].ndim = model->tvmrt.info.output[i].nb_dims;
		model->tvmrt.output_tensor[i].dtype = model->tvmrt.info.output[i].datatype;
		model->tvmrt.output_tensor[i].shape = model->tvmrt.info.output[i].shape_i64;
		model->tvmrt.output_tensor[i].strides = NULL;
		model->tvmrt.output_tensor[i].byte_offset = 0;

		plt_ml_dbg("model_id = %u, output[%u] - sz_d = %u sz_q = %u", model->model_id, i,
			   model->tvmrt.info.output[i].sz_d, model->tvmrt.info.output[i].sz_q);
	}

	return;

tvm_mrvl_model:
	cn10k_ml_layer_io_info_set(&model->tvmrt.info, &model->layer[0].glow.metadata);
}

struct cnxk_ml_io_info *
tvmrt_ml_model_io_info_get(struct cnxk_ml_model *model, uint16_t layer_id)
{
	RTE_SET_USED(layer_id);

	return &model->tvmrt.info;
}

void
tvmrt_ml_model_info_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct rte_ml_model_info *info;
	struct rte_ml_io_info *output;
	struct rte_ml_io_info *input;
	uint8_t i;

	info = PLT_PTR_CAST(model->info);
	input = PLT_PTR_ADD(info, sizeof(struct rte_ml_model_info));
	output = PLT_PTR_ADD(input, ML_CNXK_MODEL_MAX_INPUT_OUTPUT * sizeof(struct rte_ml_io_info));

	/* Reset model info */
	memset(info, 0, sizeof(struct rte_ml_model_info));

	if (model->subtype == ML_CNXK_MODEL_SUBTYPE_TVM_MRVL)
		goto tvm_mrvl_model;

	rte_memcpy(info->name, model->name, RTE_ML_STR_MAX);
	snprintf(info->version, RTE_ML_STR_MAX, "%u.%u.%u.%u", 0, 0, 0, 0);
	info->model_id = model->model_id;
	info->device_id = cnxk_mldev->mldev->data->dev_id;
	info->io_layout = RTE_ML_IO_LAYOUT_SPLIT;
	info->min_batches = model->batch_size;
	info->max_batches = model->batch_size;
	info->nb_inputs = model->tvmrt.info.nb_inputs;
	info->input_info = input;
	info->nb_outputs = model->tvmrt.info.nb_outputs;
	info->output_info = output;
	info->wb_size = 0;

	/* Set input info */
	for (i = 0; i < info->nb_inputs; i++) {
		rte_memcpy(input[i].name, model->tvmrt.info.input[i].name, MRVL_ML_INPUT_NAME_LEN);
		input[i].nb_dims = model->tvmrt.info.input[i].nb_dims;
		input[i].shape = &model->tvmrt.info.input[i].shape[0];
		input[i].type = model->tvmrt.info.input[i].qtype;
		input[i].nb_elements = model->tvmrt.info.input[i].nb_elements;
		input[i].size = model->tvmrt.info.input[i].nb_elements *
				rte_ml_io_type_size_get(model->tvmrt.info.input[i].qtype);
		input[i].scale = model->tvmrt.info.input[i].scale;
		input[i].zero_point = 0;
	}

	/* Set output info */
	for (i = 0; i < info->nb_outputs; i++) {
		rte_memcpy(output[i].name, model->tvmrt.info.output[i].name,
			   MRVL_ML_OUTPUT_NAME_LEN);
		output[i].nb_dims = model->tvmrt.info.output[i].nb_dims;
		output[i].shape = &model->tvmrt.info.output[i].shape[0];
		output[i].type = model->tvmrt.info.output[i].qtype;
		output[i].nb_elements = model->tvmrt.info.output[i].nb_elements;
		output[i].size = model->tvmrt.info.output[i].nb_elements *
				 rte_ml_io_type_size_get(model->tvmrt.info.output[i].qtype);
		output[i].scale = model->tvmrt.info.output[i].scale;
		output[i].zero_point = 0;
	}

	return;

tvm_mrvl_model:
	cn10k_ml_model_info_set(cnxk_mldev, model, &model->tvmrt.info,
				&model->layer[0].glow.metadata);

	strlcpy(info->name, model->name, RTE_ML_STR_MAX);

	info->io_layout = RTE_ML_IO_LAYOUT_PACKED;
}

void
tvmrt_ml_layer_print(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer, FILE *fp)
{
	char str[STR_LEN];
	uint8_t i;

	/* Print debug info */
	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, " Layer Information (Layer ID: %u, Name: %s)\n",
		cnxk_mldev->index_map[layer->index].layer_id, layer->name);
	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "layer_id",
		cnxk_mldev->index_map[layer->index].layer_id);
	fprintf(fp, "%*s : %s\n", FIELD_LEN, "name", layer->name);
	fprintf(fp, "%*s : %d\n", FIELD_LEN, "type", layer->type);
	fprintf(fp, "%*s : 0x%016lx\n", FIELD_LEN, "layer", PLT_U64_CAST(layer));
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "batch_size", layer->batch_size);

	/* Print model state */
	if (layer->state == ML_CNXK_LAYER_STATE_LOADED)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "loaded");
	if (layer->state == ML_CNXK_LAYER_STATE_JOB_ACTIVE)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "job_active");
	if (layer->state == ML_CNXK_LAYER_STATE_STARTED)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "started");

	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_inputs", layer->info.nb_inputs);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "num_outputs", layer->info.nb_outputs);
	fprintf(fp, "\n");

	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, "%8s  %16s  %12s\n", "input", "input_name", "input_type");
	cnxk_ml_print_line(fp, LINE_LEN);
	for (i = 0; i < layer->info.nb_inputs; i++) {
		fprintf(fp, "%8u  ", i);
		fprintf(fp, "%*s  ", 16, layer->info.input[i].name);
		rte_ml_io_type_to_str(layer->info.input[i].qtype, str, STR_LEN);
		fprintf(fp, "%*s  ", 12, str);
	}
	fprintf(fp, "\n");
	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, "\n");

	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, "%8s  %16s  %12s\n", "output", "output_name", "output_type");
	cnxk_ml_print_line(fp, LINE_LEN);
	for (i = 0; i < layer->info.nb_outputs; i++) {
		fprintf(fp, "%8u  ", i);
		fprintf(fp, "%*s  ", 16, layer->info.output[i].name);
		rte_ml_io_type_to_str(layer->info.output[i].qtype, str, STR_LEN);
		fprintf(fp, "%*s  ", 12, str);
		fprintf(fp, "\n");
	}
	fprintf(fp, "\n");
	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, "\n");
}
