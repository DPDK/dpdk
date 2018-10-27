/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_log.h>

#ifndef _RTE_TELEMETRY_INTERNAL_H_
#define _RTE_TELEMETRY_INTERNAL_H_

/* Logging Macros */
extern int telemetry_log_level;

#define TELEMETRY_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ##level, telemetry_log_level, "%s(): "fmt "\n", \
		__func__, ##args)

#define TELEMETRY_LOG_ERR(fmt, args...) \
	TELEMETRY_LOG(ERR, fmt, ## args)

#define TELEMETRY_LOG_WARN(fmt, args...) \
	TELEMETRY_LOG(WARNING, fmt, ## args)

#define TELEMETRY_LOG_INFO(fmt, args...) \
	TELEMETRY_LOG(INFO, fmt, ## args)

typedef struct telemetry_impl {
	pthread_t thread_id;
	int thread_status;
	uint32_t socket_id;
} telemetry_impl;

#endif
