/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_log.h>

int nitrox_logtype;

RTE_INIT(nitrox_init_log)
{
	nitrox_logtype = rte_log_register("pmd.crypto.nitrox");
	if (nitrox_logtype >= 0)
		rte_log_set_level(nitrox_logtype, RTE_LOG_NOTICE);
}
