/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_hexdump.h>

#include "bcmfs_logs.h"

int
bcmfs_hexdump_log(uint32_t level, uint32_t logtype, const char *title,
		const void *buf, unsigned int len)
{
	if (level > rte_log_get_global_level())
		return 0;
	if (level > (uint32_t)(rte_log_get_level(logtype)))
		return 0;

	rte_hexdump(rte_log_get_stream(), title, buf, len);
	return 0;
}

RTE_LOG_REGISTER_SUFFIX(bcmfs_conf_logtype, config, NOTICE)
RTE_LOG_REGISTER_SUFFIX(bcmfs_dp_logtype, fp, NOTICE)
