/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef _CCP_PMD_PRIVATE_H_
#define _CCP_PMD_PRIVATE_H_

#include <rte_cryptodev.h>

#define CRYPTODEV_NAME_CCP_PMD crypto_ccp

#define CCP_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_CCP_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_CCP_DEBUG
#define CCP_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_CCP_PMD), \
			__func__, __LINE__, ## args)

#define CCP_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_CCP_PMD), \
			__func__, __LINE__, ## args)
#else
#define CCP_LOG_INFO(fmt, args...)
#define CCP_LOG_DBG(fmt, args...)
#endif

/**< Maximum queue pairs supported by CCP PMD */
#define CCP_PMD_MAX_QUEUE_PAIRS	1
#define CCP_NB_MAX_DESCRIPTORS 1024
#define CCP_MAX_BURST 64

/* private data structure for each CCP crypto device */
struct ccp_private {
	unsigned int max_nb_qpairs;	/**< Max number of queue pairs */
	unsigned int max_nb_sessions;	/**< Max number of sessions */
	uint8_t crypto_num_dev;		/**< Number of working crypto devices */
};

/**< device specific operations function pointer structure */
extern struct rte_cryptodev_ops *ccp_pmd_ops;

uint16_t
ccp_cpu_pmd_enqueue_burst(void *queue_pair,
			  struct rte_crypto_op **ops,
			  uint16_t nb_ops);
uint16_t
ccp_cpu_pmd_dequeue_burst(void *queue_pair,
			  struct rte_crypto_op **ops,
			  uint16_t nb_ops);

#endif /* _CCP_PMD_PRIVATE_H_ */
