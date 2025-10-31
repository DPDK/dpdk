/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_API_NIC_SETUP_H__
#define __FLOW_API_NIC_SETUP_H__

#include "hw_mod_backend.h"
#include "flow_api.h"

/*
 * Flow capable NIC backend - creating flow api instance for adapter nr (backend)
 */
struct flow_nic_dev *nthw_flow_api_create(uint8_t adapter_no,
	const struct flow_api_backend_ops *be_if, void *be_dev);
int nthw_flow_api_done(struct flow_nic_dev *dev);
void *nthw_flow_api_get_be_dev(struct flow_nic_dev *dev);

#endif  /* __FLOW_API_NIC_SETUP_H__ */
