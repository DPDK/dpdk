/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_TELEMETRY_H_
#define _RTE_TELEMETRY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_compat.h>
#include <rte_common.h>

/** Maximum length for string used in object. */
#define RTE_TEL_MAX_STRING_LEN 128
/** Maximum length of string. */
#define RTE_TEL_MAX_SINGLE_STRING_LEN 8192
/** Maximum number of dictionary entries. */
#define RTE_TEL_MAX_DICT_ENTRIES 256
/** Maximum number of array entries. */
#define RTE_TEL_MAX_ARRAY_ENTRIES 512

/**
 * @file
 *
 * RTE Telemetry.
 *
 * The telemetry library provides a method to retrieve statistics from
 * DPDK by sending a request message over a socket. DPDK will send
 * a JSON encoded response containing telemetry data.
 ***/

/** opaque structure used internally for managing data from callbacks */
struct rte_tel_data;

/**
 * The types of data that can be managed in arrays or dicts.
 * For arrays, this must be specified at creation time, while for
 * dicts this is specified implicitly each time an element is added
 * via calling a type-specific function.
 */
enum rte_tel_value_type {
	RTE_TEL_STRING_VAL, /** a string value */
	RTE_TEL_INT_VAL,    /** a signed 64-bit int value */
	RTE_TEL_UINT_VAL,  /** an unsigned 64-bit int value */
	RTE_TEL_CONTAINER, /** a container struct */
};

#define RTE_TEL_U64_VAL RTE_TEL_UINT_VAL

/**
 * Start an array of the specified type for returning from a callback
 *
 * @param d
 *   The data structure passed to the callback
 * @param type
 *   The type of the array of data
 * @return
 *   0 on success, negative errno on error
 */
int
rte_tel_data_start_array(struct rte_tel_data *d, enum rte_tel_value_type type);

/**
 * Start a dictionary of values for returning from a callback
 *
 * Dictionaries consist of key-values pairs to be returned, where the keys,
 * or names, are strings and the values can be any of the types supported by telemetry.
 * Name strings may only contain alphanumeric characters as well as '_' or '/'
 *
 * @param d
 *   The data structure passed to the callback
 * @return
 *   0 on success, negative errno on error
 */
int
rte_tel_data_start_dict(struct rte_tel_data *d);

/**
 * Set a string for returning from a callback
 *
 * @param d
 *   The data structure passed to the callback
 * @param str
 *   The string to be returned in the data structure
 * @return
 *   0 on success, negative errno on error, E2BIG on string truncation
 */
int
rte_tel_data_string(struct rte_tel_data *d, const char *str);

/**
 * Add a string to an array.
 * The array must have been started by rte_tel_data_start_array() with
 * RTE_TEL_STRING_VAL as the type parameter.
 *
 * @param d
 *   The data structure passed to the callback
 * @param str
 *   The string to be returned in the array
 * @return
 *   0 on success, negative errno on error, E2BIG on string truncation
 */
int
rte_tel_data_add_array_string(struct rte_tel_data *d, const char *str);

/**
 * Add an int to an array.
 * The array must have been started by rte_tel_data_start_array() with
 * RTE_TEL_INT_VAL as the type parameter.
 *
 * @param d
 *   The data structure passed to the callback
 * @param x
 *   The number to be returned in the array
 * @return
 *   0 on success, negative errno on error
 */
int
rte_tel_data_add_array_int(struct rte_tel_data *d, int64_t x);

/**
 * Add an unsigned value to an array.
 * The array must have been started by rte_tel_data_start_array() with
 * RTE_TEL_UINT_VAL as the type parameter.
 *
 * @param d
 *   The data structure passed to the callback
 * @param x
 *   The number to be returned in the array
 * @return
 *   0 on success, negative errno on error
 */
