/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_log.h>
#include <rte_hexdump.h>

#include "qat_logs.h"

int
qat_hexdump_log(uint32_t level, uint32_t logtype, const char *title,
		const void *buf, unsigned int len)
{
	if (rte_log_can_log(logtype, level))
		rte_hexdump(rte_log_get_stream(), title, buf, len);

	return 0;
}

RTE_LOG_REGISTER(qat_gen_logtype, pmd.qat_general, NOTICE);
RTE_LOG_REGISTER(qat_dp_logtype, pmd.qat_dp, NOTICE);
