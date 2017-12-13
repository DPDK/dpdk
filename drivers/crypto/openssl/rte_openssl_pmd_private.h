/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _OPENSSL_PMD_PRIVATE_H_
#define _OPENSSL_PMD_PRIVATE_H_

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/des.h>

#define CRYPTODEV_NAME_OPENSSL_PMD	crypto_openssl
/**< Open SSL Crypto PMD device name */

#define OPENSSL_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_OPENSSL_DEBUG
#define OPENSSL_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD), \
			__func__, __LINE__, ## args)

#define OPENSSL_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD), \
			__func__, __LINE__, ## args)
#else
#define OPENSSL_LOG_INFO(fmt, args...)
#define OPENSSL_LOG_DBG(fmt, args...)
#endif

/* Maximum length for digest (SHA-512 needs 64 bytes) */
#define DIGEST_LENGTH_MAX 64

/** OPENSSL operation order mode enumerator */
enum openssl_chain_order {
	OPENSSL_CHAIN_ONLY_CIPHER,
	OPENSSL_CHAIN_ONLY_AUTH,
	OPENSSL_CHAIN_CIPHER_BPI,
	OPENSSL_CHAIN_CIPHER_AUTH,
	OPENSSL_CHAIN_AUTH_CIPHER,
	OPENSSL_CHAIN_COMBINED,
	OPENSSL_CHAIN_NOT_SUPPORTED
};

/** OPENSSL cipher mode enumerator */
enum openssl_cipher_mode {
	OPENSSL_CIPHER_LIB,
	OPENSSL_CIPHER_DES3CTR,
};

/** OPENSSL auth mode enumerator */
enum openssl_auth_mode {
	OPENSSL_AUTH_AS_AUTH,
	OPENSSL_AUTH_AS_HMAC,
};

/** private data structure for each OPENSSL crypto device */
struct openssl_private {
	unsigned int max_nb_qpairs;
	/**< Max number of queue pairs */
	unsigned int max_nb_sessions;
	/**< Max number of sessions */
};

/** OPENSSL crypto queue pair */
struct openssl_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
	struct rte_ring *processed_ops;
	/**< Ring for placing process packets */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_cryptodev_stats stats;
	/**< Queue pair statistics */
	uint8_t temp_digest[DIGEST_LENGTH_MAX];
	/**< Buffer used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
} __rte_cache_aligned;

/** OPENSSL crypto private session structure */
struct openssl_session {
	enum openssl_chain_order chain_order;
	/**< chain order mode */

	struct {
		uint16_t length;
		uint16_t offset;
	} iv;
	/**< IV parameters */

	enum rte_crypto_aead_algorithm aead_algo;
	/**< AEAD algorithm */

	/** Cipher Parameters */
	struct {
		enum rte_crypto_cipher_operation direction;
		/**< cipher operation direction */
		enum openssl_cipher_mode mode;
		/**< cipher operation mode */
		enum rte_crypto_cipher_algorithm algo;
		/**< cipher algorithm */

		struct {
			uint8_t data[32];
			/**< key data */
			size_t length;
			/**< key length in bytes */
		} key;

		const EVP_CIPHER *evp_algo;
		/**< pointer to EVP algorithm function */
		EVP_CIPHER_CTX *ctx;
		/**< pointer to EVP context structure */
		EVP_CIPHER_CTX *bpi_ctx;
	} cipher;

	/** Authentication Parameters */
	struct {
		enum rte_crypto_auth_operation operation;
		/**< auth operation generate or verify */
		enum openssl_auth_mode mode;
		/**< auth operation mode */
		enum rte_crypto_auth_algorithm algo;
		/**< cipher algorithm */

		union {
			struct {
				const EVP_MD *evp_algo;
				/**< pointer to EVP algorithm function */
				EVP_MD_CTX *ctx;
				/**< pointer to EVP context structure */
			} auth;

			struct {
				EVP_PKEY *pkey;
				/**< pointer to EVP key */
				const EVP_MD *evp_algo;
				/**< pointer to EVP algorithm function */
				HMAC_CTX *ctx;
				/**< pointer to EVP context structure */
			} hmac;
		};

		uint16_t aad_length;
		/**< AAD length */
		uint16_t digest_length;
		/**< digest length */
	} auth;

} __rte_cache_aligned;

/** Set and validate OPENSSL crypto session parameters */
extern int
openssl_set_session_parameters(struct openssl_session *sess,
		const struct rte_crypto_sym_xform *xform);

/** Reset OPENSSL crypto session parameters */
extern void
openssl_reset_session(struct openssl_session *sess);

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_openssl_pmd_ops;

#endif /* _OPENSSL_PMD_PRIVATE_H_ */
