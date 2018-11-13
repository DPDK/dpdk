/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>

#include "test.h"

#ifndef RTE_LIBRTE_POWER

static int
test_power_acpi_cpufreq(void)
{
	printf("Power management library not supported, skipping test\n");
	return TEST_SKIPPED;
}

static int
test_power_acpi_caps(void)
{
	printf("Power management library not supported, skipping test\n");
	return TEST_SKIPPED;
}

#else
#include <rte_power.h>

#define TEST_POWER_LCORE_ID      2U
#define TEST_POWER_LCORE_INVALID ((unsigned)RTE_MAX_LCORE)
#define TEST_POWER_FREQS_NUM_MAX ((unsigned)RTE_MAX_LCORE_FREQS)

#define TEST_POWER_SYSFILE_CUR_FREQ \
	"/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_cur_freq"

static uint32_t total_freq_num;
static uint32_t freqs[TEST_POWER_FREQS_NUM_MAX];

static int
check_cur_freq(unsigned lcore_id, uint32_t idx)
{
#define TEST_POWER_CONVERT_TO_DECIMAL 10
	FILE *f;
	char fullpath[PATH_MAX];
	char buf[BUFSIZ];
	uint32_t cur_freq;
	int ret = -1;

	if (snprintf(fullpath, sizeof(fullpath),
		TEST_POWER_SYSFILE_CUR_FREQ, lcore_id) < 0) {
		return 0;
	}
	f = fopen(fullpath, "r");
	if (f == NULL) {
		return 0;
	}
	if (fgets(buf, sizeof(buf), f) == NULL) {
		goto fail_get_cur_freq;
	}
	cur_freq = strtoul(buf, NULL, TEST_POWER_CONVERT_TO_DECIMAL);
	ret = (freqs[idx] == cur_freq ? 0 : -1);

fail_get_cur_freq:
	fclose(f);

	return ret;
}

