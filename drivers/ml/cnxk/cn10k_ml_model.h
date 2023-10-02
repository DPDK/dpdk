/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_MODEL_H_
#define _CN10K_ML_MODEL_H_

#include <rte_mldev.h>

#include <roc_api.h>

#include "cn10k_ml_dev.h"
#include "cn10k_ml_ocm.h"
#include "cn10k_ml_ops.h"

/* Model state */
enum cn10k_ml_model_state {
	ML_CN10K_MODEL_STATE_LOADED,
	ML_CN10K_MODEL_STATE_JOB_ACTIVE,
	ML_CN10K_MODEL_STATE_STARTED,
	ML_CN10K_MODEL_STATE_UNKNOWN,
};

/* Model Metadata : v 2.3.0.1 */
#define MRVL_ML_MODEL_MAGIC_STRING "MRVL"
#define MRVL_ML_MODEL_TARGET_ARCH  128
#define MRVL_ML_MODEL_VERSION_MIN  2100
#define MRVL_ML_MODEL_NAME_LEN	   64
#define MRVL_ML_INPUT_NAME_LEN	   16
#define MRVL_ML_OUTPUT_NAME_LEN	   16
#define MRVL_ML_NUM_INPUT_OUTPUT_1 8
#define MRVL_ML_NUM_INPUT_OUTPUT_2 24
#define MRVL_ML_NUM_INPUT_OUTPUT   (MRVL_ML_NUM_INPUT_OUTPUT_1 + MRVL_ML_NUM_INPUT_OUTPUT_2)

/* Header (256-byte) */
struct cn10k_ml_model_metadata_header {
	/* Magic string ('M', 'R', 'V', 'L') */
	uint8_t magic[4];

	/* Metadata version */
	uint8_t version[4];

	/* Metadata size */
	uint32_t metadata_size;

	/* Unique ID */
	uint8_t uuid[128];

	/* Model target architecture
	 * 0 = Undefined
	 * 1 = M1K
	 * 128 = MLIP
	 * 256 = Experimental
	 */
	uint32_t target_architecture;
	uint8_t reserved[104];

	/* CRC of data after header (i.e. after first 256 bytes) */
	uint32_t payload_crc32c;

	/* CRC of first 252 bytes of header, after payload_crc calculation */
	uint32_t header_crc32c;
};

/* Model information (256-byte) */
struct cn10k_ml_model_metadata_model {
	/* Model name string */
	uint8_t name[MRVL_ML_MODEL_NAME_LEN];

	/* Model version info (xx.xx.xx.xx) */
	uint8_t version[4];

	/* Model code size (Init + Main + Finish) */
	uint32_t code_size;

	/* Model data size (Weights and Bias) */
	uint32_t data_size;

	/* OCM start offset, set to ocm_wb_range_start */
	uint32_t ocm_start;

	/* OCM start offset, set to max OCM size */
	uint32_t ocm_end;

	/* Relocatable flag (always yes)
	 * 0 = Not relocatable
	 * 1 = Relocatable
	 */
	uint8_t ocm_relocatable;

	/* Tile relocatable flag (always yes)
	 * 0 = Not relocatable
	 * 1 = Relocatable
	 */
	uint8_t tile_relocatable;

	/* Start tile (Always 0) */
	uint8_t tile_start;

	/* End tile (num_tiles - 1) */
	uint8_t tile_end;

	/* Inference batch size */
	uint8_t batch_size;

	/* Number of input tensors (Max 32) */
	uint8_t num_input;

	/* Number of output tensors (Max 32) */
	uint8_t num_output;
	uint8_t reserved_1;

	/* Total input size in bytes */
	uint32_t input_size;

	/* Total output size in bytes */
	uint32_t output_size;

	/* Table size in bytes */
	uint32_t table_size;

	/* Number of layers in the network */
	uint32_t num_layers;
	uint32_t reserved_2;

	/* Floor of absolute OCM region */
	uint64_t ocm_tmp_range_floor;

