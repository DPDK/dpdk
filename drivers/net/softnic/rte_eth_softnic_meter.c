/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_mtr.h>
#include <rte_mtr_driver.h>

#include "rte_eth_softnic_internals.h"

const struct rte_mtr_ops pmd_mtr_ops = {
	.capabilities_get = NULL,

	.meter_profile_add = NULL,
	.meter_profile_delete = NULL,

	.create = NULL,
	.destroy = NULL,
	.meter_enable = NULL,
	.meter_disable = NULL,

	.meter_profile_update = NULL,
	.meter_dscp_table_update = NULL,
	.policer_actions_update = NULL,
	.stats_update = NULL,

	.stats_read = NULL,
};
