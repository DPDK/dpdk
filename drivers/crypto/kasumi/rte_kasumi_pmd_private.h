/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _RTE_KASUMI_PMD_PRIVATE_H_
#define _RTE_KASUMI_PMD_PRIVATE_H_

#include <sso_kasumi.h>

#define CRYPTODEV_NAME_KASUMI_PMD	crypto_kasumi
/**< KASUMI PMD device name */

#define KASUMI_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_KASUMI_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_KASUMI_DEBUG
#define KASUMI_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_KASUMI_PMD), \
			__func__, __LINE__, ## args)

#define KASUMI_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_KASUMI_PMD), \
			__func__, __LINE__, ## args)
#else
#define KASUMI_LOG_INFO(fmt, args...)
#define KASUMI_LOG_DBG(fmt, args...)
#endif

#define KASUMI_DIGEST_LENGTH 4

/** private data structure for each virtual KASUMI device */
struct kasumi_private {
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned max_nb_sessions;
	/**< Max number of sessions supported by device */
};

/** KASUMI buffer queue pair */
struct kasumi_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
	struct rte_ring *processed_ops;
	/**< Ring for placing processed ops */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_cryptodev_stats qp_stats;
	/**< Queue pair statistics */
	uint8_t temp_digest[KASUMI_DIGEST_LENGTH];
	/**< Buffer used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
} __rte_cache_aligned;

enum kasumi_operation {
	KASUMI_OP_ONLY_CIPHER,
	KASUMI_OP_ONLY_AUTH,
	KASUMI_OP_CIPHER_AUTH,
	KASUMI_OP_AUTH_CIPHER,
	KASUMI_OP_NOT_SUPPORTED
};

/** KASUMI private session structure */
struct kasumi_session {
	/* Keys have to be 16-byte aligned */
	sso_kasumi_key_sched_t pKeySched_cipher;
	sso_kasumi_key_sched_t pKeySched_hash;
	enum kasumi_operation op;
	enum rte_crypto_auth_operation auth_op;
	uint16_t cipher_iv_offset;
} __rte_cache_aligned;


int
kasumi_set_session_parameters(struct kasumi_session *sess,
		const struct rte_crypto_sym_xform *xform);


/** device specific operations function pointer structure */
struct rte_cryptodev_ops *rte_kasumi_pmd_ops;

#endif /* _RTE_KASUMI_PMD_PRIVATE_H_ */
