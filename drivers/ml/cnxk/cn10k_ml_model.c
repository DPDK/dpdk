/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_hash_crc.h>

#include <mldev_utils.h>

#include "cn10k_ml_dev.h"
#include "cn10k_ml_model.h"
#include "cn10k_ml_ocm.h"

static enum rte_ml_io_type
cn10k_ml_io_type_map(uint8_t type)
{
	switch (type) {
	case 1:
		return RTE_ML_IO_TYPE_INT8;
	case 2:
		return RTE_ML_IO_TYPE_UINT8;
	case 3:
		return RTE_ML_IO_TYPE_INT16;
	case 4:
		return RTE_ML_IO_TYPE_UINT16;
	case 5:
		return RTE_ML_IO_TYPE_INT32;
	case 6:
		return RTE_ML_IO_TYPE_UINT32;
	case 7:
		return RTE_ML_IO_TYPE_FP16;
	case 8:
		return RTE_ML_IO_TYPE_FP32;
	}

	return RTE_ML_IO_TYPE_UNKNOWN;
}

int
cn10k_ml_model_metadata_check(uint8_t *buffer, uint64_t size)
{
	struct cn10k_ml_model_metadata *metadata;
	uint32_t payload_crc32c;
	uint32_t header_crc32c;
	uint32_t version;
	uint8_t i;
	uint8_t j;

	metadata = (struct cn10k_ml_model_metadata *)buffer;

	/* Header CRC check */
	if (metadata->header.header_crc32c != 0) {
		header_crc32c =
			rte_hash_crc(buffer, sizeof(metadata->header) - sizeof(uint32_t), 0);

		if (header_crc32c != metadata->header.header_crc32c) {
			plt_err("Invalid model, Header CRC mismatch");
			return -EINVAL;
		}
	}

	/* Payload CRC check */
	if (metadata->header.payload_crc32c != 0) {
		payload_crc32c = rte_hash_crc(buffer + sizeof(metadata->header),
					      size - sizeof(metadata->header), 0);

		if (payload_crc32c != metadata->header.payload_crc32c) {
			plt_err("Invalid model, Payload CRC mismatch");
			return -EINVAL;
		}
	}

	/* Model magic string */
	if (strncmp((char *)metadata->header.magic, MRVL_ML_MODEL_MAGIC_STRING, 4) != 0) {
		plt_err("Invalid model, magic = %s", metadata->header.magic);
		return -EINVAL;
	}

	/* Target architecture */
	if (metadata->header.target_architecture != MRVL_ML_MODEL_TARGET_ARCH) {
		plt_err("Model target architecture (%u) not supported",
			metadata->header.target_architecture);
		return -ENOTSUP;
	}

	/* Header version */
	version = metadata->header.version[0] * 1000 + metadata->header.version[1] * 100 +
		  metadata->header.version[2] * 10 + metadata->header.version[3];
	if (version < MRVL_ML_MODEL_VERSION_MIN) {
		plt_err("Metadata version = %u.%u.%u.%u (< %u.%u.%u.%u) not supported",
			metadata->header.version[0], metadata->header.version[1],
			metadata->header.version[2], metadata->header.version[3],
			(MRVL_ML_MODEL_VERSION_MIN / 1000) % 10,
			(MRVL_ML_MODEL_VERSION_MIN / 100) % 10,
			(MRVL_ML_MODEL_VERSION_MIN / 10) % 10, MRVL_ML_MODEL_VERSION_MIN % 10);
		return -ENOTSUP;
	}

	/* Init section */
	if (metadata->init_model.file_size == 0) {
		plt_err("Invalid metadata, init_model.file_size = %u",
			metadata->init_model.file_size);
		return -EINVAL;
	}

	/* Main section */
	if (metadata->main_model.file_size == 0) {
		plt_err("Invalid metadata, main_model.file_size = %u",
			metadata->main_model.file_size);
		return -EINVAL;
	}

	/* Finish section */
	if (metadata->finish_model.file_size == 0) {
		plt_err("Invalid metadata, finish_model.file_size = %u",
			metadata->finish_model.file_size);
		return -EINVAL;
	}

	/* Weights and Bias */
	if (metadata->weights_bias.file_size == 0) {
		plt_err("Invalid metadata, weights_bias.file_size = %u",
			metadata->weights_bias.file_size);
		return -EINVAL;
	}

	if (metadata->weights_bias.relocatable != 1) {
		plt_err("Model not supported, non-relocatable weights and bias");
		return -ENOTSUP;
	}

	/* Check input count */
	if (version < 2301) {
		if (metadata->model.num_input > MRVL_ML_NUM_INPUT_OUTPUT_1) {
			plt_err("Invalid metadata, num_input  = %u (> %u)",
				metadata->model.num_input, MRVL_ML_NUM_INPUT_OUTPUT_1);
			return -EINVAL;
		}

		/* Check output count */
		if (metadata->model.num_output > MRVL_ML_NUM_INPUT_OUTPUT_1) {
			plt_err("Invalid metadata, num_output  = %u (> %u)",
				metadata->model.num_output, MRVL_ML_NUM_INPUT_OUTPUT_1);
			return -EINVAL;
		}
	} else {
		if (metadata->model.num_input > MRVL_ML_NUM_INPUT_OUTPUT) {
			plt_err("Invalid metadata, num_input  = %u (> %u)",
				metadata->model.num_input, MRVL_ML_NUM_INPUT_OUTPUT);
			return -EINVAL;
		}

		/* Check output count */
		if (metadata->model.num_output > MRVL_ML_NUM_INPUT_OUTPUT) {
			plt_err("Invalid metadata, num_output  = %u (> %u)",
				metadata->model.num_output, MRVL_ML_NUM_INPUT_OUTPUT);
			return -EINVAL;
		}
	}

	/* Inputs */
	for (i = 0; i < metadata->model.num_input; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			if (rte_ml_io_type_size_get(
				    cn10k_ml_io_type_map(metadata->input1[i].input_type)) <= 0) {
				plt_err("Invalid metadata, input1[%u] : input_type = %u", i,
					metadata->input1[i].input_type);
				return -EINVAL;
			}

			if (rte_ml_io_type_size_get(cn10k_ml_io_type_map(
				    metadata->input1[i].model_input_type)) <= 0) {
				plt_err("Invalid metadata, input1[%u] : model_input_type = %u", i,
					metadata->input1[i].model_input_type);
				return -EINVAL;
			}

			if (metadata->input1[i].relocatable != 1) {
				plt_err("Model not supported, non-relocatable input1: %u", i);
				return -ENOTSUP;
			}
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;
			if (rte_ml_io_type_size_get(
				    cn10k_ml_io_type_map(metadata->input2[j].input_type)) <= 0) {
				plt_err("Invalid metadata, input2[%u] : input_type = %u", j,
					metadata->input2[j].input_type);
				return -EINVAL;
			}

			if (rte_ml_io_type_size_get(cn10k_ml_io_type_map(
				    metadata->input2[j].model_input_type)) <= 0) {
				plt_err("Invalid metadata, input2[%u] : model_input_type = %u", j,
					metadata->input2[j].model_input_type);
				return -EINVAL;
			}

			if (metadata->input2[j].relocatable != 1) {
				plt_err("Model not supported, non-relocatable input2: %u", j);
				return -ENOTSUP;
			}
		}
	}

	/* Outputs */
	for (i = 0; i < metadata->model.num_output; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			if (rte_ml_io_type_size_get(
				    cn10k_ml_io_type_map(metadata->output1[i].output_type)) <= 0) {
				plt_err("Invalid metadata, output1[%u] : output_type = %u", i,
					metadata->output1[i].output_type);
				return -EINVAL;
			}

			if (rte_ml_io_type_size_get(cn10k_ml_io_type_map(
				    metadata->output1[i].model_output_type)) <= 0) {
				plt_err("Invalid metadata, output1[%u] : model_output_type = %u", i,
					metadata->output1[i].model_output_type);
				return -EINVAL;
			}

			if (metadata->output1[i].relocatable != 1) {
				plt_err("Model not supported, non-relocatable output1: %u", i);
				return -ENOTSUP;
			}
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;
			if (rte_ml_io_type_size_get(
				    cn10k_ml_io_type_map(metadata->output2[j].output_type)) <= 0) {
				plt_err("Invalid metadata, output2[%u] : output_type = %u", j,
					metadata->output2[j].output_type);
				return -EINVAL;
			}

			if (rte_ml_io_type_size_get(cn10k_ml_io_type_map(
				    metadata->output2[j].model_output_type)) <= 0) {
				plt_err("Invalid metadata, output2[%u] : model_output_type = %u", j,
					metadata->output2[j].model_output_type);
				return -EINVAL;
			}

			if (metadata->output2[j].relocatable != 1) {
				plt_err("Model not supported, non-relocatable output2: %u", j);
				return -ENOTSUP;
			}
		}
	}

	return 0;
}

