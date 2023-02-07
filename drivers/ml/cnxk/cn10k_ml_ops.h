/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_OPS_H_
#define _CN10K_ML_OPS_H_

#include <rte_mldev.h>

#include <roc_api.h>

#include "cn10k_ml_dev.h"

/* Request structure */
struct cn10k_ml_req {
	/* Job descriptor */
	struct cn10k_ml_jd jd;

	/* Job result */
	struct cn10k_ml_result result;

	/* Status field for poll mode requests */
	volatile uint64_t status;
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
};

/* Device ops */
extern struct rte_ml_dev_ops cn10k_ml_ops;

/* Slow-path ops */
int cn10k_ml_model_load(struct rte_ml_dev *dev, struct rte_ml_model_params *params,
			uint16_t *model_id);
int cn10k_ml_model_unload(struct rte_ml_dev *dev, uint16_t model_id);

#endif /* _CN10K_ML_OPS_H_ */