	/* Relative OCM start address of WB data block */
	uint64_t ocm_wb_range_start;

	/* Relative OCM end address of WB data block */
	uint64_t ocm_wb_range_end;

	/* Relative DDR start address of WB data block */
	uint64_t ddr_wb_range_start;

	/* Relative DDR end address of all outputs */
	uint64_t ddr_wb_range_end;

	/* Relative DDR start address of all inputs */
	uint64_t ddr_input_range_start;

	/* Relative DDR end address of all inputs */
	uint64_t ddr_input_range_end;

	/* Relative DDR start address of all outputs */
	uint64_t ddr_output_range_start;

	/* Relative DDR end address of all outputs */
	uint64_t ddr_output_range_end;

	/* Compiler version */
	uint8_t compiler_version[8];

	/* CDK version */
	uint8_t cdk_version[4];

	/* Lower batch optimization support
	 * 0 - No,
	 * 1 - Yes
	 */
	uint8_t supports_lower_batch_size_optimization;
	uint8_t reserved_3[3];

	/* Relative DDR start address of scratch space */
	uint64_t ddr_scratch_range_start;

	/* Relative DDR end address of scratch space */
	uint64_t ddr_scratch_range_end;
	uint8_t reserved_4[40];
};

/* Init section (64-byte) */
struct cn10k_ml_model_metadata_init_section {
	uint32_t file_offset;
	uint32_t file_size;
	uint8_t reserved[56];
};

/* Main section (64-byte) */
struct cn10k_ml_model_metadata_main_section {
	uint32_t file_offset;
	uint32_t file_size;
	uint8_t reserved[56];
};

/* Finish section (64-byte) */
struct cn10k_ml_model_metadata_finish_section {
	uint32_t file_offset;
	uint32_t file_size;
	uint8_t reserved[56];
};

/* Weights and Bias (64-byte) */
struct cn10k_ml_model_metadata_weights_bias_section {
	/* Memory offset, set to ddr_wb_range_start */
	uint64_t mem_offset;
	uint32_t file_offset;
	uint32_t file_size;

	/* Relocatable flag for WB
	 * 1 = Relocatable
	 * 2 = Not relocatable
	 */
	uint8_t relocatable;
	uint8_t reserved[47];
};

/* Input section (64-byte per input) */
struct cn10k_ml_model_metadata_input_section {
	/* DDR offset (in OCM absolute addresses for input) */
	uint64_t mem_offset;

	/* Relocatable flag
	 * 1 = Relocatable
	 * 2 = Not relocatable
	 */
	uint8_t relocatable;

	/* Input quantization
	 * 1 = Requires quantization
	 * 2 = Pre-quantized
	 */
	uint8_t quantize;

	/* Type of incoming input
	 * 1 = INT8, 2 = UINT8, 3 = INT16, 4 = UINT16,
	 * 5 = INT32, 6 = UINT32, 7 = FP16, 8 = FP32
	 */
	uint8_t input_type;

	/* Type of input required by model
	 * 1 = INT8, 2 = UINT8, 3 = INT16, 4 = UINT16,
	 * 5 = INT32, 6 = UINT32, 7 = FP16, 8 = FP32
	 */
	uint8_t model_input_type;

	/* float_32 qscale value
	 * quantized = non-quantized * qscale
	 */
	float qscale;

	/* Input shape */
	struct {
		/* Input format
		 * 1 = NCHW
		 * 2 = NHWC
		 */
		uint8_t format;
		uint8_t reserved[3];
		uint32_t w;
		uint32_t x;
		uint32_t y;
		uint32_t z;
	} shape;
	uint8_t reserved[4];

	/* Name of input */
	uint8_t input_name[MRVL_ML_INPUT_NAME_LEN];

	/* DDR range end
	 * new = mem_offset + size_bytes - 1
	 */
	uint64_t ddr_range_end;
};

/* Output section (64-byte per output) */
struct cn10k_ml_model_metadata_output_section {
	/* DDR offset in OCM absolute addresses for output */
	uint64_t mem_offset;

