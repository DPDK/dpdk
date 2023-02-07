/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_hash_crc.h>

#include <mldev_utils.h>

#include "cn10k_ml_dev.h"
#include "cn10k_ml_model.h"

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
	uint8_t version[4];
	uint8_t i;

	metadata = (struct cn10k_ml_model_metadata *)buffer;

	/* Header CRC check */
	if (metadata->metadata_header.header_crc32c != 0) {
		header_crc32c = rte_hash_crc(
			buffer, sizeof(metadata->metadata_header) - sizeof(uint32_t), 0);

		if (header_crc32c != metadata->metadata_header.header_crc32c) {
			plt_err("Invalid model, Header CRC mismatch");
			return -EINVAL;
		}
	}

	/* Payload CRC check */
	if (metadata->metadata_header.payload_crc32c != 0) {
		payload_crc32c = rte_hash_crc(buffer + sizeof(metadata->metadata_header),
					      size - sizeof(metadata->metadata_header), 0);

		if (payload_crc32c != metadata->metadata_header.payload_crc32c) {
			plt_err("Invalid model, Payload CRC mismatch");
			return -EINVAL;
		}
	}

	/* Model magic string */
	if (strncmp((char *)metadata->metadata_header.magic, MRVL_ML_MODEL_MAGIC_STRING, 4) != 0) {
		plt_err("Invalid model, magic = %s", metadata->metadata_header.magic);
		return -EINVAL;
	}

	/* Target architecture */
	if (metadata->metadata_header.target_architecture != MRVL_ML_MODEL_TARGET_ARCH) {
		plt_err("Model target architecture (%u) not supported",
			metadata->metadata_header.target_architecture);
		return -ENOTSUP;
	}

	/* Header version */
	rte_memcpy(version, metadata->metadata_header.version, 4 * sizeof(uint8_t));
	if (version[0] * 1000 + version[1] * 100 < MRVL_ML_MODEL_VERSION) {
		plt_err("Metadata version = %u.%u.%u.%u (< %u.%u.%u.%u) not supported", version[0],
			version[1], version[2], version[3], (MRVL_ML_MODEL_VERSION / 1000) % 10,
			(MRVL_ML_MODEL_VERSION / 100) % 10, (MRVL_ML_MODEL_VERSION / 10) % 10,
			MRVL_ML_MODEL_VERSION % 10);
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
	if (metadata->model.num_input > MRVL_ML_INPUT_OUTPUT_SIZE) {
		plt_err("Invalid metadata, num_input  = %u (> %u)", metadata->model.num_input,
			MRVL_ML_INPUT_OUTPUT_SIZE);
		return -EINVAL;
	}

	/* Check output count */
	if (metadata->model.num_output > MRVL_ML_INPUT_OUTPUT_SIZE) {
		plt_err("Invalid metadata, num_output  = %u (> %u)", metadata->model.num_output,
			MRVL_ML_INPUT_OUTPUT_SIZE);
		return -EINVAL;
	}

	/* Inputs */
	for (i = 0; i < metadata->model.num_input; i++) {
		if (rte_ml_io_type_size_get(cn10k_ml_io_type_map(metadata->input[i].input_type)) <=
		    0) {
			plt_err("Invalid metadata, input[%u] : input_type = %u", i,
				metadata->input[i].input_type);
			return -EINVAL;
		}

		if (rte_ml_io_type_size_get(
			    cn10k_ml_io_type_map(metadata->input[i].model_input_type)) <= 0) {
			plt_err("Invalid metadata, input[%u] : model_input_type = %u", i,
				metadata->input[i].model_input_type);
			return -EINVAL;
		}

		if (metadata->input[i].relocatable != 1) {
			plt_err("Model not supported, non-relocatable input: %u", i);
			return -ENOTSUP;
		}
	}

	/* Outputs */
	for (i = 0; i < metadata->model.num_output; i++) {
		if (rte_ml_io_type_size_get(
			    cn10k_ml_io_type_map(metadata->output[i].output_type)) <= 0) {
			plt_err("Invalid metadata, output[%u] : output_type = %u", i,
				metadata->output[i].output_type);
			return -EINVAL;
		}

		if (rte_ml_io_type_size_get(
			    cn10k_ml_io_type_map(metadata->output[i].model_output_type)) <= 0) {
			plt_err("Invalid metadata, output[%u] : model_output_type = %u", i,
				metadata->output[i].model_output_type);
			return -EINVAL;
		}

		if (metadata->output[i].relocatable != 1) {
			plt_err("Model not supported, non-relocatable output: %u", i);
			return -ENOTSUP;
		}
	}

	return 0;
}

void
cn10k_ml_model_metadata_update(struct cn10k_ml_model_metadata *metadata)
{
	uint8_t i;

	for (i = 0; i < metadata->model.num_input; i++) {
		metadata->input[i].input_type = cn10k_ml_io_type_map(metadata->input[i].input_type);
		metadata->input[i].model_input_type =
			cn10k_ml_io_type_map(metadata->input[i].model_input_type);

		if (metadata->input[i].shape.w == 0)
			metadata->input[i].shape.w = 1;

		if (metadata->input[i].shape.x == 0)
			metadata->input[i].shape.x = 1;

		if (metadata->input[i].shape.y == 0)
			metadata->input[i].shape.y = 1;

		if (metadata->input[i].shape.z == 0)
			metadata->input[i].shape.z = 1;
	}

	for (i = 0; i < metadata->model.num_output; i++) {
		metadata->output[i].output_type =
			cn10k_ml_io_type_map(metadata->output[i].output_type);
		metadata->output[i].model_output_type =
			cn10k_ml_io_type_map(metadata->output[i].model_output_type);
	}
}
