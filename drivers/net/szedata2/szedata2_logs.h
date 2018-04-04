/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 CESNET
 */

#ifndef _SZEDATA2_LOGS_H_
#define _SZEDATA2_LOGS_H_

#include <rte_log.h>

extern int szedata2_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, szedata2_logtype_init, \
		"%s(): " fmt "\n", __func__, ## args)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

extern int szedata2_logtype_driver;
#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, szedata2_logtype_driver, \
		"%s(): " fmt "\n", __func__, ## args)

#endif /* _SZEDATA2_LOGS_H_ */
