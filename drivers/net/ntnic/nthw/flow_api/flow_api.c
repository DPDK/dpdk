/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntnic_mod_reg.h"

#include "flow_filter.h"

static const struct flow_filter_ops ops = {
	.flow_filter_init = flow_filter_init,
};

void init_flow_filter(void)
{
	register_flow_filter_ops(&ops);
}
