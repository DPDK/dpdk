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

static struct test_command power_cmd = {
	.command = "power_autotest",
	.callback = test_power,
};
REGISTER_TEST_COMMAND(power_cmd);
