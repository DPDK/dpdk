/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_OPS_H_
#define _CN10K_ML_OPS_H_

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <roc_api.h>

struct cnxk_ml_dev;
struct cnxk_ml_qp;
struct cnxk_ml_model;
struct cnxk_ml_layer;
struct cnxk_ml_req;

/* Firmware version string length */
#define MLDEV_FIRMWARE_VERSION_LENGTH 32

/* Job types */
enum cn10k_ml_job_type {
	ML_CN10K_JOB_TYPE_MODEL_RUN = 0,
	ML_CN10K_JOB_TYPE_MODEL_STOP,
	ML_CN10K_JOB_TYPE_MODEL_START,
	ML_CN10K_JOB_TYPE_FIRMWARE_LOAD,
	ML_CN10K_JOB_TYPE_FIRMWARE_SELFTEST,
};

/* Firmware stats */
struct cn10k_ml_stats {
	/* Firmware start cycle */
	uint64_t fw_start;

	/* Firmware end cycle */
	uint64_t fw_end;

	/* Hardware start cycle */
	uint64_t hw_start;

	/* Hardware end cycle */
	uint64_t hw_end;
};

/* Result structure */
struct cn10k_ml_result {
	/* Job error code */
	uint64_t error_code;

	/* Stats */
	struct cn10k_ml_stats stats;

	/* User context pointer */
	void *user_ptr;
};

/* Firmware capability structure */
union cn10k_ml_fw_cap {
	uint64_t u64;

	struct {
		/* CMPC completion support */
		uint64_t cmpc_completions : 1;

		/* Poll mode completion support */
		uint64_t poll_completions : 1;

		/* SSO completion support */
		uint64_t sso_completions : 1;

		/* Support for model side loading */
		uint64_t side_load_model : 1;

		/* Batch execution */
		uint64_t batch_run : 1;

		/* Max number of models to be loaded in parallel */
		uint64_t max_models : 8;

		/* Firmware statistics */
		uint64_t fw_stats : 1;

		/* Hardware statistics */
		uint64_t hw_stats : 1;

		/* Max number of batches */
		uint64_t max_num_batches : 16;

		uint64_t rsvd : 33;
	} s;
};

/* Firmware debug info structure */
struct cn10k_ml_fw_debug {
	/* ACC core 0 debug buffer */
	uint64_t core0_debug_ptr;

	/* ACC core 1 debug buffer */
	uint64_t core1_debug_ptr;

	/* ACC core 0 exception state buffer */
	uint64_t core0_exception_buffer;

	/* ACC core 1 exception state buffer */
	uint64_t core1_exception_buffer;

	/* Debug buffer size per core */
	uint32_t debug_buffer_size;

	/* Exception state dump size */
	uint32_t exception_state_size;
};

/* Job descriptor header (32 bytes) */
struct cn10k_ml_jd_header {
	/* Job completion structure */
	struct ml_jce_s jce;

	/* Model ID */
	uint64_t model_id : 8;

	/* Job type */
	uint64_t job_type : 8;

	/* Flags for fast-path jobs */
	uint64_t fp_flags : 16;

	/* Flags for slow-path jobs */
	uint64_t sp_flags : 16;
	uint64_t rsvd : 16;

	/* Job result pointer */
	uint64_t *result;
};

/* Extra arguments for job descriptor */
union cn10k_ml_jd_extended_args {
	struct cn10k_ml_jd_extended_args_section_start {
		/* DDR Scratch base address */
		uint64_t ddr_scratch_base_address;

		/* DDR Scratch range start */
		uint64_t ddr_scratch_range_start;

		/* DDR Scratch range end */
		uint64_t ddr_scratch_range_end;

		uint8_t rsvd[104];
	} start;
};

/* Job descriptor structure */
struct cn10k_ml_jd {
	/* Job descriptor header (32 bytes) */
	struct cn10k_ml_jd_header hdr;

	union {
		struct cn10k_ml_jd_section_fw_load {
			/* Firmware capability structure (8 bytes) */
			union cn10k_ml_fw_cap cap;

			/* Firmware version (32 bytes) */
			uint8_t version[MLDEV_FIRMWARE_VERSION_LENGTH];

			/* Debug capability structure (40 bytes) */
			struct cn10k_ml_fw_debug debug;

			/* Flags to control error handling */
			uint64_t flags;

			uint8_t rsvd[8];
		} fw_load;

		struct cn10k_ml_jd_section_model_start {
			/* Extended arguments */
			uint64_t extended_args;

			/* Destination model start address in DDR relative to ML_MLR_BASE */
			uint64_t model_dst_ddr_addr;

			/* Offset to model init section in the model */
			uint64_t model_init_offset : 32;

			/* Size of init section in the model */
			uint64_t model_init_size : 32;

			/* Offset to model main section in the model */
			uint64_t model_main_offset : 32;

			/* Size of main section in the model */
			uint64_t model_main_size : 32;

			/* Offset to model finish section in the model */
			uint64_t model_finish_offset : 32;

			/* Size of finish section in the model */
			uint64_t model_finish_size : 32;

			/* Offset to WB in model bin */
			uint64_t model_wb_offset : 32;

