/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <rte_log.h>

extern int librte_member_logtype;
#define RTE_LOGTYPE_MEMBER librte_member_logtype

#define MEMBER_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, MEMBER, \
		"%s(): ", __func__, __VA_ARGS__)
