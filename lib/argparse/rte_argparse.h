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

/**@{@name Flag definition (in bitmask form) for an argument
 *
 * @note Bits[0~1] represent the argument whether has value,
 * bits[2~9] represent the value type which used when autosave.
 *
 * @see struct rte_argparse_arg::flags
 */
/** The argument has no value. */
#define RTE_ARGPARSE_ARG_NO_VALUE       RTE_SHIFT_VAL64(1, 0)
/** The argument must have a value. */
#define RTE_ARGPARSE_ARG_REQUIRED_VALUE RTE_SHIFT_VAL64(2, 0)
/** The argument has optional value. */
#define RTE_ARGPARSE_ARG_OPTIONAL_VALUE RTE_SHIFT_VAL64(3, 0)
/** The argument's value is int type. */
#define RTE_ARGPARSE_ARG_VALUE_INT      RTE_SHIFT_VAL64(1, 2)
/** The argument's value is uint8 type. */
#define RTE_ARGPARSE_ARG_VALUE_U8       RTE_SHIFT_VAL64(2, 2)
/** The argument's value is uint16 type. */
#define RTE_ARGPARSE_ARG_VALUE_U16      RTE_SHIFT_VAL64(3, 2)
/** The argument's value is uint32 type. */
#define RTE_ARGPARSE_ARG_VALUE_U32      RTE_SHIFT_VAL64(4, 2)
/** The argument's value is uint64 type. */
#define RTE_ARGPARSE_ARG_VALUE_U64      RTE_SHIFT_VAL64(5, 2)
/** The argument's value is string type. */
#define RTE_ARGPARSE_ARG_VALUE_STR      RTE_SHIFT_VAL64(6, 2)
/** The argument's value is boolean flag type. */
#define RTE_ARGPARSE_ARG_VALUE_BOOL     RTE_SHIFT_VAL64(7, 2)
/** Max value type. */
#define RTE_ARGPARSE_ARG_VALUE_MAX      RTE_SHIFT_VAL64(8, 2)
/**
 * Flag for that argument support occur multiple times.
 * This flag can be set only when the argument is optional.
 * When this flag is set, the callback type must be used for parsing.
 */
#define RTE_ARGPARSE_ARG_SUPPORT_MULTI  RTE_BIT64(10)
/** Reserved for this library implementation usage. */
#define RTE_ARGPARSE_ARG_RESERVED_FIELD RTE_GENMASK64(63, 48)
/**@}*/

/** Bitmask used to get the argument whether has value. */
#define RTE_ARGPARSE_HAS_VAL_BITMASK	RTE_GENMASK64(1, 0)
/** Bitmask used to get the argument's value type. */
#define RTE_ARGPARSE_VAL_TYPE_BITMASK	RTE_GENMASK64(9, 2)

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
	 * 1) If the filed is NULL, the callback way is used for parsing
	 *    argument.
	 * 2) If the field is not NULL, the autosave way is used for parsing
	 *    argument.
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

	/** Flag definition (RTE_ARGPARSE_ARG_*) for the argument. */
	uint64_t flags;
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
int rte_argparse_parse(struct rte_argparse *obj, int argc, char **argv);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Parse the value from the input string based on the value type.
 *
 * @param str
 *   Input string.
 * @param val_type
 *   The value type, @see RTE_ARGPARSE_ARG_VALUE_INT or other type.
 * @param val
 *   Saver for the value.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
__rte_experimental
int rte_argparse_parse_type(const char *str, uint64_t val_type, void *val);

#ifdef __cplusplus
}
#endif

#endif /* RTE_ARGPARSE_H */