void
cn10k_ml_model_metadata_update(struct cn10k_ml_model_metadata *metadata)
{
	uint8_t i;
	uint8_t j;

	for (i = 0; i < metadata->model.num_input; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			metadata->input1[i].input_type =
				cn10k_ml_io_type_map(metadata->input1[i].input_type);
			metadata->input1[i].model_input_type =
				cn10k_ml_io_type_map(metadata->input1[i].model_input_type);

			if (metadata->input1[i].shape.w == 0)
				metadata->input1[i].shape.w = 1;

			if (metadata->input1[i].shape.x == 0)
				metadata->input1[i].shape.x = 1;

			if (metadata->input1[i].shape.y == 0)
				metadata->input1[i].shape.y = 1;

			if (metadata->input1[i].shape.z == 0)
				metadata->input1[i].shape.z = 1;
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;
			metadata->input2[j].input_type =
				cn10k_ml_io_type_map(metadata->input2[j].input_type);
			metadata->input2[j].model_input_type =
				cn10k_ml_io_type_map(metadata->input2[j].model_input_type);

			if (metadata->input2[j].shape.w == 0)
				metadata->input2[j].shape.w = 1;

			if (metadata->input2[j].shape.x == 0)
				metadata->input2[j].shape.x = 1;

			if (metadata->input2[j].shape.y == 0)
				metadata->input2[j].shape.y = 1;

			if (metadata->input2[j].shape.z == 0)
				metadata->input2[j].shape.z = 1;
		}
	}

	for (i = 0; i < metadata->model.num_output; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			metadata->output1[i].output_type =
				cn10k_ml_io_type_map(metadata->output1[i].output_type);
			metadata->output1[i].model_output_type =
				cn10k_ml_io_type_map(metadata->output1[i].model_output_type);
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;
			metadata->output2[j].output_type =
				cn10k_ml_io_type_map(metadata->output2[j].output_type);
			metadata->output2[j].model_output_type =
				cn10k_ml_io_type_map(metadata->output2[j].model_output_type);
		}
	}
}

