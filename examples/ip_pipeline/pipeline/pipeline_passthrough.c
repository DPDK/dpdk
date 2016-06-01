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

	if (pp.lb_hash_enabled)
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
