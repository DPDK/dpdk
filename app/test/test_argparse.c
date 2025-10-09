/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#include <stdio.h>
#include <string.h>

#include <rte_argparse.h>
#include <rte_os.h>

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
test_argparse_invalid_repeated_option(void)
{
	/* test that we allow repeated args only with the MULTI flag */
	struct rte_argparse *obj;
	char *argv[3];
	int ret;

	/* check that we error out with two "-a" flags */
	obj = test_argparse_init_obj();
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("-a");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse did not error out with two '-a' flags!");

	obj = test_argparse_init_obj();
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	obj->args[0].flags |= RTE_ARGPARSE_FLAG_SUPPORT_MULTI;
	/* check that we allow two "-a" flags with MULTI flag set */
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("-a");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse failed to handle duplicate '-a' flags!");

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
test_argparse_opt_parse_corelist_of_required_val(void)
{
	struct rte_argparse *obj;
	rte_cpuset_t val_cpuset;
	char *argv[3];
	int ret;

	/* test with long option and single core - this is known to work */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--corelist";
	obj->args[0].name_short = "-c";
	obj->args[0].val_saver = (void *)&val_cpuset;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_CORELIST;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--corelist");
	argv[2] = test_strdup("1,3-5");
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse parse expect success!");
	TEST_ASSERT(!CPU_ISSET(0, &val_cpuset), "Core 0 should not be set in corelist!");
	TEST_ASSERT(CPU_ISSET(1, &val_cpuset), "Core 1 should be set in corelist!");
	TEST_ASSERT(!CPU_ISSET(2, &val_cpuset), "Core 2 should not be set in corelist!");
	TEST_ASSERT(CPU_ISSET(3, &val_cpuset), "Core 3 should be set in corelist!");
	TEST_ASSERT(CPU_ISSET(4, &val_cpuset), "Core 4 should be set in corelist!");
	TEST_ASSERT(CPU_ISSET(5, &val_cpuset), "Core 5 should be set in corelist!");
	TEST_ASSERT(!CPU_ISSET(6, &val_cpuset), "Core 6 should not be set in corelist!");

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
test_argparse_parse_type_corelist(void)
{
	char *corelist_valid_single = test_strdup("5");
	char *corelist_valid_multiple = test_strdup("0,1,5");
	char *corelist_valid_range = test_strdup("1-5");
	char *corelist_valid_mixed = test_strdup("0,1,5-10,12-16,18,20");
	char *corelist_valid_reverse_range = test_strdup("10-5");
	char *corelist_valid_initial_spaces = test_strdup("   1,2,5-7");
	char *corelist_valid_empty = test_strdup("");
	char *corelist_invalid_spaces = test_strdup(" 1 , 2 , 5-7 ");
	char *corelist_invalid_letters = test_strdup("1,a,3");
	char *corelist_invalid_range_incomplete = test_strdup("1-");
	char *corelist_invalid_range_double_dash = test_strdup("1--5");
	char *corelist_invalid_range_double_range = test_strdup("1-3-5");
	char *corelist_invalid_special_chars = test_strdup("1,2@3");
	char *corelist_invalid_comma_only = test_strdup(",");
	char *corelist_invalid_out_of_range = test_strdup("70000");
	rte_cpuset_t val_cpuset;
	int ret;

	/* test valid single core */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_valid_single,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret == 0, "Argparse parse type for corelist (single core) failed!");
	TEST_ASSERT(CPU_ISSET(5, &val_cpuset), "Core 5 should be set in corelist!");
	TEST_ASSERT(!CPU_ISSET(0, &val_cpuset), "Core 0 should not be set in corelist!");
	TEST_ASSERT(!CPU_ISSET(1, &val_cpuset), "Core 1 should not be set in corelist!");

	/* test valid multiple cores */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_valid_multiple,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret == 0, "Argparse parse type for corelist (multiple cores) failed!");
	TEST_ASSERT(CPU_ISSET(0, &val_cpuset), "Core 0 should be set in corelist!");
	TEST_ASSERT(CPU_ISSET(1, &val_cpuset), "Core 1 should be set in corelist!");
	TEST_ASSERT(CPU_ISSET(5, &val_cpuset), "Core 5 should be set in corelist!");
	TEST_ASSERT(!CPU_ISSET(2, &val_cpuset), "Core 2 should not be set in corelist!");
	TEST_ASSERT(!CPU_ISSET(3, &val_cpuset), "Core 3 should not be set in corelist!");

	/* test valid range */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_valid_range,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret == 0, "Argparse parse type for corelist (range) failed!");
	for (int i = 1; i <= 5; i++)
		TEST_ASSERT(CPU_ISSET(i, &val_cpuset), "Core %d should be set in range 1-5!", i);
	TEST_ASSERT(!CPU_ISSET(0, &val_cpuset), "Core 0 should not be set in range 1-5!");
	TEST_ASSERT(!CPU_ISSET(6, &val_cpuset), "Core 6 should not be set in range 1-5!");

	/* test valid mixed corelist */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_valid_mixed,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret == 0, "Argparse parse type for corelist (mixed) failed!");
	TEST_ASSERT(CPU_ISSET(0, &val_cpuset), "Core 0 should be set in mixed corelist!");
	TEST_ASSERT(CPU_ISSET(1, &val_cpuset), "Core 1 should be set in mixed corelist!");
	for (int i = 5; i <= 10; i++)
		TEST_ASSERT(CPU_ISSET(i, &val_cpuset), "Core %d should be set in range 5-10!", i);
	for (int i = 12; i <= 16; i++)
		TEST_ASSERT(CPU_ISSET(i, &val_cpuset), "Core %d should be set in range 12-16!", i);

	TEST_ASSERT(CPU_ISSET(18, &val_cpuset), "Core 18 should be set in mixed corelist!");
	TEST_ASSERT(CPU_ISSET(20, &val_cpuset), "Core 20 should be set in mixed corelist!");
	TEST_ASSERT(!CPU_ISSET(2, &val_cpuset), "Core 2 should not be set in mixed corelist!");
	TEST_ASSERT(!CPU_ISSET(11, &val_cpuset), "Core 11 should not be set in mixed corelist!");
	TEST_ASSERT(!CPU_ISSET(17, &val_cpuset), "Core 17 should not be set in mixed corelist!");
	TEST_ASSERT(!CPU_ISSET(19, &val_cpuset), "Core 19 should not be set in mixed corelist!");

	/* test valid reverse range (10-5 should be interpreted as 5-10) */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_valid_reverse_range,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret == 0, "Argparse parse type for corelist (reverse range) failed!");
	for (int i = 5; i <= 10; i++)
		TEST_ASSERT(CPU_ISSET(i, &val_cpuset),
				"Core %d should be set in reverse range 10-5!", i);
	TEST_ASSERT(!CPU_ISSET(4, &val_cpuset), "Core 4 should not be set in reverse range 10-5!");
	TEST_ASSERT(!CPU_ISSET(11, &val_cpuset), "Core 11 should not be set in reverse range 10-5!");

	/* test valid corelist with initial spaces only */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_valid_initial_spaces,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret == 0, "Argparse parse type for corelist (with initial spaces) failed!");
	TEST_ASSERT(CPU_ISSET(1, &val_cpuset), "Core 1 should be set in initial spaced corelist!");
	TEST_ASSERT(CPU_ISSET(2, &val_cpuset), "Core 2 should be set in initial spaced corelist!");
	for (int i = 5; i <= 7; i++)
		TEST_ASSERT(CPU_ISSET(i, &val_cpuset),
				"Core %d should be set in initial spaced range 5-7!", i);

	/* test valid empty corelist */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_valid_empty,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret == 0, "Argparse parse type for corelist (empty) failed!");
	/* Verify that no cores are set in empty corelist */
	for (int i = 0; i < CPU_SETSIZE; i++)
		TEST_ASSERT(!CPU_ISSET(i, &val_cpuset),
				"Core %d should not be set in empty corelist!", i);

	/* test invalid corelist with spaces */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_spaces,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (with spaces) should have failed!");

	/* test invalid corelist with letters */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_letters,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (with letters) should have failed!");

	/* test invalid corelist with incomplete range */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_range_incomplete,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (incomplete range) should have failed!");

	/* test invalid corelist with double dash */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_range_double_dash,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (double dash) should have failed!");

	/* test invalid corelist with double dash */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_range_double_range,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (double range) should have failed!");

	/* test invalid corelist with special characters */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_special_chars,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (special chars) should have failed!");

	/* test invalid comma-only corelist */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_comma_only,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (comma only) should have failed!");

	/* test invalid out-of-range corelist */
	CPU_ZERO(&val_cpuset);
	ret = rte_argparse_parse_type(corelist_invalid_out_of_range,
			RTE_ARGPARSE_VALUE_TYPE_CORELIST, &val_cpuset);
	TEST_ASSERT(ret != 0, "Argparse parse type for corelist (out of range) should have failed!");

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

	/* test for corelist parsing */
	ret = test_argparse_parse_type_corelist();
	if (ret != 0)
		return ret;

	return 0;
}

