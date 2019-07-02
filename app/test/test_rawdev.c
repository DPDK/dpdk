/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_dev.h>
#include <rte_rawdev.h>
#include <rte_bus_vdev.h>

#include "test.h"

static int
test_rawdev_selftest_impl(const char *pmd, const char *opts)
{
	rte_vdev_init(pmd, opts);
	return rte_rawdev_selftest(rte_rawdev_get_dev_id(pmd));
}

static int
test_rawdev_selftest_skeleton(void)
{
	return test_rawdev_selftest_impl("rawdev_skeleton", "");
}

REGISTER_TEST_COMMAND(rawdev_autotest, test_rawdev_selftest_skeleton);

static int
test_rawdev_selftest_ioat(void)
{
	const int count = rte_rawdev_count();
	int i;

	for (i = 0; i < count; i++) {
		struct rte_rawdev_info info = { .dev_private = NULL };
		if (rte_rawdev_info_get(i, &info) == 0 &&
				strstr(info.driver_name, "ioat") != NULL)
			return rte_rawdev_selftest(i) == 0 ?
					TEST_SUCCESS : TEST_FAILED;
	}

	printf("No IOAT rawdev found, skipping tests\n");
	return TEST_SKIPPED;
}

REGISTER_TEST_COMMAND(ioat_rawdev_autotest, test_rawdev_selftest_ioat);
