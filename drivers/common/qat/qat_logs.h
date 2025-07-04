/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_LOGS_H_
#define _QAT_LOGS_H_

extern int qat_gen_logtype;
#define RTE_LOGTYPE_QAT_GEN qat_gen_logtype
extern int qat_dp_logtype;
#define RTE_LOGTYPE_QAT_DP qat_dp_logtype

#define QAT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, QAT_GEN, "%s(): ", __func__, __VA_ARGS__)

#define QAT_DP_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, QAT_DP, "%s(): ", __func__, __VA_ARGS__)

#define QAT_DP_HEXDUMP_LOG(level, title, buf, len)		\
	qat_hexdump_log(RTE_LOG_ ## level, qat_dp_logtype, title, buf, len)

/**
 * qat_hexdump_log - Dump out memory in a special hex dump format.
 *
 * Dump out the message buffer in a special hex dump output format with
 * characters printed for each line of 16 hex values. The message will be sent
 * to the stream used by the rte_log infrastructure.
 */
int
qat_hexdump_log(uint32_t level, uint32_t logtype, const char *title,
		const void *buf, unsigned int len);

#endif /* _QAT_LOGS_H_ */
