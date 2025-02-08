/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <rte_hexdump.h>

#include "zsda_logs.h"

int
zsda_hexdump_log(uint32_t level, uint32_t logtype, const char *title,
		const void *buf, unsigned int len)
{
	if (rte_log_can_log(logtype, level))
		rte_hexdump(rte_log_get_stream(), title, buf, len);

	return 0;
}

RTE_LOG_REGISTER_SUFFIX(zsda_logtype_gen, gen, NOTICE);
