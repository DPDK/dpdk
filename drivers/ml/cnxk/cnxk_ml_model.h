/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_MODEL_H_
#define _CNXK_ML_MODEL_H_

#include <rte_mldev.h>

#include <roc_api.h>

#include "cn10k_ml_model.h"

#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
#include "mvtvm_ml_model.h"
#else
#include "mvtvm_ml_stubs.h"
#endif

#include "cnxk_ml_io.h"

struct cnxk_ml_dev;
struct cnxk_ml_model;
struct cnxk_ml_qp;
struct cnxk_ml_req;

/* Model type */
enum cnxk_ml_model_type {
	/* Unknown model type */
	ML_CNXK_MODEL_TYPE_UNKNOWN,

	/* Invalid model type */
	ML_CNXK_MODEL_TYPE_INVALID,

	/* Glow compiled model, for MLIP target */
	ML_CNXK_MODEL_TYPE_GLOW,

	/* TVM compiled model, for ARM64 / ARM64 + MLIP target */
	ML_CNXK_MODEL_TYPE_TVM,
};

/* Model subtype */
enum cnxk_ml_model_subtype {
	/* Marvell Glow model */
	ML_CNXK_MODEL_SUBTYPE_GLOW_MRVL,

	/* TVM model with single MRVL region */
	ML_CNXK_MODEL_SUBTYPE_TVM_MRVL,

	/* TVM model with LLVM regions only */
	ML_CNXK_MODEL_SUBTYPE_TVM_LLVM,

	/* TVM hybrid model, with both MRVL and LLVM regions or (> 1) MRVL regions*/
	ML_CNXK_MODEL_SUBTYPE_TVM_HYBRID,
};

/* Layer type */
enum cnxk_ml_layer_type {
	/* MRVL layer, for MLIP target*/
	ML_CNXK_LAYER_TYPE_UNKNOWN = 0,

	/* MRVL layer, for MLIP target*/
	ML_CNXK_LAYER_TYPE_MRVL,

	/* LLVM layer, for ARM64 target*/
	ML_CNXK_LAYER_TYPE_LLVM,
};

/* Model state */
enum cnxk_ml_model_state {
	/* Unknown state */
	ML_CNXK_MODEL_STATE_UNKNOWN,

	/* Model loaded */
	ML_CNXK_MODEL_STATE_LOADED,

	/* A slow-path job is active, start or stop */
	ML_CNXK_MODEL_STATE_JOB_ACTIVE,

	/* Model started */
	ML_CNXK_MODEL_STATE_STARTED,
};

/* Layer state */
enum cnxk_ml_layer_state {
	/* Unknown state */
	ML_CNXK_LAYER_STATE_UNKNOWN,

	/* Layer loaded */
	ML_CNXK_LAYER_STATE_LOADED,

	/* A slow-path job is active, start or stop */
	ML_CNXK_LAYER_STATE_JOB_ACTIVE,

	/* Layer started */
	ML_CNXK_LAYER_STATE_STARTED,
};

/* Layer object */
struct cnxk_ml_layer {
	/* Name*/
	char name[RTE_ML_STR_MAX];

	/* Type */
	enum cnxk_ml_layer_type type;

	/* Model handle */
	struct cnxk_ml_model *model;

	/* Index mapped with firmware's model_id */
	uint16_t index;

	/* Input / Output */
	struct cnxk_ml_io_info info;

	/* Batch size */
	uint32_t batch_size;

	/* State */
	enum cnxk_ml_layer_state state;

	/* Glow layer specific data */
	struct cn10k_ml_layer_data glow;
};

typedef bool (*enqueue_single_t)(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_op *op,
				 uint16_t layer_id, struct cnxk_ml_qp *qp, uint64_t head);
typedef void (*result_update_t)(struct cnxk_ml_dev *cnxk_mldev, int qp_id, void *request);
typedef void (*set_error_code_t)(struct cnxk_ml_req *req, uint64_t etype, uint64_t stype);
typedef void (*set_poll_addr_t)(struct cnxk_ml_req *req);

/* Model Object */
struct cnxk_ml_model {
	/* Device reference */
	struct cnxk_ml_dev *cnxk_mldev;

	/* Type */
	enum cnxk_ml_model_type type;

	/* Model subtype */
	enum cnxk_ml_model_subtype subtype;

	/* ID */
	uint16_t model_id;

	/* Name */
	char name[RTE_ML_STR_MAX];

	union {
		/* Model specific data - glow */
		struct cn10k_ml_model_data glow;

#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
		/* Model type specific data - mvtvm */
		struct mvtvm_ml_model_data mvtvm;
#endif
	};

	/* Batch size */
	uint32_t batch_size;

	/* Number of layers */
	uint16_t nb_layers;

	/* Layer info */
	struct cnxk_ml_layer layer[ML_CNXK_MODEL_MAX_LAYERS];

	/* State */
	enum cnxk_ml_model_state state;

	/* Internal model information structure
	 * Size of the buffer = sizeof(struct rte_ml_model_info)
	 *                    + num_inputs * sizeof(struct rte_ml_io_info)
	 *                    + num_outputs * sizeof(struct rte_ml_io_info).
	 * Structures would be arranged in the same order in the buffer.
	 */
	uint8_t *info;

	/* Spinlock, used to update model state */
	plt_spinlock_t lock;

	/* Fast-path functions */
	enqueue_single_t enqueue_single;
	result_update_t result_update;
	set_error_code_t set_error_code;
	set_poll_addr_t set_poll_addr;
};

enum cnxk_ml_model_type cnxk_ml_model_get_type(struct rte_ml_model_params *params);
void cnxk_ml_model_dump(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model, FILE *fp);

#endif /* _CNXK_ML_MODEL_H_ */
