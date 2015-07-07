/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __INCLUDE_PIPELINE_H__
#define __INCLUDE_PIPELINE_H__

#include <cmdline_parse.h>

#include "pipeline_be.h"

/*
 * Pipeline type front-end operations
 */

typedef void* (*pipeline_fe_op_init)(struct pipeline_params *params, void *arg);

typedef int (*pipeline_fe_op_free)(void *pipeline);

struct pipeline_fe_ops {
	pipeline_fe_op_init f_init;
	pipeline_fe_op_free f_free;
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

#endif
