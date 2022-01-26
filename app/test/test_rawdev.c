/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#include "test.h"

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_dev.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_rawdev_selftests(void)
{
	printf("rawdev not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}
#else

#include <rte_rawdev.h>
#include <rte_bus_vdev.h>

static int
test_rawdev_selftest_impl(const char *pmd, const char *opts)
{
	int ret;

	printf("\n### Test rawdev infrastructure using skeleton driver\n");
	rte_vdev_init(pmd, opts);
	ret = rte_rawdev_selftest(rte_rawdev_get_dev_id(pmd));
	rte_vdev_uninit(pmd);
	return ret;
}

static int
test_rawdev_selftest_skeleton(void)
{
	return test_rawdev_selftest_impl("rawdev_skeleton", "");
}

static int
test_rawdev_selftests(void)
{
	const int count = rte_rawdev_count();
	int ret = 0;
	int i;

	/* basic sanity on rawdev infrastructure */
	if (test_rawdev_selftest_skeleton() < 0)
		return -1;

	/* now run self-test on all rawdevs */
	if (count > 0)
		printf("\n### Run selftest on each available rawdev\n");
	for (i = 0; i < count; i++) {
		int result = rte_rawdev_selftest(i);
		printf("Rawdev %u (%s) selftest: %s\n", i,
				rte_rawdevs[i].name,
				result == 0 ? "Passed" : "Failed");
		ret |=  result;
	}

	return ret;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(rawdev_autotest, test_rawdev_selftests);
