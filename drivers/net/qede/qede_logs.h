/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef _QEDE_LOGS_H_
#define _QEDE_LOGS_H_

#define DP_ERR(p_dev, fmt, ...) \
	rte_log(RTE_LOG_ERR, RTE_LOGTYPE_PMD, \
		"[%s:%d(%s)]" fmt, \
		  __func__, __LINE__, \
		(p_dev)->name ? (p_dev)->name : "", \
		##__VA_ARGS__)

#define DP_NOTICE(p_dev, is_assert, fmt, ...) \
	rte_log(RTE_LOG_NOTICE, RTE_LOGTYPE_PMD,\
		"[QEDE PMD: (%s)]%s:" fmt, \
		(p_dev)->name ? (p_dev)->name : "", \
		 __func__, \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_QEDE_DEBUG_INFO

#define DP_INFO(p_dev, fmt, ...) \
	rte_log(RTE_LOG_INFO, RTE_LOGTYPE_PMD, \
		"[%s:%d(%s)]" fmt, \
		__func__, __LINE__, \
		(p_dev)->name ? (p_dev)->name : "", \
		##__VA_ARGS__)
#else
#define DP_INFO(p_dev, fmt, ...) do { } while (0)

#endif

#ifdef RTE_LIBRTE_QEDE_DEBUG_DRIVER
#define DP_VERBOSE(p_dev, module, fmt, ...) \
do { \
	if ((p_dev)->dp_module & module) \
		rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PMD, \
			"[%s:%d(%s)]" fmt, \
		      __func__, __LINE__, \
		      (p_dev)->name ? (p_dev)->name : "", \
		      ##__VA_ARGS__); \
} while (0)
#else
#define DP_VERBOSE(p_dev, fmt, ...) do { } while (0)
#endif

#define PMD_INIT_LOG(level, edev, fmt, args...)	\
	rte_log(RTE_LOG_ ## level, RTE_LOGTYPE_PMD, \
		"[qede_pmd: %s] %s() " fmt "\n", \
	(edev)->name, __func__, ##args)

#ifdef RTE_LIBRTE_QEDE_DEBUG_INIT
#define PMD_INIT_FUNC_TRACE(edev) PMD_INIT_LOG(DEBUG, edev, " >>")
#else
#define PMD_INIT_FUNC_TRACE(edev) do { } while (0)
#endif

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

#ifdef RTE_LIBRTE_QEDE_DEBUG_DRIVER
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt, __func__, ## args)
#else
#define PMD_DRV_LOG_RAW(level, fmt, args...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)

#endif /* _QEDE_LOGS_H_ */