int
rte_tel_data_add_array_uint(struct rte_tel_data *d, uint64_t x);

 /**
 * Add a uint64_t to an array.
 * The array must have been started by rte_tel_data_start_array() with
 * RTE_TEL_UINT_VAL as the type parameter.
 *
 * @param d
 *   The data structure passed to the callback
 * @param x
 *   The number to be returned in the array
 * @return
 *   0 on success, negative errno on error
 */
int
rte_tel_data_add_array_u64(struct rte_tel_data *d, uint64_t x)
	__rte_deprecated_msg("use 'rte_tel_data_add_array_uint' instead");

/**
 * Add a container to an array. A container is an existing telemetry data
 * array. The array the container is to be added to must have been started by
 * rte_tel_data_start_array() with RTE_TEL_CONTAINER as the type parameter.
 * The container type must be an array of type uint64_t/int/string.
 *
 * @param d
 *   The data structure passed to the callback
 * @param val
 *   The pointer to the container to be stored in the array.
 * @param keep
 *   Flag to indicate that the container memory should not be automatically
 *   freed by the telemetry library once it has finished with the data.
 *   1 = keep, 0 = free.
 * @return
 *   0 on success, negative errno on error
 */
int
rte_tel_data_add_array_container(struct rte_tel_data *d,
		struct rte_tel_data *val, int keep);

/**
 * Convert an unsigned integer to hexadecimal encoded strings
 * and add this string to an array.
 * The array must have been started by rte_tel_data_start_array()
 * with RTE_TEL_STRING_VAL as the type parameter.
 *
 * @param d
 *   The data structure passed to the callback.
 * @param val
 *   The number to be returned in the array as a hexadecimal encoded strings.
 * @param display_bitwidth
 *   The display bit width of the 'val'. If 'display_bitwidth' is zero, the
 *   value is stored in the array as no-padding zero hexadecimal encoded string,
 *   or the value is stored as padding zero to specified hexadecimal width.
 * @return
 *   0 on success, negative errno on error.
 */
__rte_experimental
int
rte_tel_data_add_array_uint_hex(struct rte_tel_data *d, uint64_t val,
		uint8_t display_bitwidth);

/**
 * Add a string value to a dictionary.
 * The dict must have been started by rte_tel_data_start_dict().
 *
 * @param d
 *   The data structure passed to the callback
 * @param name
 *   The name the value is to be stored under in the dict
 *   Must contain only alphanumeric characters or the symbols: '_' or '/'
 * @param val
 *   The string to be stored in the dict
 * @return
 *   0 on success, negative errno on error, E2BIG on string truncation of
 *   either name or value.
 */
int
rte_tel_data_add_dict_string(struct rte_tel_data *d, const char *name,
		const char *val);

/**
 * Add an int value to a dictionary.
 * The dict must have been started by rte_tel_data_start_dict().
 *
 * @param d
 *   The data structure passed to the callback
 * @param name
 *   The name the value is to be stored under in the dict
 *   Must contain only alphanumeric characters or the symbols: '_' or '/'
 * @param val
 *   The number to be stored in the dict
 * @return
 *   0 on success, negative errno on error, E2BIG on string truncation of name.
 */
int
rte_tel_data_add_dict_int(struct rte_tel_data *d, const char *name, int64_t val);

/**
 * Add an unsigned value to a dictionary.
 * The dict must have been started by rte_tel_data_start_dict().
 *
 * @param d
 *   The data structure passed to the callback
 * @param name
 *   The name the value is to be stored under in the dict
 *   Must contain only alphanumeric characters or the symbols: '_' or '/'
 * @param val
 *   The number to be stored in the dict
 * @return
 *   0 on success, negative errno on error, E2BIG on string truncation of name.
 */
int
rte_tel_data_add_dict_uint(struct rte_tel_data *d,
		const char *name, uint64_t val);

 /**
 * Add a uint64_t value to a dictionary.
 * The dict must have been started by rte_tel_data_start_dict().
 *
 * @param d
 *   The data structure passed to the callback
 * @param name
 *   The name the value is to be stored under in the dict
 *   Must contain only alphanumeric characters or the symbols: '_' or '/'
 * @param val
 *   The number to be stored in the dict
 * @return
 *   0 on success, negative errno on error, E2BIG on string truncation of name.
 */
