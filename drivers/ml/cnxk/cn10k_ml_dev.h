/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_DEV_H_
#define _CN10K_ML_DEV_H_

#include <roc_api.h>

#include "cn10k_ml_ocm.h"

/* Marvell OCTEON CN10K ML PMD device name */
#define MLDEV_NAME_CN10K_PMD ml_cn10k

/* Firmware version string length */
#define MLDEV_FIRMWARE_VERSION_LENGTH 32

/* Device alignment size */
#define ML_CN10K_ALIGN_SIZE 128

/* Maximum number of models per device */
#define ML_CN10K_MAX_MODELS 16

/* Maximum number of queue-pairs per device, spinlock version */
#define ML_CN10K_MAX_QP_PER_DEVICE_SL 16

/* Maximum number of queue-pairs per device, lock-free version */
#define ML_CN10K_MAX_QP_PER_DEVICE_LF 1

/* Maximum number of descriptors per queue-pair */
#define ML_CN10K_MAX_DESC_PER_QP 1024

/* Maximum number of segments for IO data */
#define ML_CN10K_MAX_SEGMENTS 1

/* ML command timeout in seconds */
#define ML_CN10K_CMD_TIMEOUT 5

/* ML slow-path job flags */
#define ML_CN10K_SP_FLAGS_OCM_NONRELOCATABLE BIT(0)

/* Poll mode job state */
#define ML_CN10K_POLL_JOB_START	 0
#define ML_CN10K_POLL_JOB_FINISH 1

/* Memory barrier macros */
#if defined(RTE_ARCH_ARM)
#define dmb_st ({ asm volatile("dmb st" : : : "memory"); })
#define dsb_st ({ asm volatile("dsb st" : : : "memory"); })
#else
#define dmb_st
#define dsb_st
#endif

struct cn10k_ml_req;
struct cn10k_ml_qp;

/* Job types */
enum cn10k_ml_job_type {
	ML_CN10K_JOB_TYPE_MODEL_RUN = 0,
	ML_CN10K_JOB_TYPE_MODEL_STOP,
	ML_CN10K_JOB_TYPE_MODEL_START,
	ML_CN10K_JOB_TYPE_FIRMWARE_LOAD,
	ML_CN10K_JOB_TYPE_FIRMWARE_SELFTEST,
};

/* Device configuration state enum */
enum cn10k_ml_dev_state {
	/* Probed and not configured */
	ML_CN10K_DEV_STATE_PROBED = 0,

	/* Configured */
	ML_CN10K_DEV_STATE_CONFIGURED,

	/* Started */
	ML_CN10K_DEV_STATE_STARTED,

	/* Closed */
	ML_CN10K_DEV_STATE_CLOSED
};

/* Error types enumeration */
enum cn10k_ml_error_etype {
	/* 0x0 */ ML_ETYPE_NO_ERROR = 0, /* No error */
	/* 0x1 */ ML_ETYPE_FW_NONFATAL,	 /* Firmware non-fatal error */
	/* 0x2 */ ML_ETYPE_HW_NONFATAL,	 /* Hardware non-fatal error */
	/* 0x3 */ ML_ETYPE_HW_FATAL,	 /* Hardware fatal error */
	/* 0x4 */ ML_ETYPE_HW_WARNING,	 /* Hardware warning */
	/* 0x5 */ ML_ETYPE_DRIVER,	 /* Driver specific error */
	/* 0x6 */ ML_ETYPE_UNKNOWN,	 /* Unknown error */
};

