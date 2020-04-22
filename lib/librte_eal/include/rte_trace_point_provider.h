/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_TRACE_POINT_H_
#error do not include this file directly, use <rte_trace_point.h> instead
#endif

#ifndef _RTE_TRACE_POINT_PROVIDER_H_
#define _RTE_TRACE_POINT_PROVIDER_H_

#ifdef ALLOW_EXPERIMENTAL_API

#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_per_lcore.h>
#include <rte_string_fns.h>
#include <rte_uuid.h>

#define __RTE_TRACE_EVENT_HEADER_ID_SHIFT (48)

#define __RTE_TRACE_FIELD_SIZE_SHIFT 0
#define __RTE_TRACE_FIELD_SIZE_MASK (0xffffULL << __RTE_TRACE_FIELD_SIZE_SHIFT)
#define __RTE_TRACE_FIELD_ID_SHIFT (16)
#define __RTE_TRACE_FIELD_ID_MASK (0xffffULL << __RTE_TRACE_FIELD_ID_SHIFT)
#define __RTE_TRACE_FIELD_ENABLE_MASK (1ULL << 63)
#define __RTE_TRACE_FIELD_ENABLE_DISCARD (1ULL << 62)

struct __rte_trace_stream_header {
	uint32_t magic;
	rte_uuid_t uuid;
	uint32_t lcore_id;
	char thread_name[__RTE_TRACE_EMIT_STRING_LEN_MAX];
} __rte_packed;

struct __rte_trace_header {
	uint32_t offset;
	uint32_t len;
	struct __rte_trace_stream_header stream_header;
	uint8_t mem[];
};

RTE_DECLARE_PER_LCORE(void *, trace_mem);

static __rte_always_inline void *
__rte_trace_mem_get(uint64_t in)
{
	struct __rte_trace_header *trace = RTE_PER_LCORE(trace_mem);
	const uint16_t sz = in & __RTE_TRACE_FIELD_SIZE_MASK;

	/* Trace memory is not initialized for this thread */
	if (unlikely(trace == NULL)) {
		__rte_trace_mem_per_thread_alloc();
		trace = RTE_PER_LCORE(trace_mem);
		if (unlikely(trace == NULL))
			return NULL;
	}
	/* Check the wrap around case */
	uint32_t offset = trace->offset;
	if (unlikely((offset + sz) >= trace->len)) {
		/* Disable the trace event if it in DISCARD mode */
		if (unlikely(in & __RTE_TRACE_FIELD_ENABLE_DISCARD))
			return NULL;

		offset = 0;
	}
	/* Align to event header size */
	offset = RTE_ALIGN_CEIL(offset, __RTE_TRACE_EVENT_HEADER_SZ);
	void *mem = RTE_PTR_ADD(&trace->mem[0], offset);
	offset += sz;
	trace->offset = offset;

	return mem;
}

static __rte_always_inline void *
__rte_trace_point_emit_ev_header(void *mem, uint64_t in)
{
	uint64_t val;

	/* Event header [63:0] = id [63:48] | timestamp [47:0] */
	val = rte_get_tsc_cycles() &
		~(0xffffULL << __RTE_TRACE_EVENT_HEADER_ID_SHIFT);
	val |= ((in & __RTE_TRACE_FIELD_ID_MASK) <<
		(__RTE_TRACE_EVENT_HEADER_ID_SHIFT - __RTE_TRACE_FIELD_ID_SHIFT));

	*(uint64_t *)mem = val;
	return RTE_PTR_ADD(mem, __RTE_TRACE_EVENT_HEADER_SZ);
}

#define __rte_trace_point_emit_header_generic(t) \
void *mem; \
do { \
	const uint64_t val = __atomic_load_n(t, __ATOMIC_ACQUIRE); \
	if (likely(!(val & __RTE_TRACE_FIELD_ENABLE_MASK))) \
		return; \
	mem = __rte_trace_mem_get(val); \
	if (unlikely(mem == NULL)) \
		return; \
	mem = __rte_trace_point_emit_ev_header(mem, val); \
} while (0)

#define __rte_trace_point_emit_header_fp(t) \
	if (!__rte_trace_point_fp_is_enabled()) \
		return; \
	__rte_trace_point_emit_header_generic(t)

#define __rte_trace_point_emit(in, type) \
do { \
	memcpy(mem, &(in), sizeof(in)); \
	mem = RTE_PTR_ADD(mem, sizeof(in)); \
} while (0)

#define rte_trace_point_emit_string(in) \
do { \
	if (unlikely(in == NULL)) \
		return; \
	rte_strscpy(mem, in, __RTE_TRACE_EMIT_STRING_LEN_MAX); \
	mem = RTE_PTR_ADD(mem, __RTE_TRACE_EMIT_STRING_LEN_MAX); \
} while (0)

#else

#define __rte_trace_point_emit_header_generic(t) RTE_SET_USED(t)
#define __rte_trace_point_emit_header_fp(t) RTE_SET_USED(t)
#define __rte_trace_point_emit(in, type) RTE_SET_USED(in)
#define rte_trace_point_emit_string(in) RTE_SET_USED(in)

#endif

#endif /* _RTE_TRACE_POINT_PROVIDER_H_ */
