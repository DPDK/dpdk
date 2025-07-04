/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_prefetch.h>

#include "test.h"

/*
 * Prefetch test
 * =============
 *
 * - Just test that the macro can be called and validate the compilation.
 *   The test always return success.
 */

static int
test_prefetch(void)
{
	int a = 0;

	rte_prefetch0(&a);
	rte_prefetch1(&a);
	rte_prefetch2(&a);

	rte_prefetch0_write(&a);
	rte_prefetch1_write(&a);
	rte_prefetch2_write(&a);

	rte_cldemote(&a);

	return 0;
}

REGISTER_FAST_TEST(prefetch_autotest, true, true, test_prefetch);
