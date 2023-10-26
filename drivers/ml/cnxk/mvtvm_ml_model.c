/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <archive.h>
#include <archive_entry.h>

#include <rte_mldev.h>

#include <roc_api.h>

#include "cnxk_ml_model.h"

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
		if (object[i].buffer != NULL)
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
