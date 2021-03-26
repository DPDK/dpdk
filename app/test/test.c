/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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

#ifdef RTE_LIB_CMDLINE
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>
extern cmdline_parse_ctx_t main_ctx[];
#endif

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#ifdef RTE_LIB_TIMER
#include <rte_timer.h>
#endif

#include "test.h"
#ifdef RTE_LIB_PDUMP
#include "test_pdump.h"
#endif

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

const char *prgname; /* to be set to argv[0] */

static const char *recursive_call; /* used in linux for MP and other tests */

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
#ifdef RTE_LIB_PDUMP
#ifdef RTE_NET_RING
			{ "run_pdump_server_tests", test_pdump },
#endif
#endif
			{ "test_missing_c_flag", no_action },
			{ "test_main_lcore_flag", no_action },
			{ "test_invalid_n_flag", no_action },
			{ "test_no_hpet_flag", no_action },
			{ "test_allow_flag", no_action },
			{ "test_invalid_b_flag", no_action },
			{ "test_invalid_vdev_flag", no_action },
			{ "test_invalid_r_flag", no_action },
			{ "test_misc_flags", no_action },
			{ "test_memory_flags", no_action },
			{ "test_file_prefix", no_action },
			{ "test_no_huge_flag", no_action },
#ifdef RTE_LIB_TIMER
			{ "timer_secondary_spawn_wait", test_timer_secondary },
#endif
	};

	if (recursive_call == NULL)
		return -1;
	for (i = 0; i < RTE_DIM(actions); i++) {
		if (strcmp(actions[i].env_var, recursive_call) == 0)
			return (actions[i].action_fn)();
	}
	printf("ERROR - missing action to take for %s\n", recursive_call);
	return -1;
}

int last_test_result;

#define MAX_EXTRA_ARGS 32

int
main(int argc, char **argv)
{
#ifdef RTE_LIB_CMDLINE
	struct cmdline *cl;
#endif
	char *extra_args;
	int ret;

	extra_args = getenv("DPDK_TEST_PARAMS");
	if (extra_args != NULL && strlen(extra_args) > 0) {
		char **all_argv;
		char *eargv[MAX_EXTRA_ARGS];
		int all_argc;
		int eargc;
		int i;

		RTE_LOG(INFO, APP, "Using additional DPDK_TEST_PARAMS: '%s'\n",
				extra_args);
		eargc = rte_strsplit(extra_args, strlen(extra_args),
				eargv, MAX_EXTRA_ARGS, ' ');

		/* merge argc/argv and the environment args */
		all_argc = argc + eargc;
		all_argv = malloc(sizeof(*all_argv) * (all_argc + 1));
		if (all_argv == NULL) {
			ret = -1;
			goto out;
		}

		for (i = 0; i < argc; i++)
			all_argv[i] = argv[i];
		for (i = 0; i < eargc; i++)
			all_argv[argc + i] = eargv[i];
		all_argv[all_argc] = NULL;

		/* call eal_init with combined args */
		ret = rte_eal_init(all_argc, all_argv);
		free(all_argv);
	} else
		ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		ret = -1;
		goto out;
	}

	argv += ret;

	prgname = argv[0];

#ifdef RTE_LIB_TIMER
	ret = rte_timer_subsystem_init();
	if (ret < 0 && ret != -EALREADY) {
		ret = -1;
		goto out;
	}
#endif

	if (commands_init() < 0) {
		ret = -1;
		goto out;
	}

	recursive_call = getenv(RECURSIVE_ENV_VAR);
	if (recursive_call != NULL) {
		ret = do_recursive_call();
		goto out;
	}

#ifdef RTE_LIBEAL_USE_HPET
	if (rte_eal_hpet_init(1) < 0)
#endif
		RTE_LOG(INFO, APP,
				"HPET is not enabled, using TSC as default timer\n");


