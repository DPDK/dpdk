/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _MVTVM_ML_OPS_H_
#define _MVTVM_ML_OPS_H_

#include <dlpack/dlpack.h>

#include <tvmdp.h>

#include <rte_mldev.h>

#include "cnxk_ml_xstats.h"

struct cnxk_ml_dev;
struct cnxk_ml_model;
struct cnxk_ml_layer;
struct cnxk_ml_qp;
struct cnxk_ml_req;

/* Inference stats */
struct mvtvm_ml_stats {
	/* Start ns */
	uint64_t start_ns;

	/* Start ns */
	uint64_t end_ns;
};

/* Result structure */
struct mvtvm_ml_result {
	/* Job error code */
	uint64_t error_code;

	/* Inference stats */
	struct mvtvm_ml_stats stats;

	/* User context pointer */
	void *user_ptr;
};

/* MVTVM specific request */
struct mvtvm_ml_req {
	/* Input tensors */
	DLTensor input_tensor[ML_CNXK_MODEL_MAX_INPUT_OUTPUT];

	/* Output tensors */
	DLTensor output_tensor[ML_CNXK_MODEL_MAX_INPUT_OUTPUT];

	/* Status field for poll mode requests */
	volatile uint64_t status;

	/* Result */
	struct mvtvm_ml_result result;
};

int mvtvm_ml_dev_info_get(struct cnxk_ml_dev *mldev, struct rte_ml_dev_info *dev_info);
int mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf);
int mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev);
int mvtvm_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp);
int mvtvm_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
			struct cnxk_ml_model *model);
int mvtvm_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int mvtvm_ml_model_start(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int mvtvm_ml_model_stop(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
int mvtvm_ml_io_quantize(void *device, uint16_t model_id, const char *layer_name,
			 const DLTensor **deq_tensor, void *qbuffer);
int mvtvm_ml_io_dequantize(void *device, uint16_t model_id, const char *layer_name, void *qbuffer,
			   const DLTensor **deq_tensor);

__rte_hot bool mvtvm_ml_enqueue_single(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_op *op,
				       uint16_t layer_id, struct cnxk_ml_qp *qp, uint64_t head);
__rte_hot void mvtvm_ml_result_update(struct cnxk_ml_dev *cnxk_mldev, int qp_id, void *request);
__rte_hot void mvtvm_ml_set_error_code(struct cnxk_ml_req *req, uint64_t etype, uint64_t stype);

void mvtvm_ml_model_xstat_name_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
				   uint16_t stat_id, uint16_t entry, char *suffix);
uint64_t mvtvm_ml_model_xstat_get(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model,
				  enum cnxk_ml_xstats_type type);

#endif /* _MVTVM_ML_OPS_H_ */
