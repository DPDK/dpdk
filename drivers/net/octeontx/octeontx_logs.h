/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __OCTEONTX_LOGS_H__
#define __OCTEONTX_LOGS_H__

#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, otx_net_logtype_init, \
			"%s(): " fmt "\n", __func__, ## args)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, ">>")

#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, otx_net_logtype_driver, \
			"%s(): " fmt "\n", __func__, ## args)

#define PMD_MBOX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, otx_net_logtype_mbox, \
			"%s(): " fmt "\n", __func__, ## args)

#define octeontx_log_info(fmt, args...)			\
	RTE_LOG(INFO, PMD, fmt "\n", ## args)

#define octeontx_log_err(s, ...) PMD_INIT_LOG(ERR, s, ##__VA_ARGS__)
#define octeontx_log_dbg(s, ...) PMD_DRV_LOG(DEBUG, s, ##__VA_ARGS__)
#define octeontx_mbox_log(s, ...) PMD_MBOX_LOG(DEBUG, s, ##__VA_ARGS__)

#define PMD_RX_LOG	PMD_DRV_LOG
#define PMD_TX_LOG	PMD_DRV_LOG

extern int otx_net_logtype_init;
extern int otx_net_logtype_driver;
extern int otx_net_logtype_mbox;

#endif /* __OCTEONTX_LOGS_H__*/
