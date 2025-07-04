/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_LOGS_H_
#define _BCMFS_LOGS_H_

#include <rte_log.h>

extern int bcmfs_conf_logtype;
#define RTE_LOGTYPE_BCMFS_CONF bcmfs_conf_logtype
extern int bcmfs_dp_logtype;
#define RTE_LOGTYPE_BCMFS_DP bcmfs_dp_logtype

#define BCMFS_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, BCMFS_CONF, "%s(): ", __func__, __VA_ARGS__)

#define BCMFS_DP_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, BCMFS_DP, "%s(): ", __func__, __VA_ARGS__)

#define BCMFS_DP_HEXDUMP_LOG(level, title, buf, len)	\
	bcmfs_hexdump_log(RTE_LOG_ ## level, bcmfs_dp_logtype, title, buf, len)

/**
 * bcmfs_hexdump_log Dump out memory in a special hex dump format.
 *
 * The message will be sent to the stream used by the rte_log infrastructure.
 */
int
bcmfs_hexdump_log(uint32_t level, uint32_t logtype, const char *heading,
		  const void *buf, unsigned int len);

#endif /* _BCMFS_LOGS_H_ */
