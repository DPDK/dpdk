/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef __INCLUDE_PIPELINE_H__
#define __INCLUDE_PIPELINE_H__

#include <cmdline_parse.h>

#include "pipeline_be.h"

/*
 * Pipeline type front-end operations
 */

typedef void* (*pipeline_fe_op_init)(struct pipeline_params *params,
	void *arg);

typedef int (*pipeline_fe_op_post_init)(void *pipeline);

typedef int (*pipeline_fe_op_free)(void *pipeline);

typedef int (*pipeline_fe_op_track)(struct pipeline_params *params,
	uint32_t port_in,
	uint32_t *port_out);

struct pipeline_fe_ops {
	pipeline_fe_op_init f_init;
	pipeline_fe_op_post_init f_post_init;
	pipeline_fe_op_free f_free;
	pipeline_fe_op_track f_track;
	cmdline_parse_ctx_t *cmds;
};

/*
 * Pipeline type
 */

struct pipeline_type {
	const char *name;

	/* pipeline back-end */
	struct pipeline_be_ops *be_ops;

	/* pipeline front-end */
	struct pipeline_fe_ops *fe_ops;
};

static inline uint32_t
pipeline_type_cmds_count(struct pipeline_type *ptype)
{
	cmdline_parse_ctx_t *cmds;
	uint32_t n_cmds;

	if (ptype->fe_ops == NULL)
		return 0;

	cmds = ptype->fe_ops->cmds;
	if (cmds == NULL)
		return 0;

	for (n_cmds = 0; cmds[n_cmds]; n_cmds++);

	return n_cmds;
}

int
parse_pipeline_core(uint32_t *socket,
	uint32_t *core,
	uint32_t *ht,
	const char *entry);

#endif
