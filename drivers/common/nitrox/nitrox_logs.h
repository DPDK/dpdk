/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_LOGS_H_
#define _NITROX_LOGS_H_

#define LOG_PREFIX "NITROX: "
#define NITROX_LOG(level, fmt, args...)					\
	rte_log(RTE_LOG_ ## level, nitrox_logtype,			\
		LOG_PREFIX "%s:%d " fmt, __func__, __LINE__, ## args)

extern int nitrox_logtype;

#endif /* _NITROX_LOGS_H_ */
