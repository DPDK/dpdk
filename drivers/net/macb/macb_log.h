/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#ifndef _MACB_LOG_H_
#define _MACB_LOG_H_

/* Current log type. */
extern int macb_logtype;

#define RTE_LOGTYPE_MACB macb_logtype
#define MACB_LOG(level, fmt, ...) \
	RTE_LOG_LINE(level, MACB, "%s(): " fmt, \
		__func__, ##__VA_ARGS__)

#define MACB_INFO(fmt, ...) \
	RTE_LOG_LINE(INFO, MACB, fmt, ##__VA_ARGS__)

#endif /*_MACB_LOG_H_ */
