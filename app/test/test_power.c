/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "test.h"

#ifndef RTE_LIBRTE_POWER

static int
test_power(void)
{
	printf("Power management library not supported, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_power.h>

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

	/* verify that function pointers are NULL */
	if (rte_power_freqs != NULL) {
		printf("rte_power_freqs should be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	if (rte_power_get_freq != NULL) {
		printf("rte_power_get_freq should be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	if (rte_power_set_freq != NULL) {
		printf("rte_power_set_freq should be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_up != NULL) {
		printf("rte_power_freq_up should be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_down != NULL) {
		printf("rte_power_freq_down should be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_max != NULL) {
		printf("rte_power_freq_max should be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_min != NULL) {
		printf("rte_power_freq_min should be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	rte_power_unset_env();
	return 0;
fail_all:
	rte_power_unset_env();
	return -1;
}
#endif

REGISTER_TEST_COMMAND(power_autotest, test_power);
