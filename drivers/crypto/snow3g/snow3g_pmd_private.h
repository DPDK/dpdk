/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2019 Intel Corporation
 */

#ifndef _SNOW3G_PMD_PRIVATE_H_
#define _SNOW3G_PMD_PRIVATE_H_

#include <intel-ipsec-mb.h>

#define CRYPTODEV_NAME_SNOW3G_PMD	crypto_snow3g
/**< SNOW 3G PMD device name */

/** SNOW 3G PMD LOGTYPE DRIVER */
extern int snow3g_logtype_driver;

#define SNOW3G_LOG(level, fmt, ...)  \
	rte_log(RTE_LOG_ ## level, snow3g_logtype_driver,  \
			"%s() line %u: " fmt "\n", __func__, __LINE__,  \
					## __VA_ARGS__)

#define SNOW3G_DIGEST_LENGTH 4
#define SNOW3G_MAX_KEY_SIZE  128

/** private data structure for each virtual SNOW 3G device */
struct snow3g_private {
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	MB_MGR *mgr;
	/**< Multi-buffer instance */
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
	struct rte_mempool *sess_mp_priv;
	/**< Session Private Data Mempool */
	struct rte_cryptodev_stats qp_stats;
	/**< Queue pair statistics */
	uint8_t temp_digest[SNOW3G_DIGEST_LENGTH];
	/**< Buffer used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
	MB_MGR *mgr;
	/**< Multi-buffer instance */
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
	snow3g_key_schedule_t pKeySched_cipher;
	snow3g_key_schedule_t pKeySched_hash;
	uint16_t cipher_iv_offset;
	uint16_t auth_iv_offset;
} __rte_cache_aligned;


extern int
snow3g_set_session_parameters(MB_MGR *mgr, struct snow3g_session *sess,
		const struct rte_crypto_sym_xform *xform);


/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_snow3g_pmd_ops;



#endif /* _SNOW3G_PMD_PRIVATE_H_ */