int
rte_tel_data_add_dict_u64(struct rte_tel_data *d,
		const char *name, uint64_t val)
	__rte_deprecated_msg("use 'rte_tel_data_add_dict_uint' instead");

/**
 * Add a container to a dictionary. A container is an existing telemetry data
 * array. The dict the container is to be added to must have been started by
 * rte_tel_data_start_dict(). The container must be an array of type
 * uint64_t/int/string.
 *
 * @param d
 *   The data structure passed to the callback
 * @param name
 *   The name the value is to be stored under in the dict.
 *   Must contain only alphanumeric characters or the symbols: '_' or '/'
 * @param val
 *   The pointer to the container to be stored in the dict.
 * @param keep
 *   Flag to indicate that the container memory should not be automatically
 *   freed by the telemetry library once it has finished with the data.
 *   1 = keep, 0 = free.
 * @return
 *   0 on success, negative errno on error
 */
int
rte_tel_data_add_dict_container(struct rte_tel_data *d, const char *name,
		struct rte_tel_data *val, int keep);

/**
 * Convert an unsigned integer to hexadecimal encoded strings
 * and add this string to an dictionary.
 * The dict must have been started by rte_tel_data_start_dict().
 *
 * @param d
 *   The data structure passed to the callback.
 * @param name
 *   The name of the value is to be stored in the dict.
 *   Must contain only alphanumeric characters or the symbols: '_' or '/'.
 * @param val
 *   The number to be stored in the dict as a hexadecimal encoded strings.
 * @param display_bitwidth
 *   The display bit width of the 'val'. If 'display_bitwidth' is zero, the
 *   value is stored in the array as no-padding zero hexadecimal encoded string,
 *   or the value is stored as padding zero to specified hexadecimal width.
 * @return
 *   0 on success, negative errno on error.
 */
__rte_experimental
int
rte_tel_data_add_dict_uint_hex(struct rte_tel_data *d, const char *name,
		uint64_t val, uint8_t display_bitwidth);

/**
 * This telemetry callback is used when registering a telemetry command.
 * It handles getting and formatting information to be returned to telemetry
 * when requested.
 *
 * @param cmd
 * The cmd that was requested by the client.
 * @param params
 * Contains data required by the callback function.
 * @param info
 * The information to be returned to the caller.
 *
 * @return
 * Length of buffer used on success.
 * @return
 * Negative integer on error.
 */
typedef int (*telemetry_cb)(const char *cmd, const char *params,
		struct rte_tel_data *info);

/**
 * Used for handling data received over a telemetry socket.
 *
 * @param sock_id
 * ID for the socket to be used by the handler.
 *
 * @return
 * Void.
 */
typedef void * (*handler)(void *sock_id);

/**
 * Used when registering a command and callback function with telemetry.
 *
 * @param cmd
 * The command to register with telemetry.
 * @param fn
 * Callback function to be called when the command is requested.
 * @param help
 * Help text for the command.
 *
 * @return
 *  0 on success.
 * @return
 *  -EINVAL for invalid parameters failure.
 *  @return
 *  -ENOMEM for mem allocation failure.
 */
int
rte_telemetry_register_cmd(const char *cmd, telemetry_cb fn, const char *help);


/**
 * Get a pointer to a container with memory allocated. The container is to be
 * used embedded within an existing telemetry dict/array.
 *
 * @return
 *  Pointer to a container.
 */
struct rte_tel_data *
rte_tel_data_alloc(void);

/**
 * @internal
 * Free a container that has memory allocated.
 *
 * @param data
 *  Pointer to container.
 *  If data is NULL, no operation is performed.
 */
void
rte_tel_data_free(struct rte_tel_data *data);

#ifdef __cplusplus
}
#endif

#endif
