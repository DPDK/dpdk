/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_lcore.h>

#include "main.h"

/* Defines how many testcases can be specified as cmdline args */
#define MAX_CMDLINE_TESTCASES 8

static const char tc_sep = ',';

static struct test_params {
	struct test_command *test_to_run[MAX_CMDLINE_TESTCASES];
	unsigned int num_tests;
	unsigned int num_ops;
	unsigned int burst_sz;
	unsigned int num_lcores;
	char test_vector_filename[PATH_MAX];
} test_params;

static struct test_commands_list commands_list =
	TAILQ_HEAD_INITIALIZER(commands_list);

void
add_test_command(struct test_command *t)
{
	TAILQ_INSERT_TAIL(&commands_list, t, next);
}

int
unit_test_suite_runner(struct unit_test_suite *suite)
{
	int test_result = TEST_SUCCESS;
	unsigned int total = 0, skipped = 0, succeeded = 0, failed = 0;
	uint64_t start, end;

	printf(
			"\n + ------------------------------------------------------- +\n");
	printf(" + Starting Test Suite : %s\n", suite->suite_name);

	start = rte_rdtsc_precise();

	if (suite->setup) {
		test_result = suite->setup();
		if (test_result == TEST_FAILED) {
			printf(" + Test suite setup %s failed!\n",
					suite->suite_name);
			printf(
					" + ------------------------------------------------------- +\n");
			return 1;
		}
		if (test_result == TEST_SKIPPED) {
			printf(" + Test suite setup %s skipped!\n",
					suite->suite_name);
			printf(
					" + ------------------------------------------------------- +\n");
			return 0;
		}
	}

	while (suite->unit_test_cases[total].testcase) {
		if (suite->unit_test_cases[total].setup)
			test_result = suite->unit_test_cases[total].setup();

		if (test_result == TEST_SUCCESS)
			test_result = suite->unit_test_cases[total].testcase();

		if (suite->unit_test_cases[total].teardown)
			suite->unit_test_cases[total].teardown();

		if (test_result == TEST_SUCCESS) {
			succeeded++;
			printf(" + TestCase [%2d] : %s passed\n", total,
					suite->unit_test_cases[total].name);
		} else if (test_result == TEST_SKIPPED) {
			skipped++;
			printf(" + TestCase [%2d] : %s skipped\n", total,
					suite->unit_test_cases[total].name);
		} else {
			failed++;
			printf(" + TestCase [%2d] : %s failed\n", total,
					suite->unit_test_cases[total].name);
		}

		total++;
	}

	/* Run test suite teardown */
	if (suite->teardown)
		suite->teardown();

	end = rte_rdtsc_precise();

	printf(" + ------------------------------------------------------- +\n");
	printf(" + Test Suite Summary : %s\n", suite->suite_name);
	printf(" + Tests Total :       %2d\n", total);
	printf(" + Tests Skipped :     %2d\n", skipped);
	printf(" + Tests Passed :      %2d\n", succeeded);
	printf(" + Tests Failed :      %2d\n", failed);
	printf(" + Tests Lasted :       %lg ms\n",
			((end - start) * 1000) / (double)rte_get_tsc_hz());
	printf(" + ------------------------------------------------------- +\n");

	return (failed > 0) ? 1 : 0;
}

const char *
get_vector_filename(void)
{
	return test_params.test_vector_filename;
}

unsigned int
get_num_ops(void)
{
	return test_params.num_ops;
}

unsigned int
get_burst_sz(void)
{
	return test_params.burst_sz;
}

unsigned int
get_num_lcores(void)
{
	return test_params.num_lcores;
}

static void
print_usage(const char *prog_name)
{
	struct test_command *t;

	printf("Usage: %s [EAL params] [-- [-n/--num-ops NUM_OPS]\n"
			"\t[-b/--burst-size BURST_SIZE]\n"
			"\t[-v/--test-vector VECTOR_FILE]\n"
			"\t[-c/--test-cases TEST_CASE[,TEST_CASE,...]]]\n",
			prog_name);

	printf("Available testcases: ");
	TAILQ_FOREACH(t, &commands_list, next)
		printf("%s ", t->command);
	printf("\n");
}

