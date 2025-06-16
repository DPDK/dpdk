/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#ifndef RTE_ARGPARSE_H
#define RTE_ARGPARSE_H

/**
 * @file rte_argparse.h
 *
 * Argument parsing API.
 *
 * The argument parsing API makes it easy to write user-friendly command-line
 * program. The program defines what arguments it requires,
 * and the API will parse those arguments from [argc, argv].
 *
 * The API provides the following functions:
 * 1) Support parsing optional argument (which could take with no-value,
 *    required-value and optional-value.
 * 2) Support parsing positional argument (which must take with required-value).
 * 3) Support automatic generate usage information.
 * 4) Support issue errors when provided with invalid arguments.
 *
 * There are two ways to parse arguments:
 * 1) AutoSave: for which known value types, the way can be used.
 * 2) Callback: will invoke user callback to parse.
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_bitops.h>
#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * enum defining whether an argument takes a value or not.
 */
enum rte_argparse_value_required {
	/** The argument takes no value. */
	RTE_ARGPARSE_VALUE_NONE,
	/** The argument must have a value. */
	RTE_ARGPARSE_VALUE_REQUIRED,
	/** The argument has optional value. */
	RTE_ARGPARSE_VALUE_OPTIONAL,
};

/** enum defining the type of the argument, integer, boolean or just string */
enum rte_argparse_value_type {
	/** Argument takes no value, or value type not specified.
	 * Should be used when argument is to be handled via callback.
	 */
	RTE_ARGPARSE_VALUE_TYPE_NONE = 0,
	/** The argument's value is int type. */
	RTE_ARGPARSE_VALUE_TYPE_INT,
	/** The argument's value is uint8 type. */
	RTE_ARGPARSE_VALUE_TYPE_U8,
	/** The argument's value is uint16 type. */
	RTE_ARGPARSE_VALUE_TYPE_U16,
	/** The argument's value is uint32 type. */
	RTE_ARGPARSE_VALUE_TYPE_U32,
	/** The argument's value is uint64 type. */
	RTE_ARGPARSE_VALUE_TYPE_U64,
	/** The argument's value is string type. */
	RTE_ARGPARSE_VALUE_TYPE_STR,
	/** The argument's value is boolean flag type. */
	RTE_ARGPARSE_VALUE_TYPE_BOOL,
};

/** Additional flags which may be specified for each argument */
enum rte_argparse_arg_flags {
	/** argument may be specified multiple times on the commandline */
	RTE_ARGPARSE_FLAG_SUPPORT_MULTI = RTE_BIT32(0),
};

/**
 * A structure used to hold argument's configuration.
 */
struct rte_argparse_arg {
	/**
	 * Long name of the argument:
	 * 1) If the argument is optional, it must start with ``--``.
	 * 2) If the argument is positional, it must not start with ``-``.
	 * 3) Other case will be considered as error.
	 */
	const char *name_long;
	/**
	 * Short name of the argument:
	 * 1) This field could be set only when name_long is optional, and
	 *    must start with a hyphen (-) followed by an English letter.
	 * 2) Other case it should be set NULL.
	 */
	const char *name_short;

	/** Help information of the argument, must not be NULL. */
	const char *help;

	/**
	 * Saver for the argument's value.
	 * 1) If this field is NULL, the callback is used for parsing argument.
	 * 2) If this field is not NULL, the argument's value will be automatically saved.
	 */
	void *val_saver;
	/**
	 * If val_saver is NULL, this filed (cast as (uint32_t)val_set) will be
	 * used as the first parameter to invoke callback.
	 *
	 * If val_saver is not NULL, then:
	 * 1) If argument has no value, *val_saver will be set to val_set.
	 * 2) If argument has optional value but doesn't take value this
	 *    time, *val_saver will be set to val_set.
	 * 3) Other case it should be set NULL.
	 */
	void *val_set;

	/** Specify if the argument takes a value, @see enum rte_argparse_value_required. */
	enum rte_argparse_value_required value_required;
	/** The type of the argument, @see enum rte_argparse_value_type. */
	enum rte_argparse_value_type value_type;

	/** any additional flags for this argument */
	uint32_t flags;
};

/**
 * Callback prototype used by parsing specified arguments.
 *
 * @param index
 *   The argument's index, coming from argument's val_set field.
 * @param value
 *   The value corresponding to the argument, it may be NULL (e.g. the
 *   argument has no value, or the argument has optional value but doesn't
 *   provided value).
 * @param opaque
 *   An opaque pointer coming from the caller.
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
typedef int (*rte_arg_parser_t)(uint32_t index, const char *value, void *opaque);

/**
 * A structure used to hold argparse's configuration.
 */
struct rte_argparse {
	/** Program name. Must not be NULL. */
	const char *prog_name;
	/** How to use the program. Must not be NULL. */
	const char *usage;
	/** Explain what the program does. Could be NULL. */
	const char *descriptor;
	/** Text at the bottom of help. Could be NULL. */
	const char *epilog;
	/** Whether exit when error. */
	bool exit_on_error;
	/** User callback for parsing arguments. */
	rte_arg_parser_t callback;
	/** Opaque which used to invoke callback. */
	void *opaque;
	/** Reserved field used for future extension. */
	void *reserved[16];
	/** Arguments configuration. Must ended with ARGPARSE_ARG_END(). */
	struct rte_argparse_arg args[];
};

#define ARGPARSE_ARG_END() { NULL }

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Parse parameters passed until all are processed,
 * or until a "--" parameter is encountered.
 *
 * @param obj
 *   Parser object.
 * @param argc
 *   Parameters count.
 * @param argv
 *   Array of parameters points.
 *
 * @return
 *   number of arguments parsed (>= 0) on success.
 *   Otherwise negative error code is returned.
 */
__rte_experimental
int rte_argparse_parse(const struct rte_argparse *obj, int argc, char **argv);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Parse the value from the input string based on the value type.
 *
 * @param str
 *   Input string.
 * @param val_type
 *   The value type, @see rte_argparse_value_type.
 * @param val
 *   Saver for the value.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
__rte_experimental
int rte_argparse_parse_type(const char *str, enum rte_argparse_value_type val_type, void *val);

#ifdef __cplusplus
}
#endif

#endif /* RTE_ARGPARSE_H */
