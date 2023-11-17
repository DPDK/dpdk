/* SPDX-License-Identifier: BSD-3-Clause */

extern int mbuf_logtype;
#define RTE_LOGTYPE_MBUF	mbuf_logtype
#define MBUF_LOG(level, ...) \
	RTE_LOG_LINE(level, MBUF, "" __VA_ARGS__)
