/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_H_
#define _FLOW_API_H_

#include "ntlog.h"

#include "hw_mod_backend.h"

/* registered NIC backends */
struct flow_nic_dev {
	/* NIC backend API */
	struct flow_api_backend_s be;
};

#endif
