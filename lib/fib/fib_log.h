/* SPDX-License-Identifier: BSD-3-Clause */

extern int fib_logtype;
#define RTE_LOGTYPE_FIB fib_logtype
#define FIB_LOG(level, fmt, ...) \
	RTE_LOG(level, FIB, fmt "\n", ## __VA_ARGS__)