static int
parse_args(int argc, char **argv, struct test_params *tp)
{
	int opt, option_index;
	unsigned int num_tests = 0;
	bool test_cases_present = false;
	bool test_vector_present = false;
	struct test_command *t;
	char *tokens[MAX_CMDLINE_TESTCASES];
	int tc, ret;

	static struct option lgopts[] = {
		{ "num-ops", 1, 0, 'n' },
		{ "burst-size", 1, 0, 'b' },
		{ "test-cases", 1, 0, 'c' },
		{ "test-vector", 1, 0, 'v' },
		{ "lcores", 1, 0, 'l' },
		{ "help", 0, 0, 'h' },
		{ NULL,  0, 0, 0 }
	};

	while ((opt = getopt_long(argc, argv, "hn:b:c:v:l:", lgopts,
			&option_index)) != EOF)
		switch (opt) {
		case 'n':
			TEST_ASSERT(strlen(optarg) > 0,
					"Num of operations is not provided");
			tp->num_ops = strtol(optarg, NULL, 10);
			break;
		case 'b':
			TEST_ASSERT(strlen(optarg) > 0,
					"Burst size is not provided");
			tp->burst_sz = strtol(optarg, NULL, 10);
			TEST_ASSERT(tp->burst_sz <= MAX_BURST,
					"Burst size mustn't be greater than %u",
					MAX_BURST);
			break;
		case 'c':
			TEST_ASSERT(test_cases_present == false,
					"Test cases provided more than once");
			test_cases_present = true;

			ret = rte_strsplit(optarg, strlen(optarg),
					tokens, MAX_CMDLINE_TESTCASES, tc_sep);

			TEST_ASSERT(ret <= MAX_CMDLINE_TESTCASES,
					"Too many test cases (max=%d)",
					MAX_CMDLINE_TESTCASES);

			for (tc = 0; tc < ret; ++tc) {
				/* Find matching test case */
				TAILQ_FOREACH(t, &commands_list, next)
					if (!strcmp(tokens[tc], t->command))
						tp->test_to_run[num_tests] = t;

				TEST_ASSERT(tp->test_to_run[num_tests] != NULL,
						"Unknown test case: %s",
						tokens[tc]);
				++num_tests;
			}
			break;
		case 'v':
			TEST_ASSERT(test_vector_present == false,
					"Test vector provided more than once");
			test_vector_present = true;

			TEST_ASSERT(strlen(optarg) > 0,
					"Config file name is null");

			snprintf(tp->test_vector_filename,
					sizeof(tp->test_vector_filename),
					"%s", optarg);
			break;
		case 'l':
			TEST_ASSERT(strlen(optarg) > 0,
					"Num of lcores is not provided");
			tp->num_lcores = strtol(optarg, NULL, 10);
			TEST_ASSERT(tp->num_lcores <= RTE_MAX_LCORE,
					"Num of lcores mustn't be greater than %u",
					RTE_MAX_LCORE);
			break;
		case 'h':
			print_usage(argv[0]);
			return 0;
		default:
			printf("ERROR: Unknown option: -%c\n", opt);
			return -1;
		}

	if (tp->num_ops == 0) {
		printf(
			"WARNING: Num of operations was not provided or was set 0. Set to default (%u)\n",
			DEFAULT_OPS);
		tp->num_ops = DEFAULT_OPS;
	}
	if (tp->burst_sz == 0) {
		printf(
			"WARNING: Burst size was not provided or was set 0. Set to default (%u)\n",
			DEFAULT_BURST);
		tp->burst_sz = DEFAULT_BURST;
	}
	if (tp->num_lcores == 0) {
		printf(
			"WARNING: Num of lcores was not provided or was set 0. Set to value from RTE config (%u)\n",
			rte_lcore_count());
		tp->num_lcores = rte_lcore_count();
	}

	TEST_ASSERT(tp->burst_sz <= tp->num_ops,
			"Burst size (%u) mustn't be greater than num ops (%u)",
			tp->burst_sz, tp->num_ops);

	tp->num_tests = num_tests;
	return 0;
}

static int
run_all_tests(void)
{
	int ret = TEST_SUCCESS;
	struct test_command *t;

	TAILQ_FOREACH(t, &commands_list, next)
		ret |= t->callback();

	return ret;
}

static int
run_parsed_tests(struct test_params *tp)
{
	int ret = TEST_SUCCESS;
	unsigned int i;

	for (i = 0; i < tp->num_tests; ++i)
		ret |= tp->test_to_run[i]->callback();

	return ret;
}

int
main(int argc, char **argv)
{
	int ret;

	/* Init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return 1;
	argc -= ret;
	argv += ret;

	/* Parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv, &test_params);
	if (ret < 0) {
		print_usage(argv[0]);
		return 1;
	}

	rte_log_set_global_level(RTE_LOG_INFO);

	/* If no argument provided - run all tests */
	if (test_params.num_tests == 0)
		return run_all_tests();
	else
		return run_parsed_tests(&test_params);
}