	/* Relocatable flag
	 * 1 = Relocatable
	 * 2 = Not relocatable
	 */
	uint8_t relocatable;

	/* Output dequantization
	 * 1 = De-quantization required
	 * 2 = De-quantization not required
	 */
	uint8_t dequantize;

	/* Type of outgoing output
	 * 1 = INT8, 2 = UINT8, 3 = INT16, 4 = UINT16
	 * 5 = INT32, 6 = UINT32, 7 = FP16, 8 = FP32
	 */
	uint8_t output_type;

	/* Type of output produced by model
	 * 1 = INT8, 2 = UINT8, 3 = INT16, 4 = UINT16
	 * 5 = INT32, 6 = UINT32, 7 = FP16, 8 = FP32
	 */
	uint8_t model_output_type;

	/* float_32 dscale value
	 * dequantized = quantized * dscale
	 */
	float dscale;

	/* Number of items in the output */
	uint32_t size;
	uint8_t reserved[20];

	/* DDR range end
	 * new = mem_offset + size_bytes - 1
	 */
	uint64_t ddr_range_end;
	uint8_t output_name[MRVL_ML_OUTPUT_NAME_LEN];
};

/* Model data */
struct cn10k_ml_model_metadata_data_section {
	uint8_t reserved[996];

	/* Beta: xx.xx.xx.xx,
	 * Later: YYYYMM.xx.xx
	 */
	uint8_t compiler_version[8];

	/* M1K CDK version (xx.xx.xx.xx) */
	uint8_t m1k_cdk_version[4];
};

/* Model file metadata structure */
struct cn10k_ml_model_metadata {
	/* Header (256-byte) */
	struct cn10k_ml_model_metadata_header header;

	/* Model information (256-byte) */
	struct cn10k_ml_model_metadata_model model;

	/* Init section (64-byte) */
	struct cn10k_ml_model_metadata_init_section init_model;

	/* Main section (64-byte) */
	struct cn10k_ml_model_metadata_main_section main_model;

	/* Finish section (64-byte) */
	struct cn10k_ml_model_metadata_finish_section finish_model;

	uint8_t reserved_1[512]; /* End of 2k bytes */

	/* Weights and Bias (64-byte) */
	struct cn10k_ml_model_metadata_weights_bias_section weights_bias;

	/* Input (512-bytes, 64-byte per input) provisioned for 8 inputs */
	struct cn10k_ml_model_metadata_input_section input1[MRVL_ML_NUM_INPUT_OUTPUT_1];

	/* Output (512-bytes, 64-byte per output) provisioned for 8 outputs */
	struct cn10k_ml_model_metadata_output_section output1[MRVL_ML_NUM_INPUT_OUTPUT_1];

	uint8_t reserved_2[1792];

	/* Input (1536-bytes, 64-byte per input) provisioned for 24 inputs */
	struct cn10k_ml_model_metadata_input_section input2[MRVL_ML_NUM_INPUT_OUTPUT_2];

	/* Output (1536-bytes, 64-byte per output) provisioned for 24 outputs */
	struct cn10k_ml_model_metadata_output_section output2[MRVL_ML_NUM_INPUT_OUTPUT_2];

	/* Model data */
	struct cn10k_ml_model_metadata_data_section data;

	/* Hidden 16 bytes of magic code */
	uint8_t reserved_3[16];
};

/* Model address structure */
struct cn10k_ml_model_addr {
	/* Base DMA address for load */
	void *base_dma_addr_load;

	/* Base DMA address for run */
	void *base_dma_addr_run;

	/* Init section load address */
	void *init_load_addr;

	/* Init section run address */
	void *init_run_addr;

	/* Main section load address */
	void *main_load_addr;

	/* Main section run address */
	void *main_run_addr;

	/* Finish section load address */
	void *finish_load_addr;

	/* Finish section run address */
	void *finish_run_addr;

	/* Weights and Bias base address */
	void *wb_base_addr;

	/* Weights and bias load address */
	void *wb_load_addr;

	/* Scratch base address */
	void *scratch_base_addr;

