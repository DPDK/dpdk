/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "flow_api_nic_setup.h"
#include "ntnic_mod_reg.h"

#include "flow_filter.h"

void *flow_api_get_be_dev(struct flow_nic_dev *ndev)
{
	if (!ndev) {
		NT_LOG(DBG, FILTER, "ERR: %s", __func__);
		return NULL;
	}

	return ndev->be.be_dev;
}

static const struct flow_filter_ops ops = {
	.flow_filter_init = flow_filter_init,
	.flow_filter_done = flow_filter_done,
};

void init_flow_filter(void)
{
	register_flow_filter_ops(&ops);
}