void
cn10k_ml_model_addr_update(struct cn10k_ml_model *model, uint8_t *buffer, uint8_t *base_dma_addr)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cn10k_ml_model_addr *addr;
	size_t model_data_size;
	uint8_t *dma_addr_load;
	uint8_t *dma_addr_run;
	uint8_t i;
	uint8_t j;
	int fpos;

	metadata = &model->metadata;
	addr = &model->addr;
	model_data_size = metadata->init_model.file_size + metadata->main_model.file_size +
			  metadata->finish_model.file_size + metadata->weights_bias.file_size;

	/* Base address */
	addr->base_dma_addr_load = base_dma_addr;
	addr->base_dma_addr_run = PLT_PTR_ADD(addr->base_dma_addr_load, model_data_size);

	/* Init section */
	dma_addr_load = addr->base_dma_addr_load;
	dma_addr_run = addr->base_dma_addr_run;
	fpos = sizeof(struct cn10k_ml_model_metadata);
	addr->init_load_addr = dma_addr_load;
	addr->init_run_addr = dma_addr_run;
	rte_memcpy(dma_addr_load, PLT_PTR_ADD(buffer, fpos), metadata->init_model.file_size);

	/* Main section */
	dma_addr_load += metadata->init_model.file_size;
	dma_addr_run += metadata->init_model.file_size;
	fpos += metadata->init_model.file_size;
	addr->main_load_addr = dma_addr_load;
	addr->main_run_addr = dma_addr_run;
	rte_memcpy(dma_addr_load, PLT_PTR_ADD(buffer, fpos), metadata->main_model.file_size);

	/* Finish section */
	dma_addr_load += metadata->main_model.file_size;
	dma_addr_run += metadata->main_model.file_size;
	fpos += metadata->main_model.file_size;
	addr->finish_load_addr = dma_addr_load;
	addr->finish_run_addr = dma_addr_run;
	rte_memcpy(dma_addr_load, PLT_PTR_ADD(buffer, fpos), metadata->finish_model.file_size);

	/* Weights and Bias section */
	dma_addr_load += metadata->finish_model.file_size;
	fpos += metadata->finish_model.file_size;
	addr->wb_base_addr = PLT_PTR_SUB(dma_addr_load, metadata->weights_bias.mem_offset);
	addr->wb_load_addr = PLT_PTR_ADD(addr->wb_base_addr, metadata->weights_bias.mem_offset);
	rte_memcpy(addr->wb_load_addr, PLT_PTR_ADD(buffer, fpos), metadata->weights_bias.file_size);

	/* Inputs */
	addr->total_input_sz_d = 0;
	addr->total_input_sz_q = 0;
	for (i = 0; i < metadata->model.num_input; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			addr->input[i].nb_dims = 4;
			addr->input[i].shape[0] = metadata->input1[i].shape.w;
			addr->input[i].shape[1] = metadata->input1[i].shape.x;
			addr->input[i].shape[2] = metadata->input1[i].shape.y;
			addr->input[i].shape[3] = metadata->input1[i].shape.z;

			addr->input[i].nb_elements =
				metadata->input1[i].shape.w * metadata->input1[i].shape.x *
				metadata->input1[i].shape.y * metadata->input1[i].shape.z;
			addr->input[i].sz_d =
				addr->input[i].nb_elements *
				rte_ml_io_type_size_get(metadata->input1[i].input_type);
			addr->input[i].sz_q =
				addr->input[i].nb_elements *
				rte_ml_io_type_size_get(metadata->input1[i].model_input_type);
			addr->total_input_sz_d += addr->input[i].sz_d;
			addr->total_input_sz_q += addr->input[i].sz_q;

			plt_ml_dbg(
				"model_id = %u, input[%u] - w:%u x:%u y:%u z:%u, sz_d = %u sz_q = %u",
				model->model_id, i, metadata->input1[i].shape.w,
				metadata->input1[i].shape.x, metadata->input1[i].shape.y,
				metadata->input1[i].shape.z, addr->input[i].sz_d,
				addr->input[i].sz_q);
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;

			addr->input[i].nb_dims = 4;
			addr->input[i].shape[0] = metadata->input2[j].shape.w;
			addr->input[i].shape[1] = metadata->input2[j].shape.x;
			addr->input[i].shape[2] = metadata->input2[j].shape.y;
			addr->input[i].shape[3] = metadata->input2[j].shape.z;

			addr->input[i].nb_elements =
				metadata->input2[j].shape.w * metadata->input2[j].shape.x *
				metadata->input2[j].shape.y * metadata->input2[j].shape.z;
			addr->input[i].sz_d =
				addr->input[i].nb_elements *
				rte_ml_io_type_size_get(metadata->input2[j].input_type);
			addr->input[i].sz_q =
				addr->input[i].nb_elements *
				rte_ml_io_type_size_get(metadata->input2[j].model_input_type);
			addr->total_input_sz_d += addr->input[i].sz_d;
			addr->total_input_sz_q += addr->input[i].sz_q;

			plt_ml_dbg(
				"model_id = %u, input2[%u] - w:%u x:%u y:%u z:%u, sz_d = %u sz_q = %u",
				model->model_id, j, metadata->input2[j].shape.w,
				metadata->input2[j].shape.x, metadata->input2[j].shape.y,
				metadata->input2[j].shape.z, addr->input[i].sz_d,
				addr->input[i].sz_q);
		}
	}

	/* Outputs */
	addr->total_output_sz_q = 0;
	addr->total_output_sz_d = 0;
	for (i = 0; i < metadata->model.num_output; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			addr->output[i].nb_dims = 1;
			addr->output[i].shape[0] = metadata->output1[i].size;
			addr->output[i].nb_elements = metadata->output1[i].size;
			addr->output[i].sz_d =
				addr->output[i].nb_elements *
				rte_ml_io_type_size_get(metadata->output1[i].output_type);
			addr->output[i].sz_q =
				addr->output[i].nb_elements *
				rte_ml_io_type_size_get(metadata->output1[i].model_output_type);
			addr->total_output_sz_q += addr->output[i].sz_q;
			addr->total_output_sz_d += addr->output[i].sz_d;

			plt_ml_dbg("model_id = %u, output[%u] - sz_d = %u, sz_q = %u",
				   model->model_id, i, addr->output[i].sz_d, addr->output[i].sz_q);
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;

			addr->output[i].nb_dims = 1;
			addr->output[i].shape[0] = metadata->output2[j].size;
			addr->output[i].nb_elements = metadata->output2[j].size;
			addr->output[i].sz_d =
				addr->output[i].nb_elements *
				rte_ml_io_type_size_get(metadata->output2[j].output_type);
			addr->output[i].sz_q =
				addr->output[i].nb_elements *
				rte_ml_io_type_size_get(metadata->output2[j].model_output_type);
			addr->total_output_sz_q += addr->output[i].sz_q;
			addr->total_output_sz_d += addr->output[i].sz_d;

			plt_ml_dbg("model_id = %u, output2[%u] - sz_d = %u, sz_q = %u",
				   model->model_id, j, addr->output[i].sz_d, addr->output[i].sz_q);
		}
	}
}