	/* Start tile */
	uint8_t tile_start;

	/* End tile */
	uint8_t tile_end;

	/* Input address and size */
	struct {
		/* Number of dimensions in shape */
		uint32_t nb_dims;

		/* Shape of input */
		uint32_t shape[4];

		/* Number of elements */
		uint32_t nb_elements;

		/* Dequantized input size */
		uint32_t sz_d;

		/* Quantized input size */
		uint32_t sz_q;
	} input[MRVL_ML_NUM_INPUT_OUTPUT];

	/* Output address and size */
	struct {
		/* Number of dimensions in shape */
		uint32_t nb_dims;

		/* Shape of input */
		uint32_t shape[4];

		/* Number of elements */
		uint32_t nb_elements;

		/* Dequantize output size */
		uint32_t sz_d;

		/* Quantized output size */
		uint32_t sz_q;
	} output[MRVL_ML_NUM_INPUT_OUTPUT];

	/* Total size of quantized input */
	uint32_t total_input_sz_q;

	/* Total size of dequantized input */
	uint32_t total_input_sz_d;

	/* Total size of quantized output */
	uint32_t total_output_sz_q;

	/* Total size of dequantized output */
	uint32_t total_output_sz_d;
};

/* Model fast-path stats */
struct cn10k_ml_model_stats {
	/* Total hardware latency, sum of all inferences */
	uint64_t hw_latency_tot;

	/* Minimum hardware latency */
	uint64_t hw_latency_min;

	/* Maximum hardware latency */
	uint64_t hw_latency_max;

	/* Total firmware latency, sum of all inferences */
	uint64_t fw_latency_tot;

	/* Minimum firmware latency */
	uint64_t fw_latency_min;

	/* Maximum firmware latency */
	uint64_t fw_latency_max;

	/* Total jobs dequeued */
	uint64_t dequeued_count;

	/* Hardware stats reset index */
	uint64_t hw_reset_count;

	/* Firmware stats reset index */
	uint64_t fw_reset_count;
};

/* Model Object */
struct cn10k_ml_model {
	/* Device reference */
	struct cn10k_ml_dev *mldev;

	/* Name */
	char name[RTE_ML_STR_MAX];

	/* ID */
	uint16_t model_id;

	/* Batch size */
	uint32_t batch_size;

	/* Metadata */
	struct cn10k_ml_model_metadata metadata;

	/* Address structure */
	struct cn10k_ml_model_addr addr;

	/* Tile and memory information object */
	struct cn10k_ml_ocm_model_map model_mem_map;

	/* Internal model information structure
	 * Size of the buffer = sizeof(struct rte_ml_model_info)
	 *                    + num_inputs * sizeof(struct rte_ml_io_info)
	 *                    + num_outputs * sizeof(struct rte_ml_io_info).
	 * Structures would be arranged in the same order in the buffer.
	 */
	uint8_t *info;

	/* Spinlock, used to update model state */
	plt_spinlock_t lock;

	/* State */
	enum cn10k_ml_model_state state;

	/* Slow-path operations request pointer */
	struct cn10k_ml_req *req;

	/* Stats for burst ops */
	struct cn10k_ml_model_stats *burst_stats;

	/* Stats for sync ops */
	struct cn10k_ml_model_stats *sync_stats;
};

int cn10k_ml_model_metadata_check(uint8_t *buffer, uint64_t size);
void cn10k_ml_model_metadata_update(struct cn10k_ml_model_metadata *metadata);
void cn10k_ml_model_addr_update(struct cn10k_ml_model *model, uint8_t *buffer,
				uint8_t *base_dma_addr);
int cn10k_ml_model_ocm_pages_count(struct cn10k_ml_dev *mldev, uint16_t model_id, uint8_t *buffer,
				   uint16_t *wb_pages, uint16_t *scratch_pages);
void cn10k_ml_model_info_set(struct rte_ml_dev *dev, struct cn10k_ml_model *model);

#endif /* _CN10K_ML_MODEL_H_ */
