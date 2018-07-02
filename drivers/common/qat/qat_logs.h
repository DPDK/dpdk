/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_LOGS_H_
#define _QAT_LOGS_H_

extern int qat_gen_logtype;
extern int qat_dp_logtype;

#define QAT_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, qat_gen_logtype,		\
			"%s(): " fmt "\n", __func__, ## args)

#define QAT_DP_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, qat_dp_logtype,		\
			"%s(): " fmt "\n", __func__, ## args)

#define QAT_DP_HEXDUMP_LOG(level, title, buf, len)		\
	qat_hexdump_log(RTE_LOG_ ## level, qat_dp_logtype, title, buf, len)

/**
 * qat_hexdump_log - Dump out memory in a special hex dump format.
 *
 * Dump out the message buffer in a special hex dump output format with
 * characters printed for each line of 16 hex values. The message will be sent
 * to the stream defined by rte_logs.file or to stderr in case of rte_logs.file
 * is undefined.
 */
int
qat_hexdump_log(uint32_t level, uint32_t logtype, const char *title,
		const void *buf, unsigned int len);

#endif /* _QAT_LOGS_H_ */
