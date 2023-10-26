/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_hash_crc.h>
#include <rte_mldev.h>

#include "cnxk_ml_model.h"
#include "cnxk_ml_utils.h"

enum cnxk_ml_model_type
cnxk_ml_model_get_type(struct rte_ml_model_params *params)
{
	struct cn10k_ml_model_metadata_header *metadata_header;
	enum cnxk_ml_model_type type;
	uint32_t payload_crc32c;
	uint32_t header_crc32c;

	type = mvtvm_ml_model_type_get(params);
	if (type == ML_CNXK_MODEL_TYPE_TVM)
		return ML_CNXK_MODEL_TYPE_TVM;
	else if (type == ML_CNXK_MODEL_TYPE_INVALID)
		return ML_CNXK_MODEL_TYPE_INVALID;

	/* Check model magic string */
	metadata_header = (struct cn10k_ml_model_metadata_header *)params->addr;
	if (strncmp((char *)metadata_header->magic, MRVL_ML_MODEL_MAGIC_STRING, 4) != 0) {
		plt_err("Invalid Glow model, magic = %s", metadata_header->magic);
		return ML_CNXK_MODEL_TYPE_INVALID;
	}

	/* Header CRC check */
	if (metadata_header->header_crc32c != 0) {
		header_crc32c = rte_hash_crc(
			params->addr,
			sizeof(struct cn10k_ml_model_metadata_header) - sizeof(uint32_t), 0);

		if (header_crc32c != metadata_header->header_crc32c) {
			plt_err("Invalid Glow model, Header CRC mismatch");
			return ML_CNXK_MODEL_TYPE_INVALID;
		}
	}

	/* Payload CRC check */
	if (metadata_header->payload_crc32c != 0) {
		payload_crc32c = rte_hash_crc(
			PLT_PTR_ADD(params->addr, sizeof(struct cn10k_ml_model_metadata_header)),
			params->size - sizeof(struct cn10k_ml_model_metadata_header), 0);

		if (payload_crc32c != metadata_header->payload_crc32c) {
			plt_err("Invalid Glow model, Payload CRC mismatch");
			return ML_CNXK_MODEL_TYPE_INVALID;
		}
	}

	return ML_CNXK_MODEL_TYPE_GLOW;
}

void
cnxk_ml_model_dump(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model, FILE *fp)
{
	struct cnxk_ml_layer *layer;
	uint16_t layer_id;

	/* Print debug info */
	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, " Model Information (Model ID: %u, Name: %s)\n", model->model_id, model->name);
	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "model_id", model->model_id);
	fprintf(fp, "%*s : %s\n", FIELD_LEN, "name", model->name);
	fprintf(fp, "%*s : %d\n", FIELD_LEN, "type", model->type);
	fprintf(fp, "%*s : %d\n", FIELD_LEN, "subtype", model->subtype);
	fprintf(fp, "%*s : 0x%016lx\n", FIELD_LEN, "model", PLT_U64_CAST(model));
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "batch_size", model->batch_size);
	fprintf(fp, "%*s : %u\n", FIELD_LEN, "nb_layers", model->nb_layers);

	/* Print model state */
	if (model->state == ML_CNXK_MODEL_STATE_LOADED)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "loaded");
	if (model->state == ML_CNXK_MODEL_STATE_JOB_ACTIVE)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "job_active");
	if (model->state == ML_CNXK_MODEL_STATE_STARTED)
		fprintf(fp, "%*s : %s\n", FIELD_LEN, "state", "started");
	cnxk_ml_print_line(fp, LINE_LEN);
	fprintf(fp, "\n");

	for (layer_id = 0; layer_id < model->nb_layers; layer_id++) {
		layer = &model->layer[layer_id];
		if (layer->type == ML_CNXK_LAYER_TYPE_MRVL)
			cn10k_ml_layer_print(cnxk_mldev, layer, fp);
		else
			mvtvm_ml_layer_print(cnxk_mldev, layer, fp);
	}
}
