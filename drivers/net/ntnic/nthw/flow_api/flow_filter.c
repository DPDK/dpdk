/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "flow_filter.h"
#include "ntnic_mod_reg.h"
#include "flow_api_nic_setup.h"

int flow_filter_init(nthw_fpga_t *p_fpga, struct flow_nic_dev **p_flow_device, int adapter_no)
{
	(void)p_flow_device;
	(void)adapter_no;

	void *be_dev = NULL;

	const struct flow_backend_ops *flow_backend_ops = get_flow_backend_ops();

	if (flow_backend_ops == NULL) {
		NT_LOG(ERR, FILTER, "%s: flow_backend module uninitialized", __func__);
		return -1;
	}

	NT_LOG(DBG, FILTER, "Initializing flow filter api");
	flow_backend_ops->bin_flow_backend_init(p_fpga, &be_dev);

	return 0;
}

int flow_filter_done(struct flow_nic_dev *dev)
{
	void *be_dev = flow_api_get_be_dev(dev);

	int res = 0;

	if (be_dev) {
		const struct flow_backend_ops *flow_backend_ops = get_flow_backend_ops();

		if (flow_backend_ops == NULL) {
			NT_LOG(WRN, FILTER, "%s: flow_backend module uninitialized", __func__);
			return res;
		}

		flow_backend_ops->bin_flow_backend_done(be_dev);
	}

	return res;
}
