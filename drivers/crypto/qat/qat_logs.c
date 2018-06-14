/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_log.h>

int qat_gen_logtype;

RTE_INIT(qat_pci_init_log);
static void
qat_pci_init_log(void)
{
	/* Non-data-path logging for pci device and all services */
	qat_gen_logtype = rte_log_register("pmd.qat_general");
	if (qat_gen_logtype >= 0)
		rte_log_set_level(qat_gen_logtype, RTE_LOG_NOTICE);
}
