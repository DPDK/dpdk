/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <eal_export.h>
#include <rte_log.h>

#include "rte_argparse.h"

RTE_LOG_REGISTER_DEFAULT(rte_argparse_logtype, INFO);
#define RTE_LOGTYPE_ARGPARSE rte_argparse_logtype
#define ARGPARSE_LOG(level, ...) \
	RTE_LOG_LINE(level, ARGPARSE, "" __VA_ARGS__)

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

static inline bool
is_valid_has_value_field(const struct rte_argparse_arg *arg)
{
	switch (arg->value_required) {
	case RTE_ARGPARSE_VALUE_NONE:
	case RTE_ARGPARSE_VALUE_OPTIONAL:
	case RTE_ARGPARSE_VALUE_REQUIRED:
		return true;
	/* omit default case so compiler warns on any missing enum values */
	}
	return false;
}

static inline bool
is_valid_value_type_field(const struct rte_argparse_arg *arg)
{
	switch (arg->value_type) {
	case RTE_ARGPARSE_VALUE_TYPE_NONE:
	case RTE_ARGPARSE_VALUE_TYPE_INT:
	case RTE_ARGPARSE_VALUE_TYPE_U8:
	case RTE_ARGPARSE_VALUE_TYPE_U16:
	case RTE_ARGPARSE_VALUE_TYPE_U32:
	case RTE_ARGPARSE_VALUE_TYPE_U64:
	case RTE_ARGPARSE_VALUE_TYPE_STR:
	case RTE_ARGPARSE_VALUE_TYPE_BOOL:
		return true;
	/* omit default case so compiler warns on any missing enum values */
	}
	return false;
}


static inline bool
arg_attr_flag_multi(const struct rte_argparse_arg *arg)
{
	return (arg->flags & RTE_ARGPARSE_FLAG_SUPPORT_MULTI) != 0;
}

