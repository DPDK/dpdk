/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 *  version: DPDK.L.1.2.3-3
 */
#include <stdio.h>

#include <cmdline_parse.h>

#include "test.h"

#ifndef RTE_EXEC_ENV_BAREMETAL
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#include <rte_debug.h>
#include <rte_string_fns.h>

#include "process.h"

#define mp_flag "--proc-type=secondary"
#define no_hpet "--no-hpet"
#define no_huge "--no-huge"
#define no_shconf "--no-shconf"
#define launch_proc(ARGV) process_dup(ARGV, \
		sizeof(ARGV)/(sizeof(ARGV[0])), __func__)

/*
 * Test that the app doesn't run without invalid blacklist option.
 * Final test ensures it does run with valid options as sanity check
 */
static int
test_invalid_b_flag(void)
{
	const char *blinval[][8] = {
		{prgname, mp_flag, "-n", "1", "-c", "1", "-b", "error"},
		{prgname, mp_flag, "-n", "1", "-c", "1", "-b", "0:0:0"},
		{prgname, mp_flag, "-n", "1", "-c", "1", "-b", "0:error:0.1"},
		{prgname, mp_flag, "-n", "1", "-c", "1", "-b", "0:0:0.1error"},
		{prgname, mp_flag, "-n", "1", "-c", "1", "-b", "error0:0:0.1"},
		{prgname, mp_flag, "-n", "1", "-c", "1", "-b", "0:0:0.1.2"},
	};
	/* Test with valid blacklist option */
	const char *blval[] = {prgname, mp_flag, "-n", "1", "-c", "1", "-b", "FF:09:0B.3"};

	int i;

	for (i = 0; i != sizeof (blinval) / sizeof (blinval[0]); i++) {
		if (launch_proc(blinval[i]) == 0) {
			printf("Error - process did run ok with invalid "
			    "blacklist parameter\n");
			return -1;
		}
	}
	if (launch_proc(blval) != 0) {
		printf("Error - process did not run ok with valid blacklist value\n");
		return -1;
	}
	return 0;
}


/*
 * Test that the app doesn't run with invalid -r option.
 */
static int
test_invalid_r_flag(void)
{
	const char *rinval[][8] = {
			{prgname, mp_flag, "-n", "1", "-c", "1", "-r", "error"},
			{prgname, mp_flag, "-n", "1", "-c", "1", "-r", "0"},
			{prgname, mp_flag, "-n", "1", "-c", "1", "-r", "-1"},
			{prgname, mp_flag, "-n", "1", "-c", "1", "-r", "17"},
	};
	/* Test with valid blacklist option */
	const char *rval[] = {prgname, mp_flag, "-n", "1", "-c", "1", "-r", "16"};

	int i;

	for (i = 0; i != sizeof (rinval) / sizeof (rinval[0]); i++) {
		if (launch_proc(rinval[i]) == 0) {
			printf("Error - process did run ok with invalid "
			    "-r (rank) parameter\n");
			return -1;
		}
	}
	if (launch_proc(rval) != 0) {
		printf("Error - process did not run ok with valid -r (rank) value\n");
		return -1;
	}
	return 0;
}

/*
 * Test that the app doesn't run without the coremask flag. In all cases
 * should give an error and fail to run
 */
static int
test_missing_c_flag(void)
{
	/* -c flag but no coremask value */
	const char *argv1[] = { prgname, mp_flag, "-n", "3", "-c"};
	/* No -c flag at all */
	const char *argv2[] = { prgname, mp_flag, "-n", "3"};
	/* bad coremask value */
	const char *argv3[] = { prgname, mp_flag, "-n", "3", "-c", "error" };
	/* sanity check of tests - valid coremask value */
	const char *argv4[] = { prgname, mp_flag, "-n", "3", "-c", "1" };

	if (launch_proc(argv1) == 0
			|| launch_proc(argv2) == 0
			|| launch_proc(argv3) == 0) {
		printf("Error - process ran without error when missing -c flag\n");
		return -1;
	}
	if (launch_proc(argv4) != 0) {
		printf("Error - process did not run ok with valid coremask value\n");
		return -1;
	}
	return 0;
}

