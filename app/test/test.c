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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <sys/queue.h>

#ifdef RTE_LIBRTE_CMDLINE
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>
extern cmdline_parse_ctx_t main_ctx[];
#endif

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#ifdef RTE_LIBRTE_TIMER
#include <rte_timer.h>
#endif

#include "test.h"

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

const char *prgname; /* to be set to argv[0] */

static const char *recursive_call; /* used in linuxapp for MP and other tests */

static int
no_action(void){ return 0; }

static int
do_recursive_call(void)
{
	unsigned i;
	struct {
		const char *env_var;
		int (*action_fn)(void);
	} actions[] =  {
			{ "run_secondary_instances", test_mp_secondary },
			{ "test_missing_c_flag", no_action },
			{ "test_master_lcore_flag", no_action },
			{ "test_missing_n_flag", no_action },
			{ "test_no_hpet_flag", no_action },
			{ "test_whitelist_flag", no_action },
			{ "test_invalid_b_flag", no_action },
			{ "test_invalid_vdev_flag", no_action },
			{ "test_invalid_r_flag", no_action },
#ifdef RTE_LIBRTE_XEN_DOM0
			{ "test_dom0_misc_flags", no_action },
#else
			{ "test_misc_flags", no_action },
#endif
			{ "test_memory_flags", no_action },
			{ "test_file_prefix", no_action },
			{ "test_no_huge_flag", no_action },
#ifdef RTE_LIBRTE_IVSHMEM
			{ "test_ivshmem", test_ivshmem },
#endif
	};

	if (recursive_call == NULL)
		return -1;
	for (i = 0; i < sizeof(actions)/sizeof(actions[0]); i++) {
		if (strcmp(actions[i].env_var, recursive_call) == 0)
			return (actions[i].action_fn)();
	}
	printf("ERROR - missing action to take for %s\n", recursive_call);
	return -1;
}

int
main(int argc, char **argv)
{
#ifdef RTE_LIBRTE_CMDLINE
	struct cmdline *cl;
#endif
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;

#ifdef RTE_LIBRTE_TIMER
	rte_timer_subsystem_init();
#endif

	if (commands_init() < 0)
		return -1;

	argv += ret;

	prgname = argv[0];

	if ((recursive_call = getenv(RECURSIVE_ENV_VAR)) != NULL)
		return do_recursive_call();

#ifdef RTE_LIBEAL_USE_HPET
	if (rte_eal_hpet_init(1) < 0)
#endif
		RTE_LOG(INFO, APP,
				"HPET is not enabled, using TSC as default timer\n");


#ifdef RTE_LIBRTE_CMDLINE
	cl = cmdline_stdin_new(main_ctx, "RTE>>");
	if (cl == NULL) {
		return -1;
	}
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
#endif

	return 0;
}


int
unit_test_suite_runner(struct unit_test_suite *suite)
{
	int retval, i = 0;

	if (suite->suite_name)
		printf("Test Suite : %s\n", suite->suite_name);

	if (suite->setup)
		if (suite->setup() != 0)
			return -1;

	while (suite->unit_test_cases[i].testcase) {
		/* Run test case setup */
		if (suite->unit_test_cases[i].setup) {
			retval = suite->unit_test_cases[i].setup();
			if (retval != 0)
				return retval;
		}

		/* Run test case */
		if (suite->unit_test_cases[i].testcase() == 0) {
			printf("TestCase %2d: %s\n", i,
					suite->unit_test_cases[i].success_msg ?
					suite->unit_test_cases[i].success_msg :
					"passed");
		}
		else {
			printf("TestCase %2d: %s\n", i, suite->unit_test_cases[i].fail_msg ?
					suite->unit_test_cases[i].fail_msg :
					"failed");
			return -1;
		}

		/* Run test case teardown */
		if (suite->unit_test_cases[i].teardown) {
			retval = suite->unit_test_cases[i].teardown();
			if (retval != 0)
				return retval;
		}

		i++;
	}

	/* Run test suite teardown */
	if (suite->teardown)
		if (suite->teardown() != 0)
			return -1;

	return 0;
}