static inline uint64_t
arg_attr_unused_bits(const struct rte_argparse_arg *arg)
{
#define USED_BIT_MASK	(RTE_ARGPARSE_FLAG_SUPPORT_MULTI)
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
			ARGPARSE_LOG(ERR, "optional long name %s doesn't start with '--'",
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
		ARGPARSE_LOG(ERR, "argument %s doesn't have help info!", arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_has_val(const struct rte_argparse_arg *arg)
{
	if (!is_valid_has_value_field(arg)) {
		ARGPARSE_LOG(ERR, "argument %s has invalid value field!", arg->name_long);
		return -EINVAL;
	}
	if (is_arg_positional(arg)) {
		if (arg->value_required == RTE_ARGPARSE_VALUE_REQUIRED)
			return 0;
		ARGPARSE_LOG(ERR, "argument %s is positional, must config required-val!",
			     arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_saver(const struct rte_argparse *obj, uint32_t index)
{
	const struct rte_argparse_arg *arg = &obj->args[index];

	if (arg->val_saver == NULL) {
		if (arg->value_type != RTE_ARGPARSE_VALUE_TYPE_NONE) {
			ARGPARSE_LOG(ERR, "argument %s parsed by callback, value-type should not be set!",
				     arg->name_long);
			return -EINVAL;
		}
		if (obj->callback == NULL) {
			ARGPARSE_LOG(ERR, "argument %s parsed by callback, but callback is NULL!",
				     arg->name_long);
			return -EINVAL;
		}
		return 0;
	}

	/* check value_type field */
	if (!is_valid_value_type_field(arg)) {
		ARGPARSE_LOG(ERR, "argument %s has invalid value-type field!", arg->name_long);
		return -EINVAL;
	}
	if (arg->value_type == RTE_ARGPARSE_VALUE_TYPE_NONE) {
		ARGPARSE_LOG(ERR, "missing value-type for argument %s!", arg->name_long);
		return -EINVAL;
	}

	if (arg->value_required == RTE_ARGPARSE_VALUE_REQUIRED && arg->val_set != NULL) {
		ARGPARSE_LOG(ERR, "argument %s has required value, value-set should be NULL!",
			     arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
verify_arg_flags(const struct rte_argparse *obj, uint32_t index)
{
	const struct rte_argparse_arg *arg = &obj->args[index];
	uint64_t unused_bits = arg_attr_unused_bits(arg);

	if (unused_bits != 0) {
		ARGPARSE_LOG(ERR, "argument %s flags unused bits should not be set!",
			     arg->name_long);
		return -EINVAL;
	}

	if (!(arg->flags & RTE_ARGPARSE_FLAG_SUPPORT_MULTI))
		return 0;

	if (is_arg_positional(arg)) {
		ARGPARSE_LOG(ERR, "argument %s is positional, don't support multiple times!",
			     arg->name_long);
		return -EINVAL;
	}

	if (arg->val_saver != NULL) {
		ARGPARSE_LOG(ERR, "argument %s supports multiple times, should use callback to parse!",
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

static uint32_t
calc_position_count(const struct rte_argparse *obj)
{
	const struct rte_argparse_arg *arg;
	uint32_t count = 0;
	uint32_t i;

	for (i = 0; /* NULL */; i++) {
		arg = &obj->args[i];
		if (obj->args[i].name_long == NULL)
			break;
		if (is_arg_positional(arg))
			count++;
	}

	return count;
}

static struct rte_argparse_arg *
find_position_arg(struct rte_argparse *obj, uint32_t index)
{
	struct rte_argparse_arg *arg;
	uint32_t count = 0;
	uint32_t i;

	for (i = 0; /* NULL */; i++) {
		arg = &obj->args[i];
		if (arg->name_long == NULL)
			break;
		if (!is_arg_positional(arg))
			continue;
		count++;
		if (count == index)
			return arg;
	}

	return NULL;
}

static bool
is_arg_match(struct rte_argparse_arg *arg, const char *curr_argv, uint32_t len)
{
	if (strlen(arg->name_long) == len && strncmp(arg->name_long, curr_argv, len) == 0)
		return true;

	if (arg->name_short == NULL)
		return false;

	if (strlen(arg->name_short) == len && strncmp(arg->name_short, curr_argv, len) == 0)
		return true;

	return false;
}

static struct rte_argparse_arg *
find_option_arg(struct rte_argparse *obj, const char *curr_argv, const char *has_equal,
		const char **arg_name)
{
	uint32_t len = strlen(curr_argv) - (has_equal != NULL ? strlen(has_equal) : 0);
	struct rte_argparse_arg *arg;
	uint32_t i;
	bool match;

	for (i = 0; /* nothing */; i++) {
		arg = &obj->args[i];
		if (arg->name_long == NULL)
			break;
		match = is_arg_match(arg, curr_argv, len);
		if (match) {
			/* Obtains the exact matching name (long or short). */
			*arg_name = len > 2 ? arg->name_long : arg->name_short;
			return arg;
		}
	}

	return NULL;
}

static int
parse_arg_int(struct rte_argparse_arg *arg, const char *value)
{
	char *s = NULL;

	if (value == NULL) {
		*(int *)arg->val_saver = (int)(intptr_t)arg->val_set;
		return 0;
	}

	errno = 0;
	*(int *)arg->val_saver = strtol(value, &s, 0);
	if (errno == ERANGE) {
		ARGPARSE_LOG(ERR, "argument %s numerical out of range!", arg->name_long);
		return -EINVAL;
	}

	if (s[0] != '\0') {
		ARGPARSE_LOG(ERR, "argument %s expect an integer value!", arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
parse_arg_u8(struct rte_argparse_arg *arg, const char *value)
{
	unsigned long val;
	char *s = NULL;

	if (value == NULL) {
		*(uint8_t *)arg->val_saver = (uint8_t)(intptr_t)arg->val_set;
		return 0;
	}

	errno = 0;
	val = strtoul(value, &s, 0);
	if (errno == ERANGE || val > UINT8_MAX) {
		ARGPARSE_LOG(ERR, "argument %s numerical out of range!", arg->name_long);
		return -EINVAL;
	}

	if (s[0] != '\0') {
		ARGPARSE_LOG(ERR, "argument %s expect an uint8 value!", arg->name_long);
		return -EINVAL;
	}

	*(uint8_t *)arg->val_saver = val;

	return 0;
}

static int
parse_arg_u16(struct rte_argparse_arg *arg, const char *value)
{
	unsigned long val;
	char *s = NULL;

	if (value == NULL) {
		*(uint16_t *)arg->val_saver = (uint16_t)(intptr_t)arg->val_set;
		return 0;
	}

	errno = 0;
	val = strtoul(value, &s, 0);
	if (errno == ERANGE || val > UINT16_MAX) {
		ARGPARSE_LOG(ERR, "argument %s numerical out of range!", arg->name_long);
		return -EINVAL;
	}

	if (s[0] != '\0') {
		ARGPARSE_LOG(ERR, "argument %s expect an uint16 value!", arg->name_long);
		return -EINVAL;
	}

	*(uint16_t *)arg->val_saver = val;

	return 0;
}

static int
parse_arg_u32(struct rte_argparse_arg *arg, const char *value)
{
	unsigned long val;
	char *s = NULL;

	if (value == NULL) {
		*(uint32_t *)arg->val_saver = (uint32_t)(intptr_t)arg->val_set;
		return 0;
	}

	errno = 0;
	val = strtoul(value, &s, 0);
	if (errno == ERANGE || val > UINT32_MAX) {
		ARGPARSE_LOG(ERR, "argument %s numerical out of range!", arg->name_long);
		return -EINVAL;
	}

	if (s[0] != '\0') {
		ARGPARSE_LOG(ERR, "argument %s expect an uint32 value!", arg->name_long);
		return -EINVAL;
	}

	*(uint32_t *)arg->val_saver = val;

	return 0;
}

static int
parse_arg_u64(struct rte_argparse_arg *arg, const char *value)
{
	unsigned long val;
	char *s = NULL;

	if (value == NULL) {
		*(uint64_t *)arg->val_saver = (uint64_t)(intptr_t)arg->val_set;
		return 0;
	}

	errno = 0;
	val = strtoull(value, &s, 0);
	if (errno == ERANGE) {
		ARGPARSE_LOG(ERR, "argument %s numerical out of range!", arg->name_long);
		return -EINVAL;
	}

	if (s[0] != '\0') {
		ARGPARSE_LOG(ERR, "argument %s expect an uint64 value!", arg->name_long);
		return -EINVAL;
	}

	*(uint64_t *)arg->val_saver = val;

	return 0;
}

static int
parse_arg_str(struct rte_argparse_arg *arg, const char *value)
{
	if (value == NULL) {
		*(char **)arg->val_saver = arg->val_set;
		return 0;
	}
	*(const char **)arg->val_saver = value;

	return 0;
}

static int
parse_arg_bool(struct rte_argparse_arg *arg, const char *value)
{
	if (value == NULL) {
		*(bool *)arg->val_saver = (arg->val_set != NULL);
		return 0;
	}

	if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
		*(bool *)arg->val_saver = true;
	else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
		*(bool *)arg->val_saver = false;
	else {
		ARGPARSE_LOG(ERR, "argument %s expects a boolean (true/false, 0/1) value!",
			arg->name_long);
		return -EINVAL;
	}

	return 0;
}

static int
parse_arg_autosave(struct rte_argparse_arg *arg, const char *value)
{
	switch (arg->value_type) {
	case RTE_ARGPARSE_VALUE_TYPE_NONE:
		ARGPARSE_LOG(ERR, "argument %s doesn't specify a value-type!", arg->name_long);
		return -EINVAL;
	case RTE_ARGPARSE_VALUE_TYPE_INT:
		return parse_arg_int(arg, value);
	case RTE_ARGPARSE_VALUE_TYPE_U8:
		return parse_arg_u8(arg, value);
	case RTE_ARGPARSE_VALUE_TYPE_U16:
		return parse_arg_u16(arg, value);
	case RTE_ARGPARSE_VALUE_TYPE_U32:
		return parse_arg_u32(arg, value);
	case RTE_ARGPARSE_VALUE_TYPE_U64:
		return parse_arg_u64(arg, value);
	case RTE_ARGPARSE_VALUE_TYPE_STR:
		return parse_arg_str(arg, value);
	case RTE_ARGPARSE_VALUE_TYPE_BOOL:
		return parse_arg_bool(arg, value);
	/* omit default case so compiler warns on missing enum values */
	}
	return -EINVAL;
}

/* arg_parse indicates the name entered by the user, which can be long-name or short-name. */
static int
parse_arg_val(struct rte_argparse *obj, const char *arg_name,
	      struct rte_argparse_arg *arg, char *value)
{
	int ret;

	if (arg->val_saver == NULL)
		ret = obj->callback((uint32_t)(uintptr_t)arg->val_set, value, obj->opaque);
	else
		ret = parse_arg_autosave(arg, value);
	if (ret != 0) {
		ARGPARSE_LOG(ERR, "argument %s parse value fail!", arg_name);
		return ret;
	}

	return 0;
}

static bool
is_help(const char *curr_argv)
{
	return strcmp(curr_argv, "-h") == 0 || strcmp(curr_argv, "--help") == 0;
}

static int
parse_args(struct rte_argparse *obj, int argc, char **argv, bool *show_help)
{
	uint32_t position_count = calc_position_count(obj);
	struct rte_argparse_arg *arg;
	uint32_t position_index = 0;
	const char *arg_name;
	char *curr_argv;
	char *has_equal;
	char *value;
	int ret;
	int i;

	for (i = 1; i < argc; i++) {
		curr_argv = argv[i];

		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}

		if (curr_argv[0] != '-') {
			/* process positional parameters. */
			position_index++;
			if (position_index > position_count) {
				ARGPARSE_LOG(ERR, "too many positional arguments %s!", curr_argv);
				return -EINVAL;
			}
			arg = find_position_arg(obj, position_index);
			ret = parse_arg_val(obj, arg->name_long, arg, curr_argv);
			if (ret != 0)
				return ret;
			continue;
		}

		/* process optional parameters. */
		if (is_help(curr_argv)) {
			*show_help = true;
			continue;
		}

		has_equal = strchr(curr_argv, '=');
		arg_name = NULL;
		arg = find_option_arg(obj, curr_argv, has_equal, &arg_name);
		if (arg == NULL || arg_name == NULL) {
			ARGPARSE_LOG(ERR, "unknown argument %s!", curr_argv);
			return -EINVAL;
		}

		if ((arg->flags & ARG_ATTR_FLAG_PARSED_MASK) && !arg_attr_flag_multi(arg)) {
			ARGPARSE_LOG(ERR, "argument %s should not occur multiple times!", arg_name);
			return -EINVAL;
		}

		value = (has_equal != NULL ? has_equal + 1 : NULL);
		if (arg->value_required == RTE_ARGPARSE_VALUE_NONE) {
			if (value != NULL) {
				ARGPARSE_LOG(ERR, "argument %s should not take value!", arg_name);
				return -EINVAL;
			}
		} else if (arg->value_required == RTE_ARGPARSE_VALUE_REQUIRED) {
			if (value == NULL) {
				if (i >= argc - 1) {
					ARGPARSE_LOG(ERR, "argument %s doesn't have value!",
							arg_name);
					return -EINVAL;
				}
				/* Set value and make i move next. */
				value = argv[++i];
			}
		} else {
			/* Do nothing, because it's optional value, only support arg=val or arg. */
		}

		ret = parse_arg_val(obj, arg_name, arg, value);
		if (ret != 0)
			return ret;

		/* This argument parsed success! then mark it parsed. */
		arg->flags |= ARG_ATTR_FLAG_PARSED_MASK;
	}

	return i;
}

static uint32_t
calc_help_align(const struct rte_argparse *obj)
{
	const struct rte_argparse_arg *arg;
	uint32_t width = 12; /* Default "-h, --help  " len. */
	uint32_t len;
	uint32_t i;

	for (i = 0; /* NULL */; i++) {
		arg = &obj->args[i];
		if (arg->name_long == NULL)
			break;
		len = strlen(arg->name_long);
		if (is_arg_optional(arg) && arg->name_short != NULL) {
			len += strlen(", ");
			len += strlen(arg->name_short);
		}
		width = RTE_MAX(width, 1 + len + 2); /* start with 1 & end with 2 space. */
	}

	return width;
}

static void
show_oneline_help(const struct rte_argparse_arg *arg, uint32_t width)
{
	uint32_t len = 0;
	uint32_t i;

	if (arg->name_short != NULL)
		len = printf(" %s,", arg->name_short);
	len += printf(" %s", arg->name_long);

	for (i = len; i < width; i++)
		printf(" ");

	printf("%s\n", arg->help);
}

static void
show_args_pos_help(const struct rte_argparse *obj, uint32_t align)
{
	uint32_t position_count = calc_position_count(obj);
	const struct rte_argparse_arg *arg;
	uint32_t i;

	if (position_count == 0)
		return;

	printf("\npositional arguments:\n");
	for (i = 0; /* NULL */; i++) {
		arg = &obj->args[i];
		if (arg->name_long == NULL)
			break;
		if (!is_arg_positional(arg))
			continue;
		show_oneline_help(arg, align);
	}
}

static void
show_args_opt_help(const struct rte_argparse *obj, uint32_t align)
{
	static const struct rte_argparse_arg help = {
		.name_long = "--help",
		.name_short = "-h",
		.help = "show this help message and exit.",
	};
	const struct rte_argparse_arg *arg;
	uint32_t i;

	printf("\noptions:\n");
	show_oneline_help(&help, align);
	for (i = 0; /* NULL */; i++) {
		arg = &obj->args[i];
		if (arg->name_long == NULL)
			break;
		if (!is_arg_optional(arg))
			continue;
		show_oneline_help(arg, align);
	}
}

static void
show_args_help(const struct rte_argparse *obj)
{
	uint32_t align = calc_help_align(obj);

	printf("usage: %s %s\n", obj->prog_name, obj->usage);
	if (obj->descriptor != NULL)
		printf("\ndescriptor: %s\n", obj->descriptor);

	show_args_pos_help(obj, align);
	show_args_opt_help(obj, align);

	if (obj->epilog != NULL)
		printf("\n%s\n", obj->epilog);
	else
		printf("\n");
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_argparse_parse, 24.03)
int
rte_argparse_parse(struct rte_argparse *obj, int argc, char **argv)
{
	bool show_help = false;
	int ret;

	ret = verify_argparse(obj);
	if (ret != 0)
		goto error;

	ret = parse_args(obj, argc, argv, &show_help);
	if (ret < 0)
		goto error;

	if (show_help) {
		show_args_help(obj);
		exit(0);
	}

	return ret;

error:
	if (obj->exit_on_error)
		exit(ret);
	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_argparse_parse_type, 24.03)
int
rte_argparse_parse_type(const char *str, enum rte_argparse_value_type val_type, void *val)
{
	struct rte_argparse_arg arg = {
		.name_long = str,
		.name_short = NULL,
		.val_saver = val,
		.val_set = NULL,
		.value_type = val_type,
	};
	if (val_type == RTE_ARGPARSE_VALUE_TYPE_NONE) {
		ARGPARSE_LOG(ERR, "argument %s doesn't have value-type!", str);
		return -EINVAL;
	}
	return parse_arg_autosave(&arg, str);
}
