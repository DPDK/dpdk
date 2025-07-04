/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_FILTER_HPP__
#define __FLOW_FILTER_HPP__

#include "flow_api.h"
#include "nthw_fpga_model.h"

int nthw_flow_filter_init(nthw_fpga_t *p_fpga, struct flow_nic_dev **p_flow_device, int adapter_no);
int nthw_flow_filter_done(struct flow_nic_dev *dev);
int nthw_flow_get_flm_stats(struct flow_nic_dev *ndev, uint64_t *data, uint64_t size);

#endif  /* __FLOW_FILTER_HPP__ */
