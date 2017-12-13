/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#ifndef _NULL_CRYPTO_PMD_PRIVATE_H_
#define _NULL_CRYPTO_PMD_PRIVATE_H_

#define CRYPTODEV_NAME_NULL_PMD		crypto_null
/**< Null crypto PMD device name */

#define NULL_CRYPTO_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_NULL_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_NULL_CRYPTO_DEBUG
#define NULL_CRYPTO_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_NULL_PMD), \
			__func__, __LINE__, ## args)

#define NULL_CRYPTO_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_NULL_PMD), \
			__func__, __LINE__, ## args)
#else
#define NULL_CRYPTO_LOG_INFO(fmt, args...)
#define NULL_CRYPTO_LOG_DBG(fmt, args...)
#endif


/** private data structure for each NULL crypto device */
struct null_crypto_private {
	unsigned max_nb_qpairs;		/**< Max number of queue pairs */
	unsigned max_nb_sessions;	/**< Max number of sessions */
};

/** NULL crypto queue pair */
struct null_crypto_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
	struct rte_ring *processed_pkts;
	/**< Ring for placing process packets */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_cryptodev_stats qp_stats;
	/**< Queue pair statistics */
} __rte_cache_aligned;


/** NULL crypto private session structure */
struct null_crypto_session {
	uint32_t reserved;
} __rte_cache_aligned;

/** Set and validate NULL crypto session parameters */
extern int
null_crypto_set_session_parameters(struct null_crypto_session *sess,
		const struct rte_crypto_sym_xform *xform);

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *null_crypto_pmd_ops;

#endif /* _NULL_CRYPTO_PMD_PRIVATE_H_ */
