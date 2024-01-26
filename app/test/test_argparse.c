/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#include <stdio.h>
#include <string.h>

#include <rte_argparse.h>

#include "test.h"

static int default_argc;
static char *default_argv[1];

#define MAX_STRDUP_STORE_NUM	512
static char *strdup_store_array[MAX_STRDUP_STORE_NUM];
static uint32_t strdup_store_index;

/*
 * Define strdup wrapper.
 * 1. Mainly to fix compile error "warning: assignment discards 'const'
 *    qualifier from pointer target type [-Wdiscarded-qualifiers]" for
 *    following code:
 *      argv[x] = "100";
 * 2. The strdup result will store in the strdup_store_array, and then
 *    freed in the teardown function, prevent ASAN errors from being
 *    triggered.
 */
static char *
test_strdup(const char *str)
{
	char *s = strdup(str);
	if (s == NULL) {
		printf("strdup failed! exiting...\n");
		exit(-ENOMEM);
	}
	if (strdup_store_index >= MAX_STRDUP_STORE_NUM) {
		printf("too much strdup calls! exiting...\n");
		exit(-ERANGE);
	}
	strdup_store_array[strdup_store_index++] = s;
	return s;
}

static int
test_argparse_setup(void)
{
	strdup_store_index = 0;
	default_argc = 1;
	default_argv[0] = test_strdup("test_argparse");
	return 0;
}

static void
test_argparse_teardown(void)
{
	uint32_t i;
	printf("total used strdup_store_index = %u\n", strdup_store_index);
	for (i = 0; i < strdup_store_index; i++)
		free(strdup_store_array[i]);
	strdup_store_index = 0;
}

static int
test_argparse_callback(uint32_t index, const char *value, void *opaque)
{
	RTE_SET_USED(index);
	RTE_SET_USED(value);
	RTE_SET_USED(opaque);
	return 0;
}

/* valid templater, must contain at least two args. */
#define argparse_templater() { \
	.prog_name = "test_argparse", \
	.usage = "-a xx -b yy", \
	.descriptor = NULL, \
	.epilog = NULL, \
	.exit_on_error = false, \
	.callback = test_argparse_callback, \
	.args = { \
		{ "--abc", "-a", "abc argument", (void *)1, (void *)1, RTE_ARGPARSE_ARG_NO_VALUE | RTE_ARGPARSE_ARG_VALUE_INT }, \
		{ "--xyz", "-x", "xyz argument", (void *)1, (void *)2, RTE_ARGPARSE_ARG_NO_VALUE | RTE_ARGPARSE_ARG_VALUE_INT }, \
		ARGPARSE_ARG_END(), \
	}, \
}

static void
test_argparse_copy(struct rte_argparse *dst, struct rte_argparse *src)
{
	uint32_t i;
	memcpy(dst, src, sizeof(*src));
	for (i = 0; /* NULL */; i++) {
		memcpy(&dst->args[i], &src->args[i], sizeof(src->args[i]));
		if (src->args[i].name_long == NULL)
			break;
	}
}

static struct rte_argparse *
test_argparse_init_obj(void)
{
	static struct rte_argparse backup = argparse_templater();
	static struct rte_argparse obj = argparse_templater();
	/* Because obj may be overwritten, do a deep copy. */
	test_argparse_copy(&obj, &backup);
	return &obj;
}

static int
test_argparse_invalid_basic_param(void)
{
	struct rte_argparse *obj;
	int ret;

	obj = test_argparse_init_obj();
	obj->prog_name = NULL;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->usage = NULL;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return TEST_SUCCESS;
}