/*
 * Test that the app doesn't run without the -n flag. In all cases
 * should give an error and fail to run.
 * Since -n is not compulsory for MP, we instead use --no-huge and --no-shconf
 * flags.
 */
static int
test_missing_n_flag(void)
{
	/* -n flag but no value */
	const char *argv1[] = { prgname, no_huge, no_shconf, "-c", "1", "-n"};
	/* No -n flag at all */
	const char *argv2[] = { prgname, no_huge, no_shconf, "-c", "1"};
	/* bad numeric value */
	const char *argv3[] = { prgname, no_huge, no_shconf, "-c", "1", "-n", "e" };
	/* out-of-range value */
	const char *argv4[] = { prgname, no_huge, no_shconf, "-c", "1", "-n", "9" };
	/* sanity test - check with good value */
	const char *argv5[] = { prgname, no_huge, no_shconf, "-c", "1", "-n", "2" };

	if (launch_proc(argv1) == 0
			|| launch_proc(argv2) == 0
			|| launch_proc(argv3) == 0
			|| launch_proc(argv4) == 0) {
		printf("Error - process ran without error when missing -n flag\n");
		return -1;
	}
	if (launch_proc(argv5) != 0) {
		printf("Error - process did not run ok with valid num-channel value\n");
		return -1;
	}
	return 0;
}

/*
 * Test that the app runs with HPET, and without HPET
 */
static int
test_no_hpet_flag(void)
{
	/* With --no-hpet */
	const char *argv1[] = {prgname, mp_flag, no_hpet, "-c", "1", "-n", "2"};
	/* Without --no-hpet */
	const char *argv2[] = {prgname, mp_flag, "-c", "1", "-n", "2"};

	if (launch_proc(argv1) != 0) {
		printf("Error - process did not run ok with --no-hpet flag\n");
		return -1;
	}
	if (launch_proc(argv2) != 0) {
		printf("Error - process did not run ok without --no-hpet flag\n");
		return -1;
	}
	return 0;
}

static int
test_misc_flags(void)
{
	/* check that some general flags don't prevent things from working.
	 * All cases, apart from the first, app should run.
	 * No futher testing of output done.
	 */
	/* sanity check - failure with invalid option */
	const char *argv0[] = {prgname, mp_flag, "-c", "1", "--invalid-opt"};

	/* With --no-pci */
	const char *argv1[] = {prgname, mp_flag, "-c", "1", "--no-pci"};
	/* With -v */
	const char *argv2[] = {prgname, mp_flag, "-c", "1", "-v"};
	/* With -m - ignored for secondary processes */
	const char *argv3[] = {prgname, mp_flag, "-c", "1", "-m", "32"};

	if (launch_proc(argv0) == 0) {
		printf("Error - process ran ok with invalid flag\n");
		return -1;
	}
	if (launch_proc(argv1) != 0) {
		printf("Error - process did not run ok with --no-pci flag\n");
		return -1;
	}
	if (launch_proc(argv2) != 0) {
		printf("Error - process did not run ok with -v flag\n");
		return -1;
	}
	if (launch_proc(argv3) != 0) {
		printf("Error - process did not run ok with -m flag\n");
		return -1;
	}
	return 0;
}

int
test_eal_flags(void)
{
	int ret = 0;

	ret = test_missing_c_flag();
	if (ret < 0) {
		printf("Error in test_missing_c_flag()");
		return ret;
	}

	ret = test_missing_n_flag();
	if (ret < 0) {
		printf("Error in test_missing_n_flag()");
		return ret;
	}

	ret = test_no_hpet_flag();
	if (ret < 0) {
		printf("Error in test_no_hpet_flag()");
		return ret;
	}

	ret = test_invalid_b_flag();
	if (ret < 0) {
		printf("Error in test_invalid_b_flag()");
		return ret;
	}

	ret = test_invalid_r_flag();
	if (ret < 0) {
		printf("Error in test_invalid_r_flag()");
		return ret;
	}

	ret = test_misc_flags();
	if (ret < 0) {
		printf("Error in test_misc_flags()");
		return ret;
	}

	return ret;
}

#else
/* Baremetal version
 * Multiprocess not applicable, so just return 0 always
 */
int
test_eal_flags(void)
{
	printf("Multi-process not possible for baremetal, cannot test EAL flags\n");
	return 0;
}

#endif
