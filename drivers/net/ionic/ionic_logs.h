/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_LOGS_H_
#define _IONIC_LOGS_H_

#include <rte_log.h>

extern int ionic_logtype;
#define RTE_LOGTYPE_IONIC ionic_logtype

#define IONIC_PRINT(level, ...) \
	RTE_LOG_LINE_PREFIX(level, IONIC, "%s(): ", __func__, __VA_ARGS__)

#define IONIC_PRINT_CALL() IONIC_PRINT(DEBUG, " >>")

#endif /* _IONIC_LOGS_H_ */