/* Check rte_power_freqs() */
static int
check_power_freqs(void)
{
	uint32_t ret;

	total_freq_num = 0;
	memset(freqs, 0, sizeof(freqs));

	/* test with an invalid lcore id */
	ret = rte_power_freqs(TEST_POWER_LCORE_INVALID, freqs,
					TEST_POWER_FREQS_NUM_MAX);
	if (ret > 0) {
		printf("Unexpectedly get available freqs successfully on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* test with NULL buffer to save available freqs */
	ret = rte_power_freqs(TEST_POWER_LCORE_ID, NULL,
				TEST_POWER_FREQS_NUM_MAX);
	if (ret > 0) {
		printf("Unexpectedly get available freqs successfully with "
			"NULL buffer on lcore %u\n", TEST_POWER_LCORE_ID);
		return -1;
	}

	/* test of getting zero number of freqs */
	ret = rte_power_freqs(TEST_POWER_LCORE_ID, freqs, 0);
	if (ret > 0) {
		printf("Unexpectedly get available freqs successfully with "
			"zero buffer size on lcore %u\n", TEST_POWER_LCORE_ID);
		return -1;
	}

	/* test with all valid input parameters */
	ret = rte_power_freqs(TEST_POWER_LCORE_ID, freqs,
				TEST_POWER_FREQS_NUM_MAX);
	if (ret == 0 || ret > TEST_POWER_FREQS_NUM_MAX) {
		printf("Fail to get available freqs on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Save the total number of available freqs */
	total_freq_num = ret;

	return 0;
}

/* Check rte_power_get_freq() */
static int
check_power_get_freq(void)
{
	int ret;
	uint32_t count;

	/* test with an invalid lcore id */
	count = rte_power_get_freq(TEST_POWER_LCORE_INVALID);
	if (count < TEST_POWER_FREQS_NUM_MAX) {
		printf("Unexpectedly get freq index successfully on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	count = rte_power_get_freq(TEST_POWER_LCORE_ID);
	if (count >= TEST_POWER_FREQS_NUM_MAX) {
		printf("Fail to get the freq index on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, count);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_set_freq() */
static int
check_power_set_freq(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_set_freq(TEST_POWER_LCORE_INVALID, 0);
	if (ret >= 0) {
		printf("Unexpectedly set freq index successfully on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* test with an invalid freq index */
	ret = rte_power_set_freq(TEST_POWER_LCORE_ID,
				TEST_POWER_FREQS_NUM_MAX);
	if (ret >= 0) {
		printf("Unexpectedly set an invalid freq index (%u)"
			"successfully on lcore %u\n", TEST_POWER_FREQS_NUM_MAX,
							TEST_POWER_LCORE_ID);
		return -1;
	}

	/**
	 * test with an invalid freq index which is right one bigger than
	 * total number of freqs
	 */
	ret = rte_power_set_freq(TEST_POWER_LCORE_ID, total_freq_num);
	if (ret >= 0) {
		printf("Unexpectedly set an invalid freq index (%u)"
			"successfully on lcore %u\n", total_freq_num,
						TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_set_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0) {
		printf("Fail to set freq index on lcore %u\n",
					TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_down() */
static int
check_power_freq_down(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_down(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale down successfully the freq on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* Scale down to min and then scale down one step */
	ret = rte_power_freq_min(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq to min on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_down(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0)
		return -1;

	/* Scale up to max and then scale down one step */
	ret = rte_power_freq_max(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq to max on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_down(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, 1);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_up() */
static int
check_power_freq_up(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_up(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale up successfully the freq on %u\n",
						TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* Scale down to min and then scale up one step */
	ret = rte_power_freq_min(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq to min on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_up(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 2);
	if (ret < 0)
		return -1;

	/* Scale up to max and then scale up one step */
	ret = rte_power_freq_max(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq to max on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_up(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, 0);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_max() */
static int
check_power_freq_max(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_max(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale up successfully the freq to max on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}
	ret = rte_power_freq_max(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq to max on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, 0);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_min() */
static int
check_power_freq_min(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_min(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale down successfully the freq to min "
				"on lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}
	ret = rte_power_freq_min(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq to min on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0)
		return -1;

	return 0;
}

static int
test_power_acpi_cpufreq(void)
{
	int ret = -1;
	enum power_management_env env;

	ret = rte_power_set_env(PM_ENV_ACPI_CPUFREQ);
	if (ret != 0) {
		printf("Failed on setting environment to PM_ENV_ACPI_CPUFREQ, this "
				"may occur if environment is not configured correctly or "
				" operating in another valid Power management environment\n");
		return -1;
	}

	/* Test environment configuration */
	env = rte_power_get_env();
	if (env != PM_ENV_ACPI_CPUFREQ) {
		printf("Unexpectedly got an environment other than ACPI cpufreq\n");
		goto fail_all;
	}

	/* verify that function pointers are not NULL */
	if (rte_power_freqs == NULL) {
		printf("rte_power_freqs should not be NULL, environment has not been "
				"initialised\n");
		goto fail_all;
	}
	if (rte_power_get_freq == NULL) {
		printf("rte_power_get_freq should not be NULL, environment has not "
				"been initialised\n");
		goto fail_all;
	}
	if (rte_power_set_freq == NULL) {
		printf("rte_power_set_freq should not be NULL, environment has not "
				"been initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_up == NULL) {
		printf("rte_power_freq_up should not be NULL, environment has not "
				"been initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_down == NULL) {
		printf("rte_power_freq_down should not be NULL, environment has not "
				"been initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_max == NULL) {
		printf("rte_power_freq_max should not be NULL, environment has not "
				"been initialised\n");
		goto fail_all;
	}
	if (rte_power_freq_min == NULL) {
		printf("rte_power_freq_min should not be NULL, environment has not "
				"been initialised\n");
		goto fail_all;
	}

	/* test of init power management for an invalid lcore */
	ret = rte_power_init(TEST_POWER_LCORE_INVALID);
	if (ret == 0) {
		printf("Unexpectedly initialise power management successfully "
				"for lcore %u\n", TEST_POWER_LCORE_INVALID);
		rte_power_unset_env();
		return -1;
	}

	/* Test initialisation of a valid lcore */
	ret = rte_power_init(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Cannot initialise power management for lcore %u, this "
				"may occur if environment is not configured "
				"correctly(APCI cpufreq) or operating in another valid "
				"Power management environment\n", TEST_POWER_LCORE_ID);
		rte_power_unset_env();
		return TEST_SKIPPED;
	}

	/**
	 * test of initialising power management for the lcore which has
	 * been initialised
	 */
	ret = rte_power_init(TEST_POWER_LCORE_ID);
	if (ret == 0) {
		printf("Unexpectedly init successfully power twice on "
					"lcore %u\n", TEST_POWER_LCORE_ID);
		goto fail_all;
	}

	ret = check_power_freqs();
	if (ret < 0)
		goto fail_all;

	if (total_freq_num < 2) {
		rte_power_exit(TEST_POWER_LCORE_ID);
		printf("Frequency can not be changed due to CPU itself\n");
		rte_power_unset_env();
		return 0;
	}

	ret = check_power_get_freq();
	if (ret < 0)
		goto fail_all;

	ret = check_power_set_freq();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_down();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_up();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_max();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_min();
	if (ret < 0)
		goto fail_all;

	ret = rte_power_exit(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Cannot exit power management for lcore %u\n",
						TEST_POWER_LCORE_ID);
		rte_power_unset_env();
		return -1;
	}

	/**
	 * test of exiting power management for the lcore which has been exited
	 */
	ret = rte_power_exit(TEST_POWER_LCORE_ID);
	if (ret == 0) {
		printf("Unexpectedly exit successfully power management twice "
					"on lcore %u\n", TEST_POWER_LCORE_ID);
		rte_power_unset_env();
		return -1;
	}

	/* test of exit power management for an invalid lcore */
	ret = rte_power_exit(TEST_POWER_LCORE_INVALID);
	if (ret == 0) {
		printf("Unpectedly exit power management successfully for "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		rte_power_unset_env();
		return -1;
	}
	rte_power_unset_env();
	return 0;

fail_all:
	rte_power_exit(TEST_POWER_LCORE_ID);
	rte_power_unset_env();
	return -1;
}

static int
test_power_acpi_caps(void)
{
	struct rte_power_core_capabilities caps;
	int ret;

	ret = rte_power_set_env(PM_ENV_ACPI_CPUFREQ);
	if (ret) {
		printf("Error setting ACPI environment\n");
		return -1;
	}

	ret = rte_power_init(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Cannot initialise power management for lcore %u, this "
			"may occur if environment is not configured "
			"correctly(APCI cpufreq) or operating in another valid "
			"Power management environment\n", TEST_POWER_LCORE_ID);
		rte_power_unset_env();
		return -1;
	}

	ret = rte_power_get_capabilities(TEST_POWER_LCORE_ID, &caps);
	if (ret) {
		printf("ACPI: Error getting capabilities\n");
		return -1;
	}

	printf("ACPI: Capabilities %"PRIx64"\n", caps.capabilities);

	rte_power_unset_env();
	return 0;
}

#endif

REGISTER_TEST_COMMAND(power_acpi_cpufreq_autotest, test_power_acpi_cpufreq);
REGISTER_TEST_COMMAND(power_acpi_caps_autotest, test_power_acpi_caps);
