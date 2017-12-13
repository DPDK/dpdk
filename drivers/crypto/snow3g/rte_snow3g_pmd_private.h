/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _RTE_SNOW3G_PMD_PRIVATE_H_
#define _RTE_SNOW3G_PMD_PRIVATE_H_

#include <sso_snow3g.h>

#define CRYPTODEV_NAME_SNOW3G_PMD	crypto_snow3g
/**< SNOW 3G PMD device name */

#define SNOW3G_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_SNOW3G_DEBUG
#define SNOW3G_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), \
			__func__, __LINE__, ## args)

#define SNOW3G_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), \
			__func__, __LINE__, ## args)
#else
#define SNOW3G_LOG_INFO(fmt, args...)
#define SNOW3G_LOG_DBG(fmt, args...)
#endif

#define SNOW3G_DIGEST_LENGTH 4

/** private data structure for each virtual SNOW 3G device */
struct snow3g_private {
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned max_nb_sessions;
	/**< Max number of sessions supported by device */
};

/** SNOW 3G buffer queue pair */
struct snow3g_qp {
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
	uint8_t temp_digest[SNOW3G_DIGEST_LENGTH];
	/**< Buffer used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
} __rte_cache_aligned;

enum snow3g_operation {
	SNOW3G_OP_ONLY_CIPHER,
	SNOW3G_OP_ONLY_AUTH,
	SNOW3G_OP_CIPHER_AUTH,
	SNOW3G_OP_AUTH_CIPHER,
	SNOW3G_OP_NOT_SUPPORTED
};

/** SNOW 3G private session structure */
struct snow3g_session {
	enum snow3g_operation op;
	enum rte_crypto_auth_operation auth_op;
	sso_snow3g_key_schedule_t pKeySched_cipher;
	sso_snow3g_key_schedule_t pKeySched_hash;
	uint16_t cipher_iv_offset;
	uint16_t auth_iv_offset;
} __rte_cache_aligned;


extern int
snow3g_set_session_parameters(struct snow3g_session *sess,
		const struct rte_crypto_sym_xform *xform);


/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_snow3g_pmd_ops;



#endif /* _RTE_SNOW3G_PMD_PRIVATE_H_ */
