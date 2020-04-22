/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_TRACE_POINT_H_
#error do not include this file directly, use <rte_trace_point.h> instead
#endif

#ifndef _RTE_TRACE_POINT_PROVIDER_H_
#define _RTE_TRACE_POINT_PROVIDER_H_

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

#endif /* _RTE_TRACE_POINT_PROVIDER_H_ */
