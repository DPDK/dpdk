/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef TEST_MODEL_COMMON_H
#define TEST_MODEL_COMMON_H

#include <rte_mldev.h>

#include "test_common.h"

enum model_state {
	MODEL_INITIAL,
	MODEL_LOADED,
	MODEL_STARTED,
	MODEL_ERROR,
};

struct ml_model {
	uint16_t id;
	struct rte_ml_model_info info;
	enum model_state state;
};

int ml_model_load(struct ml_test *test, struct ml_options *opt, struct ml_model *model,
		  uint16_t fid);
int ml_model_unload(struct ml_test *test, struct ml_options *opt, struct ml_model *model,
		    uint16_t fid);
int ml_model_start(struct ml_test *test, struct ml_options *opt, struct ml_model *model,
		   uint16_t fid);
int ml_model_stop(struct ml_test *test, struct ml_options *opt, struct ml_model *model,
		  uint16_t fid);

#endif /* TEST_MODEL_COMMON_H */
