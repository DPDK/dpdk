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
		{ "--abc", "-a", "abc argument", (void *)1, (void *)1, \
			RTE_ARGPARSE_VALUE_NONE, RTE_ARGPARSE_VALUE_TYPE_NONE }, \
		{ "--xyz", "-x", "xyz argument", (void *)1, (void *)2, \
			RTE_ARGPARSE_VALUE_NONE, RTE_ARGPARSE_VALUE_TYPE_NONE }, \
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
	uint64_t invalid_values[] = {
			RTE_ARGPARSE_VALUE_NONE,
			RTE_ARGPARSE_VALUE_OPTIONAL,
	};
	struct rte_argparse *obj;
	uint32_t index;
	int ret;

	/* test positional arg don't config required-value. */
	for (index = 0; index < RTE_DIM(invalid_values); index++) {
		obj = test_argparse_init_obj();
		obj->args[0].name_long = "abc";
		obj->args[0].name_short = NULL;
		obj->args[0].value_required = invalid_values[index];
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
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_NONE;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test saver == NULL with callback is NULL. */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_NONE;
	obj->callback = NULL;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test saver != NULL with val-type is zero! */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = (void *)1;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_NONE;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test saver != NULL with required value, but val-set is not NULL. */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = (void *)1;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_invalid_arg_flags(void)
{
	struct rte_argparse *obj;
	int ret;

	/* test set unused bits. */
	obj = test_argparse_init_obj();
	obj->args[0].flags |= ~(RTE_ARGPARSE_FLAG_SUPPORT_MULTI);
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test positional arg should not config multiple.  */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "positional";
	obj->args[0].name_short = NULL;
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[0].flags |= RTE_ARGPARSE_FLAG_SUPPORT_MULTI;
	ret = rte_argparse_parse(obj, default_argc, default_argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test optional arg enabled multiple but prased by autosave. */
	obj = test_argparse_init_obj();
	obj->args[0].flags |= RTE_ARGPARSE_FLAG_SUPPORT_MULTI;
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

static int
test_argparse_invalid_option(void)
{
	struct rte_argparse *obj;
	char *argv[2];
	int ret;

	obj = test_argparse_init_obj();
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--invalid");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	obj = test_argparse_init_obj();
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("invalid");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_opt_autosave_parse_int_of_no_val(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[2];
	int ret;

	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = (void *)100;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_NONE;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--test-long");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	obj->args[0].value_required = RTE_ARGPARSE_VALUE_NONE;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("-t");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	return 0;
}

static int
test_argparse_opt_autosave_parse_int_of_required_val(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[3];
	int ret;

	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--test-long");
	argv[2] = test_strdup("100");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("-t");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	/* test invalid value. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("-t");
	argv[2] = test_strdup("100a");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_opt_autosave_parse_int_of_optional_val(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[2];
	int ret;

	/* test without value. */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = (void *)100;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--test-long");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("-t");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	/* test with value. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("--test-long=200");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 200, "Argparse parse expect success!");
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("-t=200");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 200, "Argparse parse expect success!");

	/* test with option value, but with wrong value. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("--test-long=200a");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("-t=200a");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
opt_callback_parse_int_of_no_val(uint32_t index, const char *value, void *opaque)
{
	if (index != 1)
		return -EINVAL;
	if (value != NULL)
		return -EINVAL;
	*(int *)opaque = 100;
	return 0;
}

static int
test_argparse_opt_callback_parse_int_of_no_val(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[2];
	int ret;

	obj = test_argparse_init_obj();
	obj->callback = opt_callback_parse_int_of_no_val;
	obj->opaque = (void *)&val_saver;
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = NULL;
	obj->args[0].val_set = (void *)1;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_NONE;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--test-long");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	obj->args[0].value_required = RTE_ARGPARSE_VALUE_NONE;
	val_saver = 0;
	argv[1] = test_strdup("-t");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	return 0;
}

static int
opt_callback_parse_int_of_required_val(uint32_t index, const char *value, void *opaque)
{
	char *s = NULL;

	if (index != 1)
		return -EINVAL;

	if (value == NULL)
		return -EINVAL;
	*(int *)opaque = strtol(value, &s, 0);

	if (s[0] != '\0')
		return -EINVAL;

	return 0;
}

static int
test_argparse_opt_callback_parse_int_of_required_val(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[3];
	int ret;

	obj = test_argparse_init_obj();
	obj->callback = opt_callback_parse_int_of_required_val;
	obj->opaque = (void *)&val_saver;
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = NULL;
	obj->args[0].val_set = (void *)1;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--test-long");
	argv[2] = test_strdup("100");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	val_saver = 0;
	argv[1] = test_strdup("-t");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	/* test no more parameters. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test callback return failed. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	argv[2] = test_strdup("100a");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
opt_callback_parse_int_of_optional_val(uint32_t index, const char *value, void *opaque)
{
	char *s = NULL;

	if (index != 1)
		return -EINVAL;

	if (value == NULL) {
		*(int *)opaque = 10;
	} else {
		*(int *)opaque = strtol(value, &s, 0);
		if (s[0] != '\0')
			return -EINVAL;
	}

	return 0;
}

static int
test_argparse_opt_callback_parse_int_of_optional_val(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[2];
	int ret;

	obj = test_argparse_init_obj();
	obj->callback = opt_callback_parse_int_of_optional_val;
	obj->opaque = (void *)&val_saver;
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = NULL;
	obj->args[0].val_set = (void *)1;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--test-long");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 10, "Argparse parse expect success!");

	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	val_saver = 0;
	argv[1] = test_strdup("-t");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 10, "Argparse parse expect success!");

	/* test with value. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	val_saver = 0;
	argv[1] = test_strdup("--test-long=100");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	val_saver = 0;
	argv[1] = test_strdup("-t=100");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	/* test callback return failed. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	argv[1] = test_strdup("-t=100a");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_pos_autosave_parse_int(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[3];
	int ret;

	/* test positional autosave parse successful. */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "test-long";
	obj->args[0].name_short = NULL;
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("100");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse expect success!");
	TEST_ASSERT(val_saver == 100, "Argparse parse expect success!");

	/* test positional autosave parse failed. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	val_saver = 0;
	argv[1] = test_strdup("100a");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	/* test too much position parameters. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	argv[1] = test_strdup("100");
	argv[2] = test_strdup("200");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
pos_callback_parse_int(uint32_t index, const char *value, void *opaque)
{
	uint32_t int_val;
	char *s = NULL;

	if (index != 1 && index != 2)
		return -EINVAL;
	if (value == NULL)
		return -EINVAL;

	int_val = strtol(value, &s, 0);
	if (s[0] != '\0')
		return -EINVAL;

	*((int *)opaque	+ index) = int_val;

	return 0;
}

static int
test_argparse_pos_callback_parse_int(void)
{
	int val_saver[3] = { 0, 0, 0 };
	struct rte_argparse *obj;
	char *argv[3];
	int ret;

	/* test positional callback parse successful. */
	obj = test_argparse_init_obj();
	obj->callback = pos_callback_parse_int;
	obj->opaque = (void *)val_saver;
	obj->args[0].name_long = "test-long1";
	obj->args[0].name_short = NULL;
	obj->args[0].val_saver = NULL;
	obj->args[0].val_set = (void *)1;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[1].name_long = "test-long2";
	obj->args[1].name_short = NULL;
	obj->args[1].val_saver = NULL;
	obj->args[1].val_set = (void *)2;
	obj->args[1].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[2].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("100");
	argv[2] = test_strdup("200");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse parse expect success!");
	TEST_ASSERT(val_saver[1] == 100, "Argparse parse expect success!");
	TEST_ASSERT(val_saver[2] == 200, "Argparse parse expect success!");

	/* test positional callback parse failed. */
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[1].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	argv[2] = test_strdup("200a");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse parse expect failed!");

	return 0;
}

static int
test_argparse_parse_type(void)
{
	char *str_erange = test_strdup("9999999999999999999999999999999999");
	char *str_erange_u32 = test_strdup("4294967296");
	char *str_erange_u16 = test_strdup("65536");
	char *str_erange_u8 = test_strdup("256");
	char *str_invalid = test_strdup("1a");
	char *str_ok = test_strdup("123");
	char *bool_true = test_strdup("true");
	char *bool_false = test_strdup("false");
	char *bool_invalid = test_strdup("invalid");
	char *bool_numeric_true = test_strdup("1");
	char *bool_numeric_false = test_strdup("0");
	char *bool_numeric_invalid = test_strdup("2");
	uint16_t val_u16;
	uint32_t val_u32;
	uint64_t val_u64;
	uint8_t val_u8;
	int val_int;
	int ret;

	/* test for int parsing */
	ret = rte_argparse_parse_type(str_erange, RTE_ARGPARSE_VALUE_TYPE_INT, &val_int);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_invalid, RTE_ARGPARSE_VALUE_TYPE_INT, &val_int);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_ok, RTE_ARGPARSE_VALUE_TYPE_INT, &val_int);
	TEST_ASSERT(ret == 0, "Argparse parse type expect failed!");
	TEST_ASSERT(val_int == 123, "Argparse parse type expect failed!");

	/* test for u8 parsing */
	ret = rte_argparse_parse_type(str_erange, RTE_ARGPARSE_VALUE_TYPE_U8, &val_u8);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_erange_u8, RTE_ARGPARSE_VALUE_TYPE_U8, &val_u8);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_invalid, RTE_ARGPARSE_VALUE_TYPE_U8, &val_u8);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	val_u8 = 0;
	ret = rte_argparse_parse_type(str_ok, RTE_ARGPARSE_VALUE_TYPE_U8, &val_u8);
	TEST_ASSERT(ret == 0, "Argparse parse type expect failed!");
	TEST_ASSERT(val_u8 == 123, "Argparse parse type expect failed!");

	/* test for u16 parsing */
	ret = rte_argparse_parse_type(str_erange, RTE_ARGPARSE_VALUE_TYPE_U16, &val_u16);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_erange_u16, RTE_ARGPARSE_VALUE_TYPE_U16, &val_u16);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_invalid, RTE_ARGPARSE_VALUE_TYPE_U16, &val_u16);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	val_u16 = 0;
	ret = rte_argparse_parse_type(str_ok, RTE_ARGPARSE_VALUE_TYPE_U16, &val_u16);
	TEST_ASSERT(ret == 0, "Argparse parse type expect failed!");
	TEST_ASSERT(val_u16 == 123, "Argparse parse type expect failed!");

	/* test for u32 parsing */
	ret = rte_argparse_parse_type(str_erange, RTE_ARGPARSE_VALUE_TYPE_U32, &val_u32);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_erange_u32, RTE_ARGPARSE_VALUE_TYPE_U32, &val_u32);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_invalid, RTE_ARGPARSE_VALUE_TYPE_U32, &val_u32);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	val_u32 = 0;
	ret = rte_argparse_parse_type(str_ok, RTE_ARGPARSE_VALUE_TYPE_U32, &val_u32);
	TEST_ASSERT(ret == 0, "Argparse parse type expect failed!");
	TEST_ASSERT(val_u32 == 123, "Argparse parse type expect failed!");

	/* test for u64 parsing */
	ret = rte_argparse_parse_type(str_erange, RTE_ARGPARSE_VALUE_TYPE_U64, &val_u64);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	ret = rte_argparse_parse_type(str_invalid, RTE_ARGPARSE_VALUE_TYPE_U64, &val_u64);
	TEST_ASSERT(ret != 0, "Argparse parse type expect failed!");
	val_u64 = 0;
	ret = rte_argparse_parse_type(str_ok, RTE_ARGPARSE_VALUE_TYPE_U64, &val_u64);
	TEST_ASSERT(ret == 0, "Argparse parse type expect failed!");
	TEST_ASSERT(val_u64 == 123, "Argparse parse type expect failed!");

	/* test for string parsing - all it does is save string, so all are valid */
	const char *val_str;
	ret = rte_argparse_parse_type(str_erange, RTE_ARGPARSE_VALUE_TYPE_STR, &val_str);
	TEST_ASSERT(ret == 0, "Argparse parse a string failed unexpectedly!");

	/* test for boolean parsing */
	bool val_bool = false;
	ret = rte_argparse_parse_type(bool_true, RTE_ARGPARSE_VALUE_TYPE_BOOL, &val_bool);
	TEST_ASSERT(ret == 0 && val_bool == true, "Argparse parse type for bool (true) failed!");
	ret = rte_argparse_parse_type(bool_false, RTE_ARGPARSE_VALUE_TYPE_BOOL, &val_bool);
	TEST_ASSERT(ret == 0 && val_bool == false, "Argparse parse type for bool (false) failed!");
	ret = rte_argparse_parse_type(bool_invalid, RTE_ARGPARSE_VALUE_TYPE_BOOL, &val_bool);
	TEST_ASSERT(ret != 0, "Argparse parse type for bool (invalid) passed unexpectedly!");
	ret = rte_argparse_parse_type(bool_numeric_true, RTE_ARGPARSE_VALUE_TYPE_BOOL, &val_bool);
	TEST_ASSERT(ret == 0 && val_bool == true, "Argparse parse type for bool (numeric true) failed!");
	ret = rte_argparse_parse_type(bool_numeric_false, RTE_ARGPARSE_VALUE_TYPE_BOOL,
			&val_bool);
	TEST_ASSERT(ret == 0 && val_bool == false, "Argparse parse type for bool (numeric false) failed!");
	ret = rte_argparse_parse_type(bool_numeric_invalid, RTE_ARGPARSE_VALUE_TYPE_BOOL,
			&val_bool);
	TEST_ASSERT(ret != 0, "Argparse parse type for bool (numeric invalid) passed unexpectedly!");
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
		TEST_CASE(test_argparse_invalid_option),
		TEST_CASE(test_argparse_opt_autosave_parse_int_of_no_val),
		TEST_CASE(test_argparse_opt_autosave_parse_int_of_required_val),
		TEST_CASE(test_argparse_opt_autosave_parse_int_of_optional_val),
		TEST_CASE(test_argparse_opt_callback_parse_int_of_no_val),
		TEST_CASE(test_argparse_opt_callback_parse_int_of_required_val),
		TEST_CASE(test_argparse_opt_callback_parse_int_of_optional_val),
		TEST_CASE(test_argparse_pos_autosave_parse_int),
		TEST_CASE(test_argparse_pos_callback_parse_int),
		TEST_CASE(test_argparse_parse_type),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_argparse(void)
{
	return unit_test_suite_runner(&argparse_test_suite);
}

REGISTER_FAST_TEST(argparse_autotest, true, true, test_argparse);
