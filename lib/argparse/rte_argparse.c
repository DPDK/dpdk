/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <rte_log.h>

#include "rte_argparse.h"

RTE_LOG_REGISTER_DEFAULT(rte_argparse_logtype, INFO);
#define RTE_LOGTYPE_ARGPARSE rte_argparse_logtype
#define ARGPARSE_LOG(level, ...) \
	RTE_LOG_LINE(level, ARGPARSE, "" __VA_ARGS__)

#define ARG_ATTR_HAS_VAL_MASK		RTE_GENMASK64(1, 0)
#define ARG_ATTR_VAL_TYPE_MASK		RTE_GENMASK64(9, 2)
#define ARG_ATTR_SUPPORT_MULTI_MASK	RTE_BIT64(10)
#define ARG_ATTR_FLAG_PARSED_MASK	RTE_BIT64(63)

static inline bool
is_arg_optional(const struct rte_argparse_arg *arg)
{
	return arg->name_long[0] == '-';
}

static inline bool
is_arg_positional(const struct rte_argparse_arg *arg)
{
	return arg->name_long[0] != '-';
}

static inline uint32_t
arg_attr_has_val(const struct rte_argparse_arg *arg)
{
	return RTE_FIELD_GET64(ARG_ATTR_HAS_VAL_MASK, arg->flags);
}

static inline uint32_t
arg_attr_val_type(const struct rte_argparse_arg *arg)
{
	return RTE_FIELD_GET64(ARG_ATTR_VAL_TYPE_MASK, arg->flags);
}

static inline uint32_t
arg_attr_unused_bits(const struct rte_argparse_arg *arg)
{
#define USED_BIT_MASK	(ARG_ATTR_HAS_VAL_MASK | ARG_ATTR_VAL_TYPE_MASK | \
			 ARG_ATTR_SUPPORT_MULTI_MASK)
	return arg->flags & ~USED_BIT_MASK;
}