int
cn10k_ml_model_ocm_pages_count(struct cn10k_ml_dev *mldev, uint16_t model_id, uint8_t *buffer,
			       uint16_t *wb_pages, uint16_t *scratch_pages)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cn10k_ml_ocm *ocm;
	uint64_t scratch_size;
	uint64_t wb_size;

	metadata = (struct cn10k_ml_model_metadata *)buffer;
	ocm = &mldev->ocm;

	/* Assume wb_size is zero for non-relocatable models */
	if (metadata->model.ocm_relocatable)
		wb_size = metadata->model.ocm_wb_range_end - metadata->model.ocm_wb_range_start + 1;
	else
		wb_size = 0;

	if (wb_size % ocm->page_size)
		*wb_pages = wb_size / ocm->page_size + 1;
	else
		*wb_pages = wb_size / ocm->page_size;
	plt_ml_dbg("model_id = %u, wb_size = %" PRIu64 ", wb_pages = %u", model_id, wb_size,
		   *wb_pages);

	scratch_size = ocm->size_per_tile - metadata->model.ocm_tmp_range_floor;
	if (metadata->model.ocm_tmp_range_floor % ocm->page_size)
		*scratch_pages = scratch_size / ocm->page_size + 1;
	else
		*scratch_pages = scratch_size / ocm->page_size;
	plt_ml_dbg("model_id = %u, scratch_size = %" PRIu64 ", scratch_pages = %u", model_id,
		   scratch_size, *scratch_pages);

	/* Check if the model can be loaded on OCM */
	if ((*wb_pages + *scratch_pages) > mldev->ocm.num_pages) {
		plt_err("Cannot create the model, OCM relocatable = %u",
			metadata->model.ocm_relocatable);
		plt_err("wb_pages (%u) + scratch_pages (%u) > %u", *wb_pages, *scratch_pages,
			mldev->ocm.num_pages);
		return -ENOMEM;
	}

	/* Update scratch_pages to block the full tile for OCM non-relocatable model. This would
	 * prevent the library from allocating the remaining space on the tile to other models.
	 */
	if (!metadata->model.ocm_relocatable)
		*scratch_pages =
			PLT_MAX(PLT_U64_CAST(*scratch_pages), PLT_U64_CAST(mldev->ocm.num_pages));

	return 0;
}

