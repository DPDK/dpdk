/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <rte_pmu.h>

#include "test.h"

static int
test_pmu_read(void)
{
	const char *name = NULL;
	int tries = 10, event;
	uint64_t val = 0;

	if (name == NULL) {
		printf("PMU not supported on this arch\n");
		return TEST_SKIPPED;
	}

	if (rte_pmu_init() < 0)
		return TEST_FAILED;

	event = rte_pmu_add_event(name);
	while (tries--)
		val += rte_pmu_read(event);

	rte_pmu_fini();

	return val ? TEST_SUCCESS : TEST_FAILED;
}

static struct unit_test_suite pmu_tests = {
	.suite_name = "PMU autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_pmu_read),
		TEST_CASES_END()
	}
};

static int
test_pmu(void)
{
	return unit_test_suite_runner(&pmu_tests);
}

REGISTER_FAST_TEST(pmu_autotest, true, true, test_pmu);
