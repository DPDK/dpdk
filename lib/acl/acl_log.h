/* SPDX-License-Identifier: BSD-3-Clause */

#include <rte_log.h>

extern int acl_logtype;
#define RTE_LOGTYPE_ACL	acl_logtype
#define ACL_LOG(level, fmt, ...) \
	RTE_LOG(level, ACL, fmt "\n", ## __VA_ARGS__)
