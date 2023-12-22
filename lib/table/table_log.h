/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <rte_log.h>

extern int table_logtype;
#define RTE_LOGTYPE_TABLE table_logtype

#define TABLE_LOG(level, ...) \
	RTE_LOG_LINE(level, TABLE, "" __VA_ARGS__)
