/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation
 */

#ifndef _RTE_ZUC_PMD_PRIVATE_H_
#define _RTE_ZUC_PMD_PRIVATE_H_

#include <sso_zuc.h>

#define CRYPTODEV_NAME_ZUC_PMD		crypto_zuc
/**< KASUMI PMD device name */

/** ZUC PMD LOGTYPE DRIVER */
int zuc_logtype_driver;
#define ZUC_LOG(level, fmt, ...)  \
	rte_log(RTE_LOG_ ## level, zuc_logtype_driver,  \
			"%s()... line %u: " fmt "\n", __func__, __LINE__,  \
				## __VA_ARGS__)

#define ZUC_IV_KEY_LENGTH 16
#define ZUC_DIGEST_LENGTH 4

/** private data structure for each virtual ZUC device */
struct zuc_private {
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
};

/** ZUC buffer queue pair */
struct zuc_qp {
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
	uint8_t temp_digest[ZUC_DIGEST_LENGTH];
	/**< Buffer used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
} __rte_cache_aligned;

enum zuc_operation {
	ZUC_OP_ONLY_CIPHER,
	ZUC_OP_ONLY_AUTH,
	ZUC_OP_CIPHER_AUTH,
	ZUC_OP_AUTH_CIPHER,
	ZUC_OP_NOT_SUPPORTED
};

/** ZUC private session structure */
struct zuc_session {
	enum zuc_operation op;
	enum rte_crypto_auth_operation auth_op;
	uint8_t pKey_cipher[ZUC_IV_KEY_LENGTH];
	uint8_t pKey_hash[ZUC_IV_KEY_LENGTH];
	uint16_t cipher_iv_offset;
	uint16_t auth_iv_offset;
} __rte_cache_aligned;


extern int
zuc_set_session_parameters(struct zuc_session *sess,
		const struct rte_crypto_sym_xform *xform);


/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_zuc_pmd_ops;



#endif /* _RTE_ZUC_PMD_PRIVATE_H_ */
