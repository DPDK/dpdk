/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_log.h>
#include <rte_hexdump.h>

#include "qat_logs.h"

int qat_gen_logtype;
int qat_dp_logtype;

int
qat_hexdump_log(uint32_t level, uint32_t logtype, const char *title,
		const void *buf, unsigned int len)
{
	if (rte_log_can_log(logtype, level))
		rte_hexdump(rte_log_get_stream(), title, buf, len);

	return 0;
}

RTE_INIT(qat_pci_init_log)
{
	/* Non-data-path logging for pci device and all services */
	qat_gen_logtype = rte_log_register("pmd.qat_general");
	if (qat_gen_logtype >= 0)
		rte_log_set_level(qat_gen_logtype, RTE_LOG_NOTICE);

	/* data-path logging for all services */
	qat_dp_logtype = rte_log_register("pmd.qat_dp");
	if (qat_dp_logtype >= 0)
		rte_log_set_level(qat_dp_logtype, RTE_LOG_NOTICE);
}
