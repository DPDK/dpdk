/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _STACK_PVT_H_
#define _STACK_PVT_H_

#include <rte_log.h>

extern int stack_logtype;
#define RTE_LOGTYPE_STACK stack_logtype

#define STACK_LOG(level, fmt, args...) \
	RTE_LOG_LINE(level, STACK, "%s(): "fmt, __func__, ##args)

#define STACK_LOG_ERR(fmt, args...) \
	STACK_LOG(ERR, fmt, ## args)

#define STACK_LOG_WARN(fmt, args...) \
	STACK_LOG(WARNING, fmt, ## args)

#define STACK_LOG_INFO(fmt, args...) \
	STACK_LOG(INFO, fmt, ## args)

#endif /* _STACK_PVT_H_ */
