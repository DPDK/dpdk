/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_IO_H_
#define _CNXK_ML_IO_H_

#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
#include <tvmdp.h>
#endif

#include <rte_mldev.h>

/* Maximum number of models per device */
#define ML_CNXK_MAX_MODELS 16

/* Maximum number of layers per model */
#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
#define ML_CNXK_MODEL_MAX_LAYERS TVMDP_MODEL_LAYERS_MAX
#else
#define ML_CNXK_MODEL_MAX_LAYERS 1
#endif

/* Maximum number of inputs or outputs per layer or model */
#define ML_CNXK_MODEL_MAX_INPUT_OUTPUT 32

/* Maximum number of dimensions per I/O shape */
#define ML_CNXK_MODEL_MAX_DIMS 8

/* Input / Output structure */
struct cnxk_ml_io {
	/* name */
	char name[RTE_ML_STR_MAX];

	/* dequantized data type */
	enum rte_ml_io_type dtype;

	/* quantized data type */
	enum rte_ml_io_type qtype;

	/* Number of dimensions in shape */
	uint32_t nb_dims;

	/* Shape of input */
	uint32_t shape[ML_CNXK_MODEL_MAX_DIMS];

	/* Number of elements */
	uint32_t nb_elements;

	/* Dequantized input size */
	uint32_t sz_d;

	/* Quantized input size */
	uint32_t sz_q;

	/* Scale */
	float scale;
};

/* Model / Layer IO structure */
struct cnxk_ml_io_info {
	/* Number of inputs */
	uint16_t nb_inputs;

	/* Model / Layer inputs */
	struct cnxk_ml_io input[ML_CNXK_MODEL_MAX_INPUT_OUTPUT];

	/* Total size of quantized input */
	uint32_t total_input_sz_q;

	/* Total size of dequantized input */
	uint32_t total_input_sz_d;

	/* Number of outputs */
	uint16_t nb_outputs;

	/* Model / Layer outputs */
	struct cnxk_ml_io output[ML_CNXK_MODEL_MAX_INPUT_OUTPUT];

	/* Total size of quantized output */
	uint32_t total_output_sz_q;

	/* Total size of dequantized output */
	uint32_t total_output_sz_d;
};

int cnxk_ml_io_quantize_single(struct cnxk_ml_io *input, uint8_t *dbuffer, uint8_t *qbuffer);
int cnxk_ml_io_dequantize_single(struct cnxk_ml_io *output, uint8_t *qbuffer, uint8_t *dbuffer);

#endif /* _CNXK_ML_IO_H_ */
