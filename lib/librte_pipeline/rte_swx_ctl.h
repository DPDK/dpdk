/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_CTL_H__
#define __INCLUDE_RTE_SWX_CTL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Pipeline Control
 */

#include <stddef.h>
#include <stdint.h>

#include <rte_compat.h>

#include "rte_swx_table.h"

/*
 * Table Update API.
 */

/** Table state. */
struct rte_swx_table_state {
	/** Table object. */
	void *obj;

	/** Action ID of the table default action. */
	uint64_t default_action_id;

	/** Action data of the table default action. Ignored when the action
	 * data size is zero; otherwise, action data size bytes are meaningful.
	 */
	uint8_t *default_action_data;
};

/**
 * Pipeline table state get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[out] table_state
 *   After successful execution, the *table_state* contains the pointer to the
 *   current pipeline table state, which is an array of *n_tables* elements,
 *   with array element i containing the state of the i-th pipeline table. The
 *   pipeline continues to own all the data structures directly or indirectly
 *   referenced by the *table_state* until the subsequent successful invocation
 *   of function *rte_swx_pipeline_table_state_set*.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_pipeline_table_state_get(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state **table_state);

/**
 * Pipeline table state set
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[out] table_state
 *   After successful execution, the pipeline table state is updated to this
 *   *table_state*. The ownership of all the data structures directly or
 *   indirectly referenced by this *table_state* is passed from the caller to
 *   the pipeline.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_pipeline_table_state_set(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state *table_state);

#ifdef __cplusplus
}
#endif

#endif
