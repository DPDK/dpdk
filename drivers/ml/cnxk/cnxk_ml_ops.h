/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_OPS_H_
#define _CNXK_ML_OPS_H_

#include <rte_mldev.h>
#include <rte_mldev_core.h>

#include <roc_api.h>

#include "cn10k_ml_ops.h"

/* Request structure */
struct cnxk_ml_req {
	/* Device specific request */
	union {
		/* CN10K */
		struct cn10k_ml_req cn10k_req;
	};

	/* Address of status field */
	volatile uint64_t *status;

	/* Timeout cycle */
	uint64_t timeout;

	/* Op */
	struct rte_ml_op *op;
} __rte_aligned(ROC_ALIGN);

/* Request queue */
struct cnxk_ml_queue {
	/* Array of requests */
	struct cnxk_ml_req *reqs;

	/* Head of the queue, used for enqueue */
	uint64_t head;

	/* Tail of the queue, used for dequeue */
	uint64_t tail;

	/* Wait cycles before timeout */
	uint64_t wait_cycles;
};

/* Queue-pair structure */
struct cnxk_ml_qp {
	/* ID */
	uint32_t id;

	/* Number of descriptors */
	uint64_t nb_desc;

	/* Request queue */
	struct cnxk_ml_queue queue;

	/* Statistics per queue-pair */
	struct rte_ml_dev_stats stats;
};

extern struct rte_ml_dev_ops cnxk_ml_ops;

/* Temporarily set cnxk driver functions as non-static */
int cnxk_ml_dev_info_get(struct rte_ml_dev *dev, struct rte_ml_dev_info *dev_info);

#endif /* _CNXK_ML_OPS_H_ */
