/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_TRACE_POINT_H_
#define _RTE_TRACE_POINT_H_

/**
 * @file
 *
 * RTE Tracepoint API
 *
 * This file provides the tracepoint API to RTE applications.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_compat.h>

/** The tracepoint object. */
typedef uint64_t rte_trace_point_t;

/** Macro to define the tracepoint. */
#define RTE_TRACE_POINT_DEFINE(tp) \
rte_trace_point_t __attribute__((section("__rte_trace_point"))) __##tp

/**
 * Macro to define the tracepoint arguments in RTE_TRACE_POINT macro.

 * @see RTE_TRACE_POINT, RTE_TRACE_POINT_FP
 */
#define RTE_TRACE_POINT_ARGS

/** @internal Helper macro to support RTE_TRACE_POINT and RTE_TRACE_POINT_FP */
#define __RTE_TRACE_POINT(_mode, _tp, _args, ...) \
extern rte_trace_point_t __##_tp; \
static __rte_always_inline void \
_tp _args \
{ \
	__rte_trace_point_emit_header_##_mode(&__##_tp); \
	__VA_ARGS__ \
}

/**
 * Create a tracepoint.
 *
 * A tracepoint is defined by specifying:
 * - its input arguments: they are the C function style parameters to define
 *   the arguments of tracepoint function. These input arguments are embedded
 *   using the RTE_TRACE_POINT_ARGS macro.
 * - its output event fields: they are the sources of event fields that form
 *   the payload of any event that the execution of the tracepoint macro emits
 *   for this particular tracepoint. The application uses
 *   rte_trace_point_emit_* macros to emit the output event fields.
 *
 * @param tp
 *   Tracepoint object. Before using the tracepoint, an application needs to
 *   define the tracepoint using RTE_TRACE_POINT_DEFINE macro.
 * @param args
 *   C function style input arguments to define the arguments to tracepoint
 *   function.
 * @param ...
 *   Define the payload of trace function. The payload will be formed using
 *   rte_trace_point_emit_* macros. Use ";" delimiter between two payloads.
 *
 * @see RTE_TRACE_POINT_ARGS, RTE_TRACE_POINT_DEFINE, rte_trace_point_emit_*
 */
#define RTE_TRACE_POINT(tp, args, ...) \
	__RTE_TRACE_POINT(generic, tp, args, __VA_ARGS__)

/**
 * Create a tracepoint for fast path.
 *
 * Similar to RTE_TRACE_POINT, except that it is removed at compilation time
 * unless the RTE_ENABLE_TRACE_FP configuration parameter is set.
 *
 * @param tp
 *   Tracepoint object. Before using the tracepoint, an application needs to
 *   define the tracepoint using RTE_TRACE_POINT_DEFINE macro.
 * @param args
 *   C function style input arguments to define the arguments to tracepoint.
 *   function.
 * @param ...
 *   Define the payload of trace function. The payload will be formed using
 *   rte_trace_point_emit_* macros, Use ";" delimiter between two payloads.
 *
 * @see RTE_TRACE_POINT
 */
#define RTE_TRACE_POINT_FP(tp, args, ...) \
	__RTE_TRACE_POINT(fp, tp, args, __VA_ARGS__)

#ifdef __DOXYGEN__

/**
 * Macro to select rte_trace_point_emit_* definition for trace register function
 *
 * rte_trace_point_emit_* emits different definitions for trace function.
 * Application must define RTE_TRACE_POINT_REGISTER_SELECT before including
 * rte_trace_point.h in the C file where RTE_TRACE_POINT_REGISTER used.
 *
 * @see RTE_TRACE_POINT_REGISTER
 */
#define RTE_TRACE_POINT_REGISTER_SELECT

/**
 * Register a tracepoint.
 *
 * @param trace
 *   The tracepoint object created using RTE_TRACE_POINT_DEFINE.
 * @param name
 *   The name of the tracepoint object.
 * @return
 *   - 0: Successfully registered the tracepoint.
 *   - <0: Failure to register the tracepoint.
 *
 * @see RTE_TRACE_POINT_REGISTER_SELECT
 */
#define RTE_TRACE_POINT_REGISTER(trace, name)

/** Tracepoint function payload for uint64_t datatype */
#define rte_trace_point_emit_u64(val)
/** Tracepoint function payload for int64_t datatype */
#define rte_trace_point_emit_i64(val)
/** Tracepoint function payload for uint32_t datatype */
#define rte_trace_point_emit_u32(val)
/** Tracepoint function payload for int32_t datatype */
#define rte_trace_point_emit_i32(val)
/** Tracepoint function payload for uint16_t datatype */
#define rte_trace_point_emit_u16(val)
/** Tracepoint function payload for int16_t datatype */
#define rte_trace_point_emit_i16(val)
/** Tracepoint function payload for uint8_t datatype */
#define rte_trace_point_emit_u8(val)
/** Tracepoint function payload for int8_t datatype */
#define rte_trace_point_emit_i8(val)
/** Tracepoint function payload for int datatype */
#define rte_trace_point_emit_int(val)
/** Tracepoint function payload for long datatype */
#define rte_trace_point_emit_long(val)
/** Tracepoint function payload for float datatype */
#define rte_trace_point_emit_float(val)
/** Tracepoint function payload for double datatype */
#define rte_trace_point_emit_double(val)
/** Tracepoint function payload for pointer datatype */
#define rte_trace_point_emit_ptr(val)
/** Tracepoint function payload for string datatype */
#define rte_trace_point_emit_string(val)

