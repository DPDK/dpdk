/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <rte_log.h>

extern int librte_member_logtype;
#define RTE_LOGTYPE_MEMBER librte_member_logtype

#define MEMBER_LOG(level, ...) \
	RTE_LOG_LINE(level,  MEMBER, \
		RTE_FMT("%s(): " RTE_FMT_HEAD(__VA_ARGS__ ,), \
			__func__, RTE_FMT_TAIL(__VA_ARGS__ ,)))

