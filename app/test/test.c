/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
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

#define FOR_EACH_SUITE_TESTCASE(iter, suite, case)			\
	for (iter = 0, case = suite->unit_test_cases[0];		\
		suite->unit_test_cases[iter].testcase ||		\
		suite->unit_test_cases[iter].testcase_with_data;	\
		iter++, case = suite->unit_test_cases[iter])

#define FOR_EACH_SUITE_TESTSUITE(iter, suite, sub_ts)			\
	for (iter = 0, sub_ts = suite->unit_test_suites ?		\
		suite->unit_test_suites[0]:NULL; sub_ts &&		\
		suite->unit_test_suites[iter]->suite_name != NULL;	\
		iter++, sub_ts = suite->unit_test_suites[iter])

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
#ifndef RTE_EXEC_ENV_WINDOWS
			{ "run_secondary_instances", test_mp_secondary },
#endif
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
#ifndef RTE_EXEC_ENV_WINDOWS
			{ "timer_secondary_spawn_wait", test_timer_secondary },
#endif
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
	char *tests[argc]; /* store an array of tests to run */
	int test_count = 0;
	int i;
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
	argc -= ret;

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

	if (dpdk_test && strlen(dpdk_test) == 0)
		dpdk_test = NULL;

	if (dpdk_test && !command_valid(dpdk_test)) {
		RTE_LOG(WARNING, APP, "Invalid DPDK_TEST value '%s'\n", dpdk_test);
		dpdk_test = NULL;
	}

	if (dpdk_test)
		tests[test_count++] = dpdk_test;
	for (i = 1; i < argc; i++) {
		if (!command_valid(argv[i]))
			RTE_LOG(WARNING, APP, "Invalid test requested: '%s'\n", argv[i]);
		else
			tests[test_count++] = argv[i];
	}

	if (test_count > 0) {
		char buf[1024];

		cl = cmdline_new(main_ctx, "RTE>>", 0, 1);
		if (cl == NULL) {
			ret = -1;
			goto out;
		}

		for (i = 0; i < test_count; i++) {
			snprintf(buf, sizeof(buf), "%s\n", tests[i]);
			if (cmdline_in(cl, buf, strlen(buf)) < 0) {
				printf("error on cmdline input\n");

				ret = -1;
			} else
				ret = last_test_result;

			if (ret != 0)
				break;
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

static void
unit_test_suite_count_tcs_on_setup_fail(struct unit_test_suite *suite,
		int test_success, unsigned int *sub_ts_failed,
		unsigned int *sub_ts_skipped, unsigned int *sub_ts_total)
{
	struct unit_test_case tc;
	struct unit_test_suite *ts;
	int i;

	FOR_EACH_SUITE_TESTSUITE(i, suite, ts) {
		unit_test_suite_count_tcs_on_setup_fail(
			ts, test_success, sub_ts_failed,
			sub_ts_skipped, sub_ts_total);
		suite->total += ts->total;
		suite->failed += ts->failed;
		suite->skipped += ts->skipped;
		if (ts->failed)
			(*sub_ts_failed)++;
		else
			(*sub_ts_skipped)++;
		(*sub_ts_total)++;
	}
	FOR_EACH_SUITE_TESTCASE(i, suite, tc) {
		suite->total++;
		if (!tc.enabled || test_success == TEST_SKIPPED)
			suite->skipped++;
		else
			suite->failed++;
	}
}

static void
unit_test_suite_reset_counts(struct unit_test_suite *suite)
{
	struct unit_test_suite *ts;
	int i;

	FOR_EACH_SUITE_TESTSUITE(i, suite, ts)
		unit_test_suite_reset_counts(ts);
	suite->total = 0;
	suite->executed = 0;
	suite->succeeded = 0;
	suite->skipped = 0;
	suite->failed = 0;
	suite->unsupported = 0;
}

int
unit_test_suite_runner(struct unit_test_suite *suite)
{
	int test_success, i, ret;
	const char *status;
	struct unit_test_case tc;
	struct unit_test_suite *ts;
	unsigned int sub_ts_succeeded = 0, sub_ts_failed = 0;
	unsigned int sub_ts_skipped = 0, sub_ts_total = 0;

	unit_test_suite_reset_counts(suite);

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
			unit_test_suite_count_tcs_on_setup_fail(suite,
					test_success, &sub_ts_failed,
					&sub_ts_skipped, &sub_ts_total);
			goto suite_summary;
		}
	}

	printf(" + ------------------------------------------------------- +\n");

	FOR_EACH_SUITE_TESTCASE(suite->total, suite, tc) {
		if (!tc.enabled) {
			suite->skipped++;
			continue;
		} else {
			suite->executed++;
		}

		/* run test case setup */
		if (tc.setup)
			test_success = tc.setup();
		else
			test_success = TEST_SUCCESS;

		if (test_success == TEST_SUCCESS) {
			/* run the test case */
			if (tc.testcase)
				test_success = tc.testcase();
			else if (tc.testcase_with_data)
				test_success = tc.testcase_with_data(tc.data);
			else
				test_success = -ENOTSUP;

			if (test_success == TEST_SUCCESS)
				suite->succeeded++;
			else if (test_success == TEST_SKIPPED)
				suite->skipped++;
			else if (test_success == -ENOTSUP)
				suite->unsupported++;
			else
				suite->failed++;
		} else if (test_success == -ENOTSUP) {
			suite->unsupported++;
		} else {
			suite->failed++;
		}

		/* run the test case teardown */
		if (tc.teardown)
			tc.teardown();

		if (test_success == TEST_SUCCESS)
			status = "succeeded";
		else if (test_success == TEST_SKIPPED)
			status = "skipped";
		else if (test_success == -ENOTSUP)
			status = "unsupported";
		else
			status = "failed";

		printf(" + TestCase [%2d] : %s %s\n", suite->total,
				tc.name, status);
	}
	FOR_EACH_SUITE_TESTSUITE(i, suite, ts) {
		ret = unit_test_suite_runner(ts);
		if (ret == TEST_SUCCESS)
			sub_ts_succeeded++;
		else if (ret == TEST_SKIPPED)
			sub_ts_skipped++;
		else
			sub_ts_failed++;
		sub_ts_total++;

		suite->total += ts->total;
		suite->succeeded += ts->succeeded;
		suite->failed += ts->failed;
		suite->skipped += ts->skipped;
		suite->unsupported += ts->unsupported;
		suite->executed += ts->executed;
	}

	/* Run test suite teardown */
	if (suite->teardown)
		suite->teardown();

	goto suite_summary;

suite_summary:
	printf(" + ------------------------------------------------------- +\n");
	printf(" + Test Suite Summary : %s\n", suite->suite_name);
	printf(" + ------------------------------------------------------- +\n");

	FOR_EACH_SUITE_TESTSUITE(i, suite, ts)
		printf(" + %s : %d/%d passed, %d/%d skipped, "
			"%d/%d failed, %d/%d unsupported\n", ts->suite_name,
			ts->succeeded, ts->total, ts->skipped, ts->total,
			ts->failed, ts->total, ts->unsupported, ts->total);

	if (suite->unit_test_suites) {
		printf(" + ------------------------------------------------------- +\n");
		printf(" + Sub Testsuites Total :     %2d\n", sub_ts_total);
		printf(" + Sub Testsuites Skipped :   %2d\n", sub_ts_skipped);
		printf(" + Sub Testsuites Passed :    %2d\n", sub_ts_succeeded);
		printf(" + Sub Testsuites Failed :    %2d\n", sub_ts_failed);
		printf(" + ------------------------------------------------------- +\n");
	}

	printf(" + Tests Total :       %2d\n", suite->total);
	printf(" + Tests Skipped :     %2d\n", suite->skipped);
	printf(" + Tests Executed :    %2d\n", suite->executed);
	printf(" + Tests Unsupported:  %2d\n", suite->unsupported);
	printf(" + Tests Passed :      %2d\n", suite->succeeded);
	printf(" + Tests Failed :      %2d\n", suite->failed);
	printf(" + ------------------------------------------------------- +\n");

	last_test_result = suite->failed;

	if (suite->failed)
		return TEST_FAILED;
	if (suite->total == suite->skipped)
		return TEST_SKIPPED;
	return TEST_SUCCESS;
}
