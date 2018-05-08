/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#ifndef _RTE_MRVL_PMD_PRIVATE_H_
#define _RTE_MRVL_PMD_PRIVATE_H_

#include "rte_mrvl_compat.h"

#define CRYPTODEV_NAME_MRVL_PMD crypto_mvsam
/**< Marvell PMD device name */

#define MRVL_CRYPTO_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_MRVL_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_PMD_MRVL_CRYPTO_DEBUG
#define MRVL_CRYPTO_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_MRVL_PMD), \
			__func__, __LINE__, ## args)

#define MRVL_CRYPTO_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_MRVL_PMD), \
			__func__, __LINE__, ## args)

#else
#define MRVL_CRYPTO_LOG_INFO(fmt, args...)
#define MRVL_CRYPTO_LOG_DBG(fmt, args...)
#endif

/**
 * Handy bits->bytes conversion macro.
 */
#define BITS2BYTES(x) ((x) >> 3)

/** The operation order mode enumerator. */
enum mrvl_crypto_chain_order {
	MRVL_CRYPTO_CHAIN_CIPHER_ONLY,
	MRVL_CRYPTO_CHAIN_AUTH_ONLY,
	MRVL_CRYPTO_CHAIN_CIPHER_AUTH,
	MRVL_CRYPTO_CHAIN_AUTH_CIPHER,
	MRVL_CRYPTO_CHAIN_COMBINED,
	MRVL_CRYPTO_CHAIN_NOT_SUPPORTED,
};

/** Private data structure for each crypto device. */
struct mrvl_crypto_private {
	unsigned int max_nb_qpairs;	/**< Max number of queue pairs */
	unsigned int max_nb_sessions;	/**< Max number of sessions */
};

/** MRVL crypto queue pair structure. */
struct mrvl_crypto_qp {
	/** SAM CIO (MUSDK Queue Pair equivalent).*/
	struct sam_cio *cio;

	/** Session Mempool. */
	struct rte_mempool *sess_mp;

	/** Queue pair statistics. */
	struct rte_cryptodev_stats stats;

	/** CIO initialization parameters.*/
	struct sam_cio_params cio_params;
} __rte_cache_aligned;

/** MRVL crypto private session structure. */
struct mrvl_crypto_session {
	/** Crypto operations chain order. */
	enum mrvl_crypto_chain_order chain_order;

	/** Session initialization parameters. */
	struct sam_session_params sam_sess_params;

	/** SAM session pointer. */
	struct sam_sa *sam_sess;

	/** Cipher IV offset. */
	uint16_t cipher_iv_offset;
} __rte_cache_aligned;

/** Set and validate MRVL crypto session parameters */
extern int
mrvl_crypto_set_session_parameters(struct mrvl_crypto_session *sess,
		const struct rte_crypto_sym_xform *xform);

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_mrvl_crypto_pmd_ops;

#endif /* _RTE_MRVL_PMD_PRIVATE_H_ */
