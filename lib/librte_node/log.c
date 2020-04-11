/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include "node_private.h"

int rte_node_logtype;

RTE_INIT(rte_node_init_log)
{
	rte_node_logtype = rte_log_register("lib.node");
	if (rte_node_logtype >= 0)
		rte_log_set_level(rte_node_logtype, RTE_LOG_INFO);
}
