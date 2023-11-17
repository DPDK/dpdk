/* SPDX-License-Identifier: BSD-3-Clause */

extern int fib_logtype;
#define RTE_LOGTYPE_FIB fib_logtype
#define FIB_LOG(level, ...) \
	RTE_LOG_LINE(level, FIB, "" __VA_ARGS__)
