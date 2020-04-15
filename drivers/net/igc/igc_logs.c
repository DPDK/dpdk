/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#include <rte_common.h>

#include "igc_logs.h"

/* declared as extern in igc_logs.h */
int igc_logtype_init;
int igc_logtype_driver;

RTE_INIT(igc_init_log)
{
	igc_logtype_init = rte_log_register("pmd.net.igc.init");
	if (igc_logtype_init >= 0)
		rte_log_set_level(igc_logtype_init, RTE_LOG_INFO);

	igc_logtype_driver = rte_log_register("pmd.net.igc.driver");
	if (igc_logtype_driver >= 0)
		rte_log_set_level(igc_logtype_driver, RTE_LOG_INFO);
}
