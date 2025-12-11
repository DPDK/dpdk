/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2014 6WIND S.A.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rte_common.h>
#include <rte_kvargs.h>

#include "test.h"

typedef int (*f_kvargs_process)(const struct rte_kvargs *kvlist,
				const char *key_match, arg_handler_t handler,
				void *opaque_arg);

static bool use_kvargs_process_opt[] = { false, true };

/* incremented in handler, to check it is properly called once per
 * key/value association */
static unsigned count;

/* this handler increment the "count" variable at each call and check
 * that the key is "check" and the value is "value%d" */
static int
check_handler(const char *key, const char *value, __rte_unused void *opaque)
{
	char buf[16];

	/* we check that the value is "check" */
	if (strcmp(key, "check"))
		return -1;

	/* we check that the value is "value$(count)" */
	snprintf(buf, sizeof(buf), "value%d", count);
	if (strncmp(buf, value, sizeof(buf)))
		return -1;

	count ++;
	return 0;
}

static int
check_only_handler(const char *key, const char *value, __rte_unused void *opaque)
{
	if (strcmp(key, "check"))
		return -1;

	if (value != NULL)
		return -1;

	return 0;
}

static int
test_basic_token_count(void)
{
	static const struct {
		unsigned int expected;
		const char *input;
	} valid_inputs[] = {
		{ 3, "foo=1,check=1,check=2" },
		{ 3, "foo=1,check,check=2"   },
		{ 2, "foo=1,foo="            },
		{ 2, "foo=1,foo="            },
		{ 2, "foo=1,foo"             },
		{ 2, "foo=1,=2"              },
		{ 2, "foo=1,,foo=2,,"        },
		{ 1, "foo=[1,2"              },
		{ 1, ",="                    },
		{ 1, "foo=["                 },
		{ 0, ""                      },
	};
	struct rte_kvargs *kvlist;
	unsigned int count;
	const char *args;
	unsigned int i;

	for (i = 0; i < RTE_DIM(valid_inputs); i++) {
		args = valid_inputs[i].input;
		kvlist = rte_kvargs_parse(args, NULL);
		if (kvlist == NULL) {
			printf("rte_kvargs_parse() error: %s\n", args);
			return -1;
		}
		count = rte_kvargs_count(kvlist, NULL);
		if (count != valid_inputs[i].expected) {
			printf("invalid count value %u (expected %u): %s\n",
			       count, valid_inputs[i].expected, args);
			rte_kvargs_free(kvlist);
			return -1;
		}
		rte_kvargs_free(kvlist);
	}

	return 0;
}

static int
test_parse_without_valid_keys(const void *params)
{
	const bool use_opt = *(const bool *)params;
	f_kvargs_process proc_func = use_opt ? rte_kvargs_process_opt : rte_kvargs_process;
	const char *proc_name = use_opt ? "rte_kvargs_process_opt" : "rte_kvargs_process";
	const char *args = "foo=1234,check=value0,check=value1";
	struct rte_kvargs *kvlist;

	kvlist = rte_kvargs_parse(args, NULL);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error\n");
		return -1;
	}

	/* call check_handler() for all entries with key="check" */
	count = 0;
	if (proc_func(kvlist, "check", check_handler, NULL) < 0) {
		printf("%s(check) error\n", proc_name);
		rte_kvargs_free(kvlist);
		return -1;
	}
	if (count != 2) {
		printf("invalid count value %u after %s(check)\n",
			count, proc_name);
		rte_kvargs_free(kvlist);
		return -1;
	}

	/* call check_handler() for all entries with key="nonexistent_key" */
	count = 0;
	if (proc_func(kvlist, "nonexistent_key", check_handler, NULL) < 0) {
		printf("%s(nonexistent_key) error\n", proc_name);
		rte_kvargs_free(kvlist);
		return -1;
	}
	if (count != 0) {
		printf("invalid count value %d after %s(nonexistent_key)\n",
			count, proc_name);
		rte_kvargs_free(kvlist);
		return -1;
	}

	/* count all entries with key="foo" */
	count = rte_kvargs_count(kvlist, "foo");
	if (count != 1) {
		printf("invalid count value %d after rte_kvargs_count(foo)\n",
			count);
		rte_kvargs_free(kvlist);
		return -1;
	}

	/* count all entries with key="nonexistent_key" */
	count = rte_kvargs_count(kvlist, "nonexistent_key");
	if (count != 0) {
		printf("invalid count value %d after rte_kvargs_count(nonexistent_key)\n",
			count);
		rte_kvargs_free(kvlist);
		return -1;
	}

	rte_kvargs_free(kvlist);
	return 0;
}

static int
test_parse_with_valid_keys(const void *params)
{
	const bool use_opt = *(const bool *)params;
	f_kvargs_process proc_func = use_opt ? rte_kvargs_process_opt : rte_kvargs_process;
	const char *proc_name = use_opt ? "rte_kvargs_process_opt" : "rte_kvargs_process";
	const char *args = "foo=droids,check=value0,check=value1,check=wrong_value";
	const char *valid_keys[] = { "foo", "check", NULL };
	struct rte_kvargs *kvlist;

	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error\n");
		return -1;
	}

	/* call check_handler() on all entries with key="check", it
	 * should fail as the value is not recognized by the handler
	 */
	count = 0;
	if (proc_func(kvlist, "check", check_handler, NULL) == 0 || count != 2) {
		printf("%s(check) is success but should not\n", proc_name);
		rte_kvargs_free(kvlist);
		return -1;
	}

	count = rte_kvargs_count(kvlist, "check");
	if (count != 3) {
		printf("invalid count value %u after rte_kvargs_count(check)\n",
			count);
		rte_kvargs_free(kvlist);
		return -1;
	}

	rte_kvargs_free(kvlist);
	return 0;
}

