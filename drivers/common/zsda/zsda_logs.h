/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef _ZSDA_LOGS_H_
#define _ZSDA_LOGS_H_

#include <rte_log.h>

extern int zsda_logtype_gen;
#define RTE_LOGTYPE_ZSDA_GEN zsda_logtype_gen

#define ZSDA_LOG(level, ...)             \
	RTE_LOG_LINE_PREFIX(level, ZSDA_GEN, "%s(): ", \
		__func__, __VA_ARGS__)

/**
 * zsda_hexdump_log - Dump out memory in a special hex dump format.
 *
 * Dump out the message buffer in a special hex dump output format with
 * characters printed for each line of 16 hex values. The message will be sent
 * to the stream used by the rte_log infrastructure.
 */
int zsda_hexdump_log(uint32_t level, uint32_t logtype, const char *title,
		     const void *buf, unsigned int len);

#endif /* _ZSDA_LOGS_H_ */
