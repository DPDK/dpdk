/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_PIPELINE_H__
#define __INCLUDE_RTE_SWX_PIPELINE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Pipeline
 */

#include <stddef.h>
#include <stdint.h>

#include <rte_compat.h>

/*
 * Pipeline setup and operation
 */

/** Pipeline opaque data structure. */
struct rte_swx_pipeline;

/**
 * Pipeline configure
 *
 * @param[out] p
 *   Pipeline handle. Must point to valid memory. Contains valid pipeline handle
 *   when the function returns successfully.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory.
 */
__rte_experimental
int
rte_swx_pipeline_config(struct rte_swx_pipeline **p,
			int numa_node);

/**
 * Pipeline build
 *
 * Once called, the pipeline build operation marks the end of pipeline
 * configuration. At this point, all the internal data structures needed to run
 * the pipeline are built.
 *
 * @param[in] p
 *   Pipeline handle.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Pipeline was already built successfully.
 */
__rte_experimental
int
rte_swx_pipeline_build(struct rte_swx_pipeline *p);

/**
 * Pipeline free
 *
 * @param[in] p
 *   Pipeline handle.
 */
__rte_experimental
void
rte_swx_pipeline_free(struct rte_swx_pipeline *p);

#ifdef __cplusplus
}
#endif

#endif
