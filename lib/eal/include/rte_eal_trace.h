/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_EAL_TRACE_H_
#define _RTE_EAL_TRACE_H_

/**
 * @file
 *
 * API for EAL trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

/* Generic */
RTE_TRACE_POINT(
	rte_eal_trace_generic_void,
	RTE_TRACE_POINT_ARGS(void),
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_u64,
	RTE_TRACE_POINT_ARGS(uint64_t in),
	rte_trace_point_emit_u64(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_u32,
	RTE_TRACE_POINT_ARGS(uint32_t in),
	rte_trace_point_emit_u32(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_u16,
	RTE_TRACE_POINT_ARGS(uint16_t in),
	rte_trace_point_emit_u16(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_u8,
	RTE_TRACE_POINT_ARGS(uint8_t in),
	rte_trace_point_emit_u8(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_i64,
	RTE_TRACE_POINT_ARGS(int64_t in),
	rte_trace_point_emit_i64(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_i32,
	RTE_TRACE_POINT_ARGS(int32_t in),
	rte_trace_point_emit_i32(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_i16,
	RTE_TRACE_POINT_ARGS(int16_t in),
	rte_trace_point_emit_i16(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_i8,
	RTE_TRACE_POINT_ARGS(int8_t in),
	rte_trace_point_emit_i8(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_int,
	RTE_TRACE_POINT_ARGS(int in),
	rte_trace_point_emit_int(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_long,
	RTE_TRACE_POINT_ARGS(long in),
	rte_trace_point_emit_long(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_float,
	RTE_TRACE_POINT_ARGS(float in),
	rte_trace_point_emit_float(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_double,
	RTE_TRACE_POINT_ARGS(double in),
	rte_trace_point_emit_double(in);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_ptr,
	RTE_TRACE_POINT_ARGS(const void *ptr),
	rte_trace_point_emit_ptr(ptr);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_str,
	RTE_TRACE_POINT_ARGS(const char *str),
	rte_trace_point_emit_string(str);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_size_t,
	RTE_TRACE_POINT_ARGS(size_t sz),
	rte_trace_point_emit_size_t(sz);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_func,
	RTE_TRACE_POINT_ARGS(const char *func),
	rte_trace_point_emit_string(func);
)

RTE_TRACE_POINT(
	rte_eal_trace_generic_blob,
	RTE_TRACE_POINT_ARGS(const void *in, uint8_t len),
	rte_trace_point_emit_blob(in, len);
)

#define RTE_EAL_TRACE_GENERIC_FUNC rte_eal_trace_generic_func(__func__)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EAL_TRACE_H_ */
