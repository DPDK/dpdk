/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>

#include "test.h"

#ifndef RTE_LIB_POWER

static int
test_power(void)
{
	printf("Power management library not supported, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_power_cpufreq.h>

static int
test_power(void)
{
	int ret = -1;
	enum power_management_env env;

	/* Test setting an invalid environment */
	ret = rte_power_set_env(PM_ENV_NOT_SET);
	if (ret == 0) {
		printf("Unexpectedly succeeded on setting an invalid environment\n");
		return -1;
	}

	/* Test that the environment has not been set */
	env = rte_power_get_env();
	if (env != PM_ENV_NOT_SET) {
		printf("Unexpectedly got a valid environment configuration\n");
		return -1;
	}

	rte_power_unset_env();

	/* Perform tests for valid environments.*/
	const enum power_management_env envs[] = {PM_ENV_ACPI_CPUFREQ,
			PM_ENV_KVM_VM,
			PM_ENV_PSTATE_CPUFREQ,
			PM_ENV_AMD_PSTATE_CPUFREQ,
			PM_ENV_CPPC_CPUFREQ};

	unsigned int i;
	for (i = 0; i < RTE_DIM(envs); ++i) {

		/* Test setting a valid environment */
		ret = rte_power_set_env(envs[i]);
		if (ret != 0) {
			printf("Unexpectedly unsuccessful on setting a valid environment\n");
			return -1;
		}

		/* Test that the environment has been set */
		env = rte_power_get_env();
		if (env != envs[i]) {
			printf("Not expected environment configuration\n");
			return -1;
		}

		rte_power_unset_env();

	}

	return 0;
}
#endif

REGISTER_FAST_TEST(power_autotest, true, true, test_power);