			/* Number of model layers */
			uint64_t num_layers : 8;

			/* Number of gather entries, 0 means linear input mode (= no gather) */
			uint64_t num_gather_entries : 8;

			/* Number of scatter entries 0 means linear input mode (= no scatter) */
			uint64_t num_scatter_entries : 8;

			/* Tile mask to load model */
			uint64_t tilemask : 8;

			/* Batch size of model  */
			uint64_t batch_size : 32;

			/* OCM WB base address */
			uint64_t ocm_wb_base_address : 32;

			/* OCM WB range start */
			uint64_t ocm_wb_range_start : 32;

			/* OCM WB range End */
			uint64_t ocm_wb_range_end : 32;

			/* DDR WB address */
			uint64_t ddr_wb_base_address;

			/* DDR WB range start */
			uint64_t ddr_wb_range_start : 32;

			/* DDR WB range end */
			uint64_t ddr_wb_range_end : 32;

			union {
				/* Points to gather list if num_gather_entries > 0 */
				void *gather_list;
				struct {
					/* Linear input mode */
					uint64_t ddr_range_start : 32;
					uint64_t ddr_range_end : 32;
				} s;
			} input;

			union {
				/* Points to scatter list if num_scatter_entries > 0 */
				void *scatter_list;
				struct {
					/* Linear output mode */
					uint64_t ddr_range_start : 32;
					uint64_t ddr_range_end : 32;
				} s;
			} output;
		} model_start;

		struct cn10k_ml_jd_section_model_stop {
			uint8_t rsvd[96];
		} model_stop;

		struct cn10k_ml_jd_section_model_run {
			/* Address of the input for the run relative to ML_MLR_BASE */
			uint64_t input_ddr_addr;

			/* Address of the output for the run relative to ML_MLR_BASE */
			uint64_t output_ddr_addr;

			/* Number of batches to run in variable batch processing */
			uint16_t num_batches;

			uint8_t rsvd[78];
		} model_run;
	};
} __plt_aligned(ROC_ALIGN);

/* CN10K specific request */
struct cn10k_ml_req {
	/* Job descriptor */
	struct cn10k_ml_jd jd;

	/* Job descriptor extra arguments */
	union cn10k_ml_jd_extended_args extended_args;

	/* Status field for poll mode requests */
	volatile uint64_t status;

	/* Job command */
	struct ml_job_cmd_s jcmd;

	/* Result */
	struct cn10k_ml_result result;
};

/* Device ops */
int cn10k_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info);
int cn10k_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf);
int cn10k_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev);
int cn10k_ml_dev_start(struct cnxk_ml_dev *cnxk_mldev);
int cn10k_ml_dev_stop(struct cnxk_ml_dev *cnxk_mldev);
int cn10k_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp);
int cn10k_ml_dev_selftest(struct cnxk_ml_dev *cnxk_mldev);

/* Slow-path ops */
int cn10k_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
			struct cnxk_ml_model *model);
int cn10k_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int cn10k_ml_model_start(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int cn10k_ml_model_stop(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int cn10k_ml_model_params_update(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
				 void *buffer);

/* Fast-path ops */
__rte_hot bool cn10k_ml_enqueue_single(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_op *op,
				       uint16_t layer_id, struct cnxk_ml_qp *qp, uint64_t head);
__rte_hot int cn10k_ml_op_error_get(struct rte_ml_dev *dev, struct rte_ml_op *op,
				    struct rte_ml_op_error *error);
__rte_hot int cn10k_ml_inference_sync(void *device, uint16_t index, void *input, void *output,
				      uint16_t nb_batches);
__rte_hot void cn10k_ml_result_update(struct cnxk_ml_dev *cnxk_mldev, int qp_id, void *request);
__rte_hot void cn10k_ml_set_error_code(struct cnxk_ml_req *req, uint64_t etype, uint64_t stype);
__rte_hot void cn10k_ml_set_poll_addr(struct cnxk_ml_req *req);

/* Misc ops */
void cn10k_ml_qp_initialize(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_qp *qp);

/* Layer ops */
int cn10k_ml_layer_load(void *device, uint16_t model_id, const char *layer_name, uint8_t *buffer,
			size_t size, uint16_t *index);
int cn10k_ml_layer_unload(void *device, uint16_t model_id, const char *layer_name);
int cn10k_ml_layer_start(void *device, uint16_t model_id, const char *layer_name);
int cn10k_ml_layer_stop(void *device, uint16_t model_id, const char *layer_name);
int cn10k_ml_io_alloc(void *device, uint16_t model_id, const char *layer_name,
		      uint64_t **input_qbuffer, uint64_t **output_qbuffer);
int cn10k_ml_io_free(void *device, uint16_t model_id, const char *layer_name);

int cn10k_ml_malloc(const char *name, size_t size, uint32_t align, void **addr);
int cn10k_ml_free(const char *name);

/* xstats ops */
void cn10k_ml_xstat_model_name_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
				   uint16_t stat_id, uint16_t entry, char *suffix);
uint64_t cn10k_ml_model_xstat_get(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer,
				  enum cnxk_ml_xstats_type type);

#endif /* _CN10K_ML_OPS_H_ */
