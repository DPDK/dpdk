/* SPDX-License-Identifier: BSD-3-Clause */

#include <rte_log.h>

extern int rib_logtype;
#define RTE_LOGTYPE_RIB rib_logtype
#define RIB_LOG(level, fmt, ...) \
	RTE_LOG(level, RIB, fmt "\n", ## __VA_ARGS__)
