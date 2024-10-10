/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_API_NIC_SETUP_H__
#define __FLOW_API_NIC_SETUP_H__

#include "hw_mod_backend.h"
#include "flow_api.h"

void *flow_api_get_be_dev(struct flow_nic_dev *dev);

#endif  /* __FLOW_API_NIC_SETUP_H__ */
