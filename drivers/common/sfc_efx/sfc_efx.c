/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_log.h>

#include "sfc_efx_log.h"

uint32_t sfc_efx_logtype;

RTE_INIT(sfc_efx_register_logtype)
{
	int ret;

	ret = rte_log_register_type_and_pick_level("pmd.common.sfc_efx",
						   RTE_LOG_NOTICE);
	sfc_efx_logtype = (ret < 0) ? RTE_LOGTYPE_PMD : ret;
}
