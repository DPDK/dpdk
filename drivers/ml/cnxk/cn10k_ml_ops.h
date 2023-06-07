/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_OPS_H_
#define _CN10K_ML_OPS_H_

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <roc_api.h>

#include "cn10k_ml_dev.h"

/* Request structure */
struct cn10k_ml_req {
	/* Job descriptor */
	struct cn10k_ml_jd jd;

	/* Job descriptor extra arguments */
	union cn10k_ml_jd_extended_args extended_args;

	/* Job result */
	struct cn10k_ml_result result;

	/* Status field for poll mode requests */
	volatile uint64_t status;

	/* Job command */
	struct ml_job_cmd_s jcmd;

	/* Job completion W1 */
	uint64_t compl_W1;

	/* Timeout cycle */
	uint64_t timeout;

	/* Op */
	struct rte_ml_op *op;
} __rte_aligned(ROC_ALIGN);

/* Request queue */
struct cn10k_ml_queue {
	/* Array of requests */
	struct cn10k_ml_req *reqs;

	/* Head of the queue, used for enqueue */
	uint64_t head;

	/* Tail of the queue, used for dequeue */
	uint64_t tail;

	/* Wait cycles before timeout */
	uint64_t wait_cycles;
};

/* Queue-pair structure */
struct cn10k_ml_qp {
	/* ID */
	uint32_t id;

	/* Number of descriptors */
	uint64_t nb_desc;

	/* Request queue */
	struct cn10k_ml_queue queue;

	/* Statistics per queue-pair */
	struct rte_ml_dev_stats stats;

	/* Register block start for polling */
	uint32_t block_start;

	/* Register block end for polling */
	uint32_t block_size;
};

/* Device ops */
extern struct rte_ml_dev_ops cn10k_ml_ops;

/* Slow-path ops */
int cn10k_ml_model_load(struct rte_ml_dev *dev, struct rte_ml_model_params *params,
			uint16_t *model_id);
int cn10k_ml_model_unload(struct rte_ml_dev *dev, uint16_t model_id);
int cn10k_ml_model_start(struct rte_ml_dev *dev, uint16_t model_id);
int cn10k_ml_model_stop(struct rte_ml_dev *dev, uint16_t model_id);

/* Fast-path ops */
__rte_hot uint16_t cn10k_ml_enqueue_burst(struct rte_ml_dev *dev, uint16_t qp_id,
					  struct rte_ml_op **ops, uint16_t nb_ops);
__rte_hot uint16_t cn10k_ml_dequeue_burst(struct rte_ml_dev *dev, uint16_t qp_id,
					  struct rte_ml_op **ops, uint16_t nb_ops);
__rte_hot int cn10k_ml_op_error_get(struct rte_ml_dev *dev, struct rte_ml_op *op,
				    struct rte_ml_op_error *error);
__rte_hot int cn10k_ml_inference_sync(struct rte_ml_dev *dev, struct rte_ml_op *op);

#endif /* _CN10K_ML_OPS_H_ */