void
cn10k_ml_model_info_set(struct rte_ml_dev *dev, struct cn10k_ml_model *model)
{
	struct cn10k_ml_model_metadata *metadata;
	struct cn10k_ml_model_addr *addr;
	struct rte_ml_model_info *info;
	struct rte_ml_io_info *output;
	struct rte_ml_io_info *input;
	struct cn10k_ml_dev *mldev;
	uint8_t i;
	uint8_t j;

	mldev = dev->data->dev_private;
	metadata = &model->metadata;
	info = PLT_PTR_CAST(model->info);
	input = PLT_PTR_ADD(info, sizeof(struct rte_ml_model_info));
	output = PLT_PTR_ADD(input, metadata->model.num_input * sizeof(struct rte_ml_io_info));
	addr = &model->addr;

	/* Set model info */
	memset(info, 0, sizeof(struct rte_ml_model_info));
	rte_memcpy(info->name, metadata->model.name, MRVL_ML_MODEL_NAME_LEN);
	snprintf(info->version, RTE_ML_STR_MAX, "%u.%u.%u.%u", metadata->model.version[0],
		 metadata->model.version[1], metadata->model.version[2],
		 metadata->model.version[3]);
	info->model_id = model->model_id;
	info->device_id = dev->data->dev_id;
	info->io_layout = RTE_ML_IO_LAYOUT_PACKED;
	info->min_batches = model->batch_size;
	info->max_batches = mldev->fw.req->jd.fw_load.cap.s.max_num_batches / model->batch_size;
	info->nb_inputs = metadata->model.num_input;
	info->input_info = input;
	info->nb_outputs = metadata->model.num_output;
	info->output_info = output;
	info->wb_size = metadata->weights_bias.file_size;

	/* Set input info */
	for (i = 0; i < info->nb_inputs; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			rte_memcpy(input[i].name, metadata->input1[i].input_name,
				   MRVL_ML_INPUT_NAME_LEN);
			input[i].nb_dims = addr->input[i].nb_dims;
			input[i].shape = addr->input[i].shape;
			input[i].type = metadata->input1[i].model_input_type;
			input[i].nb_elements = addr->input[i].nb_elements;
			input[i].size =
				addr->input[i].nb_elements *
				rte_ml_io_type_size_get(metadata->input1[i].model_input_type);
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;

			rte_memcpy(input[i].name, metadata->input2[j].input_name,
				   MRVL_ML_INPUT_NAME_LEN);
			input[i].nb_dims = addr->input[i].nb_dims;
			input[i].shape = addr->input[i].shape;
			input[i].type = metadata->input2[j].model_input_type;
			input[i].nb_elements = addr->input[i].nb_elements;
			input[i].size =
				addr->input[i].nb_elements *
				rte_ml_io_type_size_get(metadata->input2[j].model_input_type);
		}
	}

	/* Set output info */
	for (i = 0; i < info->nb_outputs; i++) {
		if (i < MRVL_ML_NUM_INPUT_OUTPUT_1) {
			rte_memcpy(output[i].name, metadata->output1[i].output_name,
				   MRVL_ML_OUTPUT_NAME_LEN);
			output[i].nb_dims = addr->output[i].nb_dims;
			output[i].shape = addr->output[i].shape;
			output[i].type = metadata->output1[i].model_output_type;
			output[i].nb_elements = addr->output[i].nb_elements;
			output[i].size =
				addr->output[i].nb_elements *
				rte_ml_io_type_size_get(metadata->output1[i].model_output_type);
		} else {
			j = i - MRVL_ML_NUM_INPUT_OUTPUT_1;

			rte_memcpy(output[i].name, metadata->output2[j].output_name,
				   MRVL_ML_OUTPUT_NAME_LEN);
			output[i].nb_dims = addr->output[i].nb_dims;
			output[i].shape = addr->output[i].shape;
			output[i].type = metadata->output2[j].model_output_type;
			output[i].nb_elements = addr->output[i].nb_elements;
			output[i].size =
				addr->output[i].nb_elements *
				rte_ml_io_type_size_get(metadata->output2[j].model_output_type);
		}
	}
}
