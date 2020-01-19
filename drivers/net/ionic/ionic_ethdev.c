/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include "ionic_logs.h"

int ionic_logtype;

RTE_INIT(ionic_init_log)
{
	ionic_logtype = rte_log_register("pmd.net.ionic");
	if (ionic_logtype >= 0)
		rte_log_set_level(ionic_logtype, RTE_LOG_NOTICE);
}
