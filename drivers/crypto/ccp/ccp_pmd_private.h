/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef _CCP_PMD_PRIVATE_H_
#define _CCP_PMD_PRIVATE_H_

#include <rte_cryptodev.h>
#include "ccp_crypto.h"

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
#define CCP_PMD_MAX_QUEUE_PAIRS	8
#define CCP_NB_MAX_DESCRIPTORS 1024
#define CCP_MAX_BURST 256

#include "ccp_dev.h"

/* private data structure for each CCP crypto device */
struct ccp_private {
	unsigned int max_nb_qpairs;	/**< Max number of queue pairs */
	uint8_t crypto_num_dev;		/**< Number of working crypto devices */
	bool auth_opt;			/**< Authentication offload option */
	struct ccp_device *last_dev;	/**< Last working crypto device */
};

/* CCP batch info */
struct ccp_batch_info {
	struct rte_crypto_op *op[CCP_MAX_BURST];
	/**< optable populated at enque time from app*/
	int op_idx;
	uint16_t b_idx;
	struct ccp_queue *cmd_q;
	uint16_t opcnt;
	uint16_t total_nb_ops;
	/**< no. of crypto ops in batch*/
	int desccnt;
	/**< no. of ccp queue descriptors*/
	uint32_t head_offset;
	/**< ccp queue head tail offsets time of enqueue*/
	uint32_t tail_offset;
	uint8_t lsb_buf[CCP_SB_BYTES * CCP_MAX_BURST];
	phys_addr_t lsb_buf_phys;
	/**< LSB intermediate buf for passthru */
	int lsb_buf_idx;
	uint16_t auth_ctr;
	/**< auth only ops batch for CPU based auth */
} __rte_cache_aligned;

/**< CCP crypto queue pair */
struct ccp_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
	struct rte_ring *processed_pkts;
	/**< Ring for placing process packets */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_mempool *batch_mp;
	/**< Session Mempool for batch info */
	struct rte_cryptodev_stats qp_stats;
	/**< Queue pair statistics */
	struct ccp_batch_info *b_info;
	/**< Store ops pulled out of queue */
	struct rte_cryptodev *dev;
	/**< rte crypto device to which this qp belongs */
	uint8_t temp_digest[DIGEST_LENGTH_MAX];
	/**< Buffer used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
} __rte_cache_aligned;


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
