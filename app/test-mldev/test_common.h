/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <rte_mldev.h>

#include "ml_common.h"
#include "ml_test.h"

struct test_common {
	struct ml_options *opt;
	enum ml_test_result result;
	struct rte_ml_dev_info dev_info;
};

bool ml_test_cap_check(struct ml_options *opt);
int ml_test_opt_check(struct ml_options *opt);
void ml_test_opt_dump(struct ml_options *opt);
int ml_test_device_configure(struct ml_test *test, struct ml_options *opt);
int ml_test_device_close(struct ml_test *test, struct ml_options *opt);
int ml_test_device_start(struct ml_test *test, struct ml_options *opt);
int ml_test_device_stop(struct ml_test *test, struct ml_options *opt);

#endif /* TEST_COMMON_H */