#ifdef RTE_LIB_CMDLINE
	char *dpdk_test = getenv("DPDK_TEST");
	if (dpdk_test && strlen(dpdk_test)) {
		char buf[1024];

		cl = cmdline_new(main_ctx, "RTE>>", 0, 1);
		if (cl == NULL) {
			ret = -1;
			goto out;
		}

		snprintf(buf, sizeof(buf), "%s\n", dpdk_test);
		if (cmdline_in(cl, buf, strlen(buf)) < 0) {
			printf("error on cmdline input\n");

			ret = -1;
		} else {
			ret = last_test_result;
		}
		cmdline_free(cl);
		goto out;
	} else {
		/* if no DPDK_TEST env variable, go interactive */
		cl = cmdline_stdin_new(main_ctx, "RTE>>");
		if (cl == NULL) {
			ret = -1;
			goto out;
		}

		cmdline_interact(cl);
		cmdline_stdin_exit(cl);
		cmdline_free(cl);
	}
#endif
	ret = 0;

out:
#ifdef RTE_LIB_TIMER
	rte_timer_subsystem_finalize();
#endif
	rte_eal_cleanup();
	return ret;
}


int
unit_test_suite_runner(struct unit_test_suite *suite)
{
	int test_success;
	unsigned int total = 0, executed = 0, skipped = 0;
	unsigned int succeeded = 0, failed = 0, unsupported = 0;
	const char *status;

	if (suite->suite_name) {
		printf(" + ------------------------------------------------------- +\n");
		printf(" + Test Suite : %s\n", suite->suite_name);
	}

	if (suite->setup) {
		test_success = suite->setup();
		if (test_success != 0) {
			/*
			 * setup did not pass, so count all enabled tests and
			 * mark them as failed/skipped
			 */
			while (suite->unit_test_cases[total].testcase) {
				if (!suite->unit_test_cases[total].enabled ||
				    test_success == TEST_SKIPPED)
					skipped++;
				else
					failed++;
				total++;
			}
			goto suite_summary;
		}
	}

	printf(" + ------------------------------------------------------- +\n");

	while (suite->unit_test_cases[total].testcase) {
		if (!suite->unit_test_cases[total].enabled) {
			skipped++;
			total++;
			continue;
		} else {
			executed++;
		}

		/* run test case setup */
		if (suite->unit_test_cases[total].setup)
			test_success = suite->unit_test_cases[total].setup();
		else
			test_success = TEST_SUCCESS;

		if (test_success == TEST_SUCCESS) {
			/* run the test case */
			test_success = suite->unit_test_cases[total].testcase();
			if (test_success == TEST_SUCCESS)
				succeeded++;
			else if (test_success == TEST_SKIPPED)
				skipped++;
			else if (test_success == -ENOTSUP)
				unsupported++;
			else
				failed++;
		} else if (test_success == -ENOTSUP) {
			unsupported++;
		} else {
			failed++;
		}

		/* run the test case teardown */
		if (suite->unit_test_cases[total].teardown)
			suite->unit_test_cases[total].teardown();

		if (test_success == TEST_SUCCESS)
			status = "succeeded";
		else if (test_success == TEST_SKIPPED)
			status = "skipped";
		else if (test_success == -ENOTSUP)
			status = "unsupported";
		else
			status = "failed";

		printf(" + TestCase [%2d] : %s %s\n", total,
				suite->unit_test_cases[total].name, status);

		total++;
	}

	/* Run test suite teardown */
	if (suite->teardown)
		suite->teardown();

	goto suite_summary;

suite_summary:
	printf(" + ------------------------------------------------------- +\n");
	printf(" + Test Suite Summary \n");
	printf(" + Tests Total :       %2d\n", total);
	printf(" + Tests Skipped :     %2d\n", skipped);
	printf(" + Tests Executed :    %2d\n", executed);
	printf(" + Tests Unsupported:  %2d\n", unsupported);
	printf(" + Tests Passed :      %2d\n", succeeded);
	printf(" + Tests Failed :      %2d\n", failed);
	printf(" + ------------------------------------------------------- +\n");

	last_test_result = failed;

	if (failed)
		return TEST_FAILED;
	if (total == skipped)
		return TEST_SKIPPED;
	return TEST_SUCCESS;
}
