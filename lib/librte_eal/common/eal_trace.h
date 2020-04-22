/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __EAL_TRACE_H
#define __EAL_TRACE_H

#include <rte_trace.h>
#include <rte_trace_point.h>

#define trace_err(fmt, args...) \
	RTE_LOG(ERR, EAL, "%s():%u " fmt "\n", __func__, __LINE__, ## args)

#define trace_crit(fmt, args...) \
	RTE_LOG(CRIT, EAL, "%s():%u " fmt "\n", __func__, __LINE__, ## args)

#define TRACE_CTF_FIELD_SIZE 384
#define TRACE_POINT_NAME_SIZE 64

struct trace_point {
	STAILQ_ENTRY(trace_point) next;
	rte_trace_point_t *handle;
	char name[TRACE_POINT_NAME_SIZE];
	char ctf_field[TRACE_CTF_FIELD_SIZE];
};

struct trace {
	int register_errno;
	bool status;
	enum rte_trace_mode mode;
	uint32_t nb_trace_points;
};

/* Helper functions */
static inline uint16_t
trace_id_get(rte_trace_point_t *trace)
{
	return (*trace & __RTE_TRACE_FIELD_ID_MASK) >>
		__RTE_TRACE_FIELD_ID_SHIFT;
}

/* Trace point list functions */
STAILQ_HEAD(trace_point_head, trace_point);

#endif /* __EAL_TRACE_H */
