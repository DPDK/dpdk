/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "pipeline_master.h"
#include "pipeline_master_be.h"

static struct pipeline_fe_ops pipeline_master_fe_ops = {
	.f_init = NULL,
	.f_post_init = NULL,
	.f_free = NULL,
	.f_track = NULL,
	.cmds = NULL,
};

struct pipeline_type pipeline_master = {
	.name = "MASTER",
	.be_ops = &pipeline_master_be_ops,
	.fe_ops = &pipeline_master_fe_ops,
};
