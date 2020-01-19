/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_LOGS_H_
#define _IONIC_LOGS_H_

#include <rte_log.h>

extern int ionic_logtype;

#define IONIC_PRINT(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
	ionic_logtype, "%s(): " fmt "\n", __func__, ##args)

#define IONIC_PRINT_CALL() IONIC_PRINT(DEBUG, " >>")

#ifndef IONIC_WARN_ON
#define IONIC_WARN_ON(x) do { \
	int ret = !!(x); \
	if (unlikely(ret)) \
		IONIC_PRINT(WARNING, "WARN_ON: \"" #x "\" at %s:%d\n", \
			__func__, __LINE__); \
} while (0)
#endif

#endif /* _IONIC_LOGS_H_ */
