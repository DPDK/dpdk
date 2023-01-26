/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _INCLUDE_THREAD_H_
#define _INCLUDE_THREAD_H_

#include <stdint.h>

#include <rte_swx_pipeline.h>

/**
 * Control plane (CP) thread.
 */
int
thread_init(void);

int
pipeline_enable(struct rte_swx_pipeline *p, uint32_t thread_id);

void
pipeline_disable(struct rte_swx_pipeline *p);

typedef void
(*block_run_f)(void *block);

int
block_enable(block_run_f block_func, void *block, uint32_t thread_id);

void
block_disable(void *block);

/**
 * Data plane (DP) threads.
 */
int
thread_main(void *arg);

#endif /* _INCLUDE_THREAD_H_ */
