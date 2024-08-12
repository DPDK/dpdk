/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_LOGS_H_
#define _NITROX_LOGS_H_

extern int nitrox_logtype;
#define RTE_LOGTYPE_NITROX nitrox_logtype
#define NITROX_LOG_LINE(level, fmt, args...) \
	RTE_LOG(level, NITROX, "%s:%d " fmt "\n", __func__, __LINE__, ## args)

#endif /* _NITROX_LOGS_H_ */