static int
verify_arg_name(const struct rte_argparse_arg *arg)
{
	if (is_arg_optional(arg)) {
		if (strlen(arg->name_long) <= 3) {
			ARGPARSE_LOG(ERR, "optional long name %s too short!", arg->name_long);
			return -EINVAL;
		}
		if (arg->name_long[1] != '-') {
			ARGPARSE_LOG(ERR, "optional long name %s must only start with '--'",
				     arg->name_long);
			return -EINVAL;
		}
		if (arg->name_long[2] == '-') {
			ARGPARSE_LOG(ERR, "optional long name %s should not start with '---'",
				     arg->name_long);
			return -EINVAL;
		}
	}

	if (arg->name_short == NULL)
		return 0;

	if (!is_arg_optional(arg)) {
		ARGPARSE_LOG(ERR, "short name %s corresponding long name must be optional!",
			     arg->name_short);
		return -EINVAL;
	}

	if (strlen(arg->name_short) != 2 || arg->name_short[0] != '-' ||
		arg->name_short[1] == '-') {
		ARGPARSE_LOG(ERR, "short name %s must start with a hyphen (-) followed by an English letter",
			     arg->name_short);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_help(const struct rte_argparse_arg *arg)
{
	if (arg->help == NULL) {
		ARGPARSE_LOG(ERR, "argument %s must have help info!", arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_has_val(const struct rte_argparse_arg *arg)
{
	uint32_t has_val = arg_attr_has_val(arg);

	if (is_arg_positional(arg)) {
		if (has_val == RTE_ARGPARSE_ARG_REQUIRED_VALUE)
			return 0;
		ARGPARSE_LOG(ERR, "argument %s is positional, should has zero or required-val!",
			     arg->name_long);
		return -EINVAL;
	}

	if (has_val == 0) {
		ARGPARSE_LOG(ERR, "argument %s is optional, has-val config wrong!",
			     arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_saver(const struct rte_argparse *obj, uint32_t index)
{
	uint32_t cmp_max = RTE_FIELD_GET64(ARG_ATTR_VAL_TYPE_MASK, RTE_ARGPARSE_ARG_VALUE_MAX);
	const struct rte_argparse_arg *arg = &obj->args[index];
	uint32_t val_type = arg_attr_val_type(arg);
	uint32_t has_val = arg_attr_has_val(arg);

	if (arg->val_saver == NULL) {
		if (val_type != 0) {
			ARGPARSE_LOG(ERR, "argument %s parse by callback, val-type must be zero!",
				     arg->name_long);
			return -EINVAL;
		}

		if (obj->callback == NULL) {
			ARGPARSE_LOG(ERR, "argument %s parse by callback, but callback is NULL!",
				     arg->name_long);
			return -EINVAL;
		}

		return 0;
	}

	if (val_type == 0 || val_type >= cmp_max) {
		ARGPARSE_LOG(ERR, "argument %s val-type config wrong!", arg->name_long);
		return -EINVAL;
	}

	if (has_val == RTE_ARGPARSE_ARG_REQUIRED_VALUE && arg->val_set != NULL) {
		ARGPARSE_LOG(ERR, "argument %s has required value, val-set should be NULL!",
			     arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_flags(const struct rte_argparse *obj, uint32_t index)
{
	const struct rte_argparse_arg *arg = &obj->args[index];
	uint32_t unused_bits = arg_attr_unused_bits(arg);

	if (unused_bits != 0) {
		ARGPARSE_LOG(ERR, "argument %s flags set wrong!", arg->name_long);
		return -EINVAL;
	}

	if (!(arg->flags & RTE_ARGPARSE_ARG_SUPPORT_MULTI))
		return 0;

	if (is_arg_positional(arg)) {
		ARGPARSE_LOG(ERR, "argument %s is positional, don't support multiple times!",
			     arg->name_long);
		return -EINVAL;
	}

	if (arg->val_saver != NULL) {
		ARGPARSE_LOG(ERR, "argument %s could occur multiple times, should use callback to parse!",
			     arg->name_long);
		return -EINVAL;
	}

	if (obj->callback == NULL) {
		ARGPARSE_LOG(ERR, "argument %s should use callback to parse, but callback is NULL!",
			     arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_repeat(const struct rte_argparse *self, uint32_t index)
{
	const struct rte_argparse_arg *arg = &self->args[index];
	uint32_t i;

	for (i = 0; i < index; i++) {
		if (!strcmp(arg->name_long, self->args[i].name_long)) {
			ARGPARSE_LOG(ERR, "argument %s repeat!", arg->name_long);
			return -EINVAL;
		}
	}

	if (arg->name_short == NULL)
		return 0;

	for (i = 0; i < index; i++) {
		if (self->args[i].name_short == NULL)
			continue;
		if (!strcmp(arg->name_short, self->args[i].name_short)) {
			ARGPARSE_LOG(ERR, "argument %s repeat!", arg->name_short);
			return -EINVAL;
		}
	}

	return 0;
}

static int
verify_argparse_arg(const struct rte_argparse *obj, uint32_t index)
{
	const struct rte_argparse_arg *arg = &obj->args[index];
	int ret;

	ret = verify_arg_name(arg);
	if (ret != 0)
		return ret;

	ret = verify_arg_help(arg);
	if (ret != 0)
		return ret;

	ret = verify_arg_has_val(arg);
	if (ret != 0)
		return ret;

	ret = verify_arg_saver(obj, index);
	if (ret != 0)
		return ret;

	ret = verify_arg_flags(obj, index);
	if (ret != 0)
		return ret;

	ret = verify_arg_repeat(obj, index);
	if (ret != 0)
		return ret;

	return 0;
}

static int
verify_argparse(const struct rte_argparse *obj)
{
	uint32_t idx;
	int ret;

	if (obj->prog_name == NULL) {
		ARGPARSE_LOG(ERR, "program name is NULL!");
		return -EINVAL;
	}

	if (obj->usage == NULL) {
		ARGPARSE_LOG(ERR, "usage is NULL!");
		return -EINVAL;
	}

	for (idx = 0; idx < RTE_DIM(obj->reserved); idx++) {
		if (obj->reserved[idx] != 0) {
			ARGPARSE_LOG(ERR, "reserved field must be zero!");
			return -EINVAL;
		}
	}

	idx = 0;
	while (obj->args[idx].name_long != NULL) {
		ret = verify_argparse_arg(obj, idx);
		if (ret != 0)
			return ret;
		idx++;
	}

	return 0;
}

int
rte_argparse_parse(struct rte_argparse *obj, int argc, char **argv)
{
	int ret;

	(void)argc;
	(void)argv;

	ret = verify_argparse(obj);
	if (ret != 0)
		goto error;

	return 0;

error:
	if (obj->exit_on_error)
		exit(ret);
	return ret;
}
