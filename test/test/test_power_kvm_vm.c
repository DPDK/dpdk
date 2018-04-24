/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "test.h"

#ifndef RTE_LIBRTE_POWER

static int
test_power_kvm_vm(void)
{
	printf("Power management library not supported, skipping test\n");
	return TEST_SKIPPED;
}

#else
#include <rte_power.h>

#define TEST_POWER_VM_LCORE_ID            0U
#define TEST_POWER_VM_LCORE_OUT_OF_BOUNDS (RTE_MAX_LCORE+1)
#define TEST_POWER_VM_LCORE_INVALID       1U

static int
test_power_kvm_vm(void)
{
	int ret;
	enum power_management_env env;

	ret = rte_power_set_env(PM_ENV_KVM_VM);
	if (ret != 0) {
		printf("Failed on setting environment to PM_ENV_KVM_VM\n");
		return -1;
	}

	/* Test environment configuration */
	env = rte_power_get_env();
	if (env != PM_ENV_KVM_VM) {
		printf("Unexpectedly got a Power Management environment other than "
				"KVM VM\n");
		rte_power_unset_env();
		return -1;
	}

	/* verify that function pointers are not NULL */
	if (rte_power_freqs == NULL) {
		printf("rte_power_freqs should not be NULL, environment has not been "
				"initialised\n");
		return -1;
	}
	if (rte_power_get_freq == NULL) {
		printf("rte_power_get_freq should not be NULL, environment has not "
				"been initialised\n");
		return -1;
	}
	if (rte_power_set_freq == NULL) {
		printf("rte_power_set_freq should not be NULL, environment has not "
				"been initialised\n");
		return -1;
	}
	if (rte_power_freq_up == NULL) {
		printf("rte_power_freq_up should not be NULL, environment has not "
				"been initialised\n");
		return -1;
	}
	if (rte_power_freq_down == NULL) {
		printf("rte_power_freq_down should not be NULL, environment has not "
				"been initialised\n");
		return -1;
	}
	if (rte_power_freq_max == NULL) {
		printf("rte_power_freq_max should not be NULL, environment has not "
				"been initialised\n");
		return -1;
	}
	if (rte_power_freq_min == NULL) {
		printf("rte_power_freq_min should not be NULL, environment has not "
				"been initialised\n");
		return -1;
	}
	/* Test initialisation of an out of bounds lcore */
	ret = rte_power_init(TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
	if (ret != -1) {
		printf("rte_power_init unexpectedly succeeded on an invalid lcore %u\n",
				TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
		rte_power_unset_env();
		return -1;
	}

	/* Test initialisation of a valid lcore */
	ret = rte_power_init(TEST_POWER_VM_LCORE_ID);
	if (ret < 0) {
		printf("Cannot initialise power management for lcore %u, this "
				"may occur if environment is not configured "
				"correctly(KVM VM) or operating in another valid "
				"Power management environment\n",
				TEST_POWER_VM_LCORE_ID);
		rte_power_unset_env();
		return -1;
	}

	/* Test initialisation of previously initialised lcore */
	ret = rte_power_init(TEST_POWER_VM_LCORE_ID);
	if (ret == 0) {
		printf("rte_power_init unexpectedly succeeded on calling init twice on"
				" lcore %u\n", TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test frequency up of invalid lcore */
	ret = rte_power_freq_up(TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
	if (ret == 1) {
		printf("rte_power_freq_up unexpectedly succeeded on invalid lcore %u\n",
				TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
		goto fail_all;
	}

	/* Test frequency down of invalid lcore */
	ret = rte_power_freq_down(TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
	if (ret == 1) {
		printf("rte_power_freq_down unexpectedly succeeded on invalid lcore "
				"%u\n", TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
		goto fail_all;
	}

	/* Test frequency min of invalid lcore */
	ret = rte_power_freq_min(TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
	if (ret == 1) {
		printf("rte_power_freq_min unexpectedly succeeded on invalid lcore "
				"%u\n", TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
		goto fail_all;
	}

	/* Test frequency max of invalid lcore */
	ret = rte_power_freq_max(TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
	if (ret == 1) {
		printf("rte_power_freq_max unexpectedly succeeded on invalid lcore "
				"%u\n", TEST_POWER_VM_LCORE_OUT_OF_BOUNDS);
		goto fail_all;
	}

	/* Test frequency up of valid but uninitialised lcore */
	ret = rte_power_freq_up(TEST_POWER_VM_LCORE_INVALID);
	if (ret == 1) {
		printf("rte_power_freq_up unexpectedly succeeded on invalid lcore %u\n",
				TEST_POWER_VM_LCORE_INVALID);
		goto fail_all;
	}

	/* Test frequency down of valid but uninitialised lcore */
	ret = rte_power_freq_down(TEST_POWER_VM_LCORE_INVALID);
	if (ret == 1) {
		printf("rte_power_freq_down unexpectedly succeeded on invalid lcore "
				"%u\n", TEST_POWER_VM_LCORE_INVALID);
		goto fail_all;
	}

	/* Test frequency min of valid but uninitialised lcore */
	ret = rte_power_freq_min(TEST_POWER_VM_LCORE_INVALID);
	if (ret == 1) {
		printf("rte_power_freq_min unexpectedly succeeded on invalid lcore "
				"%u\n", TEST_POWER_VM_LCORE_INVALID);
		goto fail_all;
	}

	/* Test frequency max of valid but uninitialised lcore */
	ret = rte_power_freq_max(TEST_POWER_VM_LCORE_INVALID);
	if (ret == 1) {
		printf("rte_power_freq_max unexpectedly succeeded on invalid lcore "
				"%u\n", TEST_POWER_VM_LCORE_INVALID);
		goto fail_all;
	}

	/* Test KVM_VM Enable Turbo of valid core */
	ret = rte_power_freq_enable_turbo(TEST_POWER_VM_LCORE_ID);
	if (ret == -1) {
		printf("rte_power_freq_enable_turbo failed on valid lcore"
			"%u\n", TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test KVM_VM Disable Turbo of valid core */
	ret = rte_power_freq_disable_turbo(TEST_POWER_VM_LCORE_ID);
	if (ret == -1) {
		printf("rte_power_freq_disable_turbo failed on valid lcore"
		"%u\n", TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test frequency up of valid lcore */
	ret = rte_power_freq_up(TEST_POWER_VM_LCORE_ID);
	if (ret != 1) {
		printf("rte_power_freq_up unexpectedly failed on valid lcore %u\n",
				TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test frequency down of valid lcore */
	ret = rte_power_freq_down(TEST_POWER_VM_LCORE_ID);
	if (ret != 1) {
		printf("rte_power_freq_down unexpectedly failed on valid lcore "
				"%u\n", TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test frequency min of valid lcore */
	ret = rte_power_freq_min(TEST_POWER_VM_LCORE_ID);
	if (ret != 1) {
		printf("rte_power_freq_min unexpectedly failed on valid lcore "
				"%u\n", TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test frequency max of valid lcore */
	ret = rte_power_freq_max(TEST_POWER_VM_LCORE_ID);
	if (ret != 1) {
		printf("rte_power_freq_max unexpectedly failed on valid lcore "
				"%u\n", TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test unsupported rte_power_freqs */
	ret = rte_power_freqs(TEST_POWER_VM_LCORE_ID, NULL, 0);
	if (ret != -ENOTSUP) {
		printf("rte_power_freqs did not return the expected -ENOTSUP(%d) but "
				"returned %d\n", -ENOTSUP, ret);
		goto fail_all;
	}

	/* Test unsupported rte_power_get_freq */
	ret = rte_power_get_freq(TEST_POWER_VM_LCORE_ID);
	if (ret != -ENOTSUP) {
		printf("rte_power_get_freq did not return the expected -ENOTSUP(%d) but"
				" returned %d for lcore %u\n",
				-ENOTSUP, ret, TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test unsupported rte_power_set_freq */
	ret = rte_power_set_freq(TEST_POWER_VM_LCORE_ID, 0);
	if (ret != -ENOTSUP) {
		printf("rte_power_set_freq did not return the expected -ENOTSUP(%d) but"
				" returned %d for lcore %u\n",
				-ENOTSUP, ret, TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test removing of an lcore */
	ret = rte_power_exit(TEST_POWER_VM_LCORE_ID);
	if (ret != 0) {
		printf("rte_power_exit unexpectedly failed on valid lcore %u,"
				"please ensure that the environment has been configured "
				"correctly\n", TEST_POWER_VM_LCORE_ID);
		goto fail_all;
	}

	/* Test frequency up of previously removed lcore */
	ret = rte_power_freq_up(TEST_POWER_VM_LCORE_ID);
	if (ret == 0) {
		printf("rte_power_freq_up unexpectedly succeeded on a removed "
				"lcore %u\n", TEST_POWER_VM_LCORE_ID);
		return -1;
	}

	/* Test frequency down of previously removed lcore */
	ret = rte_power_freq_down(TEST_POWER_VM_LCORE_ID);
	if (ret == 0) {
		printf("rte_power_freq_down unexpectedly succeeded on a removed "
				"lcore %u\n", TEST_POWER_VM_LCORE_ID);
		return -1;
	}

	/* Test frequency min of previously removed lcore */
	ret = rte_power_freq_min(TEST_POWER_VM_LCORE_ID);
	if (ret == 0) {
		printf("rte_power_freq_min unexpectedly succeeded on a removed "
				"lcore %u\n", TEST_POWER_VM_LCORE_ID);
		return -1;
	}

	/* Test frequency max of previously removed lcore */
	ret = rte_power_freq_max(TEST_POWER_VM_LCORE_ID);
	if (ret == 0) {
		printf("rte_power_freq_max unexpectedly succeeded on a removed "
				"lcore %u\n", TEST_POWER_VM_LCORE_ID);
		return -1;
	}
	rte_power_unset_env();
	return 0;
fail_all:
	rte_power_exit(TEST_POWER_VM_LCORE_ID);
	rte_power_unset_env();
	return -1;
}
#endif

REGISTER_TEST_COMMAND(power_kvm_vm_autotest, test_power_kvm_vm);
