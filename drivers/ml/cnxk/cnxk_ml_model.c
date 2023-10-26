/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>

#include "cnxk_ml_model.h"
#include "cnxk_ml_utils.h"

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
		cn10k_ml_layer_print(cnxk_mldev, layer, fp);
	}
}