/* Firmware non-fatal error sub-type */
enum cn10k_ml_error_stype_fw_nf {
	/* 0x0 */ ML_FW_ERR_NOERR = 0,		 /* No error */
	/* 0x1 */ ML_FW_ERR_UNLOAD_ID_NOT_FOUND, /* Model ID not found during load */
	/* 0x2 */ ML_FW_ERR_LOAD_LUT_OVERFLOW,	 /* Lookup table overflow at load */
	/* 0x3 */ ML_FW_ERR_ID_IN_USE,		 /* Model ID already in use */
	/* 0x4 */ ML_FW_ERR_INVALID_TILEMASK,	 /* Invalid OCM tilemask */
	/* 0x5 */ ML_FW_ERR_RUN_LUT_OVERFLOW,	 /* Lookup table overflow at run */
	/* 0x6 */ ML_FW_ERR_RUN_ID_NOT_FOUND,	 /* Model ID not found during run */
	/* 0x7 */ ML_FW_ERR_COMMAND_NOTSUP,	 /* Unsupported command */
	/* 0x8 */ ML_FW_ERR_DDR_ADDR_RANGE,	 /* DDR address out of range */
	/* 0x9 */ ML_FW_ERR_NUM_BATCHES_INVALID, /* Invalid number of batches */
	/* 0xA */ ML_FW_ERR_INSSYNC_TIMEOUT,	 /* INS sync timeout */
};

/* Driver error sub-type */
enum cn10k_ml_error_stype_driver {
	/* 0x0 */ ML_DRIVER_ERR_NOERR = 0, /* No error */
	/* 0x1 */ ML_DRIVER_ERR_UNKNOWN,   /* Unable to determine error sub-type */
	/* 0x2 */ ML_DRIVER_ERR_EXCEPTION, /* Firmware exception */
	/* 0x3 */ ML_DRIVER_ERR_FW_ERROR,  /* Unknown firmware error */
};

/* Error structure */
union cn10k_ml_error_code {
	struct {
		/* Error type */
		uint64_t etype : 4;

		/* Error sub-type */
		uint64_t stype : 60;
	} s;

	/* WORD 0 */
	uint64_t u64;
};

/* Firmware stats */
struct cn10k_ml_fw_stats {
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
	union cn10k_ml_error_code error_code;

	/* Firmware stats */
	struct cn10k_ml_fw_stats stats;

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
			/* Source model start address in DDR relative to ML_MLR_BASE */
			uint64_t model_src_ddr_addr;

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
};

/* ML firmware structure */
struct cn10k_ml_fw {
	/* Device reference */
	struct cn10k_ml_dev *mldev;

	/* Firmware file path */
	const char *path;

	/* Enable DPE warnings */
	int enable_dpe_warnings;

	/* Report DPE warnings */
	int report_dpe_warnings;

	/* Memory to be used for polling in fast-path requests */
	const char *poll_mem;

	/* Data buffer */
	uint8_t *data;

	/* Firmware load / handshake request structure */
	struct cn10k_ml_req *req;
};

/* Device private data */
struct cn10k_ml_dev {
	/* Device ROC */
	struct roc_ml roc;

	/* Configuration state */
	enum cn10k_ml_dev_state state;

	/* Firmware */
	struct cn10k_ml_fw fw;

	/* OCM info */
	struct cn10k_ml_ocm ocm;

	/* Number of models loaded */
	uint16_t nb_models_loaded;

	/* xstats status */
	bool xstats_enabled;

	/* Enable / disable model data caching */
	int cache_model_data;

	/* Use spinlock version of ROC enqueue */
	int hw_queue_lock;

	/* OCM page size */
	int ocm_page_size;

	/* JCMD enqueue function handler */
	bool (*ml_jcmdq_enqueue)(struct roc_ml *roc_ml, struct ml_job_cmd_s *job_cmd);

	/* Poll handling function pointers */
	void (*set_poll_addr)(struct cn10k_ml_qp *qp, struct cn10k_ml_req *req, uint64_t idx);
	void (*set_poll_ptr)(struct roc_ml *roc_ml, struct cn10k_ml_req *req);
	uint64_t (*get_poll_ptr)(struct roc_ml *roc_ml, struct cn10k_ml_req *req);

	/* Memory barrier function pointers to handle synchronization */
	void (*set_enq_barrier)(void);
	void (*set_deq_barrier)(void);
};

uint64_t cn10k_ml_fw_flags_get(struct cn10k_ml_fw *fw);
int cn10k_ml_fw_load(struct cn10k_ml_dev *mldev);
void cn10k_ml_fw_unload(struct cn10k_ml_dev *mldev);

#endif /* _CN10K_ML_DEV_H_ */
