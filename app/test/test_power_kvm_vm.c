/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "test.h"

#include <rte_power.h>
#include <rte_config.h>

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
				"Power management environment\n", TEST_POWER_VM_LCORE_ID);
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

static struct test_command power_kvm_vm_cmd = {
	.command = "power_kvm_vm_autotest",
	.callback = test_power_kvm_vm,
};
REGISTER_TEST_COMMAND(power_kvm_vm_cmd);
