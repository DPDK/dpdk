/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_TRACE_POINT_REGISTER_H_
#define _RTE_TRACE_POINT_REGISTER_H_

#ifdef _RTE_TRACE_POINT_H_
#error for registration, include this file first before <rte_trace_point.h>
#endif

#include <rte_per_lcore.h>
#include <rte_trace_point.h>

#ifdef __cplusplus
extern "C" {
#endif

RTE_DECLARE_PER_LCORE(volatile int, trace_point_sz);

#define RTE_TRACE_POINT_ARGS_COUNT_(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,N, ...) \
	N
#define RTE_TRACE_POINT_ARGS_COUNT(...) \
	RTE_TRACE_POINT_ARGS_COUNT_(__VA_ARGS__,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define RTE_TRACE_POINT_ARGS_1(a) __rte_unused a
#define RTE_TRACE_POINT_ARGS_2(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_1(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_3(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_2(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_4(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_3(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_5(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_4(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_6(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_5(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_7(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_6(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_8(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_7(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_9(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_8(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_10(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_9(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_11(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_10(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_12(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_11(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_13(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_12(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_14(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_13(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_15(a, ...) __rte_unused a, RTE_TRACE_POINT_ARGS_14(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS_FUNC(a) RTE_TRACE_POINT_ARGS_ ## a
#define RTE_TRACE_POINT_ARGS_EXPAND(...) __VA_ARGS__
#define RTE_TRACE_POINT_ARGS_(N, ...) \
	RTE_TRACE_POINT_ARGS_EXPAND(RTE_TRACE_POINT_ARGS_FUNC(N))(__VA_ARGS__)
#define RTE_TRACE_POINT_ARGS(...) \
	(RTE_TRACE_POINT_ARGS_(RTE_TRACE_POINT_ARGS_COUNT(0, __VA_ARGS__), __VA_ARGS__))

#define __RTE_TRACE_POINT(_mode, _tp, _args, ...) \
extern rte_trace_point_t __##_tp; \
static __rte_always_inline void _tp _args { } \
static __rte_always_inline void \
_tp ## _register (void) \
{ \
	__rte_trace_point_emit_header_##_mode(&__##_tp); \
	__VA_ARGS__ \
}

#define RTE_TRACE_POINT_REGISTER(trace, name) \
rte_trace_point_t __rte_section("__rte_trace_point") __##trace; \
static const char __##trace##_name[] = RTE_STR(name); \
RTE_INIT(trace##_init) \
{ \
	if (!rte_trace_feature_is_enabled()) \
		return; \
	__rte_trace_point_register(&__##trace, __##trace##_name, \
		trace ## _register); \
}

#define __rte_trace_point_emit_header_generic(t) \
	RTE_PER_LCORE(trace_point_sz) = __RTE_TRACE_EVENT_HEADER_SZ

#define __rte_trace_point_emit_header_fp(t) \
	__rte_trace_point_emit_header_generic(t)

#define __rte_trace_point_emit(name, in, type) \
do { \
	__rte_trace_point_emit_field(sizeof(type), name, RTE_STR(type)); \
} while (0)

#define rte_trace_point_emit_string(in) \
do { \
	__rte_trace_point_emit_field(__RTE_TRACE_EMIT_STRING_LEN_MAX, \
		RTE_STR(in)"[32]", "string_bounded_t"); \
} while (0)

#define rte_trace_point_emit_blob(in, len) \
do { \
	__rte_trace_point_emit_field(sizeof(uint8_t), \
		RTE_STR(in) "_size", \
		RTE_STR(uint8_t)); \
	__rte_trace_point_emit_field(RTE_TRACE_BLOB_LEN_MAX, \
		RTE_STR(in)"[" RTE_STR(RTE_TRACE_BLOB_LEN_MAX)"]", \
		RTE_STR(uint8_t)); \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_TRACE_POINT_REGISTER_H_ */
