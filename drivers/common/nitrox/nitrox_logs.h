/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_LOGS_H_
#define _NITROX_LOGS_H_

extern int nitrox_logtype;
#define RTE_LOGTYPE_NITROX nitrox_logtype
#define NITROX_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NITROX, "%s:%d ", __func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)

#endif /* _NITROX_LOGS_H_ */
