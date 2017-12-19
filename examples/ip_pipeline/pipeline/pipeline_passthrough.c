/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "pipeline_passthrough.h"
#include "pipeline_passthrough_be.h"

static int
app_pipeline_passthrough_track(struct pipeline_params *p,
	uint32_t port_in,
	uint32_t *port_out)
{
	struct pipeline_passthrough_params pp;
	int status;

	/* Check input arguments */
	if ((p == NULL) ||
		(port_in >= p->n_ports_in) ||
		(port_out == NULL))
		return -1;

	status = pipeline_passthrough_parse_args(&pp, p);
	if (status)
		return -1;

	if (pp.dma_hash_lb_enabled)
		return -1;

	*port_out = port_in / (p->n_ports_in / p->n_ports_out);
	return 0;
}

static struct pipeline_fe_ops pipeline_passthrough_fe_ops = {
	.f_init = NULL,
	.f_post_init = NULL,
	.f_free = NULL,
	.f_track = app_pipeline_passthrough_track,
	.cmds = NULL,
};

struct pipeline_type pipeline_passthrough = {
	.name = "PASS-THROUGH",
	.be_ops = &pipeline_passthrough_be_ops,
	.fe_ops = &pipeline_passthrough_fe_ops,
};
