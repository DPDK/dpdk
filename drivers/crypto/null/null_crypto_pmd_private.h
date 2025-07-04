/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#ifndef _NULL_CRYPTO_PMD_PRIVATE_H_
#define _NULL_CRYPTO_PMD_PRIVATE_H_

#define CRYPTODEV_NAME_NULL_PMD		crypto_null
/**< Null crypto PMD device name */

extern int null_logtype_driver;
#define RTE_LOGTYPE_NULL_DRIVER null_logtype_driver

#define NULL_LOG(level, ...)  \
	RTE_LOG_LINE_PREFIX(level, NULL_DRIVER, "%s() line %u: ", \
		__func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)


/** private data structure for each NULL crypto device */
struct null_crypto_private {
	unsigned max_nb_qpairs;		/**< Max number of queue pairs */
};

/** NULL crypto queue pair */
struct __rte_cache_aligned null_crypto_qp {
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
};


/** NULL crypto private session structure */
struct __rte_cache_aligned null_crypto_session {
	uint32_t reserved;
};

/** Set and validate NULL crypto session parameters */
extern int
null_crypto_set_session_parameters(struct null_crypto_session *sess,
		const struct rte_crypto_sym_xform *xform);

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *null_crypto_pmd_ops;

#endif /* _NULL_CRYPTO_PMD_PRIVATE_H_ */
