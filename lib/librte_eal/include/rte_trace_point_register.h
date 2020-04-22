/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_TRACE_POINT_H_
#error do not include this file directly, use <rte_trace_point.h> instead
#endif

#ifndef _RTE_TRACE_POINT_REGISTER_H_
#define _RTE_TRACE_POINT_REGISTER_H_

#include <rte_per_lcore.h>

RTE_DECLARE_PER_LCORE(volatile int, trace_point_sz);

#define RTE_TRACE_POINT_REGISTER(trace, name) \
	__rte_trace_point_register(&__##trace, RTE_STR(name), \
		(void (*)(void)) trace)

#endif /* _RTE_TRACE_POINT_REGISTER_H_ */
