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
#include <sys/wait.h>
#include <unistd.h>

#include <rte_debug.h>
#include <rte_common.h>
#include <rte_eal.h>

#include "test.h"

/*
 * Debug test
 * ==========
 */

/* use fork() to test rte_panic() */
static int
test_panic(void)
{
	int pid;
	int status;

	pid = fork();

	if (pid == 0)
		rte_panic("Test Debug\n");
	else if (pid < 0){
		printf("Fork Failed\n");
		return -1;
	}
	wait(&status);
	if(status == 0){
		printf("Child process terminated normally!\n");
		return -1;
	} else
		printf("Child process terminated as expected - Test passed!\n");

	return 0;
}

/* use fork() to test rte_exit() */
static int
test_exit_val(int exit_val)
{
	int pid;
	int status;

	pid = fork();

	if (pid == 0)
		rte_exit(exit_val, __func__);
	else if (pid < 0){
		printf("Fork Failed\n");
		return -1;
	}
	wait(&status);
	printf("Child process status: %d\n", status);
#ifndef RTE_EAL_ALWAYS_PANIC_ON_ERROR
	if(!WIFEXITED(status) || WEXITSTATUS(status) != (uint8_t)exit_val){
		printf("Child process terminated with incorrect status (expected = %d)!\n",
				exit_val);
		return -1;
	}
#endif
	return 0;
}

static int
test_exit(void)
{
	int test_vals[] = { 0, 1, 2, 255, -1 };
	unsigned i;
	for (i = 0; i < sizeof(test_vals) / sizeof(test_vals[0]); i++){
		if (test_exit_val(test_vals[i]) < 0)
			return -1;
	}
	printf("%s Passed\n", __func__);
	return 0;
}

static void
dummy_app_usage(const char *progname)
{
	RTE_SET_USED(progname);
}

static int
test_usage(void)
{
	if (rte_set_application_usage_hook(dummy_app_usage) != NULL) {
		printf("Non-NULL value returned for initial usage hook\n");
		return -1;
	}
	if (rte_set_application_usage_hook(NULL) != dummy_app_usage) {
		printf("Incorrect value returned for application usage hook\n");
		return -1;
	}
	return 0;
}

static int
test_debug(void)
{
	rte_dump_stack();
	rte_dump_registers();
	if (test_panic() < 0)
		return -1;
	if (test_exit() < 0)
		return -1;
	if (test_usage() < 0)
		return -1;
	return 0;
}

REGISTER_TEST_COMMAND(debug_autotest, test_debug);
