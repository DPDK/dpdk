/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIBCRYPTO_PMD_PRIVATE_H_
#define _LIBCRYPTO_PMD_PRIVATE_H_

#include <openssl/evp.h>
#include <openssl/des.h>


#define LIBCRYPTO_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_LIBCRYPTO_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_LIBCRYPTO_DEBUG
#define LIBCRYPTO_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_LIBCRYPTO_PMD), \
			__func__, __LINE__, ## args)

#define LIBCRYPTO_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			RTE_STR(CRYPTODEV_NAME_LIBCRYPTO_PMD), \
			__func__, __LINE__, ## args)
#else
#define LIBCRYPTO_LOG_INFO(fmt, args...)
#define LIBCRYPTO_LOG_DBG(fmt, args...)
#endif


/** LIBCRYPTO operation order mode enumerator */
enum libcrypto_chain_order {
	LIBCRYPTO_CHAIN_ONLY_CIPHER,
	LIBCRYPTO_CHAIN_ONLY_AUTH,
	LIBCRYPTO_CHAIN_CIPHER_AUTH,
	LIBCRYPTO_CHAIN_AUTH_CIPHER,
	LIBCRYPTO_CHAIN_COMBINED,
	LIBCRYPTO_CHAIN_NOT_SUPPORTED
};

/** LIBCRYPTO cipher mode enumerator */
enum libcrypto_cipher_mode {
	LIBCRYPTO_CIPHER_LIB,
	LIBCRYPTO_CIPHER_DES3CTR,
};

/** LIBCRYPTO auth mode enumerator */
enum libcrypto_auth_mode {
	LIBCRYPTO_AUTH_AS_AUTH,
	LIBCRYPTO_AUTH_AS_HMAC,
};

/** private data structure for each LIBCRYPTO crypto device */
struct libcrypto_private {
	unsigned int max_nb_qpairs;
	/**< Max number of queue pairs */
	unsigned int max_nb_sessions;
	/**< Max number of sessions */
};

/** LIBCRYPTO crypto queue pair */
struct libcrypto_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_LEN];
	/**< Unique Queue Pair Name */
	struct rte_ring *processed_ops;
	/**< Ring for placing process packets */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_cryptodev_stats stats;
	/**< Queue pair statistics */
} __rte_cache_aligned;

/** LIBCRYPTO crypto private session structure */
struct libcrypto_session {
	enum libcrypto_chain_order chain_order;
	/**< chain order mode */

	/** Cipher Parameters */
	struct {
		enum rte_crypto_cipher_operation direction;
		/**< cipher operation direction */
		enum libcrypto_cipher_mode mode;
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
	} cipher;

	/** Authentication Parameters */
	struct {
		enum rte_crypto_auth_operation operation;
		/**< auth operation generate or verify */
		enum libcrypto_auth_mode mode;
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
				EVP_MD_CTX *ctx;
				/**< pointer to EVP context structure */
			} hmac;
		};
	} auth;

} __rte_cache_aligned;

/** Set and validate LIBCRYPTO crypto session parameters */
extern int
libcrypto_set_session_parameters(struct libcrypto_session *sess,
		const struct rte_crypto_sym_xform *xform);

/** Reset LIBCRYPTO crypto session parameters */
extern void
libcrypto_reset_session(struct libcrypto_session *sess);

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_libcrypto_pmd_ops;

#endif /* _LIBCRYPTO_PMD_PRIVATE_H_ */