static int
test_parse_list_value(void)
{
	const char *valid_keys[] = { "foo", "check", NULL };
	const char *args = "foo=[0,1],check=value2";
	struct rte_kvargs *kvlist;

	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error\n");
		return -1;
	}

	count = kvlist->count;
	if (count != 2) {
		printf("invalid count value %u\n", count);
		rte_kvargs_free(kvlist);
		return -1;
	}

	if (strcmp(kvlist->pairs[0].value, "[0,1]") != 0) {
		printf("wrong value %s", kvlist->pairs[0].value);
		rte_kvargs_free(kvlist);
		return -1;
	}

	rte_kvargs_free(kvlist);
	return 0;
}

static int
test_parse_empty_elements(void)
{
	const char *args = "foo=1,,check=value2,,";
	struct rte_kvargs *kvlist;

	kvlist = rte_kvargs_parse(args, NULL);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error\n");
		return -1;
	}

	count = kvlist->count;
	if (count != 2) {
		printf("invalid count value %u\n", count);
		rte_kvargs_free(kvlist);
		return -1;
	}

	if (rte_kvargs_count(kvlist, "foo") != 1) {
		printf("invalid count value for 'foo'\n");
		rte_kvargs_free(kvlist);
		return -1;
	}

	if (rte_kvargs_count(kvlist, "check") != 1) {
		printf("invalid count value for 'check'\n");
		rte_kvargs_free(kvlist);
		return -1;
	}

	rte_kvargs_free(kvlist);
	return 0;
}

static int
test_parse_with_only_key(void)
{
	const char *args = "foo,check";
	struct rte_kvargs *kvlist;

	kvlist = rte_kvargs_parse(args, NULL);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error\n");
		return -1;
	}

	if (rte_kvargs_process(kvlist, "check", check_only_handler, NULL) == 0) {
		printf("rte_kvargs_process(check) error\n");
		rte_kvargs_free(kvlist);
		return -1;
	}

	if (rte_kvargs_process_opt(kvlist, "check", check_only_handler, NULL) != 0) {
		printf("rte_kvargs_process_opt(check) error\n");
		rte_kvargs_free(kvlist);
		return -1;
	}

	rte_kvargs_free(kvlist);
	return 0;
}

/* test several error cases */
static int test_invalid_kvargs(void)
{
	struct rte_kvargs *kvlist;
	/* list of argument that should fail */
	const char *args_list[] = {
		"wrong-key=x",     /* key not in valid_keys_list */
		NULL };
	const char **args;
	const char *valid_keys_list[] = { "foo", "check", NULL };
	const char **valid_keys = valid_keys_list;

	for (args = args_list; *args != NULL; args++) {

		kvlist = rte_kvargs_parse(*args, valid_keys);
		if (kvlist != NULL) {
			printf("rte_kvargs_parse() returned 0 (but should not)\n");
			rte_kvargs_free(kvlist);
			goto fail;
		}
	}
	return 0;

 fail:
	printf("while processing <%s>", *args);
	if (valid_keys != NULL && *valid_keys != NULL) {
		printf(" using valid_keys=<%s", *valid_keys);
		while (*(++valid_keys) != NULL)
			printf(",%s", *valid_keys);
		printf(">");
	}
	printf("\n");
	return -1;
}

static struct unit_test_suite kvargs_test_suite  = {
	.suite_name = "Kvargs Unit Test Suite",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_basic_token_count),
		TEST_CASE_NAMED_WITH_DATA("test_parse_without_valid_keys_no_opt",
					  NULL, NULL,
					  test_parse_without_valid_keys,
					  &use_kvargs_process_opt[0]),
		TEST_CASE_NAMED_WITH_DATA("test_parse_without_valid_keys_with_opt",
					  NULL, NULL,
					  test_parse_without_valid_keys,
					  &use_kvargs_process_opt[1]),
		TEST_CASE_NAMED_WITH_DATA("test_parse_with_valid_keys_no_opt",
					  NULL, NULL,
					  test_parse_with_valid_keys,
					  &use_kvargs_process_opt[0]),
		TEST_CASE_NAMED_WITH_DATA("test_parse_with_valid_keys_with_opt",
					  NULL, NULL,
					  test_parse_with_valid_keys,
					  &use_kvargs_process_opt[1]),
		TEST_CASE(test_parse_list_value),
		TEST_CASE(test_parse_empty_elements),
		TEST_CASE(test_parse_with_only_key),
		TEST_CASE(test_invalid_kvargs),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_kvargs(void)
{
	return unit_test_suite_runner(&kvargs_test_suite);
}

REGISTER_FAST_TEST(kvargs_autotest, NOHUGE_OK, ASAN_OK, test_kvargs);
