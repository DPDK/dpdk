/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef TEST_DEVICE_OPS_H
#define TEST_DEVICE_OPS_H

#include <rte_common.h>

#include "test_common.h"

struct test_device {
	/* common data */
	struct test_common cmn;
} __rte_cache_aligned;

#endif /* TEST_DEVICE_OPS_H */
