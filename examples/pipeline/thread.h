/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _INCLUDE_THREAD_H_
#define _INCLUDE_THREAD_H_

#include <stdint.h>

#include <rte_swx_pipeline.h>

int
thread_pipeline_enable(uint32_t thread_id,
		       struct rte_swx_pipeline *p,
		       uint32_t timer_period_ms);

int
thread_pipeline_disable(uint32_t thread_id,
			struct rte_swx_pipeline *p);

int
thread_init(void);

int
thread_main(void *arg);

#endif /* _INCLUDE_THREAD_H_ */
