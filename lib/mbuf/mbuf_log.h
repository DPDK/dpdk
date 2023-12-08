/* SPDX-License-Identifier: BSD-3-Clause */

extern int mbuf_logtype;
#define RTE_LOGTYPE_MBUF	mbuf_logtype
#define MBUF_LOG(level, fmt, ...) \
	RTE_LOG(level, MBUF, fmt "\n", ## __VA_ARGS__)
