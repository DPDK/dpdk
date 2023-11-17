/* SPDX-License-Identifier: BSD-3-Clause */

extern int sched_logtype;
#define RTE_LOGTYPE_SCHED sched_logtype
#define SCHED_LOG(level, ...) \
	RTE_LOG_LINE(level, SCHED, "" __VA_ARGS__)
