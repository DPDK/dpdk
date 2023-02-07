/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef _CN10K_ML_OPS_H_
#define _CN10K_ML_OPS_H_

#include "cn10k_ml_dev.h"

/* ML request */
struct cn10k_ml_req {
	/* Job descriptor */
	struct cn10k_ml_jd jd;

	/* Job result */
	struct cn10k_ml_result result;

	/* Status field for poll mode requests */
	volatile uint64_t status;
} __rte_aligned(ROC_ALIGN);

/* Device ops */
extern struct rte_ml_dev_ops cn10k_ml_ops;

#endif /* _CN10K_ML_OPS_H_ */
