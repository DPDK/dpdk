/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "e1000_logs.h"

/* declared as extern in e1000_logs.h */
int e1000_logtype_init;
int e1000_logtype_driver;

#ifdef RTE_LIBRTE_E1000_DEBUG_RX
int e1000_logtype_rx;
#endif
#ifdef RTE_LIBRTE_E1000_DEBUG_TX
int e1000_logtype_tx;
#endif
#ifdef RTE_LIBRTE_E1000_DEBUG_TX_FREE
int e1000_logtype_tx_free;
#endif

/* avoids double registering of logs if EM and IGB drivers are in use */
static int e1000_log_initialized;

void
e1000_igb_init_log(void)
{
	if (e1000_log_initialized)
		return;

	e1000_logtype_init = rte_log_register("pmd.net.e1000.init");
	if (e1000_logtype_init >= 0)
		rte_log_set_level(e1000_logtype_init, RTE_LOG_NOTICE);
	e1000_logtype_driver = rte_log_register("pmd.net.e1000.driver");
	if (e1000_logtype_driver >= 0)
		rte_log_set_level(e1000_logtype_driver, RTE_LOG_NOTICE);

#ifdef RTE_LIBRTE_E1000_DEBUG_RX
	e1000_logtype_rx = rte_log_register("pmd.net.e1000.rx");
	if (e1000_logtype_rx >= 0)
		rte_log_set_level(e1000_logtype_rx, RTE_LOG_DEBUG);
#endif

#ifdef RTE_LIBRTE_E1000_DEBUG_TX
	e1000_logtype_tx = rte_log_register("pmd.net.e1000.tx");
	if (e1000_logtype_tx >= 0)
		rte_log_set_level(e1000_logtype_tx, RTE_LOG_DEBUG);
#endif

#ifdef RTE_LIBRTE_E1000_DEBUG_TX_FREE
	e1000_logtype_tx_free = rte_log_register("pmd.net.e1000.tx_free");
	if (e1000_logtype_tx_free >= 0)
		rte_log_set_level(e1000_logtype_tx_free, RTE_LOG_DEBUG);
#endif

	e1000_log_initialized = 1;
}
