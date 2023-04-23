/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_malloc.h>
#include <rte_mldev.h>

#include "ml_test.h"
#include "test_inference_common.h"

int ml_stats_get(struct ml_test *test, struct ml_options *opt, enum rte_ml_dev_xstats_mode,
		 int32_t fid);
int ml_throughput_get(struct ml_test *test, struct ml_options *opt);
