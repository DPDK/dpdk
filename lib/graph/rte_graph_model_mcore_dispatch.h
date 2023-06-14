/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Intel Corporation
 */

#ifndef _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_
#define _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_

/**
 * @file rte_graph_model_mcore_dispatch.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * These APIs allow to set core affinity with the node and only used for mcore
 * dispatch model.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_graph_worker_common.h"

/**
 * Set lcore affinity with the node used for mcore dispatch model.
 *
 * @param name
 *   Valid node name. In the case of the cloned node, the name will be
 * "parent node name" + "-" + name.
 * @param lcore_id
 *   The lcore ID value.
 *
 * @return
 *   0 on success, error otherwise.
 */
__rte_experimental
int rte_graph_model_mcore_dispatch_node_lcore_affinity_set(const char *name,
							   unsigned int lcore_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRAPH_MODEL_MCORE_DISPATCH_H_ */
