/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <archive.h>
#include <archive_entry.h>

#include <rte_mldev.h>

#include <mldev_utils.h>

#include <roc_api.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_utils.h"

/* Objects list */
char mvtvm_object_list[ML_MVTVM_MODEL_OBJECT_MAX][RTE_ML_STR_MAX] = {"mod.so", "mod.json",
								     "mod.params"};

enum cnxk_ml_model_type
mvtvm_ml_model_type_get(struct rte_ml_model_params *params)
{
	bool object_found[ML_MVTVM_MODEL_OBJECT_MAX] = {false, false, false};
	struct archive_entry *entry;
	struct archive *a;
	uint8_t i;
	int ret;

	/* Assume as archive and check for read status */
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	ret = archive_read_open_memory(a, params->addr, params->size);
	if (ret != ARCHIVE_OK)
		return ML_CNXK_MODEL_TYPE_UNKNOWN;

	/* Parse buffer for available objects */
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		for (i = 0; i < ML_MVTVM_MODEL_OBJECT_MAX; i++) {
			if (!object_found[i] &&
			    (strcmp(archive_entry_pathname(entry), mvtvm_object_list[i]) == 0))
				object_found[i] = true;
		}
		archive_read_data_skip(a);
	}

	/* Check if all objects are available */
	for (i = 0; i < ML_MVTVM_MODEL_OBJECT_MAX; i++) {
		if (!object_found[i]) {
			plt_err("Object %s not found in archive!\n", mvtvm_object_list[i]);
			return ML_CNXK_MODEL_TYPE_INVALID;
		}
	}

	return ML_CNXK_MODEL_TYPE_TVM;
}

int
mvtvm_ml_model_blob_parse(struct rte_ml_model_params *params, struct mvtvm_ml_model_object *object)
{
	bool object_found[ML_MVTVM_MODEL_OBJECT_MAX] = {false, false, false};
	struct archive_entry *entry;
	struct archive *a;
	uint8_t i;
	int ret;

	/* Open archive */
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	ret = archive_read_open_memory(a, params->addr, params->size);
	if (ret != ARCHIVE_OK)
		return archive_errno(a);

	/* Read archive */
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		for (i = 0; i < ML_MVTVM_MODEL_OBJECT_MAX; i++) {
			if (!object_found[i] &&
			    (strcmp(archive_entry_pathname(entry), mvtvm_object_list[i]) == 0)) {
				memcpy(object[i].name, mvtvm_object_list[i], RTE_ML_STR_MAX);
				object[i].size = archive_entry_size(entry);
				object[i].buffer = rte_malloc(NULL, object[i].size, 0);

				if (archive_read_data(a, object[i].buffer, object[i].size) !=
				    object[i].size) {
					plt_err("Failed to read object from model archive: %s",
						object[i].name);
					goto error;
				}
				object_found[i] = true;
			}
		}
		archive_read_data_skip(a);
	}

	/* Check if all objects are parsed */
	for (i = 0; i < ML_MVTVM_MODEL_OBJECT_MAX; i++) {
		if (!object_found[i]) {
			plt_err("Object %s not found in archive!\n", mvtvm_object_list[i]);
			goto error;
		}
	}
	return 0;

error:
	for (i = 0; i < ML_MVTVM_MODEL_OBJECT_MAX; i++) {
		rte_free(object[i].buffer);
	}

	return -EINVAL;
}