static int
test_argparse_invalid_arg_name(void)
{
	struct rte_argparse *obj;
	int ret;

	obj = test_argparse_init_obj();
	obj->args[0].name_long = "-ab";
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].name_long = "-abc";
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].name_long = "---c";
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].name_long = "abc";
	obj->args[0].name_short = "-a";
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].name_short = "a";
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].name_short = "abc";
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].name_short = "ab";
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_invalid_arg_help(void)
{
	struct rte_argparse *obj;
	int ret;

	obj = test_argparse_init_obj();
	obj->args[0].help = NULL;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_invalid_has_val(void)
{
	uint32_t set_mask[] = { 0,
				RTE_ARGPARSE_ARG_NO_VALUE,
				RTE_ARGPARSE_ARG_OPTIONAL_VALUE
			      };
	struct rte_argparse *obj;
	uint32_t index;
	int ret;

	obj = test_argparse_init_obj();
	obj->args[0].flags &= ~0x3u;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	for (index = 0; index < RTE_DIM(set_mask); index++) {
		obj = test_argparse_init_obj();
		obj->args[0].name_long = "abc";
		obj->args[0].name_short = NULL;
		obj->args[0].flags &= ~0x3u;
		obj->args[0].flags |= set_mask[index];
		ret = rte_argparse_parse(obj, default_argc, default_argv);
		TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");
	}

	return 0;
}

static int
test_argparse_invalid_arg_saver(void)
{
	struct rte_argparse *obj;
	int ret;

	/* test saver == NULL with val-type != 0. */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = NULL;
	obj->args[0].flags = RTE_ARGPARSE_ARG_NO_VALUE | RTE_ARGPARSE_ARG_VALUE_INT;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test saver == NULL with callback is NULL. */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = NULL;
	obj->args[0].flags = RTE_ARGPARSE_ARG_NO_VALUE;
	obj->callback = NULL;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test saver != NULL with val-type is zero! */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = (void *)1;
	obj->args[0].flags = RTE_ARGPARSE_ARG_NO_VALUE;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test saver != NULL with val-type is max. */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = (void *)1;
	obj->args[0].flags = RTE_ARGPARSE_ARG_NO_VALUE | RTE_ARGPARSE_ARG_VALUE_MAX;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test saver != NULL with required value, but val-set is not NULL. */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = (void *)1;
	obj->args[0].flags = RTE_ARGPARSE_ARG_REQUIRED_VALUE | RTE_ARGPARSE_ARG_VALUE_INT;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_invalid_arg_flags(void)
{
	struct rte_argparse *obj;
	int ret;

	obj = test_argparse_init_obj();
	obj->args[0].flags |= ~0x107FFu;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].name_long = "positional";
	obj->args[0].name_short = NULL;
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = (void *)1;
	obj->args[0].flags = RTE_ARGPARSE_ARG_REQUIRED_VALUE | RTE_ARGPARSE_ARG_VALUE_INT |
			     RTE_ARGPARSE_ARG_SUPPORT_MULTI;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].flags |= RTE_ARGPARSE_ARG_SUPPORT_MULTI;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	obj->args[0].val_saver = NULL;
	obj->args[0].flags = RTE_ARGPARSE_ARG_REQUIRED_VALUE | RTE_ARGPARSE_ARG_SUPPORT_MULTI;
	obj->callback = NULL;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_invalid_arg_repeat(void)
{
	struct rte_argparse *obj;
	int ret;

	/* test for long name repeat! */
	obj = test_argparse_init_obj();
	obj->args[1].name_long = obj->args[0].name_long;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test for short name repeat! */
	obj = test_argparse_init_obj();
	obj->args[1].name_short = obj->args[0].name_short;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static struct unit_test_suite argparse_test_suite = {
	.suite_name = "Argparse Unit Test Suite",
	.setup = test_argparse_setup,
	.teardown = test_argparse_teardown,
	.unit_test_cases = {
		TEST_CASE(test_argparse_invalid_basic_param),
		TEST_CASE(test_argparse_invalid_arg_name),
		TEST_CASE(test_argparse_invalid_arg_help),
		TEST_CASE(test_argparse_invalid_has_val),
		TEST_CASE(test_argparse_invalid_arg_saver),
		TEST_CASE(test_argparse_invalid_arg_flags),
		TEST_CASE(test_argparse_invalid_arg_repeat),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_argparse(void)
{
	return unit_test_suite_runner(&argparse_test_suite);
}

REGISTER_FAST_TEST(argparse_autotest, true, true, test_argparse);
