/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef _QEDE_LOGS_H_
#define _QEDE_LOGS_H_

extern int qede_logtype_driver;

#define DP_ERR(p_dev, fmt, ...)				\
	rte_log(RTE_LOG_ERR, qede_logtype_driver,	\
		"[%s:%d(%s)]" fmt,			\
		__func__, __LINE__,			\
		(p_dev)->name ? (p_dev)->name : "",	\
		##__VA_ARGS__)

#define DP_NOTICE(p_dev, is_assert, fmt, ...) \
do { \
	if (is_assert) \
		rte_log(RTE_LOG_ERR, qede_logtype_driver,\
			"[QEDE PMD: (%s)]%s:" fmt, \
			(p_dev)->name ? (p_dev)->name : "", \
			 __func__, \
			##__VA_ARGS__); \
	else \
		rte_log(RTE_LOG_NOTICE, qede_logtype_driver,\
			"[QEDE PMD: (%s)]%s:" fmt, \
			(p_dev)->name ? (p_dev)->name : "", \
			 __func__, \
			##__VA_ARGS__); \
} while (0)

#define DP_INFO(p_dev, fmt, ...) \
	rte_log(RTE_LOG_INFO, qede_logtype_driver, \
		"[%s:%d(%s)]" fmt, \
		__func__, __LINE__, \
		(p_dev)->name ? (p_dev)->name : "", \
		##__VA_ARGS__)

#define DP_VERBOSE(p_dev, module, fmt, ...)				\
	do {								\
		if ((p_dev)->dp_module & module)			\
			rte_log(RTE_LOG_DEBUG, qede_logtype_driver,	\
				"[%s:%d(%s)]" fmt,			\
				__func__, __LINE__,			\
				(p_dev)->name ? (p_dev)->name : "",	\
				##__VA_ARGS__);				\
	} while (0)

extern int qede_logtype_init;
#define PMD_INIT_LOG(level, edev, fmt, args...)		\
	rte_log(RTE_LOG_ ## level, qede_logtype_init,	\
		"[qede_pmd: %s] %s() " fmt "\n",	\
		(edev)->name, __func__, ##args)

#define PMD_INIT_FUNC_TRACE(edev) PMD_INIT_LOG(DEBUG, edev, " >>")

#ifdef RTE_LIBRTE_QEDE_DEBUG_TX
#define PMD_TX_LOG(level, q, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): port=%u queue=%u " fmt "\n", \
		__func__, q->port_id, q->queue_id, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_QEDE_DEBUG_RX
#define PMD_RX_LOG(level, q, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): port=%u queue=%u " fmt "\n",	\
		__func__, q->port_id, q->queue_id, ## args)
#else
#define PMD_RX_LOG(level, q, fmt, args...) do { } while (0)
#endif

#endif /* _QEDE_LOGS_H_ */
