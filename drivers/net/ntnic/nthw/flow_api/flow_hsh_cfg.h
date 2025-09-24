/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _FLOW_HSH_CFG_H_
#define _FLOW_HSH_CFG_H_

#include <rte_ethdev.h>

#include "hw_mod_backend.h"
#include "flow_api.h"

int nthw_hsh_set(struct flow_nic_dev *ndev, int hsh_idx,
	struct nt_eth_rss_conf rss_conf);

#endif /* _FLOW_HSH_CFG_H_ */
