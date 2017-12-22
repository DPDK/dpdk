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

#ifdef RTE_LIBRTE_CMDLINE
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
			{ "test_invalid_n_flag", no_action },
			{ "test_no_hpet_flag", no_action },
			{ "test_whitelist_flag", no_action },
			{ "test_invalid_b_flag", no_action },
			{ "test_invalid_vdev_flag", no_action },
			{ "test_invalid_r_flag", no_action },
			{ "test_misc_flags", no_action },
			{ "test_memory_flags", no_action },
			{ "test_file_prefix", no_action },
			{ "test_no_huge_flag", no_action },
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

int last_test_result;

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

	char *dpdk_test = getenv("DPDK_TEST");
	if (dpdk_test && strlen(dpdk_test)) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "%s\n", dpdk_test);
		if (cmdline_in(cl, buf, strlen(buf)) < 0) {
			printf("error on cmdline input\n");
			return -1;
		}

		cmdline_stdin_exit(cl);
		return last_test_result;
	}
	/* if no DPDK_TEST env variable, go interactive */
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
#endif

	return 0;
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

	if (suite->setup)
		if (suite->setup() != 0) {
			/*
			 * setup failed, so count all enabled tests and mark
			 * them as failed
			 */
			while (suite->unit_test_cases[total].testcase) {
				if (!suite->unit_test_cases[total].enabled)
					skipped++;
				else
					failed++;
				total++;
			}
			goto suite_summary;
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
		return -1;

	return 0;
}