static int
test_argparse_ignore_non_flag_args_disabled(void)
{
	struct rte_argparse *obj;
	char *argv[6];
	int ret;

	/* Test that without ignore_non_flag_args, non-flag args cause an error */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = false;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("nonflagvalue");
	argv[3] = test_strdup("-x");
	ret = rte_argparse_parse(obj, 4, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse should fail with non-flag arg when flag is disabled!");

	/* Test with non-flag args mixed with flags */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = false;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("nonflagvalue1");
	argv[2] = test_strdup("-a");
	argv[3] = test_strdup("-x");
	argv[4] = test_strdup("nonflagvalue2");
	ret = rte_argparse_parse(obj, 5, argv);
	TEST_ASSERT(ret == -EINVAL, "Argparse should fail with non-flag args when flag is disabled!");

	return 0;
}

static int
test_argparse_ignore_non_flag_args_basic(void)
{
	struct rte_argparse *obj;
	char *argv[8];
	int ret;

	/* Test basic reordering: ['app', '-a', 'nonflagvalue', '-x']
	 * Should process -a and -x, return 2 processed args, move nonflagvalue to end
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("nonflagvalue");
	argv[3] = test_strdup("-x");
	ret = rte_argparse_parse(obj, 4, argv);
	TEST_ASSERT(ret == 3, "Argparse should return 3 (processed all but 1 non-flag), got %d!",
			ret);
	TEST_ASSERT(strcmp(argv[3], "nonflagvalue") == 0,
		"Non-flag arg should be moved to end, but argv[3]='%s'!", argv[3]);

	/* Test with multiple non-flag args:
	 * ['app', '-a', 'nonflag1', '-x', 'nonflag2']
	 * Should process -a and -x, return 3, reorder to [..., 'nonflag1', 'nonflag2']
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("nonflag1");
	argv[3] = test_strdup("-x");
	argv[4] = test_strdup("nonflag2");
	ret = rte_argparse_parse(obj, 5, argv);
	TEST_ASSERT(ret == 3, "Argparse should return 3 (processed all but 2 non-flags), got %d!",
			ret);
	TEST_ASSERT(strcmp(argv[3], "nonflag1") == 0,
		"First non-flag arg should be at position 3, but argv[3]='%s'!", argv[3]);
	TEST_ASSERT(strcmp(argv[4], "nonflag2") == 0,
		"Second non-flag arg should be at position 4, but argv[4]='%s'!", argv[4]);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_with_values(void)
{
	struct rte_argparse *obj;
	int val_a = 0, val_x = 0;
	char *argv[10];
	int ret;

	/* Test with flags that take values:
	 * ['app', '-a', 'avalue', 'nonflag1', '-x', 'xvalue', 'nonflag2']
	 * Should process -a avalue and -x xvalue, move non-flags to end
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = (void *)&val_a;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].val_saver = (void *)&val_x;
	obj->args[1].val_set = NULL;
	obj->args[1].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[1].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("100");
	argv[3] = test_strdup("nonflag1");
	argv[4] = test_strdup("-x");
	argv[5] = test_strdup("200");
	argv[6] = test_strdup("nonflag2");
	ret = rte_argparse_parse(obj, 7, argv);
	TEST_ASSERT(ret == 5, "Argparse should return 5 (app + 4 flag-related + 0 non-flags), got %d!",
			ret);
	TEST_ASSERT(val_a == 100, "Value for -a should be parsed correctly, got %d!", val_a);
	TEST_ASSERT(val_x == 200, "Value for -x should be parsed correctly, got %d!", val_x);
	TEST_ASSERT(strcmp(argv[5], "nonflag1") == 0,
		"First non-flag arg should be at position 5, but argv[5]='%s'!", argv[5]);
	TEST_ASSERT(strcmp(argv[6], "nonflag2") == 0,
		"Second non-flag arg should be at position 6, but argv[6]='%s'!", argv[6]);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_complex_order(void)
{
	struct rte_argparse *obj;
	int val_a = 0;
	char *argv[10];
	int ret;

	/* Test complex reordering matching example from requirements:
	 * ['app', '-a', 'avalue', 'nonflag1', '-x', 'nonflag2']
	 * Should become: ['app', '-a', 'avalue', '-x', 'nonflag1', 'nonflag2']
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = (void *)&val_a;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("50");
	argv[3] = test_strdup("nonflag1");
	argv[4] = test_strdup("-x");
	argv[5] = test_strdup("nonflag2");
	ret = rte_argparse_parse(obj, 6, argv);
	TEST_ASSERT(ret == 4, "Argparse should return 4 (app + 3 flag-related), got %d!", ret);
	TEST_ASSERT(val_a == 50, "Value for -a should be parsed correctly, got %d!", val_a);
	/* Verify reordering */
	TEST_ASSERT(strcmp(argv[4], "nonflag1") == 0,
		"First non-flag should be at position 4, but argv[4]='%s'!", argv[4]);
	TEST_ASSERT(strcmp(argv[5], "nonflag2") == 0,
		"Second non-flag should be at position 5, but argv[5]='%s'!", argv[5]);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_only_non_flags(void)
{
	struct rte_argparse *obj;
	char *argv[5];
	int ret;

	/* Edge case: only non-flag args
	 * ['app', 'nonflag1', 'nonflag2', 'nonflag3']
	 * Should return 1 (only app name processed), argv unchanged
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("nonflag1");
	argv[2] = test_strdup("nonflag2");
	argv[3] = test_strdup("nonflag3");
	ret = rte_argparse_parse(obj, 4, argv);
	TEST_ASSERT(ret == 1, "Argparse should return 1 (only app name processed), got %d!", ret);
	TEST_ASSERT(strcmp(argv[1], "nonflag1") == 0,
		"First non-flag should remain at position 1, but argv[1]='%s'!", argv[1]);
	TEST_ASSERT(strcmp(argv[2], "nonflag2") == 0,
		"Second non-flag should remain at position 2, but argv[2]='%s'!", argv[2]);
	TEST_ASSERT(strcmp(argv[3], "nonflag3") == 0,
		"Third non-flag should remain at position 3, but argv[3]='%s'!", argv[3]);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_no_non_flags(void)
{
	struct rte_argparse *obj;
	char *argv[4];
	int ret;

	/* Edge case: no non-flag args, only flags
	 * ['app', '-a', '-x']
	 * Should process all args normally
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("-x");
	ret = rte_argparse_parse(obj, 3, argv);
	TEST_ASSERT(ret == 3, "Argparse should return 3 (all args processed), got %d!", ret);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_with_double_dash(void)
{
	struct rte_argparse *obj;
	char *argv[8];
	int ret;

	/* Test with -- separator:
	 * ['app', '-a', 'nonflag1', '--', 'arg1', 'arg2']
	 * Should process -a, stop at --, and move nonflag1 after --
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("nonflag1");
	argv[3] = test_strdup("--");
	argv[4] = test_strdup("arg1");
	argv[5] = test_strdup("arg2");
	ret = rte_argparse_parse(obj, 6, argv);
	/* Should return position of first flag after -- */
	TEST_ASSERT(ret == 3, "Argparse should return 3 (app + -a, stopped at --), got %d!", ret);
	TEST_ASSERT(strcmp(argv[2], "--") == 0,
		"-- should be moved to position 2, but argv[2]='%s'!", argv[2]);
	TEST_ASSERT(strcmp(argv[3], "nonflag1") == 0,
		"Non-flag should be moved after --, but argv[3]='%s'!", argv[3]);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_leading_non_flags(void)
{
	struct rte_argparse *obj;
	char *argv[7];
	int ret;

	/* Test with leading non-flag args:
	 * ['app', 'nonflag1', 'nonflag2', '-a', '-x']
	 * Should process -a and -x, move non-flags to end
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("nonflag1");
	argv[2] = test_strdup("nonflag2");
	argv[3] = test_strdup("-a");
	argv[4] = test_strdup("-x");
	ret = rte_argparse_parse(obj, 5, argv);
	TEST_ASSERT(ret == 3, "Argparse should return 3 (app + 2 flags), got %d!", ret);
	TEST_ASSERT(strcmp(argv[3], "nonflag1") == 0,
		"First non-flag should be at position 3, but argv[3]='%s'!", argv[3]);
	TEST_ASSERT(strcmp(argv[4], "nonflag2") == 0,
		"Second non-flag should be at position 4, but argv[4]='%s'!", argv[4]);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_trailing_non_flags(void)
{
	struct rte_argparse *obj;
	char *argv[7];
	int ret;

	/* Test with trailing non-flag args:
	 * ['app', '-a', '-x', 'nonflag1', 'nonflag2']
	 * Should process -a and -x, non-flags already at end
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-a");
	argv[2] = test_strdup("-x");
	argv[3] = test_strdup("nonflag1");
	argv[4] = test_strdup("nonflag2");
	ret = rte_argparse_parse(obj, 5, argv);
	TEST_ASSERT(ret == 3, "Argparse should return 3 (app + 2 flags), got %d!", ret);
	TEST_ASSERT(strcmp(argv[3], "nonflag1") == 0,
		"First non-flag should remain at position 3, but argv[3]='%s'!", argv[3]);
	TEST_ASSERT(strcmp(argv[4], "nonflag2") == 0,
		"Second non-flag should remain at position 4, but argv[4]='%s'!", argv[4]);

	return 0;
}

static int
test_argparse_ignore_non_flag_args_with_positional(void)
{
	struct rte_argparse *obj;
	char *argv[5];
	int ret;

	/* Test that ignore_non_flag_args cannot be used with positional args
	 * This should fail during validation
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].name_long = "positional";
	obj->args[0].name_short = NULL;
	obj->args[0].val_saver = (void *)1;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("100");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == -EINVAL,
		"Argparse should fail when ignore_non_flag_args is used with positional args!");

	return 0;
}

static int
test_argparse_ignore_non_flag_args_short_and_long(void)
{
	struct rte_argparse *obj;
	char *argv[8];
	int ret;

	/* Test with both short and long options:
	 * ['app', '--abc', 'nonflag1', '-x', 'nonflag2']
	 */
	obj = test_argparse_init_obj();
	obj->ignore_non_flag_args = true;
	obj->args[0].val_saver = NULL;
	obj->args[1].val_saver = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--abc");
	argv[2] = test_strdup("nonflag1");
	argv[3] = test_strdup("-x");
	argv[4] = test_strdup("nonflag2");
	ret = rte_argparse_parse(obj, 5, argv);
	TEST_ASSERT(ret == 3, "Argparse should return 3 (app + 2 flags), got %d!", ret);
	TEST_ASSERT(strcmp(argv[3], "nonflag1") == 0,
		"First non-flag should be at position 3, but argv[3]='%s'!", argv[3]);
	TEST_ASSERT(strcmp(argv[4], "nonflag2") == 0,
		"Second non-flag should be at position 4, but argv[4]='%s'!", argv[4]);

	return 0;
}

static int
test_argparse_short_opt_value_without_equal(void)
{
	struct rte_argparse *obj;
	int val_saver = 0;
	char *argv[3];
	int ret;

	/* Test short option with required value using -a3 syntax (no '=') */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-t42");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse with -t42 expect success, got %d!", ret);
	TEST_ASSERT(val_saver == 42, "Argparse parse -t42 should set value to 42, got %d!",
			val_saver);

	/* Test short option with optional value using -t100 syntax (no '=') */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = (void *)99;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	val_saver = 0;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-t123");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse with -t123 expect success, got %d!", ret);
	TEST_ASSERT(val_saver == 123, "Argparse parse -t123 should set value to 123, got %d!",
			val_saver);

	/* Test that -t alone with optional value uses default */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = (void *)99;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	val_saver = 0;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-t");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse with -t expect success, got %d!", ret);
	TEST_ASSERT(val_saver == 99, "Argparse parse -t should use default 99, got %d!",
			val_saver);

	/* Test short option with '=' still works */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	val_saver = 0;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-t=55");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse with -t=55 expect success, got %d!", ret);
	TEST_ASSERT(val_saver == 55, "Argparse parse -t=55 should set value to 55, got %d!",
			val_saver);

	/* Test that long option with value works with '=' */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = (void *)88;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_OPTIONAL;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	val_saver = 0;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("--test-long=66");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse with --test-long=66 expect success, got %d!", ret);
	TEST_ASSERT(val_saver == 66, "Argparse parse --test-long=66 should set value to 66, got %d!",
			val_saver);

	/* Test short option with string value without '=' */
	const char *str_saver = NULL;
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-string";
	obj->args[0].name_short = "-s";
	obj->args[0].val_saver = (void *)&str_saver;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_STR;
	obj->args[1].name_long = NULL;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-shello");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse with -shello expect success, got %d!", ret);
	TEST_ASSERT(str_saver != NULL && strcmp(str_saver, "hello") == 0,
		"Argparse parse -shello should set string to 'hello', got '%s'!",
		str_saver ? str_saver : "(null)");

	/* Test short option with negative number without '=' */
	obj = test_argparse_init_obj();
	obj->args[0].name_long = "--test-long";
	obj->args[0].name_short = "-t";
	obj->args[0].val_saver = (void *)&val_saver;
	obj->args[0].val_set = NULL;
	obj->args[0].value_required = RTE_ARGPARSE_VALUE_REQUIRED;
	obj->args[0].value_type = RTE_ARGPARSE_VALUE_TYPE_INT;
	obj->args[1].name_long = NULL;
	val_saver = 0;
	argv[0] = test_strdup(obj->prog_name);
	argv[1] = test_strdup("-t-42");
	ret = rte_argparse_parse(obj, 2, argv);
	TEST_ASSERT(ret == 2, "Argparse parse with -t-42 expect success, got %d!", ret);
	TEST_ASSERT(val_saver == -42, "Argparse parse -t-42 should set value to -42, got %d!",
			val_saver);

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
		TEST_CASE(test_argparse_invalid_repeated_option),
		TEST_CASE(test_argparse_opt_autosave_parse_int_of_no_val),
		TEST_CASE(test_argparse_opt_autosave_parse_int_of_required_val),
		TEST_CASE(test_argparse_opt_autosave_parse_int_of_optional_val),
		TEST_CASE(test_argparse_opt_parse_corelist_of_required_val),
		TEST_CASE(test_argparse_opt_callback_parse_int_of_no_val),
		TEST_CASE(test_argparse_opt_callback_parse_int_of_required_val),
		TEST_CASE(test_argparse_opt_callback_parse_int_of_optional_val),
		TEST_CASE(test_argparse_pos_autosave_parse_int),
		TEST_CASE(test_argparse_pos_callback_parse_int),
		TEST_CASE(test_argparse_parse_type),
		TEST_CASE(test_argparse_ignore_non_flag_args_disabled),
		TEST_CASE(test_argparse_ignore_non_flag_args_basic),
		TEST_CASE(test_argparse_ignore_non_flag_args_with_values),
		TEST_CASE(test_argparse_ignore_non_flag_args_complex_order),
		TEST_CASE(test_argparse_ignore_non_flag_args_only_non_flags),
		TEST_CASE(test_argparse_ignore_non_flag_args_no_non_flags),
		TEST_CASE(test_argparse_ignore_non_flag_args_with_double_dash),
		TEST_CASE(test_argparse_ignore_non_flag_args_leading_non_flags),
		TEST_CASE(test_argparse_ignore_non_flag_args_trailing_non_flags),
		TEST_CASE(test_argparse_ignore_non_flag_args_with_positional),
		TEST_CASE(test_argparse_ignore_non_flag_args_short_and_long),
		TEST_CASE(test_argparse_short_opt_value_without_equal),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_argparse(void)
{
	return unit_test_suite_runner(&argparse_test_suite);
}

REGISTER_FAST_TEST(argparse_autotest, true, true, test_argparse);
