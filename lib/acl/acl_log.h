/* SPDX-License-Identifier: BSD-3-Clause */

#include <rte_log.h>

extern int acl_logtype;
#define RTE_LOGTYPE_ACL	acl_logtype
#define ACL_LOG(level, ...) \
	RTE_LOG_LINE(level, ACL, "" __VA_ARGS__)