#endif /* __DOXYGEN__ */

/** @internal Macro to define maximum emit length of string datatype. */
#define __RTE_TRACE_EMIT_STRING_LEN_MAX 32
/** @internal Macro to define event header size. */
#define __RTE_TRACE_EVENT_HEADER_SZ sizeof(uint64_t)

/**
 * Enable recording events of the given tracepoint in the trace buffer.
 *
 * @param tp
 *   The tracepoint object to enable.
 * @return
 *   - 0: Success.
 *   - (-ERANGE): Trace object is not registered.
 */
__rte_experimental
int rte_trace_point_enable(rte_trace_point_t *tp);

/**
 * Disable recording events of the given tracepoint in the trace buffer.
 *
 * @param tp
 *   The tracepoint object to disable.
 * @return
 *   - 0: Success.
 *   - (-ERANGE): Trace object is not registered.
 */
__rte_experimental
int rte_trace_point_disable(rte_trace_point_t *tp);

/**
 * Test if recording events from the given tracepoint is enabled.
 *
 * @param tp
 *    The tracepoint object.
 * @return
 *    true if tracepoint is enabled, false otherwise.
 */
__rte_experimental
bool rte_trace_point_is_enabled(rte_trace_point_t *tp);

/**
 * Lookup a tracepoint object from its name.
 *
 * @param name
 *   The name of the tracepoint.
 * @return
 *   The tracepoint object or NULL if not found.
 */
__rte_experimental
rte_trace_point_t *rte_trace_point_lookup(const char *name);

/**
 * @internal
 *
 * Test if the tracepoint fast path compile-time option is enabled.
 *
 * @return
 *   true if tracepoint fast path enabled, false otherwise.
 */
__rte_experimental
static __rte_always_inline bool
__rte_trace_point_fp_is_enabled(void)
{
#ifdef RTE_ENABLE_TRACE_FP
	return true;
#else
	return false;
#endif
}

/**
 * @internal
 *
 * Allocate trace memory buffer per thread.
 *
 */
__rte_experimental
void __rte_trace_mem_per_thread_alloc(void);

/**
 * @internal
 *
 * Helper function to emit field.
 *
 * @param sz
 *   The tracepoint size.
 * @param field
 *   The name of the trace event.
 * @param type
 *   The datatype of the trace event as string.
 * @return
 *   - 0: Success.
 *   - <0: Failure.
 */
__rte_experimental
void __rte_trace_point_emit_field(size_t sz, const char *field,
	const char *type);

/**
 * @internal
 *
 * Helper function to register a dynamic tracepoint.
 * Use RTE_TRACE_POINT_REGISTER macro for tracepoint registration.
 *
 * @param trace
 *   The tracepoint object created using RTE_TRACE_POINT_DEFINE.
 * @param name
 *   The name of the tracepoint object.
 * @param register_fn
 *   Trace registration function.
 * @return
 *   - 0: Successfully registered the tracepoint.
 *   - <0: Failure to register the tracepoint.
 */
__rte_experimental
int __rte_trace_point_register(rte_trace_point_t *trace, const char *name,
	void (*register_fn)(void));

#ifdef RTE_TRACE_POINT_REGISTER_SELECT
#include <rte_trace_point_register.h>
#else
#include <rte_trace_point_provider.h>
#endif

#ifndef __DOXYGEN__

#define rte_trace_point_emit_u64(in) __rte_trace_point_emit(in, uint64_t)
#define rte_trace_point_emit_i64(in) __rte_trace_point_emit(in, int64_t)
#define rte_trace_point_emit_u32(in) __rte_trace_point_emit(in, uint32_t)
#define rte_trace_point_emit_i32(in) __rte_trace_point_emit(in, int32_t)
#define rte_trace_point_emit_u16(in) __rte_trace_point_emit(in, uint16_t)
#define rte_trace_point_emit_i16(in) __rte_trace_point_emit(in, int16_t)
#define rte_trace_point_emit_u8(in) __rte_trace_point_emit(in, uint8_t)
#define rte_trace_point_emit_i8(in) __rte_trace_point_emit(in, int8_t)
#define rte_trace_point_emit_int(in) __rte_trace_point_emit(in, int32_t)
#define rte_trace_point_emit_long(in) __rte_trace_point_emit(in, long)
#define rte_trace_point_emit_float(in) __rte_trace_point_emit(in, float)
#define rte_trace_point_emit_double(in) __rte_trace_point_emit(in, double)
#define rte_trace_point_emit_ptr(in) __rte_trace_point_emit(in, uintptr_t)

#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_TRACE_POINT_H_ */
