/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __OCTEONTX_LOGS_H__
#define __OCTEONTX_LOGS_H__

#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, OTX_NET_INIT, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, ">>")

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, OTX_NET_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#define PMD_MBOX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, OTX_NET_MBOX, "%s(): ", __func__, __VA_ARGS__)

#define octeontx_log_info(...) \
	RTE_LOG_LINE(INFO, OTX_NET_DRIVER, __VA_ARGS__)

#define octeontx_log_err(s, ...) PMD_INIT_LOG(ERR, s, ##__VA_ARGS__)
#define octeontx_log_dbg(s, ...) PMD_DRV_LOG(DEBUG, s, ##__VA_ARGS__)
#define octeontx_mbox_log(s, ...) PMD_MBOX_LOG(DEBUG, s, ##__VA_ARGS__)

#define PMD_RX_LOG	PMD_DRV_LOG
#define PMD_TX_LOG	PMD_DRV_LOG

extern int otx_net_logtype_init;
#define RTE_LOGTYPE_OTX_NET_INIT otx_net_logtype_init
extern int otx_net_logtype_driver;
#define RTE_LOGTYPE_OTX_NET_DRIVER otx_net_logtype_driver
extern int otx_net_logtype_mbox;
#define RTE_LOGTYPE_OTX_NET_MBOX otx_net_logtype_mbox

#endif /* __OCTEONTX_LOGS_H__*/