int
mvtvm_ml_model_get_layer_id(struct cnxk_ml_model *model, const char *layer_name, uint16_t *layer_id)
{
	uint16_t i;

	for (i = 0; i < model->mvtvm.metadata.model.nb_layers; i++) {
		if (strcmp(model->layer[i].name, layer_name) == 0)
			break;
	}

	if (i == model->mvtvm.metadata.model.nb_layers) {
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

static enum rte_ml_io_type
mvtvm_ml_io_type_map(DLDataType dltype)
{
	switch (dltype.code) {
	case kDLInt:
		if (dltype.bits == 8)
			return RTE_ML_IO_TYPE_INT8;
		else if (dltype.bits == 16)
			return RTE_ML_IO_TYPE_INT16;
		else if (dltype.bits == 32)
			return RTE_ML_IO_TYPE_INT32;
		break;
	case kDLUInt:
		if (dltype.bits == 8)
			return RTE_ML_IO_TYPE_UINT8;
		else if (dltype.bits == 16)
			return RTE_ML_IO_TYPE_UINT16;
		else if (dltype.bits == 32)
			return RTE_ML_IO_TYPE_UINT32;
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
mvtvm_ml_model_io_info_set(struct cnxk_ml_model *model)
{
	struct tvmdp_model_metadata *metadata;
	int32_t i;
	int32_t j;

	if (model->subtype == ML_CNXK_MODEL_SUBTYPE_TVM_MRVL)
		goto tvm_mrvl_model;

	metadata = &model->mvtvm.metadata;

	/* Inputs, set for layer_id = 0 */
	model->mvtvm.info.nb_inputs = metadata->model.num_input;
	model->mvtvm.info.total_input_sz_d = 0;
	model->mvtvm.info.total_input_sz_q = 0;
	for (i = 0; i < metadata->model.num_input; i++) {
		rte_strscpy(model->mvtvm.info.input[i].name, metadata->input[i].name,
			    TVMDP_NAME_STRLEN);
		model->mvtvm.info.input[i].dtype =
			mvtvm_ml_io_type_map(metadata->input[i].datatype);
		model->mvtvm.info.input[i].qtype =
			mvtvm_ml_io_type_map(metadata->input[i].model_datatype);
		model->mvtvm.info.input[i].nb_dims = metadata->input[i].ndim;

		model->mvtvm.info.input[i].nb_elements = 1;
		for (j = 0; j < metadata->input[i].ndim; j++) {
			model->mvtvm.info.input[i].shape[j] = metadata->input[i].shape[j];
			model->mvtvm.info.input[i].nb_elements *= metadata->input[i].shape[j];
		}

		model->mvtvm.info.input[i].sz_d =
			model->mvtvm.info.input[i].nb_elements *
			rte_ml_io_type_size_get(model->mvtvm.info.input[i].dtype);
		model->mvtvm.info.input[i].sz_q =
			model->mvtvm.info.input[i].nb_elements *
			rte_ml_io_type_size_get(model->mvtvm.info.input[i].qtype);
		model->mvtvm.info.input[i].scale = metadata->input[i].scale;

		model->mvtvm.info.total_input_sz_d += model->mvtvm.info.input[i].sz_d;
		model->mvtvm.info.total_input_sz_q += model->mvtvm.info.input[i].sz_q;

		model->mvtvm.input_tensor[i].device = metadata->input[i].device;
		model->mvtvm.input_tensor[i].ndim = metadata->input[i].ndim;
		model->mvtvm.input_tensor[i].dtype = metadata->input[i].datatype;
		model->mvtvm.input_tensor[i].shape = metadata->input[i].shape;
		model->mvtvm.input_tensor[i].strides = NULL;
		model->mvtvm.input_tensor[i].byte_offset = 0;

		plt_ml_dbg("model_id = %u, input[%u] - sz_d = %u sz_q = %u", model->model_id, i,
			   model->mvtvm.info.input[i].sz_d, model->mvtvm.info.input[i].sz_q);
	}

	/* Outputs, set for nb_layers - 1 */
	model->mvtvm.info.nb_outputs = metadata->model.num_output;
	model->mvtvm.info.total_output_sz_d = 0;
	model->mvtvm.info.total_output_sz_q = 0;
	for (i = 0; i < metadata->model.num_output; i++) {
		rte_strscpy(model->mvtvm.info.output[i].name, metadata->output[i].name,
			    TVMDP_NAME_STRLEN);
		model->mvtvm.info.output[i].dtype =
			mvtvm_ml_io_type_map(metadata->output[i].datatype);
		model->mvtvm.info.output[i].qtype =
			mvtvm_ml_io_type_map(metadata->output[i].model_datatype);
		model->mvtvm.info.output[i].nb_dims = metadata->output[i].ndim;

		model->mvtvm.info.output[i].nb_elements = 1;
		for (j = 0; j < metadata->output[i].ndim; j++) {
			model->mvtvm.info.output[i].shape[j] = metadata->output[i].shape[j];
			model->mvtvm.info.output[i].nb_elements *= metadata->output[i].shape[j];
		}

		model->mvtvm.info.output[i].sz_d =
			model->mvtvm.info.output[i].nb_elements *
			rte_ml_io_type_size_get(model->mvtvm.info.output[i].dtype);
		model->mvtvm.info.output[i].sz_q =
			model->mvtvm.info.output[i].nb_elements *
			rte_ml_io_type_size_get(model->mvtvm.info.output[i].qtype);
		model->mvtvm.info.output[i].scale = metadata->output[i].scale;

		model->mvtvm.info.total_output_sz_d += model->mvtvm.info.output[i].sz_d;
		model->mvtvm.info.total_output_sz_q += model->mvtvm.info.output[i].sz_q;

		model->mvtvm.output_tensor[i].device = metadata->output[i].device;
		model->mvtvm.output_tensor[i].ndim = metadata->output[i].ndim;
		model->mvtvm.output_tensor[i].dtype = metadata->output[i].datatype;
		model->mvtvm.output_tensor[i].shape = metadata->output[i].shape;
		model->mvtvm.output_tensor[i].strides = NULL;
		model->mvtvm.output_tensor[i].byte_offset = 0;

		plt_ml_dbg("model_id = %u, output[%u] - sz_d = %u sz_q = %u", model->model_id, i,
			   model->mvtvm.info.output[i].sz_d, model->mvtvm.info.output[i].sz_q);
	}

	return;

tvm_mrvl_model:
	cn10k_ml_layer_io_info_set(&model->mvtvm.info, &model->layer[0].glow.metadata);
}

struct cnxk_ml_io_info *
mvtvm_ml_model_io_info_get(struct cnxk_ml_model *model, uint16_t layer_id)
{
	RTE_SET_USED(layer_id);

	return &model->mvtvm.info;
}

void
mvtvm_ml_model_info_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct tvmdp_model_metadata *metadata;
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

	metadata = &model->mvtvm.metadata;
	rte_memcpy(info->name, metadata->model.name, TVMDP_NAME_STRLEN);
	snprintf(info->version, RTE_ML_STR_MAX, "%u.%u.%u.%u", metadata->model.version[0],
		 metadata->model.version[1], metadata->model.version[2],
		 metadata->model.version[3]);
	info->model_id = model->model_id;
	info->device_id = cnxk_mldev->mldev->data->dev_id;
	info->io_layout = RTE_ML_IO_LAYOUT_SPLIT;
	info->min_batches = model->batch_size;
	info->max_batches = model->batch_size;
	info->nb_inputs = metadata->model.num_input;
	info->input_info = input;
	info->nb_outputs = metadata->model.num_output;
	info->output_info = output;
	info->wb_size = 0;

	/* Set input info */
	for (i = 0; i < info->nb_inputs; i++) {
		rte_memcpy(input[i].name, metadata->input[i].name, MRVL_ML_INPUT_NAME_LEN);
		input[i].nb_dims = metadata->input[i].ndim;
		input[i].shape = &model->mvtvm.info.input[i].shape[0];
		input[i].type = model->mvtvm.info.input[i].qtype;
		input[i].nb_elements = model->mvtvm.info.input[i].nb_elements;
		input[i].size = model->mvtvm.info.input[i].nb_elements *
				rte_ml_io_type_size_get(model->mvtvm.info.input[i].qtype);
	}

	/* Set output info */
	for (i = 0; i < info->nb_outputs; i++) {
		rte_memcpy(output[i].name, metadata->output[i].name, MRVL_ML_OUTPUT_NAME_LEN);
		output[i].nb_dims = metadata->output[i].ndim;
		output[i].shape = &model->mvtvm.info.output[i].shape[0];
		output[i].type = model->mvtvm.info.output[i].qtype;
		output[i].nb_elements = model->mvtvm.info.output[i].nb_elements;
		output[i].size = model->mvtvm.info.output[i].nb_elements *
				 rte_ml_io_type_size_get(model->mvtvm.info.output[i].qtype);
	}

	return;

tvm_mrvl_model:
	cn10k_ml_model_info_set(cnxk_mldev, model, &model->mvtvm.info,
				&model->layer[0].glow.metadata);
	info->io_layout = RTE_ML_IO_LAYOUT_SPLIT;
}

void
mvtvm_ml_layer_print(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer, FILE *fp)
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
