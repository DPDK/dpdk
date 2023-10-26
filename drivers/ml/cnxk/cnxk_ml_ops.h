/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_OPS_H_
#define _CNXK_ML_OPS_H_

#include <rte_mldev.h>
#include <rte_mldev_core.h>

#include <roc_api.h>

#include "cn10k_ml_ops.h"

#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
#include "mvtvm_ml_ops.h"
#else
#include "mvtvm_ml_stubs.h"
#endif

/* Request structure */
struct cnxk_ml_req {
	/* Device specific request */
	union {
		/* CN10K */
		struct cn10k_ml_req cn10k_req;

#ifdef RTE_MLDEV_CNXK_ENABLE_MVTVM
		/* MVTVM */
		struct mvtvm_ml_req mvtvm_req;
#endif
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

int cnxk_ml_model_unload(struct rte_ml_dev *dev, uint16_t model_id);
int cnxk_ml_model_stop(struct rte_ml_dev *dev, uint16_t model_id);
void cnxk_ml_xstats_model_name_update(struct cnxk_ml_dev *cnxk_mldev, uint16_t model_id);

__rte_hot uint16_t cnxk_ml_enqueue_burst(struct rte_ml_dev *dev, uint16_t qp_id,
					 struct rte_ml_op **ops, uint16_t nb_ops);
__rte_hot uint16_t cnxk_ml_dequeue_burst(struct rte_ml_dev *dev, uint16_t qp_id,
					 struct rte_ml_op **ops, uint16_t nb_ops);
__rte_hot void cnxk_ml_set_poll_ptr(struct cnxk_ml_req *req);
__rte_hot uint64_t cnxk_ml_get_poll_ptr(struct cnxk_ml_req *req);

#endif /* _CNXK_ML_OPS_H_ */
