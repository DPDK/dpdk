/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef TEST_MODEL_OPS_H
#define TEST_MODEL_OPS_H

#include <rte_common.h>

#include "test_model_common.h"

struct test_model_ops {
	/* common data */
	struct test_common cmn;

	/* test specific data */
	struct ml_model model[ML_TEST_MAX_MODELS];
} __rte_cache_aligned;

#endif /* TEST_MODEL_OPS_H */
