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

#include "rte_swx_port.h"

/** Name size. */
#ifndef RTE_SWX_NAME_SIZE
#define RTE_SWX_NAME_SIZE 64
#endif
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

/*
 * Pipeline input ports
 */

/**
 * Pipeline input port type register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Input port type name.
 * @param[in] ops
 *   Input port type operations.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Input port type with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_port_in_type_register(struct rte_swx_pipeline *p,
				       const char *name,
				       struct rte_swx_port_in_ops *ops);

/**
 * Pipeline input port configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] port_id
 *   Input port ID.
 * @param[in] port_type_name
 *   Existing input port type name.
 * @param[in] args
 *   Input port creation arguments.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -ENODEV: Input port object creation error.
 */
__rte_experimental
int
rte_swx_pipeline_port_in_config(struct rte_swx_pipeline *p,
				uint32_t port_id,
				const char *port_type_name,
				void *args);

/*
 * Pipeline output ports
 */

/**
 * Pipeline output port type register
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Output port type name.
 * @param[in] ops
 *   Output port type operations.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Output port type with this name already exists.
 */
__rte_experimental
int
rte_swx_pipeline_port_out_type_register(struct rte_swx_pipeline *p,
					const char *name,
					struct rte_swx_port_out_ops *ops);

/**
 * Pipeline output port configure
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] port_id
 *   Output port ID.
 * @param[in] port_type_name
 *   Existing output port type name.
 * @param[in] args
 *   Output port creation arguments.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -ENODEV: Output port object creation error.
 */
__rte_experimental
int
rte_swx_pipeline_port_out_config(struct rte_swx_pipeline *p,
				 uint32_t port_id,
				 const char *port_type_name,
				 void *args);

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
