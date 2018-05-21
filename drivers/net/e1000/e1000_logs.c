/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "e1000_logs.h"

/* declared as extern in e1000_logs.h */
int e1000_logtype_init;
int e1000_logtype_driver;

/* avoids double registering of logs if EM and IGB drivers are in use */
static int e1000_log_initialized;

void
e1000_igb_init_log(void)
{
	if (!e1000_log_initialized) {
		e1000_logtype_init = rte_log_register("pmd.net.e1000.init");
		if (e1000_logtype_init >= 0)
			rte_log_set_level(e1000_logtype_init, RTE_LOG_NOTICE);
		e1000_logtype_driver = rte_log_register("pmd.net.e1000.driver");
		if (e1000_logtype_driver >= 0)
			rte_log_set_level(e1000_logtype_driver, RTE_LOG_NOTICE);
		e1000_log_initialized = 1;
	}
}
